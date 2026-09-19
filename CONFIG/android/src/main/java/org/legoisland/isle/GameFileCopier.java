package org.legoisland.isle;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Deque;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;

/**
 * Copies a LEGO Island installation out of a folder the user picked, for both the startup import
 * and Settings. The folder is reached through Source, which DocumentTreeSource implements over the
 * storage access framework, so this class stays plain Java and can be tested without a device. It
 * owns no UI and decides nothing about what the copy replaces.
 */
final class GameFileCopier {
    // Mirrored in ISLE/android/filepicker.cpp as ImportStatus; keep the numbering in step.
    static final int STATUS_RUNNING = -1;
    static final int STATUS_OK = 0;
    static final int STATUS_CANCELLED = 1;
    static final int STATUS_NO_SPACE = 2;
    static final int STATUS_NOT_GAME_FOLDER = 3;
    static final int STATUS_READ_FAILED = 4;
    static final int STATUS_WRITE_FAILED = 5;
    static final int STATUS_INTERNAL_ERROR = 6;

    /**
     * The destination is always literally "LEGO", whatever the source folder was called.
     * MxString::MapPathToFilesystem resolves the game's internal paths by matching a globbed
     * file as a case-insensitive suffix of the requested one, so a tree copied in as
     * "LEGO Island/Scripts/..." never matches "/LEGO/Scripts/..." and the import silently
     * produces something the game cannot read.
     */
    static final String GAME_DIR = "LEGO";

    static final String IMPORTED_PREFIX = "imported-";
    static final String UNREADABLE_MARKER = ".unreadable.";

    static final int MAX_DEPTH = 16;
    static final int MAX_FILES = 20000;
    private static final int BUFFER_BYTES = 256 * 1024;

    /** An entry in the picked folder. The size is -1 when the provider does not report one. */
    static final class Node {
        final String id;
        final String name;
        final boolean directory;
        final long size;

        Node(String id, String name, boolean directory, long size) {
            this.id = id;
            this.name = name;
            this.directory = directory;
            this.size = size;
        }
    }

    interface Source {
        Node root();

        /** The children of a directory, or null if it cannot be listed. */
        List<Node> list(Node directory);

        /** Opens a file for reading, or returns null if the provider has nothing to read. */
        InputStream open(Node file) throws IOException;
    }

    interface Cancellation {
        boolean cancelled();
    }

    /** A copy that did not finish: one of the STATUS_ constants and what to tell the player. */
    static final class Failure extends Exception {
        private static final long serialVersionUID = 1L;
        final int status;

        Failure(int status, String message) {
            this(status, message, null);
        }

        Failure(int status, String message, Throwable cause) {
            super(message, cause);
            this.status = status;
        }
    }

    private final Source mSource;
    private final Cancellation mCancellation;
    private final List<Entry> mEntries = new ArrayList<>();

    // Written on the worker, read on the UI thread for progress.
    private final AtomicLong mCopiedBytes = new AtomicLong();
    private final AtomicInteger mCopiedFiles = new AtomicInteger();
    private volatile String mPhase = "Preparing...";
    private volatile long mTotalBytes;
    private volatile int mTotalFiles;

    GameFileCopier(Source source, Cancellation cancellation) {
        mSource = source;
        mCancellation = cancellation;
    }

    String phase() {
        return mPhase;
    }

    void setPhase(String phase) {
        mPhase = phase;
    }

    long totalBytes() {
        return mTotalBytes;
    }

    int totalFiles() {
        return mTotalFiles;
    }

    long copiedBytes() {
        return mCopiedBytes.get();
    }

    int copiedFiles() {
        return mCopiedFiles.get();
    }

    /**
     * Finds the game folder in the source and lists every file to copy, filling in the totals.
     * Rejects anything that does not look like an installation.
     */
    void scan() throws Failure {
        Node root = resolveSourceRoot();
        if (root == null) {
            throw new Failure(STATUS_NOT_GAME_FOLDER, "The selected folder does not contain a LEGO folder, and "
                    + "does not look like one itself. Pick the folder that holds the LEGO folder from your "
                    + "LEGO Island installation.");
        }

        mPhase = "Scanning game files...";
        enumerate(root);
        if (mEntries.isEmpty()) {
            throw new Failure(STATUS_NOT_GAME_FOLDER, "The selected folder contains no game files.");
        }
    }

