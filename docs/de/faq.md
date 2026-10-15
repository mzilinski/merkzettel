# FAQ

## Ist Merkzettel von Microsoft?

Nein. Merkzettel ist ein unabhängiges Open-Source-Projekt. Es nutzt die
öffentliche Schnittstelle von Microsoft, Microsoft Graph, genauso wie andere
Drittanbieter-Apps.

## Welche Konten funktionieren?

Private Microsoft-Konten (outlook.com, hotmail.com, live.com …) und Konten von
Arbeit oder Schule in Microsoft 365. In manchen Organisationen muss ein
Administrator die App zuerst freigeben; siehe
[Anleitung: Dienstkonto](howto/work-account.md).

## Warum hat Merkzettel die Funktion X aus Microsoft To Do nicht?

Merkzettel kann nur, was Microsoft Graph Drittanbieter-Apps anbietet. Diese
To-Do-Funktionen sind dort **nicht** verfügbar:

| Funktion | Stand in Merkzettel |
|---|---|
| **Listen teilen**, Personen einladen, Mitglieder entfernen | Nicht möglich; **Teilen ...** öffnet die To-Do-Web-App. Geteilte Listen selbst funktionieren normal. |
| **Aufgaben zuweisen** an Personen | Nicht verfügbar |
| **Listengruppen** (Ordner) | Nicht verfügbar |
| Synchronisierung von **Mein Tag** | Nur lokal, siehe unten |
| **Designs und Hintergründe** von Listen | Nicht verfügbar |
| **Farben von Kategorien** | Graph liefert nur die Namen; die Farben gehören zu deinen Outlook-Einstellungen |
| **Manuelle Reihenfolge per Ziehen** | Graph gibt die Reihenfolge nicht heraus |

### Mein Tag {#my-day}

Microsoft To Do synchronisiert *Mein Tag* über einen internen Microsoft-Dienst
(nicht Microsoft Graph), den nur Microsofts eigene Apps nutzen dürfen.
Merkzettel speichert Mein Tag deshalb auf deinem Computer: Es funktioniert
genauso und beginnt jeden Tag leer, aber auf deinem Handy siehst du nicht
dasselbe Mein Tag.

### Aufgaben verschieben

Graph kennt keine Operation „In Liste verschieben“. Merkzettel kopiert die
Aufgabe – Titel, Notizen, Daten, Erinnerung, Wiederholung, Kategorien,
Schritte, Verknüpfungen und Anhänge – in die Zielliste und löscht danach das
Original. Die Kopie hat ein neues Erstellungsdatum, und im Verlauf der Aufgabe
in Microsofts Überwachungsprotokollen (falls deine Organisation sie nutzt)
erscheint eine neue Aufgabe.

## Wie funktioniert die Synchronisierung?

Merkzettel hält eine lokale Kopie und fragt bei Microsoft nur die Änderungen
seit der letzten Synchronisierung ab (*Delta-Synchronisierung*). Er
synchronisiert, wenn du die Liste wechselst, <kbd>F5</kbd> drückst und nach
deinen eigenen Änderungen. Bearbeitest du dieselbe Aufgabe auf einem anderen
Gerät, gewinnt das zuletzt geänderte Feld; verschiedene Felder überschreiben
sich nicht gegenseitig.

## Funktioniert Merkzettel offline?

Er startet und zeigt deine Listen auch ohne Verbindung. Änderungen brauchen
eine Verbindung und werden nicht für später gespeichert; Merkzettel sagt dir
das und verbindet sich automatisch neu, sobald das Netz zurück ist.

## Warum gibt es nach einem Update mehr Anfragen als erwartet?

Fügt eine neue Version dem lokalen Cache Daten hinzu (zum Beispiel
Kategorien), lädt Merkzettel einmal alle Listen herunter. Microsoft bremst ihn
dann eventuell kurz aus; Merkzettel wartet und macht von selbst weiter.

## Wo liegen meine Daten? Wird etwas anderswohin gesendet?

Siehe [Datenschutz](privacy.md). Kurz gesagt: nur an Microsoft, an niemanden
sonst, keine Telemetrie.

## Wie kann ich das Projekt unterstützen?

Melde Fehler, übersetze, steuere Code bei oder
[spende](https://paypal.me/eit31). Siehe
[Mitmachen](https://github.com/mzilinski/merkzettel/blob/main/CONTRIBUTING.md).
