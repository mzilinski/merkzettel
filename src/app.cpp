#include "app.h"

#include "auth/authmanager.h"
#include "graph/graphclient.h"
#include "graph/todoapi.h"
#include "cache/database.h"
#include "tray/trayicon.h"
#include "models/tasksmodel.h"
#include "models/tasklistsmodel.h"

#include <KLocalizedString>
#include <KNotification>
#include <QApplication>
#include <QDate>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QTimeZone>
#include <QRegularExpression>
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
{
    connect(m_auth.get(), &AuthManager::authenticated, this, &App::onAuthenticated);
    connect(m_auth.get(), &AuthManager::authError, this, &App::onAuthError);
    connect(m_auth.get(), &AuthManager::loggedOut, this, [this] {
        m_tasksModel->clear();
        m_listsModel->clear();
        Q_EMIT loggedInChanged();
        setStatus(i18n("Signed out"));
    });

    connect(m_todo.get(), &TodoApi::listsReceived, this, [this](const QList<TaskList> &lists) {
        m_db->upsertLists(lists);
        m_listsModel->setLists(lists);
        if (m_currentListId.isEmpty() && !lists.isEmpty()) {
            setCurrentListId(lists.first().id);
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
            // If the detail sheet is open, refresh its task snapshot from the
            // new model so checklist toggles, body edits etc. are visible.
            const QString openId = m_detailTask.value(QStringLiteral("taskId")).toString();
            if (!openId.isEmpty()) {
                const int n = m_tasksModel->rowCount();
                for (int i = 0; i < n; ++i) {
                    const auto idx = m_tasksModel->index(i, 0);
                    if (m_tasksModel->data(idx, TasksModel::IdRole).toString() == openId) {
                        m_detailTask = m_tasksModel->taskAt(i);
                        Q_EMIT detailTaskChanged();
                        break;
                    }
                }
            }
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

    connect(m_todo.get(), &TodoApi::taskMutated, this, [this](const QString &listId) {
        m_todo->fetchTasks(listId);
    });

    connect(m_todo.get(), &TodoApi::checklistItemMutated, this,
            [this](const QString &listId, const QString &) {
        m_todo->fetchTasks(listId);
    });

    connect(m_todo.get(), &TodoApi::tasksDelta, this,
            [this](const QString &listId, const QList<Task> &changed,
                   const QStringList &deletedIds, const QString &newDeltaLink) {
        m_db->applyTaskDelta(listId, changed, deletedIds);
        if (!newDeltaLink.isEmpty()) {
            m_db->setDeltaLink(listId, newDeltaLink);
        }
        // Delta payloads carry no checklist items ($expand isn't supported on
        // /tasks/delta). Refetch them per changed task so the cache stays in
        // sync; small N typically (0-3 changes per sync).
        for (const auto &t : changed) {
            m_todo->fetchChecklistItems(listId, t.id);
        }
        if (listId == m_currentListId) {
            const auto cached = m_db->tasks(listId);
            m_tasksModel->setTasks(cached);
        }
        m_tray->setOpenCount(m_db->openTaskCountForToday());
        setStatus(i18n("Synced — %1 changed, %2 removed",
                       changed.size(), deletedIds.size()));
    });

    connect(m_todo.get(), &TodoApi::tasksDeltaExpired, this,
            [this](const QString &listId) {
        // Token expired (>30 days, server-side replay limit, etc). Drop the
        // link and fall back to a full fetch — that path will bootstrap a
        // fresh delta token afterwards.
        m_db->clearDeltaLink(listId);
        m_todo->fetchTasks(listId);
    });

    connect(m_todo.get(), &TodoApi::checklistItemsReceived, this,
            [this](const QString &listId, const QString &taskId,
                   const QList<ChecklistItem> &items) {
        m_db->upsertChecklistItems(listId, taskId, items);
        if (listId == m_currentListId) {
            const auto cached = m_db->tasks(listId);
            m_tasksModel->setTasks(cached);
            // Re-emit detailTask if this task is open so subtasks UI updates.
            if (m_detailTask.value(QStringLiteral("taskId")).toString() == taskId) {
                const int n = m_tasksModel->rowCount();
                for (int i = 0; i < n; ++i) {
                    const auto idx = m_tasksModel->index(i, 0);
                    if (m_tasksModel->data(idx, TasksModel::IdRole).toString() == taskId) {
                        m_detailTask = m_tasksModel->taskAt(i);
                        Q_EMIT detailTaskChanged();
                        break;
                    }
                }
            }
        }
    });

    connect(m_todo.get(), &TodoApi::listMutated, this, [this] {
        m_todo->fetchLists();
    });

    connect(m_todo.get(), &TodoApi::errorOccurred, this, [this](const QString &msg) {
        setStatus(i18n("Error: %1", msg));
        Q_EMIT errorOccurred(msg);
    });

    connect(m_tray.get(), &TrayIcon::activated, this, &App::toggleWindow);
    connect(m_tray.get(), &TrayIcon::quitRequested, this, [] { qApp->quit(); });
    connect(m_tray.get(), &TrayIcon::syncRequested, this, &App::refresh);

    m_reminderTimer = new QTimer(this);
    m_reminderTimer->setInterval(60 * 1000);  // 1 minute
    connect(m_reminderTimer, &QTimer::timeout, this, &App::checkReminders);
}

App::~App() = default;

void App::start()
{
    if (m_demoMode) {
        loadDemoData();
        m_tray->show();
        m_tray->setOpenCount(0);
        Q_EMIT loggedInChanged();
        setStatus(i18n("Demo mode — no real data, all changes are local"));
        return;
    }

    m_db->open();
    m_tray->show();

    const auto cachedLists = m_db->lists();
    if (!cachedLists.isEmpty()) {
        m_listsModel->setLists(cachedLists);
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

bool App::loggedIn() const { return m_demoMode || m_auth->isAuthenticated(); }
QString App::status() const { return m_status; }
QString App::currentListId() const { return m_currentListId; }

TasksModel *App::tasksModel() const { return m_tasksModel; }
TaskListsModel *App::listsModel() const { return m_listsModel; }

void App::setCurrentListId(const QString &id)
{
    if (m_currentListId == id) return;
    m_currentListId = id;
    Q_EMIT currentListIdChanged();

    if (m_demoMode) {
        switchDemoList(id);
        return;
    }

    const auto cachedTasks = m_db->tasks(id);
    m_tasksModel->setTasks(cachedTasks);

    if (loggedIn()) {
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

void App::login() { m_auth->login(); }
void App::logout() { m_auth->logout(); }

void App::refresh()
{
    if (m_demoMode) {
        setStatus(i18n("Demo mode — nothing to sync"));
        return;
    }
    if (!loggedIn()) return;
    setStatus(i18n("Synchronizing ..."));
    m_todo->fetchLists();
    if (!m_currentListId.isEmpty()) {
        const QString delta = m_db->deltaLink(m_currentListId);
        if (!delta.isEmpty()) {
            m_todo->syncTasks(m_currentListId, delta);
        } else {
            m_todo->fetchTasks(m_currentListId);
        }
    }
}

QPair<QString, QDateTime> App::parseAddInput(const QString &raw) const
{
    // Parse a trailing date keyword off the end of the title.
    // Recognized (case-insensitive, last whitespace-separated token):
    //   heute, morgen, uebermorgen, mo/di/mi/do/fr/sa/so, 25.5., 25.5.2026
    // Returns {title-without-keyword, due-or-invalid}.
    static const QMap<QString, int> dayOffsets = {
        {QStringLiteral("heute"), 0},
        {QStringLiteral("morgen"), 1},
        {QStringLiteral("uebermorgen"), 2},
        {QStringLiteral("uebermrgn"), 2},
    };
    static const QMap<QString, int> weekdays = {
        {QStringLiteral("mo"), 1}, {QStringLiteral("di"), 2}, {QStringLiteral("mi"), 3},
        {QStringLiteral("do"), 4}, {QStringLiteral("fr"), 5}, {QStringLiteral("sa"), 6},
        {QStringLiteral("so"), 7},
    };
    static const QRegularExpression dateRe(
        QStringLiteral(R"(^(\d{1,2})\.(\d{1,2})\.(\d{2,4})?$)"));

    const int sep = raw.lastIndexOf(QChar::Space);
    if (sep <= 0) return {raw.trimmed(), QDateTime()};

    const QString trailing = raw.mid(sep + 1).trimmed().toLower();
    const QString head = raw.left(sep).trimmed();
    const QDate today = QDate::currentDate();
    QDate target;

    if (dayOffsets.contains(trailing)) {
        target = today.addDays(dayOffsets.value(trailing));
    } else if (weekdays.contains(trailing)) {
        const int wd = weekdays.value(trailing);
        int offset = wd - today.dayOfWeek();
        if (offset <= 0) offset += 7;  // always upcoming, never past
        target = today.addDays(offset);
    } else {
        const auto match = dateRe.match(trailing);
        if (match.hasMatch()) {
            int day = match.captured(1).toInt();
            int month = match.captured(2).toInt();
            int year = match.captured(3).isEmpty() ? today.year() : match.captured(3).toInt();
            if (year < 100) year += 2000;
            target = QDate(year, month, day);
        }
    }

    if (target.isValid() && !head.isEmpty()) {
        return {head, QDateTime(target, QTime(9, 0), QTimeZone::LocalTime)};
    }
    return {raw.trimmed(), QDateTime()};
}

void App::addTask(const QString &input)
{
    if (m_currentListId.isEmpty() || input.trimmed().isEmpty()) return;
    if (m_demoMode) { setStatus(i18n("Demo mode — change not applied")); return; }
    const auto parsed = parseAddInput(input.trimmed());
    m_todo->addTask(m_currentListId, parsed.first, parsed.second);
}

void App::completeTask(const QString &taskId)
{
    if (m_currentListId.isEmpty()) return;
    if (m_demoMode) { setStatus(i18n("Demo mode — change not applied")); return; }
    m_todo->setTaskStatus(m_currentListId, taskId, QStringLiteral("completed"));
}

void App::uncompleteTask(const QString &taskId)
{
    if (m_currentListId.isEmpty()) return;
    if (m_demoMode) { setStatus(i18n("Demo mode — change not applied")); return; }
    m_todo->setTaskStatus(m_currentListId, taskId, QStringLiteral("notStarted"));
}

void App::deleteTask(const QString &taskId)
{
    if (m_currentListId.isEmpty()) return;
    if (m_demoMode) { setStatus(i18n("Demo mode — change not applied")); return; }
    m_todo->deleteTask(m_currentListId, taskId);
}

void App::deleteList(const QString &listId)
{
    if (listId.isEmpty()) return;
    if (m_demoMode) { setStatus(i18n("Demo mode — change not applied")); return; }
    m_todo->deleteList(listId);
}

void App::renameList(const QString &listId, const QString &newName)
{
    if (listId.isEmpty() || newName.trimmed().isEmpty()) return;
    if (m_demoMode) { setStatus(i18n("Demo mode — change not applied")); return; }
    m_todo->renameList(listId, newName.trimmed());
}

void App::toggleImportance(const QString &taskId)
{
    if (m_currentListId.isEmpty()) return;
    if (m_demoMode) { setStatus(i18n("Demo mode — change not applied")); return; }
    const int n = m_tasksModel->rowCount();
    QString current = QStringLiteral("normal");
    for (int i = 0; i < n; ++i) {
        const auto idx = m_tasksModel->index(i, 0);
        if (m_tasksModel->data(idx, TasksModel::IdRole).toString() == taskId) {
            current = m_tasksModel->data(idx, TasksModel::ImportanceRole).toString();
            break;
        }
    }
    const QString next = (current == QLatin1String("high")) ? QStringLiteral("normal")
                                                            : QStringLiteral("high");
    m_todo->setTaskImportance(m_currentListId, taskId, next);
}

void App::setTaskDueDate(const QString &taskId, const QDateTime &due)
{
    if (m_currentListId.isEmpty()) return;
    if (m_demoMode) { setStatus(i18n("Demo mode — change not applied")); return; }
    m_todo->setTaskDueDate(m_currentListId, taskId, due);
}

void App::clearTaskDueDate(const QString &taskId)
{
    if (m_currentListId.isEmpty()) return;
    if (m_demoMode) { setStatus(i18n("Demo mode — change not applied")); return; }
    m_todo->setTaskDueDate(m_currentListId, taskId, {});
}

void App::setTaskReminder(const QString &taskId, const QDateTime &when)
{
    if (m_currentListId.isEmpty() || !when.isValid()) return;
    if (m_demoMode) { setStatus(i18n("Demo mode — change not applied")); return; }
    m_todo->setTaskReminder(m_currentListId, taskId, when);
}

void App::clearTaskReminder(const QString &taskId)
{
    if (m_currentListId.isEmpty()) return;
    if (m_demoMode) { setStatus(i18n("Demo mode — change not applied")); return; }
    m_todo->clearTaskReminder(m_currentListId, taskId);
}

void App::setTaskTitle(const QString &taskId, const QString &title)
{
    if (m_currentListId.isEmpty() || title.trimmed().isEmpty()) return;
    if (m_demoMode) { setStatus(i18n("Demo mode — change not applied")); return; }
    m_todo->setTaskTitle(m_currentListId, taskId, title.trimmed());
}

void App::setTaskBody(const QString &taskId, const QString &body)
{
    if (m_currentListId.isEmpty()) return;
    if (m_demoMode) { setStatus(i18n("Demo mode — change not applied")); return; }
    m_todo->setTaskBody(m_currentListId, taskId, body);
}

void App::addChecklistItem(const QString &taskId, const QString &displayName)
{
    if (m_currentListId.isEmpty() || displayName.trimmed().isEmpty()) return;
    if (m_demoMode) { setStatus(i18n("Demo mode — change not applied")); return; }
    m_todo->addChecklistItem(m_currentListId, taskId, displayName.trimmed());
}

void App::toggleChecklistItem(const QString &taskId, const QString &itemId)
{
    if (m_currentListId.isEmpty()) return;
    if (m_demoMode) { setStatus(i18n("Demo mode — change not applied")); return; }
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
    m_todo->setChecklistItemChecked(m_currentListId, taskId, itemId, !currentlyChecked);
}

void App::renameChecklistItem(const QString &taskId, const QString &itemId,
                              const QString &displayName)
{
    if (m_currentListId.isEmpty() || displayName.trimmed().isEmpty()) return;
    if (m_demoMode) { setStatus(i18n("Demo mode — change not applied")); return; }
    m_todo->renameChecklistItem(m_currentListId, taskId, itemId, displayName.trimmed());
}

void App::deleteChecklistItem(const QString &taskId, const QString &itemId)
{
    if (m_currentListId.isEmpty()) return;
    if (m_demoMode) { setStatus(i18n("Demo mode — change not applied")); return; }
    m_todo->deleteChecklistItem(m_currentListId, taskId, itemId);
}

void App::setTaskRecurrencePattern(const QString &taskId, const QString &pattern)
{
    if (m_currentListId.isEmpty()) return;
    if (m_demoMode) { setStatus(i18n("Demo mode — change not applied")); return; }

    QJsonObject rec;
    if (pattern == QLatin1String("daily")) {
        rec.insert(QStringLiteral("pattern"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("daily")},
            {QStringLiteral("interval"), 1},
        });
    } else if (pattern == QLatin1String("weekly")) {
        // Default to today's weekday so the recurrence anchors on a sensible day.
        const QStringList weekdayNames = {
            QStringLiteral("monday"), QStringLiteral("tuesday"), QStringLiteral("wednesday"),
            QStringLiteral("thursday"), QStringLiteral("friday"), QStringLiteral("saturday"),
            QStringLiteral("sunday"),
        };
        const int idx = QDate::currentDate().dayOfWeek() - 1;  // Qt: 1=Mon..7=Sun
        rec.insert(QStringLiteral("pattern"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("weekly")},
            {QStringLiteral("interval"), 1},
            {QStringLiteral("daysOfWeek"), QJsonArray{weekdayNames.at(idx)}},
        });
    } else if (pattern == QLatin1String("monthly")) {
        rec.insert(QStringLiteral("pattern"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("absoluteMonthly")},
            {QStringLiteral("interval"), 1},
            {QStringLiteral("dayOfMonth"), QDate::currentDate().day()},
        });
    } else if (pattern == QLatin1String("yearly")) {
        rec.insert(QStringLiteral("pattern"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("absoluteYearly")},
            {QStringLiteral("interval"), 1},
            {QStringLiteral("month"), QDate::currentDate().month()},
            {QStringLiteral("dayOfMonth"), QDate::currentDate().day()},
        });
    } else if (!pattern.isEmpty()) {
        return;  // Unknown — refuse silently.
    }
    if (!rec.isEmpty()) {
        rec.insert(QStringLiteral("range"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("noEnd")},
        });
    }
    m_todo->setTaskRecurrence(m_currentListId, taskId, rec);
}

void App::openTaskDetails(int row)
{
    m_detailTask = m_tasksModel->taskAt(row);
    Q_EMIT detailTaskChanged();
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
            const QDateTime existing = m_tasksModel->data(idx, TasksModel::DueDateRole).toDateTime();
            if (existing.isValid()) initial = existing;
            break;
        }
    }
    Q_EMIT pickDateRequested(taskId, initial);
}

