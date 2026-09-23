# Android host tests

## Running

Every test here, native and Java, is a ctest test. After building the desktop project with its
fetched SDL3 and iniparser, `just host-test` runs them all inside `nix develop`, which provides
JDK 17. Without `just`:

```sh
cmake -S tests/android -B build/host-tests -G Ninja \
  -DINIPARSER_INCLUDE_DIR="$PWD/build/_deps/iniparser-src/src" \
  -DINIPARSER_LIBRARY="$PWD/build/_deps/iniparser-build/libiniparser.a"
cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure
```

Pass `-R` to run one suite; each section below names its own. Alternatively, provide an installed
iniparser through CMake's search paths. Without Java 17, the Java tests are skipped, and without
a built SDL3, `gamepad_labels` is; `-DISLE_HOST_TESTS_REQUIRE_ALL=ON` makes either a configure
error instead. The Java tests run from the repository root, since several read the C++ they
mirror by relative path.

CI runs the whole set on its Linux row with that option on, and `ini_file` alone on the msys2
row, the other suites using POSIX calls.

## Saves, configuration and Settings

Save snapshot tests cover recognized filenames, immutable bytes, incomplete sets,
case collisions on case-sensitive volumes, symlinks, nonregular files, read failures
and size limits. Recovery-path tests cover defaults, overrides and invalid config.

The archive/stream tests run on Java 17 without an Android device:

```sh
ctest --test-dir build/host-tests -R '^(android_save_archive|android_save_export_journal)$' --output-on-failure
```

They cover archive contents and CRC validation, invalid names, limits, cancellation,
failed destination writes and failed stream closure. Journal tests cover failed
completion persistence followed by process restart, repeated recovery, preservation of
completed and partial output, and failed writes before export starts.

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
to GPU limits, portrait/landscape targets, asymmetric limits and invalid dimensions, and the
pixel format a render target is read back through: the byte-array aliases that line up with the
GPU format names, the sRGB variants alongside their linear ones, and a refusal for anything the
mapping cannot describe rather than a reinterpretation. A separate check ensures HRESULT remains
signed and 32-bit so failures are recognized on 64-bit hosts.

The `ini_file` test covers the shared configuration writer in `util/inifile.h`, which both the
Android settings store and the desktop `isle-config` write through. It pins the line-length
arithmetic (the 30-character name padding, the doubling of backslashes and quotes, names longer
than the padding, section entries with no colon) and proves the limit is iniparser's real cliff
rather than a guess: a 987 character value reloads and a 988 character one makes the whole dumped
file refuse to load, both lengths written literally so that moving `kLineLimit` either way fails
the test. That matters because `ASCIILINESZ` lives in iniparser's `.c` file rather than a public
header, so nothing else in the tree would notice the library changing it. It also covers the replacement itself - a fresh file leaves no `.new` sibling,
an existing file is replaced, a blocked temporary leaves the previous file byte-identical, and a
rename that cannot land removes the temporary instead of leaving it to be mistaken for the
configuration. Two further cases cover what the desktop tool's merge rests on, without needing
Qt: that iniparser lowercases entries on the way in and on the way out, so a dialog writing
`isle:Music` lands in the slot the loader made for `music` rather than adding a second one, and
that a load-modify-save leaves `[gamepad]` and `[multiplayer]` untouched. A last case covers
finding a value that loaded but is too long to write back. It is written against
`std::filesystem` rather than `mkdtemp` so the same binary runs on msys2 and MSVC; the Windows
branch of the replacement is compiled by CI through `isle-config` and run by the msys2 row's
`ini_file` step.

The desktop tool's own merge has no host test, because reaching `CConfigApp::WriteRegisterSettings`
drags in Qt, the device enumerator and miniwin. Verify it by hand instead: build `isle-config` with
`-DISLE_BUILD_CONFIG=ON`, point it at a scratch copy with `--ini`, and confirm that saving keeps
every key the tool does not own - the `[gamepad]` section, the touch layout keys, `[multiplayer]`,
`si loader:si path` and `si loader:directives`, `mediapath`, `Cursor Sensitivity`, `Active in
Background`, `Show Touch Controls` and anything hand-added. Compare the files parsed, not as text:
the dumper normalizes case, order and quoting, so a textual diff is all noise.

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

Standalone Java validation tests:

```sh
ctest --test-dir build/host-tests -R '^android_save_validation$' --output-on-failure
```