    /**
     * Copies what scan() found into dest. Any failure, cancelling included, deletes dest: the next
     * launch would otherwise stat its way into a partial tree and treat it as an installation. With
     * sync, each file reaches storage before the next is written, for a copy that something will
     * later rely on having survived a power loss.
     */
    void copy(File dest, boolean sync) throws Failure {
        try {
            copyAll(dest, sync);
        }
        catch (Failure failure) {
            mPhase = "Cleaning up...";
            deleteRecursively(dest);
            throw failure;
        }
    }

    /**
     * Picks the subtree to copy and rejects anything that does not look like an installation.
     * Returns the game directory, or null.
     */
    private Node resolveSourceRoot() {
        Node root = mSource.root();
        List<Node> children = mSource.list(root);
        if (children == null) {
            return null;
        }

        Node exact = null;
        Node prefixed = null;
        boolean hasScripts = false;
        boolean hasData = false;

        for (Node child : children) {
            if (child.name == null || !child.directory) {
                continue;
            }

            String lower = child.name.toLowerCase(Locale.ROOT);
            if (lower.equals("lego") && exact == null) {
                exact = child;
            }
            else if (lower.startsWith("lego") && prefixed == null) {
                prefixed = child;
            }

            hasScripts |= lower.equals("scripts");
            hasData |= lower.equals("data");
        }

        if (exact != null) {
            return exact;
        }
        if (prefixed != null) {
            return prefixed;
        }
        // The user picked the LEGO folder itself. Require real evidence rather than assuming it
        // whenever no lego* child was seen, which made an empty folder a successful import.
        if (hasScripts && hasData) {
            return root;
        }

        return null;
    }

    /** Walks the source tree, collecting every file to copy plus the totals for progress. */
    private void enumerate(Node root) throws Failure {
        Deque<Pending> pending = new ArrayDeque<>();
        pending.add(new Pending(root, "", 0));

        long bytes = 0;

        while (!pending.isEmpty()) {
            if (mCancellation.cancelled()) {
                throw new Failure(STATUS_CANCELLED, "Cancelled.");
            }

            Pending job = pending.removeFirst();
            if (job.mDepth > MAX_DEPTH) {
                throw new Failure(STATUS_NOT_GAME_FOLDER, "The selected folder is nested too deeply to be a game folder.");
            }

            List<Node> children = mSource.list(job.mNode);
            if (children == null) {
                throw new Failure(STATUS_READ_FAILED, "The selected folder could not be read.");
            }

            for (Node child : children) {
                if (!isUsableName(child.name)) {
                    continue;
                }

                String relPath = job.mRelPath.isEmpty() ? child.name : job.mRelPath + "/" + child.name;

                if (child.directory) {
                    pending.add(new Pending(child, relPath, job.mDepth + 1));
                    continue;
                }

                // Some providers report no size. Such a file still copies; it just cannot
                // contribute to a space check, which becomes a lower bound.
                mEntries.add(new Entry(child, relPath));
                if (child.size > 0) {
                    bytes += child.size;
                }

                if (mEntries.size() > MAX_FILES) {
                    throw new Failure(STATUS_NOT_GAME_FOLDER, "The selected folder contains far too many files to "
                            + "be a game folder. Pick the folder that holds the LEGO folder.");
                }
            }
        }

        mTotalFiles = mEntries.size();
        mTotalBytes = bytes;
    }

