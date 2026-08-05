/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DIGS_CHRONICLE_H
#define DIGS_CHRONICLE_H

#include "vox/vox_game.h"

/*
 * The chronicle: everything that survives closing the game.
 *
 * It holds the memory snapshot the miners walk in with, the messages they
 * have left for the player, and the running log of everything anyone has
 * said.  It lives next to settings.cfg in the pref path and is written the
 * same way settings are -- temporary file, then rename, with the old file
 * moved aside so a failed replacement restores rather than destroys.
 *
 * There is deliberately no way to erase it from inside the released game.
 * The history between the player and these three is meant to be theirs and
 * unrepeatable; a reset button would make it a save file instead.  Debug
 * builds get --chronicle-reset because refining the feature needs one.
 *
 * A file that fails its checksum is repaired to the last good copy and the
 * TAMPERED flag is raised, which the bots are told about in fiction rather
 * than the player being shown an error.
 */

#define DIGS_CHRONICLE_MAGIC "DIGSCHR"
#define DIGS_CHRONICLE_VERSION 1U
#define DIGS_INBOX_CAPACITY 8U
#define DIGS_INBOX_LINES 3U
#define DIGS_LOG_CAPACITY 128U

typedef struct digs_inbox_message {
    vox_u16 from;                       /* which identity left it */
    vox_u16 unread;
    vox_u32 launch;                     /* the launch it was written on */
    vox_u16 lines[DIGS_INBOX_LINES];    /* global line ids */
    vox_u16 line_count;
} digs_inbox_message;

typedef struct digs_log_entry {
    vox_u16 speaker;                    /* identity */
    vox_u16 target;                     /* identity, or IDENTITY_COUNT */
    vox_u16 line;                       /* global line id */
    vox_u16 match_index;
} digs_log_entry;

typedef struct digs_chronicle {
    vox_digs_bot_memory memory;
    digs_inbox_message inbox[DIGS_INBOX_CAPACITY];
    vox_u16 inbox_count;
    digs_log_entry log[DIGS_LOG_CAPACITY];
    vox_u16 log_count;                  /* entries held, up to capacity */
    vox_u16 log_cursor;                 /* where the next one goes */
    vox_u16 matches_recorded;
    vox_u16 tampered;
} digs_chronicle;

#ifdef __cplusplus
extern "C" {
#endif

/* A chronicle with no history in it at all. */
void digs_chronicle_reset(digs_chronicle *chronicle);

/*
 * Read the chronicle at `path`.  Returns 1 when a good file was read, 0 when
 * there was nothing readable -- in which case the chronicle is reset, and
 * `tampered` is set if a file existed but did not survive its checksum.
 */
int digs_chronicle_load(digs_chronicle *chronicle, const char *path);

/* Write atomically.  Returns 1 on success. */
int digs_chronicle_save(const digs_chronicle *chronicle, const char *path);

/* Append to the log ring, oldest falling off the end. */
void digs_chronicle_log(digs_chronicle *chronicle, vox_u16 speaker,
                        vox_u16 target, vox_u16 line);

/* Oldest-first index into the ring, for a reader that scrolls. */
const digs_log_entry *digs_chronicle_log_at(const digs_chronicle *chronicle,
                                            vox_u16 index);

/* How many messages are waiting.  This is the (N) beside INBOX. */
vox_u16 digs_chronicle_unread(const digs_chronicle *chronicle);

#ifdef __cplusplus
}
#endif

#endif
