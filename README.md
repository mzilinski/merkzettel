# Merkzettel

Nativer KDE-Client fuer **Microsoft To Do**, gebaut mit Kirigami (Qt6/QML, C++).

Status: **v0.1 — fruehe Alpha.** Laeuft, aber Featureset ist minimal: Login, Listen anzeigen, Aufgaben anlegen / abhaken / loeschen, Tray-Icon mit Faelligkeits-Badge, SQLite-Cache.

## Features

- OAuth2 Login via PKCE (Public Client, kein Secret im Binary)
- Refresh-Token in **KWallet** (via QtKeychain)
- Listet alle To-Do-Listen + Aufgaben
- Aufgaben hinzufuegen, abhaken, loeschen
- **StatusNotifierItem**-Tray-Icon mit Badge fuer faellige Aufgaben heute
- Schliessen minimiert in den Tray
- SQLite-Cache fuer Offline-Start
- Komplette Plasma-Integration (Breeze-Look, Akzentfarbe, Dark Mode)

## Abhaengigkeiten (Arch / CachyOS)

```bash
sudo pacman -S \
  qt6-base qt6-declarative qt6-networkauth \
  kirigami kcoreaddons ki18n knotifications \
  kstatusnotifieritem kconfig kconfigwidgets \
  qtkeychain-qt6 extra-cmake-modules cmake
```

## Bauen

1. Azure App Registration einrichten (siehe `docs/azure-setup.md`).
2. Konfigurieren mit deiner Client-ID:

```bash
cd ~/Developer/merkzettel
cmake -B build -S . \
  -DMERKZETTEL_CLIENT_ID=<deine-azure-client-id> \
  -DMERKZETTEL_TENANT=common \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

3. Direkt aus dem Build-Verzeichnis starten:

```bash
./build/bin/merkzettel
```

4. Optional installieren:

```bash
sudo cmake --install build
```

## Architektur (kurz)

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
| `src/Main.qml` | Hauptfenster + GlobalDrawer |
| `src/pages/LoginPage.qml` | Login-Seite |
| `src/pages/TasksPage.qml` | Aufgaben-Seite |
| `src/components/TaskDelegate.qml` | Listen-Delegate fuer Aufgaben |

## Roadmap

**v0.2**
- Delta-Sync (`@odata.deltaLink`) statt Full-Refresh
- Faelligkeitsdatum + Erinnerungen setzen
- KNotifications fuer Erinnerungen
- KGlobalAccel "Quick Add" Shortcut
- `--minimized`-Autostart

**v0.3**
- Subtasks (`checklistItems`)
- Mehrere Konten
- Ordnen / Drag&Drop von Listen
- Wichtigkeit / Sterne

## Lizenz

GPL-3.0-or-later
