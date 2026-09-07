#ifndef ANDROID_SAVESNAPSHOT_H
#define ANDROID_SAVESNAPSHOT_H

#include <cstdint>
#include <string>
#include <vector>

struct Android_SaveFile {
	std::string m_name;
	std::vector<uint8_t> m_bytes;
};

struct Android_SaveSnapshot {
	std::vector<Android_SaveFile> m_files;
	std::string m_error;
	bool m_incomplete = false;
};

// Call only while the game's save writers are quiescent. Never pumps events or writes files.
Android_SaveSnapshot Android_ReadSaveSnapshot(const std::string& p_directory);

#endif
