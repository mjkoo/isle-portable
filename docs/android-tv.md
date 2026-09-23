# Android TV

The same APK installs on Android TV and Google TV. The game is listed with the other games in
the TV launcher, with a banner. See [installing the Android build](android-install.md) for which
APK to take; sideloading it onto a TV usually means `adb install`.

## Controls

A controller is the way to play on a TV. It works as described in
[Android controllers](android-controllers.md): the left stick moves, the D-pad moves the cursor,
and Start opens the game menu.

A remote can drive the menus and point and click, but it cannot walk:

| Remote key | What it does |
|---|---|
| Back | Opens the game menu (Resume, Settings, Quit) |
| Menu, where the remote has one | Opens the game menu |
| D-pad | Moves the cursor; in the menu and Settings, moves between buttons and rows |
| Select (centre) | Clicks |

Walking needs a stick, arrow keys or touch, so with only a remote, click where to go. Android
passes a remote to the game as a controller with an unusual layout, so a remote's other keys may
do something, but only the ones above are meant to.

The touch controls, the on-screen menu button and the touch rows in Settings are not shown on a
TV.

## Getting the game files in

Many TVs have no system folder picker; Android TV answers the request with "You don't have an app
that can do this". When there is none, the first launch says so, names the folder the game reads
from, `/storage/emulated/0/Android/data/org.legoisland.isle/files`, and waits. Copy the `LEGO`
folder from a LEGO Island 1.1 (English) installation there with adb, from the folder on your
computer that holds `LEGO`:

```sh
adb push LEGO /data/local/tmp/
adb shell cp -r /data/local/tmp/LEGO /storage/emulated/0/Android/data/org.legoisland.isle/files/
adb shell rm -r /data/local/tmp/LEGO
```

Then choose **Check again**. The game starts once every file it needs is there; otherwise it names
the one still missing. `adb push` straight into `Android/data` does not work on a user build,
which is what a TV runs: adb is not allowed to create the folders there, but a copy made from
`adb shell` is. On a TV that does have a folder picker, the first launch offers it as on a phone.

## Settings without a picker

**Data > Replace game files** needs a folder picker. Without one, Settings says so: choose
**Remove game files** instead, and the next launch asks for the files as above. **Export saves**
and **Restore saves** need a file picker too, and are shown unavailable when there is none.
**Restore previous saves**, where offered, needs none.

## Limits

This has been run on the Android TV emulator only (API 34), with its remote and an emulated one,
not on TV hardware.
