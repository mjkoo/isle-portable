package org.legoisland.isle;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.view.View;

/** Draws native movement feedback without receiving or translating touch events. */
@SuppressLint("ViewConstructor") // Created with the SDL surface, never inflated from XML.
final class TouchControlsView extends View {
    // Mirrored by the value snapshot in ISLE/android/touchcontrols.cpp.
    private static final int VISIBLE = 0, SCHEME = 1, LEFT = 2, TOP = 3, RIGHT = 4, BOTTOM = 5;
    private static final int STICK_ACTIVE = 6, ORIGIN_X = 7, ORIGIN_Y = 8, AXIS_X = 9, AXIS_Y = 10, FLAGS = 11;
    private static final int ARROWS = 1, STICK = 2;
    private static final int DIRECTION_LEFT = 1, DIRECTION_RIGHT = 2, DIRECTION_UP = 4, DIRECTION_DOWN = 8;
    private static final int IDLE_ALPHA = 90, ACTIVE_ALPHA = 220;

    private static native void setNativeActive(boolean active);
    private static native long readNativeState(float[] output);

    private final View surface;
    private final float density;
    private final float[] state = new float[12];
    private final int[] surfaceLocation = new int[2], overlayLocation = new int[2];
    private final RectF surfaceBounds = new RectF(), viewport = new RectF(), stickBounds = new RectF();
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path path = new Path();
    private boolean running;
    private long revision = -1;

    TouchControlsView(Context context, View surface) {
        super(context);
        this.surface = surface;
        density = getResources().getDisplayMetrics().density;
        setClickable(false);
        setFocusable(false);
        setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeCap(Paint.Cap.ROUND);
        paint.setStrokeJoin(Paint.Join.ROUND);
    }

    void setRunning(boolean enabled) {
        if (running == enabled) return;
        running = enabled;
        removeCallbacks(frame);
        // Invalidate the cached native sample as well, so resume cannot replay held input.
        setNativeActive(enabled);
        state[VISIBLE] = 0;
        revision = -1;
        invalidate();
        if (enabled) postOnAnimation(frame);
    }

    private final Runnable frame = new Runnable() {
        @Override public void run() {
            if (!running) return;
            long nextRevision = readNativeState(state);
            surface.getLocationInWindow(surfaceLocation);
            getLocationInWindow(overlayLocation);
            float left = surfaceLocation[0] - overlayLocation[0];
            float top = surfaceLocation[1] - overlayLocation[1];
            float right = left + surface.getWidth();
            float bottom = top + surface.getHeight();
            boolean moved = surfaceBounds.left != left || surfaceBounds.top != top
                || surfaceBounds.right != right || surfaceBounds.bottom != bottom;
            surfaceBounds.set(left, top, right, bottom);
            if (nextRevision < 0) state[VISIBLE] = 0;
            if (nextRevision != revision || moved) {
                revision = nextRevision;
                invalidate();
            }
            postOnAnimation(this);
        }
    };

    @Override protected void onDetachedFromWindow() {
        setRunning(false);
        super.onDetachedFromWindow();
    }

    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if (!running || state[VISIBLE] == 0 || surfaceBounds.isEmpty()) return;
        viewport.set(
            surfaceBounds.left + state[LEFT] * surfaceBounds.width(),
            surfaceBounds.top + state[TOP] * surfaceBounds.height(),
            surfaceBounds.left + state[RIGHT] * surfaceBounds.width(),
            surfaceBounds.top + state[BOTTOM] * surfaceBounds.height());
        if (viewport.isEmpty()) return;
        int saved = canvas.save();
        canvas.clipRect(surfaceBounds);
        canvas.clipRect(viewport);
        if ((int) state[SCHEME] == ARROWS) drawArrows(canvas);
        else if ((int) state[SCHEME] == STICK && state[STICK_ACTIVE] != 0) drawStick(canvas);
        canvas.restoreToCount(saved);
    }

    private void stroke(Canvas canvas, int alpha, float width) {
        paint.setColor(Color.BLACK);
        paint.setAlpha(alpha);
        paint.setStrokeWidth((width + 2) * density);
        canvas.drawPath(path, paint);
        paint.setColor(Color.WHITE);
        paint.setAlpha(alpha);
        paint.setStrokeWidth(width * density);
        canvas.drawPath(path, paint);
    }

    private void drawStick(Canvas canvas) {
        float x = viewport.left + state[ORIGIN_X] * viewport.width();
        float y = viewport.top + state[ORIGIN_Y] * viewport.height();
        float radius = 48 * density;
        stickBounds.set(x - radius, y - radius, x + radius, y + radius);
        path.rewind();
        path.addRoundRect(stickBounds, 12 * density, 12 * density, Path.Direction.CW);
        stroke(canvas, IDLE_ALPHA, 1.5f);
        float thumbX = x + state[AXIS_X] * 32 * density;
        float thumbY = y + state[AXIS_Y] * 32 * density;
        path.rewind();
        path.moveTo(x, y);
        path.lineTo(thumbX, thumbY);
        path.addCircle(x, y, 2 * density, Path.Direction.CW);
        path.addCircle(thumbX, thumbY, 8 * density, Path.Direction.CW);
        stroke(canvas, ACTIVE_ALPHA, 2);
    }

    private void drawArrows(Canvas canvas) {
        float split = viewport.top + viewport.height() * .75f;
        path.rewind();
        path.moveTo(viewport.left, split);
        path.lineTo(viewport.right, split);
        for (int i = 1; i <= 2; i++) {
            float x = viewport.left + viewport.width() * i / 3;
            path.moveTo(x, split);
            path.lineTo(x, viewport.bottom);
        }
        stroke(canvas, 45, 1);
        arrow(canvas, .5f, .375f, 0, DIRECTION_UP);
        arrow(canvas, 1f / 6, .875f, -90, DIRECTION_LEFT);
        arrow(canvas, .5f, .875f, 180, DIRECTION_DOWN);
        arrow(canvas, 5f / 6, .875f, 90, DIRECTION_RIGHT);
    }

    private void arrow(Canvas canvas, float x, float y, float angle, int direction) {
        int saved = canvas.save();
        canvas.translate(viewport.left + x * viewport.width(), viewport.top + y * viewport.height());
        canvas.rotate(angle);
        float size = 12 * density;
        path.rewind();
        path.moveTo(0, size);
        path.lineTo(0, -size);
        path.moveTo(-size * .65f, -size * .3f);
        path.lineTo(0, -size);
        path.lineTo(size * .65f, -size * .3f);
        boolean active = ((int) state[FLAGS] & direction) != 0;
        stroke(canvas, active ? ACTIVE_ALPHA : IDLE_ALPHA, active ? 3 : 2);
        canvas.restoreToCount(saved);
    }
}
