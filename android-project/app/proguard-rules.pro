# Methods called from native code via JNI (see ISLE/android/filepicker.cpp,
# ISLE/android/quitprompt.cpp and ISLE/android/settings.cpp). Every entry point lives on
# IsleActivity, so this list is the
# whole JNI surface; the GameImport and QuitPrompt classes it delegates to are reached only from
# Java and deliberately have no keep rule of their own.
#
# A missing entry here fails only in release, where minifyEnabled is on.
-keep class org.legoisland.isle.IsleActivity {
    void startSaveRestore();
    int getSaveRestoreStatus();
    void startGameFileImport(java.lang.String);
    int getGameFileImportStatus();
    java.lang.String getImportedRoot();
    boolean hasImportedGameData();
    boolean removeImportedGameData();
    void showQuitPrompt(int);
    void showMenuButton();
    void showStartupSettings(java.lang.String);
    boolean isStartupSettingsOpen();
    void showStartupMessage(java.lang.String);
    int getQuitPromptStatus();
}

-keep class org.legoisland.isle.SettingsBridge { *; }

-keep class org.legoisland.isle.TouchControlsView {
    native <methods>;
}
