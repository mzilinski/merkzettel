#include "todoapi.h"
#include "graphclient.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrlQuery>
#include <QTimeZone>
#include <QDate>
#include <KLocalizedString>
#include <functional>
#include <memory>

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

ChecklistItem parseChecklistItem(const QJsonObject &obj)
{
    ChecklistItem c;
    c.id = obj.value(QStringLiteral("id")).toString();
    c.displayName = obj.value(QStringLiteral("displayName")).toString();
    c.isChecked = obj.value(QStringLiteral("isChecked")).toBool();
    const QString created = obj.value(QStringLiteral("createdDateTime")).toString();
    if (!created.isEmpty()) {
        c.createdDateTime = QDateTime::fromString(created, Qt::ISODate);
    }
    return c;
}

LinkedResource parseLinkedResource(const QJsonObject &obj)
{
    LinkedResource r;
    r.id = obj.value(QStringLiteral("id")).toString();
    r.applicationName = obj.value(QStringLiteral("applicationName")).toString();
    r.webUrl = obj.value(QStringLiteral("webUrl")).toString();
    r.displayName = obj.value(QStringLiteral("displayName")).toString();
    r.externalId = obj.value(QStringLiteral("externalId")).toString();
    return r;
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

    const QString created = obj.value(QStringLiteral("createdDateTime")).toString();
    if (!created.isEmpty()) {
        t.createdDate = QDateTime::fromString(created, Qt::ISODate);
    }

    const QString lm = obj.value(QStringLiteral("lastModifiedDateTime")).toString();
    if (!lm.isEmpty()) {
        t.lastModified = QDateTime::fromString(lm, Qt::ISODate);
    }

    // patternedRecurrence is a nested object; serialize verbatim so the
    // app doesn't have to track every Graph pattern type.
    const auto rec = obj.value(QStringLiteral("recurrence")).toObject();
    if (!rec.isEmpty()) {
        t.recurrenceJson = QString::fromUtf8(QJsonDocument(rec).toJson(QJsonDocument::Compact));
    }

    for (const auto &c : obj.value(QStringLiteral("categories")).toArray()) {
        const QString name = c.toString();
        if (!name.isEmpty()) t.categories.append(name);
    }

    // checklistItems comes via $expand on the tasks collection. Server orders
    // ascending by createdDateTime; we keep that order.
    const auto items = obj.value(QStringLiteral("checklistItems")).toArray();
    for (const auto &v : items) {
        t.checklistItems.append(parseChecklistItem(v.toObject()));
    }
    t.totalChecklistCount = t.checklistItems.size();
    t.openChecklistCount = 0;
    for (const auto &c : t.checklistItems) {
        if (!c.isChecked) ++t.openChecklistCount;
    }

    t.hasAttachments = obj.value(QStringLiteral("hasAttachments")).toBool();

    const auto links = obj.value(QStringLiteral("linkedResources")).toArray();
    for (const auto &v : links) {
        t.linkedResources.append(parseLinkedResource(v.toObject()));
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
    l.isShared = obj.value(QStringLiteral("isShared")).toBool();
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
    // $expand keeps subtasks in the same roundtrip. Worst case is 200 tasks * 20
    // checklist items = 4000 sub-objects per fetch, which is acceptable. Switch
    // to lazy load (only when the detail sheet opens) if this becomes a hotspot.
    q.addQueryItem(QStringLiteral("$expand"),
                   QStringLiteral("checklistItems,linkedResources"));

    m_graph->get(QStringLiteral("/me/todo/lists/%1/tasks").arg(listId), q,
                 [this, listId](const QJsonValue &val, const QString &err) {
        if (!err.isEmpty()) { Q_EMIT errorOccurred(err); return; }
        QList<Task> tasks;
        for (const auto &v : val.toObject().value(QStringLiteral("value")).toArray()) {
            Task t = parseTask(v.toObject());
            t.listId = listId;
            tasks.append(std::move(t));
        }
        Q_EMIT tasksReceived(listId, tasks);
    });
}

void TodoApi::addTask(const QString &listId, const QString &title, const QDateTime &due,
                      bool important, std::function<void(const QString &)> onCreated)
{
    QJsonObject body;
    body.insert(QStringLiteral("title"), title);
    if (important) {
        body.insert(QStringLiteral("importance"), QStringLiteral("high"));
    }
    if (due.isValid()) {
        body.insert(QStringLiteral("dueDateTime"), toGraphDateTime(due));
    }
    m_graph->post(QStringLiteral("/me/todo/lists/%1/tasks").arg(listId), body,
                  [this, listId, onCreated](const QJsonValue &created, const QString &err) {
        if (!err.isEmpty()) { Q_EMIT errorOccurred(err); return; }
        if (onCreated) onCreated(created.toObject().value(QStringLiteral("id")).toString());
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

void TodoApi::setTaskCategories(const QString &listId, const QString &taskId,
                                const QStringList &categories)
{
    updateTask(listId, taskId, QJsonObject{
        {QStringLiteral("categories"), QJsonArray::fromStringList(categories)},
    });
}

void TodoApi::setTaskRecurrence(const QString &listId, const QString &taskId,
                                const QJsonObject &recurrence)
{
    QJsonObject patch;
    if (recurrence.isEmpty()) {
        // Clearing: explicit JSON null tells Graph to drop the field.
        patch.insert(QStringLiteral("recurrence"), QJsonValue::Null);
    } else {
        // Light validation: auto-fill range.startDate (today) and
        // range.recurrenceTimeZone (UTC) if missing — saves callers from
        // having to know these are required.
        QJsonObject rec = recurrence;
        QJsonObject range = rec.value(QStringLiteral("range")).toObject();
        if (!range.contains(QStringLiteral("startDate"))) {
            range.insert(QStringLiteral("startDate"),
                         QDate::currentDate().toString(Qt::ISODate));
        }
        if (!range.contains(QStringLiteral("recurrenceTimeZone"))) {
            range.insert(QStringLiteral("recurrenceTimeZone"), QStringLiteral("UTC"));
        }
        rec.insert(QStringLiteral("range"), range);
        patch.insert(QStringLiteral("recurrence"), rec);
    }
    updateTask(listId, taskId, patch);
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

void TodoApi::createList(const QString &displayName)
{
    QJsonObject body;
    body.insert(QStringLiteral("displayName"), displayName);
    m_graph->post(QStringLiteral("/me/todo/lists"), body,
                  [this](const QJsonValue &val, const QString &err) {
        if (!err.isEmpty()) { Q_EMIT errorOccurred(err); return; }
        // Emit listCreated first so the App can preselect the new list,
        // then listMutated to trigger a refresh.
        const QString newId = val.toObject().value(QStringLiteral("id")).toString();
        if (!newId.isEmpty()) Q_EMIT listCreated(newId);
        Q_EMIT listMutated();
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

void TodoApi::syncTasks(const QString &listId, const QString &deltaLink)
{
    struct DeltaState {
        QList<Task> changed;
        QStringList deleted;
    };
    auto state = std::make_shared<DeltaState>();
    auto handler = std::make_shared<std::function<void(const QJsonValue &, const QString &)>>();
    *handler = [this, listId, state, handler](const QJsonValue &val, const QString &err) {
        if (!err.isEmpty()) {
            // 410 means the delta token is no longer valid — caller must drop
            // it and run a full fetchTasks fallback.
            if (err.contains(QStringLiteral("HTTP 410"))) {
                Q_EMIT tasksDeltaExpired(listId);
            } else {
                Q_EMIT errorOccurred(err);
            }
            return;
        }
        const auto obj = val.toObject();
        const auto items = obj.value(QStringLiteral("value")).toArray();
        for (const auto &v : items) {
            const auto taskObj = v.toObject();
            // Tasks removed since the last delta carry the @removed annotation
            // (Graph quirk: it's a top-level key, not inside the task body).
            if (taskObj.contains(QStringLiteral("@removed"))) {
                state->deleted.append(taskObj.value(QStringLiteral("id")).toString());
            } else {
                Task t = parseTask(taskObj);
                t.listId = listId;
                state->changed.append(std::move(t));
            }
        }
        const QString nextLink = obj.value(QStringLiteral("@odata.nextLink")).toString();
        if (!nextLink.isEmpty()) {
            m_graph->getAbsolute(nextLink, *handler);
            return;
        }
        const QString newDeltaLink = obj.value(QStringLiteral("@odata.deltaLink")).toString();
        Q_EMIT tasksDelta(listId, state->changed, state->deleted, newDeltaLink);
    };

    if (deltaLink.isEmpty()) {
        m_graph->get(QStringLiteral("/me/todo/lists/%1/tasks/delta").arg(listId), {}, *handler);
    } else {
        m_graph->getAbsolute(deltaLink, *handler);
    }
}

void TodoApi::bootstrapDeltaLink(const QString &listId)
{
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("$deltatoken"), QStringLiteral("latest"));
    m_graph->get(QStringLiteral("/me/todo/lists/%1/tasks/delta").arg(listId), q,
                 [this, listId](const QJsonValue &val, const QString &err) {
        if (!err.isEmpty()) { Q_EMIT errorOccurred(err); return; }
        const QString newDeltaLink = val.toObject()
            .value(QStringLiteral("@odata.deltaLink")).toString();
        Q_EMIT tasksDelta(listId, {}, {}, newDeltaLink);
    });
}

void TodoApi::fetchChecklistItems(const QString &listId, const QString &taskId)
{
    m_graph->get(QStringLiteral("/me/todo/lists/%1/tasks/%2/checklistItems").arg(listId, taskId), {},
                 [this, listId, taskId](const QJsonValue &val, const QString &err) {
        if (!err.isEmpty()) { Q_EMIT errorOccurred(err); return; }
        QList<ChecklistItem> items;
        for (const auto &v : val.toObject().value(QStringLiteral("value")).toArray()) {
            items.append(parseChecklistItem(v.toObject()));
        }
        Q_EMIT checklistItemsReceived(listId, taskId, items);
    });
}

void TodoApi::moveTask(const QString &srcListId, const QString &taskId,
                       const QString &dstListId)
{
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("$expand"), QStringLiteral("checklistItems,linkedResources"));
    const QString srcPath = QStringLiteral("/me/todo/lists/%1/tasks/%2").arg(srcListId, taskId);
    m_graph->get(srcPath, q, [this, srcListId, taskId, dstListId, srcPath](const QJsonValue &v,
                                                                          const QString &err) {
        if (!err.isEmpty()) { Q_EMIT taskMoveFailed(srcListId, taskId, err); return; }
        const QJsonObject src = v.toObject();

        // Whitelist of writable fields; everything else (ids, timestamps,
        // etags, hasAttachments) is server-owned. completedDateTime is left to
        // the server, which stamps it from status=completed.
        static const QStringList kCopied = {
            QStringLiteral("title"), QStringLiteral("body"), QStringLiteral("importance"),
            QStringLiteral("status"), QStringLiteral("dueDateTime"),
            QStringLiteral("startDateTime"),
            QStringLiteral("reminderDateTime"), QStringLiteral("isReminderOn"),
            QStringLiteral("recurrence"), QStringLiteral("categories"),
        };
        QJsonObject body;
        for (const QString &key : kCopied) {
            const QJsonValue val = src.value(key);
            if (!val.isUndefined() && !val.isNull()) body.insert(key, val);
        }

        // Copy steps run one after another (keeps step order, avoids
        // throttling). Each gets the new task's path and reports back.
        using Done = std::function<void(const QString &)>;
        using Step = std::function<void(const QString &dstTaskId, Done)>;
        auto steps = std::make_shared<QList<Step>>();
        const auto postTo = [this, dstListId](const QString &collection, const QJsonObject &payload) {
            return Step([this, dstListId, collection, payload](const QString &dstTaskId, Done done) {
                m_graph->post(QStringLiteral("/me/todo/lists/%1/tasks/%2/%3")
                                  .arg(dstListId, dstTaskId, collection),
                              payload, [done](const QJsonValue &, const QString &e) { done(e); });
            });
        };
        for (const auto &c : src.value(QStringLiteral("checklistItems")).toArray()) {
            const auto o = c.toObject();
            steps->append(postTo(QStringLiteral("checklistItems"), QJsonObject{
                {QStringLiteral("displayName"), o.value(QStringLiteral("displayName"))},
                {QStringLiteral("isChecked"), o.value(QStringLiteral("isChecked"))},
            }));
        }
        for (const auto &r : src.value(QStringLiteral("linkedResources")).toArray()) {
            const QJsonObject o = r.toObject();
            QJsonObject copy;
            for (const char *key : {"webUrl", "applicationName", "displayName", "externalId"}) {
                const QJsonValue val = o.value(QLatin1String(key));
                if (val.isString() && !val.toString().isEmpty()) copy.insert(QLatin1String(key), val);
            }
            steps->append(postTo(QStringLiteral("linkedResources"), copy));
        }

        const auto run = [this, srcListId, taskId, dstListId, srcPath, body, steps] {
            m_graph->post(QStringLiteral("/me/todo/lists/%1/tasks").arg(dstListId), body,
                          [this, srcListId, taskId, dstListId, srcPath, steps](const QJsonValue &created,
                                                                              const QString &err) {
                if (!err.isEmpty()) { Q_EMIT taskMoveFailed(srcListId, taskId, err); return; }
                const QString newId = created.toObject().value(QStringLiteral("id")).toString();

                auto next = std::make_shared<std::function<void()>>();
                *next = [this, srcListId, taskId, dstListId, srcPath, newId, steps, next] {
                    if (steps->isEmpty()) {
                        *next = nullptr;  // break the self-reference
                        m_graph->del(srcPath, [this, srcListId, taskId, dstListId, newId](
                                                  const QJsonValue &, const QString &err) {
                            if (!err.isEmpty()) {
                                // Copy exists, original still there: report it
                                // so the caller refreshes both lists.
                                Q_EMIT taskMoveFailed(srcListId, taskId, err);
                                Q_EMIT taskMutated(dstListId);
                                return;
                            }
                            Q_EMIT taskMoved(srcListId, taskId, dstListId, newId);
                        });
                        return;
                    }
                    const Step step = steps->takeFirst();
                    step(newId, [this, srcListId, taskId, dstListId, next](const QString &err) {
                        if (!err.isEmpty()) {
                            *next = nullptr;
                            Q_EMIT taskMoveFailed(srcListId, taskId, err);
                            Q_EMIT taskMutated(dstListId);
                            return;
                        }
                        (*next)();
                    });
                };
                (*next)();
            });
        };

        if (!src.value(QStringLiteral("hasAttachments")).toBool()) {
            run();
            return;
        }
        // Attachments: list them first, then download + re-upload each one
        // as part of the copy steps.
        m_graph->get(srcPath + QStringLiteral("/attachments"), {},
                     [this, srcListId, taskId, dstListId, srcPath, steps, run](const QJsonValue &v,
                                                                             const QString &err) {
            if (!err.isEmpty()) { Q_EMIT taskMoveFailed(srcListId, taskId, err); return; }
            for (const auto &a : v.toObject().value(QStringLiteral("value")).toArray()) {
                const QJsonObject o = a.toObject();
                const QString id = o.value(QStringLiteral("id")).toString();
                const QString name = o.value(QStringLiteral("name")).toString();
                const QString type = o.value(QStringLiteral("contentType")).toString();
                steps->append([this, srcPath, dstListId, id, name, type](const QString &dstTaskId,
                                                                        Done done) {
                    m_graph->getBytes(srcPath + QStringLiteral("/attachments/%1/$value").arg(id),
                                      [this, dstListId, dstTaskId, name, type, done](
                                          const QByteArray &data, const QString &e) {
                        if (!e.isEmpty()) { done(e); return; }
                        uploadAttachmentImpl(dstListId, dstTaskId, name, type, data, done);
                    });
                });
            }
            run();
        });
    });
}

namespace {
QList<Attachment> parseAttachments(const QJsonValue &v)
{
    QList<Attachment> out;
    for (const auto &a : v.toObject().value(QStringLiteral("value")).toArray()) {
        const QJsonObject o = a.toObject();
        Attachment att;
        att.id = o.value(QStringLiteral("id")).toString();
        att.name = o.value(QStringLiteral("name")).toString();
        att.contentType = o.value(QStringLiteral("contentType")).toString();
        att.size = o.value(QStringLiteral("size")).toInteger();
        out.append(att);
    }
    return out;
}
} // namespace

void TodoApi::fetchAttachments(const QString &listId, const QString &taskId)
{
    m_graph->get(QStringLiteral("/me/todo/lists/%1/tasks/%2/attachments").arg(listId, taskId), {},
                 [this, listId, taskId](const QJsonValue &v, const QString &err) {
        if (!err.isEmpty()) { Q_EMIT errorOccurred(err); return; }
        Q_EMIT attachmentsReceived(listId, taskId, parseAttachments(v));
    });
}

void TodoApi::uploadAttachment(const QString &listId, const QString &taskId, const QString &name,
                               const QString &contentType, const QByteArray &data)
{
    uploadAttachmentImpl(listId, taskId, name, contentType, data,
                         [this, listId, taskId](const QString &err) {
        if (!err.isEmpty()) { Q_EMIT errorOccurred(err); return; }
        Q_EMIT attachmentMutated(listId, taskId);
    });
}

void TodoApi::uploadAttachmentImpl(const QString &listId, const QString &taskId,
                                   const QString &name, const QString &contentType,
                                   const QByteArray &data,
                                   std::function<void(const QString &)> done)
{
    if (data.size() > kMaxAttachmentSize) {
        done(i18n("\"%1\" is larger than 25 MB, the Microsoft To Do limit.", name));
        return;
    }
    const QString base = QStringLiteral("/me/todo/lists/%1/tasks/%2/attachments").arg(listId, taskId);
    const QString type = contentType.isEmpty() ? QStringLiteral("application/octet-stream")
                                               : contentType;

    // Small files go inline as base64 (Graph's limit for this is 3 MB).
    constexpr qint64 kInlineLimit = 3 * 1024 * 1024;
    if (data.size() < kInlineLimit) {
        const QJsonObject body{
            {QStringLiteral("@odata.type"), QStringLiteral("#microsoft.graph.taskFileAttachment")},
            {QStringLiteral("name"), name},
            {QStringLiteral("contentType"), type},
            {QStringLiteral("contentBytes"), QString::fromLatin1(data.toBase64())},
        };
        m_graph->post(base, body, [done](const QJsonValue &, const QString &err) { done(err); });
        return;
    }

    const QJsonObject session{
        {QStringLiteral("attachmentInfo"), QJsonObject{
            {QStringLiteral("attachmentType"), QStringLiteral("file")},
            {QStringLiteral("name"), name},
            {QStringLiteral("size"), data.size()},
        }},
    };
    m_graph->post(base + QStringLiteral("/createUploadSession"), session,
                  [this, data, done](const QJsonValue &v, const QString &err) {
        if (!err.isEmpty()) { done(err); return; }
        const QUrl uploadUrl(v.toObject().value(QStringLiteral("uploadUrl")).toString());
        // Chunks must be multiples of 320 KiB (Graph upload-session rule).
        constexpr qint64 kChunk = 10 * 320 * 1024;
        auto sendFrom = std::make_shared<std::function<void(qint64)>>();
        *sendFrom = [this, uploadUrl, data, done, sendFrom](qint64 offset) {
            const QByteArray chunk = data.mid(offset, kChunk);
            m_graph->putUploadChunk(uploadUrl, chunk, offset, data.size(),
                                    [done, sendFrom, offset, size = chunk.size(),
                                     total = data.size()](const QJsonValue &, const QString &err) {
                const qint64 nextOffset = offset + size;
                if (!err.isEmpty() || nextOffset >= total) {
                    *sendFrom = nullptr;
                    done(err);
                    return;
                }
                (*sendFrom)(nextOffset);
            });
        };
        (*sendFrom)(0);
    });
}

void TodoApi::downloadAttachment(const QString &listId, const QString &taskId,
                                 const QString &attachmentId,
                                 std::function<void(const QByteArray &, const QString &)> cb)
{
    m_graph->getBytes(QStringLiteral("/me/todo/lists/%1/tasks/%2/attachments/%3/$value")
                          .arg(listId, taskId, attachmentId),
                      std::move(cb));
}

void TodoApi::deleteAttachment(const QString &listId, const QString &taskId,
                               const QString &attachmentId)
{
    m_graph->del(QStringLiteral("/me/todo/lists/%1/tasks/%2/attachments/%3")
                     .arg(listId, taskId, attachmentId),
                 [this, listId, taskId](const QJsonValue &, const QString &err) {
        if (!err.isEmpty()) { Q_EMIT errorOccurred(err); return; }
        Q_EMIT attachmentMutated(listId, taskId);
    });
}

void TodoApi::addChecklistItem(const QString &listId, const QString &taskId,
                               const QString &displayName)
{
    QJsonObject body;
    body.insert(QStringLiteral("displayName"), displayName);
    body.insert(QStringLiteral("isChecked"), false);
    m_graph->post(QStringLiteral("/me/todo/lists/%1/tasks/%2/checklistItems").arg(listId, taskId), body,
                  [this, listId, taskId](const QJsonValue &, const QString &err) {
        if (!err.isEmpty()) { Q_EMIT errorOccurred(err); return; }
        Q_EMIT checklistItemMutated(listId, taskId);
    });
}

void TodoApi::setChecklistItemChecked(const QString &listId, const QString &taskId,
                                      const QString &itemId, bool checked)
{
    QJsonObject patch;
    patch.insert(QStringLiteral("isChecked"), checked);
    m_graph->patch(QStringLiteral("/me/todo/lists/%1/tasks/%2/checklistItems/%3")
                       .arg(listId, taskId, itemId), patch,
                   [this, listId, taskId](const QJsonValue &, const QString &err) {
        if (!err.isEmpty()) { Q_EMIT errorOccurred(err); return; }
        Q_EMIT checklistItemMutated(listId, taskId);
    });
}

void TodoApi::renameChecklistItem(const QString &listId, const QString &taskId,
                                  const QString &itemId, const QString &displayName)
{
    QJsonObject patch;
    patch.insert(QStringLiteral("displayName"), displayName);
    m_graph->patch(QStringLiteral("/me/todo/lists/%1/tasks/%2/checklistItems/%3")
                       .arg(listId, taskId, itemId), patch,
                   [this, listId, taskId](const QJsonValue &, const QString &err) {
        if (!err.isEmpty()) { Q_EMIT errorOccurred(err); return; }
        Q_EMIT checklistItemMutated(listId, taskId);
    });
}

void TodoApi::deleteChecklistItem(const QString &listId, const QString &taskId,
                                  const QString &itemId)
{
    m_graph->del(QStringLiteral("/me/todo/lists/%1/tasks/%2/checklistItems/%3")
                     .arg(listId, taskId, itemId),
                 [this, listId, taskId](const QJsonValue &, const QString &err) {
        if (!err.isEmpty()) { Q_EMIT errorOccurred(err); return; }
        Q_EMIT checklistItemMutated(listId, taskId);
    });
}

void TodoApi::fetchLinkedResources(const QString &listId, const QString &taskId)
{
    m_graph->get(QStringLiteral("/me/todo/lists/%1/tasks/%2/linkedResources").arg(listId, taskId), {},
                 [this, listId, taskId](const QJsonValue &val, const QString &err) {
        if (!err.isEmpty()) { Q_EMIT errorOccurred(err); return; }
        QList<LinkedResource> items;
        for (const auto &v : val.toObject().value(QStringLiteral("value")).toArray()) {
            items.append(parseLinkedResource(v.toObject()));
        }
        Q_EMIT linkedResourcesReceived(listId, taskId, items);
    });
}

void TodoApi::addLinkedResource(const QString &listId, const QString &taskId,
                                const QString &applicationName,
                                const QString &webUrl,
                                const QString &displayName)
{
    QJsonObject body;
    body.insert(QStringLiteral("applicationName"), applicationName);
    body.insert(QStringLiteral("webUrl"), webUrl);
    body.insert(QStringLiteral("displayName"), displayName);
    m_graph->post(QStringLiteral("/me/todo/lists/%1/tasks/%2/linkedResources").arg(listId, taskId),
                  body,
                  [this, listId, taskId](const QJsonValue &, const QString &err) {
        if (!err.isEmpty()) { Q_EMIT errorOccurred(err); return; }
        Q_EMIT linkedResourceMutated(listId, taskId);
    });
}

void TodoApi::removeLinkedResource(const QString &listId, const QString &taskId,
                                   const QString &resourceId)
{
    m_graph->del(QStringLiteral("/me/todo/lists/%1/tasks/%2/linkedResources/%3")
                     .arg(listId, taskId, resourceId),
                 [this, listId, taskId](const QJsonValue &, const QString &err) {
        if (!err.isEmpty()) { Q_EMIT errorOccurred(err); return; }
        Q_EMIT linkedResourceMutated(listId, taskId);
    });
}

} // namespace Merkzettel
