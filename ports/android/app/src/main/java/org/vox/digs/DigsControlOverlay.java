package org.vox.digs;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;

/**
 * Large, labelled phone controls.  Every action is also exposed as an Android
 * view so TalkBack can focus and announce it instead of treating the game as
 * one opaque SDL surface.
 */
public final class DigsControlOverlay extends FrameLayout {
    static final int LEFT = 0;
    static final int RIGHT = 1;
    static final int JUMP = 2;
    static final int STEAM = 3;
    static final int ROPE = 4;
    static final int FIRE = 5;
    static final int PREVIOUS = 6;
    static final int NEXT = 7;
    static final int PAUSE = 8;

    private static final int CONTROL_FILL = Color.argb(224, 0, 38, 61);
    private static final int CONTROL_STROKE = Color.rgb(255, 238, 88);
    private static final int CONTROL_TEXT = Color.WHITE;

    private static native void nativeSetButton(int control, boolean pressed);
    private static native void nativeSetAim(float x, float y, boolean active);
    private static native void nativeTap(int control);
    private static native void nativeReleaseAll();

    DigsControlOverlay(Context context) {
        super(context);
        setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        setClipChildren(false);
        setClipToPadding(false);

        addBottomStart(new HeldButton(context, JUMP), 52, 52, 56, 108,
                "JUMP", "Jump. In menus, move up.");
        addBottomStart(new HeldButton(context, LEFT), 52, 52, 4, 54,
                "LEFT", "Move left. In menus, move left.");
        addBottomStart(new HeldButton(context, RIGHT), 52, 52, 108, 54,
                "RIGHT", "Move right. In menus, move right.");
        addBottomStart(new HeldButton(context, STEAM), 52, 52, 56, 0,
                "STEAM", "Use steam. In menus, move down.");

        AimPad aimPad = new AimPad(context);
        addBottomEnd(aimPad, 92, 92, 105, 0,
                "AIM", "Aim pad. Drag to aim in the selected direction.");
        addBottomEnd(new HeldButton(context, FIRE), 52, 52, 4, 108,
                "FIRE", "Fire the selected tool. In menus, select.");
        addBottomEnd(new HeldButton(context, ROPE), 52, 52, 4, 54,
                "ROPE", "Use rope.");
        addBottomEnd(new ToolButton(context), 52, 52, 4, 0,
                "TOOL", "Weapon selector. Tap for next weapon. Touch and hold for previous weapon.");
        addTopEnd(new HeldButton(context, PAUSE), 60, 42, 6, 6,
                "PAUSE", "Pause the match. In menus, go back.");
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        // Keep unused screen touches from becoming SDL's compatibility mouse.
        return true;
    }

    static void tap(int control) {
        nativeTap(control);
    }

    static void releaseAll() {
        nativeReleaseAll();
    }

