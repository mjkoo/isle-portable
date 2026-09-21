#define INITGUID

#include "isleapp.h"

#include "3dmanager/lego3dmanager.h"
#include "decomp.h"
#include "gamepadbindings.h"
#include "infocenter.h"
#include "legoanimationmanager.h"
#include "legobuildingmanager.h"
#include "legogamestate.h"
#include "legoinputmanager.h"
#include "legomain.h"
#include "legomodelpresenter.h"
#include "legopartpresenter.h"
#include "legosoundmanager.h"
#include "legoutils.h"
#include "legovideomanager.h"
#include "legoworldpresenter.h"
#include "misc.h"
#include "mxbackgroundaudiomanager.h"
#include "mxdirectx/mxdirect3d.h"
#include "mxdsaction.h"
#include "mxmisc.h"
#include "mxomnicreateflags.h"
#include "mxomnicreateparam.h"
#include "mxstreamer.h"
#include "mxticklemanager.h"
#include "mxtimer.h"
#include "mxtransitionmanager.h"
#include "mxutilities.h"
#include "mxvariabletable.h"
#include "outputgain.h"
#include "res/arrow_bmp.h"
#include "res/busy_bmp.h"
#include "res/isle_bmp.h"
#include "res/no_bmp.h"
#include "res/resource.h"
#include "roi/legoroi.h"
#include "tgl/d3drm/impl.h"
#include "viewmanager/viewmanager.h"

#include <array>
#include <extensions/multiplayer.h>
#include <extensions/thirdpersoncamera.h>
#include <memory>
#include <miniwin/miniwindevice.h>
#include <type_traits>
#include <vec.h>

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_revision.h>
#include <errno.h>
#include <iniparser.h>
#include <stdlib.h>
#include <time.h>

#ifdef __EMSCRIPTEN__
#include "emscripten/config.h"
#include "emscripten/events.h"
#include "emscripten/filesystem.h"
#include "emscripten/haptic.h"
#include "emscripten/messagebox.h"
#include "emscripten/window.h"
#endif

#ifdef __3DS__
#include "3ds/apthooks.h"
#include "3ds/config.h"
#endif

#ifdef __SWITCH__
#include "switch/config.h"

#include <switch.h>
#endif

#ifdef WINDOWS_STORE
#include "xbox_one_series/config.h"
#endif

#ifdef IOS
#include "ios/config.h"
#include "ios/filepicker.h"
#endif

#ifdef ANDROID
#include "android/activity.h"
#include "android/audiofocus.h"
#include "android/config.h"
#include "android/configstore.h"
#include "android/filepicker.h"
#include "android/quitprompt.h"
#include "android/settings.h"
#include "android/touchcontrols.h"
#include "android/touchinput.h"

static Android_TouchInput g_androidTouchInput;
#endif

#ifdef __vita__
#include "vita/config.h"
#include "vita/messagebox.h"

#include <psp2/appmgr.h>
#include <psp2/kernel/clib.h>
#endif

DECOMP_SIZE_ASSERT(IsleApp, 0x8c)

// GLOBAL: ISLE 0x410030
IsleApp* g_isle = NULL;

// GLOBAL: ISLE 0x410034
MxU8 g_mousedown = FALSE;

// GLOBAL: ISLE 0x410038
MxU8 g_mousemoved = FALSE;

// GLOBAL: ISLE 0x41003c
MxS32 g_closed = FALSE;

static char g_startupError[1024] = "";

#ifdef EXTENSIONS
// Set when the relay turns the session away, and acted on once Tick has returned.
static MxS32 g_multiplayerRejected = FALSE;
#endif

static GamepadBindings::Dispatcher g_gamepad(
#if defined(__vita__)
	GamepadBindings::e_platformVita
#elif defined(ANDROID)
	GamepadBindings::e_platformAndroid
#else
	GamepadBindings::e_platformDefault
#endif
);

// GLOBAL: ISLE 0x410050
MxS32 g_rmDisabled = FALSE;

// GLOBAL: ISLE 0x410054
MxS32 g_waitingForTargetDepth = TRUE;

// GLOBAL: ISLE 0x410058
MxS32 g_targetWidth = 640;

// GLOBAL: ISLE 0x41005c
MxS32 g_targetHeight = 480;

// GLOBAL: ISLE 0x410060
MxS32 g_targetDepth = 16;

// GLOBAL: ISLE 0x410064
MxS32 g_reqEnableRMDevice = FALSE;

// The one writer of the mixer's master volume. SDL thread only.
static OutputGain g_outputGain;

MxFloat g_lastJoystickMouseX = 0;
MxFloat g_lastJoystickMouseY = 0;
MxFloat g_lastMouseX = 320;
MxFloat g_lastMouseY = 240;
MxBool g_mouseWarped = FALSE;

bool g_dpadUp = false;
bool g_dpadDown = false;
bool g_dpadLeft = false;
bool g_dpadRight = false;

// STRING: ISLE 0x4101dc
#define WINDOW_TITLE "LEGO®"

SDL_Window* window;

extern const char* g_files[46];

// FUNCTION: ISLE 0x401000
IsleApp::IsleApp()
{
	m_hdPath = NULL;
	m_cdPath = NULL;
	m_deviceId = NULL;
	m_savePath = NULL;
	m_fullScreen = TRUE;
	m_flipSurfaces = FALSE;
	m_backBuffersInVram = TRUE;
	m_using8bit = FALSE;
	m_using16bit = TRUE;
	m_hasLightSupport = FALSE;
#ifdef __DJGPP__
	m_drawCursor = TRUE;
#else
	m_drawCursor = FALSE;
#endif
	m_use3dSound = TRUE;
	m_useMusic = TRUE;
	m_wideViewAngle = TRUE;
#ifdef __DJGPP__
	m_islandQuality = 1;
#else
	m_islandQuality = 2;
#endif
	m_islandTexture = 1;
	m_gameStarted = FALSE;
	m_frameDelta = 10;
	m_windowActive = TRUE;

#ifdef COMPAT_MODE
	{
		MxRect32 r(0, 0, 639, 479);
		MxVideoParamFlags flags;
		m_videoParam = MxVideoParam(r, NULL, 1, flags);
	}
#else
	m_videoParam = MxVideoParam(MxRect32(0, 0, 639, 479), NULL, 1, MxVideoParamFlags());
#endif
	m_videoParam.Flags().Set16Bit(MxDirectDraw::GetPrimaryBitDepth() == 16);

	m_windowHandle = NULL;
	m_cursorArrow = NULL;
	m_cursorBusy = NULL;
	m_cursorNo = NULL;
	m_cursorCurrent = NULL;
	m_cursorArrowBitmap = NULL;
	m_cursorBusyBitmap = NULL;
	m_cursorNoBitmap = NULL;
	m_cursorCurrentBitmap = NULL;

	LegoOmni::CreateInstance();

	m_mediaPath = NULL;
	m_iniPath = NULL;
	m_maxLod = RealtimeView::GetUserMaxLOD();
#ifdef __DJGPP__
	m_maxLod = 1.0f;
#endif
	m_maxAllowedExtras = m_islandQuality <= 1 ? 10 : 20;
	m_transitionType = MxTransitionManager::e_mosaic;
	m_cursorSensitivity = 4;
#ifdef ANDROID
	m_touchScheme = static_cast<LegoInputManager::TouchScheme>(Android_TouchSettings{}.m_scheme);
	g_androidTouchInput.Cancel();
#else
	m_touchScheme = LegoInputManager::e_gamepad;
#endif
	m_haptic = TRUE;
	m_wasd = FALSE;
#ifdef __DJGPP__
	m_xRes = 320;
	m_yRes = 200;
#else
	m_xRes = 640;
	m_yRes = 480;
#endif
	m_exclusiveXRes = m_xRes;
	m_exclusiveYRes = m_yRes;
	m_exclusiveFrameRate = 60.00f;
	m_frameRate = 100.0f;
	m_exclusiveFullScreen = FALSE;
	m_msaaSamples = 0;
	m_anisotropic = 16.0f;
	m_lightingModel = 0;
	m_activeInBackground = FALSE;
}

// FUNCTION: ISLE 0x4011a0
IsleApp::~IsleApp()
{
#ifdef ANDROID
	Android_EndTouchSettings();
#endif
	if (LegoOmni::GetInstance()) {
		if (m_gameStarted) {
			Close();
		}
		MxOmni::DestroyInstance();
	}

	SDL_free(m_hdPath);
	SDL_free(m_cdPath);
	SDL_free(m_deviceId);
	SDL_free(m_savePath);
	SDL_free(m_mediaPath);
}

// FUNCTION: ISLE 0x401260
void IsleApp::Close()
{
	MxDSAction ds;
	ds.SetUnknown24(-2);

	if (Lego()) {
		GameState()->Save(0);
		if (InputManager()) {
			InputManager()->QueueEvent(c_notificationKeyPress, 0, 0, 0, SDLK_SPACE);
		}

		VideoManager()->Get3DManager()->GetLego3DView()->GetViewManager()->RemoveAll(NULL);

		Lego()->RemoveWorld(ds.GetAtomId(), ds.GetObjectId());
		Lego()->DeleteObject(ds);
		TransitionManager()->SetWaitIndicator(NULL);
		Lego()->Resume();

		if (BackgroundAudioManager()) {
			BackgroundAudioManager()->Stop();
		}

		while (Streamer()->Close(NULL) == SUCCESS) {
		}

		while (Lego() && !Lego()->DoesEntityExist(ds)) {
			Timer()->GetRealTime();
			TickleManager()->Tickle();
		}
	}
}

// FUNCTION: ISLE 0x4013b0
MxS32 IsleApp::SetupLegoOmni()
{
	MxS32 result = FALSE;

#ifdef COMPAT_MODE
	MxS32 failure;
	{
		MxOmniCreateParam param(m_mediaPath, m_windowHandle, m_videoParam, MxOmniCreateFlags());
		failure = Lego()->Create(param) == FAILURE;
	}
#else
	MxS32 failure =
		Lego()->Create(MxOmniCreateParam(m_mediaPath, m_windowHandle, m_videoParam, MxOmniCreateFlags())) == FAILURE;
#endif

	if (!failure) {
		VariableTable()->SetVariable("ACTOR_01", "");
		TickleManager()->SetClientTickleInterval(VideoManager(), 10);
		result = TRUE;
	}

	return result;
}

