#include "database.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QVariant>

namespace Merkzettel {

Database::Database(QObject *parent)
    : QObject(parent), m_connectionName(QStringLiteral("merkzettel-cache"))
{
}

bool Database::open()
{
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    const QString path = dataDir + QStringLiteral("/cache.sqlite");

    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(path);
    if (!db.open()) {
        qWarning() << "Failed to open cache db:" << db.lastError().text();
        return false;
    }
    createSchema();
    return true;
}

void Database::createSchema()
{
    auto db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS lists ("
        "  id TEXT PRIMARY KEY,"
        "  display_name TEXT NOT NULL,"
        "  is_default INTEGER NOT NULL DEFAULT 0,"
        "  delta_link TEXT,"
        "  is_shared INTEGER NOT NULL DEFAULT 0"
        ")"));
    q.exec(QStringLiteral("ALTER TABLE lists ADD COLUMN delta_link TEXT"));
    q.exec(QStringLiteral("ALTER TABLE lists ADD COLUMN is_shared INTEGER NOT NULL DEFAULT 0"));
    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS tasks ("
        "  id TEXT PRIMARY KEY,"
        "  list_id TEXT NOT NULL,"
        "  title TEXT NOT NULL,"
        "  status TEXT NOT NULL,"
        "  importance TEXT NOT NULL DEFAULT 'normal',"
        "  body TEXT,"
        "  due_utc TEXT,"
        "  reminder_utc TEXT,"
        "  has_reminder INTEGER NOT NULL DEFAULT 0,"
        "  last_modified TEXT,"
        "  recurrence_json TEXT"
        ")"));
    q.exec(QStringLiteral("ALTER TABLE tasks ADD COLUMN recurrence_json TEXT"));
    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_tasks_list ON tasks(list_id)"));
    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS checklist_items ("
        "  id TEXT PRIMARY KEY,"
        "  task_id TEXT NOT NULL,"
        "  list_id TEXT NOT NULL,"
        "  display_name TEXT NOT NULL,"
        "  is_checked INTEGER NOT NULL DEFAULT 0,"
        "  created_utc TEXT,"
        "  sort_order INTEGER NOT NULL DEFAULT 0"
        ")"));
    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_check_task ON checklist_items(task_id)"));
    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_check_list ON checklist_items(list_id)"));
    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS linked_resources ("
        "  id TEXT PRIMARY KEY,"
        "  task_id TEXT NOT NULL,"
        "  list_id TEXT NOT NULL,"
        "  application_name TEXT,"
        "  web_url TEXT,"
        "  display_name TEXT,"
        "  external_id TEXT,"
        "  sort_order INTEGER NOT NULL DEFAULT 0"
        ")"));
    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_linkres_task ON linked_resources(task_id)"));
    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_linkres_list ON linked_resources(list_id)"));

    // Migrate older schemas: add columns if missing.
    q.exec(QStringLiteral("ALTER TABLE tasks ADD COLUMN importance TEXT NOT NULL DEFAULT 'normal'"));
    q.exec(QStringLiteral("ALTER TABLE tasks ADD COLUMN reminder_utc TEXT"));
    q.exec(QStringLiteral("ALTER TABLE tasks ADD COLUMN has_reminder INTEGER NOT NULL DEFAULT 0"));
}

void Database::upsertLists(const QList<TaskList> &lists)
{
    auto db = QSqlDatabase::database(m_connectionName);
    db.transaction();
    QSqlQuery q(db);
    // Preserve delta_link across list refreshes by not touching it here.
    q.prepare(QStringLiteral(
        "INSERT INTO lists(id, display_name, is_default, is_shared) "
        "VALUES(?, ?, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET display_name=excluded.display_name, "
        "is_default=excluded.is_default, is_shared=excluded.is_shared"));
    for (const auto &l : lists) {
        q.bindValue(0, l.id);
        q.bindValue(1, l.displayName);
        q.bindValue(2, l.isDefault ? 1 : 0);
        q.bindValue(3, l.isShared ? 1 : 0);
        q.exec();
    }
    db.commit();
}

