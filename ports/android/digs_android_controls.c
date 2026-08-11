/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <string.h>

#include <SDL.h>

#include "digs_android_controls.h"

#if defined(__ANDROID__)
#include <jni.h>

#define DIGS_ANDROID_DATA_ROOT_CAPACITY 512U

static SDL_atomic_t digs_android_buttons[DIGS_ANDROID_CONTROL_COUNT];
static SDL_atomic_t digs_android_taps[DIGS_ANDROID_CONTROL_COUNT];
static SDL_atomic_t digs_android_aim_x;
static SDL_atomic_t digs_android_aim_y;
static SDL_atomic_t digs_android_aim_active;
static SDL_atomic_t digs_android_aim_revision;
static SDL_atomic_t digs_android_pointer_x;
static SDL_atomic_t digs_android_pointer_y;
static SDL_atomic_t digs_android_pointer_active;
static SDL_atomic_t digs_android_pointer_revision;
static SDL_atomic_t digs_android_mode_value;
static char digs_android_data_root[DIGS_ANDROID_DATA_ROOT_CAPACITY];

static int digs_android_control_valid(int control)
{
    return control >= 0 && control < DIGS_ANDROID_CONTROL_COUNT;
}

static int digs_android_q15(jfloat value)
{
    if (value < -1.0f) value = -1.0f;
    if (value > 1.0f) value = 1.0f;
    return (int)(value * 32767.0f);
}

static int digs_android_unit_q15(jfloat value)
{
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    return (int)(value * 32767.0f);
}

void digs_android_controls_snapshot(digs_android_control_state *state)
{
    int control;
    if (state == 0) return;
    for (control = 0; control < DIGS_ANDROID_CONTROL_COUNT; ++control) {
        state->buttons[control] =
            SDL_AtomicGet(&digs_android_buttons[control]) != 0;
    }
    state->aim_x_q15 = SDL_AtomicGet(&digs_android_aim_x);
    state->aim_y_q15 = SDL_AtomicGet(&digs_android_aim_y);
    state->aim_active = SDL_AtomicGet(&digs_android_aim_active) != 0;
    state->aim_revision = SDL_AtomicGet(&digs_android_aim_revision);
    state->pointer_x_q15 = SDL_AtomicGet(&digs_android_pointer_x);
    state->pointer_y_q15 = SDL_AtomicGet(&digs_android_pointer_y);
    state->pointer_active =
        SDL_AtomicGet(&digs_android_pointer_active) != 0;
    state->pointer_revision =
        SDL_AtomicGet(&digs_android_pointer_revision);
}

int digs_android_controls_take_tap(int control)
{
    int count;
    if (!digs_android_control_valid(control)) return 0;
    count = SDL_AtomicGet(&digs_android_taps[control]);
    if (count > 0) {
        (void)SDL_AtomicAdd(&digs_android_taps[control], -count);
    }
    return count;
}

void digs_android_controls_release_all(void)
{
    int control;
    for (control = 0; control < DIGS_ANDROID_CONTROL_COUNT; ++control) {
        SDL_AtomicSet(&digs_android_buttons[control], 0);
        SDL_AtomicSet(&digs_android_taps[control], 0);
    }
    SDL_AtomicSet(&digs_android_aim_x, 0);
    SDL_AtomicSet(&digs_android_aim_y, 0);
    SDL_AtomicSet(&digs_android_aim_active, 0);
    (void)SDL_AtomicAdd(&digs_android_aim_revision, 1);
    SDL_AtomicSet(&digs_android_pointer_x, 0);
    SDL_AtomicSet(&digs_android_pointer_y, 0);
    SDL_AtomicSet(&digs_android_pointer_active, 0);
    (void)SDL_AtomicAdd(&digs_android_pointer_revision, 1);
}

void digs_android_controls_set_data_root(const char *path)
{
    size_t length;
    digs_android_data_root[0] = '\0';
    if (path == 0) return;
    length = strlen(path);
    if (length == 0U || length >= sizeof(digs_android_data_root)) return;
    memcpy(digs_android_data_root, path, length + 1U);
}

