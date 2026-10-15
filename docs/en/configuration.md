# Configuration

Merkzettel has no settings dialog; everything is changed where you use it and
remembered automatically.

## Appearance

**Appearance** in the sidebar menu switches between **Follow system**,
**Light** and **Dark**. *Follow system* uses your Plasma colour scheme,
including the accent colour.

## Sorting and completed tasks

- The **sort** button in the toolbar sorts the current list by importance, due
  date, title or creation date. The choice is stored **per list**; the bar
  below the search field shows the active order, reverses it (arrow) or removes
  it (×).
- **Show completed tasks** in the ⋮ menu hides or shows completed tasks in all
  lists. The *Completed* smart list always shows them.

## Tray and autostart

- Closing the window keeps Merkzettel running in the system tray; the tray icon
  shows how many tasks are due today. **Quit** in the tray menu ends it.
- To start Merkzettel with your session, open **System Settings → Autostart →
  Add → Add Application**, choose Merkzettel and, if you like, add `--tray` to
  start it hidden. The application menu also offers **Start in tray**.

## Global shortcut

The quick-add shortcut **Meta+Alt+N** brings Merkzettel to the front with the
cursor in the new-task field, from any application. Change it in **System
Settings → Keyboard → Shortcuts → Merkzettel**.

## Notifications

Reminders appear as desktop notifications with a **Mark done** action. Their
sound, position and do-not-disturb behaviour are configured in **System
Settings → Notifications → Applications → Merkzettel**.

## Files and locations

| What | Where (Flatpak: below `~/.var/app/io.github.mzilinski.merkzettel/`) |
|---|---|
| View options (sorting, colour scheme, banner flags) | `~/.config/merkzettelrc` |
| Local copy of lists and tasks, My Day | `~/.local/share/merkzettel/cache.sqlite` |
| Attachments you opened | `~/.cache/merkzettel/attachments/` |
| Sign-in token | System keychain (KWallet / Secret Service), entry `merkzettel` |

Deleting the cache is safe: Merkzettel downloads everything again on the next
start. Only **My Day** is lost, because Microsoft does not offer it to other
apps (see the [FAQ](faq.md#my-day)).
