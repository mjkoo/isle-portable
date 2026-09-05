#include "filepicker.h"

#include <SDL3/SDL.h>
#include <errno.h>
#include <iniparser.h>
#include <jni.h>
#include <stdio.h>
#include <string.h>

struct FolderDialogResult {
	SDL_Mutex* m_mutex;
	bool m_done;
	char* m_path;
};

static void SDLCALL OnFolderSelected(void* p_userdata, const char* const* p_filelist, int p_filter)
{
	FolderDialogResult* result = static_cast<FolderDialogResult*>(p_userdata);

	SDL_LockMutex(result->m_mutex);
	if (p_filelist && p_filelist[0]) {
		result->m_path = SDL_strdup(p_filelist[0]);
	}
	else if (!p_filelist) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Folder dialog error: %s", SDL_GetError());
	}
	result->m_done = true;
	SDL_UnlockMutex(result->m_mutex);
}

// A SAF session sits over the surface for as long as the user takes, and Android keeps
// feeding touches through the whole time. Pump so the drivers make progress, then drop the
// input that was just queued: nothing is drawing yet, and the game would otherwise receive
// the entire burst at once the moment it starts.
//
// Only the input ranges are flushed. Lifecycle events are unaffected either way, since SDL
// hands those straight to the event watchers rather than queueing them (see the comment on
// SDL_AddEventWatch in isleapp.cpp), and leaving the 0x100 and 0x200 ranges alone keeps the
// SDL_EVENT_QUIT that Android_OnDestroy queues on its way to SDL_AppEvent.
static void DrainInputEvents()
{
	static const struct {
		SDL_EventType m_first;
		SDL_EventType m_last;
	} ranges[] = {
		{SDL_EVENT_KEYBOARD_FIRST, SDL_EVENT_KEYBOARD_LAST},
		{SDL_EVENT_MOUSE_FIRST, SDL_EVENT_MOUSE_LAST},
		{SDL_EVENT_JOYSTICK_FIRST, SDL_EVENT_JOYSTICK_LAST},
		{SDL_EVENT_GAMEPAD_FIRST, SDL_EVENT_GAMEPAD_LAST},
		{SDL_EVENT_FINGER_FIRST, SDL_EVENT_FINGER_LAST},
		{SDL_EVENT_PINCH_FIRST, SDL_EVENT_PINCH_LAST},
	};

	SDL_PumpEvents();

	for (const auto& range : ranges) {
		SDL_FlushEvents(range.m_first, range.m_last);
	}
}

static char* ShowFolderDialog(SDL_Window* p_window)
{
	FolderDialogResult result = {SDL_CreateMutex(), false, NULL};
	SDL_ShowOpenFolderDialog(OnFolderSelected, &result, p_window, NULL, false);

	for (;;) {
		DrainInputEvents();
		SDL_LockMutex(result.m_mutex);
		bool done = result.m_done;
		SDL_UnlockMutex(result.m_mutex);
		if (done) {
			break;
		}
		SDL_Delay(100);
	}

	SDL_DestroyMutex(result.m_mutex);
	return result.m_path;
}

// Mirrors GameImport's STATUS_ constants; keep the numbering in step.
enum ImportStatus {
	e_importOk = 0,
	e_importCancelled = 1,
	e_importNoSpace = 2,
	e_importNotGameFolder = 3,
	e_importReadFailed = 4,
	e_importWriteFailed = 5,
	e_importInternalError = 6,
};

struct ActivityCall {
	JNIEnv* m_env;
	jobject m_activity;
	jclass m_class;
	jmethodID m_method;
};

// Resolves a method on the SDL activity. On success the caller must pass p_call to EndActivityCall.
static bool BeginActivityCall(ActivityCall* p_call, const char* p_name, const char* p_signature)
{
	p_call->m_env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
	p_call->m_activity = static_cast<jobject>(SDL_GetAndroidActivity());
	p_call->m_class = NULL;
	p_call->m_method = NULL;

	if (!p_call->m_env || !p_call->m_activity) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No JNI environment for IsleActivity.%s", p_name);
		return false;
	}

	p_call->m_class = p_call->m_env->GetObjectClass(p_call->m_activity);
	p_call->m_method = p_call->m_env->GetMethodID(p_call->m_class, p_name, p_signature);

	if (!p_call->m_method) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "IsleActivity.%s not found", p_name);
		p_call->m_env->ExceptionClear();
		p_call->m_env->DeleteLocalRef(p_call->m_class);
		p_call->m_env->DeleteLocalRef(p_call->m_activity);
		return false;
	}

	return true;
}

// Returns true when the call completed without a pending Java exception.
static bool EndActivityCall(ActivityCall* p_call)
{
	bool threw = p_call->m_env->ExceptionCheck();
	if (threw) {
		p_call->m_env->ExceptionDescribe();
		p_call->m_env->ExceptionClear();
	}

	p_call->m_env->DeleteLocalRef(p_call->m_class);
	p_call->m_env->DeleteLocalRef(p_call->m_activity);
	return !threw;
}

static ImportStatus ImportGameFiles(const char* p_treeUri)
{
	ActivityCall call;
	if (!BeginActivityCall(&call, "importGameFiles", "(Ljava/lang/String;)I")) {
		return e_importInternalError;
	}

	jstring treeUri = call.m_env->NewStringUTF(p_treeUri);
	jint status = call.m_env->CallIntMethod(call.m_activity, call.m_method, treeUri);
	call.m_env->DeleteLocalRef(treeUri);

	if (!EndActivityCall(&call)) {
		return e_importInternalError;
	}

	return static_cast<ImportStatus>(status);
}

