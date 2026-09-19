package org.legoisland.isle;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.util.Map;
import java.util.Set;
import java.util.TreeSet;

/**
 * Settings rows for the game's extensions, which apply on the next launch. The keys and what each
 * value may hold are validated in ISLE/android/configstore.cpp; keep them in step. Plain Java so it
 * can be tested without a device.
 *
 * <p>An extension is enabled by a boolean under [extensions] and reads its options from a section
 * named after it, so "extensions:si loader" turns the si loader on and "si loader:si path" tells it
 * where to look. The game hands an extension every key in its section, so options this screen does
 * not offer, including the "si loader:files" list the desktop tool writes, stay where they were
 * left.
 */
final class ExtensionSettings {
    static final String TEXTURE_LOADER = "extensions:texture loader";
    static final String SI_LOADER = "extensions:si loader";
    static final String THIRD_PERSON_CAMERA = "extensions:third person camera";
    static final String MULTIPLAYER = "extensions:multiplayer";

    static final String TEXTURE_PATH = "texture loader:texture path";
    static final String SI_PATH = "si loader:si path";
    static final String RELAY_URL = "multiplayer:relay url";
    static final String ROOM = "multiplayer:room";
    static final String ACTOR = "multiplayer:actor";

    /** The folders the extensions read when no others are set. */
    static final String DEFAULT_TEXTURE_PATH = "/textures";
    static final String DEFAULT_SI_PATH = "/si";

    /** The sections Settings owns. A key in one of them is an extension setting. */
    private static final String[] SECTIONS = {"extensions:", "texture loader:", "si loader:", "multiplayer:"};

    static final String[] KEYS = {
        TEXTURE_LOADER, TEXTURE_PATH, SI_LOADER, SI_PATH,
        THIRD_PERSON_CAMERA, MULTIPLAYER, RELAY_URL, ROOM, ACTOR
    };

    static final String INCOMPLETE_MULTIPLAYER = "Multiplayer needs a relay server and a room";
    static final String INCOMPLETE_MULTIPLAYER_DETAIL =
            "Without both, the game starts but never joins anyone.";
    static final String FORCED_THIRD_PERSON = "Multiplayer turns this on whatever it is set to here.";

    /** A sanity limit on the picker, so an unexpected tree cannot fill the list forever. */
    static final int MAX_FOLDERS = 256;

    static final String RELAY_HINT =
            "Enter the relay server's WebSocket address, such as wss://relay.example, or leave blank for none.";
    static final String RELAY_ERROR = "Enter a ws:// or wss:// address with no spaces.";
    static final String ROOM_HINT =
            "Enter the room to join. Everyone who picks the same room on the same relay plays together.";
    static final String ROOM_ERROR = "Enter a room name with no spaces and none of , ; # = [ ].";

    private ExtensionSettings() {}

    /**
     * The transports speak WebSocket, so anything else fails at connect time. Mirrors the native
     * validator, which has the last word.
     */
    static boolean isRelayUrl(String value) {
        int scheme = value.startsWith("ws://") ? 5 : value.startsWith("wss://") ? 6 : 0;
        // Measured in bytes, as the native validator measures it: a multi-byte address near the
        // limit would otherwise pass here and be refused there, with only the generic message.
        int length = value.getBytes(StandardCharsets.UTF_8).length;
        if (scheme == 0 || length <= scheme || length > 512) {
            return false;
        }
        for (int i = 0; i < value.length(); i++) {
            if (value.charAt(i) <= ' ' || value.charAt(i) == 127) {
                return false;
            }
        }
        return true;
    }

    /**
     * iniparser writes every value quoted, so whatever Settings stores comes back whole. A line
     * written by hand is not quoted, though, and iniparser truncates an unquoted value at a comment
     * character and will not parse one holding "=" or brackets, so both text rules here refuse
     * ,;#=[] and leave a value that reads the same either way.
     */
    private static boolean isIniSafe(char c) {
        return c > ' ' && c != 127 && ",;#=[]".indexOf(c) < 0;
    }

