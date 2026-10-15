#include "app.h"

#include "auth/authmanager.h"
#include "graph/graphclient.h"
#include "graph/todoapi.h"
#include "cache/database.h"
#include "tray/trayicon.h"
#include "models/tasksmodel.h"
#include "models/tasklistsmodel.h"
#include "util/dateparser.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <KColorSchemeManager>
#include <KColorSchemeModel>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KNotification>
#include <QAction>
#include <QApplication>
#include <QDate>
#include <QDesktopServices>
#include <QStandardPaths>
#include <QMimeDatabase>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonArray>
#include <QNetworkInformation>
#include <QJsonObject>
#include <QLocale>
#include <QTimer>
#include <QTimeZone>
#include <QRegularExpression>
#include <QUrl>
#include <QDebug>

namespace Merkzettel {

App::App(QObject *parent)
    : QObject(parent)
    , m_auth(std::make_unique<AuthManager>(this))
    , m_graph(std::make_unique<GraphClient>(m_auth.get(), this))
    , m_todo(std::make_unique<TodoApi>(m_graph.get(), this))
    , m_db(std::make_unique<Database>(this))
    , m_tray(std::make_unique<TrayIcon>(this))
    , m_tasksModel(new TasksModel(this))
    , m_listsModel(new TaskListsModel(this))
    // Singleton since KF 6.6 — the first call constructs and auto-restores
    // the previously-saved color scheme. No saved choice = follow system.
    , m_colorSchemeManager(KColorSchemeManager::instance())
{
    // Sync our cached label with whatever the manager restored — needed so the
    // QML drawer's checkmark lands on the right entry at first paint.
    {
        const QString id = m_colorSchemeManager->activeSchemeId();
        if (id == QLatin1String("BreezeLight"))
            m_colorScheme = QStringLiteral("light");
        else if (id == QLatin1String("BreezeDark"))
            m_colorScheme = QStringLiteral("dark");
        else
            m_colorScheme = QStringLiteral("auto");
    }
    connect(m_auth.get(), &AuthManager::authenticated, this, &App::onAuthenticated);
    connect(m_auth.get(), &AuthManager::authError, this, &App::onAuthError);
    connect(m_auth.get(), &AuthManager::loggedOut, this, [this] {
        // Only announce a sign-out the user can notice; a first start without
        // stored credentials just shows the login page.
        const bool hadData = m_listsModel->rowCount() > 0;
        m_tasksModel->clear();
        m_listsModel->clear();
        Q_EMIT loggedInChanged();
        if (hadData) setStatus(i18n("Signed out"));
    });
    connect(m_auth.get(), &AuthManager::sessionRestored, this, [this] {
        // Stored credentials: show the cached data right away, even offline.
        Q_EMIT loggedInChanged();
    });
    connect(m_auth.get(), &AuthManager::refreshFailed, this,
            [this](const QString &message, bool sessionEnded) {
        if (sessionEnded) {
            Q_EMIT errorOccurred(message);
            return;
        }
        setOnline(false);
        setStatus(i18n("Offline — showing saved data"));
        m_authRetryTimer->start();
    });

    // Retry a failed token refresh periodically, and right away when the
    // network comes back.
    m_authRetryTimer = new QTimer(this);
    m_authRetryTimer->setSingleShot(true);
    m_authRetryTimer->setInterval(60 * 1000);
    connect(m_authRetryTimer, &QTimer::timeout, this, [this] {
        if (m_auth->hasSession() && !m_auth->isAuthenticated()) m_auth->requestFreshToken();
    });

    connect(m_todo.get(), &TodoApi::listsReceived, this, [this](const QList<TaskList> &lists) {
        m_db->upsertLists(lists);
        m_listsModel->setLists(lists);
        Q_EMIT currentListNameChanged();
        pushListNamesToModel();
        if (m_currentListId.isEmpty() && !lists.isEmpty()) {
            setCurrentListId(lists.first().id);
        } else if (isVirtualList()) {
            // Smart-list: refresh tasks across every real list.
            syncAllLists();
        } else if (!m_currentListId.isEmpty()) {
            m_todo->fetchTasks(m_currentListId);
        }
        setStatus(i18n("Lists updated"));
    });

    connect(m_todo.get(), &TodoApi::tasksReceived, this,
            [this](const QString &listId, const QList<Task> &tasks) {
        m_db->upsertTasks(listId, tasks);
        if (listId == m_currentListId) {
            m_tasksModel->setTasks(tasks);
            refreshDetailTaskFromModel();
        } else if (isVirtualList()) {
            m_tasksModel->setTasks(m_db->allTasks());
            refreshDetailTaskFromModel();
        }
        m_tray->setOpenCount(m_db->openTaskCountForToday());
        setStatus(i18n("%1 tasks", tasks.size()));
        // After a full fetch, bootstrap a delta token so the next refresh
        // can run as a cheap incremental sync. Skip if we already have one
        // (e.g. taskMutated triggered a refetch but delta was still valid).
        if (m_db->deltaLink(listId).isEmpty()) {
            m_todo->bootstrapDeltaLink(listId);
        }
    });

    connect(m_todo.get(), &TodoApi::taskMutated, this,
            [this](const QString &listId) { m_todo->fetchTasks(listId); });

    connect(m_todo.get(), &TodoApi::checklistItemMutated, this,
            [this](const QString &listId, const QString &) { m_todo->fetchTasks(listId); });

    connect(m_todo.get(), &TodoApi::linkedResourceMutated, this,
            [this](const QString &listId, const QString &) { m_todo->fetchTasks(listId); });

    connect(
        m_todo.get(), &TodoApi::linkedResourcesReceived, this,
        [this](const QString &listId, const QString &taskId, const QList<LinkedResource> &items) {
        m_db->upsertLinkedResources(listId, taskId, items);
        if (listId == m_currentListId) {
            const auto cached = m_db->tasks(listId);
            m_tasksModel->setTasks(cached);
            if (m_detailTask.value(QStringLiteral("taskId")).toString() == taskId) {
                refreshDetailTaskFromModel();
            }
        } else if (isVirtualList()) {
            m_tasksModel->setTasks(m_db->allTasks());
            if (m_detailTask.value(QStringLiteral("taskId")).toString() == taskId) {
                refreshDetailTaskFromModel();
            }
        }
    });

    connect(m_todo.get(), &TodoApi::tasksDelta, this,
            [this](const QString &listId, const QList<Task> &changed, const QStringList &deletedIds,
                   const QString &newDeltaLink) {
        m_db->applyTaskDelta(listId, changed, deletedIds);
        if (!newDeltaLink.isEmpty()) {
            m_db->setDeltaLink(listId, newDeltaLink);
        }
        // Delta payloads carry no expand fields ($expand isn't supported on
        // /tasks/delta). Refetch sub-collections per changed task so the
        // cache stays in sync; small N typically (0-3 changes per sync).
        for (const auto &t : changed) {
            m_todo->fetchChecklistItems(listId, t.id);
            m_todo->fetchLinkedResources(listId, t.id);
        }
        if (listId == m_currentListId) {
            const auto cached = m_db->tasks(listId);
            m_tasksModel->setTasks(cached);
        } else if (isVirtualList()) {
            m_tasksModel->setTasks(m_db->allTasks());
            refreshDetailTaskFromModel();
        }
        m_tray->setOpenCount(m_db->openTaskCountForToday());
        setStatus(i18n("Synced — %1 changed, %2 removed", changed.size(), deletedIds.size()));
    });

    connect(m_todo.get(), &TodoApi::tasksDeltaExpired, this, [this](const QString &listId) {
        // Token expired (>30 days, server-side replay limit, etc). Drop the
        // link and fall back to a full fetch — that path will bootstrap a
        // fresh delta token afterwards.
        m_db->clearDeltaLink(listId);
        m_todo->fetchTasks(listId);
    });

    connect(
        m_todo.get(), &TodoApi::checklistItemsReceived, this,
        [this](const QString &listId, const QString &taskId, const QList<ChecklistItem> &items) {
        m_db->upsertChecklistItems(listId, taskId, items);
        if (listId == m_currentListId) {
            const auto cached = m_db->tasks(listId);
            m_tasksModel->setTasks(cached);
            if (m_detailTask.value(QStringLiteral("taskId")).toString() == taskId) {
                refreshDetailTaskFromModel();
            }
        } else if (isVirtualList()) {
            m_tasksModel->setTasks(m_db->allTasks());
            if (m_detailTask.value(QStringLiteral("taskId")).toString() == taskId) {
                refreshDetailTaskFromModel();
            }
        }
    });

    connect(m_todo.get(), &TodoApi::listMutated, this, [this] { m_todo->fetchLists(); });

    connect(m_todo.get(), &TodoApi::listCreated, this, [this](const QString &newId) {
        // Switch to the new list as soon as fetchLists completes (otherwise
        // setCurrentListId would race the listsModel update). Done with a
        // single-shot connection so we don't accidentally re-trigger on
        // future refreshes.
        connect(m_todo.get(), &TodoApi::listsReceived, this,
                [this, newId] { setCurrentListId(newId); }, Qt::SingleShotConnection);
    });

    connect(m_todo.get(), &TodoApi::errorOccurred, this, [this](const QString &msg) {
        setStatus(i18n("Error: %1", msg));
        Q_EMIT errorOccurred(msg);
    });

    connect(m_tray.get(), &TrayIcon::activated, this, &App::toggleWindow);
    connect(m_tray.get(), &TrayIcon::quitRequested, this, [] { qApp->quit(); });
    connect(m_tray.get(), &TrayIcon::syncRequested, this, &App::refresh);

    connect(m_todo.get(), &TodoApi::attachmentsReceived, this,
            [this](const QString &, const QString &taskId, const QList<Attachment> &atts) {
        m_attachments.insert(taskId, atts);
        if (m_detailTask.value(QStringLiteral("taskId")).toString() == taskId) {
            setDetailTask(m_detailTask);
        }
    });
    connect(m_todo.get(), &TodoApi::attachmentMutated, this,
            [this](const QString &listId, const QString &taskId) {
        m_todo->fetchAttachments(listId, taskId);
        m_todo->fetchTasks(listId); // hasAttachments flag for the list icon
        setStatus(i18n("Attachments updated"));
    });

    m_commitTimer = new QTimer(this);
    m_commitTimer->setSingleShot(true);
    m_commitTimer->setInterval(6000);
    connect(m_commitTimer, &QTimer::timeout, this, &App::commitPending);

    // Refetch every list a batch of moves touched once the last one is done,
    // instead of two full fetches per moved task.
    const auto moveFinished = [this] {
        if (--m_movesInFlight > 0) return;
        m_movesInFlight = 0;
        for (const auto &lid : std::as_const(m_moveTouchedLists))
            m_todo->fetchTasks(lid);
        m_moveTouchedLists.clear();
    };
    connect(m_todo.get(), &TodoApi::taskMoved, this,
            [this, moveFinished](const QString &, const QString &oldId, const QString &,
                                 const QString &newId) {
        if (!m_demoMode && m_tasksModel->isInMyDay(oldId)) {
            m_db->renameMyDayTask(oldId, newId);
            reloadMyDay();
        }
        moveFinished();
    });
    connect(m_todo.get(), &TodoApi::taskMoveFailed, this,
            [this, moveFinished](const QString &, const QString &taskId, const QString &msg) {
        m_tasksModel->unhideTasks({taskId});
        setStatus(i18n("Error: %1", msg));
        Q_EMIT errorOccurred(msg);
        moveFinished();
    });

    m_reminderTimer = new QTimer(this);
    m_reminderTimer->setInterval(60 * 1000); // 1 minute
    connect(m_reminderTimer, &QTimer::timeout, this, &App::checkReminders);
    // My Day starts empty every day.
    connect(m_reminderTimer, &QTimer::timeout, this, [this] {
        if (m_myDayDate != QDate::currentDate()) reloadMyDay();
    });
}

App::~App() = default;

void App::start()
{
    if (m_demoMode) {
        loadDemoData();
        m_tray->show();
        registerGlobalShortcuts();
        reloadMyDay();
        m_tray->setOpenCount(0);
        Q_EMIT loggedInChanged();
        setStatus(i18n("Demo mode — no real data, all changes are local"));
        return;
    }

    m_db->open();
    m_tray->show();
    registerGlobalShortcuts();
    reloadMyDay();
    if (QNetworkInformation::loadBackendByFeatures(QNetworkInformation::Feature::Reachability)) {
        connect(QNetworkInformation::instance(), &QNetworkInformation::reachabilityChanged, this,
                [this](QNetworkInformation::Reachability r) {
            if (r == QNetworkInformation::Reachability::Online) {
                if (m_auth->hasSession() && !m_auth->isAuthenticated()) m_auth->requestFreshToken();
            } else if (r == QNetworkInformation::Reachability::Disconnected) {
                setOnline(false);
            }
        });
    }

    const auto cachedLists = m_db->lists();
    if (!cachedLists.isEmpty()) {
        m_listsModel->setLists(cachedLists);
        Q_EMIT currentListNameChanged();
        pushListNamesToModel();
        if (m_currentListId.isEmpty()) {
            setCurrentListId(cachedLists.first().id);
        }
    }
    m_tray->setOpenCount(m_db->openTaskCountForToday());

    // Mark every already-overdue reminder as "fired" so launching the app
    // doesn't unleash a flood of notifications. Anything firing from now on
    // is genuine.
    const auto stale = m_db->reminderHitsBefore(QDateTime::currentDateTimeUtc());
    for (const auto &h : stale) {
        m_firedReminders.insert(h.taskId + QLatin1Char('|')
                                + h.reminderUtc.toUTC().toString(Qt::ISODate));
    }
    m_reminderTimer->start();

    m_auth->start();
    Q_EMIT loggedInChanged();
}

void App::registerGlobalShortcuts()
{
    // KGlobalAccel keys the shortcut by component (KAboutData) + action
    // objectName; the user can rebind it in System Settings > Shortcuts.
    auto *quickAdd = new QAction(i18n("Quick add task"), this);
    quickAdd->setObjectName(QStringLiteral("quick_add_task"));
    KGlobalAccel::setGlobalShortcut(quickAdd, QKeySequence(Qt::META | Qt::ALT | Qt::Key_N));
    connect(quickAdd, &QAction::triggered, this, [this] {
        activateWindow();
        Q_EMIT quickAddRequested();
    });
}

bool App::loggedIn() const
{
    return m_demoMode || m_auth->hasSession();
}
bool App::canSync() const
{
    return !m_demoMode && m_auth->isAuthenticated();
}

void App::setOnline(bool online)
{
    if (online == m_online) return;
    m_online = online;
    Q_EMIT onlineChanged();
}
QString App::status() const
{
    return m_status;
}
QString App::currentListId() const
{
    return m_currentListId;
}

TasksModel *App::tasksModel() const
{
    return m_tasksModel;
}
TaskListsModel *App::listsModel() const
{
    return m_listsModel;
}

void App::setCurrentListId(const QString &id)
{
    if (m_currentListId == id) return;
    // Hidden pending tasks only make sense in the view they were hidden in.
    commitPending();
    m_currentListId = id;
    Q_EMIT currentListIdChanged();
    Q_EMIT currentListNameChanged();
    applyListViewSettings();

    if (m_demoMode) {
        switchDemoList(id);
        return;
    }

    if (isVirtualList()) {
        // Aggregate everything we already have cached, then trigger a sync of
        // every real list so the aggregated view picks up remote changes.
        m_tasksModel->setTasks(m_db->allTasks());
        if (canSync()) syncAllLists();
        return;
    }

    const auto cachedTasks = m_db->tasks(id);
    m_tasksModel->setTasks(cachedTasks);

    if (canSync()) {
        // Prefer incremental delta sync when we have a token from a prior run.
        // Otherwise fall back to a full fetch (which will get a delta token
        // bootstrapped on its tasksReceived path).
        const QString delta = m_db->deltaLink(id);
        if (!delta.isEmpty()) {
            m_todo->syncTasks(id, delta);
        } else {
            m_todo->fetchTasks(id);
        }
    }
}

bool App::isVirtualList() const
{
    return TaskListsModel::isVirtualId(m_currentListId);
}

void App::applyListViewSettings()
{
    m_tasksModel->setSortScope(m_currentListId);

    using SF = TasksModel::SmartFilter;
    SF filter = SF::None;
    if (m_currentListId == QLatin1String(TaskListsModel::kVirtualMyDayId))
        filter = SF::MyDay;
    else if (m_currentListId == QLatin1String(TaskListsModel::kVirtualImportantId))
        filter = SF::Important;
    else if (m_currentListId == QLatin1String(TaskListsModel::kVirtualPlannedId))
        filter = SF::Planned;
    else if (m_currentListId == QLatin1String(TaskListsModel::kVirtualCompletedId))
        filter = SF::Completed;
    m_tasksModel->setSmartFilter(filter);
}

QString App::listIdForTask(const QString &taskId) const
{
    if (isVirtualList()) return m_tasksModel->listIdForTask(taskId);
    return m_currentListId;
}

void App::refreshAggregatedTasks()
{
    if (isVirtualList()) m_tasksModel->setTasks(m_db->allTasks());
}

void App::pushListNamesToModel()
{
    QHash<QString, QString> names;
    const int n = m_listsModel->rowCount();
    for (int i = 0; i < n; ++i) {
        const auto idx = m_listsModel->index(i, 0);
        if (m_listsModel->data(idx, TaskListsModel::IsVirtualRole).toBool()) continue;
        const QString id = m_listsModel->data(idx, TaskListsModel::IdRole).toString();
        const QString name = m_listsModel->data(idx, TaskListsModel::DisplayNameRole).toString();
        if (!id.isEmpty()) names.insert(id, name);
    }
    m_tasksModel->setListNames(names);
}

void App::syncAllLists()
{
    if (!canSync()) return;
    const int n = m_listsModel->rowCount();
    for (int i = 0; i < n; ++i) {
        const auto idx = m_listsModel->index(i, 0);
        if (m_listsModel->data(idx, TaskListsModel::IsVirtualRole).toBool()) continue;
        const QString id = m_listsModel->data(idx, TaskListsModel::IdRole).toString();
        if (id.isEmpty()) continue;
        const QString delta = m_db->deltaLink(id);
        if (!delta.isEmpty())
            m_todo->syncTasks(id, delta);
        else
            m_todo->fetchTasks(id);
    }
}

void App::refreshDetailTaskFromModel()
{
    const QString openId = m_detailTask.value(QStringLiteral("taskId")).toString();
    if (openId.isEmpty()) return;
    const int n = m_tasksModel->rowCount();
    for (int i = 0; i < n; ++i) {
        const auto idx = m_tasksModel->index(i, 0);
        if (m_tasksModel->data(idx, TasksModel::IdRole).toString() == openId) {
            setDetailTask(m_tasksModel->taskAt(i));
            return;
        }
    }
}

void App::login()
{
    m_auth->login();
}

bool App::signInAvailable() const
{
    return sizeof(MERKZETTEL_CLIENT_ID) > 1; // string literal: "" has size 1
}

void App::enterDemoMode()
{
    if (m_demoMode || m_auth->hasSession()) return;
    m_demoMode = true;
    m_currentListId.clear();
    loadDemoData();
    reloadMyDay();
    Q_EMIT demoModeChanged();
    Q_EMIT loggedInChanged();
    setStatus(i18n("Demo mode — no real data, all changes are local"));
}

void App::leaveDemoMode()
{
    if (!canLeaveDemo()) return;
    m_demoMode = false;
    m_demoTasks.clear();
    m_demoMyDay.clear();
    m_attachments.clear();
    m_currentListId.clear();
    m_tasksModel->clear();
    m_listsModel->clear();
    Q_EMIT currentListIdChanged();
    Q_EMIT demoModeChanged();
    Q_EMIT loggedInChanged();
}
void App::logout()
{
    m_auth->logout();
}

void App::refresh()
{
    if (m_demoMode) {
        setStatus(i18n("Demo mode — nothing to sync"));
        return;
    }
    if (!loggedIn()) return;
    if (!canSync()) {
        // Signed in but no access token (offline at startup, or a failed
        // refresh): a manual sync retries the token refresh first.
        setStatus(i18n("Connecting to Microsoft ..."));
        m_auth->requestFreshToken();
        return;
    }
    setStatus(i18n("Synchronizing ..."));
    m_todo->fetchLists();
    if (isVirtualList()) {
        syncAllLists();
    } else if (!m_currentListId.isEmpty()) {
        const QString delta = m_db->deltaLink(m_currentListId);
        if (!delta.isEmpty()) {
            m_todo->syncTasks(m_currentListId, delta);
        } else {
            m_todo->fetchTasks(m_currentListId);
        }
    }
}

void App::addTask(const QString &input)
{
    if (input.trimmed().isEmpty()) return;
    if (m_demoMode) {
        setStatus(i18n("Demo mode — change not applied"));
        return;
    }

    QString targetList = m_currentListId;
    if (isVirtualList() || targetList.isEmpty()) {
        // Virtual smart-list (or no selection yet): pick the user's default
        // list, falling back to the first real list if none is flagged.
        QString defaultId, firstRealId;
        const int n = m_listsModel->rowCount();
        for (int i = 0; i < n; ++i) {
            const auto idx = m_listsModel->index(i, 0);
            if (m_listsModel->data(idx, TaskListsModel::IsVirtualRole).toBool()) continue;
            const QString id = m_listsModel->data(idx, TaskListsModel::IdRole).toString();
            if (firstRealId.isEmpty()) firstRealId = id;
            if (m_listsModel->data(idx, TaskListsModel::IsDefaultRole).toBool()) {
                defaultId = id;
                break;
            }
        }
        targetList = defaultId.isEmpty() ? firstRealId : defaultId;
    }
    if (targetList.isEmpty()) return;

    // Like MS To Do: adding from a smart-list gives the task the property
    // that list filters on, so it doesn't vanish right after creation.
    auto parsed = parseQuickAdd(input.trimmed(), QDate::currentDate());
    const bool important = m_currentListId == QLatin1String(TaskListsModel::kVirtualImportantId);
    if (m_currentListId == QLatin1String(TaskListsModel::kVirtualPlannedId)
        && !parsed.second.isValid()) {
        parsed.second = QDateTime(QDate::currentDate(), QTime(9, 0), QTimeZone::LocalTime);
    }
    std::function<void(const QString &)> onCreated;
    if (m_currentListId == QLatin1String(TaskListsModel::kVirtualMyDayId)) {
        onCreated = [this](const QString &id) { setInMyDay({id}, true); };
    }
    m_todo->addTask(targetList, parsed.first, parsed.second, important, onCreated);
}

void App::completeTask(const QString &taskId)
{
    const QString lid = listIdForTask(taskId);
    if (lid.isEmpty()) return;
    if (m_demoMode) {
        setStatus(i18n("Demo mode — change not applied"));
        return;
    }
    m_todo->setTaskStatus(lid, taskId, QStringLiteral("completed"));
}

void App::uncompleteTask(const QString &taskId)
{
    const QString lid = listIdForTask(taskId);
    if (lid.isEmpty()) return;
    if (m_demoMode) {
        setStatus(i18n("Demo mode — change not applied"));
        return;
    }
    m_todo->setTaskStatus(lid, taskId, QStringLiteral("notStarted"));
}

void App::deleteTask(const QString &taskId)
{
    deleteTasks({taskId});
}

QString App::listDisplayName(const QString &listId) const
{
    const int n = m_listsModel->rowCount();
    for (int i = 0; i < n; ++i) {
        const auto idx = m_listsModel->index(i, 0);
        if (m_listsModel->data(idx, TaskListsModel::IdRole).toString() == listId)
            return m_listsModel->data(idx, TaskListsModel::DisplayNameRole).toString();
    }
    return {};
}

void App::deleteTasks(const QStringList &taskIds)
{
    commitPending();
    UndoState op;
    op.kind = UndoState::Delete;
    op.pending = true;
    for (const auto &id : taskIds) {
        const QString lid = listIdForTask(id);
        if (!lid.isEmpty()) op.tasks.append({lid, id});
    }
    if (op.tasks.isEmpty()) return;
    m_undo = op;
    m_tasksModel->hideTasks(taskIds);
    m_commitTimer->start();
    Q_EMIT undoOffered(i18np("Task deleted", "%1 tasks deleted", op.tasks.size()),
                       m_commitTimer->interval());
}

void App::moveTasks(const QStringList &taskIds, const QString &targetListId)
{
    if (targetListId.isEmpty() || TaskListsModel::isVirtualId(targetListId)) return;
    commitPending();
    UndoState op;
    op.kind = UndoState::Move;
    op.pending = true;
    op.targetListId = targetListId;
    QStringList moving;
    for (const auto &id : taskIds) {
        const QString lid = listIdForTask(id);
        if (lid.isEmpty() || lid == targetListId) continue;
        op.tasks.append({lid, id});
        moving.append(id);
    }
    if (op.tasks.isEmpty()) return;
    m_undo = op;
    m_tasksModel->hideTasks(moving);
    m_commitTimer->start();
    Q_EMIT undoOffered(i18np("Task moved to \"%2\"", "%1 tasks moved to \"%2\"", op.tasks.size(),
                             listDisplayName(targetListId)),
                       m_commitTimer->interval());
}

void App::completeTasks(const QStringList &taskIds)
{
    if (m_demoMode) {
        setStatus(i18n("Demo mode — change not applied"));
        return;
    }
    commitPending();
    UndoState op;
    op.kind = UndoState::Complete;
    for (const auto &id : taskIds) {
        const QString lid = listIdForTask(id);
        if (lid.isEmpty()) continue;
        op.tasks.append({lid, id});
        m_todo->setTaskStatus(lid, id, QStringLiteral("completed"));
    }
    if (op.tasks.isEmpty()) return;
    m_undo = op;
    Q_EMIT undoOffered(i18np("Task completed", "%1 tasks completed", op.tasks.size()),
                       m_commitTimer->interval());
}

void App::setTasksImportant(const QStringList &taskIds, bool important)
{
    if (m_demoMode) {
        setStatus(i18n("Demo mode — change not applied"));
        return;
    }
    for (const auto &id : taskIds) {
        const QString lid = listIdForTask(id);
        if (lid.isEmpty()) continue;
        m_todo->setTaskImportance(lid, id,
                                  important ? QStringLiteral("high") : QStringLiteral("normal"));
    }
}

void App::reloadMyDay()
{
    m_myDayDate = QDate::currentDate();
    m_tasksModel->setMyDayIds(m_demoMode ? m_demoMyDay : m_db->myDayTaskIds(m_myDayDate));
    refreshDetailTaskFromModel();
}

void App::setInMyDay(const QStringList &taskIds, bool inMyDay)
{
    if (taskIds.isEmpty()) return;
    if (m_demoMode) {
        for (const auto &id : taskIds) {
            if (inMyDay)
                m_demoMyDay.insert(id);
            else
                m_demoMyDay.remove(id);
        }
    } else {
        m_db->setInMyDay(taskIds, inMyDay, QDate::currentDate());
    }
    reloadMyDay();
}

void App::toggleMyDay(const QStringList &taskIds)
{
    bool allIn = !taskIds.isEmpty();
    for (const auto &id : taskIds)
        allIn = allIn && m_tasksModel->isInMyDay(id);
    setInMyDay(taskIds, !allIn);
}

void App::undo()
{
    if (m_undo.kind == UndoState::None) return;
    QStringList ids;
    for (const auto &t : std::as_const(m_undo.tasks))
        ids.append(t.second);
    if (m_undo.pending) {
        // Never sent to Graph: just show the tasks again.
        m_commitTimer->stop();
        m_tasksModel->unhideTasks(ids);
    } else if (m_undo.kind == UndoState::Complete) {
        // Note: completing a recurring task makes Graph spawn the next
        // occurrence; reopening the original doesn't remove that one.
        for (const auto &t : std::as_const(m_undo.tasks))
            m_todo->setTaskStatus(t.first, t.second, QStringLiteral("notStarted"));
    }
    m_undo = {};
}

void App::commitPending()
{
    if (!m_undo.pending) return;
    m_commitTimer->stop();
    const UndoState op = m_undo;
    m_undo = {}; // committed operations can't be undone anymore

    if (m_demoMode) {
        // Apply locally so the demo behaves like the real thing.
        for (const auto &t : op.tasks) {
            auto &src = m_demoTasks[t.first];
            for (int i = 0; i < src.size(); ++i) {
                if (src.at(i).id != t.second) continue;
                const Task task = src.takeAt(i);
                if (op.kind == UndoState::Move) m_demoTasks[op.targetListId].append(task);
                break;
            }
        }
        switchDemoList(m_currentListId);
        return;
    }

    for (const auto &t : op.tasks) {
        if (op.kind == UndoState::Delete) {
            m_todo->deleteTask(t.first, t.second);
        } else {
            ++m_movesInFlight;
            m_moveTouchedLists.insert(t.first);
            m_moveTouchedLists.insert(op.targetListId);
            m_todo->moveTask(t.first, t.second, op.targetListId);
        }
    }
}

void App::createList(const QString &displayName)
{
    if (displayName.trimmed().isEmpty()) return;
    if (m_demoMode) {
        setStatus(i18n("Demo mode — change not applied"));
        return;
    }
    m_todo->createList(displayName.trimmed());
}

void App::deleteList(const QString &listId)
{
    if (listId.isEmpty()) return;
    if (m_demoMode) {
        setStatus(i18n("Demo mode — change not applied"));
        return;
    }
    m_todo->deleteList(listId);
}

void App::renameList(const QString &listId, const QString &newName)
{
    if (listId.isEmpty() || newName.trimmed().isEmpty()) return;
    if (m_demoMode) {
        setStatus(i18n("Demo mode — change not applied"));
        return;
    }
    m_todo->renameList(listId, newName.trimmed());
}

void App::toggleImportance(const QString &taskId)
{
    const QString lid = listIdForTask(taskId);
    if (lid.isEmpty()) return;
    if (m_demoMode) {
        setStatus(i18n("Demo mode — change not applied"));
        return;
    }
    const int n = m_tasksModel->rowCount();
    QString current = QStringLiteral("normal");
    for (int i = 0; i < n; ++i) {
        const auto idx = m_tasksModel->index(i, 0);
        if (m_tasksModel->data(idx, TasksModel::IdRole).toString() == taskId) {
            current = m_tasksModel->data(idx, TasksModel::ImportanceRole).toString();
            break;
        }
    }
    const QString next
        = (current == QLatin1String("high")) ? QStringLiteral("normal") : QStringLiteral("high");
    m_todo->setTaskImportance(lid, taskId, next);
}

void App::setTaskDueDate(const QString &taskId, const QDateTime &due)
{
    const QString lid = listIdForTask(taskId);
    if (lid.isEmpty()) return;
    if (m_demoMode) {
        setStatus(i18n("Demo mode — change not applied"));
        return;
    }
    m_todo->setTaskDueDate(lid, taskId, due);
}

void App::clearTaskDueDate(const QString &taskId)
{
    const QString lid = listIdForTask(taskId);
    if (lid.isEmpty()) return;
    if (m_demoMode) {
        setStatus(i18n("Demo mode — change not applied"));
        return;
    }
    m_todo->setTaskDueDate(lid, taskId, {});
}

void App::setTaskReminder(const QString &taskId, const QDateTime &when)
{
    const QString lid = listIdForTask(taskId);
    if (lid.isEmpty() || !when.isValid()) return;
    if (m_demoMode) {
        setStatus(i18n("Demo mode — change not applied"));
        return;
    }
    m_todo->setTaskReminder(lid, taskId, when);
}

void App::clearTaskReminder(const QString &taskId)
{
    const QString lid = listIdForTask(taskId);
    if (lid.isEmpty()) return;
    if (m_demoMode) {
        setStatus(i18n("Demo mode — change not applied"));
        return;
    }
    m_todo->clearTaskReminder(lid, taskId);
}

void App::setTaskTitle(const QString &taskId, const QString &title)
{
    const QString lid = listIdForTask(taskId);
    if (lid.isEmpty() || title.trimmed().isEmpty()) return;
    if (m_demoMode) {
        setStatus(i18n("Demo mode — change not applied"));
        return;
    }
    m_todo->setTaskTitle(lid, taskId, title.trimmed());
}

void App::setTaskBody(const QString &taskId, const QString &body)
{
    const QString lid = listIdForTask(taskId);
    if (lid.isEmpty()) return;
    if (m_demoMode) {
        setStatus(i18n("Demo mode — change not applied"));
        return;
    }
    m_todo->setTaskBody(lid, taskId, body);
}

void App::addChecklistItem(const QString &taskId, const QString &displayName)
{
    const QString lid = listIdForTask(taskId);
    if (lid.isEmpty() || displayName.trimmed().isEmpty()) return;
    if (m_demoMode) {
        setStatus(i18n("Demo mode — change not applied"));
        return;
    }
    m_todo->addChecklistItem(lid, taskId, displayName.trimmed());
}

void App::toggleChecklistItem(const QString &taskId, const QString &itemId)
{
    const QString lid = listIdForTask(taskId);
    if (lid.isEmpty()) return;
    if (m_demoMode) {
        setStatus(i18n("Demo mode — change not applied"));
        return;
    }
    // Look up current state from the open detail task to avoid an extra GET.
    bool currentlyChecked = false;
    const auto items = m_detailTask.value(QStringLiteral("checklistItems")).toList();
    for (const auto &v : items) {
        const auto m = v.toMap();
        if (m.value(QStringLiteral("id")).toString() == itemId) {
            currentlyChecked = m.value(QStringLiteral("isChecked")).toBool();
            break;
        }
    }
    m_todo->setChecklistItemChecked(lid, taskId, itemId, !currentlyChecked);
}

void App::renameChecklistItem(const QString &taskId, const QString &itemId,
                              const QString &displayName)
{
    const QString lid = listIdForTask(taskId);
    if (lid.isEmpty() || displayName.trimmed().isEmpty()) return;
    if (m_demoMode) {
        setStatus(i18n("Demo mode — change not applied"));
        return;
    }
    m_todo->renameChecklistItem(lid, taskId, itemId, displayName.trimmed());
}

void App::deleteChecklistItem(const QString &taskId, const QString &itemId)
{
    const QString lid = listIdForTask(taskId);
    if (lid.isEmpty()) return;
    if (m_demoMode) {
        setStatus(i18n("Demo mode — change not applied"));
        return;
    }
    m_todo->deleteChecklistItem(lid, taskId, itemId);
}

void App::setTaskRecurrencePattern(const QString &taskId, const QString &pattern)
{
    if (pattern.isEmpty()) {
        const QString lid = listIdForTask(taskId);
        if (lid.isEmpty()) return;
        if (m_demoMode) {
            setStatus(i18n("Demo mode — change not applied"));
            return;
        }
        m_todo->setTaskRecurrence(lid, taskId, {});
    } else if (pattern == QLatin1String("daily")) {
        setTaskRecurrence(taskId, QStringLiteral("day"), 1, {});
    } else if (pattern == QLatin1String("weekdays")) {
        setTaskRecurrence(taskId, QStringLiteral("week"), 1,
                          {QStringLiteral("monday"), QStringLiteral("tuesday"),
                           QStringLiteral("wednesday"), QStringLiteral("thursday"),
                           QStringLiteral("friday")});
    } else if (pattern == QLatin1String("weekly")) {
        setTaskRecurrence(taskId, QStringLiteral("week"), 1, {});
    } else if (pattern == QLatin1String("monthly")) {
        setTaskRecurrence(taskId, QStringLiteral("month"), 1, {});
    } else if (pattern == QLatin1String("yearly")) {
        setTaskRecurrence(taskId, QStringLiteral("year"), 1, {});
    }
}

void App::setTaskRecurrence(const QString &taskId, const QString &unit, int interval,
                            const QStringList &days)
{
    const QString lid = listIdForTask(taskId);
    if (lid.isEmpty() || interval < 1) return;
    if (m_demoMode) {
        setStatus(i18n("Demo mode — change not applied"));
        return;
    }

    static const QStringList kWeekdays = {
        QStringLiteral("monday"),   QStringLiteral("tuesday"), QStringLiteral("wednesday"),
        QStringLiteral("thursday"), QStringLiteral("friday"),  QStringLiteral("saturday"),
        QStringLiteral("sunday"),
    };
    const Task task = m_tasksModel->taskById(taskId);
    const QDate anchor
        = task.dueDate.isValid() ? task.dueDate.toLocalTime().date() : QDate::currentDate();

    QJsonObject pattern {{QStringLiteral("interval"), interval}};
    if (unit == QLatin1String("day")) {
        pattern.insert(QStringLiteral("type"), QStringLiteral("daily"));
    } else if (unit == QLatin1String("week")) {
        QStringList valid;
        for (const auto &d : kWeekdays)
            if (days.contains(d)) valid.append(d);
        if (valid.isEmpty()) valid.append(kWeekdays.at(anchor.dayOfWeek() - 1)); // Qt: 1=Mon
        pattern.insert(QStringLiteral("type"), QStringLiteral("weekly"));
        pattern.insert(QStringLiteral("daysOfWeek"), QJsonArray::fromStringList(valid));
        pattern.insert(QStringLiteral("firstDayOfWeek"),
                       kWeekdays.at(QLocale().firstDayOfWeek() - 1));
    } else if (unit == QLatin1String("month")) {
        pattern.insert(QStringLiteral("type"), QStringLiteral("absoluteMonthly"));
        pattern.insert(QStringLiteral("dayOfMonth"), anchor.day());
    } else if (unit == QLatin1String("year")) {
        pattern.insert(QStringLiteral("type"), QStringLiteral("absoluteYearly"));
        pattern.insert(QStringLiteral("dayOfMonth"), anchor.day());
        pattern.insert(QStringLiteral("month"), anchor.month());
    } else {
        return;
    }
    const QJsonObject rec {
        {QStringLiteral("pattern"), pattern},
        {QStringLiteral("range"),
         QJsonObject {
             {QStringLiteral("type"), QStringLiteral("noEnd")},
             {QStringLiteral("startDate"), anchor.toString(Qt::ISODate)},
         }},
    };
    m_todo->setTaskRecurrence(lid, taskId, rec);
}

void App::setTaskCategories(const QString &taskId, const QStringList &categories)
{
    const QString lid = listIdForTask(taskId);
    if (lid.isEmpty()) return;
    if (m_demoMode) {
        setStatus(i18n("Demo mode — change not applied"));
        return;
    }
    QStringList cleaned;
    for (const auto &c : categories) {
        const QString name = c.trimmed();
        if (!name.isEmpty() && !cleaned.contains(name, Qt::CaseInsensitive)) cleaned.append(name);
    }
    m_todo->setTaskCategories(lid, taskId, cleaned);
}

QStringList App::knownCategories() const
{
    QStringList all;
    const auto collect = [&all](const QList<Task> &tasks) {
        for (const auto &t : tasks)
            for (const auto &c : t.categories)
                if (!all.contains(c, Qt::CaseInsensitive)) all.append(c);
    };
    if (m_demoMode) {
        for (const auto &tasks : m_demoTasks)
            collect(tasks);
    } else {
        collect(m_db->allTasks());
    }
    all.sort(Qt::CaseInsensitive);
    return all;
}

void App::addLinkedResource(const QString &taskId, const QString &webUrl,
                            const QString &displayName)
{
    const QString lid = listIdForTask(taskId);
    if (lid.isEmpty() || webUrl.trimmed().isEmpty()) return;
    if (m_demoMode) {
        setStatus(i18n("Demo mode — change not applied"));
        return;
    }
    // applicationName defaults to the URL host (more useful than a fixed
    // "Merkzettel" label when listing alongside Outlook-created links).
    const QUrl url(webUrl.trimmed());
    const QString appName = url.host().isEmpty() ? QStringLiteral("Merkzettel") : url.host();
    const QString name = displayName.trimmed().isEmpty() ? webUrl.trimmed() : displayName.trimmed();
    m_todo->addLinkedResource(lid, taskId, appName, webUrl.trimmed(), name);
}

void App::removeLinkedResource(const QString &taskId, const QString &resourceId)
{
    const QString lid = listIdForTask(taskId);
    if (lid.isEmpty()) return;
    if (m_demoMode) {
        setStatus(i18n("Demo mode — change not applied"));
        return;
    }
    m_todo->removeLinkedResource(lid, taskId, resourceId);
}

QString App::plain(const QString &text)
{
    QString out = text;
    return out.replace(QLatin1Char('<'), QStringLiteral("<\u200B"));
}

void App::openLinkedResource(const QString &webUrl)
{
    if (webUrl.isEmpty()) return;
    // Links may come from shared lists; only hand well-known schemes to the
    // desktop so a crafted link cannot launch arbitrary URL handlers.
    const QUrl url = QUrl::fromUserInput(webUrl);
    static const QStringList kAllowed = {
        QStringLiteral("https"),
        QStringLiteral("http"),
        QStringLiteral("mailto"),
        QStringLiteral("file"),
    };
    if (!url.isValid() || !kAllowed.contains(url.scheme().toLower())) {
        Q_EMIT errorOccurred(i18n("Refusing to open link with unsupported scheme: %1", webUrl));
        return;
    }
    QDesktopServices::openUrl(url);
}

void App::openTaskDetails(int row)
{
    const QVariantMap task = m_tasksModel->taskAt(row);
    setDetailTask(task);
    if (!m_demoMode && task.value(QStringLiteral("hasAttachments")).toBool()) {
        m_todo->fetchAttachments(task.value(QStringLiteral("listId")).toString(),
                                 task.value(QStringLiteral("taskId")).toString());
    }
}

void App::setDetailTask(const QVariantMap &task)
{
    m_detailTask = task;
    const QString id = task.value(QStringLiteral("taskId")).toString();
    if (!id.isEmpty()) {
        QMimeDatabase mimes;
        QVariantList list;
        const auto attachmentList = m_attachments.value(id);
        for (const auto &a : attachmentList) {
            list.append(QVariantMap {
                {QStringLiteral("id"), a.id},
                {QStringLiteral("name"), a.name},
                {QStringLiteral("sizeLabel"), QLocale().formattedDataSize(a.size, 1)},
                {QStringLiteral("iconName"), mimes.mimeTypeForName(a.contentType).iconName()},
            });
        }
        m_detailTask.insert(QStringLiteral("attachments"), list);
    }
    Q_EMIT detailTaskChanged();
}

namespace {
// Attachment names come from the server (possibly a shared list): keep only
// the last path component and never "", "." or "..".
QString safeFileName(const QString &name)
{
    QString base = QFileInfo(name).fileName();
    static const QRegularExpression unsafe(QStringLiteral("[\\x00-\\x1f/\\\\]"));
    base.remove(unsafe);
    if (base.isEmpty() || base == QLatin1String(".") || base == QLatin1String(".."))
        return QStringLiteral("attachment");
    return base;
}
} // namespace

Attachment App::findAttachment(const QString &taskId, const QString &attachmentId) const
{
    const auto attachmentList = m_attachments.value(taskId);
    for (const auto &a : attachmentList) {
        if (a.id == attachmentId) return a;
    }
    return {};
}

void App::addAttachments(const QString &taskId, const QList<QUrl> &fileUrls)
{
    const QString lid = listIdForTask(taskId);
    if (lid.isEmpty() || fileUrls.isEmpty()) return;
    if (m_demoMode) {
        setStatus(i18n("Demo mode — change not applied"));
        return;
    }
    QMimeDatabase mimes;
    int started = 0;
    for (const QUrl &url : fileUrls) {
        QFile file(url.toLocalFile());
        if (!url.isLocalFile() || !file.open(QIODevice::ReadOnly)) {
            Q_EMIT errorOccurred(i18n("Could not read \"%1\".", url.toDisplayString()));
            continue;
        }
        const QString name = QFileInfo(file).fileName();
        if (file.size() > TodoApi::kMaxAttachmentSize) {
            Q_EMIT errorOccurred(
                i18n("\"%1\" is larger than 25 MB, the Microsoft To Do limit.", name));
            continue;
        }
        m_todo->uploadAttachment(lid, taskId, name, mimes.mimeTypeForFile(name).name(),
                                 file.readAll());
        ++started;
    }
    if (started > 0) setStatus(i18np("Uploading %1 file ...", "Uploading %1 files ...", started));
}

void App::openAttachment(const QString &taskId, const QString &attachmentId)
{
    const QString lid = listIdForTask(taskId);
    const Attachment att = findAttachment(taskId, attachmentId);
    if (lid.isEmpty() || att.id.isEmpty()) return;
    if (m_demoMode) {
        setStatus(i18n("Demo mode — files are not available"));
        return;
    }
    setStatus(i18n("Downloading \"%1\" ...", att.name));
    m_todo->downloadAttachment(lid, taskId, attachmentId,
                               [this, att](const QByteArray &data, const QString &err) {
        if (!err.isEmpty()) {
            Q_EMIT errorOccurred(err);
            return;
        }
        // One directory per attachment id keeps the original file name
        // (which the opening application shows) without collisions.
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
            + QStringLiteral("/attachments/")
            + QString(att.id).replace(QLatin1Char('/'), QLatin1Char('_'));
        QDir().mkpath(dir);
        QFile::setPermissions(
            dir, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        QFile out(dir + QLatin1Char('/') + safeFileName(att.name));
        if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size()) {
            Q_EMIT errorOccurred(i18n("Could not write \"%1\".", out.fileName()));
            return;
        }
        out.close();
        QDesktopServices::openUrl(QUrl::fromLocalFile(out.fileName()));
    });
}

void App::saveAttachment(const QString &taskId, const QString &attachmentId, const QUrl &target)
{
    const QString lid = listIdForTask(taskId);
    const Attachment att = findAttachment(taskId, attachmentId);
    if (lid.isEmpty() || att.id.isEmpty() || !target.isLocalFile()) return;
    if (m_demoMode) {
        setStatus(i18n("Demo mode — files are not available"));
        return;
    }
    m_todo->downloadAttachment(lid, taskId, attachmentId,
                               [this, target](const QByteArray &data, const QString &err) {
        if (!err.isEmpty()) {
            Q_EMIT errorOccurred(err);
            return;
        }
        QFile out(target.toLocalFile());
        if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size()) {
            Q_EMIT errorOccurred(i18n("Could not write \"%1\".", target.toLocalFile()));
            return;
        }
        setStatus(i18n("Saved \"%1\"", QFileInfo(out).fileName()));
    });
}

