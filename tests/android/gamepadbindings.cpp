#include "gamepadbindings.h"

#include <cassert>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using namespace GamepadBindings;

static const Platform c_platforms[] = {e_platformDefault, e_platformVita, e_platformAndroid};
static const SDL_JoystickID c_pad = 1;

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
				// The one deliberate change: on Android, Start opens the Android menu, which no
				// controller input could reach before.
				bool menu = in == e_start && platform == e_platformAndroid;
				assert(Resolve(table, in, eastIsA, platform) == (menu ? e_menu : Today(in, eastIsA, platform)));
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
	assert(Same(pad.Button(c_pad, e_south, true, false), e_click, true));
	assert(Same(pad.Button(c_pad, e_south, false, false), e_click, false));
	assert(Same(pad.Button(c_pad, e_east, true, false), e_space, true));
	assert(Same(pad.Button(c_pad, e_east, false, false), e_none, false));
	assert(Same(pad.Button(c_pad, e_back, true, false), e_escape, true));
	assert(Same(pad.Button(c_pad, e_back, false, false), e_none, false));
	assert(Same(pad.Button(c_pad, e_start, true, false), e_pause, true));
	assert(Same(pad.Button(c_pad, e_start, false, false), e_none, false));
	assert(Same(pad.Button(c_pad, e_north, true, false), e_none, true));
	assert(Same(pad.Button(c_pad, e_north, false, false), e_none, false));

	// A layout toggle while held: the release still ends the click the press started.
	assert(Same(pad.Button(c_pad, e_south, true, false), e_click, true));
	assert(Same(pad.Button(c_pad, e_south, false, true), e_click, false));
	assert(Same(pad.Button(c_pad, e_east, true, false), e_space, true));
	assert(Same(pad.Button(c_pad, e_east, false, true), e_none, false));

	// A rebind while held does the same.
	assert(Same(pad.Button(c_pad, e_south, true, false), e_click, true));
	pad.SetTable(ParseMap({{"gamepad:south", "space"}}));
	assert(Same(pad.Button(c_pad, e_south, false, false), e_click, false));
	assert(Same(pad.Button(c_pad, e_south, true, false), e_space, true));
	assert(Same(pad.Button(c_pad, e_south, false, false), e_none, false));

	// Cancelling forgets held presses, so their releases do nothing.
	pad.SetTable(Table());
	assert(Same(pad.Button(c_pad, e_south, true, false), e_click, true));
	pad.Cancel();
	assert(Same(pad.Button(c_pad, e_south, false, false), e_none, false));

	// A release without a press does nothing.
	assert(Same(pad.Button(c_pad, e_east, false, true), e_none, false));
}

static void TriggersPressOnceAcrossTheDeadZone()
{
	Dispatcher pad(e_platformDefault);
	assert(Same(pad.Trigger(c_pad, e_rightTrigger, 8000, false), e_none, false));
	assert(Same(pad.Trigger(c_pad, e_rightTrigger, 8001, false), e_click, true));
	assert(Same(pad.Trigger(c_pad, e_rightTrigger, 20000, true), e_none, false));
	assert(Same(pad.Trigger(c_pad, e_rightTrigger, 32767, true), e_none, false));
	assert(Same(pad.Trigger(c_pad, e_rightTrigger, 8000, true), e_click, false));
	assert(Same(pad.Trigger(c_pad, e_rightTrigger, 0, false), e_none, false));
	assert(Same(pad.Trigger(c_pad, e_rightTrigger, -8001, false), e_click, true));
	assert(Same(pad.Trigger(c_pad, e_rightTrigger, 0, true), e_click, false));

	// An unbound trigger never acts.
	assert(Same(pad.Trigger(c_pad, e_leftTrigger, 32767, false), e_none, true));
	assert(Same(pad.Trigger(c_pad, e_leftTrigger, 0, false), e_none, false));

	// A trigger bound to a key sends it once per pull.
	pad.SetTable(ParseMap({{"gamepad:lefttrigger", "space"}}));
	assert(Same(pad.Trigger(c_pad, e_leftTrigger, 9000, false), e_space, true));
	assert(Same(pad.Trigger(c_pad, e_leftTrigger, 12000, false), e_none, false));
	assert(Same(pad.Trigger(c_pad, e_leftTrigger, 0, false), e_none, false));
	assert(Same(pad.Trigger(c_pad, e_leftTrigger, 9000, false), e_space, true));
	assert(Same(pad.Trigger(c_pad, e_leftTrigger, 0, false), e_none, false));
}

