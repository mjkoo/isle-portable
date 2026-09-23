package org.legoisland.isle;

/**
 * What changes on a TV: which system pickers are real, which Back key is a remote's, and whether
 * touch controls mean anything. Plain Java so it can be tested without a device, which is why the
 * framework constants below are copied rather than imported.
 */
final class TvSupport {
    /** android.view.KeyEvent.KEYCODE_BACK. */
    static final int KEYCODE_BACK = 4;
    /** android.view.InputDevice.SOURCE_DPAD. */
    static final int SOURCE_DPAD = 0x00000201;
    /** android.view.InputDevice.SOURCE_GAMEPAD. */
    static final int SOURCE_GAMEPAD = 0x00000401;
    /** android.view.InputDevice.SOURCE_CLASS_JOYSTICK. */
    static final int SOURCE_CLASS_JOYSTICK = 0x00000010;

    /**
     * Android TV's stand-in for the system file picker. It answers the document intents, so they
     * resolve, but only shows "You don't have an app that can do this" and returns a cancel.
     */
    static final String PICKER_STUB_PACKAGE = "com.android.tv.frameworkpackagestubs";

    static final String NO_FILE_PICKER = "Needs a file picker, and this device has none.";

    static final String NO_FOLDER_PICKER = "This device has no folder picker to copy game files from. "
        + "To replace them, choose Remove game files: the game then closes, and the next launch says "
        + "where to copy the new LEGO folder with adb.";

    private TvSupport() {}

    /** Whether the activity a document intent resolved to, named by its package, can pick anything. */
    static boolean isPicker(String packageName) {
        return packageName != null && !PICKER_STUB_PACKAGE.equals(packageName);
    }

    /**
     * Whether a key is a remote's Back, which should open the game menu. SDL takes every
     * non-virtual device with a D-pad for a joystick (SDLControllerManager.isDeviceSDLJoystick)
     * and guesses a gamepad layout for it, so a remote's Back reaches the game as whichever
     * controller button that guess makes it, and never opens the menu. A real controller keeps
     * its Back, which means Esc, and a virtual device's Back already arrives as the system Back.
     */
    static boolean isRemoteBack(int keyCode, boolean virtual, int sources) {
        if (keyCode != KEYCODE_BACK || virtual) return false;
        // Both are multi-bit, sharing SOURCE_CLASS_BUTTON with the keyboard, so test all the bits.
        boolean dpad = (sources & SOURCE_DPAD) == SOURCE_DPAD;
        boolean controller = (sources & SOURCE_GAMEPAD) == SOURCE_GAMEPAD || (sources & SOURCE_CLASS_JOYSTICK) != 0;
        return dpad && !controller;
    }

    /**
     * Whether touch controls are offered. A TV has none to offer them to, even where it reports a
     * touchscreen, as the Android TV emulator does.
     */
    static boolean hasTouchControls(boolean touchscreen, boolean television) {
        return touchscreen && !television;
    }
}
