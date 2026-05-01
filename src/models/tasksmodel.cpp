#include "tasksmodel.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <KLocalizedString>
#include <algorithm>

namespace Merkzettel {

namespace {
constexpr int kSectionOverdue   = 0;
constexpr int kSectionToday     = 1;
constexpr int kSectionTomorrow  = 2;
constexpr int kSectionThisWeek  = 3;
constexpr int kSectionLater     = 4;
constexpr int kSectionNoDate    = 5;
constexpr int kSectionCompleted = 9;

QDate toLocalDate(const QDateTime &dt)
{
    return dt.toLocalTime().date();
}
} // namespace

TasksModel::TasksModel(QObject *parent) : QAbstractListModel(parent) {}

int TasksModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_tasks.size();
}

int TasksModel::sectionRank(const Task &t)
{
    if (t.completed()) return kSectionCompleted;
    if (!t.dueDate.isValid()) return kSectionNoDate;

    const QDate d = toLocalDate(t.dueDate);
    const QDate today = QDate::currentDate();
    if (d < today) return kSectionOverdue;
    if (d == today) return kSectionToday;
    if (d == today.addDays(1)) return kSectionTomorrow;
    if (d <= today.addDays(7)) return kSectionThisWeek;
    return kSectionLater;
}

QString TasksModel::sectionKey(const Task &t)
{
    return QString::number(sectionRank(t));
}

QString TasksModel::sectionLabel(const Task &t)
{
    switch (sectionRank(t)) {
    case kSectionOverdue:   return i18n("Overdue");
    case kSectionToday:     return i18n("Today");
    case kSectionTomorrow:  return i18n("Tomorrow");
    case kSectionThisWeek:  return i18n("This week");
    case kSectionLater:     return i18n("Later");
    case kSectionNoDate:    return i18n("No date");
    case kSectionCompleted: return i18n("Completed");
    }
    return {};
}

QVariant TasksModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tasks.size()) {
        return {};
    }
    const Task &t = m_tasks.at(index.row());
    switch (role) {
    case IdRole:           return t.id;
    case TitleRole:        return t.title;
    case StatusRole:       return t.status;
    case CompletedRole:    return t.completed();
    case ImportanceRole:   return t.importance.isEmpty() ? QStringLiteral("normal") : t.importance;
    case IsImportantRole:  return t.isImportant();
    case DueDateRole:      return t.dueDate;
    case BodyRole:         return t.body;
    case ReminderDateRole: return t.reminderDate;
    case HasReminderRole:  return t.hasReminder;
    case SectionKeyRole:   return sectionKey(t);
    case SectionLabelRole: return sectionLabel(t);
    case ChecklistTotalRole: return t.totalChecklistCount;
    case HasRecurrenceRole: return !t.recurrenceJson.isEmpty();
    case LinkedResourceCountRole: return int(t.linkedResources.size());
    case ChecklistProgressRole: {
        if (t.totalChecklistCount <= 0) return QString();
        const int done = t.totalChecklistCount - t.openChecklistCount;
        return QStringLiteral("%1/%2").arg(done).arg(t.totalChecklistCount);
    }
    case DueLabelRole: {
        if (!t.dueDate.isValid()) return QString();
        const QDate d = toLocalDate(t.dueDate);
        const QDate today = QDate::currentDate();
        if (d == today)               return i18n("Today");
        if (d == today.addDays(1))    return i18n("Tomorrow");
        if (d == today.addDays(-1))   return i18n("Yesterday");
        return QLocale().toString(d, QLocale::ShortFormat);
    }
    }
    return {};
}

QHash<int, QByteArray> TasksModel::roleNames() const
{
    return {
        {IdRole, "taskId"},
        {TitleRole, "title"},
        {StatusRole, "status"},
        {CompletedRole, "completed"},
        {ImportanceRole, "importance"},
        {IsImportantRole, "important"},
        {DueDateRole, "dueDate"},
        {DueLabelRole, "dueLabel"},
        {BodyRole, "body"},
        {ReminderDateRole, "reminderDate"},
        {HasReminderRole, "hasReminder"},
        {SectionKeyRole, "sectionKey"},
        {SectionLabelRole, "sectionLabel"},
        {ChecklistProgressRole, "checklistProgress"},
        {ChecklistTotalRole, "checklistTotal"},
        {HasRecurrenceRole, "hasRecurrence"},
        {LinkedResourceCountRole, "linkedResourceCount"},
    };
}

