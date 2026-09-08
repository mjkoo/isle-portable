package org.legoisland.isle;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/** Bounded, read-only parsing of the engine's little-endian save format. */
final class SaveValidation {
    static final int LIMIT = 16 * 1024 * 1024;
    // These are the color variables emitted by the retail save writer.
    private static final Set<String> COLOR_VARIABLES = new HashSet<>(Arrays.asList(
        "c_dbbkfny0", "c_dbbkxly0", "c_chbasey0", "c_chbacky0",
        "c_chdishy0", "c_chhorny0", "c_chljety1", "c_chrjety1",
        "c_chmidly0", "c_chmotry0", "c_chsidly0", "c_chsidry0",
        "c_chstuty0", "c_chtaily0", "c_chwindy1", "c_dbfbrdy0",
        "c_dbflagy0", "c_dbfrfny4", "c_dbfrxly0", "c_dbhndly0",
        "c_dbltbry0", "c_jsdashy0", "c_jsexhy0", "c_jsfrnty5",
        "c_jshndly0", "c_jslsidy0", "c_jsrsidy0", "c_jsskiby0",
        "c_jswnshy5", "c_rcbacky6", "c_rcedgey0", "c_rcfrmey0",
        "c_rcfrnty6", "c_rcmotry0", "c_rcsidey0", "c_rcstery0",
        "c_rcstrpy0", "c_rctailya", "c_rcwhl1y0", "c_rcwhl2y0",
        "c_jsbasey0", "c_chblady0", "c_chseaty0"));
    private static final Set<String> COLORS = new HashSet<>(Arrays.asList(
        "lego white", "lego black", "lego yellow", "lego red", "lego blue", "lego brown", "lego lt grey", "lego green"));
    private SaveValidation() { }

    static int validate(Map<String, byte[]> files) throws IOException {
        Reader players = new Reader(required(files, "Players.gsi"));
        int count = range(players.s16(), 1, 9);
        for (int i = 0; i < count; i++) username(players);
        players.end();
        Reader history = new Reader(required(files, "History.gsi"));
        history.s16(); // IDs are signed and can wrap; they are not array indexes.
        int scores = range(history.s16(), 0, 20);
        for (int i = 0; i < scores; i++) {
            require(history.s16() == i, "Invalid history index.");
            history.s16();
            for (int j = 0; j < 25; j++) range(history.u8(), 0, 3);
            username(history);
            history.s16();
        }
        history.end();
        for (int i = 0; i < count; i++) {
            String name = "G" + i + ".GS";
            try { game(required(files, name)); }
            catch (IOException e) { throw new IOException(name + ": " + e.getMessage(), e); }
        }
        require(files.size() == count + 2, "The archive has slots outside its player list.");
        return count;
    }

    private static byte[] required(Map<String, byte[]> files, String name) throws IOException {
        byte[] bytes = files.get(name);
        require(bytes != null, "Incomplete save set: missing " + name + ".");
        return bytes;
    }

    private static void username(Reader r) throws IOException {
        for (int i = 0; i < 7; i++) range(r.s16(), -1, 32);
    }

    static void game(byte[] bytes) throws IOException {
        Reader r = new Reader(bytes);
        require(r.u32() == 0x1000c, "Unsupported saved-game version.");
        r.s16();
        range(r.s16(), 0, 2);
        range(r.u8(), 0, 5);
        variables(r);
        for (int i = 0; i < 66; i++) {
            r.skip(8); // Sound and movement are script offsets, not file lengths.
            r.u8();
            int hats = i == 0 || i == 56 ? 20 : i == 5 || (i >= 48 && i <= 53) ? 0 : 19;
            range(r.u8(), 0, hats);
            for (int j = 0; j < 6; j++) range(r.u8(), 0, 7);
        }
        for (int i = 0; i < 81; i++) {
            range(r.u8(), 0, 3);
            r.skip(9);
            range(r.u8(), 0, 4);
            r.u8();
        }
        r.skip(16 * 10);
        range(r.u8(), 0, 4);
        int count = range(r.s16(), 0, 15);
        Set<String> states = new HashSet<>();
        for (int i = 0; i < count; i++) {
            String state = r.string(r.s16(), 79); // The engine reads into an 80-byte buffer.
            require(states.add(state), "Duplicate saved-game state.");
            switch (state) {
                case "PizzeriaState":
                    for (int j = 0; j < 5; j++) range(r.s16(), 0, 2);
                    break;
                case "PizzaMissionState":
                    for (int j = 0; j < 5; j++) { r.skip(4); score(r); score(r); }
                    break;
                case "TowTrackMissionState": case "AmbulanceMissionState":
                    for (int j = 0; j < 10; j++) score(r);
                    break;
                case "HospitalState":
                    for (int j = 0; j < 6; j++) range(r.s16(), 0, 5);
                    break;
                case "GasStationState":
                    for (int j = 0; j < 5; j++) range(r.s16(), 0, 5);
                    break;
                case "PoliceState": r.skip(4); break;
                case "JetskiRaceState": case "CarRaceState":
                    for (int j = 1; j <= 5; j++) {
                        require(r.u8() == j, "Invalid race actor index."); score(r); score(r);
                    }
                    break;
                // Part counts come from the retail build animation trees. All three
                // variants of each vehicle contain the same number of placeable parts.
                case "LegoJetskiBuildState": vehicleBuild(r, 9); break;
                case "LegoCopterBuildState": vehicleBuild(r, 15); break;
                case "LegoDuneCarBuildState": vehicleBuild(r, 8); break;
                case "LegoRaceCarBuildState": vehicleBuild(r, 11); break;
                case "AnimState":
                    long extra = r.u32(), animations = r.u32();
                    // Retail island animation data has 369 entries. The game copies
                    // this array without checking its destination length.
                    require(animations == 0 || animations == 369, "Unsupported island animation table.");
                    require(animations == 0 || extra < 47, "Invalid extra character index.");
                    r.skip(animations * 2);
                    long flags = r.u32();
                    require(flags == (animations == 0 ? 0 : 70), "Unsupported animation location table.");
                    require(flags <= r.remaining(), "Truncated animation flags.");
                    for (long j = 0; j < flags; j++) range(r.u8(), 0, 1);
                    break;
                case "Act1State": act1(r); break;
                default: throw new IOException("Unsupported saved-game state: " + state + ".");
            }
        }
        r.s16();
        r.end();
    }

