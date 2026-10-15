<!--
SPDX-FileCopyrightText: 2026 Malte Zilinski <malte@zilinski.eu>
SPDX-License-Identifier: CC0-1.0
-->

# HUMAN_TODO — what only the maintainer can do

Everything else for the 0.4.0 release is done in the repository (see
`REPORT.md`). The steps below need your accounts, your identity or a legal
decision. Do them **in this order**; later steps depend on earlier ones.

| # | Task | Why it cannot be automated | Effort |
|---|---|---|---|
| 1 | Imprint address and legal review | Personal data and legal responsibility | 15 min + review |
| 2 | Enable GitHub Pages | Repository setting | 2 min |
| 3 | Azure app for public releases | Your Microsoft account; no app may be created on your behalf | 15 min |
| 4 | Client ID into CI and packaging | Depends on 3 | 5 min |
| 5 | GPG key for signed tags (optional, recommended) | Your identity | 15 min |
| 6 | Tag and publish v0.4.0 | Release decision | 10 min |
| 7 | AUR packages | Needs your AUR account | 20 min |
| 8 | Flathub submission | Needs your GitHub account and review dialogue | 30 min + review time |
| 9 | Check the donation link | Your PayPal account | 2 min |

## 1. Imprint address and legal review

The imprint (`/impressum/`) and privacy policy (`/datenschutz/`) are drafts
written along § 5 DDG and the GDPR, with GitHub Pages named as host. They
are marked on the page as *"Entwurf – rechtliche Prüfung ausstehend"*.

1. Open `website/src/components/pages/Imprint.astro` and replace
   `Postanschrift: wird vor der Veröffentlichung ergänzt` (German) and
   `Postal address: to be added before publication` (English) with a postal
   address where you can be served (a c/o or impressum service is possible).
2. Have both pages reviewed (e.g. by a lawyer or an
   imprint generator you trust). Check in particular whether the
   § 18 (2) MStV block is needed for your case.
3. When done, remove the draft notice: delete the line
   `<p class="notice" role="note">{T[lang].draft}</p>` in `Imprint.astro`
   and `Privacy.astro`.
4. Commit and push; the website workflow redeploys automatically.

## 2. Enable GitHub Pages

1. GitHub → *mzilinski/merkzettel* → **Settings → Pages**.
2. **Source: GitHub Actions**.
3. **Actions → Website → Run workflow** (or push any change below
   `website/` or `docs/`).
4. Open <https://mzilinski.github.io/merkzettel/> and
   <https://mzilinski.github.io/merkzettel/docs/>.
5. Videos are stored with Git LFS. GitHub's free LFS quota is enough for them;
   if a deploy shows broken videos, check **Settings → Billing → Git LFS**.

Do this before step 8: the AppStream metainfo links to the website, and
Flathub's linter checks the links.

## 3. Azure app registration for public releases

Packages from Flathub and the AUR need a client ID that belongs to the
project, not to a private test registration (decision D2). Follow
`docs/en/azure-setup.md` with these values:

