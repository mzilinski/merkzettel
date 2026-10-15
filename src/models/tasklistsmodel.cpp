#include "tasklistsmodel.h"

#include <KLocalizedString>

namespace Merkzettel {

TaskListsModel::TaskListsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

bool TaskListsModel::isVirtualId(const QString &id)
{
    return id == QLatin1String(kVirtualMyDayId) || id == QLatin1String(kVirtualAllId)
        || id == QLatin1String(kVirtualImportantId) || id == QLatin1String(kVirtualPlannedId)
        || id == QLatin1String(kVirtualCompletedId);
}

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
    case IdRole: return l.id;
    case DisplayNameRole: return l.displayName;
    case IsDefaultRole: return l.isDefault;
    case IsSharedRole: return l.isShared;
    case IsVirtualRole: return l.isVirtual;
    case IconNameRole:
        if (l.id == QLatin1String(kVirtualMyDayId)) return QStringLiteral("weather-clear-symbolic");
        if (l.id == QLatin1String(kVirtualAllId)) return QStringLiteral("view-list-icons");
        if (l.id == QLatin1String(kVirtualImportantId)) return QStringLiteral("starred-symbolic");
        if (l.id == QLatin1String(kVirtualPlannedId)) return QStringLiteral("view-calendar");
        if (l.id == QLatin1String(kVirtualCompletedId)) return QStringLiteral("checkmark");
        return l.isDefault ? QStringLiteral("go-home") : QStringLiteral("view-list-text");
    default: break;
    }
    return {};
}

QHash<int, QByteArray> TaskListsModel::roleNames() const
{
    return {
        {IdRole, "listId"},         {DisplayNameRole, "displayName"}, {IsDefaultRole, "isDefault"},
        {IsSharedRole, "isShared"}, {IsVirtualRole, "isVirtual"},     {IconNameRole, "iconName"},
    };
}

void TaskListsModel::setLists(const QList<TaskList> &lists)
{
    beginResetModel();
    m_lists.clear();
    m_lists.reserve(lists.size() + 5);
    if (!lists.isEmpty()) {
        const auto addVirtual = [this](const char *id, const QString &name) {
            TaskList l;
            l.id = QString::fromLatin1(id);
            l.displayName = name;
            l.isVirtual = true;
            m_lists.append(l);
        };
        addVirtual(kVirtualMyDayId, i18n("My Day"));
        addVirtual(kVirtualAllId, i18n("All"));
        addVirtual(kVirtualImportantId, i18n("Important"));
        addVirtual(kVirtualPlannedId, i18n("Planned"));
        addVirtual(kVirtualCompletedId, i18n("Completed"));
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
