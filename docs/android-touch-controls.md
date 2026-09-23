# Android touch controls

Choose a touch scheme in Settings > Input. Show touch controls is enabled by default
and can be switched off without disabling touch input. Touch button size scales the
menu, Space and Esc buttons from 50% to 200%, and Touch control opacity fades the
buttons and movement hints down to 10%. Save settings, then Resume to apply these
options to the current game. They also persist for the next launch. Other settings
still require quitting and relaunching. Cancel leaves the settings unchanged. Reset
these settings followed by Save restores Virtual mouse, visible controls, full size
and full opacity; button positions are kept. Changing schemes cancels held gestures;
touch again after resuming. A failed save leaves the current controls unchanged and
can be retried.

On a TV none of this is shown: no touch controls or menu button are drawn, and Settings
leaves out the touch rows, even where the TV reports a touchscreen. The values already in
`isle.ini` are kept.

With **Arrow-key regions**, the upper three quarters of the game image move forward.
The bottom quarter is split evenly into left, backward and right. Faint lines show
these boundaries, and held directions brighten. Multiple fingers can hold different
directions together.

With **Virtual stick**, the first finger touching the game image owns movement until
released. Drag from its starting point to move. The floating square shows a scaled
representation of the movement axes, not the finger's travel distance: each axis
reaches full movement at a quarter of the game image's width or height. A second
finger does not take over the stick. The indicator may be clipped near image edges.

A touch starting in a black bar cannot start movement. An arrow-region finger that
moves into a bar becomes neutral and can return to the image. An existing stick
gesture continues when dragged outside the image. Release always ends the gesture.
Pausing, losing focus or resizing the game cancels held movement; touch again after
returning to play.

The hints are visual feedback only. Existing game clicks and dragging still reach
the game. Virtual mouse and Disabled have no movement hints. Physical mouse,
keyboard or controller use hides the hints; touchscreen use restores them. Menus,
Settings and backgrounding hide them as well. The game menu button stays separate.

The **Space** and **Esc** buttons, beside the menu button by default, send the
corresponding game keys. All three buttons stay clear of the navigation bar and
display cutouts.
Space can interrupt animations or end free navigation; Esc can return to the
infocenter. Their effect depends on the current scene. Esc does not open the Android
menu. The existing double-tap gesture still sends Space.

Tap and release inside a button to activate it once. Holding does not repeat.
Dragging outside cancels that press, even if you drag back in. Button touches do
not move the player or click the scene. You can hold movement with one finger and
tap an action with another. A finger that starts on the game image stays with the
game when dragged across a button. Hiding controls, pausing, changing focus or
resizing cancels pending button presses; returning to play never replays them.

To move the buttons, open the game menu, choose Settings, then Edit touch layout.
The game stays paused while you drag the menu, Space and Esc buttons; each follows
one finger and stays clear of system bars and display cutouts. A touch that barely
moves leaves a button where it is. Reset returns every button to its default place,
Done saves the layout and returns to the menu, and Back discards your changes. If
saving fails, the editor stays open with your changes so you can choose Done again.
Nothing is saved until Done, and the editor never reopens by itself after a restart.
Edit touch layout appears only in Settings opened from the game menu, and asks you
to save or cancel other settings changes first.

The editor always shows Space and Esc, even when the current touch scheme or Show
touch controls hides them during play; their positions apply whenever they appear.
The game image behind the editor may be black, because the paused game does not
redraw after Settings. Positions are kept relative to the area clear of system bars
and cutouts, so a layout carries over to other screen sizes and rotations, and a
button that would no longer fit is moved inside. Movement regions and the virtual
stick do not move: arrow regions always cover the game image, and the stick starts
wherever your finger lands.
