# Android renderers

Settings > Display > Renderer chooses which 3D backend the game draws with. The choice applies
on the next launch, and the game says which one it resolved to in its log:

```
Direct3D driver name="Miniwin driver" description="OpenGL ES 3.0 HAL"
```

## What the list contains

An Android build compiles SDL3 GPU HAL, OpenGL ES 3.0 HAL, OpenGL ES 2.0 HAL, Miniwin Paletted
Software and Miniwin Emulation. **Game default** clears the setting and lets the game pick,
which is OpenGL ES 3.0 on anything that can run it.

The list is not only what the running session could initialize. It cannot be: a renderer is
normally discovered by trying to create it against the window the game already has, and the two
renderers that matter here each need a window the other cannot use. So the list adds whatever
this build compiled in but the current window could not offer, and a value the game no longer
recognizes still appears as "Current: ...", so nothing you have chosen disappears from view.

Because it is the compiled-in list, it can offer a renderer a particular device does not have.
Choosing one that is not there is not a dead end: see below. **Reset these settings**, under
Troubleshooting, clears the renderer along with everything else, which is the shortest way back
if the game will not start.

## Vulkan

SDL3 GPU HAL is the Vulkan path. It is not the default: it has not been measured against the
OpenGL ES renderer on real hardware, so the game keeps the renderer it has always used until
you ask for something else.

Choosing it is what makes it possible. Android binds an EGL surface to a window as soon as that
window is created for OpenGL, and Vulkan cannot then take that same window for the rest of its
life. The game therefore has to decide before it creates the window, which it does by reading
the renderer you chose. A device with no usable Vulkan at all keeps the OpenGL window, and the
game quietly starts on OpenGL ES rather than failing.

If Vulkan gets that far and still cannot start - the window is refused, or the pipelines will
not build - the game cannot fall back within that launch. The game falls back to another device
only when the configured renderer cannot be found at all; one that is found and then fails to
start ends the launch. (That is also what makes the paragraph above work: with no usable Vulkan
the GPU renderer is never found, so the game picks the best one it did find.) It stops with a
message naming the renderer and offers Settings, where choosing another renderer, Game default
or Reset these settings fixes the next launch.

## Render resolution

Render resolution is independent of the renderer. The game's own coordinates stay at 640 × 480
and the rendered image is scaled to the window, preserving its aspect ratio, whichever backend
draws it. The OpenGL ES backends additionally reduce the render target to fit the GPU's texture
and viewport limits and log that they did; the Vulkan backend does not, because its limits sit
far above anything a phone window asks for.
