# Entwicklerhandbuch

## Bauen

Die Abhängigkeiten findest du unter
[Installation → Aus dem Quellcode bauen](installation.md#build-from-source).
Für die Entwicklung:

```bash
make CLIENT_ID=<your-client-id>   # configure + build into build/
make test                          # unit tests + QML smoke test (headless)
make run-demo                      # start with sample data
```

Nützliche CMake-Optionen:

| Option | Standard | Zweck |
|---|---|---|
| `MERKZETTEL_CLIENT_ID` | leer | Azure-Client-ID; leer deaktiviert die Anmeldung |
| `MERKZETTEL_TENANT` | `common` | Mandant (`common`, `organizations`, `consumers`, ID) |
| `MERKZETTEL_SANITIZERS` | `OFF` | AddressSanitizer + UBSan |
| `MERKZETTEL_WERROR` | `OFF` | Compiler-Warnungen als Fehler behandeln (CI) |
| `BUILD_TESTING` | `ON` | Testsuite bauen |

## Tests

| Test | Deckt ab |
|---|---|
| `tst_graphclient` | Graph-Client gegen einen Mock-Server im Prozess: Begrenzung paralleler Anfragen, 429/503 mit Retry-After, begrenzte Wiederholungen, Paginierung, Delta-Synchronisierung, abgelaufene Delta-Tokens |
| `tst_dateparser` | Datumswörter der Eingabezeile (Deutsch und Englisch) |
| `tst_database` | Cache-Hin-und-Rückweg, Delta, 2 000 Aufgaben, Aufräumen von Mein Tag, Dateirechte |
| `tst_tasksmodel` | Smart-Filter, Sortierarten, Sortierung pro Liste, Suche, Dekodierung von Wiederholungen, 5 000 Aufgaben |
| `tst_appdemo` | Lädt die echte QML im Demo-Modus, scheitert bei QML-Warnungen, spielt die Kernabläufe durch (Label `qml`) |

Einen einzelnen Test führst du mit
`ctest --test-dir build -R tst_database --output-on-failure` aus. Ohne Anzeige
ist `QT_QPA_PLATFORM=offscreen` nötig.

## Qualitätsprüfungen

`make lint` führt alles aus, was die CI neben den Builds prüft:

- `reuse lint` – Lizenzangaben für jede Datei
- `appstreamcli validate --strict` und `desktop-file-validate`
- `scripts/check-version.py` – CMake, Metainfo und CHANGELOG stimmen überein
- `scripts/update-translations.sh --check` – Vorlage aktuell, Deutsch vollständig
- `scripts/check-accessibility.py` – jedes Bedienelement hat einen zugänglichen Namen
- `clang-format --dry-run --Werror`
- `scripts/static-analysis.sh` – clang-tidy, clazy, cppcheck

Die Git-Hooks installierst du mit `pre-commit install`.

## Make-Ziele

| Ziel | Ergebnis |
|---|---|
| `make` / `make test` / `make lint` | Bauen, testen, prüfen |
| `make pot` | `po/merkzettel.pot` neu erzeugen, Übersetzungen abgleichen |
| `make screenshots` | Screenshots für Doku, README und Metainfo (Xvfb + KWin) |
| `make docs` | Diese Dokumentation nach `website/public/docs` |
| `make videos` | Anleitungsvideos mit Sprecher nach `website/public/videos` |
| `make site` | Die Website (inklusive Doku) nach `website/dist` |
| `make flatpak` | Flatpak aus dem Arbeitsverzeichnis |

## Continuous Integration

`.github/workflows/ci.yml` baut und testet auf Arch (mit `-Werror`), Fedora,
openSUSE Tumbleweed und Ubuntu 26.04 LTS, führt die statische Analyse und die
Sanitizer-Tests aus und baut das Flatpak. `release.yml` läuft bei `v*`-Tags und
veröffentlicht Quell-Tarball, Flatpak-Bundle und Prüfsummen mit dem
CHANGELOG-Abschnitt als Release-Notes. `pages.yml` veröffentlicht die Website.

## Release erstellen

```bash
python3 scripts/release.py 0.5.0     # CHANGELOG, CMake version, metainfo release
git commit -am "Release 0.5.0"
git tag -s v0.5.0 -m "Merkzettel 0.5.0"
git push --follow-tags
```

Danach das Flathub-Manifest (`flathub/`) und die AUR-Pakete
(`packaging/aur/`) auf den neuen Tag aktualisieren.

## Code-Konventionen

- Englisch für Code, Kommentare, Commits und Dokumentation; UI-Texte über
  `i18n()`; deutsche Übersetzung mit „du“.
- `clang-format`-Stil aus `.clang-format`; Qt-Schlüsselwörter aus
  (`Q_SIGNALS`, `Q_EMIT`, `Q_SLOTS`).
- Daten aus dem Netz in QML mit `Text.PlainText` oder `app.plain()` anzeigen.
- Designentscheidungen in `DECISIONS.md` festhalten, sichtbare Änderungen in
  `CHANGELOG.md` unter *Unreleased*.