void App::deleteAttachment(const QString &taskId, const QString &attachmentId)
{
    const QString lid = listIdForTask(taskId);
    if (lid.isEmpty()) return;
    if (m_demoMode) {
        setStatus(i18n("Demo mode — change not applied"));
        return;
    }
    m_todo->deleteAttachment(lid, taskId, attachmentId);
}

void App::closeTaskDetails()
{
    m_detailTask.clear();
    Q_EMIT detailTaskChanged();
}

void App::requestPickDateForDue(const QString &taskId)
{
    QDateTime initial = QDateTime::currentDateTime();
    const int n = m_tasksModel->rowCount();
    for (int i = 0; i < n; ++i) {
        const auto idx = m_tasksModel->index(i, 0);
        if (m_tasksModel->data(idx, TasksModel::IdRole).toString() == taskId) {
            const QDateTime existing
                = m_tasksModel->data(idx, TasksModel::DueDateRole).toDateTime();
            if (existing.isValid()) initial = existing;
            break;
        }
    }
    Q_EMIT pickDateRequested(taskId, initial);
}

void App::toggleWindow()
{
    Q_EMIT windowToggleRequested();
}

void App::activateWindow()
{
    Q_EMIT windowActivateRequested();
}

bool App::shareExplainerSeen() const
{
    return KSharedConfig::openConfig()
        ->group(QStringLiteral("Share"))
        .readEntry("explainerSeen", false);
}

