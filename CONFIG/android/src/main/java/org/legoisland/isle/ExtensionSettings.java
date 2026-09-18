package org.legoisland.isle;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.TreeSet;

/**
 * Settings rows for the game's extensions, which apply on the next launch. The keys and what each
 * value may hold are validated in ISLE/android/configstore.cpp; keep them in step. Plain Java so it
 * can be tested without a device.
 *
 * <p>An extension is enabled by a boolean under [extensions] and reads its options from a section
 * named after it, so "extensions:si loader" turns the si loader on and "si loader:files" tells it
 * what to load. The game hands an extension every key in its section, so options this screen does
 * not offer stay where a hand edit left them.
 */
final class ExtensionSettings {
    static final String TEXTURE_LOADER = "extensions:texture loader";
    static final String SI_LOADER = "extensions:si loader";
    static final String THIRD_PERSON_CAMERA = "extensions:third person camera";
    static final String MULTIPLAYER = "extensions:multiplayer";

    static final String TEXTURE_PATH = "texture loader:texture path";
    static final String SI_FILES = "si loader:files";
    static final String RELAY_URL = "multiplayer:relay url";
    static final String ROOM = "multiplayer:room";
    static final String ACTOR = "multiplayer:actor";

    /** The folder the texture loader reads when no other is set. */
    static final String DEFAULT_TEXTURE_PATH = "/textures";

    /** The sections Settings owns. A key in one of them is an extension setting. */
    private static final String[] SECTIONS = {"extensions:", "texture loader:", "si loader:", "multiplayer:"};

    static final String[] KEYS = {
        TEXTURE_LOADER, TEXTURE_PATH, SI_LOADER, SI_FILES,
        THIRD_PERSON_CAMERA, MULTIPLAYER, RELAY_URL, ROOM, ACTOR
    };

    static final String INCOMPLETE_MULTIPLAYER = "Multiplayer needs a relay server and a room";
    static final String INCOMPLETE_MULTIPLAYER_DETAIL =
            "Without both, the game starts but never joins anyone.";
    static final String FORCED_THIRD_PERSON = "Multiplayer turns this on whatever it is set to here.";

    /** As many entries as the validator accepts, and the same cap on what the pickers offer. */
    static final int MAX_FILES = 32;

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
        if (scheme == 0 || value.length() <= scheme || value.length() > 512) {
            return false;
        }
        for (int i = 0; i < value.length(); i++) {
            if (value.charAt(i) <= ' ' || value.charAt(i) == 127) {
                return false;
            }
        }
        return true;
    }

    /** A value that survives the ini round trip: no comment, section or separator characters. */
    static boolean isIniWord(String value, int max) {
        if (value.isEmpty() || value.length() > max) {
            return false;
        }
        for (int i = 0; i < value.length(); i++) {
            char c = value.charAt(i);
            if (c <= ' ' || c >= 127 || ",;#=[]".indexOf(c) >= 0) {
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
     * The si loader reads one comma-separated list. Entries are written in the order they are
     * offered so the same selection always produces the same line; anything else the file already
     * held keeps its own order after them.
     */
    static String joinFiles(Collection<String> selected, String[] offered) {
        Set<String> remaining = new LinkedHashSet<>(selected);
        List<String> ordered = new ArrayList<>();
        for (String file : offered) {
            if (remaining.remove(file)) {
                ordered.add(file);
            }
        }
        ordered.addAll(new TreeSet<>(remaining));
        return ordered.isEmpty() ? null : String.join(",", ordered);
    }

    /** The files named by a stored list, keeping their order. */
    static Set<String> splitFiles(String value) {
        Set<String> files = new LinkedHashSet<>();
        if (value == null) {
            return files;
        }
        for (String entry : value.split(",", -1)) {
            String file = entry.trim();
            if (!file.isEmpty()) {
                files.add(file);
            }
        }
        return files;
    }

    /**
     * The folders inside the game files, as the extensions name them: a path from the game data
     * root, starting with its own slash. The default texture folder is always offered, whether or
     * not it is there yet, so the row can return to it.
     */
    static String[] folders(File root) {
        Set<String> found = new TreeSet<>();
        found.add(DEFAULT_TEXTURE_PATH);
        collect(root, "", found, true, null, 0);
        return found.toArray(new String[0]);
    }

    /**
     * The .si files inside the game files that the game does not ship, which is what the si loader
     * is for: a stock script is already loaded, and replacing one is not what this row does.
     */
    static String[] siFiles(File root, String[] stock) {
        Set<String> skip = new TreeSet<>(String.CASE_INSENSITIVE_ORDER);
        skip.addAll(Arrays.asList(stock));
        Set<String> found = new TreeSet<>();
        collect(root, "", found, false, skip, 0);
        return found.toArray(new String[0]);
    }

    private static void collect(File dir, String prefix, Set<String> found, boolean wantDirs, Set<String> skip,
            int depth) {
        if (depth >= GameFileCopier.MAX_DEPTH || found.size() > MAX_FILES * 4) {
            return;
        }
        File[] children = dir.listFiles();
        if (children == null) {
            return;
        }
        for (File child : children) {
            String path = prefix + "/" + child.getName();
            if (child.isDirectory()) {
                if (wantDirs) {
                    found.add(path);
                }
                collect(child, path, found, wantDirs, skip, depth + 1);
            }
            else if (!wantDirs && child.getName().toLowerCase(Locale.ROOT).endsWith(".si") && !skip.contains(path)) {
                found.add(path);
            }
        }
    }
}
