// SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QDir>
#include <QElapsedTimer>
#include <QStandardPaths>
#include <QTest>

#include "cache/database.h"

using namespace Merkzettel;

namespace {
Task makeTask(const QString &id, const QString &title)
{
    Task t;
    t.id = id;
    t.title = title;
    t.status = QStringLiteral("notStarted");
    t.importance = QStringLiteral("normal");
    return t;
}
} // namespace

class TestDatabase : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // Redirects AppDataLocation to ~/.qttest so the real cache is untouched.
        QStandardPaths::setTestModeEnabled(true);
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).removeRecursively();
    }

    void roundTripsTasks()
    {
        Database db;
        QVERIFY(db.open());
        db.upsertLists({{QStringLiteral("L"), QStringLiteral("List"), true}});

        Task t = makeTask(QStringLiteral("a"), QStringLiteral("Alpha"));
        t.importance = QStringLiteral("high");
        t.body = QStringLiteral("notes");
        t.categories = {QStringLiteral("Work"), QStringLiteral("Finance")};
        t.hasAttachments = true;
        t.dueDate = QDateTime(QDate(2026, 10, 20), QTime(9, 0), QTimeZone::UTC);
        t.recurrenceJson = QStringLiteral(R"({"pattern":{"type":"daily","interval":2}})");
        t.checklistItems = {{QStringLiteral("c1"), QStringLiteral("Step"), {}, true}};
        db.upsertTasks(QStringLiteral("L"),
                       {t, makeTask(QStringLiteral("b"), QStringLiteral("Beta"))});

        const auto tasks = db.tasks(QStringLiteral("L"));
        QCOMPARE(tasks.size(), 2);
        const auto it = std::find_if(tasks.cbegin(), tasks.cend(),
                                     [](const Task &x) { return x.id == QLatin1String("a"); });
        QVERIFY(it != tasks.cend());
        QCOMPARE(it->title, QStringLiteral("Alpha"));
        QCOMPARE(it->importance, QStringLiteral("high"));
        QCOMPARE(it->body, QStringLiteral("notes"));
        QCOMPARE(it->categories, (QStringList {QStringLiteral("Work"), QStringLiteral("Finance")}));
        QVERIFY(it->hasAttachments);
        QCOMPARE(it->dueDate, t.dueDate);
        QCOMPARE(it->recurrenceJson, t.recurrenceJson);
        QCOMPARE(it->totalChecklistCount, 1);
        QCOMPARE(it->openChecklistCount, 0);
        QCOMPARE(db.allTasks().size(), 2);
    }

    void fullFetchReplacesList()
    {
        Database db;
        QVERIFY(db.open());
        db.upsertTasks(QStringLiteral("L"),
                       {makeTask(QStringLiteral("only"), QStringLiteral("Only"))});
        const auto tasks = db.tasks(QStringLiteral("L"));
        QCOMPARE(tasks.size(), 1);
        QCOMPARE(tasks.first().id, QStringLiteral("only"));
    }

    void deltaUpdatesAndRemoves()
    {
        Database db;
        QVERIFY(db.open());
        db.upsertTasks(QStringLiteral("L"),
                       {makeTask(QStringLiteral("x"), QStringLiteral("Old")),
                        makeTask(QStringLiteral("y"), QStringLiteral("Gone"))});
        db.applyTaskDelta(QStringLiteral("L"),
                          {makeTask(QStringLiteral("x"), QStringLiteral("New")),
                           makeTask(QStringLiteral("z"), QStringLiteral("Added"))},
                          {QStringLiteral("y")});
        QStringList titles;
        const auto cachedTasks = db.tasks(QStringLiteral("L"));
        for (const auto &t : cachedTasks)
            titles.append(t.title);
        titles.sort();
        QCOMPARE(titles, (QStringList {QStringLiteral("Added"), QStringLiteral("New")}));

        db.setDeltaLink(QStringLiteral("L"), QStringLiteral("https://delta"));
        QCOMPARE(db.deltaLink(QStringLiteral("L")), QStringLiteral("https://delta"));
        db.clearDeltaLink(QStringLiteral("L"));
        QVERIFY(db.deltaLink(QStringLiteral("L")).isEmpty());
    }

    void handlesLargeLists()
    {
        Database db;
        QVERIFY(db.open());
        QList<Task> many;
        for (int i = 0; i < 2000; ++i)
            many.append(makeTask(QStringLiteral("id%1").arg(i), QStringLiteral("Task %1").arg(i)));
        QElapsedTimer timer;
        timer.start();
        db.upsertTasks(QStringLiteral("Big"), many);
        QCOMPARE(db.tasks(QStringLiteral("Big")).size(), 2000);
        QVERIFY2(timer.elapsed() < 5000, "2000 tasks should cache well under 5 s");
    }

    void myDayIsPerDay()
    {
        Database db;
        QVERIFY(db.open());
        const QDate day1(2026, 10, 15);
        db.setInMyDay({QStringLiteral("a"), QStringLiteral("b")}, true, day1);
        QCOMPARE(db.myDayTaskIds(day1), (QSet<QString> {QStringLiteral("a"), QStringLiteral("b")}));

        db.setInMyDay({QStringLiteral("b")}, false, day1);
        db.renameMyDayTask(QStringLiteral("a"), QStringLiteral("a2"));
        QCOMPARE(db.myDayTaskIds(day1), QSet<QString> {QStringLiteral("a2")});

        // Next day: yesterday's entries are pruned.
        QVERIFY(db.myDayTaskIds(day1.addDays(1)).isEmpty());
        QVERIFY(db.myDayTaskIds(day1).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestDatabase)
#include "tst_database.moc"
