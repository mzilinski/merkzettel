#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Print the CHANGELOG.md section of one version (used for release notes).

Usage: scripts/changelog-excerpt.py 0.4.0
"""
import pathlib
import re
import sys

text = (pathlib.Path(__file__).resolve().parent.parent / "CHANGELOG.md").read_text()
version = sys.argv[1].removeprefix("v")
m = re.search(rf"^## \[{re.escape(version)}\][^\n]*\n(.*?)(?=^## \[|^\[[^\]]+\]: |\Z)", text, re.M | re.S)
if not m:
    sys.exit(f"error: no CHANGELOG section for {version}")
print(m.group(1).strip())
