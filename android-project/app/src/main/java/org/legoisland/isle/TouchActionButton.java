package org.legoisland.isle;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Color;
import android.view.MotionEvent;
import android.widget.Button;

/** Owns only gestures that begin on this target; other fingers stay with the SDL surface. */
// SDLActivity uses a platform theme, so this view deliberately uses the platform Button.
@SuppressLint({"ViewConstructor", "AppCompatCustomView"})
final class TouchActionButton extends Button {
    private final int action;
    private long generation, pressedGeneration;
    private int pointer = -1;
    private boolean cancelled;

    TouchActionButton(Context context, String label, int action) {
        super(context);
        this.action = action;
        setText(label);
        setTextSize(14);
        setTextColor(Color.WHITE);
        setAllCaps(false);
        setPadding(0, 0, 0, 0);
        setMinWidth(0);
        setMinHeight(0);
        setMinimumWidth(0);
        setMinimumHeight(0);
        setBackgroundResource(R.drawable.game_menu_background);
        setContentDescription(label + " game key");
        setFocusable(false);
        setVisibility(INVISIBLE);
    }

    void updateGeneration(long next) {
        if (next != generation) {
            cancelled = true;
            setPressed(false);
            generation = next;
        }
        setVisibility(next == 0 ? INVISIBLE : VISIBLE);
    }

    @Override public boolean onTouchEvent(MotionEvent event) {
        int type = event.getActionMasked();
        if (type == MotionEvent.ACTION_DOWN) {
            pointer = event.getPointerId(0);
            pressedGeneration = generation;
            cancelled = generation == 0;
            setPressed(!cancelled);
        } else if (type == MotionEvent.ACTION_CANCEL) {
            cancelled = true;
            pointer = -1;
            setPressed(false);
        } else {
            int index = event.findPointerIndex(pointer);
            if (index < 0 || event.getX(index) < 0 || event.getX(index) >= getWidth()
                || event.getY(index) < 0 || event.getY(index) >= getHeight()) {
                cancelled = true;
                setPressed(false);
            }
            if (type == MotionEvent.ACTION_UP || (type == MotionEvent.ACTION_POINTER_UP
                && event.getPointerId(event.getActionIndex()) == pointer)) {
                if (!cancelled && generation != 0 && pressedGeneration == generation) performClick();
                pointer = -1;
                cancelled = true;
                setPressed(false);
            }
        }
        return true;
    }

    @Override public boolean performClick() {
        super.performClick();
        if (generation != 0) TouchControlsView.submitAction(action, generation);
        return true;
    }
}