Startup-gate tests exercise destruction before work starts, during native recovery,
and after worker completion with the UI callback still queued. An abandoned gate
must remain pending while native work runs, then close without draining the UI
queue. Normal startup still waits for the UI to handle the result or retry.

```sh
ctest --test-dir build/host-tests -R '^android_save_restore_gate$' --output-on-failure
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
build uses `dev.mjkoo.isle.touchtest`, so Gradle's test installation and
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
ctest --test-dir build/host-tests -R '^android_touch_layout$' --output-on-failure
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
the fixed mapping it replaced for every input, button label and platform (except Start, which
now opens the menu on Android), case-insensitive
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
ctest --test-dir build/host-tests -R '^android_controller_bindings$' --output-on-failure
```

A real pad is best on device. Without one, Android's `uinput` shell command registers a kernel
input device (no root needed on the API 35 emulator): a pad declared with Xbox 360 ids
(`045e:028e`) gets Android's own key layout and SDL opens it as a gamepad. `adb shell input
gamepad` cannot stand in, because SDL ignores Android's virtual input device. A pad declared with
Switch Pro ids produced no gamepad events on the API 35 emulator. Check every default with no
`[gamepad]` section; Start opening the menu once the game has started and doing nothing while it
starts; B closing the menu, the D-pad moving between its buttons and A choosing one; reaching
Settings and its Save settings row with the pad alone, and B leaving without saving; a rebind
applying on Resume; a button held across the menu releasing without a click; a trigger resting
part-way pressing once; and unplugging a pad with a button or trigger held ending the click.

## Graphics settings

Configuration tests cover the graphics keys: range edges, the plain decimal the game needs
for whole-number keys, the refused Low model quality and transition types, removal, and that
saving them publishes no live touch or controller update. Standalone Java tests read the
native validator's range table and check that every row is listed there, that every value
Settings offers is within its range, that both sides agree on which rows are whole numbers,
and that Reset these settings clears every row. They also check that values the game writes
as `%f` show as their listed entry and that whole-number values are shown as written:

```sh
ctest --test-dir build/host-tests -R '^android_graphics_settings$' --output-on-failure
```

On device, a fresh configuration must show each row's value as a listed entry rather than
"Current: 3.600000". A saved change must reach `isle.ini` without rewriting other keys, and
must apply only after relaunching. Compare screenshots of Medium and High model quality in
the Infocenter. Every transition uses the configured effect, and tapping the Infocenter's left
or right arrow starts one without registering a player: record that under two transitions. For
each frame rate limit, read the game surface's average and present-to-present histogram from
`dumpsys SurfaceFlinger --timestats` (`--latency` reports nothing for it on API 35). Reset
followed by Save removes the keys; a hand-edited value outside the list shows as its current
value and is not rewritten.

## Extensions

Configuration tests cover the extension keys: the enable flags, a texture folder and an SI
folder that must be rooted in the game files, free of the whitespace and commas the si loader
splits its own file list on and of the characters an unquoted hand-edited line would lose,
while leaving a non-ASCII folder name alone; the WebSocket relay address, the room and
character that must survive the ini round trip, the lighting model's exact values, removal of
each, and that a section is created for the options and reused afterwards without disturbing a
hand-edited `files` or `directives`, neither of which the validator accepts.

Separately, every value has to fit on one line of the configuration file. iniparser does not
skip an over-long line, it fails the whole load, and a configuration that will not load is
replaced with defaults, so one long value would cost every other setting. The limit is
asserted by writing a value at it and reading it back, then confirming one character more is
refused and the file is left as it was, with a second pair of cases covering the backslash
that is written escaped and so costs two characters rather than one.

A standalone Java test reads the native validator and checks that every key Settings offers
appears there, that extension keys survive Reset these settings while the two Display rows do
not, and that the typed relay, room and folder rules match on both sides, lengths included,
which the native side measures in bytes. It also checks the folder enumeration: both extension
defaults are always offered, a name the native validator would refuse is skipped along with
everything below it, so are what a game file import leaves behind and the hidden work
directories a game files swap leaves in flight, every path that is offered passes the same
rule, and the cap holds inside a single directory rather than only on the way down:

```sh
ctest --test-dir build/host-tests -R '^android_extension_settings$' --output-on-failure
```

On device, a fresh configuration must show every row as Game default, with both folder rows
offering the folders the imported game files hold plus the two extension defaults. Third
person camera is the cheapest end-to-end check: turn it on, save, relaunch, and the camera is
visibly behind the player. A folder whose name holds a space or a comma must not appear in
either folder row. A `textures/` folder included in the source before a Replace game files
import must change a texture in the Infocenter. Replacing a character's head texture with a
24-bit BMP is the case worth watching: a replacement that is not itself 8-bit borrows the
palette of the texture it replaces, and the phoneme animation copies that texture down to 8
bits, so check that character's face while they speak. An `si/` folder holding two `.si` files
must load both once Custom SI files is on, and choosing a different folder must load only what
that one holds. Multiplayer must warn until both a relay and a room are set, refuse an
`http://` address or one with a space, and say that it turns the third person camera on. Reset
these settings must leave every extension key untouched while Reset extensions clears them,
and a hand-edited `files` or `directives` must survive both and keep loading.

