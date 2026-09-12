package org.legoisland.isle;

import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.Insets;
import android.os.SystemClock;
import android.util.TypedValue;
import android.view.ContextThemeWrapper;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowInsets;
import android.widget.ImageButton;
import android.widget.RelativeLayout;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SdkSuppress;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

/** Places the production buttons in a real host layout, without starting the game. */
@RunWith(AndroidJUnit4.class)
public final class TouchControlsLayerTest {
    private static final int WIDTH = 2400, HEIGHT = 1080;
    private Context context;
    private float density;
    private long downTime;

    @Before public void setUp() {
        context = new ContextThemeWrapper(
            InstrumentationRegistry.getInstrumentation().getTargetContext(), android.R.style.Theme);
        density = context.getResources().getDisplayMetrics().density;
        downTime = SystemClock.uptimeMillis();
    }

    private static void layOut(View view) {
        view.measure(View.MeasureSpec.makeMeasureSpec(WIDTH, View.MeasureSpec.EXACTLY),
            View.MeasureSpec.makeMeasureSpec(HEIGHT, View.MeasureSpec.EXACTLY));
        view.layout(0, 0, WIDTH, HEIGHT);
    }

    /** Lays the host out, places the buttons for its size and insets, then lays it out again. */
    private static void place(RelativeLayout host, TouchControlsLayer layer, int... insets) {
        layOut(host);
        if (insets.length == 4) layer.setSafeInsets(insets[0], insets[1], insets[2], insets[3]);
        layer.refresh();
        layOut(host);
    }

    private static void assertBounds(String name, View expected, View actual) {
        assertEquals(name + " left", expected.getLeft(), actual.getLeft());
        assertEquals(name + " top", expected.getTop(), actual.getTop());
        assertEquals(name + " right", expected.getRight(), actual.getRight());
        assertEquals(name + " bottom", expected.getBottom(), actual.getBottom());
    }

    /** The fixed RelativeLayout rules the buttons used before their layout became configurable. */
    private View[] fixedLayout(int insetTop, int insetRight) {
        RelativeLayout parent = new RelativeLayout(context);
        View menu = new View(context), escape = new View(context), space = new View(context);
        menu.setId(View.generateViewId());
        escape.setId(View.generateViewId());
        int size = (int) (48 * density + 0.5f), margin = (int) (8 * density + 0.5f);
        int actionWidth = (int) (64 * density + 0.5f);
        RelativeLayout.LayoutParams params = new RelativeLayout.LayoutParams(size, size);
        params.addRule(RelativeLayout.ALIGN_PARENT_RIGHT);
        params.setMargins(margin, margin + insetTop, margin + insetRight, margin);
        parent.addView(menu, params);
        RelativeLayout.LayoutParams escapeParams = new RelativeLayout.LayoutParams(actionWidth, size);
        escapeParams.addRule(RelativeLayout.LEFT_OF, menu.getId());
        escapeParams.addRule(RelativeLayout.ALIGN_TOP, menu.getId());
        parent.addView(escape, escapeParams);
        RelativeLayout.LayoutParams spaceParams = new RelativeLayout.LayoutParams(actionWidth, size);
        spaceParams.addRule(RelativeLayout.LEFT_OF, escape.getId());
        spaceParams.addRule(RelativeLayout.ALIGN_TOP, menu.getId());
        spaceParams.rightMargin = margin;
        parent.addView(space, spaceParams);
        layOut(parent);
        return new View[] {menu, escape, space};
    }

    private MotionEvent event(int action, float... xy) {
        MotionEvent.PointerProperties[] properties = new MotionEvent.PointerProperties[xy.length / 2];
        MotionEvent.PointerCoords[] coordinates = new MotionEvent.PointerCoords[xy.length / 2];
        for (int i = 0; i < properties.length; i++) {
            properties[i] = new MotionEvent.PointerProperties();
            properties[i].id = i + 7;
            properties[i].toolType = MotionEvent.TOOL_TYPE_FINGER;
            coordinates[i] = new MotionEvent.PointerCoords();
            coordinates[i].x = xy[2 * i];
            coordinates[i].y = xy[2 * i + 1];
            coordinates[i].pressure = 1;
        }
        return MotionEvent.obtain(downTime, SystemClock.uptimeMillis(), action, properties.length, properties,
            coordinates, 0, 0, 1, 1, 0, 0, InputDevice.SOURCE_TOUCHSCREEN, 0);
    }

    private static void dispatch(View view, MotionEvent event) {
        try {
            view.dispatchTouchEvent(event);
        } finally {
            event.recycle();
        }
    }

