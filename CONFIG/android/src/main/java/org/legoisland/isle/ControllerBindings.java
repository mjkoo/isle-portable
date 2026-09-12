package org.legoisland.isle;

import java.util.Arrays;
import java.util.Locale;
import java.util.Map;

/**
 * Settings rows for the isle.ini [gamepad] section, with the game's defaults on Android. The keys,
 * values and defaults mirror ISLE/gamepadbindings.h; keep them in step.
 */
final class ControllerBindings {
    static final String CONFIRM = "gamepad:confirm";
    /** Physical inputs, named by position, in the order the rows appear. */
    static final String[] KEYS = {
        "gamepad:south", "gamepad:east", "gamepad:west", "gamepad:north",
        "gamepad:leftshoulder", "gamepad:rightshoulder", "gamepad:lefttrigger", "gamepad:righttrigger",
        "gamepad:leftstick", "gamepad:rightstick", "gamepad:back", "gamepad:start", "gamepad:guide"
    };
    static final String[] TITLES = {
        "Bottom face button", "Right face button", "Left face button", "Top face button",
        "Left shoulder", "Right shoulder", "Left trigger", "Right trigger",
        "Left stick click", "Right stick click", "Back / Select", "Start", "Guide / Home"
    };
    static final String[] ACTION_VALUES = {"click", "space", "escape", "pause", "menu", "none"};
    static final String[] ACTION_LABELS = {"Click", "Space", "Esc", "Pause", "Open menu", "Nothing"};
    static final String[] CONFIRM_VALUES = {"label", "south", "east"};
    static final String[] CONFIRM_LABELS = {"Button labelled A", "Bottom face button", "Right face button"};

    private ControllerBindings() {}

    static boolean isControllerKey(String key) {
        return key.startsWith("gamepad:");
    }

    /** Hand-edited values may use any letter case; Settings shows and compares them in lowercase. */
    static String normalize(String key, String value) {
        return value != null && isControllerKey(key) ? value.toLowerCase(Locale.ROOT) : value;
    }

    /** The "Game default" entry for a row, given the draft's confirm value, which may be unset. */
    static String defaultLabel(String key, String confirm) {
        return "Game default (" + defaultAction(key, confirm) + ")";
    }

    private static String defaultAction(String key, String confirm) {
        switch (key) {
            case CONFIRM:
                return CONFIRM_LABELS[0];
            case "gamepad:south":
            case "gamepad:east": {
                boolean east = key.equals("gamepad:east");
                if ("south".equals(confirm)) return east ? "Space" : "Click";
                if ("east".equals(confirm)) return east ? "Click" : "Space";
                return "Click if labelled A, otherwise Space";
            }
            case "gamepad:righttrigger":
                return "Click";
            case "gamepad:back":
                return "Esc";
            case "gamepad:start":
                return "Open menu";
            default:
                return "Nothing";
        }
    }

    /** Whether no controller button would open the menu, so Settings can warn before saving. */
    static boolean menuUnbound(Map<String, String> draft) {
        for (String key : KEYS) {
            if ("menu".equals(draft.get(key))) return false;
        }
        // The game ignores an unusable value, which leaves Start opening the menu.
        String start = draft.get("gamepad:start");
        return start != null && Arrays.asList(ACTION_VALUES).contains(start);
    }
}