    private void copyAll(File dest, boolean sync) throws Failure {
        byte[] buffer = new byte[BUFFER_BYTES];

        for (Entry entry : mEntries) {
            if (mCancellation.cancelled()) {
                throw new Failure(STATUS_CANCELLED, "Cancelled.");
            }

            File out = new File(dest, entry.mRelPath);
            File parent = out.getParentFile();
            if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
                throw new Failure(STATUS_WRITE_FAILED, "Could not create " + entry.mRelPath + " in this app's storage.");
            }

            copyOne(entry, out, buffer, sync);
            mCopiedFiles.incrementAndGet();
        }
    }

    private void copyOne(Entry entry, File out, byte[] buffer, boolean sync) throws Failure {
        // Open the source first: opening the destination in the same try-with-resources header
        // leaves a zero-byte file behind whenever the source turns out to be unreadable.
        try (InputStream in = mSource.open(entry.mNode)) {
            if (in == null) {
                throw new Failure(STATUS_READ_FAILED, "Could not read " + entry.mRelPath + " from the selected folder.");
            }

            try (FileOutputStream os = new FileOutputStream(out)) {
                int read;
                while ((read = in.read(buffer)) != -1) {
                    if (mCancellation.cancelled()) {
                        throw new Failure(STATUS_CANCELLED, "Cancelled.");
                    }

                    os.write(buffer, 0, read);
                    mCopiedBytes.addAndGet(read);
                }
                if (sync) {
                    os.getFD().sync();
                }
            }
        }
        catch (IOException e) {
            if (isOutOfSpace(e)) {
                throw new Failure(STATUS_NO_SPACE, "The device ran out of space while copying " + entry.mRelPath + ".", e);
            }
            throw new Failure(STATUS_WRITE_FAILED, entry.mRelPath + " could not be copied.", e);
        }
        catch (SecurityException e) {
            // The folder grant dies with the activity, so this is what a mid-copy teardown looks like.
            throw new Failure(STATUS_READ_FAILED, "Access to the selected folder was lost before the copy finished.", e);
        }

        // Existence is all the game's own startup check can test for, so catch a short write
        // here, where the expected size is still known.
        if (entry.mNode.size >= 0 && out.length() != entry.mNode.size) {
            throw new Failure(STATUS_WRITE_FAILED, entry.mRelPath + " was copied incompletely ("
                    + out.length() + " of " + entry.mNode.size + " bytes).");
        }
    }

    // --- leftovers --------------------------------------------------------------------

    /**
     * The game data under the external files directory: what an import created, plus a LEGO
     * directory that arrived some other way, such as one pushed in over adb. Removal is only
     * offered when that data has already failed to load, so a manual install that got this far
     * is broken too and clearing it is the point. Deliberately never saves/ and never isle.ini.
     */
    static List<File> importedPaths(File filesDir) {
        List<File> paths = new ArrayList<>();
        if (filesDir == null) {
            return paths;
        }

        File game = new File(filesDir, GAME_DIR);
        if (game.exists()) {
            paths.add(game);
        }

        File[] children = filesDir.listFiles();
        if (children != null) {
            for (File child : children) {
                String name = child.getName();
                if (name.startsWith(IMPORTED_PREFIX) || name.contains(UNREADABLE_MARKER)) {
                    paths.add(child);
                }
            }
        }

        return paths;
    }

    static boolean hasImportedData(File filesDir) {
        return !importedPaths(filesDir).isEmpty();
    }

    /**
     * Removes every path importedPaths lists. Returns true when none remain: an
     * "*.unreadable.*" leftover is by construction something this app could not delete, so
     * false is an expected outcome and the caller imports beside it instead.
     *
     * Only safe while the game is not reading its data, which is why the two callers are the
     * start of an import and the removal button, both inside the startup file check.
     */
    static boolean removeImportedData(File filesDir) {
        boolean removedAll = true;

        for (File path : importedPaths(filesDir)) {
            removedAll &= deleteRecursively(path);
        }

        return removedAll;
    }

    static boolean deleteRecursively(File path) {
        if (!path.exists()) {
            return true;
        }

        File[] children = path.listFiles();
        if (children != null) {
            for (File child : children) {
                deleteRecursively(child);
            }
        }

        return path.delete();
    }

    static long sizeOf(File path) {
        File[] children = path.listFiles();
        if (children == null) {
            return path.length();
        }

        long total = 0;
        for (File child : children) {
            total += sizeOf(child);
        }
        return total;
    }

    /** Rejects dotfiles, and any display name that could escape the destination directory. */
    static boolean isUsableName(String name) {
        return name != null && !name.isEmpty() && !name.startsWith(".") && !name.equals("..")
                && !name.contains("/") && !name.contains("\\");
    }

    static boolean isOutOfSpace(IOException e) {
        String message = e.getMessage();
        if (message == null) {
            return false;
        }

        String lower = message.toLowerCase(Locale.ROOT);
        return lower.contains("enospc") || lower.contains("no space left");
    }

    private static final class Entry {
        final Node mNode;
        final String mRelPath;

        Entry(Node node, String relPath) {
            mNode = node;
            mRelPath = relPath;
        }
    }

    private static final class Pending {
        final Node mNode;
        final String mRelPath;
        final int mDepth;

        Pending(Node node, String relPath, int depth) {
            mNode = node;
            mRelPath = relPath;
            mDepth = depth;
        }
    }
}
