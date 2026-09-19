#include "inifile.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

// std::filesystem rather than mkdtemp, so this same binary runs on msys2 and MSVC and exercises
// the Windows branch of the replacement, which is the only place SDL_RenamePath is reached.
static std::filesystem::path MakeDirectory(const char* p_name)
{
	std::filesystem::path directory = std::filesystem::temp_directory_path() / "isle-inifile-test" / p_name;
	std::filesystem::remove_all(directory);
	std::filesystem::create_directories(directory);
	return directory;
}

static std::string Read(const std::filesystem::path& p_path)
{
	std::ifstream file(p_path, std::ios::binary);
	return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

using Dictionary = std::unique_ptr<dictionary, decltype(&iniparser_freedict)>;

static Dictionary MakeDictionary(const char* p_key, const std::string& p_value)
{
	Dictionary dict(dictionary_new(0), iniparser_freedict);
	assert(dict);
	assert(iniparser_set(dict.get(), "isle", nullptr) == 0);
	assert(iniparser_set(dict.get(), p_key, p_value.c_str()) == 0);
	return dict;
}

int main()
{
	// The arithmetic: a name shorter than the padding still occupies it, and " = \"\"" is five
	// more characters than the value itself.
	const size_t shortKeyRoom = IniFile::kLineLimit - IniFile::kNamePadding - 5;
	assert(IniFile::FitsOnOneLine("isle:x", std::string(shortKeyRoom, 'a').c_str()));
	assert(!IniFile::FitsOnOneLine("isle:x", std::string(shortKeyRoom + 1, 'a').c_str()));

	// iniparser escapes backslashes and quotes on the way out, so each one costs two characters.
	assert(IniFile::FitsOnOneLine("isle:x", std::string(shortKeyRoom / 2, '\\').c_str()));
	assert(!IniFile::FitsOnOneLine("isle:x", std::string(shortKeyRoom / 2 + 1, '\\').c_str()));
	assert(IniFile::FitsOnOneLine("isle:x", std::string(shortKeyRoom / 2, '"').c_str()));
	assert(!IniFile::FitsOnOneLine("isle:x", std::string(shortKeyRoom / 2 + 1, '"').c_str()));

	// A name longer than the padding is measured instead of it. Only the part after the colon is
	// the name; the section is not printed on the key's line.
	const std::string longName(IniFile::kNamePadding + 10, 'k');
	const size_t longKeyRoom = IniFile::kLineLimit - longName.size() - 5;
	assert(IniFile::FitsOnOneLine("isle:" + longName, std::string(longKeyRoom, 'a').c_str()));
	assert(!IniFile::FitsOnOneLine("isle:" + longName, std::string(longKeyRoom + 1, 'a').c_str()));

	// A key with no colon is a section entry: the whole of it is the name.
	assert(IniFile::FitsOnOneLine(longName, std::string(longKeyRoom, 'a').c_str()));
	assert(!IniFile::FitsOnOneLine(longName, std::string(longKeyRoom + 1, 'a').c_str()));

	assert(IniFile::FitsOnOneLine("isle:x", ""));

	// The limit is iniparser's own cliff, not a guess. A value at the limit survives a dump and a
	// load; one character more and the load refuses the whole file, taking every other key with
	// it. Built from backslashes because iniparser_set truncates a stored value at ASCIILINESZ,
	// which would hide the case if the length came from plain characters.
	{
		std::filesystem::path directory = MakeDirectory("limit");
		const std::string atLimit(shortKeyRoom / 2, '\\');
		assert(IniFile::FitsOnOneLine("isle:x", atLimit.c_str()));
		std::string path = (directory / "isle.ini").string();
		assert(IniFile::Save(path, MakeDictionary("isle:x", atLimit).get()).empty());
		Dictionary loaded(iniparser_load(path.c_str()), iniparser_freedict);
		assert(loaded);
		assert(iniparser_getstring(loaded.get(), "isle:x", nullptr) == atLimit);

		const std::string overLimit(shortKeyRoom / 2 + 1, '\\');
		assert(!IniFile::FitsOnOneLine("isle:x", overLimit.c_str()));
		std::string overPath = (directory / "over.ini").string();
		assert(IniFile::Save(overPath, MakeDictionary("isle:x", overLimit).get()).empty());
		Dictionary refused(iniparser_load(overPath.c_str()), iniparser_freedict);
		assert(!refused);
	}

	// A fresh file, and no temporary left beside it.
	{
		std::filesystem::path directory = MakeDirectory("fresh");
		std::string path = (directory / "isle.ini").string();
		assert(IniFile::Save(path, MakeDictionary("isle:music", "true").get()).empty());
		Dictionary loaded(iniparser_load(path.c_str()), iniparser_freedict);
		assert(loaded);
		assert(iniparser_getstring(loaded.get(), "isle:music", nullptr) == std::string("true"));
		assert(!std::filesystem::exists(path + ".new"));
	}

	// Replacing an existing file. The Windows CRT's rename fails on an existing destination, so
	// this is what pins the replacing rename there.
	{
		std::filesystem::path directory = MakeDirectory("replace");
		std::string path = (directory / "isle.ini").string();
		std::ofstream(path) << "[isle]\nmusic = \"false\"\n";
		assert(IniFile::Save(path, MakeDictionary("isle:music", "true").get()).empty());
		Dictionary loaded(iniparser_load(path.c_str()), iniparser_freedict);
		assert(loaded);
		assert(iniparser_getstring(loaded.get(), "isle:music", nullptr) == std::string("true"));
		// Only what the caller passed is written: preserving the rest is the caller's job, done by
		// loading the file before it changes anything.
		assert(Read(path).find("custom") == std::string::npos);
	}

	// A temporary that cannot be written leaves the previous file exactly as it was.
	{
		std::filesystem::path directory = MakeDirectory("blocked-temp");
		std::string path = (directory / "isle.ini").string();
		const std::string original = "[isle]\nmusic = \"false\"\n";
		std::ofstream(path) << original;
		std::filesystem::create_directory(path + ".new");
		std::string error = IniFile::Save(path, MakeDictionary("isle:music", "true").get());
		assert(!error.empty());
		assert(error.compare(0, 31, "Could not write configuration: ") == 0);
		assert(Read(path) == original);
		std::filesystem::remove(path + ".new");
	}

	// A rename that cannot land removes the temporary rather than leaving it to be mistaken for
	// the configuration.
	{
		std::filesystem::path directory = MakeDirectory("blocked-rename");
		std::string path = (directory / "isle.ini").string();
		std::filesystem::create_directory(path);
		std::string error = IniFile::Save(path, MakeDictionary("isle:music", "true").get());
		assert(!error.empty());
		assert(error.compare(0, 30, "Could not save configuration: ") == 0);
		assert(!std::filesystem::exists(path + ".new"));
		assert(std::filesystem::is_directory(path));
	}

	// Nowhere to write at all.
	{
		std::filesystem::path directory = MakeDirectory("missing-parent");
		std::string path = (directory / "absent" / "isle.ini").string();
		assert(!IniFile::Save(path, MakeDictionary("isle:music", "true").get()).empty());
		assert(!std::filesystem::exists(path));
		assert(!std::filesystem::exists(path + ".new"));
	}

	std::filesystem::remove_all(std::filesystem::temp_directory_path() / "isle-inifile-test");
	return 0;
}
