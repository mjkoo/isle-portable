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

The texture folder and the SI files name a place **inside the game files**, not somewhere on
your device. The game looks for them under the folder it reads its data from, so a texture
folder of `/textures` means a `textures` folder sitting beside `LEGO`.

There is no folder picker for them, and there does not need to be: Settings lists the folders
and `.si` files it finds in the game files. To add your own, put them in the folder you select
in **Data > Game files > Replace game files**, then choose them here. The SI list leaves out
the 26 scripts a complete install already has, so what it offers is what you added.

**Custom textures** looks for `<texture folder>/<name>.bmp` for each texture the game draws.
The default folder is `/textures`.

**Custom SI files** loads each file you select. Selecting nothing loads none.

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
files = /LEGO/Scripts/MYMOD.SI

[multiplayer]
relay url = wss://relay.example
room = lobby
actor = pepper
```

`si loader:directives`, which rewires which stream objects start and stop each other, has no
row here: it is a modding tool with no values to offer. Settings never writes it, so one you
add by hand stays as you left it. The same is true of any other key you add to these sections.
