// SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Render the title and end cards of the how-to videos (1920x1080 PNG) from
// HTML in headless Chromium. Called by scripts/videos/make_videos.py with a
// JSON file: [{ "out": "...png", "kind": "title"|"end", "kicker": "...",
// "title": "...", "lines": ["..."] }]. Only local fonts, no network.
import { chromium } from "playwright";
import { readFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "../..");
const logo = readFileSync(join(root, "icons/io.github.mzilinski.merkzettel.svg"), "utf8");
const cards = JSON.parse(readFileSync(process.argv[2], "utf8"));

const esc = (s) => s.replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" })[c]);

// Background behind the recorded window (kind "bg"): the card gradient and a
// shadow at the window's place (c.win = [x, y, w, h]).
const bg = (c) => `<!doctype html><html><head><style>
  html, body { margin: 0; width: 1920px; height: 1080px; }
  body { background: radial-gradient(circle at 30% 20%, #3daee9 0%, #1d6fa5 45%, #14324a 100%); }
  div { position: absolute; left: ${c.win[0]}px; top: ${c.win[1]}px; width: ${c.win[2]}px;
        height: ${c.win[3]}px; box-shadow: 0 18px 60px rgba(0,0,0,.45); background: #000; }
</style></head><body><div></div></body></html>`;

const html = (c) => `<!doctype html><html><head><meta charset="utf-8"><style>
  html, body { margin: 0; width: 1920px; height: 1080px; }
  body { font-family: "Noto Sans", "DejaVu Sans", sans-serif; color: #fcfcfc;
         background: radial-gradient(circle at 30% 20%, #3daee9 0%, #1d6fa5 45%, #14324a 100%);
         display: flex; flex-direction: column; justify-content: center; padding: 0 180px;
         box-sizing: border-box; }
  .logo svg { width: ${c.kind === "title" ? 150 : 120}px; height: auto;
              filter: drop-shadow(0 6px 18px rgba(0,0,0,.35)); }
  .kicker { margin-top: 48px; font-size: 38px; letter-spacing: .04em; opacity: .85; }
  h1 { margin: 14px 0 0; font-size: ${c.kind === "title" ? 96 : 76}px; line-height: 1.12; font-weight: 700; }
  .lines { margin-top: 56px; font-size: 32px; line-height: 1.6; opacity: .92; }
  .lines .small { font-size: 26px; opacity: .8; }
</style></head><body>
  <div class="logo">${logo.replace(/<\?xml[^>]*>/, "")}</div>
  <div class="kicker">${esc(c.kicker)}</div>
  <h1>${esc(c.title)}</h1>
  <div class="lines">${(c.lines || []).map((l) => l.startsWith("~")
      ? `<div class="small">${esc(l.slice(1))}</div>` : `<div>${esc(l)}</div>`).join("")}</div>
</body></html>`;

const browser = await chromium.launch();
const page = await browser.newPage({ viewport: { width: 1920, height: 1080 } });
for (const c of cards) {
  await page.setContent(c.kind === "bg" ? bg(c) : html(c));
  await page.screenshot({ path: c.out });
}
await browser.close();