void TasksModel::setTasks(const QList<Task> &tasks)
{
    beginResetModel();
    m_tasks = tasks;
    // Sort: by section rank, then important first, then by due date, then by title.
    std::sort(m_tasks.begin(), m_tasks.end(), [](const Task &a, const Task &b) {
        const int sa = sectionRank(a);
        const int sb = sectionRank(b);
        if (sa != sb) return sa < sb;
        if (a.isImportant() != b.isImportant()) return a.isImportant();
        if (a.dueDate.isValid() && b.dueDate.isValid() && a.dueDate != b.dueDate)
            return a.dueDate < b.dueDate;
        if (a.dueDate.isValid() != b.dueDate.isValid())
            return a.dueDate.isValid();
        return a.title.localeAwareCompare(b.title) < 0;
    });
    endResetModel();
    Q_EMIT countChanged();
}

void TasksModel::clear()
{
    if (m_tasks.isEmpty()) return;
    beginResetModel();
    m_tasks.clear();
    endResetModel();
    Q_EMIT countChanged();
}

QVariantMap TasksModel::taskAt(int row) const
{
    if (row < 0 || row >= m_tasks.size()) return {};
    const Task &t = m_tasks.at(row);
    QVariantList items;
    for (const auto &c : t.checklistItems) {
        items.append(QVariantMap{
            {QStringLiteral("id"), c.id},
            {QStringLiteral("displayName"), c.displayName},
            {QStringLiteral("isChecked"), c.isChecked},
        });
    }
    QVariantList links;
    for (const auto &r : t.linkedResources) {
        links.append(QVariantMap{
            {QStringLiteral("id"), r.id},
            {QStringLiteral("displayName"), r.displayName},
            {QStringLiteral("webUrl"), r.webUrl},
            {QStringLiteral("applicationName"), r.applicationName},
        });
    }
    // Surface a coarse pattern string for the UI ComboBox: "" / "daily" /
    // "weekly" / "monthly" / "yearly". Anything else (custom interval,
    // weekly with specific weekdays, ...) becomes "custom" — the UI then
    // shows it as read-only because we don't have a designer for it.
    QString patternKind;
    bool patternCustom = false;
    if (!t.recurrenceJson.isEmpty()) {
        const auto doc = QJsonDocument::fromJson(t.recurrenceJson.toUtf8());
        const auto pat = doc.object().value(QStringLiteral("pattern")).toObject();
        const QString type = pat.value(QStringLiteral("type")).toString();
        const int interval = pat.value(QStringLiteral("interval")).toInt(1);
        if (type == QLatin1String("daily") && interval == 1) patternKind = QStringLiteral("daily");
        else if (type == QLatin1String("weekly") && interval == 1) patternKind = QStringLiteral("weekly");
        else if (type == QLatin1String("absoluteMonthly") && interval == 1) patternKind = QStringLiteral("monthly");
        else if (type == QLatin1String("absoluteYearly") && interval == 1) patternKind = QStringLiteral("yearly");
        else { patternKind = QStringLiteral("custom"); patternCustom = true; }
    }
    return {
        {QStringLiteral("taskId"), t.id},
        {QStringLiteral("title"), t.title},
        {QStringLiteral("body"), t.body},
        {QStringLiteral("status"), t.status},
        {QStringLiteral("completed"), t.completed()},
        {QStringLiteral("importance"), t.importance.isEmpty() ? QStringLiteral("normal") : t.importance},
        {QStringLiteral("important"), t.isImportant()},
        {QStringLiteral("dueDate"), t.dueDate},
        {QStringLiteral("hasReminder"), t.hasReminder},
        {QStringLiteral("reminderDate"), t.reminderDate},
        {QStringLiteral("checklistItems"), items},
        {QStringLiteral("openChecklistCount"), t.openChecklistCount},
        {QStringLiteral("totalChecklistCount"), t.totalChecklistCount},
        {QStringLiteral("recurrencePattern"), patternKind},
        {QStringLiteral("recurrenceCustom"), patternCustom},
        {QStringLiteral("linkedResources"), links},
        {QStringLiteral("linkedResourceCount"), int(t.linkedResources.size())},
    };
}

} // namespace Merkzettel
