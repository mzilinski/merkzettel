# Experimental: Share-Management for To-Do Lists

## Status

**Not implemented.** This document captures the design and risks of retrofitting full share management — creating invitations, listing members, removing members — onto Merkzettel. Microsoft Graph does not expose these operations, so the only viable path is reverse-engineering the internal API the official Microsoft To-Do clients use, via mitmproxy or equivalent.

The current shipping behaviour (v0.3.0) is read-only:

- The Graph property `isShared` is parsed and surfaced as a sidebar icon next to shared lists.
- A "Share ..." action in the list context menu opens `https://to-do.office.com/tasks/list/{listId}` in the browser, where the official share UI handles invitation creation. A first-time inline banner explains the handoff.
- Once an invitation is accepted in the web/native MS app, the shared list appears in `GET /me/todo/lists` with `isShared = true` and Merkzettel can read/write tasks in it normally — Graph treats shared and owned lists identically once membership exists.

What's missing: creating the invitation, listing members, removing them, changing permission levels.

## Why Graph isn't enough

Microsoft has consistently declined to add share-management to the public Graph To-Do surface. The official MS To-Do clients call an internal Outlook-Tasks/Substrate backend that uses a different audience and a non-Graph URL scheme.

Confirmed: zero hits for `share`/`invitation`/`member` in `mcp-outlook365/GRAPH_API_REFERENCE.md` and `mcp-outlook365/outlook365_mcp/tools/todo.py` (the latter only maps `isShared` as a read-only property).

## Reverse-engineering plan

Standard interop-research workflow. **Do this against your own account only.**

1. **Set up mitmproxy locally.**
   - `pip install mitmproxy`
   - `mitmweb --listen-port 8080`
   - Install the mitmproxy CA cert into the OS trust store (or browser, depending on which client you intercept).

2. **Choose a target client.**
   - **Web** (`to-do.office.com`) — easiest, runs in a regular browser, mitmproxy works via system proxy + cert install.
   - **Android** (Microsoft To Do app) — needs a rooted device or Frida bypass for cert pinning.
   - **Windows desktop** — works without rooting if you can route via system proxy. Cert pinning may apply.

   Web is the right starting point.

3. **Capture the four core operations.**
   - Create share invitation (link or email-based)
   - Accept invitation (open the link from another account)
   - List members of a shared list
   - Remove a member / leave a list

4. **Inspect each request.**
   - Endpoint host and path (likely `outlook.office.com/api/v2.0/me/...` or `substrate.office.com/Todo/...`, **not** `graph.microsoft.com`)
   - Authorization header — decode the JWT, note the `aud` claim (audience). It will not match the Graph audience; you'll need to request a token for that audience separately.
   - Required scopes (visible in the AAD consent screen if you trigger a fresh login)
   - Request/response JSON shape

5. **Document the findings.**
   - File a follow-up doc `docs/internal-todo-api.md` with the captured endpoints, request shapes, scope requirements, and any anti-replay/CSRF-token quirks observed.

## Implementation sketch (when API is known)

- `src/auth/authmanager.cpp`: add a second token cache for the Outlook audience. Refresh-token flow stays in KWallet; multiple access-tokens keyed by audience.
- `src/graph/graphclient.cpp`: extract a `HttpClient` base; `GraphClient` becomes one configuration (host + audience), `OutlookClient` another.
- `src/graph/todoapi.cpp`: new methods `createShareInvitation`, `listMembers`, `removeMember`. Wire into the same TodoApi class — callers don't care which backend serves a given operation.
- UI: replace the "open browser" handoff with an in-app sharing dialog (recipient email, permission level), only when the user has opted into the experimental flag.

**Surface as opt-in only.** Default behaviour stays the safe Graph-only path.

- New CLI flag `--experimental-share`, OR `KConfig` entry `Share/UseExperimentalApi=true`, set explicitly by the user.
- The "Share ..." action checks the flag and routes to either the in-app dialog (experimental) or the existing browser handoff (default).

## Risks

| Risk | Mitigation |
|---|---|
| Microsoft changes the internal API without notice. | Pin path versions (`v2.0`). CI smoke-tests against a dedicated test account. Keep the Graph-only fallback intact and default. |
| Microsoft Terms of Service prohibit "automated access via undocumented APIs". | Practical enforcement targets large-scale abuse, not solo interop. EU Software Directive Art. 6 and DMCA §1201(f) explicitly permit reverse engineering for interoperability. Position the feature as opt-in for personal use, not a default-on capability. |
| Token-audience separation requires double PKCE flow. | Single OAuth login already authorizes multiple audiences via the v2.0 endpoint; just request both `https://graph.microsoft.com/.default` and `https://outlook.office.com/Tasks.ReadWrite` in the same consent. |
| Anti-abuse rate limits on the public app id. | Merkzettel already uses its own Azure App Registration (`MERKZETTEL_CLIENT_ID`), so it isn't competing for a shared quota. Risk is low at single-user volumes. |
| Cert pinning blocks mitmproxy on mobile/desktop clients. | Use the web client for capture — no pinning. |

## When to revisit

Trigger conditions that would justify pulling this off the backlog:

- Multiple users open issues asking for in-app share management.
- Microsoft adds the operations to Graph (would unblock everything; check Graph beta endpoints quarterly).
- A maintained third-party library appears that has already done the reverse-engineering work and tracks API drift.
