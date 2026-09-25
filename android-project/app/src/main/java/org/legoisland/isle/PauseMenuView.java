package org.legoisland.isle;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.StateListDrawable;
import android.os.Build;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

/**
 * The game menu's content: a dark panel centred over the dimmed game, with the title, what the
 * save did, and one large button per choice in a column. Every button takes D-pad focus and shows
 * it plainly, so a controller can drive the menu as well as a finger.
 *
 * The safe choice comes first and takes the initial focus, so a press of A without looking never
 * quits: Resume, or Settings when there is nothing to resume.
 */
// Created by QuitPrompt, never inflated from XML.
@SuppressLint("ViewConstructor")
final class PauseMenuView extends FrameLayout {
    private static final int SCRIM = 0x99000000;
    private static final int PANEL = 0xF01E2226;
    private static final int BUTTON = 0x33FFFFFF;
    private static final int BUTTON_PRESSED = 0xFF2A8FBF;
    private static final int MESSAGE = 0xFFD0D4D8;

    private final float density;
    private final View first;

    /** onResume is unused when the text offers no Resume. */
    PauseMenuView(Context context, QuitPromptText text, Runnable onQuit, Runnable onResume, Runnable onSettings) {
        super(context);
        density = context.getResources().getDisplayMetrics().density;
        setBackgroundColor(SCRIM);

        LinearLayout panel = new LinearLayout(context);
        panel.setOrientation(LinearLayout.VERTICAL);
        int padding = dp(24);
        panel.setPadding(padding, padding, padding, padding);
        GradientDrawable shape = new GradientDrawable();
        shape.setColor(PANEL);
        shape.setCornerRadius(dp(16));
        panel.setBackground(shape);

        TextView title = new TextView(context);
        title.setText(text.title);
        title.setTextColor(Color.WHITE);
        title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 24);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        panel.addView(title);

        TextView message = new TextView(context);
        message.setText(text.message);
        message.setTextColor(MESSAGE);
        message.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        LinearLayout.LayoutParams messageParams = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        messageParams.topMargin = dp(8);
        messageParams.bottomMargin = dp(16);
        panel.addView(message, messageParams);

        View resume = text.resume != null ? addButton(panel, text.resume, onResume) : null;
        View settings = addButton(panel, text.settings, onSettings);
        addButton(panel, text.quit, onQuit);
        first = resume != null ? resume : settings;

        // Scrolls only on a screen too short for the whole panel, such as a phone held sideways
        // with a large font.
        ScrollView scroll = new ScrollView(context);
        // The buttons take the focus, never the frame around them.
        scroll.setFocusable(false);
        scroll.setDescendantFocusability(FOCUS_AFTER_DESCENDANTS);
        scroll.addView(panel);
        int width = Math.min(dp(420), (int) (context.getResources().getDisplayMetrics().widthPixels * 0.9f));
        addView(scroll, new FrameLayout.LayoutParams(width, LayoutParams.WRAP_CONTENT, Gravity.CENTER));
    }

    /** Puts the D-pad focus on the safe choice. */
    void focusFirst() {
        first.requestFocus();
    }

    /** A controller can take over after touch without activating the newly focused choice. */
    void focusFirstFromController() {
        first.requestFocusFromTouch();
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        // Once the dialog's window exists, which is what focus is granted by.
        post(this::focusFirst);
    }

    private View addButton(LinearLayout panel, String label, Runnable action) {
        Button button = new Button(getContext());
        button.setText(label);
        button.setAllCaps(false);
        // The fill already shows focus and presses; the platform's raised shadow would not match it.
        button.setStateListAnimator(null);
        button.setGravity(Gravity.CENTER);
        button.setTextSize(TypedValue.COMPLEX_UNIT_SP, 18);
        button.setTypeface(Typeface.DEFAULT_BOLD);
        button.setMinHeight(dp(52));
        if (Build.VERSION.SDK_INT >= 26) button.setDefaultFocusHighlightEnabled(false);
        button.setOnClickListener(view -> action.run());

        StateListDrawable background = new StateListDrawable();
        background.addState(new int[] {android.R.attr.state_pressed}, rounded(BUTTON_PRESSED));
        background.addState(new int[] {android.R.attr.state_focused}, rounded(TouchLayoutEditor.ACCENT));
        background.addState(new int[0], rounded(BUTTON));
        button.setBackground(background);
        // Dark text on the bright focused fill, white everywhere else.
        button.setTextColor(new ColorStateList(
            new int[][] {{android.R.attr.state_pressed}, {android.R.attr.state_focused}, {}},
            new int[] {Color.WHITE, Color.BLACK, Color.WHITE}));

        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        params.topMargin = dp(8);
        panel.addView(button, params);
        return button;
    }

    private GradientDrawable rounded(int color) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(color);
        drawable.setCornerRadius(dp(10));
        return drawable;
    }

    private int dp(float value) {
        return Math.round(value * density);
    }
}
