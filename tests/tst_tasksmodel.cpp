// SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QStandardPaths>
#include <QTest>

#include "models/tasklistsmodel.h"
#include "models/tasksmodel.h"

using namespace Merkzettel;

namespace {
Task makeTask(const QString &id, const QString &title, int dueInDays = INT_MIN,
              bool important = false, bool completed = false)
{
    Task t;
    t.id = id;
    t.title = title;
    t.listId = QStringLiteral("L");
    t.status = completed ? QStringLiteral("completed") : QStringLiteral("notStarted");
    t.importance = important ? QStringLiteral("high") : QStringLiteral("normal");
    if (dueInDays != INT_MIN)
        t.dueDate = QDateTime(QDate::currentDate().addDays(dueInDays), QTime(9, 0));
    return t;
}

QStringList ids(const TasksModel &m)
{
    QStringList out;
    for (int i = 0; i < m.rowCount(); ++i)
        out.append(m.data(m.index(i, 0), TasksModel::IdRole).toString());
    return out;
}
} // namespace

class TestTasksModel : public QObject
{
    Q_OBJECT

    QList<Task> sample() const
    {
        Task withCategory = makeTask(QStringLiteral("cat"), QStringLiteral("Expenses"));
        withCategory.categories = {QStringLiteral("Finance")};
        return {
            makeTask(QStringLiteral("over"), QStringLiteral("Overdue"), -2),
            makeTask(QStringLiteral("today"), QStringLiteral("Today task"), 0, true),
            makeTask(QStringLiteral("later"), QStringLiteral("Alpha later"), 30),
            makeTask(QStringLiteral("nodate"), QStringLiteral("Zulu"), INT_MIN, true),
            makeTask(QStringLiteral("done"), QStringLiteral("Done"), -1, false, true),
            withCategory,
        };
    }

private Q_SLOTS:
    void initTestCase()
    {
        // Sort/view options persist through KConfig; keep them out of ~/.config.
        QStandardPaths::setTestModeEnabled(true);
    }

    void defaultOrderIsBySection()
    {
        TasksModel m;
        m.setSortScope(QStringLiteral("test-default"));
        m.setSortMode(QStringLiteral("default"));
        m.setShowCompleted(true);
        m.setTasks(sample());
        QCOMPARE(ids(m),
                 (QStringList {QStringLiteral("over"), QStringLiteral("today"),
                               QStringLiteral("later"), QStringLiteral("nodate"),
                               QStringLiteral("cat"), QStringLiteral("done")}));
        QCOMPARE(m.data(m.index(0, 0), TasksModel::SectionLabelRole).toString(),
                 QStringLiteral("Overdue"));
    }

    void smartFiltersAndCompletedToggle()
    {
        TasksModel m;
        m.setShowCompleted(true);
        m.setTasks(sample());

        m.setSmartFilter(TasksModel::SmartFilter::Important);
        const QStringList important = ids(m);
        QCOMPARE(QSet<QString>(important.cbegin(), important.cend()),
                 (QSet<QString> {QStringLiteral("today"), QStringLiteral("nodate")}));

        m.setSmartFilter(TasksModel::SmartFilter::Planned);
        QVERIFY(!ids(m).contains(QStringLiteral("nodate")));
        QVERIFY(ids(m).contains(QStringLiteral("later")));

        m.setSmartFilter(TasksModel::SmartFilter::Completed);
        QCOMPARE(ids(m), QStringList {QStringLiteral("done")});
        m.setShowCompleted(false); // the Completed list ignores the toggle
        QCOMPARE(ids(m), QStringList {QStringLiteral("done")});

        m.setSmartFilter(TasksModel::SmartFilter::None);
        QVERIFY(!ids(m).contains(QStringLiteral("done")));

        m.setMyDayIds({QStringLiteral("later")});
        m.setSmartFilter(TasksModel::SmartFilter::MyDay);
        QCOMPARE(ids(m), QStringList {QStringLiteral("later")});
        m.setShowCompleted(true);
    }

    void sortModes()
    {
        TasksModel m;
        m.setSortScope(QStringLiteral("test-sort"));
        m.setShowCompleted(false);
        m.setTasks(sample());

        m.setSortMode(QStringLiteral("title"));
        QVERIFY(!m.sortDescending());
        QCOMPARE(ids(m).first(), QStringLiteral("later")); // "Alpha later"
        QCOMPARE(ids(m).last(), QStringLiteral("nodate")); // "Zulu"
        // Explicit sorts drop the date sections for open tasks.
        QVERIFY(m.data(m.index(0, 0), TasksModel::SectionLabelRole).toString().isEmpty());

        m.setSortDescending(true);
        QCOMPARE(ids(m).first(), QStringLiteral("nodate"));

        m.setSortMode(QStringLiteral("importance"));
        QVERIFY(!m.sortDescending()); // picking a mode resets the direction
        const QStringList byImportance = ids(m);
        QVERIFY(byImportance.indexOf(QStringLiteral("today"))
                < byImportance.indexOf(QStringLiteral("over")));

        m.setSortMode(QStringLiteral("dueDate"));
        QCOMPARE(ids(m).first(), QStringLiteral("over"));
        QVERIFY(ids(m).indexOf(QStringLiteral("nodate")) > ids(m).indexOf(QStringLiteral("later")));

        m.setSortMode(QStringLiteral("default"));
    }

