#include "tasksmodel.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QLocale>
#include <QUrl>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
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

const QStringList kSortModes = {
    QStringLiteral("default"), QStringLiteral("importance"), QStringLiteral("dueDate"),
    QStringLiteral("title"), QStringLiteral("created"),
};

// Direction a sort mode starts out with when picked, matching MS To Do:
// newest first for the creation date, everything else ascending.
bool defaultDescending(const QString &mode)
{
    return mode == QLatin1String("created");
}

KConfigGroup viewConfig()
{
    return KSharedConfig::openConfig()->group(QStringLiteral("View"));
}

KConfigGroup sortConfig()
{
    return KSharedConfig::openConfig()->group(QStringLiteral("ListSort"));
}

// Graph list ids are base64 and may contain '=' / '/', so percent-encode
// them before using them as config keys.
QString sortKey(const QString &listId)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(listId));
}
} // namespace

TasksModel::TasksModel(QObject *parent) : QAbstractListModel(parent)
{
    m_showCompleted = viewConfig().readEntry("ShowCompleted", true);
}

void TasksModel::saveSortConfig() const
{
    if (m_sortScope.isEmpty()) return;
    KConfigGroup cfg = sortConfig();
    if (m_sortMode == QLatin1String("default")) {
        cfg.deleteEntry(sortKey(m_sortScope));
    } else {
        cfg.writeEntry(sortKey(m_sortScope),
                       QStringList{m_sortMode, m_sortDescending ? QStringLiteral("desc")
                                                                : QStringLiteral("asc")});
    }
    cfg.sync();
}

void TasksModel::setSortScope(const QString &listId)
{
    if (listId == m_sortScope) return;
    m_sortScope = listId;
    const QStringList stored = sortConfig().readEntry(sortKey(listId), QStringList());
    QString mode = stored.value(0, QStringLiteral("default"));
    if (!kSortModes.contains(mode)) mode = QStringLiteral("default");
    const bool desc = stored.value(1) == QLatin1String("desc");
    const bool modeChanged = mode != m_sortMode;
    const bool descChanged = desc != m_sortDescending;
    m_sortMode = mode;
    m_sortDescending = desc;
    if (modeChanged) Q_EMIT sortModeChanged();
    if (descChanged) Q_EMIT sortDescendingChanged();
    if (modeChanged || descChanged) applyFilter();
}

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

// With an explicit sort order the due-date buckets would fight the ordering,
// so only the open/completed split survives.
int TasksModel::effectiveSectionRank(const Task &t) const
{
    if (m_sortMode == QLatin1String("default")) return sectionRank(t);
    return t.completed() ? kSectionCompleted : kSectionOverdue;
}

QString TasksModel::sectionLabel(const Task &t) const
{
    if (m_sortMode != QLatin1String("default") && !t.completed()) {
        return QString();  // single unlabeled bucket; the view hides empty headers
    }
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
    case SectionKeyRole:   return QString::number(effectiveSectionRank(t));
    case SectionLabelRole: return sectionLabel(t);
    case ChecklistTotalRole: return t.totalChecklistCount;
    case HasRecurrenceRole: return !t.recurrenceJson.isEmpty();
    case LinkedResourceCountRole: return int(t.linkedResources.size());
    case ListIdRole:       return t.listId;
    case ListNameRole:     return m_listNames.value(t.listId);
    case CategoriesRole:   return t.categories;
    case HasAttachmentsRole: return t.hasAttachments;
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
        {ListIdRole, "listId"},
        {ListNameRole, "listName"},
        {CategoriesRole, "categories"},
        {HasAttachmentsRole, "hasAttachments"},
    };
}

bool TasksModel::lessThan(const Task &a, const Task &b) const
{
    const int sa = effectiveSectionRank(a);
    const int sb = effectiveSectionRank(b);
    if (sa != sb) return sa < sb;

    // Primary key per sort mode; ties fall through to the title. Tasks
    // without a value for the key (no due date, no created date) always go
    // last, regardless of direction.
    int c = 0;
    if (m_sortMode == QLatin1String("default")) {
        if (a.isImportant() != b.isImportant()) return a.isImportant();
        if (a.dueDate.isValid() != b.dueDate.isValid()) return a.dueDate.isValid();
        if (a.dueDate != b.dueDate) return a.dueDate < b.dueDate;
    } else if (m_sortMode == QLatin1String("importance")) {
        if (a.isImportant() != b.isImportant()) c = a.isImportant() ? -1 : 1;
    } else if (m_sortMode == QLatin1String("dueDate")) {
        if (a.dueDate.isValid() != b.dueDate.isValid()) return a.dueDate.isValid();
        if (a.dueDate != b.dueDate) c = a.dueDate < b.dueDate ? -1 : 1;
    } else if (m_sortMode == QLatin1String("created")) {
        if (a.createdDate.isValid() != b.createdDate.isValid()) return a.createdDate.isValid();
        if (a.createdDate != b.createdDate) c = a.createdDate < b.createdDate ? -1 : 1;
    } else if (m_sortMode == QLatin1String("title")) {
        c = a.title.localeAwareCompare(b.title);
    }
    if (c != 0) return m_sortDescending ? c > 0 : c < 0;
    return a.title.localeAwareCompare(b.title) < 0;
}

