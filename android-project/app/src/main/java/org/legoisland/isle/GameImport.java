package org.legoisland.isle;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ContentResolver;
import android.content.DialogInterface;
import android.database.Cursor;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.provider.DocumentsContract;
import android.text.format.Formatter;
import android.util.Log;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Deque;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;

/**
 * Copies a LEGO Island installation out of a user-picked SAF tree and into the app's external
 * files directory, which is the only place the game can read from without a permission.
 *
 * The caller is the SDL thread, inside SDL_AppInit, so nothing is drawing yet and there is no
 * renderer to put a progress bar on. The work therefore runs on a worker thread with an Android
 * dialog over the surface, and the SDL thread simply waits: blocking it is safe because SDL runs
 * the game off the UI thread, so this is not the thread Android watches for ANRs.
 *
 * Reached only from IsleActivity, so unlike IsleActivity this class needs no proguard keep rule.
 */
final class GameImport {
    // Mirrored in ISLE/android/filepicker.h as AndroidImportStatus; keep the numbering in step.
    static final int STATUS_OK = 0;
    static final int STATUS_CANCELLED = 1;
    static final int STATUS_NO_SPACE = 2;
    static final int STATUS_NOT_GAME_FOLDER = 3;
    static final int STATUS_READ_FAILED = 4;
    static final int STATUS_WRITE_FAILED = 5;
    static final int STATUS_INTERNAL_ERROR = 6;

    private static final String TAG = "IsleActivity";

    /**
     * The destination is always literally "LEGO", whatever the source folder was called.
     * MxString::MapPathToFilesystem resolves the game's internal paths by matching a globbed
     * file as a case-insensitive suffix of the requested one, so a tree copied in as
     * "LEGO Island/Scripts/..." never matches "/LEGO/Scripts/..." and the import silently
     * produces something the game cannot read.
     */
    private static final String GAME_DIR = "LEGO";

    private static final String IMPORTED_PREFIX = "imported-";
    private static final String UNREADABLE_MARKER = ".unreadable.";

    private static final int MAX_DEPTH = 16;
    private static final int MAX_FILES = 20000;
    private static final int BUFFER_BYTES = 256 * 1024;
    private static final long SPACE_MARGIN_BYTES = 32L * 1024 * 1024;
    private static final long TICK_MS = 150;
    private static final int PROGRESS_STEPS = 1000;

    private final Activity mActivity;
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private final AtomicBoolean mCancelled = new AtomicBoolean();
    private final AtomicLong mCopiedBytes = new AtomicLong();
    private final AtomicInteger mCopiedFiles = new AtomicInteger();

    private volatile String mPhase = "Preparing...";
    private volatile long mTotalBytes;
    private volatile int mTotalFiles;
    private volatile boolean mFinished;

    private String mImportedRoot;
    private String mFailureDetail;

    // Touched on the UI thread only.
    private AlertDialog mDialog;
    private TextView mStatusView;
    private TextView mDetailView;
    private ProgressBar mBar;
    private boolean mHadKeepScreenOn;

    GameImport(Activity activity) {
        mActivity = activity;
    }

    String getImportedRoot() {
        return mImportedRoot;
    }

    /**
     * Runs one import to completion. Blocks the calling thread, which must not be the UI thread.
     */
    int run(final String treeUri) {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            // The worker would never get to run its UI posts, and this would deadlock.
            Log.e(TAG, "importGameFiles must not be called on the UI thread");
            return STATUS_INTERNAL_ERROR;
        }

        final CountDownLatch done = new CountDownLatch(1);
        final int[] status = {STATUS_INTERNAL_ERROR};

        mHandler.post(this::showProgressDialog);

        Thread worker = new Thread(() -> {
            try {
                status[0] = importTree(treeUri);
            }
            catch (Throwable t) {
                Log.e(TAG, "Failed to import game files from " + treeUri, t);
                status[0] = STATUS_INTERNAL_ERROR;
            }
            finally {
                final int result = status[0];
                // Tear the dialog down before releasing the SDL thread, so nothing is left
                // floating over the surface as the game starts creating its renderer.
                mHandler.post(() -> {
                    dismissProgressDialog();
                    if (result == STATUS_OK || result == STATUS_CANCELLED) {
                        done.countDown();
                    }
                    else {
                        // Hold the SDL thread until the user has read what went wrong; native
                        // is about to put its own message box on screen otherwise.
                        showFailureDialog(result, done);
                    }
                });
            }
        }, "IsleGameImport");
        worker.start();

