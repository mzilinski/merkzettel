# SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
# SPDX-License-Identifier: GPL-3.0-or-later
"""A headless Plasma Wayland session for screenshots and videos.

Runs a nested `kwin_wayland --virtual` (own socket, inside the caller's
`dbus-run-session`) and drives it only through interfaces KWin offers to
every Wayland client:

- input:   KWin's EIS server (org.kde.KWin.EIS.RemoteDesktop) via libei,
           key names resolved through the session keymap with libxkbcommon
- windows: KWin scripts (org.kde.kwin.Scripting) that report back over D-Bus
- pixels:  org.kde.KWin.ScreenShot2 (windows with decoration, full frames)

Needs: kwin_wayland, libei, libxkbcommon, PyGObject (Gio/GLib).
"""
from __future__ import annotations

import ctypes
import ctypes.util
import itertools
import json
import mmap
import os
import select
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

from gi.repository import Gio, GLib

# ------------------------------------------------------------------ libei ---

_ei = ctypes.CDLL(ctypes.util.find_library("ei") or "libei.so.1")
_P = ctypes.c_void_p
for name, res, args in [
    ("ei_new_sender", _P, [_P]),
    ("ei_configure_name", None, [_P, ctypes.c_char_p]),
    ("ei_setup_backend_fd", ctypes.c_int, [_P, ctypes.c_int]),
    ("ei_get_fd", ctypes.c_int, [_P]),
    ("ei_dispatch", None, [_P]),
    ("ei_get_event", _P, [_P]),
    ("ei_now", ctypes.c_uint64, [_P]),
    ("ei_unref", _P, [_P]),
    ("ei_event_get_type", ctypes.c_int, [_P]),
    ("ei_event_get_seat", _P, [_P]),
    ("ei_event_get_device", _P, [_P]),
    ("ei_event_unref", _P, [_P]),
    ("ei_device_ref", _P, [_P]),
    ("ei_device_has_capability", ctypes.c_bool, [_P, ctypes.c_int]),
    ("ei_device_start_emulating", None, [_P, ctypes.c_uint32]),
    ("ei_device_frame", None, [_P, ctypes.c_uint64]),
    ("ei_device_pointer_motion_absolute", None, [_P, ctypes.c_double, ctypes.c_double]),
    ("ei_device_button_button", None, [_P, ctypes.c_uint32, ctypes.c_bool]),
    ("ei_device_keyboard_key", None, [_P, ctypes.c_uint32, ctypes.c_bool]),
    ("ei_device_keyboard_get_keymap", _P, [_P]),
    ("ei_keymap_get_fd", ctypes.c_int, [_P]),
    ("ei_keymap_get_size", ctypes.c_size_t, [_P]),
]:
    f = getattr(_ei, name)
    f.restype, f.argtypes = res, args
_ei.ei_seat_bind_capabilities.restype = None   # variadic, NULL-terminated

EV_CONNECT, EV_DISCONNECT, EV_SEAT_ADDED = 1, 2, 3
EV_DEVICE_ADDED, EV_DEVICE_REMOVED, EV_DEVICE_PAUSED, EV_DEVICE_RESUMED = 5, 6, 7, 8
CAP_POINTER_ABS, CAP_KEYBOARD, CAP_BUTTON = 1 << 1, 1 << 2, 1 << 5
PORTAL_KEYBOARD, PORTAL_POINTER = 1, 2   # RemoteDesktop portal device types
BTN = {1: 0x110, 2: 0x112, 3: 0x111}   # BTN_LEFT, BTN_MIDDLE, BTN_RIGHT

# ------------------------------------------------------------ xkbcommon ---

