{
  description = "isle-portable development shells";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      nixpkgs,
      flake-utils,
      ...
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = import nixpkgs {
          inherit system;
          config = {
            allowUnfree = true;
            android_sdk.accept_license = true;
          };
        };

        # Versions the Android Gradle Plugin in android-project/ asks for:
        # cmake is pinned by `externalNativeBuild.cmake.version` in app/build.gradle,
        # the NDK is AGP 8.7's default, and compileSdk/targetSdk are 35.
        android = pkgs.androidenv.composeAndroidPackages {
          platformVersions = [ "35" ];
          buildToolsVersions = [
            "35.0.0"
            "34.0.0"
          ];
          includeNDK = true;
          ndkVersions = [ "27.0.12077973" ];
          includeCmake = true;
          cmakeVersions = [ "3.30.5" ];
        };

        # The same package set plus the emulator and an arm64 system image, so a device
        # can be brought up locally. Kept out of `android` so a plain APK build does not
        # have to fetch ~1.6 GiB it never uses; the build-tools, NDK and platform
        # derivations underneath are shared between the two.
        androidEmulator = pkgs.androidenv.composeAndroidPackages {
          platformVersions = [ "35" ];
          buildToolsVersions = [
            "35.0.0"
            "34.0.0"
          ];
          includeNDK = true;
          ndkVersions = [ "27.0.12077973" ];
          includeCmake = true;
          cmakeVersions = [ "3.30.5" ];

          includeEmulator = true;
          # Newest emulator in the pinned repo.json with a darwin_aarch64 archive.
          emulatorVersion = "37.2.4";
          includeSystemImages = true;
          # google_apis rather than google_apis_playstore: it is a userdebug build, so
          # `adb root` works and the game data can be pushed under Android/data.
          systemImageTypes = [ "google_apis" ];
          abiVersions = [ "arm64-v8a" ];
        };

        # An Android TV image, for the leanback launcher and a device with no touchscreen and
        # no folder picker. Its own set because the pinned repo.json has android-tv images for
        # API 34 and 36 but not 35, and every image type is fetched for every platform version.
        androidTvEmulator = pkgs.androidenv.composeAndroidPackages {
          platformVersions = [ "34" ];
          includeEmulator = true;
          emulatorVersion = "37.2.4";
          includeSystemImages = true;
          systemImageTypes = [ "android-tv" ];
          abiVersions = [ "arm64-v8a" ];
        };

        androidSdkRoot = "${android.androidsdk}/libexec/android-sdk";
        androidEmulatorSdkRoot = "${androidEmulator.androidsdk}/libexec/android-sdk";
        androidTvEmulatorSdkRoot = "${androidTvEmulator.androidsdk}/libexec/android-sdk";

        # The SDK itself lives read-only in the store, but AVDs, the adb key and the
        # emulator's scratch state all need somewhere writable. ANDROID_HOME stays on
        # the store path so an AVD's image.sysdir.1 still resolves.
        emulatorShellHook = ''
          repoRoot="$(git rev-parse --show-toplevel 2>/dev/null || echo "$PWD")"
          export ANDROID_USER_HOME="''${ANDROID_USER_HOME:-$repoRoot/.android}"
          export ANDROID_AVD_HOME="''${ANDROID_AVD_HOME:-$ANDROID_USER_HOME/avd}"
          export ANDROID_EMULATOR_HOME="$ANDROID_USER_HOME"
          mkdir -p "$ANDROID_AVD_HOME"
        '';
      in
      {
        devShells.android = pkgs.mkShell {
          packages = [
            pkgs.temurin-bin-17 # CI builds Android with Java 17
            android.androidsdk
            # SDL3's build-release.py builds the SDL3 .aar, and app/build.gradle
            # invokes it as `python`, which nixpkgs' python3 does not provide.
            (pkgs.writeShellScriptBin "python" ''exec ${pkgs.python3}/bin/python3 "$@"'')
            pkgs.git
          ];

          ANDROID_HOME = androidSdkRoot;
          ANDROID_SDK_ROOT = androidSdkRoot;
          ANDROID_NDK_HOME = "${androidSdkRoot}/ndk-bundle";
          JAVA_HOME = pkgs.temurin-bin-17.home;

          shellHook = ''
            # The downloadSDL3 gradle task shells out to `cmake`, and AGP pins
            # the version, so put the SDK's own copy (and its ninja) on PATH.
            export PATH="${androidSdkRoot}/cmake/3.30.5/bin:$PATH"
          '';
        };

        # adb, avdmanager and emulator, for running the game on a local arm64 AVD.
        devShells.android-emulator = pkgs.mkShell {
          packages = [
            pkgs.temurin-bin-17
            androidEmulator.androidsdk
            pkgs.git
          ];

          ANDROID_HOME = androidEmulatorSdkRoot;
          ANDROID_SDK_ROOT = androidEmulatorSdkRoot;
          JAVA_HOME = pkgs.temurin-bin-17.home;

          shellHook = emulatorShellHook;
        };

        # The same tools against the Android TV image. An AVD only boots from the shell whose
        # SDK holds its image, so the TV AVD is run from here rather than from android-emulator.
        devShells.android-tv-emulator = pkgs.mkShell {
          packages = [
            pkgs.temurin-bin-17
            androidTvEmulator.androidsdk
            pkgs.git
          ];

          ANDROID_HOME = androidTvEmulatorSdkRoot;
          ANDROID_SDK_ROOT = androidTvEmulatorSdkRoot;
          JAVA_HOME = pkgs.temurin-bin-17.home;

          shellHook = emulatorShellHook;
        };

        devShells.default = pkgs.mkShell {
          packages = [
            pkgs.cmake
            pkgs.ninja
            pkgs.pkg-config
          ];
        };
      }
    );
}
