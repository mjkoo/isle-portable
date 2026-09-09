package org.legoisland.isle;

import android.app.Dialog;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.Toast;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.fragment.app.DialogFragment;
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
        new Control("Input", "isle:show touch controls", "Show touch controls", BOOL_LABELS, BOOL_VALUES),
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
        enum State { LOADING, READY, SAVING, SAVED, ERROR }

        final MutableLiveData<State> state = new MutableLiveData<>(State.LOADING);
        final Handler main = new Handler(Looper.getMainLooper());
        final Map<String, String> original = new LinkedHashMap<>();
        final Map<String, String> draft = new LinkedHashMap<>();
        final ExecutorService worker = Executors.newSingleThreadExecutor();
        String[] renderers = new String[0];
        String error;
        String configPath;
        boolean started;
        boolean loaded;

        boolean isBusy() { return state.getValue() == State.LOADING || state.getValue() == State.SAVING; }

        void load(Bundle saved, android.content.Intent intent) {
            if (started) return;
            started = true;
            configPath = intent.getStringExtra("configPath");
            String[] choices = intent.getStringArrayExtra("renderers");
            renderers = choices == null ? new String[0] : choices;
            if (saved != null) {
                for (String key : saved.keySet()) draft.put(key, saved.getString(key));
            }
            worker.execute(() -> {
                try {
                    ArrayList<String> keys = new ArrayList<>();
                    for (Control control : CONTROLS) {
                        if (!RESOLUTION.equals(control.key)) keys.add(control.key);
                    }
                    keys.add(WIDTH);
                    keys.add(HEIGHT);
                    String[] values = SettingsBridge.read(configPath, keys.toArray(new String[0]));
                    main.post(() -> {
                        for (int i = 0; i < values.length; i++) {
                            String key = keys.get(i);
                            original.put(key, values[i]);
                            if (!draft.containsKey(key)) draft.put(key, values[i]);
                        }
                        loaded = true;
                        state.setValue(State.READY);
                    });
                } catch (RuntimeException e) {
                    main.post(() -> {
                        error = e.toString();
                        state.setValue(State.ERROR);
                    });
                }
            });
        }

        void save() {
            if (!loaded || isBusy()) return;
            ArrayList<String> keys = new ArrayList<>();
            ArrayList<String> values = new ArrayList<>();
            for (String key : original.keySet()) {
                if (!Objects.equals(original.get(key), draft.get(key))) {
                    keys.add(key);
                    values.add(draft.get(key));
                }
            }
            state.setValue(State.SAVING);
            worker.execute(() -> {
                String failure;
                try {
                    failure = keys.isEmpty() ? null : SettingsBridge.write(
                        configPath, keys.toArray(new String[0]), values.toArray(new String[0]), renderers);
                } catch (RuntimeException e) {
                    failure = e.toString();
                }
                final String message = failure;
                main.post(() -> {
                    error = message;
                    state.setValue(message == null ? State.SAVED : State.ERROR);
                });
            });
        }

        @Override protected void onCleared() { worker.shutdown(); }
    }

    public static final class ErrorDialog extends DialogFragment {
        @Override public Dialog onCreateDialog(Bundle savedInstanceState) {
            return new AlertDialog.Builder(requireContext()).setTitle("Could not update settings")
                .setMessage(requireArguments().getString("message")).setPositiveButton("OK", null).create();
        }
    }

    private SettingsModel model;
    private SaveExportModel export;
    private SaveRestoreModel restore;
    private final ActivityResultLauncher<String> exportDestination = registerForActivityResult(
        new ActivityResultContracts.CreateDocument("application/zip"), uri -> export.destination(uri));

    private final ActivityResultLauncher<String[]> restoreSource = registerForActivityResult(
        new ActivityResultContracts.OpenDocument(), uri -> restore.selected(uri));

    private void startRestore(boolean previous) {
        if (!model.original.equals(model.draft)) {
            Toast.makeText(this, "Choose Save or Cancel for your settings edits, then reopen Settings to restore saves.",
                Toast.LENGTH_LONG).show();
            return;
        }
        if (export.isBusy() || model.isBusy()) return;
        if (restore.start(previous)) {
            try { restoreSource.launch(new String[] {"application/zip", "application/x-zip-compressed", "application/octet-stream"}); }
            catch (RuntimeException e) { restore.error("Could not open the archive picker: " + e.getMessage()); }
        }
    }

    public static final class RestoreDialog extends DialogFragment {
        @Override public Dialog onCreateDialog(Bundle state) {
            SaveRestoreModel model = new ViewModelProvider(requireActivity()).get(SaveRestoreModel.class);
            SaveRestoreModel.Phase phase = model.phase.getValue();
            AlertDialog.Builder builder = new AlertDialog.Builder(requireContext()).setTitle("Restore saves");
            if (phase == SaveRestoreModel.Phase.CONFIRM) {
                builder.setMessage(model.confirmation());
                builder.setPositiveButton("Replace and close", (dialog, which) -> model.confirm());
                builder.setNegativeButton("Cancel", (dialog, which) -> model.cancel());
            } else if (phase == SaveRestoreModel.Phase.ERROR) {
                builder.setMessage(model.message).setPositiveButton("OK", (dialog, which) -> model.acknowledge());
            } else {
                builder.setMessage(phase == SaveRestoreModel.Phase.SCHEDULING ? "Recording restore request..."
                    : phase == SaveRestoreModel.Phase.CANCELLING ? "Cancelling restore..." : "Reading and checking save files...");
                if (phase == SaveRestoreModel.Phase.READING) {
                    builder.setNegativeButton("Cancel", (dialog, which) -> model.cancel());
                }
            }
            setCancelable(false);
            return builder.create();
        }
    }

    private void renderRestoreUi() {
        if (isFinishing() || isDestroyed() || getSupportFragmentManager().isStateSaved()) return;
        SaveRestoreModel.Phase phase = restore.phase.getValue();
        RestoreDialog previous = (RestoreDialog) getSupportFragmentManager().findFragmentByTag("save-restore");
        if (previous != null && previous.requireArguments().getString("phase").equals(phase.name())) return;
        if (previous != null) {
            previous.dismiss();
            getSupportFragmentManager().executePendingTransactions();
        }
        if (phase == SaveRestoreModel.Phase.CLOSING) {
            Toast.makeText(this, restore.message, Toast.LENGTH_LONG).show();
            finish();
        } else if (phase == SaveRestoreModel.Phase.CONFIRM || phase == SaveRestoreModel.Phase.READING
                || phase == SaveRestoreModel.Phase.SCHEDULING || phase == SaveRestoreModel.Phase.CANCELLING
                || (phase == SaveRestoreModel.Phase.ERROR && restore.message != null)) {
            RestoreDialog dialog = new RestoreDialog();
            Bundle arguments = new Bundle(); arguments.putString("phase", phase.name()); dialog.setArguments(arguments);
            dialog.showNow(getSupportFragmentManager(), "save-restore");
        }
        invalidateOptionsMenu();
    }

    public static final class ExportDialog extends DialogFragment {
        @Override public Dialog onCreateDialog(Bundle savedInstanceState) {
            SaveExportModel model = new ViewModelProvider(requireActivity()).get(SaveExportModel.class);
            SaveExportModel.Phase phase = model.phase.getValue();
            AlertDialog.Builder builder = new AlertDialog.Builder(requireContext()).setTitle("Export saves");
            if (phase == SaveExportModel.Phase.CONFIRM) {
                builder.setMessage(model.warning() + "Export the available files?");
                builder.setPositiveButton("Export", (dialog, which) -> model.prepare());
                builder.setNegativeButton("Cancel", (dialog, which) -> model.cancel());
            } else if (phase == SaveExportModel.Phase.DONE || phase == SaveExportModel.Phase.ERROR) {
                builder.setMessage(model.message).setPositiveButton("OK", (dialog, which) -> model.acknowledge());
            } else {
                builder.setMessage(phase == SaveExportModel.Phase.TRANSFERRING ? "Exporting save files..."
                    : phase == SaveExportModel.Phase.CANCELLING ? "Cancelling export..." : "Preparing save archive...");
                if (phase != SaveExportModel.Phase.CANCELLING) {
                    builder.setNegativeButton("Cancel", (dialog, which) -> model.cancel());
                }
            }
            setCancelable(false);
            return builder.create();
        }
    }

    private void updateExportUi() {
        new Handler(Looper.getMainLooper()).post(this::renderExportUi);
    }

    private void renderExportUi() {
        if (isFinishing() || isDestroyed()) return;
        invalidateOptionsMenu();
        if (getSupportFragmentManager().isStateSaved()) return;
        SaveExportModel.Phase phase = export.phase.getValue();
        ExportDialog previous = (ExportDialog) getSupportFragmentManager().findFragmentByTag("save-export");
        if (previous != null && phase.name().equals(previous.requireArguments().getString("phase"))) return;
        if (previous != null) {
            previous.dismiss();
            getSupportFragmentManager().executePendingTransactions();
        }
        if (phase == SaveExportModel.Phase.READY) {
            export.picking();
            try { exportDestination.launch(export.filename()); }
            catch (RuntimeException e) { export.pickerFailed("Could not open the save picker: " + e.getMessage()); }
        } else if (phase == SaveExportModel.Phase.CONFIRM || phase == SaveExportModel.Phase.PREPARING
                || phase == SaveExportModel.Phase.TRANSFERRING || phase == SaveExportModel.Phase.CANCELLING
                || ((phase == SaveExportModel.Phase.DONE || phase == SaveExportModel.Phase.ERROR) && export.message != null)) {
            ExportDialog dialog = new ExportDialog();
            Bundle arguments = new Bundle();
            arguments.putString("phase", phase.name());
            dialog.setArguments(arguments);
            dialog.showNow(getSupportFragmentManager(), "save-export");
        }
    }

    @Override protected void onPostResume() {
        super.onPostResume();
        updateExportUi();
        new Handler(Looper.getMainLooper()).post(this::renderRestoreUi);
    }

    @Override protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setTitle("LEGO Island Settings");
        getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        model = new ViewModelProvider(this).get(SettingsModel.class);
        restore = new ViewModelProvider(this).get(SaveRestoreModel.class);
        restore.load(getIntent().getStringExtra("exportId"), savedInstanceState != null && savedInstanceState.getBoolean("restoreBusy"));
        restore.phase.observe(this, ignored -> new Handler(Looper.getMainLooper()).post(this::renderRestoreUi));
        export = new ViewModelProvider(this).get(SaveExportModel.class);
        export.load(getIntent().getStringExtra("exportId"), savedInstanceState != null && savedInstanceState.getBoolean("exportBusy"));
        export.phase.observe(this, ignored -> updateExportUi());
        model.state.observe(this, result -> {
            invalidateOptionsMenu();
            if (result == SettingsModel.State.SAVED) {
                setResult(RESULT_OK);
                Toast.makeText(this, "Settings saved. Changes apply on the next game launch.", Toast.LENGTH_LONG).show();
                finish();
            } else if (model.error != null) {
                String message = model.error;
                model.error = null;
                Bundle arguments = new Bundle();
                arguments.putString("message", message);
                ErrorDialog dialog = new ErrorDialog();
                dialog.setArguments(arguments);
                dialog.show(getSupportFragmentManager(), "settings-error");
            }
        });
        if (savedInstanceState == null) {
            getSupportFragmentManager().beginTransaction()
                .replace(android.R.id.content, new SettingsFragment()).commit();
        }
        model.load(savedInstanceState == null ? null : savedInstanceState.getBundle("draft"), getIntent());
    }

    @Override protected void onSaveInstanceState(Bundle state) {
        state.putBoolean("restoreBusy", restore.busy());
        state.putBoolean("exportBusy", export.isBusy());
        if (!model.draft.isEmpty()) {
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
        menu.findItem(1).setEnabled(model.loaded && !model.isBusy() && !export.isBusy() && !restore.busy());
        menu.findItem(2).setEnabled(!model.isBusy() && !export.isBusy() && !restore.busy());
        return super.onPrepareOptionsMenu(menu);
    }

    @Override public boolean onOptionsItemSelected(MenuItem item) {
        if (export.isBusy() || restore.busy()) return true;
        if (item.getItemId() == 1) { model.save(); return true; }
        if (item.getItemId() == 2 || item.getItemId() == android.R.id.home) { onBackPressed(); return true; }
        return super.onOptionsItemSelected(item);
    }

    @Override public void onBackPressed() {
        if (!model.isBusy() && !export.isBusy() && !restore.busy()) super.onBackPressed();
    }

    public static final class SettingsFragment extends PreferenceFragmentCompat {
        private SettingsModel model;
        private SaveExportModel export;
        private SaveRestoreModel restore;
        private boolean updating;

        @Override public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
            model = new ViewModelProvider(requireActivity()).get(SettingsModel.class);
            export = new ViewModelProvider(requireActivity()).get(SaveExportModel.class);
            restore = new ViewModelProvider(requireActivity()).get(SaveRestoreModel.class);
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
                    if (updating) return;
                    value = DEFAULT.equals(value) ? null : value;
                    if (RESOLUTION.equals(key)) {
                        String[] pair = value == null ? new String[] {null, null} : value.split("x");
                        model.draft.put(WIDTH, "default".equals(pair[0]) ? null : pair[0]);
                        model.draft.put(HEIGHT, "default".equals(pair[1]) ? null : pair[1]);
                    } else model.draft.put(key, value);
                }
            });
            buildPreferences();
            model.state.observe(this, ignored -> refresh());
            export.phase.observe(this, ignored -> refresh());
            restore.phase.observe(this, ignored -> refresh());
        }

        private void buildPreferences() {
            PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(requireContext());
            setPreferenceScreen(screen);
            Preference notice = new Preference(requireContext());
            notice.setTitle("Changes apply on the next game launch");
            notice.setSummary("Save keeps your changes. Cancel leaves your configuration unchanged.");
            notice.setSelectable(false);
            notice.setIconSpaceReserved(false);
            screen.addPreference(notice);
            PreferenceCategory data = new PreferenceCategory(requireContext());
            data.setTitle("Data");
            data.setIconSpaceReserved(false);
            screen.addPreference(data);
            Preference exportSaves = new Preference(requireContext());
            exportSaves.setKey("export-saves");
            exportSaves.setTitle("Export saves");
            exportSaves.setIconSpaceReserved(false);
            exportSaves.setOnPreferenceClickListener(p -> { export.start(); return true; });
            data.addPreference(exportSaves);
            Preference restoreSaves = new Preference(requireContext());
            restoreSaves.setKey("restore-saves"); restoreSaves.setTitle("Restore saves");
            restoreSaves.setIconSpaceReserved(false);
            restoreSaves.setOnPreferenceClickListener(p -> { ((SettingsActivity) requireActivity()).startRestore(false); return true; });
            data.addPreference(restoreSaves);
            Preference previousSaves = new Preference(requireContext());
            previousSaves.setKey("previous-saves"); previousSaves.setTitle("Restore previous saves");
            previousSaves.setIconSpaceReserved(false);
            previousSaves.setOnPreferenceClickListener(p -> { ((SettingsActivity) requireActivity()).startRestore(true); return true; });
            data.addPreference(previousSaves);
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
                            if (!Double.isInfinite(number) && !Double.isNaN(number) && number >= 0.1 && number <= 20) {
                                edit.setText(Double.toString(number));
                                return false;
                            }
                        } catch (NumberFormatException ignored) { }
                        Toast.makeText(requireContext(), "Enter a number from 0.1 to 20.", Toast.LENGTH_SHORT).show();
                        return false;
                    });
                    edit.setSummaryProvider(p -> DEFAULT.equals(edit.getText()) ? "Game default" : edit.getText());
                    preference = edit;
                } else {
                    ListPreference list = new ListPreference(requireContext());
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
                updating = true;
                category.addPreference(preference);
                updating = false;
            }
            Preference reset = new Preference(requireContext());
            reset.setTitle("Reset these settings");
            reset.setIconSpaceReserved(false);
            reset.setSummary("Use game defaults for Input, Audio and Display. Paths and other settings are kept. Choose Save to apply.");
            reset.setOnPreferenceClickListener(p -> {
                for (String key : model.draft.keySet()) model.draft.put(key, null);
                refresh();
                return true;
            });
            screen.addPreference(reset);
            refresh();
        }

        private void refresh() {
            Preference exportSaves = findPreference("export-saves");
            exportSaves.setSummary(export.summary());
            exportSaves.setEnabled(!export.isBusy() && !restore.busy() && !model.isBusy());
            Preference restoreSaves = findPreference("restore-saves");
            restoreSaves.setSummary(restore.summary());
            restoreSaves.setEnabled(!export.isBusy() && !restore.busy() && !model.isBusy());
            Preference previousSaves = findPreference("previous-saves");
            previousSaves.setSummary(restore.previousSummary());
            previousSaves.setVisible(restore.hasPrevious());
            previousSaves.setEnabled(!export.isBusy() && !restore.busy() && !model.isBusy());
            updating = true;
            for (Control control : CONTROLS) {
                Preference preference = findPreference(control.key);
                String current = getPreferenceManager().getPreferenceDataStore().getString(control.key, DEFAULT);
                if (preference instanceof EditTextPreference) {
                    ((EditTextPreference) preference).setText(current);
                } else {
                    ListPreference list = (ListPreference) preference;
                    ArrayList<String> labels = new ArrayList<>(Arrays.asList("Game default"));
                    ArrayList<String> values = new ArrayList<>(Arrays.asList(DEFAULT));
                    if ("isle:3d device id".equals(control.key)) {
                        for (int i = 0; i + 1 < model.renderers.length; i += 2) {
                            labels.add(model.renderers[i]);
                            values.add(model.renderers[i + 1]);
                        }
                    } else {
                        labels.addAll(Arrays.asList(control.labels));
                        values.addAll(Arrays.asList(control.values));
                    }
                    if (!values.contains(current)) { labels.add("Current: " + current); values.add(current); }
                    list.setEntries(labels.toArray(new String[0]));
                    list.setEntryValues(values.toArray(new String[0]));
                    list.setValue(current);
                }
            }
            updating = false;
            getPreferenceScreen().setEnabled(model.loaded && !model.isBusy() && !restore.busy() && !export.isBusy());
        }
    }
}
