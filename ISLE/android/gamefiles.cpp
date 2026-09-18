#include "gamefiles.h"

#include "configstore.h"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <map>
#include <memory>
#include <stdexcept>
#include <strings.h>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>

extern const char* g_files[46];

namespace
{
using Hook = Android_GameFiles::Checkpoint;

// The game globs only top-level entries starting with "lego", and the startup import only clears
// LEGO, imported-* and *.unreadable.*, so neither ever sees these work directories.
const char* const kGameDir = "LEGO";
const char* const kStaging = ".isle-staging-";
const char* const kReplaced = ".isle-replaced-";
const char* const kRemoved = ".isle-removed-";
const char* const kImported = "imported-";
const char* const kUnreadable = ".unreadable.";

constexpr uint64_t kMagic = 0x49534c4547465331ULL;
constexpr size_t kRecordLimit = 65536;
enum Operation : uint64_t {
	e_none = 0,
	e_replace = 1,
	e_remove = 2,
};

struct Record {
	uint64_t op = e_none;
	std::string id, root, config;
};

void Require(bool p_ok, const char* p_message)
{
	if (!p_ok) {
		throw std::runtime_error(p_message);
	}
}
void Point(const Hook& p_hook, const char* p_name)
{
	if (p_hook) {
		p_hook(p_name);
	}
}
struct FD {
	int fd;
	explicit FD(int p_fd) : fd(p_fd) { Require(fd >= 0, "Could not open game file storage."); }
	~FD()
	{
		if (fd >= 0) {
			close(fd);
		}
	}
	FD(const FD&) = delete;
	FD& operator=(const FD&) = delete;
	void Close()
	{
		int old = fd;
		fd = -1;
		Require(close(old) == 0, "Could not close game file storage.");
	}
};

bool StartsWith(const std::string& p_text, const char* p_prefix)
{
	return p_text.compare(0, strlen(p_prefix), p_prefix) == 0;
}
bool IsId(const std::string& p_id)
{
	return !p_id.empty() && p_id.size() <= 32 && p_id.find_first_not_of("0123456789") == std::string::npos;
}
std::string Join(const std::string& p_dir, const std::string& p_name)
{
	return p_dir + "/" + p_name;
}
bool Exists(const std::string& p_path)
{
	struct stat info;
	return lstat(p_path.c_str(), &info) == 0;
}
bool IsDirectory(const std::string& p_path)
{
	struct stat info;
	return lstat(p_path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}
std::vector<std::string> List(const std::string& p_dir)
{
	std::unique_ptr<DIR, decltype(&closedir)> dir(opendir(p_dir.c_str()), closedir);
	Require(dir != nullptr, "Could not list the game file storage.");
	std::vector<std::string> names;
	for (;;) {
		errno = 0;
		dirent* entry = readdir(dir.get());
		if (!entry) {
			Require(errno == 0, "Could not list the game file storage.");
			return names;
		}
		if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
			names.emplace_back(entry->d_name);
		}
	}
}
std::string Canonical(const std::string& p_path)
{
	Require(!p_path.empty() && p_path[0] == '/', "The game file storage path is not absolute.");
	std::unique_ptr<char, decltype(&free)> path(realpath(p_path.c_str(), nullptr), free);
	Require(path != nullptr && IsDirectory(path.get()), "The game file storage is unavailable.");
	return path.get();
}

// The live game directory, whatever its case, or empty.
std::string LiveName(const std::string& p_root)
{
	for (const std::string& name : List(p_root)) {
		if (strcasecmp(name.c_str(), kGameDir) == 0 && IsDirectory(Join(p_root, name))) {
			return name;
		}
	}
	return {};
}

// The private record directory is on internal storage, where a failed sync is a real failure.
void Sync(int p_fd, const Hook& p_hook)
{
	Require(fsync(p_fd) == 0, "Could not synchronize game file storage.");
	Point(p_hook, "sync");
}
// External storage can be a FUSE or sdcardfs mount that does not support syncing a directory.
void SyncPath(const std::string& p_path, const Hook& p_hook)
{
	FD dir(open(p_path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
	if (fsync(dir.fd) != 0) {
		Require(
			errno == EINVAL || errno == ENOSYS || errno == ENOTSUP || errno == EROFS,
			"Could not synchronize the game files."
		);
	}
	Point(p_hook, "sync");
}
// Makes every directory entry in a staged tree durable. The files themselves were synced as
// they were copied.
void SyncTree(const std::string& p_dir, const Hook& p_hook)
{
	for (const std::string& name : List(p_dir)) {
		std::string path = Join(p_dir, name);
		if (IsDirectory(path)) {
			SyncTree(path, p_hook);
		}
	}
	SyncPath(p_dir, p_hook);
}

uint32_t Crc(const std::vector<uint8_t>& p_bytes)
{
	uint32_t crc = ~0U;
	for (uint8_t byte : p_bytes) {
		crc ^= byte;
		for (int bit = 0; bit < 8; bit++) {
			crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1)));
		}
	}
	return ~crc;
}
void Number(std::vector<uint8_t>& p_out, uint64_t p_n)
{
	for (int i = 0; i < 8; i++) {
		p_out.push_back(p_n & 255);
		p_n >>= 8;
	}
}
void String(std::vector<uint8_t>& p_out, const std::string& p_text)
{
	Number(p_out, p_text.size());
	p_out.insert(p_out.end(), p_text.begin(), p_text.end());
}
struct Reader {
	const std::vector<uint8_t>& bytes;
	size_t pos = 0;
	uint64_t Num()
	{
		Require(bytes.size() - pos >= 8, "The game file record is truncated.");
		uint64_t n = 0;
		for (int i = 0; i < 8; i++) {
			n |= static_cast<uint64_t>(bytes[pos++]) << (8 * i);
		}
		return n;
	}
	std::string Text()
	{
		uint64_t n = Num();
		Require(n <= 4096 && n <= bytes.size() - pos, "Invalid game file record string.");
		std::string text(bytes.begin() + pos, bytes.begin() + pos + n);
		pos += n;
		Require(text.find('\0') == std::string::npos, "Invalid game file record string.");
		return text;
	}
};

