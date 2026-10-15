// SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
// SPDX-License-Identifier: GPL-3.0-or-later
import { defineConfig } from "astro/config";
import sitemap from "@astrojs/sitemap";

// Served by GitHub Pages at https://mzilinski.github.io/merkzettel/ (D3).
// German is the default language at the root, English below /en/.
export default defineConfig({
  site: "https://mzilinski.github.io",
  base: "/merkzettel",
  trailingSlash: "always",
  build: { format: "directory", inlineStylesheets: "always" },
  i18n: {
    defaultLocale: "de",
    locales: ["de", "en"],
    routing: { prefixDefaultLocale: false },
  },
  integrations: [
    sitemap({
      i18n: { defaultLocale: "de", locales: { de: "de-DE", en: "en-US" } },
      // The MkDocs output in public/docs has its own sitemap.
      filter: (page) => !page.includes("/docs/"),
    }),
  ],
});
