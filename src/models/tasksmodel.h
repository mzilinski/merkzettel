#pragma once

#include <QAbstractListModel>
#include "graph/todoapi.h"

namespace Merkzettel {

class TasksModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
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
        ListIdRole,
        ListNameRole,
    };

    explicit TasksModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setTasks(const QList<Task> &tasks);
    void clear();

    QString filterText() const;
    void setFilterText(const QString &text);

    // Map listId -> displayName, used to resolve ListNameRole. App refreshes
    // this whenever listsReceived fires.
    void setListNames(const QHash<QString, QString> &names);

    Q_INVOKABLE QVariantMap taskAt(int row) const;
    Q_INVOKABLE QString listIdForTask(const QString &taskId) const;

Q_SIGNALS:
    void countChanged();
    void filterTextChanged();

private:
    static int sectionRank(const Task &t);
    static QString sectionKey(const Task &t);
    static QString sectionLabel(const Task &t);

    void applyFilter();
    bool matchesFilter(const Task &t) const;

    QList<Task> m_allTasks;
    QList<Task> m_tasks;
    QString m_filterText;
    QHash<QString, QString> m_listNames;
};

} // namespace Merkzettel
