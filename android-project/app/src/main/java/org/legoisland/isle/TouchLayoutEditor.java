package org.legoisland.isle;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.util.TypedValue;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.widget.ImageView;
import android.widget.TextView;

/**
 * Moves the touch buttons by dragging them over the paused game.
 *
 * One finger drags one button, which stays inside the safe area. A button moves only once its
 * finger travels past the platform touch slop, so resting a finger on it leaves it in place.
 * Lifting the finger keeps the button's new place in the draft; nothing is saved until Done. Done
 * and Reset act when a press is released inside them, and dragging out of one cancels that press.
 * Every touch is consumed, so none reaches the game or the real buttons, which stay hidden while
 * editing.
 */
// Created by TouchLayoutController, never inflated from XML. Dragging has no click equivalent, so there is
// no performClick to call; Back, which the activity handles, remains the accessible way out.
@SuppressLint({"ViewConstructor", "ClickableViewAccessibility"})
final class TouchLayoutEditor extends View {
    interface Listener { void onDone(TouchLayout draft); }

    static final int RESET = 0, DONE = 1;
    private static final String[] TOOL_LABELS = {"Reset", "Done"};
    private static final int ACCENT = 0xFF4FC3F7;

    private final Listener listener;
    private final float density;
    // The game's own button views, never attached: drawn here, sized and scaled as the real ones.
    private final View[] previews = new View[TouchLayout.COUNT];
    private final Matrix iconMatrix = new Matrix();
    private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG), outline = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Rect[] boxes = new Rect[TouchLayout.COUNT], dragStart = new Rect[TouchLayout.COUNT];
    private final Rect[] tools = {new Rect(), new Rect()};
    private final int[] dragPointer = new int[TouchLayout.COUNT];
    private final float[] grabX = new float[TouchLayout.COUNT], grabY = new float[TouchLayout.COUNT];
    private final float[] downX = new float[TouchLayout.COUNT], downY = new float[TouchLayout.COUNT];
    private final boolean[] moving = new boolean[TouchLayout.COUNT];
    private final int touchSlop;
    private final int[] bounds = new int[4];
    private final RectF shape = new RectF();
    private TouchLayout draft;
    private int safeLeft, safeTop, safeRight, safeBottom;
    private int toolPointer = -1, pressedTool = -1;
    private boolean toolCancelled, busy;

    TouchLayoutEditor(Context context, TouchLayout layout, Listener listener) {
        super(context);
        this.listener = listener;
        draft = layout;
        density = getResources().getDisplayMetrics().density;
        touchSlop = ViewConfiguration.get(context).getScaledTouchSlop();
        previews[TouchLayout.MENU] = TouchControlsLayer.createMenuButton(context);
        for (int i = TouchLayout.ESCAPE; i <= TouchLayout.SPACE; i++) {
            previews[i] = new TouchActionButton(context, TouchControlsLayer.LABELS[i], generation -> { });
        }
        for (int i = 0; i < TouchLayout.COUNT; i++) {
            boxes[i] = new Rect();
            dragStart[i] = new Rect();
            dragPointer[i] = -1;
        }
        outline.setStyle(Paint.Style.STROKE);
        text.setColor(Color.WHITE);
        text.setTextAlign(Paint.Align.CENTER);
        setContentDescription("Touch layout editor. Drag the buttons to move them, then choose Done.");
    }

    TouchLayout draft() { return draft; }

    Rect controlBounds(int control) { return new Rect(boxes[control]); }

    Rect toolBounds(int tool) { return new Rect(tools[tool]); }

    boolean isBusy() { return busy; }

    /** Ignores input while the layout is being saved. */
    void setBusy(boolean next) {
        if (next) cancelGestures();
        busy = next;
        invalidate();
    }

    /** Takes a reloaded size and opacity while keeping the positions being edited. */
    void setAppearance(TouchLayout layout) {
        cancelGestures();
        draft = draft.withAppearanceOf(layout);
        placeTools();
        placeControls();
    }

    void setSafeArea(int left, int top, int right, int bottom) {
        if (left == safeLeft && top == safeTop && right == safeRight && bottom == safeBottom) return;
        cancelGestures();
        safeLeft = left;
        safeTop = top;
        safeRight = right;
        safeBottom = bottom;
        placeTools();
        placeControls();
    }

    /**
     * Centers the toolbar in the safe area, but below the default buttons along its top edge and
     * the hint line above the toolbar: in a short window, large buttons would reach the middle.
     * If even that does not fit, buttons still take touches first and can be dragged off it.
     */
    private void placeTools() {
        int width = dp(96), height = dp(40), gap = dp(16);
        int clear = safeTop + dp(8) + TouchLayout.height(density, draft.scale) + dp(40);
        int toolTop = Math.max((safeTop + safeBottom) / 2 - height / 2, clear);
        toolTop = Math.max(safeTop, Math.min(toolTop, safeBottom - height));
        int center = (safeLeft + safeRight) / 2;
        tools[RESET].set(center - gap / 2 - width, toolTop, center - gap / 2, toolTop + height);
        tools[DONE].set(center + gap / 2, toolTop, center + gap / 2 + width, toolTop + height);
    }

    private void placeControls() {
        for (int i = 0; i < TouchLayout.COUNT; i++) {
            draft.bounds(i, safeLeft, safeTop, safeRight, safeBottom, density, bounds);
            boxes[i].set(bounds[0], bounds[1], bounds[2], bounds[3]);
            sizePreview(i);
        }
        invalidate();
    }

    /** Lays a preview out at its box's size, scaled as TouchControlsLayer scales the game's button. */
    private void sizePreview(int control) {
        View preview = previews[control];
        int width = boxes[control].width(), height = boxes[control].height();
        if (preview instanceof TextView) TouchControlsLayer.scaleLabel((TextView) preview, draft.scale);
        preview.measure(MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY),
            MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY));
        preview.layout(0, 0, width, height);
        if (preview instanceof ImageView) {
            TouchControlsLayer.scaleIcon((ImageView) preview, draft.scale, width, height, iconMatrix);
        }
    }

    /** Returns every button being dragged to where its drag began, and drops any toolbar press. */
    void cancelGestures() {
        for (int i = 0; i < TouchLayout.COUNT; i++) {
            if (dragPointer[i] >= 0) {
                boxes[i].set(dragStart[i]);
                dragPointer[i] = -1;
            }
        }
        toolPointer = -1;
        pressedTool = -1;
        toolCancelled = false;
        invalidate();
    }

    @Override public boolean onTouchEvent(MotionEvent event) {
        if (busy) return true;
        switch (event.getActionMasked()) {
        case MotionEvent.ACTION_DOWN:
            // A new gesture: nothing from an earlier one can still be held.
            cancelGestures();
            press(event, event.getActionIndex());
            break;
        case MotionEvent.ACTION_POINTER_DOWN:
            press(event, event.getActionIndex());
            break;
        case MotionEvent.ACTION_MOVE:
            for (int i = 0; i < TouchLayout.COUNT; i++) {
                int index = dragPointer[i] < 0 ? -1 : event.findPointerIndex(dragPointer[i]);
                if (index >= 0 && startsMoving(i, event.getX(index), event.getY(index))) {
                    moveTo(i, event.getX(index) - grabX[i], event.getY(index) - grabY[i]);
                }
            }
            if (toolPointer >= 0 && !toolCancelled
                    && !stayedInside(event, event.findPointerIndex(toolPointer), tools[pressedTool])) {
                toolCancelled = true;
                invalidate();
            }
            break;
        case MotionEvent.ACTION_POINTER_UP:
        case MotionEvent.ACTION_UP:
            release(event, event.getActionIndex());
            break;
        case MotionEvent.ACTION_CANCEL:
            cancelGestures();
            break;
        }
        return true;
    }

    private void press(MotionEvent event, int index) {
        int id = event.getPointerId(index);
        float x = event.getX(index), y = event.getY(index);
        // Buttons before the toolbar: a button over Done can always be dragged away, but one hidden
        // under the toolbar could otherwise never be picked up again.
        for (int i = TouchLayout.COUNT - 1; i >= 0; i--) {
            if (dragPointer[i] < 0 && inside(boxes[i], x, y)) {
                dragPointer[i] = id;
                grabX[i] = x - boxes[i].left;
                grabY[i] = y - boxes[i].top;
                downX[i] = x;
                downY[i] = y;
                moving[i] = false;
                dragStart[i].set(boxes[i]);
                invalidate();
                return;
            }
        }
        if (toolPointer >= 0) return;
        for (int tool = 0; tool < tools.length; tool++) {
            if (inside(tools[tool], x, y)) {
                toolPointer = id;
                pressedTool = tool;
                toolCancelled = false;
                invalidate();
                return;
            }
        }
    }

    private void release(MotionEvent event, int index) {
        int id = event.getPointerId(index);
        // A pointer the system rejected, such as a palm, must not move a button or press one.
        boolean rejected = Build.VERSION.SDK_INT >= 33 && (event.getFlags() & MotionEvent.FLAG_CANCELED) != 0;
        for (int i = 0; i < TouchLayout.COUNT; i++) {
            if (dragPointer[i] != id) continue;
            dragPointer[i] = -1;
            // A button that was only touched keeps its place, and a default one is not pinned.
            if (rejected || !startsMoving(i, event.getX(index), event.getY(index))) {
                boxes[i].set(dragStart[i]);
                continue;
            }
            moveTo(i, event.getX(index) - grabX[i], event.getY(index) - grabY[i]);
            if (!boxes[i].equals(dragStart[i])) commit(i);
        }
        if (id == toolPointer) {
            int tool = pressedTool;
            boolean activate = !rejected && !toolCancelled && stayedInside(event, index, tools[tool]);
            toolPointer = -1;
            pressedTool = -1;
            toolCancelled = false;
            if (activate) activate(tool);
        }
        invalidate();
    }

    private void activate(int tool) {
        cancelGestures();
        if (tool == RESET) {
            draft = draft.withDefaultPositions();
            placeControls();
        } else {
            listener.onDone(draft);
        }
    }

    /** Whether a held button follows its finger yet: once past the touch slop, for the rest of the drag. */
    private boolean startsMoving(int control, float x, float y) {
        if (!moving[control]) {
            float dx = x - downX[control], dy = y - downY[control];
            moving[control] = dx * dx + dy * dy >= (float) touchSlop * touchSlop;
        }
        return moving[control];
    }

    private void moveTo(int control, float left, float top) {
        Rect box = boxes[control];
        int x = Math.max(safeLeft, Math.min(Math.round(left), safeRight - box.width()));
        int y = Math.max(safeTop, Math.min(Math.round(top), safeBottom - box.height()));
        box.offsetTo(x, y);
        invalidate();
    }

    private void commit(int control) {
        Rect box = boxes[control];
        draft = draft.withPosition(control, TouchLayout.fraction(box.exactCenterX(), safeLeft, safeRight),
            TouchLayout.fraction(box.exactCenterY(), safeTop, safeBottom));
        // Show the button exactly where its stored position will place it.
        draft.bounds(control, safeLeft, safeTop, safeRight, safeBottom, density, bounds);
        box.set(bounds[0], bounds[1], bounds[2], bounds[3]);
    }

    private static boolean inside(Rect box, float x, float y) {
        return x >= box.left && x < box.right && y >= box.top && y < box.bottom;
    }

    private static boolean stayedInside(MotionEvent event, int index, Rect box) {
        if (index < 0) return false;
        // Android batches moves; returning inside must not hide an earlier excursion.
        for (int sample = 0; sample < event.getHistorySize(); sample++) {
            if (!inside(box, event.getHistoricalX(index, sample), event.getHistoricalY(index, sample))) return false;
        }
        return inside(box, event.getX(index), event.getY(index));
    }

    @Override public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (!hasFocus) cancelGestures();
    }

    @Override protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);
        cancelGestures();
    }

    @Override protected void onDetachedFromWindow() {
        cancelGestures();
        super.onDetachedFromWindow();
    }

    private int dp(float value) { return (int) (value * density + 0.5f); }

    private float sp(float value) {
        return TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, value, getResources().getDisplayMetrics());
    }

    private void drawLabel(Canvas canvas, String label, Rect box, float size, int alpha) {
        text.setTextSize(size);
        text.setAlpha(alpha);
        canvas.drawText(label, box.exactCenterX(), box.exactCenterY() - (text.descent() + text.ascent()) / 2, text);
    }

    /**
     * Draws a button as the game shows it. A view with a background fades through one layer, so its
     * icon or label does not let the background show through; the preview fades the same way.
     */
    void drawControl(Canvas canvas, int control) {
        Rect box = boxes[control];
        int saved = draft.opacity < 1
            ? canvas.saveLayerAlpha(box.left, box.top, box.right, box.bottom, (int) (255 * draft.opacity))
            : canvas.save();
        canvas.translate(box.left, box.top);
        previews[control].draw(canvas);
        canvas.restoreToCount(saved);
    }

    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        canvas.drawColor(0x99000000);
        outline.setColor(Color.WHITE);
        outline.setAlpha(120);
        outline.setStrokeWidth(density);
        canvas.drawRect(safeLeft + density / 2, safeTop + density / 2, safeRight - density / 2,
            safeBottom - density / 2, outline);
        text.setTextSize(sp(14));
        text.setAlpha(255);
        canvas.drawText(busy ? "Saving..." : "Drag the buttons to move them. Back cancels.",
            (safeLeft + safeRight) / 2f, tools[DONE].top - dp(16), text);
        for (int tool = 0; tool < tools.length; tool++) {
            shape.set(tools[tool]);
            fill.setColor(pressedTool == tool && !toolCancelled ? 0xFF505050 : 0xE0202020);
            canvas.drawRoundRect(shape, dp(8), dp(8), fill);
            outline.setColor(Color.WHITE);
            outline.setAlpha(busy ? 90 : 200);
            canvas.drawRoundRect(shape, dp(8), dp(8), outline);
            drawLabel(canvas, TOOL_LABELS[tool], tools[tool], sp(16), busy ? 90 : 255);
        }
        for (int i = 0; i < TouchLayout.COUNT; i++) {
            drawControl(canvas, i);
            Rect box = boxes[i];
            boolean dragging = dragPointer[i] >= 0;
            float stroke = (dragging ? 3 : 2) * density;
            outline.setColor(dragging ? ACCENT : Color.WHITE);
            outline.setStrokeWidth(stroke);
            shape.set(box);
            shape.inset(stroke / 2, stroke / 2);
            canvas.drawOval(shape, outline);
        }
    }
}