QList<Database::ReminderHit> Database::reminderHitsBefore(const QDateTime &cutoff) const
{
    QList<ReminderHit> result;
    auto db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT list_id, id, title, reminder_utc FROM tasks "
        "WHERE has_reminder = 1 AND status != 'completed' "
        "AND reminder_utc IS NOT NULL AND reminder_utc <= ? "
        "ORDER BY reminder_utc ASC"));
    q.bindValue(0, cutoff.toUTC().toString(Qt::ISODate));
    q.exec();
    while (q.next()) {
        ReminderHit h;
        h.listId = q.value(0).toString();
        h.taskId = q.value(1).toString();
        h.title = q.value(2).toString();
        const QString r = q.value(3).toString();
        if (!r.isEmpty()) h.reminderUtc = QDateTime::fromString(r, Qt::ISODate);
        result.append(h);
    }
    return result;
}

QString Database::deltaLink(const QString &listId) const
{
    auto db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT delta_link FROM lists WHERE id = ?"));
    q.bindValue(0, listId);
    q.exec();
    if (q.next()) return q.value(0).toString();
    return {};
}

void Database::setDeltaLink(const QString &listId, const QString &link)
{
    auto db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE lists SET delta_link = ? WHERE id = ?"));
    q.bindValue(0, link);
    q.bindValue(1, listId);
    q.exec();
}

void Database::clearDeltaLink(const QString &listId)
{
    setDeltaLink(listId, QString());
}

