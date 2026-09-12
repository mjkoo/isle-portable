package org.legoisland.isle;

import android.content.Context;
import android.graphics.Insets;
import android.graphics.Matrix;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.util.TypedValue;
import android.view.DisplayCutout;
import android.view.View;
import android.view.WindowInsets;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.TextView;

/**
 * Places the menu, Esc and Space buttons from a TouchLayout, inside the part of the host clear of
 * system bars and display cutouts, and re-clamps them whenever the insets or the host size change.
 *
 * The buttons stay direct children of the host, placed by their margins, rather than children of a
 * full-screen group. Android gives each later finger of a gesture to a child already receiving
 * touches when that child's bounds contain it, so a full-screen parent would take every finger that
 * lands while a button is held, and none of them would reach the game surface.
 */
final class TouchControlsLayer {
    private final RelativeLayout host;
    private final View[] controls = new View[TouchLayout.COUNT];
    // Fills the host without taking touches, to report its size and the window insets.
    private final View frame;
    private final float density;
    private final int[] box = new int[4];
    private final Matrix iconMatrix = new Matrix();
    private TouchLayout layout = TouchLayout.DEFAULT;
    private int insetLeft, insetTop, insetRight, insetBottom;
    private TouchLayoutEditor editor;

    /** The action buttons' labels, by control; the menu button shows an icon instead. */
    static final String[] LABELS = {null, "Esc", "Space"};

    /** The game menu button as it looks, without what it does. */
    static ImageButton createMenuButton(Context context) {
        ImageButton menu = new ImageButton(context);
        menu.setImageResource(R.drawable.game_menu);
        menu.setBackgroundResource(R.drawable.game_menu_background);
        menu.setContentDescription("Game menu");
        menu.setPadding(0, 0, 0, 0);
        return menu;
    }

    TouchControlsLayer(RelativeLayout host, View menu, View escape, View space) {
        this.host = host;
        Context context = host.getContext();
        density = context.getResources().getDisplayMetrics().density;
        controls[TouchLayout.MENU] = menu;
        controls[TouchLayout.ESCAPE] = escape;
        controls[TouchLayout.SPACE] = space;
        frame = new View(context);
        frame.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        frame.setOnApplyWindowInsetsListener((view, insets) -> {
            applyInsets(insets);
            return insets;
        });
        // After each layout pass and before drawing, follow the host's new size. When a button
        // moves, that draw is skipped for another pass, so no frame shows the old placement.
        frame.getViewTreeObserver().addOnPreDrawListener(() -> !refresh());
        host.addView(frame, new RelativeLayout.LayoutParams(
            RelativeLayout.LayoutParams.MATCH_PARENT, RelativeLayout.LayoutParams.MATCH_PARENT));
        for (View control : controls) {
            RelativeLayout.LayoutParams params = new RelativeLayout.LayoutParams(0, 0);
            params.addRule(RelativeLayout.ALIGN_PARENT_LEFT);
            params.addRule(RelativeLayout.ALIGN_PARENT_TOP);
            host.addView(control, params);
        }
        refresh();
    }

    private void applyInsets(WindowInsets insets) {
        int left, top, right, bottom;
        if (Build.VERSION.SDK_INT >= 30) {
            Insets safe = insets.getInsets(WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout());
            left = safe.left;
            top = safe.top;
            right = safe.right;
            bottom = safe.bottom;
        } else {
            left = insets.getSystemWindowInsetLeft();
            top = insets.getSystemWindowInsetTop();
            right = insets.getSystemWindowInsetRight();
            bottom = insets.getSystemWindowInsetBottom();
            if (Build.VERSION.SDK_INT >= 28 && insets.getDisplayCutout() != null) {
                DisplayCutout cutout = insets.getDisplayCutout();
                left = Math.max(left, cutout.getSafeInsetLeft());
                top = Math.max(top, cutout.getSafeInsetTop());
                right = Math.max(right, cutout.getSafeInsetRight());
                bottom = Math.max(bottom, cutout.getSafeInsetBottom());
            }
        }
        setSafeInsets(left, top, right, bottom);
    }

    void setSafeInsets(int left, int top, int right, int bottom) {
        if (left == insetLeft && top == insetTop && right == insetRight && bottom == insetBottom) return;
        insetLeft = left;
        insetTop = top;
        insetRight = right;
        insetBottom = bottom;
        refresh();
    }

    void requestApplyInsets() { frame.requestApplyInsets(); }

    void setTouchLayout(TouchLayout next) {
        layout = next;
        for (View control : controls) {
            control.setAlpha(next.opacity);
            if (control instanceof TextView) scaleLabel((TextView) control, next.scale);
        }
        refresh();
    }

    /** Shows the editor over everything, sharing this layer's safe area. */
    void showEditor(TouchLayoutEditor next) {
        hideEditor();
        editor = next;
        host.addView(next, new RelativeLayout.LayoutParams(
            RelativeLayout.LayoutParams.MATCH_PARENT, RelativeLayout.LayoutParams.MATCH_PARENT));
        refresh();
    }

    void hideEditor() {
        if (editor == null) return;
        host.removeView(editor);
        editor = null;
    }

    /** Places every button for the current layout, insets and host size; true if any moved. */
    boolean refresh() {
        int width = frame.getWidth(), height = frame.getHeight();
        boolean moved = false;
        for (int i = 0; i < TouchLayout.COUNT; i++) {
            layout.bounds(i, insetLeft, insetTop, width - insetRight, height - insetBottom, density, box);
            int boxWidth = box[2] - box[0], boxHeight = box[3] - box[1];
            RelativeLayout.LayoutParams params = (RelativeLayout.LayoutParams) controls[i].getLayoutParams();
            if (params.width != boxWidth || params.height != boxHeight || params.leftMargin != box[0]
                    || params.topMargin != box[1]) {
                params.width = boxWidth;
                params.height = boxHeight;
                params.leftMargin = box[0];
                params.topMargin = box[1];
                // RelativeLayout otherwise narrows a child to the space left after its margins, so
                // a button larger than a tiny window would shrink instead of keeping its size.
                params.rightMargin = -boxWidth;
                params.bottomMargin = -boxHeight;
                controls[i].setLayoutParams(params);
                moved = true;
            }
            if (controls[i] instanceof ImageView) {
                scaleIcon((ImageView) controls[i], layout.scale, boxWidth, boxHeight, iconMatrix);
            }
        }
        if (editor != null) editor.setSafeArea(insetLeft, insetTop, width - insetRight, height - insetBottom);
        return moved;
    }

    /** Sizes a button's label for the button scale. */
    static void scaleLabel(TextView label, float scale) {
        label.setTextSize(TypedValue.COMPLEX_UNIT_SP, TouchActionButton.TEXT_SIZE_SP * scale);
    }

    /**
     * Centers the icon of a width by height button at its own size times the button scale, using
     * matrix as scratch. At the default size this is the same placement the button's centered image
     * had, so the icon grows with the button without changing the default look.
     */
    static void scaleIcon(ImageView view, float scale, int width, int height, Matrix matrix) {
        Drawable icon = view.getDrawable();
        if (icon == null) return;
        matrix.setScale(scale, scale);
        matrix.postTranslate(Math.round((width - icon.getIntrinsicWidth() * scale) * 0.5f),
            Math.round((height - icon.getIntrinsicHeight() * scale) * 0.5f));
        view.setScaleType(ImageView.ScaleType.MATRIX);
        view.setImageMatrix(matrix);
    }
}
