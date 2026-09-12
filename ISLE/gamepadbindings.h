#ifndef GAMEPADBINDINGS_H
#define GAMEPADBINDINGS_H

#include <SDL3/SDL_gamepad.h>

// Game actions bound to physical gamepad inputs through the [gamepad] section of isle.ini.
// Bindings name SDL's positional buttons, so one set applies to every connected pad.
namespace GamepadBindings
{
enum Input {
	e_south,
	e_east,
	e_west,
	e_north,
	e_leftShoulder,
	e_rightShoulder,
	e_leftTrigger,
	e_rightTrigger,
	e_leftStick,
	e_rightStick,
	e_back,
	e_start,
	e_guide,
	e_inputCount
};

enum Action {
	e_unset = -1,
	e_none,
	e_click,
	e_space,
	e_escape,
	e_pause
};

// Which of South and East clicks by default: the one labelled A, or a fixed choice.
enum Confirm {
	e_confirmLabel,
	e_confirmSouth,
	e_confirmEast
};

enum Platform {
	e_platformDefault,
	e_platformVita,
	e_platformAndroid
};

// Unset inputs take the game's default for the platform.
struct Table {
	Table()
	{
		for (Action& action : m_actions) {
			action = e_unset;
		}
	}

	Confirm m_confirm = e_confirmLabel;
	Action m_actions[e_inputCount];
};

// What one input event does; e_none does nothing.
struct Result {
	Action m_action = e_none;
	bool m_pressed = false;
};

inline const char* Key(Input p_input)
{
	static const char* const keys[e_inputCount] = {
		"gamepad:south",
		"gamepad:east",
		"gamepad:west",
		"gamepad:north",
		"gamepad:leftshoulder",
		"gamepad:rightshoulder",
		"gamepad:lefttrigger",
		"gamepad:righttrigger",
		"gamepad:leftstick",
		"gamepad:rightstick",
		"gamepad:back",
		"gamepad:start",
		"gamepad:guide"
	};
	return keys[p_input];
}

inline const char* ConfirmKey()
{
	return "gamepad:confirm";
}

// Values are hand-editable, so they match without regard to ASCII case.
inline bool Matches(const char* p_value, const char* p_name)
{
	for (; *p_value && *p_name; p_value++, p_name++) {
		char c = *p_value >= 'A' && *p_value <= 'Z' ? *p_value - 'A' + 'a' : *p_value;
		if (c != *p_name) {
			return false;
		}
	}
	return !*p_value && !*p_name;
}

inline bool ParseAction(const char* p_value, Action& p_action)
{
	static const char* const names[] = {"none", "click", "space", "escape", "pause"};
	for (int i = 0; i < (int) (sizeof(names) / sizeof(names[0])); i++) {
		if (Matches(p_value, names[i])) {
			p_action = static_cast<Action>(i);
			return true;
		}
	}
	return false;
}

inline bool ParseConfirm(const char* p_value, Confirm& p_confirm)
{
	static const char* const names[] = {"label", "south", "east"};
	for (int i = 0; i < (int) (sizeof(names) / sizeof(names[0])); i++) {
		if (Matches(p_value, names[i])) {
			p_confirm = static_cast<Confirm>(i);
			return true;
		}
	}
	return false;
}

// p_get returns a key's value or null; p_invalid reports a value that falls back to the default.
template <class Get, class Invalid>
Table Parse(Get p_get, Invalid p_invalid)
{
	Table table;
	const char* value = p_get(ConfirmKey());
	if (value && !ParseConfirm(value, table.m_confirm)) {
		p_invalid(ConfirmKey(), value);
	}
	for (int i = 0; i < e_inputCount; i++) {
		value = p_get(Key(static_cast<Input>(i)));
		if (value && !ParseAction(value, table.m_actions[i])) {
			p_invalid(Key(static_cast<Input>(i)), value);
		}
	}
	return table;
}

inline Action Resolve(const Table& p_table, Input p_input, bool p_eastIsA, Platform p_platform)
{
	if (p_table.m_actions[p_input] != e_unset) {
		return p_table.m_actions[p_input];
	}

	switch (p_input) {
	case e_south:
	case e_east: {
		bool eastClicks = p_table.m_confirm == e_confirmLabel ? p_eastIsA : p_table.m_confirm == e_confirmEast;
		return (p_input == e_east) == eastClicks ? e_click : e_space;
	}
	case e_rightTrigger:
		return e_click;
	case e_back:
		return e_escape;
	case e_start:
		// On Vita, Start conflicts with the screenshot button combination.
		return p_platform == e_platformVita ? e_none : e_pause;
	default:
		return e_none;
	}
}

inline bool FromButton(SDL_GamepadButton p_button, Input& p_input)
{
	switch (p_button) {
	case SDL_GAMEPAD_BUTTON_SOUTH:
		p_input = e_south;
		return true;
	case SDL_GAMEPAD_BUTTON_EAST:
		p_input = e_east;
		return true;
	case SDL_GAMEPAD_BUTTON_WEST:
		p_input = e_west;
		return true;
	case SDL_GAMEPAD_BUTTON_NORTH:
		p_input = e_north;
		return true;
	case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
		p_input = e_leftShoulder;
		return true;
	case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
		p_input = e_rightShoulder;
		return true;
	case SDL_GAMEPAD_BUTTON_LEFT_STICK:
		p_input = e_leftStick;
		return true;
	case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
		p_input = e_rightStick;
		return true;
	case SDL_GAMEPAD_BUTTON_BACK:
		p_input = e_back;
		return true;
	case SDL_GAMEPAD_BUTTON_START:
		p_input = e_start;
		return true;
	case SDL_GAMEPAD_BUTTON_GUIDE:
		p_input = e_guide;
		return true;
	default:
		return false;
	}
}

inline bool FromTrigger(SDL_GamepadAxis p_axis, Input& p_input)
{
	switch (p_axis) {
	case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
		p_input = e_leftTrigger;
		return true;
	case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
		p_input = e_rightTrigger;
		return true;
	default:
		return false;
	}
}

// Turns input events into actions. A release acts on what its press did, so a binding or label
// that changes while an input is held cannot leave a click held. Records are per input and shared
// by every connected pad.
class Dispatcher {
public:
	explicit Dispatcher(Platform p_platform) : m_platform(p_platform) { Cancel(); }