void Database::upsertTasks(const QString &listId, const QList<Task> &tasks)
{
    auto db = QSqlDatabase::database(m_connectionName);
    db.transaction();

    QSqlQuery del(db);
    del.prepare(QStringLiteral("DELETE FROM tasks WHERE list_id = ?"));
    del.bindValue(0, listId);
    del.exec();

    QSqlQuery delChk(db);
    delChk.prepare(QStringLiteral("DELETE FROM checklist_items WHERE list_id = ?"));
    delChk.bindValue(0, listId);
    delChk.exec();

    QSqlQuery delLinks(db);
    delLinks.prepare(QStringLiteral("DELETE FROM linked_resources WHERE list_id = ?"));
    delLinks.bindValue(0, listId);
    delLinks.exec();

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO tasks(id, list_id, title, status, importance, body, due_utc, "
        "reminder_utc, has_reminder, last_modified, recurrence_json) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    QSqlQuery chk(db);
    chk.prepare(QStringLiteral(
        "INSERT INTO checklist_items(id, task_id, list_id, display_name, is_checked, "
        "created_utc, sort_order) "
        "VALUES(?, ?, ?, ?, ?, ?, ?)"));

    QSqlQuery lnk(db);
    lnk.prepare(QStringLiteral(
        "INSERT INTO linked_resources(id, task_id, list_id, application_name, "
        "web_url, display_name, external_id, sort_order) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?)"));

    for (const auto &t : tasks) {
        q.bindValue(0, t.id);
        q.bindValue(1, listId);
        q.bindValue(2, t.title);
        q.bindValue(3, t.status);
        q.bindValue(4, t.importance.isEmpty() ? QStringLiteral("normal") : t.importance);
        q.bindValue(5, t.body);
        q.bindValue(6, t.dueDate.isValid() ? t.dueDate.toUTC().toString(Qt::ISODate) : QVariant());
        q.bindValue(7, t.reminderDate.isValid() ? t.reminderDate.toUTC().toString(Qt::ISODate) : QVariant());
        q.bindValue(8, t.hasReminder ? 1 : 0);
        q.bindValue(9, t.lastModified.isValid() ? t.lastModified.toUTC().toString(Qt::ISODate) : QVariant());
        q.bindValue(10, t.recurrenceJson.isEmpty() ? QVariant() : t.recurrenceJson);
        q.exec();

        int order = 0;
        for (const auto &c : t.checklistItems) {
            chk.bindValue(0, c.id);
            chk.bindValue(1, t.id);
            chk.bindValue(2, listId);
            chk.bindValue(3, c.displayName);
            chk.bindValue(4, c.isChecked ? 1 : 0);
            chk.bindValue(5, c.createdDateTime.isValid()
                              ? c.createdDateTime.toUTC().toString(Qt::ISODate) : QVariant());
            chk.bindValue(6, order++);
            chk.exec();
        }
        order = 0;
        for (const auto &r : t.linkedResources) {
            lnk.bindValue(0, r.id);
            lnk.bindValue(1, t.id);
            lnk.bindValue(2, listId);
            lnk.bindValue(3, r.applicationName);
            lnk.bindValue(4, r.webUrl);
            lnk.bindValue(5, r.displayName);
            lnk.bindValue(6, r.externalId);
            lnk.bindValue(7, order++);
            lnk.exec();
        }
    }
    db.commit();
}

void Database::applyTaskDelta(const QString &listId,
                              const QList<Task> &changed,
                              const QStringList &deletedIds)
{
    auto db = QSqlDatabase::database(m_connectionName);
    db.transaction();

    QSqlQuery delTask(db);
    delTask.prepare(QStringLiteral("DELETE FROM tasks WHERE id = ?"));
    QSqlQuery delChk(db);
    delChk.prepare(QStringLiteral("DELETE FROM checklist_items WHERE task_id = ?"));
    for (const QString &id : deletedIds) {
        delTask.bindValue(0, id);
        delTask.exec();
        delChk.bindValue(0, id);
        delChk.exec();
    }

    QSqlQuery upsert(db);
    upsert.prepare(QStringLiteral(
        "INSERT INTO tasks(id, list_id, title, status, importance, body, due_utc, "
        "reminder_utc, has_reminder, last_modified, recurrence_json) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET "
        "title=excluded.title, status=excluded.status, importance=excluded.importance, "
        "body=excluded.body, due_utc=excluded.due_utc, reminder_utc=excluded.reminder_utc, "
        "has_reminder=excluded.has_reminder, last_modified=excluded.last_modified, "
        "recurrence_json=excluded.recurrence_json"));
    for (const auto &t : changed) {
        upsert.bindValue(0, t.id);
        upsert.bindValue(1, listId);
        upsert.bindValue(2, t.title);
        upsert.bindValue(3, t.status);
        upsert.bindValue(4, t.importance.isEmpty() ? QStringLiteral("normal") : t.importance);
        upsert.bindValue(5, t.body);
        upsert.bindValue(6, t.dueDate.isValid() ? t.dueDate.toUTC().toString(Qt::ISODate) : QVariant());
        upsert.bindValue(7, t.reminderDate.isValid() ? t.reminderDate.toUTC().toString(Qt::ISODate) : QVariant());
        upsert.bindValue(8, t.hasReminder ? 1 : 0);
        upsert.bindValue(9, t.lastModified.isValid() ? t.lastModified.toUTC().toString(Qt::ISODate) : QVariant());
        upsert.bindValue(10, t.recurrenceJson.isEmpty() ? QVariant() : t.recurrenceJson);
        upsert.exec();
    }
    db.commit();
}

void Database::upsertChecklistItems(const QString &listId, const QString &taskId,
                                    const QList<ChecklistItem> &items)
{
    auto db = QSqlDatabase::database(m_connectionName);
    db.transaction();

    QSqlQuery del(db);
    del.prepare(QStringLiteral("DELETE FROM checklist_items WHERE task_id = ?"));
    del.bindValue(0, taskId);
    del.exec();

    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT INTO checklist_items(id, task_id, list_id, display_name, is_checked, "
        "created_utc, sort_order) VALUES(?, ?, ?, ?, ?, ?, ?)"));
    int order = 0;
    for (const auto &c : items) {
        ins.bindValue(0, c.id);
        ins.bindValue(1, taskId);
        ins.bindValue(2, listId);
        ins.bindValue(3, c.displayName);
        ins.bindValue(4, c.isChecked ? 1 : 0);
        ins.bindValue(5, c.createdDateTime.isValid()
                          ? c.createdDateTime.toUTC().toString(Qt::ISODate) : QVariant());
        ins.bindValue(6, order++);
        ins.exec();
    }
    db.commit();
}

