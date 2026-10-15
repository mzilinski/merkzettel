// SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>

#include "util/dateparser.h"

using namespace Merkzettel;

class TestDateParser : public QObject
{
    Q_OBJECT

    // Thursday, 15 October 2026
    const QDate today {2026, 10, 15};

private Q_SLOTS:
    void keywords_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("title");
        QTest::addColumn<QDate>("due");

        QTest::newRow("no keyword") << "Buy milk" << "Buy milk" << QDate();
        QTest::newRow("heute") << "Buy milk heute" << "Buy milk" << today;
        QTest::newRow("today") << "Buy milk today" << "Buy milk" << today;
        QTest::newRow("morgen") << "Buy milk morgen" << "Buy milk" << today.addDays(1);
        QTest::newRow("tomorrow, mixed case")
            << "Buy milk Tomorrow" << "Buy milk" << today.addDays(1);
        QTest::newRow("übermorgen") << "Arzt übermorgen" << "Arzt" << today.addDays(2);
        QTest::newRow("uebermorgen") << "Arzt uebermorgen" << "Arzt" << today.addDays(2);
        // Weekdays always resolve to the *next* occurrence, never today.
        QTest::newRow("mo") << "Pay rent mo" << "Pay rent" << QDate(2026, 10, 19);
        QTest::newRow("do is next week") << "Call do" << "Call" << QDate(2026, 10, 22);
        QTest::newRow("freitag") << "Report freitag" << "Report" << QDate(2026, 10, 16);
        QTest::newRow("wed") << "Gym wed" << "Gym" << QDate(2026, 10, 21);
        QTest::newRow("sunday") << "Rest sunday" << "Rest" << QDate(2026, 10, 18);
        QTest::newRow("dd.mm.") << "Doctor 25.5." << "Doctor" << QDate(2026, 5, 25);
        QTest::newRow("dd.mm.yy") << "Doctor 3.1.27" << "Doctor" << QDate(2027, 1, 3);
        QTest::newRow("dd.mm.yyyy") << "Doctor 24.12.2026" << "Doctor" << QDate(2026, 12, 24);
        // Two-letter English abbreviations are deliberately not keywords.
        QTest::newRow("trailing 'we' stays") << "What do we" << "What do we" << QDate();
        QTest::newRow("keyword alone is a title") << "morgen" << "morgen" << QDate();
        QTest::newRow("invalid date") << "Party 31.2." << "Party 31.2." << QDate();
    }

    void keywords()
    {
        QFETCH(QString, input);
        QFETCH(QString, title);
        QFETCH(QDate, due);

        const auto parsed = parseQuickAdd(input, today);
        QCOMPARE(parsed.first, title);
        QCOMPARE(parsed.second.isValid(), due.isValid());
        if (due.isValid()) {
            QCOMPARE(parsed.second.date(), due);
            QCOMPARE(parsed.second.time(), QTime(9, 0));
        }
    }
};

QTEST_GUILESS_MAIN(TestDateParser)
#include "tst_dateparser.moc"
