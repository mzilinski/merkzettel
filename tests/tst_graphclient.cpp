// SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

#include "auth/authmanager.h"
#include "graph/graphclient.h"
#include "graph/todoapi.h"
#include "mockhttpserver.h"

using namespace Merkzettel;

namespace {
QByteArray json(const QJsonObject &o)
{
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

QJsonObject task(int i)
{
    return {
        {QStringLiteral("id"), QStringLiteral("t%1").arg(i)},
        {QStringLiteral("title"), QStringLiteral("Task %1").arg(i)},
        {QStringLiteral("status"), QStringLiteral("notStarted")},
        {QStringLiteral("importance"), QStringLiteral("normal")},
    };
}
} // namespace

class TestGraphClient : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void limitsConcurrency()
    {
        MockHttpServer server([](const MockHttpServer::Request &) {
            MockHttpServer::Response r;
            r.delayMs = 150;
            return r;
        });
        AuthManager auth;
        GraphClient graph(&auth);
        graph.setBaseUrl(server.baseUrl());

        int done = 0;
        for (int i = 0; i < 10; ++i) {
            graph.get(QStringLiteral("/item/%1").arg(i), {},
                      [&done](const QJsonValue &, const QString &err) {
                QVERIFY(err.isEmpty());
                ++done;
            });
        }
        QTRY_COMPARE_WITH_TIMEOUT(done, 10, 5000);
        QCOMPARE(server.requestCount(), 10);
        QVERIFY(server.maxConcurrent() <= 4);
        QVERIFY(server.maxConcurrent() >= 2); // the queue does run in parallel
    }

    void retriesAfterThrottling()
    {
        int throttled = 0;
        MockHttpServer server([&throttled](const MockHttpServer::Request &) {
            MockHttpServer::Response r;
            if (throttled < 2) {
                ++throttled;
                r.status = 429;
                r.headers.insert("Retry-After", "1");
                r.body = R"({"error":{"code":"TooManyRequests","message":"slow down"}})";
            } else {
                r.body = R"({"ok":true})";
            }
            return r;
        });
        AuthManager auth;
        GraphClient graph(&auth);
        graph.setBaseUrl(server.baseUrl());

        QString error = QStringLiteral("pending");
        bool ok = false;
        QElapsedTimer timer;
        timer.start();
        graph.get(QStringLiteral("/x"), {}, [&](const QJsonValue &v, const QString &err) {
            error = err;
            ok = v.toObject().value(QStringLiteral("ok")).toBool();
        });
        QTRY_VERIFY_WITH_TIMEOUT(error != QStringLiteral("pending"), 10000);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(ok);
        QCOMPARE(server.requestCount(), 3);
        QVERIFY(timer.elapsed() >= 1900); // waited Retry-After twice
    }

    void givesUpAfterBoundedRetries()
    {
        MockHttpServer server([](const MockHttpServer::Request &) {
            MockHttpServer::Response r;
            r.status = 503;
            r.headers.insert("Retry-After", "1");
            r.body = R"({"error":{"message":"unavailable"}})";
            return r;
        });
        AuthManager auth;
        GraphClient graph(&auth);
        graph.setBaseUrl(server.baseUrl());

        QString error;
        graph.get(QStringLiteral("/x"), {},
                  [&error](const QJsonValue &, const QString &err) { error = err; });
        QTRY_VERIFY_WITH_TIMEOUT(!error.isEmpty(), 20000);
        QVERIFY(error.startsWith(QStringLiteral("HTTP 503")));
        QCOMPARE(server.requestCount(), 6); // first try + 5 retries
    }

    void fetchTasksFollowsNextLink()
    {
        QString base;
        MockHttpServer server([&base](const MockHttpServer::Request &req) {
            MockHttpServer::Response r;
            QJsonArray value;
            if (req.path.contains("page=2")) {
                for (int i = 200; i < 250; ++i)
                    value.append(task(i));
                r.body = json({{QStringLiteral("value"), value}});
            } else {
                for (int i = 0; i < 200; ++i)
                    value.append(task(i));
                r.body = json({
                    {QStringLiteral("value"), value},
                    {QStringLiteral("@odata.nextLink"),
                     QString(base + QStringLiteral("/me/todo/lists/L/tasks?page=2"))},
                });
            }
            return r;
        });
        base = server.baseUrl();
        AuthManager auth;
        GraphClient graph(&auth);
        graph.setBaseUrl(base);
        TodoApi api(&graph);

        QSignalSpy spy(&api, &TodoApi::tasksReceived);
        api.fetchTasks(QStringLiteral("L"));
        QVERIFY(spy.wait(5000));
        QCOMPARE(spy.count(), 1); // one emission with everything, not per page
        const auto tasks = spy.at(0).at(1).value<QList<Task>>();
        QCOMPARE(tasks.size(), 250);
        QCOMPARE(tasks.last().id, QStringLiteral("t249"));
        QCOMPARE(tasks.first().listId, QStringLiteral("L"));
    }

