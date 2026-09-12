package org.legoisland.isle;

import android.view.View;
import android.view.ViewTreeObserver;
import android.widget.FrameLayout;
import android.widget.RelativeLayout;

import androidx.test.core.app.ActivityScenario;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

/** Places the production buttons in a real window, where only what is drawn reaches the player. */
@RunWith(AndroidJUnit4.class)
public final class TouchControlsWindowTest {
    private static final int MATCH = FrameLayout.LayoutParams.MATCH_PARENT;

    @Test public void resizedHostIsNeverDrawnWithTheOldPlacement() throws Exception {
        try (ActivityScenario<TouchTestActivity> scenario = ActivityScenario.launch(TouchTestActivity.class)) {
            RelativeLayout[] host = new RelativeLayout[1];
            View[] menu = new View[1];
            CountDownLatch firstDraw = new CountDownLatch(1);
            ViewTreeObserver.OnDrawListener drawn = firstDraw::countDown;
            scenario.onActivity(activity -> {
                FrameLayout root = new FrameLayout(activity);
                host[0] = new RelativeLayout(activity);
                menu[0] = new View(activity);
                TouchControlsLayer layer = new TouchControlsLayer(host[0], menu[0], new View(activity),
                    new View(activity));
                // The bottom-right corner, so shrinking the host in either direction moves it.
                layer.setTouchLayout(TouchLayout.DEFAULT.withPosition(TouchLayout.MENU, 1, 1));
                root.addView(host[0], new FrameLayout.LayoutParams(MATCH, MATCH));
                activity.setContentView(root);
                root.getViewTreeObserver().addOnDrawListener(drawn);
            });
            // By the first draw the window insets have been dispatched too.
            assertTrue("the window was never drawn", firstDraw.await(5, TimeUnit.SECONDS));

            int[] before = new int[2], target = new int[2];
            List<int[]> draws = new ArrayList<>();
            CountDownLatch resized = new CountDownLatch(1);
            ViewTreeObserver.OnDrawListener recorder = () -> {
                int width = host[0].getWidth(), height = host[0].getHeight();
                if (width != target[0] || height != target[1]) return;
                draws.add(new int[] {menu[0].getRight(), menu[0].getBottom()});
                resized.countDown();
            };
            scenario.onActivity(activity -> {
                host[0].getViewTreeObserver().removeOnDrawListener(drawn);
                before[0] = menu[0].getRight();
                before[1] = menu[0].getBottom();
                target[0] = host[0].getWidth() * 2 / 3;
                target[1] = host[0].getHeight() * 2 / 3;
                host[0].getViewTreeObserver().addOnDrawListener(recorder);
                host[0].setLayoutParams(new FrameLayout.LayoutParams(target[0], target[1]));
            });
            assertTrue("the resized host was never drawn", resized.await(5, TimeUnit.SECONDS));

            int[] settled = new int[2];
            scenario.onActivity(activity -> {
                // A draw listener cannot remove itself while the draw is running.
                host[0].getViewTreeObserver().removeOnDrawListener(recorder);
                settled[0] = menu[0].getRight();
                settled[1] = menu[0].getBottom();
            });
            assertNotEquals("the resize must move the menu", before[0], settled[0]);
            assertNotEquals("the resize must move the menu", before[1], settled[1]);
            assertFalse(draws.isEmpty());
            for (int[] draw : draws) {
                assertEquals("menu right when drawn at the new size", settled[0], draw[0]);
                assertEquals("menu bottom when drawn at the new size", settled[1], draw[1]);
            }
        }
    }
}
