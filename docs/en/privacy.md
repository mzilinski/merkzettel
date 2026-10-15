# Privacy

Merkzettel is a client for your Microsoft account. It has no server of its own,
no telemetry, no crash reporting and no update check.

## What is sent where

| Destination | What | Why |
|---|---|---|
| `login.microsoftonline.com` | Sign-in and token refresh | To access your account |
| `graph.microsoft.com` | Your lists, tasks, steps, links, categories, attachments | To read and change your To Do data |

Nothing is sent to the Merkzettel project or any other third party. Opening a
link or an attachment hands it to the application your desktop uses for it.

## What is stored on your computer

| Data | Location | Protection |
|---|---|---|
| Sign-in (refresh) token | System keychain (KWallet / Secret Service) | Encrypted by the keychain |
| Copy of lists and tasks, My Day | `~/.local/share/merkzettel/cache.sqlite` | Readable only by your user (0600) |
| Attachments you opened | `~/.cache/merkzettel/attachments/` | Readable only by your user (0700) |
| View settings | `~/.config/merkzettelrc` | Normal user file |

The Flatpak keeps these files under `~/.var/app/io.github.mzilinski.merkzettel/`.

## Removing your data

1. **Sign out** in Merkzettel removes the token from the keychain.
2. Delete the files above.
3. To revoke Merkzettel's access on Microsoft's side, open
   [myapps.microsoft.com](https://myapps.microsoft.com) (work/school) or
   [account.live.com/consent/Manage](https://account.live.com/consent/Manage)
   (personal account) and remove the app.

## Microsoft

Your To Do data is processed by Microsoft under Microsoft's privacy statement
and, for work or school accounts, your organisation's agreements with
Microsoft. Merkzettel does not change that.

The privacy policy of the Merkzettel website is on the
[website](https://mzilinski.github.io/merkzettel/en/privacy/).