void TasksModel::setTasks(const QList<Task> &tasks)
{
    m_allTasks = tasks;
    if (!m_hiddenIds.isEmpty()) {
        QSet<QString> present;
        for (const auto &t : tasks) present.insert(t.id);
        m_hiddenIds.intersect(present);
    }
    applyFilter();
}

void TasksModel::hideTasks(const QStringList &taskIds)
{
    for (const auto &id : taskIds) m_hiddenIds.insert(id);
    applyFilter();
}

void TasksModel::unhideTasks(const QStringList &taskIds)
{
    for (const auto &id : taskIds) m_hiddenIds.remove(id);
    applyFilter();
}

void TasksModel::setShowCompleted(bool show)
{
    if (show == m_showCompleted) return;
    m_showCompleted = show;
    KConfigGroup cfg = viewConfig();
    cfg.writeEntry("ShowCompleted", show);
    cfg.sync();
    Q_EMIT showCompletedChanged();
    applyFilter();
}

void TasksModel::setSortMode(const QString &mode)
{
    if (mode == m_sortMode || !kSortModes.contains(mode)) return;
    m_sortMode = mode;
    const bool desc = defaultDescending(mode);
    const bool descChanged = desc != m_sortDescending;
    m_sortDescending = desc;
    saveSortConfig();
    Q_EMIT sortModeChanged();
    if (descChanged) Q_EMIT sortDescendingChanged();
    applyFilter();
}

void TasksModel::setSortDescending(bool descending)
{
    if (descending == m_sortDescending) return;
    m_sortDescending = descending;
    saveSortConfig();
    Q_EMIT sortDescendingChanged();
    applyFilter();
}

void TasksModel::setSmartFilter(SmartFilter filter)
{
    if (filter == m_smartFilter) return;
    m_smartFilter = filter;
    applyFilter();
}

void TasksModel::clear()
{
    if (m_allTasks.isEmpty() && m_tasks.isEmpty()) return;
    beginResetModel();
    m_allTasks.clear();
    m_tasks.clear();
    endResetModel();
    Q_EMIT countChanged();
}

QString TasksModel::filterText() const
{
    return m_filterText;
}

void TasksModel::setFilterText(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed == m_filterText) return;
    m_filterText = trimmed;
    Q_EMIT filterTextChanged();
    applyFilter();
}

bool TasksModel::matchesFilter(const Task &t) const
{
    if (m_hiddenIds.contains(t.id)) return false;
    switch (m_smartFilter) {
    case SmartFilter::None:
        break;
    case SmartFilter::Important:
        if (!t.isImportant()) return false;
        break;
    case SmartFilter::Planned:
        if (!t.dueDate.isValid() && !t.hasReminder) return false;
        break;
    case SmartFilter::Completed:
        if (!t.completed()) return false;
        break;
    }
    // The "Completed" smart-list exists to show exactly what this hides.
    if (!m_showCompleted && t.completed() && m_smartFilter != SmartFilter::Completed) {
        return false;
    }
    if (m_filterText.isEmpty()) return true;
    if (t.title.contains(m_filterText, Qt::CaseInsensitive)
        || t.body.contains(m_filterText, Qt::CaseInsensitive)) {
        return true;
    }
    for (const auto &c : t.categories) {
        if (c.contains(m_filterText, Qt::CaseInsensitive)) return true;
    }
    return false;
}

void TasksModel::applyFilter()
{
    beginResetModel();
    m_tasks.clear();
    m_tasks.reserve(m_allTasks.size());
    for (const auto &t : m_allTasks) {
        if (matchesFilter(t)) m_tasks.append(t);
    }
    std::sort(m_tasks.begin(), m_tasks.end(),
              [this](const Task &a, const Task &b) { return lessThan(a, b); });
    endResetModel();
    Q_EMIT countChanged();
}

