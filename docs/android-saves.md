# Transferring Android saves

Open the game menu using Back or the on-screen menu button, choose Settings, then
Data > Export saves. Choose a location in the system save picker. No storage
permission is required. The same action is available from startup-error Settings
when the save directory can be resolved and read.

The ZIP contains existing `G0.GS` through `G8.GS`, `Players.gsi` and `History.gsi`
under `saves/`. It excludes game assets and `isle.ini`. An explicit save-directory
override is honored. Empty sets cannot be exported; incomplete sets require
confirmation. Symlinks, ambiguous filenames and sets larger than 16 MiB are refused.

The files are captured when the game menu opens, after its save attempt. Settings
shows the capture time. Reopen the menu to capture again. Export preserves those
bytes; it cannot guarantee that the engine serialized every recent change. A
detected save failure is reported before export. Success means the document was
written and closed, not that a cloud provider finished uploading it.

The menu message reports the save attempt made when the menu opens. Its action is
**Save and quit** after an apparently successful save attempt, **Quit** when there
is nothing to save, or **Quit anyway** after a detected failure. Resume and Settings
remain available in all three cases. Startup errors instead offer Settings and Close.

Android background events can trigger more save attempts while the prompt is open,
including when opening Settings. Shutdown attempts another save and does not check
its result. These later attempts do not update the menu's message or captured export
bytes. Reopening the menu retries the reported save. The engine does not report
every write or serialization failure, so no message guarantees a file was written.

Export does not apply or discard staged settings edits. Cancellation and failed
writes attempt to remove the newly created output. If the provider refuses removal,
the error identifies the potentially incomplete document. Closing Settings is
disabled during export; cancellation must wait for an outstanding provider I/O call
to return. After process death, recovery preserves any recorded destination and
shows its location for inspection: the document may be complete or incomplete.
This also protects completed exports when saving the recovery information fails.
A fresh menu capture is required to export again after process death.

To transfer to desktop, close the desktop game, preserve its existing saves, and
extract the ZIP's `saves/` contents into the desktop game's configured save
directory. Keep all exported files together. Android restore is described below.
Auto Backup eligibility remains independent of this explicit export feature.

## Restoring on Android

Open Settings > Data > Restore saves and select an exported ZIP. The app checks
its contents and shows its file and player counts before asking to replace all
current players and progress. Resolve pending settings edits with Save or Cancel
before starting restore. Cancelling archive selection or its confirmation leaves
saves unchanged. The selected ZIP is only read, never changed or deleted.

Choose Replace and close, then reopen the game. Normal shutdown finishes against
the old save set. On reopening, the app preserves those files and installs the
replacement before the engine starts. Custom save directories are honored; an
invalid or changed destination never falls back to the default directory. Config,
game assets and unrelated files in that directory are preserved.

Restore accepts complete sets in the export layout: Players.gsi, History.gsi and
one through nine consecutive slots beginning with G0.GS, matching the player count.
Partial exports remain useful for manual salvage but cannot be restored here.
Unsupported versions, serialized states, extra entries, ambiguous names, unsafe
paths and damaged archives are rejected. The limits are 20 MiB for the ZIP and
16 MiB for expanded saves. Binary validation supports the retail save format,
including its island animation table and vehicle build limits. Saved variables are
restricted to the writer's color and lighting settings with checked values; modified
formats are not supported.
Checks establish structural compatibility, not that every saved gameplay state
will behave correctly or that the archive came from a trusted source.

If installation is interrupted, startup recovers the original complete set before
starting the game. An unresolved storage or journal error blocks startup and offers
Retry and Close while preserving recovery data. Before replacement begins,
Cancel restore can discard the request without changing saves. Free space or restore access before
retrying. Cancelling provider I/O waits for the outstanding read to return. Process
death before confirmation requires selecting the ZIP again; confirmed requests
remain in private persistent storage for the next launch.

## Restoring the previous saves

After a successful ZIP restore, Settings offers Restore previous saves with the
backup's date and time. Startup-error Settings offers the same action. This is one
automatic copy of the saves immediately before replacement, including any final
shutdown save. If there were no saves, the confirmation explicitly offers to return
to no saved players.

Restoring this copy replaces current saves and loses progress made since the ZIP
restore. It uses the same close-and-reopen flow. A failed attempt keeps the recovery
option; success consumes it. A later successful ZIP restore replaces the recovery
copy with the set that newer restore displaced. It is not an undo/redo history.

The local recovery copy is excluded from Auto Backup and device transfer and does
not survive uninstall. Export saves separately when you need an independent backup.
