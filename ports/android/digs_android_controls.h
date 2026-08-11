/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DIGS_ANDROID_CONTROLS_H
#define DIGS_ANDROID_CONTROLS_H

typedef enum digs_android_control {
    DIGS_ANDROID_CONTROL_LEFT = 0,
    DIGS_ANDROID_CONTROL_RIGHT = 1,
    DIGS_ANDROID_CONTROL_JUMP = 2,
    DIGS_ANDROID_CONTROL_STEAM = 3,
    DIGS_ANDROID_CONTROL_ROPE = 4,
    DIGS_ANDROID_CONTROL_FIRE = 5,
    DIGS_ANDROID_CONTROL_PREVIOUS = 6,
    DIGS_ANDROID_CONTROL_NEXT = 7,
    DIGS_ANDROID_CONTROL_PAUSE = 8,
    DIGS_ANDROID_CONTROL_COUNT = 9
} digs_android_control;

typedef struct digs_android_control_state {
    int buttons[DIGS_ANDROID_CONTROL_COUNT];
    int aim_x_q15;
    int aim_y_q15;
    int aim_active;
    int aim_revision;
} digs_android_control_state;

void digs_android_controls_snapshot(digs_android_control_state *state);
int digs_android_controls_take_tap(int control);
void digs_android_controls_release_all(void);
void digs_android_controls_set_data_root(const char *path);
const char *digs_android_controls_data_root(void);

#endif
