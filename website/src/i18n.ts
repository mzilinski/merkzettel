// SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Routes and shared strings of the website. German lives at the root,
// English below /en/; every page names its counterpart in the other language.

export type Lang = "de" | "en";
export type PageKey =
  | "home" | "features" | "useCases" | "install" | "openSource"
  | "contact" | "imprint" | "privacy";

const BASE = import.meta.env.BASE_URL.replace(/\/$/, "");

const ROUTES: Record<PageKey, Record<Lang, string>> = {
  home: { de: "/", en: "/en/" },
  features: { de: "/funktionen/", en: "/en/features/" },
  useCases: { de: "/anwendungsfaelle/", en: "/en/use-cases/" },
  install: { de: "/installation/", en: "/en/installation/" },
  openSource: { de: "/open-source/", en: "/en/open-source/" },
  contact: { de: "/kontakt/", en: "/en/contact/" },
  imprint: { de: "/impressum/", en: "/en/imprint/" },
  privacy: { de: "/datenschutz/", en: "/en/privacy/" },
};

export const url = (page: PageKey, lang: Lang) => BASE + ROUTES[page][lang];
export const asset = (path: string) => `${BASE}/${path.replace(/^\//, "")}`;
export const docsUrl = (lang: Lang, page = "") =>
  `${BASE}/docs/${lang === "en" ? "en/" : ""}${page}`;

export const REPO = "https://github.com/mzilinski/merkzettel";
export const FLATHUB = "https://flathub.org/apps/io.github.mzilinski.merkzettel";
export const AUR = "https://aur.archlinux.org/packages/merkzettel";
export const DONATE = "https://paypal.me/eit31";
export const EMAIL = "malte@zilinski.eu";

export const T = {
  de: {
    tagline: "Microsoft To Do, nativ auf Plasma",
    description:
      "Merkzettel ist ein nativer KDE-Client für Microsoft To Do: schnell, in Plasma integriert und offline nutzbar. Freie Software unter der GPL.",
    nav: { features: "Funktionen", useCases: "Anwendungsfälle", install: "Installation", docs: "Doku", openSource: "Open Source" },
    footer: { contact: "Kontakt", imprint: "Impressum", privacy: "Datenschutz", source: "Quellcode", license: "Merkzettel ist freie Software unter der GPL-3.0-or-later. Website-Inhalte: CC BY-SA 4.0." },
    switchTo: "English",
    switchLabel: "Switch to English",
    skip: "Zum Inhalt springen",
    menu: "Menü",
    flathub: "Bei Flathub holen",
    aur: "Aus dem AUR installieren",
    more: "Mehr erfahren",
    draft: "Entwurf – rechtliche Prüfung ausstehend",
    notAffiliated: "Merkzettel ist kein Produkt von Microsoft und steht in keiner Verbindung zu Microsoft. Microsoft und Microsoft To Do sind Marken der Microsoft Corporation.",
    subtitles: "Untertitel",
    videoFallback: "Dein Browser kann dieses Video nicht abspielen. Du kannst es herunterladen:",
  },
  en: {
    tagline: "Microsoft To Do, native on Plasma",
    description:
      "Merkzettel is a native KDE client for Microsoft To Do: fast, integrated with Plasma and usable offline. Free software under the GPL.",
    nav: { features: "Features", useCases: "Use cases", install: "Installation", docs: "Docs", openSource: "Open source" },
    footer: { contact: "Contact", imprint: "Imprint", privacy: "Privacy", source: "Source code", license: "Merkzettel is free software under the GPL-3.0-or-later. Website content: CC BY-SA 4.0." },
    switchTo: "Deutsch",
    switchLabel: "Auf Deutsch lesen",
    skip: "Skip to content",
    menu: "Menu",
    flathub: "Get it on Flathub",
    aur: "Install from the AUR",
    more: "Learn more",
    draft: "Draft – legal review pending",
    notAffiliated: "Merkzettel is not a Microsoft product and is not affiliated with Microsoft. Microsoft and Microsoft To Do are trademarks of Microsoft Corporation.",
    subtitles: "Subtitles",
    videoFallback: "Your browser cannot play this video. You can download it:",
  },
} as const;
