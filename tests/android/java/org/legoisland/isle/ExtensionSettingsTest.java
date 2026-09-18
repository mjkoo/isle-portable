package org.legoisland.isle;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

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
        // Directives stay hand-edited: the screen never lists them, so reset must not claim them
        // either, and the validator must keep rejecting them.
        assert !Arrays.asList(ExtensionSettings.KEYS).contains("si loader:directives");
        assert !store.contains("\"si loader:directives\"");
        // ...though the section is still ours, so a reset of the group would reach it.
        assert ExtensionSettings.isExtensionKey("si loader:directives");
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

        // The stored list is written in the order the rows are offered, so the same selection always
        // produces the same line whatever order the picker returns it in.
        String[] offered = {"/LEGO/Scripts/A.SI", "/LEGO/Scripts/B.SI", "/LEGO/Scripts/C.SI"};
        Set<String> picked = new LinkedHashSet<>(Arrays.asList("/LEGO/Scripts/C.SI", "/LEGO/Scripts/A.SI"));
        assert ExtensionSettings.joinFiles(picked, offered).equals("/LEGO/Scripts/A.SI,/LEGO/Scripts/C.SI");
        assert ExtensionSettings.joinFiles(new LinkedHashSet<>(), offered) == null;
        // A hand-edited entry the picker never offered is kept, after the ones it did.
        picked.add("/LEGO/Scripts/ZZ.SI");
        assert ExtensionSettings.joinFiles(picked, offered)
                .equals("/LEGO/Scripts/A.SI,/LEGO/Scripts/C.SI,/LEGO/Scripts/ZZ.SI");
        assert ExtensionSettings.joinFiles(picked, new String[0])
                .equals("/LEGO/Scripts/A.SI,/LEGO/Scripts/C.SI,/LEGO/Scripts/ZZ.SI");

        assert ExtensionSettings.splitFiles(null).isEmpty();
        assert ExtensionSettings.splitFiles("").isEmpty();
        assert ExtensionSettings.splitFiles("/a").equals(new LinkedHashSet<>(Arrays.asList("/a")));
        assert ExtensionSettings.splitFiles("/a,/b").equals(new LinkedHashSet<>(Arrays.asList("/a", "/b")));
        assert ExtensionSettings.splitFiles(" /a , /b ").equals(new LinkedHashSet<>(Arrays.asList("/a", "/b")));
        assert ExtensionSettings.splitFiles("/a,,/b").equals(new LinkedHashSet<>(Arrays.asList("/a", "/b")));
        // A list round trips through the picker unchanged.
        String stored = "/LEGO/Scripts/A.SI,/LEGO/Scripts/C.SI";
        assert ExtensionSettings.joinFiles(ExtensionSettings.splitFiles(stored), offered).equals(stored);

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

        // Enumeration. Paths are game-relative with their own leading slash, as the extensions read them.
        File root = temp();
        write(root, "LEGO/Scripts/CREDITS.SI");
        write(root, "LEGO/Scripts/MYMOD.SI");
        write(root, "LEGO/Scripts/Isle/lower.si");
        write(root, "LEGO/data/WORLD.WDB");
        write(root, "textures/brick.bmp");
        write(root, "notes.txt");

        List<String> folders = Arrays.asList(ExtensionSettings.folders(root));
        assert folders.contains("/LEGO") : folders;
        assert folders.contains("/LEGO/Scripts") : folders;
        assert folders.contains("/LEGO/Scripts/Isle") : folders;
        assert folders.contains("/textures") : folders;
        assert !folders.contains("/notes.txt") : folders;
        // The default folder is always offered, so the row can go back to it.
        assert folders.contains(ExtensionSettings.DEFAULT_TEXTURE_PATH);
        File empty = temp();
        assert Arrays.equals(ExtensionSettings.folders(empty), new String[] {ExtensionSettings.DEFAULT_TEXTURE_PATH});
        assert Arrays.equals(
                ExtensionSettings.folders(new File(empty, "missing")),
                new String[] {ExtensionSettings.DEFAULT_TEXTURE_PATH});

        // Only the .si files the game does not ship, matched without regard to case as the game's
        // own lookup does.
        String[] stock = {"/LEGO/Scripts/CREDITS.SI", "/LEGO/data/WORLD.WDB"};
        List<String> si = Arrays.asList(ExtensionSettings.siFiles(root, stock));
        assert si.contains("/LEGO/Scripts/MYMOD.SI") : si;
        assert si.contains("/LEGO/Scripts/Isle/lower.si") : si;
        assert !si.contains("/LEGO/Scripts/CREDITS.SI") : si;
        assert !si.contains("/LEGO/data/WORLD.WDB") : si;
        assert !si.contains("/notes.txt") : si;
        assert si.size() == 2 : si;
        assert ExtensionSettings.siFiles(root, new String[] {"/lego/scripts/credits.si"}).length == 2;
        assert ExtensionSettings.siFiles(empty, stock).length == 0;

        // Every offered path is one the validator accepts: rooted, no "..", no backslash, no comma
        // and no whitespace, since the si loader splits its list on all of those.
        for (String path : ExtensionSettings.siFiles(root, stock)) {
            assert path.startsWith("/") && !path.contains("..") && !path.matches(".*[\\s,\\\\].*") : path;
        }
        for (String path : ExtensionSettings.folders(root)) {
            assert path.startsWith("/") && !path.contains("..") && !path.matches(".*[\\s,\\\\].*") : path;
        }
    }
}
