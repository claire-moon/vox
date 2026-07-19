/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vox_sdl_ui.h"

static const vox_u8 vox_ui_letters[26][7] = {
    {14U,17U,17U,31U,17U,17U,17U},
    {30U,17U,17U,30U,17U,17U,30U},
    {14U,17U,16U,16U,16U,17U,14U},
    {30U,17U,17U,17U,17U,17U,30U},
    {31U,16U,16U,30U,16U,16U,31U},
    {31U,16U,16U,30U,16U,16U,16U},
    {14U,17U,16U,23U,17U,17U,15U},
    {17U,17U,17U,31U,17U,17U,17U},
    {14U,4U,4U,4U,4U,4U,14U},
    {7U,2U,2U,2U,18U,18U,12U},
    {17U,18U,20U,24U,20U,18U,17U},
    {16U,16U,16U,16U,16U,16U,31U},
    {17U,27U,21U,21U,17U,17U,17U},
    {17U,25U,21U,19U,17U,17U,17U},
    {14U,17U,17U,17U,17U,17U,14U},
    {30U,17U,17U,30U,16U,16U,16U},
    {14U,17U,17U,17U,21U,18U,13U},
    {30U,17U,17U,30U,20U,18U,17U},
    {15U,16U,16U,14U,1U,1U,30U},
    {31U,4U,4U,4U,4U,4U,4U},
    {17U,17U,17U,17U,17U,17U,14U},
    {17U,17U,17U,17U,17U,10U,4U},
    {17U,17U,17U,21U,21U,21U,10U},
    {17U,17U,10U,4U,10U,17U,17U},
    {17U,17U,10U,4U,4U,4U,4U},
    {31U,1U,2U,4U,8U,16U,31U}
};

static const vox_u8 vox_ui_digits[10][7] = {
    {14U,17U,19U,21U,25U,17U,14U},
    {4U,12U,4U,4U,4U,4U,14U},
    {14U,17U,1U,2U,4U,8U,31U},
    {30U,1U,1U,14U,1U,1U,30U},
    {2U,6U,10U,18U,31U,2U,2U},
    {31U,16U,16U,30U,1U,1U,30U},
    {14U,16U,16U,30U,17U,17U,14U},
    {31U,1U,2U,4U,8U,8U,8U},
    {14U,17U,17U,14U,17U,17U,14U},
    {14U,17U,17U,15U,1U,1U,14U}
};

