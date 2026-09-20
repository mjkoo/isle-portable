#include "inifile.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <random>
#include <string>

// std::filesystem rather than mkdtemp, so this same binary runs on msys2 and MSVC and exercises
// the Windows branch of the replacement. The root carries a per-run number so two runs cannot
// delete each other's trees; a failing run leaves its tree behind to be looked at, since assert
// aborts without unwinding.
static const std::filesystem::path& Root()
{
	static const std::filesystem::path root =
		std::filesystem::temp_directory_path() / ("isle-inifile-test-" + std::to_string(std::random_device{}()));
	return root;
}

static std::filesystem::path MakeDirectory(const char* p_name)
{
	std::filesystem::path directory = Root() / p_name;
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

// Counts case-insensitively, so a key that came back in its original case rather than lowercased
// is still counted rather than missed.
static size_t Occurrences(const std::string& p_text, const std::string& p_needle)
{
	std::string text = p_text;
	std::transform(text.begin(), text.end(), text.begin(), [](unsigned char p_char) {
		return static_cast<char>(std::tolower(p_char));
	});
	size_t count = 0;
	for (size_t at = text.find(p_needle); at != std::string::npos; at = text.find(p_needle, at + 1)) {
		++count;
	}
	return count;
}

// iniparser_getstring hands back the default, here nullptr, for a key it does not hold. Comparing
// that against a std::string is undefined, and a missing key is exactly what these assertions are
// watching for, so check the pointer before it is read.
static std::string Value(const dictionary* p_dict, const char* p_key)
{
	const char* value = iniparser_getstring(p_dict, p_key, nullptr);
	assert(value);
	return value;
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

	// A key with no colon is measured whole. iniparser dumps such an entry as a section header
	// rather than a padded key, so the limit cannot bite there; FitsOnOneLine is simply
	// conservative about it.
	assert(IniFile::FitsOnOneLine(longName, std::string(longKeyRoom, 'a').c_str()));
	assert(!IniFile::FitsOnOneLine(longName, std::string(longKeyRoom + 1, 'a').c_str()));

	assert(IniFile::FitsOnOneLine("isle:x", ""));

	// The limit is iniparser's own cliff, not a guess, and these two lengths are literal rather
	// than derived from kLineLimit so that moving the constant either way fails this test. A 987
	// character value dumps as a 1022 character line - 30 columns of padded name, " = \"\"", and
	// the value - which iniparser's 1024 byte fgets reads back whole. One character more and the
	// load refuses the entire file, taking every other setting in it.
	{
		std::filesystem::path directory = MakeDirectory("limit");
		const std::string atLimit(987, 'a');
		assert(IniFile::FitsOnOneLine("isle:x", atLimit.c_str()));
		std::string path = (directory / "isle.ini").string();
		assert(IniFile::Save(path, MakeDictionary("isle:x", atLimit).get()).empty());
		Dictionary loaded(iniparser_load(path.c_str()), iniparser_freedict);
		assert(loaded);
		assert(Value(loaded.get(), "isle:x") == atLimit);

		const std::string overLimit(988, 'a');
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
		assert(Value(loaded.get(), "isle:music") == "true");
		assert(!std::filesystem::exists(path + ".new"));
	}

	// Replacing an existing file. The Windows CRT's rename fails on an existing destination, so
	// this is what pins the replacing rename there.
	{
		std::filesystem::path directory = MakeDirectory("replace");
		std::string path = (directory / "isle.ini").string();
		std::ofstream(path) << "[isle]\nmusic = \"false\"\ncustom = \"keep\"\n";
		assert(IniFile::Save(path, MakeDictionary("isle:music", "true").get()).empty());
		Dictionary loaded(iniparser_load(path.c_str()), iniparser_freedict);
		assert(loaded);
		assert(Value(loaded.get(), "isle:music") == "true");
		// Save writes the dictionary it is given and nothing else: the key that was only on disk
		// is gone. Preserving it is the caller's job, done by loading the file first.
		assert(Read(path).find("custom") == std::string::npos);
		assert(!std::filesystem::exists(path + ".new"));
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

	// What the desktop tool's merge rests on: iniparser lowercases entries on the way in and on
	// the way out, so a dialog that sets "isle:Music" lands in the slot the loader made for
	// "music" instead of adding a second one. That is a property of a fetched library, and the
	// merge silently doubles every key it writes if it ever stops holding.
	{
		std::filesystem::path directory = MakeDirectory("mixed-case");
		std::string path = (directory / "isle.ini").string();
		std::ofstream(path) << "[isle]\nmusic = \"false\"\n";
		Dictionary dict(iniparser_load(path.c_str()), iniparser_freedict);
		assert(dict);
		assert(iniparser_set(dict.get(), "isle:Music", "true") == 0);
		assert(IniFile::Save(path, dict.get()).empty());
		Dictionary loaded(iniparser_load(path.c_str()), iniparser_freedict);
		assert(loaded);
		assert(Value(loaded.get(), "isle:music") == "true");
		assert(Occurrences(Read(path), "music") == 1);
	}

	// The whole load-modify-save idiom, which is what keeps settings the writer does not know
	// about. Sections it never touches have to come back untouched.
	{
		std::filesystem::path directory = MakeDirectory("merge");
		std::string path = (directory / "isle.ini").string();
		std::ofstream(path) << "[isle]\nmusic = \"false\"\ncustom = \"keep\"\n"
							<< "[gamepad]\nsouth = \"click\"\n"
							<< "[multiplayer]\nroom = \"islanders\"\n";
		Dictionary dict(iniparser_load(path.c_str()), iniparser_freedict);
		assert(dict);
		assert(iniparser_set(dict.get(), "isle:Music", "true") == 0);
		assert(iniparser_set(dict.get(), "isle:Island Quality", "2") == 0);
		assert(IniFile::Save(path, dict.get()).empty());
		Dictionary loaded(iniparser_load(path.c_str()), iniparser_freedict);
		assert(loaded);
		assert(Value(loaded.get(), "isle:music") == "true");
		assert(Value(loaded.get(), "isle:island quality") == "2");
		assert(Value(loaded.get(), "isle:custom") == "keep");
		assert(Value(loaded.get(), "gamepad:south") == "click");
		assert(Value(loaded.get(), "multiplayer:room") == "islanders");
	}

	std::filesystem::remove_all(Root());
	return 0;
}
