# LEGO Island, portable

[Development Vlog](https://www.youtube.com/playlist?list=PLbpl-gZkNl2Db4xcAsT_xOfOwRk-2DPHL) | [Contributing](/CONTRIBUTING.md) | [Matrix](https://matrix.to/#/#isledecomp:matrix.org) | [Forums](https://forum.mattkc.com/viewforum.php?f=1) | [Patreon](https://www.patreon.com/mattkc)
  
This initiative is a portable version of LEGO Island (Version 1.1, English) based on the [decompilation project](https://github.com/isledecomp/isle). Our primary goal is to transform the codebase to achieve platform independence, thereby enhancing compatibility across various systems while preserving the original game's experience as faithfully as possible.

Please note: this project is primarily dedicated to achieving platform independence without altering the core gameplay or rewriting code for improvement's sake. While those are worthwhile objectives, they are not within the scope of this project. `isle-portable` offers support for light modding using [`extensions`](/extensions). 

## Status

| Platform | Status |
| - | - | 
| Windows (x86, x64, arm64) | [![CI](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml) | 
| MacOS (arm64, x64) | [![CI](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml) |
| Linux (x64, arm64) | [![CI](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml) |
| FreeBSD (x64) | [![CI](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml) |
| DOS (x86) | [![CI](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml) |
| [Web](https://isle.pizza) | [![CI](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml) |
| iOS | [![CI](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml) |
| Android | [![CI](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml) |
| Xbox One | [![CI](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml) |
| Playstation Vita | [![CI](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml) |
| Nintendo Switch | [![CI](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml) |
| Nintendo 3DS | [![CI](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/isle-portable/actions/workflows/ci.yml) |

We are actively working to support more platforms. If you have experience with a particular platform, we encourage you to contribute to `isle-portable`. You can find a [list of ongoing efforts](https://github.com/isledecomp/isle-portable/wiki/Work%E2%80%90in%E2%80%90progress-ports) in our Wiki.

## Usage

**An existing copy of LEGO Island is required to use this project.**

As it stands, builds provided in the [Releases tab](https://github.com/isledecomp/isle-portable/releases/tag/continuous) are mainly for developers; as such, they may not work properly for all end-users. Work is currently ongoing to create workable release builds ready for gameplay and general use by end-users. If you are technically inclined, you may find it easiest to compile the project yourself to get it running at this current point in time.

[Installation instructions](https://github.com/isledecomp/isle-portable/wiki/Installation) for some ports can be found in our Wiki.

On Android, `isle.ini` and the default `saves/` directory live in the app's private internal
files directory. Imported game data lives in the app-scoped external files directory under
`Android/data/org.legoisland.isle/files/`. Removing imported data in the app leaves saves and
settings intact. Android backup rules include the internal saves and config, but exclude game
data; backup and restore depend on the device's backup settings and service. An explicit
`savepath` override is honored, but saves outside the default directory are not covered by
these rules. Uninstalling removes both app-specific storage directories.

## Library substitutions

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

The Android app launches directly into the game. Tap the menu button, in the upper-right
corner by default, or use Android's Back button or gesture, to open Resume / Settings / Quit.

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

**Data > Export saves** writes a ZIP of the save files captured when the menu opened.
Choose a destination in the system picker; no storage permission is required. Export
preserves staged settings edits and excludes config and game assets. See
[exporting Android saves](docs/android-saves.md) for desktop transfer and limitations.

Render resolution sets the game content quality independently of the screen size. Android
keeps the game and touch coordinates at 640 × 480, scales the rendered image to fit the
screen, and preserves its aspect ratio. GLES render targets are reduced proportionally when
needed to fit the GPU's limits. The default content resolution is 640 × 480.

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
for region boundaries and gesture behavior. Renderer choices reflect what the device could
initialize at startup. Vulkan is not enabled by this settings screen.

**Controller** chooses what each gamepad button does; Start opens the game menu by default, so
a controller alone can reach Settings. See [Android controllers](docs/android-controllers.md).

**Extensions** turns on the parts that are not in the original game: custom textures, extra SI
files, a third person camera and multiplayer. All are off by default and apply on the next
launch. The texture folder and the SI files name a place inside the game files, so Settings
lists what it finds there rather than asking for a path; add your own to the folder you select
in Replace game files. Multiplayer connects to the relay server and room you name and turns the
third person camera on. **Reset extensions** turns them all off, and Reset these settings leaves
them alone. See [Android extensions](docs/android-extensions.md).

Configuration (`isle.ini`) and default saves (`saves/`) live in private internal storage.
Imported game assets remain in app-scoped external storage. Settings preserves custom paths
and other INI keys. Android backup eligibility does not guarantee that a backup or restore
will occur.

## Building

This project uses the [CMake](https://cmake.org/) build system, which allows for a high degree of versatility regarding compilers and development environments. Please refer to the [GitHub action](/.github/workflows//ci.yml) for guidance.

## Contributing

If you're interested in helping or contributing to this project, check out the [CONTRIBUTING](/CONTRIBUTING.md) page.