const char *digs_android_controls_data_root(void)
{
    return digs_android_data_root[0] == '\0' ? 0 : digs_android_data_root;
}

void digs_android_controls_set_mode(int mode)
{
    SDL_AtomicSet(&digs_android_mode_value,
                  mode == DIGS_ANDROID_CONTROL_MODE_GAMEPLAY ?
                  DIGS_ANDROID_CONTROL_MODE_GAMEPLAY :
                  DIGS_ANDROID_CONTROL_MODE_MENU);
}

int digs_android_controls_get_mode(void)
{
    return SDL_AtomicGet(&digs_android_mode_value) ==
           DIGS_ANDROID_CONTROL_MODE_GAMEPLAY ?
           DIGS_ANDROID_CONTROL_MODE_GAMEPLAY :
           DIGS_ANDROID_CONTROL_MODE_MENU;
}

JNIEXPORT void JNICALL
Java_org_vox_digs_DigsControlOverlay_nativeSetButton(JNIEnv *environment,
                                                       jclass type,
                                                       jint control,
                                                       jboolean pressed)
{
    (void)environment;
    (void)type;
    if (digs_android_control_valid((int)control)) {
        SDL_AtomicSet(&digs_android_buttons[(int)control],
                      pressed == JNI_TRUE ? 1 : 0);
    }
}

JNIEXPORT void JNICALL
Java_org_vox_digs_DigsControlOverlay_nativeSetAim(JNIEnv *environment,
                                                    jclass type,
                                                    jfloat x,
                                                    jfloat y,
                                                    jboolean active)
{
    (void)environment;
    (void)type;
    SDL_AtomicSet(&digs_android_aim_x, digs_android_q15(x));
    SDL_AtomicSet(&digs_android_aim_y, digs_android_q15(y));
    SDL_AtomicSet(&digs_android_aim_active, active == JNI_TRUE ? 1 : 0);
    (void)SDL_AtomicAdd(&digs_android_aim_revision, 1);
}

JNIEXPORT void JNICALL
Java_org_vox_digs_DigsControlOverlay_nativeSetPointerAim(JNIEnv *environment,
                                                           jclass type,
                                                           jfloat x,
                                                           jfloat y,
                                                           jboolean active)
{
    (void)environment;
    (void)type;
    SDL_AtomicSet(&digs_android_pointer_x, digs_android_unit_q15(x));
    SDL_AtomicSet(&digs_android_pointer_y, digs_android_unit_q15(y));
    SDL_AtomicSet(&digs_android_pointer_active,
                  active == JNI_TRUE ? 1 : 0);
    (void)SDL_AtomicAdd(&digs_android_pointer_revision, 1);
}

JNIEXPORT jint JNICALL
Java_org_vox_digs_DigsControlOverlay_nativeGetControlMode(JNIEnv *environment,
                                                            jclass type)
{
    (void)environment;
    (void)type;
    return (jint)digs_android_controls_get_mode();
}

JNIEXPORT void JNICALL
Java_org_vox_digs_DigsControlOverlay_nativeTap(JNIEnv *environment,
                                                 jclass type,
                                                 jint control)
{
    (void)environment;
    (void)type;
    if (digs_android_control_valid((int)control)) {
        (void)SDL_AtomicAdd(&digs_android_taps[(int)control], 1);
    }
}

JNIEXPORT void JNICALL
Java_org_vox_digs_DigsControlOverlay_nativeReleaseAll(JNIEnv *environment,
                                                        jclass type)
{
    (void)environment;
    (void)type;
    digs_android_controls_release_all();
}

JNIEXPORT void JNICALL
Java_org_vox_digs_DigsActivity_nativeSetDataRoot(JNIEnv *environment,
                                                   jclass type,
                                                   jstring path)
{
    const char *value;
    (void)type;
    if (path == 0) {
        digs_android_controls_set_data_root(0);
        return;
    }
    value = (*environment)->GetStringUTFChars(environment, path, 0);
    if (value != 0) {
        digs_android_controls_set_data_root(value);
        (*environment)->ReleaseStringUTFChars(environment, path, value);
    }
}

#endif
