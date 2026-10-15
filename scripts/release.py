#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepare a release: python3 scripts/release.py 0.5.0

Moves the CHANGELOG "Unreleased" section under the new version with today's
date, bumps project(... VERSION) in CMakeLists.txt and prepends a <release>
entry (built from the changelog bullets) to the AppStream metainfo. Commit,
then tag with `git tag -s vX.Y.Z`.
"""
import datetime
import html
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def main() -> int:
    if len(sys.argv) != 2 or not re.fullmatch(r"\d+\.\d+\.\d+", sys.argv[1]):
        print(__doc__)
        return 2
    version, today = sys.argv[1], datetime.date.today().isoformat()

    changelog = ROOT / "CHANGELOG.md"
    text = changelog.read_text()
    m = re.search(r"^## \[Unreleased\]\n(.*?)(?=^## \[)", text, re.M | re.S)
    if not m or not m.group(1).strip():
        print("error: CHANGELOG has no entries under [Unreleased]", file=sys.stderr)
        return 1
    notes = m.group(1).strip()
    text = text.replace(m.group(0), f"## [Unreleased]\n\n## [{version}] - {today}\n\n{notes}\n\n", 1)
    text = re.sub(r"^\[Unreleased\]: .*$",
                  f"[Unreleased]: https://github.com/mzilinski/merkzettel/compare/v{version}...HEAD\n"
                  f"[{version}]: https://github.com/mzilinski/merkzettel/releases/tag/v{version}",
                  text, count=1, flags=re.M)
    changelog.write_text(text)

    cmake = ROOT / "CMakeLists.txt"
    cmake.write_text(re.sub(r"(project\(merkzettel VERSION )\S+", rf"\g<1>{version}", cmake.read_text(), count=1))

    bullets = [html.escape(b.strip()) for b in re.findall(r"^- (.+(?:\n  .+)*)", notes, re.M)]
    items = "\n".join(f"          <li>{' '.join(b.split())}</li>" for b in bullets)
    release = (f'    <release version="{version}" date="{today}">\n'
               f"      <description>\n        <ul>\n{items}\n        </ul>\n      </description>\n"
               f"    </release>\n")
    meta = ROOT / "io.github.mzilinski.merkzettel.metainfo.xml"
    meta.write_text(meta.read_text().replace("  <releases>\n", "  <releases>\n" + release, 1))

    print(f"Prepared {version}. Review, commit, then: git tag -s v{version}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
