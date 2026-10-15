// SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Copy the app icon into public/ (favicon, logo) so the site uses the same
// SVG as the application. Runs before `astro dev` and `astro build`.
import { copyFileSync, mkdirSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "../..");
mkdirSync(join(root, "website/public"), { recursive: true });
copyFileSync(join(root, "icons/io.github.mzilinski.merkzettel.svg"), join(root, "website/public/favicon.svg"));