// FUNCTION: ISLE 0x401560
void IsleApp::SetupVideoFlags(
	MxS32 fullScreen,
	MxS32 flipSurfaces,
	MxS32 backBuffers,
	MxS32 using8bit,
	MxS32 using16bit,
	MxS32 hasLightSupport,
	MxS32 param_7,
	MxS32 wideViewAngle,
	char* deviceId
)
{
	m_videoParam.Flags().SetFullScreen(fullScreen);
	m_videoParam.Flags().SetFlipSurfaces(flipSurfaces);
	m_videoParam.Flags().SetBackBuffers(!backBuffers);
	m_videoParam.Flags().SetLacksLightSupport(!hasLightSupport);
	m_videoParam.Flags().SetF1bit7(param_7);
	m_videoParam.Flags().SetWideViewAngle(wideViewAngle);
	m_videoParam.Flags().SetEnabled(TRUE);
	m_videoParam.SetDeviceName(deviceId);
	if (using8bit) {
		m_videoParam.Flags().Set16Bit(0);
	}
	if (using16bit) {
		m_videoParam.Flags().Set16Bit(1);
	}
}

static void ShowFatalError(const char* p_message)
{
#ifdef ANDROID
	Android_ClearTouchControls();
#endif
	if (g_isle) {
		IsleApp* isle = g_isle;
		g_isle = NULL;
		delete isle;
	}
	if (window) {
		SDL_DestroyWindow(window);
		window = NULL;
	}
	Any_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "LEGO® Island Error", p_message, NULL);
}

#if defined(ANDROID) || defined(IOS) || defined(__EMSCRIPTEN__)
// Persist progress at the points where the platform may destroy the process without
// running a normal shutdown. Cheap and idempotent: LegoGameState::Save writes nothing
// until the player has registered.
static bool SaveGameStateForLifecycleEvent(const char* p_reason)
{
	// Lego() has to be checked before GameState(), which only asserts on a missing
	// LegoOmni and so dereferences NULL in release builds.
	if (!g_isle || !g_isle->GetGameStarted() || !Lego() || !GameState()) {
		return false;
	}

	// LegoGameState::Save reports success without writing anything until the player has
	// registered, so this says what was requested rather than claiming a file was written.
	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Saving game state (%s)", p_reason);

	if (GameState()->Save(0) != SUCCESS) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to save game state (%s)", p_reason);
		return false;
	}

	return true;
}

#ifdef ANDROID
static void CancelInputForQuitPrompt();
static bool g_androidBackgrounded = false;
static bool g_androidLowMemorySaveAttempted = false;
#endif

static bool SDLCALL LifecycleEventWatch(void* p_userdata, SDL_Event* p_event)
{
	switch (p_event->type) {
	case SDL_EVENT_DID_ENTER_BACKGROUND:
#ifdef ANDROID
		if (!g_androidBackgrounded) {
			g_androidLowMemorySaveAttempted = false;
		}
		g_androidBackgrounded = true;
		CancelInputForQuitPrompt();
#endif
		// Deliberately not WILL_ENTER_BACKGROUND. On Android both fire back to back
		// before the SDL thread blocks, so DID is still early enough, and by then the
		// window events that SDL_OnApplicationWillEnterBackground queues ahead of WILL
		// have been drained into SDL_AppEvent. On iOS, WILL is sceneWillResignActive:,
		// which also fires for notification banners and the control centre.
		SaveGameStateForLifecycleEvent("backgrounded");
		break;
#ifdef ANDROID
	case SDL_EVENT_WILL_ENTER_FOREGROUND:
		g_androidBackgrounded = false;
		break;
	case SDL_EVENT_LOW_MEMORY:
		// SDL forwards every Android trim level, including routine foreground memory
		// pressure. Do not turn those notifications into synchronous saves during play.
		SDL_LogWarn(
			SDL_LOG_CATEGORY_APPLICATION,
			"Low memory (%s)",
			g_androidBackgrounded ? "background" : "foreground"
		);
		if (g_androidBackgrounded && !g_androidLowMemorySaveAttempted) {
			// Mark the attempt first: saving can re-enter the event watchers, and even a
			// failed save must not cause repeated writes during the same background interval.
			g_androidLowMemorySaveAttempted = true;
			SaveGameStateForLifecycleEvent("low memory");
		}
		break;
#endif
	case SDL_EVENT_TERMINATING:
		SaveGameStateForLifecycleEvent("terminating");
		break;
	default:
		break;
	}

	// Watchers cannot suppress an event; only an SDL_SetEventFilter callback can.
	return true;
}
#endif

// Plays the game at the volume its pause state and the system between them call for.
//
// Polled rather than hung off each Pause()/Resume(), because LEGO1 pauses itself too: the Pause
// key goes through LegoNavController, which no call site here can see.
static void ApplyOutputGain()
{
#ifdef ANDROID
	float focus;
	if (Android_TakeAudioFocusGain(&focus)) {
		// What the system asked for, which is not what the game plays at while it is paused.
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Audio focus changed to gain %.2f", focus);
		g_outputGain.SetFocus(focus);
	}
#endif

	// Taking the gain is what records it as applied, so nothing may be taken before there is an
	// engine to apply it to. A focus change that arrives first waits in the gain above instead.
	if (!Lego() || !SoundManager()) {
		return;
	}

	g_outputGain.SetPaused(Lego()->IsPaused());

	float gain;
	if (g_outputGain.Take(&gain)) {
		// What the game plays at, as against what the system asked for: a pause is not reported
		// anywhere else, and the two part company whenever one outlives the other.
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Playing at gain %.2f", gain);
		SoundManager()->SetOutputGain(gain);
	}
}

// Shuts the game down. ~IsleApp runs IsleApp::Close, which saves and then tickles the world
// down, so a quit has to come through here: returning SDL_APP_SUCCESS on its own skips all of
// it, since SDL_AppQuit never touches g_isle.
//
// Clear the global first: ~IsleApp tickles the game while it shuts down, so a lifecycle event
// arriving meanwhile must not find a half-destructed IsleApp.
static void CloseGame()
{
#ifdef ANDROID
	Android_ClearTouchControls();
#endif
	if (g_closed) {
		return;
	}

	IsleApp* isle = g_isle;
	g_isle = NULL;
	g_closed = TRUE;
	delete isle;
}

#ifdef ANDROID
static bool g_confirmingQuit = false;
static Uint64 g_quitPromptInputCutoff = 0;

static bool GameAbandoned()
{
	return g_closed;
}

// Whether a save would actually write a file. LegoGameState::Save reports success without
// writing anything until the player has registered, which is fine for a lifecycle save but not
// for telling the player their game is safe. Asks the same question Save does.
static bool GameStateIsSaveable()
{
	if (!GameState()) {
		return false;
	}

	InfocenterState* infocenterState = (InfocenterState*) GameState()->GetState("InfocenterState");
	return infocenterState && infocenterState->HasRegistered();
}

// The prompt consumes releases, so start the next interaction from neutral input state.
static void CancelInputForQuitPrompt()
{
	Android_ClearTouchControls();
	g_androidTouchInput.Cancel();
	g_mousedown = FALSE;
	g_mousemoved = FALSE;
	g_lastJoystickMouseX = 0;
	g_lastJoystickMouseY = 0;
	g_dpadUp = g_dpadDown = g_dpadLeft = g_dpadRight = false;
	g_gamepad.Cancel();

	if (g_isle && Lego() && InputManager()) {
		InputManager()->CancelPointerInput();
	}
	if (window) {
		SDL_SetWindowRelativeMouseMode(window, false);
	}
}

// The back button, as a decision rather than an accident: pause, save, and ask.
static SDL_AppResult HandleBackButton()
{
	// Coalesce a touch-menu request with Back if both arrive in the same event batch.
	Android_TakeMenuRequest();
	if (!g_isle->GetGameStarted()) {
		// Nothing to save and nothing to lose, so keep the platform's own meaning for the
		// button rather than making it inert while the game starts up.
		CloseGame();
		return SDL_APP_SUCCESS;
	}

	// Only undo what this does: the focus-lost path pauses too, and resuming a game the player
	// alt-tabbed away from would be wrong.
	bool pausedHere = Lego() && !Lego()->IsPaused();
	if (pausedHere) {
		Lego()->Pause();
	}

	// The prompt below loops here rather than returning to SDL_AppIterate, so the pause has to
	// reach the mixer from this side; left to the pump it would not arrive until the player had
	// answered.
	ApplyOutputGain();

	// Before the prompt rather than after the answer, so the dialog can say what actually
	// happened. Android can reclaim the process at any point while a prompt is up, and the save
	// is cheap and idempotent, so paying it on a cancelled press is the better trade.
	QuitPromptSaveResult saveResult;
	if (!GameStateIsSaveable()) {
		// Nothing was written, and saying so is not the same as reporting a failure.
		SaveGameStateForLifecycleEvent("back button");
		saveResult = e_quitPromptNothingToSave;
	}
	else {
		// Save does not propagate every serialization or close failure, so success only
		// establishes that a save was attempted, not that the entire file was written.
		saveResult = SaveGameStateForLifecycleEvent("back button") ? e_quitPromptSaveAttempted : e_quitPromptSaveFailed;
	}

	g_confirmingQuit = true;
	CancelInputForQuitPrompt();
	// Capture before pumping or opening Settings: lifecycle saves may run after either.
	Android_CaptureSaveExport(g_isle->GetSavePath(), saveResult);
	bool quit = Android_ConfirmQuit(GameAbandoned, saveResult);
	Android_ClearSaveExport();
	// SDL may already have removed input into a dispatch batch before entering this callback.
	// Flushing cannot reach that batch; reject its old events when dispatch resumes as well.
	g_quitPromptInputCutoff = SDL_GetTicksNS();
	CancelInputForQuitPrompt();
	Android_TakeMenuRequest();
	g_confirmingQuit = false;
	Android_TouchSettings touch;
	if (!quit && !g_closed && g_isle && g_isle->GetGameStarted() && Android_TakeTouchSettings(touch)) {
		g_isle->SetTouchScheme(static_cast<LegoInputManager::TouchScheme>(touch.m_scheme));
		Android_UpdateTouchControls(touch.m_visible);
	}
	GamepadBindings::Table gamepad;
	if (!quit && !g_closed && g_isle && g_isle->GetGameStarted() && Android_TakeGamepadSettings(gamepad)) {
		g_gamepad.SetTable(gamepad);
	}

	// Resume even when quitting. IsleApp::Close queues a keypress that the input manager drops
	// while the game is paused, and Close only resumes after it. Both are checked again because
	// the prompt pumps, and the game can be torn down while it is up.
	if (pausedHere && Lego() && (!g_androidBackgrounded || quit)) {
		Lego()->Resume();
	}

	// The other half of the pair above: the sound comes back before the prompt's caller does.
	ApplyOutputGain();

	if (quit) {
		CloseGame();
		return SDL_APP_SUCCESS;
	}

	return SDL_APP_CONTINUE;
}
#endif

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv)
{
	*appstate = NULL;

	{
		int version = SDL_GetVersion();
		SDL_Log(
			"SDL version %d.%d.%d (%s)",
			SDL_VERSIONNUM_MAJOR(version),
			SDL_VERSIONNUM_MINOR(version),
			SDL_VERSIONNUM_MICRO(version),
			SDL_GetRevision()
		);
	}

	SDL_SetAppMetadata("Isle Portable", NULL, "org.legoisland.Isle");

	SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
	SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
#ifdef ANDROID
	// SDL derives the activity orientation from the window's resizable flag and calls
	// setRequestedOrientation(), which overrides android:screenOrientation from the manifest.
	// Ask for landscape here so the game stays landscape whatever the display is doing.
	SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");

	// Stop the back button finishing the activity behind the game's back. Android would
	// otherwise destroy it and give the whole teardown - a save, then tickling the world down -
	// the one second SDLActivity.onDestroy waits before it gives up on the SDL thread. Trapped,
	// the press arrives as SDL_SCANCODE_AC_BACK and the quit becomes the player's decision.
	//
	// Set before SDL_Init deliberately: SDL registers the hint's callback during video init and
	// SDL_AddHintCallback fires it as it registers, so the trap is in place before SetupWindow,
	// which can sit in the import prompt for minutes.
	SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
#endif
#ifdef __DJGPP__
	SDL_SetHint("SDL_DOS_ALLOW_DIRECT_FRAMEBUFFER", "1");
#endif

	Uint32 initFlags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD;
#ifndef __DJGPP__
	initFlags |= SDL_INIT_HAPTIC;
#endif

	if (!SDL_Init(initFlags)) {
		char buffer[256];
		SDL_snprintf(
			buffer,
			sizeof(buffer),
			"\"LEGO® Island\" failed to start.\nPlease quit all other applications and try again.\nSDL error: %s",
			SDL_GetError()
		);
		Any_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "LEGO® Island Error", buffer, NULL);
		return SDL_APP_FAILURE;
	}

	// [library:window]
	// Original game checks for an existing instance here.
	// We don't really need that.