    private static void variables(Reader r) throws IOException {
        Set<String> seen = new HashSet<>();
        for (;;) {
            String name = r.string(r.u8(), 255);
            if (name.equals("END_OF_VARIABLES")) return;
            require(seen.add(name), "Duplicate saved-game variable.");
            String value = r.string(r.u8(), 255);
            // The engine dispatches variable names to setters, some of which assume
            // valid commands. Length checks alone do not make arbitrary variables safe.
            if (COLOR_VARIABLES.contains(name)) {
                require(COLORS.contains(value), "Unsupported saved part color.");
            } else if (name.equals("lightposition")) {
                decimal(value, 5);
            } else if (name.equals("backgroundcolor")) {
                backgroundColor(value);
            } else {
                throw new IOException("Unsupported saved-game variable: " + name + ".");
            }
        }
    }

    private static void backgroundColor(String value) throws IOException {
        if (value.equals("reset")) return;
        String[] fields = value.split("[ \t]+", -1);
        require(fields.length == 4 && fields[0].equals("set"), "Invalid saved background color.");
        for (int i = 1; i < fields.length; i++) decimal(fields[i], 100);
    }

    private static void decimal(String value, int max) throws IOException {
        require(value.matches("[0-9]{1,3}"), "Invalid saved numeric variable.");
        range(Integer.parseInt(value), 0, max);
    }

    private static void vehicleBuild(Reader r, int parts) throws IOException {
        r.u8(); // Introduction counter wraps as an unsigned byte.
        range(r.u8(), 0, 1);
        range(r.u8(), 0, 1);
        range(r.u8(), 0, parts);
    }

    private static void score(Reader r) throws IOException { range(r.s16(), 0, 3); }

    private static void act1(Reader r) throws IOException {
        boolean[] named = new boolean[7];
        for (int i = 0; i < 7; i++) {
            named[i] = !r.string(r.s16(), 1024).isEmpty();
            for (int j = 0; j < 9; j++) {
                float value = Float.intBitsToFloat((int) r.u32());
                // Empty planes carry unused vectors which the engine does not initialize.
                require(!named[i] || (!Float.isNaN(value) && !Float.isInfinite(value)), "Invalid vehicle position.");
            }
        }
        int textures = (named[3] ? 3 : 0) + (named[4] ? 2 : 0) + (named[5] ? 1 : 0) + (named[6] ? 3 : 0);
        for (int i = 0; i < textures; i++) {
            r.string(r.s16(), 1024);
            long width = r.u32(), height = r.u32(), colors = r.u32();
            require(width > 0 && height > 0 && width <= 4096 && height <= 4096
                && width * height <= LIMIT && colors > 0 && colors <= 256, "Unsupported embedded texture dimensions.");
            r.skip(colors * 3);
            r.skip(width * height);
        }
        range(r.s16(), 0, 2);
        range(r.u8(), 0, 1);
    }

    static void require(boolean condition, String error) throws IOException {
        if (!condition) throw new IOException(error);
    }
    private static int range(int value, int min, int max) throws IOException {
        require(value >= min && value <= max, "Saved-game value is out of range.");
        return value;
    }

    private static final class Reader {
        private final byte[] bytes;
        private int offset;
        Reader(byte[] bytes) throws IOException {
            require(bytes != null && bytes.length <= LIMIT, "Invalid save size.");
            this.bytes = bytes;
        }
        int remaining() { return bytes.length - offset; }
        int u8() throws IOException { require(remaining() >= 1, "Truncated saved game."); return bytes[offset++] & 255; }
        int s16() throws IOException { return (short) (u8() | u8() << 8); }
        long u32() throws IOException { return (u8() | (long) u8() << 8 | (long) u8() << 16 | (long) u8() << 24); }
        void skip(long count) throws IOException {
            require(count >= 0 && count <= remaining(), "Truncated saved game."); offset += (int) count;
        }
        String string(int count, int max) throws IOException {
            require(count >= 0 && count <= max && count <= remaining(), "Invalid saved-game string length.");
            for (int i = 0; i < count; i++) require(bytes[offset + i] != 0, "Embedded null in saved-game string.");
            String value = new String(bytes, offset, count, StandardCharsets.US_ASCII);
            offset += count;
            return value;
        }
        void end() throws IOException { require(remaining() == 0, "Unexpected trailing saved-game data."); }
    }
}