static vox_u8 vox_ui_glyph_row(char character, int row)
{
    if (character >= 'a' && character <= 'z') {
        character = (char)(character - 'a' + 'A');
    }
    if (character >= 'A' && character <= 'Z') {
        return vox_ui_letters[(int)(character - 'A')][row];
    }
    if (character >= '0' && character <= '9') {
        return vox_ui_digits[(int)(character - '0')][row];
    }
    if (character == '-') {
        return row == 3 ? 31U : 0U;
    }
    if (character == '_') {
        return row == 6 ? 31U : 0U;
    }
    if (character == '=') {
        return row == 2 || row == 4 ? 31U : 0U;
    }
    if (character == ':') {
        return row == 2 || row == 5 ? 4U : 0U;
    }
    if (character == '.') {
        return row == 6 ? 4U : 0U;
    }
    if (character == ',') {
        return row == 5 ? 4U : (row == 6 ? 8U : 0U);
    }
    if (character == '!') {
        return row < 5 || row == 6 ? 4U : 0U;
    }
    if (character == '?') {
        return (row == 0 ? 14U : row == 1 ? 17U : row == 2 ? 2U :
                row == 3 ? 4U : row == 5 ? 4U : 0U);
    }
    if (character == '/') {
        return (vox_u8)(1U << (row < 5 ? (4 - row) : 0));
    }
    if (character == '+') {
        return row == 3 ? 14U : (row == 1 || row == 2 || row == 4 ||
                                  row == 5 ? 4U : 0U);
    }
    if (character == '<') {
        return row == 1 || row == 5 ? 2U :
               (row == 2 || row == 4 ? 4U : row == 3 ? 8U : 0U);
    }
    if (character == '>') {
        return row == 1 || row == 5 ? 8U :
               (row == 2 || row == 4 ? 4U : row == 3 ? 2U : 0U);
    }
    if (character == '#') {
        return row == 2 || row == 4 ? 31U :
               (row > 0 && row < 6 ? 10U : 0U);
    }
    if (character == '%') {
        return row == 0 || row == 1 ? 17U : row == 2 ? 2U :
               row == 3 ? 4U : row == 4 ? 8U :
               (row == 5 || row == 6 ? 17U : 0U);
    }
    if (character == '\'') {
        return row < 2 ? 4U : 0U;
    }
    if (character == '(') {
        return row == 0 || row == 6 ? 2U :
               (row == 1 || row == 5 ? 4U : 8U);
    }
    if (character == ')') {
        return row == 0 || row == 6 ? 8U :
               (row == 1 || row == 5 ? 4U : 2U);
    }
    if (character == '[') {
        return row == 0 || row == 6 ? 14U : 8U;
    }
    if (character == ']') {
        return row == 0 || row == 6 ? 14U : 2U;
    }
    if (character == '&') {
        return row == 0 ? 12U : row == 1 ? 18U : row == 2 ? 20U :
               row == 3 ? 8U : row == 4 ? 21U : row == 5 ? 18U : 13U;
    }
    return 0U;
}

void vox_ui_fill(vox_ui_surface *surface, vox_u8 red, vox_u8 green,
                 vox_u8 blue)
{
    if (surface == 0) {
        return;
    }
    vox_ui_rect(surface, 0, 0, (int)surface->width, (int)surface->height,
                red, green, blue);
}

void vox_ui_rect(vox_ui_surface *surface, int x, int y, int width, int height,
                 vox_u8 red, vox_u8 green, vox_u8 blue)
{
    int end_x;
    int end_y;
    int pixel_x;
    int pixel_y;
    if (surface == 0 || surface->pixels == 0 || width <= 0 || height <= 0) {
        return;
    }
    end_x = x + width;
    end_y = y + height;
    if (end_x <= 0 || end_y <= 0 || x >= (int)surface->width ||
        y >= (int)surface->height) {
        return;
    }
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if (end_x > (int)surface->width) {
        end_x = (int)surface->width;
    }
    if (end_y > (int)surface->height) {
        end_y = (int)surface->height;
    }
    for (pixel_y = y; pixel_y < end_y; ++pixel_y) {
        vox_u8 *pixel = surface->pixels +
                        (vox_u32)pixel_y * surface->stride +
                        (vox_u32)x * 3U;
        for (pixel_x = x; pixel_x < end_x; ++pixel_x) {
            pixel[0] = red;
            pixel[1] = green;
            pixel[2] = blue;
            pixel += 3;
        }
    }
}

void vox_ui_frame(vox_ui_surface *surface, int x, int y, int width, int height,
                  vox_u8 red, vox_u8 green, vox_u8 blue)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    vox_ui_rect(surface, x, y, width, 1, red, green, blue);
    vox_ui_rect(surface, x, y + height - 1, width, 1, red, green, blue);
    vox_ui_rect(surface, x, y, 1, height, red, green, blue);
    vox_ui_rect(surface, x + width - 1, y, 1, height, red, green, blue);
}

int vox_ui_text_width(const char *text, int scale)
{
    int count = 0;
    if (text == 0 || scale <= 0) {
        return 0;
    }
    while (*text != '\0') {
        ++count;
        ++text;
    }
    return count == 0 ? 0 : count * 6 * scale - scale;
}

