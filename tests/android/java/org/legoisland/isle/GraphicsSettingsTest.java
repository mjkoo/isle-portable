package org.legoisland.isle;

import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class GraphicsSettingsTest {
    private static String[] values(String key) {
        return GraphicsSettings.VALUES[Arrays.asList(GraphicsSettings.KEYS).indexOf(key)];
    }

    public static void main(String[] args) throws Exception {
        boolean assertions = false;
        assert assertions = true;
        if (!assertions) throw new AssertionError("Run with java -ea");

        assert GraphicsSettings.KEYS.length == 6 && GraphicsSettings.TITLES.length == 6;
        assert GraphicsSettings.LABELS.length == 6 && GraphicsSettings.VALUES.length == 6;
        assert GraphicsSettings.WHOLE.length == 6;

        // Every row matches the native validator's table, run from the repository root: its range
        // holds every listed value and it agrees on which rows are whole numbers.
        String store = new String(Files.readAllBytes(Paths.get("ISLE/android/configstore.cpp")), StandardCharsets.UTF_8);
        Matcher entry = Pattern.compile("\\{\"(isle:[^\"]+)\", ([0-9.]+), ([0-9.]+), (true|false)\\}").matcher(store);
        Map<String, double[]> ranges = new HashMap<>();
        Map<String, Boolean> whole = new HashMap<>();
        while (entry.find()) {
            ranges.put(entry.group(1), new double[] {Double.parseDouble(entry.group(2)), Double.parseDouble(entry.group(3))});
            whole.put(entry.group(1), Boolean.parseBoolean(entry.group(4)));
        }
        assert ranges.size() == GraphicsSettings.KEYS.length : ranges.keySet();
        for (int i = 0; i < GraphicsSettings.KEYS.length; i++) {
            String key = GraphicsSettings.KEYS[i];
            assert GraphicsSettings.LABELS[i].length == GraphicsSettings.VALUES[i].length : key;
            assert ranges.containsKey(key) : key;
            assert whole.get(key) == GraphicsSettings.WHOLE[i] : key;
            for (String value : GraphicsSettings.VALUES[i]) {
                double number = Double.parseDouble(value);
                assert number >= ranges.get(key)[0] && number <= ranges.get(key)[1] : key + " " + value;
                assert !GraphicsSettings.WHOLE[i] || value.matches("0|[1-9][0-9]*") : key + " " + value;
            }
            // Reset these settings clears every key that is not a controller key.
            assert !ControllerBindings.isControllerKey(key) : key;
        }

        // What the desktop tool marks broken is never offered.
        assert Arrays.equals(values("isle:island quality"), new String[] {"1", "2"});
        String[] transitions = values("isle:transition type");
        assert !Arrays.asList(transitions).contains("0") && !Arrays.asList(transitions).contains("6");
        // A fresh configuration stores the game's own default of 10, so it is listed too.
        assert Arrays.equals(values("isle:frame delta"), new String[] {"31", "14", "10"});

        // Values the game and the desktop tool write as "%f" show as their listed entry.
        assert GraphicsSettings.normalize("isle:max lod", "3.600000").equals("3.6");
        assert GraphicsSettings.normalize("isle:max lod", "3.500000").equals("3.5");
        assert GraphicsSettings.normalize("isle:max lod", "6.000000").equals("6");
        assert GraphicsSettings.normalize("isle:frame delta", "10.000000").equals("10");
        assert GraphicsSettings.normalize("isle:frame delta", "31.000000").equals("31");
        for (String key : GraphicsSettings.KEYS) {
            for (String value : values(key)) assert GraphicsSettings.normalize(key, value).equals(value) : key;
        }
        // Whole-number rows are shown as written, since the game would read some forms differently.
        assert GraphicsSettings.normalize("isle:max allowed extras", "010").equals("010");
        assert GraphicsSettings.normalize("isle:transition type", "0.2e1").equals("0.2e1");
        assert GraphicsSettings.normalize("isle:island quality", "2.0").equals("2.0");
        // Anything else is kept as written, so Settings shows it as the current value.
        assert GraphicsSettings.normalize("isle:max lod", "3.7").equals("3.7");
        assert GraphicsSettings.normalize("isle:frame delta", "16.666666").equals("16.666666");
        assert GraphicsSettings.normalize("isle:transition type", "0").equals("0");
        assert GraphicsSettings.normalize("isle:max lod", "garbage").equals("garbage");
        assert GraphicsSettings.normalize("isle:max lod", "NaN").equals("NaN");
        assert GraphicsSettings.normalize("isle:max lod", null) == null;
        assert GraphicsSettings.normalize("isle:msaa", "4.000000").equals("4.000000");
    }
}
