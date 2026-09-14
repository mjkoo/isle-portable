package org.legoisland.isle;

import android.annotation.SuppressLint;
import android.app.Application;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.text.format.Formatter;
import android.util.Log;

import androidx.lifecycle.AndroidViewModel;
import androidx.lifecycle.MutableLiveData;

import java.io.File;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Settings > Data > Game files. Replacing copies a picked folder into a staging directory beside
 * the current files, checks it and schedules it; removing only schedules. The game has its files
 * open, so either change waits for the next startup and the game closes.
 */
public final class GameFilesModel extends AndroidViewModel {
    enum Phase { LOADING, IDLE, CHOOSE, PICK, PICKING, COPYING, CANCELLING, CONFIRM_REPLACE, CONFIRM_REMOVE, SCHEDULING, CLOSING, ERROR }
    final MutableLiveData<Phase> phase = new MutableLiveData<>(Phase.LOADING);

    private static final String TAG = "IsleActivity";
    private final Handler main = new Handler(Looper.getMainLooper());
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private final AtomicBoolean cancelled = new AtomicBoolean();
    private final GameFilesPolicy.Sizes sizes;
    private boolean started;
    private volatile boolean cleared;
    private String config;
    private File root;
    private File filesDir;

    // Set on the main thread when loading finishes.
    private String unavailable;
    private String diskpath;
    private long size = -1;
    private boolean pending;
    private boolean removable;
    // Set on the main thread as the flow moves on.
    private int copiedFiles;
    private long copiedBytes;
    String message;

    // The copy in flight, read on the main thread for progress.
    private volatile GameFileCopier copier;
    // Touched on the worker only.
    private String stagingId;
    private File staging;
    private boolean scheduled;

    public GameFilesModel(Application app) {
        super(app);
        sizes = bytes -> Formatter.formatFileSize(app, bytes);
    }

    void load(String configPath, boolean wasBusy) {
        if (started) return;
        started = true;
        config = configPath;
        root = getApplication().getExternalFilesDir(null);
        filesDir = getApplication().getFilesDir();
        worker.execute(() -> {
            String problem = null;
            String disk = null;
            long bytes = -1;
            boolean waiting = false;
            boolean data = false;
            if (root == null) {
                problem = "This app's storage is unavailable right now.";
            } else if (config == null || config.isEmpty()) {
                problem = "The game settings file is unavailable.";
            } else {
                try {
                    disk = SettingsBridge.read(config, new String[] {"isle:diskpath"})[0];
                    waiting = SettingsBridge.gameFilesPending(filesDir.getPath(), root.getPath());
                    bytes = GameFilesPolicy.measure(GameFilesPolicy.location(disk, root));
                    data = GameFilesPolicy.inAppStorage(disk, root) && GameFileCopier.hasImportedData(root);
                } catch (Throwable e) {
                    // Anything short of reaching IDLE would leave Settings locked, Back included.
                    problem = "Could not read the game files: " + e;
                }
            }
            final String result = problem, location = disk;
            final long measured = bytes;
            final boolean change = waiting, removal = data;
            main.post(() -> {
                if (cleared) return;
                unavailable = result;
                diskpath = location;
                size = measured;
                pending = change;
                removable = removal;
                // A change recorded before the process died is already waiting; say that instead.
                if (wasBusy && !change) {
                    message = "Changing the game files was interrupted. Nothing was changed.";
                    phase.setValue(Phase.ERROR);
                } else phase.setValue(Phase.IDLE);
            });
        });
    }

    boolean busy() { return phase.getValue() != Phase.IDLE && phase.getValue() != Phase.ERROR; }
    /** Whether work would be lost if the process died now. */
    boolean working() {
        Phase current = phase.getValue();
        return current == Phase.PICK || current == Phase.PICKING || current == Phase.COPYING || current == Phase.CANCELLING
            || current == Phase.CONFIRM_REPLACE || current == Phase.SCHEDULING;
    }
    boolean keepScreenOn() { return phase.getValue() == Phase.COPYING || phase.getValue() == Phase.CANCELLING; }
    boolean canRemove() { return removable; }
    GameFileCopier copier() { return copier; }

    String summary() {
        if (phase.getValue() == Phase.LOADING) return "Checking game files...";
        if (unavailable != null) return unavailable;
        return GameFilesPolicy.summary(diskpath, root, size, pending, sizes);
    }
    String replaceConfirmation() { return GameFilesPolicy.replaceConfirmation(copiedFiles, copiedBytes, diskpath, root, sizes); }
    String removeConfirmation() { return GameFilesPolicy.removeConfirmation(size, sizes); }

    void choose() {
        if (busy()) return;
        if (unavailable != null) { error(unavailable); return; }
        if (pending) { error(GameFilesPolicy.WAITING); return; }
        phase.setValue(Phase.CHOOSE);
    }
    void replace() { if (phase.getValue() == Phase.CHOOSE) phase.setValue(Phase.PICK); }
    void picking() { if (phase.getValue() == Phase.PICK) phase.setValue(Phase.PICKING); }
    void remove() { if (phase.getValue() == Phase.CHOOSE && removable) phase.setValue(Phase.CONFIRM_REMOVE); }

    void selected(Uri tree) {
        if (phase.getValue() != Phase.PICKING) return;
        if (tree == null) { phase.setValue(Phase.IDLE); return; }
        cancelled.set(false);
        phase.setValue(Phase.COPYING);
        worker.execute(() -> copy(tree));
    }