- **Name:** `Merkzettel` (users see it on Microsoft's consent screen)
- **Supported account types:** *Accounts in any organizational directory and
  personal Microsoft accounts*
- **Redirect URI:** *Public client/native*, `http://localhost:53682/callback`
- **Authentication → Allow public client flows:** *Yes*
- **API permissions (delegated):** `Tasks.ReadWrite`, `User.Read`,
  `offline_access`, `openid`, `profile` — **no client secret**
- Recommended: **Branding & properties** → publisher domain / homepage
  `https://mzilinski.github.io/merkzettel/`, privacy statement
  `https://mzilinski.github.io/merkzettel/datenschutz/`. Optional later:
  *publisher verification* (needs a Microsoft Partner account) removes the
  "unverified" label on the consent screen.

Test it once: `cmake -B build -DMERKZETTEL_CLIENT_ID=<id> && cmake --build build && ./build/bin/merkzettel`,
sign in with a personal and, if possible, a work account.

## 4. Put the client ID where releases are built

The client ID is not secret (it is visible in every binary), but it is kept
out of the repository so forks do not reuse it by accident.

1. GitHub → **Settings → Secrets and variables → Actions → New repository
   secret**: name `MERKZETTEL_CLIENT_ID`, value = the UUID. CI and the release
   workflow pick it up automatically.
2. AUR: set `_clientid=<uuid>` in `packaging/aur/merkzettel/PKGBUILD` and
   `packaging/aur/merkzettel-git/PKGBUILD`, then run `makepkg --printsrcinfo > .SRCINFO`
   in both directories. Commit.
3. Flathub: passed to `flathub/prepare.py` in step 8.

## 5. GPG key for signed tags (recommended)

`scripts/release.py` and the AUR package expect signed tags.

```bash
gpg --full-generate-key                      # ed25519, your name and malte@zilinski.eu
gpg --list-secret-keys --keyid-format=long   # copy the key ID
git config --global user.signingkey <KEYID>
gpg --armor --export <KEYID>                 # add under GitHub → Settings → SSH and GPG keys
```

Optionally add the fingerprint to `validpgpkeys=()` in the release PKGBUILD.

## 6. Tag and publish v0.4.0

The changelog, version and metainfo are already prepared for 0.4.0
(`python3 scripts/check-version.py` passes).

1. If more than a day passes before tagging, update the date: in
   `CHANGELOG.md` (`## [0.4.0] - YYYY-MM-DD`) and in
   `io.github.mzilinski.merkzettel.metainfo.xml` (`<release ... date=...>`).
2. Make sure CI is green on `main`.
3. `git tag -s v0.4.0 -m "Merkzettel 0.4.0"` (without GPG: `git tag -a`)
4. `git push origin v0.4.0`
5. The *Release* workflow builds the tarball, checksum and a Flatpak bundle
   and publishes the GitHub release with the changelog excerpt. Check it under
   **Releases**.

## 7. AUR packages

1. Create an account on <https://aur.archlinux.org/register> and add your SSH
   public key in the account settings.
2. For each package (`merkzettel`, `merkzettel-git`):

   ```bash
   git clone ssh://aur@aur.archlinux.org/merkzettel.git aur-merkzettel
   cp packaging/aur/merkzettel/{PKGBUILD,.SRCINFO} aur-merkzettel/
   cd aur-merkzettel && makepkg -si && namcap PKGBUILD *.pkg.tar.zst
   git add PKGBUILD .SRCINFO && git commit -m "Initial import: 0.4.0" && git push
   ```

   `packaging/aur/README.md` describes updates for later releases.

## 8. Flathub submission

Requirements: steps 2, 3 and 6 are done (website online, client ID exists,
tag pushed). `flathub/README.md` contains the checklist; every item except
the pull request is already checked.

1. Fork <https://github.com/flathub/flathub> on GitHub (**uncheck** "copy the
   master branch only").
2. ```bash
   git clone --branch new-pr git@github.com:<you>/flathub.git flathub-submission
   cd flathub-submission && git checkout -b io.github.mzilinski.merkzettel
   python3 ../merkzettel/flathub/prepare.py --client-id <uuid> .
   flatpak run --command=flathub-build org.flatpak.Builder --install io.github.mzilinski.merkzettel.yml
   flatpak run --command=flatpak-builder-lint org.flatpak.Builder manifest io.github.mzilinski.merkzettel.yml
   flatpak run io.github.mzilinski.merkzettel     # sign in once, check tray and shortcut
   git add io.github.mzilinski.merkzettel.yml && git commit -m "Add io.github.mzilinski.merkzettel"
   git push origin io.github.mzilinski.merkzettel
   ```
3. Open a pull request **against the `new-pr` branch** of `flathub/flathub`,
   title `Add io.github.mzilinski.merkzettel`, and fill in the template.
4. Answer the reviewers. Typical questions: why `org.kde.kglobalaccel` (global
   shortcut, no portal exists), why `org.freedesktop.secrets` /
   `org.kde.kwalletd6` (refresh token in the keychain).
5. After the merge you get write access to
   `flathub/io.github.mzilinski.merkzettel`. Accept the invitation and enable
   two-factor authentication if asked. Then the Flathub and AUR buttons on the
   website lead to the published packages.

## 9. Donation link

The app (About dialog, sidebar) and the website link to
`https://paypal.me/eit31`. Open it once and confirm it is the account you want
to receive tips on. To change it, edit `src/Main.qml`,
`website/src/i18n.ts` (`DONATE`) and the `<url type="donation">` in the
metainfo.
