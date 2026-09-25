package org.legoisland.isle;

import android.widget.Toast;
import android.widget.ImageButton;
import android.widget.RelativeLayout;
import android.content.Intent;
import android.os.Bundle;
import android.os.SystemClock;
import android.util.Log;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

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
    private static final long MENU_BUTTON_FADE_MS = 250;
    // The share of an axis's travel that counts as deliberate, as native input counts it.
    private static final float CONTROLLER_AXIS_THRESHOLD = 8000f / 32767f;
    private static final int[] CONTROLLER_AXES = {
        MotionEvent.AXIS_X, MotionEvent.AXIS_Y, MotionEvent.AXIS_Z, MotionEvent.AXIS_RZ,
        MotionEvent.AXIS_HAT_X, MotionEvent.AXIS_HAT_Y, MotionEvent.AXIS_LTRIGGER, MotionEvent.AXIS_RTRIGGER,
        MotionEvent.AXIS_BRAKE, MotionEvent.AXIS_GAS,
    };
    private ImageButton mMenuButton;
    /** Whether the menu button may be up at all; the policy then decides how much of it shows. */
    private boolean mMenuButtonAllowed;
    private final MenuButtonPolicy mMenuButtonPolicy = new MenuButtonPolicy(SystemClock.uptimeMillis());
    private final Runnable mMenuButtonTick = this::applyMenuButton;
    private boolean mGameReady;
    private boolean mResumed;
    private TouchControlsView mTouchControls;
    private TouchLayoutController mTouchLayoutController;
    private boolean mLayoutRequested;
    /** False on a TV, where the menu button, Esc, Space and the touch hints stay hidden. */
    private boolean mTouchUi;
    private AudioFocus mAudioFocus;
    /**
     * False when SDLActivity gave up in onCreate over libraries it could not load, which is also
     * the one path where libisle.so is never loaded. Audio focus is the only thing this class
     * calls into native of its own accord rather than being called from it, so it is the only
     * thing that has to ask.
     */
    private boolean mLibrariesLoaded;

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        if (mLayout == null) return;
        mTouchUi = DeviceSupport.hasTouchControls(this);
        mLayout.setMotionEventSplittingEnabled(true);
        mTouchControls = new TouchControlsView(this, mSurface);
        mLayout.addView(mTouchControls, new RelativeLayout.LayoutParams(
            RelativeLayout.LayoutParams.MATCH_PARENT, RelativeLayout.LayoutParams.MATCH_PARENT));
        TouchActionButton space = new TouchActionButton(this, TouchControlsLayer.LABELS[TouchLayout.SPACE],
            generation -> TouchControlsView.submitAction(1, generation));
        TouchActionButton escape = new TouchActionButton(this, TouchControlsLayer.LABELS[TouchLayout.ESCAPE],
            generation -> TouchControlsView.submitAction(2, generation));
        mTouchControls.setActionButtons(space, escape);
        mMenuButton = TouchControlsLayer.createMenuButton(this);
        mMenuButton.setVisibility(View.GONE);
        mMenuButton.setOnClickListener(view -> requestMenu());
        // SDLActivity creates its layout as a RelativeLayout; the overlay above relies on that too.
        mTouchLayoutController = new TouchLayoutController((RelativeLayout) mLayout, mMenuButton, escape, space,
            this::onTouchLayoutRead);
        mLibrariesLoaded = true;
    }

    public void showMenuButton() {
        runOnUiThread(() -> {
            mGameReady = true;
            // The game has started, so the settings path is known.
            if (!mLayoutRequested && mTouchLayoutController != null) {
                mLayoutRequested = true;
                mTouchLayoutController.load();
            }
            restoreMenuButton();
        });
    }

    private void requestMenu() {
        hideMenuButton();
        mTouchControls.setRunning(false);
        SettingsBridge.requestMenu();
    }

    void restoreMenuButton() {
        if (mTouchUi && mGameReady && touchLayoutLoaded() && mMenuButton != null && !isFinishing()) {
            if (!mMenuButtonAllowed) mMenuButtonPolicy.onShown(SystemClock.uptimeMillis());
            mMenuButtonAllowed = true;
            applyMenuButton();
            mTouchLayoutController.requestApplyInsets();
        }
        updateTouchControls();
    }

    private void hideMenuButton() {
        mMenuButtonAllowed = false;
        applyMenuButton();
    }

    /** Brings the menu button in line with the policy, and schedules the next change it expects. */
    private void applyMenuButton() {
        if (mMenuButton == null || mTouchLayoutController == null) return;
        mMenuButton.removeCallbacks(mMenuButtonTick);
        long now = SystemClock.uptimeMillis();
        int state = mMenuButtonAllowed ? mMenuButtonPolicy.state(now) : MenuButtonPolicy.HIDDEN;
        if (state == MenuButtonPolicy.HIDDEN) {
            if (mMenuButton.getVisibility() == View.GONE) return;
            mMenuButton.animate().cancel();
            mMenuButton.setVisibility(View.GONE);
            return;
        }
        // Within the player's own touch control opacity, which the layout applies to every control.
        float opacity = mTouchLayoutController.layout().opacity;
        float alpha = state == MenuButtonPolicy.FULL ? opacity : opacity * MenuButtonPolicy.DIMMED_SHARE;
        if (mMenuButton.getVisibility() != View.VISIBLE) {
            mMenuButton.animate().cancel();
            mMenuButton.setAlpha(alpha);
            mMenuButton.setVisibility(View.VISIBLE);
        }
        else if (mMenuButton.getAlpha() != alpha) {
            mMenuButton.animate().alpha(alpha).setDuration(MENU_BUTTON_FADE_MS);
        }
        long next = mMenuButtonPolicy.nextChange(now);
        if (next > 0) mMenuButton.postDelayed(mMenuButtonTick, next);
    }

    private void onControllerInput() {
        mMenuButtonPolicy.onController();
        applyMenuButton();
    }

    private static boolean isController(InputDevice device) {
        return device != null && MenuButtonPolicy.isController(device.isVirtual(), device.getSources(),
            device.getKeyboardType() == InputDevice.KEYBOARD_TYPE_ALPHABETIC);
    }

    // Both observe input on its way to SDL and never consume it.
    @Override public boolean dispatchTouchEvent(MotionEvent event) {
        // Dispatched first, so the touch that brings a hidden button back is not also a press of it.
        boolean handled = super.dispatchTouchEvent(event);
        if (event.getActionMasked() == MotionEvent.ACTION_DOWN
                && (event.getSource() & InputDevice.SOURCE_TOUCHSCREEN) == InputDevice.SOURCE_TOUCHSCREEN) {
            mMenuButtonPolicy.onTouch(event.getEventTime());
            applyMenuButton();
        }
        return handled;
    }

    @Override public boolean dispatchGenericMotionEvent(MotionEvent event) {
        if (event.getActionMasked() == MotionEvent.ACTION_MOVE
                && (event.getSource() & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK) {
            for (int axis : CONTROLLER_AXES) {
                if (Math.abs(event.getAxisValue(axis)) > CONTROLLER_AXIS_THRESHOLD) {
                    onControllerInput();
                    break;
                }
            }
        }
        return super.dispatchGenericMotionEvent(event);
    }

    private boolean touchLayoutLoaded() {
        return mTouchLayoutController != null && mTouchLayoutController.isLoaded();
    }

    private void onTouchLayoutRead(TouchLayout layout, boolean first) {
        mTouchControls.setOpacity(layout.opacity);
        // The layout just set every control's alpha, the menu button's included, to the full
        // opacity; put the button back at the strength the policy wants.
        if (!first) {
            applyMenuButton();
            return;
        }
        // Back can open the menu before the first read finishes; the button stays down then.
        QuitPrompt prompt = mQuitPrompt;
        if (prompt == null || prompt.getStatus() != QuitPrompt.STATUS_PENDING) restoreMenuButton();
        else updateTouchControls();
    }

    /**
     * Opens the layout editor over the paused game, for the quit prompt returning from Settings.
     * onClosed runs when Done or Back closes the editor, never after the activity has gone.
     */
    boolean startTouchLayoutEditor(Runnable onClosed) {
        return mGameReady && !isFinishing() && mTouchLayoutController != null
            && mTouchLayoutController.startEditor(onClosed);
    }

    @Override public boolean dispatchKeyEvent(KeyEvent event) {
        // Back cancels the editor and discards its draft, and never reaches the game. This relies
        // on Back arriving as a key event; an app opted into predictive back would need an
        // OnBackInvokedCallback here instead.
        if (mTouchLayoutController != null && mTouchLayoutController.dispatchBack(event)) return true;
        // Volume keys and the navigation bar's Back come from devices that are not controllers.
        if (event.getAction() == KeyEvent.ACTION_DOWN && isController(event.getDevice())) onControllerInput();
        // Before the game is ready there is no menu to open, and Back keeps its old meaning.
        InputDevice device = event.getDevice();
        if (mGameReady && device != null
                && TvSupport.isRemoteBack(event.getKeyCode(), device.isVirtual(), device.getSources())) {
            // The up and any repeats are swallowed too, so SDL never sees half a press.
            if (event.getAction() == KeyEvent.ACTION_DOWN && event.getRepeatCount() == 0) requestMenu();
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    private void updateTouchControls() {
        if (mTouchControls != null) {
            mTouchControls.setRunning(mTouchUi && mGameReady && touchLayoutLoaded() && mResumed && hasWindowFocus() && !isFinishing());
        }
    }

    /**
     * Audio focus follows onStart/onStop rather than onResume/onPause, because that is where SDL
     * stops and starts the thread that mixes: SDLActivity.mHasMultiWindow is a compile-time
     * SDK_INT >= 24, so from Android 7 onwards its pauseNativeThread runs in onStop. Asking in
     * onResume would leave the game mixing at full volume, holding no focus, for the whole of a
     * pause that never becomes a stop - a split-screen or dialog-themed activity over the game.
     */
    @Override protected void onStart() {
        super.onStart();
        if (!mLibrariesLoaded) return;
        if (mAudioFocus == null) mAudioFocus = new AudioFocus(this);
        mAudioFocus.request();
    }

    @Override protected void onStop() {
        if (mAudioFocus != null) mAudioFocus.abandon();
        super.onStop();
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
        // Asking again costs nothing once focus is held, and it is the only event that arrives
        // when a call the game was refused focus under finally ends: the activity never stopped,
        // so nothing else fires, and a refusal leaves no registration to be called back on.
        if (hasFocus && mAudioFocus != null) mAudioFocus.request();
        updateTouchControls();
    }

    /** The layout editor is offered only over a running game, never from startup recovery. */
    void openSettings(boolean layoutEditor) {
        startActivityForResult(new Intent(this, SettingsActivity.class)
            .putExtra("configPath", SettingsBridge.path())
            .putExtra("exportId", SettingsBridge.exportId())
            .putExtra("renderers", SettingsBridge.renderers())
            .putExtra(SettingsActivity.EXTRA_TOUCH_LAYOUT_EDITOR, layoutEditor && mTouchUi && mGameReady && touchLayoutLoaded()),
            SETTINGS_REQUEST);
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == SETTINGS_REQUEST && resultCode == RESULT_OK && mTouchLayoutController != null) {
            mTouchLayoutController.load();
        }
        if (requestCode == SETTINGS_REQUEST && mQuitPrompt != null) {
            mQuitPrompt.returnedFromSettings(resultCode == SettingsActivity.RESULT_EDIT_TOUCH_LAYOUT);
        }
    }

    /**
     * Whether the first-run import can show a folder picker. Asked before SDL's, whose failure
     * on a TV cannot be told from a cancel.
     *
     * Called from native code (see ISLE/android/filepicker.cpp); kept by proguard-rules.pro.
     */
    public boolean hasFolderPicker() {
        return DeviceSupport.canPickFolder(this);
    }

    public void showStartupSettings(String error) {
        QuitPrompt prompt = new QuitPrompt(this, QuitPromptText.SAVE_NOTHING_TO_SAVE, error);
        mQuitPrompt = prompt;
        prompt.show();
    }

    public boolean isStartupSettingsOpen() {
        QuitPrompt prompt = mQuitPrompt;
        return prompt != null && prompt.getStatus() == QuitPrompt.STATUS_PENDING;
    }

    /**
     * Tells the player what a change applied at startup did, such as replacing the game files.
     *
     * Called from native code (see ISLE/android/settings.cpp); kept by proguard-rules.pro.
     */
    public void showStartupMessage(String message) {
        runOnUiThread(() -> Toast.makeText(this, message, Toast.LENGTH_LONG).show());
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
        if (mMenuButton != null) mMenuButton.removeCallbacks(mMenuButtonTick);
        if (mTouchLayoutController != null) mTouchLayoutController.destroy();
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
        return mImport != null ? mImport.getStatus() : GameFileCopier.STATUS_INTERNAL_ERROR;
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
     * one of QuitPromptText's SAVE_ constants. Returns immediately; the caller polls
     * getQuitPromptStatus().
     *
     * Called from native code (see ISLE/android/quitprompt.cpp); kept by proguard-rules.pro.
     */
    public void showQuitPrompt(int saveResult) {
        QuitPrompt prompt = new QuitPrompt(this, saveResult);
        // Published before hiding the button, so a layout read finishing in between sees the
        // pending prompt and does not bring the button back under it.
        mQuitPrompt = prompt;
        runOnUiThread(this::hideMenuButton);
        prompt.show();
    }

    /**
     * One of QuitPrompt's STATUS_ constants, which ISLE/android/quitprompt.cpp mirrors, or
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
        return GameFileCopier.hasImportedData(getExternalFilesDir(null));
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
        boolean removed = GameFileCopier.removeImportedData(getExternalFilesDir(null));
        if (!removed) {
            Log.w("IsleActivity", "Could not remove " + GameFileCopier.importedPaths(getExternalFilesDir(null)));
        }
        final String message = removed ? "Game data removed"
                : "Some game data could not be removed";
        runOnUiThread(() -> Toast.makeText(this, message, Toast.LENGTH_LONG).show());
        return removed;
    }
}
