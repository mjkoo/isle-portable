package org.legoisland.isle;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.widget.RelativeLayout;
import android.widget.Toast;

import java.util.ArrayList;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.RejectedExecutionException;

/**
 * Reads the saved touch layout, places the buttons from it, and runs the layout editor.
 *
 * Config reads and writes run on one background thread, so they complete in request order, and
 * their results come back to the UI thread, where everything else here happens. Results arriving
 * after destroy() are dropped.
 */
final class TouchLayoutController {
    /** The config file, behind an interface so tests need not load the game's native libraries. */
    interface ConfigStore {
        String path();
        /** Values in keys order, null for an absent key. */
        String[] read(String path, String[] keys);
        /** Null once written, otherwise a sentence for the player saying what failed. */
        String write(String path, String[] keys, String[] values);
    }

    interface Listener {
        /** A read has been applied, or failed and left the layout as it was; first only once. */
        void onLayoutRead(TouchLayout layout, boolean first);
    }

    static final ConfigStore SETTINGS = new ConfigStore() {
        @Override public String path() { return SettingsBridge.path(); }

        @Override public String[] read(String path, String[] keys) { return SettingsBridge.read(path, keys); }

        @Override public String write(String path, String[] keys, String[] values) {
            return SettingsBridge.write(path, keys, values, new String[0]);
        }
    };

    private static final String TAG = "IsleActivity";

    private final Context context;
    private final TouchControlsLayer layer;
    private final Listener listener;
    private final ConfigStore store;
    private final ExecutorService io;
    private final Handler ui = new Handler(Looper.getMainLooper());
    private TouchLayout layout = TouchLayout.DEFAULT;
    // The buttons stay hidden until the saved layout has been read, so none first appears in the
    // wrong place, and the editor never starts from positions that were not read yet.
    private boolean loaded;
    private boolean destroyed;
    // Only while the editor is open over the paused game.
    private TouchLayoutEditor editor;
    private TouchLayout editorOriginal;
    private Runnable editorClosed;

    TouchLayoutController(RelativeLayout host, View menu, View escape, View space, Listener listener) {
        this(host, menu, escape, space, listener, SETTINGS, Executors.newSingleThreadExecutor());
    }

    TouchLayoutController(RelativeLayout host, View menu, View escape, View space, Listener listener,
            ConfigStore store, ExecutorService io) {
        context = host.getContext();
        layer = new TouchControlsLayer(host, menu, escape, space);
        this.listener = listener;
        this.store = store;
        this.io = io;
    }

    boolean isLoaded() { return loaded; }

    /** The layout in effect: the default until the first read. */
    TouchLayout layout() { return layout; }

    void requestApplyInsets() { layer.requestApplyInsets(); }

    /** Reads the saved layout and applies it, then tells the listener. */
    void load() {
        try {
            io.execute(() -> {
                TouchLayout read = null;
                try {
                    read = TouchLayout.parse(store.read(store.path(), TouchLayout.KEYS));
                } catch (RuntimeException e) {
                    Log.w(TAG, "Could not read the touch layout; using the default", e);
                } finally {
                    // Whatever the read did, the buttons must still appear, if only at the default.
                    TouchLayout result = read;
                    ui.post(() -> apply(result));
                }
            });
        } catch (RejectedExecutionException e) {
            // Only after destroy, when there are no controls left to update.
        }
    }

    private void apply(TouchLayout read) {
        if (destroyed) return;
        if (read != null) layout = read;
        layer.setTouchLayout(layout);
        if (editor != null) editor.setAppearance(layout);
        boolean first = !loaded;
        loaded = true;
        listener.onLayoutRead(layout, first);
    }

    /**
     * Opens the editor over everything. onClosed runs when Done or Back closes it, never after
     * destroy. Refused before the first read, while the editor is open, and after destroy.
     */
    boolean startEditor(Runnable onClosed) {
        if (destroyed || !loaded || editor != null) return false;
        editorOriginal = layout;
        editorClosed = onClosed;
        editor = new TouchLayoutEditor(context, layout, this::save);
        layer.showEditor(editor);
        return true;
    }

    private void save(TouchLayout draft) {
        ArrayList<String> keys = new ArrayList<>(), values = new ArrayList<>();
        draft.changedPositions(editorOriginal, keys, values);
        if (keys.isEmpty()) {
            closeEditor();
            return;
        }
        TouchLayoutEditor current = editor;
        current.setBusy(true);
        String path = store.path();
        try {
            io.execute(() -> {
                String failure;
                try {
                    failure = store.write(path, keys.toArray(new String[0]), values.toArray(new String[0]));
                } catch (RuntimeException e) {
                    Log.w(TAG, "Could not save the touch layout", e);
                    failure = "Could not save the touch layout.";
                }
                String message = failure;
                ui.post(() -> {
                    if (destroyed || editor != current) return;
                    if (message != null) {
                        Log.w(TAG, "Touch layout not saved: " + message);
                        // Stay in the editor with the draft intact, so Done can be retried. The
                        // message already says what failed.
                        current.setBusy(false);
                        Toast.makeText(context, message + " Choose Done to try again.", Toast.LENGTH_LONG).show();
                        return;
                    }
                    layout = draft.withAppearanceOf(layout);
                    layer.setTouchLayout(layout);
                    closeEditor();
                });
            });
        } catch (RejectedExecutionException e) {
            Log.w(TAG, "Touch layout not saved: the activity is closing", e);
            current.setBusy(false);
        }
    }

    private void closeEditor() {
        if (editor == null) return;
        layer.hideEditor();
        editor = null;
        editorOriginal = null;
        Runnable closed = editorClosed;
        editorClosed = null;
        if (closed != null) closed.run();
    }

    /**
     * Back closes the editor and discards its draft, on a release that was not cancelled and not
     * while saving. True if the editor took the event, so it must not reach the game.
     */
    boolean dispatchBack(KeyEvent event) {
        if (editor == null || event.getKeyCode() != KeyEvent.KEYCODE_BACK) return false;
        if (event.getAction() == KeyEvent.ACTION_UP && !event.isCanceled() && !editor.isBusy()) closeEditor();
        return true;
    }

    void destroy() {
        destroyed = true;
        io.shutdown();
    }
}
