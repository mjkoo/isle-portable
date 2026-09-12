# Android configuration tests

Save snapshot tests cover recognized filenames, immutable bytes, incomplete sets,
case collisions on case-sensitive volumes, symlinks, nonregular files, read failures
and size limits. Recovery-path tests cover defaults, overrides and invalid config.

The archive/stream tests run on Java 17 without an Android device:

```sh
mkdir -p build/save-export-java-tests
javac -d build/save-export-java-tests \
  CONFIG/android/src/main/java/org/legoisland/isle/SaveArchive.java \
  CONFIG/android/src/main/java/org/legoisland/isle/SaveExportJournal.java \
  tests/android/java/org/legoisland/isle/SaveArchiveTest.java \
  tests/android/java/org/legoisland/isle/SaveExportJournalTest.java
java -cp build/save-export-java-tests org.legoisland.isle.SaveArchiveTest
java -cp build/save-export-java-tests org.legoisland.isle.SaveExportJournalTest
```

Run these commands inside `nix develop .#android` if Java is unavailable. They cover
archive contents and CRC validation, invalid names, limits, cancellation, failed
destination writes and failed stream closure. Journal tests cover failed completion
persistence followed by process restart, repeated recovery, preservation of completed
and partial output, and failed writes before export starts.

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

## Save restore

The `saverestore` native target exercises complete replacement, original-name and
absence preservation, custom destination identity, previous-save consumption,
empty originals, symlink rejection, corrupt journals, interrupted scheduling and
interrupted install/rollback. Filesystem checkpoints inject failure after writes,
syncs, publication and individual replacement operations. Reopening must expose
one complete generation and must not replay committed installation over new saves.
The test can also run as an Android native executable with TMPDIR set to a writable
directory. It does not need assets or alter the installed game's saves.

Standalone Java validation tests cover variable names and command syntax, numeric
bounds, and vehicle part counts in addition to archive structure. Retail jetski,
copter, dune car and race car builds have 9, 15, 8 and 11 placeable parts respectively;
each count is shared by all three build animation variants. Test both the completed
count and the first invalid count to preserve valid progress while rejecting unsafe
array indexes. Missing-directory recovery tests also verify that an unavailable
previous backup does not prevent selecting a new ZIP.

Standalone Java validation tests use the same JDK as the Android build:

```sh
mkdir -p build/android-restore-java
javac -d build/android-restore-java \
  CONFIG/android/src/main/java/org/legoisland/isle/SaveValidation.java \
  CONFIG/android/src/main/java/org/legoisland/isle/SaveRestoreArchive.java \
  tests/android/java/org/legoisland/isle/SaveValidationTest.java
java -ea -cp build/android-restore-java org.legoisland.isle.SaveValidationTest
```

Startup-gate tests exercise destruction before work starts, during native recovery,
and after worker completion with the UI callback still queued. An abandoned gate
must remain pending while native work runs, then close without draining the UI
queue. Normal startup still waits for the UI to handle the result or retry.

```sh
javac -d build/android-restore-java \
  android-project/app/src/main/java/org/legoisland/isle/SaveRestoreGate.java \
  tests/android/java/org/legoisland/isle/SaveRestoreGateTest.java
java -ea -cp build/android-restore-java org.legoisland.isle.SaveRestoreGateTest
```

Native tests also cover abandoned journal writes with and without a previous backup,
interrupted cleanup, and preservation of both journal files when published state is
corrupt. Startup removes an unreferenced temporary write only after establishing
valid authoritative state.

For device acceptance, preserve the device's config and saves first. Transfer a
newly registered player's export between independent Android installations through
the document picker, confirm replacement, and reopen. Verify player identities,
selected character and a concrete progress marker. Check exact archive bytes before
selecting a player, because selecting a player changes slot ordering and saves.
Exercise Restore previous saves and verify original spelling and absent slots too.
Test confirmation cancellation, incomplete archives, settings drafts, recreation,
process death with the picker open, startup-error access and minified release JNI.
Use an isolated custom save directory for destructive storage-failure scenarios.
Fixture-only and same-directory checks do not establish a real progress transfer.

## Touch movement feedback

The native `touchmovement` target exercises the same movement helpers used by the
input manager: letterbox admission, region boundaries, multiple arrow fingers,
neutral movement outside the viewport, stick ownership and per-axis clamping,
release outside the viewport, and cancellation without reacquisition on motion.
Configuration tests cover the visibility boolean and its scoped reset.

Manually exercise Show touch controls in both movement schemes on GLES3, software
and palette-software. Check viewport alignment, edge clipping, held-direction
highlights, first-finger ownership, additional fingers and unchanged object clicks
and dragging. Change resolution and rotate. Pause or background while holding a
gesture, return and confirm it stays neutral until a new touch. Check physical-input
hiding and touch restoration, Settings Save/Cancel/reset and minified release JNI.
Preserve original device config and saves before testing, and restore them afterward.

## Touch action buttons

