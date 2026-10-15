---
title_en: Offline on the road
title_de: Offline unterwegs
setup: launch --demo
env: MERKZETTEL_DEMO_OFFLINE=1
---

# Video script: Offline on the road

Times are positions in the finished video. Actions use screen coordinates of
the 1920×1080 recording (window at 210,75, 1500×930); `en:`/`de:` limit an
action to one language. See [README](README.md) for the action reference.

| Time | Action | English | Deutsch |
|---|---|---|---|
| 0:00 | `title` | On the train or on a plane, the connection comes and goes. You still want to see what is on your list. | Im Zug oder im Flugzeug ist das Netz mal da, mal weg. Trotzdem willst du sehen, was auf deiner Liste steht. |
| 0:08 | `move 900 147 1.2; poster` | Merkzettel keeps a local copy of all your lists. It starts without a network, and a banner tells you that you are offline. | Merkzettel hält eine lokale Kopie all deiner Listen. Es startet ohne Netz, und ein Hinweis sagt dir, dass du offline bist. |
| 0:18 | `click 264 848; wait 2; click 264 812; wait 1; click 700 642` | Lists, tasks, steps, notes and categories are all there. | Listen, Aufgaben, Schritte, Notizen und Kategorien sind alle da. |
| 0:26 | `wait 1.5; keyq Escape; key ctrl+f; en: type "expenses"; de: type "Reisekosten"` | Search, sorting and My Day work offline, too. | Suche, Sortierung und „Mein Tag“ funktionieren auch offline. |
| 0:33 | `keyq ctrl+a BackSpace Escape; move 900 600 1.0` | Changing tasks needs a connection. Merkzettel does not queue changes silently. It tells you when something could not be saved. | Änderungen an Aufgaben brauchen eine Verbindung. Merkzettel sammelt sie nicht heimlich, sondern sagt dir, wenn etwas nicht gespeichert werden konnte. |
| 0:45 | `en: move 1647 147 1.0; de: move 1626 147 1.0; wait 1.5; en: click 1647 147; de: click 1626 147` | Back online, Merkzettel reconnects by itself, or you choose Reconnect. Then it fetches everything that changed in the meantime. | Wieder online, verbindet sich Merkzettel von selbst, oder du wählst Erneut verbinden. Dann holt es alles, was sich inzwischen geändert hat. |
| 0:56 | `click 255 668` | A tip for the road: open the All list once before you leave. It updates every list at once. | Ein Tipp für unterwegs: Öffne vor der Abfahrt einmal die Liste Alle. Sie aktualisiert alle Listen auf einmal. |
| 1:06 | `end` | That is how Merkzettel works offline. | So arbeitest du mit Merkzettel offline. |
