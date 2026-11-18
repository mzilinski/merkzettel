#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Reproducible screenshots of the demo mode for docs, website, README and
the AppStream metainfo: Breeze Light/Dark x English/German.

Every set runs in its own headless Plasma Wayland session (a nested
`kwin_wayland --virtual`, see wayland_session.py), so Merkzettel runs as a
native Wayland client and the windows carry the real Breeze decoration.

Usage: scripts/screenshots.py [build-dir]
Needs: kwin_wayland, dbus-run-session, libei, PyGObject, ImageMagick (magick).
Output: docs/assets/screenshots/<lang>/<view>-<scheme>.png, plus the English
        and German sets in screenshots/.
"""
from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs/assets/screenshots"
SCREEN = (1280, 880)
WIN = (90, 60, 1100, 760)   # client area of the Merkzettel window
ADD = (420, 66)             # add-task field, relative to the client area
QUICK_ADD = {"en": "Pay rent tomorrow", "de": "Miete zahlen morgen"}
LAYOUT = {"en": "us", "de": "de"}


def outer(build: str) -> None:
    binary = ROOT / build / "bin/merkzettel"
    if not os.access(binary, os.X_OK):
        sys.exit(f"error: {binary} not built")
    with tempfile.TemporaryDirectory(prefix="merkzettel-shots-") as work:
        for scheme in ("light", "dark"):
            for lang in ("en", "de"):
                cfg = Path(work) / f"{scheme}-{lang}"
                for d in ("config", "data", "cache"):
                    (cfg / d).mkdir(parents=True)
                (cfg / "config/merkzettelrc").write_text("[ListSort]\ndemo-work=title,asc\n")
                colors = "BreezeDark" if scheme == "dark" else "BreezeLight"
                shutil.copy(f"/usr/share/color-schemes/{colors}.colors", cfg / "config/kdeglobals")
                print(f"== {scheme} / {lang}", flush=True)
                env = {k: v for k, v in os.environ.items() if k not in ("WAYLAND_DISPLAY", "DISPLAY")}
                env.update(XDG_CONFIG_HOME=str(cfg / "config"), XDG_DATA_HOME=str(cfg / "data"),
                           XDG_CACHE_HOME=str(cfg / "cache"))
                rc = subprocess.run(["dbus-run-session", "--", sys.executable, __file__, "--session",
                                     scheme, lang, str(binary), str(cfg / "kwin.log")], env=env).returncode
                if rc != 0:
                    sys.stderr.write((cfg / "kwin.log").read_text()[-3000:])
                    sys.exit(f"error: session {scheme}/{lang} failed ({rc})")
    # The README and the AppStream metainfo use these copies.
    for lang in ("en", "de"):
        (ROOT / "screenshots" / lang).mkdir(parents=True, exist_ok=True)
        for png in (OUT / lang).glob("*.png"):
            shutil.copy(png, ROOT / "screenshots" / lang / png.name)
    print(f"screenshots written to {OUT} and {ROOT / 'screenshots'}")


def inner(scheme: str, lang: str, binary: str, log: str) -> None:
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from wayland_session import Session

    locale = "de_DE.UTF-8" if lang == "de" else "en_US.UTF-8"
    os.environ.update(LANG=locale, LC_ALL=locale, LANGUAGE=lang)
    out = OUT / lang
    out.mkdir(parents=True, exist_ok=True)
    s = Session(*SCREEN, layout=LAYOUT[lang], log=Path(log))
    inp = s.input
    app = win = None

    def launch(*args, env=None):
        nonlocal app, win
        app = s.spawn([binary, *args], env=env)
        win = s.place(*WIN, pid=app.pid)
        time.sleep(6)   # let the start-up notification disappear
        s.activate(win["uuid"])

    def quit():
        app.terminate()
        app.wait(10)
        time.sleep(1)

    def click(x, y, button=1):
        c = win["client"]
        inp.move(c["x"] + x, c["y"] + y)
        inp.click(button)
        time.sleep(0.4)

    def key(*combos):
        for combo in combos:
            inp.combo(combo)
            time.sleep(0.25)

    def away():
        inp.move(SCREEN[0] - 10, SCREEN[1] - 10)
        time.sleep(0.3)

    def shot(name):
        s.window_png(win["uuid"], out / f"{name}-{scheme}.png")

    try:
        launch("--demo"); away(); shot("tasks"); quit()

        launch("--demo"); click(*ADD)
        key(*["alt+Up"] * 5); away(); shot("myday"); quit()

        launch("--demo"); click(*ADD); key(*["Down"] * 6, "Return"); time.sleep(1.2); away()
        shot("details"); quit()

        launch("--demo"); click(*ADD); key("Down", "space", "Down", "Down", "Down", "space"); away()
        shot("selection"); quit()

        launch("--demo"); click(*ADD); key("alt+Down"); away(); shot("sorted"); quit()

        launch("--demo"); click(*ADD); inp.type(QUICK_ADD[lang]); away(); shot("quickadd"); quit()

        launch("--demo", env={"MERKZETTEL_DEMO_OFFLINE": "1"}); away(); shot("offline"); quit()

        launch(); away(); shot("login"); quit()
    finally:
        s.close()


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--session":
        inner(*sys.argv[2:6])
    else:
        outer(sys.argv[1] if len(sys.argv) > 1 else "build")