#ifdef ANDROID
	// Both change what the engine reads, so both finish before it exists.
	Android_ApplyGameFilesBeforeStartup();
	if (!Android_RestoreBeforeStartup()) {
		return SDL_APP_FAILURE;
	}
#endif

	// Create global app instance
	g_isle = new IsleApp();

#if defined(ANDROID) || defined(IOS) || defined(__EMSCRIPTEN__)
	// SDL never queues the app lifecycle events; SDL_SendAppEvent hands them straight to
	// the event watchers. SDL's own watcher does forward them into SDL_AppEvent, but it
	// drains the event queue first, so on Android the SDL_EVENT_QUIT that Android_OnDestroy
	// queues just before SDL_EVENT_TERMINATING has already deleted g_isle by the time
	// SDL_AppEvent sees the terminate. SDL registers that watcher only after SDL_AppInit
	// returns, so registering ours here puts it ahead in the list, where it still sees a
	// live game. Do not move this registration out of SDL_AppInit, and keep it ahead of
	// SetupWindow(), which can sit in the Android import prompt for minutes.
	//
	// This is deliberately never removed. SDL_Quit frees the watch list, and removing a
	// watch from inside a dispatch would compact the list while the outer dispatch is
	// still walking it, which saving can trigger: Save emits e_saveSlotWritten, and that
	// SDL_PushEvent re-enters the watch list.
#ifdef ANDROID
	g_androidBackgrounded = false;
	g_androidLowMemorySaveAttempted = false;
#endif
	SDL_AddEventWatch(LifecycleEventWatch, NULL);
#endif

#ifdef __vita__
	SceAppUtilInitParam appUtilInitParam = {0};
	SceAppUtilBootParam appUtilBootParam = {0};
	sceAppUtilInit(&appUtilInitParam, &appUtilBootParam);
	SceAppUtilAppEventParam eventParam = {0};
	sceAppUtilReceiveAppEvent(&eventParam);
	if (eventParam.type == 0x05) {
		g_isle->LoadConfig();
		char buffer[2048];
		sceAppUtilAppEventParseLiveArea(&eventParam, buffer);
		if (strstr(buffer, "-config")) {
			sceClibPrintf("Loading Config App.\n");
			sceAppMgrLoadExec("app0:/isle-config.self", NULL, NULL);
		}
	}
#endif

	switch (g_isle->ParseArguments(argc, argv)) {
	case SDL_APP_FAILURE:
		Any_ShowSimpleMessageBox(
			SDL_MESSAGEBOX_ERROR,
			"LEGO® Island Error",
			"\"LEGO® Island\" failed to start.  Invalid CLI arguments.",
			window
		);
		return SDL_APP_FAILURE;
	case SDL_APP_SUCCESS:
		return SDL_APP_SUCCESS;
	case SDL_APP_CONTINUE:
		break;
	}

	// Create window
	if (g_isle->SetupWindow() != SUCCESS) {
#ifdef ANDROID
		Android_ShowStartupSettings(
			g_startupError[0] != '\0'
				? g_startupError
				: "\"LEGO® Island\" failed to start.\nPlease quit all other applications and try again."
				  "\nFailed to initialize; see logs for details",
			g_isle->GetSavePath()
		);
#else
		ShowFatalError(
			g_startupError[0] != '\0'
				? g_startupError
				: "\"LEGO® Island\" failed to start.\nPlease quit all other applications and try again."
				  "\nFailed to initialize; see logs for details"
		);
#endif
		return SDL_APP_FAILURE;
	}

	// Get reference to window
	*appstate = g_isle->GetWindowHandle();

#ifdef __3DS__
	N3DS_SetupAptHooks();
#endif
	return SDL_APP_CONTINUE;
}

#ifdef ANDROID
static void PublishTouchControls()
{
	bool visible = g_isle && g_isle->GetGameStarted() && !g_closed && !g_confirmingQuit && !g_androidBackgrounded &&
				   Lego() && !Lego()->IsPaused() && InputManager() && window &&
				   (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS);
	TouchMovement::State movement;
	if (visible) {
		movement = InputManager()->GetTouchMovementState();
	}
	Android_PublishTouchControls(
		window,
		visible,
		g_isle ? g_isle->GetTouchScheme() : -1,
		g_targetWidth,
		g_targetHeight,
		movement
	);
}
#endif

SDL_AppResult SDL_AppIterate(void* appstate)
{
#ifdef ANDROID
	if (Android_TakeTouchControlsReset()) {
		CancelInputForQuitPrompt();
	}
	if (Android_TakeMenuRequest() && g_isle && g_isle->GetGameStarted() && !g_closed && !g_confirmingQuit) {
		return HandleBackButton();
	}
#endif

#ifdef ANDROID
	// Refresh readiness before each request: an earlier action may have changed the game state.
	for (int i = 0; i < 16; ++i) {
		PublishTouchControls();
		TouchActions::Action action = Android_TakeTouchAction();
		if (action == TouchActions::e_none) {
			break;
		}
		SDL_Keycode key = action == TouchActions::e_space ? SDLK_SPACE : SDLK_ESCAPE;
		InputManager()->QueueEvent(c_notificationKeyPress, key, 0, 0, key);
	}
#endif

	if (g_closed) {
		return SDL_APP_SUCCESS;
	}

	if (!g_isle->Tick()) {
		ShowFatalError("\"LEGO® Island\" failed to start.\nPlease quit all other applications and try again."
					   "\nFailed to initialize; see logs for details");
		return SDL_APP_FAILURE;
	}

#ifdef EXTENSIONS
	// Tick could not do this itself without deleting the IsleApp it was running in. Closing through
	// CloseGame saves the game first, which setting g_closed by hand would have skipped.
	if (g_multiplayerRejected) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Multiplayer session rejected, closing");
		CloseGame();
		return SDL_APP_SUCCESS;
	}
#endif

	if (!g_closed) {
		if (g_reqEnableRMDevice) {
			g_reqEnableRMDevice = FALSE;
			VideoManager()->EnableRMDevice();
			g_rmDisabled = FALSE;
			Lego()->Resume();
		}

		if (g_closed) {
			return SDL_APP_SUCCESS;
		}

		if (g_mousedown && g_mousemoved && g_isle) {
			if (!g_isle->Tick()) {
				ShowFatalError("\"LEGO® Island\" failed to start.\nPlease quit all other applications and try again."
							   "\nFailed to initialize; see logs for details");
				return SDL_APP_FAILURE;
			}
		}

		if (g_mousemoved) {
			g_mousemoved = FALSE;
		}

		g_isle->MoveVirtualMouseViaJoystick();
	}

#ifdef ANDROID
	PublishTouchControls();
#endif

	// After the tick rather than before it, because two of the things that pause the game do so
	// from inside one: LEGO1's own Pause key, and the resume that follows re-enabling the renderer.
	// Taken at the top they would each be a frame late. The paths that return before this one
	// either end the game or apply the gain themselves.
	ApplyOutputGain();

	return SDL_APP_CONTINUE;
}

static SDL_GamepadButton GetGamepadClickButton(SDL_JoystickID p_joystickID)
{
	SDL_Gamepad* gamepad = SDL_GetGamepadFromID(p_joystickID);
	if (gamepad && SDL_GetGamepadButtonLabel(gamepad, SDL_GAMEPAD_BUTTON_EAST) == SDL_GAMEPAD_BUTTON_LABEL_A) {
		return SDL_GAMEPAD_BUTTON_EAST;
	}
	return SDL_GAMEPAD_BUTTON_SOUTH;
}

static void HandleGamepadAction(GamepadBindings::Result p_result)
{
	switch (p_result.m_action) {
	case GamepadBindings::e_click:
		g_mousedown = p_result.m_pressed ? TRUE : FALSE;
		if (InputManager()) {
			InputManager()->QueueEvent(
				p_result.m_pressed ? c_notificationButtonDown : c_notificationButtonUp,
				LegoEventNotificationParam::c_lButtonState,
				g_lastMouseX,
				g_lastMouseY,
				0
			);
		}
		break;
	case GamepadBindings::e_space:
		if (InputManager()) {
			InputManager()->QueueEvent(c_notificationKeyPress, SDLK_SPACE, 0, 0, SDLK_SPACE);
		}
		break;
	case GamepadBindings::e_escape:
		if (InputManager()) {
			InputManager()->QueueEvent(c_notificationKeyPress, SDLK_ESCAPE, 0, 0, SDLK_ESCAPE);
		}
		break;
	case GamepadBindings::e_pause:
		if (InputManager()) {
			InputManager()->QueueEvent(c_notificationKeyPress, 0, 0, 0, SDLK_PAUSE);
		}
		break;
#ifdef ANDROID
	case GamepadBindings::e_menu:
		Android_RequestMenu();
		break;
#endif
	default:
		break;
	}
}

