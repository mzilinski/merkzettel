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
