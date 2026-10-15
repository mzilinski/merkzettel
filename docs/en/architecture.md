# Architecture

Merkzettel is a single-process Qt 6 application. All application logic runs on
the GUI thread; networking is asynchronous through `QNetworkAccessManager` and
the SQLite cache is small enough to be used synchronously, so there is no
shared state between threads.

## Components

![Component overview](../assets/diagrams/components.svg)

| Part | Files | Responsibility |
|---|---|---|
| Entry point | `src/main.cpp` | `KAboutData`, single instance (`KDBusService`), migration of old data locations, QML engine |
| Controller | `src/app.{h,cpp}` | Exposed to QML as `app`: routes UI actions to Graph and the cache, undo window, My Day, demo mode |
| Authentication | `src/auth/` | OAuth 2.0 authorization code flow with PKCE (QtNetworkAuth), loopback redirect, refresh token via QtKeychain |
| Graph client | `src/graph/graphclient.*` | Request queue (max. 4 concurrent), Retry-After back-off for 429/503, token refresh on 401, raw downloads and upload-session chunks |
| To Do API | `src/graph/todoapi.*` | Lists, tasks, steps, linked resources, attachments, delta sync, paging, move-by-copy |
| Cache | `src/cache/database.*` | SQLite: lists, tasks, steps, linked resources, delta links, My Day; schema upgrades with `ALTER TABLE` |
| Models | `src/models/` | `TaskListsModel` (smart lists + lists), `TasksModel` (filter, sort, sections, hidden pending deletes) |
| Utilities | `src/util/dateparser.*` | Date words in the add bar |
| UI | `src/*.qml` | Kirigami pages, detail sheet, dialogs |
| Tray | `src/tray/` | `KStatusNotifierItem` with due-today badge |

`merkzettel_core` (auth, Graph, cache, models, utilities) and `merkzettel_app`
(controller, tray) are static libraries; the executable only adds `main.cpp`.
The tests link the same libraries.

## Synchronisation

![Synchronisation sequence](../assets/diagrams/sync.svg)

- The UI always shows the cache first and refreshes when Graph answers.
- Lists with a stored delta link are synchronised incrementally; an expired
  delta token (HTTP 410) falls back to a full fetch.
- A full fetch follows every `@odata.nextLink` before replacing the cached list.
- Changes are sent as PATCH of the changed field only (per-field last writer
  wins, see `DECISIONS.md` D11).
- Deletes and moves are hidden locally and sent after a six-second undo window.

## Sign-in

![Sign-in sequence](../assets/diagrams/auth.svg)

A stored refresh token counts as "signed in": the window shows cached data
immediately and synchronisation starts once a refresh succeeds. Network errors
during refresh keep the session and retry (every minute and when
`QNetworkInformation` reports connectivity); `invalid_grant` ends the session.

## Security notes

- Text from tasks is never rendered as rich text (`Text.PlainText` or
  `App::plain()`), so content of shared lists cannot inject markup or load
  remote images.
- Only `https`, `http`, `mailto` and `file` links are opened.
- Cache and attachment directories are private to the user.
- See [`SECURITY.md`](https://github.com/mzilinski/merkzettel/blob/main/SECURITY.md).

## Decisions

Design decisions (app ID, client ID policy, licensing, offline behaviour, …)
are recorded in
[`DECISIONS.md`](https://github.com/mzilinski/merkzettel/blob/main/DECISIONS.md).
