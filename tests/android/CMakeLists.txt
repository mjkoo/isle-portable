cmake_minimum_required(VERSION 3.20)
project(android_host_tests LANGUAGES CXX)

# When off, a test whose dependency is missing is skipped with a status line. CI turns it on so
# that a runner losing Java or the built SDL3 fails the job instead of quietly running fewer tests.
option(ISLE_HOST_TESTS_REQUIRE_ALL "Fail to configure rather than skip tests whose dependencies are missing" OFF)

# glibc declares closedir nonnull, and GCC warns that the decltype(&closedir) deleters in
# ISLE/android drop the attribute. Clang, which builds them for Android, does not.
add_compile_options($<$<CXX_COMPILER_ID:GNU>:-Wno-ignored-attributes>)

find_package(Threads REQUIRED)
find_path(SDL3_INCLUDE_DIR SDL3/SDL_video.h HINTS ../../build/_deps/sdl3-src/include REQUIRED)
find_path(INIPARSER_INCLUDE_DIR iniparser.h REQUIRED)
find_library(INIPARSER_LIBRARY NAMES iniparser REQUIRED)
add_executable(configstore configstore.cpp ../../ISLE/android/configstore.cpp)
target_compile_features(configstore PRIVATE cxx_std_17)
target_compile_options(configstore PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(configstore PRIVATE ../../ISLE/android ../../util "${INIPARSER_INCLUDE_DIR}" "${SDL3_INCLUDE_DIR}")
target_link_libraries(configstore PRIVATE "${INIPARSER_LIBRARY}" Threads::Threads)
enable_testing()
add_test(NAME android_config_store COMMAND configstore)

add_executable(inifile inifile.cpp)
target_compile_features(inifile PRIVATE cxx_std_17)
target_compile_options(inifile PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(inifile PRIVATE ../../util "${INIPARSER_INCLUDE_DIR}")
target_link_libraries(inifile PRIVATE "${INIPARSER_LIBRARY}")
add_test(NAME ini_file COMMAND inifile)

add_executable(savesnapshot savesnapshot.cpp ../../ISLE/android/savesnapshot.cpp)
target_compile_features(savesnapshot PRIVATE cxx_std_17)
target_compile_options(savesnapshot PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(savesnapshot PRIVATE ../../ISLE/android)
add_test(NAME android_save_snapshot COMMAND savesnapshot)

add_executable(rendertarget rendertarget.cpp)
target_compile_features(rendertarget PRIVATE cxx_std_17)
target_compile_options(rendertarget PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(rendertarget PRIVATE ../../miniwin/src/internal "${SDL3_INCLUDE_DIR}")
add_test(NAME android_render_target_limits COMMAND rendertarget)

add_executable(hresult hresult.cpp)
target_compile_features(hresult PRIVATE cxx_std_17)
target_compile_options(hresult PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(hresult PRIVATE ../../miniwin/include "${SDL3_INCLUDE_DIR}")
add_test(NAME android_hresult_failure_codes COMMAND hresult)

add_executable(touchmovement touchmovement.cpp)
target_compile_features(touchmovement PRIVATE cxx_std_17)
target_compile_options(touchmovement PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(touchmovement PRIVATE ../../util "${SDL3_INCLUDE_DIR}")
add_test(NAME android_touch_movement COMMAND touchmovement)

add_executable(saverestore saverestore.cpp ../../ISLE/android/saverestore.cpp)
target_compile_features(saverestore PRIVATE cxx_std_17)
target_compile_options(saverestore PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(saverestore PRIVATE ../../ISLE/android)
add_test(NAME android_save_restore COMMAND saverestore)

add_executable(gamefiles gamefiles.cpp ../../ISLE/android/gamefiles.cpp ../../ISLE/android/configstore.cpp
  ../../ISLE/islefiles.cpp)
target_compile_features(gamefiles PRIVATE cxx_std_17)
target_compile_options(gamefiles PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(gamefiles PRIVATE ../../ISLE/android ../../util "${INIPARSER_INCLUDE_DIR}" "${SDL3_INCLUDE_DIR}")
target_link_libraries(gamefiles PRIVATE "${INIPARSER_LIBRARY}" Threads::Threads)
add_test(NAME android_game_files COMMAND gamefiles)

add_executable(touchactions touchactions.cpp)
target_compile_features(touchactions PRIVATE cxx_std_17)
target_compile_options(touchactions PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(touchactions PRIVATE ../../ISLE/android)
add_test(NAME android_touch_actions COMMAND touchactions)

add_executable(touchinput touchinput.cpp)
target_compile_features(touchinput PRIVATE cxx_std_17)
target_compile_options(touchinput PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(touchinput PRIVATE ../../ISLE/android "${SDL3_INCLUDE_DIR}")
add_test(NAME android_touch_input COMMAND touchinput)

add_executable(renderers renderers.cpp ../../ISLE/android/renderers.cpp)
target_compile_features(renderers PRIVATE cxx_std_17)
target_compile_options(renderers PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(renderers PRIVATE ../../ISLE/android ../../miniwin/include "${SDL3_INCLUDE_DIR}")
# Miniwin's own headers, for the real device GUID and the parse that reads it back. SYSTEM
# because the renderer headers they drag in are not written to this test's warning settings.
target_include_directories(renderers SYSTEM PRIVATE ../../miniwin/src/internal)
add_test(NAME android_renderers COMMAND renderers)

add_executable(gamepadbindings gamepadbindings.cpp)
target_compile_features(gamepadbindings PRIVATE cxx_std_17)
target_compile_options(gamepadbindings PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(gamepadbindings PRIVATE ../../ISLE "${SDL3_INCLUDE_DIR}")
add_test(NAME gamepad_bindings COMMAND gamepadbindings)

add_executable(audiofocus audiofocus.cpp)
target_compile_features(audiofocus PRIVATE cxx_std_17)
target_compile_options(audiofocus PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(audiofocus PRIVATE ../../ISLE/android)
add_test(NAME android_audio_focus COMMAND audiofocus)

add_executable(outputgain outputgain.cpp)
target_compile_features(outputgain PRIVATE cxx_std_17)
target_compile_options(outputgain PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_include_directories(outputgain PRIVATE ../../ISLE)
add_test(NAME output_gain COMMAND outputgain)

# Checks the face-button labels the default Click relies on against SDL itself. It needs a built
# SDL3, such as the desktop build's, and is skipped without one.
find_library(SDL3_LIBRARY NAMES SDL3 HINTS ../../build/_deps/sdl3-build NO_DEFAULT_PATH)
if(SDL3_LIBRARY)
  add_executable(gamepadlabels gamepadlabels.cpp)
  target_compile_features(gamepadlabels PRIVATE cxx_std_17)
  target_compile_options(gamepadlabels PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  target_include_directories(gamepadlabels PRIVATE ../../ISLE "${SDL3_INCLUDE_DIR}")
  target_link_libraries(gamepadlabels PRIVATE "${SDL3_LIBRARY}")
  add_test(NAME gamepad_labels COMMAND gamepadlabels)
elseif(ISLE_HOST_TESTS_REQUIRE_ALL)
  message(FATAL_ERROR "gamepad_labels needs a built SDL3 in build/_deps/sdl3-build")
endif()

# The plain-Java tests: the Android app's policy classes, which import nothing from Android, built
# with only the sources each test needs. They run from the repository root because several read
# the C++ side they mirror, such as ISLE/android/configstore.cpp, by relative path.
if(ISLE_HOST_TESTS_REQUIRE_ALL)
  find_package(Java 17 REQUIRED COMPONENTS Development Runtime)
else()
  find_package(Java 17 COMPONENTS Development Runtime)
endif()
if(Java_FOUND)
  include(UseJava)
  # Java 17 reads sources in the locale's encoding, which is ASCII on a bare runner.
  set(CMAKE_JAVA_COMPILE_FLAGS -encoding UTF-8)
  set(ISLE_JAVA_CONFIG ../../CONFIG/android/src/main/java/org/legoisland/isle)
  set(ISLE_JAVA_APP ../../android-project/app/src/main/java/org/legoisland/isle)
  set(ISLE_JAVA_TESTS java/org/legoisland/isle)
  function(add_java_test name class)
    add_jar(${class} SOURCES ${ISLE_JAVA_TESTS}/${class}.java ${ARGN})
    get_target_property(jar ${class} JAR_FILE)
    add_test(NAME ${name} COMMAND "${Java_JAVA_EXECUTABLE}" -ea -cp "${jar}" org.legoisland.isle.${class}
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
  endfunction()

  add_java_test(android_save_archive SaveArchiveTest ${ISLE_JAVA_CONFIG}/SaveArchive.java)
  add_java_test(android_save_export_journal SaveExportJournalTest ${ISLE_JAVA_CONFIG}/SaveArchive.java
    ${ISLE_JAVA_CONFIG}/SaveExportJournal.java ${ISLE_JAVA_TESTS}/SaveArchiveTest.java)
  add_java_test(android_save_validation SaveValidationTest ${ISLE_JAVA_CONFIG}/SaveValidation.java
    ${ISLE_JAVA_CONFIG}/SaveRestoreArchive.java)
  add_java_test(android_save_restore_gate SaveRestoreGateTest ${ISLE_JAVA_APP}/SaveRestoreGate.java)
  add_java_test(android_touch_layout TouchLayoutTest ${ISLE_JAVA_APP}/TouchLayout.java)
  add_java_test(android_controller_bindings ControllerBindingsTest ${ISLE_JAVA_CONFIG}/ControllerBindings.java)
  add_java_test(android_graphics_settings GraphicsSettingsTest ${ISLE_JAVA_CONFIG}/GraphicsSettings.java
    ${ISLE_JAVA_CONFIG}/ControllerBindings.java)
  add_java_test(android_extension_settings ExtensionSettingsTest ${ISLE_JAVA_CONFIG}/ExtensionSettings.java
    ${ISLE_JAVA_CONFIG}/ControllerBindings.java ${ISLE_JAVA_CONFIG}/GameFileCopier.java)
  add_java_test(android_game_file_copier GameFileCopierTest ${ISLE_JAVA_CONFIG}/GameFileCopier.java)
  add_java_test(android_game_files_policy GameFilesPolicyTest ${ISLE_JAVA_CONFIG}/GameFilesPolicy.java
    ${ISLE_JAVA_CONFIG}/GameFileCopier.java)
  add_java_test(android_quit_prompt_text QuitPromptTextTest ${ISLE_JAVA_APP}/QuitPromptText.java)
  add_java_test(android_tv_support TvSupportTest ${ISLE_JAVA_CONFIG}/TvSupport.java)
else()
  message(STATUS "Java 17 not found; skipping the Java tests")
endif()
