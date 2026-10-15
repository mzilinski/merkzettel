#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QVariantMap>
#include <QSet>
#include <QUrl>
#include <functional>
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
    // False while Microsoft can't be reached; the UI shows cached data.
    Q_PROPERTY(bool online READ online NOTIFY onlineChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(
        QString currentListId READ currentListId WRITE setCurrentListId NOTIFY currentListIdChanged)
    // True for the synthetic smart-lists (All/Important/Planned/Completed).
    Q_PROPERTY(bool currentListVirtual READ isVirtualList NOTIFY currentListIdChanged)
    // Display name of the selected list (page title when the sidebar is folded).
    Q_PROPERTY(QString currentListName READ currentListName NOTIFY currentListNameChanged)
    Q_PROPERTY(TasksModel *tasksModel READ tasksModel CONSTANT)
    Q_PROPERTY(TaskListsModel *listsModel READ listsModel CONSTANT)
    Q_PROPERTY(QVariantMap detailTask READ detailTask NOTIFY detailTaskChanged)
    Q_PROPERTY(bool startMinimized READ startMinimized CONSTANT)
    // Demo data instead of an account (--demo, or started from the login page).
    Q_PROPERTY(bool demoMode READ demoMode NOTIFY demoModeChanged)
    // False for builds without an Azure client ID: sign-in cannot work.
    Q_PROPERTY(bool signInAvailable READ signInAvailable CONSTANT)
    // Demo entered from the login page can be left again (not --demo).
    Q_PROPERTY(bool canLeaveDemo READ canLeaveDemo NOTIFY demoModeChanged)
    // "auto" / "light" / "dark" — drives the color-scheme menu's checked state.
    Q_PROPERTY(QString colorScheme READ colorScheme NOTIFY colorSchemeChanged)
    // "Sharing happens in the web app" banner acknowledged (merkzettelrc).
    Q_PROPERTY(bool shareExplainerSeen READ shareExplainerSeen WRITE setShareExplainerSeen NOTIFY
                   shareExplainerSeenChanged)

public:
    explicit App(QObject *parent = nullptr);
    ~App() override;

    void start();
    void setStartMinimized(bool minimized) { m_startMinimized = minimized; }
    bool startMinimized() const { return m_startMinimized; }
    // --demo on the command line: no account handling at all this session.
    void setDemoMode(bool on)
    {
        m_demoMode = on;
        m_demoFromCommandLine = on;
    }
    bool demoMode() const { return m_demoMode; }
    bool signInAvailable() const;
    bool canLeaveDemo() const { return m_demoMode && !m_demoFromCommandLine; }

    bool loggedIn() const;
    bool online() const { return m_online; }
    QString status() const;
    QString currentListId() const;
    QString currentListName() const { return listDisplayName(m_currentListId); }
    void setCurrentListId(const QString &id);
    QVariantMap detailTask() const { return m_detailTask; }
    QString colorScheme() const { return m_colorScheme; }
    bool shareExplainerSeen() const;
    void setShareExplainerSeen(bool seen);

    TasksModel *tasksModel() const;
    TaskListsModel *listsModel() const;

    // Every category name used on any cached task, for autocompletion.
    Q_INVOKABLE QStringList knownCategories() const;
    // Text for components whose labels interpret HTML (Text.AutoText):
    // breaks every '<' with a zero-width space so remote data such as a task
    // title from a shared list can never render markup or load images.
    Q_INVOKABLE static QString plain(const QString &text);

public Q_SLOTS:
    void login();
    // Try the app with demo data from the login page, and go back.
    void enterDemoMode();
    void leaveDemoMode();
    void logout();
    void refresh();
    void addTask(const QString &input);
    void completeTask(const QString &taskId);
    void uncompleteTask(const QString &taskId);
    void deleteTask(const QString &taskId);
    // Bulk operations for the multi-selection (also used for single tasks).
    // Delete and move are delayed by kUndoWindowMs and can be undone until
    // then; completion is applied immediately and undone by reopening.
    void deleteTasks(const QStringList &taskIds);
    void moveTasks(const QStringList &taskIds, const QString &targetListId);
    void completeTasks(const QStringList &taskIds);
    void setTasksImportant(const QStringList &taskIds, bool important);
    void undo();
    // Local-only "My Day" (see Database::myDayTaskIds for why).
    void setInMyDay(const QStringList &taskIds, bool inMyDay);
    // Ctrl+T: add all to My Day, or remove them if all are already in it.
    void toggleMyDay(const QStringList &taskIds);
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
    // pattern: "" (clear) / "daily" / "weekdays" / "weekly" / "monthly" /
    // "yearly". Unknown values are no-ops.
    void setTaskRecurrencePattern(const QString &taskId, const QString &pattern);
    // Custom recurrence like MS To Do's "Repeat every N days/weeks/months/
    // years". days (Graph weekday names) only applies to "week"; weekly,
    // monthly and yearly anchor on the task's due date, else today.
    void setTaskRecurrence(const QString &taskId, const QString &unit, int interval,
                           const QStringList &days);
    void setTaskCategories(const QString &taskId, const QStringList &categories);

    void addLinkedResource(const QString &taskId, const QString &webUrl,
                           const QString &displayName);
    void removeLinkedResource(const QString &taskId, const QString &resourceId);
    void openLinkedResource(const QString &webUrl);
    void openTaskDetails(int row);
    // File attachments of the task open in the detail sheet. Upload reads
    // local files (≤ 25 MB each); open downloads into the cache dir and hands
    // the file to the default application.
    void addAttachments(const QString &taskId, const QList<QUrl> &fileUrls);
    void openAttachment(const QString &taskId, const QString &attachmentId);
    void saveAttachment(const QString &taskId, const QString &attachmentId, const QUrl &target);
    void deleteAttachment(const QString &taskId, const QString &attachmentId);
    void closeTaskDetails();
    void requestPickDateForDue(const QString &taskId);
    void toggleWindow();
    // Always bring the window to the front — used by the second-instance
    // hand-off so launching merkzettel again never hides a visible window.
    void activateWindow();
    // Bring the window up with the cursor in the new-task field (global
    // shortcut, tray menu, `merkzettel --quick-add`).
    void quickAdd();
    // Color scheme picker — "auto"/"light"/"dark" only. KColorSchemeManager
    // auto-saves; the choice survives across launches.
    void setColorScheme(const QString &scheme);
    // Alt+Up / Alt+Down: select the previous/next entry in the sidebar,
    // wrapping around.
    void selectAdjacentList(int delta);

Q_SIGNALS:
    void loggedInChanged();
    void demoModeChanged();
    void onlineChanged();
    void statusChanged();
    void currentListIdChanged();
    void currentListNameChanged();
    void detailTaskChanged();
    void errorOccurred(const QString &message);
    void windowToggleRequested();
    void windowActivateRequested();
    void pickDateRequested(const QString &taskId, const QDateTime &initial);
    void colorSchemeChanged();
    void shareExplainerSeenChanged();
    // Global "Quick add" shortcut fired: show the window, focus the add field.
    void quickAddRequested();
    // An undoable operation happened; QML shows message with an Undo action.
    void undoOffered(const QString &message, int timeoutMs);

private:
    void onAuthenticated();
    void onAuthError(const QString &message);
    void setStatus(const QString &status);
    void registerGlobalShortcuts();
    // Signed in *and* holding an access token, i.e. requests can go out.
    bool canSync() const;
    void setOnline(bool online);
    // Reload today's My Day ids into TasksModel (startup, changes, midnight).
    void reloadMyDay();
    // Execute a delayed delete/move now (timer expiry, list switch, or a
    // new undoable operation replacing it).
    void commitPending();
    QString listDisplayName(const QString &listId) const;
    void loadDemoData();
    // Demo mode: apply a change to the sample task locally (nothing is sent
    // to Microsoft or stored), then refresh the views.
    void applyDemo(const QString &taskId, const std::function<void(Task &)> &change);
    void demoChanged();
    void switchDemoList(const QString &id);

    // True iff one of the synthetic smart-lists is selected.
    bool isVirtualList() const;
    // Push the smart-list filter and the per-list sort order matching
    // m_currentListId to TasksModel.
    void applyListViewSettings();
    // Resolve the originating listId for a task that lives in m_tasksModel.
    // Used by mutations when a smart-list is active. Falls back to
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
    // Set m_detailTask from a TasksModel::taskAt() map, adding the cached
    // attachment list, and notify QML.
    void setDetailTask(const QVariantMap &task);
    Attachment findAttachment(const QString &taskId, const QString &attachmentId) const;

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

    struct UndoState {
        enum Kind { None, Delete, Move, Complete } kind = None;
        QList<QPair<QString, QString>> tasks; // (listId, taskId)
        QString targetListId;                 // Move only
        bool pending = false;                 // Delete/Move not yet sent
    };
    QHash<QString, QList<Attachment>> m_attachments; // taskId -> attachments
    QSet<QString> m_demoMyDay;
    QDate m_myDayDate; // day the loaded My Day set belongs to
    UndoState m_undo;
    QTimer *m_commitTimer = nullptr;
    QTimer *m_authRetryTimer = nullptr;
    bool m_online = true;
    int m_movesInFlight = 0;
    QSet<QString> m_moveTouchedLists;
    QVariantMap m_detailTask;
    bool m_startMinimized = false;
    bool m_demoMode = false;
    bool m_demoFromCommandLine = false;
    int m_demoCounter = 0;
    QHash<QString, QList<Task>> m_demoTasks;
    KColorSchemeManager *m_colorSchemeManager = nullptr;
    QString m_colorScheme = QStringLiteral("auto");
};

} // namespace Merkzettel
