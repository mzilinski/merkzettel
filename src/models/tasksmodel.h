#pragma once

#include <QAbstractListModel>
#include <QSet>
#include "graph/todoapi.h"

namespace Merkzettel {

class TasksModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    // Global view option, persisted in merkzettelrc [View].
    Q_PROPERTY(bool showCompleted READ showCompleted WRITE setShowCompleted NOTIFY showCompletedChanged)
    // Per-list sort order like MS To Do, persisted in merkzettelrc [ListSort]
    // (Graph doesn't sync it). "default" = sectioned by due date; otherwise
    // "importance" / "dueDate" / "title" / "created".
    Q_PROPERTY(QString sortMode READ sortMode WRITE setSortMode NOTIFY sortModeChanged)
    Q_PROPERTY(bool sortDescending READ sortDescending WRITE setSortDescending NOTIFY sortDescendingChanged)
public:
    // Local filter applied on top of the task set, driven by the smart-lists.
    enum class SmartFilter { None, Important, Planned, Completed };

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
        CategoriesRole,
        HasAttachmentsRole,
    };

    explicit TasksModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setTasks(const QList<Task> &tasks);
    void clear();

    QString filterText() const;
    void setFilterText(const QString &text);

    bool showCompleted() const { return m_showCompleted; }
    void setShowCompleted(bool show);
    QString sortMode() const { return m_sortMode; }
    void setSortMode(const QString &mode);
    bool sortDescending() const { return m_sortDescending; }
    void setSortDescending(bool descending);
    // Switch the sort settings to those stored for listId.
    void setSortScope(const QString &listId);

    void setSmartFilter(SmartFilter filter);

    // Tasks with a pending (undoable) delete/move are hidden locally until
    // the operation is committed. Ids that no longer appear in a later
    // setTasks() are dropped from the hidden set automatically.
    void hideTasks(const QStringList &taskIds);
    void unhideTasks(const QStringList &taskIds);

    // Map listId -> displayName, used to resolve ListNameRole. App refreshes
    // this whenever listsReceived fires.
    void setListNames(const QHash<QString, QString> &names);

    Q_INVOKABLE QVariantMap taskAt(int row) const;
    Q_INVOKABLE QString listIdForTask(const QString &taskId) const;
    // Copy of the task with that id from the unfiltered set; default-constructed
    // (empty id) when unknown.
    Task taskById(const QString &taskId) const;

Q_SIGNALS:
    void countChanged();
    void filterTextChanged();
    void showCompletedChanged();
    void sortModeChanged();
    void sortDescendingChanged();

private:
    static int sectionRank(const Task &t);
    static QVariantMap recurrenceInfo(const QString &recurrenceJson);
    int effectiveSectionRank(const Task &t) const;
    QString sectionLabel(const Task &t) const;
    bool lessThan(const Task &a, const Task &b) const;
    void saveSortConfig() const;

    void applyFilter();
    bool matchesFilter(const Task &t) const;

    QList<Task> m_allTasks;
    QList<Task> m_tasks;
    QString m_filterText;
    SmartFilter m_smartFilter = SmartFilter::None;
    bool m_showCompleted = true;
    QString m_sortMode = QStringLiteral("default");
    bool m_sortDescending = false;
    QString m_sortScope;
    QHash<QString, QString> m_listNames;
    QSet<QString> m_hiddenIds;
};

} // namespace Merkzettel
