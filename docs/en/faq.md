# FAQ

## Is Merkzettel made by Microsoft?

No. Merkzettel is an independent open-source project. It uses Microsoft's
public API, Microsoft Graph, the same way other third-party apps do.

## Which accounts work?

Personal Microsoft accounts (outlook.com, hotmail.com, live.com …) and work or
school accounts in Microsoft 365. In some organisations an administrator has
to approve the app first; see [How-to: work account](howto/work-account.md).

## Why does Merkzettel not have feature X from Microsoft To Do?

Merkzettel can only do what Microsoft Graph offers to third-party apps. These
To Do features are **not** available there:

| Feature | Situation in Merkzettel |
|---|---|
| **Sharing lists**, inviting people, removing members | Not possible; **Share …** opens the To Do web app. Shared lists themselves work normally. |
| **Assigning tasks** to people | Not available |
| **List groups** (folders) | Not available |
| **My Day** synchronisation | Local only, see below |
| **List themes and backgrounds** | Not available |
| **Category colours** | Graph only returns category names; the colours belong to your Outlook settings |
| **Manual drag-and-drop order** | Graph does not expose the order |

### My Day

Microsoft To Do synchronises *My Day* through an internal Microsoft service
(not Microsoft Graph) that only Microsoft's own apps may use. Merkzettel
therefore keeps My Day on your computer: it works the same way and starts
empty every day, but you will not see the same My Day on your phone.

### Moving tasks

Graph has no "move to list" operation. Merkzettel copies the task — title,
notes, dates, reminder, recurrence, categories, steps, links and attachments —
into the target list and then deletes the original. The copy has a new
creation date, and the task's history in Microsoft's audit logs (if your
organisation uses them) shows a new task.

## How does synchronisation work?

Merkzettel keeps a local copy and asks Microsoft only for changes since the
last sync (*delta sync*). It syncs when you switch lists, press
<kbd>F5</kbd>, and after your own changes. If you edit the same task on
another device, the field changed last wins; different fields do not overwrite
each other.

## Does Merkzettel work offline?

It starts and shows your lists without a connection. Changes need a
connection and are not stored for later; Merkzettel says so and reconnects
automatically when the network returns.

## Why are there more requests than expected after an update?

When a new version adds data to the local cache (for example categories),
Merkzettel downloads all lists once. Microsoft may then briefly slow it down;
Merkzettel waits and continues on its own.

## Where is my data stored? Is anything sent elsewhere?

See [Privacy](privacy.md). In short: only to Microsoft, nothing to anyone
else, no telemetry.

## How can I support the project?

Report bugs, translate, contribute code or [donate](https://paypal.me/eit31).
See [Contributing](https://github.com/mzilinski/merkzettel/blob/main/CONTRIBUTING.md).