static void HandleGamepadButton(const SDL_GamepadButtonEvent& p_event, bool p_down)
{
	GamepadBindings::Input input;
	if (GamepadBindings::FromButton(static_cast<SDL_GamepadButton>(p_event.button), input)) {
		bool eastIsA = GetGamepadClickButton(p_event.which) == SDL_GAMEPAD_BUTTON_EAST;
		HandleGamepadAction(g_gamepad.Button(p_event.which, input, p_down, eastIsA));
	}
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
	if (!g_isle) {
		return SDL_APP_CONTINUE;
	}

#ifdef ANDROID
	// Pumping can dispatch input inline during a lifecycle event, including another Back.
	// After the dialog, discard any input SDL had already copied into its dispatch batch.
	if (Android_IsInputEvent(event->type) &&
		(g_confirmingQuit || (g_quitPromptInputCutoff && event->common.timestamp <= g_quitPromptInputCutoff))) {
		return SDL_APP_CONTINUE;
	}

	// AC_BACK is the system back button, not a game key, and it is handled ahead of
	// UpdateLastInputMethod because a key event there makes the keyboard the last input method,
	// which switches the touch gamepad's virtual stick off until the next finger event.
	if ((event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP) && event->key.key == SDLK_AC_BACK) {
		if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
			return HandleBackButton();
		}

		// The repeat and the key-up are nothing to act on, and on API 33 and up the up arrives
		// half a second after the down, well after the prompt has been answered.
		return SDL_APP_CONTINUE;
	}
#endif

#ifdef ANDROID
	if (Android_TakeTouchControlsReset()) {
		CancelInputForQuitPrompt();
	}
	if ((event->type == SDL_EVENT_FINGER_DOWN || event->type == SDL_EVENT_FINGER_MOTION ||
		 event->type == SDL_EVENT_FINGER_UP || event->type == SDL_EVENT_FINGER_CANCELED) &&
		!g_androidTouchInput.Accept(event->tfinger)) {
		return SDL_APP_CONTINUE;
	}
	Android_ObserveTouchControlsInput(*event, g_mouseWarped);
	struct PublishAfterEvent {
		~PublishAfterEvent() { PublishTouchControls(); }
	} publishAfterEvent;
	if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || event->type == SDL_EVENT_WINDOW_FOCUS_LOST) {
		CancelInputForQuitPrompt();
	}
#endif

	if (InputManager()) {
		InputManager()->UpdateLastInputMethod(event);
	}

	switch (event->type) {
	case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
	case SDL_EVENT_MOUSE_MOTION:
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP:
	case SDL_EVENT_FINGER_MOTION:
	case SDL_EVENT_FINGER_DOWN:
	case SDL_EVENT_FINGER_UP:
	case SDL_EVENT_FINGER_CANCELED:
		IDirect3DRMMiniwinDevice* device = GetD3DRMMiniwinDevice();
		if (device) {
			bool converted = device->ConvertEventToRenderCoordinates(event);
			device->Release();
			if (!converted) {
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Rendering failed: %s", SDL_GetError());
#ifdef ANDROID
				if (Lego()) {
					Lego()->Pause();
					ApplyOutputGain();
				}
				bool saved = SaveGameStateForLifecycleEvent("render failure");
				g_confirmingQuit = true;
				CancelInputForQuitPrompt();
				Android_ShowStartupSettings(
					"Could not resize the game's render target. Choose a lower resolution or "
					"another renderer in Settings, "
					"then close and relaunch the game.",
					g_isle->GetSavePath(),
					saved ? e_quitPromptSaveAttempted : e_quitPromptSaveFailed
				);
#else
				ShowFatalError("Could not resize the game's render target. See logs for details.");
#endif
				return SDL_APP_FAILURE;
			}
		}

#ifdef __EMSCRIPTEN__
		Emscripten_ConvertEventToRenderCoordinates(event);
#endif
		break;
	}

#ifdef __vita__
	// reject back touch panel
	switch (event->type) {
	case SDL_EVENT_FINGER_MOTION:
	case SDL_EVENT_FINGER_DOWN:
	case SDL_EVENT_FINGER_UP:
	case SDL_EVENT_FINGER_CANCELED:
		if (event->tfinger.touchID == 2) {
			return SDL_APP_CONTINUE;
		}
	}
#endif

	switch (event->type) {
	case SDL_EVENT_WINDOW_FOCUS_GAINED:
		if (!g_isle->GetActiveInBackground()) {
			g_isle->SetWindowActive(TRUE);
#ifdef ANDROID
			if (!g_confirmingQuit) {
				Lego()->Resume();
			}
#else
			Lego()->Resume();
#endif
		}
		break;
	case SDL_EVENT_WINDOW_FOCUS_LOST:
		if (!g_isle->GetActiveInBackground() && g_isle->GetGameStarted()) {
			g_isle->SetWindowActive(FALSE);
			Lego()->Pause();
#ifdef __EMSCRIPTEN__
			// Emscripten only: the browser has no background event, and on desktop this
			// fires on every alt-tab.
			SaveGameStateForLifecycleEvent("focus lost");
#endif
		}
		break;
	case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
	case SDL_EVENT_QUIT:
		CloseGame();
		break;
	case SDL_EVENT_KEY_DOWN: {
		if (event->key.repeat) {
			break;
		}

		SDL_Keycode keyCode = event->key.key;

		if ((event->key.mod & SDL_KMOD_LALT) && keyCode == SDLK_RETURN) {
			SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN));
		}
		else {
			if (InputManager()) {
				InputManager()->QueueEvent(c_notificationKeyPress, keyCode, 0, 0, keyCode);
			}
		}
		break;
	}
	case SDL_EVENT_KEYBOARD_ADDED:
		if (InputManager()) {
			InputManager()->AddKeyboard(event->kdevice.which);
		}
		break;
	case SDL_EVENT_KEYBOARD_REMOVED:
		if (InputManager()) {
			InputManager()->RemoveKeyboard(event->kdevice.which);
		}
		break;
	case SDL_EVENT_MOUSE_ADDED:
		if (InputManager()) {
			InputManager()->AddMouse(event->mdevice.which);
		}
		break;
	case SDL_EVENT_MOUSE_REMOVED:
		if (InputManager()) {
			InputManager()->RemoveMouse(event->mdevice.which);
		}
		break;
	case SDL_EVENT_GAMEPAD_ADDED:
		if (InputManager()) {
			InputManager()->AddJoystick(event->jdevice.which);
		}
		break;
	case SDL_EVENT_GAMEPAD_REMOVED:
		g_gamepad.Removed(event->gdevice.which);
		if (InputManager()) {
			InputManager()->RemoveJoystick(event->jdevice.which);
		}
		break;
	case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
		switch (event->gbutton.button) {
		case SDL_GAMEPAD_BUTTON_DPAD_UP:
			g_dpadUp = true;
			break;
		case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
			g_dpadDown = true;
			break;
		case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
			g_dpadLeft = true;
			break;
		case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
			g_dpadRight = true;
			break;
		default:
			HandleGamepadButton(event->gbutton, true);
			break;
		}
		break;
	}

	case SDL_EVENT_GAMEPAD_BUTTON_UP: {
		switch (event->gbutton.button) {
		case SDL_GAMEPAD_BUTTON_DPAD_UP:
			g_dpadUp = false;
			break;
		case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
			g_dpadDown = false;
			break;
		case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
			g_dpadLeft = false;
			break;
		case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
			g_dpadRight = false;
			break;
		default:
			HandleGamepadButton(event->gbutton, false);
			break;
		}
		break;
	}
	case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
		MxS16 axisValue = 0;
		if (event->gaxis.value < -8000 || event->gaxis.value > 8000) {
			// Ignore small axis values
			axisValue = event->gaxis.value;
		}
#ifdef __3DS__
		// The circle pad (left stick) doubles as a cursor stick since only
		// New 3DS models have a C-stick; both contribute to cursor movement
		static MxS32 g_cpadX = 0, g_cpadY = 0, g_cstickX = 0, g_cstickY = 0;
		switch (event->gaxis.axis) {
		case SDL_GAMEPAD_AXIS_LEFTX:
			g_cpadX = axisValue;
			break;
		case SDL_GAMEPAD_AXIS_LEFTY:
			g_cpadY = axisValue;
			break;
		case SDL_GAMEPAD_AXIS_RIGHTX:
			g_cstickX = axisValue;
			break;
		case SDL_GAMEPAD_AXIS_RIGHTY:
			g_cstickY = axisValue;
			break;
		}
		MxS32 combinedX = SDL_clamp(g_cpadX + g_cstickX, -SDL_JOYSTICK_AXIS_MAX, SDL_JOYSTICK_AXIS_MAX);
		MxS32 combinedY = SDL_clamp(g_cpadY + g_cstickY, -SDL_JOYSTICK_AXIS_MAX, SDL_JOYSTICK_AXIS_MAX);
		g_lastJoystickMouseX = ((MxFloat) combinedX) / SDL_JOYSTICK_AXIS_MAX * g_isle->GetCursorSensitivity();
		g_lastJoystickMouseY = ((MxFloat) combinedY) / SDL_JOYSTICK_AXIS_MAX * g_isle->GetCursorSensitivity();
#endif
#ifndef __3DS__
		if (event->gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTX) {
			g_lastJoystickMouseX = ((MxFloat) axisValue) / SDL_JOYSTICK_AXIS_MAX * g_isle->GetCursorSensitivity();
		}
		else if (event->gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTY) {
			g_lastJoystickMouseY = ((MxFloat) axisValue) / SDL_JOYSTICK_AXIS_MAX * g_isle->GetCursorSensitivity();
		}
#endif
		GamepadBindings::Input trigger;
		if (GamepadBindings::FromTrigger(static_cast<SDL_GamepadAxis>(event->gaxis.axis), trigger)) {
			HandleGamepadAction(g_gamepad.Trigger(event->gaxis.which, trigger, event->gaxis.value, g_mousedown));
		}
		break;
	}
	case SDL_EVENT_MOUSE_MOTION:
		if (g_mouseWarped) {
			g_mouseWarped = FALSE;
			break;
		}

		g_mousemoved = TRUE;

		if (InputManager()) {
			InputManager()->QueueEvent(
				c_notificationMouseMove,
				IsleApp::MapMouseButtonFlagsToModifier(event->motion.state),
				event->motion.x,
				event->motion.y,
				0
			);
		}

		g_lastMouseX = event->motion.x;
		g_lastMouseY = event->motion.y;

#ifdef __DJGPP__
		if (VideoManager()) {
			VideoManager()->MoveCursor(Min((MxS32) g_lastMouseX, 639), Min((MxS32) g_lastMouseY, 479));
		}
#else
		SDL_ShowCursor();
		g_isle->SetDrawCursor(FALSE);
		if (VideoManager()) {
			VideoManager()->SetCursorBitmap(NULL);
		}