_xkb = ctypes.CDLL(ctypes.util.find_library("xkbcommon") or "libxkbcommon.so.0")
for name, res, args in [
    ("xkb_context_new", _P, [ctypes.c_int]),
    ("xkb_keymap_new_from_string", _P, [_P, ctypes.c_char_p, ctypes.c_int, ctypes.c_int]),
    ("xkb_keymap_min_keycode", ctypes.c_uint32, [_P]),
    ("xkb_keymap_max_keycode", ctypes.c_uint32, [_P]),
    ("xkb_keymap_num_levels_for_key", ctypes.c_uint32, [_P, ctypes.c_uint32, ctypes.c_uint32]),
    ("xkb_keymap_key_get_syms_by_level", ctypes.c_int,
     [_P, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32, ctypes.POINTER(ctypes.POINTER(ctypes.c_uint32))]),
    ("xkb_keymap_key_get_mods_for_level", ctypes.c_size_t,
     [_P, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32), ctypes.c_size_t]),
    ("xkb_keymap_mod_get_index", ctypes.c_uint32, [_P, ctypes.c_char_p]),
    ("xkb_keysym_from_name", ctypes.c_uint32, [ctypes.c_char_p, ctypes.c_int]),
    ("xkb_utf32_to_keysym", ctypes.c_uint32, [ctypes.c_uint32]),
]:
    f = getattr(_xkb, name)
    f.restype, f.argtypes = res, args

# xdotool-style modifier names -> keysyms of the keys that set them
MODIFIER_KEYS = {"ctrl": "Control_L", "control": "Control_L", "alt": "Alt_L", "shift": "Shift_L",
                 "super": "Super_L", "meta": "Super_L"}


class Keymap:
    """keysym -> (evdev keycode, modifier keycodes) for one XKB keymap."""

    def __init__(self, text: bytes):
        ctx = _xkb.xkb_context_new(0)
        km = _xkb.xkb_keymap_new_from_string(ctx, text.rstrip(b"\0"), 1, 0)
        if not km:
            raise RuntimeError("cannot compile the session keymap")
        mod_index = {n: _xkb.xkb_keymap_mod_get_index(km, n.encode())
                     for n in ("Shift", "Lock", "Control", "Mod1", "Mod4", "Mod5", "LevelThree")}
        self.syms: dict[int, tuple[int, int]] = {}   # keysym -> (xkb keycode, mod mask)
        for kc in range(_xkb.xkb_keymap_min_keycode(km), _xkb.xkb_keymap_max_keycode(km) + 1):
            for level in range(_xkb.xkb_keymap_num_levels_for_key(km, kc, 0)):
                arr = ctypes.POINTER(ctypes.c_uint32)()
                n = _xkb.xkb_keymap_key_get_syms_by_level(km, kc, 0, level, ctypes.byref(arr))
                masks = (ctypes.c_uint32 * 8)()
                nm = _xkb.xkb_keymap_key_get_mods_for_level(km, kc, 0, level, masks, 8)
                if n != 1 or nm == 0:
                    continue
                mask = min(masks[:nm], key=lambda m: bin(m).count("1"))
                if mask & (1 << mod_index["Lock"]):
                    continue
                sym = arr[0]
                if sym not in self.syms or bin(mask).count("1") < bin(self.syms[sym][1]).count("1"):
                    self.syms[sym] = (kc, mask)
        # keys that produce each modifier bit
        self.mod_keys: dict[int, int] = {}
        for bit_name, sym_name in (("Shift", "Shift_L"), ("Control", "Control_L"), ("Mod1", "Alt_L"),
                                   ("Mod4", "Super_L"), ("Mod5", "ISO_Level3_Shift"),
                                   ("LevelThree", "ISO_Level3_Shift")):
            idx = mod_index[bit_name]
            sym = _xkb.xkb_keysym_from_name(sym_name.encode(), 0)
            if idx != 0xFFFFFFFF and sym in self.syms:
                self.mod_keys[1 << idx] = self.syms[sym][0]

    def resolve(self, sym: int) -> list[int]:
        """Evdev keycodes to hold, modifiers first, for one keysym."""
        if sym not in self.syms:
            raise KeyError(f"keysym 0x{sym:x} is not on the session keyboard")
        kc, mask = self.syms[sym]
        mods = [k for bit, k in self.mod_keys.items() if mask & bit]
        if len(mods) != bin(mask).count("1"):
            raise KeyError(f"keysym 0x{sym:x} needs an unsupported modifier")
        return [k - 8 for k in dict.fromkeys(mods)] + [kc - 8]

    def combo(self, combo: str) -> list[int]:
        """'ctrl+shift+a', 'Return', 'super+alt+n' -> keycodes to hold."""
        *mods, key = combo.split("+")
        codes = []
        for m in mods:
            codes += self.resolve(self.sym(MODIFIER_KEYS.get(m.lower(), m)))
        sym = self.sym(MODIFIER_KEYS.get(key.lower(), key))
        if mods and key.isalpha() and len(key) == 1:
            sym = self.sym(key.lower())   # ctrl+A means the A key, not Shift
        codes += self.resolve(sym)
        return list(dict.fromkeys(codes))

    @staticmethod
    def sym(name: str) -> int:
        s = _xkb.xkb_keysym_from_name(name.encode(), 0)
        if not s and len(name) == 1:
            s = _xkb.xkb_utf32_to_keysym(ord(name))
        if not s:
            raise KeyError(f"unknown key name '{name}'")
        return s


