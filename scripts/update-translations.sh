#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Regenerate po/merkzettel.pot from the C++ and QML sources and merge it into
# every po/<lang>/merkzettel.po. With --check, only verify that the template is
# current and that every language is fully translated (used by CI).
set -euo pipefail
cd "$(dirname "$0")/.."

KEYWORDS=(--keyword=i18n:1 --keyword=i18nc:1c,2 --keyword=i18np:1,2 --keyword=i18ncp:1c,2,3
          --keyword=ki18n:1 --keyword=ki18nc:1c,2 --keyword=ki18np:1,2 --keyword=ki18ncp:1c,2,3)
COMMON=(--from-code=UTF-8 --add-comments=TRANSLATORS --no-wrap --sort-by-file
        --package-name=merkzettel --msgid-bugs-address=https://github.com/mzilinski/merkzettel/issues
        "${KEYWORDS[@]}")

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
git ls-files 'src/*.cpp' 'src/*.h' > "$tmp/cpp.txt"
git ls-files 'src/*.qml' > "$tmp/qml.txt"
xgettext "${COMMON[@]}" -C --kde -f "$tmp/cpp.txt" -o "$tmp/cpp.pot"
xgettext "${COMMON[@]}" -L JavaScript -f "$tmp/qml.txt" -o "$tmp/qml.pot"
msgcat --no-wrap --sort-by-file "$tmp/cpp.pot" "$tmp/qml.pot" -o "$tmp/merkzettel.pot"
# Strip the volatile creation date so an unchanged template diffs clean, and
# replace gettext's generic header comment with the project's.
sed -i -e '/^"POT-Creation-Date:/d' -e '1,4d' "$tmp/merkzettel.pot"
sed -i '1i # Translation template for Merkzettel.\n# SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>\n# SPDX-License-Identifier: GPL-3.0-or-later' "$tmp/merkzettel.pot"

if [ "${1:-}" = "--check" ]; then
    status=0
    if ! diff -q <(grep -v '^#' po/merkzettel.pot) <(grep -v '^#' "$tmp/merkzettel.pot") >/dev/null; then
        echo "error: po/merkzettel.pot is outdated, run scripts/update-translations.sh" >&2
        status=1
    fi
    for po in po/*/merkzettel.po; do
        merged="$tmp/$(basename "$(dirname "$po")").po"
        msgmerge --quiet --no-wrap --no-fuzzy-matching "$po" "$tmp/merkzettel.pot" -o "$merged"
        stats=$(LC_ALL=C msgfmt --statistics -o /dev/null "$merged" 2>&1)
        echo "$po: $stats"
        if echo "$stats" | grep -qE 'untranslated|fuzzy'; then status=1; fi
    done
    exit $status
fi

cp "$tmp/merkzettel.pot" po/merkzettel.pot
for po in po/*/merkzettel.po; do
    msgmerge --quiet --update --backup=none --no-wrap --no-fuzzy-matching "$po" po/merkzettel.pot
    msgattrib --no-obsolete --no-wrap -o "$po" "$po"
    echo "$po: $(LC_ALL=C msgfmt --statistics -o /dev/null "$po" 2>&1)"
done