// The directory the copy landed in. Kept separate from the status on purpose: taking a returned
// path as proof of success is what let an import that copied nothing report the directory
// diskpath already named, and count as progress.
static char* GetImportedRoot()
{
	ActivityCall call;
	if (!BeginActivityCall(&call, "getImportedRoot", "()Ljava/lang/String;")) {
		return NULL;
	}

	jstring root = static_cast<jstring>(call.m_env->CallObjectMethod(call.m_activity, call.m_method));
	char* importedRoot = NULL;

	if (root) {
		const char* utf = call.m_env->GetStringUTFChars(root, NULL);
		if (utf) {
			importedRoot = SDL_strdup(utf);
			call.m_env->ReleaseStringUTFChars(root, utf);
		}
		call.m_env->DeleteLocalRef(root);
	}

	if (!EndActivityCall(&call)) {
		SDL_free(importedRoot);
		return NULL;
	}

	return importedRoot;
}

// isle.ini is small, it is the only record of where the game data went, and since it is
// covered by the backup rules it is also what a restored install starts from. Write a
// sibling and rename over the original rather than truncating the file we still need: an
// interrupted dump would otherwise leave an empty config behind.
static void UpdateConfigDiskPath(const char* p_iniPath, const char* p_diskPath)
{
	char* iniConfig;
	if (p_iniPath) {
		iniConfig = SDL_strdup(p_iniPath);
	}
	else {
		SDL_asprintf(&iniConfig, "%s/isle.ini", SDL_GetAndroidExternalStoragePath());
	}

	dictionary* dict = iniparser_load(iniConfig);
	if (dict) {
		char* iniTemp;
		SDL_asprintf(&iniTemp, "%s.new", iniConfig);

		FILE* iniFP = fopen(iniTemp, "wb");
		if (iniFP) {
			iniparser_set(dict, "isle:diskpath", p_diskPath);
			iniparser_dump_ini(dict, iniFP);

			bool written = fflush(iniFP) == 0;
			fclose(iniFP);

			if (written && SDL_RenamePath(iniTemp, iniConfig)) {
				SDL_Log("Updated diskpath to '%s' in config at '%s'", p_diskPath, iniConfig);
			}
			else {
				SDL_LogError(
					SDL_LOG_CATEGORY_APPLICATION,
					"Failed to replace config at '%s': %s",
					iniConfig,
					written ? SDL_GetError() : strerror(errno)
				);
				SDL_RemovePath(iniTemp);
			}
		}
		else {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to write config at '%s': %s", iniTemp, strerror(errno));
		}

		SDL_free(iniTemp);
		iniparser_freedict(dict);
	}
	else {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load config at '%s'", iniConfig);
	}

	SDL_free(iniConfig);
}

bool Android_TryImportGameFiles(
	SDL_Window* p_window,
	const char* p_iniPath,
	char** p_hdPath,
	const char* p_missingFile,
	int p_attempt
)
{
	const char* missing = p_missingFile ? p_missingFile : "a game file";
	char message[1024];

	if (p_attempt == 0) {
		SDL_snprintf(
			message,
			sizeof(message),
			"The game files could not be found or read (%s is missing).\n\n"
			"If you have a copy of the LEGO® Island files on this device (in a regular folder such as Download), "
			"you can select the folder containing them and they will be copied into this app's storage.",
			missing
		);
	}
	else {
		// A retry only helps if the user picks a different folder, so say what was wrong with
		// the last one rather than repeating the invitation.
		SDL_snprintf(
			message,
			sizeof(message),
			"The game files are still incomplete: %s was not found after copying.\n\n"
			"Select the folder that contains the LEGO folder from your LEGO® Island installation, "
			"rather than a folder inside it.",
			missing
		);
	}

	const SDL_MessageBoxButtonData buttons[] = {
		{SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Select folder"},
		{SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel"},
	};
	const SDL_MessageBoxData messageBox =
		{SDL_MESSAGEBOX_INFORMATION, p_window, "LEGO® Island", message, SDL_arraysize(buttons), buttons, NULL};

	int button = 0;
	if (!SDL_ShowMessageBox(&messageBox, &button) || button != 1) {
		return false;
	}

	char* treeUri = ShowFolderDialog(p_window);
	if (!treeUri) {
		return false;
	}

	ImportStatus status = ImportGameFiles(treeUri);
	SDL_free(treeUri);
	DrainInputEvents();

	if (status != e_importOk) {
		// Java has already told the user what went wrong: it owns the copy-level errors,
		// because it has the dialog and the byte counts. Native only reports what the game
		// itself can tell, which is which file is still missing.
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Game file import failed with status %d", status);
		return false;
	}

	char* importedRoot = GetImportedRoot();
	if (!importedRoot) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Import reported success without a destination");
		return false;
	}

	if (SDL_strcmp(importedRoot, *p_hdPath) != 0) {
		SDL_free(*p_hdPath);
		*p_hdPath = importedRoot;
		UpdateConfigDiskPath(p_iniPath, importedRoot);
	}
	else {
		SDL_free(importedRoot);
	}

	return true;
}
