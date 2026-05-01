#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QHash>

#include "graph/todoapi.h"

namespace Merkzettel {

class Database : public QObject
{
    Q_OBJECT
public:
    explicit Database(QObject *parent = nullptr);

    bool open();

    void upsertLists(const QList<TaskList> &lists);
    void upsertTasks(const QString &listId, const QList<Task> &tasks);
    // Apply a delta-sync result: upsert changed tasks (preserving cached
    // checklist items if the delta payload doesn't carry them — the delta
    // endpoint doesn't support $expand) and remove deleted ones.
    void applyTaskDelta(const QString &listId,
                        const QList<Task> &changed,
                        const QStringList &deletedIds);
    // Replace the cached checklist items for a single task. Used by the
    // dedicated /checklistItems fetch after delta detects a changed task.
    void upsertChecklistItems(const QString &listId, const QString &taskId,
                              const QList<ChecklistItem> &items);
    void upsertLinkedResources(const QString &listId, const QString &taskId,
                               const QList<LinkedResource> &items);

    QList<TaskList> lists() const;
    QList<Task> tasks(const QString &listId) const;

    int openTaskCountForToday() const;

    struct ReminderHit {
        QString listId;
        QString taskId;
        QString title;
        QDateTime reminderUtc;
    };
    // Open tasks across all lists whose reminder fires at or before `cutoff`.
    QList<ReminderHit> reminderHitsBefore(const QDateTime &cutoff) const;

    QString deltaLink(const QString &listId) const;
    void setDeltaLink(const QString &listId, const QString &deltaLink);
    void clearDeltaLink(const QString &listId);

private:
    void createSchema();
    QHash<QString, QList<ChecklistItem>> checklistItemsByTask(const QString &listId) const;
    QHash<QString, QList<LinkedResource>> linkedResourcesByTask(const QString &listId) const;
    QString m_connectionName;
};

} // namespace Merkzettel
