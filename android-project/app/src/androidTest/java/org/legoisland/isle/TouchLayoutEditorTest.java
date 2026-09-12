package org.legoisland.isle;

import android.annotation.TargetApi;
import android.graphics.Rect;
import android.os.SystemClock;
import android.view.ContextThemeWrapper;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.ViewConfiguration;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SdkSuppress;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

/** Drags the production editor's buttons with real Android events, without starting the game. */
@RunWith(AndroidJUnit4.class)
public final class TouchLayoutEditorTest {
    private static final int WIDTH = 2400, HEIGHT = 1080;
    private TouchLayoutEditor editor;
    private TouchLayout reported;
    private int doneCount;
    private long downTime;

    @Before public void setUp() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            reported = null;
            doneCount = 0;
            downTime = SystemClock.uptimeMillis();
            editor = new TouchLayoutEditor(new ContextThemeWrapper(
                InstrumentationRegistry.getInstrumentation().getTargetContext(), android.R.style.Theme),
                TouchLayout.DEFAULT, draft -> { reported = draft; doneCount++; });
            editor.layout(0, 0, WIDTH, HEIGHT);
            editor.setSafeArea(0, 0, WIDTH, HEIGHT);
        });
    }

    private static void onMain(Runnable test) { InstrumentationRegistry.getInstrumentation().runOnMainSync(test); }

    private static MotionEvent.PointerCoords[] coordinates(float... xy) {
        MotionEvent.PointerCoords[] points = new MotionEvent.PointerCoords[xy.length / 2];
        for (int i = 0; i < points.length; i++) {
            points[i] = new MotionEvent.PointerCoords();
            points[i].x = xy[2 * i];
            points[i].y = xy[2 * i + 1];
            points[i].pressure = 1;
        }
        return points;
    }

    /** One event with a pointer per x, y pair; pointer ids start at 7. */
    private MotionEvent event(int action, int flags, float... xy) {
        MotionEvent.PointerProperties[] properties = new MotionEvent.PointerProperties[xy.length / 2];
        for (int i = 0; i < properties.length; i++) {
            properties[i] = new MotionEvent.PointerProperties();
            properties[i].id = i + 7;
            properties[i].toolType = MotionEvent.TOOL_TYPE_FINGER;
        }
        return MotionEvent.obtain(downTime, SystemClock.uptimeMillis(), action, properties.length, properties,
            coordinates(xy), 0, 0, 1, 1, 0, 0, InputDevice.SOURCE_TOUCHSCREEN, flags);
    }

    private void send(MotionEvent event) {
        try {
            assertTrue("the editor consumes every touch", editor.dispatchTouchEvent(event));
        } finally {
            event.recycle();
        }
    }

    private void send(int action, float... xy) { send(event(action, 0, xy)); }

    private static int second(int action) { return action | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT); }

    private void drag(int control, float x, float y) {
        Rect box = editor.controlBounds(control);
        send(MotionEvent.ACTION_DOWN, box.exactCenterX(), box.exactCenterY());
        send(MotionEvent.ACTION_MOVE, x, y);
        send(MotionEvent.ACTION_UP, x, y);
    }

    private void tap(int tool) {
        Rect box = editor.toolBounds(tool);
        send(MotionEvent.ACTION_DOWN, box.exactCenterX(), box.exactCenterY());
        send(MotionEvent.ACTION_UP, box.exactCenterX(), box.exactCenterY());
    }

    private Rect defaultBounds(int control) {
        int[] out = new int[4];
        TouchLayout.DEFAULT.bounds(control, 0, 0, WIDTH, HEIGHT,
            InstrumentationRegistry.getInstrumentation().getTargetContext().getResources().getDisplayMetrics().density,
            out);
        return new Rect(out[0], out[1], out[2], out[3]);
    }

    private static void assertCenter(Rect box, float x, float y) {
        assertEquals(x, box.exactCenterX(), 1.5f);
        assertEquals(y, box.exactCenterY(), 1.5f);
    }

    @Test public void dragCommitsOnLift() {
        onMain(() -> {
            drag(TouchLayout.MENU, 500, 800);
            assertCenter(editor.controlBounds(TouchLayout.MENU), 500, 800);
            assertNotNull("lifting keeps the new place", editor.draft().positionValue(TouchLayout.MENU));
            assertEquals("moving saves nothing by itself", 0, doneCount);
        });
    }

    @Test public void twoFingersDragTwoButtons() {
        onMain(() -> {
            Rect escape = editor.controlBounds(TouchLayout.ESCAPE), space = editor.controlBounds(TouchLayout.SPACE);
            send(MotionEvent.ACTION_DOWN, escape.exactCenterX(), escape.exactCenterY());
            send(second(MotionEvent.ACTION_POINTER_DOWN), escape.exactCenterX(), escape.exactCenterY(),
                space.exactCenterX(), space.exactCenterY());
            send(MotionEvent.ACTION_MOVE, 600, 700, 1600, 700);
            send(second(MotionEvent.ACTION_POINTER_UP), 600, 700, 1600, 700);
            send(MotionEvent.ACTION_UP, 600, 700);
            assertCenter(editor.controlBounds(TouchLayout.ESCAPE), 600, 700);
            assertCenter(editor.controlBounds(TouchLayout.SPACE), 1600, 700);
        });
    }

    @Test public void secondFingerCannotTakeAHeldButton() {
        onMain(() -> {
            Rect menu = editor.controlBounds(TouchLayout.MENU);
            float x = menu.exactCenterX(), y = menu.exactCenterY();
            send(MotionEvent.ACTION_DOWN, x, y);
            send(second(MotionEvent.ACTION_POINTER_DOWN), x, y, x, y);
            send(MotionEvent.ACTION_MOVE, x, y, 300, 300);
            send(second(MotionEvent.ACTION_POINTER_UP), x, y, 300, 300);
            send(MotionEvent.ACTION_UP, x, y);
            assertEquals("only the owning finger moves the button", menu, editor.controlBounds(TouchLayout.MENU));
            assertNull("an unmoved button keeps its default place", editor.draft().positionValue(TouchLayout.MENU));
        });
    }

    @Test public void batchedMovesFollowTheLatestPoint() {
        onMain(() -> {
            Rect menu = editor.controlBounds(TouchLayout.MENU);
            send(MotionEvent.ACTION_DOWN, menu.exactCenterX(), menu.exactCenterY());
            MotionEvent move = event(MotionEvent.ACTION_MOVE, 0, 900, 400);
            move.addBatch(move.getEventTime() + 1, coordinates(1100, 600), 0);
            send(move);
            send(MotionEvent.ACTION_UP, 1100, 600);
            assertCenter(editor.controlBounds(TouchLayout.MENU), 1100, 600);
        });
    }

    @Test public void cancellationUndoesTheDrag() {
        onMain(() -> {
            Rect menu = editor.controlBounds(TouchLayout.MENU);
            send(MotionEvent.ACTION_DOWN, menu.exactCenterX(), menu.exactCenterY());
            send(MotionEvent.ACTION_MOVE, 1000, 500);
            send(MotionEvent.ACTION_CANCEL, 1000, 500);
            assertEquals(menu, editor.controlBounds(TouchLayout.MENU));
            assertNull(editor.draft().positionValue(TouchLayout.MENU));
        });
    }

    @SdkSuppress(minSdkVersion = 33)
    @TargetApi(33)
    @Test public void rejectedLiftUndoesTheDrag() {
        onMain(() -> {
            Rect menu = editor.controlBounds(TouchLayout.MENU);
            send(MotionEvent.ACTION_DOWN, menu.exactCenterX(), menu.exactCenterY());
            send(MotionEvent.ACTION_MOVE, 1000, 500);
            send(event(MotionEvent.ACTION_UP, MotionEvent.FLAG_CANCELED, 1000, 500));
            assertEquals(menu, editor.controlBounds(TouchLayout.MENU));
            assertNull(editor.draft().positionValue(TouchLayout.MENU));
        });
    }

    @Test public void toolbarActsOnRelease() {
        onMain(() -> {
            Rect done = editor.toolBounds(TouchLayoutEditor.DONE);
            send(MotionEvent.ACTION_DOWN, done.exactCenterX(), done.exactCenterY());
            assertEquals("pressing Done does nothing yet", 0, doneCount);
            send(MotionEvent.ACTION_UP, done.exactCenterX(), done.exactCenterY());
            assertEquals("releasing inside Done reports once", 1, doneCount);
        });
    }

    @Test public void toolbarExcursionCancels() {
        onMain(() -> {
            drag(TouchLayout.MENU, 500, 800);
            Rect reset = editor.toolBounds(TouchLayoutEditor.RESET);
            send(MotionEvent.ACTION_DOWN, reset.exactCenterX(), reset.exactCenterY());
            MotionEvent move = event(MotionEvent.ACTION_MOVE, 0, reset.exactCenterX(), reset.bottom + 50);
            move.addBatch(move.getEventTime() + 1, coordinates(reset.exactCenterX(), reset.exactCenterY()), 0);
            send(move);
            send(MotionEvent.ACTION_UP, reset.exactCenterX(), reset.exactCenterY());
            assertNotNull("returning inside must not reset", editor.draft().positionValue(TouchLayout.MENU));
        });
    }

    @Test public void resetDuringADragRestoresEveryDefault() {
        onMain(() -> {
            drag(TouchLayout.MENU, 500, 800);
            Rect escape = editor.controlBounds(TouchLayout.ESCAPE);
            send(MotionEvent.ACTION_DOWN, escape.exactCenterX(), escape.exactCenterY());
            send(MotionEvent.ACTION_MOVE, 700, 700);
            Rect reset = editor.toolBounds(TouchLayoutEditor.RESET);
            send(second(MotionEvent.ACTION_POINTER_DOWN), 700, 700, reset.exactCenterX(), reset.exactCenterY());
            send(second(MotionEvent.ACTION_POINTER_UP), 700, 700, reset.exactCenterX(), reset.exactCenterY());
            send(MotionEvent.ACTION_UP, 700, 700);
            for (int i = 0; i < TouchLayout.COUNT; i++) {
                assertNull("control " + i + " returns to its default", editor.draft().positionValue(i));
                assertEquals(defaultBounds(i), editor.controlBounds(i));
            }
            assertEquals("Reset saves nothing by itself", 0, doneCount);
        });
    }

    @Test public void doneReportsTheDraftAndDropsUnfinishedDrags() {
        onMain(() -> {
            drag(TouchLayout.MENU, 500, 800);
            Rect escape = editor.controlBounds(TouchLayout.ESCAPE);
            send(MotionEvent.ACTION_DOWN, escape.exactCenterX(), escape.exactCenterY());
            send(MotionEvent.ACTION_MOVE, 700, 700);
            Rect done = editor.toolBounds(TouchLayoutEditor.DONE);
            send(second(MotionEvent.ACTION_POINTER_DOWN), 700, 700, done.exactCenterX(), done.exactCenterY());
            send(second(MotionEvent.ACTION_POINTER_UP), 700, 700, done.exactCenterX(), done.exactCenterY());
            assertEquals(1, doneCount);
            assertNotNull(reported.positionValue(TouchLayout.MENU));
            assertNull("an unfinished drag is not saved", reported.positionValue(TouchLayout.ESCAPE));
            assertEquals(escape, editor.controlBounds(TouchLayout.ESCAPE));
        });
    }

    @Test public void busyIgnoresInput() {
        onMain(() -> {
            editor.setBusy(true);
            tap(TouchLayoutEditor.DONE);
            drag(TouchLayout.MENU, 500, 800);
            assertEquals(0, doneCount);
            assertNull(editor.draft().positionValue(TouchLayout.MENU));
        });
    }

    @Test public void dragsStayInsideTheSafeArea() {
        onMain(() -> {
            editor.setSafeArea(100, 50, 2300, 1000);
            drag(TouchLayout.MENU, -500, -500);
            Rect menu = editor.controlBounds(TouchLayout.MENU);
            assertEquals("clamped to the left of the safe area", 100, menu.left);
            assertEquals("clamped to the top of the safe area", 50, menu.top);
            drag(TouchLayout.SPACE, 5000, 5000);
            Rect space = editor.controlBounds(TouchLayout.SPACE);
            assertEquals("clamped to the right of the safe area", 2300, space.right);
            assertEquals("clamped to the bottom of the safe area", 1000, space.bottom);
        });
    }

    private int touchSlop() {
        return ViewConfiguration.get(InstrumentationRegistry.getInstrumentation().getTargetContext())
            .getScaledTouchSlop();
    }

    @Test public void smallMovementsLeaveAButtonInPlace() {
        onMain(() -> {
            Rect menu = editor.controlBounds(TouchLayout.MENU);
            float x = menu.exactCenterX(), y = menu.exactCenterY(), wobble = touchSlop() / 2f;
            send(MotionEvent.ACTION_DOWN, x, y);
            send(MotionEvent.ACTION_MOVE, x - wobble, y + wobble / 2);
            assertEquals("a wobble inside the slop does not move the button", menu,
                editor.controlBounds(TouchLayout.MENU));
            send(MotionEvent.ACTION_UP, x - wobble, y + wobble / 2);
            assertEquals(menu, editor.controlBounds(TouchLayout.MENU));
            assertNull("a touched default button is not pinned", editor.draft().positionValue(TouchLayout.MENU));
        });
    }

    @Test public void dragsPastTheSlopFollowTheFingerExactly() {
        onMain(() -> {
            Rect menu = editor.controlBounds(TouchLayout.MENU);
            float x = menu.exactCenterX(), y = menu.exactCenterY(), step = touchSlop() * 3;
            send(MotionEvent.ACTION_DOWN, x, y);
            send(MotionEvent.ACTION_MOVE, x - step, y + step);
            assertCenter(editor.controlBounds(TouchLayout.MENU), x - step, y + step);
            send(MotionEvent.ACTION_UP, x - step, y + step);
            assertNotNull(editor.draft().positionValue(TouchLayout.MENU));
        });
    }

    @Test public void focusLossUndoesAnUnfinishedDrag() {
        onMain(() -> {
            Rect menu = editor.controlBounds(TouchLayout.MENU);
            send(MotionEvent.ACTION_DOWN, menu.exactCenterX(), menu.exactCenterY());
            send(MotionEvent.ACTION_MOVE, 500, 800);
            editor.onWindowFocusChanged(false);
            assertEquals(menu, editor.controlBounds(TouchLayout.MENU));
            send(MotionEvent.ACTION_UP, 500, 800);
            assertNull("a lift after losing focus saves nothing", editor.draft().positionValue(TouchLayout.MENU));
        });
    }

    @Test public void safeAreaChangesUndoAnUnfinishedDrag() {
        onMain(() -> {
            Rect menu = editor.controlBounds(TouchLayout.MENU);
            send(MotionEvent.ACTION_DOWN, menu.exactCenterX(), menu.exactCenterY());
            send(MotionEvent.ACTION_MOVE, 500, 800);
            editor.setSafeArea(100, 50, 2300, 1000);
            send(MotionEvent.ACTION_UP, 500, 800);
            assertNull(editor.draft().positionValue(TouchLayout.MENU));
        });
    }

    @Test public void appearanceReloadsUndoAnUnfinishedDrag() {
        onMain(() -> {
            Rect menu = editor.controlBounds(TouchLayout.MENU);
            send(MotionEvent.ACTION_DOWN, menu.exactCenterX(), menu.exactCenterY());
            send(MotionEvent.ACTION_MOVE, 500, 800);
            editor.setAppearance(TouchLayout.parse(new String[] {"1.5", "0.5", null, null, null}));
            send(MotionEvent.ACTION_UP, 500, 800);
            assertNull(editor.draft().positionValue(TouchLayout.MENU));
            assertEquals("the reloaded size applies", 1.5f, editor.draft().scale, 0);
        });
    }

    @Test public void aButtonOverDoneTakesThePress() {
        onMain(() -> {
            Rect done = editor.toolBounds(TouchLayoutEditor.DONE);
            drag(TouchLayout.MENU, done.exactCenterX(), done.exactCenterY());
            send(MotionEvent.ACTION_DOWN, done.exactCenterX(), done.exactCenterY());
            send(MotionEvent.ACTION_MOVE, 500, 800);
            send(MotionEvent.ACTION_UP, 500, 800);
            assertEquals("the press moved the button instead of finishing", 0, doneCount);
            assertCenter(editor.controlBounds(TouchLayout.MENU), 500, 800);
        });
    }

    @Test public void largeDefaultButtonsStartClearOfTheToolbar() {
        onMain(() -> {
            TouchLayoutEditor large = new TouchLayoutEditor(new ContextThemeWrapper(
                InstrumentationRegistry.getInstrumentation().getTargetContext(), android.R.style.Theme),
                TouchLayout.parse(new String[] {"2", null, null, null, null}), draft -> { });
            for (int[] size : new int[][] {{2400, 1080}, {1920, 1080}, {1440, 1080}, {1280, 720}, {960, 540}}) {
                large.layout(0, 0, size[0], size[1]);
                large.setSafeArea(0, 0, size[0], size[1]);
                for (int control = 0; control < TouchLayout.COUNT; control++) {
                    for (int tool : new int[] {TouchLayoutEditor.RESET, TouchLayoutEditor.DONE}) {
                        assertFalse("control " + control + " covers tool " + tool + " at " + size[0] + "x" + size[1],
                            Rect.intersects(large.controlBounds(control), large.toolBounds(tool)));
                    }
                }
            }
        });
    }

    @Test public void emptyAreasAreConsumed() {
        onMain(() -> {
            send(MotionEvent.ACTION_DOWN, 10, HEIGHT - 10);
            send(MotionEvent.ACTION_UP, 10, HEIGHT - 10);
            assertEquals(0, doneCount);
        });
    }
}
