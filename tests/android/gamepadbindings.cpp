#include "gamepadbindings.h"

#include <cassert>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using namespace GamepadBindings;

static const Platform c_platforms[] = {e_platformDefault, e_platformVita, e_platformAndroid};

// The fixed mapping the table replaced: the face button labelled A clicks, the other face button
// sends Space, Back sends Escape, Start pauses (except on Vita) and the right trigger clicks.
static Action Today(Input p_input, bool p_eastIsA, Platform p_platform)
{
	switch (p_input) {
	case e_south:
		return p_eastIsA ? e_space : e_click;
	case e_east:
		return p_eastIsA ? e_click : e_space;
	case e_back:
		return e_escape;
	case e_start:
		return p_platform == e_platformVita ? e_none : e_pause;
	case e_rightTrigger:
		return e_click;
	default:
		return e_none;
	}
}

static Table ParseMap(const std::map<std::string, std::string>& p_values, std::vector<std::string>* p_invalid = nullptr)
{
	return Parse(
		[&](const char* p_key) -> const char* {
			auto value = p_values.find(p_key);
			return value == p_values.end() ? nullptr : value->second.c_str();
		},
		[&](const char* p_key, const char* p_value) {
			if (p_invalid) {
				p_invalid->push_back(std::string(p_key) + "=" + p_value);
			}
		}
	);
}

static bool Same(Result p_result, Action p_action, bool p_pressed)
{
	return p_result.m_action == p_action && p_result.m_pressed == p_pressed;
}

static void DefaultsMatchTheFixedMapping()
{
	Table table;
	for (int input = 0; input < e_inputCount; input++) {
		for (bool eastIsA : {false, true}) {
			for (Platform platform : c_platforms) {
				Input in = static_cast<Input>(input);
				assert(Resolve(table, in, eastIsA, platform) == Today(in, eastIsA, platform));
			}
		}
	}
}

