# Everything the JNI boundary needs by name. Three kinds, and each keep rule below is one of them:
# methods native calls on the activity (see ISLE/android/filepicker.cpp, quitprompt.cpp and
# settings.cpp), the bridge class native reads settings through, and the native methods Java
# declares and calls down into. The GameImport, GameFileCopier and QuitPrompt classes IsleActivity
# delegates to are reached only from Java and deliberately have no keep rule of their own.
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
    boolean hasFolderPicker();
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

-keep class org.legoisland.isle.AudioFocus {
    native <methods>;
}
