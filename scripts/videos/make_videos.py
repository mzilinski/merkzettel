#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build the how-to videos from the scripts in docs/videos/*.md.

Every script holds a table of segments with a fixed start time, the actions
performed in the app and the narration in English and German. For each
script and language this tool

  1. synthesises the narration with Piper (one WAV per sentence, cached),
  2. records the demo mode in Xvfb (1920x1080, KWin X11 for the Breeze window
     decoration, kglobalacceld for the global shortcut) with ffmpeg x11grab
     while xdotool performs the actions at their start times,
  3. renders title and end cards from HTML (website/scripts/render-cards.mjs),
  4. mixes the narration, normalises it (EBU R128 loudnorm) and encodes
     H.264/AAC MP4 with key-press captions,
  5. writes SRT and WebVTT subtitles and a JPEG poster.

Because the timeline is fixed by the script, both languages share it: the
English and German subtitles fit either video.

Usage: scripts/videos/make_videos.py [--build build] [--only ID ...]
                                      [--lang en,de] [--out DIR]
Needs: xvfb-run, dbus-run-session, kwin_x11, kglobalacceld, xdotool, xprop,
       ffmpeg (libx264, x11grab), piper (pip: piper-tts), node + Playwright
       (website/node_modules), the voices in $MERKZETTEL_VOICES
       (default ~/.cache/merkzettel-videos/voices).
