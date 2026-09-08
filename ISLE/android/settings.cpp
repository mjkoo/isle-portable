#include "settings.h"

#include "activity.h"
#include "configstore.h"
#include "mxdirectx/legodxinfo.h"
#include "saverestore.h"
#include "savesnapshot.h"

#include <atomic>
#include <chrono>
#include <jni.h>
#include <mutex>
#include <stdexcept>
#include <sys/stat.h>

static std::string g_settingsPath;
static std::vector<std::string> g_renderers;
static std::atomic<bool> g_menuRequested{false};
static std::mutex g_exportMutex;
static Android_SaveSnapshot g_export;
static std::string g_exportId;
static std::string g_exportTime;
static int g_exportSaveResult;
static std::string g_restorePath;
static std::mutex g_restoreMutex;
static std::string g_restoreRoot;
static bool g_restoreStartup = false;
static std::atomic<bool> g_restoreQuit{false};

void Android_CaptureSaveExport(const char* p_savePath, int p_saveResult)
{
	Uint64 started = SDL_GetTicksNS();
	std::string path = p_savePath ? p_savePath : "";
	std::string error;
	if (!p_savePath) {
		const char* internal = SDL_GetAndroidInternalStoragePath();
		if (!internal || !*internal) {
			error = "The app's internal storage directory is unavailable.";
		}
		else {
			std::string config = g_settingsPath.empty() ? std::string(internal) + "/isle.ini" : g_settingsPath;
			error = Android_ResolveSaveExportPath(config, std::string(internal) + "/saves", path);
		}
	}
	Android_SaveSnapshot snapshot =
		error.empty() ? Android_ReadSaveSnapshot(path) : Android_SaveSnapshot{{}, error, false};
	auto now = std::chrono::system_clock::now().time_since_epoch();
	std::lock_guard<std::mutex> lock(g_exportMutex);
	g_exportId = std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
	g_exportTime = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
	g_exportSaveResult = p_saveResult;
	g_restorePath = error.empty() ? path : "";
	g_export = std::move(snapshot);
	SDL_LogInfo(
		SDL_LOG_CATEGORY_APPLICATION,
		"Captured %zu save files in %.2f ms%s",
		g_export.m_files.size(),
		(SDL_GetTicksNS() - started) / 1000000.0,
		g_export.m_error.empty() ? "" : " (export unavailable)"
	);
}

void Android_ClearSaveExport()
{
	std::lock_guard<std::mutex> lock(g_exportMutex);
	g_export = {};
	g_exportId.clear();
}

void Android_SetSettingsPath(const char* p_path)
{
	g_settingsPath = p_path;
}

void Android_CaptureRenderers(SDL_Window* p_window)
{
	g_renderers.clear();
	LegoDeviceEnumerate devices;
	if (devices.DoEnumerate(reinterpret_cast<HWND>(p_window)) != 0) {
		return;
	}
	for (int index = 0;; index++) {
		MxDriver* driver;
		Direct3DDeviceInfo* device;
		if (devices.GetDevice(index, driver, device) != 0) {
			break;
		}
		char id[128];
		if (device->m_guid && devices.FormatDeviceName(id, driver, device) == 0) {
			g_renderers.emplace_back(device->m_deviceDesc ? device->m_deviceDesc : id);
			g_renderers.emplace_back(id);
		}
	}
}

void Android_ShowMenuButton()
{
	Android_ActivityCall call;
	if (Android_BeginActivityCall(&call, "showMenuButton", "()V")) {
		call.m_env->CallVoidMethod(call.m_activity, call.m_method);
		Android_EndActivityCall(&call);
	}
}

bool Android_TakeMenuRequest()
{
	return g_menuRequested.exchange(false);
}

void Android_ShowStartupSettings(const char* p_error, const char* p_savePath, int p_saveResult)
{
	Android_CaptureSaveExport(p_savePath, p_saveResult);
	struct ClearExport {
		~ClearExport() { Android_ClearSaveExport(); }
	} clear;
	Android_ActivityCall call;
	if (!Android_BeginActivityCall(&call, "showStartupSettings", "(Ljava/lang/String;)V")) {
		return;
	}
	jstring error = call.m_env->NewStringUTF(p_error);
	call.m_env->CallVoidMethod(call.m_activity, call.m_method, error);
	call.m_env->DeleteLocalRef(error);
	if (!Android_EndActivityCall(&call)) {
		return;
	}
	while (!g_restoreQuit.load() && Android_CallActivityBooleanMethod("isStartupSettingsOpen")) {
		Android_DrainInputEvents();
		SDL_Delay(100);
	}
}

