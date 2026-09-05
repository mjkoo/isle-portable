package org.legoisland.isle;

import android.widget.Toast;

import org.libsdl.app.SDLActivity;

public class IsleActivity extends SDLActivity {
    private GameImport mImport;

    protected String[] getLibraries() {
        return new String[] { "SDL3", "lego1", "isle" };
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
     * Whether a previous import left anything behind that removeImportedGameData could clear.
     * Drives whether the import prompt offers the removal button at all.
     *
     * Called from native code (see ISLE/android/filepicker.cpp); kept by proguard-rules.pro.
     */
    public boolean hasImportedGameData() {
        return GameImport.hasImportedData(getExternalFilesDir(null));
    }

    /**
     * Deletes everything an import has copied in, so a bad or unwanted one can be undone
     * without reinstalling. Never touches saves or isle.ini.
     *
     * Called from native code (see ISLE/android/filepicker.cpp); kept by proguard-rules.pro.
     */
    public boolean removeImportedGameData() {
        boolean removed = GameImport.removeImportedData(getExternalFilesDir(null));
        final String message = removed ? "Copied game files removed"
                : "Some copied game files could not be removed";
        runOnUiThread(() -> Toast.makeText(this, message, Toast.LENGTH_LONG).show());
        return removed;
    }
}