	void SetTable(const Table& p_table) { m_table = p_table; }

	Result Button(Input p_input, bool p_down, bool p_eastIsA)
	{
		Result result;
		if (p_down) {
			result.m_action = m_held[p_input] = Resolve(m_table, p_input, p_eastIsA, m_platform);
			result.m_pressed = true;
		}
		else {
			// Only a click is held; key actions act once, on the press.
			if (m_held[p_input] == e_click) {
				result.m_action = e_click;
			}
			m_held[p_input] = e_none;
		}
		return result;
	}

	// A trigger acts once per pull past the sticks' dead zone. Its click defers to one another
	// source already holds, and its release only ends a click it started.
	Result Trigger(Input p_input, Sint16 p_value, bool p_clickDown)
	{
		bool& latched = m_latched[p_input == e_leftTrigger ? 0 : 1];
		bool pulled = p_value < -8000 || p_value > 8000;
		Result result;
		if (pulled == latched) {
			return result;
		}

		latched = pulled;
		if (pulled) {
			Action action = Resolve(m_table, p_input, false, m_platform);
			result.m_action = m_held[p_input] = action == e_click && p_clickDown ? e_none : action;
			result.m_pressed = true;
		}
		else {
			if (m_held[p_input] == e_click && p_clickDown) {
				result.m_action = e_click;
			}
			m_held[p_input] = e_none;
		}
		return result;
	}

	// Forgets held inputs without releasing them, for when the caller has cancelled input itself.
	void Cancel()
	{
		for (Action& held : m_held) {
			held = e_none;
		}
		m_latched[0] = m_latched[1] = false;
	}

private:
	Platform m_platform;
	Table m_table;
	Action m_held[e_inputCount];
	bool m_latched[2];
};
} // namespace GamepadBindings

#endif