class Input:
    """Pointer and keyboard through KWin's EIS server."""

    def __init__(self, bus: Gio.DBusConnection):
        res, fds = bus.call_with_unix_fd_list_sync(
            "org.kde.KWin", "/org/kde/KWin/EIS/RemoteDesktop", "org.kde.KWin.EIS.RemoteDesktop",
            "connectToEIS", GLib.Variant("(i)", (PORTAL_KEYBOARD | PORTAL_POINTER,)),
            GLib.VariantType("(hi)"), Gio.DBusCallFlags.NONE, 10000, None, None)
        fd = fds.get(res.unpack()[0])
        self.ei = _ei.ei_new_sender(None)
        _ei.ei_configure_name(self.ei, b"merkzettel-docs")
        if _ei.ei_setup_backend_fd(self.ei, fd) != 0:
            raise RuntimeError("libei: cannot use the EIS socket")
        self.fd = _ei.ei_get_fd(self.ei)
        self.pointer = self.keyboard = None
        self.keymap: Keymap | None = None
        self.seq = itertools.count(1)
        self.pos = (0.0, 0.0)
        deadline = time.monotonic() + 10
        while not (self.pointer and self.keyboard):
            if time.monotonic() > deadline:
                raise RuntimeError("EIS: no pointer/keyboard device")
            self.pump(0.1)

    def pump(self, timeout: float = 0.0):
        select.select([self.fd], [], [], timeout)
        _ei.ei_dispatch(self.ei)
        while ev := _ei.ei_get_event(self.ei):
            t = _ei.ei_event_get_type(ev)
            if t == EV_SEAT_ADDED:
                seat = _ei.ei_event_get_seat(ev)
                _ei.ei_seat_bind_capabilities(_P(seat), ctypes.c_int(CAP_POINTER_ABS),
                                              ctypes.c_int(CAP_KEYBOARD), ctypes.c_int(CAP_BUTTON), _P(None))
            elif t == EV_DEVICE_ADDED:
                dev = _ei.ei_event_get_device(ev)
                if _ei.ei_device_has_capability(dev, CAP_POINTER_ABS) and not self.pointer:
                    self.pointer = _ei.ei_device_ref(dev)
                if _ei.ei_device_has_capability(dev, CAP_KEYBOARD) and not self.keyboard:
                    self.keyboard = _ei.ei_device_ref(dev)
                    km = _ei.ei_device_keyboard_get_keymap(dev)
                    if km:
                        size = _ei.ei_keymap_get_size(km)
                        with mmap.mmap(_ei.ei_keymap_get_fd(km), size, mmap.MAP_PRIVATE, mmap.PROT_READ) as m:
                            self.keymap = Keymap(m[:size])
            elif t == EV_DEVICE_RESUMED:
                _ei.ei_device_start_emulating(_ei.ei_event_get_device(ev), next(self.seq))
            elif t == EV_DISCONNECT:
                raise RuntimeError("EIS: disconnected")
            _ei.ei_event_unref(ev)

    def _frame(self, dev):
        _ei.ei_device_frame(dev, _ei.ei_now(self.ei))
        self.pump()

    # pointer
    def move(self, x: float, y: float):
        _ei.ei_device_pointer_motion_absolute(self.pointer, float(x), float(y))
        self._frame(self.pointer)
        self.pos = (x, y)

    def glide(self, x: float, y: float, duration: float):
        """Smooth (smoothstep) motion at 60 Hz."""
        x0, y0 = self.pos
        steps = max(2, int(duration * 60))
        for i in range(1, steps + 1):
            f = i / steps
            f = f * f * (3 - 2 * f)
            self.move(x0 + (x - x0) * f, y0 + (y - y0) * f)
            time.sleep(duration / steps)

    def button(self, button: int, press: bool):
        _ei.ei_device_button_button(self.pointer, BTN[button], press)
        self._frame(self.pointer)

    def click(self, button: int = 1, hold: list[int] | None = None):
        for k in hold or []:
            self.key(k, True)
        self.button(button, True)
        time.sleep(0.03)
        self.button(button, False)
        for k in reversed(hold or []):
            self.key(k, False)

    # keyboard
    def key(self, code: int, press: bool):
        _ei.ei_device_keyboard_key(self.keyboard, code, press)
        self._frame(self.keyboard)

    def press(self, codes: list[int], hold: float = 0.03):
        for c in codes:
            self.key(c, True)
            time.sleep(0.01)
        time.sleep(hold)
        for c in reversed(codes):
            self.key(c, False)
            time.sleep(0.01)

    def combo(self, combo: str):
        """Press an xdotool-style key combination like 'ctrl+f' or 'Return'."""
        self.press(self.keymap.combo(combo))

    def type(self, text: str, delay: float = 0.04):
        """Type text, one character every `delay` seconds."""
        start = time.monotonic()
        for i, ch in enumerate(text):
            self.press(self.keymap.resolve(Keymap.sym(ch)), hold=0.01)
            pause = start + (i + 1) * delay - time.monotonic()
            if pause > 0:
                time.sleep(pause)


