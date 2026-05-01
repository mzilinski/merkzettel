# Merkzettel

[![Build](https://github.com/mzilinski/merkzettel/actions/workflows/build.yml/badge.svg)](https://github.com/mzilinski/merkzettel/actions/workflows/build.yml)
[![License: GPL v3+](https://img.shields.io/badge/License-GPLv3%2B-blue.svg)](LICENSES/GPL-3.0-or-later.txt)

> **Native KDE/Kirigami client for Microsoft To Do.** Written in C++/Qt6, talks Microsoft Graph directly — no PWA, no web wrapper. Tray integration via KStatusNotifierItem, refresh tokens stored in KWallet, sub-second startup, sectioned task view (Overdue / Today / Tomorrow / This Week / Later / No date / Completed). English (source) and German (`po/de`) translations included.
>
> *Deutsche Beschreibung weiter unten.*

| English | Deutsch |
|:---:|:---:|
| ![English screenshot](screenshots/main-en.png) | ![Deutscher Screenshot](screenshots/main-de.png) |

> Try it without an Azure account: `merkzettel --demo` runs the UI with hard-coded demo lists and tasks (no sign-in, no network, all mutations no-ops). Both screenshots above were taken with `--demo`.

---

## Deutsch

Nativer KDE-Client fuer **Microsoft To Do**, gebaut mit Kirigami (Qt6/QML, C++).

Status: **v0.2** — laeuft, alltagstauglich. Kommuniziert direkt mit Microsoft Graph (kein Wrapper, keine PWA).

### Features

- OAuth2-Login via PKCE (Public Client, kein Secret im Binary)
- Refresh-Token in **KWallet** (via QtKeychain)
- Listen + Aufgaben aus Microsoft Graph
- Aufgaben anlegen, abhaken, loeschen, Wichtigkeit (Stern), Notizen
- Faelligkeit + Erinnerung mit DatePopup/TimePopup (kirigami-addons)
- **Sektionen** im Listenbild: Ueberfaellig / Heute / Morgen / Diese Woche / Spaeter / Ohne Datum / Erledigt
- Datum-Parsing in der Add-Bar: `Einkaufen morgen`, `Steuer mo`, `Termin 25.5.`
- Kontextmenue (Rechtsklick), Detail-Sheet (Klick), Stern-Toggle
- **StatusNotifierItem**-Tray-Icon mit Badge fuer faellige Aufgaben heute
- `--tray` Flag fuer minimierten Start (per Desktop-Action verfuegbar)
- Schliessen minimiert in den Tray
- SQLite-Cache fuer Offline-Start
- Plasma-Integration: Breeze, Akzentfarbe, Dark Mode, KDE-About-Dialog
- i18n via KLocalizedString — Englisch (Source) + Deutsch

### Abhaengigkeiten

Arch / CachyOS:

```bash
sudo pacman -S \
  qt6-base qt6-declarative qt6-networkauth \
  kirigami kirigami-addons kcoreaddons ki18n knotifications \
  kstatusnotifieritem kconfig kconfigwidgets \
  qtkeychain-qt6 extra-cmake-modules cmake gettext
```

Andere Distros: aequivalente KF6-Pakete (>= 6.0) + Qt6 (>= 6.6) + qtkeychain.

### Bauen

1. Azure App Registration einrichten (siehe `docs/azure-setup.md`).
2. Mit deiner Client-ID konfigurieren und bauen:

```bash
cmake -B build -S . \
  -DMERKZETTEL_CLIENT_ID=<deine-azure-client-id> \
  -DMERKZETTEL_TENANT=common \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

3. Direkt aus dem Build-Verzeichnis starten (Translations werden in-place gefunden):

```bash
./build/bin/merkzettel
```

4. Optional installieren:

```bash
cmake --install build --prefix ~/.local
```

### Kommandozeilen-Flags

| Flag | Wirkung |
|---|---|
| `--tray`, `-t` | Startet versteckt im System-Tray (Klick aufs Tray-Icon zeigt das Fenster) |
| `--demo` | Laeuft mit eingebauten Demo-Daten — keine Anmeldung, kein Netzwerk, keine Persistenz. Nuetzlich fuer Screenshots oder zum Ausprobieren ohne Azure-Setup |
| `--help` | Zeigt alle KAboutData-Optionen |

### Architektur

| Datei | Zweck |
|---|---|
| `src/main.cpp` | QApplication + KAboutData + QML-Engine |
| `src/app.{h,cpp}` | Top-Level Controller, an QML als `app` exponiert |
| `src/auth/authmanager.{h,cpp}` | PKCE OAuth2-Flow (QtNetworkAuth) |
| `src/auth/tokenstore.{h,cpp}` | Refresh-Token in KWallet (QtKeychain) |
| `src/graph/graphclient.{h,cpp}` | HTTP-Wrapper, 401→Refresh→Retry |
| `src/graph/todoapi.{h,cpp}` | To-Do-Endpunkte: lists/tasks CRUD |
| `src/cache/database.{h,cpp}` | SQLite-Cache |
| `src/models/*.{h,cpp}` | `QAbstractListModel` fuer QML |
| `src/tray/trayicon.{h,cpp}` | KStatusNotifierItem |
| `src/Main.qml` | Hauptfenster + GlobalDrawer + DatePopup/TimePopup |
| `src/LoginPage.qml` | Login-Seite |
| `src/TasksPage.qml` | Aufgaben-Seite mit Sektionen |
| `src/TaskDelegate.qml` | Listen-Delegate inkl. Stern + Kontextmenue |
| `src/TaskDetailSheet.qml` | Detail-Editor (Titel, Notiz, Datum, Erinnerung, Wichtigkeit) |
| `po/de/merkzettel.po` | Deutsche Uebersetzung |

### Roadmap

**v0.3 — geplant**
- Delta-Sync (`@odata.deltaLink`) statt Full-Refresh
- KNotifications fuer Erinnerungen (Reminder-Popups)
- KGlobalAccel "Quick Add" globaler Shortcut
- Subtasks (`checklistItems`)
- Mehrere Konten

**v0.4 — Ideen**
- Drag & Drop zwischen Listen
- Wiederholende Aufgaben (`recurrence`)
- Anhaenge + verknuepfte Ressourcen
- Plasmoid-Variante fuer den Panel-Direktzugriff

### Mitwirken / Bugs

Issues und PRs willkommen. Code-Konventionen:

- C++ + Kommentare in Englisch
- Commit-Messages und Doku in Deutsch (Umlaute als `ae`/`oe`/`ue` in Markdown)
- User-facing Strings in Quellcode immer **Englisch** + i18n() + Uebersetzung in `po/<lang>/merkzettel.po`

### Lizenz

**GPL-3.0-or-later** — siehe [LICENSE](LICENSE) und [LICENSES/GPL-3.0-or-later.txt](LICENSES/GPL-3.0-or-later.txt). Repo folgt [REUSE 3.0](https://reuse.software/).

### Spenden

Wenn dir Merkzettel nuetzt: <https://paypal.me/eit31> ❤
Der KDE/Qt-Unterbau wird von [KDE e.V.](https://kde.org/community/donations/) gepflegt.
