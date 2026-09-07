# Android configuration tests

Save snapshot tests cover recognized filenames, immutable bytes, incomplete sets,
case collisions on case-sensitive volumes, symlinks, nonregular files, read failures
and size limits. Recovery-path tests cover defaults, overrides and invalid config.

The archive/stream tests run on Java 17 without an Android device:

```sh
mkdir -p build/save-export-java-tests
javac -d build/save-export-java-tests \
  CONFIG/android/src/main/java/org/legoisland/isle/SaveArchive.java \
  tests/android/java/org/legoisland/isle/SaveArchiveTest.java
java -cp build/save-export-java-tests org.legoisland.isle.SaveArchiveTest
```

Run these commands inside `nix develop .#android` if Java is unavailable. They cover
archive contents and CRC validation, invalid names, limits, cancellation, failed
destination writes and failed stream closure.

For export runtime validation, exercise Settings > Data > Export saves on both a
debug and minified release APK. Check empty/partial sets, registered-player progress,
custom paths and startup recovery. Cancel the picker, rotate Settings and the picker,
inject trim events, and kill the background process with the picker open. The latter
must report interruption and must not write an old snapshot after relaunch. Preserve
the original config/save bytes and restore them after destructive test setup. Extract
an exported registered-player set on desktop and verify it loads; fixture hashes
alone are insufficient evidence of that round trip.

These host tests exercise the native configuration store without Android or SDL. They cover
unrelated-key preservation, absent defaults, scoped resets, concurrent updates, failed writes,
malformed files and settings validation. Render-target tests also cover proportional fitting
to GPU limits, portrait/landscape targets, asymmetric limits and invalid dimensions. A separate
check ensures HRESULT remains signed and 32-bit so failures are recognized on 64-bit hosts.

After building the desktop project with its fetched iniparser dependency:

```sh
cmake -S tests/android -B build/android-config-tests \
  -DINIPARSER_INCLUDE_DIR="$PWD/build/_deps/iniparser-src/src" \
  -DINIPARSER_LIBRARY="$PWD/build/_deps/iniparser-build/libiniparser.a"
cmake --build build/android-config-tests
ctest --test-dir build/android-config-tests --output-on-failure
```

Alternatively, provide an installed iniparser through CMake's search paths. On this project's
Nix setup, run the commands inside `nix develop`.

The controls and lifecycle integration also need Android verification: menu-button and Back
access, Save/Cancel, next-launch application, renderer switching, startup-error recovery,
activity recreation, background/foreground, low memory and held input. Check the release
shrinker output as well as the debug APK because the settings bridge is reached through JNI.

For process restoration, leave the Cursor sensitivity dialog open, press Home, terminate
only the background app process, then select its card in Recents. Verify that the dialog
restores without crashing, accepts an edit, and that Save persists it. Repeat with a list
preference open. Launching the root activity directly does not exercise the same restoration.

For resolution, compare 640 × 480 and 1280 × 960 after a full game relaunch on each Android
renderer. Verify the logged render-target dimensions change, the image fills the same screen
area without distortion, and taps hit the same game controls. Include GLES3 with MSAA enabled,
landscape/portrait rotation, Save/Cancel and resetting to game defaults.

When checking limited GPUs, test a 1280 x 960 content preset in portrait with a 2048-pixel
renderbuffer limit. Both target dimensions must remain within the limit, and the displayed
image and touch targets must keep their alignment. Check GLES2 texture limits as well as
renderbuffer and viewport limits. Inject a framebuffer allocation failure after renderer
probing and verify startup offers Settings instead of continuing with an incomplete target.
