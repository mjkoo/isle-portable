package org.legoisland.isle;

import android.app.Activity;
import android.app.AlertDialog;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

/**
 * The "quit to Android?" confirmation the back button raises.
 *
 * Deliberately not SDL_ShowMessageBox, which blocks the calling thread on a monitor until a
 * button is pressed. That thread is the SDL thread, and it is the only thread that dispatches
 * the Android lifecycle events: while it is blocked, a backgrounded app never reaches the
 * watch in isleapp.cpp that saves, and the UI thread's surface teardown spins for half a
 * second and then releases the EGL surface with the context possibly still active. So the
 * native side shows this and then polls getStatus() while pumping, the same shape the import
 * uses.
 *
 * Reached only from IsleActivity, so unlike IsleActivity this class needs no proguard keep
 * rule.
 */
final class QuitPrompt {
    // Mirrored in ISLE/android/quitprompt.h; keep the numbering in step.
    static final int STATUS_PENDING = -1;
    static final int STATUS_RESUME = 0;
    static final int STATUS_QUIT = 1;

    // What the save that precedes the prompt did. Also mirrored in quitprompt.h.
    static final int SAVE_WRITTEN = 0;
    static final int SAVE_NOTHING_TO_SAVE = 1;
    static final int SAVE_FAILED = 2;

    private static final String TAG = "IsleActivity";

    private final Activity mActivity;
    private final int mSaveResult;
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    // Written on the UI thread, read on the SDL thread.
    private volatile int mStatus = STATUS_PENDING;

    // Touched on the UI thread only: show() posts showDialog there, and abandon() is called from
    // the activity's own onDestroy.
    private AlertDialog mDialog;
    private boolean mAbandoned;

    QuitPrompt(Activity activity, int saveResult) {
        mActivity = activity;
        mSaveResult = saveResult;
    }

    /** STATUS_PENDING until the user has answered. */
    int getStatus() {
        return mStatus;
    }

    /** Posts the dialog and returns immediately. The caller polls getStatus(). */
    void show() {
        mHandler.post(this::showDialog);
    }

    /**
     * Takes the dialog down without an answer, for when the activity is going away underneath
     * it. Its own window would otherwise outlive the activity, which the framework reports as a
     * leak. Answers "keep playing" because there is no longer a game to quit.
     */
    void abandon() {
        // Checked by showDialog, which may still be sitting in the looper queue: dismissing a
        // dialog that has not been built yet does nothing, and the posted runnable would then
        // put one on screen after the activity is gone.
        mAbandoned = true;

        // Only if nobody has answered. The SDL thread polls every 100 ms, so a confirmed quit
        // can be waiting to be read, and overwriting it would discard the user's decision.
        if (mStatus == STATUS_PENDING) {
            finish(STATUS_RESUME);
        }
    }

    private void showDialog() {
        if (mAbandoned) {
            return;
        }

        AlertDialog.Builder builder = new AlertDialog.Builder(mActivity);
        builder.setTitle("Quit LEGO Island?");
        // Say what actually happened. A player whose save just failed is exactly the one who
        // must not be told otherwise with a Quit button in front of them - and one who has not
        // registered has nothing saved either, which is not the same as a failure.
        builder.setMessage(saveMessage());
        builder.setPositiveButton("Quit", (dialog, which) -> finish(STATUS_QUIT));
        builder.setNegativeButton("Keep playing", (dialog, which) -> finish(STATUS_RESUME));

        // A second back press answers "keep playing" rather than stacking another prompt: the
        // game is already saved and paused, so the safe answer is the one that costs nothing.
        builder.setOnCancelListener(dialog -> finish(STATUS_RESUME));

        mDialog = builder.create();
        mDialog.setCanceledOnTouchOutside(false);

        try {
            mDialog.show();
        }
        catch (RuntimeException e) {
            // A window token that died between posting this and running it takes the dialog with
            // it, and then no button callback will ever run. The SDL thread is waiting on one, so
            // answer for it rather than leaving it to poll a dialog that does not exist.
            Log.e(TAG, "Could not show the quit prompt", e);
            finish(STATUS_RESUME);
        }
    }

    private String saveMessage() {
        switch (mSaveResult) {
        case SAVE_WRITTEN:
            return "Your game has been saved.";
        case SAVE_FAILED:
            return "Your game could not be saved.";
        default:
            return "There is no saved game yet.";
        }
    }

    private void finish(int status) {
        if (mDialog != null) {
            mDialog.dismiss();
            mDialog = null;
        }

        // Published last: the SDL thread tears the game down the moment it reads this, and
        // nothing may be left floating over the surface while that happens.
        mStatus = status;
    }
}