# ---------------------------------------------------------------- session ---

_libc = ctypes.CDLL(None, use_errno=True)


def _die_with_parent():
    """preexec_fn: SIGTERM when the Python process goes, even on SIGKILL."""
    _libc.prctl(1, 15)   # PR_SET_PDEATHSIG, SIGTERM


# kwin_wayland carries cap_sys_nice, and exec'ing it clears the parent-death
# signal. This watchdog keeps it and passes it on to KWin.
_WATCHDOG = """
import os, signal, sys
pid = int(sys.argv[1])
def stop(*_):
    try: os.kill(pid, signal.SIGTERM)
    except ProcessLookupError: pass
    sys.exit(0)
signal.signal(signal.SIGTERM, stop)
while True:
    signal.pause()
"""


class Session:
    """kwin_wayland --virtual on its own socket in the current D-Bus session."""

    def __init__(self, width: int, height: int, layout: str = "us", log: Path | None = None):
        self.width, self.height = width, height
        self.socket = f"wl-merkzettel-docs-{os.getpid()}"
        cfg = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config"))
        cfg.mkdir(parents=True, exist_ok=True)
        (cfg / "kxkbrc").write_text(f"[Layout]\nLayoutList={layout}\nUse=true\n")
        self.log = open(log or os.devnull, "w")
        self.procs: list[subprocess.Popen] = []
        env = dict(os.environ)
        env.pop("DISPLAY", None)
        env.pop("WAYLAND_DISPLAY", None)
        self.kwin = subprocess.Popen(
            ["kwin_wayland", "--virtual", "--no-lockscreen", "--width", str(width), "--height", str(height),
             "--socket", self.socket],
            env=env, stdout=self.log, stderr=subprocess.STDOUT)
        self.watchdog = subprocess.Popen([sys.executable, "-c", _WATCHDOG, str(self.kwin.pid)],
                                         preexec_fn=_die_with_parent)
        self.bus = Gio.bus_get_sync(Gio.BusType.SESSION)
        self._wait_for("org.kde.KWin", 30)
        self._wait_for_path("/org/kde/KWin/ScreenShot2", 10)
        self.env = dict(env, WAYLAND_DISPLAY=self.socket, QT_QPA_PLATFORM="wayland",
                        XDG_SESSION_TYPE="wayland", XDG_CURRENT_DESKTOP="KDE",
                        KDE_FULL_SESSION="true", KDE_SESSION_VERSION="6")
        # D-Bus-activated services (portals, kded) must find the nested session
        self.bus.call_sync("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                           "UpdateActivationEnvironment",
                           GLib.Variant("(a{ss})", ({k: self.env[k] for k in (
                               "WAYLAND_DISPLAY", "QT_QPA_PLATFORM", "XDG_SESSION_TYPE",
                               "XDG_CURRENT_DESKTOP", "KDE_FULL_SESSION", "KDE_SESSION_VERSION")},)),
                           None, Gio.DBusCallFlags.NONE, 5000, None)
        self.input = Input(self.bus)
        self._results: dict[str, dict] = {}
        self._reply_id = self.bus.register_object(
            "/MerkzettelDocs",
            Gio.DBusNodeInfo.new_for_xml(
                "<node><interface name='org.kde.MerkzettelDocs'><method name='reply'>"
                "<arg type='s' direction='in'/><arg type='s' direction='in'/></method>"
                "</interface></node>").interfaces[0],
            self._on_call, None, None)
        self._script_n = itertools.count()

    # ------------------------------------------------------------ helpers
    def _wait_for(self, name: str, timeout: float):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self.kwin.poll() is not None:
                raise RuntimeError("kwin_wayland exited; see its log")
            owner = self.bus.call_sync("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                       "org.freedesktop.DBus", "NameHasOwner", GLib.Variant("(s)", (name,)),
                                       None, Gio.DBusCallFlags.NONE, 1000, None).unpack()[0]
            if owner:
                return
            time.sleep(0.2)
        raise RuntimeError(f"{name} did not appear")

    def _wait_for_path(self, path: str, timeout: float):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                self.bus.call_sync("org.kde.KWin", path, "org.freedesktop.DBus.Peer", "Ping", None, None,
                                   Gio.DBusCallFlags.NONE, 1000, None)
                xml = self.bus.call_sync("org.kde.KWin", path, "org.freedesktop.DBus.Introspectable",
                                         "Introspect", None, None, Gio.DBusCallFlags.NONE, 1000, None)
                if "interface name" in xml.unpack()[0]:
                    return
            except GLib.Error:
                pass
            time.sleep(0.2)
        raise RuntimeError(f"{path} did not appear")

    def _on_call(self, conn, sender, path, iface, method, params, invocation):
        token, payload = params.unpack()
        self._results[token] = json.loads(payload)
        invocation.return_value(None)

    def spawn(self, cmd: list[str], env: dict | None = None) -> subprocess.Popen:
        p = subprocess.Popen(cmd, env=dict(self.env, **(env or {})),
                             stdout=self.log, stderr=subprocess.STDOUT, preexec_fn=_die_with_parent)
        self.procs.append(p)
        return p

    def script(self, body: str, timeout: float = 30) -> dict:
        """Run a KWin script; `body` calls reply(object) exactly once."""
        token = f"t{next(self._script_n)}"
        name = f"merkzettel-docs-{token}"
        src = (f"function reply(o) {{ callDBus('{self.bus.get_unique_name()}', '/MerkzettelDocs', "
               f"'org.kde.MerkzettelDocs', 'reply', '{token}', JSON.stringify(o)); }}\n{body}\n")
        try:
            self._load(src, name)
            ctx = GLib.MainContext.default()
            deadline = time.monotonic() + timeout
            while token not in self._results:
                if time.monotonic() > deadline:
                    raise RuntimeError(f"KWin script timed out:\n{body}")
                ctx.iteration(False) or time.sleep(0.02)
            return self._results.pop(token)
        finally:
            self.bus.call_sync("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting", "unloadScript",
                               GLib.Variant("(s)", (name,)), None, Gio.DBusCallFlags.NONE, 5000, None)

    def _load(self, src: str, name: str):
        """Load and start a KWin script; it stays until unloaded."""
        with tempfile.NamedTemporaryFile("w", suffix=".js", delete=False) as f:
            f.write(src)
        try:
            sid = self.bus.call_sync("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting", "loadScript",
                                     GLib.Variant("(ss)", (f.name, name)), GLib.VariantType("(i)"),
                                     Gio.DBusCallFlags.NONE, 5000, None).unpack()[0]
            self.bus.call_sync("org.kde.KWin", f"/Scripting/Script{sid}", "org.kde.kwin.Script", "run",
                               None, None, Gio.DBusCallFlags.NONE, 5000, None)
        finally:
            os.unlink(f.name)

    # ------------------------------------------------------------ windows
    def place(self, x: int, y: int, w: int, h: int, *, caption: str | None = None, pid: int | None = None,
              timeout: float = 60) -> dict:
        """Wait for a window and give its client area the rectangle x,y,w,h.

        A Wayland client cannot position its own windows, so a window the
        application hides and shows again (closed to the tray, raised by a
        global shortcut) would land wherever KWin places it. The placement
        therefore stays in force for every later window of the same process.

        Returns the resulting client and frame geometry."""
        match = (f"c.pid === {pid}" if pid else f"c.caption === {json.dumps(caption)}")
        body = f"""
function geo(r) {{ return {{x: r.x, y: r.y, width: r.width, height: r.height}}; }}
const found = workspace.windowList().filter(c => c.normalWindow && ({match}));
if (!found.length) {{ reply(null); }}
else {{
    const c = found[found.length - 1], f = c.frameGeometry, k = c.clientGeometry;
    c.frameGeometry = {{x: {x} - (k.x - f.x), y: {y} - (k.y - f.y),
                        width: {w} + f.width - k.width, height: {h} + f.height - k.height}};
    workspace.activeWindow = c;
    reply({{uuid: c.internalId.toString(), client: geo(c.clientGeometry), frame: geo(c.frameGeometry)}});
}}
"""
        deadline = time.monotonic() + timeout
        while (res := self.script(body)) is None:
            if time.monotonic() > deadline:
                raise RuntimeError(f"no window {caption or pid} appeared")
            time.sleep(0.25)
        # the client may answer the resize a moment later; settle and re-read
        time.sleep(0.5)
        res = self.script(body)
        if pid:
            self._load(f"""
workspace.windowAdded.connect(function (c) {{
    if (!c.normalWindow || c.pid !== {pid}) return;
    const f = c.frameGeometry, k = c.clientGeometry;
    c.frameGeometry = {{x: {x} - (k.x - f.x), y: {y} - (k.y - f.y),
                        width: {w} + f.width - k.width, height: {h} + f.height - k.height}};
}});
""", f"merkzettel-docs-keep-{pid}")
        return res

    def activate(self, uuid: str):
        self.script(f"""
const c = workspace.windowList().find(w => w.internalId.toString() === {json.dumps(uuid)});
if (c) workspace.activeWindow = c;
reply({{ok: !!c}});
""")

    # ------------------------------------------------------------- pixels
    def _capture(self, method: str, args: tuple, sig: str) -> tuple[dict, bytes]:
        r, w = os.pipe()
        fds = Gio.UnixFDList.new()
        idx = fds.append(w)
        os.close(w)
        params = GLib.Variant(f"({sig}a{{sv}}h)", (*args, {
            "include-cursor": GLib.Variant("b", method == "CaptureScreen"),
            "include-decoration": GLib.Variant("b", True),
            "include-shadow": GLib.Variant("b", False),
            "native-resolution": GLib.Variant("b", True)}, idx))
        res, _ = self.bus.call_with_unix_fd_list_sync(
            "org.kde.KWin", "/org/kde/KWin/ScreenShot2", "org.kde.KWin.ScreenShot2", method, params,
            GLib.VariantType("(a{sv})"), Gio.DBusCallFlags.NONE, 10000, fds, None)
        for fd in fds.steal_fds():   # our copy of the write end, or the read never ends
            os.close(fd)
        info = res.unpack()[0]
        chunks = []
        with os.fdopen(r, "rb") as f:
            while chunk := f.read(1 << 20):
                chunks.append(chunk)
        return info, b"".join(chunks)

    def window_png(self, uuid: str, out: Path):
        """The window including its Breeze decoration, as PNG."""
        info, data = self._capture("CaptureWindow", ("{" + uuid.strip("{}") + "}",), "s")
        write_png(info, data, out)

    def screen_raw(self) -> tuple[dict, bytes]:
        return self._capture("CaptureScreen", ("Virtual-0",), "s")

    def close(self):
        for p in reversed(self.procs):
            if p.poll() is None:
                p.terminate()
        for p in self.procs:
            try:
                p.wait(5)
            except subprocess.TimeoutExpired:
                p.kill()
        self.kwin.terminate()
        try:
            self.kwin.wait(10)
        except subprocess.TimeoutExpired:
            self.kwin.kill()
        self.watchdog.terminate()
        self.watchdog.wait()
        self.log.close()


