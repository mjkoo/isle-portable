# Methods called from native code via JNI (see ISLE/android/filepicker.cpp). Every entry point
# lives on IsleActivity, so this list is the whole JNI surface; the GameImport class it delegates
# to is reached only from Java and deliberately has no keep rule of its own.
#
# A missing entry here fails only in release, where minifyEnabled is on.
-keep class org.legoisland.isle.IsleActivity {
    int importGameFiles(java.lang.String);
    java.lang.String getImportedRoot();
}
