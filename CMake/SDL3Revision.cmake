# The SDL3 revision every tree in this repository builds against.
#
# Both the CMake build and android-project/downloadSDL3.cmake include this file, because they
# fetch SDL3 independently into separate trees. Before it existed they both followed "main" and
# drifted: four checkouts on this machine sat at three different revisions, and the desktop build
# and the Android APK were two days apart.
#
# It has to be a commit on main, not a release tag. ISLE/android/activity.cpp uses
# SDL_EVENT_GAMEPAD_CAPSENSE_RELEASE and the SDL_EVENT_FINGER_* / SDL_EVENT_PINCH_* range
# constants, none of which exist in release-3.4.16.
#
# A build tree configured before this file existed carries FETCHCONTENT_UPDATES_DISCONNECTED=ON
# in its cache, left there by the UPDATE_DISCONNECTED the SDL3 declaration used to set. Removing
# that from the declaration does not clear the cache entry, so the first reconfigure refuses to
# fetch a revision it does not already have and fails with "not allowed to contact remote".
# Reconfigure such a tree once with -DFETCHCONTENT_UPDATES_DISCONNECTED=OFF; nothing has to be
# deleted, and the existing clone is reused. Fresh trees are unaffected.
#
# Override with -DISLE_SDL3_REVISION=<ref> to test an SDL change or chase a regression.

set(ISLE_SDL3_DEFAULT_GIT_REPO "https://github.com/libsdl-org/SDL.git")

# SDL 3.5.0 prerelease. Chosen because it is the revision the Android APK was built from and the
# emulator verification of the Vulkan renderer and the extensions settings actually ran on, which
# makes it the best-evidenced revision this repository has.
set(ISLE_SDL3_DEFAULT_GIT_TAG "42ad5c99551b332073b4ca45daed6937fb738093")
