package org.legoisland.isle;

import android.app.Application;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;

import androidx.lifecycle.AndroidViewModel;
import androidx.lifecycle.MutableLiveData;

import java.io.IOException;
import java.io.File;
import java.text.DateFormat;
import java.util.Date;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;

/** Owns unconfirmed input across recreation. Confirmed bytes belong to the native journal. */
public final class SaveRestoreModel extends AndroidViewModel {
    enum Phase { LOADING, IDLE, PICKING, READING, CONFIRM, SCHEDULING, CANCELLING, CLOSING, ERROR }
    final MutableLiveData<Phase> phase = new MutableLiveData<>(Phase.LOADING);
    private final Handler main = new Handler(Looper.getMainLooper());
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private final AtomicBoolean cancelled = new AtomicBoolean();
    private boolean started;
    private volatile boolean cleared;
    private String id;
    private String unavailable = "Loading restore information...";
    private String previous = "";
    private boolean usePrevious;
    private SaveRestoreArchive.Contents contents;
    String message;

    public SaveRestoreModel(Application app) { super(app); }
    void load(String captureId, boolean wasBusy) {
        if (started) return;
        started = true;
        id = captureId;
        worker.execute(() -> {
            File[] abandoned = getApplication().getCacheDir().listFiles(
                (directory, name) -> name.startsWith("save-restore-") && name.endsWith(".zip"));
            if (abandoned != null) for (File file : abandoned) file.delete();
            String[] info;
            try { info = SettingsBridge.restoreInfo(id); }
            catch (RuntimeException | OutOfMemoryError e) { info = new String[] {"Could not read restore information.", ""}; }
            final String[] result = info;
            main.post(() -> {
                if (cleared) return;
                unavailable = result[0]; previous = result[1];
                if (wasBusy) {
                    message = "Restore selection was interrupted. Reopen the game menu and select the archive again.";
                    phase.setValue(Phase.ERROR);
                } else phase.setValue(Phase.IDLE);
            });
        });
    }
    boolean busy() { return phase.getValue() != Phase.IDLE && phase.getValue() != Phase.ERROR; }
    boolean hasPrevious() { return !previous.isEmpty(); }
    String previousSummary() {
        if (previous.isEmpty()) return "No previous save set is available for this directory.";
        return "Backup from " + previousDate() + (previous.endsWith(":empty") ? " (no saved players)." : ".");
    }
    private String previousDate() {
        return DateFormat.getDateTimeInstance().format(new Date(Long.parseLong(previous.split(":")[0])));
    }
    String summary() { return unavailable.isEmpty() ? "Replace all players and progress from an exported ZIP." : unavailable; }
    boolean start(boolean previousSet) {
        if (busy()) return false;
        if (!unavailable.isEmpty()) { error(unavailable); return false; }
        if (previousSet && previous.isEmpty()) { error(previousSummary()); return false; }
        usePrevious = previousSet;
        cancelled.set(false);
        contents = null; message = null;
        phase.setValue(previousSet ? Phase.CONFIRM : Phase.PICKING);
        return !previousSet;
    }
    String confirmation() {
        String prefix = usePrevious
            ? "Replace current saves with the backup from " + previousDate() + "? Progress made since then will be lost."
                + (previous.endsWith(":empty") ? " This returns the game to no saved players." : "")
            : contents.files.size() + " files, " + contents.players + (contents.players == 1 ? " player." : " players.")
                + "\n\nReplace all current players and progress?";
        return prefix + "\n\nThe game will close. Reopen it to restore these saves.";
    }
    void selected(Uri uri) {
        if (phase.getValue() != Phase.PICKING) return;
        if (uri == null) { cancel(); return; }
        phase.setValue(Phase.READING);
        worker.execute(() -> {
            try {
                SaveRestoreArchive.Contents result = SaveRestoreArchive.read(
                    getApplication().getContentResolver().openInputStream(uri), getApplication().getCacheDir(), cancelled::get);
                main.post(() -> {
                    if (cleared) return;
                    if (cancelled.get()) { contents = null; phase.setValue(Phase.IDLE); }
                    else { contents = result; phase.setValue(Phase.CONFIRM); }
                });
            } catch (IOException | RuntimeException | OutOfMemoryError e) {
                main.post(() -> { if (!cleared) { if (cancelled.get()) phase.setValue(Phase.IDLE); else error(e.getMessage()); } });
            }
        });
    }
    void confirm() {
        if (phase.getValue() != Phase.CONFIRM) return;
        final SaveRestoreArchive.Contents selected = contents;
        final boolean restorePrevious = usePrevious;
        phase.setValue(Phase.SCHEDULING);
        worker.execute(() -> {
            String failure;
            boolean closing;
            try {
                failure = SettingsBridge.scheduleRestore(id,
                    restorePrevious ? null : selected.files.keySet().toArray(new String[0]),
                    restorePrevious ? null : selected.files.values().toArray(new byte[0][]), restorePrevious);
                closing = SettingsBridge.startupWorkScheduled();
            } catch (RuntimeException | OutOfMemoryError e) {
                failure = e.getMessage(); closing = SettingsBridge.startupWorkScheduled();
            }
            final String result = failure;
            final boolean closeGame = closing;
            main.post(() -> {
                if (cleared) return;
                contents = null;
                if (closeGame) {
                    message = result == null ? "Restore scheduled. Reopen the game to apply it."
                        : "Restore confirmation was interrupted. Close and reopen the game to resolve it. " + result;
                    phase.setValue(Phase.CLOSING);
                } else error(result == null ? "Restore could not be scheduled." : result);
            });
        });
    }
    void cancel() {
        if (phase.getValue() == Phase.SCHEDULING || phase.getValue() == Phase.CLOSING) return;
        cancelled.set(true);
        if (phase.getValue() == Phase.READING || phase.getValue() == Phase.CANCELLING) phase.setValue(Phase.CANCELLING);
        else { contents = null; phase.setValue(Phase.IDLE); }
    }
    void error(String text) { contents = null; message = text; phase.setValue(Phase.ERROR); }
    void acknowledge() { message = null; phase.setValue(Phase.IDLE); }
    @Override protected void onCleared() {
        cleared = true; cancelled.set(true); contents = null; worker.shutdown();
    }
}
