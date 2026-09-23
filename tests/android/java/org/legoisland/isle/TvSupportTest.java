package org.legoisland.isle;

public final class TvSupportTest {
    // android.view.InputDevice sources a real device reports, spelled out rather than built from
    // TvSupport's copies so a wrong copy cannot pass.
    private static final int KEYBOARD = 0x00000101;
    private static final int DPAD = 0x00000201;
    private static final int GAMEPAD = 0x00000401;
    private static final int JOYSTICK = 0x01000010;
    private static final int MOUSE = 0x00002002;
    private static final int TOUCHSCREEN = 0x00001002;

    public static void main(String[] args) {
        boolean assertions = false;
        assert assertions = true;
        if (!assertions) throw new AssertionError("Run with java -ea");

        assert TvSupport.KEYCODE_BACK == 4;
        assert TvSupport.SOURCE_DPAD == DPAD;
        assert TvSupport.SOURCE_GAMEPAD == GAMEPAD;
        assert TvSupport.SOURCE_CLASS_JOYSTICK == 0x00000010;

        // A remote is a keyboard with a D-pad, and SDL takes it for a joystick.
        assert TvSupport.isRemoteBack(4, false, KEYBOARD | DPAD);
        assert TvSupport.isRemoteBack(4, false, DPAD);
        // The keyboard shares SOURCE_CLASS_BUTTON with the gamepad; that bit alone is not a gamepad.
        assert TvSupport.isRemoteBack(4, false, KEYBOARD | DPAD | 0x1);
        // A controller keeps Back as Esc, however it is reported.
        assert !TvSupport.isRemoteBack(4, false, GAMEPAD);
        assert !TvSupport.isRemoteBack(4, false, KEYBOARD | DPAD | GAMEPAD);
        assert !TvSupport.isRemoteBack(4, false, KEYBOARD | DPAD | JOYSTICK);
        assert !TvSupport.isRemoteBack(4, false, KEYBOARD | JOYSTICK);
        // Nothing SDL routes as a keyboard is taken: those already reach the game as system Back.
        assert !TvSupport.isRemoteBack(4, true, KEYBOARD | DPAD);
        assert !TvSupport.isRemoteBack(4, false, KEYBOARD);
        assert !TvSupport.isRemoteBack(4, false, MOUSE);
        assert !TvSupport.isRemoteBack(4, false, TOUCHSCREEN);
        assert !TvSupport.isRemoteBack(4, false, 0);
        // Only Back.
        assert !TvSupport.isRemoteBack(23, false, KEYBOARD | DPAD);
        assert !TvSupport.isRemoteBack(82, false, KEYBOARD | DPAD);
        assert !TvSupport.isRemoteBack(111, false, KEYBOARD | DPAD);

        assert TvSupport.isPicker("com.android.documentsui");
        assert TvSupport.isPicker("com.google.android.documentsui");
        assert TvSupport.isPicker("android");
        assert !TvSupport.isPicker("com.android.tv.frameworkpackagestubs");
        assert !TvSupport.isPicker(null);

        assert TvSupport.hasTouchControls(true, false);
        assert !TvSupport.hasTouchControls(true, true);
        assert !TvSupport.hasTouchControls(false, false);
        assert !TvSupport.hasTouchControls(false, true);

        System.out.println("TvSupportTest passed");
    }
}
