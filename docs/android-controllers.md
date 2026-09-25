# Android controllers

A connected gamepad plays the game as it does on desktop: the left stick moves, and the right
stick and D-pad move the cursor. The buttons below can be changed in Settings.

| Button | Default action |
|---|---|
| The face button labelled A | Click |
| The other of the bottom and right face buttons | Space |
| Back or Select | Esc |
| Start | Open this game's menu |
| Right trigger | Click |
| Left and top face buttons, shoulders, left trigger, stick clicks, Guide | Nothing |

On Xbox and PlayStation layouts the bottom face button is labelled A (Cross); on Nintendo
layouts the right one is. Start opens the same menu as Android Back and the touch menu button,
so a controller alone can reach Settings. Start does nothing until the game has started.

In the menu the D-pad moves between the choices, starting at Resume. A chooses the highlighted
one, and B or Start resumes. After touch input clears the highlight, the first A press
highlights Resume without choosing it; the next press chooses it. Releasing the button
that opened the menu does nothing.

## Changing buttons

Open the game menu, choose Settings, then the Controller group. Each row is a physical button;
choose Click, Space, Esc, Pause, Open menu or Nothing for it. Several buttons can share an
action, and changing one row never changes another. **Confirm button** chooses which of the
bottom and right face buttons clicks by default: the one labelled A, or always the bottom or
the right one. A row you have set yourself keeps its own action.

Save settings, then Resume to use the new buttons in the current game; they also apply on later
launches. Cancel leaves them unchanged. With only a controller, **Save settings** at the bottom of
the list saves, and B leaves Settings without saving. **Reset controller buttons** followed by Save returns
every row to its default, and Reset these settings leaves controller buttons alone. Settings
warns when no button would open the menu; Android Back, a TV remote's Back and the touch menu
button still open it.

Releasing a button always ends what pressing it started, even if its action or the pad's layout
changed while it was held. Buttons held while the menu opens do nothing when released after
Resume, and a trigger still held when you resume does nothing until you release it. A trigger acts
once each time it is pulled past about a quarter of its travel.

## Configuration file

The buttons are stored in the `[gamepad]` section of `isle.ini`, which every platform reads, so
desktop players can edit it by hand:

```ini
[gamepad]
confirm = label
start = pause
north = escape
```

Keys are `south`, `east`, `west`, `north`, `leftshoulder`, `rightshoulder`, `lefttrigger`,
`righttrigger`, `leftstick`, `rightstick`, `back`, `start` and `guide`, named by position.
Values are `click`, `space`, `escape`, `pause`, `menu` or `none`, in any letter case; `confirm`
is `label`, `south` or `east`. A missing or unusable value keeps the default, and the game logs
the unusable ones. `menu` does nothing outside Android. On Vita, Start has no default action
because it is part of the system screenshot button combination; binding it brings that conflict
back. The desktop isle-config program keeps this section when it saves, along with every other
key it does not edit, so configuring on the desktop no longer removes custom buttons.

A TV remote is not a controller, although Android passes it to the game as one: its Back opens
the game menu rather than acting as Esc. See [Android TV](android-tv.md).

## Limits

One set of buttons applies to every connected pad. Pads whose L2 and R2 report as buttons rather
than as triggers cannot use the trigger rows. On a pad without a right face button, SDL treats
Back as that button. Handhelds with a Nintendo or Xbox layout switch change which button is
labelled A, and so which one clicks while Confirm button is left at its default.
