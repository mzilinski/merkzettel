# Flathub submission

`io.github.mzilinski.merkzettel.yml` is the manifest for
[Flathub](https://docs.flathub.org/docs/for-app-authors/submission). It
builds the release tag `v0.4.0` with the KDE 6.11 runtime. The development
manifest in `flatpak/` builds the working tree instead.

`prepare.py` writes the copy that goes into the Flathub repository: it adds
the commit of the tag (Flathub requires tag and commit) and the client ID of
the public Azure registration.

```bash
python3 flathub/prepare.py --client-id <uuid> ../flathub-submission
```

## Checklist

Checked items were verified in this repository; the open ones need the
maintainer (see `HUMAN_TODO.md`).

- [x] App ID `io.github.mzilinski.merkzettel` matches the GitHub account
      `mzilinski` (automatic verification for `io.github.*`)
- [x] AppStream metainfo passes `appstreamcli validate --strict` and
      `flatpak-builder-lint appstream` (only "URL not reachable" until the
      website is published); it has a `<developer id>`, OARS rating,
      branding colours, release notes, screenshots, homepage, bug tracker,
      help, FAQ, donation, translation, contribution and VCS URLs
- [x] Desktop file passes `desktop-file-validate`; scalable SVG icon
- [x] Licence: GPL-3.0-or-later (`project_license`), REUSE compliant
- [x] Stable runtime `org.kde.Platform//6.11`; no network access during the
      build; the one extra dependency (QtKeychain) is pinned by SHA-256 and
      has `x-checker-data`
- [x] The app source is the release git tag, with `x-checker-data` for new tags
- [x] `flatpak-builder-lint manifest` reports no errors; the only warning
      (tag without commit) is resolved by `prepare.py`
- [x] Minimal permissions, each explained in the manifest: network, Wayland
      with X11 fallback, GPU, the tray watcher, KGlobalAccel (no portal
      exists for global shortcuts), the Secret Service/KWallet for the
      refresh token and read-only `kdeglobals` for the colour scheme.
      Notifications, links and file dialogs use portals; no home or host
      file system access
- [x] Built and run locally from the development manifest (sign-in, tray,
      notifications, global shortcut, attachments through the portal)
- [ ] Release `v0.4.0` tagged and pushed
- [ ] Client ID of the public Azure registration created
- [ ] Pull request against `flathub/flathub` (branch `new-pr`) opened,
      review comments answered
- [ ] After acceptance: repository `flathub/io.github.mzilinski.merkzettel`
      maintained (new releases are proposed by the external data checker)
