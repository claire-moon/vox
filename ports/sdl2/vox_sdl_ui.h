/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VOX_SDL_UI_H
#define VOX_SDL_UI_H

#include "vox/vox_types.h"

typedef struct vox_ui_surface {
    vox_u8 *pixels;
    vox_u32 width;
    vox_u32 height;
    vox_u32 stride;
} vox_ui_surface;

/*
 * The UI owns this tiny bitmap face rather than loading a host font.  Its
 * compact square grid is deliberately inspired by VGA-era DOS applications,
 * but the glyphs and metrics below are original project data.  Integer
 * scaling makes the same layout deterministic on every supported host.
 */
#define VOX_UI_DOS_GLYPH_WIDTH 5
#define VOX_UI_DOS_GLYPH_HEIGHT 7
#define VOX_UI_DOS_ADVANCE 5
#define VOX_UI_DOS_LINE_HEIGHT 8

void vox_ui_fill(vox_ui_surface *surface, vox_u8 red, vox_u8 green,
                 vox_u8 blue);
void vox_ui_rect(vox_ui_surface *surface, int x, int y, int width, int height,
                 vox_u8 red, vox_u8 green, vox_u8 blue);
void vox_ui_frame(vox_ui_surface *surface, int x, int y, int width, int height,
                  vox_u8 red, vox_u8 green, vox_u8 blue);
void vox_ui_text(vox_ui_surface *surface, int x, int y, int scale,
                 const char *text, vox_u8 red, vox_u8 green, vox_u8 blue);
void vox_ui_text_center(vox_ui_surface *surface, int center_x, int y,
                        int scale, const char *text, vox_u8 red,
                        vox_u8 green, vox_u8 blue);
void vox_ui_text_shadow(vox_ui_surface *surface, int x, int y, int scale,
                        const char *text, vox_u8 red, vox_u8 green,
                        vox_u8 blue);
void vox_ui_text_center_shadow(vox_ui_surface *surface, int center_x, int y,
                               int scale, const char *text, vox_u8 red,
                               vox_u8 green, vox_u8 blue);
int vox_ui_text_width(const char *text, int scale);
int vox_ui_text_wrap(vox_ui_surface *surface, int x, int y, int width,
                     int max_lines, int scale, const char *text,
                     vox_u8 red, vox_u8 green, vox_u8 blue);

#endif
