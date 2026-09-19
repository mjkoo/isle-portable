# Android extensions

Extensions are the parts of `isle-portable` that are not in the original game. All four are
built into the Android app and all are off until you turn them on in **Settings > Extensions**.
They apply on the next launch, so save your settings, quit and open the game again.

| Row | What it does |
|---|---|
| Custom textures | Replaces the game's textures with `.bmp` files of the same name |
| Custom SI files | Loads extra `.si` files alongside the game's own |
| Third person camera | Puts the camera behind your character instead of at their eyes |
| Multiplayer | Joins other players through a relay server |

## Where the files go

Both folders name a place **inside the game files**, not somewhere on your device. The game
looks for them under the folder it reads its data from, so a texture folder of `/textures`
means a `textures` folder sitting beside `LEGO`.

There is no device folder picker for them, and there does not need to be: Settings lists the
folders it finds in the game files. To add your own, put them in the folder you select in
**Data > Game files > Replace game files**, then choose them here. Both defaults are always
offered, so you can pick one before you have created it.

**Custom textures** looks for `<texture folder>/<name>.bmp` for each texture the game draws.
The default folder is `/textures`. A pack may hold as many files as you like.

**Custom SI files** loads every `.si` file directly inside the SI folder, in alphabetical
order. The default folder is `/si`. Files in subfolders are not loaded.

## Multiplayer

Multiplayer connects to the relay server you name, so it reaches the network; nothing is sent
until you turn it on and give it somewhere to go. It needs both:

- **Relay server**, a WebSocket address such as `wss://relay.example`.
- **Room**. Everyone who picks the same room on the same relay plays together.

Settings says so until both are set. **Character** picks which character other players see you
as, from the game's own list.

Multiplayer turns the third person camera on whatever the Third person camera row says, since
it needs it to show other players.

If the relay turns your session away, for instance because the room is full, the game saves and
closes.

## Resetting

**Reset extensions**, at the bottom of the group, turns every extension off and forgets its
settings. **Reset these settings**, which covers Input, Audio, Display and Graphics, leaves
extensions alone.

## Configuration file

Each extension is a boolean in the `[extensions]` section of `isle.ini`, and its options live in
a section named after it, which every platform reads:

```ini
[extensions]
texture loader = true
si loader = false
third person camera = true
multiplayer = false

[texture loader]
texture path = /textures

[si loader]
si path = /si

[multiplayer]
relay url = wss://relay.example
room = lobby
actor = pepper
```

Two keys under `[si loader]` have no row here, and Settings never writes either, so one you
add by hand or in the desktop configuration tool stays as you left it:

- `files`, a comma-separated list of individual `.si` files. It still loads, and the desktop
  tool edits it, but it has to fit on one line of `isle.ini` and none of its paths may contain
  a space, which is why the folder is what this screen offers. Files it names load before the
  folder's, and a file in both loads once.
- `directives`, which rewires which stream objects start and stop each other. It is a modding
  tool with no values to offer.

The same is true of any other key you add to these sections.