QHash<QString, QList<ChecklistItem>> Database::checklistItemsByTask(const QString &listId) const
{
    QHash<QString, QList<ChecklistItem>> result;
    auto db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT id, task_id, display_name, is_checked, created_utc "
        "FROM checklist_items WHERE list_id = ? "
        "ORDER BY task_id ASC, sort_order ASC"));
    q.bindValue(0, listId);
    q.exec();
    while (q.next()) {
        ChecklistItem c;
        c.id = q.value(0).toString();
        const QString taskId = q.value(1).toString();
        c.displayName = q.value(2).toString();
        c.isChecked = q.value(3).toInt() != 0;
        const QString created = q.value(4).toString();
        if (!created.isEmpty()) c.createdDateTime = QDateTime::fromString(created, Qt::ISODate);
        result[taskId].append(c);
    }
    return result;
}

QList<TaskList> Database::lists() const
{
    QList<TaskList> result;
    auto db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.exec(QStringLiteral("SELECT id, display_name, is_default, is_shared FROM lists "
                          "ORDER BY is_default DESC, display_name ASC"));
    while (q.next()) {
        TaskList l;
        l.id = q.value(0).toString();
        l.displayName = q.value(1).toString();
        l.isDefault = q.value(2).toInt() != 0;
        l.isShared = q.value(3).toInt() != 0;
        result.append(l);
    }
    return result;
}

QList<Task> Database::tasks(const QString &listId) const
{
    QList<Task> result;
    auto db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT id, title, status, importance, body, due_utc, reminder_utc, has_reminder, "
        "last_modified, recurrence_json "
        "FROM tasks WHERE list_id = ? "
        "ORDER BY (status = 'completed') ASC, due_utc ASC NULLS LAST, "
        "(importance = 'high') DESC, title ASC"));
    q.bindValue(0, listId);
    q.exec();
    while (q.next()) {
        Task t;
        t.id = q.value(0).toString();
        t.listId = listId;
        t.title = q.value(1).toString();
        t.status = q.value(2).toString();
        t.importance = q.value(3).toString();
        t.body = q.value(4).toString();
        const QString due = q.value(5).toString();
        if (!due.isEmpty()) t.dueDate = QDateTime::fromString(due, Qt::ISODate);
        const QString rem = q.value(6).toString();
        if (!rem.isEmpty()) t.reminderDate = QDateTime::fromString(rem, Qt::ISODate);
        t.hasReminder = q.value(7).toInt() != 0;
        const QString lm = q.value(8).toString();
        if (!lm.isEmpty()) t.lastModified = QDateTime::fromString(lm, Qt::ISODate);
        t.recurrenceJson = q.value(9).toString();
        result.append(t);
    }

    const auto itemsByTask = checklistItemsByTask(listId);
    const auto linksByTask = linkedResourcesByTask(listId);
    for (auto &t : result) {
        t.checklistItems = itemsByTask.value(t.id);
        t.totalChecklistCount = t.checklistItems.size();
        t.openChecklistCount = 0;
        for (const auto &c : t.checklistItems) {
            if (!c.isChecked) ++t.openChecklistCount;
        }
        t.linkedResources = linksByTask.value(t.id);
    }
    return result;
}

