# Installation

Merkzettel läuft auf jedem Linux-Desktop mit Qt 6 und KDE Frameworks 6; am
besten passt er zu Plasma 6. Wähle einen der folgenden Wege.

## Flathub (empfohlen)

Das Flatpak funktioniert auf jeder Distribution und aktualisiert sich über
deine Softwareverwaltung.

=== "Softwareverwaltung"

    Suche in Discover (Plasma) oder GNOME Software nach **Merkzettel** und
    klicke auf **Installieren**.

=== "Terminal"

    ```bash
    flatpak install flathub io.github.mzilinski.merkzettel
    flatpak run io.github.mzilinski.merkzettel
    ```

Das Flatpak erhält nur die Berechtigungen, die es braucht: Netzwerkzugriff,
das Symbol im Systemabschnitt, den Schlüsselbund des Systems für das
Anmelde-Token und den Dienst für globale Kurzbefehle. Benachrichtigungen, das
Öffnen von Links und Dateidialoge laufen über die Desktop-Portale.

## Arch Linux und Ableger (AUR)

```bash
# release version
paru -S merkzettel          # or: yay -S merkzettel
# development snapshot of the main branch
paru -S merkzettel-git
```

## Aus dem Quellcode bauen {#build-from-source}

Du brauchst CMake ≥ 3.20, einen C++17-Compiler, Qt ≥ 6.6, KDE Frameworks ≥ 6.8,
Kirigami Addons und QtKeychain (Qt-6-Build). Zur Laufzeit braucht Merkzettel
außerdem den SQLite-Treiber für Qt und den Breeze-Stil für Qt Quick
(`qqc2-desktop-style`).

=== "Arch"

    ```bash
    sudo pacman -S --needed base-devel cmake ninja git extra-cmake-modules gettext \
      qt6-base qt6-declarative qt6-networkauth qtkeychain-qt6 \
      kirigami kirigami-addons qqc2-desktop-style kcoreaddons kdbusaddons \
      kglobalaccel ki18n knotifications kstatusnotifieritem kconfig \
      kconfigwidgets kcolorscheme
    ```

=== "Fedora"

    ```bash
    sudo dnf install gcc-c++ cmake ninja-build git extra-cmake-modules gettext \
      qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtnetworkauth-devel \
      qtkeychain-qt6-devel kf6-kirigami-devel kf6-kirigami-addons-devel \
      kf6-qqc2-desktop-style kf6-kcoreaddons-devel kf6-kdbusaddons-devel \
      kf6-kglobalaccel-devel kf6-ki18n-devel kf6-knotifications-devel \
      kf6-kstatusnotifieritem-devel kf6-kconfig-devel kf6-kconfigwidgets-devel \
      kf6-kcolorscheme-devel
    ```

=== "openSUSE Tumbleweed"

    ```bash
    sudo zypper install gcc-c++ cmake ninja git kf6-extra-cmake-modules gettext-tools \
      qt6-base-devel qt6-declarative-devel qt6-networkauth-devel qt6-sql-sqlite \
      qtkeychain-qt6-devel kf6-kirigami-devel kirigami-addons6-devel \
      kf6-qqc2-desktop-style kf6-kcoreaddons-devel kf6-kdbusaddons-devel \
      kf6-kglobalaccel-devel kf6-ki18n-devel kf6-knotifications-devel \
      kf6-kstatusnotifieritem-devel kf6-kconfig-devel kf6-kconfigwidgets-devel \
      kf6-kcolorscheme-devel
    ```

=== "Ubuntu 26.04 LTS"

    ```bash
    sudo apt install build-essential cmake ninja-build git extra-cmake-modules gettext \
      qt6-base-dev qt6-declarative-dev qt6-networkauth-dev libqt6sql6-sqlite \
      qtkeychain-qt6-dev libkirigami-dev kirigami-addons-dev qml6-module-org-kde-desktop \
      libkf6coreaddons-dev libkf6dbusaddons-dev libkf6globalaccel-dev libkf6i18n-dev \
      libkf6notifications-dev libkf6statusnotifieritem-dev libkf6config-dev \
      libkf6configwidgets-dev libkf6colorscheme-dev
    ```

Anschließend bauen und installieren:

```bash
git clone https://github.com/mzilinski/merkzettel.git
cd merkzettel
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DMERKZETTEL_CLIENT_ID=<your-client-id>
cmake --build build
sudo cmake --install build      # or run ./build/bin/merkzettel directly
```

!!! note "Client-ID"
    Selbst gebaute Versionen brauchen zum Anmelden die Client-ID einer
    Azure-App-Registrierung. Sie anzulegen dauert etwa fünf Minuten und ist
    kostenlos, siehe [Azure-Einrichtung](azure-setup.md). Ohne sie baut die App
    und die Demo funktioniert, aber der Knopf **Anmelden** bleibt deaktiviert.

### Flatpak aus dem Quellcode

```bash
flatpak install flathub org.kde.Sdk//6.11 org.kde.Platform//6.11
sed "s/__MERKZETTEL_CLIENT_ID__/<your-client-id>/" \
  flatpak/io.github.mzilinski.merkzettel.yml > /tmp/merkzettel.yml
flatpak-builder --user --install --force-clean build-flatpak /tmp/merkzettel.yml
```

## Kommandozeilenoptionen

| Option | Wirkung |
|---|---|
| `--tray`, `-t` | Versteckt im Systemabschnitt starten |
| `--quick-add` | Fenster mit dem Cursor im Feld für neue Aufgaben anzeigen; funktioniert auch bei bereits laufender Instanz |
| `--demo` | Mit Beispieldaten starten, ohne Konto |
| `--help` | Alle Optionen anzeigen |

## Deinstallieren

Entferne das Paket mit dem Werkzeug, mit dem du es installiert hast. Deine
lokalen Daten bleiben in `~/.local/share/merkzettel`, `~/.cache/merkzettel` und
`~/.config/merkzettelrc` (Flatpak: `~/.var/app/io.github.mzilinski.merkzettel`);
lösche sie, um alles zu entfernen. Das Anmelde-Token wird entfernt, wenn du in
der App **Abmelden** wählst, oder über die Verwaltung deines Schlüsselbunds
(Eintrag `merkzettel` / `refresh_token`).
