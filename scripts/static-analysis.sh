#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs clang-tidy, clazy and cppcheck on the project sources.
# Usage: scripts/static-analysis.sh [build-dir]   (default: build)
# The build dir must have been configured with CMAKE_EXPORT_COMPILE_COMMANDS=ON
# and built once (generated moc/qrc sources must exist). Exits non-zero on any
# finding, so CI treats warnings as errors.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD=${1:-build}
DB="$ROOT/$BUILD/analysis"
mkdir -p "$DB"
# Qt adds a GCC-only flag that clang-based tools reject.
sed 's/ -mno-direct-extern-access//g' "$ROOT/$BUILD/compile_commands.json" > "$DB/compile_commands.json"

mapfile -t SOURCES < <(cd "$ROOT" && git ls-files 'src/*.cpp' 'tests/*.cpp' | sed "s|^|$ROOT/|")
status=0

echo "== clang-tidy"
if ! run-clang-tidy -p "$DB" -quiet "${SOURCES[@]}" > "$DB/clang-tidy.log" 2>&1 ||
   grep -qE 'warning:|error:' "$DB/clang-tidy.log"; then
    grep -E 'warning:|error:' -A3 "$DB/clang-tidy.log" || cat "$DB/clang-tidy.log"
    status=1
fi

echo "== clazy"
: > "$DB/clazy.log"
for f in "${SOURCES[@]}"; do
    # -Wno-unknown-warning-option: clang does not know GCC's -Wlogical-op.
    clazy-standalone -p "$DB" --checks=level0,level1,no-non-pod-global-static \
        --extra-arg=-Wno-unknown-warning-option \
        --ignore-dirs="$BUILD|/usr" "$f" 2>&1 | grep -E -A3 'warning:|error:' \
        | grep -vE '^[0-9]+ warnings? generated\.$' >> "$DB/clazy.log" || true
done
if [ -s "$DB/clazy.log" ]; then cat "$DB/clazy.log"; status=1; fi

echo "== cppcheck"
# returnByReference: Qt property getters return implicitly shared values by
# value on purpose. preprocessorErrorDirective in .moc: cppcheck does not see
# QObject through <QTest>.
cppcheck --project="$DB/compile_commands.json" --enable=warning,performance,portability \
    --inline-suppr --suppress=missingIncludeSystem --suppress=returnByReference \
    --suppress='preprocessorErrorDirective:*.moc' \
    --file-filter="$ROOT/src/*" --file-filter="$ROOT/tests/*" \
    --library=qt -q --error-exitcode=1 --check-level=exhaustive -j"$(nproc)" || status=1

[ $status -eq 0 ] && echo "static analysis: clean"
exit $status
