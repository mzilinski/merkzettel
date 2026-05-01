# Azure App Registration fuer Merkzettel

Damit Merkzettel sich gegen Microsoft To Do (Microsoft Graph) authentifizieren kann, brauchst du eine eigene **Azure App Registration**. Microsoft erlaubt das fuer persoenliche und Work/School-Accounts kostenlos. Der Vorgang dauert ca. 5 Minuten.

## 1. Anlegen

1. Gehe zu <https://entra.microsoft.com/> → **Anwendungen** → **App-Registrierungen** → **Neue Registrierung**.
2. **Name**: `Merkzettel` (frei waehlbar).
3. **Unterstuetzte Kontotypen**: waehle, was zu dir passt. Fuer eine private + Work-Anbindung:
   - **Konten in einem beliebigen Verzeichnis (Multitenant) und persoenliche Microsoft-Konten**
4. **Umleitungs-URI**: Plattform **Oeffentlicher Client/native (mobile und Desktop)**, URI exakt:
   `http://localhost:53682/callback`
   (Merkzettel hoert auf festem Port 53682. Wenn du den Port aendern willst, muss `kCallbackPort` in `src/auth/authmanager.cpp` und die Azure-URI uebereinstimmen.)
5. **Registrieren** klicken.

## 2. Public-Client-Modus aktivieren

In der neuen App-Registrierung:

1. Linke Sidebar → **Authentifizierung**.
2. Scrolle zu **Erweiterte Einstellungen** → **Oeffentliche Clientflows zulassen** → **Ja**.
3. Speichern.

> Wichtig: KEIN „Client Secret" anlegen. Public Clients verwenden PKCE statt Secret.

## 3. API-Berechtigungen setzen

1. Linke Sidebar → **API-Berechtigungen** → **Berechtigung hinzufuegen** → **Microsoft Graph** → **Delegierte Berechtigungen**.
2. Suche und aktiviere:
   - `Tasks.ReadWrite`
   - `User.Read`
   - `offline_access`
3. **Berechtigungen hinzufuegen**.
4. Fuer persoenliche Microsoft-Konten ist keine Admin-Zustimmung noetig. Fuer Work/School-Tenants kann ein Admin-Consent noetig sein — frag deinen IT-Admin oder klicke „Administratorzustimmung erteilen".

## 4. Werte herauskopieren

Im **Ueberblick** der App-Registrierung:

- **Anwendungs-(Client-)ID**: lange UUID, z. B. `12345678-aaaa-bbbb-cccc-1234567890ab`
- **Verzeichnis-(Mandanten-)ID**: nur fuer single-tenant relevant. Fuer den Mehrkontenfall verwenden wir `common`.

## 5. Merkzettel bauen

```bash
cmake -B build -S . \
  -DMERKZETTEL_CLIENT_ID=12345678-aaaa-bbbb-cccc-1234567890ab \
  -DMERKZETTEL_TENANT=common
cmake --build build -j
./build/bin/merkzettel
```

Beim ersten Start oeffnet sich der Browser, Microsoft fragt nach Zustimmung, und Merkzettel speichert das Refresh-Token in KWallet.

## Tenant-Werte im Ueberblick

| Wert | Bedeutung |
|---|---|
| `common` | Persoenliche **und** Work/School-Konten |
| `organizations` | Nur Work/School (alle Tenants) |
| `consumers` | Nur persoenliche Microsoft-Konten |
| `<tenant-uuid>` | Genau dein eigener Work/School-Tenant |

## Token-Speicher

Das Refresh-Token landet in **KWallet** unter:
- Service: `merkzettel`
- Key: `refresh_token`

Loeschen via KWallet-Manager oder `Abmelden` im App-Menue.