// Decode Graph's patternedRecurrence into what the editor needs:
//   preset   "" / "daily" / "weekdays" / "weekly" / "monthly" / "yearly" /
//            "custom" (anything the presets don't cover)
//   unit     "day" / "week" / "month" / "year" ("" for relative patterns)
//   interval every N units
//   days     Graph weekday names for weekly patterns
//   summary  human-readable text, e.g. "Every 2 weeks on Mon, Thu"
QVariantMap TasksModel::recurrenceInfo(const QString &recurrenceJson)
{
    if (recurrenceJson.isEmpty()) return {{QStringLiteral("preset"), QString()}};
    const auto pat = QJsonDocument::fromJson(recurrenceJson.toUtf8()).object()
                         .value(QStringLiteral("pattern")).toObject();
    const QString type = pat.value(QStringLiteral("type")).toString();
    const int interval = qMax(1, pat.value(QStringLiteral("interval")).toInt(1));
    QStringList days;
    for (const auto &d : pat.value(QStringLiteral("daysOfWeek")).toArray()) days.append(d.toString());

    static const QStringList kWeekdays = {
        QStringLiteral("monday"), QStringLiteral("tuesday"), QStringLiteral("wednesday"),
        QStringLiteral("thursday"), QStringLiteral("friday"), QStringLiteral("saturday"),
        QStringLiteral("sunday"),
    };
    QString unit;
    if (type == QLatin1String("daily")) unit = QStringLiteral("day");
    else if (type == QLatin1String("weekly")) unit = QStringLiteral("week");
    else if (type == QLatin1String("absoluteMonthly")) unit = QStringLiteral("month");
    else if (type == QLatin1String("absoluteYearly")) unit = QStringLiteral("year");

    QStringList sortedDays;
    for (const auto &d : kWeekdays) if (days.contains(d)) sortedDays.append(d);
    const bool workweek = sortedDays == kWeekdays.mid(0, 5);

    QString preset = QStringLiteral("custom");
    if (interval == 1) {
        if (unit == QLatin1String("day")) preset = QStringLiteral("daily");
        else if (unit == QLatin1String("week") && workweek) preset = QStringLiteral("weekdays");
        else if (unit == QLatin1String("week") && sortedDays.size() <= 1) preset = QStringLiteral("weekly");
        else if (unit == QLatin1String("month")) preset = QStringLiteral("monthly");
        else if (unit == QLatin1String("year")) preset = QStringLiteral("yearly");
    }

    QString summary;
    if (unit == QLatin1String("day")) {
        summary = i18np("Every day", "Every %1 days", interval);
    } else if (unit == QLatin1String("week")) {
        QStringList names;
        for (const auto &d : sortedDays) {
            names.append(QLocale().dayName(kWeekdays.indexOf(d) + 1, QLocale::ShortFormat));
        }
        summary = workweek && interval == 1
            ? i18n("Weekdays")
            : (names.isEmpty() ? i18np("Every week", "Every %1 weeks", interval)
                               : i18np("Every week on %2", "Every %1 weeks on %2", interval,
                                       names.join(QStringLiteral(", "))));
    } else if (unit == QLatin1String("month")) {
        summary = i18np("Every month", "Every %1 months", interval);
    } else if (unit == QLatin1String("year")) {
        summary = i18np("Every year", "Every %1 years", interval);
    } else {
        summary = i18n("Custom");  // relative monthly/yearly — not editable here
    }

    return {
        {QStringLiteral("preset"), preset},
        {QStringLiteral("unit"), unit},
        {QStringLiteral("interval"), interval},
        {QStringLiteral("days"), sortedDays},
        {QStringLiteral("summary"), summary},
    };
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
    const QVariantMap rec = recurrenceInfo(t.recurrenceJson);
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
        {QStringLiteral("createdDate"), t.createdDate},
        {QStringLiteral("lastModified"), t.lastModified},
        {QStringLiteral("checklistItems"), items},
        {QStringLiteral("openChecklistCount"), t.openChecklistCount},
        {QStringLiteral("totalChecklistCount"), t.totalChecklistCount},
        {QStringLiteral("recurrence"), rec},
        {QStringLiteral("categories"), t.categories},
        {QStringLiteral("hasAttachments"), t.hasAttachments},
        {QStringLiteral("linkedResources"), links},
        {QStringLiteral("linkedResourceCount"), int(t.linkedResources.size())},
        {QStringLiteral("listId"), t.listId},
        {QStringLiteral("listName"), m_listNames.value(t.listId)},
    };
}

Task TasksModel::taskById(const QString &taskId) const
{
    for (const auto &t : m_allTasks) {
        if (t.id == taskId) return t;
    }
    return {};
}

QString TasksModel::listIdForTask(const QString &taskId) const
{
    for (const auto &t : m_allTasks) {
        if (t.id == taskId) return t.listId;
    }
    return {};
}

void TasksModel::setListNames(const QHash<QString, QString> &names)
{
    if (m_listNames == names) return;
    m_listNames = names;
    if (!m_tasks.isEmpty()) {
        // Re-emit dataChanged for ListNameRole so existing rows pick up the
        // resolved names immediately.
        Q_EMIT dataChanged(index(0, 0), index(m_tasks.size() - 1, 0),
                           {ListNameRole});
    }
}

} // namespace Merkzettel
