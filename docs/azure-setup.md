# Azure App Registration for Merkzettel

To let Merkzettel authenticate against Microsoft To Do (Microsoft Graph), you need your own **Azure App Registration**. Microsoft allows this for free for both personal and work/school accounts. The whole process takes about 5 minutes.

## 1. Register

1. Go to <https://entra.microsoft.com/> → **Applications** → **App registrations** → **New registration**.
2. **Name**: `Merkzettel` (anything works).
3. **Supported account types**: pick what fits you. For both personal and work use:
   - **Accounts in any organizational directory (multitenant) and personal Microsoft accounts**
4. **Redirect URI**: platform **Mobile and desktop applications**, URI exactly:
   `http://localhost:53682/callback`
   (Merkzettel listens on the fixed port 53682. If you want a different port, change `kCallbackPort` in `src/auth/authmanager.cpp` and the Azure URI to match.)
5. Click **Register**.

## 2. Enable public-client mode

In the new app registration:

1. Left sidebar → **Authentication**.
2. Scroll to **Advanced settings** → **Allow public client flows** → **Yes**.
3. Save.

> Important: do NOT create a "Client Secret". Public clients use PKCE instead of a secret.

## 3. Configure API permissions

1. Left sidebar → **API permissions** → **Add a permission** → **Microsoft Graph** → **Delegated permissions**.
2. Search and enable:
   - `Tasks.ReadWrite`
   - `User.Read`
   - `offline_access`
   - `openid`
   - `profile`
3. Click **Add permissions**.
4. Personal Microsoft accounts do not need admin consent. Work/school tenants may need an admin consent — ask your IT admin or click "Grant admin consent" if you have rights.

## 4. Copy the values

In the **Overview** of the app registration:

- **Application (client) ID**: a long UUID, e.g. `12345678-aaaa-bbbb-cccc-1234567890ab`
- **Directory (tenant) ID**: only relevant for single-tenant. For multi-tenant we use `common`.

## 5. Build Merkzettel

```bash
cmake -B build -S . \
  -DMERKZETTEL_CLIENT_ID=12345678-aaaa-bbbb-cccc-1234567890ab \
  -DMERKZETTEL_TENANT=common
cmake --build build -j
./build/bin/merkzettel
```

On first launch the browser opens, Microsoft asks for consent, and Merkzettel stores the refresh token in KWallet.

## Tenant values at a glance

| Value | Meaning |
|---|---|
| `common` | Personal **and** work/school accounts |
| `organizations` | Work/school only (any tenant) |
| `consumers` | Personal Microsoft accounts only |
| `<tenant-uuid>` | Exactly your own work/school tenant |

## Token storage

The refresh token lives in **KWallet** under:
- Service: `merkzettel`
- Key: `refresh_token`

Delete it via the KWallet manager or the `Sign out` action in the app menu.
