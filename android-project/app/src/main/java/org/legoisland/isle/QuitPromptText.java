package org.legoisland.isle;

/** Dependency-free wording for the game menu and startup recovery dialog. */
final class QuitPromptText {
    // Mirrors QuitPromptSaveResult in ISLE/android/quitprompt.h; keep the numbering in step.
    static final int SAVE_ATTEMPTED = 0;
    static final int SAVE_NOTHING_TO_SAVE = 1;
    static final int SAVE_FAILED = 2;

    final String title;
    final String message;
    final String negative;
    final String neutral = "Settings";
    final String positive;

    QuitPromptText(int saveResult, String startupError) {
        title = startupError == null ? "LEGO Island" : "LEGO Island could not start";
        negative = startupError == null ? "Resume" : null;
        if (startupError != null) {
            message = startupError;
            positive = "Close";
            return;
        }

        switch (saveResult) {
        case SAVE_ATTEMPTED:
            // The save API does not report every write failure, so do not promise persistence.
            message = "Game paused.";
            positive = "Save and quit";
            break;
        case SAVE_FAILED:
            message = "Your game could not be saved.";
            positive = "Quit anyway";
            break;
        default:
            message = "There is no saved game yet.";
            positive = "Quit";
            break;
        }
    }
}