void App::toggleWindow() { Q_EMIT windowToggleRequested(); }

void App::checkReminders()
{
    if (m_demoMode) return;
    const auto hits = m_db->reminderHitsBefore(QDateTime::currentDateTimeUtc());
    for (const auto &h : hits) {
        const QString key = h.taskId + QLatin1Char('|')
                            + h.reminderUtc.toUTC().toString(Qt::ISODate);
        if (m_firedReminders.contains(key)) continue;
        m_firedReminders.insert(key);

        auto *notif = new KNotification(QStringLiteral("reminder"),
                                        KNotification::CloseOnTimeout);
        notif->setTitle(i18n("Reminder"));
        notif->setText(h.title);
        notif->setIconName(QStringLiteral("appointment-soon"));

        // "Mark done" closes the task right from the notification — single
        // most useful action here. Show window stays the implicit fallback
        // when the user clicks the notification body.
        const QString listId = h.listId;
        const QString taskId = h.taskId;
        auto *doneAction = notif->addAction(i18n("Mark done"));
        connect(doneAction, &KNotificationAction::activated, this, [this, listId, taskId] {
            if (loggedIn()) m_todo->setTaskStatus(listId, taskId, QStringLiteral("completed"));
        });

        notif->sendEvent();
    }
}

void App::loadDemoData()
{
    QList<TaskList> lists = {
        {QStringLiteral("demo-tasks"),    i18n("Tasks"),    true},
        {QStringLiteral("demo-work"),     i18n("Work"),     false},
        {QStringLiteral("demo-home"),     i18n("Home"),     false},
        {QStringLiteral("demo-shopping"), i18n("Shopping"), false},
        {QStringLiteral("demo-learning"), i18n("Learning"), false},
    };
    m_listsModel->setLists(lists);

    const QDate today = QDate::currentDate();
    auto due = [](const QDate &d, int hour = 9) {
        return QDateTime(d, QTime(hour, 0), QTimeZone::UTC);
    };
    auto mk = [](const char *id, const QString &title, const char *status,
                 const char *importance, const QString &body,
                 QDateTime d, QDateTime reminder = {}) {
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
        for (const auto &c : t.checklistItems) if (!c.isChecked) ++t.openChecklistCount;
        return t;
    };

    m_demoTasks[QStringLiteral("demo-tasks")] = {
        mk("d1",  i18n("Pay electricity bill"),                "notStarted", "normal", QString(),
           due(today.addDays(-2))),
        mk("d2",  i18n("Reply to Anna about the proposal"),    "notStarted", "high",   QString(),
           due(today)),
        withItems(mk("d3",  i18n("Buy groceries"),              "notStarted", "normal",
           i18n("milk, bread, coffee, oranges"),
           due(today)), {
               {"Milk", true},
               {"Bread", true},
               {"Coffee", false},
               {"Oranges", false},
           }),
        mk("d4",  i18n("Team standup"),                        "notStarted", "normal", QString(),
           due(today.addDays(1)), due(today.addDays(1), 9)),
        mk("d5",  i18n("Pick up parcel from the post office"), "notStarted", "normal", QString(),
           due(today.addDays(1))),
        mk("d6",  i18n("Submit travel expenses"),              "notStarted", "normal", QString(),
           due(today.addDays(3))),
        mk("d7",  i18n("Read 'The Pragmatic Programmer' ch.4"),"notStarted", "normal",
           i18n("Focus on the chapter on orthogonality."),
           due(today.addDays(4))),
        withItems(mk("d8",  i18n("Plan summer holiday"),       "notStarted", "normal", QString(),
           due(today.addDays(45))), {
               {"Pick destination", true},
               {"Book flights", false},
               {"Reserve hotel", false},
               {"Buy travel insurance", false},
           }),
        mk("d9",  i18n("Learn Rust"),                          "notStarted", "high",
           i18n("Start with the official book."),
           QDateTime()),
        mk("d10", i18n("Refactor printer driver"),             "notStarted", "normal", QString(),
           QDateTime()),
        mk("d11", i18n("Update CV"),                           "completed",  "normal", QString(),
           due(today.addDays(-5))),
        mk("d12", i18n("Renew library card"),                  "completed",  "normal", QString(),
           due(today.addDays(-7))),
    };

    m_demoTasks[QStringLiteral("demo-work")] = {
        withItems(mk("w1", i18n("Quarterly report"), "notStarted", "high", QString(), due(today.addDays(2))), {
            {"Pull Q1 numbers", true},
            {"Draft executive summary", false},
            {"Review with manager", false},
        }),
        mk("w2", i18n("Code review for Markus"),  "notStarted", "normal", QString(), due(today.addDays(1))),
        mk("w3", i18n("1:1 with manager"),        "notStarted", "normal", QString(), due(today.addDays(7), 14)),
    };

    m_demoTasks[QStringLiteral("demo-home")] = {
        mk("h1", i18n("Water the plants"),        "notStarted", "normal", QString(), due(today)),
        mk("h2", i18n("Fix the bathroom faucet"), "notStarted", "normal", QString(), QDateTime()),
    };

    m_demoTasks[QStringLiteral("demo-shopping")] = {
        mk("s1", i18n("New running shoes"),       "notStarted", "normal", QString(), QDateTime()),
        mk("s2", i18n("Birthday present for Mum"),"notStarted", "high",   QString(), due(today.addDays(10))),
    };

    m_demoTasks[QStringLiteral("demo-learning")] = {
        mk("l1", i18n("Finish KDE Frameworks 6 tutorial"),  "notStarted", "normal", QString(), QDateTime()),
        mk("l2", i18n("Watch Qt 6.6 release video"),        "completed",  "normal", QString(), QDateTime()),
    };

    setCurrentListId(QStringLiteral("demo-tasks"));
}

void App::switchDemoList(const QString &id)
{
    m_tasksModel->setTasks(m_demoTasks.value(id));
}

void App::onAuthenticated()
{
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
