// SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Render docs/diagrams/*.mmd to docs/assets/diagrams/*.svg with Mermaid in
// headless Chromium (Playwright). Mermaid is loaded from website/node_modules,
// so nothing is fetched from a CDN and the docs ship static SVGs.
// Usage: (cd website && npm run diagrams)
import { chromium } from "playwright";
import { readFileSync, readdirSync, writeFileSync, mkdirSync } from "node:fs";
import { join, dirname, basename } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "../..");
const srcDir = join(root, "docs/diagrams");
const outDir = join(root, "docs/assets/diagrams");
const mermaidJs = readFileSync(join(root, "website/node_modules/mermaid/dist/mermaid.min.js"), "utf8");
mkdirSync(outDir, { recursive: true });

const browser = await chromium.launch();
const page = await browser.newPage();
await page.setContent("<!doctype html><html><body></body></html>");
await page.addScriptTag({ content: mermaidJs });
await page.evaluate(() =>
  window.mermaid.initialize({ startOnLoad: false, theme: "neutral", securityLevel: "strict",
                              htmlLabels: false, flowchart: { htmlLabels: false },
                              fontFamily: "Noto Sans, DejaVu Sans, sans-serif" }));

for (const file of readdirSync(srcDir).filter((f) => f.endsWith(".mmd")).sort()) {
  const source = readFileSync(join(srcDir, file), "utf8");
  const id = "d" + basename(file, ".mmd").replace(/\W/g, "");
  let svg = await page.evaluate(async ({ id, source }) => (await window.mermaid.render(id, source)).svg,
                                { id, source });
  // Opaque background so the diagram stays readable in the dark docs theme.
  svg = svg.replace(/^<svg([^>]*?) style="/, '<svg$1 style="background-color:#ffffff;');
  writeFileSync(join(outDir, basename(file, ".mmd") + ".svg"), svg);
  console.log("rendered", file);
}
await browser.close();
