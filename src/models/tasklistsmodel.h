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
        IsVirtualRole,
        IconNameRole,
    };

    // Sentinel ids for the synthetic smart-list entries. They aggregate tasks
    // across every real list and filter them locally (see TasksModel).
    static constexpr auto kVirtualAllId = "__all__";
    static constexpr auto kVirtualImportantId = "__important__";
    static constexpr auto kVirtualPlannedId = "__planned__";
    static constexpr auto kVirtualCompletedId = "__completed__";

    static bool isVirtualId(const QString &id);

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
