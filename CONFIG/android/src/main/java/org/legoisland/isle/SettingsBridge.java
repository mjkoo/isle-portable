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
    static native boolean restoreClosing();
    static native void requestMenu();
    static native String exportId();
    // Metadata is error, capture time in milliseconds, warning, then canonical filenames.
    static native String[] exportInfo(String id);
    static native byte[][] exportData(String id);
    static native String path();
    static native String[] renderers();
    static native String[] read(String path, String[] keys);
    static native String write(String path, String[] keys, String[] values, String[] renderers);
}
