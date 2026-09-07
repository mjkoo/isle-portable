# Exporting Android saves

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

Export does not apply or discard staged settings edits. Cancellation and failed
writes attempt to remove the newly created output. If the provider refuses removal,
the error identifies the potentially incomplete document. Closing Settings is
disabled during export; cancellation must wait for an outstanding provider I/O call
to return. Process death interrupts export, and a fresh menu capture is required.

To transfer to desktop, close the desktop game, preserve its existing saves, and
extract the ZIP's `saves/` contents into the desktop game's configured save
directory. Keep all exported files together. Android restore is not yet provided.
Auto Backup eligibility remains independent of this explicit export feature.
