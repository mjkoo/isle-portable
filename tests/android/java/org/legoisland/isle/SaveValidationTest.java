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
    static void number(ByteArrayOutputStream out, int value, int length) {
        for (int i = 0; i < length; i++) out.write(value >>> (8 * i));
    }
    static byte[] allStates() throws Exception {
        Map<String, byte[]> states = new LinkedHashMap<>();
        states.put("PizzeriaState", new byte[10]); states.put("PizzaMissionState", new byte[40]);
        states.put("TowTrackMissionState", new byte[20]); states.put("AmbulanceMissionState", new byte[20]);
        states.put("HospitalState", new byte[12]); states.put("GasStationState", new byte[10]);
        states.put("PoliceState", new byte[4]);
        byte[] race = new byte[25]; for (int i = 0; i < 5; i++) race[5 * i] = (byte) (i + 1);
        states.put("JetskiRaceState", race); states.put("CarRaceState", race);
        for (String vehicle : new String[] {"Jetski", "Copter", "DuneCar", "RaceCar"}) {
            states.put("Lego" + vehicle + "BuildState", new byte[4]);
        }
        states.put("AnimState", new byte[12]); states.put("Act1State", new byte[269]);
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        number(out, 0x1000c, 4); out.write(new byte[5]);
        byte[] end = "END_OF_VARIABLES".getBytes(java.nio.charset.StandardCharsets.US_ASCII);
        out.write(end.length); out.write(end);
        out.write(new byte[66 * 16 + 81 * 12 + 16 * 10 + 1]);
        number(out, states.size(), 2);
        for (Map.Entry<String, byte[]> state : states.entrySet()) {
            number(out, state.getKey().length(), 2);
            out.write(state.getKey().getBytes(java.nio.charset.StandardCharsets.US_ASCII));
            out.write(state.getValue());
        }
        number(out, 0, 2);
        return out.toByteArray();
    }
    public static void main(String[] args) throws Exception {
        SaveValidation.game(allStates());
        byte[] game = Files.readAllBytes(Path.of("docs/samples/G0.GS"));
        SaveValidation.game(game);
        byte[] unusedPlane = game.clone();
        byte[] marker = "Act1State".getBytes(java.nio.charset.StandardCharsets.US_ASCII);
        int plane = -1;
        for (int i = 0; i < game.length - marker.length; i++) {
            if (Arrays.equals(Arrays.copyOfRange(game, i, i + marker.length), marker)) { plane = i + marker.length; break; }
        }
        if (plane >= 0 && game[plane] == 0 && game[plane + 1] == 0) {
            java.nio.ByteBuffer.wrap(unusedPlane).order(java.nio.ByteOrder.LITTLE_ENDIAN).putInt(plane + 2, 0x7fc00000);
            SaveValidation.game(unusedPlane);
        }
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
        byte[] tooLarge = new byte[21 * 1024 * 1024];
        rejects(() -> SaveRestoreArchive.read(new ByteArrayInputStream(tooLarge), cache, () -> false));
        entries.remove("../G0.GS"); entries.put("saves/g0.gs", game);
        rejects(() -> SaveRestoreArchive.read(new ByteArrayInputStream(zip(entries)), cache, () -> false));
        entries.remove("saves/g0.gs");
        entries.put("saves/G0.GS", new byte[SaveValidation.LIMIT + 1]);
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
