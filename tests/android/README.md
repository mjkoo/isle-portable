# Android configuration tests

These host tests exercise the native configuration store without Android or SDL. They cover
unrelated-key preservation, absent defaults, scoped resets, concurrent updates, failed writes,
malformed files and settings validation.

After building the desktop project with its fetched iniparser dependency:

```sh
cmake -S tests/android -B build/android-config-tests \
  -DINIPARSER_INCLUDE_DIR="$PWD/build/_deps/iniparser-src/src" \
  -DINIPARSER_LIBRARY="$PWD/build/_deps/iniparser-build/libiniparser.a"
cmake --build build/android-config-tests
ctest --test-dir build/android-config-tests --output-on-failure
```

Alternatively, provide an installed iniparser through CMake's search paths. On this project's
Nix setup, run the commands inside `nix develop`.

The controls and lifecycle integration also need Android verification: menu-button and Back
access, Save/Cancel, next-launch application, renderer switching, startup-error recovery,
activity recreation, background/foreground, low memory and held input. Check the release
shrinker output as well as the debug APK because the settings bridge is reached through JNI.
