package org.legoisland.isle;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

public final class SaveValidationTest {
    interface Attempt { void run() throws Exception; }
    static void rejects(Attempt attempt) throws Exception {
        try { attempt.run(); throw new AssertionError("Expected rejection"); }
        catch (IOException expected) { }
    }
    static byte[] zip(Map<String, byte[]> files) throws Exception {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        try (ZipOutputStream zip = new ZipOutputStream(bytes)) {
            for (Map.Entry<String, byte[]> entry : files.entrySet()) {
                zip.putNextEntry(new ZipEntry(entry.getKey())); zip.write(entry.getValue()); zip.closeEntry();
            }
        }
        return bytes.toByteArray();
    }
    public static void main(String[] args) throws Exception {
        byte[] game = Files.readAllBytes(Path.of("docs/samples/G0.GS"));
        SaveValidation.game(game);
        for (int n = 0; n < game.length; n++) {
            final int length = n;
            rejects(() -> SaveValidation.game(Arrays.copyOf(game, length)));
        }
        byte[] wrong = game.clone(); wrong[0] ^= 1;
        rejects(() -> SaveValidation.game(wrong));
        rejects(() -> SaveValidation.game(Arrays.copyOf(game, game.length + 1)));
        for (int act = 0; act < 3; act++) {
            byte[] variant = game.clone(); variant[6] = (byte) act; variant[7] = 0;
            SaveValidation.game(variant);
        }
        Map<String, byte[]> set = new LinkedHashMap<>();
        byte[] players = new byte[16]; players[0] = 1;
        Arrays.fill(players, 2, 16, (byte) 255); players[2] = 0; players[3] = 0;
        set.put("G0.GS", game); set.put("Players.gsi", players); set.put("History.gsi", new byte[4]);
        assert SaveValidation.validate(set) == 1;
        set.put("G8.GS", game); rejects(() -> SaveValidation.validate(set)); set.remove("G8.GS");
        players[0] = 2; rejects(() -> SaveValidation.validate(set)); players[0] = 1;
        Map<String, byte[]> entries = new LinkedHashMap<>();
        set.forEach((name, bytes) -> entries.put("saves/" + name, bytes));
        File cache = Files.createTempDirectory("save-restore-java").toFile();
        byte[] archive = zip(entries);
        assert SaveRestoreArchive.read(new ByteArrayInputStream(archive), cache, () -> false).players == 1;
        entries.put("../G0.GS", game);
        rejects(() -> SaveRestoreArchive.read(new ByteArrayInputStream(zip(entries)), cache, () -> false));
        entries.remove("../G0.GS"); entries.put("saves/g0.gs", game);
        rejects(() -> SaveRestoreArchive.read(new ByteArrayInputStream(zip(entries)), cache, () -> false));
        rejects(() -> SaveRestoreArchive.read(new ByteArrayInputStream(archive) {
            @Override public void close() throws IOException { throw new IOException("provider close failed"); }
        }, cache, () -> false));
        try {
            SaveRestoreArchive.read(new ByteArrayInputStream(archive), cache, () -> true);
            throw new AssertionError("Expected cancellation");
        } catch (java.util.concurrent.CancellationException expected) { }
        assert cache.list().length == 0;
        cache.delete();
        System.out.println("Save validation and archive tests passed");
    }
}
