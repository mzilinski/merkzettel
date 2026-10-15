# Azure setup

Merkzettel signs in to Microsoft through an **app registration** in Microsoft
Entra ID (formerly Azure AD). Packages from Flathub and the AUR already contain
one, so most users can skip this page. You need your own registration only when
you build Merkzettel from source, or when your organisation requires apps to be
registered in its own tenant.

The registration is free, works for personal Microsoft accounts and for work or
school accounts, and takes about five minutes. Merkzettel is a *public client*:
it uses the OAuth 2.0 authorization code flow with PKCE and has **no client
secret**.

## 1. Create the registration

1. Open the [Microsoft Entra admin center](https://entra.microsoft.com/) and
   sign in with any Microsoft account.
2. Go to **Identity → Applications → App registrations** and choose
   **New registration**.
3. **Name:** `Merkzettel` (any name works; users see it on the consent
   screen).
4. **Supported account types:** choose
   **Accounts in any organizational directory and personal Microsoft accounts**
   to use both kinds of accounts. Choose a narrower option if you only need one.
5. **Redirect URI:** platform **Public client/native (mobile & desktop)**, value
   exactly `http://localhost:53682/callback`.
6. Choose **Register**.

## 2. Allow public client flows

1. In the new registration open **Authentication**.
2. Under **Advanced settings**, set **Allow public client flows** to **Yes**.
3. Choose **Save**.

!!! warning
    Do not create a client secret. Desktop apps cannot keep secrets; PKCE
    replaces it.

## 3. Add API permissions

1. Open **API permissions → Add a permission → Microsoft Graph → Delegated
   permissions**.
2. Select `Tasks.ReadWrite`, `User.Read`, `offline_access`, `openid` and
   `profile`.
3. Choose **Add permissions**.

Personal accounts consent on first sign-in. In a work or school tenant an
administrator may have to grant consent (**Grant admin consent for …**).

## 4. Copy the client ID

On the registration's **Overview** page copy the **Application (client) ID**,
a UUID such as `12345678-aaaa-bbbb-cccc-1234567890ab`.

## 5. Build Merkzettel with it

```bash
cmake -B build -S . -DMERKZETTEL_CLIENT_ID=12345678-aaaa-bbbb-cccc-1234567890ab \
  -DMERKZETTEL_TENANT=common
cmake --build build
./build/bin/merkzettel
```

On the first start choose **Sign in**. Your browser opens Microsoft's sign-in
and consent page; afterwards Merkzettel receives the result on
`http://localhost:53682/callback` and stores the refresh token in the system
keychain.

## Tenant values

| `MERKZETTEL_TENANT` | Who can sign in |
|---|---|
| `common` | Personal and work/school accounts |
| `organizations` | Work/school accounts of any tenant |
| `consumers` | Personal Microsoft accounts only |
| `<tenant ID>` | Only accounts of that one tenant |

## What the permissions are used for

| Permission | Used for |
|---|---|
| `Tasks.ReadWrite` | Reading and changing your To Do lists and tasks |
| `User.Read`, `openid`, `profile` | Identifying the signed-in account |
| `offline_access` | Staying signed in (refresh token) |

Merkzettel requests nothing else — no mail, calendar or files.
