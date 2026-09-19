#include "configstore.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iniparser.h>
#include <memory>
#include <mutex>
#include <optional>
#include <sys/stat.h>
#include <unistd.h>

static std::mutex g_configMutex;
static std::string g_touchSettingsPath;
static std::optional<Android_TouchSettings> g_pendingTouchSettings;
static std::optional<GamepadBindings::Table> g_pendingGamepadSettings;
using ConfigDictionary = std::unique_ptr<dictionary, decltype(&iniparser_freedict)>;

static bool IsGamepadKey(const std::string& p_key)
{
	return p_key.compare(0, 8, "gamepad:") == 0;
}

void Android_BeginTouchSettings(const std::string& p_path)
{
	std::lock_guard<std::mutex> lock(g_configMutex);
	g_touchSettingsPath = p_path;
	g_pendingTouchSettings.reset();
	g_pendingGamepadSettings.reset();
}

void Android_EndTouchSettings()
{
	Android_BeginTouchSettings({});
}

bool Android_TakeTouchSettings(Android_TouchSettings& p_settings)
{
	std::lock_guard<std::mutex> lock(g_configMutex);
	if (!g_pendingTouchSettings) {
		return false;
	}
	p_settings = *g_pendingTouchSettings;
	g_pendingTouchSettings.reset();
	return true;
}

bool Android_TakeGamepadSettings(GamepadBindings::Table& p_table)
{
	std::lock_guard<std::mutex> lock(g_configMutex);
	if (!g_pendingGamepadSettings) {
		return false;
	}
	p_table = *g_pendingGamepadSettings;
	g_pendingGamepadSettings.reset();
	return true;
}

std::string Android_ResolveSaveExportPath(
	const std::string& p_config,
	const std::string& p_default,
	std::string& p_path
)
{
	std::lock_guard<std::mutex> lock(g_configMutex);
	p_path.clear();
	struct stat info;
	if (stat(p_config.c_str(), &info) != 0) {
		if (errno != ENOENT) {
			return "Could not read the configuration to locate saves.";
		}
		p_path = p_default;
	}
	else {
		ConfigDictionary dict(iniparser_load(p_config.c_str()), iniparser_freedict);
		if (!dict) {
			return "Could not read the configuration to locate saves.";
		}
		p_path = iniparser_getstring(dict.get(), "isle:savepath", p_default.c_str());
	}
	return p_path.empty() ? "The save directory must not be empty." : "";
}

std::string Android_ReadConfig(
	const std::string& p_path,
	const std::vector<std::string>& p_keys,
	std::vector<std::pair<bool, std::string>>& p_values
)
{
	std::lock_guard<std::mutex> lock(g_configMutex);
	ConfigDictionary dict(iniparser_load(p_path.c_str()), iniparser_freedict);
	if (!dict || dict->n == 0) {
		return "Could not read isle.ini. The existing configuration has not been changed.";
	}
	for (const auto& key : p_keys) {
		const char* value = iniparser_getstring(dict.get(), key.c_str(), nullptr);
		p_values.emplace_back(value != nullptr, value ? value : "");
	}
	return {};
}

// A value has to survive the file it is written to. iniparser dumps a key as
// printf("%-30s = \"%s\"\n", ...), padding the name out to 30 characters and doubling every
// backslash and quote in the value, and reads a line back through fgets into a 1024 byte buffer. An
// over-long line is not skipped: the load fails outright and returns no dictionary, and a
// configuration that will not load is replaced with defaults. One value too long therefore costs
// every other setting in the file, so refuse it while the file is still intact.
static constexpr size_t kConfigLineLimit = 1022;
static constexpr size_t kConfigNamePadding = 30;

static bool FitsOnOneLine(const std::string& p_key, const char* p_value)
{
	size_t colon = p_key.find(':');
	size_t name = colon == std::string::npos ? p_key.size() : p_key.size() - colon - 1;
	size_t length = (name > kConfigNamePadding ? name : kConfigNamePadding) + sizeof(" = \"\"") - 1;
	for (const char* c = p_value; *c; ++c) {
		length += (*c == '\\' || *c == '"') ? 2 : 1;
	}
	return length <= kConfigLineLimit;
}

