#include "configstore.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iniparser.h>
#include <memory>
#include <mutex>
#include <unistd.h>

static std::mutex g_configMutex;
using ConfigDictionary = std::unique_ptr<dictionary, decltype(&iniparser_freedict)>;

std::string Android_ReadConfig(
	const std::string& p_path,
	const std::vector<std::string>& p_keys,
	std::vector<std::pair<bool, std::string>>& p_values
)
{
	std::lock_guard<std::mutex> lock(g_configMutex);
	ConfigDictionary dict(iniparser_load(p_path.c_str()), iniparser_freedict);
	if (!dict || !iniparser_find_entry(dict.get(), "isle")) {
		return "Could not read isle.ini. The existing configuration has not been changed.";
	}
	for (const auto& key : p_keys) {
		const char* value = iniparser_getstring(dict.get(), key.c_str(), nullptr);
		p_values.emplace_back(value != nullptr, value ? value : "");
	}
	return {};
}

std::string Android_UpdateConfig(
	const std::string& p_path,
	const std::vector<std::pair<std::string, const char*>>& p_changes
)
{
	std::lock_guard<std::mutex> lock(g_configMutex);
	ConfigDictionary dict(iniparser_load(p_path.c_str()), iniparser_freedict);
	if (!dict || !iniparser_find_entry(dict.get(), "isle")) {
		return "Could not read isle.ini. The existing configuration has not been changed.";
	}
	for (const auto& change : p_changes) {
		if (change.second) {
			if (iniparser_set(dict.get(), change.first.c_str(), change.second) != 0) {
				return "Not enough memory to update the configuration.";
			}
		}
		else {
			iniparser_unset(dict.get(), change.first.c_str());
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
	return {};
}

bool Android_ValidateSetting(const std::string& p_key, const char* p_value, const std::vector<std::string>& p_renderers)
{
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
	if (p_key == "isle:music" || p_key == "isle:3dsound" || p_key == "isle:haptic" || p_key == "isle:wasd") {
		return !p_value || std::string(p_value) == "true" || std::string(p_value) == "false";
	}
	bool sensitivity = p_key == "isle:cursor sensitivity";
	bool touch = p_key == "isle:touch scheme";
	bool width = p_key == "isle:horizontal resolution";
	bool height = p_key == "isle:vertical resolution";
	bool msaa = p_key == "isle:msaa";
	bool anisotropic = p_key == "isle:anisotropic";
	if (!(sensitivity || touch || width || height || msaa || anisotropic)) {
		return false;
	}
	if (!p_value) {
		return true;
	}
	char* end;
	double value = strtod(p_value, &end);
	if (end == p_value || *end || !std::isfinite(value)) {
		return false;
	}
	if (sensitivity) {
		return value >= 0.1 && value <= 20;
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
