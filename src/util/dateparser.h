// SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDate>
#include <QDateTime>
#include <QPair>
#include <QString>

namespace Merkzettel {

// Quick-add parsing for the add bar: strips a trailing date keyword
// ("tomorrow", "mo", "25.5.", ...) and returns {title, due}. due is invalid
// when no keyword matched; due dates are 09:00 local time. `today` is a
// parameter so tests are deterministic.
QPair<QString, QDateTime> parseQuickAdd(const QString &raw, const QDate &today);

} // namespace Merkzettel
