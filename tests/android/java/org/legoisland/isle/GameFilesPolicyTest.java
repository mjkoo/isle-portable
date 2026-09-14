package org.legoisland.isle;

import java.io.File;
import java.nio.file.Files;

public final class GameFilesPolicyTest {
    static void check(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }

    public static void main(String[] args) throws Exception {
        GameFilesPolicy.Sizes bytes = size -> size + " B";
        long margin = GameFilesPolicy.SPACE_MARGIN_BYTES;

        // The new files and the margin must fit beside the current files, which count for nothing.
        check(GameFilesPolicy.required(1000) == 1000 + margin, "margin added");
        check(GameFilesPolicy.required(Long.MAX_VALUE) == Long.MAX_VALUE, "no overflow");
        check(GameFilesPolicy.spaceRefusal(1000, 1000 + margin, bytes) == null, "exact fit allowed");
        String refusal = GameFilesPolicy.spaceRefusal(1000, 999 + margin, bytes);
        check(refusal != null, "one byte short refused");
        check(refusal.contains((1000 + margin) + " B free") && refusal.contains("1000 B for the new files")
            && refusal.contains(margin + " B to spare") && refusal.contains("Only " + (999 + margin) + " B is free"),
            "refusal gives every number: " + refusal);
        check(refusal.contains("Remove game files"), "refusal points at Remove");

        // Unset, the root, and imports directly inside it are app storage; anything else was set by hand.
        File root = new File("/storage/emulated/0/Android/data/org.legoisland.isle/files");
        check(GameFilesPolicy.inAppStorage(null, root) && GameFilesPolicy.inAppStorage("", root), "unset is the root");
        check(GameFilesPolicy.inAppStorage(root.getPath(), root), "root");
        check(GameFilesPolicy.inAppStorage(root.getPath() + "/", root), "root with a trailing slash");
        check(GameFilesPolicy.inAppStorage(root.getPath() + "/imported-17", root), "earlier import");
        check(!GameFilesPolicy.inAppStorage(root.getPath() + "/other", root), "other child");
        check(!GameFilesPolicy.inAppStorage(root.getPath() + "/imported-17/LEGO", root), "inside an import");
        check(!GameFilesPolicy.inAppStorage("/sdcard/isle", root), "custom location");
        check(GameFilesPolicy.location("", root).equals(root), "unset reads the root");
        File real = Files.createTempDirectory("game-files-policy-root").toFile();
        File alias = new File(real.getParentFile(), real.getName() + "-alias");
        Files.createSymbolicLink(alias.toPath(), real.toPath());
        check(GameFilesPolicy.inAppStorage(alias.getPath(), real), "a link to the root is the root");
        check(GameFilesPolicy.inAppStorage(alias.getPath() + "/imported-2", real), "an import through a link");
        alias.delete();
        real.delete();

        // Only what the game would glob counts: top-level entries starting with lego, any case.
        File dir = Files.createTempDirectory("game-files-policy-test").toFile();
        check(GameFilesPolicy.measure(dir) == -1, "empty directory has no game files");
        check(GameFilesPolicy.measure(new File(dir, "absent")) == -1, "missing directory has no game files");
        Files.createDirectories(new File(dir, "LEGO/Scripts").toPath());
        Files.write(new File(dir, "LEGO/Scripts/INTRO.SI").toPath(), new byte[10]);
        Files.write(new File(dir, "lego.txt").toPath(), new byte[5]);
        Files.write(new File(dir, "other").toPath(), new byte[100]);
        check(GameFilesPolicy.measure(dir) == 15, "lego entries measured");

        check(GameFilesPolicy.summary("", root, 15, false, bytes).equals("App storage, 15 B"), "app storage summary");
        check(GameFilesPolicy.summary("/sdcard/isle", root, 15, false, bytes).equals("/sdcard/isle, 15 B (custom location)"),
            "custom summary");
        check(GameFilesPolicy.summary("/sdcard/isle", root, -1, false, bytes).equals("No game files found in /sdcard/isle."),
            "missing custom summary");
        check(GameFilesPolicy.summary("", root, -1, false, bytes).equals("No game files found in app storage."),
            "missing app storage summary");
        check(GameFilesPolicy.summary("", root, 15, true, bytes).contains("waiting"), "pending summary");

        String custom = GameFilesPolicy.replaceConfirmation(46, 15, "/sdcard/isle", root, bytes);
        check(custom.contains("46 files (15 B)") && custom.contains("instead of /sdcard/isle"), "custom confirmation");
        check(!GameFilesPolicy.replaceConfirmation(46, 15, "", root, bytes).contains("instead of"), "app storage confirmation");
        check(GameFilesPolicy.removeConfirmation(15, bytes).contains("(15 B)"), "remove confirmation");

        GameFileCopier.deleteRecursively(dir);
        System.out.println("Game files space, location, measurement and wording tests passed.");
    }
}
