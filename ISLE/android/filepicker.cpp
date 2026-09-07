#include "filepicker.h"

#include "activity.h"

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

static char* ShowFolderDialog(SDL_Window* p_window)
{
	FolderDialogResult result = {SDL_CreateMutex(), false, NULL};
	SDL_ShowOpenFolderDialog(OnFolderSelected, &result, p_window, NULL, false);

	for (;;) {
		Android_DrainInputEvents();
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
	e_importRunning = -1,
	e_importOk = 0,
	e_importCancelled = 1,
	e_importNoSpace = 2,
	e_importNotGameFolder = 3,
	e_importReadFailed = 4,
	e_importWriteFailed = 5,
	e_importInternalError = 6,
};

static bool StartImportGameFiles(const char* p_treeUri)
{
	Android_ActivityCall call;
	if (!Android_BeginActivityCall(&call, "startGameFileImport", "(Ljava/lang/String;)V")) {
		return false;
	}

	jstring treeUri = call.m_env->NewStringUTF(p_treeUri);
	call.m_env->CallVoidMethod(call.m_activity, call.m_method, treeUri);
	call.m_env->DeleteLocalRef(treeUri);

	return Android_EndActivityCall(&call);
}

static ImportStatus GetImportStatus()
{
	Android_ActivityCall call;
	if (!Android_BeginActivityCall(&call, "getGameFileImportStatus", "()I")) {
		return e_importInternalError;
	}

	jint status = call.m_env->CallIntMethod(call.m_activity, call.m_method);
	if (!Android_EndActivityCall(&call)) {
		return e_importInternalError;
	}

	return static_cast<ImportStatus>(status);
}

// Runs the import to completion. Deliberately a start plus a poll rather than one blocking call:
// the Android UI thread can be waiting on this one to service the surface teardown the file
// picker left behind, and until we pump, nothing on the UI thread runs at all - not even the
// import's own progress dialog. Pumping here also keeps lifecycle events flowing through a copy
// that takes minutes. Same shape as ShowFolderDialog above.
static ImportStatus ImportGameFiles(const char* p_treeUri)
{
	if (!StartImportGameFiles(p_treeUri)) {
		return e_importInternalError;
	}

	for (;;) {
		Android_DrainInputEvents();

		ImportStatus status = GetImportStatus();
		if (status != e_importRunning) {
			return status;
		}

		SDL_Delay(100);
	}
}

// The directory the copy landed in. Kept separate from the status on purpose: taking a returned
// path as proof of success is what let an import that copied nothing report the directory
// diskpath already named, and count as progress.
static char* GetImportedRoot()
{
	Android_ActivityCall call;
	if (!Android_BeginActivityCall(&call, "getImportedRoot", "()Ljava/lang/String;")) {
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

	if (!Android_EndActivityCall(&call)) {
		SDL_free(importedRoot);
		return NULL;
	}

	return importedRoot;
}

static bool HasImportedGameData()
{
	return Android_CallActivityBooleanMethod("hasImportedGameData");
}

static bool RemoveImportedGameData()
{
	return Android_CallActivityBooleanMethod("removeImportedGameData");
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
		char* prefPath = SDL_GetPrefPath("isledecomp", "isle");
		if (!prefPath || !*prefPath) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Internal config directory unavailable: %s", SDL_GetError());
			SDL_free(prefPath);
			return;
		}
		SDL_asprintf(&iniConfig, "%sisle.ini", prefPath);
		SDL_free(prefPath);
	}

	// SDL_asprintf leaves the pointer NULL when it fails, and so would a failed SDL_strdup.
	if (!iniConfig) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Out of memory building the config path");
		return;
	}

	dictionary* dict = iniparser_load(iniConfig);
	if (dict) {
		char* iniTemp;
		SDL_asprintf(&iniTemp, "%s.new", iniConfig);

		FILE* iniFP = iniTemp ? fopen(iniTemp, "wb") : NULL;
		if (iniFP) {
			iniparser_set(dict, "isle:diskpath", p_diskPath);
			iniparser_dump_ini(dict, iniFP);

			// fflush reports the write errors, fclose whatever the close itself hits; the
			// rename must not happen unless both came back clean. Keep the first errno, since
			// the second call overwrites it.
			bool written = fflush(iniFP) == 0;
			int writeErrno = errno;

			if (fclose(iniFP) != 0 && written) {
				written = false;
				writeErrno = errno;
			}

			if (written && SDL_RenamePath(iniTemp, iniConfig)) {
				SDL_Log("Updated diskpath to '%s' in config at '%s'", p_diskPath, iniConfig);
			}
			else {
				SDL_LogError(
					SDL_LOG_CATEGORY_APPLICATION,
					"Failed to replace config at '%s': %s",
					iniConfig,
					written ? SDL_GetError() : strerror(writeErrno)
				);
				SDL_RemovePath(iniTemp);
			}
		}
		else {
			SDL_LogError(
				SDL_LOG_CATEGORY_APPLICATION,
				"Failed to write config at '%s': %s",
				iniTemp ? iniTemp : iniConfig,
				strerror(errno)
			);
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

	// At most two rounds: removing the game data takes the third button away, so the second
	// prompt normally offers only a pick or a cancel. Removal can come back incomplete though,
	// leaving the button in place, so the pick is tracked rather than inferred from falling out
	// of the loop - that dropped the user into the folder picker unasked.
	bool picked = false;

	for (int prompt = 0; prompt < 2 && !picked; prompt++) {
		SDL_MessageBoxButtonData buttons[3];
		int count = 0;

		buttons[count++] = {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Select folder"};
		// Terse on purpose: SDL lays the buttons out in one horizontal row with no wrapping,
		// and a longer label here squeezes "Cancel" into three stacked lines.
		if (HasImportedGameData()) {
			buttons[count++] = {0, 2, "Remove data"};
		}
		buttons[count++] = {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel"};

		const SDL_MessageBoxData messageBox =
			{SDL_MESSAGEBOX_INFORMATION, p_window, "LEGO® Island", message, count, buttons, NULL};

		int button = 0;
		if (!SDL_ShowMessageBox(&messageBox, &button)) {
			return false;
		}

		if (button == 2) {
			if (RemoveImportedGameData()) {
				// diskpath may name an imported-<timestamp> root that no longer exists, and
				// isle.ini is part of the backup set, so leaving it would follow the user to
				// their next device. Put it back where a fresh install would look.
				const char* defaultRoot = SDL_GetAndroidExternalStoragePath();
				if (defaultRoot) {
					SDL_free(*p_hdPath);
					*p_hdPath = SDL_strdup(defaultRoot);
					UpdateConfigDiskPath(p_iniPath, defaultRoot);
				}
			}
			continue;
		}

		if (button != 1) {
			return false;
		}

		picked = true;
	}

	if (!picked) {
		return false;
	}

	char* treeUri = ShowFolderDialog(p_window);
	if (!treeUri) {
		return false;
	}

	ImportStatus status = ImportGameFiles(treeUri);
	SDL_free(treeUri);
	Android_DrainInputEvents();

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
