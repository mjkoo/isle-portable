#ifndef ANDROID_CONFIGSTORE_H
#define ANDROID_CONFIGSTORE_H

#include "../gamepadbindings.h"

#include <string>
#include <utility>
#include <vector>

struct Android_TouchSettings {
	int m_scheme = 0;
	bool m_visible = true;
};

// Live touch and controller updates belong to one engine session and are consumed only on its SDL
// thread. Beginning or ending a session discards both.
void Android_BeginTouchSettings(const std::string& p_path);
void Android_EndTouchSettings();
bool Android_TakeTouchSettings(Android_TouchSettings& p_settings);
bool Android_TakeGamepadSettings(GamepadBindings::Table& p_table);

// Read-only resolution for recovery export, including a missing configuration.
std::string Android_ResolveSaveExportPath(
	const std::string& p_config,
	const std::string& p_default,
	std::string& p_path
);

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
