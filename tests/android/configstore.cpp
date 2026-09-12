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
	std::filesystem::remove_all(directory);
}
