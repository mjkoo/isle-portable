package org.legoisland.isle;

import android.app.Dialog;
import android.graphics.drawable.ColorDrawable;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

/**
 * The game menu and startup recovery dialog, coordinated with the SDL thread.
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
    // Mirrored in ISLE/android/quitprompt.cpp; keep the numbering in step.
    static final int STATUS_PENDING = -1;
    static final int STATUS_RESUME = 0;
    static final int STATUS_QUIT = 1;

    private static final String TAG = "IsleActivity";

    private final IsleActivity mActivity;
    private final String mStartupError;
    private final int mSaveResult;
    private final String mPlayerName;
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    // Written on the UI thread, read on the SDL thread.
    private volatile int mStatus = STATUS_PENDING;

    // Touched on the UI thread only: show() posts showDialog there, and abandon() is called from
    // the activity's own onDestroy.
    private Dialog mDialog;
    private boolean mAbandoned;

    QuitPrompt(IsleActivity activity, int saveResult, String playerName) {
        this(activity, saveResult, playerName, null);
    }

    static QuitPrompt forStartupError(IsleActivity activity, String startupError) {
        return new QuitPrompt(activity, QuitPromptText.SAVE_NOTHING_TO_SAVE, null, startupError);
    }

    private QuitPrompt(IsleActivity activity, int saveResult, String playerName, String startupError) {
        mStartupError = startupError;
        mActivity = activity;
        mSaveResult = saveResult;
        mPlayerName = playerName;
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

        QuitPromptText text = new QuitPromptText(mSaveResult, mPlayerName, mStartupError);
        // A dialog rather than a view over the game: its own window takes the key focus, so the
        // D-pad and a controller's buttons reach the menu instead of the SDL surface.
        Dialog dialog = new PauseMenuDialog(mActivity, text,
            () -> finish(STATUS_QUIT),
            () -> finish(STATUS_RESUME),
            () -> {
                Dialog shown = mDialog;
                mDialog = null;
                if (shown != null) shown.dismiss();
                try {
                    mActivity.openSettings(mStartupError == null);
                } catch (RuntimeException e) {
                    Log.e(TAG, "Could not open settings", e);
                    showDialog();
                }
            });
        dialog.setOnCancelListener(ignored -> finish(STATUS_RESUME));
        dialog.setCanceledOnTouchOutside(false);
        keepFullscreen(dialog.getWindow());
        mDialog = dialog;

        try {
            dialog.show();
        }
        catch (RuntimeException e) {
            // A window token that died between posting this and running it takes the dialog with
            // it, and then no button callback will ever run. The SDL thread is waiting on one, so
            // answer for it rather than leaving it to poll a dialog that does not exist.
            Log.e(TAG, "Could not show the quit prompt", e);
            finish(STATUS_RESUME);
        }
    }

    /** Keeps the system bars hidden behind the menu, as the game has them. */
    private static void keepFullscreen(Window window) {
        if (window == null) return;
        window.setBackgroundDrawable(new ColorDrawable(0));
        window.setLayout(WindowManager.LayoutParams.MATCH_PARENT, WindowManager.LayoutParams.MATCH_PARENT);
        if (Build.VERSION.SDK_INT >= 28) {
            window.getAttributes().layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }
        if (Build.VERSION.SDK_INT >= 30) {
            window.setDecorFitsSystemWindows(false);
            WindowInsetsController controller = window.getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.systemBars());
                controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            window.getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        }
    }

    /**
     * The editor runs while this prompt is still pending, so the game stays paused and the SDL
     * thread keeps discarding input until the player is back at this dialog.
     */
    void returnedFromSettings(boolean editLayout) {
        if (SettingsBridge.startupWorkScheduled()) { finish(STATUS_QUIT); return; }
        if (mAbandoned || mStatus != STATUS_PENDING) return;
        if (editLayout && mStartupError == null && mActivity.startTouchLayoutEditor(this::showDialog)) return;
        showDialog();
    }

    private void finish(int status) {
        if (mDialog != null) {
            mDialog.dismiss();
            mDialog = null;
        }

        // Published last: the SDL thread tears the game down the moment it reads this, and
        // nothing may be left floating over the surface while that happens.
        mStatus = status;
        if (!mAbandoned && status == STATUS_RESUME) mActivity.restoreMenuButton();
    }
}
