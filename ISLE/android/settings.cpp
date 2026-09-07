#include "settings.h"

#include "activity.h"
#include "configstore.h"
#include "mxdirectx/legodxinfo.h"
#include "savesnapshot.h"

#include <atomic>
#include <chrono>
#include <jni.h>
#include <mutex>

static std::string g_settingsPath;
static std::vector<std::string> g_renderers;
static std::atomic<bool> g_menuRequested{false};
static std::mutex g_exportMutex;
static Android_SaveSnapshot g_export;
static std::string g_exportId;
static std::string g_exportTime;
static int g_exportSaveResult;

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
	while (Android_CallActivityBooleanMethod("isStartupSettingsOpen")) {
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
