# Benutzerhandbuch

![Merkzettel mit Listen links und Aufgaben rechts](../assets/screenshots/de/tasks-light.png#only-light){ .screenshot }
![Merkzettel mit Listen links und Aufgaben rechts](../assets/screenshots/de/tasks-dark.png#only-dark){ .screenshot }

Das Fenster besteht aus zwei Teilen: der **Seitenleiste** mit Smart-Listen und
deinen Listen und der **Aufgabenliste** der gewählten Liste. In schmalen
Fenstern klappt die Seitenleiste in den Knopf ☰ ein, und der Seitentitel zeigt
die aktuelle Liste.

## Anmelden

Beim ersten Start zeigt Merkzettel die Anmeldeseite. **Anmelden** öffnet deinen
Browser mit der Anmeldung von Microsoft; nachdem du zugestimmt hast, lädt
Merkzettel deine Listen. Die Seite zeigt genau, welche Berechtigungen
angefragt werden und wofür. **Demo ausprobieren** zeigt stattdessen
Beispieldaten; **Demo beenden** in der Seitenleiste führt zurück zur
Anmeldeseite.

Du bleibst angemeldet, bis du **Abmelden** wählst. Widerruft Microsoft die
Anmeldung (Passwortänderung, entzogene Zustimmung, lange Inaktivität), sagt
Merkzettel dir das und zeigt wieder die Anmeldeseite.

## Listen

- **Anlegen:** **Neue Liste ...** im Menü der Seitenleiste
  (<kbd>Strg</kbd>+<kbd>L</kbd>).
- **Umbenennen / löschen:** Rechtsklick auf eine Liste. Die Standardliste
  *Aufgaben* lässt sich weder umbenennen noch löschen.
- **Geteilte Listen** tragen ein Teilen-Symbol. Merkzettel zeigt und bearbeitet
  sie wie deine eigenen Listen; **Teilen ...** öffnet die Web-App von
  Microsoft To Do, weil Microsoft anderen Apps die Verwaltung der Freigaben
  nicht erlaubt.

## Smart-Listen

| Smart-Liste | Zeigt |
|---|---|
| **Mein Tag** | Aufgaben, die du für heute ausgewählt hast (auf diesem Gerät gespeichert, beginnt jeden Tag leer) |
| **Alle** | Jede Aufgabe aus jeder Liste |
| **Wichtig** | Aufgaben mit Stern |
| **Geplant** | Aufgaben mit Fälligkeitsdatum oder Erinnerung |
| **Erledigt** | Erledigte Aufgaben |

Aufgaben, die du hinzufügst, während eine Smart-Liste gewählt ist, landen in
deiner Standardliste und bekommen die passende Eigenschaft – einen Stern in
*Wichtig*, Fälligkeit heute in *Geplant*, einen Platz in *Mein Tag* in
*Mein Tag*.

## Aufgaben hinzufügen

Tippe in **Neue Aufgabe ...** und drücke <kbd>Eingabe</kbd>. Ein Datumswort am
Ende setzt das Fälligkeitsdatum:

| Du tippst | Fällig |
|---|---|
| `Milch kaufen heute` / `today` | heute |
| `Anna anrufen morgen` / `tomorrow` | morgen |
| `Zahnarzt übermorgen` | übermorgen |
| `Miete zahlen mo` / `montag` / `mon` / `monday` | am nächsten Montag (nie heute) |
| `Arzt 25.5.` oder `25.5.2027` | an diesem Datum |

![Aufgabe mit Datumswort tippen](../assets/screenshots/de/quickadd-light.png#only-light){ .screenshot }
![Aufgabe mit Datumswort tippen](../assets/screenshots/de/quickadd-dark.png#only-dark){ .screenshot }

## Abschnitte, Sortierung und Suche

Ohne ausdrückliche Sortierung werden Aufgaben in **Überfällig**, **Heute**,
**Morgen**, **Diese Woche**, **Später**, **Ohne Datum** und **Erledigt**
gruppiert. Der Knopf **Sortieren** ordnet die Liste stattdessen nach
Wichtigkeit, Fälligkeitsdatum, Titel oder Erstellungsdatum; jede Liste merkt
sich ihre eigene Reihenfolge. Das Suchfeld filtert nach Titel, Notizen und
Kategorie.

![Alphabetisch sortierte Liste](../assets/screenshots/de/sorted-light.png#only-light){ .screenshot }
![Alphabetisch sortierte Liste](../assets/screenshots/de/sorted-dark.png#only-dark){ .screenshot }

## Aufgabendetails

Klicke auf eine Aufgabe (oder wähle sie aus und drücke <kbd>Eingabe</kbd>), um
ihre Details zu öffnen:

- **Titel**, **Wichtigkeit** (Stern) und **Mein Tag**
- **Fälligkeit** mit *Heute*, *Morgen* oder einer Datumsauswahl
- **Wiederholung:** täglich, werktags, wöchentlich, monatlich, jährlich oder
  **Benutzerdefiniert ...** – alle *N* Tage, Wochen, Monate oder Jahre, bei
  Wochen an ausgewählten Wochentagen. Wiederholungen richten sich nach dem
  Fälligkeitsdatum.
- **Kategorien:** Tippe einen Namen ein oder wähle einen, der an anderen
  Aufgaben vorkommt
- **Erinnerung** mit Datum und Uhrzeit; du bekommst eine
  Desktop-Benachrichtigung
- **Unteraufgaben** (Schritte) mit Ankreuzfeldern
- **Verknüpfungen** zu Webseiten oder Dateien
- **Dateien:** mit **Datei hinzufügen ...** anhängen oder Dateien auf den
  Bereich ziehen (bis 25 MB pro Datei); öffnen, speichern oder löschen
- **Notizen** sowie wann die Aufgabe erstellt und zuletzt geändert wurde

![Aufgabendetails](../assets/screenshots/de/details-light.png#only-light){ .screenshot }
![Aufgabendetails](../assets/screenshots/de/details-dark.png#only-dark){ .screenshot }

## Erledigen, löschen und rückgängig machen

Hake das Kästchen an, um eine Aufgabe zu erledigen; hakst du es wieder ab,
ist sie wieder offen. **Löschen** (im Kontextmenü, über den Knopf beim
Überfahren mit der Maus oder mit <kbd>Entf</kbd>) entfernt eine Aufgabe nach
einigen Sekunden – bis dahin holt **Rückgängig** in der Benachrichtigung oder
<kbd>Strg</kbd>+<kbd>Z</kbd> sie zurück.

## Mehrere Aufgaben auf einmal

<kbd>Strg</kbd>+Klick fügt eine Aufgabe zur Auswahl hinzu,
<kbd>Umschalt</kbd>+Klick wählt einen Bereich, <kbd>Leertaste</kbd> schaltet
die aktuelle Aufgabe um und <kbd>Strg</kbd>+<kbd>A</kbd> wählt alle aus. Die
Auswahlleiste erledigt dann alle ausgewählten Aufgaben, markiert sie mit
Stern, fügt sie zu Mein Tag hinzu, verschiebt oder löscht sie.

![Zwei ausgewählte Aufgaben](../assets/screenshots/de/selection-light.png#only-light){ .screenshot }
![Zwei ausgewählte Aufgaben](../assets/screenshots/de/selection-dark.png#only-dark){ .screenshot }

## Aufgaben zwischen Listen verschieben

Rechtsklick auf eine Aufgabe → **Verschieben nach**, oder über die
Auswahlleiste. Microsoft Graph kennt kein „Verschieben“, deshalb kopiert
Merkzettel die Aufgabe mit Schritten, Verknüpfungen und Dateien in die
Zielliste und löscht danach das Original. Die Kopie bekommt ein neues
Erstellungsdatum. Geht unterwegs etwas schief, bleibt das Original erhalten.

## Mein Tag

**Mein Tag** ist dein Plan für heute: Füge Aufgaben mit
<kbd>Strg</kbd>+<kbd>T</kbd>, über das Kontextmenü, die Auswahlleiste oder den
Schalter in den Details hinzu. Aufgaben aus Mein Tag tragen in allen anderen
Listen eine Sonne. Mein Tag beginnt jeden Morgen leer.

![Mein Tag](../assets/screenshots/de/myday-light.png#only-light){ .screenshot }
![Mein Tag](../assets/screenshots/de/myday-dark.png#only-dark){ .screenshot }

!!! info "Nur auf diesem Gerät"
    Microsoft To Do speichert Mein Tag in einem internen Dienst, den
    Drittanbieter-Apps nicht nutzen können. Merkzettel speichert Mein Tag
    deshalb lokal; es wird nicht mit Microsoft To Do auf deinem Handy oder im
    Web geteilt.

## Erinnerungen

Ist eine Erinnerung fällig, zeigt Merkzettel eine Desktop-Benachrichtigung mit
**Erledigt**. Erinnerungen kommen nur, während Merkzettel läuft – lass ihn im
Systemabschnitt oder starte ihn mit deiner Sitzung (siehe
[Konfiguration](configuration.md)).

## Offline

Merkzettel hält eine Kopie deiner Listen. Ohne Verbindung startet er mit dieser
Kopie und zeigt einen Hinweis; Änderungen brauchen eine Verbindung und werden
nicht vorgemerkt. Kommt das Netz zurück, verbindet sich Merkzettel von selbst
neu, oder du wählst **Erneut verbinden**.

![Offline-Hinweis](../assets/screenshots/de/offline-light.png#only-light){ .screenshot }
![Offline-Hinweis](../assets/screenshots/de/offline-dark.png#only-dark){ .screenshot }

## Synchronisierung

Merkzettel holt Änderungen, wenn du die Liste wechselst, wenn du
<kbd>Strg</kbd>+<kbd>R</kbd> / <kbd>F5</kbd> drückst und nach deinen eigenen
Änderungen. Er fragt bei Microsoft nur ab, was sich seit dem letzten Mal
geändert hat (Delta-Synchronisierung). Bittet Microsoft ihn, langsamer zu
werden, wartet er und versucht es von selbst erneut.
