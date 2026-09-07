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

	assert(Android_ValidateSetting("isle:music", nullptr, {}));
	assert(Android_ValidateSetting("isle:music", "false", {}));
	assert(!Android_ValidateSetting("isle:music", "garbage", {}));
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
	std::filesystem::remove_all(directory);
}
