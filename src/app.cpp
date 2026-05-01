#include "app.h"

#include "auth/authmanager.h"
#include "graph/graphclient.h"
#include "graph/todoapi.h"
#include "cache/database.h"
#include "tray/trayicon.h"
#include "models/tasksmodel.h"
#include "models/tasklistsmodel.h"

#include <KLocalizedString>
#include <QApplication>
#include <QDate>
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
        }
        m_tray->setOpenCount(m_db->openTaskCountForToday());
        setStatus(i18n("%1 tasks", tasks.size()));
    });

    connect(m_todo.get(), &TodoApi::taskMutated, this, [this](const QString &listId) {
        m_todo->fetchTasks(listId);
    });

    connect(m_todo.get(), &TodoApi::errorOccurred, this, [this](const QString &msg) {
        setStatus(i18n("Error: %1", msg));
        Q_EMIT errorOccurred(msg);
    });

    connect(m_tray.get(), &TrayIcon::activated, this, &App::toggleWindow);
    connect(m_tray.get(), &TrayIcon::quitRequested, this, [] { qApp->quit(); });
    connect(m_tray.get(), &TrayIcon::syncRequested, this, &App::refresh);
}

App::~App() = default;

void App::start()
{
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

    m_auth->start();
    Q_EMIT loggedInChanged();
}

bool App::loggedIn() const { return m_auth->isAuthenticated(); }
QString App::status() const { return m_status; }
QString App::currentListId() const { return m_currentListId; }

TasksModel *App::tasksModel() const { return m_tasksModel; }
TaskListsModel *App::listsModel() const { return m_listsModel; }

void App::setCurrentListId(const QString &id)
{
    if (m_currentListId == id) return;
    m_currentListId = id;
    Q_EMIT currentListIdChanged();

    const auto cachedTasks = m_db->tasks(id);
    m_tasksModel->setTasks(cachedTasks);

    if (loggedIn()) {
        m_todo->fetchTasks(id);
    }
}

void App::login() { m_auth->login(); }
void App::logout() { m_auth->logout(); }

void App::refresh()
{
    if (!loggedIn()) return;
    setStatus(i18n("Synchronizing ..."));
    m_todo->fetchLists();
    if (!m_currentListId.isEmpty()) {
        m_todo->fetchTasks(m_currentListId);
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
    const auto parsed = parseAddInput(input.trimmed());
    m_todo->addTask(m_currentListId, parsed.first, parsed.second);
}

void App::completeTask(const QString &taskId)
{
    if (m_currentListId.isEmpty()) return;
    m_todo->setTaskStatus(m_currentListId, taskId, QStringLiteral("completed"));
}

void App::uncompleteTask(const QString &taskId)
{
    if (m_currentListId.isEmpty()) return;
    m_todo->setTaskStatus(m_currentListId, taskId, QStringLiteral("notStarted"));
}

void App::deleteTask(const QString &taskId)
{
    if (m_currentListId.isEmpty()) return;
    m_todo->deleteTask(m_currentListId, taskId);
}

void App::toggleImportance(const QString &taskId)
{
    if (m_currentListId.isEmpty()) return;
    // Find current importance from model.
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
    m_todo->setTaskDueDate(m_currentListId, taskId, due);
}

void App::clearTaskDueDate(const QString &taskId)
{
    if (m_currentListId.isEmpty()) return;
    m_todo->setTaskDueDate(m_currentListId, taskId, {});
}

void App::setTaskReminder(const QString &taskId, const QDateTime &when)
{
    if (m_currentListId.isEmpty() || !when.isValid()) return;
    m_todo->setTaskReminder(m_currentListId, taskId, when);
}

void App::clearTaskReminder(const QString &taskId)
{
    if (m_currentListId.isEmpty()) return;
    m_todo->clearTaskReminder(m_currentListId, taskId);
}

void App::setTaskTitle(const QString &taskId, const QString &title)
{
    if (m_currentListId.isEmpty() || title.trimmed().isEmpty()) return;
    m_todo->setTaskTitle(m_currentListId, taskId, title.trimmed());
}

void App::setTaskBody(const QString &taskId, const QString &body)
{
    if (m_currentListId.isEmpty()) return;
    m_todo->setTaskBody(m_currentListId, taskId, body);
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
