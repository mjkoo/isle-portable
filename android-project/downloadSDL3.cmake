cmake_minimum_required(VERSION 3.25...4.0 FATAL_ERROR)

include(FetchContent)

# The same revision the CMake build uses. Fetched separately because Gradle needs an SDL3 source
# tree before the native build runs, in order to build the .aar the app links against.
include("${CMAKE_CURRENT_LIST_DIR}/../CMake/SDL3Revision.cmake")

set(FETCHCONTENT_BASE_DIR "build/_deps")

FetchContent_Populate(
  SDL3
  GIT_REPOSITORY "${ISLE_SDL3_DEFAULT_GIT_REPO}"
  GIT_TAG "${ISLE_SDL3_DEFAULT_GIT_TAG}"
  SOURCE_DIR "build/_deps/sdl3-src"
  BINARY_DIR "build/_deps/sdl3-build"
)
