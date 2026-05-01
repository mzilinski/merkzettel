#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QDateTime>
#include <QJsonObject>

namespace Merkzettel {

class GraphClient;

struct TaskList {
    QString id;
    QString displayName;
    bool isDefault = false;
};

struct Task {
    QString id;
    QString title;
    QString status;        // "notStarted", "inProgress", "completed"
    QString importance;    // "low", "normal", "high"
    QString body;
    QDateTime dueDate;        // UTC, may be invalid
    QDateTime reminderDate;   // UTC, valid only if hasReminder
    QDateTime lastModified;
    bool hasReminder = false;
    bool completed() const { return status == QLatin1String("completed"); }
    bool isImportant() const { return importance == QLatin1String("high"); }
};

class TodoApi : public QObject
{
    Q_OBJECT
public:
    explicit TodoApi(GraphClient *graph, QObject *parent = nullptr);

    void fetchLists();
    void fetchTasks(const QString &listId);
    void deleteList(const QString &listId);
    void renameList(const QString &listId, const QString &newName);
    void addTask(const QString &listId, const QString &title, const QDateTime &due = {});
    void setTaskStatus(const QString &listId, const QString &taskId, const QString &status);
    void setTaskImportance(const QString &listId, const QString &taskId, const QString &importance);
    void setTaskDueDate(const QString &listId, const QString &taskId, const QDateTime &due);
    void setTaskReminder(const QString &listId, const QString &taskId, const QDateTime &when);
    void clearTaskReminder(const QString &listId, const QString &taskId);
    void setTaskTitle(const QString &listId, const QString &taskId, const QString &title);
    void setTaskBody(const QString &listId, const QString &taskId, const QString &body);
    void updateTask(const QString &listId, const QString &taskId, const QJsonObject &patch);
    void deleteTask(const QString &listId, const QString &taskId);

Q_SIGNALS:
    void listsReceived(const QList<Merkzettel::TaskList> &lists);
    void tasksReceived(const QString &listId, const QList<Merkzettel::Task> &tasks);
    void taskMutated(const QString &listId);
    void listMutated();
    void errorOccurred(const QString &message);

private:
    GraphClient *m_graph;
};

} // namespace Merkzettel

Q_DECLARE_METATYPE(Merkzettel::TaskList)
Q_DECLARE_METATYPE(Merkzettel::Task)