        try {
            done.await();
        }
        catch (InterruptedException e) {
            // Not expected: the SDL thread is never interrupted. Do not leave the worker
            // copying into a directory nobody is waiting for.
            mCancelled.set(true);
            Thread.currentThread().interrupt();
            return STATUS_INTERNAL_ERROR;
        }

        return status[0];
    }

    // --- the work ---------------------------------------------------------------------

    private int importTree(String treeUri) {
        File filesDir = mActivity.getExternalFilesDir(null);
        if (filesDir == null) {
            mFailureDetail = "This app has no external storage directory right now.";
            return STATUS_WRITE_FAILED;
        }

        ContentResolver resolver = mActivity.getContentResolver();
        Uri tree = Uri.parse(treeUri);
        String rootId = DocumentsContract.getTreeDocumentId(tree);

        String sourceId = resolveSourceRoot(resolver, tree, rootId);
        if (sourceId == null) {
            mFailureDetail = "The selected folder does not contain a LEGO folder, and does not "
                    + "look like one itself. Pick the folder that holds the LEGO folder from "
                    + "your LEGO Island installation.";
            return STATUS_NOT_GAME_FOLDER;
        }

        setPhase("Scanning game files...");
        List<Entry> entries = new ArrayList<>();
        int status = enumerate(resolver, tree, sourceId, entries);
        if (status != STATUS_OK) {
            return status;
        }
        if (entries.isEmpty()) {
            mFailureDetail = "The selected folder contains no game files.";
            return STATUS_NOT_GAME_FOLDER;
        }

        // Everything the previous import left behind is about to be deleted, so its space
        // counts as available.
        long reclaimable = 0;
        for (File path : importedPaths(filesDir)) {
            reclaimable += sizeOf(path);
        }

        long required = mTotalBytes + SPACE_MARGIN_BYTES;
        long available = filesDir.getUsableSpace() + reclaimable;
        if (available < required) {
            mFailureDetail = "Copying the game files needs "
                    + Formatter.formatFileSize(mActivity, mTotalBytes) + ", but only "
                    + Formatter.formatFileSize(mActivity, available) + " is free. Free up about "
                    + Formatter.formatFileSize(mActivity, required - available) + " and try again.";
            return STATUS_NO_SPACE;
        }

        // Delete first, then copy. That is what makes "a second import does not leave the first
        // behind" true by construction, and it means the space measured above is the space
        // actually needed rather than twice it. The cost is that cancelling a re-import leaves
        // nothing, which is acceptable: this prompt is only reached because the data already
        // present is unusable.
        setPhase("Removing previous game data...");
        boolean cleared = removeImportedData(filesDir);

        File root = filesDir;
        if (!cleared && new File(filesDir, GAME_DIR).exists()) {
            // Something in the way could not be deleted, typically a tree pushed in by adb with
            // foreign ownership. Import beside it instead of failing.
            root = new File(filesDir, IMPORTED_PREFIX + System.currentTimeMillis());
        }

        File dest = new File(root, GAME_DIR);
        setPhase("Copying game files...");
        status = copyAll(resolver, tree, entries, dest);

        if (status != STATUS_OK) {
            // Never leave a partial tree behind: the next launch would stat its way into it and
            // treat it as a real installation.
            setPhase("Cleaning up...");
            deleteRecursively(dest);
            if (root != filesDir) {
                deleteRecursively(root);
            }
            return status;
        }

        mImportedRoot = root.getAbsolutePath();
        Log.i(TAG, "Imported " + mTotalFiles + " files (" + mTotalBytes + " bytes) into " + dest);
        return STATUS_OK;
    }

    /**
     * Picks the subtree to copy and rejects anything that does not look like an installation.
     * Returns the document id of the game directory, or null.
     */
    private String resolveSourceRoot(ContentResolver resolver, Uri tree, String rootId) {
        Cursor cursor = queryChildren(resolver, tree, rootId);
        if (cursor == null) {
            return null;
        }

        String exact = null;
        String prefixed = null;
        boolean hasScripts = false;
        boolean hasData = false;

        try {
            while (cursor.moveToNext()) {
                String id = cursor.getString(0);
                String name = cursor.getString(1);
                if (name == null || !DocumentsContract.Document.MIME_TYPE_DIR.equals(cursor.getString(2))) {
                    continue;
                }

                String lower = name.toLowerCase(Locale.ROOT);
                if (lower.equals("lego") && exact == null) {
                    exact = id;
                }
                else if (lower.startsWith("lego") && prefixed == null) {
                    prefixed = id;
                }

                hasScripts |= lower.equals("scripts");
                hasData |= lower.equals("data");
            }
        }
        finally {
            cursor.close();
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
            return rootId;
        }

        return null;
    }

    /** Walks the source tree, collecting every file to copy plus the totals for the progress bar. */
    private int enumerate(ContentResolver resolver, Uri tree, String rootId, List<Entry> out) {
        Deque<Pending> pending = new ArrayDeque<>();
        pending.add(new Pending(rootId, "", 0));

        long bytes = 0;

        while (!pending.isEmpty()) {
            if (mCancelled.get()) {
                return STATUS_CANCELLED;
            }

            Pending job = pending.removeFirst();
            if (job.mDepth > MAX_DEPTH) {
                mFailureDetail = "The selected folder is nested too deeply to be a game folder.";
                return STATUS_NOT_GAME_FOLDER;
            }

            Cursor cursor = queryChildren(resolver, tree, job.mDocId);
            if (cursor == null) {
                mFailureDetail = "The selected folder could not be read.";
                return STATUS_READ_FAILED;
            }

            try {
                while (cursor.moveToNext()) {
                    String id = cursor.getString(0);
                    String name = cursor.getString(1);
                    String mime = cursor.getString(2);

                    if (!isUsableName(name)) {
                        continue;
                    }

                    String relPath = job.mRelPath.isEmpty() ? name : job.mRelPath + "/" + name;

                    if (DocumentsContract.Document.MIME_TYPE_DIR.equals(mime)) {
                        pending.add(new Pending(id, relPath, job.mDepth + 1));
                        continue;
                    }

                    // Some providers report no size. Such a file still copies; it just cannot
                    // contribute to the precheck, which becomes a lower bound.
                    long size = cursor.isNull(3) ? -1 : cursor.getLong(3);
                    out.add(new Entry(id, relPath, size));
                    if (size > 0) {
                        bytes += size;
                    }

                    if (out.size() > MAX_FILES) {
                        mFailureDetail = "The selected folder contains far too many files to be a "
                                + "game folder. Pick the folder that holds the LEGO folder.";
                        return STATUS_NOT_GAME_FOLDER;
                    }
                }
            }
            finally {
                cursor.close();
            }
        }

        mTotalFiles = out.size();
        mTotalBytes = bytes;
        return STATUS_OK;
    }

    private int copyAll(ContentResolver resolver, Uri tree, List<Entry> entries, File dest) {
        byte[] buffer = new byte[BUFFER_BYTES];

        for (Entry entry : entries) {
            if (mCancelled.get()) {
                return STATUS_CANCELLED;
            }

            File out = new File(dest, entry.mRelPath);
            File parent = out.getParentFile();
            if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
                mFailureDetail = "Could not create " + entry.mRelPath + " in this app's storage.";
                return STATUS_WRITE_FAILED;
            }

            int status = copyOne(resolver, tree, entry, out, buffer);
            if (status != STATUS_OK) {
                return status;
            }

            mCopiedFiles.incrementAndGet();
        }

        return STATUS_OK;
    }

    private int copyOne(ContentResolver resolver, Uri tree, Entry entry, File out, byte[] buffer) {
        Uri document = DocumentsContract.buildDocumentUriUsingTree(tree, entry.mDocId);

        // Open the source first: opening the destination in the same try-with-resources header
        // leaves a zero-byte file behind whenever the source turns out to be unreadable.
        try (InputStream in = resolver.openInputStream(document)) {
            if (in == null) {
                mFailureDetail = "Could not read " + entry.mRelPath + " from the selected folder.";
                return STATUS_READ_FAILED;
            }

            try (OutputStream os = new FileOutputStream(out)) {
                int read;
                while ((read = in.read(buffer)) != -1) {
                    if (mCancelled.get()) {
                        return STATUS_CANCELLED;
                    }

                    os.write(buffer, 0, read);
                    mCopiedBytes.addAndGet(read);
                }
            }
        }
        catch (IOException e) {
            Log.e(TAG, "Failed to copy " + document + " to " + out, e);
            if (isOutOfSpace(e)) {
                mFailureDetail = "The device ran out of space while copying " + entry.mRelPath + ".";
                return STATUS_NO_SPACE;
            }
            mFailureDetail = entry.mRelPath + " could not be copied.";
            return STATUS_WRITE_FAILED;
        }
        catch (SecurityException e) {
            // The SAF grant dies with the activity, so this is what a mid-copy teardown looks like.
            Log.e(TAG, "Lost access to " + document, e);
            mFailureDetail = "Access to the selected folder was lost before the copy finished.";
            return STATUS_READ_FAILED;
        }

        // Existence is all the game's own startup check can test for, so catch a short write
        // here, where the expected size is still known.
        if (entry.mSize >= 0 && out.length() != entry.mSize) {
            Log.e(TAG, "Short copy of " + entry.mRelPath + ": " + out.length() + " of " + entry.mSize);
            mFailureDetail = entry.mRelPath + " was copied incompletely.";
            return STATUS_WRITE_FAILED;
        }

        return STATUS_OK;
    }

    // --- leftovers --------------------------------------------------------------------

    /**
     * Everything an import has ever created under the external files directory. Deliberately
     * never saves/ and never isle.ini.
     */
    private static List<File> importedPaths(File filesDir) {
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
     * Removes every imported path. Returns true when none remain: an "*.unreadable.*" leftover
     * is by construction something this app could not delete, so false is an expected outcome
     * and the caller imports beside it instead.
     *
     * Only safe while the game is not reading its data, which is why the two callers are the
     * start of an import and the removal button, both inside the startup file check.
     */
    static boolean removeImportedData(File filesDir) {
        boolean removedAll = true;

        for (File path : importedPaths(filesDir)) {
            if (!deleteRecursively(path)) {
                Log.w(TAG, "Could not remove " + path);
                removedAll = false;
            }
        }

        return removedAll;
    }

    private static boolean deleteRecursively(File path) {
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

    private static long sizeOf(File path) {
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

    // --- UI ---------------------------------------------------------------------------

    private void showProgressDialog() {
        float density = mActivity.getResources().getDisplayMetrics().density;
        int padding = (int) (24 * density);
        int spacing = (int) (12 * density);

        LinearLayout content = new LinearLayout(mActivity);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setPadding(padding, padding, padding, padding);

        mStatusView = new TextView(mActivity);
        mStatusView.setText(mPhase);
        content.addView(mStatusView);

        mBar = new ProgressBar(mActivity, null, android.R.attr.progressBarStyleHorizontal);
        mBar.setMax(PROGRESS_STEPS);
        mBar.setIndeterminate(true);
        LinearLayout.LayoutParams barParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
        );
        barParams.topMargin = spacing;
        barParams.bottomMargin = spacing;
        content.addView(mBar, barParams);

        mDetailView = new TextView(mActivity);
        content.addView(mDetailView);

        mDialog = new AlertDialog.Builder(mActivity)
                .setTitle("LEGO® Island")
                .setView(content)
                .setNegativeButton("Cancel", null)
                .setCancelable(false)
                .create();
        mDialog.setCanceledOnTouchOutside(false);
        mDialog.show();

        // The builder's own listener always dismisses. Replace it after show(), which is also
        // the first moment getButton returns anything, so the dialog can stay up saying what it
        // is doing while the worker unwinds and deletes what it had written.
        final Button cancel = mDialog.getButton(AlertDialog.BUTTON_NEGATIVE);
        cancel.setOnClickListener(v -> {
            mCancelled.set(true);
            cancel.setEnabled(false);
            mStatusView.setText("Cancelling...");
        });

        // SDL manages this flag itself, so save and restore rather than clearing unconditionally.
        mHadKeepScreenOn = (mActivity.getWindow().getAttributes().flags
                & WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON) != 0;
        mActivity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        mHandler.postDelayed(mTicker, TICK_MS);
    }

    /**
     * Repaints from the worker's counters on the UI thread. The worker only ever bumps atomics,
     * so it cannot flood the main looper however small the copy buffer gets.
     */
    private final Runnable mTicker = new Runnable() {
        @Override
        public void run() {
            if (mFinished || mDialog == null) {
                return;
            }

            mStatusView.setText(mCancelled.get() ? "Cancelling..." : mPhase);

            long total = mTotalBytes;
            if (total > 0) {
                long copied = mCopiedBytes.get();
                mBar.setIndeterminate(false);
                mBar.setProgress((int) Math.min(PROGRESS_STEPS, copied * PROGRESS_STEPS / total));
                mDetailView.setText(String.format(
                        Locale.ROOT,
                        "%s of %s (%d of %d files)",
                        Formatter.formatFileSize(mActivity, copied),
                        Formatter.formatFileSize(mActivity, total),
                        mCopiedFiles.get(),
                        mTotalFiles
                ));
            }

            mHandler.postDelayed(this, TICK_MS);
        }
    };

    private void dismissProgressDialog() {
        mFinished = true;
        mHandler.removeCallbacks(mTicker);

        if (!mHadKeepScreenOn) {
            mActivity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        }

        if (mDialog == null) {
            return;
        }

        try {
            if (!mActivity.isFinishing() && !mActivity.isDestroyed()) {
                mDialog.dismiss();
            }
        }
        catch (IllegalArgumentException e) {
            // The activity went away underneath the dialog; nothing left to dismiss.
            Log.w(TAG, "Could not dismiss the import dialog", e);
        }

        mDialog = null;
    }

    private void showFailureDialog(int status, final CountDownLatch done) {
        String detail = mFailureDetail != null ? mFailureDetail : "The game files could not be copied.";
        String title = status == STATUS_NO_SPACE ? "Not enough space" : "Copying game files failed";

        DialogInterface.OnDismissListener release = dialog -> done.countDown();

        try {
            new AlertDialog.Builder(mActivity)
                    .setTitle(title)
                    .setMessage(detail)
                    .setPositiveButton("OK", null)
                    .setOnDismissListener(release)
                    .show();
        }
        catch (RuntimeException e) {
            // No window to show it in. Log it and let the game get on with reporting its own
            // failure rather than leaving the SDL thread parked forever.
            Log.e(TAG, detail, e);
            done.countDown();
        }
    }

    private void setPhase(String phase) {
        mPhase = phase;
    }

    // --- helpers ----------------------------------------------------------------------

    private static Cursor queryChildren(ContentResolver resolver, Uri tree, String docId) {
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, docId);
        String[] columns = {
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE,
            DocumentsContract.Document.COLUMN_SIZE
        };

        try {
            return resolver.query(children, columns, null, null, null);
        }
        catch (RuntimeException e) {
            Log.e(TAG, "Failed to list children of " + docId, e);
            return null;
        }
    }

    /** Rejects dotfiles, and any display name that could escape the destination directory. */
    private static boolean isUsableName(String name) {
        return name != null && !name.isEmpty() && !name.startsWith(".") && !name.equals("..")
                && !name.contains("/") && !name.contains("\\");
    }

    private static boolean isOutOfSpace(IOException e) {
        String message = e.getMessage();
        if (message == null) {
            return false;
        }

        String lower = message.toLowerCase(Locale.ROOT);
        return lower.contains("enospc") || lower.contains("no space left");
    }

    private static final class Entry {
        final String mDocId;
        final String mRelPath;
        final long mSize;

        Entry(String docId, String relPath, long size) {
            mDocId = docId;
            mRelPath = relPath;
            mSize = size;
        }
    }

    private static final class Pending {
        final String mDocId;
        final String mRelPath;
        final int mDepth;

        Pending(String docId, String relPath, int depth) {
            mDocId = docId;
            mRelPath = relPath;
            mDepth = depth;
        }
    }
}
