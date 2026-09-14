package org.legoisland.isle;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public final class GameFileCopierTest {
    interface Operation { void run() throws Exception; }
    static int fails(Operation operation) throws Exception {
        try { operation.run(); }
        catch (GameFileCopier.Failure failure) { return failure.status; }
        throw new AssertionError("Expected the copy to fail");
    }
    static void check(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }

    /** A folder on disk, the way a document provider presents one. */
    static class FileSource implements GameFileCopier.Source {
        final File mRoot;
        FileSource(File root) { mRoot = root; }
        @Override public GameFileCopier.Node root() { return node(mRoot); }
        @Override public List<GameFileCopier.Node> list(GameFileCopier.Node directory) {
            File[] children = new File(directory.id).listFiles();
            if (children == null) return null;
            List<GameFileCopier.Node> nodes = new ArrayList<>();
            for (File child : children) nodes.add(node(child));
            return nodes;
        }
        @Override public InputStream open(GameFileCopier.Node file) throws IOException { return new FileInputStream(file.id); }
        GameFileCopier.Node node(File file) {
            return new GameFileCopier.Node(file.getPath(), file.getName(), file.isDirectory(), file.isDirectory() ? -1 : file.length());
        }
    }

    /** One LEGO folder holding the named files, each containing "x". */
    static final class NamedSource implements GameFileCopier.Source {
        final List<String> mNames;
        NamedSource(List<String> names) { mNames = names; }
        @Override public GameFileCopier.Node root() { return new GameFileCopier.Node("root", "", true, -1); }
        @Override public List<GameFileCopier.Node> list(GameFileCopier.Node directory) {
            List<GameFileCopier.Node> nodes = new ArrayList<>();
            if (directory.id.equals("root")) nodes.add(new GameFileCopier.Node("lego", "LEGO", true, -1));
            else for (String name : mNames) nodes.add(new GameFileCopier.Node(name, name, false, 1));
            return nodes;
        }
        @Override public InputStream open(GameFileCopier.Node file) { return new ByteArrayInputStream(new byte[] {'x'}); }
    }

    static void write(File file, String text) throws IOException {
        file.getParentFile().mkdirs();
        Files.write(file.toPath(), text.getBytes(StandardCharsets.UTF_8));
    }
    static String read(File file) throws IOException {
        return new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8);
    }
    /** A small installation: Scripts and data, one file nested a level deeper. 14 bytes in all. */
    static void install(File lego) throws IOException {
        write(new File(lego, "Scripts/INTRO.SI"), "intro");
        write(new File(lego, "Scripts/Act2/ACT2MAIN.SI"), "act2");
        write(new File(lego, "data/WORLD.WDB"), "world");
    }
    static GameFileCopier copier(File root) {
        return new GameFileCopier(new FileSource(root), () -> false);
    }

    public static void main(String[] args) throws Exception {
        File base = Files.createTempDirectory("game-file-copier-test").toFile();

        // An exact LEGO folder wins over a longer lego* name, and lands where the caller says.
        File picked = new File(base, "picked");
        install(new File(picked, "LEGO"));
        write(new File(picked, "LEGO Island/Scripts/INTRO.SI"), "other");
        GameFileCopier exact = copier(picked);
        exact.scan();
        check(exact.totalFiles() == 3 && exact.totalBytes() == 14, "totals cover the LEGO folder only");
        File dest = new File(base, "dest/LEGO");
        exact.copy(dest, true);
        check(read(new File(dest, "Scripts/INTRO.SI")).equals("intro"), "exact LEGO folder copied");
        check(read(new File(dest, "Scripts/Act2/ACT2MAIN.SI")).equals("act2"), "nested file copied");
        check(read(new File(dest, "data/WORLD.WDB")).equals("world"), "data copied");
        check(exact.copiedFiles() == 3 && exact.copiedBytes() == 14, "progress reaches the totals");

        // Otherwise a lego* folder, or the picked folder itself when it holds Scripts and data.
        File prefixed = new File(base, "prefixed");
        install(new File(prefixed, "lego island"));
        GameFileCopier byPrefix = copier(prefixed);
        byPrefix.scan();
        check(byPrefix.totalFiles() == 3, "lego* folder chosen");
        File itself = new File(base, "itself");
        install(itself);
        GameFileCopier byContents = copier(itself);
        byContents.scan();
        check(byContents.totalFiles() == 3, "picked LEGO folder itself accepted");

        // Anything else is not a game folder.
        File scriptsOnly = new File(base, "scripts-only");
        write(new File(scriptsOnly, "Scripts/INTRO.SI"), "x");
        check(fails(() -> copier(scriptsOnly).scan()) == GameFileCopier.STATUS_NOT_GAME_FOLDER, "Scripts alone rejected");
        File empty = new File(base, "empty");
        new File(empty, "LEGO").mkdirs();
        check(fails(() -> copier(empty).scan()) == GameFileCopier.STATUS_NOT_GAME_FOLDER, "empty LEGO folder rejected");
        File nested = new File(base, "deep/LEGO");
        for (int i = 0; i <= GameFileCopier.MAX_DEPTH; i++) nested = new File(nested, "d");
        write(new File(nested, "f"), "x");
        check(fails(() -> copier(new File(base, "deep")).scan()) == GameFileCopier.STATUS_NOT_GAME_FOLDER, "too deep");
        List<String> crowd = new ArrayList<>();
        for (int i = 0; i <= GameFileCopier.MAX_FILES; i++) crowd.add("F" + i);
        check(fails(() -> new GameFileCopier(new NamedSource(crowd), () -> false).scan())
            == GameFileCopier.STATUS_NOT_GAME_FOLDER, "too many files");

        // Dotfiles and names that could escape the destination are skipped.
        GameFileCopier odd = new GameFileCopier(new NamedSource(Arrays.asList(".hidden", "..", "a/b", "c\\d", "ok")), () -> false);
        odd.scan();
        check(odd.totalFiles() == 1, "only usable names counted");
        File oddDest = new File(base, "odd/LEGO");
        odd.copy(oddDest, false);
        check(Arrays.equals(oddDest.list(), new String[] {"ok"}), "only usable names copied");

        // Cancelling during the copy deletes what was written.
        int[] opened = {0};
        GameFileCopier cancelled = new GameFileCopier(new FileSource(picked) {
            @Override public InputStream open(GameFileCopier.Node file) throws IOException { opened[0]++; return super.open(file); }
        }, () -> opened[0] > 0);
        cancelled.scan();
        File cancelledDest = new File(base, "cancelled/LEGO");
        check(fails(() -> cancelled.copy(cancelledDest, false)) == GameFileCopier.STATUS_CANCELLED, "cancel reported");
        check(!cancelledDest.exists(), "cancel removes the partial copy");

        // Provider failures map to what the player is told, and never leave a partial copy.
        Object[][] failures = {
            {null, GameFileCopier.STATUS_READ_FAILED},
            {new SecurityException("grant revoked"), GameFileCopier.STATUS_READ_FAILED},
            {new IOException("boom"), GameFileCopier.STATUS_WRITE_FAILED},
            {new IOException("write failed: ENOSPC (No space left on device)"), GameFileCopier.STATUS_NO_SPACE},
        };
        for (Object[] failure : failures) {
            GameFileCopier broken = new GameFileCopier(new FileSource(picked) {
                @Override public InputStream open(GameFileCopier.Node file) throws IOException {
                    if (failure[0] instanceof IOException) throw (IOException) failure[0];
                    if (failure[0] instanceof RuntimeException) throw (RuntimeException) failure[0];
                    return null;
                }
            }, () -> false);
            broken.scan();
            File brokenDest = new File(base, "broken/LEGO");
            check(fails(() -> broken.copy(brokenDest, false)) == (Integer) failure[1], "failure mapped: " + failure[0]);
            check(!brokenDest.exists(), "failure removes the partial copy");
        }
        check(GameFileCopier.isOutOfSpace(new IOException("No space left on device")), "ENOSPC text recognized");
        check(!GameFileCopier.isOutOfSpace(new IOException((String) null)), "no message is not ENOSPC");

        // A provider that promised more bytes than it delivered is a short copy.
        GameFileCopier shortCopy = new GameFileCopier(new FileSource(picked) {
            @Override GameFileCopier.Node node(File file) {
                GameFileCopier.Node node = super.node(file);
                return node.directory ? node : new GameFileCopier.Node(node.id, node.name, false, node.size + 1);
            }
        }, () -> false);
        shortCopy.scan();
        File shortDest = new File(base, "short/LEGO");
        check(fails(() -> shortCopy.copy(shortDest, false)) == GameFileCopier.STATUS_WRITE_FAILED, "short copy reported");
        check(!shortDest.exists(), "short copy removed");

        // Earlier imports are found and cleared; saves never are.
        File filesDir = new File(base, "files");
        install(new File(filesDir, "LEGO"));
        install(new File(filesDir, "imported-1/LEGO"));
        write(new File(filesDir, "INTRO.SI.unreadable.2"), "x");
        write(new File(filesDir, "saves/G0.GS"), "save");
        check(GameFileCopier.importedPaths(filesDir).size() == 3, "earlier imports found");
        check(GameFileCopier.removeImportedData(filesDir), "earlier imports cleared");
        check(!GameFileCopier.hasImportedData(filesDir), "nothing left to clear");
        check(read(new File(filesDir, "saves/G0.GS")).equals("save"), "saves kept");

        GameFileCopier.deleteRecursively(base);
        System.out.println("Game file copier source, copy, cancellation and failure tests passed.");
    }
}