std::string Android_UpdateConfig(
	const std::string& p_path,
	const std::vector<std::pair<std::string, const char*>>& p_changes
)
{
	std::lock_guard<std::mutex> lock(g_configMutex);
	ConfigDictionary dict(iniparser_load(p_path.c_str()), iniparser_freedict);
	if (!dict || dict->n == 0) {
		return "Could not read isle.ini. The existing configuration has not been changed.";
	}
	// Checked before anything is applied, and for every caller: the game data root is written
	// straight through here without passing Android_ValidateSetting's whitelist.
	for (const auto& change : p_changes) {
		if (change.second && !FitsOnOneLine(change.first, change.second)) {
			return "Could not store \"" + change.first + "\": too long for the configuration file.";
		}
	}
	for (const auto& change : p_changes) {
		if (change.second) {
			// iniparser writes a key only under an existing section entry.
			std::string section = change.first.substr(0, change.first.find(':'));
			if ((!iniparser_find_entry(dict.get(), section.c_str()) &&
				 iniparser_set(dict.get(), section.c_str(), nullptr) != 0) ||
				iniparser_set(dict.get(), change.first.c_str(), change.second) != 0) {
				return "Not enough memory to update the configuration.";
			}
		}
		else {
			iniparser_unset(dict.get(), change.first.c_str());
		}
	}
	bool touchChanged = false, gamepadChanged = false;
	for (const auto& change : p_changes) {
		touchChanged |= change.first == "isle:touch scheme" || change.first == "isle:show touch controls";
		gamepadChanged |= IsGamepadKey(change.first);
	}
	GamepadBindings::Table gamepad;
	if (gamepadChanged) {
		gamepad = GamepadBindings::Parse(
			[&dict](const char* p_key) { return iniparser_getstring(dict.get(), p_key, nullptr); },
			[](const char*, const char*) {}
		);
	}
	Android_TouchSettings touch;
	if (touchChanged) {
		touch.m_scheme = iniparser_getint(dict.get(), "isle:touch scheme", touch.m_scheme);
		touch.m_visible = iniparser_getboolean(dict.get(), "isle:show touch controls", touch.m_visible);
		if (touch.m_scheme < -1 || touch.m_scheme > 2) {
			return "Invalid touch scheme. Choose a supported value and try again.";
		}
	}
	std::string temp = p_path + ".new";
	FILE* file = fopen(temp.c_str(), "wb");
	if (!file) {
		return std::string("Could not write configuration: ") + strerror(errno);
	}
	iniparser_dump_ini(dict.get(), file);
	int error = ferror(file) ? EIO : 0;
	if (fflush(file) != 0 && !error) {
		error = errno;
	}
	if (!error && fsync(fileno(file)) != 0) {
		error = errno;
	}
	if (fclose(file) != 0 && !error) {
		error = errno;
	}
	if (!error && rename(temp.c_str(), p_path.c_str()) != 0) {
		error = errno;
	}
	if (error) {
		remove(temp.c_str());
		return std::string("Could not save configuration: ") + strerror(error);
	}
	if (!g_touchSettingsPath.empty() && p_path == g_touchSettingsPath) {
		if (touchChanged) {
			g_pendingTouchSettings = touch;
		}
		if (gamepadChanged) {
			g_pendingGamepadSettings = gamepad;
		}
	}
	return {};
}

static bool IsFraction(double p_value)
{
	return std::isfinite(p_value) && p_value >= 0 && p_value <= 1;
}

// A touch control's center as "x,y", each a fraction of the area clear of system bars and cutouts.
static bool IsTouchPosition(const char* p_value)
{
	char* end;
	double x = strtod(p_value, &end);
	if (end == p_value || *end != ',') {
		return false;
	}
	const char* start = end + 1;
	double y = strtod(start, &end);
	return end != start && !*end && IsFraction(x) && IsFraction(y);
}

