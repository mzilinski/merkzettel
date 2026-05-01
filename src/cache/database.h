#pragma once

#include <QObject>
#include <QString>
#include <QList>

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

    QList<TaskList> lists() const;
    QList<Task> tasks(const QString &listId) const;

    int openTaskCountForToday() const;

private:
    void createSchema();
    QString m_connectionName;
};

} // namespace Merkzettel
