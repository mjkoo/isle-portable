package org.legoisland.isle;

import android.widget.Toast;

import org.libsdl.app.SDLActivity;

public class IsleActivity extends SDLActivity {
    private GameImport mImport;
    private QuitPrompt mQuitPrompt;

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
        if (mQuitPrompt != null) {
            mQuitPrompt.abandon();
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
     * Posts the confirmation the back button raises, saying whether the save that precedes it
     * succeeded. Returns immediately; the caller polls getQuitPromptStatus().
     *
     * Called from native code (see ISLE/android/quitprompt.cpp); kept by proguard-rules.pro.
     */
    public void showQuitPrompt(boolean saved) {
        mQuitPrompt = new QuitPrompt(this, saved);
        mQuitPrompt.show();
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
        return mQuitPrompt != null ? mQuitPrompt.getStatus() : QuitPrompt.STATUS_RESUME;
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
