package org.legoisland.isle;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public final class TouchLayoutTest {
    private static int[] bounds(TouchLayout layout, int control, int[] safe, float density) {
        int[] out = new int[4];
        layout.bounds(control, safe[0], safe[1], safe[2], safe[3], density, out);
        return out;
    }

    private static TouchLayout positions(String menu, String escape, String space) {
        return TouchLayout.parse(new String[] {null, null, menu, escape, space});
    }

    public static void main(String[] args) {
        boolean assertions = false;
        assert assertions = true;
        if (!assertions) throw new AssertionError("Run with java -ea");

        // Defaults reproduce the fixed RelativeLayout: the menu inset from the right and top
        // edges, then Esc and Space chained to its left.
        for (float density : new float[] {1, 1.5f, 2.625f, 3}) {
            int size = (int) (48 * density + 0.5f), margin = (int) (8 * density + 0.5f);
            int actionWidth = (int) (64 * density + 0.5f);
            for (int[] insets : new int[][] {{0, 0, 0, 0}, {0, 24, 0, 0}, {80, 0, 96, 32}}) {
                int width = 2400, height = 1080;
                int[] safe = {insets[0], insets[1], width - insets[2], height - insets[3]};
                int menuRight = width - (margin + insets[2]), top = margin + insets[1];
                int[] menu = bounds(TouchLayout.DEFAULT, TouchLayout.MENU, safe, density);
                assert menu[0] == menuRight - size && menu[1] == top;
                assert menu[2] == menuRight && menu[3] == top + size;
                int escapeRight = menuRight - size - margin;
                int[] escape = bounds(TouchLayout.DEFAULT, TouchLayout.ESCAPE, safe, density);
                assert escape[0] == escapeRight - actionWidth && escape[1] == top;
                assert escape[2] == escapeRight && escape[3] == top + size;
                int spaceRight = escapeRight - actionWidth - margin;
                int[] space = bounds(TouchLayout.DEFAULT, TouchLayout.SPACE, safe, density);
                assert space[0] == spaceRight - actionWidth && space[1] == top;
                assert space[2] == spaceRight && space[3] == top + size;
            }
        }

        // Every button stays inside the safe area at the extremes of position and size.
        int[] safe = {100, 40, 2300, 1000};
        float density = 2.625f;
        for (String scale : new String[] {"0.5", "1", "2"}) {
            TouchLayout layout = TouchLayout.parse(new String[] {scale, null, "1,1", "0,0", "0.5,0"});
            for (int control = 0; control < TouchLayout.COUNT; control++) {
                int[] box = bounds(layout, control, safe, density);
                assert box[0] >= safe[0] && box[1] >= safe[1] && box[2] <= safe[2] && box[3] <= safe[3];
            }
            int[] corner = bounds(layout, TouchLayout.MENU, safe, density);
            assert corner[2] == safe[2] && corner[3] == safe[3];
            int[] origin = bounds(layout, TouchLayout.ESCAPE, safe, density);
            assert origin[0] == safe[0] && origin[1] == safe[1];
            TouchLayout defaults = TouchLayout.parse(new String[] {scale, null, null, null, null});
            for (int control = 0; control < TouchLayout.COUNT; control++) {
                int[] box = bounds(defaults, control, safe, density);
                assert box[0] >= safe[0] && box[1] >= safe[1] && box[2] <= safe[2] && box[3] <= safe[3];
            }
        }

        // A stored center reproduces the box it was taken from.
        for (float x : new float[] {0.1f, 0.37f, 0.5f, 0.93f}) {
            for (float y : new float[] {0.2f, 0.5f, 0.8f}) {
                int[] box = bounds(TouchLayout.DEFAULT.withPosition(TouchLayout.SPACE, x, y), TouchLayout.SPACE,
                    safe, density);
                float fx = TouchLayout.fraction((box[0] + box[2]) / 2f, safe[0], safe[2]);
                float fy = TouchLayout.fraction((box[1] + box[3]) / 2f, safe[1], safe[3]);
                String stored = TouchLayout.DEFAULT.withPosition(TouchLayout.SPACE, fx, fy)
                    .positionValue(TouchLayout.SPACE);
                int[] again = bounds(positions(null, null, stored), TouchLayout.SPACE, safe, density);
                assert Math.abs(again[0] - box[0]) <= 1 && Math.abs(again[1] - box[1]) <= 1;
            }
        }

        // A safe area smaller than a button aligns it to the top-left rather than failing.
        int[] tiny = {10, 20, 30, 40};
        int[] cramped = bounds(TouchLayout.DEFAULT, TouchLayout.MENU, tiny, 3);
        assert cramped[0] == 10 && cramped[1] == 20;
        cramped = bounds(positions("1,1", null, null), TouchLayout.MENU, tiny, 3);
        assert cramped[0] == 10 && cramped[1] == 20;
        assert TouchLayout.fraction(5, 10, 10) == 0.5f;
        assert TouchLayout.fraction(-50, 0, 100) == 0 && TouchLayout.fraction(150, 0, 100) == 1;

        // Each unusable value falls back on its own.
        TouchLayout mixed = TouchLayout.parse(new String[] {"3", "abc", "0.5", "0.2000,0.3000", "0.5,0.5,0.5"});
        assert mixed.scale == 1 && mixed.opacity == 1;
        assert mixed.isDefault(TouchLayout.MENU) && mixed.isDefault(TouchLayout.SPACE);
        assert "0.2000,0.3000".equals(mixed.positionValue(TouchLayout.ESCAPE));
        for (String invalid : new String[] {"", ",", "NaN,0", "0,Infinity", "-0.1,0.5", "0.5,1.5", "0.5,"}) {
            assert positions(invalid, null, null).isDefault(TouchLayout.MENU) : invalid;
        }
        TouchLayout edges = TouchLayout.parse(new String[] {"0.5", "0.1", null, null, null});
        assert edges.scale == 0.5f && edges.opacity == 0.1f;
        edges = TouchLayout.parse(new String[] {"2", "1", null, null, null});
        assert edges.scale == 2 && edges.opacity == 1;
        edges = TouchLayout.parse(new String[] {"0.49", "0.09", null, null, null});
        assert edges.scale == 1 && edges.opacity == 1;

        // Stored positions never depend on the device's decimal separator.
        Locale previous = Locale.getDefault();
        Locale.setDefault(Locale.GERMANY);
        try {
            String value = TouchLayout.DEFAULT.withPosition(TouchLayout.MENU, .25f, .75f).positionValue(TouchLayout.MENU);
            assert "0.2500,0.7500".equals(value) : value;
        } finally {
            Locale.setDefault(previous);
        }

        // Only positions that changed are written; returning to the default removes the key.
        TouchLayout original = positions("0.2500,0.7500", null, "0.9000,0.1000");
        TouchLayout draft = original.withPosition(TouchLayout.ESCAPE, .5f, .5f)
            .withPosition(TouchLayout.SPACE, .9f, .1f);
        List<String> keys = new ArrayList<>(), values = new ArrayList<>();
        draft.changedPositions(original, keys, values);
        assert keys.equals(List.of(TouchLayout.POSITION_KEYS[TouchLayout.ESCAPE]));
        assert values.equals(List.of("0.5000,0.5000"));
        keys.clear();
        values.clear();
        draft.withDefaultPositions().changedPositions(original, keys, values);
        assert keys.equals(List.of(TouchLayout.POSITION_KEYS[TouchLayout.MENU],
            TouchLayout.POSITION_KEYS[TouchLayout.SPACE]));
        assert values.size() == 2 && values.get(0) == null && values.get(1) == null;

        // A reloaded size and opacity apply without discarding positions being edited.
        TouchLayout appearance = TouchLayout.parse(new String[] {"1.5", "0.25", null, null, null});
        TouchLayout merged = draft.withAppearanceOf(appearance);
        assert merged.scale == 1.5f && merged.opacity == 0.25f;
        assert "0.5000,0.5000".equals(merged.positionValue(TouchLayout.ESCAPE));
        System.out.println("Touch layout tests passed");
    }
}
