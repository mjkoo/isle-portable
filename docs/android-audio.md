# Android audio

The game takes the system's audio focus while it is in front, so starting it stops music another
app was playing, and it gives the focus back when you leave it. It holds the focus for exactly as
long as it is making a sound: opening its own menu or quit prompt does not give it up, but leaving
for another app does.

## When the game is paused

Anything that pauses the game stops its sound with it: the in-game menu, the quit prompt, Settings,
and leaving for another app. That covers speech and the sounds of the island, not only the music
and the cutscenes.

A stopped sound does not wait where it left off. It keeps running silently, so a line of dialogue
that began just before you opened the menu may be over, or part way through, when you come back.

## When something else needs the sound

The game turns down or falls silent rather than pausing, so a cutscene keeps running while another
app talks over it.

| What happens | What the game does |
| - | - |
| A notification or a navigation prompt | Turns down until it has finished talking |
| A call rings | Falls silent until you answer or dismiss it |
| You answer a call | Leaves the game in the background, silent, until you return |
| Another app takes the sound for good | Falls silent until you leave the game and come back |

If you return to the game while a call is still going, the game stays silent: the system will not
hand the sound to anything else during a call. It comes back by itself once the call ends. On
Android 7 and older, which cannot queue the request, it comes back the next time you leave the game
and return.

## Volume

The game has no volume setting of its own. It plays at the media volume, and the volume keys change
it while the game is in front.

Settings > Audio turns the music and the 3D sound on and off; both apply on the next launch. See
[Android install](android-install.md) for the Settings screen itself.