static void TriggerClicksShareTheClickWithOtherSources()
{
	Dispatcher pad(e_platformDefault);

	// Another source already holds the click: the trigger does not press, and its release does
	// not end that other click.
	assert(Same(pad.Trigger(c_pad, e_rightTrigger, 9000, true), e_none, true));
	assert(Same(pad.Trigger(c_pad, e_rightTrigger, 0, true), e_none, false));

	// The click was ended elsewhere while the trigger was held: the trigger neither presses
	// again while still held nor sends a second release.
	assert(Same(pad.Trigger(c_pad, e_rightTrigger, 9000, false), e_click, true));
	assert(Same(pad.Trigger(c_pad, e_rightTrigger, 9500, false), e_none, false));
	assert(Same(pad.Trigger(c_pad, e_rightTrigger, 0, false), e_none, false));

	// Two buttons bound to Click share one click: the first release ends it.
	pad.SetTable(ParseMap({{"gamepad:north", "click"}}));
	assert(Same(pad.Button(c_pad, e_south, true, false), e_click, true));
	assert(Same(pad.Button(c_pad, e_north, true, false), e_click, true));
	assert(Same(pad.Button(c_pad, e_north, false, false), e_click, false));
	assert(Same(pad.Button(c_pad, e_south, false, false), e_click, false));
}

static void CancellingKeepsTriggerLatches()
{
	Dispatcher pad(e_platformDefault);
	assert(Same(pad.Trigger(c_pad, e_rightTrigger, 9000, false), e_click, true));

	// Still pulled when input is cancelled: it does not act on its next movement, and its release
	// ends nothing, since cancelling already ended the click.
	pad.Cancel();
	assert(Same(pad.Trigger(c_pad, e_rightTrigger, 9100, false), e_none, false));
	assert(Same(pad.Trigger(c_pad, e_rightTrigger, 0, false), e_none, false));
	assert(Same(pad.Trigger(c_pad, e_rightTrigger, 9000, false), e_click, true));

	// Released while its events were discarded: an analog pull passes below the dead zone on the
	// way, which clears the latch, so the pull still acts.
	pad.Cancel();
	assert(Same(pad.Trigger(c_pad, e_rightTrigger, 3000, false), e_none, false));
	assert(Same(pad.Trigger(c_pad, e_rightTrigger, 9000, false), e_click, true));

	// A trigger that opened the menu does not reopen it when still held after the menu closes.
	Dispatcher android(e_platformAndroid);
	android.SetTable(ParseMap({{"gamepad:lefttrigger", "menu"}}));
	assert(Same(android.Trigger(c_pad, e_leftTrigger, 20000, false), e_menu, true));
	android.Cancel();
	assert(Same(android.Trigger(c_pad, e_leftTrigger, 21000, false), e_none, false));
}

static void VitaLeavesStartUnboundUnlessConfigured()
{
	Dispatcher vita(e_platformVita);
	assert(Same(vita.Button(c_pad, e_start, true, false), e_none, true));
	assert(Same(vita.Button(c_pad, e_start, false, false), e_none, false));
	vita.SetTable(ParseMap({{"gamepad:start", "pause"}}));
	assert(Same(vita.Button(c_pad, e_start, true, false), e_pause, true));
}

static void TriggersLatchIndependently()
{
	Dispatcher pad(e_platformAndroid);
	pad.SetTable(ParseMap({{"gamepad:lefttrigger", "menu"}}));
	assert(Same(pad.Trigger(c_pad, e_rightTrigger, 9000, false), e_click, true));
	assert(Same(pad.Trigger(c_pad, e_leftTrigger, 9000, true), e_menu, true));
	assert(Same(pad.Trigger(c_pad, e_leftTrigger, 12000, true), e_none, false));
	assert(Same(pad.Trigger(c_pad, e_rightTrigger, 12000, true), e_none, false));
	assert(Same(pad.Trigger(c_pad, e_leftTrigger, 0, true), e_none, false));
	assert(Same(pad.Trigger(c_pad, e_rightTrigger, 0, true), e_click, false));
	assert(Same(pad.Trigger(c_pad, e_leftTrigger, 9000, false), e_menu, true));
}