#endif
		break;
	case SDL_EVENT_FINGER_MOTION: {
		g_mousemoved = TRUE;

		float x = event->tfinger.x * g_targetWidth;
		float y = event->tfinger.y * g_targetHeight;

		if (InputManager()) {
			MxU8 modifier = LegoEventNotificationParam::c_lButtonState;
			if (InputManager()->HandleTouchEvent(event, g_isle->GetTouchScheme())) {
				modifier |= LegoEventNotificationParam::c_motionHandled;
			}

			InputManager()->QueueEvent(c_notificationMouseMove, modifier, x, y, 0);
		}

		g_lastMouseX = x;
		g_lastMouseY = y;

		SDL_HideCursor();
		g_isle->SetDrawCursor(FALSE);
		if (VideoManager()) {
			VideoManager()->SetCursorBitmap(NULL);
		}
		break;
	}
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		g_mousedown = TRUE;

		if (InputManager()) {
			InputManager()->QueueEvent(
				c_notificationButtonDown,
				IsleApp::MapMouseButtonFlagsToModifier(SDL_GetMouseState(NULL, NULL)),
				event->button.x,
				event->button.y,
				0
			);
		}
		break;
	case SDL_EVENT_FINGER_DOWN: {
		g_mousedown = TRUE;

		float x = event->tfinger.x * g_targetWidth;
		float y = event->tfinger.y * g_targetHeight;

		if (InputManager()) {
			InputManager()->HandleTouchEvent(event, g_isle->GetTouchScheme());
			InputManager()->QueueEvent(c_notificationButtonDown, LegoEventNotificationParam::c_lButtonState, x, y, 0);
		}

		g_lastMouseX = x;
		g_lastMouseY = y;

		SDL_HideCursor();
		g_isle->SetDrawCursor(FALSE);
		if (VideoManager()) {
			VideoManager()->SetCursorBitmap(NULL);
		}
		break;
	}
	case SDL_EVENT_MOUSE_BUTTON_UP:
		g_mousedown = FALSE;

		if (InputManager()) {
			InputManager()->QueueEvent(
				c_notificationButtonUp,
				IsleApp::MapMouseButtonFlagsToModifier(SDL_GetMouseState(NULL, NULL)),
				event->button.x,
				event->button.y,
				0
			);
		}
		break;
	case SDL_EVENT_FINGER_UP:
	case SDL_EVENT_FINGER_CANCELED: {
		g_mousedown = FALSE;

		float x = event->tfinger.x * g_targetWidth;
		float y = event->tfinger.y * g_targetHeight;

		g_isle->DetectDoubleTap(event->tfinger);

		if (InputManager()) {
			InputManager()->HandleTouchEvent(event, g_isle->GetTouchScheme());
			InputManager()->QueueEvent(c_notificationButtonUp, 0, x, y, 0);
		}
		break;
	}
	}

	if (event->user.type == g_legoSdlEvents.m_windowsMessage) {
		switch (event->user.code) {
		case WM_ISLE_SETCURSOR:
			g_isle->SetupCursor((Cursor) (uintptr_t) event->user.data1);
			break;
		case WM_TIMER:
			if (InputManager()) {
				InputManager()->QueueEvent(c_notificationTimer, (MxU8) (uintptr_t) event->user.data1, 0, 0, 0);
			}
			break;
		default:
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unknown SDL Windows message: 0x%" SDL_PRIx32, event->user.code);
			break;
		}
	}
	else if (event->user.type == g_legoSdlEvents.m_presenterProgress) {
		MxDSAction* action = static_cast<MxDSAction*>(event->user.data1);
		MxPresenter::TickleState state = static_cast<MxPresenter::TickleState>(event->user.code);

#ifdef __EMSCRIPTEN__
		if (!g_isle->GetGameStarted()) {
			Emscripten_SendPresenterProgress(action, state);
		}
#endif

		if (!g_isle->GetGameStarted() && action && state == MxPresenter::e_ready &&
			!SDL_strncmp(action->GetObjectName(), "Lego_Smk", 8)) {
			g_isle->SetGameStarted(TRUE);

#ifdef __EMSCRIPTEN__
			Emscripten_SetupWindow((SDL_Window*) g_isle->GetWindowHandle());
#endif

#ifdef ANDROID
			Android_ShowMenuButton();
#endif
			SDL_Log("Game started");
		}
	}
	else if (event->user.type == g_legoSdlEvents.m_gameEvent) {
		auto rumble = [](float p_strength, float p_lowFrequencyRumble, float p_highFrequencyRumble, MxU32 p_milliseconds
					  ) {
			if (g_isle->GetHaptic() &&
				!InputManager()
					 ->HandleRumbleEvent(p_strength, p_lowFrequencyRumble, p_highFrequencyRumble, p_milliseconds)) {
// Platform-specific handling
#ifdef __EMSCRIPTEN__
				Emscripten_HandleRumbleEvent(p_lowFrequencyRumble, p_highFrequencyRumble, p_milliseconds);
#endif
			}
		};

		switch (event->user.code) {
		case e_hitActor:
			rumble(0.5f, 0.5f, 0.5f, 700);
			break;
		case e_skeletonKick:
			rumble(0.8f, 0.8f, 0.8f, 2500);
			break;
		case e_raceFinished:
		case e_goodEnding:
		case e_badEnding:
			rumble(1.0f, 1.0f, 1.0f, 3000);
			break;
#ifdef __EMSCRIPTEN__
		case e_saveSlotWritten:
			Emscripten_SendSaveSlotWritten((MxS32) (intptr_t) event->user.data1);
			break;
		case e_saveStateChanged:
			Emscripten_SendSaveStateChanged();
			break;
#endif
		}
	}

#ifdef EXTENSIONS
	Extensions::ThirdPersonCameraExt::HandleSDLEvent(event);
#endif

	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
#ifdef ANDROID
	Android_ClearTouchControls();
#endif
	if (window) {
		SDL_DestroyWindow(window);
		window = NULL;
	}

	SDL_Quit();
}

MxU8 IsleApp::MapMouseButtonFlagsToModifier(SDL_MouseButtonFlags p_flags)
{
	// [library:window]
	// Map button states to Windows button states (LegoEventNotificationParam)
	// Not mapping mod keys SHIFT and CTRL since they are not used by the game.

	MxU8 modifier = 0;
	if (p_flags & SDL_BUTTON_LMASK) {
		modifier |= LegoEventNotificationParam::c_lButtonState;
	}
	if (p_flags & SDL_BUTTON_RMASK) {
		modifier |= LegoEventNotificationParam::c_rButtonState;
	}

	return modifier;
}

// FUNCTION: ISLE 0x4023e0
MxResult IsleApp::SetupWindow()
{
#ifdef ANDROID
	// Ask what could be offered before the window exists, so that any startup failure from here
	// on - the configuration itself included - still leaves the Settings screen a renderer list
	// to show.
	Android_CaptureRenderers(nullptr);
#endif

	if (!LoadConfig()) {
		return FAILURE;
	}

#if defined(ANDROID)
	// Render quality is independent of the game's logical input and UI canvas.
	g_targetWidth = 640;
	g_targetHeight = 480;
#elif defined(MINIWIN)
	// MINIWIN: window/VESA mode matches the game's rendering resolution.
	g_targetWidth = m_xRes;
	g_targetHeight = m_yRes;
#else
	// DX5: fullscreen uses exclusive resolution for display mode switching.
	if (m_fullScreen) {
		g_targetWidth = m_exclusiveXRes;
		g_targetHeight = m_exclusiveYRes;
	}
	else {
		g_targetWidth = m_xRes;
		g_targetHeight = m_yRes;
	}
#endif

	SetupVideoFlags(
		m_fullScreen,
		m_flipSurfaces,
		m_backBuffersInVram,
		m_using8bit,
		m_using16bit,
		m_hasLightSupport,
		FALSE,
		m_wideViewAngle,
		m_deviceId
	);

	MxOmni::SetSound3D(m_use3dSound);

	srand(time(NULL));

	// [library:window] Use original game cursors in the resources instead?
	m_cursorCurrent = m_cursorArrow = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
	m_cursorBusy = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT);
	m_cursorNo = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NOT_ALLOWED);
	SDL_SetCursor(m_cursorCurrent);
	m_cursorCurrentBitmap = m_cursorArrowBitmap = &arrow_cursor;
	m_cursorBusyBitmap = &busy_cursor;
	m_cursorNoBitmap = &no_cursor;

	SDL_PropertiesID props = SDL_CreateProperties();
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, g_targetWidth);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, g_targetHeight);
	SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, m_fullScreen);
	SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, WINDOW_TITLE);
#ifndef __EMSCRIPTEN__
	SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
#endif
#if defined(MINIWIN) && !defined(__EMSCRIPTEN__)
	SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
#endif
#ifdef MINIWIN
	// The device the window is being built specifically for, if any. Only a startup failure
	// reads it back, so a build whose platform never makes that choice sets it and stops there.
	[[maybe_unused]] const char* exclusiveDevice = Miniwin_SetupWindowCreateProperties(props, m_deviceId);
#endif

	window = SDL_CreateWindowWithProperties(props);

#ifdef __EMSCRIPTEN__
	// Force correct window size since SDL3 may have picked up CSS dimensions
	SDL_SetWindowSize(window, g_targetWidth, g_targetHeight);
#endif

	SDL_SetPointerProperty(SDL_GetWindowProperties(window), ISLE_PROP_WINDOW_CREATE_VIDEO_PARAM, &m_videoParam);

#ifdef __DJGPP__
	// DOS: request an 8-bit (INDEX8) fullscreen mode so the VESA
	// framebuffer is paletted and we can blit INDEX8 surfaces directly.
	{
		SDL_DisplayMode mode = {};
		mode.w = g_targetWidth;
		mode.h = g_targetHeight;
		mode.format = SDL_PIXELFORMAT_INDEX8;
		SDL_SetWindowFullscreenMode(window, &mode);
	}
#else
	if (m_exclusiveFullScreen && m_fullScreen) {
		SDL_DisplayMode closestMode;
		SDL_DisplayID displayID = SDL_GetDisplayForWindow(window);
		if (SDL_GetClosestFullscreenDisplayMode(
				displayID,
				m_exclusiveXRes,
				m_exclusiveYRes,
				m_exclusiveFrameRate,
				true,
				&closestMode
			)) {
			SDL_SetWindowFullscreenMode(window, &closestMode);
		}
	}
#endif

#ifdef MINIWIN
	m_windowHandle = reinterpret_cast<HWND>(window);
#else
	m_windowHandle =
		(HWND) SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
#endif

	SDL_DestroyProperties(props);

	if (!m_windowHandle) {
		return FAILURE;
	}

	SDL_IOStream* icon_stream = SDL_IOFromMem(isle_bmp, isle_bmp_len);

	if (icon_stream) {
		SDL_Surface* icon = SDL_LoadBMP_IO(icon_stream, true);

		if (icon) {
			SDL_SetWindowIcon(window, icon);
			SDL_DestroySurface(icon);
		}
		else {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load icon: %s", SDL_GetError());
		}
	}
	else {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open SDL_IOStream for icon: %s", SDL_GetError());
	}