# QImage::Format values ScreenShot2 reports -> ImageMagick/ffmpeg layouts
_QIMAGE = {4: ("bgra", "bgr0"), 5: ("bgra", "bgra"), 6: ("bgra", "bgra"),
           16: ("rgba", "rgb0"), 17: ("rgba", "rgba"), 18: ("rgba", "rgba")}


def pixel_format(info: dict) -> str:
    """ffmpeg pix_fmt of a ScreenShot2 raw image."""
    return _QIMAGE[info["format"]][1]


def write_png(info: dict, data: bytes, out: Path):
    w, h, stride = info["width"], info["height"], info["stride"]
    if stride != w * 4:
        data = b"".join(data[y * stride:y * stride + w * 4] for y in range(h))
    fmt = _QIMAGE[info["format"]][0]
    alpha = [] if info["format"] in (5, 6, 17, 18) else ["-alpha", "off"]
    subprocess.run(["magick", "-size", f"{w}x{h}", "-depth", "8", f"{fmt}:-", *alpha,
                    "-strip", "-define", "png:compression-level=9", str(out)],
                   input=data, check=True)


class ScreenRecorder(threading.Thread):
    """Grabs the screen at a fixed rate and pipes the frames into ffmpeg.

    Frame n shows the screen at t0 + n/fps (the latest capture at that
    moment), so the video timeline matches the monotonic clock exactly."""

    def __init__(self, session: Session, out: Path, fps: int):
        super().__init__(daemon=True)
        self.session, self.out, self.fps = session, out, fps
        self.stop_flag = threading.Event()
        self.t0: float | None = None
        self.started = threading.Event()
        self.error: BaseException | None = None

    def run(self):
        try:
            # a second connection: the main thread keeps using the first one
            bus = Gio.DBusConnection.new_for_address_sync(
                Gio.dbus_address_get_for_bus_sync(Gio.BusType.SESSION),
                Gio.DBusConnectionFlags.AUTHENTICATION_CLIENT | Gio.DBusConnectionFlags.MESSAGE_BUS_CONNECTION)
            grab = _Grabber(bus)
            info, frame = grab()
            ff = subprocess.Popen(
                ["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", pixel_format(info),
                 "-video_size", f"{info['width']}x{info['height']}", "-framerate", str(self.fps), "-i", "-",
                 "-c:v", "libx264", "-preset", "ultrafast", "-qp", "0", str(self.out)],
                stdin=subprocess.PIPE)
            self.t0 = time.monotonic()
            self.started.set()
            n = 0
            while not self.stop_flag.is_set():
                due = self.t0 + n / self.fps
                now = time.monotonic()
                if now < due:
                    time.sleep(due - now)
                info, frame = grab()
                # catch up if a capture took longer than a frame
                behind = int((time.monotonic() - self.t0) * self.fps) - n
                for _ in range(max(1, behind)):
                    ff.stdin.write(frame)
                    n += 1
            ff.stdin.close()
            ff.wait()
        except BaseException as e:   # reported by stop()
            self.error = e
            self.started.set()

    def stop(self):
        self.stop_flag.set()
        self.join()
        if self.error:
            raise self.error


