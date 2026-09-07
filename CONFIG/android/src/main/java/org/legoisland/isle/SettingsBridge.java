package org.legoisland.isle;

final class SettingsBridge {
    static {
        System.loadLibrary("SDL3");
        System.loadLibrary("lego1");
        System.loadLibrary("isle");
    }

    static native void requestMenu();
    static native String path();
    static native String[] renderers();
    static native String[] read(String path, String[] keys);
    static native String write(String path, String[] keys, String[] values, String[] renderers);
}
