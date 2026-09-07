#ifndef ANDROID_SETTINGS_H
#define ANDROID_SETTINGS_H

#include <SDL3/SDL.h>

void Android_SetSettingsPath(const char* p_path);
void Android_CaptureRenderers(SDL_Window* p_window);
void Android_ShowMenuButton();
bool Android_TakeMenuRequest();
void Android_ShowStartupSettings(const char* p_error);

#endif
