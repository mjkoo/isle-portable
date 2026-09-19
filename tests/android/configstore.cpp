#include "configstore.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <thread>
#include <unistd.h>

static std::string Read(const std::string& p_path)
{
	std::ifstream file(p_path);
	return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

int main()
{
	char directory[] = "/tmp/isle-config-test-XXXXXX";
	assert(mkdtemp(directory));
	std::string path = std::string(directory) + "/isle.ini";
	std::string exportPath;
	assert(Android_ResolveSaveExportPath(path, "/private/saves", exportPath).empty());
	assert(exportPath == "/private/saves" && !std::filesystem::exists(path));
	std::ofstream(path) << "[isle]\nsavepath=/custom/progress\n";
	assert(Android_ResolveSaveExportPath(path, "/private/saves", exportPath).empty());
	assert(exportPath == "/custom/progress");
	std::ofstream(path) << "[isle]\nsavepath=\n";
	assert(!Android_ResolveSaveExportPath(path, "/private/saves", exportPath).empty());
	std::ofstream(path) << "[isle]\nmalformed config\n";
	assert(!Android_ResolveSaveExportPath(path, "/private/saves", exportPath).empty());
	const std::string original =
		"[isle]\nmusic=true\ndiskpath=/assets\ncustom=preserve\n[extensions]\nmultiplayer=true\n";
	std::ofstream(path) << original;
	assert(Android_UpdateConfig(path, {{"isle:music", "false"}}).empty());
	std::vector<std::pair<bool, std::string>> values;
	assert(Android_ReadConfig(
			   path,
			   {"isle:music", "isle:diskpath", "isle:custom", "extensions:multiplayer", "isle:savepath"},
			   values
	)
			   .empty());
	assert(values[0].second == "false" && values[1].second == "/assets");
	assert(values[2].second == "preserve" && values[3].second == "true" && !values[4].first);
	assert(Android_UpdateConfig(path, {{"isle:music", nullptr}}).empty());
	values.clear();
	assert(Android_ReadConfig(path, {"isle:music", "isle:diskpath"}, values).empty());
	assert(!values[0].first && values[1].second == "/assets");

	std::thread first([&] { assert(Android_UpdateConfig(path, {{"isle:music", "true"}}).empty()); });
	std::thread second([&] { assert(Android_UpdateConfig(path, {{"isle:diskpath", "/new/assets"}}).empty()); });
	first.join();
	second.join();
	values.clear();
	assert(Android_ReadConfig(path, {"isle:music", "isle:diskpath"}, values).empty());
	assert(values[0].second == "true" && values[1].second == "/new/assets");

	std::string before = Read(path);
	std::filesystem::create_directory(path + ".new");
	assert(!Android_UpdateConfig(path, {{"isle:music", "false"}}).empty());
	assert(Read(path) == before);
	std::filesystem::remove(path + ".new");
	assert(Android_UpdateConfig(path, {{"isle:music", "false"}}).empty());

	std::ofstream(path) << "[isle]\nthis is not valid ini\n";
	before = Read(path);
	assert(!Android_UpdateConfig(path, {{"isle:music", "false"}}).empty());
	assert(Read(path) == before);
	values.clear();
	assert(!Android_ReadConfig(path, {"isle:music"}, values).empty());
	assert(!Android_UpdateConfig(path + ".missing", {{"isle:music", "true"}}).empty());
	assert(!std::filesystem::exists(path + ".missing"));

	std::ofstream(path) << "[extensions]\nmultiplayer=true\n";
	values.clear();
	assert(Android_ReadConfig(path, {"isle:music"}, values).empty());
	assert(!values[0].first);
	assert(Android_UpdateConfig(path, {{"isle:music", "false"}}).empty());
	values.clear();
	assert(Android_ReadConfig(path, {"isle:music", "extensions:multiplayer"}, values).empty());
	assert(values[0].second == "false" && values[1].second == "true");

	assert(Android_ValidateSetting("isle:music", nullptr, {}));
	assert(Android_ValidateSetting("isle:music", "false", {}));
	assert(!Android_ValidateSetting("isle:music", "garbage", {}));
	assert(Android_ValidateSetting("isle:show touch controls", nullptr, {}));
	assert(Android_ValidateSetting("isle:show touch controls", "true", {}));
	assert(Android_ValidateSetting("isle:show touch controls", "false", {}));
	assert(!Android_ValidateSetting("isle:show touch controls", "1", {}));
	assert(Android_UpdateConfig(path, {{"isle:show touch controls", "false"}}).empty());
	values.clear();
	assert(Android_ReadConfig(path, {"isle:show touch controls"}, values).empty());
	assert(values[0].first && values[0].second == "false");
	assert(Android_UpdateConfig(path, {{"isle:show touch controls", nullptr}}).empty());
	values.clear();
	assert(Android_ReadConfig(path, {"isle:show touch controls", "extensions:multiplayer"}, values).empty());
	assert(!values[0].first && values[1].second == "true");
	assert(!Android_ValidateSetting("isle:savepath", nullptr, {}));
	assert(Android_ValidateSetting("isle:cursor sensitivity", "0.1", {}));
	assert(Android_ValidateSetting("isle:cursor sensitivity", "20", {}));
	for (const char* invalid : {"", "nan", "inf", "0", "21", "2garbage"}) {
		assert(!Android_ValidateSetting("isle:cursor sensitivity", invalid, {}));
	}
	assert(!Android_ValidateSetting("isle:horizontal resolution", "0", {}));
	assert(!Android_ValidateSetting("isle:msaa", "3", {}));
	assert(Android_ValidateSetting("isle:3d device id", "available", {"GLES", "available"}));
	assert(!Android_ValidateSetting("isle:3d device id", "unavailable", {"GLES", "available"}));
	assert(Android_ValidateSetting("isle:touch button scale", nullptr, {}));
	assert(Android_ValidateSetting("isle:touch button scale", "0.5", {}));
	assert(Android_ValidateSetting("isle:touch button scale", "2", {}));
	for (const char* invalid : {"", "0.49", "2.01", "nan", "1x"}) {
		assert(!Android_ValidateSetting("isle:touch button scale", invalid, {}));
	}
	assert(Android_ValidateSetting("isle:touch control opacity", nullptr, {}));
	assert(Android_ValidateSetting("isle:touch control opacity", "0.1", {}));
	assert(Android_ValidateSetting("isle:touch control opacity", "1", {}));
	for (const char* invalid : {"0", "0.09", "1.01", "inf"}) {
		assert(!Android_ValidateSetting("isle:touch control opacity", invalid, {}));
	}
	for (const char* key : {"isle:touch menu position", "isle:touch escape position", "isle:touch space position"}) {
		assert(Android_ValidateSetting(key, nullptr, {}));
		for (const char* valid : {"0,0", "1,1", "0.5000,0.2500"}) {
			assert(Android_ValidateSetting(key, valid, {}));
		}
		// "0,5000,0,2500" is what a comma-decimal locale would produce.
		for (const char* invalid :
			 {"",
			  "0.5",
			  "0.5,",
			  ",0.5",
			  "0.5,0.5,0.5",
			  "1.1,0",
			  "-0.1,0",
			  "nan,0",
			  "0.5,inf",
			  "0,5",
			  "0.5;0.5",
			  "0,5000,0,2500"}) {
			assert(!Android_ValidateSetting(key, invalid, {}));
		}
	}
	assert(!Android_ValidateSetting("isle:touch other position", "0.5,0.5", {}));

	// Range edges and hand-edited "%f" values. GraphicsSettingsTest checks every value Settings offers
	// against these ranges.
	const std::vector<std::pair<const char*, std::vector<const char*>>> validGraphics = {
		{"isle:island quality", {"1", "2"}},
		{"isle:island texture", {"0", "1"}},
		{"isle:max lod", {"0", "3.600000", "6"}},
		{"isle:max allowed extras", {"5", "40"}},
		{"isle:transition type", {"1", "5"}},
		{"isle:frame delta", {"1", "16.666666", "1000"}},
	};
	// Whole-number keys take plain decimal only: the game reads them with strtol's base detection, so
	// "010" would be 8 and "0.2e1" would be 0.
	const std::vector<std::pair<const char*, std::vector<const char*>>> invalidGraphics = {
		{"isle:island quality", {"0", "3", "1.5", "02", "0.2e1", "+2", " 2", "2.0"}},
		{"isle:island texture", {"-1", "2", "0.5", "01", "+1", "1.0"}},
		{"isle:max lod", {"-0.1", "6.1"}},
		{"isle:max allowed extras", {"4", "41", "20.5", "010", "0.1e2", "+10", " 10", "10.0"}},
		{"isle:transition type", {"0", "6", "2.5", "02", "0.2e1", "0x2"}},
		{"isle:frame delta", {"0", "0.5", "1001"}},
	};
	for (const auto& [key, valid] : validGraphics) {
		assert(Android_ValidateSetting(key, nullptr, {}));
		for (const char* value : valid) {
			assert(Android_ValidateSetting(key, value, {}));
		}
	}
	for (const auto& [key, invalid] : invalidGraphics) {
		for (const char* value : invalid) {
			assert(!Android_ValidateSetting(key, value, {}));
		}
		for (const char* value : {"", "nan", "inf", "2garbage"}) {
			assert(!Android_ValidateSetting(key, value, {}));
		}
	}

	// The extension keys. ExtensionSettingsTest checks every value Settings offers against these.
	const std::vector<std::pair<const char*, std::vector<const char*>>> validExtensions = {
		{"extensions:texture loader", {"true", "false"}},
		{"extensions:si loader", {"true", "false"}},
		{"extensions:third person camera", {"true", "false"}},
		{"extensions:multiplayer", {"true", "false"}},
		{"isle:wide view angle", {"true", "false"}},
		{"texture loader:texture path", {"/textures", "/LEGO/mytextures", "/a"}},
		{"si loader:si path", {"/si", "/LEGO/mymods", "/a"}},
		{"multiplayer:relay url", {"ws://host", "wss://host.example:8080/path", "ws://1"}},
		{"multiplayer:room", {"lobby", "room-1", "A"}},
		{"multiplayer:actor", {"pepper", "Pepper", "brickstr"}},
		{"isle:lighting model", {"0", "1"}},
	};
	for (const auto& [key, valid] : validExtensions) {
		assert(Android_ValidateSetting(key, nullptr, {}));
		for (const char* value : valid) {
			assert(Android_ValidateSetting(key, value, {}));
		}
		assert(!Android_ValidateSetting(key, "", {}));
	}
	const std::vector<std::pair<const char*, std::vector<const char*>>> invalidExtensions = {
		{"extensions:texture loader", {"1", "yes", "True", "on"}},
		{"extensions:multiplayer", {"0", "no", "FALSE"}},
		{"isle:wide view angle", {"1", "yes"}},
		// A path is relative to the game data root, so it carries its own leading slash; "..", a
		// backslash, a comma and whitespace would each break the path or the list it sits in.
		{"texture loader:texture path",
		 {"textures", "/", "/LEGO/../etc", "/LEGO\\textures", "/my textures", "/a,b", "/a\tb"}},
		{"si loader:si path", {"mymods", "/", "/LEGO/../mods", "/LEGO\\mods", "/my mods", "/a,b"}},
		{"multiplayer:relay url", {"http://host", "https://host", "host", "ws://", "wss://", "ws:// host", "ws://ho st"}
		},
		{"multiplayer:room", {"my room", "room;1", "room#1", "room=1", "room,1", "[room]", "rööm"}},
		{"multiplayer:actor", {"pep per", "pepper;", "pepper#"}},
		{"isle:lighting model", {"2", "-1", "00", "01", "0.0", "1.5", "0x1", " 1"}},
	};
	for (const auto& [key, invalid] : invalidExtensions) {
		for (const char* value : invalid) {
			assert(!Android_ValidateSetting(key, value, {}));
		}
	}
	// Bounds: 255 characters for one path.
	assert(Android_ValidateSetting("texture loader:texture path", ("/" + std::string(254, 'a')).c_str(), {}));
	assert(!Android_ValidateSetting("texture loader:texture path", ("/" + std::string(255, 'a')).c_str(), {}));
	assert(Android_ValidateSetting("si loader:si path", ("/" + std::string(254, 'a')).c_str(), {}));
	assert(!Android_ValidateSetting("si loader:si path", ("/" + std::string(255, 'a')).c_str(), {}));
	// The file list and the directives are both edited elsewhere, so Settings must never accept and
	// rewrite either: the list would not fit one line of the file, and the directives have no values
	// to offer.
	assert(!Android_ValidateSetting("si loader:files", "/LEGO/Scripts/MOD.SI", {}));
	assert(!Android_ValidateSetting("si loader:files", nullptr, {}));
	assert(!Android_ValidateSetting("si loader:directives", "StartWith:isle.si:1:isle.si:2", {}));
	assert(!Android_ValidateSetting("si loader:directives", nullptr, {}));
	assert(!Android_ValidateSetting("extensions:unknown", "true", {}));
	assert(!Android_ValidateSetting("multiplayer:unknown", "x", {}));

	Android_TouchSettings touch;
	Android_BeginTouchSettings(path);
	assert(!Android_TakeTouchSettings(touch));
	assert(Android_UpdateConfig(path, {{"isle:touch scheme", "2"}}).empty());
	assert(Android_TakeTouchSettings(touch) && touch.m_scheme == 2 && touch.m_visible);
	assert(!Android_TakeTouchSettings(touch));
	// A later partial update keeps the other saved value, even before Resume.
	assert(Android_UpdateConfig(path, {{"isle:touch scheme", "1"}}).empty());
	assert(Android_UpdateConfig(path, {{"isle:show touch controls", "false"}}).empty());
	assert(Android_UpdateConfig(path, {{"isle:music", "false"}}).empty());
	assert(Android_TakeTouchSettings(touch) && touch.m_scheme == 1 && !touch.m_visible);
	assert(Android_UpdateConfig(path, {}).empty());
	assert(!Android_TakeTouchSettings(touch));
	// Layout values are presentation only, so saving them publishes no live touch update.
	assert(Android_UpdateConfig(
			   path,
			   {{"isle:touch menu position", "0.2500,0.7500"},
				{"isle:touch button scale", "1.5"},
				{"isle:touch control opacity", "0.5"}}
	).empty());
	assert(!Android_TakeTouchSettings(touch));
	const std::vector<std::string> layoutKeys =
		{"isle:touch menu position", "isle:touch button scale", "isle:touch control opacity"};
	values.clear();
	assert(Android_ReadConfig(path, layoutKeys, values).empty());
	assert(values[0].second == "0.2500,0.7500" && values[1].second == "1.5" && values[2].second == "0.5");
	assert(Android_UpdateConfig(
			   path,
			   {{"isle:touch menu position", nullptr},
				{"isle:touch button scale", nullptr},
				{"isle:touch control opacity", nullptr}}
	).empty());
	values.clear();
	assert(Android_ReadConfig(path, layoutKeys, values).empty());
	assert(!values[0].first && !values[1].first && !values[2].first);
	assert(!Android_TakeTouchSettings(touch));
	// Graphics settings apply on the next launch, so saving them publishes no live touch or controller
	// update, and the music setting saved earlier is left as it was. Game default removes them again.
	const std::vector<std::string> graphicsKeys =
		{"isle:island quality", "isle:max lod", "isle:frame delta", "isle:music"};
	assert(Android_UpdateConfig(
			   path,
			   {{"isle:island quality", "1"}, {"isle:max lod", "1.5"}, {"isle:frame delta", "31"}}
	).empty());
	assert(!Android_TakeTouchSettings(touch));
	GamepadBindings::Table unchanged;
	assert(!Android_TakeGamepadSettings(unchanged));
	values.clear();
	assert(Android_ReadConfig(path, graphicsKeys, values).empty());
	assert(values[0].second == "1" && values[1].second == "1.5" && values[2].second == "31");
	assert(values[3].second == "false");
	assert(Android_UpdateConfig(
			   path,
			   {{"isle:island quality", nullptr}, {"isle:max lod", nullptr}, {"isle:frame delta", nullptr}}
	).empty());
	values.clear();
	assert(Android_ReadConfig(path, graphicsKeys, values).empty());
	assert(!values[0].first && !values[1].first && !values[2].first && values[3].second == "false");

	assert(Android_UpdateConfig(path, {{"isle:touch scheme", "-1"}}).empty());
	before = Read(path);
	std::filesystem::create_directory(path + ".new");
	assert(!Android_UpdateConfig(path, {{"isle:touch scheme", "2"}}).empty());
	assert(Read(path) == before);
	assert(Android_TakeTouchSettings(touch) && touch.m_scheme == -1 && !touch.m_visible);
	assert(!Android_UpdateConfig(path, {{"isle:touch scheme", "2"}}).empty());
	assert(!Android_TakeTouchSettings(touch));
	std::filesystem::remove(path + ".new");
	assert(Android_UpdateConfig(path, {{"isle:touch scheme", "2"}}).empty());
	assert(Android_TakeTouchSettings(touch) && touch.m_scheme == 2 && !touch.m_visible);
	assert(Android_UpdateConfig(path, {{"isle:touch scheme", nullptr}, {"isle:show touch controls", nullptr}}).empty());
	assert(Android_TakeTouchSettings(touch) && touch.m_scheme == 0 && touch.m_visible);
	assert(!Android_UpdateConfig(path, {{"isle:touch scheme", "3"}}).empty());
	assert(!Android_TakeTouchSettings(touch));

	std::string other = std::string(directory) + "/other.ini";
	std::ofstream(other) << "[isle]\nmusic=true\n";
	assert(Android_UpdateConfig(other, {{"isle:touch scheme", "2"}}).empty());
	assert(!Android_TakeTouchSettings(touch));
	assert(Android_UpdateConfig(path, {{"isle:touch scheme", "2"}}).empty());
	Android_EndTouchSettings();
	assert(!Android_TakeTouchSettings(touch));
	assert(Android_UpdateConfig(path, {{"isle:touch scheme", "1"}}).empty());
	assert(!Android_TakeTouchSettings(touch));
	Android_BeginTouchSettings(path);
	assert(!Android_TakeTouchSettings(touch));
	assert(Android_UpdateConfig(path, {{"isle:touch scheme", "2"}}).empty());
	Android_BeginTouchSettings(path);
	assert(!Android_TakeTouchSettings(touch));
	Android_EndTouchSettings();

	// Extension options live in a section named after the extension, which a config may not have yet,
	// and turning one on must not disturb the rest of the file.
	std::ofstream(path) << "[isle]\nmusic=true\n[si loader]\nfiles=/LEGO/Scripts/MOD.SI\n"
						   "directives=StartWith:isle.si:1:isle.si:2\n";
	assert(Android_UpdateConfig(
			   path,
			   {{"extensions:texture loader", "true"},
				{"texture loader:texture path", "/mytextures"},
				{"extensions:si loader", "true"},
				{"si loader:si path", "/mymods"},
				{"extensions:multiplayer", "true"},
				{"multiplayer:relay url", "wss://relay.example"},
				{"multiplayer:room", "lobby"}}
	).empty());
	assert(Read(path).find("[texture loader]") != std::string::npos);
	assert(Read(path).find("[multiplayer]") != std::string::npos);
	values.clear();
	// The folder this screen writes lands beside the list and the directives it does not, and
	// neither of those is disturbed.
	assert(Android_ReadConfig(
			   path,
			   {"isle:music",
				"si loader:directives",
				"si loader:files",
				"si loader:si path",
				"texture loader:texture path",
				"multiplayer:room"},
			   values
	)
			   .empty());
	assert(values[0].second == "true" && values[1].second == "StartWith:isle.si:1:isle.si:2");
	assert(values[2].second == "/LEGO/Scripts/MOD.SI" && values[3].second == "/mymods");
	assert(values[4].second == "/mytextures" && values[5].second == "lobby");
	// Later updates reuse the sections rather than adding more.
	assert(Android_UpdateConfig(path, {{"texture loader:texture path", "/other"}}).empty());
	assert(Read(path).find("[texture loader]") == Read(path).rfind("[texture loader]"));
	// Removing an option leaves the rest of the file alone.
	assert(Android_UpdateConfig(path, {{"multiplayer:room", nullptr}}).empty());
	values.clear();
	assert(Android_ReadConfig(path, {"multiplayer:room", "multiplayer:relay url"}, values).empty());
	assert(!values[0].first && values[1].second == "wss://relay.example");

	{
		using namespace GamepadBindings;

		// Controller bindings live in their own section, which a config may not have yet.
		for (const char* start : {"[isle]\nmusic=true\n", "[extensions]\nmultiplayer=true\n"}) {
			std::ofstream(path) << start;
			assert(Android_UpdateConfig(path, {{"gamepad:start", "pause"}, {"gamepad:confirm", "east"}}).empty());
			assert(Read(path).find("[gamepad]") != std::string::npos);
			values.clear();
			assert(Android_ReadConfig(path, {"gamepad:start", "gamepad:confirm"}, values).empty());
			assert(values[0].second == "pause" && values[1].second == "east");
		}
		// Later updates reuse the section rather than adding another.
		assert(Android_UpdateConfig(path, {{"gamepad:north", "space"}}).empty());
		assert(Android_UpdateConfig(path, {{"gamepad:west", "escape"}}).empty());
		assert(Read(path).find("[gamepad]") == Read(path).rfind("[gamepad]"));
		std::ofstream(path) << "[isle]\nmusic=true\n";
		assert(Android_UpdateConfig(path, {{"gamepad:south", nullptr}}).empty());
		assert(Read(path).find("[gamepad]") == std::string::npos);

		for (const char* key : {"gamepad:south", "gamepad:guide", "gamepad:lefttrigger"}) {
			assert(Android_ValidateSetting(key, nullptr, {}));
			for (const char* valid : {"click", "space", "escape", "pause", "menu", "none", "Click"}) {
				assert(Android_ValidateSetting(key, valid, {}));
			}
			for (const char* invalid : {"", "jump", "label", "click "}) {
				assert(!Android_ValidateSetting(key, invalid, {}));
			}
		}
		for (const char* valid : {"label", "south", "east", "East"}) {
			assert(Android_ValidateSetting("gamepad:confirm", valid, {}));
		}
		assert(Android_ValidateSetting("gamepad:confirm", nullptr, {}));
		assert(!Android_ValidateSetting("gamepad:confirm", "click", {}));
		assert(!Android_ValidateSetting("gamepad:dpup", "click", {}));
		assert(!Android_ValidateSetting("isle:south", "click", {}));

		Table gamepad;
		Android_BeginTouchSettings(path);
		assert(!Android_TakeGamepadSettings(gamepad));
		assert(Android_UpdateConfig(path, {{"isle:music", "false"}}).empty());
		assert(!Android_TakeGamepadSettings(gamepad));
		assert(Android_UpdateConfig(path, {{"gamepad:north", "menu"}, {"gamepad:confirm", "south"}}).empty());
		assert(!Android_TakeTouchSettings(touch));
		assert(Android_TakeGamepadSettings(gamepad));
		assert(gamepad.m_actions[e_north] == e_menu && gamepad.m_confirm == e_confirmSouth);
		assert(gamepad.m_actions[e_south] == e_unset);
		assert(!Android_TakeGamepadSettings(gamepad));
		// A later partial update keeps the other saved bindings.
		assert(Android_UpdateConfig(path, {{"gamepad:south", "space"}}).empty());
		assert(Android_TakeGamepadSettings(gamepad));
		assert(gamepad.m_actions[e_north] == e_menu && gamepad.m_actions[e_south] == e_space);
		// Removed keys return to the game's defaults.
		assert(Android_UpdateConfig(
				   path,
				   {{"gamepad:north", nullptr}, {"gamepad:south", nullptr}, {"gamepad:confirm", nullptr}}
		).empty());
		assert(Android_TakeGamepadSettings(gamepad));
		assert(gamepad.m_actions[e_north] == e_unset && gamepad.m_confirm == e_confirmLabel);
		// A hand-edited value the game cannot use keeps its default in the published table.
		std::ofstream(path) << "[isle]\nmusic=true\n[gamepad]\nguide=jump\n";
		assert(Android_UpdateConfig(path, {{"gamepad:west", "pause"}}).empty());
		assert(Android_TakeGamepadSettings(gamepad));
		assert(gamepad.m_actions[e_guide] == e_unset && gamepad.m_actions[e_west] == e_pause);
		// Touch and controller updates are published separately.
		assert(Android_UpdateConfig(path, {{"isle:touch scheme", "1"}, {"gamepad:back", "none"}}).empty());
		assert(Android_TakeTouchSettings(touch) && touch.m_scheme == 1);
		assert(Android_TakeGamepadSettings(gamepad) && gamepad.m_actions[e_back] == e_none);
		// A failed write publishes nothing.
		std::filesystem::create_directory(path + ".new");
		assert(!Android_UpdateConfig(path, {{"gamepad:back", "escape"}}).empty());
		assert(!Android_TakeGamepadSettings(gamepad));
		std::filesystem::remove(path + ".new");
		// Only the running game's own config publishes, and only while it runs.
		assert(Android_UpdateConfig(other, {{"gamepad:back", "escape"}}).empty());
		assert(!Android_TakeGamepadSettings(gamepad));
		assert(Android_UpdateConfig(path, {{"gamepad:back", "escape"}}).empty());
		Android_BeginTouchSettings(path);
		assert(!Android_TakeGamepadSettings(gamepad));
		Android_EndTouchSettings();
		assert(Android_UpdateConfig(path, {{"gamepad:back", "pause"}}).empty());
		assert(!Android_TakeGamepadSettings(gamepad));
	}
	std::filesystem::remove_all(directory);
}
