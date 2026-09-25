# LEGO Island, portable - Android-focused fork

This repository is a modified version of [isle-portable](https://github.com/isledecomp/isle-portable),
the portable version of LEGO Island (Version 1.1, English) by the
[isledecomp contributors](https://github.com/isledecomp/isle-portable/graphs/contributors), which is
itself built on the [LEGO Island decompilation](https://github.com/isledecomp/isle). It was forked
from upstream commit [`fdf918e0`](https://github.com/isledecomp/isle-portable/commit/fdf918e0)
(2026-08-31) and has been changed since, mainly in its Android port. The git history keeps
upstream's commits and authorship as they are; everything after the fork point is this fork's.

This fork is not affiliated with or endorsed by the isledecomp project. Please report problems
with this fork here, not upstream. LEGO® is a trademark of the LEGO Group, which does not sponsor,
authorize or endorse this project. **An existing copy of LEGO Island is required.**

## What this fork changes

The Android build gains an in-app settings screen, visible touch controls with an editable
layout, controller remapping, save export and restore, audio focus, Android TV support, a Vulkan
renderer option and per-architecture APKs, and it installs as its own application,
`dev.mjkoo.isle`, beside upstream's. A few changes reach every platform, such as the game going
quiet while paused and the desktop settings tool keeping `isle.ini` keys it does not own. The
Android sections below describe the result. Download the APKs from this repository's
[Releases page](https://github.com/mjkoo/isle-portable/releases); see
[installing the Android build](docs/android-install.md).

## AI disclosure

Most of this fork's changes since the fork point were written with
[Claude Code](https://claude.com/claude-code), Anthropic's AI coding assistant, directed and
reviewed by the maintainer. Commits written with it carry a `Co-Authored-By: Claude` trailer. They
have been tested mainly on Android emulators rather than on a range of physical devices. The
upstream project and the decompilation it builds on are the work of their own contributors, and
this disclosure does not describe them.

## License and credits

Licensed under the [GNU LGPL-3.0](LICENSE), unchanged from upstream. The complete source for each
release is the tag it was built from. Third-party components keep their own licenses: see
`3rdparty/`, and SDL3 and iniparser, which the build fetches.

Credit for the game port belongs to the isle-portable and decompilation contributors. Upstream's
community: [Development Vlog](https://www.youtube.com/playlist?list=PLbpl-gZkNl2Db4xcAsT_xOfOwRk-2DPHL) | [Contributing](https://github.com/isledecomp/isle-portable/blob/master/CONTRIBUTING.md) | [Matrix](https://matrix.to/#/#isledecomp:matrix.org) | [Forums](https://forum.mattkc.com/viewforum.php?f=1) | [Patreon](https://www.patreon.com/mattkc)

## Upstream's description

The rest of this section is upstream's own description of isle-portable, lightly adjusted for
this repository; "we" and "our" refer to the isledecomp project.

This initiative is a portable version of LEGO Island (Version 1.1, English) based on the [decompilation project](https://github.com/isledecomp/isle). Our primary goal is to transform the codebase to achieve platform independence, thereby enhancing compatibility across various systems while preserving the original game's experience as faithfully as possible.

Please note: this project is primarily dedicated to achieving platform independence without altering the core gameplay or rewriting code for improvement's sake. While those are worthwhile objectives, they are not within the scope of this project. `isle-portable` offers support for light modding using [`extensions`](/extensions). 

### Status

The badges show this repository's CI, which builds every platform upstream does.

| Platform | Status |
| - | - | 
| Windows (x86, x64, arm64) | [![CI](https://github.com/mjkoo/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/mjkoo/isle-portable/actions/workflows/ci.yml) | 
| MacOS (arm64, x64) | [![CI](https://github.com/mjkoo/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/mjkoo/isle-portable/actions/workflows/ci.yml) |
| Linux (x64, arm64) | [![CI](https://github.com/mjkoo/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/mjkoo/isle-portable/actions/workflows/ci.yml) |
| FreeBSD (x64) | [![CI](https://github.com/mjkoo/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/mjkoo/isle-portable/actions/workflows/ci.yml) |
| DOS (x86) | [![CI](https://github.com/mjkoo/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/mjkoo/isle-portable/actions/workflows/ci.yml) |
| [Web](https://isle.pizza) | [![CI](https://github.com/mjkoo/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/mjkoo/isle-portable/actions/workflows/ci.yml) |
| iOS | [![CI](https://github.com/mjkoo/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/mjkoo/isle-portable/actions/workflows/ci.yml) |
| Android | [![CI](https://github.com/mjkoo/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/mjkoo/isle-portable/actions/workflows/ci.yml) |
| Xbox One | [![CI](https://github.com/mjkoo/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/mjkoo/isle-portable/actions/workflows/ci.yml) |
| Playstation Vita | [![CI](https://github.com/mjkoo/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/mjkoo/isle-portable/actions/workflows/ci.yml) |
| Nintendo Switch | [![CI](https://github.com/mjkoo/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/mjkoo/isle-portable/actions/workflows/ci.yml) |
| Nintendo 3DS | [![CI](https://github.com/mjkoo/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/mjkoo/isle-portable/actions/workflows/ci.yml) |

We are actively working to support more platforms. If you have experience with a particular platform, we encourage you to contribute to `isle-portable`. You can find a [list of ongoing efforts](https://github.com/isledecomp/isle-portable/wiki/Work%E2%80%90in%E2%80%90progress-ports) in our Wiki.

### Usage

**An existing copy of LEGO Island is required to use this project.**

This repository releases the Android build only. For other platforms, upstream's builds in its [Releases tab](https://github.com/isledecomp/isle-portable/releases/tag/continuous) are mainly for developers; as such, they may not work properly for all end-users. If you are technically inclined, you may find it easiest to compile the project yourself to get it running at this current point in time.

[Installation instructions](https://github.com/isledecomp/isle-portable/wiki/Installation) for some ports can be found in our Wiki.

### Library substitutions

To achieve our goal of platform independence, we need to replace any Windows-only libraries with platform-independent alternatives. This ensures that our codebase remains versatile and compatible across various systems. The following table serves as an overview of major libraries / subsystems and their chosen replacements. For any significant changes or additions, it's recommended to discuss them with the team on the Matrix chat first to ensure consistency and alignment with our project's objectives.

| Library/subsystem | Substitution | Status | |
| - | - | - | - |
| Window, Events | [SDL3](https://www.libsdl.org/) | ✅ | [Remarks](https://github.com/search?q=repo%3Aisledecomp%2Fisle-portable+%22%2F%2F+%5Blibrary%3Awindow%5D%22&type=code) |
| Windows Registry (Configuration) | [libiniparser](https://gitlab.com/iniparser/iniparser) | ✅ | [Remarks](https://github.com/search?q=repo%3Aisledecomp%2Fisle-portable+%22%2F%2F+%5Blibrary%3Aconfig%5D%22&type=code) |
| Filesystem | [SDL3](https://www.libsdl.org/) | ✅ | [Remarks](https://github.com/search?q=repo%3Aisledecomp%2Fisle-portable+%22%2F%2F+%5Blibrary%3Afilesystem%5D%22&type=code) |
| Threads, Mutexes (Synchronization) | [SDL3](https://www.libsdl.org/) | ✅ | [Remarks](https://github.com/search?q=repo%3Aisledecomp%2Fisle-portable+%22%2F%2F+%5Blibrary%3Asynchronization%5D%22&type=code) |
| Keyboard/Mouse, DirectInput (Input) | [SDL3](https://www.libsdl.org/) | ✅ | [Remarks](https://github.com/search?q=repo%3Aisledecomp%2Fisle-portable+%22%2F%2F+%5Blibrary%3Ainput%5D%22&type=code) |
| Joystick/Gamepad, DirectInput (Input) | [SDL3](https://www.libsdl.org/) | ✅ | [Remarks](https://github.com/search?q=repo%3Aisledecomp%2Fisle-portable+%22%2F%2F+%5Blibrary%3Ainput%5D%22&type=code) |
| WinMM, DirectSound (Audio) | [SDL3](https://www.libsdl.org/), [miniaudio](https://miniaud.io/) | ✅ | [Remarks](https://github.com/search?q=repo%3Aisledecomp%2Fisle-portable+%22%2F%2F+%5Blibrary%3Aaudio%5D%22&type=code) |
| DirectDraw (2D video) | [SDL3](https://www.libsdl.org/) | ✅ | [Remarks](https://github.com/search?q=repo%3Aisledecomp%2Fisle-portable+%22%2F%2F+%5Blibrary%3A2d%5D%22&type=code) |
| [Smacker](https://github.com/isledecomp/isle/tree/master/3rdparty/smacker) | [libsmacker](https://github.com/foxtacles/libsmacker) | ✅ | [Remarks](https://github.com/search?q=repo%3Aisledecomp%2Fisle-portable%20%22%2F%2F%20%5Blibrary%3Alibsmacker%5D%22&type=code) |
| Direct3D (3D video) | [SDL3 (Vulkan, Metal, D3D12)](https://www.libsdl.org/), D3D9, OpenGL 1.1, OpenGL ES 2.0, OpenGL ES 3.0, Software | ✅ | [Remarks](https://github.com/search?q=repo%3Aisledecomp%2Fisle-portable+%22%2F%2F+%5Blibrary%3A3d%5D%22&type=code) |
| Direct3D Retained Mode | Custom re-implementation | ✅ | [Remarks](https://github.com/search?q=repo%3Aisledecomp%2Fisle-portable+%22%2F%2F+%5Blibrary%3Aretained%5D%22&type=code) |
| [SmartHeap](https://github.com/isledecomp/isle/tree/master/3rdparty/smartheap) | Default memory allocator | - | - |

## Android settings

On Android, `isle.ini` and the default `saves/` directory live in the app's private internal
files directory. Imported game data lives in the app-scoped external files directory under
`Android/data/dev.mjkoo.isle/files/`. Removing imported data in the app leaves saves and
settings intact. Android backup rules include the internal saves and config, but exclude game
data; backup and restore depend on the device's backup settings and service. An explicit
`savepath` override is honored, but saves outside the default directory are not covered by
these rules. Uninstalling removes both app-specific storage directories.

The releases page carries one APK per CPU architecture plus a universal one; see
[installing the Android build](docs/android-install.md) for which to take.

The Android app launches directly into the game. Tap the menu button, in the upper-right
corner by default, or use Android's Back button or gesture, to open Resume / Settings / Quit.
The menu names the signed-in player, or says that nothing is saved until someone signs in at the
Information Center; the game saves nothing before then. Quit reads **Quit anyway** when the
menu-opening save attempt reports failure. Backgrounding the app can attempt another
save while the menu is open, and shutdown attempts one more without checking its result.
Reopen the menu to retry the reported save; no message guarantees that a file was written.
See [Android saves](docs/android-saves.md) for save timing and transfers.

Settings exposes touch schemes, touch button size and opacity, cursor sensitivity, haptics,
WASD, audio, render resolution, available renderers, filtering, graphics quality, controller
buttons and extensions. Choose **Save**: touch scheme, Show touch controls, button size,
opacity and controller buttons apply when you resume, and other changes apply after quitting
and launching the game again.
**Cancel** discards edits. **Reset these settings** restores defaults for these controls when
saved; it keeps game paths, saves, touch button positions, controller buttons and extension
configuration. **Input > Edit touch layout** moves the menu,
Space and Esc buttons over the paused game. A startup error also offers Settings so an
unsuitable display configuration can be reset.

**Data > Game files** shows where the game reads its files from and how much space they take.
**Replace game files** copies a folder you pick beside the current files and puts it in place
the next time the game starts; **Remove game files** deletes the copy in app storage, keeping
saves and settings, and the game asks for a folder again when it next starts. Both close the
game, and the current files stay playable until then. See
[Android game files](docs/android-game-files.md).

The game holds the system's audio focus while it is in front, so it stops music another app was
playing, and it turns down or falls silent when something else needs the sound rather than
pausing. Pausing the game is separate and silences it on every platform, this one included. See
[Android audio](docs/android-audio.md).

**Data > Export saves** writes a ZIP of the save files captured when the menu opened.
Choose a destination in the system picker; no storage permission is required. Export
preserves staged settings edits and excludes config and game assets. See
[exporting Android saves](docs/android-saves.md) for desktop transfer and limitations.

Render resolution sets the game content quality independently of the screen size. Android
keeps the game and touch coordinates at 640 × 480, scales the rendered image to fit the
screen, and preserves its aspect ratio. GLES render targets are reduced proportionally when
needed to fit the GPU's limits. The default content resolution is 640 × 480.

**Display > Renderer** chooses the 3D backend, on the next launch. OpenGL ES 3.0 is the
default; SDL3 GPU HAL is the Vulkan path, and choosing it is what causes the game to build a
window Vulkan can draw into, since a window made for OpenGL cannot be handed to Vulkan
afterwards. The list offers every renderer this build supports, not only the ones the current
window can initialize, so the way back is always there. If the chosen one cannot start, the game
says so and offers Settings. See [Android renderers](docs/android-renderers.md).

**Graphics** sets model and texture quality, level of detail, the maximum number of actors,
the screen transition and a frame rate limit, a subset of the desktop configuration tool's
options; **Display** adds the lighting model and wide view angle. They apply on the next
launch; lower quality, detail and actor settings ease the load on slower devices. The frame
rate limit caps how often the game draws, at 30, 60 or 90 fps, 90 being the game's default.
Options the desktop tool marks broken, Low model quality and two transition types, are not
offered.

**Input > Show touch controls** enables movement hints, on by default. Arrow-key regions
show direction markers and highlight held directions. Virtual stick shows an indicator at
the first finger's starting point, with its marker reflecting the current movement axes.
These hints do not intercept touches or change how game objects are clicked. They hide while
using physical controls and return on touch. See [Android touch controls](docs/android-touch-controls.md)
for region boundaries and gesture behavior.

**Controller** chooses what each gamepad button does; Start opens the game menu by default, so
a controller alone can reach Settings. See [Android controllers](docs/android-controllers.md).

On Android TV the game is listed in the TV launcher and is best played with a controller; a
remote's Back opens the game menu, and the touch controls are hidden. Where the TV has no folder
picker, the first launch says where to copy the game files with adb. See
[Android TV](docs/android-tv.md).

**Extensions** turns on the parts that are not in the original game: custom textures, extra SI
files, a third person camera and multiplayer. All are off by default and apply on the next
launch. The texture folder and the SI folder name a place inside the game files, so Settings
lists the folders it finds there rather than asking for a path; add your own to the folder you
select in Replace game files. Multiplayer connects to the relay server and room you name and
turns the third person camera on. **Reset extensions** turns them all off, and Reset these
settings leaves them alone. See [Android extensions](docs/android-extensions.md).

**About**, at the end of Settings, shows the version, credits isle-portable and the
decompilation this build is based on, and links to the license, the source of this build and the
AI disclosure above.

Configuration (`isle.ini`) and default saves (`saves/`) live in private internal storage.
Imported game assets remain in app-scoped external storage. Settings preserves custom paths
and other INI keys. Android backup eligibility does not guarantee that a backup or restore
will occur.

## Building

This project uses the [CMake](https://cmake.org/) build system, which allows for a high degree of versatility regarding compilers and development environments. Please refer to the [GitHub action](/.github/workflows//ci.yml) for guidance.

## Contributing

Issues and pull requests about this fork's Android work belong here. Work on the game port
itself, on other platforms or on the decompilation belongs upstream, in
[isle-portable](https://github.com/isledecomp/isle-portable) or the
[decompilation project](https://github.com/isledecomp/isle); see their
[CONTRIBUTING](/CONTRIBUTING.md) page, which is upstream's.
