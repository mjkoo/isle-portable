package org.legoisland.isle;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.text.format.Formatter;
import android.util.Log;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import java.io.File;
import java.util.Locale;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Copies a LEGO Island installation out of a user-picked SAF tree and into the app's external
 * files directory, which is the only place the game can read from without a permission. The copy
 * itself is GameFileCopier's; this class owns the startup import's dialogs and its policy of
 * replacing whatever game data was there before.
 *
 * The caller is the SDL thread, inside SDL_AppInit, so nothing is drawing yet and there is no
 * renderer to put a progress bar on. The work therefore runs on a worker thread with an Android
 * dialog over the surface.
 *
 * The SDL thread must not simply block until that finishes. It is not the thread Android watches
 * for ANRs, but the UI thread can be waiting on it to service the surface teardown the file
 * picker left behind, and until the SDL thread pumps its event queue the UI thread runs nothing
 * at all - including the dialog this class posts to it. So the native side starts the import and
 * then polls getStatus() while pumping, the same shape the folder dialog above it already uses.
 *
 * Reached only from IsleActivity, so unlike IsleActivity this class needs no proguard keep rule.
 */
final class GameImport {
    private static final String TAG = "IsleActivity";

    private static final long SPACE_MARGIN_BYTES = 32L * 1024 * 1024;
    private static final long TICK_MS = 150;
    private static final int PROGRESS_STEPS = 1000;

    private final Activity mActivity;
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private final AtomicBoolean mCancelled = new AtomicBoolean();

    // Set on the worker once the copy starts, read on the UI thread for progress.
    private volatile GameFileCopier mCopier;
    private volatile boolean mFinished;

    private volatile int mStatus = GameFileCopier.STATUS_RUNNING;
    // Written on the worker, read on the SDL and UI threads. Publication is already ordered by
    // the volatile mStatus below it, but not visibly so; say it here instead.
    private volatile String mImportedRoot;
    private volatile String mFailureDetail;

    // Touched on the UI thread only.
    private AlertDialog mDialog;
    private TextView mStatusView;
    private TextView mDetailView;
    private ProgressBar mBar;
    private boolean mHadKeepScreenOn;

    GameImport(Activity activity) {
        mActivity = activity;
    }

    String getImportedRoot() {
        return mImportedRoot;
    }

    /** The status of the import in flight, or STATUS_RUNNING until it has finished. */
    int getStatus() {
        return mStatus;
    }

    /**
     * Starts one import and returns immediately. The caller polls getStatus() until it stops
     * reporting STATUS_RUNNING, pumping its own event queue in between so the UI thread is free
     * to run the progress dialog.
     */
    void start(final String treeUri) {
        mHandler.post(this::showProgressDialog);

        Thread worker = new Thread(() -> {
            int result;
            try {
                result = importTree(treeUri);
            }
            catch (Throwable t) {
                Log.e(TAG, "Failed to import game files from " + treeUri, t);
                result = GameFileCopier.STATUS_INTERNAL_ERROR;
            }

            final int status = result;
            // Tear the dialog down before publishing the status, so nothing is left floating
            // over the surface as the game starts creating its renderer.
            mHandler.post(() -> {
                dismissProgressDialog();
                if (status == GameFileCopier.STATUS_OK || status == GameFileCopier.STATUS_CANCELLED) {
                    mStatus = status;
                }
                else {
                    // Keep reporting RUNNING until the user has read what went wrong, otherwise
                    // native puts its own message box up on top of this one.
                    showFailureDialog(status);
                }
            });
        }, "IsleGameImport");
        worker.start();
    }

    // --- the work ---------------------------------------------------------------------

    private int importTree(String treeUri) {
        File filesDir = mActivity.getExternalFilesDir(null);
        if (filesDir == null) {
            mFailureDetail = "This app has no external storage directory right now.";
            return GameFileCopier.STATUS_WRITE_FAILED;
        }

        GameFileCopier copier = new GameFileCopier(
                new DocumentTreeSource(mActivity.getContentResolver(), Uri.parse(treeUri)), mCancelled::get);
        mCopier = copier;

        File root = filesDir;
        try {
            copier.scan();

            // Everything the previous import left behind is about to be deleted, so its space
            // counts as available.
            long reclaimable = 0;
            for (File path : GameFileCopier.importedPaths(filesDir)) {
                reclaimable += GameFileCopier.sizeOf(path);
            }

            long total = copier.totalBytes();
            long required = total + SPACE_MARGIN_BYTES;
            long available = filesDir.getUsableSpace() + reclaimable;
            if (available < required) {
                throw new GameFileCopier.Failure(GameFileCopier.STATUS_NO_SPACE, "Copying the game files needs "
                        + Formatter.formatFileSize(mActivity, total) + ", but only "
                        + Formatter.formatFileSize(mActivity, available) + " is free. Free up about "
                        + Formatter.formatFileSize(mActivity, required - available) + " and try again.");
            }

            // Delete first, then copy. That is what makes "a second import does not leave the first
            // behind" true by construction, and it means the space measured above is the space
            // actually needed rather than twice it. The cost is that cancelling a re-import leaves
            // nothing, which is acceptable: this prompt is only reached because the data already
            // present is unusable.
            copier.setPhase("Removing previous game data...");
            boolean cleared = GameFileCopier.removeImportedData(filesDir);

            if (!cleared && new File(filesDir, GameFileCopier.GAME_DIR).exists()) {
                // Something in the way could not be deleted, typically a tree pushed in by adb with
                // foreign ownership. Import beside it instead of failing.
                root = new File(filesDir, GameFileCopier.IMPORTED_PREFIX + System.currentTimeMillis());
            }

            File dest = new File(root, GameFileCopier.GAME_DIR);
            copier.setPhase("Copying game files...");
            copier.copy(dest, false);

            mImportedRoot = root.getAbsolutePath();
            Log.i(TAG, "Imported " + copier.totalFiles() + " files (" + total + " bytes) into " + dest);
            return GameFileCopier.STATUS_OK;
        }
        catch (GameFileCopier.Failure failure) {
            if (failure.status != GameFileCopier.STATUS_CANCELLED) {
                Log.e(TAG, "Game file import failed: " + failure.getMessage(), failure.getCause());
            }
            // The copier has already deleted its partial tree; an import beside undeletable data
            // also leaves the directory it made for itself.
            if (root != filesDir) {
                GameFileCopier.deleteRecursively(root);
            }
            mFailureDetail = failure.getMessage();
            return failure.status;
        }
    }