#ifdef ANDROID
	SDL_SetNumberProperty(SDL_GetWindowProperties(window), MINIWIN_PROP_RENDER_WIDTH, SDL_clamp(m_xRes, 640, 1280));
	SDL_SetNumberProperty(SDL_GetWindowProperties(window), MINIWIN_PROP_RENDER_HEIGHT, SDL_clamp(m_yRes, 480, 960));
	Android_CaptureRenderers(window);
#endif

	if (!SetupLegoOmni()) {
#ifdef ANDROID
		// A window built for one device has no way back within this launch.
		// LegoVideoManager::Create falls back to the best device only when the configured id
		// does not resolve; a device that resolves and then fails to create ends the startup.
		// The startup dialog's Settings button is the way out, so say which setting to change
		// rather than leaving the generic "quit all other applications" message to do it.
		//
		// SetupLegoOmni fails for sound, media and allocation reasons too, none of which set
		// this message, so claim only what is certain: the game did not start with the renderer
		// that was chosen. Miniwin supplies the name so it reads the way the Settings row does,
		// since "Vulkan" appears nowhere a player can see.
		if (exclusiveDevice && g_startupError[0] == '\0') {
			SDL_snprintf(
				g_startupError,
				sizeof(g_startupError),
				"\"LEGO® Island\" did not start with the %s renderer.\n"
				"Choose a different one under Display > Renderer, or Game default.\n"
				"The log says what failed.",
				exclusiveDevice
			);
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", g_startupError);
		}
#endif
		return FAILURE;
	}

	GameState()->SetSavePath(m_savePath);

	if (VerifyFilesystem() != SUCCESS) {
		return FAILURE;
	}

	Lego()->LoadSiLoader();

	DetectGameVersion();
	GameState()->SerializePlayersInfo(LegoStorage::c_read);
	GameState()->SerializeScoreHistory(LegoStorage::c_read);

	MxS32 iVar10;
	switch (m_islandQuality) {
	case 0:
		iVar10 = 1;
		break;
	case 1:
		iVar10 = 2;
		break;
	default:
		iVar10 = 100;
	}

	MxS32 uVar1 = (m_islandTexture == 0);
	LegoModelPresenter::configureLegoModelPresenter(uVar1);
	LegoPartPresenter::configureLegoPartPresenter(uVar1, iVar10);
	LegoWorldPresenter::configureLegoWorldPresenter(m_islandQuality);
	LegoBuildingManager::configureLegoBuildingManager(m_islandQuality);
	LegoROI::configureLegoROI(iVar10);
	LegoAnimationManager::configureLegoAnimationManager(m_maxAllowedExtras);
	MxTransitionManager::configureMxTransitionManager(m_transitionType);
	RealtimeView::SetUserMaxLOD(m_maxLod);
	if (LegoOmni::GetInstance()) {
		if (LegoOmni::GetInstance()->GetVideoManager()) {
			LegoOmni::GetInstance()->GetVideoManager()->SetCursorBitmap(m_cursorCurrentBitmap);
		}
		MxDirect3D* d3d = LegoOmni::GetInstance()->GetVideoManager()->GetDirect3D();
		if (d3d) {
			SDL_Log(
				"Direct3D driver name=\"%s\" description=\"%s\"",
				d3d->GetDeviceName().c_str(),
				d3d->GetDeviceDescription().c_str()
			);
		}
		else {
			SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to get D3D device name and description");
		}
		if (LegoOmni::GetInstance()->GetInputManager()) {
			LegoOmni::GetInstance()->GetInputManager()->SetWasd(m_wasd);
		}
	}

	return SUCCESS;
}

// FUNCTION: ISLE 0x4028d0
bool IsleApp::LoadConfig()
{
#ifdef IOS
	const char* prefPath = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
#elif defined(ANDROID)
	std::unique_ptr<char, decltype(&SDL_free)> androidPrefPath(SDL_GetPrefPath("isledecomp", "isle"), SDL_free);
	const char* prefPath = androidPrefPath.get();
	if (!prefPath || !*prefPath) {
		SDL_snprintf(
			g_startupError,
			sizeof(g_startupError),
			"The app's internal storage directory is unavailable. Please try again.\n%s",
			SDL_GetError()
		);
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", g_startupError);
		return false;
	}
	MxString androidSavePath = MxString(prefPath) + "saves/";
#elif defined(EMSCRIPTEN)
	if (m_iniPath && !Emscripten_SetupConfig(m_iniPath)) {
		m_iniPath = NULL;
	}
	char* prefPath = SDL_GetPrefPath("isledecomp", "isle");
#else
	char* prefPath = SDL_GetPrefPath("isledecomp", "isle");
#endif

	MxString iniConfig;
	if (m_iniPath) {
		iniConfig = m_iniPath;
	}
	else if (prefPath) {
		iniConfig = prefPath;
		iniConfig += "isle.ini";
	}
	else {
		iniConfig = "isle.ini";
	}

#ifdef ANDROID
	Android_SetSettingsPath(iniConfig.GetData());
#endif

	SDL_Log("Reading configuration from \"%s\"", iniConfig.GetData());

	dictionary* dict = iniparser_load(iniConfig.GetData());

	// [library:config]
	// Load sane defaults if dictionary failed to load
	if (!dict || dict->n == 0) {
		iniparser_freedict(dict);

		if (m_iniPath) {
#ifdef __EMSCRIPTEN__
			SDL_Log("Invalid config path '%s', falling back to defaults", m_iniPath);
			m_iniPath = NULL;
			if (prefPath) {
				iniConfig = prefPath;
				iniConfig += "isle.ini";
			}
			else {
				iniConfig = "isle.ini";
			}
#else
			SDL_Log("Invalid config path '%s'", m_iniPath);
			return false;
#endif
		}

#ifdef ANDROID
		const char* dataPath = SDL_GetAndroidExternalStoragePath();
		if (!dataPath || !*dataPath) {
			SDL_snprintf(
				g_startupError,
				sizeof(g_startupError),
				"The app's external storage directory for game data is unavailable. Please try again.\n%s",
				SDL_GetError()
			);
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", g_startupError);
			return false;
		}
#endif

		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Loading sane defaults");
		FILE* iniFP = fopen(iniConfig.GetData(), "wb");

		if (!iniFP) {
			SDL_LogError(
				SDL_LOG_CATEGORY_APPLICATION,
				"Failed to write config at '%s': %s",
				iniConfig.GetData(),
				strerror(errno)
			);
			return false;
		}

		char buf[32];
		dict = dictionary_new(0);
		iniparser_set(dict, "isle", NULL);

		iniparser_set(dict, "isle:diskpath", SDL_GetBasePath());
		iniparser_set(dict, "isle:cdpath", MxOmni::GetCD());
		iniparser_set(dict, "isle:mediapath", SDL_GetBasePath());
		iniparser_set(dict, "isle:savepath", prefPath);

		iniparser_set(dict, "isle:Flip Surfaces", m_flipSurfaces ? "true" : "false");
		iniparser_set(dict, "isle:Full Screen", m_fullScreen ? "true" : "false");
		iniparser_set(dict, "isle:Exclusive Full Screen", m_exclusiveFullScreen ? "true" : "false");
		iniparser_set(dict, "isle:Wide View Angle", m_wideViewAngle ? "true" : "false");

		iniparser_set(dict, "isle:3DSound", m_use3dSound ? "true" : "false");
		iniparser_set(dict, "isle:Music", m_useMusic ? "true" : "false");

		SDL_snprintf(buf, sizeof(buf), "%f", m_cursorSensitivity);
		iniparser_set(dict, "isle:Cursor Sensitivity", buf);

		iniparser_set(dict, "isle:Back Buffers in Video RAM", "-1");

		iniparser_set(dict, "isle:Island Quality", SDL_itoa(m_islandQuality, buf, 10));
		iniparser_set(dict, "isle:Island Texture", SDL_itoa(m_islandTexture, buf, 10));
		SDL_snprintf(buf, sizeof(buf), "%f", m_maxLod);
		iniparser_set(dict, "isle:Max LOD", buf);
		iniparser_set(dict, "isle:Max Allowed Extras", SDL_itoa(m_maxAllowedExtras, buf, 10));
		iniparser_set(dict, "isle:Transition Type", SDL_itoa(m_transitionType, buf, 10));
		iniparser_set(dict, "isle:Touch Scheme", SDL_itoa(m_touchScheme, buf, 10));
		iniparser_set(dict, "isle:Haptic", m_haptic ? "true" : "false");
		iniparser_set(dict, "isle:WASD", m_wasd ? "true" : "false");
		iniparser_set(dict, "isle:Horizontal Resolution", SDL_itoa(m_xRes, buf, 10));
		iniparser_set(dict, "isle:Vertical Resolution", SDL_itoa(m_yRes, buf, 10));
		iniparser_set(dict, "isle:Exclusive X Resolution", SDL_itoa(m_exclusiveXRes, buf, 10));
		iniparser_set(dict, "isle:Exclusive Y Resolution", SDL_itoa(m_exclusiveYRes, buf, 10));
		iniparser_set(dict, "isle:Exclusive Framerate", SDL_itoa(m_exclusiveFrameRate, buf, 10));
		iniparser_set(dict, "isle:Frame Delta", SDL_itoa(m_frameDelta, buf, 10));
		iniparser_set(dict, "isle:MSAA", SDL_itoa(m_msaaSamples, buf, 10));
		iniparser_set(dict, "isle:Anisotropic", SDL_itoa(m_anisotropic, buf, 10));
		iniparser_set(dict, "isle:Lighting Model", SDL_itoa(m_lightingModel, buf, 10));
		iniparser_set(dict, "isle:Active in background", m_activeInBackground ? "true" : "false");

#ifdef EXTENSIONS
		iniparser_set(dict, "extensions", NULL);
		for (const char* key : Extensions::availableExtensions) {
			iniparser_set(dict, key, "false");
		}
#endif

#ifdef __3DS__
		N3DS_SetupDefaultConfigOverrides(dict);
#endif
#ifdef __SWITCH__
		NX_SetupDefaultConfigOverrides(dict);
#endif
#ifdef WINDOWS_STORE
		XBONE_SetupDefaultConfigOverrides(dict);
#endif
#ifdef IOS
		IOS_SetupDefaultConfigOverrides(dict);
#endif
#ifdef ANDROID
		Android_SetupDefaultConfigOverrides(dict, dataPath);
#endif

#ifdef __vita__
		VITA_SetupDefaultConfigOverrides(dict);
#endif
		iniparser_dump_ini(dict, iniFP);
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "New config written at '%s'", iniConfig.GetData());
		fclose(iniFP);
	}

