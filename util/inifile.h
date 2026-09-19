#ifndef INIFILE_H
#define INIFILE_H

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iniparser.h>
#include <string>

#ifdef _WIN32
#include <SDL3/SDL_filesystem.h>
#include <io.h>
#else
#include <unistd.h>
#endif

// Writing a configuration file that the next load will refuse is the same as losing it, so both
// halves of that risk live here: the length a value may reach, and a replacement that cannot leave
// a half-written file behind. Every settings surface writes through this.
namespace IniFile
{

// A value has to survive the file it is written to. iniparser dumps a key as
// printf("%-30s = \"%s\"\n", ...), padding the name out to 30 characters and doubling every
// backslash and quote in the value, and reads a line back through fgets into a 1024 byte buffer. An
// over-long line is not skipped: the load fails outright and returns no dictionary, and a
// configuration that will not load is replaced with defaults. One value too long therefore costs
// every other setting in the file, so refuse it while the file is still intact.
//
// This checks one value, not the whole dictionary, so a caller can name the setting it is refusing
// and can refuse before it has mutated anything. A value already on disk that grows past the limit
// when escaped is not caught here.
inline constexpr size_t kLineLimit = 1022;
inline constexpr size_t kNamePadding = 30;

inline bool FitsOnOneLine(const std::string& p_key, const char* p_value)
{
	size_t colon = p_key.find(':');
	size_t name = colon == std::string::npos ? p_key.size() : p_key.size() - colon - 1;
	size_t length = (name > kNamePadding ? name : kNamePadding) + sizeof(" = \"\"") - 1;
	for (const char* c = p_value; *c; ++c) {
		length += (*c == '\\' || *c == '"') ? 2 : 1;
	}
	return length <= kLineLimit;
}

// Push the bytes past the standard library's buffers, so a crash after Save returns cannot lose a
// file the caller has been told was written.
inline int Commit(FILE* p_file)
{
#ifdef _WIN32
	return _commit(_fileno(p_file));
#else
	return fsync(fileno(p_file));
#endif
}

// Replace p_path with p_temp, which must not be readable as two files at any point. POSIX rename
// replaces the destination; the Windows CRT's does not, so that branch goes through SDL, which is
// MoveFileExW with MOVEFILE_REPLACE_EXISTING and converts the path to wide characters on the way.
// The real <windows.h> is not an option here: CONFIG/qt builds against miniwin's shim of that name.
inline std::string RenameOver(const std::string& p_temp, const std::string& p_path)
{
#ifdef _WIN32
	if (!SDL_RenamePath(p_temp.c_str(), p_path.c_str())) {
		const char* error = SDL_GetError();
		return error && *error ? error : "the file could not be replaced";
	}
#else
	if (rename(p_temp.c_str(), p_path.c_str()) != 0) {
		return strerror(errno);
	}
#endif
	return {};
}

// Dump p_dictionary over p_path by writing a sibling and renaming it into place, so an interrupted
// or failed write leaves the previous file exactly as it was. Returns an empty string on success,
// otherwise a message naming what failed. Whatever is not in p_dictionary is not in the result:
// preserving what the caller does not own means loading the file first, not calling this.
inline std::string Save(const std::string& p_path, const dictionary* p_dictionary)
{
	std::string temp = p_path + ".new";
	FILE* file = fopen(temp.c_str(), "wb");
	if (!file) {
		return std::string("Could not write configuration: ") + strerror(errno);
	}
	iniparser_dump_ini(p_dictionary, file);
	int error = ferror(file) ? EIO : 0;
	if (fflush(file) != 0 && !error) {
		error = errno;
	}
	if (!error && Commit(file) != 0) {
		error = errno;
	}
	if (fclose(file) != 0 && !error) {
		error = errno;
	}
	std::string failure = error ? strerror(error) : RenameOver(temp, p_path);
	if (!failure.empty()) {
		remove(temp.c_str());
		return "Could not save configuration: " + failure;
	}
	return {};
}

} // namespace IniFile

#endif // INIFILE_H