    /**
     * The ini rule above, and ASCII besides: the room and the character are typed by hand on every
     * device that means to meet in the same place, so the characters have to be ones every keyboard
     * agrees on rather than ones that merely survive the file.
     */
    static boolean isIniWord(String value, int max) {
        if (value.isEmpty() || value.length() > max) {
            return false;
        }
        for (int i = 0; i < value.length(); i++) {
            char c = value.charAt(i);
            if (c >= 127 || !isIniSafe(c)) {
                return false;
            }
        }
        return true;
    }

    /**
     * A folder inside the game files, as the extensions name it. ResolveGamePath joins the game
     * data root and the value without inserting a separator, so the value carries its own leading
     * slash, and ".." would reach outside the game files. The si loader splits its own file list on
     * whitespace as well as commas, so a path holding either could never sit in that list. Bytes
     * above 127 are left alone: the name comes from the filesystem and may well not be ASCII.
     * Mirrors the native validator, which has the last word.
     */
    static boolean isGamePath(String path) {
        byte[] bytes = path.getBytes(StandardCharsets.UTF_8);
        if (bytes.length < 2 || bytes.length > 255 || bytes[0] != '/') {
            return false;
        }
        if (path.contains("..") || path.indexOf('\\') >= 0) {
            return false;
        }
        for (byte b : bytes) {
            int c = b & 0xFF;
            if (c <= 127 && !isIniSafe((char) c)) {
                return false;
            }
        }
        return true;
    }

    /** Whether the key is one of this screen's extension settings rather than a game option. */
    static boolean isExtensionKey(String key) {
        for (String section : SECTIONS) {
            if (key.startsWith(section)) {
                return true;
            }
        }
        return false;
    }

    /** Whether multiplayer is on without both of the things it needs to reach anyone. */
    static boolean multiplayerIncomplete(Map<String, String> draft) {
        if (!"true".equals(draft.get(MULTIPLAYER))) {
            return false;
        }
        return isBlank(draft.get(RELAY_URL)) || isBlank(draft.get(ROOM));
    }

    private static boolean isBlank(String value) {
        return value == null || value.trim().isEmpty();
    }

    /**
     * The folders inside the game files, as the extensions name them: a path from the game data
     * root, starting with its own slash. Both extension defaults are always offered, whether or not
     * they are there yet, so either row can return to its own without the folder existing first.
     */
    static String[] folders(File root) {
        Set<String> found = new TreeSet<>();
        found.add(DEFAULT_TEXTURE_PATH);
        found.add(DEFAULT_SI_PATH);
        collect(root, "", found, 0);
        return found.toArray(new String[0]);
    }

    private static void collect(File dir, String prefix, Set<String> found, int depth) {
        if (depth >= GameFileCopier.MAX_DEPTH) {
            return;
        }
        File[] children = dir.listFiles();
        if (children == null) {
            return;
        }
        for (File child : children) {
            // Checked here rather than on the way in: one directory can hold more entries on its
            // own than the whole list is allowed to offer.
            if (found.size() >= MAX_FOLDERS) {
                return;
            }
            if (!child.isDirectory()) {
                continue;
            }
            String name = child.getName();
            // What a game file import leaves behind is never a texture or an SI pack.
            if (name.startsWith(GameFileCopier.IMPORTED_PREFIX)
                    || name.contains(GameFileCopier.UNREADABLE_MARKER)) {
                continue;
            }
            String path = prefix + "/" + name;
            // A name the native validator would refuse takes its whole subtree with it, since every
            // path below carries the same prefix. Offering one would cost the entire save: the
            // bridge validates every key before writing any, so one bad path refuses them all.
            if (!isGamePath(path)) {
                continue;
            }
            found.add(path);
            collect(child, path, found, depth + 1);
        }
    }
}
