# Installation

Merkzettel runs on any Linux desktop with Qt 6 and KDE Frameworks 6; it fits
best into Plasma 6. Pick one of the following ways.

## Flathub (recommended)

The Flatpak works on every distribution and updates itself through your
software centre.

=== "Software centre"

    Search for **Merkzettel** in Discover (Plasma) or GNOME Software and click
    **Install**.

=== "Terminal"

    ```bash
    flatpak install flathub io.github.mzilinski.merkzettel
    flatpak run io.github.mzilinski.merkzettel
    ```

The Flatpak only gets the permissions it needs: network access, the tray icon,
the system keychain for the sign-in token and the global shortcut service.
Notifications, opening links and file dialogs go through the desktop portals.

## Arch Linux and derivatives (AUR)

```bash
# release version
paru -S merkzettel          # or: yay -S merkzettel
# development snapshot of the main branch
paru -S merkzettel-git
```

## Build from source

You need CMake ≥ 3.20, a C++17 compiler, Qt ≥ 6.6, KDE Frameworks ≥ 6.8,
Kirigami Addons and QtKeychain (Qt 6 build). At runtime Merkzettel also needs
the Qt SQLite driver and the Breeze Qt Quick style (`qqc2-desktop-style`).

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

Then build and install:

```bash
git clone https://github.com/mzilinski/merkzettel.git
cd merkzettel
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DMERKZETTEL_CLIENT_ID=<your-client-id>
cmake --build build
sudo cmake --install build      # or run ./build/bin/merkzettel directly
```

!!! note "Client ID"
    Builds from source need the client ID of an Azure app registration to
    sign in. Creating one takes about five minutes and is free; see
    [Azure setup](azure-setup.md). Without it the app builds and the demo
    works, but the **Sign in** button stays disabled.

### Flatpak from source

```bash
flatpak install flathub org.kde.Sdk//6.11 org.kde.Platform//6.11
sed "s/__MERKZETTEL_CLIENT_ID__/<your-client-id>/" \
  flatpak/io.github.mzilinski.merkzettel.yml > /tmp/merkzettel.yml
flatpak-builder --user --install --force-clean build-flatpak /tmp/merkzettel.yml
```

## Command-line options

| Option | Effect |
|---|---|
| `--tray`, `-t` | Start hidden in the system tray |
| `--demo` | Start with sample data, without an account |
| `--help` | Show all options |

## Uninstall

Remove the package with the tool you installed it with. Your local data stays
in `~/.local/share/merkzettel`, `~/.cache/merkzettel` and
`~/.config/merkzettelrc` (Flatpak: `~/.var/app/io.github.mzilinski.merkzettel`);
delete these to remove everything. The sign-in token is removed when you
choose **Sign out** in the app, or with your keychain manager (entry
`merkzettel` / `refresh_token`).
