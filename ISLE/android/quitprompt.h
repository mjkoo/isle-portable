#ifndef ANDROID_QUITPROMPT_H
#define ANDROID_QUITPROMPT_H

// Asks whether to quit to Android, returning true when the user confirmed. Blocks until the
// user answers, pumping the event queue throughout: the Android UI thread cannot run the
// dialog, service a surface teardown, or deliver a lifecycle event while this thread is idle.
// The caller must therefore treat everything the game owns as having moved on when this
// returns, and must have saved before calling.
bool Android_ConfirmQuit();

#endif // ANDROID_QUITPROMPT_H
