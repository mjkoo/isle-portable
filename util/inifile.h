#ifndef INIFILE_H
#define INIFILE_H

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <iniparser.h>
#include <string>

#ifdef _WIN32
#include <filesystem>
#include <io.h>
#include <system_error>
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

// A dictionary that was loaded before it was edited carries values this writer never produced,
// and the dumper's padding and escaping can push one of them past the limit even though it read
// back fine. Dumping it anyway would write a file the next load refuses, so a caller that merges
// asks this first. Returns the first entry that would not survive, or an empty string.
inline std::string FindOverlongEntry(const dictionary* p_dictionary)
{
	if (!p_dictionary) {
		return {};
	}
	for (size_t i = 0; i < p_dictionary->size; ++i) {
		if (p_dictionary->key[i] && p_dictionary->val[i] &&
			!FitsOnOneLine(p_dictionary->key[i], p_dictionary->val[i])) {
			return p_dictionary->key[i];
		}
	}
	return {};
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

// Replace p_path with p_temp. POSIX rename replaces the destination; the Windows CRT's rename
// fails when it already exists, so that branch goes through std::filesystem::rename, which the
// standard requires to replace an existing file and which MSVC implements as MoveFileExW with
// MOVEFILE_REPLACE_EXISTING. It also takes the path in the same narrow encoding fopen and remove
// do, which matters: an SDL or wide-character rename would read these bytes as UTF-8 while the
// open and the cleanup read them as the ANSI code page, and the three calls would name different
// files as soon as the path left ASCII. The real <windows.h> is not an option for MoveFileEx here,
// because CONFIG/qt builds against miniwin's shim of that name.
inline std::string RenameOver(const std::string& p_temp, const std::string& p_path)
{
#ifdef _WIN32
	std::error_code code;
	std::filesystem::rename(p_temp, p_path, code);
	if (code) {
		return code.message();
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

#endif
