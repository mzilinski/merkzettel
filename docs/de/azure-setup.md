# Azure-Einrichtung

Merkzettel meldet sich über eine **App-Registrierung** in Microsoft Entra ID
(früher Azure AD) bei Microsoft an. Die Pakete von Flathub und aus dem AUR
enthalten bereits eine, die meisten können diese Seite also überspringen. Eine
eigene Registrierung brauchst du nur, wenn du Merkzettel aus dem Quellcode
baust oder deine Organisation verlangt, dass Apps in ihrem eigenen Mandanten
registriert sind.

Die Registrierung ist kostenlos, funktioniert für private Microsoft-Konten und
für Konten von Arbeit oder Schule und dauert etwa fünf Minuten. Merkzettel ist
ein *öffentlicher Client*: Er nutzt den OAuth-2.0-Autorisierungscode-Ablauf mit
PKCE und hat **kein Client-Secret**.

## 1. Registrierung anlegen

1. Öffne das [Microsoft Entra Admin Center](https://entra.microsoft.com/) und
   melde dich mit einem beliebigen Microsoft-Konto an.
2. Gehe zu **Identität → Anwendungen → App-Registrierungen** und wähle
   **Neue Registrierung**.
3. **Name:** `Merkzettel` (jeder Name geht; er erscheint auf der
   Zustimmungsseite).
4. **Unterstützte Kontotypen:** Wähle
   **Konten in einem beliebigen Organisationsverzeichnis und persönliche
   Microsoft-Konten**, um beide Kontoarten zu nutzen. Brauchst du nur eine,
   wähle eine engere Option.
5. **Umleitungs-URI:** Plattform **Öffentlicher Client/nativ (mobil und
   Desktop)**, Wert genau `http://localhost:53682/callback`.
6. Wähle **Registrieren**.

## 2. Öffentliche Clientflows erlauben

1. Öffne in der neuen Registrierung **Authentifizierung**.
2. Stelle unter **Erweiterte Einstellungen** die Option **Öffentliche
   Clientflows zulassen** auf **Ja**.
3. Wähle **Speichern**.

!!! warning "Achtung"
    Lege kein Client-Secret an. Desktop-Apps können keine Geheimnisse
    bewahren; PKCE ersetzt es.

## 3. API-Berechtigungen hinzufügen

1. Öffne **API-Berechtigungen → Berechtigung hinzufügen → Microsoft Graph →
   Delegierte Berechtigungen**.
2. Wähle `Tasks.ReadWrite`, `User.Read`, `offline_access`, `openid` und
   `profile`.
3. Wähle **Berechtigungen hinzufügen**.

Private Konten stimmen bei der ersten Anmeldung selbst zu. In einem Mandanten
von Arbeit oder Schule muss eventuell ein Administrator zustimmen
(**Administratorzustimmung für … erteilen**).

## 4. Client-ID kopieren

Kopiere auf der Seite **Übersicht** der Registrierung die
**Anwendungs-ID (Client)**, eine UUID wie
`12345678-aaaa-bbbb-cccc-1234567890ab`.

## 5. Merkzettel damit bauen

```bash
cmake -B build -S . -DMERKZETTEL_CLIENT_ID=12345678-aaaa-bbbb-cccc-1234567890ab \
  -DMERKZETTEL_TENANT=common
cmake --build build
./build/bin/merkzettel
```

Wähle beim ersten Start **Anmelden**. Dein Browser öffnet die Anmelde- und
Zustimmungsseite von Microsoft; danach erhält Merkzettel das Ergebnis über
`http://localhost:53682/callback` und speichert das Refresh-Token im
Schlüsselbund des Systems.

## Mandantenwerte

| `MERKZETTEL_TENANT` | Wer sich anmelden kann |
|---|---|
| `common` | Private Konten und Konten von Arbeit oder Schule |
| `organizations` | Konten von Arbeit oder Schule aus beliebigen Mandanten |
| `consumers` | Nur private Microsoft-Konten |
| `<tenant ID>` | Nur Konten genau dieses Mandanten |

## Wofür die Berechtigungen dienen

| Berechtigung | Wofür |
|---|---|
| `Tasks.ReadWrite` | Deine To-Do-Listen und -Aufgaben lesen und ändern |
| `User.Read`, `openid`, `profile` | Das angemeldete Konto erkennen |
| `offline_access` | Angemeldet bleiben (Refresh-Token) |

Merkzettel fragt nichts weiter an – keine Mails, keinen Kalender, keine Dateien.
