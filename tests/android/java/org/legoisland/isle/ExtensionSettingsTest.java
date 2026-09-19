package org.legoisland.isle;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeSet;

public final class ExtensionSettingsTest {
    private static File temp() throws Exception {
        File dir = Files.createTempDirectory("isle-extensions-test").toFile();
        dir.deleteOnExit();
        return dir;
    }

    private static void write(File root, String path) throws Exception {
        File file = new File(root, path);
        assert file.getParentFile().isDirectory() || file.getParentFile().mkdirs();
        Files.write(file.toPath(), new byte[0]);
    }

    public static void main(String[] args) throws Exception {
        boolean assertions = false;
        assert assertions = true;
        if (!assertions) throw new AssertionError("Run with java -ea");

        // Every key this screen offers is one the native validator accepts, read from the repository
        // root. A key missing there would make Save fail with "Invalid setting".
        String store = new String(Files.readAllBytes(Paths.get("ISLE/android/configstore.cpp")), StandardCharsets.UTF_8);
        for (String key : ExtensionSettings.KEYS) {
            assert store.contains("\"" + key + "\"") : key;
            // Extension keys survive Reset these settings, which clears every other non-controller key.
            assert ExtensionSettings.isExtensionKey(key) : key;
            assert !ControllerBindings.isControllerKey(key) : key;
        }
        assert ExtensionSettings.KEYS.length == 9;
        // The two Display rows are game options, so reset does clear them.
        for (String key : new String[] {"isle:lighting model", "isle:wide view angle"}) {
            assert store.contains("\"" + key + "\"") : key;
            assert !ExtensionSettings.isExtensionKey(key) : key;
        }
        // The file list and the directives are edited in the desktop tool or by hand: the screen
        // never lists them, so reset must not claim them either, and the validator must keep
        // rejecting them so a save can never rewrite one.
        for (String key : new String[] {"si loader:files", "si loader:directives"}) {
            assert !Arrays.asList(ExtensionSettings.KEYS).contains(key) : key;
            assert !store.contains("\"" + key + "\"") : key;
            // ...though the section is still ours, so a reset of the group would reach it.
            assert ExtensionSettings.isExtensionKey(key) : key;
        }
        assert !ExtensionSettings.isExtensionKey("isle:music");
        assert !ExtensionSettings.isExtensionKey("gamepad:start");

        // Multiplayer only connects with both a relay and a room, so anything less is worth warning about.
        Map<String, String> draft = new LinkedHashMap<>();
        assert !ExtensionSettings.multiplayerIncomplete(draft);
        draft.put(ExtensionSettings.MULTIPLAYER, "true");
        assert ExtensionSettings.multiplayerIncomplete(draft);
        draft.put(ExtensionSettings.RELAY_URL, "wss://relay.example");
        assert ExtensionSettings.multiplayerIncomplete(draft);
        draft.put(ExtensionSettings.ROOM, "lobby");
        assert !ExtensionSettings.multiplayerIncomplete(draft);
        draft.put(ExtensionSettings.ROOM, "   ");
        assert ExtensionSettings.multiplayerIncomplete(draft);
        draft.put(ExtensionSettings.ROOM, "lobby");
        draft.put(ExtensionSettings.MULTIPLAYER, null);
        assert !ExtensionSettings.multiplayerIncomplete(draft);
        draft.put(ExtensionSettings.MULTIPLAYER, "false");
        assert !ExtensionSettings.multiplayerIncomplete(draft);

        // Typed text is checked here before the native validator sees it, so the two must agree on
        // what they accept; the native test asserts the same cases from the other side.
        for (String url : new String[] {"ws://host", "wss://host.example:8080/path", "ws://1"}) {
            assert ExtensionSettings.isRelayUrl(url) : url;
        }
        for (String url : new String[] {"http://host", "https://host", "host", "ws://", "wss://", "ws://ho st", ""}) {
            assert !ExtensionSettings.isRelayUrl(url) : url;
        }
        StringBuilder long512 = new StringBuilder("ws://");
        while (long512.length() < 512) long512.append('a');
        assert ExtensionSettings.isRelayUrl(long512.toString());
        assert !ExtensionSettings.isRelayUrl(long512.append('a').toString());

        for (String room : new String[] {"lobby", "room-1", "A", "a/b"}) {
            assert ExtensionSettings.isIniWord(room, 64) : room;
        }
        for (String room : new String[] {"", "my room", "room;1", "room#1", "room=1", "room,1", "[room]", "röom"}) {
            assert !ExtensionSettings.isIniWord(room, 64) : room;
        }
        StringBuilder long64 = new StringBuilder();
        while (long64.length() < 64) long64.append('a');
        assert ExtensionSettings.isIniWord(long64.toString(), 64);
        assert !ExtensionSettings.isIniWord(long64.append('a').toString(), 64);
        assert !ExtensionSettings.isIniWord("aaa", 2);

        // Enumeration. Paths are game-relative with their own leading slash, as the extensions read
        // them, and both rows pick from the same list of folders.
        File root = temp();
        write(root, "LEGO/Scripts/CREDITS.SI");
        write(root, "LEGO/Scripts/Isle/ISLE.SI");
        write(root, "LEGO/data/WORLD.WDB");
        write(root, "textures/brick.bmp");
        write(root, "mods/MYMOD.SI");
        write(root, "notes.txt");

        List<String> folders = Arrays.asList(ExtensionSettings.folders(root));
        assert folders.contains("/LEGO") : folders;
        assert folders.contains("/LEGO/Scripts") : folders;
        assert folders.contains("/LEGO/Scripts/Isle") : folders;
        assert folders.contains("/textures") : folders;
        assert folders.contains("/mods") : folders;
        // Files are not folders, whatever they are named.
        assert !folders.contains("/notes.txt") : folders;
        assert !folders.contains("/mods/MYMOD.SI") : folders;
        // Both defaults are always offered, so either row can go back to its own before the folder
        // it names exists.
        String[] defaults = {ExtensionSettings.DEFAULT_TEXTURE_PATH, ExtensionSettings.DEFAULT_SI_PATH};
        for (String path : defaults) {
            assert folders.contains(path) : path;
        }
        File empty = temp();
        assert new TreeSet<>(Arrays.asList(ExtensionSettings.folders(empty)))
                .equals(new TreeSet<>(Arrays.asList(defaults)));
        assert new TreeSet<>(Arrays.asList(ExtensionSettings.folders(new File(empty, "missing"))))
                .equals(new TreeSet<>(Arrays.asList(defaults)));

        // Every offered path is one the validator accepts: rooted, no "..", no backslash, no comma
        // and no whitespace.
        for (String path : ExtensionSettings.folders(root)) {
            assert path.startsWith("/") && !path.contains("..") && !path.matches(".*[\\s,\\\\].*") : path;
        }

        // One directory can hold more entries than the list is allowed to offer, so the cap has to
        // hold inside a single level and not only on the way down.
        File wide = temp();
        for (int i = 0; i < ExtensionSettings.MAX_FOLDERS + 50; i++) {
            write(wide, String.format("dir%03d/placeholder", i));
        }
        assert ExtensionSettings.folders(wide).length <= ExtensionSettings.MAX_FOLDERS
                : ExtensionSettings.folders(wide).length;
    }
}
