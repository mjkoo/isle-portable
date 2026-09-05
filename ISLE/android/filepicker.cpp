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

static char* ImportGameFiles(const char* p_treeUri)
{
	JNIEnv* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
	jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
	char* importedRoot = NULL;

	if (env && activity) {
		jclass activityClass = env->GetObjectClass(activity);
		jmethodID importGameFiles =
			env->GetMethodID(activityClass, "importGameFiles", "(Ljava/lang/String;)Ljava/lang/String;");

		if (importGameFiles) {
			jstring treeUri = env->NewStringUTF(p_treeUri);
			jstring destRoot = static_cast<jstring>(env->CallObjectMethod(activity, importGameFiles, treeUri));
			if (env->ExceptionCheck()) {
				env->ExceptionDescribe();
				env->ExceptionClear();
				destRoot = NULL;
			}

			if (destRoot) {
				const char* utf = env->GetStringUTFChars(destRoot, NULL);
				if (utf) {
					importedRoot = SDL_strdup(utf);
					env->ReleaseStringUTFChars(destRoot, utf);
				}
				env->DeleteLocalRef(destRoot);
			}

			env->DeleteLocalRef(treeUri);
		}
		else {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "IsleActivity.importGameFiles not found");
			env->ExceptionClear();
		}

		env->DeleteLocalRef(activityClass);
		env->DeleteLocalRef(activity);
	}

	return importedRoot;
}

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
		FILE* iniFP = fopen(iniConfig, "wb");
		if (iniFP) {
			iniparser_set(dict, "isle:diskpath", p_diskPath);
			iniparser_dump_ini(dict, iniFP);
			fclose(iniFP);
			SDL_Log("Updated diskpath to '%s' in config at '%s'", p_diskPath, iniConfig);
		}
		else {
			SDL_LogError(
				SDL_LOG_CATEGORY_APPLICATION,
				"Failed to write config at '%s': %s",
				iniConfig,
				strerror(errno)
			);
		}
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

	char* importedRoot = ImportGameFiles(treeUri);
	SDL_free(treeUri);
	DrainInputEvents();

	if (!importedRoot) {
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
