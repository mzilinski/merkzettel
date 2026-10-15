#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Reproducible screenshots of the demo mode for docs, website, README and the
# AppStream metainfo: Breeze Light/Dark x English/German, KWin (X11) inside
# Xvfb so windows carry the real Breeze decoration.
#
# Usage: scripts/screenshots.sh [build-dir]
# Needs: Xvfb (xvfb-run), dbus-run-session, kwin_x11, xdotool, xprop,
#        ImageMagick (magick). Output: docs/assets/screenshots/<lang>/
#        <view>-<scheme>.png, plus the English/German sets in screenshots/.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD=${1:-build}
BIN="$ROOT/$BUILD/bin/merkzettel"
OUT="$ROOT/docs/assets/screenshots"
[ -x "$BIN" ] || { echo "error: $BIN not built" >&2; exit 1; }

if [ "${MERKZETTEL_SCREENSHOT_SESSION:-}" != 1 ]; then
    work=$(mktemp -d)
    trap 'rm -rf "$work"' EXIT
    for scheme in light dark; do
        for lang in en de; do
            cfg="$work/$scheme-$lang"
            mkdir -p "$cfg/config" "$cfg/data" "$cfg/cache"
            printf '[ListSort]\ndemo-work=title,asc\n' > "$cfg/config/merkzettelrc"
            if [ "$scheme" = dark ]; then
                cp /usr/share/color-schemes/BreezeDark.colors "$cfg/config/kdeglobals"
            else
                cp /usr/share/color-schemes/BreezeLight.colors "$cfg/config/kdeglobals"
            fi
            echo "== $scheme / $lang"
            env -u WAYLAND_DISPLAY MERKZETTEL_SCREENSHOT_SESSION=1 \
                XDG_CONFIG_HOME="$cfg/config" XDG_DATA_HOME="$cfg/data" XDG_CACHE_HOME="$cfg/cache" \
                SHOT_LANG="$lang" SHOT_SCHEME="$scheme" SHOT_BIN="$BIN" SHOT_OUT="$OUT/$lang" \
                xvfb-run -a -s '-screen 0 1280x880x24' dbus-run-session -- "$0" "$BUILD"
        done
    done
    # The README and the AppStream metainfo use these copies.
    for lang in en de; do
        mkdir -p "$ROOT/screenshots/$lang"
        cp "$OUT/$lang"/*.png "$ROOT/screenshots/$lang/"
    done
    echo "screenshots written to $OUT and $ROOT/screenshots"
    exit 0
fi

# ---- inside one Xvfb + D-Bus session --------------------------------------
mkdir -p "$SHOT_OUT"
if [ "$SHOT_LANG" = de ]; then
    export LANG=de_DE.UTF-8 LANGUAGE=de LC_ALL=de_DE.UTF-8
    QUICK_ADD="Miete zahlen morgen"
else
    export LANG=en_US.UTF-8 LANGUAGE=en LC_ALL=en_US.UTF-8
    QUICK_ADD="Pay rent tomorrow"
fi
export QT_QPA_PLATFORM=xcb
KWIN_COMPOSE=N kwin_x11 --replace >/dev/null 2>&1 &
sleep 3

WIN=
launch() {
    "$SHOT_BIN" "$@" >/dev/null 2>&1 &
    WIN=$(xdotool search --sync --onlyvisible --name '^Merkzettel$' | tail -1)
    xdotool windowsize "$WIN" 1100 760 windowmove "$WIN" 90 60
    sleep 6   # let the start-up notification disappear
    xdotool windowactivate --sync "$WIN" 2>/dev/null || true
}
quit() {
    kill %2 2>/dev/null || true
    wait %2 2>/dev/null || true
    sleep 1
}
# Click relative to the window's client area.
click() { xdotool mousemove --window "$WIN" "$1" "$2" click "${3:-1}"; sleep 0.4; }
key() { xdotool key --clearmodifiers "$@"; sleep 0.25; }
away() { xdotool mousemove 1270 870; sleep 0.3; }
shot() {
    local name=$1
    eval "$(xdotool getwindowgeometry --shell "$WIN")"
    # _NET_FRAME_EXTENTS = left, right, top, bottom (window decoration)
    read -r l r t b < <(xprop -id "$WIN" _NET_FRAME_EXTENTS | sed 's/.*= //; s/,//g')
    magick import -window root -crop "$((WIDTH + l + r))x$((HEIGHT + t + b))+$((X - l))+$((Y - t))" \
        +repage -strip -define png:compression-level=9 "$SHOT_OUT/$name-$SHOT_SCHEME.png"
}

ADD_X=420; ADD_Y=66      # add-task field
launch --demo; away; shot tasks; quit

launch --demo; click $ADD_X $ADD_Y
for _ in 1 2 3 4 5; do key alt+Up; done; away; shot myday; quit

launch --demo; click $ADD_X $ADD_Y; key Down Down Down Down Down Down Return; sleep 1.2; away; shot details; quit

launch --demo; click $ADD_X $ADD_Y; key Down space Down Down Down space; away; shot selection; quit

launch --demo; click $ADD_X $ADD_Y; key alt+Down; away; shot sorted; quit

launch --demo; click $ADD_X $ADD_Y; xdotool type --delay 40 "$QUICK_ADD"; away; shot quickadd; quit

MERKZETTEL_DEMO_OFFLINE=1 launch --demo; away; shot offline; quit

launch; away; shot login; quit
