/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DIGS_LINES_H
#define DIGS_LINES_H

#include "vox/vox_game.h"

/*
 * Everything the miners can say, and the index that finds it.
 *
 * This replaces the mad-lib bark generator, which assembled sentences from
 * fragments at runtime and is the documented source of the nonsense.  Nothing
 * is assembled here: every line is written out in full, and the only
 * substitution is %T for the name of whoever is being talked about.
 *
 * The quality filter is the index, not a runtime check.  A line can only be
 * spoken if somebody wrote one for that combination of speaker, mood and
 * situation, so an empty pool is a hole in the writing rather than a bug that
 * shows up as a blank speech bubble.
 */

#define DIGS_VOICE_RIVET 0U
#define DIGS_VOICE_CINDER 1U
#define DIGS_VOICE_FLAMEY 2U
#define DIGS_VOICE_MINER 3U      /* whoever is holding the controller */
#define DIGS_VOICE_COUNT 4U

/*
 * A run of lines in the flat table.  Pools are addressed by a global line id
 * so a pair can remember what it just said and not repeat it -- ids stay
 * stable for the life of the build, which is all the recent-line ring needs.
 */
typedef struct digs_line_pool {
    vox_u16 first;
    vox_u16 count;
} digs_line_pool;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Best pool for this combination, resolved through the fallback chain:
 * the exact (voice, tone, stimulus) cell first, then the voice's own take on
 * the stimulus, then whatever anyone would say about it.  Returns a pool with
 * count 0 only if nobody wrote anything at all for the stimulus.
 */
digs_line_pool digs_lines_pool(vox_u16 voice, vox_u16 tone, vox_u16 stimulus);

/* The line itself, by global id.  Never returns null. */
const char *digs_lines_text(vox_u16 id);

/* Non-zero when the line-id packing still fits: no set larger than the
 * stride, and no id past a vox_u16. */
int digs_lines_stride_is_sound(void);

/* How many lines exist in total; ids run from 0 to this minus one. */
vox_u16 digs_lines_total(void);

/*
 * Which voice speaks for a slot -- the bot's archetype, or the miner's own
 * voice for anyone holding a controller.
 */
vox_u16 digs_lines_voice_for(const vox_digs_match *match, vox_u16 player);

#ifdef __cplusplus
}
#endif

#endif
