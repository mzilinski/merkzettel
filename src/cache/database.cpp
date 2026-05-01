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
        "  is_default INTEGER NOT NULL DEFAULT 0"
        ")"));
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
        "  last_modified TEXT"
        ")"));
    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_tasks_list ON tasks(list_id)"));

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
    q.prepare(QStringLiteral(
        "INSERT INTO lists(id, display_name, is_default) VALUES(?, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET display_name=excluded.display_name, "
        "is_default=excluded.is_default"));
    for (const auto &l : lists) {
        q.bindValue(0, l.id);
        q.bindValue(1, l.displayName);
        q.bindValue(2, l.isDefault ? 1 : 0);
        q.exec();
    }
    db.commit();
}

void Database::upsertTasks(const QString &listId, const QList<Task> &tasks)
{
    auto db = QSqlDatabase::database(m_connectionName);
    db.transaction();

    QSqlQuery del(db);
    del.prepare(QStringLiteral("DELETE FROM tasks WHERE list_id = ?"));
    del.bindValue(0, listId);
    del.exec();

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO tasks(id, list_id, title, status, importance, body, due_utc, "
        "reminder_utc, has_reminder, last_modified) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
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
        q.exec();
    }
    db.commit();
}

QList<TaskList> Database::lists() const
{
    QList<TaskList> result;
    auto db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.exec(QStringLiteral("SELECT id, display_name, is_default FROM lists ORDER BY is_default DESC, display_name ASC"));
    while (q.next()) {
        TaskList l;
        l.id = q.value(0).toString();
        l.displayName = q.value(1).toString();
        l.isDefault = q.value(2).toInt() != 0;
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
        "SELECT id, title, status, importance, body, due_utc, reminder_utc, has_reminder, last_modified "
        "FROM tasks WHERE list_id = ? "
        "ORDER BY (status = 'completed') ASC, due_utc ASC NULLS LAST, "
        "(importance = 'high') DESC, title ASC"));
    q.bindValue(0, listId);
    q.exec();
    while (q.next()) {
        Task t;
        t.id = q.value(0).toString();
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
        result.append(t);
    }
    return result;
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
