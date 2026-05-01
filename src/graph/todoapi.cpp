#include "todoapi.h"
#include "graphclient.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QTimeZone>

namespace Merkzettel {

namespace {

QDateTime parseGraphDateTime(const QJsonObject &obj)
{
    const QString dt = obj.value(QStringLiteral("dateTime")).toString();
    if (dt.isEmpty()) return {};

    const QString tz = obj.value(QStringLiteral("timeZone")).toString();
    QDateTime parsed = QDateTime::fromString(dt, Qt::ISODate);
    if (!parsed.isValid()) return {};

    if (parsed.timeSpec() == Qt::LocalTime) {
        // Graph delivers naive ISO without offset; the timeZone field is separate.
        if (tz == QLatin1String("UTC") || tz.isEmpty()) {
            parsed.setTimeZone(QTimeZone::UTC);
        } else {
            QTimeZone zone(tz.toUtf8());
            if (zone.isValid()) parsed.setTimeZone(zone);
            else parsed.setTimeZone(QTimeZone::UTC);
        }
    }
    return parsed.toUTC();
}

QJsonObject toGraphDateTime(const QDateTime &dt)
{
    return {
        {QStringLiteral("dateTime"), dt.toUTC().toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss.000000"))},
        {QStringLiteral("timeZone"), QStringLiteral("UTC")},
    };
}

Task parseTask(const QJsonObject &obj)
{
    Task t;
    t.id = obj.value(QStringLiteral("id")).toString();
    t.title = obj.value(QStringLiteral("title")).toString();
    t.status = obj.value(QStringLiteral("status")).toString();
    t.importance = obj.value(QStringLiteral("importance")).toString();
    t.body = obj.value(QStringLiteral("body")).toObject()
                .value(QStringLiteral("content")).toString();

    t.dueDate = parseGraphDateTime(obj.value(QStringLiteral("dueDateTime")).toObject());
    t.reminderDate = parseGraphDateTime(obj.value(QStringLiteral("reminderDateTime")).toObject());
    t.hasReminder = obj.value(QStringLiteral("isReminderOn")).toBool();

    const QString lm = obj.value(QStringLiteral("lastModifiedDateTime")).toString();
    if (!lm.isEmpty()) {
        t.lastModified = QDateTime::fromString(lm, Qt::ISODate);
    }
    return t;
}

TaskList parseList(const QJsonObject &obj)
{
    TaskList l;
    l.id = obj.value(QStringLiteral("id")).toString();
    l.displayName = obj.value(QStringLiteral("displayName")).toString();
    l.isDefault = obj.value(QStringLiteral("wellknownListName")).toString()
                  == QLatin1String("defaultList");
    return l;
}

} // namespace

TodoApi::TodoApi(GraphClient *graph, QObject *parent)
    : QObject(parent), m_graph(graph) {}

void TodoApi::fetchLists()
{
    m_graph->get(QStringLiteral("/me/todo/lists"), {},
                 [this](const QJsonValue &val, const QString &err) {
        if (!err.isEmpty()) { Q_EMIT errorOccurred(err); return; }
        QList<TaskList> lists;
        for (const auto &v : val.toObject().value(QStringLiteral("value")).toArray()) {
            lists.append(parseList(v.toObject()));
        }
        Q_EMIT listsReceived(lists);
    });
}

void TodoApi::fetchTasks(const QString &listId)
{
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("$top"), QStringLiteral("200"));
    q.addQueryItem(QStringLiteral("$orderby"), QStringLiteral("createdDateTime desc"));

