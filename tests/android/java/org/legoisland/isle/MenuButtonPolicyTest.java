package org.legoisland.isle;

public final class MenuButtonPolicyTest {
    public static void main(String[] args) {
        boolean assertions = false;
        assert assertions = true;
        if (!assertions) throw new AssertionError("Run with java -ea");

        long delay = MenuButtonPolicy.DIM_AFTER_MS;

        // A touch device starts with the button up, and it fades once left alone.
        MenuButtonPolicy fresh = new MenuButtonPolicy(1000);
        assert fresh.state(1000) == MenuButtonPolicy.FULL;
        assert fresh.nextChange(1000) == delay;
        assert fresh.state(1000 + delay - 1) == MenuButtonPolicy.FULL;
        assert fresh.nextChange(1000 + delay - 1) == 1;
        assert fresh.state(1000 + delay) == MenuButtonPolicy.DIMMED;
        assert fresh.nextChange(1000 + delay) == -1;

        // Any touch brings it back to full and restarts the fade.
        fresh.onTouch(20000);
        assert fresh.state(20000) == MenuButtonPolicy.FULL;
        assert fresh.nextChange(20500) == delay - 500;

        // Coming up after a while away, such as behind the menu, restarts the fade.
        MenuButtonPolicy returning = new MenuButtonPolicy(0);
        assert returning.state(delay) == MenuButtonPolicy.DIMMED;
        returning.onShown(30000);
        assert returning.state(30000) == MenuButtonPolicy.FULL;

        // Controller input takes it away entirely, whatever the clock says, until the next touch.
        MenuButtonPolicy pad = new MenuButtonPolicy(0);
        pad.onController();
        assert pad.state(0) == MenuButtonPolicy.HIDDEN;
        assert pad.state(delay * 10) == MenuButtonPolicy.HIDDEN;
        assert pad.nextChange(0) == -1;
        pad.onShown(40000);
        assert pad.state(40000) == MenuButtonPolicy.HIDDEN;
        pad.onTouch(50000);
        assert pad.state(50000) == MenuButtonPolicy.FULL;
        assert pad.state(50000 + delay) == MenuButtonPolicy.DIMMED;

        // Real values from InputDevice: a pad reports SOURCE_GAMEPAD | SOURCE_JOYSTICK, and a
        // phone's built-in keys (volume, power) a keyboard that is not alphabetic.
        int keyboard = 0x00000101, touchscreen = 0x00001002;
        int gamepad = MenuButtonPolicy.SOURCE_GAMEPAD | MenuButtonPolicy.SOURCE_JOYSTICK | keyboard;
        assert MenuButtonPolicy.isController(false, gamepad, false);
        assert MenuButtonPolicy.isController(false, MenuButtonPolicy.SOURCE_JOYSTICK, false);
        assert MenuButtonPolicy.isController(false, MenuButtonPolicy.SOURCE_DPAD | keyboard, false);
        assert MenuButtonPolicy.isController(false, keyboard, true);
        assert !MenuButtonPolicy.isController(false, keyboard, false);
        assert !MenuButtonPolicy.isController(false, touchscreen, false);
        // The navigation bar's Back, and anything adb injects, arrive from a virtual device.
        assert !MenuButtonPolicy.isController(true, gamepad, true);
    }
}