Connect an Android device or start the existing emulator with
`just android-emulator`, then run `just android-test`. The dedicated `touchTest`
build uses `org.legoisland.isle.touchtest`, so Gradle's test installation and
uninstallation do not affect the normal app or its data. AndroidJUnit4 tests exercise
the production button with real `MotionEvent` objects on the UI thread. They do not
launch the game, load game assets, or modify config and saves.

The tests cover ordinary taps, batched movement outside and back inside the target,
inside-only history, extra-pointer movement, whole-gesture cancellation and generation
changes while held. On API 33 and newer, they also cover cancelled owner releases and
cancelled extra pointers. These tests supplement the native `touchactions` queue
tests; renderer, lifecycle and simultaneous surface/button integration still need
the device checks described above.

## Runtime touch settings

Configuration tests cover live update coalescing, partial changes, reset defaults,
failed writes and retry, unrelated saves, path isolation and engine-session cleanup.
The `touchinput` target covers contact ownership, late motion/releases after
cancellation, independent touch devices, and double-tap history across cancellation.

On device, save each of the four touch schemes and Resume without restarting the
process. Check movement and overlay behavior, visibility, Cancel, reset/save,
reset/cancel, failed writes and retry, repeated saves before Resume, and persistence
after relaunch. Hold movement or an action while opening the menu, and verify stale
releases do nothing after Resume. Check backgrounding and activity recreation;
audio/display settings saved alongside touch settings must remain next-launch only.
Run against GLES3, software and palette-software, including minified release.

## Touch layout

Configuration tests cover the button size, control opacity and button position keys:
range edges, malformed and comma-decimal positions, removal, and that saving them
publishes no live touch update. Standalone Java tests cover the layout geometry:
default placement identical to the original fixed layout across densities and insets,
clamping inside the safe area at every size, stored centers reproducing their boxes,
per-field fallback for unusable values, locale-independent formatting and writing only
changed positions.

```sh
mkdir -p build/android-touch-java
javac -d build/android-touch-java \
  android-project/app/src/main/java/org/legoisland/isle/TouchLayout.java \
  tests/android/java/org/legoisland/isle/TouchLayoutTest.java
java -ea -cp build/android-touch-java org.legoisland.isle.TouchLayoutTest
```

The instrumented tests (`just android-test`) lay out the production button layer
beside the original fixed RelativeLayout rules and require identical bounds, then
cover inset changes, window insets, buttons larger than the safe area, size, opacity,
the menu icon's placement, touch pass-through, and later fingers reaching the game
while a button is held. Editor tests cover single and two-finger drags, a second finger
on a held button, the touch slop, batched moves, cancellation, rejected lifts, focus
loss, safe-area and appearance changes during a drag, toolbar release and excursions,
a button lying over Done, Reset during a drag, Done with an unfinished drag, the saving
state, safe-area clamping, large default buttons starting clear of the toolbar, and
the editor drawing each button pixel for pixel as the game draws it. A window test
attaches the layer to an activity built only into the touchTest APK and requires every
draw after a resize to show the new placement. Controller tests use a fake config file:
the first read reported once and later reads in order, an unreadable layout still
showing the buttons, the editor waiting for the first read, a failed save kept for a
retry, Back ignored while saving and honored only on a real release, and results after
the activity is destroyed dropped.

On device, open Menu > Settings > Edit touch layout and check Done, Back, Reset then
Done, Done without changes, the unsaved-settings toast, and a failed save (make
`isle.ini.new` a directory) followed by a retry. Check Developer options > Display
cutout variants in both landscape rotations, gesture and three-button navigation,
transient system bars, 16:9, 20:9 and 4:3 shapes, and each size and opacity in both
movement schemes. Press Home during a drag and kill the process while editing: the
game must stay paused, stale touches must do nothing after Resume, and edit mode must
never return on relaunch. With Don't keep activities, opening Settings ends the game
activity (SDL finishes it once the game's main loop exits), so leaving Settings, even
through Edit touch layout, returns to the launcher: nothing may crash, saved settings
must apply on the next launch, and the editor must not open. A controller does nothing
in the editor. Check reading and saving the layout on the minified release APK as well.

## Controller bindings

Host tests cover the `[gamepad]` binding table every platform reads: defaults identical to
the fixed mapping it replaced for every input, button label and platform, case-insensitive
values, invalid values falling back to their defaults, the confirm choice, and releases
acting on what their press did across label changes, rebinding, cancellation, trigger dead
zones and clicks held by another source. When the desktop build's SDL3 library is present,
`gamepad_labels` also checks against SDL which layouts label East as A. Both run with the
configuration tests above, which also cover the controller keys: validation, creating the
`[gamepad]` section in a configuration without one, and publishing saved bindings for Resume
only after a successful write to the running game's configuration.

Standalone Java tests cover the Settings rows: their order, the default shown for each confirm
choice, lowercase handling of hand-edited values and the warning when no button opens the menu.

```sh
mkdir -p build/android-controller-java
javac -d build/android-controller-java \
  CONFIG/android/src/main/java/org/legoisland/isle/ControllerBindings.java \
  tests/android/java/org/legoisland/isle/ControllerBindingsTest.java
java -ea -cp build/android-controller-java org.legoisland.isle.ControllerBindingsTest
```
