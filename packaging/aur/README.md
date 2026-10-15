# AUR packages

- `merkzettel/` — release package, builds the signed tag `v$pkgver`.
- `merkzettel-git/` — development snapshot of `main`.

Both are `namcap`-clean apart from two expected notes: `kirigami-addons` and
`qqc2-desktop-style` are QML runtime imports that namcap's ELF scan cannot see,
but the app does not start without them.

## Client ID

Set `_clientid` to the client ID of the public-release Azure app registration
(see `HUMAN_TODO.md`). Without it the package builds and the demo works, but
sign-in is disabled. Anyone can build with their own registration:

```bash
MERKZETTEL_CLIENT_ID=<your-client-id> makepkg -si
```

## Publishing

```bash
git clone ssh://aur@aur.archlinux.org/merkzettel.git aur-merkzettel
cp packaging/aur/merkzettel/{PKGBUILD,.SRCINFO} aur-merkzettel/
cd aur-merkzettel && git add PKGBUILD .SRCINFO && git commit -m "Update to 0.4.0" && git push
```

After a release: bump `pkgver`, reset `pkgrel=1`, regenerate `.SRCINFO` with
`makepkg --printsrcinfo > .SRCINFO`.