static std::string FromJava(JNIEnv* p_env, jstring p_value)
{
	if (!p_value) {
		return {};
	}
	const char* value = p_env->GetStringUTFChars(p_value, nullptr);
	std::string result = value ? value : "";
	if (value) {
		p_env->ReleaseStringUTFChars(p_value, value);
	}
	return result;
}

static jobjectArray ToJava(JNIEnv* p_env, const std::vector<std::string>& p_values)
{
	jclass cls = p_env->FindClass("java/lang/String");
	if (!cls) {
		return nullptr;
	}
	jobjectArray result = p_env->NewObjectArray(p_values.size(), cls, nullptr);
	p_env->DeleteLocalRef(cls);
	for (size_t i = 0; result && i < p_values.size(); i++) {
		jstring value = p_env->NewStringUTF(p_values[i].c_str());
		if (!value) {
			return nullptr;
		}
		p_env->SetObjectArrayElement(result, i, value);
		p_env->DeleteLocalRef(value);
		if (p_env->ExceptionCheck()) {
			return nullptr;
		}
	}
	return result;
}

extern "C" JNIEXPORT jstring JNICALL Java_org_legoisland_isle_SettingsBridge_exportId(JNIEnv* p_env, jclass)
{
	std::lock_guard<std::mutex> lock(g_exportMutex);
	return p_env->NewStringUTF(g_exportId.c_str());
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_org_legoisland_isle_SettingsBridge_exportInfo(JNIEnv* p_env, jclass, jstring p_id)
{
	std::string id = FromJava(p_env, p_id);
	std::lock_guard<std::mutex> lock(g_exportMutex);
	if (id.empty() || id != g_exportId) {
		return ToJava(p_env, {"Reopen the game and its menu to capture saves for export.", "0", ""});
	}
	std::string warning;
	if (g_exportSaveResult == 2) {
		warning = "The game could not save. Recent progress may be missing. ";
	}
	if (g_export.m_incomplete && !g_export.m_files.empty()) {
		warning += "Some companion save files are missing. These files may not form a loadable game. ";
	}
	std::vector<std::string> info{g_export.m_error, g_exportTime, warning};
	for (const auto& file : g_export.m_files) {
		info.push_back(file.m_name);
	}
	return ToJava(p_env, info);
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_org_legoisland_isle_SettingsBridge_exportData(JNIEnv* p_env, jclass, jstring p_id)
{
	std::string id = FromJava(p_env, p_id);
	std::lock_guard<std::mutex> lock(g_exportMutex);
	if (id.empty() || id != g_exportId || !g_export.m_error.empty()) {
		return nullptr;
	}
	jclass cls = p_env->FindClass("[B");
	if (!cls) {
		return nullptr;
	}
	jobjectArray result = p_env->NewObjectArray(g_export.m_files.size(), cls, nullptr);
	p_env->DeleteLocalRef(cls);
	for (size_t i = 0; result && i < g_export.m_files.size(); i++) {
		const auto& bytes = g_export.m_files[i].m_bytes;
		jbyteArray array = p_env->NewByteArray(bytes.size());
		if (!array) {
			return nullptr;
		}
		if (!bytes.empty()) {
			p_env->SetByteArrayRegion(array, 0, bytes.size(), reinterpret_cast<const jbyte*>(bytes.data()));
		}
		if (!p_env->ExceptionCheck()) {
			p_env->SetObjectArrayElement(result, i, array);
		}
		p_env->DeleteLocalRef(array);
		if (p_env->ExceptionCheck()) {
			return nullptr;
		}
	}
	return result;
}

extern "C" JNIEXPORT void JNICALL Java_org_legoisland_isle_SettingsBridge_requestMenu(JNIEnv*, jclass)
{
	g_menuRequested = true;
}

extern "C" JNIEXPORT jstring JNICALL Java_org_legoisland_isle_SettingsBridge_path(JNIEnv* p_env, jclass)
{
	return p_env->NewStringUTF(g_settingsPath.c_str());
}

extern "C" JNIEXPORT jobjectArray JNICALL Java_org_legoisland_isle_SettingsBridge_renderers(JNIEnv* p_env, jclass)
{
	return ToJava(p_env, g_renderers);
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_org_legoisland_isle_SettingsBridge_read(JNIEnv* p_env, jclass, jstring p_path, jobjectArray p_keys)
{
	std::vector<std::string> keys;
	for (jsize i = 0; i < p_env->GetArrayLength(p_keys); i++) {
		jstring key = static_cast<jstring>(p_env->GetObjectArrayElement(p_keys, i));
		keys.push_back(FromJava(p_env, key));
		p_env->DeleteLocalRef(key);
	}
	std::vector<std::pair<bool, std::string>> values;
	std::string error = Android_ReadConfig(FromJava(p_env, p_path), keys, values);
	if (!error.empty()) {
		jclass cls = p_env->FindClass("java/lang/IllegalStateException");
		p_env->ThrowNew(cls, error.c_str());
		p_env->DeleteLocalRef(cls);
		return nullptr;
	}
	std::vector<std::string> strings;
	for (const auto& value : values) {
		strings.push_back(value.second);
	}
	jobjectArray result = ToJava(p_env, strings);
	for (size_t i = 0; result && i < values.size(); i++) {
		if (!values[i].first) {
			p_env->SetObjectArrayElement(result, i, nullptr);
		}
	}
	return result;
}

extern "C" JNIEXPORT jstring JNICALL Java_org_legoisland_isle_SettingsBridge_write(
	JNIEnv* p_env,
	jclass,
	jstring p_path,
	jobjectArray p_keys,
	jobjectArray p_values,
	jobjectArray p_renderers
)
{
	std::vector<std::string> renderers;
	for (jsize i = 0; i < p_env->GetArrayLength(p_renderers); i++) {
		jstring value = static_cast<jstring>(p_env->GetObjectArrayElement(p_renderers, i));
		renderers.push_back(FromJava(p_env, value));
		p_env->DeleteLocalRef(value);
	}
	jsize count = p_env->GetArrayLength(p_keys);
	if (count != p_env->GetArrayLength(p_values)) {
		return p_env->NewStringUTF("Invalid settings update.");
	}
	std::vector<std::string> values(count);
	std::vector<std::pair<std::string, const char*>> changes;
	for (jsize i = 0; i < count; i++) {
		jstring key = static_cast<jstring>(p_env->GetObjectArrayElement(p_keys, i));
		jstring value = static_cast<jstring>(p_env->GetObjectArrayElement(p_values, i));
		std::string name = FromJava(p_env, key);
		values[i] = FromJava(p_env, value);
		const char* text = value ? values[i].c_str() : nullptr;
		bool valid = Android_ValidateSetting(name, text, renderers);
		p_env->DeleteLocalRef(key);
		p_env->DeleteLocalRef(value);
		if (!valid) {
			return p_env->NewStringUTF("Invalid setting. Choose a supported value and try again.");
		}
		changes.emplace_back(name, text);
	}
	std::string error = Android_UpdateConfig(FromJava(p_env, p_path), changes);
	return error.empty() ? nullptr : p_env->NewStringUTF(error.c_str());
}

namespace
{
std::string RestoreDestination()
{
	const char* internal = SDL_GetAndroidInternalStoragePath();
	if (!internal || !*internal) {
		throw std::runtime_error("Internal storage is unavailable.");
	}
	std::string path, fallback = std::string(internal) + "/saves";
	std::string error = Android_ResolveSaveExportPath(std::string(internal) + "/isle.ini", fallback, path);
	if (!error.empty()) {
		throw std::runtime_error(error);
	}
	if (path == fallback) {
		if (mkdir(path.c_str(), 0700) != 0 && errno != EEXIST) {
			throw std::runtime_error("Could not create the default save directory.");
		}
	}
	return path;
}
std::string MenuRestorePath(JNIEnv* p_env, jstring p_id)
{
	std::lock_guard<std::mutex> lock(g_exportMutex);
	if (g_exportId.empty() || FromJava(p_env, p_id) != g_exportId || g_restorePath.empty()) {
		throw std::runtime_error("Reopen the game menu to restore saves. The save directory must be accessible.");
	}
	return g_restorePath;
}
} // namespace

bool Android_SaveRestoreClosing()
{
	return g_restoreQuit.load();
}

bool Android_RestoreBeforeStartup()
{
	const char* internal = SDL_GetAndroidInternalStoragePath();
	if (!internal || !*internal) {
		return false;
	}
	{
		std::lock_guard<std::mutex> lock(g_restoreMutex);
		g_restoreRoot = std::string(internal) + "/save-restore";
		g_restoreStartup = true;
		g_restoreQuit = false;
	}
	Android_ActivityCall call;
	bool success = false;
	if (Android_BeginActivityCall(&call, "startSaveRestore", "()V")) {
		call.m_env->CallVoidMethod(call.m_activity, call.m_method);
		if (Android_EndActivityCall(&call)) {
			for (;;) {
				Android_DrainInputEvents();
				if (!Android_BeginActivityCall(&call, "getSaveRestoreStatus", "()I")) {
					break;
				}
				int status = call.m_env->CallIntMethod(call.m_activity, call.m_method);
				if (!Android_EndActivityCall(&call)) {
					break;
				}
				if (status != -1) {
					success = status == 0;
					break;
				}
				SDL_Delay(50);
			}
		}
	}
	// Do not release the engine-start barrier while an abandoned worker is writing.
	while (!g_restoreMutex.try_lock()) {
		Android_DrainInputEvents();
		SDL_Delay(50);
	}
	g_restoreStartup = false;
	g_restoreMutex.unlock();
	return success;
}

extern "C" JNIEXPORT jstring JNICALL Java_org_legoisland_isle_SettingsBridge_recoverRestore(JNIEnv* p_env, jclass)
{
	std::lock_guard<std::mutex> lock(g_restoreMutex);
	try {
		if (!g_restoreStartup) {
			throw std::runtime_error("Restore is only available before the game starts.");
		}
		Android_SaveRestore store(g_restoreRoot);
		std::string message = store.Pending() ? store.Recover(RestoreDestination()) : "";
		return p_env->NewStringUTF(("OK:" + message).c_str());
	}
	catch (const std::exception& error) {
		return p_env->NewStringUTF(error.what());
	}
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_org_legoisland_isle_SettingsBridge_restoreInfo(JNIEnv* p_env, jclass, jstring p_id)
{
	std::lock_guard<std::mutex> lock(g_restoreMutex);
	try {
		std::string path = MenuRestorePath(p_env, p_id);
		return ToJava(p_env, {"", Android_SaveRestore(g_restoreRoot).Previous(path)});
	}
	catch (const std::exception& error) {
		return ToJava(p_env, {error.what(), ""});
	}
}

extern "C" JNIEXPORT jboolean JNICALL Java_org_legoisland_isle_SettingsBridge_restoreClosing(JNIEnv*, jclass)
{
	return g_restoreQuit.load();
}

extern "C" JNIEXPORT jstring JNICALL Java_org_legoisland_isle_SettingsBridge_scheduleRestore(
	JNIEnv* p_env,
	jclass,
	jstring p_id,
	jobjectArray p_names,
	jobjectArray p_data,
	jboolean p_previous
)
{
	std::lock_guard<std::mutex> lock(g_restoreMutex);
	try {
		std::string path = MenuRestorePath(p_env, p_id);
		std::vector<Android_SaveFile> files;
		if (!p_previous) {
			if (!p_names || !p_data || p_env->GetArrayLength(p_names) != p_env->GetArrayLength(p_data) ||
				p_env->GetArrayLength(p_names) > 11) {
				throw std::runtime_error("Invalid restore files.");
			}
			size_t total = 0;
			for (jsize i = 0; i < p_env->GetArrayLength(p_names); i++) {
				jstring name = static_cast<jstring>(p_env->GetObjectArrayElement(p_names, i));
				jbyteArray data = static_cast<jbyteArray>(p_env->GetObjectArrayElement(p_data, i));
				if (!data) {
					throw std::runtime_error("Missing restore bytes.");
				}
				jsize size = p_env->GetArrayLength(data);
				total += size;
				if (total > 16 * 1024 * 1024) {
					throw std::runtime_error("Restore exceeds the size limit.");
				}
				Android_SaveFile file{FromJava(p_env, name), std::vector<uint8_t>(size)};
				p_env->GetByteArrayRegion(data, 0, size, reinterpret_cast<jbyte*>(file.m_bytes.data()));
				p_env->DeleteLocalRef(name);
				p_env->DeleteLocalRef(data);
				if (p_env->ExceptionCheck()) {
					return nullptr;
				}
				files.push_back(std::move(file));
			}
		}
		Android_SaveRestore(g_restoreRoot).Schedule(path, files, p_previous);
		g_restoreQuit = true;
		return nullptr;
	}
	catch (const std::exception& error) {
		// A failed directory sync can follow a published request. Do not resume the
		// old game with that confirmed request outstanding.
		try {
			if (Android_SaveRestore(g_restoreRoot).Pending()) {
				g_restoreQuit = true;
			}
		}
		catch (...) {
			g_restoreQuit = true;
		}
		return p_env->NewStringUTF(error.what());
	}
}
