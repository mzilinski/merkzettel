# Fehlerbehebung

Starte Merkzettel aus einem Terminal (`merkzettel`, Flatpak:
`flatpak run io.github.mzilinski.merkzettel`), um seine Meldungen zu sehen.

## Anmeldung

**Der Browser öffnet sich nicht.**
Merkzettel bittet den Desktop, die Anmeldeseite zu öffnen. Prüfe, ob ein
Standard-Webbrowser eingestellt ist (*Systemeinstellungen →
Standardanwendungen*). Im Flatpak läuft das über das Desktop-Portal
(`xdg-desktop-portal` muss laufen).

**„Port 53682 in use“.**
Das Ergebnis der Anmeldung wird an `http://localhost:53682/callback`
geliefert. Schließe andere Programme, die diesen Port belegen (oder einen
zweiten Anmeldeversuch), und versuche es erneut.

**„Need admin approval“ / „AADSTS65001“.**
Deine Organisation verlangt, dass ein Administrator die App freigibt. Siehe
[Anleitung: Dienstkonto](howto/work-account.md).

**„Diesem Build fehlt eine Microsoft-App-Registrierung“.**
Die App wurde ohne Client-ID gebaut. Installiere sie von Flathub oder aus dem
AUR, oder baue sie mit deiner eigenen Registrierung
([Azure-Einrichtung](azure-setup.md)).

**Unerwartet abgemeldet.**
Microsoft hat die Anmeldung widerrufen (Passwortänderung, entzogene
Zustimmung, lange Inaktivität). Melde dich erneut an; deine Listen sind noch
im Cache.

## Schlüsselbund (KWallet / Secret Service)

Merkzettel speichert das Anmelde-Token im Schlüsselbund des Systems.

- **Nach jedem Start wieder anmelden:** Der Schlüsselbund ist gesperrt oder
  deaktiviert. Öffne unter Plasma *Systemeinstellungen → KDE-Passwortspeicher*
  und aktiviere den Passwortspeicher, oder sorge dafür, dass ein
  Secret-Service-Anbieter (KWallet 6, GNOME Keyring) läuft.
- **Beim Login erscheint eine Passwortabfrage für den Passwortspeicher:**
  Setze das Passwort des Passwortspeichers auf dein Login-Passwort, dann wird
  er automatisch entsperrt (PAM).
- **Token von Hand entfernen:** Öffne *KWalletManager* (oder *Seahorse*) und
  lösche den Eintrag `merkzettel` / `refresh_token`.

## Symbol im Systemabschnitt

- **Kein Symbol unter Plasma:** Prüfe, ob *Systemabschnitt* unter *Einträge*
  *Merkzettel* anzeigt (es kann ausgeblendet sein).
- **GNOME:** Installiere die Erweiterung *AppIndicator and
  KStatusNotifierItem Support*; GNOME hat standardmäßig keinen
  Systemabschnitt. Ohne ihn versteckt das Schließen des Fensters Merkzettel –
  starte ihn dann erneut aus dem Anwendungsmenü.
- **Wayland:** Der Systemabschnitt funktioniert unter Plasma Wayland. Holt ein
  Klick auf das Symbol das Fenster nicht nach vorn, nutze den globalen
  Kurzbefehl <kbd>Meta</kbd>+<kbd>Alt</kbd>+<kbd>N</kbd>.

## Benachrichtigungen

Keine Erinnerungen? Merkzettel muss laufen (Systemabschnitt). Prüfe
*Systemeinstellungen → Benachrichtigungen → Anwendungen → Merkzettel* und den
Modus „Nicht stören“. Das Flatpak sendet Benachrichtigungen über das Portal.

## Globaler Kurzbefehl reagiert nicht

Der Kurzbefehl wird bei KGlobalAccel registriert. Außerhalb von Plasma fehlt
dieser Dienst eventuell. Prüfe unter Plasma *Systemeinstellungen → Tastatur →
Kurzbefehle → Merkzettel* auf Konflikte. Auf anderen Desktops legst du in den
Tastatureinstellungen eine Taste auf `merkzettel --quick-add`.

## Flatpak

- **Dunkles Design wird nicht übernommen:** Das Flatpak liest
  `~/.config/kdeglobals`; sorge dafür, dass das Theme der KDE-Runtime passt
  (`flatpak update`).
- **Anhänge aus anderen Ordnern:** Dateien werden über das Portal zur
  Dateiauswahl geöffnet und gespeichert; Ziehen und Ablegen funktioniert aus
  Dolphin.

## Synchronisierung

- **„Keine Verbindung zu Microsoft“:** Du bist offline, oder ein Proxy
  blockiert `graph.microsoft.com` / `login.microsoftonline.com`.
- **Änderungen vom Handy erscheinen spät:** Drücke <kbd>F5</kbd>. Microsoft
  braucht eventuell ein paar Sekunden, bis Änderungen für andere Apps sichtbar
  sind.
- **Neu anfangen:** Beende Merkzettel und lösche `~/.local/share/merkzettel/`;
  alles außer Mein Tag wird neu heruntergeladen.

## Fehler melden

Eröffne ein Issue auf
[GitHub](https://github.com/mzilinski/merkzettel/issues/new/choose) mit der
Version (*Über Merkzettel*), der Installationsart, deiner Distribution und der
Terminalausgabe. Entferne Aufgabentitel oder E-Mail-Adressen, die du nicht
veröffentlichen willst.