struct GraphicsRange {
	const char* m_key;
	double m_min;
	double m_max;
	bool m_whole;
};

// The graphics keys mirror GraphicsSettings.java; keep them in step. The level of detail and actor ranges
// follow the desktop configuration tool; quality and transition leave out what it marks broken.
static const GraphicsRange g_graphicsRanges[] = {
	// The desktop tool marks Low (0) broken.
	{"isle:island quality", 1, 2, true},
	{"isle:island texture", 0, 1, true},
	{"isle:max lod", 0, 6, false},
	{"isle:max allowed extras", 5, 40, true},
	// The desktop tool marks idle (0) and the last type (6) broken; 6 also locks the game up.
	{"isle:transition type", 1, 5, true},
	// The game truncates the frame delta to whole milliseconds, so below 1 would mean no limit; 1000 (1 fps)
	// is a sanity bound.
	{"isle:frame delta", 1, 1000, false},
};

static bool ParseNumber(const char* p_value, double& p_number)
{
	char* end;
	p_number = strtod(p_value, &end);
	return end != p_value && !*end && std::isfinite(p_number);
}

// The extension keys and rules below mirror ExtensionSettings.java; keep them in step.

// An extension path names a folder inside the game files. ResolveGamePath concatenates the game
// data root with the value and inserts no separator, so the value has to start with its own slash,
// and "/.." would reach outside the game files. The si loader splits its own file list on
// whitespace as well as commas, so a path holding either could never be read back whole.
static bool IsGamePath(const std::string& p_path)
{
	if (p_path.size() < 2 || p_path.size() > 255 || p_path[0] != '/') {
		return false;
	}
	if (p_path.find("..") != std::string::npos || p_path.find('\\') != std::string::npos) {
		return false;
	}
	for (unsigned char c : p_path) {
		if (c <= ' ' || c == 127 || c == ',') {
			return false;
		}
	}
	return true;
}

// The multiplayer transports speak WebSocket, so anything else would fail at connect time.
static bool IsRelayUrl(const std::string& p_value)
{
	size_t scheme = 0;
	if (p_value.compare(0, 5, "ws://") == 0) {
		scheme = 5;
	}
	else if (p_value.compare(0, 6, "wss://") == 0) {
		scheme = 6;
	}
	if (scheme == 0 || p_value.size() <= scheme || p_value.size() > 512) {
		return false;
	}
	for (unsigned char c : p_value) {
		if (c <= ' ' || c == 127) {
			return false;
		}
	}
	return true;
}

// iniparser writes "key = value" and reads a line back up to its comment character, so a value
// carrying one of these would not survive the round trip.
static bool IsIniWord(const std::string& p_value, size_t p_max)
{
	if (p_value.empty() || p_value.size() > p_max) {
		return false;
	}
	for (unsigned char c : p_value) {
		if (c <= ' ' || c >= 127 || strchr(",;#=[]", c)) {
			return false;
		}
	}
	return true;
}

// Plain decimal digits: the game reads whole-number keys with strtol's base detection, which would
// read "010" as 8 and stop "0.2e1" at the decimal point.
static bool IsPlainWholeNumber(const char* p_value)
{
	if (!*p_value || (p_value[0] == '0' && p_value[1])) {
		return false;
	}
	for (const char* c = p_value; *c; c++) {
		if (*c < '0' || *c > '9') {
			return false;
		}
	}
	return true;
}

