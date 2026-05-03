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
    bool isShared = false;  // Read-only — Graph doesn't expose share management.
    bool isVirtual = false; // Synthetic local entry (e.g. "Alle"), not a Graph list.
};

struct ChecklistItem {
    QString id;
    QString displayName;
    QDateTime createdDateTime;  // UTC, may be invalid
    bool isChecked = false;
};

struct LinkedResource {
    QString id;
    QString applicationName;  // source app label (e.g. "Outlook", "Merkzettel")
    QString webUrl;           // target URL
    QString displayName;      // user-facing label
    QString externalId;       // opaque external-system id, pass-through
};

struct Task {
    QString id;
    QString listId;        // Parent list this task belongs to.
    QString title;
    QString status;        // "notStarted", "inProgress", "completed"
    QString importance;    // "low", "normal", "high"
    QString body;
    QDateTime dueDate;        // UTC, may be invalid
    QDateTime reminderDate;   // UTC, valid only if hasReminder
    QDateTime lastModified;
    bool hasReminder = false;
    // Microsoft Graph patternedRecurrence as a JSON string (or empty if none).
    // Stored verbatim so the API and cache stay schema-compatible if Graph
    // adds new pattern types in the future.
    QString recurrenceJson;
    QList<ChecklistItem> checklistItems;  // empty unless populated by $expand or cache
    int openChecklistCount = 0;
    int totalChecklistCount = 0;
    QList<LinkedResource> linkedResources;  // empty unless populated by $expand or cache
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
    void createList(const QString &displayName);
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
    void setTaskRecurrence(const QString &listId, const QString &taskId,
                           const QJsonObject &recurrence);  // empty = clear
    void updateTask(const QString &listId, const QString &taskId, const QJsonObject &patch);
    void deleteTask(const QString &listId, const QString &taskId);

    // Delta sync. If deltaLink is empty: starts a full delta walk against
    // /me/todo/lists/{lid}/tasks/delta. Otherwise: resumes from deltaLink and
    // returns only changes since that token. Paginates internally via
    // @odata.nextLink and emits tasksDelta on completion.
    void syncTasks(const QString &listId, const QString &deltaLink);
    // One-shot bootstrap: requests a fresh delta token without transferring
    // any task data (Graph's $deltatoken=latest shortcut). Use after a full
    // fetchTasks so subsequent refresh() calls can run as cheap incremental
    // delta syncs. Emits tasksDelta with empty changed/deleted but a
    // populated newDeltaLink.
    void bootstrapDeltaLink(const QString &listId);

    void fetchChecklistItems(const QString &listId, const QString &taskId);
    void addChecklistItem(const QString &listId, const QString &taskId,
                          const QString &displayName);
    void setChecklistItemChecked(const QString &listId, const QString &taskId,
                                 const QString &itemId, bool checked);
    void renameChecklistItem(const QString &listId, const QString &taskId,
                             const QString &itemId, const QString &displayName);
    void deleteChecklistItem(const QString &listId, const QString &taskId,
                             const QString &itemId);

    void fetchLinkedResources(const QString &listId, const QString &taskId);
    void addLinkedResource(const QString &listId, const QString &taskId,
                           const QString &applicationName,
                           const QString &webUrl,
                           const QString &displayName);
    void removeLinkedResource(const QString &listId, const QString &taskId,
                              const QString &resourceId);

Q_SIGNALS:
    void listsReceived(const QList<Merkzettel::TaskList> &lists);
    void tasksReceived(const QString &listId, const QList<Merkzettel::Task> &tasks);
    void taskMutated(const QString &listId);
    void listMutated();
    void listCreated(const QString &listId);  // newly-created id, for select-after-create
    void checklistItemsReceived(const QString &listId, const QString &taskId,
                                const QList<Merkzettel::ChecklistItem> &items);
    void checklistItemMutated(const QString &listId, const QString &taskId);
    void linkedResourcesReceived(const QString &listId, const QString &taskId,
                                 const QList<Merkzettel::LinkedResource> &items);
    void linkedResourceMutated(const QString &listId, const QString &taskId);
    void tasksDelta(const QString &listId,
                    const QList<Merkzettel::Task> &changed,
                    const QStringList &deletedIds,
                    const QString &newDeltaLink);
    void tasksDeltaExpired(const QString &listId);
    void errorOccurred(const QString &message);

private:
    GraphClient *m_graph;
};

} // namespace Merkzettel

Q_DECLARE_METATYPE(Merkzettel::TaskList)
Q_DECLARE_METATYPE(Merkzettel::Task)
Q_DECLARE_METATYPE(Merkzettel::ChecklistItem)
Q_DECLARE_METATYPE(Merkzettel::LinkedResource)
