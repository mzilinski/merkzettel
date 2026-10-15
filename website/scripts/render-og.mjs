// SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Render the OpenGraph preview image (public/og.png, 1200x630) from the
// app icon and the English task-list screenshot. Run after make screenshots:
// (cd website && npm run og)
import { chromium } from "playwright";
import { readFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "../..");
const logo = readFileSync(join(root, "icons/io.github.mzilinski.merkzettel.svg"), "utf8").replace(/<\?xml[^>]*>/, "");
const shot = readFileSync(join(root, "docs/assets/screenshots/en/tasks-light.png")).toString("base64");

const html = `<!doctype html><html><head><meta charset="utf-8"><style>
  html, body { margin: 0; width: 1200px; height: 630px; overflow: hidden; }
  body { font-family: "Noto Sans", "DejaVu Sans", sans-serif; color: #fcfcfc;
         background: radial-gradient(circle at 25% 20%, #3daee9 0%, #1d6fa5 45%, #14324a 100%); }
  .text { position: absolute; left: 70px; top: 120px; width: 470px; }
  .text svg { width: 96px; height: auto; }
  h1 { font-size: 64px; margin: 24px 0 10px; }
  p { font-size: 30px; line-height: 1.3; margin: 0; opacity: .92; }
  img { position: absolute; left: 600px; top: 90px; width: 760px; border-radius: 10px;
        box-shadow: 0 18px 60px rgba(0,0,0,.45); }
</style></head><body>
  <div class="text">${logo}<h1>Merkzettel</h1><p>Microsoft To Do,<br>native on Plasma</p></div>
  <img src="data:image/png;base64,${shot}">
</body></html>`;

const browser = await chromium.launch();
const page = await browser.newPage({ viewport: { width: 1200, height: 630 } });
await page.setContent(html);
await page.screenshot({ path: join(root, "website/public/og.png") });
await browser.close();
