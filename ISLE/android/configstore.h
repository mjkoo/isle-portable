#ifndef ANDROID_CONFIGSTORE_H
#define ANDROID_CONFIGSTORE_H

#include <string>
#include <utility>
#include <vector>

// A null value removes a key. All writers reload under the same lock before replacing the file.
std::string Android_UpdateConfig(
	const std::string& p_path,
	const std::vector<std::pair<std::string, const char*>>& p_changes
);
std::string Android_ReadConfig(
	const std::string& p_path,
	const std::vector<std::string>& p_keys,
	std::vector<std::pair<bool, std::string>>& p_values
);

bool Android_ValidateSetting(
	const std::string& p_key,
	const char* p_value,
	const std::vector<std::string>& p_renderers
);

#endif