## Game files

The `gamefiles` native target covers the startup step that Settings > Data > Game files
schedules: the required-file check (case-insensitive per path component, regular files only),
refusals, replacement with and without an earlier installation or configuration, removal, a
configuration that cannot be written, a staged copy lost or a game folder reappearing after the
old one was retired, collection of work directories other than those a running Settings owns, a
retired tree coming back when its record was lost, damaged and moved records, and deletion that
never follows symlinks. Every filesystem step of scheduling, replacement and removal is
interrupted in turn: each run must leave a game folder in place or the change still waiting, and
the next run must finish it. It builds with the configuration tests above.

Standalone Java tests cover the copier shared by the startup import and Settings (folder rules,
unusable names, cancellation and every failure the player is told about) and the Settings space
check, location and wording:

```sh
ctest --test-dir build/host-tests -R '^(android_game_file_copier|android_game_files_policy)$' --output-on-failure
```

On device, preserve the config, saves and game assets first, and keep a source copy of the game
in a shared folder such as Download. Replace from the in-game menu: the game closes, and the next
launch reports the replacement, starts, and leaves one `LEGO` folder and no `.isle-*`
directories, with `diskpath` naming the app's directory and other keys and saves unchanged.
Replace from startup-error Settings with the game files missing and with `diskpath` naming a
missing folder; from an `imported-*` diskpath; and from a hand-set one, which must be left
untouched. Fill the disk to see the space refusal and check that no staging directory remains.
Pick a wrong folder and one missing a single file. Cancel while copying and at the confirmation;
rotate, press Home, enable Don't keep activities and kill the process during the copy: the
current files must stay playable and the staged copy must be gone after the next launch. Kill the
process right after confirming: the change must apply on the next launch. Remove, reopen, import
through the prompt and play; saves must be unchanged. Check the startup import still behaves as
before, a controller through every dialog, and the minified release.

## Renderers

`renderers` covers the device id miniwin synthesizes for a renderer the enumeration could not
report: that `Miniwin_FormatDeviceId` round-trips through `LegoDeviceEnumerate::ParseDeviceName`
(the GUID's bytes, driver ordinal zero), that two devices never collapse onto one id, and that
the union adds a missing candidate once, matches on the id rather than the label, and leaves an
already-enumerated device alone. It also covers `Miniwin_DeviceIdNamesGuid`, which reads the same
id back to decide what kind of window the game gets, and so has to accept and reject exactly what
`ParseDeviceName` does - down to the trailing rubbish its `sscanf` ignores. Format and parse sit
in the same header for that reason, so the test exercises the pair that has to agree.

On device, both directions matter. From the default OpenGL ES session, Settings must list
SDL3 GPU HAL; choosing it must produce a window Vulkan can claim on the next launch, the frame
must fill the screen with the same letterboxing OpenGL ES produces rather than sitting in a
corner, and a screen transition must not come out with red and blue swapped, since that path
reads the frame back. From the Vulkan session, Settings must still offer the OpenGL ES entries,
and choosing one must get back. Check both render resolutions, rotation, and the startup-error
message when the GPU renderer cannot start, which must name it as the Settings row does and
reach Settings with a populated list. An emulator that cannot present Vulkan can still show the fallback: a device id
naming SDL3 GPU HAL there must start on OpenGL ES instead of failing.

## Output gain

The `output_gain` target covers the arbiter in `ISLE/outputgain.h`. The arbiter writes nothing
itself; it decides the one value `ApplyOutputGain` in `ISLE/isleapp.cpp` hands to
`MxSoundManager::SetOutputGain`, which is the tree's only caller of that and so the only writer of
the mixer's master volume. Two things turn the game down - its own pause and whatever the system
last did to the sound - and the test is where their composition is pinned: a pause silences the mix
whatever the system left, the system moving the sound around while the game is paused writes
nothing, and a resume restores exactly the gain the system asked for, so a duck that outlives a
pause comes back at 0.2 rather than at full volume. It also covers what every caller relies on: a
take reports a move and only a move, an unmoved take leaves the caller's gain alone (the caller
declares it uninitialised), several changes between two takes collapse to the last state, and a
gain the class was never told a name for passes through unaltered. It links neither SDL nor
iniparser, though the test project requires both to configure, and builds with the configuration
tests above:

```sh
ctest --test-dir build/host-tests -R '^output_gain$' --output-on-failure
```

Note that writing the composition as a product rather than as a pause that wins outright is
*equivalent*, both factors being exact, so no test distinguishes the two. Mutations that do get
caught: dropping the pause from the composition, restoring full volume on resume instead of the
system's gain, removing the guard that reports only a move, writing through on a take that reports
nothing, a pause that writes again over a mix the system had already silenced, and a `SetFocus`
that snaps a value it does not recognise. Not an exhaustive list, and one thing it cannot reach at
all: the order in `ApplyOutputGain`, where the system's gain is taken *before* the sound manager is
checked so that a focus change arriving during startup is kept rather than consumed. That ordering
is held by a comment and nothing else, because reaching it needs LEGO1.

On device, the ear decides, and **it has to be the right sound**. A streamed line - a cutscene, the
music - was already stopped by `MxSoundManager::Pause` before any of this existed, so checking one
of those passes on an unpatched build. Listen instead for what this changes: the island's ambient
sound, or a character's cached line. Open the in-game menu while one is playing: it must go quiet,
not carry on under the menu, and come back on Resume. Repeat for the quit prompt, a Settings round
trip, and Home and back. Those sounds keep running while they are silent, so one may have finished
by the time you return; a streamed line, by contrast, must pick up where it stopped. Both are
correct, and confusing them is how this check goes wrong.

With the focus driver below, duck the game first and then pause and resume it: `Playing at gain`
must report `0.20` on the resume rather than `1.00`. Read that line and not `Audio focus changed to
gain`, which reports what the system asked for and not what comes out. If no duck is logged at all,
see the caveat in that section: from API 26 the system may turn the app down itself without ever
calling the listener, in which case the game's own gain never moves and this check cannot run.

## Audio focus

The `audiofocus` target covers what the game does when the system takes its sound away. It reads
the `AudioManager.AUDIOFOCUS_*` numbers Java passes through untranslated, so the mapping has one
definition and the test is where the numbers are written down: every positive value, 1 for
`AUDIOFOCUS_GAIN` and 2, 3 and 4 for the `GAIN_TRANSIENT` variants an external focus policy can
send, has to mean the sound is ours again, or the game is left silent with nothing able to put it
right. It also covers -1, -2 and -3 reaching their gains, `AUDIOFOCUS_NONE` and undefined values
refused so the gain stays where it is, a duck applied once and not re-applied, the sound coming
back restoring exactly 1.0 rather than an approximation of it, several reports between two takes
collapsing to the last, and the two silent cases applying nothing between them. It needs neither
SDL nor iniparser, and builds with the configuration tests above:

```sh
ctest --test-dir build/host-tests -R '^android_audio_focus$' --output-on-failure
```

On device, preserve the config and saves first. Play audio in another app and launch the game: the
other app must stop, and `adb shell dumpsys audio` must show `dev.mjkoo.isle` holding focus
with `GAIN`. Focus follows onStart and onStop, not onResume and onPause, because that is where SDL
stops and starts mixing - so the game must still hold focus while its own quit prompt is up, which
does not stop the activity (it does pause the game, and so silences it: see Output gain above), and
must have released it after Home and after opening Settings, which stop the activity.

Nothing on a stock emulator image asks for focus on demand, so this needs a throwaway app of its
own. It has to be a **foreground service**: an activity would take window focus and pause the game
under test, and a background receiver is refused focus outright. Give it modes that request
`AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK`, `AUDIOFOCUS_GAIN_TRANSIENT`,
`AUDIOFOCUS_GAIN_TRANSIENT_EXCLUSIVE` and `AUDIOFOCUS_GAIN`, and one that abandons, then drive it
with `adb shell am start-foreground-service -n <pkg>/.FocusService --es mode <mode>` and read the
gains out of the game's log. Remove it afterwards.

