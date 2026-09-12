package org.legoisland.isle;

import android.content.Context;
import android.graphics.Rect;
import android.os.SystemClock;
import android.view.ContextThemeWrapper;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.widget.RelativeLayout;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

/** Loads, edits and saves the touch layout against a fake config file, without starting the game. */
@RunWith(AndroidJUnit4.class)
public final class TouchLayoutControllerTest {
    private static final int WIDTH = 2400, HEIGHT = 1080;
    private static final String MENU_KEY = TouchLayout.POSITION_KEYS[TouchLayout.MENU];

    private final FakeStore store = new FakeStore();
    private final List<String> reports = Collections.synchronizedList(new ArrayList<>());
    private final AtomicInteger closings = new AtomicInteger();
    private ExecutorService io;
    private RelativeLayout host;
    private View menu;
    private float density;
    private TouchLayoutController controller;
    private long downTime;

    /** Answers reads from a queue and records writes; either can be held until released. */
    private static final class FakeStore implements TouchLayoutController.ConfigStore {
        private final ArrayDeque<Object> reads = new ArrayDeque<>();
        private final List<String> writes = new ArrayList<>();
        private String writeFailure;
        private CountDownLatch gate;

        /** Values in TouchLayout.KEYS order, or a RuntimeException for the read to throw. */
        synchronized void queueRead(Object result) { reads.add(result); }

        synchronized void failWrites(String message) { writeFailure = message; }

        synchronized List<String> writes() { return new ArrayList<>(writes); }

        synchronized CountDownLatch hold() {
            gate = new CountDownLatch(1);
            return gate;
        }

        @Override public String path() { return "isle.ini"; }

        @Override public String[] read(String path, String[] keys) {
            Object result;
            synchronized (this) { result = reads.poll(); }
            awaitRelease();
            if (result instanceof RuntimeException) throw (RuntimeException) result;
            return result != null ? (String[]) result : new String[keys.length];
        }

        @Override public String write(String path, String[] keys, String[] values) {
            awaitRelease();
            synchronized (this) {
                writes.add(Arrays.toString(keys) + "=" + Arrays.toString(values));
                return writeFailure;
            }
        }

