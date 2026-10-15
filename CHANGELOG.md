<!--
SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
SPDX-License-Identifier: CC0-1.0
-->

# Changelog

All notable changes to Merkzettel are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project
uses [Semantic Versioning](https://semver.org/). The version is maintained
in `CMakeLists.txt` only; `scripts/release.py` keeps this file and the
AppStream metainfo in sync.

## [Unreleased]

## [0.4.0] - 2026-10-15

First public release.

### Added

- My Day (stored locally — Microsoft syncs it through an API that third-party
  apps cannot use), Important, Planned and Completed smart lists.
- Per-list sorting by importance, due date, title or creation date, with an
  indicator to reverse or remove it; option to hide completed tasks.
- Keyboard shortcuts modelled on Microsoft To Do (Ctrl+N, Ctrl+L, Ctrl+F,
  Ctrl+D, Ctrl+T, Ctrl+R/F5, Alt+Up/Down, Ctrl+Z) and a global quick-add
  shortcut (Meta+Alt+N).
- Moving tasks between lists, multi-selection with bulk actions and undo for
  delete, move and bulk complete.
- Categories, a custom recurrence editor (every N days/weeks/months/years,
  weekday picker) and file attachments (up to 25 MB, drag and drop).
- Works offline with the cached data and reconnects automatically.
- Quick-add understands English and German date keywords.
- `merkzettel --quick-add` and a *New task …* tray entry open the new-task
  field, also for a running instance.
- The sign-in page lists the requested permissions and offers a demo mode
  in which tasks can be added and edited locally.

### Changed

- Application ID is now `io.github.mzilinski.merkzettel`; existing data is
  migrated on first start.
- Dates follow the formats locale; narrow windows show the current list as
  page title.

### Fixed

- Lists with more than 200 tasks were truncated in the local cache.
- Microsoft Graph throttling (HTTP 429) is handled with a request queue and
  Retry-After instead of surfacing errors.
- Escape closes the task detail sheet; Space toggles the selection; readable
  text on selected rows.
- Expired or revoked sign-ins end the session with an explanation instead of
  leaving requests hanging.

## [0.3.1] - 2026-05-01

Development version, not published.

### Added

- Steps (checklist items), delta sync, shared-list indicator, recurrence,
  reminder notifications, linked resources, list creation, search, the All
  smart list, colour scheme switcher and single-instance handling.

## [0.2.0] - 2026-05-01

Development version, not published.

### Added

- Sign-in with OAuth 2.0 + PKCE, refresh token in KWallet, lists and tasks
  with due dates, reminders, importance and notes, sections by due date,
  tray icon, SQLite cache, German translation and a demo mode.

[Unreleased]: https://github.com/mzilinski/merkzettel/compare/v0.4.0...HEAD
[0.4.0]: https://github.com/mzilinski/merkzettel/releases/tag/v0.4.0
