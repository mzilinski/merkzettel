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
