# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project scope

`isle-portable` is a portable fork of the [LEGO Island decompilation](https://github.com/isledecomp/isle).
The goal is platform independence, not improvement of the game code. Two rules follow from that and
shape almost every decision here:

- Changes under `LEGO1/` are only acceptable when they directly serve platform compatibility. The
  decompilation project upstream is the source of truth; keep structural/layout churn minimal so its
  changes keep merging cleanly. Gameplay changes and refactors-for-cleanliness belong nowhere, or in
  `extensions/`.
- `LEGO1/` carries decomp annotations (`// FUNCTION: LEGO1 0x100abcd0`, `// VTABLE:`, `// GLOBAL:`,
  `// SYNTHETIC:`, `// STRING:`, `// TEMPLATE:`, `// LIBRARY:`). Preserve them and their placement
  directly above the entity they annotate. `// [library:audio]`-style markers flag places where a
  Windows API was substituted; they are grep targets for porting work, keep them.

## Build

CMake, C++17. The only tests are host tests under `tests/android` (see Test below). The
[CI workflow](.github/workflows/ci.yml) is the authoritative reference for every platform's
configure line.

```sh
# Desktop build (downloads SDL3 + iniparser via FetchContent by default)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
# Produces build/isle and, when Qt6 is found, build/isle-config
```

Useful options (all in the top-level `CMakeLists.txt`, most are `cmake_dependent_option`):

- `-DDOWNLOAD_DEPENDENCIES=OFF` - use system SDL3/iniparser/libweaver via `find_package` instead of
  FetchContent. Add search paths with `-DCMAKE_PREFIX_PATH=...`. iniparser is pinned; a build tree
  that fetched it before the pin keeps the old checkout, because `UPDATE_DISCONNECTED` will not
  move it, and fails with `'version.h' file not found`. Delete `_deps/iniparser-*` in that tree.
- `-DISLE_SDL3_REVISION=<ref>` / `-DISLE_SDL3_REPOSITORY=<url>` - SDL3 is pinned repo-wide in
  `CMake/SDL3Revision.cmake`, which `android-project/downloadSDL3.cmake` and
  `android-project/app/build.gradle` read too, so the desktop build and the APK cannot disagree
  about it. Neither is a cache entry, so a bump lands on the next configure in every tree; passing
  either on the command line does create one, which is what makes an override stick until it is
  deleted from `CMakeCache.txt`. Gradle forwards the revision only, spelled
  `-PisleSdl3Revision=<ref>`; it always takes the repository from the pin file.
- `-DISLE_BUILD_CONFIG=OFF` - skip `isle-config` (the Qt6 settings GUI) if Qt6 is unavailable.
- `-DISLE_WERROR=ON` - what CI uses on most platforms.
- `-DENABLE_CLANG_TIDY=ON` - CI runs clang-tidy on the Linux and mingw64 jobs only.
- `-DISLE_ASAN=ON` / `-DISLE_UBSAN=ON`.
- `-DISLE_BUILD_ASSETS=ON` - build the `.si` assets under `assets/` (needs `git lfs pull` and libweaver).
- `-DISLE_USE_DX5=ON` - 32-bit MSVC only; builds against the real DirectX 5 SDK and disables miniwin.
- `-DISLE_COMPILE_SHADERS=ON` - requires `shadercross` on PATH and Python 3.12; otherwise the
  pre-generated shader headers are used.
- Renderer backends are opted in/out in `miniwin/CMakeLists.txt` (`ISLE_RENDERER_SOFTWARE`,
  `ISLE_RENDERER_SDLGPU`, and per-platform blocks).

Cross-compiles are driven entirely by toolchain files / SDK env, e.g.
`--toolchain CMake/i586-pc-msdosdjgpp.cmake` (DOS), `emcmake cmake ...` (web),
`-DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/3DS.cmake`, `-DCMAKE_SYSTEM_NAME=iOS`. Android builds
through Gradle in `android-project/` (`./gradlew assembleDebug -PcmakeArgs="..."`), writing one APK
per ABI plus a universal one into `app/build/outputs/apk/debug/`; `versionCode` is the commit
count, so it wants a full clone or `-PisleVersionCode=<n>`. Packaging lives in `packaging/` and
runs through CPack.

Running the game requires an existing copy of LEGO Island 1.1 (English); paths are configured in
`isle.ini` next to the executable or under `SDL_GetPrefPath("isledecomp", "isle")`.

## Test

`tests/android` is its own CMake project of host tests: C++ tests of `ISLE`, `util` and
`miniwin` helpers, plus plain-Java tests of the Android app's policy classes, all run by ctest.
They build against the SDL3 and iniparser a desktop build fetched, so build `build/` first, then
`just host-test` (which uses `nix develop` for JDK 17). CI runs them all on the Linux row with
`-DISLE_HOST_TESTS_REQUIRE_ALL=ON`, and `ini_file` alone on msys2. A new test is appended to
`tests/android/CMakeLists.txt`, which is kept in append order.

## Lint

```sh
# Formatting - CI fails on any diff. Note the pin: clang-format 17.x, not the system one.
find CONFIG LEGO1 ISLE miniwin -iname '*.h' -o -iname '*.cpp' | \
  xargs pipx run "clang-format>=17,<18" --style=file -i

# Naming conventions (LEGO1 only) - needs libclang 16.x
pip install -r tools/requirements.txt
python3 tools/ncc/ncc.py --clang-lib <path>/libclang.so --recurse \
  --style tools/ncc/ncc.style --skip tools/ncc/skip.yml ... # see the `ncc` job in ci.yml
```

`.editorconfig` is authoritative for indentation: tabs in `.cpp`/`.h`, 4 spaces in Python, 2 in CMake.

## Architecture

### Layers, bottom to top

- `miniwin/` - the portability shim. Reimplements the Windows/DirectX surface the game calls into
  (`windows.h`, `ddraw.h`, `dinput.h`, `d3d.h`, `d3drm.h`) on top of SDL3. Used whenever
  `ISLE_MINIWIN` is on, which is everywhere except 32-bit MSVC + DX5. Public headers in
  `miniwin/include/miniwin/`, implementations in `miniwin/src/{windows,ddraw,d3drm}`, private
  implementation headers in `miniwin/src/internal/`.
- `miniwin/src/d3drm/backends/` - the actual 3D renderers, all implementing the
  `Direct3DRMRenderer` interface in `src/internal/d3drmrenderer.h`: `sdl3gpu`, `opengl1`,
  `opengles2`, `opengles3`, `directx9`, `citro3d` (3DS), `gxm` (Vita), `glide` (DOS/Voodoo),
  `software`, `palettesw`. Each is compiled in behind a `USE_*` define and selected at runtime by
  GUID in `d3drmrenderer.cpp` (`CreateDirect3DRMRenderer` / `Direct3DRMRenderer_EnumDevices`).
  `Miniwin_GetDeviceCandidates`, in the same file, is the compiled-in list a settings screen can
  offer before any window exists; its names must match what each `*_EnumDevice` reports.
  Adding a backend means: a `renderer.cpp` under `backends/`, a `d3drmrenderer_*.h` in
  `src/internal/`, a GUID + branch in all three functions above, and a `target_sources` block in
  `miniwin/CMakeLists.txt`.
- `LEGO1/tgl/` - "Tgl", the game's own thin 3D abstraction. `tgl/d3drm/` implements it against
  Direct3D Retained Mode (so, against miniwin). This is decompiled code.
- `LEGO1/realtime/`, `LEGO1/viewmanager/` - math (matrix/vector), ROIs, LOD lists, view culling.
- `LEGO1/omni/` - Mindscape's in-house engine ("Omni"), file pattern `mx*`. Streaming/media engine:
  `mxds*` classes model the `.SI` data-stream file format, `mx*presenter` classes play its chunks,
  plus tickle-based managers for video, audio, events, notifications, and threading.
- `LEGO1/lego/` - the game itself. `legoomni/` is the LEGO Island-specific extension of Omni
  (world/act state machines, actors, path controllers, input, build mode, race mini-games);
  `sources/` holds the 3D manager, animation, geometry, ROI and shape support code.
- `ISLE/isleapp.cpp` - `main()`, SDL window/event loop, `isle.ini` parsing via iniparser, and the
  bridge into `LEGO1`. Per-platform config and hooks are in `ISLE/{3ds,android,emscripten,ios,switch,vita,xbox_one_series}/`.
- `CONFIG/qt/` - the `isle-config` settings GUI (Qt6). `CONFIG/vita/` is its Vita counterpart.

`lego1` is built as a shared library where the platform allows it, static otherwise
(`BUILD_SHARED_LIBS`); anything crossing that boundary needs `LEGO1_EXPORT` from `LEGO1/lego1_export.h`.

### Extensions

`extensions/` is the only sanctioned place for behavior that is not in the original game (texture
loader, SI loader, third person camera, multiplayer). Everything is gated: an extension is a type
with a static `enabled` flag, registered in `availableExtensions` in
`extensions/include/extensions/extensions.h`, enabled at runtime from `isle.ini`, and invoked from
`LEGO1` through `Extensions::Extension<T>::Call(...)`, which compiles to nothing when `EXTENSIONS`
is undefined. Keep the call sites in `LEGO1` to that one-line shape; put the logic in `extensions/`.

### Compatibility headers

`util/compat.h` (compiler/platform macro shims) and `util/decomp.h` (`DECOMP_SIZE_ASSERT`, the
`undefined`/`undefined2`/`undefined4` Ghidra types) are included pervasively by `LEGO1`. Size
asserts are only active in release builds, since the release `LEGO1.DLL` is the decompilation target.

### File formats

`docs/*.ksy` are Kaitai Struct definitions for the game's binary formats (`.SI` interleaf data is
handled by libweaver; saves, `.wdb`, `.dta`, `.tex`, `.ani` are documented here). Useful when
debugging asset loading.
