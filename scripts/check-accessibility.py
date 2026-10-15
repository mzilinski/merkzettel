#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fail if a QML button, check box or switch has neither text nor Accessible.name.

A tooltip is not an accessible name; screen readers announce `text` or
`Accessible.name`. Icon-only buttons should set `Accessible.name`
(e.g. `Accessible.name: QQC2.ToolTip.text`).
"""
import pathlib
import re
import sys

CONTROL = re.compile(r"\s*(QQC2\.(?:ToolButton|Button|RoundButton|CheckBox|Switch))\s*\{")
problems = []
for path in sorted(pathlib.Path(__file__).resolve().parent.parent.glob("src/*.qml")):
    lines = path.read_text().split("\n")
    for i, line in enumerate(lines):
        m = CONTROL.match(line)
        if not m:
            continue
        depth, block = 0, []
        for j in range(i, len(lines)):
            depth += lines[j].count("{") - lines[j].count("}")
            block.append(lines[j])
            if depth <= 0:
                break
        body = "\n".join(block)
        if not re.search(r'^\s*text:\s*(?!"")', body, re.M) and "Accessible.name" not in body:
            problems.append(f"{path.name}:{i + 1}: {m.group(1)} has no text or Accessible.name")

print("\n".join(problems) if problems else "accessibility: all controls named")
sys.exit(1 if problems else 0)