void vox_ui_text(vox_ui_surface *surface, int x, int y, int scale,
                 const char *text, vox_u8 red, vox_u8 green, vox_u8 blue)
{
    int origin_x = x;
    if (surface == 0 || text == 0 || scale <= 0) {
        return;
    }
    while (*text != '\0') {
        int row;
        int column;
        if (*text == '\n') {
            x = origin_x;
            y += 8 * scale;
            ++text;
            continue;
        }
        for (row = 0; row < 7; ++row) {
            vox_u8 bits = vox_ui_glyph_row(*text, row);
            for (column = 0; column < 5; ++column) {
                if (bits & (vox_u8)(1U << (4 - column))) {
                    vox_ui_rect(surface, x + column * scale,
                                y + row * scale, scale, scale,
                                red, green, blue);
                }
            }
        }
        x += 6 * scale;
        ++text;
    }
}

void vox_ui_text_center(vox_ui_surface *surface, int center_x, int y,
                        int scale, const char *text, vox_u8 red,
                        vox_u8 green, vox_u8 blue)
{
    vox_ui_text(surface, center_x - vox_ui_text_width(text, scale) / 2,
                y, scale, text, red, green, blue);
}

void vox_ui_text_shadow(vox_ui_surface *surface, int x, int y, int scale,
                        const char *text, vox_u8 red, vox_u8 green,
                        vox_u8 blue)
{
    int offset = scale > 1 ? 2 : 1;
    vox_ui_text(surface, x + offset, y + offset, scale, text, 0U, 0U, 0U);
    vox_ui_text(surface, x, y, scale, text, red, green, blue);
}

void vox_ui_text_center_shadow(vox_ui_surface *surface, int center_x, int y,
                               int scale, const char *text, vox_u8 red,
                               vox_u8 green, vox_u8 blue)
{
    vox_ui_text_shadow(surface,
                       center_x - vox_ui_text_width(text, scale) / 2,
                       y, scale, text, red, green, blue);
}

int vox_ui_text_wrap(vox_ui_surface *surface, int x, int y, int width,
                     int max_lines, int scale, const char *text,
                     vox_u8 red, vox_u8 green, vox_u8 blue)
{
    char line[96];
    int line_length;
    int line_count;
    int max_characters;
    const char *cursor;
    if (surface == 0 || text == 0 || width <= 0 || max_lines <= 0 ||
        scale <= 0) {
        return 0;
    }
    max_characters = (width + scale) / (6 * scale);
    if (max_characters < 1) {
        return 0;
    }
    if (max_characters > (int)sizeof(line) - 1) {
        max_characters = (int)sizeof(line) - 1;
    }
    cursor = text;
    line_count = 0;
    while (*cursor != '\0' && line_count < max_lines) {
        int candidate_length;
        int last_space;
        int consumed;
        line_length = 0;
        last_space = -1;
        while (cursor[line_length] != '\0' &&
               cursor[line_length] != '\n' &&
               line_length < max_characters) {
            if (cursor[line_length] == ' ') {
                last_space = line_length;
            }
            ++line_length;
        }
        candidate_length = line_length;
        if (cursor[line_length] != '\0' && cursor[line_length] != '\n' &&
            last_space > 0) {
            candidate_length = last_space;
        }
        while (candidate_length > 0 && cursor[candidate_length - 1] == ' ') {
            --candidate_length;
        }
        if (candidate_length > 0) {
            int index;
            for (index = 0; index < candidate_length; ++index) {
                line[index] = cursor[index];
            }
            line[candidate_length] = '\0';
            vox_ui_text(surface, x, y + line_count * 9 * scale, scale,
                        line, red, green, blue);
        }
        consumed = line_length;
        if (last_space > 0 && candidate_length == last_space) {
            consumed = last_space;
        }
        if (cursor[consumed] == '\n') {
            ++consumed;
        }
        while (cursor[consumed] == ' ') {
            ++consumed;
        }
        if (consumed <= 0) {
            consumed = 1;
        }
        cursor += consumed;
        ++line_count;
    }
    return line_count;
}
