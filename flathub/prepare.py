#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write the Flathub copy of the manifest.

    flathub/prepare.py --client-id <uuid> <dir>

Fills in the Azure client ID of the public registration and the commit of
the release tag (Flathub requires tag *and* commit), then writes
<dir>/io.github.mzilinski.merkzettel.yml. <dir> is usually a checkout of the
Flathub submission fork. The tag must already be pushed.
"""
import argparse
import pathlib
import re
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
NAME = "io.github.mzilinski.merkzettel.yml"
REPO = "https://github.com/mzilinski/merkzettel.git"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--client-id", required=True)
    ap.add_argument("--repo", default=REPO, help="where to look up the tag (default: GitHub)")
    ap.add_argument("dest", type=pathlib.Path)
    a = ap.parse_args()

    if not re.fullmatch(r"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}", a.client_id):
        print("error: the client ID must be a UUID", file=sys.stderr)
        return 2
    text = (HERE / NAME).read_text()
    tag = re.search(r"^\s+tag: (\S+)$", text, re.M).group(1)
    out = subprocess.run(["git", "ls-remote", a.repo, f"refs/tags/{tag}", f"refs/tags/{tag}^{{}}"],
                         capture_output=True, text=True, check=True).stdout.split("\n")
    refs = dict(line.split("\t")[::-1] for line in out if line)
    commit = refs.get(f"refs/tags/{tag}^{{}}") or refs.get(f"refs/tags/{tag}")
    if not commit:
        print(f"error: tag {tag} not found in {a.repo}", file=sys.stderr)
        return 1

    text = text.replace("__MERKZETTEL_CLIENT_ID__", a.client_id)
    text = re.sub(r"^(\s+)tag: (\S+)$", rf"\1tag: \2\n\1commit: {commit}", text, count=1, flags=re.M)
    # The Flathub repository has its own licence; drop our header comment.
    text = re.sub(r"\A(#.*\n)+", "", text)
    a.dest.mkdir(parents=True, exist_ok=True)
    (a.dest / NAME).write_text(text)
    print(f"wrote {a.dest / NAME} ({tag} = {commit})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