    // --- UI ---------------------------------------------------------------------------

    private void showProgressDialog() {
        float density = mActivity.getResources().getDisplayMetrics().density;
        int padding = (int) (24 * density);
        int spacing = (int) (12 * density);

        LinearLayout content = new LinearLayout(mActivity);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setPadding(padding, padding, padding, padding);

        mStatusView = new TextView(mActivity);
        mStatusView.setText(phase());
        content.addView(mStatusView);

        mBar = new ProgressBar(mActivity, null, android.R.attr.progressBarStyleHorizontal);
        mBar.setMax(PROGRESS_STEPS);
        mBar.setIndeterminate(true);
        LinearLayout.LayoutParams barParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
        );
        barParams.topMargin = spacing;
        barParams.bottomMargin = spacing;
        content.addView(mBar, barParams);

        mDetailView = new TextView(mActivity);
        content.addView(mDetailView);

        mDialog = new AlertDialog.Builder(mActivity)
                .setTitle("LEGO® Island")
                .setView(content)
                .setNegativeButton("Cancel", null)
                .setCancelable(false)
                .create();
        mDialog.setCanceledOnTouchOutside(false);
        mDialog.show();

        // The builder's own listener always dismisses. Replace it after show(), which is also
        // the first moment getButton returns anything, so the dialog can stay up saying what it
        // is doing while the worker unwinds and deletes what it had written.
        final Button cancel = mDialog.getButton(AlertDialog.BUTTON_NEGATIVE);
        cancel.setOnClickListener(v -> {
            mCancelled.set(true);
            cancel.setEnabled(false);
            mStatusView.setText("Cancelling...");
        });

        // SDL manages this flag itself, so save and restore rather than clearing unconditionally.
        mHadKeepScreenOn = (mActivity.getWindow().getAttributes().flags
                & WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON) != 0;
        mActivity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        mHandler.postDelayed(mTicker, TICK_MS);
    }

    private String phase() {
        GameFileCopier copier = mCopier;
        return copier == null ? "Preparing..." : copier.phase();
    }

    /**
     * Repaints from the copier's counters on the UI thread. The worker only ever bumps atomics,
     * so it cannot flood the main looper however small the copy buffer gets.
     */
    private final Runnable mTicker = new Runnable() {
        @Override
        public void run() {
            if (mFinished || mDialog == null) {
                return;
            }

            mStatusView.setText(mCancelled.get() ? "Cancelling..." : phase());

            GameFileCopier copier = mCopier;
            long total = copier == null ? 0 : copier.totalBytes();
            if (total > 0) {
                long copied = copier.copiedBytes();
                mBar.setIndeterminate(false);
                mBar.setProgress((int) Math.min(PROGRESS_STEPS, copied * PROGRESS_STEPS / total));
                mDetailView.setText(String.format(
                        Locale.ROOT,
                        "%s of %s (%d of %d files)",
                        Formatter.formatFileSize(mActivity, copied),
                        Formatter.formatFileSize(mActivity, total),
                        copier.copiedFiles(),
                        copier.totalFiles()
                ));
            }

            mHandler.postDelayed(this, TICK_MS);
        }
    };

    private void dismissProgressDialog() {
        mFinished = true;
        mHandler.removeCallbacks(mTicker);

        if (!mHadKeepScreenOn) {
            mActivity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        }

        if (mDialog == null) {
            return;
        }

        try {
            if (!mActivity.isFinishing() && !mActivity.isDestroyed()) {
                mDialog.dismiss();
            }
        }
        catch (IllegalArgumentException e) {
            // The activity went away underneath the dialog; nothing left to dismiss.
            Log.w(TAG, "Could not dismiss the import dialog", e);
        }

        mDialog = null;
    }

    private void showFailureDialog(final int status) {
        String detail = mFailureDetail != null ? mFailureDetail : "The game files could not be copied.";
        String title = status == GameFileCopier.STATUS_NO_SPACE ? "Not enough space" : "Copying game files failed";

        DialogInterface.OnDismissListener release = dialog -> mStatus = status;

        try {
            new AlertDialog.Builder(mActivity)
                    .setTitle(title)
                    .setMessage(detail)
                    .setPositiveButton("OK", null)
                    .setOnDismissListener(release)
                    .show();
        }
        catch (RuntimeException e) {
            // No window to show it in. Log it and let the game get on with reporting its own
            // failure rather than leaving the caller polling forever.
            Log.e(TAG, detail, e);
            mStatus = status;
        }
    }
}
