#include "settings.h"

#include "activity.h"
#include "configstore.h"
#include "mxdirectx/legodxinfo.h"

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <jni.h>

static std::string g_settingsPath;
static std::vector<std::string> g_renderers;
static std::atomic<bool> g_menuRequested{false};

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

void Android_ShowStartupSettings(const char* p_error)
{
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
	jobjectArray result = p_env->NewObjectArray(p_values.size(), cls, nullptr);
	p_env->DeleteLocalRef(cls);
	for (size_t i = 0; result && i < p_values.size(); i++) {
		jstring value = p_env->NewStringUTF(p_values[i].c_str());
		p_env->SetObjectArrayElement(result, i, value);
		p_env->DeleteLocalRef(value);
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
