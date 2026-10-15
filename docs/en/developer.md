# Developer guide

## Building

See [Installation → Build from source](installation.md#build-from-source) for
the dependencies. For development:

```bash
make CLIENT_ID=<your-client-id>   # configure + build into build/
make test                          # unit tests + QML smoke test (headless)
make run-demo                      # start with sample data
```

Useful CMake options:

| Option | Default | Purpose |
|---|---|---|
| `MERKZETTEL_CLIENT_ID` | empty | Azure client ID; empty disables sign-in |
| `MERKZETTEL_TENANT` | `common` | Tenant (`common`, `organizations`, `consumers`, ID) |
| `MERKZETTEL_SANITIZERS` | `OFF` | AddressSanitizer + UBSan |
| `MERKZETTEL_WERROR` | `OFF` | Treat compiler warnings as errors (CI) |
| `BUILD_TESTING` | `ON` | Build the test suite |

## Tests

| Test | Covers |
|---|---|
| `tst_graphclient` | Graph client against an in-process mock server: concurrency limit, 429/503 with Retry-After, bounded retries, paging, delta sync, expired delta tokens |
| `tst_dateparser` | Date words of the add bar (German and English) |
| `tst_database` | Cache round trip, delta, 2 000 tasks, My Day pruning, file permissions |
| `tst_tasksmodel` | Smart filters, sort modes, per-list sort memory, search, recurrence decoding, 5 000 tasks |
| `tst_appdemo` | Loads the real QML in demo mode, fails on QML warnings, drives core flows (label `qml`) |

Run a single test with `ctest --test-dir build -R tst_database --output-on-failure`.
`QT_QPA_PLATFORM=offscreen` is needed without a display.

## Quality checks

`make lint` runs everything CI checks besides the builds:

- `reuse lint` — licensing information for every file
- `appstreamcli validate --strict` and `desktop-file-validate`
- `scripts/check-version.py` — CMake, metainfo and CHANGELOG agree
- `scripts/update-translations.sh --check` — template current, German complete
- `scripts/check-accessibility.py` — every control has an accessible name
- `clang-format --dry-run --Werror`
- `scripts/static-analysis.sh` — clang-tidy, clazy, cppcheck

Install the git hooks with `pre-commit install`.

## Make targets

| Target | Result |
|---|---|
| `make` / `make test` / `make lint` | Build, test, check |
| `make pot` | Regenerate `po/merkzettel.pot`, merge translations |
| `make screenshots` | Screenshots for docs, README and metainfo (Xvfb + KWin) |
| `make docs` | This documentation into `website/public/docs` |
| `make videos` | How-to videos with narration into `website/public/videos` |
| `make site` | The website (includes docs) into `website/dist` |
| `make flatpak` | Flatpak from the working tree |

## Continuous integration

`.github/workflows/ci.yml` builds and tests on Arch (with `-Werror`), Fedora,
openSUSE Tumbleweed and Ubuntu 26.04 LTS, runs the static analysis and the
sanitizer tests and builds the Flatpak. `release.yml` runs on `v*` tags and
publishes the source tarball, the Flatpak bundle and checksums with the
CHANGELOG section as release notes. `pages.yml` publishes the website.

## Releasing

```bash
python3 scripts/release.py 0.5.0     # CHANGELOG, CMake version, metainfo release
git commit -am "Release 0.5.0"
git tag -s v0.5.0 -m "Merkzettel 0.5.0"
git push --follow-tags
```

Afterwards update the Flathub manifest (`flathub/`) and the AUR packages
(`packaging/aur/`) to the new tag.

## Code conventions

- English for code, comments, commits and documentation; UI strings through
  `i18n()`; German translation with "du".
- `clang-format` style from `.clang-format`; Qt keywords off
  (`Q_SIGNALS`, `Q_EMIT`, `Q_SLOTS`).
- Show data from the network with `Text.PlainText` or `app.plain()` in QML.
- Record design decisions in `DECISIONS.md`, user-visible changes in
  `CHANGELOG.md` under *Unreleased*.