void App::setShareExplainerSeen(bool seen)
{
    if (seen == shareExplainerSeen()) return;
    KConfigGroup group = KSharedConfig::openConfig()->group(QStringLiteral("Share"));
    group.writeEntry("explainerSeen", seen);
    group.sync();
    Q_EMIT shareExplainerSeenChanged();
}

void App::selectAdjacentList(int delta)
{
    const int n = m_listsModel->rowCount();
    if (n == 0) return;
    int current = -1;
    for (int i = 0; i < n; ++i) {
        if (m_listsModel->data(m_listsModel->index(i, 0), TaskListsModel::IdRole).toString()
            == m_currentListId) {
            current = i;
            break;
        }
    }
    const int next = current < 0 ? 0 : ((current + delta) % n + n) % n;
    setCurrentListId(
        m_listsModel->data(m_listsModel->index(next, 0), TaskListsModel::IdRole).toString());
}

void App::setColorScheme(const QString &scheme)
{
    if (scheme == m_colorScheme) return;
    if (scheme == QLatin1String("light")) {
        m_colorSchemeManager->activateScheme(
            m_colorSchemeManager->indexForSchemeId(QStringLiteral("BreezeLight")));
    } else if (scheme == QLatin1String("dark")) {
        m_colorSchemeManager->activateScheme(
            m_colorSchemeManager->indexForSchemeId(QStringLiteral("BreezeDark")));
    } else {
        // Anything else (incl. "auto") = follow system. An invalid model index
        // resets the manager to "no override".
        m_colorSchemeManager->activateScheme(QModelIndex());
    }
    m_colorScheme = scheme;
    Q_EMIT colorSchemeChanged();
}