The exclusive mode is the one worth keeping: it locks the focus stack, which is what a player who
returns to the game during a call runs into. The request is queued rather than refused, so the game
must go silent rather than stay at whatever gain it had, and must come back to 1.00 when the driver
abandons **without the game's window being touched** - that is the whole point of asking for
delayed focus, and the check fails if it takes a switch away and back. Android 7 and older cannot
queue a request and still needs the window, but no emulator image kept around here runs that far
back.

`adb emu gsm call` is not a substitute. It reports `OK` and never reaches the framework on the
API 35 `google_apis` image: `dumpsys telephony.registry` keeps `mCallState=0`, `dumpsys telecom`
lists no ringing call, and `gsm.sim.state` is empty. The ringing-call case is covered by the
driver's transient mode, which is what a ringing call requests.

Confirm by ear as well as by log that a duck leaves dialogue audible rather than silencing it. A
duck with nothing in the log is not a failure: from API 26 the system may turn the app down itself
and never call the listener, and the game's own gain only covers the times it does not. The ear is
what decides this one.

Check the paths that already pause - the in-game menu, a Settings round trip, the quit prompt, Home
and resume. Each of those now silences the game on the way in, so what is checked here is that
audio returns on the way out. Install the **x86** APK on an arm64 device to exercise
the path where the native libraries will not load: SDL's error dialog must appear and survive,
because audio focus is the one thing this app calls into native on its own account. Repeat the
request check and one duck on the minified release, the JNI method being reached only through
R8-processed Java.

## Pause prompt

```sh
ctest --test-dir build/host-tests -R '^android_quit_prompt_text$' --output-on-failure
```

This pins the title, message and button labels for each save result and startup
errors, including unknown save results and empty error text. Settings is always
available; Resume appears only for a running game. It also checks the mirrored
save constants against `quitprompt.h` and status constants against `quitprompt.cpp`.
The host test cannot reach AlertDialog wiring or establish save durability.

### Device procedure

Use an API 35 arm64 emulator with game data. Record the actual API level and device.
Read the effective save directory from `isle:savepath` first; `files/saves` is only
the default. Back up config and saves and record directory mode bits before testing.
Restore all of them even if a step aborts, then compare the restored bytes.

1. Register a player and change some progress. Open the menu with Back. Expect
   **Game paused.** and **Save and quit | Settings | Resume** in positive, neutral,
   negative order on the API 35 emulator with the app's current theme. The old build
   says Quit, so this distinguishes the change.
2. Use a fresh, unregistered player set. Expect **There is no saved game yet.** and
   **Quit | Settings | Resume**. The engine can return success without writing for
   an unregistered player; that must not produce Save and quit.
3. Restore the registered player. On the debug build, run
   `adb shell run-as dev.mjkoo.isle chmod 000 <savedir>` against the effective
   save directory. This blocks traversal even when slot files already exist.
   Open the menu. Expect **Your game could not be saved.** and
   **Quit anyway | Settings | Resume**. Require the real
   `Failed to save game state (back button)` message in `adb logcat -s SDL`;
   successful chmod alone does not establish a save failure. Confirm Resume remains
   available, then Quit anyway exits without a second prompt.
4. Restore the recorded mode bits. Relaunch and verify the earlier player and
   progress load. Back must again show **Game paused.** / **Save and quit**.
5. With game data unavailable, relaunch and exercise startup failure. Expect
   **LEGO Island could not start**, the error text, **Close | Settings**, and no Resume.
6. Check a Settings round trip reapplies touch scheme and controller buttons on
   Resume. Scheduled Replace game files or Restore saves must close the game.
   A second Back over the prompt resumes. Menu and touch controls return on Resume.
   Island ambient sound goes quiet while paused and returns on Resume.
7. Build with `just android-apk` and `just android-apk-release`. Repeat the first
   three cases on the minified release, with a suitable failure-injection setup
   (release builds do not permit `run-as`). The wording class deliberately has no
   proguard keep rule.

