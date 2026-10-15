<!--
SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
SPDX-License-Identifier: CC0-1.0
-->

# Release preparation report

## Phase 0 — Assessment (2026-10-15)

### Architecture

Single-process Qt 6 / KF6 application, all work on the GUI thread
(networking is asynchronous through `QNetworkAccessManager`, SQLite is
accessed synchronously from the same thread, so there is no shared mutable
state across threads).

| Layer | Files | Role |
|---|---|---|
| Entry | `src/main.cpp` | `KAboutData`, single instance (`KDBusService`), QML engine |
| Controller | `src/app.{h,cpp}` | Exposed to QML as `app`; routes UI actions to Graph and cache, undo, My Day |
| Auth | `src/auth/` | OAuth 2.0 authorization code + PKCE via QtNetworkAuth, loopback redirect on `localhost:53682`, refresh token in the system keychain via QtKeychain |
| Graph | `src/graph/graphclient.*`, `todoapi.*` | REST wrapper with request queue, 401 refresh, 429 back-off; To Do endpoints incl. delta sync, attachments, move-by-copy |
| Cache | `src/cache/database.*` | SQLite: lists, tasks, steps, linked resources, delta links, local My Day |
| Models | `src/models/` | `QAbstractListModel`s for lists and tasks (filter, sort, sections) |
| UI | `src/*.qml` | Kirigami pages, detail sheet, dialogs |
| Tray | `src/tray/` | `KStatusNotifierItem` with due-today badge |

Dependencies: Qt 6.6+ (Core, Gui, Qml, Quick, QuickControls2, Network,
NetworkAuth, Sql, Widgets), KF6 (CoreAddons, DBusAddons, GlobalAccel, I18n,
Notifications, StatusNotifierItem, Config, ConfigWidgets, ColorScheme,
Kirigami), kirigami-addons, QtKeychain.

### Findings

- **Tests:** none.
- **Paging:** `fetchTasks` requests `$top=200` and ignores
  `@odata.nextLink`; `upsertTasks` then replaces the list in the cache, so
  tasks beyond 200 disappear locally. `fetchLists` does not page either.
  Delta sync pages correctly.
- **Offline start:** `App::loggedIn()` requires a valid access token. Without
  network the refresh fails and the login page hides the cached data.
- **Token errors:** a failed refresh after a 401 leaves the waiting request
  connected to a signal that never fires (request silently dropped). A
  revoked refresh token is not cleared from the keychain.
- **Leak:** the delta-sync handler stores a `shared_ptr` to itself.
- **Rate limits:** fixed during this assessment (`50fa7c5`): at most four
  concurrent requests, Retry-After honoured, bounded retries.
- **Offline mutations:** fail with raw HTTP/network text; no queue.
  Conflicts are last-writer-wins (PATCH of single fields).
- **Thread safety:** no threads besides Qt's internal network threads; no
  issue found.
- **Startup:** cache-first; the UI shows cached lists before the network
  answers.
- **Packaging:** `org.kde.merkzettel` ID (not usable on Flathub), metainfo
  minimal, no AUR package, CI without tests/linters, version only in CMake.

### Privacy inventory

Stored locally:

- `~/.local/share/<app>/cache.sqlite` — copy of lists, tasks, steps, linked
  resources, categories, delta tokens, My Day membership.
- `~/.cache/<app>/attachments/` — attachments the user opened.
- `~/.config/merkzettelrc` — view options (sort order per list, show
  completed, colour scheme, global shortcut config via KGlobalAccel).
- System keychain (KWallet / Secret Service) — the OAuth refresh token only.

Sent to Microsoft: sign-in (login.microsoftonline.com) and all task data
through Microsoft Graph (graph.microsoft.com). Nothing is sent anywhere
else: no telemetry, no crash reporting, no update checks.

### Plan

See [ROADMAP.md](ROADMAP.md); decisions in [DECISIONS.md](DECISIONS.md).

### Security review (Phase 1)

- **OAuth:** authorization code + PKCE (S256, added by QtNetworkAuth ≥ 6.5),
  `state` verified by Qt, loopback handler bound to 127.0.0.1:53682 only and
  closed again by Qt after the redirect. Tokens are never logged.
- **Markup injection (fixed):** QML labels default to `Text.AutoText`, so a
  task title, list name, category or file name containing HTML — e.g. from a
  list shared by someone else — was rendered as rich text and `<img>` sources
  were fetched (verified: three requests to a test server, i.e. a tracking
  pixel). Data labels now use `Text.PlainText`; style-owned labels get
  `App::plain()`; the clickable link label escapes name and URL; reminder
  notification bodies are HTML-escaped. Re-test: zero requests.
