package org.legoisland.isle;

import android.os.Bundle;
import android.text.InputType;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.Toast;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.lifecycle.MutableLiveData;
import androidx.lifecycle.ViewModel;
import androidx.lifecycle.ViewModelProvider;
import androidx.preference.EditTextPreference;
import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceDataStore;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class SettingsActivity extends AppCompatActivity {
    private static final String DEFAULT = "@default";
    private static final String RESOLUTION = "resolution";
    private static final String WIDTH = "isle:horizontal resolution";
    private static final String HEIGHT = "isle:vertical resolution";

    private static final class Control {
        final String group, key, title;
        final String[] labels, values;

        Control(String group, String key, String title, String[] labels, String[] values) {
            this.group = group;
            this.key = key;
            this.title = title;
            this.labels = labels;
            this.values = values;
        }
    }

    private static final String[] BOOL_LABELS = {"On", "Off"};
    private static final String[] BOOL_VALUES = {"true", "false"};
    // The UI's complete INI mapping. Engine defaults remain in native configuration loading.
    private static final Control[] CONTROLS = {
        new Control("Input", "isle:touch scheme", "Touch scheme",
            new String[] {"Virtual mouse", "Arrow-key regions", "Virtual stick", "Disabled"},
            new String[] {"0", "1", "2", "-1"}),
        new Control("Input", "isle:cursor sensitivity", "Cursor sensitivity", null, null),
        new Control("Input", "isle:haptic", "Haptics", BOOL_LABELS, BOOL_VALUES),
        new Control("Input", "isle:wasd", "WASD", BOOL_LABELS, BOOL_VALUES),
        new Control("Audio", "isle:music", "Music", BOOL_LABELS, BOOL_VALUES),
        new Control("Audio", "isle:3dsound", "3D sound", BOOL_LABELS, BOOL_VALUES),
        new Control("Display", RESOLUTION, "Render resolution",
            new String[] {"640 × 480", "800 × 600", "1024 × 768", "1280 × 960"},
            new String[] {"640x480", "800x600", "1024x768", "1280x960"}),
        new Control("Display", "isle:3d device id", "Renderer", new String[0], new String[0]),
        new Control("Display", "isle:msaa", "MSAA",
            new String[] {"Off", "2×", "4×", "8×", "16×"}, new String[] {"0", "2", "4", "8", "16"}),
        new Control("Display", "isle:anisotropic", "Anisotropic filtering",
            new String[] {"Off", "2×", "4×", "8×", "16×"}, new String[] {"0", "2", "4", "8", "16"})
    };

    public static final class SettingsModel extends ViewModel {
        final MutableLiveData<Integer> state = new MutableLiveData<>();
        final Map<String, String> original = new LinkedHashMap<>();
        final Map<String, String> draft = new LinkedHashMap<>();
        final ExecutorService worker = Executors.newSingleThreadExecutor();
        String[] renderers = new String[0];
        String error;
        String configPath;
        boolean started;
        volatile boolean busy, loaded;

        void load(Bundle saved, android.content.Intent intent) {
            if (started) return;
            started = true;
            configPath = intent.getStringExtra("configPath");
            String[] choices = intent.getStringArrayExtra("renderers");
            renderers = choices == null ? new String[0] : choices;
            busy = true;
            worker.execute(() -> {
                try {
                    ArrayList<String> keys = new ArrayList<>();
                    for (Control control : CONTROLS) {
                        if (!RESOLUTION.equals(control.key)) keys.add(control.key);
                    }
                    keys.add(WIDTH);
                    keys.add(HEIGHT);
                    String[] values = SettingsBridge.read(configPath, keys.toArray(new String[0]));
                    for (int i = 0; i < values.length; i++) original.put(keys.get(i), values[i]);
                    draft.putAll(original);
                    if (saved != null) {
                        for (String key : original.keySet()) {
                            if (saved.containsKey(key)) draft.put(key, saved.getString(key));
                        }
                    }
                    loaded = true;
                } catch (RuntimeException e) {
                    error = e.getMessage();
                }
                busy = false;
                state.postValue(0);
            });
        }

        void save() {
            if (!loaded || busy) return;
            ArrayList<String> keys = new ArrayList<>();
            ArrayList<String> values = new ArrayList<>();
            for (String key : original.keySet()) {
                if (!Objects.equals(original.get(key), draft.get(key))) {
                    keys.add(key);
                    values.add(draft.get(key));
                }
            }
            busy = true;
            state.setValue(0);
            worker.execute(() -> {
                try {
                    error = keys.isEmpty() ? null : SettingsBridge.write(
                        configPath, keys.toArray(new String[0]), values.toArray(new String[0]), renderers);
                } catch (RuntimeException e) {
                    error = e.getMessage();
                }
                busy = false;
                state.postValue(error == null ? 1 : 0);
            });
        }

        @Override protected void onCleared() { worker.shutdown(); }
    }

    private SettingsModel model;

    @Override protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setTitle("LEGO Island Settings");
        getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        model = new ViewModelProvider(this).get(SettingsModel.class);
        model.state.observe(this, result -> {
            invalidateOptionsMenu();
            if (result == 1) {
                setResult(RESULT_OK);
                Toast.makeText(this, "Settings saved. Changes apply on the next game launch.", Toast.LENGTH_LONG).show();
                finish();
            } else if (model.error != null) {
                String message = model.error;
                model.error = null;
                new AlertDialog.Builder(this).setTitle("Could not update settings")
                    .setMessage(message).setPositiveButton("OK", null).show();
            }
        });
        if (savedInstanceState == null) {
            getSupportFragmentManager().beginTransaction()
                .replace(android.R.id.content, new SettingsFragment()).commit();
        }
        model.load(savedInstanceState == null ? null : savedInstanceState.getBundle("draft"), getIntent());
    }

    @Override protected void onSaveInstanceState(Bundle state) {
        if (model.loaded) {
            Bundle draft = new Bundle();
            for (Map.Entry<String, String> entry : model.draft.entrySet()) draft.putString(entry.getKey(), entry.getValue());
            state.putBundle("draft", draft);
        }
        super.onSaveInstanceState(state);
    }

    @Override public boolean onCreateOptionsMenu(Menu menu) {
        menu.add(0, 1, 0, "Save").setShowAsAction(MenuItem.SHOW_AS_ACTION_IF_ROOM);
        menu.add(0, 2, 1, "Cancel").setShowAsAction(MenuItem.SHOW_AS_ACTION_IF_ROOM);
        return true;
    }

    @Override public boolean onPrepareOptionsMenu(Menu menu) {
        menu.findItem(1).setEnabled(model.loaded && !model.busy);
        menu.findItem(2).setEnabled(!model.busy);
        return super.onPrepareOptionsMenu(menu);
    }

    @Override public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == 1) { model.save(); return true; }
        if (item.getItemId() == 2 || item.getItemId() == android.R.id.home) { onBackPressed(); return true; }
        return super.onOptionsItemSelected(item);
    }

    @Override public void onBackPressed() {
        if (!model.busy) super.onBackPressed();
    }

    public static final class SettingsFragment extends PreferenceFragmentCompat {
        private SettingsModel model;

        @Override public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
            model = new ViewModelProvider(requireActivity()).get(SettingsModel.class);
            getPreferenceManager().setPreferenceDataStore(new PreferenceDataStore() {
                @Override public String getString(String key, String fallback) {
                    if (RESOLUTION.equals(key)) {
                        String x = model.draft.get(WIDTH), y = model.draft.get(HEIGHT);
                        return x == null && y == null ? DEFAULT : (x == null ? "default" : x) + "x" + (y == null ? "default" : y);
                    }
                    String value = model.draft.get(key);
                    return value == null ? DEFAULT : value;
                }
                @Override public void putString(String key, String value) {
                    value = DEFAULT.equals(value) ? null : value;
                    if (RESOLUTION.equals(key)) {
                        String[] pair = value == null ? new String[] {null, null} : value.split("x");
                        model.draft.put(WIDTH, "default".equals(pair[0]) ? null : pair[0]);
                        model.draft.put(HEIGHT, "default".equals(pair[1]) ? null : pair[1]);
                    } else model.draft.put(key, value);
                }
            });
            rebuild();
            model.state.observe(this, ignored -> { if (model.loaded) rebuild(); });
        }

        private void rebuild() {
            PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(requireContext());
            setPreferenceScreen(screen);
            Preference notice = new Preference(requireContext());
            notice.setTitle("Changes apply on the next game launch");
            notice.setSummary("Save keeps your changes. Cancel leaves your configuration unchanged.");
            notice.setSelectable(false);
            notice.setIconSpaceReserved(false);
            screen.addPreference(notice);
            if (!model.loaded) return;
            String group = "";
            PreferenceCategory category = null;
            for (Control control : CONTROLS) {
                if (!group.equals(control.group)) {
                    category = new PreferenceCategory(requireContext());
                    category.setTitle(control.group);
                    category.setIconSpaceReserved(false);
                    screen.addPreference(category);
                    group = control.group;
                }
                Preference preference;
                if (control.labels == null) {
                    EditTextPreference edit = new EditTextPreference(requireContext());
                    edit.setDialogMessage("Enter a value from 0.1 to 20, or leave blank for the game default.");
                    edit.setOnBindEditTextListener(field -> {
                        field.setInputType(InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_DECIMAL);
                        if (DEFAULT.equals(field.getText().toString())) field.setText("");
                    });
                    edit.setOnPreferenceChangeListener((p, value) -> {
                        String text = value.toString().trim();
                        if (text.isEmpty()) { edit.setText(DEFAULT); return false; }
                        try {
                            double number = Double.parseDouble(text);
                            if (!Double.isInfinite(number) && !Double.isNaN(number) && number >= 0.1 && number <= 20) return true;
                        } catch (NumberFormatException ignored) { }
                        Toast.makeText(requireContext(), "Enter a number from 0.1 to 20.", Toast.LENGTH_SHORT).show();
                        return false;
                    });
                    edit.setSummaryProvider(p -> DEFAULT.equals(edit.getText()) ? "Game default" : edit.getText());
                    preference = edit;
                } else {
                    ListPreference list = new ListPreference(requireContext());
                    ArrayList<String> labels = new ArrayList<>(Arrays.asList("Game default"));
                    ArrayList<String> values = new ArrayList<>(Arrays.asList(DEFAULT));
                    if ("isle:3d device id".equals(control.key)) {
                        for (int i = 0; i < model.renderers.length; i += 2) {
                            labels.add(model.renderers[i]); values.add(model.renderers[i + 1]);
                        }
                    } else {
                        labels.addAll(Arrays.asList(control.labels)); values.addAll(Arrays.asList(control.values));
                    }
                    String current = getPreferenceManager().getPreferenceDataStore().getString(control.key, DEFAULT);
                    if (!values.contains(current)) { labels.add("Current: " + current); values.add(current); }
                    list.setEntries(labels.toArray(new String[0]));
                    list.setEntryValues(values.toArray(new String[0]));
                    list.setSummaryProvider(ListPreference.SimpleSummaryProvider.getInstance());
                    if ("isle:touch scheme".equals(control.key)) {
                        list.setSummaryProvider(p -> list.getEntry() + " (no control overlay)");
                    }
                    preference = list;
                }
                preference.setIconSpaceReserved(false);
                preference.setKey(control.key);
                preference.setTitle(control.title);
                preference.setDefaultValue(DEFAULT);
                category.addPreference(preference);
            }
            Preference reset = new Preference(requireContext());
            reset.setTitle("Reset these settings");
            reset.setIconSpaceReserved(false);
            reset.setSummary("Use game defaults for Input, Audio and Display. Paths and other settings are kept. Choose Save to apply.");
            reset.setOnPreferenceClickListener(p -> {
                for (String key : model.draft.keySet()) model.draft.put(key, null);
                rebuild();
                return true;
            });
            screen.addPreference(reset);
            screen.setEnabled(!model.busy);
        }
    }
}
