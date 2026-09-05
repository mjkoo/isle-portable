# Build recipes for isle-portable. `just --list` shows them all.

# Everything the Android build needs (JDK 17, the NDK, the CMake version the
# Android Gradle Plugin pins) lives in the flake's android devshell.
android_shell := "nix develop 'path:" + justfile_directory() + "#android' --command"

# The flags CI passes through to the native build, from .github/workflows/ci.yml.
android_cmake_args := "-DCMAKE_BUILD_TYPE=Release -DISLE_USE_DX5=false -DISLE_BUILD_CONFIG=false -DENABLE_CLANG_TIDY=false -DISLE_WERROR=true -Werror=dev"

default:
    @just --list

# Build the debug APK into android-project/app/build/outputs/apk/debug/.
[working-directory: 'android-project']
android-apk:
    {{ android_shell }} ./gradlew assembleDebug -PcmakeArgs="{{ android_cmake_args }}"

# Needs SIGNING_KEY_ALIAS, SIGNING_KEY_PASSWORD, SIGNING_STORE_FILE and
# SIGNING_STORE_PASSWORD in the environment.
[doc('Build the signed release APK')]
[working-directory: 'android-project']
android-apk-release:
    {{ android_shell }} ./gradlew packageRelease -PcmakeArgs="{{ android_cmake_args }}"

# The daemon caches the environment it was started in, so one started outside
# the devshell fails the build with "Cannot run program 'cmake'".
[doc('Stop the Gradle daemon')]
[working-directory: 'android-project']
android-stop:
    {{ android_shell }} ./gradlew --stop

# --- Local Android device (arm64 AVD on Apple Silicon) ---------------------

emulator_shell := "nix develop 'path:" + justfile_directory() + "#android-emulator' --command"
avd_name := "isle-api35"
avd_image := "system-images;android-35;google_apis;arm64-v8a"

# Create the AVD. Re-running replaces it, which wipes its userdata.
android-avd:
    # --device matters: without a profile the AVD gets a 320x640 panel that will not
    # rotate, and the game (screenOrientation=landscape) ends up squeezed into portrait.
    # avdmanager prints a devices.xml error under nix but still applies the profile.
    {{ emulator_shell }} sh -c 'echo no | avdmanager create avd --name {{ avd_name }} --package "{{ avd_image }}" --device medium_phone --force'
    sed -i.bak -e 's/^hw.initialOrientation=.*/hw.initialOrientation=landscape/' -e 's/^hw.keyboard=.*/hw.keyboard=yes/' .android/avd/{{ avd_name }}.avd/config.ini

# The AVD wants ~2 GB plus GPU buffers, and the Gradle daemon sits on another 2 GB
# (org.gradle.jvmargs=-Xmx2048m), so run `just android-stop` before a test session
# rather than building and emulating at the same time.
[doc('Boot the AVD (pass swiftshader_indirect if the host GPU fails)')]
android-emulator gpu="host":
    {{ emulator_shell }} emulator -avd {{ avd_name }} -no-snapshot -no-boot-anim -gpu {{ gpu }}

# Wait for the booted device, then install the debug APK over any existing one.
android-install:
    {{ emulator_shell }} sh -c 'adb wait-for-device && while [ -z "$(adb shell getprop sys.boot_completed | tr -d "\r")" ]; do sleep 1; done && adb install -r android-project/app/build/outputs/apk/debug/app-debug.apk'

# adb push leaves the tree owned by `shell` mode 0770, which the app's own uid cannot
# read: startup fails with "Error enumerating files ... Permission denied". Hence chmod.
[doc('Push a game data tree (a dir holding LEGO/Scripts and LEGO/data) to the device')]
android-push-data dir:
    {{ emulator_shell }} sh -c 'adb shell mkdir -p /sdcard/Android/data/org.legoisland.isle/files && adb push "{{ dir }}/LEGO" /sdcard/Android/data/org.legoisland.isle/files/ && adb shell chmod -R 777 /sdcard/Android/data/org.legoisland.isle/files/LEGO'

# Follow the game's own SDL log lines.
android-logcat:
    {{ emulator_shell }} adb logcat -v time SDL:V SDL/APP:V IsleActivity:V '*:S'
