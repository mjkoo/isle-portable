package org.legoisland.isle;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Color;
import android.os.Build;
import android.view.MotionEvent;
import android.widget.Button;

/** Owns only gestures that begin on this target; other fingers stay with the SDL surface. */
// SDLActivity uses a platform theme, so this view deliberately uses the platform Button.
@SuppressLint({"ViewConstructor", "AppCompatCustomView"})
final class TouchActionButton extends Button {
    interface ActionListener { void onAction(long generation); }

    private final ActionListener action;
    private long generation, pressedGeneration;
    private int pointer = -1;
    private boolean cancelled;

    TouchActionButton(Context context, String label, ActionListener action) {
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
            if (!stayedInside(event, index)) {
                cancelled = true;
                setPressed(false);
            }
            if (type == MotionEvent.ACTION_UP || (type == MotionEvent.ACTION_POINTER_UP
                && event.getPointerId(event.getActionIndex()) == pointer)) {
                // A rejected pointer can end independently of the other fingers on this button.
                if (Build.VERSION.SDK_INT >= 33 && (event.getFlags() & MotionEvent.FLAG_CANCELED) != 0) {
                    cancelled = true;
                }
                if (!cancelled && generation != 0 && pressedGeneration == generation) performClick();
                pointer = -1;
                cancelled = true;
                setPressed(false);
            }
        }
        return true;
    }

    private boolean stayedInside(MotionEvent event, int index) {
        if (index < 0) return false;
        // Android batches moves; returning inside must not hide an earlier excursion.
        for (int sample = 0; sample < event.getHistorySize(); sample++) {
            if (!contains(event.getHistoricalX(index, sample), event.getHistoricalY(index, sample))) return false;
        }
        return contains(event.getX(index), event.getY(index));
    }

    private boolean contains(float x, float y) {
        return x >= 0 && x < getWidth() && y >= 0 && y < getHeight();
    }

    @Override public boolean performClick() {
        super.performClick();
        if (generation != 0) action.onAction(generation);
        return true;
    }
}
