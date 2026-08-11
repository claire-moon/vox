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

import java.util.ArrayList;

/**
 * Phone controls for the SDL game.  Menu and gameplay controls are separate
 * visible modes so a new player is never asked to infer a keyboard mapping
 * from a gameplay-only label.
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
    static final int MENU_UP = 9;
    static final int MENU_DOWN = 10;
    static final int MENU_SELECT = 11;
    static final int MENU_BACK = 12;

    private static final int MODE_MENU = 0;
    private static final int MODE_GAMEPLAY = 1;
    private static final long MODE_POLL_INTERVAL_MS = 90L;

    private static final int CONTROL_FILL = Color.argb(224, 0, 38, 61);
    private static final int CONTROL_STROKE = Color.rgb(255, 238, 88);
    private static final int CONTROL_TEXT = Color.WHITE;

    private static native void nativeSetButton(int control, boolean pressed);
    private static native void nativeSetAim(float x, float y, boolean active);
    private static native void nativeSetPointerAim(float x, float y,
                                                   boolean active);
    private static native void nativeTap(int control);
    private static native void nativeReleaseAll();
    private static native int nativeGetControlMode();

    private final ArrayList<View> menuControls = new ArrayList<View>();
    private final ArrayList<View> gameplayControls = new ArrayList<View>();
    private final TouchAimSurface touchAimSurface;
    private boolean gameplayMode = true;
    private final Runnable modePoller = new Runnable() {
        @Override
        public void run() {
            setControlMode(nativeGetControlMode() == MODE_GAMEPLAY);
            postDelayed(this, MODE_POLL_INTERVAL_MS);
        }
    };

    DigsControlOverlay(Context context) {
        super(context);
        setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        setClipChildren(false);
        setClipToPadding(false);
        setMotionEventSplittingEnabled(true);

        touchAimSurface = new TouchAimSurface(context);
        addView(touchAimSurface, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));
        gameplayControls.add(touchAimSurface);

        addMenuBottomStart(new HeldButton(context, MENU_UP), 56, 52, 56, 108,
                "UP", "Move the menu selection up.");
        addMenuBottomStart(new HeldButton(context, LEFT), 56, 52, 2, 54,
                "LEFT", "Change the selected menu value left.");
        addMenuBottomStart(new HeldButton(context, RIGHT), 56, 52, 110, 54,
                "RIGHT", "Change the selected menu value right.");
        addMenuBottomStart(new HeldButton(context, MENU_DOWN), 56, 52, 56, 0,
                "DOWN", "Move the menu selection down.");
        addMenuBottomEnd(new HeldButton(context, MENU_SELECT), 84, 56, 4, 54,
                "SELECT", "Activate the selected menu item.");
        addMenuTopEnd(new HeldButton(context, MENU_BACK), 64, 42, 6, 6,
                "BACK", "Go back to the previous menu.");

        addGameBottomStart(new HeldButton(context, JUMP), 52, 52, 56, 108,
                "JUMP", "Jump.");
        addGameBottomStart(new HeldButton(context, LEFT), 52, 52, 4, 54,
                "LEFT", "Move left.");
        addGameBottomStart(new HeldButton(context, RIGHT), 52, 52, 108, 54,
                "RIGHT", "Move right.");
        addGameBottomStart(new HeldButton(context, STEAM), 52, 52, 56, 0,
                "STEAM", "Use steam.");

        AimPad aimPad = new AimPad(context);
        addGameBottomEnd(aimPad, 92, 92, 105, 0,
                "AIM", "Aim pad. Drag to aim in the selected direction.");
        addGameBottomEnd(new HeldButton(context, FIRE), 52, 52, 4, 108,
                "FIRE", "Fire the selected tool. Hold for charge weapons.");
        addGameBottomEnd(new HeldButton(context, ROPE), 52, 52, 4, 54,
                "ROPE", "Use rope.");
        addGameBottomEnd(new ToolButton(context), 52, 52, 4, 0,
                "TOOL", "Weapon selector. Tap for next weapon. Touch and hold for previous weapon.");
        addGameTopEnd(new HeldButton(context, PAUSE), 64, 42, 6, 6,
                "PAUSE", "Pause the match.");

        setControlMode(false);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        removeCallbacks(modePoller);
        post(modePoller);
    }

    @Override
    protected void onDetachedFromWindow() {
        removeCallbacks(modePoller);
        super.onDetachedFromWindow();
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        /* Menu blank-space touches should never reach SDL's compatibility
         * mouse. Gameplay blank-space touches are handled by TouchAimSurface. */
        return true;
    }

    static void tap(int control) {
        nativeTap(control);
    }

    static void back() {
        nativeTap(nativeGetControlMode() == MODE_GAMEPLAY ? PAUSE : MENU_BACK);
    }

    static void releaseAll() {
        nativeReleaseAll();
    }

    private void setControlMode(boolean gameplay) {
        int index;
        if (gameplayMode == gameplay) {
            return;
        }
        gameplayMode = gameplay;
        for (index = 0; index < menuControls.size(); ++index) {
            menuControls.get(index).setVisibility(gameplay ? GONE : VISIBLE);
        }
        for (index = 0; index < gameplayControls.size(); ++index) {
            gameplayControls.get(index).setVisibility(gameplay ? VISIBLE : GONE);
        }
        if (!gameplay) {
            nativeSetPointerAim(0.5f, 0.5f, false);
        }
    }

    private void addMenuBottomStart(View view, int width, int height,
                                    int left, int bottom, String label,
                                    String description) {
        addBottomStart(view, width, height, left, bottom, label, description,
                menuControls);
    }

    private void addMenuBottomEnd(View view, int width, int height,
                                  int right, int bottom, String label,
                                  String description) {
        addBottomEnd(view, width, height, right, bottom, label, description,
                menuControls);
    }

    private void addMenuTopEnd(View view, int width, int height,
                               int right, int top, String label,
                               String description) {
        addTopEnd(view, width, height, right, top, label, description,
                menuControls);
    }

    private void addGameBottomStart(View view, int width, int height,
                                    int left, int bottom, String label,
                                    String description) {
        addBottomStart(view, width, height, left, bottom, label, description,
                gameplayControls);
    }

    private void addGameBottomEnd(View view, int width, int height,
                                  int right, int bottom, String label,
                                  String description) {
        addBottomEnd(view, width, height, right, bottom, label, description,
                gameplayControls);
    }

    private void addGameTopEnd(View view, int width, int height,
                               int right, int top, String label,
                               String description) {
        addTopEnd(view, width, height, right, top, label, description,
                gameplayControls);
    }

    private void addBottomStart(View view, int width, int height,
                                int left, int bottom, String label,
                                String description, ArrayList<View> group) {
        styleControl(view, description);
        if (view instanceof Button) {
            ((Button) view).setText(label);
        }
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                dp(width), dp(height), Gravity.BOTTOM | Gravity.START);
        params.leftMargin = dp(left);
        params.bottomMargin = dp(bottom);
        addView(view, params);
        group.add(view);
    }

    private void addBottomEnd(View view, int width, int height,
                              int right, int bottom, String label,
                              String description, ArrayList<View> group) {
        styleControl(view, description);
        if (view instanceof Button) {
            ((Button) view).setText(label);
        }
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                dp(width), dp(height), Gravity.BOTTOM | Gravity.END);
        params.rightMargin = dp(right);
        params.bottomMargin = dp(bottom);
        addView(view, params);
        group.add(view);
    }

    private void addTopEnd(View view, int width, int height,
                           int right, int top, String label,
                           String description, ArrayList<View> group) {
        styleControl(view, description);
        if (view instanceof Button) {
            ((Button) view).setText(label);
        }
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                dp(width), dp(height), Gravity.TOP | Gravity.END);
        params.rightMargin = dp(right);
        params.topMargin = dp(top);
        addView(view, params);
        group.add(view);
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

    private final class TouchAimSurface extends View {
        private int pointerId = MotionEvent.INVALID_POINTER_ID;
        private float lastX;
        private float lastY;

        TouchAimSurface(Context context) {
            super(context);
            setClickable(true);
            setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            int index;
            switch (event.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    pointerId = event.getPointerId(0);
                    updatePointer(event.getX(0), event.getY(0), true);
                    return true;
                case MotionEvent.ACTION_POINTER_DOWN:
                    if (pointerId == MotionEvent.INVALID_POINTER_ID) {
                        index = event.getActionIndex();
                        pointerId = event.getPointerId(index);
                        updatePointer(event.getX(index), event.getY(index), true);
                    }
                    return true;
                case MotionEvent.ACTION_MOVE:
                    index = event.findPointerIndex(pointerId);
                    if (index >= 0) {
                        updatePointer(event.getX(index), event.getY(index), true);
                    }
                    return true;
                case MotionEvent.ACTION_POINTER_UP:
                    index = event.getActionIndex();
                    if (event.getPointerId(index) == pointerId) {
                        updatePointer(event.getX(index), event.getY(index), false);
                        pointerId = MotionEvent.INVALID_POINTER_ID;
                    }
                    return true;
                case MotionEvent.ACTION_UP:
                    updatePointer(event.getX(), event.getY(), false);
                    pointerId = MotionEvent.INVALID_POINTER_ID;
                    performClick();
                    return true;
                case MotionEvent.ACTION_CANCEL:
                    nativeSetPointerAim(normalizeX(lastX), normalizeY(lastY), false);
                    pointerId = MotionEvent.INVALID_POINTER_ID;
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

        private void updatePointer(float x, float y, boolean active) {
            lastX = x;
            lastY = y;
            nativeSetPointerAim(normalizeX(x), normalizeY(y), active);
        }

        private float normalizeX(float x) {
            float width = Math.max(1.0f, (float) getWidth());
            float normalized = x / width;
            if (normalized < 0.0f) return 0.0f;
            if (normalized > 1.0f) return 1.0f;
            return normalized;
        }

        private float normalizeY(float y) {
            float height = Math.max(1.0f, (float) getHeight());
            float normalized = y / height;
            if (normalized < 0.0f) return 0.0f;
            if (normalized > 1.0f) return 1.0f;
            return normalized;
        }
    }

    private final class AimPad extends View {
        private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint stroke = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);
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
                    updateAim(event.getX(), event.getY());
                    return true;
                case MotionEvent.ACTION_UP:
                    nativeSetAim(aimX, aimY, false);
                    performClick();
                    aimX = 0.0f;
                    aimY = 0.0f;
                    invalidate();
                    return true;
                case MotionEvent.ACTION_CANCEL:
                    nativeSetAim(0.0f, 0.0f, false);
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
