# Konfiguration

Merkzettel hat keinen Einstellungsdialog; alles wird dort geändert, wo du es
benutzt, und automatisch gespeichert.

## Erscheinungsbild

**Erscheinungsbild** im Menü der Seitenleiste wechselt zwischen
**Systemeinstellung folgen**, **Hell** und **Dunkel**. *Systemeinstellung
folgen* übernimmt dein Plasma-Farbschema samt Akzentfarbe.

## Sortierung und erledigte Aufgaben

- Der Knopf **Sortieren** in der Werkzeugleiste sortiert die aktuelle Liste
  nach Wichtigkeit, Fälligkeitsdatum, Titel oder Erstellungsdatum. Die Wahl
  wird **pro Liste** gespeichert; die Leiste unter dem Suchfeld zeigt die
  aktive Reihenfolge, kehrt sie um (Pfeil) oder entfernt sie (×).
- **Erledigte Aufgaben anzeigen** im ⋮-Menü blendet erledigte Aufgaben in
  allen Listen aus oder ein. Die Smart-Liste *Erledigt* zeigt sie immer.

## Systemabschnitt und Autostart

- Schließt du das Fenster, läuft Merkzettel im Systemabschnitt weiter; das
  Symbol dort zeigt, wie viele Aufgaben heute fällig sind. **Beenden** im Menü
  des Symbols beendet ihn.
- Damit Merkzettel mit deiner Sitzung startet, öffne **Systemeinstellungen →
  Autostart → Hinzufügen → Anwendung hinzufügen**, wähle Merkzettel und hänge
  bei Bedarf `--tray` an, um ihn versteckt zu starten. Das Anwendungsmenü
  bietet außerdem **Im Systemabschnitt starten**.

## Globaler Kurzbefehl

Der Kurzbefehl zum schnellen Erfassen, **Meta+Alt+N**, holt Merkzettel aus
jeder Anwendung nach vorn, mit dem Cursor im Feld für neue Aufgaben. Ändern
kannst du ihn unter **Systemeinstellungen → Tastatur → Kurzbefehle →
Merkzettel**. Dasselbe bietet das Menü des Systemabschnitts als
**Neue Aufgabe …**. Auf Desktops ohne KDEs Dienst für globale Kurzbefehle
(GNOME, Sway, …) legst du eine beliebige Taste auf den Befehl
`merkzettel --quick-add`.

## Benachrichtigungen

Erinnerungen erscheinen als Desktop-Benachrichtigungen mit der Aktion
**Erledigt**. Ton, Position und Verhalten bei „Nicht stören“ stellst du unter
**Systemeinstellungen → Benachrichtigungen → Anwendungen → Merkzettel** ein.

## Dateien und Speicherorte

| Was | Wo (Flatpak: unter `~/.var/app/io.github.mzilinski.merkzettel/`) |
|---|---|
| Ansichtsoptionen (Sortierung, Farbschema, Hinweise) | `~/.config/merkzettelrc` |
| Lokale Kopie der Listen und Aufgaben, Mein Tag | `~/.local/share/merkzettel/cache.sqlite` |
| Geöffnete Anhänge | `~/.cache/merkzettel/attachments/` |
| Anmelde-Token | Schlüsselbund des Systems (KWallet / Secret Service), Eintrag `merkzettel` |

Den Cache zu löschen ist unbedenklich: Merkzettel lädt beim nächsten Start
alles neu herunter. Nur **Mein Tag** geht verloren, weil Microsoft diese
Funktion anderen Apps nicht anbietet (siehe [FAQ](faq.md#my-day)).
