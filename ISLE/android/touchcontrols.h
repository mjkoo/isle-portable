#ifndef ANDROID_TOUCHCONTROLS_H
#define ANDROID_TOUCHCONTROLS_H

#include "touchmovement.h"

#include <SDL3/SDL_video.h>

void Android_ConfigureTouchControls(bool p_enabled);
void Android_ObserveTouchControlsInput(const SDL_Event& p_event, bool p_mouseWarped);
bool Android_TakeTouchControlsReset();
void Android_ClearTouchControls();
void Android_PublishTouchControls(
	SDL_Window* p_window,
	bool p_visible,
	int p_scheme,
	int p_contentWidth,
	int p_contentHeight,
	const TouchMovement::State& p_movement
);

#endif