class _Grabber:
    def __init__(self, bus):
        self.bus = bus

    def __call__(self) -> tuple[dict, bytes]:
        r, w = os.pipe()
        fds = Gio.UnixFDList.new()
        idx = fds.append(w)
        os.close(w)
        res, _ = self.bus.call_with_unix_fd_list_sync(
            "org.kde.KWin", "/org/kde/KWin/ScreenShot2", "org.kde.KWin.ScreenShot2", "CaptureScreen",
            GLib.Variant("(sa{sv}h)", ("Virtual-0", {"include-cursor": GLib.Variant("b", True)}, idx)),
            GLib.VariantType("(a{sv})"), Gio.DBusCallFlags.NONE, 10000, fds, None)
        for fd in fds.steal_fds():   # our copy of the write end, or the read never ends
            os.close(fd)
        info = res.unpack()[0]
        size = info["stride"] * info["height"]
        buf = bytearray(size)
        view = memoryview(buf)
        got = 0
        with os.fdopen(r, "rb", buffering=0) as f:
            while got < size:
                n = f.readinto(view[got:])
                if not n:
                    break
                got += n
        if info["stride"] != info["width"] * 4:
            w4 = info["width"] * 4
            buf = b"".join(buf[y * info["stride"]:y * info["stride"] + w4] for y in range(info["height"]))
        return info, bytes(buf)
