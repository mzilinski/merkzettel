# Security policy

## Supported versions

Only the latest release receives security fixes.

## Reporting a vulnerability

Please **do not** open a public issue. Use GitHub's
[private vulnerability reporting](https://github.com/mzilinski/merkzettel/security/advisories/new)
or e-mail **malte@zilinski.eu** with "Merkzettel security" in the subject.

Include the affected version, steps to reproduce and the impact you see. You
will get an acknowledgement within seven days. Fixes are released as soon as
possible and credited in the changelog unless you prefer otherwise.

## What Merkzettel protects

- The OAuth refresh token is stored only in the system keychain (Secret
  Service / KWallet) and never written to logs or files.
- Sign-in uses the OAuth 2.0 authorization code flow with PKCE on a
  loopback redirect (`http://localhost:53682/callback`); the `state`
  parameter is verified.
- Task data is cached in `~/.local/share/merkzettel/cache.sqlite` with the
  user's file permissions; opened attachments in `~/.cache/merkzettel/`.
- Merkzettel contacts only `login.microsoftonline.com` and
  `graph.microsoft.com`.

Issues in Microsoft's services themselves should be reported to Microsoft.
