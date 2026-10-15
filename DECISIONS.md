<!--
SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
SPDX-License-Identifier: CC0-1.0
-->

# Decisions

Architecture and process decisions taken while preparing Merkzettel for
release. Newest last. Each entry: context, decision, consequences.

## D1 — Application ID `io.github.mzilinski.merkzettel`

- **Context:** `org.kde.*` IDs are reserved for KDE-hosted projects and cannot
  be verified on Flathub. A `io.github.<user>.<repo>` ID is verified
  automatically through the GitHub account.
- **Decision:** Rename the app ID everywhere (desktop file, AppStream
  metainfo, icon name, Flatpak manifest, D-Bus single-instance name,
  `KAboutData::desktopFileName`). The organization domain becomes
  `mzilinski.github.io`, the organization name is dropped so data lands in
  `~/.local/share/merkzettel` and `~/.cache/merkzettel`.
- **Consequences:** Existing native installs keep their data through a
  one-time migration from the old `KDE/merkzettel` locations on first start.
  `merkzettelrc` and the KWallet entry (`merkzettel`/`refresh_token`) do not
  depend on the ID and stay where they are.

## D2 — Azure client ID for public builds

- **Context:** Flathub builds from a public manifest without secrets, so a
  release build needs its client ID in a public file. A public-client (PKCE)
  client ID is not a secret, but the owner was unsure about publishing the
  ID of the existing app registration.
- **Decision:** The existing client ID is **not** committed. `MERKZETTEL_CLIENT_ID`
  stays a CMake cache variable; CI keeps using the `MERKZETTEL_CLIENT_ID`
  secret. A dedicated "Merkzettel (public releases)" app registration is a
  [HUMAN] task; its ID is entered in exactly two places (`flathub/` manifest,
  AUR `PKGBUILD`) as documented in `HUMAN_TODO.md`.
- **Consequences:** Until then, Flathub/AUR artefacts cannot be published;
  source builders use their own registration (`docs/azure-setup.md`).
  Builds without a client ID still run (`--demo`) and explain the missing
  registration on the login page.

## D3 — No custom domain

- **Context:** The brief allows a custom domain or GitHub Pages.
- **Decision:** The owner chose "only the GitHub page": the website is served
  at `https://mzilinski.github.io/merkzettel/`. No `CNAME` file.

## D4 — Legal pages

- **Decision:** Imprint and privacy policy are drafted under German law (DDG,
  GDPR) with the owner's name and e-mail. The postal address required by
  § 5 DDG is supplied by the owner before go-live ([HUMAN]); until then both
  pages carry a visible "Draft – legal review pending" banner.

## D5 — German translation uses "du"

- **Context:** Existing German strings already address the user informally
  ("deine Aufgaben", "Konzentriere dich"), as KDE's German team does.
- **Decision:** Informal "du" throughout app, docs and website.

## D6 — Version source of truth

- **Decision:** `project(merkzettel VERSION x.y.z)` in `CMakeLists.txt` is the
  only place the version is maintained. It is compiled into the binary and
  checked against the latest `CHANGELOG.md` / metainfo release entry in CI.

## D7 — Ubuntu LTS CI target

- **Context:** The brief asks for Ubuntu LTS. Ubuntu 24.04 does not ship KF6 /
  Kirigami 6; Ubuntu 26.04 LTS does.
- **Decision:** CI builds on `ubuntu:26.04` (container) in addition to Arch,
  Fedora and openSUSE.

## D8 — First public release is 0.4.0

- **Context:** `0.3.1` was set in CMake but never tagged or released; a lot
  of user-visible functionality was added since.
- **Decision:** The first tagged, public release is `0.4.0`. `scripts/release.sh`
  bumps CMake version, metainfo release entry and `CHANGELOG.md` together.

## D9 — Flatpak runtime and keychain access

- **Decision:** `org.kde.Platform` 6.11 (newest on Flathub at the time).
  QtKeychain is built with libsecret so the refresh token goes through the
  Secret Service (`org.freedesktop.secrets`, provided by KWallet 6/ksecretd
  and GNOME Keyring); `org.kde.kwalletd6` stays for Plasma setups without
  the Secret Service interface. Notifications, opening URLs/files and file
  dialogs use portals and need no extra permission. KGlobalAccel has no
  portal, hence `--talk-name=org.kde.kglobalaccel`.
- **Single settings writer:** QML `Settings` (QSettings) is no longer used;
  all persistent UI state goes through KConfig (`merkzettelrc`) so two
  writers cannot clobber each other.

## D10 — Licensing of non-code files and code style

- **Decision:** `REUSE.toml` licenses code, QML, icons and screenshots as
  GPL-3.0-or-later, metadata/config as CC0-1.0 (required for AppStream) and
  documentation/website content as CC-BY-SA-4.0.
- **Code style:** `.clang-format` (WebKit-based, 100 columns) was chosen for
  the least churn against the existing hand-formatted code and applied once
  in a formatting-only commit listed in `.git-blame-ignore-revs`.
- **Code of conduct:** written in the project's own words (not a verbatim
  third-party text) so no further license file is needed.

## D11 — Concurrent edits and offline changes

- **Context:** Microsoft To Do may change the same task on another device
  while Merkzettel shows it; Graph offers no ETag-based conditional updates
  for To Do tasks that are worth relying on.
- **Decision:** Last writer wins **per field**: Merkzettel PATCHes only the
  field the user changed (title, due date, importance, …), so a remote edit
  of a different field is kept. The next delta sync (on every list switch,
  manual sync and after each change) shows the server state. While offline,
  changes are not queued; they fail with "No connection to Microsoft" and
  the cached data stays read-only in practice. A persistent offline queue
  is a candidate for v0.4.

## D12 — Multiple accounts deferred to v0.4

- **Context:** The v0.3 roadmap listed multiple accounts. Graph supports it
  with the existing scopes, but the token store, cache schema (per-account
  database), list sidebar, tray badge, reminders and every Graph call would
  need an account dimension.
- **Decision:** Not part of 0.4.0. It is the first item proposed for v0.4
  (REPORT.md) so the release can ship with the reviewed single-account code.

## D13 — Documentation toolchain

- **Decision:** MkDocs 1.6 with Material 9.7 and mkdocs-static-i18n, pinned
  in `docs/requirements.txt`. The Material team warns that MkDocs 2.0 removes
  the plugin system; the docs stay on 1.x until a migration path exists.
  Diagrams are rendered to static SVG at build time (Mermaid in headless
  Chromium) so no JavaScript or CDN is needed on the documentation pages.
