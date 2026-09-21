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

    private static void check(int result, String error, String message, String positive) {
        QuitPromptText text = new QuitPromptText(result, error);
        assert text.title.equals(error == null ? "LEGO Island" : "LEGO Island could not start");
        assert text.message.equals(message);
        assert text.positive.equals(positive);
        assert text.neutral.equals("Settings");
        assert !text.positive.equals("Save and quit")
            || (error == null && result == QuitPromptText.SAVE_ATTEMPTED);
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

        check(QuitPromptText.SAVE_ATTEMPTED, null, "Game paused.", "Quit");
        check(QuitPromptText.SAVE_NOTHING_TO_SAVE, null, "There is no saved game yet.", "Quit");
        check(QuitPromptText.SAVE_FAILED, null, "Your game could not be saved.", "Quit");
        check(-1, null, "There is no saved game yet.", "Quit");
        check(99, null, "There is no saved game yet.", "Quit");
        for (int result : new int[] {0, 1, 2, -1, 99}) {
            check(result, "Missing game data.", "Missing game data.", "Close");
            check(result, "", "", "Close");
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
