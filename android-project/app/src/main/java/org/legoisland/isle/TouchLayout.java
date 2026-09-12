package org.legoisland.isle;

import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import java.util.Objects;

/**
 * The size, opacity and placement of the on-screen touch buttons.
 *
 * A moved button is stored as its center, in fractions of the safe area: the window less its
 * system bars and display cutout. Fractions rather than pixels keep a layout usable after a
 * resolution change or on another device, and clamping to the safe area on every layout pass
 * keeps a button from ending up under a cutout. A button without a stored position keeps its
 * default place in the top-right corner.
 *
 * Free of Android classes so the geometry can be tested on the host.
 */
final class TouchLayout {
    static final int MENU = 0, ESCAPE = 1, SPACE = 2, COUNT = 3;

    static final String SCALE_KEY = "isle:touch button scale";
    static final String OPACITY_KEY = "isle:touch control opacity";
    static final String[] POSITION_KEYS = {
        "isle:touch menu position", "isle:touch escape position", "isle:touch space position"};
    /** Every key parse reads, in the order it expects their values. */
    static final String[] KEYS = {SCALE_KEY, OPACITY_KEY, POSITION_KEYS[MENU], POSITION_KEYS[ESCAPE],
        POSITION_KEYS[SPACE]};

    // The ranges ISLE/android/configstore.cpp accepts on write.
    static final double MIN_SCALE = 0.5, MAX_SCALE = 2, MIN_OPACITY = 0.1, MAX_OPACITY = 1;

    private static final int[] WIDTH_DP = {48, 64, 64};
    private static final int HEIGHT_DP = 48, MARGIN_DP = 8;

    static final TouchLayout DEFAULT = new TouchLayout(1, 1, defaultPositions());

    final float scale, opacity;
    // Center x and y for each button, or NaN for its default place.
    private final float[] positions;

    private TouchLayout(float scale, float opacity, float[] positions) {
        this.scale = scale;
        this.opacity = opacity;
        this.positions = positions;
    }

    private static float[] defaultPositions() {
        float[] positions = new float[COUNT * 2];
        Arrays.fill(positions, Float.NaN);
        return positions;
    }

    /**
     * Reads values in KEYS order, null for an absent key. Each unusable value falls back to its
     * own default rather than discarding the rest of the layout.
     */
    static TouchLayout parse(String[] values) {
        float scale = number(values[0], MIN_SCALE, MAX_SCALE, 1);
        float opacity = number(values[1], MIN_OPACITY, MAX_OPACITY, 1);
        float[] positions = defaultPositions();
        for (int i = 0; i < COUNT; i++) {
            String value = values[2 + i];
            if (value == null) continue;
            String[] parts = value.split(",", -1);
            if (parts.length != 2) continue;
            float x = number(parts[0], 0, 1, Float.NaN), y = number(parts[1], 0, 1, Float.NaN);
            if (Float.isNaN(x) || Float.isNaN(y)) continue;
            positions[2 * i] = x;
            positions[2 * i + 1] = y;
        }
        return new TouchLayout(scale, opacity, positions);
    }

    private static float number(String text, double min, double max, float fallback) {
        if (text == null) return fallback;
        try {
            double value = Double.parseDouble(text);
            // NaN fails both comparisons.
            if (value >= min && value <= max) return (float) value;
        } catch (NumberFormatException ignored) { }
        return fallback;
    }

    boolean isDefault(int control) { return Float.isNaN(positions[2 * control]); }

    /** The stored form of a button's position, or null for its default place. */
    String positionValue(int control) {
        if (isDefault(control)) return null;
        // ROOT: a comma-decimal locale would otherwise write "0,5000,0,2500".
        return String.format(Locale.ROOT, "%.4f,%.4f", positions[2 * control], positions[2 * control + 1]);
    }

    TouchLayout withPosition(int control, float x, float y) {
        float[] next = positions.clone();
        next[2 * control] = clamp(x);
        next[2 * control + 1] = clamp(y);
        return new TouchLayout(scale, opacity, next);
    }

    TouchLayout withDefaultPositions() { return new TouchLayout(scale, opacity, defaultPositions()); }

    /** These positions with another layout's size and opacity. */
    TouchLayout withAppearanceOf(TouchLayout other) {
        return new TouchLayout(other.scale, other.opacity, positions);
    }

    /** Adds each position key whose stored form differs from original's, with its new value. */
    void changedPositions(TouchLayout original, List<String> keys, List<String> values) {
        for (int i = 0; i < COUNT; i++) {
            String value = positionValue(i);
            if (!Objects.equals(value, original.positionValue(i))) {
                keys.add(POSITION_KEYS[i]);
                values.add(value);
            }
        }
    }

    // The same rounding the fixed layout used, so the default size is unchanged to the pixel.
    static int width(int control, float density, float scale) {
        return (int) (WIDTH_DP[control] * density * scale + 0.5f);
    }

    static int height(float density, float scale) { return (int) (HEIGHT_DP * density * scale + 0.5f); }

    /** Writes a button's left, top, right and bottom, kept inside the safe area, into out. */
    void bounds(int control, int safeLeft, int safeTop, int safeRight, int safeBottom, float density, int[] out) {
        int width = width(control, density, scale), height = height(density, scale);
        int left, top;
        if (isDefault(control)) {
            // Menu in the top-right corner, then Esc and Space to its left, each a margin apart.
            // The margins do not scale, so larger buttons stay tucked into the corner.
            int margin = (int) (MARGIN_DP * density + 0.5f);
            int right = safeRight - margin;
            for (int i = MENU; i < control; i++) right -= width(i, density, scale) + margin;
            left = right - width;
            top = safeTop + margin;
        } else {
            left = Math.round(safeLeft + positions[2 * control] * (safeRight - safeLeft) - width / 2f);
            top = Math.round(safeTop + positions[2 * control + 1] * (safeBottom - safeTop) - height / 2f);
        }
        // A button larger than the safe area aligns to its left or top edge.
        left = Math.max(safeLeft, Math.min(left, safeRight - width));
        top = Math.max(safeTop, Math.min(top, safeBottom - height));
        out[0] = left;
        out[1] = top;
        out[2] = left + width;
        out[3] = top + height;
    }

    /** A button center as a fraction of one safe-area dimension, clamped to it. */
    static float fraction(float center, int safeStart, int safeEnd) {
        if (safeEnd <= safeStart) return 0.5f;
        return clamp((center - safeStart) / (safeEnd - safeStart));
    }

    private static float clamp(float value) { return Math.max(0, Math.min(value, 1)); }
}