#ifdef __EMSCRIPTEN__
	Emscripten_SetupDefaultConfigOverrides(dict);
#endif
#ifdef IOS
	// [library:config]
	// iOS relocates both the app bundle and the data container on every app update,
	// so absolute paths stored in the config go stale. Re-derive them on every launch.
	IOS_SetupDefaultConfigOverrides(dict);
#endif

	MxOmni::SetHD((m_hdPath = SDL_strdup(iniparser_getstring(dict, "isle:diskpath", SDL_GetBasePath()))));
	MxOmni::SetCD((m_cdPath = SDL_strdup(iniparser_getstring(dict, "isle:cdpath", MxOmni::GetCD()))));
#ifdef ANDROID
	m_savePath = SDL_strdup(iniparser_getstring(dict, "isle:savepath", androidSavePath.GetData()));
#else
	m_savePath = SDL_strdup(iniparser_getstring(dict, "isle:savepath", prefPath));
#endif

	// A restored or hand-written config may name a save directory that does not exist yet.
	// LegoGameState::Save opens the slot file for writing and does not create parent directories.
#ifdef ANDROID
	const char* savePathError = NULL;
	if (!m_savePath) {
		savePathError = "Out of memory resolving the save directory.";
	}
	else if (!*m_savePath) {
		savePathError = "The savepath setting must not be empty.";
	}
	else if (!SDL_CreateDirectory(m_savePath)) {
		savePathError = SDL_GetError();
	}
	if (savePathError) {
		SDL_snprintf(
			g_startupError,
			sizeof(g_startupError),
			"Could not create the save directory '%s'.\n%s",
			m_savePath ? m_savePath : "",
			savePathError
		);
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", g_startupError);
		iniparser_freedict(dict);
		return false;
	}
#else
	if (!SDL_GetPathInfo(m_savePath, NULL)) {
		SDL_CreateDirectory(m_savePath);
	}
#endif
	m_mediaPath = SDL_strdup(iniparser_getstring(dict, "isle:mediapath", m_hdPath));
	m_flipSurfaces = iniparser_getboolean(dict, "isle:Flip Surfaces", m_flipSurfaces);
	m_fullScreen = iniparser_getboolean(dict, "isle:Full Screen", m_fullScreen);
	m_exclusiveFullScreen = iniparser_getboolean(dict, "isle:Exclusive Full Screen", m_exclusiveFullScreen);
	m_wideViewAngle = iniparser_getboolean(dict, "isle:Wide View Angle", m_wideViewAngle);
	m_use3dSound = iniparser_getboolean(dict, "isle:3DSound", m_use3dSound);
	m_useMusic = iniparser_getboolean(dict, "isle:Music", m_useMusic);
	m_cursorSensitivity = iniparser_getdouble(dict, "isle:Cursor Sensitivity", m_cursorSensitivity);

	MxS32 backBuffersInVRAM = iniparser_getboolean(dict, "isle:Back Buffers in Video RAM", -1);
	if (backBuffersInVRAM != -1) {
		m_backBuffersInVram = !backBuffersInVRAM;
	}

	MxS32 bitDepth = iniparser_getint(dict, "isle:Display Bit Depth", -1);
	if (bitDepth != -1) {
		if (bitDepth == 8) {
			m_using8bit = TRUE;
		}
		else if (bitDepth == 16) {
			m_using16bit = TRUE;
		}
	}

	m_islandQuality = iniparser_getint(dict, "isle:Island Quality", m_islandQuality);
	m_islandTexture = iniparser_getint(dict, "isle:Island Texture", m_islandTexture);
	m_maxLod = iniparser_getdouble(dict, "isle:Max LOD", m_maxLod);
	m_maxAllowedExtras = iniparser_getint(dict, "isle:Max Allowed Extras", m_maxAllowedExtras);
	m_transitionType =
		(MxTransitionManager::TransitionType) iniparser_getint(dict, "isle:Transition Type", m_transitionType);
	m_touchScheme = (LegoInputManager::TouchScheme) iniparser_getint(dict, "isle:Touch Scheme", m_touchScheme);
#ifdef ANDROID
	Android_ConfigureTouchControls(
		iniparser_getboolean(dict, "isle:Show Touch Controls", Android_TouchSettings{}.m_visible)
	);
#endif
	m_haptic = iniparser_getboolean(dict, "isle:Haptic", m_haptic);
	m_wasd = iniparser_getboolean(dict, "isle:WASD", m_wasd);
	g_gamepad.SetTable(GamepadBindings::Parse(
		[dict](const char* p_key) { return iniparser_getstring(dict, p_key, NULL); },
		[](const char* p_key, const char* p_value) {
			SDL_Log("Ignoring invalid %s value \"%s\"; using its default", p_key, p_value);
		}
	));
	m_xRes = iniparser_getint(dict, "isle:Horizontal Resolution", m_xRes);
	m_yRes = iniparser_getint(dict, "isle:Vertical Resolution", m_yRes);
	m_exclusiveXRes = iniparser_getint(dict, "isle:Exclusive X Resolution", m_exclusiveXRes);
	m_exclusiveYRes = iniparser_getint(dict, "isle:Exclusive Y Resolution", m_exclusiveYRes);
	m_exclusiveFrameRate = iniparser_getdouble(dict, "isle:Exclusive Framerate", m_exclusiveFrameRate);
#ifdef ANDROID
	m_videoParam.GetRect() = MxRect32(0, 0, 639, 479);
#else
	if (!m_fullScreen) {
		m_videoParam.GetRect() = MxRect32(0, 0, (m_xRes - 1), (m_yRes - 1));
	}
#endif
	m_frameRate = (1000.0f / iniparser_getdouble(dict, "isle:Frame Delta", m_frameDelta));
	m_frameDelta = static_cast<int>(iniparser_getdouble(dict, "isle:Frame Delta", m_frameDelta));
	m_videoParam.SetMSAASamples((m_msaaSamples = iniparser_getint(dict, "isle:MSAA", m_msaaSamples)));
	m_videoParam.SetAnisotropic((m_anisotropic = iniparser_getdouble(dict, "isle:Anisotropic", m_anisotropic)));
	m_videoParam.SetLightingModel((m_lightingModel = iniparser_getint(dict, "isle:Lighting Model", m_lightingModel)));
	m_activeInBackground = iniparser_getboolean(dict, "isle:Active in Background", m_activeInBackground);

	const char* deviceId = iniparser_getstring(dict, "isle:3D Device ID", NULL);
	if (deviceId != NULL) {
		m_deviceId = SDL_strdup(deviceId);
	}

#ifdef EXTENSIONS
	for (const char* key : Extensions::availableExtensions) {
		if (iniparser_getboolean(dict, key, 0)) {
			std::vector<const char*> extensionKeys;
			const char* section = SDL_strchr(key, ':') + 1;
			extensionKeys.resize(iniparser_getsecnkeys(dict, section));
			iniparser_getseckeys(dict, section, extensionKeys.data());

			std::map<std::string, std::string> extensionDict;
			for (const char* key : extensionKeys) {
				extensionDict[key] = iniparser_getstring(dict, key, NULL);
			}

			Extensions::Enable(key, std::move(extensionDict));
		}
	}
#endif

	iniparser_freedict(dict);

	[](auto path) {
		if constexpr (std::is_same_v<decltype(path), char*>) {
			SDL_free(path);
		}
	}(prefPath);
	return true;
}

// FUNCTION: ISLE 0x402c20
inline bool IsleApp::Tick()
{
	// GLOBAL: ISLE 0x4101c0
	static MxLong g_lastFrameTime = 0;

	// GLOBAL: ISLE 0x4101bc
	static MxS32 g_startupDelay = 1;

	if (!m_windowActive) {
		SDL_Delay(1);
		return true;
	}

	if (!Lego()) {
		return true;
	}

#ifdef EXTENSIONS
	// Closing here would delete this IsleApp out from under the Tick still running, so record it
	// and let SDL_AppIterate close the game once Tick has returned.
	if (Extensions::IsMultiplayerRejected()) {
		g_multiplayerRejected = TRUE;
		return true;
	}
#endif

	if (!TickleManager()) {
		return true;
	}
	if (!Timer()) {
		return true;
	}

	MxLong currentTime = Timer()->GetRealTime();
	if (currentTime < g_lastFrameTime) {
		g_lastFrameTime = -m_frameDelta;
	}

	if (m_frameDelta + g_lastFrameTime >= currentTime) {
		SDL_Delay(1);
		return true;
	}

	if (!Lego()->IsPaused()) {
		TickleManager()->Tickle();
	}
	g_lastFrameTime = currentTime;

	if (g_startupDelay == 0) {
		return true;
	}

	g_startupDelay--;
	if (g_startupDelay != 0) {
		return true;
	}

	LegoOmni::GetInstance()->CreateBackgroundAudio();
	BackgroundAudioManager()->Enable(m_useMusic);

	MxStreamController* stream = Streamer()->Open("\\lego\\scripts\\isle\\isle", MxStreamer::e_diskStream);
	MxDSAction ds;

	if (!stream) {
		stream = Streamer()->Open("\\lego\\scripts\\nocd", MxStreamer::e_diskStream);
		if (!stream) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open NOCD.si: Streamer failed to load");
			return false;
		}

		ds.SetAtomId(stream->GetAtom());
		ds.SetUnknown24(-1);
		ds.SetObjectId(0);
		VideoManager()->EnableFullScreenMovie(TRUE, TRUE);

		if (Start(&ds) != SUCCESS) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open NOCD.si: Failed to start initial action");
			return false;
		}
	}
	else {
		ds.SetAtomId(stream->GetAtom());
		ds.SetUnknown24(-1);
		ds.SetObjectId(0);
		if (Start(&ds) != SUCCESS) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open ISLE.si: Failed to start initial action");
			return false;
		}
	}

	return true;
}

// FUNCTION: ISLE 0x402e80
void IsleApp::SetupCursor(Cursor p_cursor)
{
	switch (p_cursor) {
	case e_cursorArrow:
		m_cursorCurrent = m_cursorArrow;
		m_cursorCurrentBitmap = m_cursorArrowBitmap;
		break;
	case e_cursorBusy:
		m_cursorCurrent = m_cursorBusy;
		m_cursorCurrentBitmap = m_cursorBusyBitmap;
		break;
	case e_cursorNo:
		m_cursorCurrent = m_cursorNo;
		m_cursorCurrentBitmap = m_cursorNoBitmap;
		break;
	case e_cursorNone:
		m_cursorCurrent = NULL;
		m_cursorCurrentBitmap = NULL;
	case e_cursorUnused3:
	case e_cursorUnused4:
	case e_cursorUnused5:
	case e_cursorUnused6:
	case e_cursorUnused7:
	case e_cursorUnused8:
	case e_cursorUnused9:
	case e_cursorUnused10:
		break;
	}

	if (g_isle->GetDrawCursor()) {
		VideoManager()->SetCursorBitmap(m_cursorCurrentBitmap);
	}
	else {
		if (m_cursorCurrent != NULL) {
			SDL_SetCursor(m_cursorCurrent);
			SDL_ShowCursor();
		}
		else {
			SDL_HideCursor();
		}
	}
}

