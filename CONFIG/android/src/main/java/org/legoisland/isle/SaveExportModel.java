package org.legoisland.isle;

import android.app.Application;
import android.content.ContentResolver;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.provider.DocumentsContract;

import androidx.lifecycle.AndroidViewModel;
import androidx.lifecycle.MutableLiveData;

import java.io.File;
import java.io.IOException;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Arrays;
import java.util.Date;
import java.util.Locale;
import java.util.concurrent.CancellationException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;

/** Owns one export across Settings recreation; workers hold only application context. */
public final class SaveExportModel extends AndroidViewModel {
    enum Phase { LOADING, IDLE, CONFIRM, PREPARING, READY, PICKING, TRANSFERRING, CANCELLING, DONE, ERROR }

    final MutableLiveData<Phase> phase = new MutableLiveData<>(Phase.LOADING);
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private final Handler main = new Handler(Looper.getMainLooper());
    private final AtomicBoolean cancelled = new AtomicBoolean();
    private final SharedPreferences journal;
    private final ContentResolver resolver;
    private final File cache;
    private boolean started;
    private String id;
    private String[] info = {"Loading save snapshot...", "0", ""};
    private File archive;
    String message;

    public SaveExportModel(Application application) {
        super(application);
        journal = application.getSharedPreferences("save-export", 0);
        resolver = application.getContentResolver();
        cache = application.getCacheDir();
    }

    void load(String snapshotId, boolean wasBusy) {
        if (started) return;
        started = true;
        id = snapshotId;
        worker.execute(() -> {
            String interrupted = null;
            if (journal.getBoolean("active", false) || wasBusy) {
                interrupted = "Export was interrupted. Reopen the game and its menu to capture saves again.";
                String destination = journal.getString("uri", null);
                if (destination != null) interrupted += deleteDestination(Uri.parse(destination));
                journal.edit().clear().commit();
            }
            File[] leftovers = cache.listFiles((dir, name) -> name.startsWith("save-export-") && name.endsWith(".zip"));
            if (leftovers != null) for (File file : leftovers) file.delete();
            String[] metadata;
            try { metadata = SettingsBridge.exportInfo(id); }
            catch (RuntimeException | OutOfMemoryError e) { metadata = new String[] {"Could not load the save snapshot.", "0", ""}; }
            final String[] result = metadata;
            final String failure = interrupted;
            main.post(() -> {
                info = result;
                message = failure;
                phase.setValue(failure == null ? Phase.IDLE : Phase.ERROR);
            });
        });
    }

    boolean isBusy() {
        Phase current = phase.getValue();
        return current == Phase.LOADING || current == Phase.CONFIRM || current == Phase.PREPARING || current == Phase.READY
            || current == Phase.PICKING || current == Phase.TRANSFERRING || current == Phase.CANCELLING;
    }

    String summary() {
        if (!info[0].isEmpty()) return info[0];
        if (info.length == 3) return "No saves to export.";
        return "Exports save files captured when this menu opened: "
            + DateFormat.getDateTimeInstance().format(new Date(Long.parseLong(info[1]))) + ".";
    }

    String warning() { return info[2]; }

    String filename() {
        return "lego-island-saves-" + new SimpleDateFormat("yyyyMMdd-HHmmss", Locale.ROOT)
            .format(new Date(Long.parseLong(info[1]))) + ".zip";
    }

    void start() {
        if (isBusy()) return;
        message = null;
        if (!info[0].isEmpty() || info.length == 3) {
            message = summary();
            phase.setValue(Phase.ERROR);
        } else if (!warning().isEmpty()) phase.setValue(Phase.CONFIRM);
        else prepare();
    }

    void prepare() {
        if (phase.getValue() != Phase.CONFIRM && isBusy()) return;
        cancelled.set(false);
        phase.setValue(Phase.PREPARING);
        worker.execute(() -> {
            try {
                SaveArchive.checkCancelled(cancelled::get);
                if (!journal.edit().putBoolean("active", true).remove("uri").commit()) {
                    throw new IOException("Could not record export state. Try again.");
                }
                archive = SaveArchive.create(cache, Arrays.copyOfRange(info, 3, info.length),
                    SettingsBridge.exportData(id), cancelled::get);
                main.post(() -> {
                    if (cancelled.get()) worker.execute(() -> finish("Export cancelled.", false));
                    else phase.setValue(Phase.READY);
                });
            } catch (IOException | RuntimeException | OutOfMemoryError e) {
                finish(e instanceof CancellationException ? "Export cancelled." : "Could not prepare export: " + e.getMessage(), false);
            }
        });
    }

    void picking() { phase.setValue(Phase.PICKING); }

    void pickerFailed(String error) {
        worker.execute(() -> finish(error, false));
    }

    void destination(Uri uri) {
        if (phase.getValue() != Phase.PICKING || archive == null || cancelled.get()) {
            if (uri != null) {
                worker.execute(() -> finish("Export was interrupted." + deleteDestination(uri), false));
            }
            return;
        }
        if (uri == null) { cancel(); return; }
        phase.setValue(Phase.TRANSFERRING);
        worker.execute(() -> {
            String failure = null;
            try {
                if (!journal.edit().putString("uri", uri.toString()).commit()) {
                    throw new IOException("Could not record the export destination.");
                }
                SaveArchive.checkCancelled(cancelled::get);
                SaveArchive.transfer(archive, resolver.openOutputStream(uri, "w"), cancelled::get);
                SaveArchive.checkCancelled(cancelled::get);
            } catch (IOException | RuntimeException | OutOfMemoryError e) {
                failure = (cancelled.get() ? "Export cancelled." : "Could not export saves: " + e.getMessage())
                    + deleteDestination(uri);
            }
            finish(failure == null ? "Save files exported." : failure, failure == null);
        });
    }

    void cancel() {
        cancelled.set(true);
        Phase current = phase.getValue();
        if (current == Phase.CONFIRM) { phase.setValue(Phase.IDLE); return; }
        phase.setValue(Phase.CANCELLING);
        if (current == Phase.PREPARING || current == Phase.TRANSFERRING || current == Phase.CANCELLING) return;
        // Serialized after any in-flight provider call. A blocked provider must return before cleanup.
        worker.execute(() -> {
            if (phase.getValue() == Phase.CANCELLING) finish("Export cancelled.", false);
        });
    }

    void acknowledge() {
        message = null;
        phase.setValue(Phase.IDLE);
    }

    private String deleteDestination(Uri uri) {
        try {
            if (DocumentsContract.deleteDocument(resolver, uri)) return "";
        } catch (IOException | RuntimeException ignored) { }
        return " An incomplete document may remain at " + uri + ".";
    }

    // Runs on the worker; completion is published only after streams and temporary files are closed.
    private void finish(String text, boolean success) {
        if (archive != null) { archive.delete(); archive = null; }
        journal.edit().clear().commit();
        main.post(() -> {
            message = text;
            phase.setValue(success ? Phase.DONE : Phase.ERROR);
        });
    }

    @Override protected void onCleared() {
        cancelled.set(true);
        worker.execute(() -> {
            if (archive != null) { archive.delete(); archive = null; }
        });
        worker.shutdown();
    }
}
