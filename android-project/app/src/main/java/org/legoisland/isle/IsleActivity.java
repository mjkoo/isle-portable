package org.legoisland.isle;

import android.widget.Toast;
import android.widget.ImageButton;
import android.widget.RelativeLayout;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;

import java.util.ArrayList;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.RejectedExecutionException;

import org.libsdl.app.SDLActivity;

public class IsleActivity extends SDLActivity {
    private GameImport mImport;
    private volatile SaveRestoreStartup mRestoreStartup;

    public void startSaveRestore() {
        SaveRestoreStartup gate = new SaveRestoreStartup(this);
        mRestoreStartup = gate;
        gate.start();
    }

    public int getSaveRestoreStatus() {
        SaveRestoreStartup gate = mRestoreStartup;
        return gate == null ? 1 : gate.status();
    }

    private static final int SETTINGS_REQUEST = 4801;
    private static final String TAG = "IsleActivity";
    private ImageButton mMenuButton;
    private boolean mGameReady;
    private boolean mResumed;
    private TouchControlsView mTouchControls;
    private TouchControlsLayer mControls;
    // Reads the saved layout off the UI thread, completing in request order.
    private final ExecutorService mLayoutIo = Executors.newSingleThreadExecutor();
    private TouchLayout mTouchLayout = TouchLayout.DEFAULT;
    private boolean mLayoutRequested;
    // The buttons stay hidden until the saved layout has been read, so none first appears in the
    // wrong place, and the editor never starts from positions that were not read yet.
    private boolean mLayoutLoaded;
    // Only while the layout editor is open over the paused game.
    private TouchLayoutEditor mEditor;
    private TouchLayout mEditorOriginal;
    private Runnable mEditorClosed;

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        if (mLayout == null) return;
        mLayout.setMotionEventSplittingEnabled(true);
        mTouchControls = new TouchControlsView(this, mSurface);
        mLayout.addView(mTouchControls, new RelativeLayout.LayoutParams(
            RelativeLayout.LayoutParams.MATCH_PARENT, RelativeLayout.LayoutParams.MATCH_PARENT));
        TouchActionButton space = new TouchActionButton(this, "Space",
            generation -> TouchControlsView.submitAction(1, generation));
        TouchActionButton escape = new TouchActionButton(this, "Esc",
            generation -> TouchControlsView.submitAction(2, generation));
        mTouchControls.setActionButtons(space, escape);
        mMenuButton = new ImageButton(this);
        mMenuButton.setImageResource(R.drawable.game_menu);
        mMenuButton.setBackgroundResource(R.drawable.game_menu_background);
        mMenuButton.setContentDescription("Game menu");
        mMenuButton.setPadding(0, 0, 0, 0);
        mMenuButton.setVisibility(View.GONE);
        mMenuButton.setOnClickListener(view -> {
            view.setVisibility(View.GONE);
            mTouchControls.setRunning(false);
            SettingsBridge.requestMenu();
        });
        // SDLActivity creates its layout as a RelativeLayout; the overlay above relies on that too.
        mControls = new TouchControlsLayer((RelativeLayout) mLayout, mMenuButton, escape, space);
    }

    public void showMenuButton() {
        runOnUiThread(() -> {
            mGameReady = true;
            // The game has started, so the settings path is known.
            if (!mLayoutRequested) {
                mLayoutRequested = true;
                loadTouchLayout();
            }
            restoreMenuButton();
        });
    }

    void restoreMenuButton() {
        if (mGameReady && mLayoutLoaded && mMenuButton != null && !isFinishing()) {
            mMenuButton.setVisibility(View.VISIBLE);
            mControls.requestApplyInsets();
        }
        updateTouchControls();
    }

    private void loadTouchLayout() {
        try {
            mLayoutIo.execute(() -> {
                TouchLayout loaded = null;
                try {
                    loaded = TouchLayout.parse(SettingsBridge.read(SettingsBridge.path(), TouchLayout.KEYS));
                } catch (RuntimeException e) {
                    Log.w(TAG, "Could not read the touch layout; using the default", e);
                } finally {
                    // Whatever the read did, the buttons must still appear, if only at the default.
                    TouchLayout result = loaded;
                    runOnUiThread(() -> applyTouchLayout(result));
                }
            });
        } catch (RejectedExecutionException e) {
            // Only after onDestroy, when there are no controls left to update.
        }
    }

    private void applyTouchLayout(TouchLayout loaded) {
        if (isDestroyed() || mControls == null) return;
        if (loaded != null) mTouchLayout = loaded;
        mControls.setTouchLayout(mTouchLayout);
        mTouchControls.setOpacity(mTouchLayout.opacity);
        if (mEditor != null) mEditor.setAppearance(mTouchLayout);
        if (!mLayoutLoaded) {
            mLayoutLoaded = true;
            // Back can open the menu before the first read finishes; the button stays down then.
            QuitPrompt prompt = mQuitPrompt;
            if (prompt == null || prompt.getStatus() != QuitPrompt.STATUS_PENDING) restoreMenuButton();
            else updateTouchControls();
        }
    }

    /**
     * Opens the layout editor over the paused game, for the quit prompt returning from Settings.
     * onClosed runs when Done or Back closes the editor, never after the activity has gone.
     */
    boolean startTouchLayoutEditor(Runnable onClosed) {
        if (mControls == null || !mGameReady || !mLayoutLoaded || mEditor != null || isFinishing()) return false;
        mEditorOriginal = mTouchLayout;
        mEditorClosed = onClosed;
        mEditor = new TouchLayoutEditor(this, mTouchLayout, this::saveTouchLayout);
        mControls.showEditor(mEditor);
        return true;
    }

    private void saveTouchLayout(TouchLayout draft) {
        ArrayList<String> keys = new ArrayList<>(), values = new ArrayList<>();
        draft.changedPositions(mEditorOriginal, keys, values);
        if (keys.isEmpty()) {
            closeTouchLayoutEditor();
            return;
        }
        TouchLayoutEditor editor = mEditor;
        editor.setBusy(true);
        String path = SettingsBridge.path();
        try {
            mLayoutIo.execute(() -> {
                String failure;
                try {
                    failure = SettingsBridge.write(path, keys.toArray(new String[0]),
                        values.toArray(new String[0]), new String[0]);
                } catch (RuntimeException e) {
                    Log.w(TAG, "Could not save the touch layout", e);
                    failure = "Could not save the touch layout.";
                }
                String message = failure;
                runOnUiThread(() -> {
                    if (isDestroyed() || mEditor != editor) return;
                    if (message != null) {
                        Log.w(TAG, "Touch layout not saved: " + message);
                        // Stay in the editor with the draft intact, so Done can be retried. The
                        // message already says what failed.
                        editor.setBusy(false);
                        Toast.makeText(this, message + " Choose Done to try again.", Toast.LENGTH_LONG).show();
                        return;
                    }
                    mTouchLayout = draft.withAppearanceOf(mTouchLayout);
                    mControls.setTouchLayout(mTouchLayout);
                    closeTouchLayoutEditor();
                });
            });
        } catch (RejectedExecutionException e) {
            Log.w(TAG, "Touch layout not saved: the activity is closing", e);
            editor.setBusy(false);
        }
    }

    private void closeTouchLayoutEditor() {
        if (mEditor == null) return;
        mControls.hideEditor();
        mEditor = null;
        mEditorOriginal = null;
        Runnable closed = mEditorClosed;
        mEditorClosed = null;
        if (closed != null) closed.run();
    }

    @Override public boolean dispatchKeyEvent(KeyEvent event) {
        // Back cancels the editor and discards its draft, and never reaches the game. This relies
        // on Back arriving as a key event; an app opted into predictive back would need an
        // OnBackInvokedCallback here instead.
        if (mEditor != null && event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
            if (event.getAction() == KeyEvent.ACTION_UP && !event.isCanceled() && !mEditor.isBusy()) {
                closeTouchLayoutEditor();
            }
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    private void updateTouchControls() {
        if (mTouchControls != null) {
            mTouchControls.setRunning(mGameReady && mLayoutLoaded && mResumed && hasWindowFocus() && !isFinishing());
        }
    }

    @Override protected void onResume() {
        super.onResume();
        mResumed = true;
        updateTouchControls();
    }

    @Override protected void onPause() {
        mResumed = false;
        updateTouchControls();
        super.onPause();
    }

    @Override public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        updateTouchControls();
    }

    /** The layout editor is offered only over a running game, never from startup recovery. */
    void openSettings(boolean layoutEditor) {
        startActivityForResult(new Intent(this, SettingsActivity.class)
            .putExtra("configPath", SettingsBridge.path())
            .putExtra("exportId", SettingsBridge.exportId())
            .putExtra("renderers", SettingsBridge.renderers())
            .putExtra(SettingsActivity.EXTRA_TOUCH_LAYOUT_EDITOR, layoutEditor && mGameReady && mLayoutLoaded),
            SETTINGS_REQUEST);
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == SETTINGS_REQUEST && resultCode == RESULT_OK) loadTouchLayout();
        if (requestCode == SETTINGS_REQUEST && mQuitPrompt != null) {
            mQuitPrompt.returnedFromSettings(resultCode == SettingsActivity.RESULT_EDIT_TOUCH_LAYOUT);
        }
    }

    public void showStartupSettings(String error) {
        QuitPrompt prompt = new QuitPrompt(this, QuitPrompt.SAVE_NOTHING_TO_SAVE, error);
        mQuitPrompt = prompt;
        prompt.show();
    }

    public boolean isStartupSettingsOpen() {
        QuitPrompt prompt = mQuitPrompt;
        return prompt != null && prompt.getStatus() == QuitPrompt.STATUS_PENDING;
    }


    // Unlike mImport, which only the SDL thread touches, this is written by the SDL thread in
    // showQuitPrompt and read by the UI thread in onDestroy.
    private volatile QuitPrompt mQuitPrompt;

    protected String[] getLibraries() {
        return new String[] { "SDL3", "lego1", "isle" };
    }

    /**
     * The quit prompt holds a window of its own, and nothing dismisses it when the activity is
     * destroyed out from under it - a configuration change the activity does not handle, say.
     * Take it down here rather than leaving the framework to report a leaked window. The native
     * side is not waiting on this: it stops polling once the game has been torn down.
     */
    @Override
    protected void onDestroy() {
        if (mTouchControls != null) mTouchControls.setRunning(false);
        mLayoutIo.shutdown();
        if (mRestoreStartup != null) mRestoreStartup.abandon();
        QuitPrompt prompt = mQuitPrompt;
        if (prompt != null) {
            prompt.abandon();
            mQuitPrompt = null;
        }

        super.onDestroy();
    }

    /**
     * Starts copying the game files from the document tree the user selected into the app's
     * external files directory, so they are owned and readable by this app. Returns immediately;
     * the caller polls getGameFileImportStatus().
     *
     * Deliberately not a single blocking call. The SDL thread that calls this has to keep
     * pumping its event queue for the UI thread to run at all, so it cannot sit inside JNI for
     * the length of a copy.
     *
     * Called from native code (see ISLE/android/filepicker.cpp); kept by proguard-rules.pro.
     */
    public void startGameFileImport(String treeUri) {
        mImport = new GameImport(this);
        mImport.start(treeUri);
    }

    /**
     * One of GameImport's STATUS_ constants, which ISLE/android/filepicker.h mirrors, or
     * STATUS_RUNNING while the import is still in flight.
     *
     * Called from native code (see ISLE/android/filepicker.cpp); kept by proguard-rules.pro.
     */
    public int getGameFileImportStatus() {
        return mImport != null ? mImport.getStatus() : GameImport.STATUS_INTERNAL_ERROR;
    }

    /**
     * The directory the last successful import copied into, or null. Deliberately
     * separate from the status: inferring success from the path is what let an import that
     * copied nothing report the directory diskpath already named and count as progress.
     *
     * Called from native code (see ISLE/android/filepicker.cpp); kept by proguard-rules.pro.
     */
    public String getImportedRoot() {
        return mImport != null ? mImport.getImportedRoot() : null;
    }

    /**
     * Posts the confirmation the back button raises, told what the save that precedes it did as
     * one of QuitPrompt's SAVE_ constants. Returns immediately; the caller polls
     * getQuitPromptStatus().
     *
     * Called from native code (see ISLE/android/quitprompt.cpp); kept by proguard-rules.pro.
     */
    public void showQuitPrompt(int saveResult) {
        QuitPrompt prompt = new QuitPrompt(this, saveResult);
        // Published before hiding the button, so a layout read finishing in between sees the
        // pending prompt and does not bring the button back under it.
        mQuitPrompt = prompt;
        runOnUiThread(() -> { if (mMenuButton != null) mMenuButton.setVisibility(View.GONE); });
        prompt.show();
    }

    /**
     * One of QuitPrompt's STATUS_ constants, which ISLE/android/quitprompt.h mirrors, or
     * STATUS_PENDING while the user has yet to answer. Reports STATUS_RESUME rather than
     * STATUS_QUIT if there is no prompt to answer, so a failure to post one cannot quit the
     * game on the player's behalf.
     *
     * Called from native code (see ISLE/android/quitprompt.cpp); kept by proguard-rules.pro.
     */
    public int getQuitPromptStatus() {
        QuitPrompt prompt = mQuitPrompt;
        return prompt != null ? prompt.getStatus() : QuitPrompt.STATUS_RESUME;
    }

    /**
     * Whether a previous import left anything behind that removeImportedGameData could clear.
     * Drives whether the import prompt offers the removal button at all.
     *
     * Called from native code (see ISLE/android/filepicker.cpp); kept by proguard-rules.pro.
     */
    public boolean hasImportedGameData() {
        return GameImport.hasImportedData(getExternalFilesDir(null));
    }

    /**
     * Deletes the game data in this app's storage, so a bad or unwanted import can be undone
     * without reinstalling. Clears a tree pushed in by hand as well, which is what the caller
     * wants: it only offers this once that data has failed to load. Never touches saves or
     * isle.ini.
     *
     * Called from native code (see ISLE/android/filepicker.cpp); kept by proguard-rules.pro.
     */
    public boolean removeImportedGameData() {
        boolean removed = GameImport.removeImportedData(getExternalFilesDir(null));
        final String message = removed ? "Game data removed"
                : "Some game data could not be removed";
        runOnUiThread(() -> Toast.makeText(this, message, Toast.LENGTH_LONG).show());
        return removed;
    }
}
