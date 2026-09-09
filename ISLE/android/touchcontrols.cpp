#include "touchcontrols.h"

#include "../isleapp.h"

#include <array>
#include <atomic>
#include <jni.h>
#include <mutex>

// Copied by the UI thread, never used there to access game or renderer objects.
// The float layout is mirrored by TouchControlsView's named indices.
static std::array<jfloat, 12> g_touchSnapshot{};
static std::mutex g_touchMutex;
static jlong g_touchRevision = 0;
static TouchActions g_touchActions;
static bool g_touchUiActive = false;
static std::atomic<bool> g_touchResetRequested{false};
static bool g_touchControlsEnabled = true;
static bool g_touchPreferred = true;

void Android_ConfigureTouchControls(bool p_enabled)
{
	g_touchControlsEnabled = p_enabled;
	g_touchPreferred = true;
	Android_ClearTouchControls();
}

void Android_ObserveTouchControlsInput(const SDL_Event& p_event, bool p_mouseWarped)
{
	switch (p_event.type) {
	case SDL_EVENT_FINGER_DOWN:
	case SDL_EVENT_FINGER_MOTION:
	case SDL_EVENT_FINGER_UP:
		if (p_event.tfinger.touchID != SDL_MOUSE_TOUCHID) {
			g_touchPreferred = true;
		}
		break;
	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
		g_touchPreferred = false;
		break;
	case SDL_EVENT_GAMEPAD_AXIS_MOTION:
		if (p_event.gaxis.value < -8000 || p_event.gaxis.value > 8000) {
			g_touchPreferred = false;
		}
		break;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		if (p_event.button.which != SDL_TOUCH_MOUSEID) {
			g_touchPreferred = false;
		}
		break;
	case SDL_EVENT_MOUSE_MOTION:
		if (!p_mouseWarped && p_event.motion.which != SDL_TOUCH_MOUSEID &&
			(p_event.motion.xrel != 0 || p_event.motion.yrel != 0)) {
			g_touchPreferred = false;
		}
		break;
	}
}

bool Android_TakeTouchControlsReset()
{
	return g_touchResetRequested.exchange(false);
}

void Android_ClearTouchControls()
{
	std::lock_guard<std::mutex> lock(g_touchMutex);
	g_touchSnapshot = {};
	g_touchActions.Invalidate();
	++g_touchRevision;
}

void Android_PublishTouchControls(
	SDL_Window* p_window,
	bool p_visible,
	int p_scheme,
	int p_contentWidth,
	int p_contentHeight,
	const TouchMovement::State& p_movement
)
{
	std::array<jfloat, 12> snapshot{};
	if (p_visible && g_touchControlsEnabled && g_touchPreferred && p_window &&
		(p_scheme == LegoInputManager::e_arrowKeys || p_scheme == LegoInputManager::e_gamepad)) {
		int width, height;
		IDirect3DRMMiniwinDevice* device = GetD3DRMMiniwinDevice();
		if (device) {
			Sint32 left, top, right, bottom;
			if (SDL_GetWindowSize(p_window, &width, &height) && width > 0 && height > 0 &&
				device->ConvertRenderToWindowCoordinates(0, 0, left, top) &&
				device->ConvertRenderToWindowCoordinates(p_contentWidth, p_contentHeight, right, bottom) &&
				right > left && bottom > top) {
				snapshot = {
					1.0f,
					static_cast<float>(p_scheme),
					left / static_cast<float>(width),
					top / static_cast<float>(height),
					right / static_cast<float>(width),
					bottom / static_cast<float>(height),
					p_movement.stickActive ? 1.0f : 0.0f,
					p_movement.origin.x,
					p_movement.origin.y,
					p_movement.axes.x,
					p_movement.axes.y,
					static_cast<float>(p_movement.directions)
				};
			}
			device->Release();
		}
	}
	std::lock_guard<std::mutex> lock(g_touchMutex);
	// A UI lifecycle change invalidates even a sample that was already being built.
	if (!g_touchUiActive || g_touchResetRequested.load()) {
		snapshot = {};
	}
	g_touchActions.SetAvailable(snapshot[0] != 0);
	if (g_touchSnapshot != snapshot) {
		g_touchSnapshot = snapshot;
		++g_touchRevision;
	}
}

extern "C" JNIEXPORT void JNICALL
Java_org_legoisland_isle_TouchControlsView_setNativeActive(JNIEnv*, jclass, jboolean p_active)
{
	std::lock_guard<std::mutex> lock(g_touchMutex);
	g_touchUiActive = p_active;
	g_touchResetRequested.store(true);
	g_touchSnapshot = {};
	g_touchActions.Invalidate();
	++g_touchRevision;
}

extern "C" JNIEXPORT jlong JNICALL Java_org_legoisland_isle_TouchControlsView_readNativeState(
	JNIEnv* p_env,
	jclass,
	jfloatArray p_output,
	jlongArray p_generation
)
{
	if (!p_output || !p_generation || p_env->GetArrayLength(p_generation) != 1 ||
		p_env->GetArrayLength(p_output) != static_cast<jsize>(g_touchSnapshot.size())) {
		return -1;
	}
	std::lock_guard<std::mutex> lock(g_touchMutex);
	p_env->SetFloatArrayRegion(p_output, 0, g_touchSnapshot.size(), g_touchSnapshot.data());
	jlong generation = g_touchActions.Generation();
	p_env->SetLongArrayRegion(p_generation, 0, 1, &generation);
	return g_touchRevision;
}

extern "C" JNIEXPORT void JNICALL
Java_org_legoisland_isle_TouchControlsView_submitAction(JNIEnv*, jclass, jint p_action, jlong p_generation)
{
	std::lock_guard<std::mutex> lock(g_touchMutex);
	g_touchActions.Submit(p_action, p_generation);
}

TouchActions::Action Android_TakeTouchAction()
{
	std::lock_guard<std::mutex> lock(g_touchMutex);
	return g_touchActions.Take();
}
