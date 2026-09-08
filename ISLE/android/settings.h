#ifndef ANDROID_SETTINGS_H
#define ANDROID_SETTINGS_H

#include <SDL3/SDL.h>

bool Android_RestoreBeforeStartup();
bool Android_SaveRestoreClosing();
void Android_SetSettingsPath(const char* p_path);
void Android_CaptureRenderers(SDL_Window* p_window);
void Android_ShowMenuButton();
bool Android_TakeMenuRequest();
void Android_CaptureSaveExport(const char* p_savePath, int p_saveResult);
void Android_ClearSaveExport();
void Android_ShowStartupSettings(const char* p_error, const char* p_savePath = nullptr, int p_saveResult = 1);

#endif
