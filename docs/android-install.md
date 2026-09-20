# Installing the Android build

The [Releases tab](https://github.com/isledecomp/isle-portable/releases/tag/continuous) carries
several Android APKs per build. They are the same application; they differ only in which CPU
architectures they contain.

## Which one to take

| File | For |
| - | - |
| `app-arm64-v8a-*.apk` | Almost every phone, tablet and handheld made since about 2016 |
| `app-armeabi-v7a-*.apk` | Older 32-bit ARM devices |
| `app-x86_64-*.apk` | Emulators, Chromebooks and x86 tablets |
| `app-x86-*.apk` | Older 32-bit x86 emulators |
| `app-universal-*.apk` | All four at once: take this if you are not sure |

Picking the one that matches your device is worth doing: the universal APK is roughly three times
the size of a single-architecture one, because the other three architectures are dead weight on it.

If you do not know what your device is, the universal APK always works. To check, run
`adb shell getprop ro.product.cpu.abi`, or read the CPU line in an app like Device Info HW.

`debug` builds log more and are not minified; `release` builds are what you want for playing.

Within one of those, all five APKs are the same version and carry the same signature, so you can
install a different one over what you already have without uninstalling first. Swapping between a
`debug` and a `release` APK is not the same thing: they are signed with different keys, and Android
refuses the install until the old one is removed.

## Which build you are running

The version name is the project version, the number of commits in the history it was built from,
and the commit itself:

```
0.1.2721+g92a28e0e
```

Android shows it under Settings > Apps > Lego Island, and `adb shell dumpsys package
org.legoisland.isle` prints both it and the version code. The version code is the commit count on
its own, so a later build always sorts above an earlier one and Android treats it as an upgrade.

A build made outside a full git clone cannot know any of this, and says `+unknown` rather than
guessing. A build given its version code directly, with `-PisleVersionCode=<n>`, uses that number
and names itself `0.1.<n>` with no commit on the end.

## After installing

The game needs an existing copy of LEGO Island 1.1 (English). The first launch asks for a folder
holding one, either the folder that contains `LEGO` or the `LEGO` folder itself, and copies it into
`Android/data/org.legoisland.isle/files/`. No storage permission is required. See
[Android game files](android-game-files.md) for replacing or removing it later, and
[Android saves](android-saves.md) for getting saves on and off the device.