        private void awaitRelease() {
            CountDownLatch held;
            synchronized (this) { held = gate; }
            if (held == null) return;
            try {
                held.await(5, TimeUnit.SECONDS);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
    }

    private static String[] saved(String scale, String menuPosition) {
        return new String[] {scale, null, menuPosition, null, null};
    }

    @Before public void setUp() {
        io = Executors.newSingleThreadExecutor();
        downTime = SystemClock.uptimeMillis();
        onMain(() -> {
            Context context = new ContextThemeWrapper(
                InstrumentationRegistry.getInstrumentation().getTargetContext(), android.R.style.Theme);
            density = context.getResources().getDisplayMetrics().density;
            host = new RelativeLayout(context);
            menu = new View(context);
            controller = new TouchLayoutController(host, menu, new View(context), new View(context),
                (layout, first) -> reports.add(layout.scale + (first ? " first" : "")), store, io);
            layOut(host);
        });
    }

    @After public void tearDown() {
        onMain(() -> controller.destroy());
        io.shutdownNow();
    }

    private static void onMain(Runnable test) { InstrumentationRegistry.getInstrumentation().runOnMainSync(test); }

    private static void layOut(View view) {
        view.measure(View.MeasureSpec.makeMeasureSpec(WIDTH, View.MeasureSpec.EXACTLY),
            View.MeasureSpec.makeMeasureSpec(HEIGHT, View.MeasureSpec.EXACTLY));
        view.layout(0, 0, WIDTH, HEIGHT);
    }

    /** Waits for every read or write queued so far, then for the results they posted. */
    private void settle() throws Exception {
        io.submit(() -> { }).get(5, TimeUnit.SECONDS);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    /** After destroy, when nothing more can be queued: waits for the held work to finish. */
    private void drain() throws Exception {
        assertTrue(io.awaitTermination(5, TimeUnit.SECONDS));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private TouchLayoutEditor editor() {
        for (int i = 0; i < host.getChildCount(); i++) {
            if (host.getChildAt(i) instanceof TouchLayoutEditor) return (TouchLayoutEditor) host.getChildAt(i);
        }
        return null;
    }

    /** Reads the queued layout, then opens the editor over the laid-out host. */
    private void openEditor() throws Exception {
        onMain(controller::load);
        settle();
        onMain(() -> {
            assertTrue(controller.startEditor(closings::incrementAndGet));
            layOut(host);
        });
    }

    private void dispatch(View view, int action, float x, float y) {
        MotionEvent event = MotionEvent.obtain(downTime, SystemClock.uptimeMillis(), action, x, y, 0);
        try {
            view.dispatchTouchEvent(event);
        } finally {
            event.recycle();
        }
    }

    private void tap(int tool) {
        onMain(() -> {
            TouchLayoutEditor editor = editor();
            Rect box = editor.toolBounds(tool);
            dispatch(editor, MotionEvent.ACTION_DOWN, box.exactCenterX(), box.exactCenterY());
            dispatch(editor, MotionEvent.ACTION_UP, box.exactCenterX(), box.exactCenterY());
        });
    }

    @Test public void firstReadIsReportedOnceAndLaterReadsInOrder() throws Exception {
        store.queueRead(saved("1.5", null));
        store.queueRead(saved("2", null));
        onMain(() -> {
            assertFalse(controller.isLoaded());
            controller.load();
            controller.load();
        });
        settle();
        assertEquals(Arrays.asList("1.5 first", "2.0"), reports);
        onMain(() -> {
            assertTrue(controller.isLoaded());
            layOut(host);
            assertEquals(TouchLayout.width(TouchLayout.MENU, density, 2), menu.getWidth());
        });
    }

    @Test public void unreadableLayoutStillShowsTheButtons() throws Exception {
        store.queueRead(new IllegalStateException("unreadable"));
        onMain(controller::load);
        settle();
        assertEquals(Collections.singletonList("1.0 first"), reports);
        onMain(() -> assertTrue(controller.isLoaded()));
    }

    @Test public void editorWaitsForTheFirstRead() throws Exception {
        // The read's result is posted to this thread, so it cannot land before startEditor runs.
        onMain(() -> {
            controller.load();
            assertFalse(controller.startEditor(closings::incrementAndGet));
        });
        settle();
        onMain(() -> {
            assertTrue(controller.startEditor(closings::incrementAndGet));
            assertFalse(controller.startEditor(closings::incrementAndGet));
            assertNotNull(editor());
        });
    }

    @Test public void failedSaveKeepsTheDraftForARetry() throws Exception {
        store.queueRead(saved(null, "0.2500,0.2500"));
        openEditor();
        store.failWrites("Could not write isle.ini.");
        // Held, or the failure could come back before the editor is checked for saving.
        CountDownLatch release = store.hold();
        tap(TouchLayoutEditor.RESET);
        tap(TouchLayoutEditor.DONE);
        onMain(() -> assertTrue(editor().isBusy()));
        release.countDown();
        settle();
        onMain(() -> {
            assertNotNull(editor());
            assertFalse(editor().isBusy());
            assertTrue(editor().draft().isDefault(TouchLayout.MENU));
        });
        assertEquals(0, closings.get());
        assertEquals(Collections.singletonList("[" + MENU_KEY + "]=[null]"), store.writes());

        store.failWrites(null);
        tap(TouchLayoutEditor.DONE);
        settle();
        onMain(() -> assertNull(editor()));
        assertEquals(1, closings.get());
        assertEquals(2, store.writes().size());
        onMain(() -> {
            assertTrue(controller.startEditor(closings::incrementAndGet));
            assertTrue(editor().draft().isDefault(TouchLayout.MENU));
        });
    }

    @Test public void backIsIgnoredWhileSaving() throws Exception {
        store.queueRead(saved(null, "0.2500,0.2500"));
        openEditor();
        CountDownLatch release = store.hold();
        tap(TouchLayoutEditor.RESET);
        tap(TouchLayoutEditor.DONE);
        onMain(() -> {
            assertTrue(controller.dispatchBack(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK)));
            assertTrue(controller.dispatchBack(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_BACK)));
            assertNotNull(editor());
        });
        release.countDown();
        settle();
        onMain(() -> assertNull(editor()));
        assertEquals(1, closings.get());
    }

    @Test public void backCancelsOnlyOnARealRelease() throws Exception {
        openEditor();
        onMain(() -> {
            assertFalse(controller.dispatchBack(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_BUTTON_A)));
            assertTrue(controller.dispatchBack(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK)));
            assertNotNull(editor());
            KeyEvent cancelled = KeyEvent.changeFlags(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_BACK),
                KeyEvent.FLAG_CANCELED);
            assertTrue(controller.dispatchBack(cancelled));
            assertNotNull(editor());
            assertTrue(controller.dispatchBack(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_BACK)));
            assertNull(editor());
            assertFalse(controller.dispatchBack(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_BACK)));
        });
        assertEquals(1, closings.get());
        assertTrue(store.writes().isEmpty());
    }

    @Test public void unchangedDraftClosesWithoutSaving() throws Exception {
        openEditor();
        tap(TouchLayoutEditor.DONE);
        settle();
        onMain(() -> assertNull(editor()));
        assertEquals(1, closings.get());
        assertTrue(store.writes().isEmpty());
    }

    @Test public void resultsAfterDestroyAreIgnored() throws Exception {
        store.queueRead(saved(null, "0.2500,0.2500"));
        openEditor();
        reports.clear();
        CountDownLatch release = store.hold();
        store.queueRead(saved("2", null));
        tap(TouchLayoutEditor.RESET);
        tap(TouchLayoutEditor.DONE);
        onMain(() -> {
            controller.load();
            controller.destroy();
        });
        release.countDown();
        drain();
        onMain(() -> assertNotNull(editor()));
        assertEquals(0, closings.get());
        assertTrue(reports.isEmpty());
        assertEquals(1, store.writes().size());
    }

    @Test public void saveAfterDestroyWritesNothing() throws Exception {
        store.queueRead(saved(null, "0.2500,0.2500"));
        openEditor();
        onMain(controller::destroy);
        tap(TouchLayoutEditor.RESET);
        tap(TouchLayoutEditor.DONE);
        drain();
        onMain(() -> {
            assertNotNull(editor());
            assertFalse(editor().isBusy());
            assertFalse(controller.startEditor(closings::incrementAndGet));
        });
        assertEquals(0, closings.get());
        assertTrue(store.writes().isEmpty());
    }
}
