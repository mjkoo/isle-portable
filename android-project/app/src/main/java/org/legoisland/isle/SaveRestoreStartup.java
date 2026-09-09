package org.legoisland.isle;

import android.app.AlertDialog;
import android.os.Handler;
import android.os.Looper;
import android.widget.Toast;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Holds native startup until recovery completes; never runs against a live game. */
final class SaveRestoreStartup {
    private final IsleActivity activity;
    private final Handler main = new Handler(Looper.getMainLooper());
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private AlertDialog dialog;
    private final SaveRestoreGate gate = new SaveRestoreGate();

    SaveRestoreStartup(IsleActivity activity) { this.activity = activity; }
    int status() { return gate.status(); }
    void start() { main.post(() -> attempt(false)); }
    private void attempt(boolean cancel) {
        if (!gate.begin()) return;
        worker.execute(() -> {
            String outcome;
            boolean cancellable;
            try {
                try { outcome = cancel ? SettingsBridge.cancelRestore() : SettingsBridge.recoverRestore(); }
                catch (RuntimeException | OutOfMemoryError e) { outcome = "Could not recover saves: " + e.getMessage(); }
                try { cancellable = SettingsBridge.canCancelRestore(); }
                catch (RuntimeException | OutOfMemoryError e) { cancellable = false; }
            } finally {
                // onDestroy can block the main thread waiting for native startup to
                // exit. Release an abandoned gate directly after the last JNI call.
                gate.workerFinished();
            }
            final String result = outcome;
            final boolean canCancel = cancellable;
            main.post(() -> {
                if (gate.isAbandoned()) return;
                if (result != null && result.startsWith("OK:")) {
                    String message = result.substring(3);
                    if (!message.isEmpty()) Toast.makeText(activity, message, Toast.LENGTH_LONG).show();
                    finish(SaveRestoreGate.READY);
                } else {
                    try {
                        AlertDialog.Builder builder = new AlertDialog.Builder(activity).setTitle("Save recovery needs attention")
                            .setMessage(result + "\n\nThe game has not started. Recovery files have been kept.")
                            .setPositiveButton("Retry", (d, which) -> { dialog = null; attempt(false); })
                            .setNegativeButton("Close", (d, which) -> finish(SaveRestoreGate.CLOSED))
                            .setCancelable(false);
                        if (canCancel) builder.setNeutralButton("Cancel restore", (d, which) -> { dialog = null; attempt(true); });
                        dialog = builder.show();
                    } catch (RuntimeException e) { finish(SaveRestoreGate.CLOSED); }
                }
            });
        });
    }
    void abandon() {
        gate.abandon();
        worker.shutdown();
        if (dialog != null) { dialog.dismiss(); dialog = null; }
    }
    private void finish(int result) {
        if (dialog != null) { dialog.dismiss(); dialog = null; }
        worker.shutdown();
        gate.finish(result);
    }
}
