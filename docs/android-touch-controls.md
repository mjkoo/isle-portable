# Android touch controls

Choose a touch scheme in Settings > Input. Show touch controls is enabled by default
and can be switched off without disabling touch input. Save settings, then quit and
relaunch to apply them. Reset these settings restores the default visibility.

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
