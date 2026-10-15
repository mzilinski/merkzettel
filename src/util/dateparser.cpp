// SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dateparser.h"

#include <QMap>
#include <QRegularExpression>
#include <QTimeZone>

namespace Merkzettel {

QPair<QString, QDateTime> parseQuickAdd(const QString &raw, const QDate &today)
{
    // Parse a trailing date keyword off the end of the title.
    // Recognized (case-insensitive, last whitespace-separated token):
    //   heute/today, morgen/tomorrow, übermorgen, weekday names and
    //   abbreviations in German and English, 25.5., 25.5.2026
    // Returns {title-without-keyword, due-or-invalid}.
    // German and English keywords are both accepted regardless of the UI
    // language (the add-field placeholder advertises them per language).
    static const QMap<QString, int> dayOffsets = {
        {QStringLiteral("heute"), 0},      {QStringLiteral("morgen"), 1},
        {QStringLiteral("übermorgen"), 2}, {QStringLiteral("uebermorgen"), 2},
        {QStringLiteral("uebermrgn"), 2},  {QStringLiteral("today"), 0},
        {QStringLiteral("tomorrow"), 1},
    };
    static const QMap<QString, int> weekdays = {
        {QStringLiteral("mo"), 1},
        {QStringLiteral("di"), 2},
        {QStringLiteral("mi"), 3},
        {QStringLiteral("do"), 4},
        {QStringLiteral("fr"), 5},
        {QStringLiteral("sa"), 6},
        {QStringLiteral("so"), 7},
        {QStringLiteral("montag"), 1},
        {QStringLiteral("dienstag"), 2},
        {QStringLiteral("mittwoch"), 3},
        {QStringLiteral("donnerstag"), 4},
        {QStringLiteral("freitag"), 5},
        {QStringLiteral("samstag"), 6},
        {QStringLiteral("sonntag"), 7},
        // English: three-letter forms only — two-letter ones like "we" would
        // misfire on ordinary trailing words. ("mo"/"fr"/"sa" come from German.)
        {QStringLiteral("mon"), 1},
        {QStringLiteral("tue"), 2},
        {QStringLiteral("wed"), 3},
        {QStringLiteral("thu"), 4},
        {QStringLiteral("fri"), 5},
        {QStringLiteral("sat"), 6},
        {QStringLiteral("sun"), 7},
        {QStringLiteral("monday"), 1},
        {QStringLiteral("tuesday"), 2},
        {QStringLiteral("wednesday"), 3},
        {QStringLiteral("thursday"), 4},
        {QStringLiteral("friday"), 5},
        {QStringLiteral("saturday"), 6},
        {QStringLiteral("sunday"), 7},
    };
    static const QRegularExpression dateRe(QStringLiteral(R"(^(\d{1,2})\.(\d{1,2})\.(\d{2,4})?$)"));

    const int sep = raw.lastIndexOf(QChar::Space);
    if (sep <= 0) return {raw.trimmed(), QDateTime()};

    const QString trailing = raw.mid(sep + 1).trimmed().toLower();
    const QString head = raw.left(sep).trimmed();
    QDate target;

    if (dayOffsets.contains(trailing)) {
        target = today.addDays(dayOffsets.value(trailing));
    } else if (weekdays.contains(trailing)) {
        const int wd = weekdays.value(trailing);
        int offset = wd - today.dayOfWeek();
        if (offset <= 0) offset += 7; // always upcoming, never past
        target = today.addDays(offset);
    } else {
        const auto match = dateRe.match(trailing);
        if (match.hasMatch()) {
            int day = match.captured(1).toInt();
            int month = match.captured(2).toInt();
            int year = match.captured(3).isEmpty() ? today.year() : match.captured(3).toInt();
            if (year < 100) year += 2000;
            target = QDate(year, month, day);
        }
    }

    if (target.isValid() && !head.isEmpty()) {
        return {head, QDateTime(target, QTime(9, 0), QTimeZone::LocalTime)};
    }
    return {raw.trimmed(), QDateTime()};
}

} // namespace Merkzettel
