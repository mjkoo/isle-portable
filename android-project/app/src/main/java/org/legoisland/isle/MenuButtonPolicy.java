package org.legoisland.isle;

/**
 * How present the on-screen menu button is, from the input the player last used. A player on a
 * gamepad or keyboard has Start or Back for the menu and never needs the button, so it goes away
 * until the next touch. A player on touch keeps it, but it fades back once they leave it alone.
 *
 * Plain Java so it can be tested without a device, which is why the framework constants below are
 * copied rather than imported. Times are milliseconds from one monotonic clock. Touched on the UI
 * thread only.
 */
final class MenuButtonPolicy {
    static final int HIDDEN = 0, DIMMED = 1, FULL = 2;
    static final long DIM_AFTER_MS = 4000;
    /** The dimmed button's share of the player's touch control opacity. */
    static final float DIMMED_SHARE = 0.3f;

    /** android.view.InputDevice.SOURCE_DPAD. */
    static final int SOURCE_DPAD = 0x00000201;
    /** android.view.InputDevice.SOURCE_GAMEPAD. */
    static final int SOURCE_GAMEPAD = 0x00000401;
    /** android.view.InputDevice.SOURCE_JOYSTICK. */
    static final int SOURCE_JOYSTICK = 0x01000010;

    private boolean touching = true;
    private long lastTouch;

    MenuButtonPolicy(long now) { lastTouch = now; }

    /**
     * Whether input from a device means the player is on a controller or keyboard. The built-in
     * volume and power keys come from a keyboard that is not alphabetic, and the navigation bar's
     * Back from a virtual device, so neither counts.
     */
    static boolean isController(boolean virtual, int sources, boolean alphabeticKeyboard) {
        if (virtual) return false;
        // All three are multi-bit, sharing their class bits with other sources, so test all the bits.
        return (sources & SOURCE_GAMEPAD) == SOURCE_GAMEPAD
            || (sources & SOURCE_JOYSTICK) == SOURCE_JOYSTICK
            || (sources & SOURCE_DPAD) == SOURCE_DPAD
            || alphabeticKeyboard;
    }

    void onTouch(long now) {
        touching = true;
        lastTouch = now;
    }

    void onController() { touching = false; }

    /** The button has just come up, so a touch player gets the whole delay before it fades. */
    void onShown(long now) {
        if (touching) lastTouch = now;
    }

    int state(long now) {
        if (!touching) return HIDDEN;
        return now - lastTouch >= DIM_AFTER_MS ? DIMMED : FULL;
    }

    /** How long until the time alone changes state(), or -1 when only input can change it. */
    long nextChange(long now) {
        if (!touching) return -1;
        long left = DIM_AFTER_MS - (now - lastTouch);
        return left > 0 ? left : -1;
    }
}
