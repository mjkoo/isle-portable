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
    private boolean abandoned;
    private boolean running;
    private volatile int status = -1;

    SaveRestoreStartup(IsleActivity activity) { this.activity = activity; }
    int status() { return status; }
    void start() { main.post(() -> attempt(false)); }
    private void attempt(boolean cancel) {
        if (abandoned || running) return;
        running = true;
        worker.execute(() -> {
            String outcome;
            try { outcome = cancel ? SettingsBridge.cancelRestore() : SettingsBridge.recoverRestore(); }
            catch (RuntimeException | OutOfMemoryError e) { outcome = "Could not recover saves: " + e.getMessage(); }
            final String result = outcome;
            boolean cancellable;
            try { cancellable = SettingsBridge.canCancelRestore(); }
            catch (RuntimeException | OutOfMemoryError e) { cancellable = false; }
            final boolean canCancel = cancellable;
            main.post(() -> {
                running = false;
                if (abandoned) { finish(1); return; }
                if (result != null && result.startsWith("OK:")) {
                    String message = result.substring(3);
                    if (!message.isEmpty()) Toast.makeText(activity, message, Toast.LENGTH_LONG).show();
                    finish(0);
                } else {
                    try {
                        AlertDialog.Builder builder = new AlertDialog.Builder(activity).setTitle("Save recovery needs attention")
                            .setMessage(result + "\n\nThe game has not started. Recovery files have been kept.")
                            .setPositiveButton("Retry", (d, which) -> { dialog = null; attempt(false); })
                            .setNegativeButton("Close", (d, which) -> finish(1))
                            .setCancelable(false);
                        if (canCancel) builder.setNeutralButton("Cancel restore", (d, which) -> { dialog = null; attempt(true); });
                        dialog = builder.show();
                    } catch (RuntimeException e) { finish(1); }
                }
            });
        });
    }
    void abandon() {
        abandoned = true;
        if (dialog != null) { dialog.dismiss(); dialog = null; }
        if (!running) finish(1);
    }
    private void finish(int result) {
        if (dialog != null) { dialog.dismiss(); dialog = null; }
        worker.shutdown();
        status = result;
    }
}
