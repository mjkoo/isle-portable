package org.legoisland.isle;

import java.io.File;
import java.util.Locale;

/**
 * What Settings says and allows about the game files: the space a replacement needs, where the
 * game currently reads from, and the wording. Plain Java so it can be tested without a device.
 */
final class GameFilesPolicy {
    /** Slack left free beyond the new files, as the startup import leaves. */
    static final long SPACE_MARGIN_BYTES = 32L * 1024 * 1024;

    interface Sizes {
        String format(long bytes);
    }

    private GameFilesPolicy() {}

    /** Free space a replacement of this size needs. The current files stay until it is in place. */
    static long required(long total) {
        return total > Long.MAX_VALUE - SPACE_MARGIN_BYTES ? Long.MAX_VALUE : total + SPACE_MARGIN_BYTES;
    }

    /** Why a replacement of this size does not fit beside the current files, or null if it does. */
    static String spaceRefusal(long total, long usable, Sizes sizes) {
        long required = required(total);
        if (usable >= required) {
            return null;
        }
        return "Replacing the game files needs " + sizes.format(required) + " free: " + sizes.format(total)
                + " for the new files and " + sizes.format(SPACE_MARGIN_BYTES) + " to spare. Only "
                + sizes.format(usable) + " is free. The current files stay until the new ones are in place, so "
                + "both must fit. Free up space, or choose Remove game files and select the folder again when "
                + "the game reopens.";
    }

    /**
     * Whether diskpath names this app's storage: unset, the storage root itself, or an earlier
     * import directly inside it. Anything else is a location someone set by hand.
     */
    static boolean inAppStorage(String diskpath, File root) {
        if (diskpath == null || diskpath.isEmpty()) {
            return true;
        }
        File path = new File(diskpath);
        String rootPath = root.getAbsolutePath();
        if (path.getAbsolutePath().equals(rootPath)) {
            return true;
        }
        File parent = path.getAbsoluteFile().getParentFile();
        return parent != null && parent.getPath().equals(rootPath)
                && path.getName().startsWith(GameFileCopier.IMPORTED_PREFIX);
    }

    /** The directory the game reads its files from. */
    static File location(String diskpath, File root) {
        return diskpath == null || diskpath.isEmpty() ? root : new File(diskpath);
    }

    /**
     * The size of what the game would read in dir: its top-level entries starting with "lego", as
     * the game finds them. -1 when there are none or dir cannot be read.
     */
    static long measure(File dir) {
        File[] children = dir.listFiles();
        if (children == null) {
            return -1;
        }
        long total = 0;
        boolean found = false;
        for (File child : children) {
            if (child.getName().toLowerCase(Locale.ROOT).startsWith("lego")) {
                total += GameFileCopier.sizeOf(child);
                found = true;
            }
        }
        return found ? total : -1;
    }

    static String summary(String diskpath, File root, long size, boolean pending, Sizes sizes) {
        if (pending) {
            return "A change to the game files is waiting. Close and reopen the game to apply it.";
        }
        boolean inApp = inAppStorage(diskpath, root);
        if (size < 0) {
            return "No game files found in " + (inApp ? "app storage" : diskpath) + ".";
        }
        return inApp ? "App storage, " + sizes.format(size) : diskpath + ", " + sizes.format(size) + " (custom location)";
    }

    static String replaceConfirmation(int files, long bytes, String diskpath, File root, Sizes sizes) {
        String text = "Replace the game files with the " + files + " files (" + sizes.format(bytes) + ") just "
                + "copied?\n\nThe game will close. The new files are put in place when you reopen it.";
        if (!inAppStorage(diskpath, root)) {
            text += "\n\nThe game will read from this app's storage instead of " + diskpath
                    + ". Files there are not deleted.";
        }
        return text;
    }

    static String removeConfirmation(long size, Sizes sizes) {
        return "Remove the game files from this app" + (size >= 0 ? " (" + sizes.format(size) + ")" : "")
                + "? Saves and settings are kept.\n\nThe game will close. When you reopen it, select the folder "
                + "that holds the LEGO folder to play again.";
    }
}
