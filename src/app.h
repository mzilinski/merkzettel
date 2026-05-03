#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QVariantMap>
#include <QSet>
#include <memory>

#include "models/tasksmodel.h"
#include "models/tasklistsmodel.h"

class QTimer;
class KColorSchemeManager;

namespace Merkzettel {

class AuthManager;
class GraphClient;
class TodoApi;
class Database;
class TrayIcon;

class App : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY loggedInChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString currentListId READ currentListId WRITE setCurrentListId NOTIFY currentListIdChanged)
    Q_PROPERTY(TasksModel *tasksModel READ tasksModel CONSTANT)
    Q_PROPERTY(TaskListsModel *listsModel READ listsModel CONSTANT)
    Q_PROPERTY(QVariantMap detailTask READ detailTask NOTIFY detailTaskChanged)
    Q_PROPERTY(bool startMinimized READ startMinimized CONSTANT)
    // "auto" / "light" / "dark" — drives the color-scheme menu's checked state.
    Q_PROPERTY(QString colorScheme READ colorScheme NOTIFY colorSchemeChanged)

public:
    explicit App(QObject *parent = nullptr);
    ~App() override;

    void start();
    void setStartMinimized(bool minimized) { m_startMinimized = minimized; }
    bool startMinimized() const { return m_startMinimized; }
    void setDemoMode(bool on) { m_demoMode = on; }
    bool demoMode() const { return m_demoMode; }

    bool loggedIn() const;
    QString status() const;
    QString currentListId() const;
    void setCurrentListId(const QString &id);
    QVariantMap detailTask() const { return m_detailTask; }
    QString colorScheme() const { return m_colorScheme; }

    TasksModel *tasksModel() const;
    TaskListsModel *listsModel() const;

public Q_SLOTS:
    void login();
    void logout();
    void refresh();
    void addTask(const QString &input);
    void completeTask(const QString &taskId);
    void uncompleteTask(const QString &taskId);
    void deleteTask(const QString &taskId);
    void createList(const QString &displayName);
    void deleteList(const QString &listId);
    void renameList(const QString &listId, const QString &newName);
    void toggleImportance(const QString &taskId);
    void setTaskDueDate(const QString &taskId, const QDateTime &due);
    void clearTaskDueDate(const QString &taskId);
    void setTaskReminder(const QString &taskId, const QDateTime &when);
    void clearTaskReminder(const QString &taskId);
    void setTaskTitle(const QString &taskId, const QString &title);
    void setTaskBody(const QString &taskId, const QString &body);
    void addChecklistItem(const QString &taskId, const QString &displayName);
    void toggleChecklistItem(const QString &taskId, const QString &itemId);
    void renameChecklistItem(const QString &taskId, const QString &itemId,
                             const QString &displayName);
    void deleteChecklistItem(const QString &taskId, const QString &itemId);
    // pattern: "" (clear) / "daily" / "weekly" / "monthly" / "yearly".
    // Unknown values are no-ops.
    void setTaskRecurrencePattern(const QString &taskId, const QString &pattern);
    void addLinkedResource(const QString &taskId, const QString &webUrl,
                           const QString &displayName);
    void removeLinkedResource(const QString &taskId, const QString &resourceId);
    void openLinkedResource(const QString &webUrl);
    void openTaskDetails(int row);
    void closeTaskDetails();
    void requestPickDateForDue(const QString &taskId);
    void toggleWindow();
    // Color scheme picker — "auto"/"light"/"dark" only. KColorSchemeManager
    // auto-saves; the choice survives across launches.
    void setColorScheme(const QString &scheme);

Q_SIGNALS:
    void loggedInChanged();
    void statusChanged();
    void currentListIdChanged();
    void detailTaskChanged();
    void errorOccurred(const QString &message);
    void windowToggleRequested();
    void pickDateRequested(const QString &taskId, const QDateTime &initial);
    void colorSchemeChanged();

private:
    void onAuthenticated();
    void onAuthError(const QString &message);
    void setStatus(const QString &status);
    QPair<QString, QDateTime> parseAddInput(const QString &raw) const;
    void loadDemoData();
    void switchDemoList(const QString &id);

    // True iff the synthetic "Alle" smart-list is selected.
    bool isVirtualAll() const;
    // Resolve the originating listId for a task that lives in m_tasksModel.
    // Used by mutations when the virtual all-list is active. Falls back to
    // m_currentListId for the non-virtual path.
    QString listIdForTask(const QString &taskId) const;
    // Refresh m_tasksModel from the local cache when virtual is active.
    void refreshAggregatedTasks();
    // Push current display-name map to TasksModel for ListNameRole resolution.
    void pushListNamesToModel();
    // Trigger a sync (delta or full fetch) for every real list. Used by
    // refresh() when virtual is active and on entering the virtual list.
    void syncAllLists();
    // After m_tasksModel is replaced, re-resolve the open detail-sheet task
    // by id from the new model contents.
    void refreshDetailTaskFromModel();

    void checkReminders();

    std::unique_ptr<AuthManager> m_auth;
    std::unique_ptr<GraphClient> m_graph;
    std::unique_ptr<TodoApi> m_todo;
    std::unique_ptr<Database> m_db;
    std::unique_ptr<TrayIcon> m_tray;
    TasksModel *m_tasksModel;
    TaskListsModel *m_listsModel;
    QTimer *m_reminderTimer = nullptr;
    // Composite key "taskId|reminderIsoUtc" so that resetting a reminder to
    // a new time produces a new key and re-fires.
    QSet<QString> m_firedReminders;

    QString m_status;
    QString m_currentListId;
    QVariantMap m_detailTask;
    bool m_startMinimized = false;
    bool m_demoMode = false;
    QHash<QString, QList<Task>> m_demoTasks;
    KColorSchemeManager *m_colorSchemeManager = nullptr;
    QString m_colorScheme = QStringLiteral("auto");
};

} // namespace Merkzettel