SDL_AppResult IsleApp::ParseArguments(int argc, char** argv)
{
	for (int i = 1, consumed; i < argc; i += consumed) {
		consumed = -1;

		if (strcmp(argv[i], "--ini") == 0 && i + 1 < argc) {
			m_iniPath = argv[i + 1];
			consumed = 2;
		}
		else if (strcmp(argv[i], "--help") == 0) {
			DisplayArgumentHelp(argv[0]);
			return SDL_APP_SUCCESS;
		}
		if (consumed <= 0) {
			SDL_Log("Invalid argument(s): %s", argv[i]);
			DisplayArgumentHelp(argv[0]);
			return SDL_APP_FAILURE;
		}
	}

	return SDL_APP_CONTINUE;
}

void IsleApp::DisplayArgumentHelp(const char* p_execName)
{
	SDL_Log("Usage: %s [options]", p_execName);
	SDL_Log("Options:");
	SDL_Log("	--ini <path>		Set custom path to .ini config");
	SDL_Log("	--help			Show this help message");
}

#ifndef __EMSCRIPTEN__
// Returns the first entry of g_files that cannot be found under ".", p_hdPath or p_cdPath, or
// NULL when every file is present. p_attempts receives the paths tried for the file that is
// reported missing, for the error message. No side effects, so it is safe to re-run: callers
// must have set MxOmni::SetHD/SetCD first, since MapPathToFilesystem resolves against the
// globbed file lists.
static const char* FindMissingGameFile(const char* p_hdPath, const char* p_cdPath, MxString& p_attempts)
{
	for (const char* file : g_files) {
		const char* searchPaths[] = {".", p_hdPath, p_cdPath};
		bool found = false;

		p_attempts = "";

		for (const char* base : searchPaths) {
			MxString path(base);
			path += file;
			path.MapPathToFilesystem();

			if (SDL_GetPathInfo(path.GetData(), NULL)) {
				found = true;
				break;
			}

			p_attempts += "\n";
			p_attempts += path.GetData();
			p_attempts += " (";
			p_attempts += SDL_GetError();
			p_attempts += ")";
		}

		if (!found) {
			return file;
		}
	}

	return NULL;
}

#if defined(ANDROID) || defined(IOS)
// The bound on the import loop, and the only one: this used to be an unbounded self-recursion.
// Deliberately more than one, because picking the wrong folder is the common mistake and the
// retry prompt names what is still missing, so a second go is the recovery path rather than a
// repeat of the same question. Each round is a full user interaction - a prompt, a folder pick
// and a copy of hundreds of megabytes - so a handful is already more patience than anyone has.
static const int c_maxImportAttempts = 3;

// One import attempt. Returns true when files were imported and the check is worth re-running.
static bool TryImportGameFiles(
	SDL_Window* p_window,
	const char* p_iniPath,
	char** p_hdPath,
	const char* p_missingFile,
	int p_attempt
)
{
#ifdef ANDROID
	return Android_TryImportGameFiles(p_window, p_iniPath, p_hdPath, p_missingFile, p_attempt);
#else
	// iOS imports into the configured diskpath, so it has no path to thread back and its
	// prompt does not name the missing file yet. It still inherits the bound below.
	return IOS_TryImportGameFiles(p_window, *p_hdPath);
#endif
}
#endif
#endif

MxResult IsleApp::VerifyFilesystem()
{
#ifdef __EMSCRIPTEN__
	Emscripten_SetupFilesystem();
#else
	MxString attempts;
	const char* missing = FindMissingGameFile(m_hdPath, m_cdPath, attempts);

#if defined(ANDROID) || defined(IOS)
	for (int attempt = 0; missing != NULL && attempt < c_maxImportAttempts; attempt++) {
		bool imported =
			TryImportGameFiles(reinterpret_cast<SDL_Window*>(m_windowHandle), m_iniPath, &m_hdPath, missing, attempt);

		// Re-check even when the prompt was declined: removing the imported game data repoints
		// m_hdPath at the default root on its way out, and the failure message below has to name
		// the paths that were actually searched.
		MxOmni::SetHD(m_hdPath);
		MxOmni::SetCD(m_cdPath);
		missing = FindMissingGameFile(m_hdPath, m_cdPath, attempts);

		if (!imported) {
			break;
		}
	}
#endif

	if (missing != NULL) {
		char buffer[1024];
		SDL_snprintf(
			buffer,
			sizeof(buffer),
			"\"LEGO® Island\" failed to start.\nPlease make sure the file %s is located in either diskpath or "
			"cdpath.%s",
			missing,
			attempts.GetData()
		);

		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", buffer);
		SDL_strlcpy(g_startupError, buffer, sizeof(g_startupError));
		return FAILURE;
	}
#endif

	return SUCCESS;
}

void IsleApp::DetectGameVersion()
{
	const char* file = "/lego/scripts/infocntr/infomain.si";
	SDL_PathInfo info;
	bool success = false;

	MxString path = MxString(m_hdPath) + file;
	path.MapPathToFilesystem();
	if (!(success = SDL_GetPathInfo(path.GetData(), &info))) {
		path = MxString(m_cdPath) + file;
		path.MapPathToFilesystem();
		success = SDL_GetPathInfo(path.GetData(), &info);
	}

	assert(success);

	// File sizes of INFOMAIN.SI in English 1.0 and Japanese 1.0
	Lego()->SetVersion10(info.size == 58130432 || info.size == 57737216);

	if (Lego()->IsVersion10()) {
		SDL_Log("Detected game version 1.0");
		SDL_SetWindowTitle(reinterpret_cast<SDL_Window*>(m_windowHandle), "Lego Island");
	}
	else {
		SDL_Log("Detected game version 1.1");
	}
}

IDirect3DRMMiniwinDevice* GetD3DRMMiniwinDevice()
{
	LegoVideoManager* videoManager = LegoOmni::GetInstance()->GetVideoManager();
	if (!videoManager) {
		return nullptr;
	}
	Lego3DManager* lego3DManager = videoManager->Get3DManager();
	if (!lego3DManager) {
		return nullptr;
	}
	Lego3DView* lego3DView = lego3DManager->GetLego3DView();
	if (!lego3DView) {
		return nullptr;
	}
	TglImpl::DeviceImpl* tgl_device = (TglImpl::DeviceImpl*) lego3DView->GetDevice();
	if (!tgl_device) {
		return nullptr;
	}
	IDirect3DRMDevice2* d3drmdev = tgl_device->ImplementationData();
	if (!d3drmdev) {
		return nullptr;
	}
	IDirect3DRMMiniwinDevice* d3drmMiniwinDev = nullptr;
	if (!SUCCEEDED(d3drmdev->QueryInterface(IID_IDirect3DRMMiniwinDevice, (void**) &d3drmMiniwinDev))) {
		return nullptr;
	}
	return d3drmMiniwinDev;
}

void IsleApp::MoveVirtualMouseViaJoystick()
{
	// Cursor sensitivity is expressed in pixels per 60 Hz frame
	static Uint64 g_lastMoveTime = 0;
	Uint64 now = SDL_GetTicksNS();
	float frames = 1.0f;
	if (g_lastMoveTime != 0) {
		frames = SDL_min((float) (now - g_lastMoveTime), SDL_NS_PER_SECOND / 10.0f) * (60.0f / SDL_NS_PER_SECOND);
	}
	g_lastMoveTime = now;

	float dpadX = 0.0f;
	float dpadY = 0.0f;

#ifndef __3DS__
	if (g_dpadLeft) {
		dpadX -= m_cursorSensitivity;
	}
	if (g_dpadRight) {
		dpadX += m_cursorSensitivity;
	}
	if (g_dpadUp) {
		dpadY -= m_cursorSensitivity;
	}
	if (g_dpadDown) {
		dpadY += m_cursorSensitivity;
	}
#endif

	// Use joystick axis if non-zero, else fall back to dpad
	float moveX = ((g_lastJoystickMouseX != 0) ? g_lastJoystickMouseX : dpadX) * frames;
	float moveY = ((g_lastJoystickMouseY != 0) ? g_lastJoystickMouseY : dpadY) * frames;

	if (moveX != 0 || moveY != 0) {
		g_mousemoved = TRUE;

		g_lastMouseX = SDL_clamp(g_lastMouseX + moveX, 0, g_targetWidth);
		g_lastMouseY = SDL_clamp(g_lastMouseY + moveY, 0, g_targetHeight);

		if (InputManager()) {
			InputManager()->QueueEvent(
				c_notificationMouseMove,
				g_mousedown ? LegoEventNotificationParam::c_lButtonState : 0,
				g_lastMouseX,
				g_lastMouseY,
				0
			);
		}

		SDL_HideCursor();
		g_isle->SetDrawCursor(TRUE);
		if (VideoManager()) {
			VideoManager()->SetCursorBitmap(m_cursorCurrentBitmap);
			VideoManager()->MoveCursor(Min((MxS32) g_lastMouseX, 639), Min((MxS32) g_lastMouseY, 479));
		}
		IDirect3DRMMiniwinDevice* device = GetD3DRMMiniwinDevice();
		if (device) {
			Sint32 x, y;
			device->ConvertRenderToWindowCoordinates(g_lastMouseX, g_lastMouseY, x, y);
			g_mouseWarped = TRUE;
			SDL_WarpMouseInWindow(window, x, y);
		}
	}
}

void IsleApp::DetectDoubleTap(const SDL_TouchFingerEvent& p_event)
{
#ifdef ANDROID
	if (g_androidTouchInput.DoubleTap(p_event) && InputManager()) {
		InputManager()->QueueEvent(c_notificationKeyPress, SDLK_SPACE, 0, 0, SDLK_SPACE);
	}
#else
	typedef std::pair<Uint64, std::array<float, 2>> LastTap;

	const MxU32 doubleTapMs = 500;
	const float doubleTapDist = 0.001;
	static LastTap lastTap = {0, {0, 0}};

	LastTap currentTap = {p_event.timestamp, {p_event.x, p_event.y}};
	if (SDL_NS_TO_MS(currentTap.first - lastTap.first) < doubleTapMs &&
		DISTSQRD2(currentTap.second, lastTap.second) < doubleTapDist) {

		if (InputManager()) {
			InputManager()->QueueEvent(c_notificationKeyPress, SDLK_SPACE, 0, 0, SDLK_SPACE);
		}

		lastTap = {0, {0, 0}};
	}
	else {
		lastTap = currentTap;
	}
#endif
}
