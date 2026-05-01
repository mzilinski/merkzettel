#pragma once

#include <QAbstractListModel>
#include "graph/todoapi.h"

namespace Merkzettel {

class TaskListsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        DisplayNameRole,
        IsDefaultRole,
        IsSharedRole,
    };

    explicit TaskListsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setLists(const QList<TaskList> &lists);
    void clear();

Q_SIGNALS:
    void countChanged();

private:
    QList<TaskList> m_lists;
};

} // namespace Merkzettel
