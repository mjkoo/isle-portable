package org.legoisland.isle;

public final class MenuButtonPolicyTest {
    public static void main(String[] args) {
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
    }
}
