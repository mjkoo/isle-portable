package org.legoisland.isle;

import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class QuitPromptTextTest {
    private static String read(String path) throws Exception {
        return new String(Files.readAllBytes(Paths.get(path)), StandardCharsets.UTF_8);
    }

    private static int constant(String source, String name) {
        Matcher match = Pattern.compile("\\b" + name + "\\s*=\\s*(-?\\d+)\\s*[,;]").matcher(source);
        assert match.find() : name;
        return Integer.parseInt(match.group(1));
    }

    private static void check(int result, String name, String error, String message, String positive) {
        QuitPromptText text = new QuitPromptText(result, name, error);
        assert text.title.equals(error == null ? "LEGO Island" : "LEGO Island could not start");
        assert text.message.equals(message);
        assert text.positive.equals(positive);
        assert text.neutral.equals("Settings");
        // Quitting does not save; the save already happened before the menu came up.
        assert !text.positive.toLowerCase().contains("save");
        if (error == null) {
            assert "Resume".equals(text.negative);
        } else {
            assert text.negative == null;
            assert text.positive.equals("Close");
        }
    }

    public static void main(String[] args) throws Exception {
        boolean assertions = false;
        assert assertions = true;
        if (!assertions) throw new AssertionError("Run with java -ea");

        String notSignedIn = "Game paused. Nothing is saved until you sign in at the Information Center.";
        check(QuitPromptText.SAVE_ATTEMPTED, "PEPPER", null, "Game paused. Signed in as PEPPER.", "Quit");
        // Native always names a player once a save was attempted, but a missing name must not
        // read as a sentence with a hole in it.
        check(QuitPromptText.SAVE_ATTEMPTED, null, null, "Game paused.", "Quit");
        check(QuitPromptText.SAVE_ATTEMPTED, "", null, "Game paused.", "Quit");
        check(QuitPromptText.SAVE_NOTHING_TO_SAVE, null, null, notSignedIn, "Quit");
        check(QuitPromptText.SAVE_FAILED, "PEPPER", null, "Your game could not be saved.", "Quit anyway");
        check(-1, null, null, notSignedIn, "Quit");
        check(99, null, null, notSignedIn, "Quit");
        for (int result : new int[] {0, 1, 2, -1, 99}) {
            check(result, "PEPPER", "Missing game data.", "Missing game data.", "Close");
            check(result, null, "", "", "Close");
        }

        String nativeSave = read("ISLE/android/quitprompt.h");
        assert constant(nativeSave, "e_quitPromptSaveAttempted") == QuitPromptText.SAVE_ATTEMPTED;
        assert constant(nativeSave, "e_quitPromptNothingToSave") == QuitPromptText.SAVE_NOTHING_TO_SAVE;
        assert constant(nativeSave, "e_quitPromptSaveFailed") == QuitPromptText.SAVE_FAILED;

        // Read the Android-dependent class as text so this test remains standalone.
        String javaStatus = read("android-project/app/src/main/java/org/legoisland/isle/QuitPrompt.java");
        String nativeStatus = read("ISLE/android/quitprompt.cpp");
        assert constant(nativeStatus, "e_quitPromptPending") == constant(javaStatus, "STATUS_PENDING");
        assert constant(nativeStatus, "e_quitPromptResume") == constant(javaStatus, "STATUS_RESUME");
        assert constant(nativeStatus, "e_quitPromptQuit") == constant(javaStatus, "STATUS_QUIT");
    }
}