    // The staged copy must fit beside the current files in space that is free now. Cache the system
    // could clear is not ours to count on, so getAllocatableBytes is the wrong question here.
    @SuppressLint("UsableSpace")
    private void copy(Uri tree) {
        GameFileCopier current = new GameFileCopier(
            new DocumentTreeSource(getApplication().getContentResolver(), tree), cancelled::get);
        copier = current;
        String failure = null;
        boolean stopped = false;
        try {
            String[] claim = SettingsBridge.beginGameFilesStaging(root.getPath());
            stagingId = claim[0];
            staging = new File(claim[1]);
            current.scan();
            String refusal = GameFilesPolicy.spaceRefusal(current.totalBytes(), root.getUsableSpace(), sizes);
            if (refusal != null) throw new GameFileCopier.Failure(GameFileCopier.STATUS_NO_SPACE, refusal);
            current.setPhase("Copying game files...");
            current.copy(new File(staging, GameFileCopier.GAME_DIR), true);
            current.setPhase("Checking game files...");
            String missing = SettingsBridge.missingGameFile(staging.getPath());
            if (missing != null) {
                throw new GameFileCopier.Failure(GameFileCopier.STATUS_NOT_GAME_FOLDER, "The selected folder is missing "
                    + missing + ". Select the folder that holds the LEGO folder from a complete LEGO Island installation.");
            }
        } catch (GameFileCopier.Failure e) {
            stopped = e.status == GameFileCopier.STATUS_CANCELLED;
            failure = e.getMessage();
            if (!stopped) Log.e(TAG, "Copying game files failed: " + failure, e.getCause());
        } catch (Throwable e) {
            Log.e(TAG, "Copying game files failed", e);
            failure = "Copying the game files failed. " + e;
        }
        if (failure != null) discardStaging();
        final String result = failure;
        final boolean wasCancelled = stopped;
        final int files = current.totalFiles();
        final long bytes = current.totalBytes();
        main.post(() -> {
            copier = null;
            if (cleared) return;
            if (phase.getValue() == Phase.CANCELLING) {
                // Cancelled just as the copy finished: the staged copy is complete but unwanted.
                if (result == null) worker.execute(this::discardThenIdle);
                else phase.setValue(Phase.IDLE);
            } else if (result == null) {
                copiedFiles = files;
                copiedBytes = bytes;
                phase.setValue(Phase.CONFIRM_REPLACE);
            } else if (wasCancelled) {
                phase.setValue(Phase.IDLE);
            } else error(result);
        });
    }

    void confirmReplace() {
        if (phase.getValue() != Phase.CONFIRM_REPLACE) return;
        phase.setValue(Phase.SCHEDULING);
        worker.execute(() -> schedule(true));
    }
    void confirmRemove() {
        if (phase.getValue() != Phase.CONFIRM_REMOVE) return;
        phase.setValue(Phase.SCHEDULING);
        worker.execute(() -> schedule(false));
    }

    private void schedule(boolean replacing) {
        String failure;
        try {
            failure = SettingsBridge.scheduleGameFiles(filesDir.getPath(), root.getPath(), config, replacing ? stagingId : null);
        } catch (Throwable e) {
            failure = e.toString();
        }
        boolean closing = SettingsBridge.startupWorkScheduled();
        if (replacing) {
            if (closing) {
                // The waiting record keeps startup from collecting the staged copy now.
                scheduled = true;
                SettingsBridge.endGameFilesStaging(stagingId);
            } else discardStaging();
        }
        final String result = failure;
        main.post(() -> {
            if (cleared) return;
            if (closing) {
                // A failure with a change still waiting, such as an earlier one, closes the game too.
                message = result != null
                    ? "The game will close to finish a waiting game file change. " + result
                    : replacing ? "The new game files are ready. Reopen the game to put them in place."
                    : "The game files will be removed when you reopen the game.";
                phase.setValue(Phase.CLOSING);
            } else error(result == null ? "The game file change could not be recorded." : result);
        });
    }

    void cancel() {
        Phase current = phase.getValue();
        if (current == Phase.COPYING) {
            cancelled.set(true);
            phase.setValue(Phase.CANCELLING);
        } else if (current == Phase.CONFIRM_REPLACE) {
            phase.setValue(Phase.CANCELLING);
            worker.execute(this::discardThenIdle);
        } else if (current == Phase.CHOOSE || current == Phase.PICK || current == Phase.PICKING || current == Phase.CONFIRM_REMOVE) {
            phase.setValue(Phase.IDLE);
        }
    }

    void error(String text) { message = text; phase.setValue(Phase.ERROR); }
    void acknowledge() { message = null; phase.setValue(Phase.IDLE); }

    private void discardThenIdle() {
        discardStaging();
        main.post(() -> { if (!cleared) phase.setValue(Phase.IDLE); });
    }

    /** Deletes an unscheduled staged copy. Anything left behind is collected at the next startup. */
    private void discardStaging() {
        if (stagingId == null || scheduled) return;
        GameFileCopier.deleteRecursively(staging);
        SettingsBridge.endGameFilesStaging(stagingId);
        stagingId = null;
        staging = null;
    }

    @Override protected void onCleared() {
        cleared = true;
        cancelled.set(true);
        worker.execute(this::discardStaging);
        worker.shutdown();
    }
}