    void sortIsRememberedPerList()
    {
        TasksModel m;
        m.setSortScope(QStringLiteral("list-a"));
        m.setSortMode(QStringLiteral("title"));
        m.setSortScope(QStringLiteral("list-b"));
        QCOMPARE(m.sortMode(), QStringLiteral("default"));
        m.setSortScope(QStringLiteral("list-a"));
        QCOMPARE(m.sortMode(), QStringLiteral("title"));
        m.setSortMode(QStringLiteral("default"));
    }

    void searchMatchesTitleBodyAndCategories()
    {
        TasksModel m;
        m.setTasks(sample());
        m.setFilterText(QStringLiteral("finance"));
        QCOMPARE(ids(m), QStringList {QStringLiteral("cat")});
        m.setFilterText(QStringLiteral("ZULU"));
        QCOMPARE(ids(m), QStringList {QStringLiteral("nodate")});
    }

    void hiddenTasksAreExcludedUntilGone()
    {
        TasksModel m;
        m.setTasks(sample());
        m.hideTasks({QStringLiteral("over")});
        QVERIFY(!ids(m).contains(QStringLiteral("over")));
        m.unhideTasks({QStringLiteral("over")});
        QVERIFY(ids(m).contains(QStringLiteral("over")));
    }

    void recurrenceDecoding_data()
    {
        QTest::addColumn<QString>("json");
        QTest::addColumn<QString>("preset");
        QTest::addColumn<QString>("unit");
        QTest::addColumn<int>("interval");

        QTest::newRow("none") << QString() << QString() << QString() << 0;
        QTest::newRow("daily") << R"({"pattern":{"type":"daily","interval":1}})" << "daily" << "day"
                               << 1;
        QTest::newRow("every 3 days")
            << R"({"pattern":{"type":"daily","interval":3}})" << "custom" << "day" << 3;
        QTest::newRow("weekdays")
            << R"({"pattern":{"type":"weekly","interval":1,"daysOfWeek":["friday","monday","tuesday","wednesday","thursday"]}})"
            << "weekdays" << "week" << 1;
        QTest::newRow("weekly one day")
            << R"({"pattern":{"type":"weekly","interval":1,"daysOfWeek":["monday"]}})"
            << "weekly" << "week" << 1;
        QTest::newRow("biweekly two days")
            << R"({"pattern":{"type":"weekly","interval":2,"daysOfWeek":["monday","thursday"]}})"
            << "custom" << "week" << 2;
        QTest::newRow("monthly")
            << R"({"pattern":{"type":"absoluteMonthly","interval":1,"dayOfMonth":5}})"
            << "monthly" << "month" << 1;
        QTest::newRow("yearly") << R"({"pattern":{"type":"absoluteYearly","interval":1}})"
                                << "yearly" << "year" << 1;
        QTest::newRow("relative monthly")
            << R"({"pattern":{"type":"relativeMonthly","interval":1}})"
            << "custom" << "" << 1;
    }

    void recurrenceDecoding()
    {
        QFETCH(QString, json);
        QFETCH(QString, preset);
        QFETCH(QString, unit);
        QFETCH(int, interval);

        Task t = makeTask(QStringLiteral("r"), QStringLiteral("Recurring"));
        t.recurrenceJson = json;
        TasksModel m;
        m.setTasks({t});
        const QVariantMap info = m.taskAt(0).value(QStringLiteral("recurrence")).toMap();
        QCOMPARE(info.value(QStringLiteral("preset")).toString(), preset);
        if (!json.isEmpty()) {
            QCOMPARE(info.value(QStringLiteral("unit")).toString(), unit);
            QCOMPARE(info.value(QStringLiteral("interval")).toInt(), interval);
            QVERIFY(!info.value(QStringLiteral("summary")).toString().isEmpty());
        }
    }

    void listsModelHasSmartListsFirst()
    {
        TaskListsModel m;
        m.setLists({{QStringLiteral("L1"), QStringLiteral("Tasks"), true}});
        QCOMPARE(m.rowCount(), 6);
        QCOMPARE(m.data(m.index(0, 0), TaskListsModel::IdRole).toString(),
                 QString::fromLatin1(TaskListsModel::kVirtualMyDayId));
        QVERIFY(m.data(m.index(0, 0), TaskListsModel::IsVirtualRole).toBool());
        QCOMPARE(m.data(m.index(5, 0), TaskListsModel::IdRole).toString(), QStringLiteral("L1"));
        QVERIFY(TaskListsModel::isVirtualId(QStringLiteral("__planned__")));
        QVERIFY(!TaskListsModel::isVirtualId(QStringLiteral("L1")));
    }
};

QTEST_GUILESS_MAIN(TestTasksModel)
#include "tst_tasksmodel.moc"