    m_graph->get(QStringLiteral("/me/todo/lists/%1/tasks").arg(listId), q,
                 [this, listId](const QJsonValue &val, const QString &err) {
        if (!err.isEmpty()) { Q_EMIT errorOccurred(err); return; }
        QList<Task> tasks;
        for (const auto &v : val.toObject().value(QStringLiteral("value")).toArray()) {
            tasks.append(parseTask(v.toObject()));
        }
        Q_EMIT tasksReceived(listId, tasks);
    });
}

void TodoApi::addTask(const QString &listId, const QString &title, const QDateTime &due)
{
    QJsonObject body;
    body.insert(QStringLiteral("title"), title);
    if (due.isValid()) {
        body.insert(QStringLiteral("dueDateTime"), toGraphDateTime(due));
    }
    m_graph->post(QStringLiteral("/me/todo/lists/%1/tasks").arg(listId), body,
                  [this, listId](const QJsonValue &, const QString &err) {
        if (!err.isEmpty()) { Q_EMIT errorOccurred(err); return; }
        Q_EMIT taskMutated(listId);
    });
}

void TodoApi::setTaskStatus(const QString &listId, const QString &taskId, const QString &status)
{
    updateTask(listId, taskId, {{QStringLiteral("status"), status}});
}

void TodoApi::setTaskImportance(const QString &listId, const QString &taskId, const QString &importance)
{
    updateTask(listId, taskId, {{QStringLiteral("importance"), importance}});
}

void TodoApi::setTaskDueDate(const QString &listId, const QString &taskId, const QDateTime &due)
{
    QJsonObject patch;
    if (due.isValid()) {
        patch.insert(QStringLiteral("dueDateTime"), toGraphDateTime(due));
    } else {
        patch.insert(QStringLiteral("dueDateTime"), QJsonValue());
    }
    updateTask(listId, taskId, patch);
}

void TodoApi::setTaskReminder(const QString &listId, const QString &taskId, const QDateTime &when)
{
    QJsonObject patch;
    patch.insert(QStringLiteral("isReminderOn"), true);
    patch.insert(QStringLiteral("reminderDateTime"), toGraphDateTime(when));
    updateTask(listId, taskId, patch);
}

void TodoApi::clearTaskReminder(const QString &listId, const QString &taskId)
{
    updateTask(listId, taskId, {{QStringLiteral("isReminderOn"), false}});
}

void TodoApi::setTaskTitle(const QString &listId, const QString &taskId, const QString &title)
{
    updateTask(listId, taskId, {{QStringLiteral("title"), title}});
}

void TodoApi::setTaskBody(const QString &listId, const QString &taskId, const QString &body)
{
    QJsonObject bodyObj{
        {QStringLiteral("contentType"), QStringLiteral("text")},
        {QStringLiteral("content"), body},
    };
    updateTask(listId, taskId, {{QStringLiteral("body"), bodyObj}});
}

void TodoApi::updateTask(const QString &listId, const QString &taskId, const QJsonObject &patch)
{
    m_graph->patch(QStringLiteral("/me/todo/lists/%1/tasks/%2").arg(listId, taskId), patch,
                   [this, listId](const QJsonValue &, const QString &err) {
        if (!err.isEmpty()) { Q_EMIT errorOccurred(err); return; }
        Q_EMIT taskMutated(listId);
    });
}

void TodoApi::deleteTask(const QString &listId, const QString &taskId)
{
    m_graph->del(QStringLiteral("/me/todo/lists/%1/tasks/%2").arg(listId, taskId),
                 [this, listId](const QJsonValue &, const QString &err) {
        if (!err.isEmpty()) { Q_EMIT errorOccurred(err); return; }
        Q_EMIT taskMutated(listId);
    });
}

void TodoApi::deleteList(const QString &listId)
{
    m_graph->del(QStringLiteral("/me/todo/lists/%1").arg(listId),
                 [this](const QJsonValue &, const QString &err) {
        if (!err.isEmpty()) { Q_EMIT errorOccurred(err); return; }
        Q_EMIT listMutated();
    });
}

void TodoApi::renameList(const QString &listId, const QString &newName)
{
    QJsonObject body;
    body.insert(QStringLiteral("displayName"), newName);
    m_graph->patch(QStringLiteral("/me/todo/lists/%1").arg(listId), body,
                   [this](const QJsonValue &, const QString &err) {
        if (!err.isEmpty()) { Q_EMIT errorOccurred(err); return; }
        Q_EMIT listMutated();
    });
}

} // namespace Merkzettel