std::vector<uint8_t> ReadRecord(int p_dir)
{
	FD file(openat(p_dir, "state", O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
	struct stat info;
	Require(
		fstat(file.fd, &info) == 0 && S_ISREG(info.st_mode) && info.st_size >= 0 &&
			static_cast<uint64_t>(info.st_size) <= kRecordLimit,
		"The game file record is not a regular file."
	);
	std::vector<uint8_t> bytes;
	uint8_t buffer[4096];
	for (;;) {
		ssize_t count = read(file.fd, buffer, sizeof(buffer));
		if (count < 0 && errno == EINTR) {
			continue;
		}
		Require(count >= 0, "Could not read the game file record.");
		if (!count) {
			break;
		}
		Require(static_cast<size_t>(count) <= kRecordLimit - bytes.size(), "The game file record is too large.");
		bytes.insert(bytes.end(), buffer, buffer + count);
	}
	file.Close();
	return bytes;
}
Record Load(int p_dir)
{
	struct stat info;
	if (fstatat(p_dir, "state", &info, AT_SYMLINK_NOFOLLOW) != 0) {
		Require(errno == ENOENT, "Could not inspect the game file record.");
		return {};
	}
	std::vector<uint8_t> bytes = ReadRecord(p_dir);
	Require(bytes.size() >= 8, "The game file record is damaged.");
	Reader checksum{bytes, bytes.size() - 8};
	uint64_t expected = checksum.Num();
	bytes.resize(bytes.size() - 8);
	Require(Crc(bytes) == expected, "The game file record is damaged.");
	Reader reader{bytes};
	Require(reader.Num() == kMagic, "Unsupported game file record version.");
	Record record;
	record.op = reader.Num();
	record.id = reader.Text();
	record.root = reader.Text();
	record.config = reader.Text();
	Require(reader.pos == bytes.size(), "Unexpected game file record data.");
	Require(record.op == e_replace || record.op == e_remove, "Invalid game file operation.");
	Require(IsId(record.id), "Invalid game file operation ID.");
	Require(
		!record.root.empty() && record.root[0] == '/' && !record.config.empty() && record.config[0] == '/',
		"Invalid game file record paths."
	);
	return record;
}
void Store(int p_dir, const Record& p_record, const Hook& p_hook)
{
	std::vector<uint8_t> bytes;
	Number(bytes, kMagic);
	Number(bytes, p_record.op);
	String(bytes, p_record.id);
	String(bytes, p_record.root);
	String(bytes, p_record.config);
	Number(bytes, Crc(bytes));
	Require(unlinkat(p_dir, "state.tmp", 0) == 0 || errno == ENOENT, "Could not prepare the game file record.");
	{
		FD file(openat(p_dir, "state.tmp", O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600));
		Point(p_hook, "create");
		size_t offset = 0;
		while (offset < bytes.size()) {
			ssize_t count = write(file.fd, bytes.data() + offset, bytes.size() - offset);
			if (count < 0 && errno == EINTR) {
				continue;
			}
			Require(count > 0, "Could not write the game file record. Check available space.");
			offset += count;
		}
		Sync(file.fd, p_hook);
		file.Close();
	}
	Require(renameat(p_dir, "state.tmp", p_dir, "state") == 0, "Could not publish the game file record.");
	Point(p_hook, "record");
	Sync(p_dir, p_hook);
}
void Clear(int p_dir, const Hook& p_hook)
{
	Require(unlinkat(p_dir, "state", 0) == 0 || errno == ENOENT, "Could not clear the game file record.");
	Point(p_hook, "record");
	Sync(p_dir, p_hook);
}

// Points diskpath at the root. A missing configuration needs nothing: the game writes a new one
// whose diskpath is already the root.
std::string SetDiskPath(const std::string& p_config, const std::string& p_root, const Hook& p_hook)
{
	if (!Exists(p_config)) {
		return {};
	}
	std::string error = Android_UpdateConfig(p_config, {{"isle:diskpath", p_root.c_str()}});
	if (!error.empty()) {
		return error;
	}
	Point(p_hook, "config");
	std::string dir = p_config.substr(0, p_config.find_last_of('/'));
	SyncPath(dir.empty() ? "/" : dir, p_hook);
	return {};
}

// Renames p_from to p_to. The checkpoint named p_step runs first, so tests can make the rename fail
// the way the file system would, by throwing std::system_error from it.
bool Move(const std::string& p_from, const std::string& p_to, const Hook& p_hook, const char* p_step)
{
	try {
		Point(p_hook, p_step);
	}
	catch (const std::system_error& error) {
		errno = error.code().value();
		return false;
	}
	return rename(p_from.c_str(), p_to.c_str()) == 0;
}

// Syncs after a rename that has already happened, where failing must not stop the change halfway.
void Settle(const std::string& p_path, const Hook& p_hook)
{
	int fd = open(p_path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (fd >= 0) {
		(void) fsync(fd);
		close(fd);
	}
	Point(p_hook, "sync");
}

// Renames an entry out of the way under a name no one else uses, returning its new path, or empty
// if it could not be moved. A rename within the same parent works even for a tree this app cannot
// delete, such as one pushed in over adb.
std::string Discard(const std::string& p_root, const std::string& p_name, const std::string& p_id, const Hook& p_hook)
{
	std::string target;
	for (int n = 0;; n++) {
		target = Join(p_root, kRemoved + p_id + "-" + std::to_string(n));
		if (!Exists(target)) {
			break;
		}
	}
	if (!Move(Join(p_root, p_name), target, p_hook, "discard?")) {
		return {};
	}
	Point(p_hook, "discard");
	return target;
}

// Moves a retired tree back into place, returning whether it did. It runs where there may be no game
// folder, so it never throws; the rename itself refuses to replace a game folder with files in it.
bool Reinstate(const std::string& p_root, const std::string& p_retired, const Hook& p_hook)
{
	std::string retired = Join(p_root, p_retired);
	if (!IsDirectory(retired) || !Move(retired, Join(p_root, kGameDir), p_hook, "reinstate?")) {
		return false;
	}
	Point(p_hook, "reinstate");
	Settle(p_root, p_hook);
	return true;
}

// When the previous files cannot be put back, the record stays, so both copies survive until a later
// start tries again.
const char* const kBothKept =
	"The previous game files could not be put back yet. Both copies were kept, and the next start tries again.";

std::string ApplyReplace(
	int p_dir,
	const Record& p_record,
	const std::string& p_root,
	const std::string& p_diskPath,
	const Hook& p_hook
)
{
	std::string staging = Join(p_root, Android_GameFiles::StagingName(p_record.id));
	std::string staged = Join(staging, kGameDir);
	std::string retired = kReplaced + p_record.id;

	if (IsDirectory(staged)) {
		std::string live = LiveName(p_root);
		bool wasRetired = Exists(Join(p_root, retired));
		if (!live.empty() && wasRetired) {
			// Something other than this change put a game folder back after the old one was
			// retired. Keep it if it is complete; otherwise the retired files come back.
			if (Android_FindMissingGameFile(p_root) != nullptr &&
				(Discard(p_root, live, p_record.id, p_hook).empty() || !Reinstate(p_root, retired, p_hook))) {
				return kBothKept;
			}
			Clear(p_dir, p_hook);
			return "The game files changed while a replacement was waiting, so it was skipped.";
		}
		if (Android_FindMissingGameFile(staging) != nullptr) {
			// Checked before anything moves, so an incomplete copy never displaces the live files. A
			// start that died after retiring them leaves them to be brought back.
			if (wasRetired && !Reinstate(p_root, retired, p_hook)) {
				return kBothKept;
			}
			Clear(p_dir, p_hook);
			return "The new game files were incomplete, so the previous files were kept.";
		}
		if (!live.empty()) {
			if (!Move(Join(p_root, live), Join(p_root, retired), p_hook, "retire?")) {
				std::string reason = strerror(errno);
				Clear(p_dir, p_hook);
				return "The previous game files could not be moved aside (" + reason + "), so they were kept.";
			}
			Point(p_hook, "retire");
			// Until the new tree is in place there is no game folder, so nothing here may throw.
			Settle(p_root, p_hook);
		}
		if (!Move(staged, Join(p_root, kGameDir), p_hook, "install?")) {
			std::string reason = strerror(errno);
			if (Exists(Join(p_root, retired)) && !Reinstate(p_root, retired, p_hook)) {
				return kBothKept;
			}
			Clear(p_dir, p_hook);
			return "The new game files could not be put in place (" + reason + "), so the previous files were kept.";
		}
		Point(p_hook, "install");
		Settle(p_root, p_hook);
	}
	else if (!IsDirectory(staging) || LiveName(p_root).empty()) {
		// An installed copy leaves its staging directory empty with the game folder in place;
		// anything else means the staged copy was lost.
		if (Exists(Join(p_root, retired)) && !Reinstate(p_root, retired, p_hook)) {
			return kBothKept;
		}
		Clear(p_dir, p_hook);
		return "The new game files were missing, so the previous files were kept.";
	}

	if (Android_FindMissingGameFile(p_root) != nullptr) {
		// Only reachable if something changed the installed tree; do not clear what else there is.
		Clear(p_dir, p_hook);
		return "The new game files could not be checked after they were put in place.";
	}
	std::string error = SetDiskPath(p_record.config, p_diskPath, p_hook);
	if (!error.empty()) {
		return "The new game files are in place, but the game settings could not be updated. This will be "
			   "retried the next time the game starts. " +
			   error;
	}
	// Earlier imports are unused once diskpath names the root.
	for (const std::string& name : List(p_root)) {
		if (StartsWith(name, kImported) || name.find(kUnreadable) != std::string::npos) {
			Discard(p_root, name, p_record.id, p_hook);
		}
	}
	SyncPath(p_root, p_hook);
	Clear(p_dir, p_hook);
	return "Game files replaced.";
}

std::string ApplyRemove(
	int p_dir,
	const Record& p_record,
	const std::string& p_root,
	const std::string& p_diskPath,
	const Hook& p_hook
)
{
	bool kept = false;
	for (const std::string& name : List(p_root)) {
		if (strcasecmp(name.c_str(), kGameDir) == 0 || StartsWith(name, kImported) ||
			name.find(kUnreadable) != std::string::npos || StartsWith(name, kReplaced)) {
			kept |= Discard(p_root, name, p_record.id, p_hook).empty();
		}
	}
	SyncPath(p_root, p_hook);
	std::string error = SetDiskPath(p_record.config, p_diskPath, p_hook);
	if (!error.empty()) {
		return "The game files were removed, but the game settings could not be updated. This will be retried "
			   "the next time the game starts. " +
			   error;
	}
	Clear(p_dir, p_hook);
	return kept ? "Some game files could not be removed." : "Game files removed.";
}

// Work directories that no recorded change or running copy needs any more.
void Sweep(
	const std::string& p_root,
	const std::string& p_pending,
	const std::set<std::string>& p_claimed,
	std::vector<std::string>& p_garbage,
	std::string& p_message,
	const Hook& p_hook
)
{
	std::vector<std::string> names = List(p_root);
	bool live = !LiveName(p_root).empty();
	if (!live && p_pending.empty()) {
		// Only a lost record leaves a retired tree with nothing in its place.
		std::string newest;
		for (const std::string& name : names) {
			if (!StartsWith(name, kReplaced)) {
				continue;
			}
			std::string id = name.substr(strlen(kReplaced));
			if (IsId(id) &&
				(newest.empty() || id.size() > newest.size() || (id.size() == newest.size() && id > newest))) {
				newest = id;
			}
		}
		if (!newest.empty() && Reinstate(p_root, kReplaced + newest, p_hook)) {
			live = true;
			names = List(p_root);
			if (p_message.empty()) {
				p_message = "The previous game files were recovered.";
			}
		}
	}
	for (const std::string& name : names) {
		std::string id;
		if (StartsWith(name, kStaging)) {
			id = name.substr(strlen(kStaging));
		}
		else if (StartsWith(name, kReplaced)) {
			if (!live) {
				continue;
			}
			id = name.substr(strlen(kReplaced));
		}
		else if (!StartsWith(name, kRemoved)) {
			continue;
		}
		if (!id.empty() && (id == p_pending || p_claimed.count(id))) {
			continue;
		}
		if (StartsWith(name, kReplaced)) {
			// Renamed before it is handed out, so a tree that then fails to delete can never be
			// taken later for one to bring back. One that cannot be renamed stays whole for now.
			std::string discarded = Discard(p_root, name, id, p_hook);
			if (!discarded.empty()) {
				p_garbage.push_back(discarded);
			}
			continue;
		}
		p_garbage.push_back(Join(p_root, name));
	}
}

bool DeleteAt(int p_parent, const char* p_name, uint64_t& p_bytes, const std::function<void()>& p_pump)
{
	struct stat info;
	if (fstatat(p_parent, p_name, &info, AT_SYMLINK_NOFOLLOW) != 0) {
		return errno == ENOENT;
	}
	bool deleted = true;
	if (S_ISDIR(info.st_mode)) {
		int fd = openat(p_parent, p_name, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
		std::unique_ptr<DIR, decltype(&closedir)> dir(fd >= 0 ? fdopendir(fd) : nullptr, closedir);
		if (!dir) {
			if (fd >= 0) {
				close(fd);
			}
			return false;
		}
		std::vector<std::string> children;
		while (dirent* entry = readdir(dir.get())) {
			if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
				children.emplace_back(entry->d_name);
			}
		}
		for (const std::string& child : children) {
			deleted &= DeleteAt(dirfd(dir.get()), child.c_str(), p_bytes, p_pump);
		}
		dir.reset();
		deleted &= unlinkat(p_parent, p_name, AT_REMOVEDIR) == 0;
	}
	else if (unlinkat(p_parent, p_name, 0) == 0) {
		if (S_ISREG(info.st_mode)) {
			p_bytes += info.st_size;
		}
	}
	else {
		deleted = false;
	}
	if (p_pump) {
		p_pump();
	}
	return deleted;
}
} // namespace

Android_GameFiles::Android_GameFiles(std::string p_recordDir, std::string p_root, Checkpoint p_checkpoint)
	: m_recordDir(std::move(p_recordDir)), m_root(std::move(p_root)), m_checkpoint(std::move(p_checkpoint))
{
	Require(mkdir(m_recordDir.c_str(), 0700) == 0 || errno == EEXIST, "Could not create private game file storage.");
	FD dir(open(m_recordDir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
	// Only the published record is authoritative; a temporary one never finished publishing.
	Require(unlinkat(dir.fd, "state.tmp", 0) == 0 || errno == ENOENT, "Could not clean game file storage.");
	// A record published before a failed directory sync must be durable before the game runs.
	Sync(dir.fd, m_checkpoint);
}

std::string Android_GameFiles::StagingName(const std::string& p_id)
{
	return kStaging + p_id;
}

std::string Android_GameFiles::NewId()
{
	return std::to_string(
		std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch())
			.count()
	);
}

void Android_GameFiles::Schedule(const std::string& p_config, const std::string& p_id)
{
	FD dir(open(m_recordDir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
	Require(
		Load(dir.fd).op == e_none,
		"A change to the game files is already waiting. Close and reopen the game to apply it."
	);
	Require(!p_config.empty() && p_config[0] == '/', "The game settings file is unavailable.");
	Record record;
	record.root = Canonical(m_root);
	record.config = p_config;
	if (p_id.empty()) {
		record.op = e_remove;
		record.id = NewId();
	}
	else {
		Require(IsId(p_id), "Invalid game file operation ID.");
		std::string staging = Join(record.root, StagingName(p_id));
		Require(
			IsDirectory(staging) && IsDirectory(Join(staging, kGameDir)),
			"The copied game files are missing. Select the folder again."
		);
		const char* missing = Android_FindMissingGameFile(staging);
		if (missing) {
			throw std::runtime_error(std::string("The copied game files are incomplete: ") + missing + " is missing.");
		}
		SyncTree(staging, m_checkpoint);
		SyncPath(record.root, m_checkpoint);
		record.op = e_replace;
		record.id = p_id;
	}
	Store(dir.fd, record, m_checkpoint);
}

bool Android_GameFiles::Pending()
{
	FD dir(open(m_recordDir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
	return Load(dir.fd).op != e_none;
}

std::string Android_GameFiles::Apply(std::vector<std::string>& p_garbage, const std::set<std::string>& p_claimed)
{
	FD dir(open(m_recordDir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
	std::string message;
	Record record;
	try {
		record = Load(dir.fd);
	}
	catch (const std::exception&) {
		// Nothing in a damaged record can be trusted, and the files it names are consistent
		// either way. Keep it for inspection and carry on with whatever is in place.
		Require(renameat(dir.fd, "state", dir.fd, "state.damaged") == 0, "Could not set aside the game file record.");
		Point(m_checkpoint, "record");
		Sync(dir.fd, m_checkpoint);
		message = "A waiting game file change could not be read, so it was skipped.";
	}

	std::string root = Canonical(m_root);
	if (record.op != e_none && record.root != root) {
		Clear(dir.fd, m_checkpoint);
		record = {};
		message = "The game file storage moved, so the waiting change was skipped.";
	}
	if (record.op == e_replace) {
		message = ApplyReplace(dir.fd, record, root, m_root, m_checkpoint);
	}
	else if (record.op == e_remove) {
		message = ApplyRemove(dir.fd, record, root, m_root, m_checkpoint);
	}

	Record pending = Load(dir.fd);
	Sweep(root, pending.id, p_claimed, p_garbage, message, m_checkpoint);
	return message;
}

const char* Android_FindMissingGameFile(const std::string& p_root)
{
	std::map<std::string, std::vector<std::string>> listings;
	auto list = [&](const std::string& p_dir) -> const std::vector<std::string>& {
		auto found = listings.find(p_dir);
		if (found == listings.end()) {
			std::vector<std::string> names;
			std::unique_ptr<DIR, decltype(&closedir)> dir(opendir(p_dir.c_str()), closedir);
			while (dir) {
				dirent* entry = readdir(dir.get());
				if (!entry) {
					break;
				}
				names.emplace_back(entry->d_name);
			}
			found = listings.emplace(p_dir, std::move(names)).first;
		}
		return found->second;
	};
	// Tries every entry that matches a component ignoring case, since a case-sensitive volume can
	// hold several.
	std::function<bool(const std::string&, const std::string&)> find = [&](const std::string& p_dir,
																		   const std::string& p_rest) {
		size_t slash = p_rest.find('/');
		std::string component = p_rest.substr(0, slash);
		for (const std::string& name : list(p_dir)) {
			if (strcasecmp(name.c_str(), component.c_str()) != 0) {
				continue;
			}
			std::string path = Join(p_dir, name);
			struct stat info;
			if (stat(path.c_str(), &info) != 0) {
				continue;
			}
			if (slash == std::string::npos ? S_ISREG(info.st_mode)
										   : S_ISDIR(info.st_mode) && find(path, p_rest.substr(slash + 1))) {
				return true;
			}
		}
		return false;
	};
	for (const char* file : g_files) {
		if (!find(p_root, file[0] == '/' ? file + 1 : file)) {
			return file;
		}
	}
	return nullptr;
}

std::vector<std::string> Android_StockGameFiles()
{
	return {std::begin(g_files), std::end(g_files)};
}

bool Android_DeleteTree(const std::string& p_path, uint64_t& p_bytes, const std::function<void()>& p_pump)
{
	size_t slash = p_path.find_last_of('/');
	if (slash == std::string::npos || slash + 1 == p_path.size()) {
		return false;
	}
	std::string parent = slash ? p_path.substr(0, slash) : "/";
	int fd = open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (fd < 0) {
		return false;
	}
	bool deleted = DeleteAt(fd, p_path.c_str() + slash + 1, p_bytes, p_pump);
	close(fd);
	return deleted;
}