Only the device procedure checks actual AlertDialog wiring. Host assertions cannot
catch labels passed to the wrong builder buttons. Neither procedure proves every
engine write reached disk: serialization can ignore failures, and shutdown ignores
the save result. Storage changes after the menu-opening attempt are not detected
by this feature. Other API levels, physical devices and desktop require separate
validation.

## Android TV

```sh
ctest --test-dir build/host-tests -R '^android_tv_support$' --output-on-failure
```

This pins which Back key opens the game menu: one from a non-virtual device with a D-pad that is
neither a gamepad nor a joystick, which is what a remote reports and what SDL would otherwise take
for a controller. The sources are multi-bit and share `SOURCE_CLASS_BUTTON` with the keyboard, so
the test includes a keyboard with a D-pad, which a test of any one bit misclassifies. It also pins
that the TV's picker stub is not a picker and that a TV has no touch controls. The copied
framework constants are checked against literal values. The host test cannot reach how SDL
routes a device, whether a picker intent resolves, or the dialogs.

### Device procedure

Use the Android TV emulator: `just android-tv-avd`, then `just android-tv-emulator`. Its image is
API 34, a user build, so `adb root` is unavailable; `run-as` works on the debug build. With the
phone AVD also running it is `emulator-5556` or `emulator-5554`, whichever booted second, so
check `ro.build.flavor` before every `adb -s`. Back up `isle.ini` with `run-as` first and compare
it afterwards.

1. `cmd package query-activities -a android.intent.action.OPEN_DOCUMENT_TREE` names
   `com.android.tv.frameworkpackagestubs`: the intent resolves, to a stub. That is what a plain
   resolve check would take for a picker.
2. Install the debug APK. The game is under Installed Games in the Apps tab with its banner. The
   launcher caches banners: after changing one, force-stop the launcher to see it.
3. With no `LEGO` folder under `Android/data/dev.mjkoo.isle/files`, launch. Expect the
   no-picker message naming that folder and the three adb commands, with **Check again** and
   **Cancel**. The old build offers Select folder, which toasts "You don't have an app that can do
   this" and fails to start. Check again with nothing copied names the missing file. Run the
   three commands as shown, then Check again: the game starts. `adb push` straight into
   `Android/data` fails with `secure_mkdirs failed`, which is why the message goes through
   `/data/local/tmp`.
4. No menu button or touch hints are drawn. Settings shows no touch rows, Export and Restore
   saves are disabled and say a file picker is needed (Restore previous saves, when a backup
   exists, stays enabled), and Replace game files explains that there is no folder picker.
5. Register a remote with `uinput`, a keyboard-class device with only the D-pad, select, Back and
   Menu keys (`adb shell dumpsys input` reports `Sources: KEYBOARD | DPAD`). Its Back opens the
   game menu, with `Saving game state (back button)` in the log. On the old build it opened
   nothing: SDL passed it on as a controller button. Its Menu key opens the menu too, select
   clicks, and the D-pad moves the cursor.
6. Register an Xbox-layout pad with `uinput`. Its Back still acts as Esc and does not open the
   menu. `adb shell input keyevent KEYCODE_BACK` arrives from the virtual keyboard, so it takes
   the system Back path on either build and cannot tell the two apart.
7. On the phone AVD, repeat the Back, Settings and Replace game files checks: the menu button and
   touch rows are there, Export and Restore are enabled, and Replace opens the system picker.

Neither procedure covers TV hardware, a real remote, or a TV that ships a folder picker.

## About

```sh
ctest --test-dir build/host-tests -R '^android_about_text$' --output-on-failure
```

This pins Settings > About: the rows in order, the upstream, decompilation, license and
disclosure links, and the wording that credits the isledecomp contributors and says the fork is
not affiliated with them. It also pins the source link, which follows the commit
`build.gradle` appends to the version name and falls back to the repository when a build does
not know its commit. The URLs are spelled out in the test rather than read from `AboutText`, so a
wrong constant cannot pass.

On device, the About group is the last in Settings, after Save settings. Version matches
`adb shell dumpsys package dev.mjkoo.isle | grep versionName`. Each row with a link opens it in a
browser, Source code at the build's commit, and shows its address under the summary; the
Version and Not affiliated rows cannot be chosen. The TV AVD has no browser, only the framework's
stand-in, which answers a link with "You don't have an app that can do this" while Settings stays
open; the address on the row is what tells the viewer where it goes. A device with no handler at
all gets "No app can open" and the address instead.
