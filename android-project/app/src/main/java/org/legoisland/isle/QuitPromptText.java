package org.legoisland.isle;

/** Dependency-free wording for the game menu and startup recovery dialog. */
final class QuitPromptText {
    // Mirrors QuitPromptSaveResult in ISLE/android/quitprompt.h; keep the numbering in step.
    static final int SAVE_ATTEMPTED = 0;
    static final int SAVE_NOTHING_TO_SAVE = 1;
    static final int SAVE_FAILED = 2;

    final String title;
    final String message;
    /** Null for a startup error, where there is no game to go back to. */
    final String resume;
    final String settings = "Settings";
    final String quit;

    /**
     * playerName is whose progress the save was for, or null when no one has signed in. The game
     * writes nothing until someone has, and then loads their progress only when their name is
     * picked in the registration book, so the menu says which of the two applies.
     */
    QuitPromptText(int saveResult, String playerName, String startupError) {
        title = startupError == null ? "LEGO Island" : "LEGO Island could not start";
        resume = startupError == null ? "Resume" : null;
        if (startupError != null) {
            message = startupError;
            quit = "Close";
            return;
        }

        switch (saveResult) {
        case SAVE_ATTEMPTED:
            // The save API does not report every write failure, so do not promise persistence;
            // say who is signed in, which is what the save was made for.
            message = playerName == null || playerName.isEmpty()
                ? "Game paused."
                : "Game paused. Signed in as " + playerName + ".";
            quit = "Quit";
            break;
        case SAVE_FAILED:
            message = "Your game could not be saved.";
            quit = "Quit anyway";
            break;
        default:
            message = "Game paused. Nothing is saved until you sign in at the Information Center.";
            quit = "Quit";
            break;
        }
    }
}