    void deltaSyncCollectsChangesAndRemovals()
    {
        QString base;
        MockHttpServer server([&base](const MockHttpServer::Request &req) {
            MockHttpServer::Response r;
            if (req.path.contains("page=2")) {
                r.body = json({
                    {QStringLiteral("value"),
                     QJsonArray {QJsonObject {
                         {QStringLiteral("id"), QStringLiteral("gone")},
                         {QStringLiteral("@removed"),
                          QJsonObject {{QStringLiteral("reason"), QStringLiteral("deleted")}}},
                     }}},
                    {QStringLiteral("@odata.deltaLink"),
                     QString(base + QStringLiteral("/delta?token=next"))},
                });
            } else {
                r.body = json({
                    {QStringLiteral("value"), QJsonArray {task(1)}},
                    {QStringLiteral("@odata.nextLink"),
                     QString(base + QStringLiteral("/delta?page=2"))},
                });
            }
            return r;
        });
        base = server.baseUrl();
        AuthManager auth;
        GraphClient graph(&auth);
        graph.setBaseUrl(base);
        TodoApi api(&graph);

        QSignalSpy spy(&api, &TodoApi::tasksDelta);
        api.syncTasks(QStringLiteral("L"), base + QStringLiteral("/delta?token=old"));
        QVERIFY(spy.wait(5000));
        const auto changed = spy.at(0).at(1).value<QList<Task>>();
        const auto removed = spy.at(0).at(2).toStringList();
        QCOMPARE(changed.size(), 1);
        QCOMPARE(changed.first().id, QStringLiteral("t1"));
        QCOMPARE(removed, QStringList {QStringLiteral("gone")});
        QVERIFY(spy.at(0).at(3).toString().endsWith(QStringLiteral("token=next")));
    }

    void expiredDeltaTokenRequestsFullFetch()
    {
        MockHttpServer server([](const MockHttpServer::Request &) {
            MockHttpServer::Response r;
            r.status = 410;
            r.body = R"({"error":{"message":"SyncStateNotFound"}})";
            return r;
        });
        AuthManager auth;
        GraphClient graph(&auth);
        graph.setBaseUrl(server.baseUrl());
        TodoApi api(&graph);

        QSignalSpy expired(&api, &TodoApi::tasksDeltaExpired);
        api.syncTasks(QStringLiteral("L"), server.baseUrl() + QStringLiteral("/delta?token=old"));
        QVERIFY(expired.wait(5000));
        QCOMPARE(expired.at(0).at(0).toString(), QStringLiteral("L"));
    }

    void addTaskReportsCreatedId()
    {
        QByteArray postedBody;
        MockHttpServer server([&postedBody](const MockHttpServer::Request &req) {
            MockHttpServer::Response r;
            postedBody = req.body;
            r.status = 201;
            r.body = R"({"id":"new-id"})";
            return r;
        });
        AuthManager auth;
        GraphClient graph(&auth);
        graph.setBaseUrl(server.baseUrl());
        TodoApi api(&graph);

        QString createdId;
        api.addTask(QStringLiteral("L"), QStringLiteral("Milk"), {}, true,
                    [&createdId](const QString &id) { createdId = id; });
        QTRY_COMPARE_WITH_TIMEOUT(createdId, QStringLiteral("new-id"), 5000);
        const QJsonObject body = QJsonDocument::fromJson(postedBody).object();
        QCOMPARE(body.value(QStringLiteral("title")).toString(), QStringLiteral("Milk"));
        QCOMPARE(body.value(QStringLiteral("importance")).toString(), QStringLiteral("high"));
    }
};

QTEST_GUILESS_MAIN(TestGraphClient)
#include "tst_graphclient.moc"
