# The SDL3 sources this repository fetches, each named at one revision.
#
# The CMake build and android-project/downloadSDL3.cmake fetch SDL3 independently, into separate
# trees. Naming the revision in one place is what keeps the desktop build and the Android APK on
# the same SDL3 rather than on whatever "main" pointed at when each of them last fetched.
#
# Upstream has to be a commit on main, not a release tag. ISLE/android/activity.cpp uses
# SDL_EVENT_GAMEPAD_CAPSENSE_RELEASE and the SDL_EVENT_FINGER_* / SDL_EVENT_PINCH_* range
# constants, none of which exist in release-3.4.16.
#
# Nothing here is a cache entry, deliberately. A cached default is written once and then never
# replaced, so a tree configured before a bump would go on building the revision it first saw.
# Plain variables mean a bump lands on the next configure everywhere. Passing
# -DISLE_SDL3_REVISION=<ref> does create a cache entry, which is what makes a deliberate override
# stick; the guards below then leave it alone. Drop such an override by deleting the entry from
# that tree's CMakeCache.txt.
#
# To build against a different SDL, pass -DISLE_SDL3_REVISION=<ref> to cmake, or
# -PisleSdl3Revision=<ref> to Gradle, which forwards it to downloadSDL3.cmake.
#
# Not every SDL3 fetch is pinned here: the Nintendo Switch build uses a fork pinned by tag at its
# own declaration in CMakeLists.txt.

if(NOT DEFINED ISLE_SDL3_REPOSITORY)
  set(ISLE_SDL3_REPOSITORY "https://github.com/libsdl-org/SDL.git")
endif()

if(NOT DEFINED ISLE_SDL3_REVISION)
  # SDL 3.5.0 prerelease. Chosen because it is the revision the Android APK was built from and the
  # one the Vulkan renderer and the extensions settings were verified against on an emulator,
  # which makes it the best-evidenced revision this repository has.
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
