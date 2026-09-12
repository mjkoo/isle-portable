package org.legoisland.isle;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

public final class ControllerBindingsTest {
    private static Map<String, String> draft(String... pairs) {
        Map<String, String> draft = new HashMap<>();
        for (int i = 0; i < pairs.length; i += 2) draft.put(pairs[i], pairs[i + 1]);
        return draft;
    }

    public static void main(String[] args) {
        boolean assertions = false;
        assert assertions = true;
        if (!assertions) throw new AssertionError("Run with java -ea");

        // One row per bindable input, in the native table's order.
        assert ControllerBindings.KEYS.length == 13 && ControllerBindings.TITLES.length == 13;
        assert ControllerBindings.KEYS[0].equals("gamepad:south");
        assert ControllerBindings.KEYS[6].equals("gamepad:lefttrigger");
        assert ControllerBindings.KEYS[12].equals("gamepad:guide");
        for (String key : ControllerBindings.KEYS) assert ControllerBindings.isControllerKey(key);
        assert ControllerBindings.isControllerKey(ControllerBindings.CONFIRM);
        assert !ControllerBindings.isControllerKey("isle:touch scheme");
        assert ControllerBindings.ACTION_LABELS.length == ControllerBindings.ACTION_VALUES.length;
        assert Arrays.asList(ControllerBindings.ACTION_VALUES)
            .containsAll(Arrays.asList("click", "space", "escape", "pause", "menu", "none"));
        assert ControllerBindings.CONFIRM_LABELS.length == ControllerBindings.CONFIRM_VALUES.length;

        // Defaults as the game resolves them on Android.
        String face = "Game default (Click if labelled A, otherwise Space)";
        assert ControllerBindings.defaultLabel("gamepad:south", null).equals(face);
        assert ControllerBindings.defaultLabel("gamepad:east", "label").equals(face);
        assert ControllerBindings.defaultLabel("gamepad:south", "south").equals("Game default (Click)");
        assert ControllerBindings.defaultLabel("gamepad:east", "south").equals("Game default (Space)");
        assert ControllerBindings.defaultLabel("gamepad:south", "east").equals("Game default (Space)");
        assert ControllerBindings.defaultLabel("gamepad:east", "east").equals("Game default (Click)");
        // An unusable confirm value falls back as the game's does.
        assert ControllerBindings.defaultLabel("gamepad:south", "jump").equals(face);
        assert ControllerBindings.defaultLabel("gamepad:start", null).equals("Game default (Open menu)");
        assert ControllerBindings.defaultLabel("gamepad:back", null).equals("Game default (Esc)");
        assert ControllerBindings.defaultLabel("gamepad:righttrigger", null).equals("Game default (Click)");
        assert ControllerBindings.defaultLabel("gamepad:north", null).equals("Game default (Nothing)");
        assert ControllerBindings.defaultLabel(ControllerBindings.CONFIRM, null)
            .equals("Game default (Button labelled A)");

        // Hand-edited values are shown and compared in lowercase.
        assert ControllerBindings.normalize("gamepad:south", "Click").equals("click");
        assert ControllerBindings.normalize("gamepad:confirm", "EAST").equals("east");
        assert ControllerBindings.normalize("isle:3d device id", "GLES3").equals("GLES3");
        assert ControllerBindings.normalize("gamepad:south", null) == null;

        // Settings warns when no controller button would open the menu.
        assert !ControllerBindings.menuUnbound(draft());
        assert !ControllerBindings.menuUnbound(draft("gamepad:start", "menu"));
        assert ControllerBindings.menuUnbound(draft("gamepad:start", "pause"));
        assert !ControllerBindings.menuUnbound(draft("gamepad:start", "pause", "gamepad:guide", "menu"));
        assert ControllerBindings.menuUnbound(draft("gamepad:start", "none", "gamepad:north", "space"));
        // The game ignores an unusable value and keeps Start's default, the menu.
        assert !ControllerBindings.menuUnbound(draft("gamepad:start", "jump"));
    }
}