QList<Task> Database::allTasks() const
{
    QList<Task> result;
    auto db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.exec(QStringLiteral(
        "SELECT id, list_id, title, status, importance, body, due_utc, reminder_utc, "
        "has_reminder, last_modified, recurrence_json "
        "FROM tasks "
        "ORDER BY (status = 'completed') ASC, due_utc ASC NULLS LAST, "
        "(importance = 'high') DESC, title ASC"));
    while (q.next()) {
        Task t;
        t.id = q.value(0).toString();
        t.listId = q.value(1).toString();
        t.title = q.value(2).toString();
        t.status = q.value(3).toString();
        t.importance = q.value(4).toString();
        t.body = q.value(5).toString();
        const QString due = q.value(6).toString();
        if (!due.isEmpty()) t.dueDate = QDateTime::fromString(due, Qt::ISODate);
        const QString rem = q.value(7).toString();
        if (!rem.isEmpty()) t.reminderDate = QDateTime::fromString(rem, Qt::ISODate);
        t.hasReminder = q.value(8).toInt() != 0;
        const QString lm = q.value(9).toString();
        if (!lm.isEmpty()) t.lastModified = QDateTime::fromString(lm, Qt::ISODate);
        t.recurrenceJson = q.value(10).toString();
        result.append(t);
    }

    // Hydrate checklist items and linked resources globally — single query
    // each, group by task_id.
    QHash<QString, QList<ChecklistItem>> itemsByTask;
    {
        QSqlQuery cq(db);
        cq.exec(QStringLiteral(
            "SELECT id, task_id, display_name, is_checked, created_utc "
            "FROM checklist_items ORDER BY task_id ASC, sort_order ASC"));
        while (cq.next()) {
            ChecklistItem c;
            c.id = cq.value(0).toString();
            const QString taskId = cq.value(1).toString();
            c.displayName = cq.value(2).toString();
            c.isChecked = cq.value(3).toInt() != 0;
            const QString created = cq.value(4).toString();
            if (!created.isEmpty()) c.createdDateTime = QDateTime::fromString(created, Qt::ISODate);
            itemsByTask[taskId].append(c);
        }
    }
    QHash<QString, QList<LinkedResource>> linksByTask;
    {
        QSqlQuery lq(db);
        lq.exec(QStringLiteral(
            "SELECT id, task_id, application_name, web_url, display_name, external_id "
            "FROM linked_resources ORDER BY task_id ASC, sort_order ASC"));
        while (lq.next()) {
            LinkedResource r;
            r.id = lq.value(0).toString();
            const QString taskId = lq.value(1).toString();
            r.applicationName = lq.value(2).toString();
            r.webUrl = lq.value(3).toString();
            r.displayName = lq.value(4).toString();
            r.externalId = lq.value(5).toString();
            linksByTask[taskId].append(r);
        }
    }
    for (auto &t : result) {
        t.checklistItems = itemsByTask.value(t.id);
        t.totalChecklistCount = t.checklistItems.size();
        t.openChecklistCount = 0;
        for (const auto &c : t.checklistItems) {
            if (!c.isChecked) ++t.openChecklistCount;
        }
        t.linkedResources = linksByTask.value(t.id);
    }
    return result;
}

QHash<QString, QList<LinkedResource>> Database::linkedResourcesByTask(const QString &listId) const
{
    QHash<QString, QList<LinkedResource>> result;
    auto db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT id, task_id, application_name, web_url, display_name, external_id "
        "FROM linked_resources WHERE list_id = ? "
        "ORDER BY task_id ASC, sort_order ASC"));
    q.bindValue(0, listId);
    q.exec();
    while (q.next()) {
        LinkedResource r;
        r.id = q.value(0).toString();
        const QString taskId = q.value(1).toString();
        r.applicationName = q.value(2).toString();
        r.webUrl = q.value(3).toString();
        r.displayName = q.value(4).toString();
        r.externalId = q.value(5).toString();
        result[taskId].append(r);
    }
    return result;
}

void Database::upsertLinkedResources(const QString &listId, const QString &taskId,
                                     const QList<LinkedResource> &items)
{
    auto db = QSqlDatabase::database(m_connectionName);
    db.transaction();

    QSqlQuery del(db);
    del.prepare(QStringLiteral("DELETE FROM linked_resources WHERE task_id = ?"));
    del.bindValue(0, taskId);
    del.exec();

    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT INTO linked_resources(id, task_id, list_id, application_name, "
        "web_url, display_name, external_id, sort_order) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?)"));
    int order = 0;
    for (const auto &r : items) {
        ins.bindValue(0, r.id);
        ins.bindValue(1, taskId);
        ins.bindValue(2, listId);
        ins.bindValue(3, r.applicationName);
        ins.bindValue(4, r.webUrl);
        ins.bindValue(5, r.displayName);
        ins.bindValue(6, r.externalId);
        ins.bindValue(7, order++);
        ins.exec();
    }
    db.commit();
}

int Database::openTaskCountForToday() const
{
    auto db = QSqlDatabase::database(m_connectionName);
    const QDateTime endOfToday = QDateTime(QDate::currentDate(), QTime(23, 59, 59), QTimeZone::UTC);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM tasks WHERE status != 'completed' AND due_utc IS NOT NULL AND due_utc <= ?"));
    q.bindValue(0, endOfToday.toString(Qt::ISODate));
    q.exec();
    if (q.next()) return q.value(0).toInt();
    return 0;
}

} // namespace Merkzettel