- **Links (fixed):** only `https`, `http`, `mailto` and `file` URLs are handed
  to the desktop.
- **Attachment names (fixed):** reduced to a safe base name before writing to
  the cache directory.
- **File permissions (fixed):** cache database, its directory and the
  attachment cache are owner-only (0600/0700).
- **Dependencies:** Qt 6, KDE Frameworks 6 and QtKeychain 0.16.0 (pinned with
  SHA-256 in the Flatpak manifest); no bundled third-party code.

### Performance (Phase 1)

- Model: 5 000 tasks, five sort-mode switches and a category search in
  ~180 ms (`tst_tasksmodel::handlesLargeLists`).
- Cache: 2 000 tasks written and read back well under a second
  (`tst_database::handlesLargeLists`).
- Startup: the demo window is mapped ~0.5 s after launch (Xvfb, warm QML
  cache, three runs). With an account the cached lists are shown before the
  network answers.

## Phase 1 — Product (summary)

Done (see ROADMAP.md for the full list):

- **Correctness:** paging through `@odata.nextLink` (lists > 200 tasks were
  truncated), request queue with Retry-After for HTTP 429/503, offline start
  with cached data and automatic reconnect, clean sign-out on revoked tokens,
  no hanging requests after failed refreshes, delta-handler leak fixed.
- **Identity and packaging:** app ID `io.github.mzilinski.merkzettel` with
  data migration, complete AppStream metainfo (`--strict` clean), Flatpak on
  KDE 6.11 with minimal permissions (built and run in the sandbox), AUR
  `merkzettel` and `merkzettel-git` (built with makepkg, namcap clean),
  version 0.4.0 with CHANGELOG and release scripts.
- **Quality gates:** 6 test suites (Graph client against a mock server,
  date parser, SQLite cache, models, QML smoke test of the demo flows),
  clang-tidy/clazy/cppcheck clean, ASan/UBSan clean, `-Werror` on Arch, CI on
  Arch/Fedora/openSUSE/Ubuntu 26.04 + Flatpak, release workflow, REUSE 3.3,
  pre-commit hooks, community files.
- **Translation and accessibility:** `.pot` generated by script, German
  complete ("du"), every control has an accessible name (checked in CI).
- **First run:** login page explains the requested permissions and offers
  the demo; builds without a client ID say why sign-in is unavailable.
- **Security:** markup/tracking-pixel injection through task data fixed,
  link scheme allow-list, safe attachment file names, private cache files.

Open from Phase 1: multiple accounts (deferred to v0.5, D12); an offline
change queue (D11); the Kirigami OverlaySheet binding-loop warning (upstream,
detail view could move to `Kirigami.Dialog`).

## Phase 2 — Documentation (summary)

- **MkDocs site** (Material, DE default + EN, `mkdocs build --strict`
  clean): installation per distribution, Azure setup, configuration, user
  guide, shortcuts, FAQ, troubleshooting, privacy, architecture with Mermaid
  diagrams rendered to static SVG, developer and translation guides.
- **How-tos** for five use cases (work account, quick capture, plan your
  day, organise tasks, offline), each with screenshots and a video.
- **`make screenshots`:** 8 views × Breeze Light/Dark × EN/DE from the demo
  mode in Xvfb with KWin, used by docs, README, metainfo and website.
- **`make videos`:** scripts in `docs/videos/*.md` (timeline, actions,
  narration EN/DE) drive a recorder in Xvfb (KWin, kglobalacceld, xdotool,
  ffmpeg x11grab). Piper narrates (Thorsten CC0 / LibriTTS-R CC BY 4.0),
  loudnorm to −16 LUFS, title and end cards rendered from HTML with
  Playwright, key presses shown as captions. Output: 10 MP4s (1920×1080,
  H.264/AAC, 71–94 s, 2–3 MB each), SRT + WebVTT in both languages, posters.
  `--check` verifies that every narration fits its slot without recording.
- The demo mode became editable (add, complete, edit, undo) so videos and
  visitors can try the real flows; `merkzettel --quick-add` and a tray entry
  were added for the quick-capture use case.

## Phase 3 — Website (summary)

- **Astro 7, static, no client-side JavaScript**, German at `/merkzettel/`,
  English at `/merkzettel/en/`, language switch and `hreflang` on every page.
- Pages: home (tagline, three benefits, light/dark screenshot, Flathub/AUR
  buttons), features, five use cases (problem → solution → result, video,
  screenshot, link to the how-to), installation, docs (MkDocs below
  `/docs/`), open source (licence, repository, contributing, translating,
  roadmap, donations), contact, imprint and privacy policy as drafts marked
  "Entwurf – rechtliche Prüfung ausstehend" (GitHub Pages named as host).
