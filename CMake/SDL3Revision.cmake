# The SDL3 sources this repository fetches, each named at one revision.
#
# Both the CMake build and android-project/downloadSDL3.cmake include this file, because they
# fetch SDL3 independently into separate trees. Before it existed they both followed "main" and
# drifted: four checkouts on this machine sat at three different revisions, and the desktop build
# and the Android APK were two days apart.
#
# Upstream has to be a commit on main, not a release tag. ISLE/android/activity.cpp uses
# SDL_EVENT_GAMEPAD_CAPSENSE_RELEASE and the SDL_EVENT_FINGER_* / SDL_EVENT_PINCH_* range
# constants, none of which exist in release-3.4.16.
#
# Nothing here is a cache entry, deliberately. A cached default is written once and then never
# replaced, so every tree that had configured before a revision moved would keep building the old
# one: that is exactly what the ISLE_SDL3_GIT_TAG=main entry this file replaced did. Plain
# variables mean a bump lands on the next configure everywhere. Passing -DISLE_SDL3_REVISION=<ref>
# does make a cache entry, which is what keeps a deliberate override sticky; the guards below then
# leave it alone.
#
# To build against a different SDL, pass -DISLE_SDL3_REVISION=<ref> to cmake, or
# -PisleSdl3Revision=<ref> to Gradle, which forwards it to downloadSDL3.cmake.
#
# A build tree configured before this file existed carries FETCHCONTENT_UPDATES_DISCONNECTED=ON
# in its cache, left there by the UPDATE_DISCONNECTED the SDL3 declaration used to set. Removing
# that from the declaration does not clear the cache entry, so the first reconfigure refuses to
# fetch a revision it does not already have and fails with "not allowed to contact remote".
# Reconfigure such a tree once with -DFETCHCONTENT_UPDATES_DISCONNECTED=OFF; nothing has to be
# deleted, and the existing clone is reused. Fresh trees are unaffected.

if(NOT DEFINED ISLE_SDL3_REPOSITORY)
  set(ISLE_SDL3_REPOSITORY "https://github.com/libsdl-org/SDL.git")
endif()

if(NOT DEFINED ISLE_SDL3_REVISION)
  # SDL 3.5.0 prerelease. Chosen because it is the revision the Android APK was built from and the
  # emulator verification of the Vulkan renderer and the extensions settings actually ran on, which
  # makes it the best-evidenced revision this repository has.
  set(ISLE_SDL3_REVISION "42ad5c99551b332073b4ca45daed6937fb738093")
endif()

# The Windows Store build fetches a fork instead of upstream. It followed that fork's main until
# this pin; the fork has not moved since 2025-07-06, so this revision is simply the one CI has
# been fetching all along.
if(NOT DEFINED ISLE_SDL3_UWP_REPOSITORY)
  set(ISLE_SDL3_UWP_REPOSITORY "https://github.com/Helloyunho/SDL3-uwp.git")
endif()

if(NOT DEFINED ISLE_SDL3_UWP_REVISION)
  set(ISLE_SDL3_UWP_REVISION "3fa5bba61a84a5c96ec37d8c6a82ab645fbc20ce")
endif()

# The Nintendo Switch build fetches a third fork. It is pinned at its own declaration, by tag,
# alongside the record of which forks were tried before it.
