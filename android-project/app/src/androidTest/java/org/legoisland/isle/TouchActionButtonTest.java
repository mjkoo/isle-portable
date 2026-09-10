package org.legoisland.isle;

import android.annotation.TargetApi;
import android.os.SystemClock;
import android.view.ContextThemeWrapper;
import android.view.InputDevice;
import android.view.MotionEvent;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SdkSuppress;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import static org.junit.Assert.assertTrue;

/** Exercises the production button with real Android events, without starting the game. */
@RunWith(AndroidJUnit4.class)
public final class TouchActionButtonTest {
    private TouchActionButton button;
    private int activations;
    private long receivedGeneration, downTime;

    @Before public void setUp() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            activations = 0;
            receivedGeneration = 0;
            downTime = SystemClock.uptimeMillis();
            button = new TouchActionButton(new ContextThemeWrapper(
                InstrumentationRegistry.getInstrumentation().getTargetContext(), android.R.style.Theme),
                "Space", generation -> { activations++; receivedGeneration = generation; });
            button.layout(0, 0, 64, 48);
            button.updateGeneration(7);
        });
    }

    private MotionEvent.PointerCoords[] coordinates(float... xs) {
        MotionEvent.PointerCoords[] points = new MotionEvent.PointerCoords[xs.length];
        for (int i = 0; i < xs.length; i++) {
            points[i] = new MotionEvent.PointerCoords();
            points[i].x = xs[i];
            points[i].y = 24;
            points[i].pressure = 1;
        }
        return points;
    }

    private MotionEvent event(int action, int flags, float... xs) {
        MotionEvent.PointerProperties[] properties = new MotionEvent.PointerProperties[xs.length];
        for (int i = 0; i < xs.length; i++) {
            properties[i] = new MotionEvent.PointerProperties();
            properties[i].id = i + 7;
            properties[i].toolType = MotionEvent.TOOL_TYPE_FINGER;
        }
        return MotionEvent.obtain(downTime, SystemClock.uptimeMillis(), action, xs.length,
            properties, coordinates(xs), 0, 0, 1, 1, 0, 0, InputDevice.SOURCE_TOUCHSCREEN, flags);
    }

    private void send(MotionEvent event) {
        try {
            assertTrue("button must consume its gesture", button.dispatchTouchEvent(event));
        } finally {
            event.recycle();
        }
    }

    private void send(int action, float... xs) { send(event(action, 0, xs)); }

    @Test public void ordinaryTap() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            send(MotionEvent.ACTION_DOWN, 32);
            assertTrue("down must only show pressed feedback", activations == 0 && button.isPressed());
            send(MotionEvent.ACTION_UP, 32);
            assertTrue("tap must dispatch once with its generation", activations == 1 && receivedGeneration == 7);
        });
    }

    @Test public void batchedExcursionCancels() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            send(MotionEvent.ACTION_DOWN, 32);
            MotionEvent move = event(MotionEvent.ACTION_MOVE, 0, -1);
            move.addBatch(move.getEventTime() + 1, coordinates(32), 0);
            assertTrue("test event must contain an outside historical sample", move.getHistorySize() == 1);
            send(move);
            assertTrue("historical excursion must clear feedback", !button.isPressed());
            send(MotionEvent.ACTION_UP, 32);
            assertTrue("returning inside must not reactivate the gesture", activations == 0);
        });
    }

    @Test public void insideBatchPreservesTap() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            send(MotionEvent.ACTION_DOWN, 32);
            MotionEvent move = event(MotionEvent.ACTION_MOVE, 0, 20);
            move.addBatch(move.getEventTime() + 1, coordinates(40), 0);
            send(move);
            send(MotionEvent.ACTION_UP, 40);
            assertTrue("inside history must preserve the tap", activations == 1);
        });
    }

    @Test public void extraPointerHistoryDoesNotCancelOwner() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            send(MotionEvent.ACTION_DOWN, 32);
            send(MotionEvent.ACTION_POINTER_DOWN | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT), 32, 40);
            MotionEvent move = event(MotionEvent.ACTION_MOVE, 0, 32, -1);
            move.addBatch(move.getEventTime() + 1, coordinates(32, 40), 0);
            send(move);
            send(MotionEvent.ACTION_POINTER_UP, 32, 40);
            assertTrue("only owner history should affect activation", activations == 1);
        });
    }

    @SdkSuppress(minSdkVersion = 33)
    @TargetApi(33)
    @Test public void cancelledOwnerRelease() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            send(MotionEvent.ACTION_DOWN, 32);
            send(MotionEvent.ACTION_POINTER_DOWN | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT), 32, 40);
            send(event(MotionEvent.ACTION_POINTER_UP, MotionEvent.FLAG_CANCELED, 32, 40));
            assertTrue("rejected owner touch must not activate", activations == 0 && !button.isPressed());
        });
    }

    @SdkSuppress(minSdkVersion = 33)
    @TargetApi(33)
    @Test public void cancelledExtraPointerPreservesOwner() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            send(MotionEvent.ACTION_DOWN, 32);
            send(MotionEvent.ACTION_POINTER_DOWN | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT), 32, 40);
            send(event(MotionEvent.ACTION_POINTER_UP | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                MotionEvent.FLAG_CANCELED, 32, 40));
            send(MotionEvent.ACTION_UP, 32);
            assertTrue("rejecting an extra pointer must not cancel the owner", activations == 1);
        });
    }

    @Test public void wholeGestureCancellation() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            send(MotionEvent.ACTION_DOWN, 32);
            send(MotionEvent.ACTION_CANCEL, 32);
            send(MotionEvent.ACTION_UP, 32);
            assertTrue("cancelled gestures must not activate", activations == 0);
        });
    }

    @Test public void generationChangeCancelsHeldTap() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            send(MotionEvent.ACTION_DOWN, 32);
            button.updateGeneration(0);
            button.updateGeneration(8);
            send(MotionEvent.ACTION_UP, 32);
            assertTrue("old release must not activate after resume", activations == 0);
        });
    }
}