- No trackers, cookies, external fonts or CDNs; WebP screenshots with
  `prefers-color-scheme` sources; OpenGraph image, sitemap, `robots.txt`;
  `make site`; `.github/workflows/pages.yml` deploys docs + site with videos
  from Git LFS.
- Lighthouse (local preview, Chromium 1248): home, use cases, features and
  privacy pages 96–100 in performance, accessibility, best practices and SEO
  on mobile and desktop; the MkDocs start page 92–97. No broken internal
  links in `website/dist`.

## Phase 4 — Release preparation and final report

### Clean-room test

A fresh `git clone` from GitHub (with Git LFS) was built and checked as the
docs describe:

- `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build`
  without a client ID: builds, 6/6 test suites pass; the sign-in page
  explains the missing app registration and offers the demo, as the
  installation guide says.
- `make site` with the pinned tools from `docs/requirements.txt` and
  `npm ci`: docs (strict) and all 16 website pages build.
- `make_videos.py --check` passes for all ten videos (narration fits every
  slot); the full `make videos` ran locally (10 videos, ~25 min).
- CI on GitHub: the website workflow builds docs and site including the LFS
  videos; the deployment waits for Pages to be enabled (`DEPLOY_PAGES`).
- The Flatpak without `--own-name` was built and installed; inside the
  sandbox the tray icon registers with a StatusNotifierWatcher under its
  unique bus name.

Deviations found and fixed during Phase 4: Alt+F4 did nothing in the video
sessions (KWin registers it through kglobalacceld, which has to start first);
the translation template was out of date after the demo changes (CI lint
failed once); the Flathub linter rejects `--own-name` for the tray, which
turned out to be unnecessary.

### Done

All `[AUTO]` items of ROADMAP.md: the product hardening of Phase 1, MkDocs
documentation with how-tos, screenshots and narrated videos (Phase 2), the
Astro website with use cases, legal drafts and Pages workflow (Phase 3), the
`flathub/` submission directory with checklist and `prepare.py`, and
`HUMAN_TODO.md`.

### Open (needs the maintainer, see HUMAN_TODO.md)

1. Postal address in the imprint and legal review of imprint and privacy
   policy.
2. Enable GitHub Pages and set `DEPLOY_PAGES=true`.
3. Azure app registration for public releases; client ID as CI secret and in
   the PKGBUILDs.
4. Optional GPG key; tag and push `v0.4.0` (release workflow publishes the
   tarball and Flatpak bundle).
5. AUR account and upload of `merkzettel` / `merkzettel-git`.
6. Flathub pull request (`flathub/prepare.py`), review, then maintenance.
7. Confirm the PayPal donation link.

### Risks

- **Microsoft app registration:** without publisher verification, Microsoft
  shows the app as "unverified"; some organisations block such apps, and
  users then need admin approval (documented in the work-account how-to).
  Microsoft could also change consent policies for multi-tenant apps.
- **My Day** is stored locally because the official Graph API does not expose
  it; users switching between phone and desktop will notice. The private
  Substrate API that To Do uses is deliberately not used (stability, terms).
- **Flathub review** may question `org.kde.kglobalaccel` and KWallet access;
  the reasons are in the manifest comments and HUMAN_TODO.md.
- **Legal pages** are drafts; publishing the site before step 1 would put an
  incomplete imprint online (the deploy gate prevents this by default).
- **Git LFS quota:** re-rendering the videos often adds ~24 MB per run to LFS
  storage; the free quota covers many runs, but old objects are not pruned.
- **Kirigami OverlaySheet** emits a binding-loop warning (upstream); harmless,
  but noisy in logs.
- **Single maintainer:** bus factor of one for releases, Flathub and AUR.

### Proposal for v0.5

The brief calls the next version "v0.4"; since this release is 0.4.0, the
next feature release is 0.5.

1. **Multiple accounts** (D12): account dimension in token store, cache and
   sidebar; switch without signing out.
2. **Offline change queue** (D11): record edits made offline and replay them
   with conflict detection on reconnect.
3. **Task details as `Kirigami.Dialog`** instead of `OverlaySheet`: fixes the
   binding-loop warning and improves keyboard focus handling.
4. **Outlook category colours** via `/me/outlook/masterCategories` (needs the
   additional `MailboxSettings.Read` scope — to be weighed against the
   minimal-permission promise).
5. **Translations:** invite translators (French, Spanish, Dutch) through KDE
   or Weblate; the gettext setup is ready.
6. **Publisher verification** of the Azure app to remove the "unverified"
   label.
