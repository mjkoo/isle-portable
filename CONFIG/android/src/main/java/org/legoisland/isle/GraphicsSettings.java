package org.legoisland.isle;

import java.util.Arrays;

/**
 * Settings rows for the game's graphics options, which apply on the next launch. The keys, their
 * ranges and which rows are whole numbers are validated in ISLE/android/configstore.cpp; keep them
 * in step.
 */
final class GraphicsSettings {
    static final String[] KEYS = {
        "isle:island quality", "isle:island texture", "isle:max lod",
        "isle:max allowed extras", "isle:transition type", "isle:frame delta"
    };
    static final String[] TITLES = {
        "Model quality", "Texture quality", "Level of detail", "Maximum actors", "Transition", "Frame rate limit"
    };
    static final String[][] LABELS = {
        {"Medium", "High"},
        {"Fast", "High"},
        {"1.5", "2.5", "3.5", "3.6", "4.5", "6"},
        {"5", "10", "20", "30", "40"},
        {"No animation", "Dissolve", "Mosaic", "Wipe down", "Windows"},
        {"30 fps", "60 fps", "90 fps"}
    };
    static final String[][] VALUES = {
        // The desktop tool marks Low (0) broken.
        {"1", "2"},
        {"0", "1"},
        // 3.5 is what the desktop tool's high preset writes, 3.6 the game's own default.
        {"1.5", "2.5", "3.5", "3.6", "4.5", "6"},
        {"5", "10", "20", "30", "40"},
        // The desktop tool marks idle (0) and the last type (6) broken; 6 also locks the game up.
        {"1", "2", "3", "4", "5"},
        // The game starts a frame once more than this many milliseconds have passed and checks about
        // every millisecond, so a frame starts up to two milliseconds after the value. That matters
        // only when a frame spans several refreshes: 31 keeps frames within two 60 Hz refreshes
        // (33.3 ms) and 14 within two 120 Hz refreshes (16.7 ms). At one frame per refresh the game
        // waits for the refresh anyway, so 14 keeps up with a 60 Hz display and 10, the game's own
        // default that a fresh configuration stores, with a 90 Hz one.
        {"31", "14", "10"}
    };
    /** Rows the game reads as integers, with strtol's base detection. */
    static final boolean[] WHOLE = {true, true, false, true, true, false};

    private GraphicsSettings() {}

    /**
     * The game and the desktop tool write fractional values as "%f"; Settings shows those as their
     * listed entry. Whole-number rows are shown as written, since the game's strtol would read "010"
     * as 8.
     */
    static String normalize(String key, String value) {
        int row = Arrays.asList(KEYS).indexOf(key);
        if (value == null || row < 0 || WHOLE[row]) return value;
        double number;
        try {
            number = Double.parseDouble(value);
        } catch (NumberFormatException e) {
            return value;
        }
        for (String listed : VALUES[row]) {
            if (Double.parseDouble(listed) == number) return listed;
        }
        return value;
    }
}
