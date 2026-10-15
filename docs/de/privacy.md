# Datenschutz

Merkzettel ist ein Client für dein Microsoft-Konto. Er hat keinen eigenen
Server, keine Telemetrie, keine Absturzberichte und keine Update-Prüfung.

## Was wohin gesendet wird

| Ziel | Was | Warum |
|---|---|---|
| `login.microsoftonline.com` | Anmeldung und Token-Erneuerung | Um auf dein Konto zuzugreifen |
| `graph.microsoft.com` | Deine Listen, Aufgaben, Schritte, Verknüpfungen, Kategorien, Anhänge | Um deine To-Do-Daten zu lesen und zu ändern |

An das Merkzettel-Projekt oder sonstige Dritte wird nichts gesendet. Öffnest
du eine Verknüpfung oder einen Anhang, wird sie an die Anwendung übergeben,
die dein Desktop dafür verwendet.

## Was auf deinem Computer gespeichert wird

| Daten | Ort | Schutz |
|---|---|---|
| Anmelde-Token (Refresh-Token) | Schlüsselbund des Systems (KWallet / Secret Service) | Vom Schlüsselbund verschlüsselt |
| Kopie der Listen und Aufgaben, Mein Tag | `~/.local/share/merkzettel/cache.sqlite` | Nur für deinen Benutzer lesbar (0600) |
| Geöffnete Anhänge | `~/.cache/merkzettel/attachments/` | Nur für deinen Benutzer lesbar (0700) |
| Ansichtseinstellungen | `~/.config/merkzettelrc` | Normale Benutzerdatei |

Das Flatpak legt diese Dateien unter
`~/.var/app/io.github.mzilinski.merkzettel/` ab.

## Deine Daten entfernen

1. **Abmelden** in Merkzettel entfernt das Token aus dem Schlüsselbund.
2. Lösche die oben genannten Dateien.
3. Um Merkzettels Zugriff auf Microsofts Seite zu widerrufen, öffne
   [myapps.microsoft.com](https://myapps.microsoft.com) (Arbeit/Schule) oder
   [account.live.com/consent/Manage](https://account.live.com/consent/Manage)
   (privates Konto) und entferne die App.

## Microsoft

Deine To-Do-Daten verarbeitet Microsoft gemäß seiner Datenschutzerklärung und,
bei Konten von Arbeit oder Schule, den Vereinbarungen deiner Organisation mit
Microsoft. Daran ändert Merkzettel nichts.

Die Datenschutzerklärung der Merkzettel-Website findest du auf der
[Website](https://mzilinski.github.io/merkzettel/datenschutz/).
