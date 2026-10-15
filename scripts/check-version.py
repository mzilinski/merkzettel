#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fail if CMake, AppStream metainfo and CHANGELOG disagree on the version.

With --tag vX.Y.Z (release CI) the tag must match as well.
"""
import argparse
import pathlib
import re
import sys
import xml.etree.ElementTree as ET

ROOT = pathlib.Path(__file__).resolve().parent.parent


def cmake_version() -> str:
    text = (ROOT / "CMakeLists.txt").read_text()
    return re.search(r"project\(merkzettel VERSION (\S+)", text).group(1)


def metainfo_version() -> str:
    tree = ET.parse(ROOT / "io.github.mzilinski.merkzettel.metainfo.xml")
    return tree.find("./releases/release").get("version")


def changelog_versions() -> list[str]:
    return re.findall(r"^## \[(\d+\.\d+\.\d+)\]", (ROOT / "CHANGELOG.md").read_text(), re.M)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tag", help="release tag, e.g. v0.4.0")
    args = parser.parse_args()

    cmake, meta, log = cmake_version(), metainfo_version(), changelog_versions()
    errors = []
    if meta != cmake:
        errors.append(f"metainfo newest release {meta} != CMake {cmake}")
    if not log or log[0] != cmake:
        errors.append(f"CHANGELOG newest version {log[0] if log else None} != CMake {cmake}")
    if args.tag and args.tag != f"v{cmake}":
        errors.append(f"tag {args.tag} != v{cmake}")
    for e in errors:
        print(f"error: {e}", file=sys.stderr)
    if not errors:
        print(f"version {cmake} consistent")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
