# Handoff — current state

Quick orientation for the next person (or the next-you-after-a-break) picking up the codebase. Detail-lists live in their authoritative docs; this file just points at them.

## Current version

**0.3.1** — defined in `CMakeLists.txt:3`, propagated to C++ via `MERKZETTEL_VERSION` and into QML via the `appVersion` context property (`src/main.cpp`). Single source of truth: bump the CMake project version, both surfaces follow.

## What's in 0.3 (compared to 0.2 baseline)

Aligned the feature surface with `mcp-outlook365`'s v0.3 modern-todo-app analysis (see `whatsnew.md` in that repo for the requirements list).

| Feature | Where | Notes |
|---|---|---|
| **Subtasks (checklist items)** | `src/graph/todoapi.cpp` (5 new methods + parser) | Eager-loaded via `$expand=checklistItems` on the tasks fetch. UI in `TaskDetailSheet.qml`, progress indicator `x/y` in `TaskDelegate.qml`. Risks: `memory/checklist_items_risks.md`. |
| **Delta sync** | `src/graph/todoapi.cpp::syncTasks` + `bootstrapDeltaLink` | Per-list delta token persisted in `lists.delta_link`. Falls back to full fetch on HTTP 410 (token expired). Subtask changes are picked up by re-fetching `/checklistItems` for each delta-changed task. Risks: `memory/delta_sync_risks.md`. |
| **Shared-list visibility** | `src/Main.qml` sidebar | Shows `emblem-shared` icon on lists with `isShared = true`. `Share ...` context menu item opens `to-do.office.com` in the browser. First-time inline banner explains the handoff. Read-only by Graph constraint — see `docs/experimental-share.md` for the retrofit plan. |
| **Cache schema migrations** | `src/cache/database.cpp::createSchema` | `CREATE TABLE IF NOT EXISTS` plus best-effort `ALTER TABLE ADD COLUMN` for each column added since v0.2. Existing caches upgrade silently on next open. |

## Architecture, unchanged

| Layer | Path | Purpose |
|---|---|---|
| OAuth + token | `src/auth/` | PKCE flow, refresh token in KWallet via QtKeychain. |
| HTTP transport | `src/graph/graphclient.cpp` | Bearer auth, 401-refresh-retry, `getAbsolute()` for delta `nextLink`/`deltaLink`. Errors are formatted `HTTP <status>: <msg>` so callers can detect specific codes (e.g. 410). |
| To-Do REST surface | `src/graph/todoapi.cpp` | All `/me/todo/lists/...` endpoints. 27 methods now (Lists CRUD, Tasks CRUD, ChecklistItems, Delta-Sync). |
| Cache | `src/cache/database.cpp` | SQLite. Three tables: `lists`, `tasks`, `checklist_items`. |
| Models | `src/models/` | `TaskListsModel`, `TasksModel` — `QAbstractListModel` exposed to QML. |
| UI | `src/*.qml` | Kirigami. `Main.qml` is the shell (drawer + page stack), `TasksPage.qml` the list view, `TaskDetailSheet.qml` the editor, `TaskDelegate.qml` a row. |

## Known risks (in memory)

- `memory/checklist_items_risks.md` — Graph 20-items-per-task cap, `$expand` payload bloat thresholds.
- `memory/delta_sync_risks.md` — `$expand` not supported on `/tasks/delta`, HTTP-410 detection via string-prefix is brittle, bootstrap race on first login.

## Open work

Backlog, in roughly the order I'd attack it:

1. **Recurrence** (`recurrence` field passthrough + minimal Daily/Weekly/Monthly/Yearly UI). Vorlage in `mcp-outlook365/tools/todo.py`.
2. **KNotifications** for reminders — currently the reminder field is stored but never fires a popup. KF6::Notifications is already linked in `src/CMakeLists.txt`.
3. **Quick-Add global shortcut** via KGlobalAccel. New top-level entry point that opens just the add bar.
4. **Drag-and-drop between lists** — needs `move_todo` atomicity pattern from `mcp-outlook365/tools/todo.py` (`$batch` + `dependsOn`).
5. **Linked Resources** (`/linkedResources` endpoint) — link tasks to URLs / files. Useful even without sharing.
6. **Attachments** (inline up to 3 MiB).
7. **Experimental share-management.** Plan: `docs/experimental-share.md`. Reverse-engineering work, opt-in only.
8. **`GraphCallback` refactor**: pass HTTP status to callbacks instead of detecting it via error-message string-prefix. Touches every Graph caller, low-risk but invasive — wait until there's another reason to refactor that area.

## Useful commands

```bash
# Build + run
cmake --build build -j
./build/bin/merkzettel --demo               # demo data, no sign-in, no network

# Headless smoke check (used in CI mental model)
QT_QPA_PLATFORM=offscreen timeout 5 ./build/bin/merkzettel --demo

# CI status
gh run list --limit 5
```

## References

- `README.md` — user-facing intro, install, build, dependencies.
- `docs/azure-setup.md` — App Registration steps.
- `docs/experimental-share.md` — design + risk doc for the share-management retrofit (mitmproxy-based reverse engineering).
- `mcp-outlook365/whatsnew.md` (sibling repo) — feature requirements analysis used to scope v0.3.