static void EachPadKeepsItsOwnPresses()
{
	Dispatcher pads(e_platformDefault);
	const SDL_JoystickID xbox = 1, nintendo = 2;

	// The same button on two layouts: each release ends what that pad's press started.
	assert(Same(pads.Button(xbox, e_south, true, false), e_click, true));
	assert(Same(pads.Button(nintendo, e_south, true, true), e_space, true));
	assert(Same(pads.Button(xbox, e_south, false, false), e_click, false));
	assert(Same(pads.Button(nintendo, e_south, false, true), e_none, false));

	// One pad's trigger at rest does not release another pad's pull.
	assert(Same(pads.Trigger(xbox, e_rightTrigger, 9000, false), e_click, true));
	assert(Same(pads.Trigger(nintendo, e_rightTrigger, 0, true), e_none, false));
	assert(Same(pads.Trigger(nintendo, e_rightTrigger, 9000, true), e_none, true));
	assert(Same(pads.Trigger(xbox, e_rightTrigger, 0, true), e_click, false));
	assert(Same(pads.Trigger(nintendo, e_rightTrigger, 0, false), e_none, false));

	// Cancelling forgets every pad's presses.
	assert(Same(pads.Button(xbox, e_south, true, false), e_click, true));
	assert(Same(pads.Button(nintendo, e_east, true, true), e_click, true));
	pads.Cancel();
	assert(Same(pads.Button(xbox, e_south, false, false), e_none, false));
	assert(Same(pads.Button(nintendo, e_east, false, true), e_none, false));

	// SDL returns a removed pad's trigger to rest before reporting the removal, which ends its
	// click; the pad is then forgotten, so a pad given the same id starts afresh.
	assert(Same(pads.Trigger(xbox, e_rightTrigger, 9000, false), e_click, true));
	assert(Same(pads.Trigger(xbox, e_rightTrigger, 0, true), e_click, false));
	pads.Removed(xbox);
	assert(Same(pads.Trigger(nintendo, e_rightTrigger, 9000, false), e_click, true));
	pads.Removed(nintendo);
	assert(Same(pads.Trigger(nintendo, e_rightTrigger, 9000, false), e_click, true));
}

static void MenuOpensOnlyOnAndroid()
{
	Action action = e_none;
	assert(ParseAction("Menu", action) && action == e_menu);

	Table table = ParseMap({{"gamepad:start", "menu"}, {"gamepad:north", "menu"}});
	assert(Resolve(table, e_start, false, e_platformAndroid) == e_menu);
	assert(Resolve(table, e_north, false, e_platformAndroid) == e_menu);
	for (Platform platform : {e_platformDefault, e_platformVita}) {
		assert(Resolve(table, e_start, false, platform) == e_none);
		assert(Resolve(table, e_north, false, platform) == e_none);
	}

	// Pause stays available on Android when chosen explicitly.
	assert(Resolve(ParseMap({{"gamepad:start", "pause"}}), e_start, false, e_platformAndroid) == e_pause);

	Dispatcher android(e_platformAndroid);
	assert(Same(android.Button(c_pad, e_start, true, false), e_menu, true));
	assert(Same(android.Button(c_pad, e_start, false, false), e_none, false));
}

int main()
{
	MenuOpensOnlyOnAndroid();
	DefaultsMatchTheFixedMapping();
	KeysAreLowercaseAndDistinct();
	ButtonsAndTriggersMapToInputs();
	ParsingIgnoresCaseAndRejectsUnknownValues();
	ConfirmChoosesTheClickingFaceButton();
	ExplicitBindingsApplyOnEveryPlatform();
	ButtonReleasesFollowTheirPress();
	TriggersPressOnceAcrossTheDeadZone();
	TriggerClicksShareTheClickWithOtherSources();
	CancellingKeepsTriggerLatches();
	VitaLeavesStartUnboundUnlessConfigured();
	TriggersLatchIndependently();
	EachPadKeepsItsOwnPresses();
}
