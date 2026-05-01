#pragma once

#include <QAbstractListModel>
#include "graph/todoapi.h"

namespace Merkzettel {

class TasksModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        StatusRole,
        CompletedRole,
        ImportanceRole,
        IsImportantRole,
        DueDateRole,
        DueLabelRole,
        BodyRole,
        ReminderDateRole,
        HasReminderRole,
        SectionKeyRole,
        SectionLabelRole,
        ChecklistProgressRole,
        ChecklistTotalRole,
        HasRecurrenceRole,
        LinkedResourceCountRole,
    };

    explicit TasksModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setTasks(const QList<Task> &tasks);
    void clear();

    Q_INVOKABLE QVariantMap taskAt(int row) const;

Q_SIGNALS:
    void countChanged();

private:
    static int sectionRank(const Task &t);
    static QString sectionKey(const Task &t);
    static QString sectionLabel(const Task &t);

    QList<Task> m_tasks;
};

} // namespace Merkzettel