// The touch button scale, opacity and position keys and ranges mirror TouchLayout.java; keep them in step.
bool Android_ValidateSetting(const std::string& p_key, const char* p_value, const std::vector<std::string>& p_renderers)
{
	// The controller keys and values are listed for Settings in ControllerBindings.java.
	if (IsGamepadKey(p_key)) {
		if (p_key == GamepadBindings::ConfirmKey()) {
			GamepadBindings::Confirm confirm;
			return !p_value || GamepadBindings::ParseConfirm(p_value, confirm);
		}
		for (int i = 0; i < GamepadBindings::e_inputCount; i++) {
			if (p_key == GamepadBindings::Key(static_cast<GamepadBindings::Input>(i))) {
				GamepadBindings::Action action;
				return !p_value || GamepadBindings::ParseAction(p_value, action);
			}
		}
		return false;
	}
	if (p_key == "isle:touch menu position" || p_key == "isle:touch escape position" ||
		p_key == "isle:touch space position") {
		return !p_value || IsTouchPosition(p_value);
	}
	if (p_key == "isle:3d device id") {
		if (!p_value) {
			return true;
		}
		for (size_t i = 1; i < p_renderers.size(); i += 2) {
			if (p_renderers[i] == p_value) {
				return true;
			}
		}
		return false;
	}
	// The extension enable keys are Extensions::availableExtensions; the game reads each as a boolean.
	if (p_key == "isle:music" || p_key == "isle:3dsound" || p_key == "isle:haptic" || p_key == "isle:wasd" ||
		p_key == "isle:show touch controls" || p_key == "isle:wide view angle" ||
		p_key == "extensions:texture loader" || p_key == "extensions:si loader" ||
		p_key == "extensions:third person camera" || p_key == "extensions:multiplayer") {
		return !p_value || std::string(p_value) == "true" || std::string(p_value) == "false";
	}
	if (p_key == "texture loader:texture path") {
		return !p_value || IsGamePath(p_value);
	}
	// The si loader's own file list is deliberately absent, as its directives are: both are edited
	// elsewhere, and this being a whitelist is what stops Settings from ever rewriting them.
	if (p_key == "si loader:si path") {
		return !p_value || IsGamePath(p_value);
	}
	if (p_key == "multiplayer:relay url") {
		return !p_value || IsRelayUrl(p_value);
	}
	if (p_key == "multiplayer:room") {
		return !p_value || IsIniWord(p_value, 64);
	}
	// An actor the game does not know resolves to no actor and is ignored, so the name only has to
	// survive the file; Settings offers the game's own list.
	if (p_key == "multiplayer:actor") {
		return !p_value || IsIniWord(p_value, 32);
	}
	// Kept out of g_graphicsRanges: that table is the Graphics group, and GraphicsSettingsTest reads
	// it back to prove the two sides agree. Matched exactly rather than parsed as a number, so the
	// base detection the game reads it with never gets the chance to make "010" mean something.
	if (p_key == "isle:lighting model") {
		return !p_value || std::string(p_value) == "0" || std::string(p_value) == "1";
	}
	for (const GraphicsRange& range : g_graphicsRanges) {
		if (p_key == range.m_key) {
			if (!p_value) {
				return true;
			}
			double value;
			return (!range.m_whole || IsPlainWholeNumber(p_value)) && ParseNumber(p_value, value) &&
				   value >= range.m_min && value <= range.m_max;
		}
	}
	bool sensitivity = p_key == "isle:cursor sensitivity";
	bool touch = p_key == "isle:touch scheme";
	bool width = p_key == "isle:horizontal resolution";
	bool height = p_key == "isle:vertical resolution";
	bool msaa = p_key == "isle:msaa";
	bool anisotropic = p_key == "isle:anisotropic";
	bool buttonScale = p_key == "isle:touch button scale";
	bool opacity = p_key == "isle:touch control opacity";
	if (!(sensitivity || touch || width || height || msaa || anisotropic || buttonScale || opacity)) {
		return false;
	}
	if (!p_value) {
		return true;
	}
	double value;
	if (!ParseNumber(p_value, value)) {
		return false;
	}
	if (sensitivity) {
		return value >= 0.1 && value <= 20;
	}
	if (buttonScale) {
		return value >= 0.5 && value <= 2;
	}
	if (opacity) {
		return value >= 0.1 && value <= 1;
	}
	if (touch) {
		return value == -1 || value == 0 || value == 1 || value == 2;
	}
	if (width) {
		return value == 640 || value == 800 || value == 1024 || value == 1280;
	}
	if (height) {
		return value == 480 || value == 600 || value == 768 || value == 960;
	}
	return value == 0 || value == 2 || value == 4 || value == 8 || value == 16 || (anisotropic && value == 1);
}
