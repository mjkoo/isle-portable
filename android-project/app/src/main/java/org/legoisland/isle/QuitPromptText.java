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
     * playerName is whose progress the save was for. When no one has signed in, it is whoever
     * played last, or null when no one ever has. The game writes nothing until someone signs in,
     * and loads a returning player's progress only when their name is picked in the registration
     * book, so the menu says which applies.
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
            // Every launch starts signed out, so a returning player sees this too. Their earlier
            // progress is not lost, and saying nothing is saved would read as if it were.
            message = playerName == null || playerName.isEmpty()
                ? "Game paused. Nothing is saved until you sign in at the Information Center."
                : "Game paused. To continue as " + playerName
                    + ", pick that name in the registration book at the Information Center.";
            quit = "Quit";
            break;
        }
    }
}
