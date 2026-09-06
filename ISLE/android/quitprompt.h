#ifndef ANDROID_QUITPROMPT_H
#define ANDROID_QUITPROMPT_H

// Asks whether to quit to Android, returning true when the user confirmed. Blocks until the
// user answers, pumping the event queue throughout: the Android UI thread cannot run the
// dialog, service a surface teardown, or deliver a lifecycle event while this thread is idle.
// The caller must therefore treat everything the game owns as having moved on when this
// returns.
//
// p_abandoned is polled alongside the dialog, and answers false when it reports true. The
// dialog cannot always answer for itself: destroying the activity takes its window down
// without running either button callback, and pumping is exactly what lets that happen here,
// so a loop waiting only on the user is a loop that can wait forever.
// Mirrors QuitPrompt's SAVE_ constants; keep the numbering in step.
enum QuitPromptSaveResult {
	e_quitPromptSaveWritten = 0,
	e_quitPromptNothingToSave = 1,
	e_quitPromptSaveFailed = 2,
};

bool Android_ConfirmQuit(bool (*p_abandoned)(), QuitPromptSaveResult p_saveResult);

#endif // ANDROID_QUITPROMPT_H
