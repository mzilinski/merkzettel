# How-to videos

One video per use case, in English and German, built by `make videos`
(`scripts/videos/make_videos.py`) into `website/public/videos/`:

| File | Content |
|---|---|
| `<id>-<lang>.mp4` | 1920×1080, H.264 + AAC (narration normalised to −16 LUFS) |
| `<id>-<lang>.srt`, `.vtt` | Subtitles; both languages fit both videos |
| `<id>-<lang>.jpg` | Poster frame |

## Script format

Each `<id>.md` starts with front matter:

- `title_en`, `title_de` — title card and MP4 title
- `setup` — actions before the recording starts (`;`-separated)
- `env` — extra environment variables for the app (`KEY=value …`)

followed by a table `Time | Action | English | Deutsch`. *Time* is the start
of the segment in the finished video; the narration of a segment must end
before the next one starts (`make_videos.py --check` verifies this without
recording). The first segment is `title` (title card), the last one `end`
(end card with credits).

Actions run one after another from the segment's start. Coordinates are
screen coordinates of the 1920×1080 recording, with the Merkzettel window at
210,75 and 1500×930. A prefix `en:` or `de:` limits an action to one language.

| Action | Effect |
|---|---|
| `launch [args]` | Start Merkzettel (setup only), place the window |
| `other X Y W H cmd …` | Start another application and place its window (`kwrite` gets an empty file) |
| `move X Y [seconds]` | Move the mouse smoothly |
| `click X Y`, `rclick X Y`, `ctrlclick X Y` | Move there and click (right, with Ctrl) |
| `key combo …` | Press keys (xdotool names) and show them as a caption |
| `keyq combo …` | Press keys without a caption |
| `type "text"` | Type text |
| `wait seconds` | Pause |
| `poster` | Take the poster frame 1.5 s after this segment starts |

## Requirements

Xvfb (`xvfb-run`), `dbus-run-session`, `kwin_x11`, `kglobalacceld`,
`xdotool`, `ffmpeg` with libx264, Node.js with `website/node_modules`
(`make website-deps`), [Piper](https://github.com/OHF-Voice/piper1-gpl)
(`pipx install piper-tts`) and two voices in
`~/.cache/merkzettel-videos/voices/` (or `$MERKZETTEL_VOICES`):

```bash
mkdir -p ~/.cache/merkzettel-videos/voices && cd ~/.cache/merkzettel-videos/voices
base=https://huggingface.co/rhasspy/piper-voices/resolve/v1.0.0
for v in de/de_DE/thorsten/high/de_DE-thorsten-high en/en_US/libritts_r/medium/en_US-libritts_r-medium; do
  curl -LO "$base/$v.onnx" -LO "$base/$v.onnx.json"
done
```

## Voices and licences

- German: *Thorsten* (Thorsten-Voice, Thorsten Müller), CC0.
- English: *libritts_r medium*, speaker 20, trained on LibriTTS-R
  (Koizumi et al.), CC BY 4.0 — the attribution is on every end card.

The videos themselves are CC BY-SA 4.0, like the documentation.