    @Test public void defaultsMatchFixedLayout() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            // The fixed layout honored only the top and right insets; left and bottom insets must
            // not move the default placement either.
            for (int[] insets : new int[][] {{0, 0, 0, 0}, {0, 63, 0, 0}, {0, 0, 126, 0}, {0, 40, 90, 0},
                    {80, 40, 90, 30}}) {
                View[] expected = fixedLayout(insets[1], insets[2]);
                RelativeLayout host = new RelativeLayout(context);
                View[] actual = {new View(context), new View(context), new View(context)};
                TouchControlsLayer layer = new TouchControlsLayer(host, actual[0], actual[1], actual[2]);
                place(host, layer, insets);
                for (int i = 0; i < TouchLayout.COUNT; i++) {
                    assertBounds("control " + i + " with insets " + java.util.Arrays.toString(insets),
                        expected[i], actual[i]);
                }
            }
        });
    }

    @Test public void insetChangesKeepMovedButtonsInside() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            RelativeLayout host = new RelativeLayout(context);
            View menu = new View(context);
            TouchControlsLayer layer = new TouchControlsLayer(host, menu, new View(context), new View(context));
            layer.setTouchLayout(TouchLayout.DEFAULT.withPosition(TouchLayout.MENU, 1, 1));
            place(host, layer);
            assertEquals("a corner position sits flush against the right", WIDTH, menu.getRight());
            assertEquals("a corner position sits flush against the bottom", HEIGHT, menu.getBottom());
            place(host, layer, 0, 0, 120, 50);
            assertEquals("a right inset moves the button out from under it", WIDTH - 120, menu.getRight());
            assertEquals("a bottom inset moves the button out from under it", HEIGHT - 50, menu.getBottom());
        });
    }

    @SdkSuppress(minSdkVersion = 30)
    @TargetApi(30)
    @Test public void windowInsetsReachTheButtons() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            RelativeLayout host = new RelativeLayout(context);
            View menu = new View(context);
            new TouchControlsLayer(host, menu, new View(context), new View(context));
            layOut(host);
            host.dispatchApplyWindowInsets(new WindowInsets.Builder()
                .setInsets(WindowInsets.Type.displayCutout(), Insets.of(0, 40, 150, 0)).build());
            layOut(host);
            int margin = (int) (8 * density + 0.5f);
            assertEquals("a right cutout inset moves the menu left", WIDTH - 150 - margin, menu.getRight());
            assertEquals("a top cutout inset moves the menu down", 40 + margin, menu.getTop());
        });
    }

    @Test public void buttonsLargerThanTheSafeAreaKeepTheirSize() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            RelativeLayout host = new RelativeLayout(context);
            View menu = new View(context);
            TouchControlsLayer layer = new TouchControlsLayer(host, menu, new View(context), new View(context));
            layer.setTouchLayout(TouchLayout.parse(new String[] {"2", null, null, null, null}));
            place(host, layer, WIDTH - 100, 0, 0, HEIGHT - 100);
            int size = (int) (48 * density * 2 + 0.5f);
            assertEquals("the button keeps its width", size, menu.getWidth());
            assertEquals("the button keeps its height", size, menu.getHeight());
            assertEquals("aligned to the safe area's left", WIDTH - 100, menu.getLeft());
            assertEquals("aligned to the safe area's top", 0, menu.getTop());
        });
    }

    @Test public void sizeAndOpacityApplyToEveryButton() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            RelativeLayout host = new RelativeLayout(context);
            View menu = new View(context);
            TouchActionButton escape = new TouchActionButton(context, "Esc", generation -> { });
            TouchActionButton space = new TouchActionButton(context, "Space", generation -> { });
            TouchControlsLayer layer = new TouchControlsLayer(host, menu, escape, space);
            layer.setTouchLayout(TouchLayout.parse(new String[] {"2", "0.25", null, null, null}));
            place(host, layer);
            int size = (int) (48 * density * 2 + 0.5f);
            assertEquals("menu width doubles", size, menu.getWidth());
            assertEquals("menu height doubles", size, menu.getHeight());
            assertEquals((int) (64 * density * 2 + 0.5f), escape.getWidth());
            float text = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, TouchActionButton.TEXT_SIZE_SP * 2,
                context.getResources().getDisplayMetrics());
            assertEquals(text, escape.getTextSize(), 0.5f);
            for (View view : new View[] {menu, escape, space}) assertEquals(0.25f, view.getAlpha(), 0);
        });
    }

    @Test public void menuIconKeepsItsPlaceAndGrowsWithTheButton() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            // The menu button as it was before: its centered image at its own size.
            ImageButton fixed = new ImageButton(context);
            fixed.setImageResource(R.drawable.game_menu);
            fixed.setPadding(0, 0, 0, 0);
            int size = (int) (48 * density + 0.5f);
            fixed.measure(View.MeasureSpec.makeMeasureSpec(size, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(size, View.MeasureSpec.EXACTLY));
            fixed.layout(0, 0, size, size);
            float[] expected = new float[9];
            fixed.getImageMatrix().getValues(expected);

            RelativeLayout host = new RelativeLayout(context);
            ImageButton menu = new ImageButton(context);
            menu.setImageResource(R.drawable.game_menu);
            menu.setPadding(0, 0, 0, 0);
            TouchControlsLayer layer = new TouchControlsLayer(host, menu, new View(context), new View(context));
            place(host, layer);
            float[] actual = new float[9];
            menu.getImageMatrix().getValues(actual);
            assertArrayEquals("the default icon placement is unchanged", expected, actual, 0);

            layer.setTouchLayout(TouchLayout.parse(new String[] {"2", null, null, null, null}));
            place(host, layer);
            menu.getImageMatrix().getValues(actual);
            assertEquals("the icon doubles with the button", 2, actual[android.graphics.Matrix.MSCALE_X], 0);
            float drawn = menu.getDrawable().getIntrinsicWidth() * 2;
            assertEquals("the doubled icon stays centered", Math.round((menu.getWidth() - drawn) * 0.5f),
                actual[android.graphics.Matrix.MTRANS_X], 0);
        });
    }

    @Test public void laterFingersReachTheGameWhileAButtonIsHeld() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            RelativeLayout host = new RelativeLayout(context);
            host.setMotionEventSplittingEnabled(true);
            List<Integer> surfaceDowns = new ArrayList<>();
            View surface = new View(context);
            surface.setOnTouchListener((view, event) -> {
                if (event.getActionMasked() == MotionEvent.ACTION_DOWN) surfaceDowns.add(event.getPointerId(0));
                return true;
            });
            host.addView(surface, new RelativeLayout.LayoutParams(
                RelativeLayout.LayoutParams.MATCH_PARENT, RelativeLayout.LayoutParams.MATCH_PARENT));
            int[] activations = new int[1];
            TouchActionButton escape = new TouchActionButton(context, "Esc", generation -> activations[0]++);
            escape.updateGeneration(7);
            TouchControlsLayer layer = new TouchControlsLayer(host, new View(context), escape, new View(context));
            place(host, layer);
            float x = (escape.getLeft() + escape.getRight()) / 2f, y = (escape.getTop() + escape.getBottom()) / 2f;
            dispatch(host, event(MotionEvent.ACTION_DOWN, x, y));
            assertTrue("the first finger holds the button", escape.isPressed() && surfaceDowns.isEmpty());
            dispatch(host, event(MotionEvent.ACTION_POINTER_DOWN | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                x, y, 10, HEIGHT - 10));
            assertEquals("a finger missing the buttons reaches the game", List.of(8), surfaceDowns);
            dispatch(host, event(MotionEvent.ACTION_POINTER_UP | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                x, y, 10, HEIGHT - 10));
            dispatch(host, event(MotionEvent.ACTION_UP, x, y));
            assertEquals("the held button still activates once", 1, activations[0]);
        });
    }

    @Test public void touchesMissingButtonsReachTheGame() {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            RelativeLayout host = new RelativeLayout(context);
            List<Integer> surfaceDowns = new ArrayList<>();
            View surface = new View(context);
            surface.setOnTouchListener((view, event) -> {
                if (event.getActionMasked() == MotionEvent.ACTION_DOWN) surfaceDowns.add(event.getPointerId(0));
                return true;
            });
            host.addView(surface, new RelativeLayout.LayoutParams(
                RelativeLayout.LayoutParams.MATCH_PARENT, RelativeLayout.LayoutParams.MATCH_PARENT));
            View menu = new View(context);
            menu.setClickable(true);
            TouchControlsLayer layer = new TouchControlsLayer(host, menu, new View(context), new View(context));
            place(host, layer);
            dispatch(host, event(MotionEvent.ACTION_DOWN, 10, HEIGHT - 10));
            assertEquals("an empty area reaches the game", List.of(7), surfaceDowns);
            surfaceDowns.clear();
            dispatch(host, event(MotionEvent.ACTION_DOWN, (menu.getLeft() + menu.getRight()) / 2f,
                (menu.getTop() + menu.getBottom()) / 2f));
            assertTrue("the button keeps its own touch", surfaceDowns.isEmpty());
        });
    }
}
