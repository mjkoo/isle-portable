package org.legoisland.isle;

import android.app.Activity;
import android.app.AlertDialog;
import android.os.Handler;
import android.os.Looper;

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

    private final Activity mActivity;
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    // Written on the UI thread, read on the SDL thread.
    private volatile int mStatus = STATUS_PENDING;

    // Touched on the UI thread only.
    private AlertDialog mDialog;

    QuitPrompt(Activity activity) {
        mActivity = activity;
    }

    /** STATUS_PENDING until the user has answered. */
    int getStatus() {
        return mStatus;
    }

    /** Posts the dialog and returns immediately. The caller polls getStatus(). */
    void show() {
        mHandler.post(this::showDialog);
    }

    private void showDialog() {
        AlertDialog.Builder builder = new AlertDialog.Builder(mActivity);
        builder.setTitle("Quit LEGO Island?");
        builder.setMessage("Your game has been saved.");
        builder.setPositiveButton("Quit", (dialog, which) -> finish(STATUS_QUIT));
        builder.setNegativeButton("Keep playing", (dialog, which) -> finish(STATUS_RESUME));

        // A second back press answers "keep playing" rather than stacking another prompt: the
        // game is already saved and paused, so the safe answer is the one that costs nothing.
        builder.setOnCancelListener(dialog -> finish(STATUS_RESUME));

        mDialog = builder.create();
        mDialog.setCanceledOnTouchOutside(false);
        mDialog.show();
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
