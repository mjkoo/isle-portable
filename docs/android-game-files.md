# Android game files

The game reads its files from the folder named by `diskpath` in `isle.ini`. A fresh install
points it at the app's external files directory, `Android/data/dev.mjkoo.isle/files/`,
and the first launch asks for a folder holding a LEGO Island installation and copies its
`LEGO` folder there. No storage permission is required.

A device with no system folder picker, as many TVs are, cannot be asked for a folder. The first
launch then names the folder and the adb commands that copy `LEGO` into it, and checks again when
asked; see [Android TV](android-tv.md). Replace game files is refused there for the same reason,
with Remove game files as the way round it.

## Settings > Data > Game files

Open the game menu using Back, the on-screen menu button or a controller's Start button, choose
Settings, then Data > Game files. The row shows where the game reads its files from and how much
space they take: "App storage" for the app's own directory, or the path when `diskpath` was set by
hand. Only top-level entries whose names start with `lego` count, since those are the only ones
the game looks at. The same row is available from startup-error Settings, including when the game
files are missing, so a broken installation can be repaired there.

Choosing the row offers:

- **Replace game files**: pick a folder in the system picker, either the folder that holds the
  `LEGO` folder or the `LEGO` folder itself. The app copies it beside the current files, checks
  that every file the game needs is present, and asks for confirmation. The game then closes.
  The next launch puts the new files in place before the game starts and says what happened.
  The new files always go to app storage: a folder named by a hand-set `diskpath` is not
  deleted, but the game stops reading from it.
- **Remove game files**: offered when the game reads its files from app storage and there are
  files there. After confirmation the game closes, and the next launch deletes the copy in app
  storage and asks for a folder, as on a fresh install. Saves and settings are kept.

Resolve settings edits with Save or Cancel first. The current files stay in place and playable
until the next launch, which means:

- Replacing needs room for both copies at once: the new files plus a 32 MiB margin (shown as
  33.55 MB). Settings refuses otherwise and says how much space is needed and how much is
  free. Without that room, remove
  the game files and select the folder when the game reopens; that import deletes the old copy
  before copying.
- Cancelling, a failed copy, or the app being stopped before confirmation leaves the current
  files as they are. A partial copy is deleted straight away, or at the latest on the next
  launch.
- Once a change is waiting for the next launch, another cannot be started until the game has
  been reopened.

## How a change is applied

The copy is made in a hidden directory, `.isle-staging-<id>`, beside the `LEGO` folder. The
game ignores top-level directories whose names do not start with `lego`, so it never sees work
in progress. Confirming records the change in the app's private storage, in `game-files/state`.

On the next launch, before the game reads anything, the current `LEGO` folder is renamed to
`.isle-replaced-<id>`, the staged folder is renamed into its place, and `diskpath` is pointed at
the app's directory. Earlier imports (`imported-*`) are then set aside, the record is removed,
and the directories left over are deleted; the log reports how much was deleted and how long it
took. Removal renames the game data aside the same way before pointing `diskpath` back.

Every move is a rename within one directory, and the record is only removed once `diskpath` has
been updated. A launch interrupted at any point therefore finishes the change, or goes back to
the previous files, the next time the game starts. If the new files turn out incomplete or cannot
be put in place, the previous ones are kept and the launch says so. If the configuration cannot
be updated, the new files are already in place and the update is retried on the next launch;
until then, a `diskpath` that names an earlier import keeps the game reading that import.
