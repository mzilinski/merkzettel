# User guide

![Merkzettel with lists on the left and tasks on the right](../assets/screenshots/en/tasks-light.png#only-light){ .screenshot }
![Merkzettel with lists on the left and tasks on the right](../assets/screenshots/en/tasks-dark.png#only-dark){ .screenshot }

The window has two parts: the **sidebar** with smart lists and your lists, and
the **task list** of the selected list. On narrow windows the sidebar folds
into the ☰ button and the page title shows the current list.

## Signing in

On the first start Merkzettel shows the sign-in page. **Sign in** opens your
browser with Microsoft's sign-in; after you approve, Merkzettel loads your
lists. The page lists exactly which permissions are requested and why.
**Try the demo** shows sample data instead; **Leave demo** in the sidebar
returns to the sign-in page.

You stay signed in until you choose **Sign out**. If Microsoft revokes the
sign-in (password change, consent withdrawn, long inactivity), Merkzettel
says so and shows the sign-in page again.

## Lists

- **Create:** **New list …** in the sidebar menu (<kbd>Ctrl</kbd>+<kbd>L</kbd>).
- **Rename / delete:** right-click a list. The default list *Tasks* cannot be
  renamed or deleted.
- **Shared lists** carry a share icon. Merkzettel shows and edits them like
  your own lists; **Share …** opens the Microsoft To Do web app, because
  Microsoft does not let other apps manage sharing.

## Smart lists

| Smart list | Shows |
|---|---|
| **My Day** | Tasks you picked for today (stored on this device, starts empty every day) |
| **All** | Every task from every list |
| **Important** | Tasks marked with a star |
| **Planned** | Tasks with a due date or reminder |
| **Completed** | Completed tasks |

Tasks you add while a smart list is selected go to your default list and get
the matching property — starred in *Important*, due today in *Planned*, in My
Day in *My Day*.

## Adding tasks

Type into **New task …** and press <kbd>Enter</kbd>. A date word at the end
sets the due date:

| You type | Due date |
|---|---|
| `Buy milk today` / `heute` | today |
| `Call Anna tomorrow` / `morgen` | tomorrow |
| `Dentist übermorgen` | the day after tomorrow |
| `Pay rent mon` / `mo` / `monday` / `montag` | the next Monday (never today) |
| `Doctor 25.5.` or `25.5.2027` | that date |

![Typing a task with a date word](../assets/screenshots/en/quickadd-light.png#only-light){ .screenshot }
![Typing a task with a date word](../assets/screenshots/en/quickadd-dark.png#only-dark){ .screenshot }

## Sections, sorting and search

Without an explicit sort order tasks are grouped into **Overdue**, **Today**,
**Tomorrow**, **This week**, **Later**, **No date** and **Completed**.
The sort button orders the list by importance, due date, title or creation
date instead; each list remembers its own order. The search field filters by
title, notes and category.

![A list sorted alphabetically](../assets/screenshots/en/sorted-light.png#only-light){ .screenshot }
![A list sorted alphabetically](../assets/screenshots/en/sorted-dark.png#only-dark){ .screenshot }

## Task details

Click a task (or select it and press <kbd>Enter</kbd>) to open its details:

- **Title**, **importance** (star) and **My Day**
- **Due date** with *Today*, *Tomorrow* or a date picker
- **Repeat:** daily, weekdays, weekly, monthly, yearly or **Custom …** — every
  *N* days, weeks, months or years, for weeks on chosen weekdays. Repeats are
  anchored on the due date.
- **Categories:** type a name or pick one used on other tasks
- **Reminder** with date and time; you get a desktop notification
- **Subtasks** (steps) with check boxes
- **Links** to web pages or files
- **Files:** attach with **Add file …** or by dragging files onto the section
  (up to 25 MB each); open, save or delete them
- **Notes**, plus when the task was created and last changed

![Task details](../assets/screenshots/en/details-light.png#only-light){ .screenshot }
![Task details](../assets/screenshots/en/details-dark.png#only-dark){ .screenshot }

## Completing, deleting and undo

Tick the check box to complete a task; tick it again to reopen it. **Delete**
(in the context menu, the hover button or with <kbd>Delete</kbd>) removes a
task after a few seconds — until then **Undo** in the notification or
<kbd>Ctrl</kbd>+<kbd>Z</kbd> brings it back.

## Several tasks at once

<kbd>Ctrl</kbd>+click adds a task to the selection, <kbd>Shift</kbd>+click
selects a range, <kbd>Space</kbd> toggles the current task and
<kbd>Ctrl</kbd>+<kbd>A</kbd> selects all. The selection bar then completes,
stars, adds to My Day, moves or deletes all selected tasks.

![Two tasks selected](../assets/screenshots/en/selection-light.png#only-light){ .screenshot }
![Two tasks selected](../assets/screenshots/en/selection-dark.png#only-dark){ .screenshot }

## Moving tasks between lists

Right-click a task → **Move to**, or use the selection bar. Microsoft Graph has
no "move" operation, so Merkzettel copies the task with its steps, links and
files into the target list and then deletes the original. The copy gets a new
creation date. If something fails along the way, the original is kept.

## My Day

**My Day** is your plan for today: add tasks with <kbd>Ctrl</kbd>+<kbd>T</kbd>,
the context menu, the selection bar or the switch in the details. Tasks in My
Day show a sun in all other lists. My Day starts empty every morning.

![My Day](../assets/screenshots/en/myday-light.png#only-light){ .screenshot }
![My Day](../assets/screenshots/en/myday-dark.png#only-dark){ .screenshot }

!!! info "Only on this device"
    Microsoft To Do keeps My Day in an internal service that third-party apps
    cannot use. Merkzettel therefore stores My Day locally; it is not shared
    with Microsoft To Do on your phone or the web.

## Reminders

When a reminder is due, Merkzettel shows a desktop notification with
**Mark done**. Reminders only fire while Merkzettel runs — keep it in the tray
or start it with your session (see [Configuration](configuration.md)).

## Offline

Merkzettel keeps a copy of your lists. Without a connection it starts with
that copy and shows a banner; changes need a connection and are not queued.
When the network returns, Merkzettel reconnects by itself, or choose
**Reconnect**.

![Offline banner](../assets/screenshots/en/offline-light.png#only-light){ .screenshot }
![Offline banner](../assets/screenshots/en/offline-dark.png#only-dark){ .screenshot }

## Synchronisation

Merkzettel fetches changes when you switch lists, when you press
<kbd>Ctrl</kbd>+<kbd>R</kbd> / <kbd>F5</kbd> and after your own changes. It
only asks Microsoft for what changed since the last time (delta sync). If
Microsoft asks it to slow down, it waits and retries on its own.
