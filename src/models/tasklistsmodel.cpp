#include "tasklistsmodel.h"

#include <KLocalizedString>

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
    case IsVirtualRole:   return l.isVirtual;
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
        {IsVirtualRole, "isVirtual"},
    };
}

void TaskListsModel::setLists(const QList<TaskList> &lists)
{
    beginResetModel();
    m_lists.clear();
    m_lists.reserve(lists.size() + 1);
    if (!lists.isEmpty()) {
        TaskList all;
        all.id = QString::fromLatin1(kVirtualAllId);
        all.displayName = i18n("All");
        all.isVirtual = true;
        m_lists.append(all);
    }
    m_lists.append(lists);
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