static void KeysAreLowercaseAndDistinct()
{
	const char* expected[] = {
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
	static_assert(sizeof(expected) / sizeof(expected[0]) == e_inputCount, "one key per input");
	for (int input = 0; input < e_inputCount; input++) {
		assert(!std::strcmp(Key(static_cast<Input>(input)), expected[input]));
	}
	assert(!std::strcmp(ConfirmKey(), "gamepad:confirm"));
}

static void ButtonsAndTriggersMapToInputs()
{
	struct {
		SDL_GamepadButton m_button;
		Input m_input;
	} buttons[] = {
		{SDL_GAMEPAD_BUTTON_SOUTH, e_south},
		{SDL_GAMEPAD_BUTTON_EAST, e_east},
		{SDL_GAMEPAD_BUTTON_WEST, e_west},
		{SDL_GAMEPAD_BUTTON_NORTH, e_north},
		{SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, e_leftShoulder},
		{SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, e_rightShoulder},
		{SDL_GAMEPAD_BUTTON_LEFT_STICK, e_leftStick},
		{SDL_GAMEPAD_BUTTON_RIGHT_STICK, e_rightStick},
		{SDL_GAMEPAD_BUTTON_BACK, e_back},
		{SDL_GAMEPAD_BUTTON_START, e_start},
		{SDL_GAMEPAD_BUTTON_GUIDE, e_guide},
	};
	for (const auto& button : buttons) {
		Input input;
		assert(FromButton(button.m_button, input) && input == button.m_input);
	}
	Input input = e_guide;
	for (SDL_GamepadButton button :
		 {SDL_GAMEPAD_BUTTON_DPAD_UP,
		  SDL_GAMEPAD_BUTTON_DPAD_DOWN,
		  SDL_GAMEPAD_BUTTON_DPAD_LEFT,
		  SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
		  SDL_GAMEPAD_BUTTON_MISC1,
		  SDL_GAMEPAD_BUTTON_TOUCHPAD,
		  SDL_GAMEPAD_BUTTON_INVALID}) {
		assert(!FromButton(button, input));
	}
	assert(FromTrigger(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, input) && input == e_leftTrigger);
	assert(FromTrigger(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, input) && input == e_rightTrigger);
	assert(!FromTrigger(SDL_GAMEPAD_AXIS_LEFTX, input));
	assert(!FromTrigger(SDL_GAMEPAD_AXIS_RIGHTY, input));
}

static void ParsingIgnoresCaseAndRejectsUnknownValues()
{
	std::vector<std::string> invalid;
	Table table = ParseMap(
		{{"gamepad:north", "Space"},
		 {"gamepad:west", "ESCAPE"},
		 {"gamepad:back", "none"},
		 {"gamepad:guide", "pause"},
		 {"gamepad:leftshoulder", "Click"},
		 {"gamepad:start", "jump"},
		 {"gamepad:rightshoulder", ""},
		 {"gamepad:confirm", "East"}},
		&invalid
	);
	assert(table.m_actions[e_north] == e_space);
	assert(table.m_actions[e_west] == e_escape);
	assert(table.m_actions[e_back] == e_none);
	assert(table.m_actions[e_guide] == e_pause);
	assert(table.m_actions[e_leftShoulder] == e_click);
	assert(table.m_actions[e_start] == e_unset);
	assert(table.m_actions[e_rightShoulder] == e_unset);
	assert(table.m_actions[e_south] == e_unset);
	assert(table.m_confirm == e_confirmEast);
	assert(invalid.size() == 2);
	assert(invalid[0] == "gamepad:rightshoulder=" || invalid[1] == "gamepad:rightshoulder=");
	assert(invalid[0] == "gamepad:start=jump" || invalid[1] == "gamepad:start=jump");

	invalid.clear();
	table = ParseMap({{"gamepad:confirm", "click"}}, &invalid);
	assert(table.m_confirm == e_confirmLabel);
	assert(invalid.size() == 1 && invalid[0] == "gamepad:confirm=click");
	assert(ParseMap({{"gamepad:confirm", "SOUTH"}}).m_confirm == e_confirmSouth);
	assert(ParseMap({{"gamepad:confirm", "Label"}}).m_confirm == e_confirmLabel);

	Action action = e_space;
	assert(!ParseAction("spacebar", action) && action == e_space);
	assert(!ParseAction("spac", action) && action == e_space);
	assert(ParseAction("NoNe", action) && action == e_none);
	Confirm confirm = e_confirmEast;
	assert(!ParseConfirm("", confirm) && confirm == e_confirmEast);
}

static void ConfirmChoosesTheClickingFaceButton()
{
	for (bool eastIsA : {false, true}) {
		Table south = ParseMap({{"gamepad:confirm", "south"}});
		assert(Resolve(south, e_south, eastIsA, e_platformDefault) == e_click);
		assert(Resolve(south, e_east, eastIsA, e_platformDefault) == e_space);
		Table east = ParseMap({{"gamepad:confirm", "east"}});
		assert(Resolve(east, e_south, eastIsA, e_platformDefault) == e_space);
		assert(Resolve(east, e_east, eastIsA, e_platformDefault) == e_click);
	}

	// An explicit binding wins over confirm, and leaves the other face button's default alone.
	Table table = ParseMap({{"gamepad:confirm", "east"}, {"gamepad:east", "pause"}});
	assert(Resolve(table, e_east, false, e_platformDefault) == e_pause);
	assert(Resolve(table, e_south, false, e_platformDefault) == e_space);
}

static void ExplicitBindingsApplyOnEveryPlatform()
{
	Table table = ParseMap({{"gamepad:start", "pause"}, {"gamepad:righttrigger", "none"}, {"gamepad:guide", "escape"}});
	for (Platform platform : c_platforms) {
		assert(Resolve(table, e_start, false, platform) == e_pause);
		assert(Resolve(table, e_rightTrigger, false, platform) == e_none);
		assert(Resolve(table, e_guide, false, platform) == e_escape);
	}
}

static void ButtonReleasesFollowTheirPress()
{
	Dispatcher pad(e_platformDefault);
	assert(Same(pad.Button(e_south, true, false), e_click, true));
	assert(Same(pad.Button(e_south, false, false), e_click, false));
	assert(Same(pad.Button(e_east, true, false), e_space, true));
	assert(Same(pad.Button(e_east, false, false), e_none, false));
	assert(Same(pad.Button(e_back, true, false), e_escape, true));
	assert(Same(pad.Button(e_back, false, false), e_none, false));
	assert(Same(pad.Button(e_start, true, false), e_pause, true));
	assert(Same(pad.Button(e_start, false, false), e_none, false));
	assert(Same(pad.Button(e_north, true, false), e_none, true));
	assert(Same(pad.Button(e_north, false, false), e_none, false));

	// A layout toggle while held: the release still ends the click the press started.
	assert(Same(pad.Button(e_south, true, false), e_click, true));
	assert(Same(pad.Button(e_south, false, true), e_click, false));
	assert(Same(pad.Button(e_east, true, false), e_space, true));
	assert(Same(pad.Button(e_east, false, true), e_none, false));

	// A rebind while held does the same.
	assert(Same(pad.Button(e_south, true, false), e_click, true));
	pad.SetTable(ParseMap({{"gamepad:south", "space"}}));
	assert(Same(pad.Button(e_south, false, false), e_click, false));
	assert(Same(pad.Button(e_south, true, false), e_space, true));
	assert(Same(pad.Button(e_south, false, false), e_none, false));

	// Cancelling forgets held presses, so their releases do nothing.
	pad.SetTable(Table());
	assert(Same(pad.Button(e_south, true, false), e_click, true));
	pad.Cancel();
	assert(Same(pad.Button(e_south, false, false), e_none, false));

	// A release without a press does nothing.
	assert(Same(pad.Button(e_east, false, true), e_none, false));
}

static void TriggersPressOnceAcrossTheDeadZone()
{
	Dispatcher pad(e_platformDefault);
	assert(Same(pad.Trigger(e_rightTrigger, 8000, false), e_none, false));
	assert(Same(pad.Trigger(e_rightTrigger, 8001, false), e_click, true));
	assert(Same(pad.Trigger(e_rightTrigger, 20000, true), e_none, false));
	assert(Same(pad.Trigger(e_rightTrigger, 32767, true), e_none, false));
	assert(Same(pad.Trigger(e_rightTrigger, 8000, true), e_click, false));
	assert(Same(pad.Trigger(e_rightTrigger, 0, false), e_none, false));
	assert(Same(pad.Trigger(e_rightTrigger, -8001, false), e_click, true));
	assert(Same(pad.Trigger(e_rightTrigger, 0, true), e_click, false));

	// An unbound trigger never acts.
	assert(Same(pad.Trigger(e_leftTrigger, 32767, false), e_none, true));
	assert(Same(pad.Trigger(e_leftTrigger, 0, false), e_none, false));

	// A trigger bound to a key sends it once per pull.
	pad.SetTable(ParseMap({{"gamepad:lefttrigger", "space"}}));
	assert(Same(pad.Trigger(e_leftTrigger, 9000, false), e_space, true));
	assert(Same(pad.Trigger(e_leftTrigger, 12000, false), e_none, false));
	assert(Same(pad.Trigger(e_leftTrigger, 0, false), e_none, false));
	assert(Same(pad.Trigger(e_leftTrigger, 9000, false), e_space, true));
	assert(Same(pad.Trigger(e_leftTrigger, 0, false), e_none, false));
}

static void TriggerClicksShareTheClickWithOtherSources()
{
	Dispatcher pad(e_platformDefault);

	// Another source already holds the click: the trigger does not press, and its release does
	// not end that other click.
	assert(Same(pad.Trigger(e_rightTrigger, 9000, true), e_none, true));
	assert(Same(pad.Trigger(e_rightTrigger, 0, true), e_none, false));

	// The click was ended elsewhere while the trigger was held: the trigger neither presses
	// again while still held nor sends a second release.
	assert(Same(pad.Trigger(e_rightTrigger, 9000, false), e_click, true));
	assert(Same(pad.Trigger(e_rightTrigger, 9500, false), e_none, false));
	assert(Same(pad.Trigger(e_rightTrigger, 0, false), e_none, false));

	// Two buttons bound to Click share one click: the first release ends it.
	pad.SetTable(ParseMap({{"gamepad:north", "click"}}));
	assert(Same(pad.Button(e_south, true, false), e_click, true));
	assert(Same(pad.Button(e_north, true, false), e_click, true));
	assert(Same(pad.Button(e_north, false, false), e_click, false));
	assert(Same(pad.Button(e_south, false, false), e_click, false));
}

static void CancellingReleasesTheTriggerLatch()
{
	Dispatcher pad(e_platformDefault);
	assert(Same(pad.Trigger(e_rightTrigger, 9000, false), e_click, true));
	pad.Cancel();

	// Still held after the menu closes: the next event presses again, as before the table.
	assert(Same(pad.Trigger(e_rightTrigger, 9100, false), e_click, true));
	assert(Same(pad.Trigger(e_rightTrigger, 0, true), e_click, false));
}

static void VitaLeavesStartUnboundUnlessConfigured()
{
	Dispatcher vita(e_platformVita);
	assert(Same(vita.Button(e_start, true, false), e_none, true));
	assert(Same(vita.Button(e_start, false, false), e_none, false));
	vita.SetTable(ParseMap({{"gamepad:start", "pause"}}));
	assert(Same(vita.Button(e_start, true, false), e_pause, true));
}

int main()
{
	DefaultsMatchTheFixedMapping();
	KeysAreLowercaseAndDistinct();
	ButtonsAndTriggersMapToInputs();
	ParsingIgnoresCaseAndRejectsUnknownValues();
	ConfirmChoosesTheClickingFaceButton();
	ExplicitBindingsApplyOnEveryPlatform();
	ButtonReleasesFollowTheirPress();
	TriggersPressOnceAcrossTheDeadZone();
	TriggerClicksShareTheClickWithOtherSources();
	CancellingReleasesTheTriggerLatch();
	VitaLeavesStartUnboundUnlessConfigured();
}
