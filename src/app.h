#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QVariantMap>
#include <memory>

#include "models/tasksmodel.h"
#include "models/tasklistsmodel.h"

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
    void openTaskDetails(int row);
    void closeTaskDetails();
    void requestPickDateForDue(const QString &taskId);
    void toggleWindow();

Q_SIGNALS:
    void loggedInChanged();
    void statusChanged();
    void currentListIdChanged();
    void detailTaskChanged();
    void errorOccurred(const QString &message);
    void windowToggleRequested();
    void pickDateRequested(const QString &taskId, const QDateTime &initial);

private:
    void onAuthenticated();
    void onAuthError(const QString &message);
    void setStatus(const QString &status);
    QPair<QString, QDateTime> parseAddInput(const QString &raw) const;
    void loadDemoData();
    void switchDemoList(const QString &id);

    std::unique_ptr<AuthManager> m_auth;
    std::unique_ptr<GraphClient> m_graph;
    std::unique_ptr<TodoApi> m_todo;
    std::unique_ptr<Database> m_db;
    std::unique_ptr<TrayIcon> m_tray;
    TasksModel *m_tasksModel;
    TaskListsModel *m_listsModel;

    QString m_status;
    QString m_currentListId;
    QVariantMap m_detailTask;
    bool m_startMinimized = false;
    bool m_demoMode = false;
    QHash<QString, QList<Task>> m_demoTasks;
};

} // namespace Merkzettel