void App::checkReminders()
{
    if (m_demoMode) return;
    const auto hits = m_db->reminderHitsBefore(QDateTime::currentDateTimeUtc());
    for (const auto &h : hits) {
        const QString key
            = h.taskId + QLatin1Char('|') + h.reminderUtc.toUTC().toString(Qt::ISODate);
        if (m_firedReminders.contains(key)) continue;
        m_firedReminders.insert(key);

        auto *notif = new KNotification(QStringLiteral("reminder"), KNotification::CloseOnTimeout);
        notif->setTitle(i18n("Reminder"));
        // Notification bodies may contain markup; show the title verbatim.
        notif->setText(h.title.toHtmlEscaped());
        notif->setIconName(QStringLiteral("task-reminder"));

        // "Mark done" closes the task right from the notification — single
        // most useful action here. Show window stays the implicit fallback
        // when the user clicks the notification body.
        const QString listId = h.listId;
        const QString taskId = h.taskId;
        auto *doneAction = notif->addAction(i18n("Mark done"));
        connect(doneAction, &KNotificationAction::activated, this, [this, listId, taskId] {
            if (canSync()) m_todo->setTaskStatus(listId, taskId, QStringLiteral("completed"));
        });

        notif->sendEvent();
    }
}

void App::loadDemoData()
{
    QList<TaskList> lists = {
        {QStringLiteral("demo-tasks"), i18n("Tasks"), true},
        {QStringLiteral("demo-work"), i18n("Work"), false},
        {QStringLiteral("demo-home"), i18n("Home"), false},
        {QStringLiteral("demo-shopping"), i18n("Shopping"), false},
        {QStringLiteral("demo-learning"), i18n("Learning"), false},
    };
    m_listsModel->setLists(lists);
    Q_EMIT currentListNameChanged();
    pushListNamesToModel();

    const QDate today = QDate::currentDate();
    auto due
        = [](const QDate &d, int hour = 9) { return QDateTime(d, QTime(hour, 0), QTimeZone::UTC); };
    auto mk = [](const char *id, const QString &title, const char *status, const char *importance,
                 const QString &body, QDateTime d, QDateTime reminder = {}) {
        Task t;
        t.id = QString::fromLatin1(id);
        t.title = title;
        t.status = QString::fromLatin1(status);
        t.importance = QString::fromLatin1(importance);
        t.body = body;
        t.dueDate = std::move(d);
        if (reminder.isValid()) {
            t.reminderDate = std::move(reminder);
            t.hasReminder = true;
        }
        t.lastModified = QDateTime::currentDateTimeUtc();
        return t;
    };
    auto withItems = [](Task t, std::initializer_list<std::pair<const char *, bool>> items) {
        int i = 0;
        for (const auto &[name, checked] : items) {
            ChecklistItem c;
            c.id = QStringLiteral("%1-c%2").arg(t.id).arg(i++);
            c.displayName = QString::fromUtf8(name);
            c.isChecked = checked;
            t.checklistItems.append(c);
        }
        t.totalChecklistCount = t.checklistItems.size();
        for (const auto &c : std::as_const(t.checklistItems))
            if (!c.isChecked) ++t.openChecklistCount;
        return t;
    };

    m_demoTasks[QStringLiteral("demo-tasks")] = {
        mk("d1", i18n("Pay electricity bill"), "notStarted", "normal", QString(),
           due(today.addDays(-2))),
        mk("d2", i18n("Reply to Anna about the proposal"), "notStarted", "high", QString(),
           due(today)),
        withItems(mk("d3", i18n("Buy groceries"), "notStarted", "normal",
                     i18n("milk, bread, coffee, oranges"), due(today)),
                  {
                      {"Milk", true},
                      {"Bread", true},
                      {"Coffee", false},
                      {"Oranges", false},
                  }),
        mk("d4", i18n("Team standup"), "notStarted", "normal", QString(), due(today.addDays(1)),
           due(today.addDays(1), 9)),
        mk("d5", i18n("Pick up parcel from the post office"), "notStarted", "normal", QString(),
           due(today.addDays(1))),
        mk("d6", i18n("Submit travel expenses"), "notStarted", "normal", QString(),
           due(today.addDays(3))),
        mk("d7", i18n("Read 'The Pragmatic Programmer' ch.4"), "notStarted", "normal",
           i18n("Focus on the chapter on orthogonality."), due(today.addDays(4))),
        withItems(mk("d8", i18n("Plan summer holiday"), "notStarted", "normal", QString(),
                     due(today.addDays(45))),
                  {
                      {"Pick destination", true},
                      {"Book flights", false},
                      {"Reserve hotel", false},
                      {"Buy travel insurance", false},
                  }),
        mk("d9", i18n("Learn Rust"), "notStarted", "high", i18n("Start with the official book."),
           QDateTime()),
        mk("d10", i18n("Refactor printer driver"), "notStarted", "normal", QString(), QDateTime()),
        mk("d11", i18n("Update CV"), "completed", "normal", QString(), due(today.addDays(-5))),
        mk("d12", i18n("Renew library card"), "completed", "normal", QString(),
           due(today.addDays(-7))),
    };

    m_demoTasks[QStringLiteral("demo-work")] = {
        withItems(mk("w1", i18n("Quarterly report"), "notStarted", "high", QString(),
                     due(today.addDays(2))),
                  {
                      {"Pull Q1 numbers", true},
                      {"Draft executive summary", false},
                      {"Review with manager", false},
                  }),
        mk("w2", i18n("Code review for Markus"), "notStarted", "normal", QString(),
           due(today.addDays(1))),
        mk("w3", i18n("1:1 with manager"), "notStarted", "normal", QString(),
           due(today.addDays(7), 14)),
    };

    m_demoTasks[QStringLiteral("demo-home")] = {
        mk("h1", i18n("Water the plants"), "notStarted", "normal", QString(), due(today)),
        mk("h2", i18n("Fix the bathroom faucet"), "notStarted", "normal", QString(), QDateTime()),
    };

    m_demoTasks[QStringLiteral("demo-shopping")] = {
        mk("s1", i18n("New running shoes"), "notStarted", "normal", QString(), QDateTime()),
        mk("s2", i18n("Birthday present for Mum"), "notStarted", "high", QString(),
           due(today.addDays(10))),
    };

    m_demoTasks[QStringLiteral("demo-learning")] = {
        mk("l1", i18n("Finish KDE Frameworks 6 tutorial"), "notStarted", "normal", QString(),
           QDateTime()),
        mk("l2", i18n("Watch Qt 6.6 release video"), "completed", "normal", QString(), QDateTime()),
    };

    // A few categories so the chips and the category search show up.
    const QHash<QString, QStringList> demoCategories = {
        {QStringLiteral("d1"), {i18n("Finance")}},
        {QStringLiteral("d6"), {i18n("Finance"), i18n("Work")}},
        {QStringLiteral("w1"), {i18n("Work")}},
        {QStringLiteral("s2"), {i18n("Family")}},
    };
    for (auto &tasks : m_demoTasks)
        for (auto &t : tasks)
            t.categories = demoCategories.value(t.id);

    m_demoMyDay = {QStringLiteral("d2"), QStringLiteral("d3"), QStringLiteral("w1")};

    // Fake attachment metadata so the Files section can be shown.
    m_attachments.insert(
        QStringLiteral("d6"),
        {
            {QStringLiteral("a1"), i18n("Hotel receipt.pdf"), QStringLiteral("application/pdf"),
             245760},
            {QStringLiteral("a2"), i18n("Train ticket.png"), QStringLiteral("image/png"), 1258291},
        });
    for (auto &t : m_demoTasks[QStringLiteral("demo-tasks")])
        if (t.id == QLatin1String("d6")) t.hasAttachments = true;

    setCurrentListId(QStringLiteral("demo-tasks"));
}

void App::switchDemoList(const QString &id)
{
    if (TaskListsModel::isVirtualId(id)) {
        QList<Task> all;
        for (auto it = m_demoTasks.constBegin(); it != m_demoTasks.constEnd(); ++it) {
            for (Task t : it.value()) {
                t.listId = it.key();
                all.append(std::move(t));
            }
        }
        m_tasksModel->setTasks(all);
        return;
    }
    QList<Task> tasks = m_demoTasks.value(id);
    for (auto &t : tasks)
        t.listId = id;
    m_tasksModel->setTasks(tasks);
}

void App::onAuthenticated()
{
    setOnline(true);
    m_authRetryTimer->stop();
    Q_EMIT loggedInChanged();
    setStatus(i18n("Signed in, loading lists ..."));
    m_todo->fetchLists();
}

void App::onAuthError(const QString &message)
{
    setStatus(i18n("Sign-in error: %1", message));
    Q_EMIT errorOccurred(message);
}

void App::setStatus(const QString &status)
{
    if (m_status == status) return;
    m_status = status;
    Q_EMIT statusChanged();
}

} // namespace Merkzettel
