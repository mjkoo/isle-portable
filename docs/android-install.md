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

Picking the one that matches your device is worth doing: the arm64 release APK is 4.4 MiB against
14 MiB for the universal one, because the other three architectures are dead weight on it.

If you do not know what your device is, the universal APK always works. To check, run
`adb shell getprop ro.product.cpu.abi`, or read the CPU line in an app like Device Info HW.

All five are the same version and are signed with the same key, so one can be installed over
another without uninstalling first.

`debug` builds log more and are not minified; `release` builds are what you want for playing.

## Which build you are running

The version name is the project version, the number of commits behind it, and the commit it was
built from:

```
0.1.2721+g92a28e0e
```

Android shows it under Settings > Apps > Lego Island, and `adb shell dumpsys package
org.legoisland.isle` prints both it and the version code. The version code is the commit count on
its own, so a later build always sorts above an earlier one and Android treats it as an upgrade.

A build made outside a full git clone cannot know any of this, and says `+unknown` rather than
guessing.

## After installing

The game needs an existing copy of LEGO Island 1.1 (English). The first launch asks for the folder
holding `LEGO/Scripts` and `LEGO/data` and copies it into the app's own storage. See
[Android game files](android-game-files.md) for replacing or removing it later, and
[Android saves](android-saves.md) for getting saves on and off the device.