    private void addBottomStart(View view, int width, int height,
                                int left, int bottom, String label,
                                String description) {
        styleControl(view, description);
        if (view instanceof Button) {
            ((Button) view).setText(label);
        }
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                dp(width), dp(height), Gravity.BOTTOM | Gravity.START);
        params.leftMargin = dp(left);
        params.bottomMargin = dp(bottom);
        addView(view, params);
    }

    private void addBottomEnd(View view, int width, int height,
                              int right, int bottom, String label,
                              String description) {
        styleControl(view, description);
        if (view instanceof Button) {
            ((Button) view).setText(label);
        }
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                dp(width), dp(height), Gravity.BOTTOM | Gravity.END);
        params.rightMargin = dp(right);
        params.bottomMargin = dp(bottom);
        addView(view, params);
    }

    private void addTopEnd(View view, int width, int height,
                           int right, int top, String label,
                           String description) {
        styleControl(view, description);
        if (view instanceof Button) {
            ((Button) view).setText(label);
        }
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                dp(width), dp(height), Gravity.TOP | Gravity.END);
        params.rightMargin = dp(right);
        params.topMargin = dp(top);
        addView(view, params);
    }

    private void styleControl(View view, String description) {
        view.setContentDescription(description);
        view.setFocusable(true);
        view.setClickable(true);
        if (view instanceof Button) {
            Button button = (Button) view;
            button.setAllCaps(false);
            button.setGravity(Gravity.CENTER);
            button.setIncludeFontPadding(false);
            button.setMinWidth(0);
            button.setMinHeight(0);
            button.setPadding(dp(2), dp(2), dp(2), dp(2));
            button.setTextColor(CONTROL_TEXT);
            button.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
            button.setTypeface(Typeface.DEFAULT_BOLD);
            button.setBackground(controlBackground());
        }
    }

    private GradientDrawable controlBackground() {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(CONTROL_FILL);
        drawable.setCornerRadius(dp(12));
        drawable.setStroke(dp(2), CONTROL_STROKE);
        return drawable;
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private static final class HeldButton extends Button {
        private final int control;
        private boolean touchInProgress;

        HeldButton(Context context, int control) {
            super(context);
            this.control = control;
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            switch (event.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    touchInProgress = true;
                    // Preserve a very quick touch until the native frame can
                    // sample it; low-end phones can otherwise miss it between
                    // two presentation frames.
                    nativeTap(control);
                    nativeSetButton(control, true);
                    setPressed(true);
                    return true;
                case MotionEvent.ACTION_UP:
                    nativeSetButton(control, false);
                    performClick();
                    touchInProgress = false;
                    setPressed(false);
                    return true;
                case MotionEvent.ACTION_CANCEL:
                    nativeSetButton(control, false);
                    touchInProgress = false;
                    setPressed(false);
                    return true;
                default:
                    return true;
            }
        }

        @Override
        public boolean performClick() {
            super.performClick();
            if (!touchInProgress) {
                nativeTap(control);
            }
            return true;
        }
    }

    private static final class ToolButton extends Button {
        private boolean touchInProgress;
        private long touchStartedAt;

        ToolButton(Context context) {
            super(context);
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            switch (event.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    touchInProgress = true;
                    touchStartedAt = event.getEventTime();
                    setPressed(true);
                    return true;
                case MotionEvent.ACTION_UP:
                    if (event.getEventTime() - touchStartedAt >= 500L) {
                        nativeTap(PREVIOUS);
                    } else {
                        nativeTap(NEXT);
                    }
                    performClick();
                    touchInProgress = false;
                    setPressed(false);
                    return true;
                case MotionEvent.ACTION_CANCEL:
                    touchInProgress = false;
                    setPressed(false);
                    return true;
                default:
                    return true;
            }
        }

        @Override
        public boolean performClick() {
            super.performClick();
            if (!touchInProgress) {
                nativeTap(NEXT);
            }
            return true;
        }

        @Override
        public boolean performLongClick() {
            super.performLongClick();
            if (!touchInProgress) {
                nativeTap(PREVIOUS);
            }
            return true;
        }
    }

    private final class AimPad extends View {
        private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint stroke = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);
        private boolean touchInProgress;
        private float aimX;
        private float aimY;

        AimPad(Context context) {
            super(context);
            fill.setColor(CONTROL_FILL);
            stroke.setColor(CONTROL_STROKE);
            stroke.setStyle(Paint.Style.STROKE);
            stroke.setStrokeWidth(dp(2));
            text.setColor(CONTROL_TEXT);
            text.setTextAlign(Paint.Align.CENTER);
            text.setTextSize(dp(12));
            text.setTypeface(Typeface.DEFAULT_BOLD);
        }

        @Override
        protected void onDraw(Canvas canvas) {
            float centerX = getWidth() * 0.5f;
            float centerY = getHeight() * 0.5f;
            float radius = Math.min(getWidth(), getHeight()) * 0.5f - dp(3);
            canvas.drawCircle(centerX, centerY, radius, fill);
            canvas.drawCircle(centerX, centerY, radius, stroke);
            canvas.drawLine(centerX - radius * 0.55f, centerY,
                    centerX + radius * 0.55f, centerY, stroke);
            canvas.drawLine(centerX, centerY - radius * 0.55f,
                    centerX, centerY + radius * 0.55f, stroke);
            canvas.drawCircle(centerX + aimX * radius * 0.58f,
                    centerY + aimY * radius * 0.58f, dp(6), stroke);
            canvas.drawText("AIM", centerX,
                    centerY - (text.ascent() + text.descent()) * 0.5f, text);
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            switch (event.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                case MotionEvent.ACTION_MOVE:
                    touchInProgress = true;
                    updateAim(event.getX(), event.getY());
                    return true;
                case MotionEvent.ACTION_UP:
                    // Retain the last drag vector for one native sample. This
                    // lets a quick aim flick establish a direction even when
                    // the Java touch finishes between two slow render frames.
                    nativeSetAim(aimX, aimY, false);
                    performClick();
                    touchInProgress = false;
                    aimX = 0.0f;
                    aimY = 0.0f;
                    invalidate();
                    return true;
                case MotionEvent.ACTION_CANCEL:
                    nativeSetAim(0.0f, 0.0f, false);
                    touchInProgress = false;
                    aimX = 0.0f;
                    aimY = 0.0f;
                    invalidate();
                    return true;
                default:
                    return true;
            }
        }

        @Override
        public boolean performClick() {
            super.performClick();
            return true;
        }

        private void updateAim(float x, float y) {
            float centerX = getWidth() * 0.5f;
            float centerY = getHeight() * 0.5f;
            float radius = Math.max(1.0f,
                    Math.min(getWidth(), getHeight()) * 0.5f - dp(8));
            float distance;
            aimX = (x - centerX) / radius;
            aimY = (y - centerY) / radius;
            distance = (float) Math.sqrt(aimX * aimX + aimY * aimY);
            if (distance > 1.0f) {
                aimX /= distance;
                aimY /= distance;
            }
            nativeSetAim(aimX, aimY, true);
            invalidate();
        }
    }
}