"""
from __future__ import annotations

import argparse
import array
import hashlib
import json
import math
import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import textwrap
import time
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPTS = ROOT / "docs/videos"
CACHE = Path(os.environ.get("XDG_CACHE_HOME", Path.home() / ".cache")) / "merkzettel-videos"
VOICES = Path(os.environ.get("MERKZETTEL_VOICES", CACHE / "voices"))
FONT = "/usr/share/fonts/noto/NotoSans-Bold.ttf"
SITE = "mzilinski.github.io/merkzettel"

# Window geometry inside the 1920x1080 screen; the coordinates in the scripts
# are screen coordinates for this placement.
WIN_X, WIN_Y, WIN_W, WIN_H = 210, 75, 1500, 930
# The visible frame including the title bar; everything outside it is
# replaced by a background image in the final video.
FRAME = (WIN_X, WIN_Y - 28, WIN_W, WIN_H + 28)
FPS = 30
NARRATION_DELAY = 0.35   # seconds between segment start and narration
SENTENCE_GAP = 0.3

VOICE = {
    "en": {"model": "en_US-libritts_r-medium", "speaker": "20", "length_scale": "1.0"},
    "de": {"model": "de_DE-thorsten-high", "speaker": None, "length_scale": "0.95"},
}
TEXT = {
    "en": {
        "kicker": "Merkzettel – Microsoft To Do on Plasma",
        "more": "More how-tos and the documentation:",
        "voice": "~Voice: Piper TTS, en_US-libritts_r-medium, trained on LibriTTS-R "
                 "(Koizumi et al., CC BY 4.0)",
        "license": "~Video: CC BY-SA 4.0 · Merkzettel is free software (GPL-3.0-or-later)",
        "end": "Thanks for watching",
    },
    "de": {
        "kicker": "Merkzettel – Microsoft To Do, nativ auf Plasma",
        "more": "Weitere Anleitungen und die Dokumentation:",
        "voice": "~Stimme: Piper TTS, de_DE-thorsten-high (Thorsten-Voice, Thorsten Müller, CC0)",
        "license": "~Video: CC BY-SA 4.0 · Merkzettel ist freie Software (GPL-3.0-or-later)",
        "end": "Danke fürs Zuschauen",
    },
}
KEY_LABELS = {
    "super": "Meta", "ctrl": "Ctrl", "alt": "Alt", "shift": "Shift", "Return": "Enter",
    "Escape": "Esc", "Down": "↓", "Up": "↑", "Left": "←", "Right": "→", "space": "Space",
    "Delete": "Del", "Tab": "Tab", "F5": "F5",
}
KEY_LABELS_DE = {"ctrl": "Strg", "Escape": "Esc", "Return": "Enter", "space": "Leertaste",
                 "Delete": "Entf", "shift": "Umschalt"}


def die(msg: str) -> None:
    print(f"error: {msg}", file=sys.stderr)
    sys.exit(1)


def run(cmd: list[str], **kw) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, check=True, **kw)


# ---------------------------------------------------------------- scripts ---

def parse_time(s: str) -> float:
    m = re.fullmatch(r"(\d+):(\d{2}(?:\.\d+)?)", s.strip())
    if not m:
        die(f"bad time '{s}'")
    return int(m.group(1)) * 60 + float(m.group(2))


def parse_script(path: Path) -> dict:
    """Front matter (key: value between --- lines) plus the segment table."""
    text = path.read_text(encoding="utf-8")
    m = re.match(r"---\n(.*?)\n---\n", text, re.S)
    if not m:
        die(f"{path}: missing front matter")
    meta = {}
    for line in m.group(1).splitlines():
        if ":" in line:
            k, v = line.split(":", 1)
            meta[k.strip()] = v.strip()
    rows = []
    for line in text[m.end():].splitlines():
        if not line.startswith("|") or re.match(r"\|\s*-", line):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if cells[0].lower() == "time" or len(cells) != 4:
            continue
        rows.append({
            "time": parse_time(cells[0]),
            "actions": parse_actions(cells[1].replace("`", "")),
            "en": cells[2], "de": cells[3],
        })
    if not rows or rows[0]["actions"][:1] != [("all", "title", [])]:
        die(f"{path}: the first segment must be `title`")
    if rows[-1]["actions"][:1] != [("all", "end", [])]:
        die(f"{path}: the last segment must be `end`")
    for a, b in zip(rows, rows[1:]):
        if b["time"] <= a["time"]:
            die(f"{path}: times must increase ({b['time']})")
    return {"id": path.stem, "meta": meta, "rows": rows}


def parse_actions(cell: str) -> list[tuple[str, str, list[str]]]:
    """'click 10 20; de: type "x"' -> [(lang|'all', verb, args)]."""
    out = []
    for part in (p.strip() for p in cell.split(";")):
        if not part or part == "–":
            continue
        lang = "all"
        m = re.match(r"(en|de):\s*(.*)", part)
        if m:
            lang, part = m.group(1), m.group(2)
        verb, _, rest = part.partition(" ")
        args = re.findall(r'"([^"]*)"|(\S+)', rest)
        out.append((lang, verb, [a or b for a, b in args]))
    return out


# -------------------------------------------------------------------- TTS ---

def sentences(text: str) -> list[str]:
    text = re.sub(r"\s+", " ", text).strip()
    return [s for s in re.split(r"(?<=[.!?])\s+", text) if s]


def speakable(text: str) -> str:
    """Text for the voice: the subtitles keep symbols, the voice gets words."""
    text = text.replace("–", ",").replace("…", "...")
    text = re.sub(r"\*\*?|`", "", text)
    return text


def tts(lang: str, sentence: str) -> Path:
    v = VOICE[lang]
    key = hashlib.sha256(json.dumps([v, sentence]).encode()).hexdigest()[:20]
    out = CACHE / "tts" / f"{lang}-{key}.wav"
    if out.exists():
        return out
    out.parent.mkdir(parents=True, exist_ok=True)
    model = VOICES / f"{v['model']}.onnx"
    if not model.exists():
        die(f"voice {model} missing; see docs/videos/README.md")
    cmd = [shutil.which("piper") or str(Path.home() / ".local/bin/piper"), "-m", str(model),
           "--length-scale", v["length_scale"], "-f", str(out) + ".tmp"]
    if v["speaker"]:
        cmd += ["-s", v["speaker"]]
    run(cmd, input=speakable(sentence).encode(), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    os.replace(str(out) + ".tmp", out)
    return out


def wav_duration(p: Path) -> float:
    with wave.open(str(p)) as w:
        return w.getnframes() / w.getframerate()


def plan_narration(script: dict, lang: str) -> list[dict]:
    """Per sentence: wav, start and end in video time. Checks the fit."""
    rows = script["rows"]
    plan = []
    for i, row in enumerate(rows):
        t = row["time"] + NARRATION_DELAY
        limit = rows[i + 1]["time"] if i + 1 < len(rows) else None
        for s in sentences(row[lang]):
            wav = tts(lang, s)
            d = wav_duration(wav)
            plan.append({"text": s, "wav": wav, "start": t, "end": t + d, "row": i})
            t += d + SENTENCE_GAP
        if limit is not None and t - SENTENCE_GAP > limit - 0.25:
            die(f"{script['id']} [{lang}] segment {fmt(row['time'])}: narration needs "
                f"{t - SENTENCE_GAP - row['time']:.1f}s, slot is {limit - row['time']:.1f}s")
    return plan


def mix(plan: list[dict], total: float, out: Path) -> None:
    rate = None
    buf = None
    for p in plan:
        with wave.open(str(p["wav"])) as w:
            if rate is None:
                rate = w.getframerate()
                buf = array.array("h", bytes(2 * int(math.ceil(total * rate))))
            assert w.getsampwidth() == 2 and w.getnchannels() == 1 and w.getframerate() == rate
            samples = array.array("h", w.readframes(w.getnframes()))
        o = int(p["start"] * rate)
        buf[o:o + len(samples)] = samples[: max(0, len(buf) - o)]
    with wave.open(str(out), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(buf.tobytes())


# -------------------------------------------------------------- subtitles ---

def fmt(t: float, sep: str = ".") -> str:
    h, rem = divmod(t, 3600)
    m, s = divmod(rem, 60)
    return f"{int(h):02d}:{int(m):02d}:{int(s):02d}{sep}{int(round((s % 1) * 1000)) % 1000:03d}"


def cues(plan: list[dict]) -> list[tuple[float, float, str]]:
    """Sentences, split into two-line cues of at most 2x42 characters."""
    out = []
    for p in plan:
        chunks, cur = [], ""
        for word in p["text"].split():
            if len(cur) + 1 + len(word) > 84 and cur:
                chunks.append(cur)
                cur = word
            else:
                cur = f"{cur} {word}".strip()
        chunks.append(cur)
        total = sum(len(c) for c in chunks)
        t = p["start"]
        for c in chunks:
            d = (p["end"] - p["start"]) * len(c) / total
            out.append((t, t + d, "\n".join(textwrap.wrap(c, 42))))
            t += d
    # Keep each cue on screen until shortly before the next one.
    result = []
    for i, (a, b, txt) in enumerate(out):
        nxt = out[i + 1][0] if i + 1 < len(out) else b + 1.0
        result.append((a, min(max(b, a + 1.2), nxt - 0.05), txt))
    return result


def write_subtitles(plan: list[dict], stem: Path) -> None:
    cs = cues(plan)
    srt = "".join(f"{i}\n{fmt(a, ',')} --> {fmt(b, ',')}\n{t}\n\n" for i, (a, b, t) in enumerate(cs, 1))
    vtt = "WEBVTT\n\n" + "".join(f"{fmt(a)} --> {fmt(b)}\n{t}\n\n" for a, b, t in cs)
    stem.with_suffix(".srt").write_text(srt, encoding="utf-8")
    stem.with_suffix(".vtt").write_text(vtt, encoding="utf-8")


# ---------------------------------------------------------------- record ----

def key_label(combo: str, lang: str) -> str:
    labels = dict(KEY_LABELS, **(KEY_LABELS_DE if lang == "de" else {}))
    return " + ".join(labels.get(k, k.upper() if len(k) == 1 else k) for k in combo.split("+"))


def record(spec: dict, build: str, workdir: Path) -> tuple[Path, list]:
    """Run the recorder in its own Xvfb + D-Bus session."""
    spec_file = workdir / "spec.json"
    spec_file.write_text(json.dumps(spec))
    lang = spec["lang"]
    env = dict(os.environ)
    env.pop("WAYLAND_DISPLAY", None)
    for k in ("config", "data", "cache"):
        (workdir / k).mkdir(exist_ok=True)
    shutil.copy("/usr/share/color-schemes/BreezeLight.colors", workdir / "config/kdeglobals")
    (workdir / "config/kwriterc").write_text("[General]\nShow welcome view for new window=false\n")
    env.update({
        "XDG_CONFIG_HOME": str(workdir / "config"), "XDG_DATA_HOME": str(workdir / "data"),
        "XDG_CACHE_HOME": str(workdir / "cache"), "QT_QPA_PLATFORM": "xcb",
        "XDG_SESSION_TYPE": "x11", "MERKZETTEL_BIN": str(ROOT / build / "bin/merkzettel"),
        "LANG": "de_DE.UTF-8" if lang == "de" else "en_US.UTF-8",
        "LC_ALL": "de_DE.UTF-8" if lang == "de" else "en_US.UTF-8",
        "LANGUAGE": lang,
    })
    for k, v in spec["env"].items():
        env[k] = v
    with open(workdir / "session.log", "w") as log:
        rc = subprocess.run(["xvfb-run", "-a", "-s", "-screen 0 1920x1080x24 -nolisten tcp",
                             "dbus-run-session", "--", sys.executable, __file__, "--record",
                             str(spec_file)], env=env, stdout=log, stderr=subprocess.STDOUT).returncode
    if rc != 0 or not (workdir / "result.json").exists():
        sys.stderr.write((workdir / "session.log").read_text()[-4000:])
        die(f"recording failed ({rc})")
    result = json.loads((workdir / "result.json").read_text())
    return workdir / "rec.mkv", result


class Recorder:
    """Runs inside Xvfb: starts the session, performs the actions."""

    def __init__(self, spec: dict):
        self.spec = spec
        self.lang = spec["lang"]
        self.work = Path(spec["workdir"])
        self.procs: list[subprocess.Popen] = []
        self.keys: list[tuple[float, str]] = []
        self.t0 = None
        self.mouse = (WIN_X + WIN_W - 40, WIN_Y + WIN_H - 40)
        self.win = None

    def spawn(self, cmd, **kw):
        p = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, **kw)
        self.procs.append(p)
        return p

    def xdo(self, *args):
        subprocess.run(["xdotool", *map(str, args)], check=False,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def now(self) -> float:
        """Current time on the video timeline."""
        return time.monotonic() - self.t0 + self.spec["rec_start"]

    def place(self, pid: int | None, name: str | None, geom):
        args = ["xdotool", "search", "--sync", "--onlyvisible"]
        args += ["--pid", str(pid)] if pid else ["--name", name]
        win = subprocess.run(args, capture_output=True, text=True, timeout=60).stdout.split()[-1]
        x, y, w, h = geom
        self.xdo("windowsize", win, w, h)
        self.xdo("windowmove", win, x, y)
        return win

    # --- actions
    def act(self, verb: str, args: list[str]):
        if verb in ("title", "end", "poster"):
            return
        if verb == "launch":
            self.spawn([os.environ["MERKZETTEL_BIN"], *args])
            self.win = self.place(None, "^Merkzettel$", (WIN_X, WIN_Y, WIN_W, WIN_H))
            time.sleep(6)   # the start-up notification disappears
            self.xdo("windowactivate", "--sync", self.win)
        elif verb == "other":
            # other X Y W H command ...
            x, y, w, h = map(int, args[:4])
            cmd = args[4:]
            if cmd[0] == "kwrite" and len(cmd) == 1:
                cmd.append(str(self.work / ("notizen.txt" if self.lang == "de" else "notes.txt")))
            p = self.spawn(cmd)
            win = self.place(p.pid, None, (x, y, w, h))
            time.sleep(2)
            self.xdo("windowactivate", "--sync", win)
        elif verb == "move":
            self.move(int(args[0]), int(args[1]), float(args[2]) if len(args) > 2 else 0.7)
        elif verb in ("click", "rclick", "ctrlclick"):
            self.move(int(args[0]), int(args[1]), 0.6)
            time.sleep(0.15)
            if verb == "ctrlclick":
                self.xdo("keydown", "ctrl")
                self.xdo("click", 1)
                self.xdo("keyup", "ctrl")
            else:
                self.xdo("click", 3 if verb == "rclick" else 1)
            time.sleep(0.25)
        elif verb in ("key", "keyq"):
            for i, combo in enumerate(args):
                if verb == "key":
                    self.keys.append((self.now(), key_label(combo, self.lang)))
                self.xdo("key", "--clearmodifiers", combo)
                time.sleep(0.45)
        elif verb == "type":
            self.xdo("type", "--delay", 85, args[0])
            time.sleep(0.2)
        elif verb == "wait":
            time.sleep(float(args[0]))
        elif verb == "activate":
            self.xdo("windowactivate", "--sync", self.win)
        else:
            raise SystemExit(f"unknown action {verb}")

    def move(self, x: int, y: int, dur: float):
        x0, y0 = self.mouse
        steps = max(2, int(dur * 60))
        for i in range(1, steps + 1):
            f = i / steps
            f = f * f * (3 - 2 * f)   # smoothstep
            self.xdo("mousemove", int(x0 + (x - x0) * f), int(y0 + (y - y0) * f))
            time.sleep(dur / steps / 2)
        self.mouse = (x, y)

    def run_actions(self, actions):
        for lang, verb, args in actions:
            if lang in ("all", self.lang):
                self.act(verb, args)

    def main(self):
        disp = os.environ["DISPLAY"]
        # kglobalacceld first: KWin registers Alt+F4 and friends through it.
        self.spawn(["/usr/lib/kglobalacceld"])
        time.sleep(1)
        self.spawn(["kwin_x11", "--replace"], env=dict(os.environ, KWIN_COMPOSE="N"))
        time.sleep(3)
        (self.work / ("notizen.txt" if self.lang == "de" else "notes.txt")).write_text("")
        self.xdo("mousemove", *self.mouse)
        self.run_actions(self.spec["setup"])
        if self.win:
            self.xdo("mousemove", *self.mouse)
        rec = self.work / "rec.mkv"
        ff = subprocess.Popen(
            ["ffmpeg", "-y", "-loglevel", "error", "-f", "x11grab", "-draw_mouse", "1",
             "-framerate", str(FPS), "-video_size", "1920x1080", "-i", disp, "-copyts",
             "-c:v", "libx264", "-preset", "ultrafast", "-qp", "0", str(rec)],
            stdin=subprocess.PIPE)
        time.sleep(2.0)
        self.t0 = time.monotonic()
        wall0 = time.time()
        for row in self.spec["rows"]:
            delay = row["time"] - self.now()
            if delay > 0:
                time.sleep(delay)
            elif delay < -0.3:
                print(f"warning: segment {row['time']:.1f}s starts {-delay:.1f}s late", file=sys.stderr)
            self.run_actions(row["actions"])
        end = self.spec["rec_end"] + 0.5
        if end > self.now():
            time.sleep(end - self.now())
        ff.communicate(b"q")
        (self.work / "result.json").write_text(json.dumps({"wall0": wall0, "keys": self.keys}))
        for p in self.procs:
            p.send_signal(signal.SIGTERM)


# --------------------------------------------------------------- assemble ---

def drawtext_escape(s: str) -> str:
    return s.replace("\\", "\\\\").replace("'", "’").replace(":", "\\:").replace("%", "\\%")


def build_video(script: dict, lang: str, plan: list[dict], cards: dict, rec: Path, result: dict,
                out: Path, work: Path) -> None:
    rows = script["rows"]
    t1 = rows[1]["time"]
    te = rows[-1]["time"]
    total = max(te + 5.0, plan[-1]["end"] + 1.8)
    narration = work / "narration.wav"
    mix(plan, total, narration)

    # x11grab stamps frames with the wall clock; -copyts keeps it.
    start = float(subprocess.run(
        ["ffprobe", "-v", "error", "-show_entries", "format=start_time", "-of", "csv=p=0", str(rec)],
        capture_output=True, text=True, check=True).stdout.strip())
    offset = result["wall0"] - start
    if not 0 <= offset < 10:
        offset = 2.0

    draw = []
    for t, label in result["keys"]:
        a = t - t1
        draw.append(
            f"drawtext=fontfile={FONT}:text='{drawtext_escape(label)}':fontsize=46:fontcolor=white:"
            f"box=1:boxcolor=0x232629@0.82:boxborderw=26:x=(w-text_w)/2:y=h-190:"
            f"enable='between(t,{a:.2f},{a + 1.8:.2f})'")
    fx, fy, fw, fh = FRAME
    filt = (
        f"[0:v]fps=30,format=yuv420p,fade=t=out:st={t1 - 0.4:.2f}:d=0.4[a];"
        f"[1:v]setpts=PTS-STARTPTS,fps=30,crop={fw}:{fh}:{fx}:{fy}[win];"
        f"[4:v][win]overlay={fx}:{fy}:shortest=1,format=yuv420p"
        + ("," + ",".join(draw) if draw else "") +
        f",fade=t=in:st=0:d=0.4,fade=t=out:st={te - t1 - 0.4:.2f}:d=0.4[b];"
        f"[2:v]fps=30,format=yuv420p,fade=t=in:st=0:d=0.5[c];"
        f"[a][b][c]concat=n=3:v=1:a=0[v];"
        f"[3:a]loudnorm=I=-16:TP=-1.5:LRA=11,aresample=48000,apad,atrim=0:{total:.3f}[au]"
    )
    tmp = out.with_suffix(".tmp.mp4")
    run(["ffmpeg", "-y", "-loglevel", "error",
         "-loop", "1", "-framerate", "30", "-t", f"{t1:.3f}", "-i", str(cards["title"]),
         "-ss", f"{offset:.3f}", "-t", f"{te - t1:.3f}", "-i", str(rec),
         "-loop", "1", "-framerate", "30", "-t", f"{total - te:.3f}", "-i", str(cards["end"]),
         "-i", str(narration),
         "-loop", "1", "-framerate", "30", "-t", f"{te - t1:.3f}", "-i", str(cards["bg"]),
         "-filter_complex", filt, "-map", "[v]", "-map", "[au]",
         "-c:v", "libx264", "-preset", "slow", "-crf", "28", "-tune", "stillimage",
         "-pix_fmt", "yuv420p", "-r", "30", "-g", "150",
         "-c:a", "aac", "-b:a", "128k", "-ac", "1",
         "-metadata", f"title={script['meta']['title_' + lang]}",
         "-metadata:s:a:0", f"language={'deu' if lang == 'de' else 'eng'}",
         "-movflags", "+faststart", str(tmp)])
    os.replace(tmp, out)

    poster_t = next((r["time"] + 1.5 for r in rows
                     if any(v == "poster" for _, v, _ in r["actions"])), t1 + (te - t1) * 0.45)
    run(["ffmpeg", "-y", "-loglevel", "error", "-ss", f"{poster_t:.2f}", "-i", str(out),
         "-frames:v", "1", "-vf", "scale=1280:-2", "-q:v", "4", str(out.with_suffix(".jpg"))])


def render_cards(scripts: list[dict], langs: list[str], work: Path) -> dict:
    jobs, paths = [], {}
    for s in scripts:
        for lang in langs:
            t = TEXT[lang]
            title = s["meta"]["title_" + lang]
            for kind in ("title", "end"):
                p = work / f"{s['id']}-{lang}-{kind}.png"
                paths[(s["id"], lang, kind)] = p
                if kind == "title":
                    jobs.append({"out": str(p), "kind": kind, "kicker": t["kicker"], "title": title,
                                 "lines": []})
                else:
                    jobs.append({"out": str(p), "kind": kind, "kicker": title, "title": t["end"],
                                 "lines": [t["more"], SITE, "", t["voice"], t["license"]]})
    paths["bg"] = work / "background.png"
    jobs.append({"out": str(paths["bg"]), "kind": "bg", "win": list(FRAME)})
    spec = work / "cards.json"
    spec.write_text(json.dumps(jobs))
    run(["node", str(ROOT / "website/scripts/render-cards.mjs"), str(spec)], cwd=ROOT / "website")
    return paths


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--build", default="build")
    ap.add_argument("--only", nargs="*")
    ap.add_argument("--lang", default="en,de")
    ap.add_argument("--out", default=str(ROOT / "website/public/videos"))
    ap.add_argument("--check", action="store_true", help="only synthesise and check the timing")
    ap.add_argument("--record", help=argparse.SUPPRESS)
    a = ap.parse_args()

    if a.record:
        Recorder(json.loads(Path(a.record).read_text())).main()
        return

    langs = a.lang.split(",")
    files = sorted(SCRIPTS.glob("*.md"))
    files = [f for f in files if f.name != "README.md" and (not a.only or f.stem in a.only)]
    if not files:
        die("no scripts selected")
    scripts = [parse_script(f) for f in files]
    plans = {(s["id"], lang): plan_narration(s, lang) for s in scripts for lang in langs}
    for (sid, lang), plan in plans.items():
        s = next(x for x in scripts if x["id"] == sid)
        print(f"{sid} [{lang}]: {max(s['rows'][-1]['time'] + 5, plan[-1]['end'] + 1.8):.0f}s")
    if a.check:
        return
    if not (ROOT / a.build / "bin/merkzettel").exists():
        die(f"{a.build}/bin/merkzettel not built")

    out_dir = Path(a.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="merkzettel-videos-") as tmp:
        tmp = Path(tmp)
        cards = render_cards(scripts, langs, tmp)
        for s in scripts:
            for lang in langs:
                print(f"== {s['id']} [{lang}]", flush=True)
                work = tmp / f"{s['id']}-{lang}"
                work.mkdir()
                rows = s["rows"]
                spec = {
                    "lang": lang, "workdir": str(work),
                    "env": dict(kv.split("=", 1) for kv in s["meta"].get("env", "").split() if kv),
                    "setup": parse_actions(s["meta"].get("setup", "")),
                    "rows": [{"time": r["time"], "actions": r["actions"]} for r in rows[1:-1]],
                    "rec_start": rows[1]["time"], "rec_end": rows[-1]["time"],
                }
                rec, result = record(spec, a.build, work)
                stem = out_dir / f"{s['id']}-{lang}"
                build_video(s, lang, plans[(s["id"], lang)],
                            dict({k: cards[(s["id"], lang, k)] for k in ("title", "end")}, bg=cards["bg"]),
                            rec, result, stem.with_suffix(".mp4"), work)
                write_subtitles(plans[(s["id"], lang)], stem)
                print(f"   {stem.with_suffix('.mp4')} ({stem.with_suffix('.mp4').stat().st_size >> 10} KiB)")


if __name__ == "__main__":
    main()
