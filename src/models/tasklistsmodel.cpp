#include "tasklistsmodel.h"

namespace Merkzettel {

TaskListsModel::TaskListsModel(QObject *parent) : QAbstractListModel(parent) {}

int TaskListsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_lists.size();
}

QVariant TaskListsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_lists.size()) {
        return {};
    }
    const TaskList &l = m_lists.at(index.row());
    switch (role) {
    case IdRole:          return l.id;
    case DisplayNameRole: return l.displayName;
    case IsDefaultRole:   return l.isDefault;
    case IsSharedRole:    return l.isShared;
    }
    return {};
}

QHash<int, QByteArray> TaskListsModel::roleNames() const
{
    return {
        {IdRole, "listId"},
        {DisplayNameRole, "displayName"},
        {IsDefaultRole, "isDefault"},
        {IsSharedRole, "isShared"},
    };
}

void TaskListsModel::setLists(const QList<TaskList> &lists)
{
    beginResetModel();
    m_lists = lists;
    endResetModel();
    Q_EMIT countChanged();
}

void TaskListsModel::clear()
{
    if (m_lists.isEmpty()) return;
    beginResetModel();
    m_lists.clear();
    endResetModel();
    Q_EMIT countChanged();
}

} // namespace Merkzettel
