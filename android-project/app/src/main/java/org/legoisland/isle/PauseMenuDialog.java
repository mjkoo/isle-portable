package org.legoisland.isle;

import android.app.Dialog;
import android.content.Context;
import android.view.KeyEvent;
import android.view.View;

import java.util.HashSet;
import java.util.Set;

/** Owns controller input for the menu window, including presses interrupted by another window. */
final class PauseMenuDialog extends Dialog {
    private final Set<Long> pressed = new HashSet<>();
    private final PauseMenuView menu;

    PauseMenuDialog(Context context, QuitPromptText text, Runnable onQuit, Runnable onResume, Runnable onSettings) {
        super(context, android.R.style.Theme_Translucent_NoTitleBar_Fullscreen);
        menu = new PauseMenuView(context, text, onQuit, onResume, onSettings);
        setContentView(menu);
    }

    @Override public boolean dispatchKeyEvent(KeyEvent event) {
        int key = event.getKeyCode();
        if (key != KeyEvent.KEYCODE_BUTTON_A && key != KeyEvent.KEYCODE_BUTTON_B
                && key != KeyEvent.KEYCODE_BUTTON_START) return super.dispatchKeyEvent(event);

        long identity = ((long) event.getDeviceId() << 32) | (key & 0xffffffffL);
        if (event.getAction() == KeyEvent.ACTION_DOWN) {
            if (event.getRepeatCount() == 0) pressed.add(identity);
        } else if (event.getAction() == KeyEvent.ACTION_UP) {
            // Start opens the menu: its release alone must not close it again. A release from
            // another key or controller must not discard the press we are still waiting for.
            boolean matched = pressed.remove(identity);
            if (matched && !event.isCanceled()) {
                if (key != KeyEvent.KEYCODE_BUTTON_A) {
                    cancel();
                } else {
                    View focused = menu.findFocus();
                    if (focused != null) focused.performClick();
                    else menu.focusFirstFromController();
                }
            }
        }
        return true;
    }

    @Override public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (!hasFocus) pressed.clear();
    }

    @Override public void dismiss() {
        pressed.clear();
        super.dismiss();
    }
}
