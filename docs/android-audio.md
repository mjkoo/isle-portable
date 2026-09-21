# Android audio

The game takes the system's audio focus while it is in front, so starting it stops music another
app was playing, and it gives the focus back when you leave it. It keeps the focus for as long as
it is the app you are in: opening its own menu or quit prompt does not give it up, even though the
game falls quiet behind them, but leaving for another app does.

## When the game is paused

Pausing the game takes its sound with it. That happens when you open the menu or the quit prompt,
when you go into Settings, and when you leave for another app. (`Active in Background` in
`isle.ini` stops the game pausing when you leave it. Settings does not offer it, because on
Android the game's own settings screen counts as leaving.)

What a paused sound does depends on which kind it is. The music, the cutscenes and the spoken
lines that stream from disk stop where they are and pick up from the same place when you come
back. The island's own sounds - a character's cached line, the noise of a machine - go quiet but
keep running underneath, so one of those may have finished by the time you return.

## When something else needs the sound

This is not the same as pausing: when another app wants the sound, the game keeps playing and
turns itself down or quiet, so a cutscene carries on while something talks over it. Answering a
call is the exception, because leaving the game pauses it as above.

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
