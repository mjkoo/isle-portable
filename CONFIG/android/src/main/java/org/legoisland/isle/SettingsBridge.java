package org.legoisland.isle;

final class SettingsBridge {
    static {
        System.loadLibrary("SDL3");
        System.loadLibrary("lego1");
        System.loadLibrary("isle");
    }

    static native String recoverRestore();
    static native boolean canCancelRestore();
    static native String cancelRestore();
    // Error, then timestamp and empty/saved marker for the previous set.
    static native String[] restoreInfo(String id);
    static native String scheduleRestore(String id, String[] names, byte[][] data, boolean previous);
    // Settings recorded work that applies before the next engine start, so the game must close.
    static native boolean startupWorkScheduled();
    // Id, then the directory to copy the new LEGO folder into. Claimed until endGameFilesStaging,
    // so a startup in this process does not collect it meanwhile.
    static native String[] beginGameFilesStaging(String root);
    static native void endGameFilesStaging(String id);
    // The first required game file missing under root, or null when the tree is complete.
    static native String missingGameFile(String root);
    static native boolean gameFilesPending(String filesDir, String root);
    // Error, or null once the change waits for the next startup. A null id removes the game files.
    static native String scheduleGameFiles(String filesDir, String root, String config, String id);
    static native void requestMenu();
    static native String exportId();
    // Metadata is error, capture time in milliseconds, warning, then canonical filenames.
    static native String[] exportInfo(String id);
    static native byte[][] exportData(String id);
    static native String path();
    static native String[] renderers();
    // The characters multiplayer can display, from the table it resolves its own option against.
    static native String[] actors();
    static native String[] read(String path, String[] keys);
    static native String write(String path, String[] keys, String[] values, String[] renderers);
}
