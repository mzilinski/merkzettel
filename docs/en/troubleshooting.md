# Troubleshooting

Start Merkzettel from a terminal (`merkzettel`, Flatpak:
`flatpak run io.github.mzilinski.merkzettel`) to see its messages.

## Sign-in

**The browser does not open.**
Merkzettel asks the desktop to open the sign-in page. Check that a default web
browser is set (*System Settings → Default Applications*). In the Flatpak this
goes through the desktop portal (`xdg-desktop-portal` must be running).

**"Port 53682 in use".**
The sign-in result is delivered to `http://localhost:53682/callback`. Close
other programs using that port (or a second sign-in attempt) and retry.

**"Need admin approval" / "AADSTS65001".**
Your organisation requires an administrator to approve the app. See
[How-to: work account](howto/work-account.md).

**"This build has no Microsoft app registration".**
The app was built without a client ID. Install it from Flathub or the AUR, or
build with your own registration ([Azure setup](azure-setup.md)).

**Signed out unexpectedly.**
Microsoft revoked the sign-in (password change, consent withdrawn, long
inactivity). Sign in again; your lists are still cached.

## Keychain (KWallet / Secret Service)

Merkzettel stores the sign-in token in the system keychain.

- **Asked to sign in after every start:** the keychain is locked or disabled.
  On Plasma open *System Settings → KDE Wallet* and enable the wallet, or make
  sure a Secret Service provider (KWallet 6, GNOME Keyring) runs.
- **A wallet password prompt appears at login:** set the wallet password to your
  login password to unlock it automatically (PAM).
- **Remove the token manually:** open *KWalletManager* (or *Seahorse*) and
  delete the entry `merkzettel` / `refresh_token`.

## Tray icon

- **No tray icon on Plasma:** check that *System Tray* shows
  *Merkzettel* under *Entries* (it may be hidden).
- **GNOME:** install the *AppIndicator and KStatusNotifierItem Support*
  extension; GNOME has no tray by default. Without a tray, closing the window
  hides Merkzettel — start it again from the application menu.
- **Wayland:** the tray works on Plasma Wayland. If clicking the icon does not
  raise the window, use the global shortcut <kbd>Meta</kbd>+<kbd>Alt</kbd>+<kbd>N</kbd>.

## Notifications

No reminder notifications? Merkzettel has to be running (tray). Check
*System Settings → Notifications → Applications → Merkzettel* and
do-not-disturb mode. The Flatpak sends notifications through the portal.

## Global shortcut does nothing

The shortcut is registered with KGlobalAccel. Outside Plasma this service may
be missing. On Plasma, check *System Settings → Keyboard → Shortcuts →
Merkzettel* for conflicts. On other desktops bind a key to
`merkzettel --quick-add` in the desktop's keyboard settings.

## Flatpak

- **Dark mode is not applied:** the Flatpak reads `~/.config/kdeglobals`; make
  sure the KDE runtime theme matches (`flatpak update`).
- **Attachments from other folders:** files are opened and saved through the
  file-chooser portal; drag and drop works from Dolphin.

## Synchronisation

- **"No connection to Microsoft":** you are offline or a proxy blocks
  `graph.microsoft.com` / `login.microsoftonline.com`.
- **Changes from the phone appear late:** press <kbd>F5</kbd>. Microsoft may
  take a few seconds to make changes visible to other apps.
- **Start over:** quit Merkzettel and delete `~/.local/share/merkzettel/`;
  everything except My Day is downloaded again.

## Reporting a bug

Open an issue on [GitHub](https://github.com/mzilinski/merkzettel/issues/new/choose)
with the version (*About Merkzettel*), how you installed it, your distribution
and the terminal output. Remove task titles or e-mail addresses you do not want
to make public.
