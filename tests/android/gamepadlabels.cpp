#include "gamepadbindings.h"

#include <cassert>

using namespace GamepadBindings;

// The default Click follows the face button labelled A, which SDL places East only on Nintendo
// layouts. A connected pad's mapping can still override its labels.
int main()
{
	struct {
		SDL_GamepadType m_type;
		SDL_GamepadButtonLabel m_east;
		Input m_click;
	} layouts[] = {
		{SDL_GAMEPAD_TYPE_STANDARD, SDL_GAMEPAD_BUTTON_LABEL_B, e_south},
		{SDL_GAMEPAD_TYPE_XBOX360, SDL_GAMEPAD_BUTTON_LABEL_B, e_south},
		{SDL_GAMEPAD_TYPE_XBOXONE, SDL_GAMEPAD_BUTTON_LABEL_B, e_south},
		{SDL_GAMEPAD_TYPE_PS4, SDL_GAMEPAD_BUTTON_LABEL_CIRCLE, e_south},
		{SDL_GAMEPAD_TYPE_PS5, SDL_GAMEPAD_BUTTON_LABEL_CIRCLE, e_south},
		{SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO, SDL_GAMEPAD_BUTTON_LABEL_A, e_east},
		{SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR, SDL_GAMEPAD_BUTTON_LABEL_A, e_east},
		{SDL_GAMEPAD_TYPE_GAMECUBE, SDL_GAMEPAD_BUTTON_LABEL_X, e_south},
	};
	for (const auto& layout : layouts) {
		SDL_GamepadButtonLabel east = SDL_GetGamepadButtonLabelForType(layout.m_type, SDL_GAMEPAD_BUTTON_EAST);
		assert(east == layout.m_east);
		bool eastIsA = east == SDL_GAMEPAD_BUTTON_LABEL_A;
		Input other = layout.m_click == e_south ? e_east : e_south;
		assert(Resolve(Table(), layout.m_click, eastIsA, e_platformDefault) == e_click);
		assert(Resolve(Table(), other, eastIsA, e_platformDefault) == e_space);
	}
}
