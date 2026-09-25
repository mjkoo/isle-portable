package org.legoisland.isle;

import android.content.res.Configuration;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ScrollView;

import androidx.test.core.app.ActivityScenario;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

/** Exercises the same dialog and controller dispatch used over the game, without loading SDL. */
@RunWith(AndroidJUnit4.class)
public final class PauseMenuDialogTest {
    private static final int A = KeyEvent.KEYCODE_BUTTON_A;
    private static final int B = KeyEvent.KEYCODE_BUTTON_B;
    private static final int START = KeyEvent.KEYCODE_BUTTON_START;

    private static void send(PauseMenuDialog dialog, int key, int action, int device, int repeat, int flags) {
        long now = SystemClock.uptimeMillis();
        assertTrue(dialog.dispatchKeyEvent(new KeyEvent(now, now, action, key, repeat, 0,
            device, 0, flags, InputDevice.SOURCE_GAMEPAD)));
    }

    private static void down(PauseMenuDialog dialog, int key) {
        send(dialog, key, KeyEvent.ACTION_DOWN, 1, 0, 0);
    }

    private static void up(PauseMenuDialog dialog, int key) {
        send(dialog, key, KeyEvent.ACTION_UP, 1, 0, 0);
    }

    private static List<Button> buttons(View view) {
        List<Button> result = new ArrayList<>();
        if (view instanceof Button) result.add((Button) view);
        if (view instanceof ViewGroup) {
            ViewGroup group = (ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); i++) result.addAll(buttons(group.getChildAt(i)));
        }
        return result;
    }

    @Test public void controllerTakesOverFromTouchWithoutActivating() {
        for (String error : new String[] {null, "Missing data"}) {
            try (ActivityScenario<TouchTestActivity> scenario = ActivityScenario.launch(TouchTestActivity.class)) {
                PauseMenuDialog[] dialog = new PauseMenuDialog[1];
                int[] actions = new int[3];
                scenario.onActivity(activity -> {
                    dialog[0] = new PauseMenuDialog(activity,
                        new QuitPromptText(QuitPromptText.SAVE_ATTEMPTED, "PEPPER", error),
                        () -> actions[0]++, () -> actions[1]++, () -> actions[2]++);
                    dialog[0].show();
                });
                InstrumentationRegistry.getInstrumentation().waitForIdleSync();
                long touchTime = SystemClock.uptimeMillis();
                for (int action : new int[] {MotionEvent.ACTION_DOWN, MotionEvent.ACTION_UP}) {
                    MotionEvent touch = MotionEvent.obtain(touchTime, touchTime, action, 1, 1, 0);
                    InstrumentationRegistry.getInstrumentation().sendPointerSync(touch);
                    touch.recycle();
                }
                InstrumentationRegistry.getInstrumentation().waitForIdleSync();
                scenario.onActivity(activity -> {
                    View decor = dialog[0].getWindow().getDecorView();
                    assertTrue(decor.isInTouchMode());
                    assertNull(dialog[0].getCurrentFocus());
                    down(dialog[0], A);
                    up(dialog[0], A);
                    List<Button> choices = buttons(decor);
                    assertEquals(error == null ? "Resume" : "Settings", choices.get(0).getText().toString());
                    assertEquals(error == null ? "Quit" : "Close", choices.get(choices.size() - 1).getText().toString());
                    assertSame(choices.get(0), dialog[0].getCurrentFocus());
                    assertArrayEquals(new int[3], actions);
                    down(dialog[0], A);
                    up(dialog[0], A);
                    assertEquals(1, actions[error == null ? 1 : 2]);
                    assertEquals(0, actions[0]);
                    dialog[0].dismiss();
                });
            }
        }
    }

    @Test public void releasesAreMatchedByKeyAndDevice() {
        try (ActivityScenario<TouchTestActivity> scenario = ActivityScenario.launch(TouchTestActivity.class)) {
            scenario.onActivity(activity -> {
                PauseMenuDialog dialog = new PauseMenuDialog(activity,
                    new QuitPromptText(0, null, null), () -> {}, () -> {}, () -> {});
                dialog.show();
                up(dialog, START);
                assertTrue(dialog.isShowing());
                down(dialog, A);
                down(dialog, B);
                up(dialog, A);
                up(dialog, B);
                assertFalse("A release must not discard B's press", dialog.isShowing());
                dialog.show();
                down(dialog, START);
                send(dialog, START, KeyEvent.ACTION_UP, 2, 0, 0);
                assertTrue(dialog.isShowing());
                up(dialog, START);
                assertFalse(dialog.isShowing());
                dialog.show();
                send(dialog, B, KeyEvent.ACTION_DOWN, 1, 1, 0);
                up(dialog, B);
                assertTrue("a repeat cannot start a press", dialog.isShowing());
                down(dialog, B);
                send(dialog, B, KeyEvent.ACTION_DOWN, 1, 1, 0);
                assertTrue(dialog.isShowing());
                send(dialog, B, KeyEvent.ACTION_UP, 1, 0, KeyEvent.FLAG_CANCELED);
                up(dialog, B);
                assertTrue("canceled releases cannot act", dialog.isShowing());
                down(dialog, START);
                dialog.onWindowFocusChanged(false);
                dialog.onWindowFocusChanged(true);
                up(dialog, START);
                assertTrue(dialog.isShowing());
                down(dialog, START);
                dialog.dismiss();
                dialog.show();
                up(dialog, START);
                assertTrue("dismiss must discard held keys", dialog.isShowing());
                down(dialog, START);
                up(dialog, START);
                assertFalse(dialog.isShowing());
            });
        }
    }

    @Test public void buttonsExposeRolesAndSupportNavigationAndTouch() {
        try (ActivityScenario<TouchTestActivity> scenario = ActivityScenario.launch(TouchTestActivity.class)) {
            int[] actions = new int[3];
            PauseMenuDialog[] dialog = new PauseMenuDialog[1];
            scenario.onActivity(activity -> {
                dialog[0] = new PauseMenuDialog(activity, new QuitPromptText(0, null, null),
                    () -> actions[0]++, () -> actions[1]++, () -> actions[2]++);
                dialog[0].show();
            });
            InstrumentationRegistry.getInstrumentation().waitForIdleSync();
            scenario.onActivity(activity -> {
                List<Button> choices = buttons(dialog[0].getWindow().getDecorView());
                assertEquals(3, choices.size());
                String[] labels = {"Resume", "Settings", "Quit"};
                for (int i = 0; i < choices.size(); i++) {
                    assertEquals(labels[i], choices.get(i).getText().toString());
                    assertEquals(Button.class.getName(), choices.get(i).createAccessibilityNodeInfo().getClassName());
                }
                choices.get(0).requestFocusFromTouch();
            });
            InstrumentationRegistry.getInstrumentation().sendKeyDownUpSync(KeyEvent.KEYCODE_DPAD_DOWN);
            scenario.onActivity(activity -> assertSame(buttons(dialog[0].getWindow().getDecorView()).get(1),
                dialog[0].getCurrentFocus()));
            InstrumentationRegistry.getInstrumentation().sendKeyDownUpSync(KeyEvent.KEYCODE_ENTER);
            InstrumentationRegistry.getInstrumentation().waitForIdleSync();
            scenario.onActivity(activity -> {
                assertEquals(1, actions[2]);
                Button quit = buttons(dialog[0].getWindow().getDecorView()).get(2);
                long now = SystemClock.uptimeMillis();
                for (int action : new int[] {MotionEvent.ACTION_DOWN, MotionEvent.ACTION_UP}) {
                    MotionEvent event = MotionEvent.obtain(now, now, action, quit.getWidth() / 2f,
                        quit.getHeight() / 2f, 0);
                    quit.dispatchTouchEvent(event);
                    event.recycle();
                }
            });
            InstrumentationRegistry.getInstrumentation().waitForIdleSync();
            scenario.onActivity(activity -> {
                assertEquals(1, actions[0]);
                dialog[0].dismiss();
            });
        }
    }

    @Test public void largeTextInShortWindowKeepsAllChoicesScrollable() {
        try (ActivityScenario<TouchTestActivity> scenario = ActivityScenario.launch(TouchTestActivity.class)) {
            scenario.onActivity(activity -> {
                Configuration config = new Configuration(activity.getResources().getConfiguration());
                config.fontScale = 2;
                for (String error : new String[] {null, "Missing game data"}) {
                    PauseMenuView menu = new PauseMenuView(activity.createConfigurationContext(config),
                        new QuitPromptText(0, null, error), () -> {}, () -> {}, () -> {});
                    menu.measure(View.MeasureSpec.makeMeasureSpec(800, View.MeasureSpec.EXACTLY),
                        View.MeasureSpec.makeMeasureSpec(240, View.MeasureSpec.EXACTLY));
                    menu.layout(0, 0, 800, 240);
                    ScrollView scroll = (ScrollView) menu.getChildAt(0);
                    assertTrue(scroll.getChildAt(0).getHeight() > scroll.getHeight());
                    List<Button> choices = buttons(menu);
                    assertEquals(error == null ? 3 : 2, choices.size());
                    for (Button button : choices) {
                        android.graphics.Rect rect = new android.graphics.Rect(0, 0, button.getWidth(), button.getHeight());
                        button.requestRectangleOnScreen(rect, true);
                        assertTrue(button.getGlobalVisibleRect(new android.graphics.Rect()));
                        assertEquals(Button.class.getName(), button.getAccessibilityClassName());
                    }
                }
            });
        }
    }
}
