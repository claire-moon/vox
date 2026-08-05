/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "digs_chronicle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * The format is line-based text, like settings.cfg beside it.
 *
 * Binary would be smaller and would also make the file a hostage to struct
 * padding and byte order across the four platforms this ships on.  Text costs
 * a couple of kilobytes on disk and nothing in the payload, and it means a
 * corrupt chronicle can be looked at rather than guessed about.
 *
 * Every line before SUM is covered by the checksum.  SUM is last so the file
 * can be written in one pass.
 */

#define DIGS_CHRONICLE_LINE 256

static vox_u32 digs_chronicle_mix(vox_u32 hash, const char *text)
{
    size_t i;
    for (i = 0U; text[i] != '\0'; ++i) {
        hash ^= (vox_u32)(unsigned char)text[i];
        hash *= 16777619U;
    }
    return hash;
}

void digs_chronicle_reset(digs_chronicle *chronicle)
{
    vox_u16 i;
    if (chronicle == 0) {
        return;
    }
    vox_digs_memory_init(&chronicle->memory);
    for (i = 0U; i < DIGS_INBOX_CAPACITY; ++i) {
        vox_u16 j;
        chronicle->inbox[i].from = (vox_u16)VOX_DIGS_IDENTITY_COUNT;
        chronicle->inbox[i].unread = 0U;
        chronicle->inbox[i].launch = 0U;
        chronicle->inbox[i].line_count = 0U;
        for (j = 0U; j < DIGS_INBOX_LINES; ++j) {
            chronicle->inbox[i].lines[j] = 0U;
        }
    }
    chronicle->inbox_count = 0U;
    for (i = 0U; i < DIGS_LOG_CAPACITY; ++i) {
        chronicle->log[i].speaker = (vox_u16)VOX_DIGS_IDENTITY_COUNT;
        chronicle->log[i].target = (vox_u16)VOX_DIGS_IDENTITY_COUNT;
        chronicle->log[i].line = 0U;
        chronicle->log[i].match_index = 0U;
    }
    chronicle->log_count = 0U;
    chronicle->log_cursor = 0U;
    chronicle->matches_recorded = 0U;
    chronicle->tampered = 0U;
}

void digs_chronicle_log(digs_chronicle *chronicle, vox_u16 speaker,
                        vox_u16 target, vox_u16 line)
{
    digs_log_entry *entry;
    if (chronicle == 0 || chronicle->log_cursor >= DIGS_LOG_CAPACITY) {
        return;
    }
    entry = &chronicle->log[chronicle->log_cursor];
    entry->speaker = speaker;
    entry->target = target;
    entry->line = line;
    entry->match_index = chronicle->matches_recorded;
    chronicle->log_cursor =
        (vox_u16)((chronicle->log_cursor + 1U) % DIGS_LOG_CAPACITY);
    if (chronicle->log_count < DIGS_LOG_CAPACITY) {
        chronicle->log_count++;
    }
}

const digs_log_entry *digs_chronicle_log_at(const digs_chronicle *chronicle,
                                            vox_u16 index)
{
    vox_u16 oldest;
    if (chronicle == 0 || index >= chronicle->log_count) {
        return 0;
    }
    /* Once the ring has wrapped, the cursor sits on the oldest entry. */
    oldest = chronicle->log_count < DIGS_LOG_CAPACITY ? 0U :
             chronicle->log_cursor;
    return &chronicle->log[(oldest + index) % DIGS_LOG_CAPACITY];
}

vox_u16 digs_chronicle_unread(const digs_chronicle *chronicle)
{
    vox_u16 i;
    vox_u16 count = 0U;
    if (chronicle == 0) {
        return 0U;
    }
    for (i = 0U; i < chronicle->inbox_count && i < DIGS_INBOX_CAPACITY; ++i) {
        if (chronicle->inbox[i].unread) {
            count++;
        }
    }
    return count;
}

/* ---- writing --------------------------------------------------------- */

static int digs_chronicle_emit(FILE *file, vox_u32 *sum, const char *line)
{
    *sum = digs_chronicle_mix(*sum, line);
    return fputs(line, file) != EOF;
}

static int digs_chronicle_write_body(FILE *file, vox_u32 *sum,
                                     const digs_chronicle *chronicle)
{
    char line[DIGS_CHRONICLE_LINE];
    vox_u16 i;
    vox_u16 j;
    sprintf(line, "%s %u\n", DIGS_CHRONICLE_MAGIC,
            (unsigned int)DIGS_CHRONICLE_VERSION);
    if (!digs_chronicle_emit(file, sum, line)) return 0;
    sprintf(line, "LAUNCH %lu %lu %u %u\n",
            (unsigned long)chronicle->memory.launch_counter,
            (unsigned long)chronicle->memory.elapsed_coarse,
            (unsigned int)chronicle->matches_recorded,
            (unsigned int)chronicle->tampered);
    if (!digs_chronicle_emit(file, sum, line)) return 0;
    for (i = 0U; i < VOX_DIGS_IDENTITY_COUNT; ++i) {
        const vox_digs_identity_record *r = &chronicle->memory.identities[i];
        sprintf(line, "ID %u %u %u %u %u %u %u %u %u %u\n",
                (unsigned int)i,
                (unsigned int)r->traits.aggression,
                (unsigned int)r->traits.patience,
                (unsigned int)r->traits.caution,
                (unsigned int)r->traits.grudge,
                (unsigned int)r->traits.sociability,
                (unsigned int)r->matches_played,
                (unsigned int)r->wins,
                (unsigned int)r->kills,
                (unsigned int)r->deaths);
        if (!digs_chronicle_emit(file, sum, line)) return 0;
    }
    for (i = 0U; i < VOX_DIGS_MAX_PAIRS; ++i) {
        const vox_digs_regard *g = &chronicle->memory.regard[i];
        sprintf(line, "RG %u %u %d %u %u %u %u %u\n",
                (unsigned int)i, (unsigned int)g->tone, (int)g->valence,
                (unsigned int)g->matches_met, (unsigned int)g->kills_for,
                (unsigned int)g->kills_against, (unsigned int)g->truces,
                (unsigned int)g->betrayals);
        if (!digs_chronicle_emit(file, sum, line)) return 0;
    }
    for (i = 0U; i < chronicle->inbox_count && i < DIGS_INBOX_CAPACITY; ++i) {
        const digs_inbox_message *m = &chronicle->inbox[i];
        sprintf(line, "MSG %u %u %lu %u", (unsigned int)m->from,
                (unsigned int)m->unread, (unsigned long)m->launch,
                (unsigned int)m->line_count);
        if (!digs_chronicle_emit(file, sum, line)) return 0;
        for (j = 0U; j < m->line_count && j < DIGS_INBOX_LINES; ++j) {
            char part[32];
            sprintf(part, " %u", (unsigned int)m->lines[j]);
            if (!digs_chronicle_emit(file, sum, part)) return 0;
        }
        if (!digs_chronicle_emit(file, sum, "\n")) return 0;
    }
    for (i = 0U; i < chronicle->log_count; ++i) {
        const digs_log_entry *e = digs_chronicle_log_at(chronicle, i);
        if (e == 0) continue;
        sprintf(line, "LOG %u %u %u %u\n", (unsigned int)e->speaker,
                (unsigned int)e->target, (unsigned int)e->line,
                (unsigned int)e->match_index);
        if (!digs_chronicle_emit(file, sum, line)) return 0;
    }
    return 1;
}

int digs_chronicle_save(const digs_chronicle *chronicle, const char *path)
{
    char temporary[1032];
    char backup[1032];
    char line[DIGS_CHRONICLE_LINE];
    FILE *file;
    vox_u32 sum = 2166136261U;
    if (chronicle == 0 || path == 0 || strlen(path) + 5U >= sizeof(temporary)) {
        return 0;
    }
    sprintf(temporary, "%s.tmp", path);
    file = fopen(temporary, "w");
    if (file == 0) {
        return 0;
    }
    if (!digs_chronicle_write_body(file, &sum, chronicle)) {
        (void)fclose(file);
        (void)remove(temporary);
        return 0;
    }
    sprintf(line, "SUM %08lX\n", (unsigned long)sum);
    if (fputs(line, file) == EOF) {
        (void)fclose(file);
        (void)remove(temporary);
        return 0;
    }
    if (fclose(file) != 0) {
        (void)remove(temporary);
        return 0;
    }
    if (rename(temporary, path) != 0) {
        /*
         * ISO C rename replaces atomically on POSIX; older Windows CRTs do
         * not.  Move the old file aside so a failed replacement restores it
         * rather than leaving the player with no history at all.
         */
        if (strlen(path) + 5U >= sizeof(backup)) {
            (void)remove(temporary);
            return 0;
        }
        sprintf(backup, "%s.bak", path);
        (void)remove(backup);
        if (rename(path, backup) != 0 || rename(temporary, path) != 0) {
            (void)rename(backup, path);
            (void)remove(temporary);
            return 0;
        }
        (void)remove(backup);
    }
    return 1;
}

/* ---- reading --------------------------------------------------------- */

static int digs_chronicle_read_file(digs_chronicle *chronicle,
                                    const char *path)
{
    char line[DIGS_CHRONICLE_LINE];
    FILE *file = fopen(path, "r");
    vox_u32 sum = 2166136261U;
    vox_u32 stored = 0U;
    int have_sum = 0;
    int have_magic = 0;
    unsigned int version = 0U;
    if (file == 0) {
        return -1;                   /* nothing there at all */
    }
    digs_chronicle_reset(chronicle);
    while (fgets(line, (int)sizeof(line), file) != 0) {
        unsigned int a;
        unsigned int b;
        unsigned int c;
        unsigned int d;
        unsigned int e;
        unsigned int f;
        unsigned int g;
        unsigned int h;
        unsigned int i;
        unsigned int j;
        unsigned long l1;
        unsigned long l2;
        int signed_v;
        if (sscanf(line, "SUM %lX", &l1) == 1) {
            stored = (vox_u32)l1;
            have_sum = 1;
            break;                   /* SUM is last and is not itself summed */
        }
        sum = digs_chronicle_mix(sum, line);
        if (sscanf(line, DIGS_CHRONICLE_MAGIC " %u", &version) == 1) {
            have_magic = 1;
            continue;
        }
        if (sscanf(line, "LAUNCH %lu %lu %u %u", &l1, &l2, &a, &b) == 4) {
            chronicle->memory.launch_counter = (vox_u32)l1;
            chronicle->memory.elapsed_coarse = (vox_u32)l2;
            chronicle->matches_recorded = (vox_u16)a;
            chronicle->tampered = (vox_u16)(b != 0U ? 1U : 0U);
            continue;
        }
        if (sscanf(line, "ID %u %u %u %u %u %u %u %u %u %u",
                   &a, &b, &c, &d, &e, &f, &g, &h, &i, &j) == 10 &&
            a < VOX_DIGS_IDENTITY_COUNT) {
            vox_digs_identity_record *r = &chronicle->memory.identities[a];
            r->traits.aggression = (vox_u16)b;
            r->traits.patience = (vox_u16)c;
            r->traits.caution = (vox_u16)d;
            r->traits.grudge = (vox_u16)e;
            r->traits.sociability = (vox_u16)f;
            r->matches_played = (vox_u16)g;
            r->wins = (vox_u16)h;
            r->kills = (vox_u16)i;
            r->deaths = (vox_u16)j;
            continue;
        }
        if (sscanf(line, "RG %u %u %d %u %u %u %u %u",
                   &a, &b, &signed_v, &c, &d, &e, &f, &g) == 8 &&
            a < VOX_DIGS_MAX_PAIRS) {
            vox_digs_regard *rg = &chronicle->memory.regard[a];
            rg->tone = (vox_u16)(b < VOX_DIGS_TONE_COUNT ?
                                 b : (unsigned int)VOX_DIGS_TONE_NEUTRAL);
            rg->valence = (vox_i16)signed_v;
            rg->matches_met = (vox_u16)c;
            rg->kills_for = (vox_u16)d;
            rg->kills_against = (vox_u16)e;
            rg->truces = (vox_u16)f;
            rg->betrayals = (vox_u16)g;
            continue;
        }
        if (sscanf(line, "MSG %u %u %lu %u %u %u %u",
                   &a, &b, &l1, &c, &d, &e, &f) >= 4 &&
            chronicle->inbox_count < DIGS_INBOX_CAPACITY) {
            digs_inbox_message *m =
                &chronicle->inbox[chronicle->inbox_count];
            unsigned int parts[DIGS_INBOX_LINES];
            int got;
            m->from = (vox_u16)a;
            m->unread = (vox_u16)(b != 0U ? 1U : 0U);
            m->launch = (vox_u32)l1;
            got = sscanf(line, "MSG %u %u %lu %u %u %u %u", &a, &b, &l1, &c,
                         &parts[0], &parts[1], &parts[2]) - 4;
            if (got < 0) got = 0;
            if ((unsigned int)got > DIGS_INBOX_LINES) {
                got = (int)DIGS_INBOX_LINES;
            }
            m->line_count = (vox_u16)got;
            for (a = 0U; a < (unsigned int)got; ++a) {
                m->lines[a] = (vox_u16)parts[a];
            }
            chronicle->inbox_count++;
            continue;
        }
        if (sscanf(line, "LOG %u %u %u %u", &a, &b, &c, &d) == 4) {
            chronicle->log[chronicle->log_cursor].speaker = (vox_u16)a;
            chronicle->log[chronicle->log_cursor].target = (vox_u16)b;
            chronicle->log[chronicle->log_cursor].line = (vox_u16)c;
            chronicle->log[chronicle->log_cursor].match_index = (vox_u16)d;
            chronicle->log_cursor =
                (vox_u16)((chronicle->log_cursor + 1U) % DIGS_LOG_CAPACITY);
            if (chronicle->log_count < DIGS_LOG_CAPACITY) {
                chronicle->log_count++;
            }
            continue;
        }
    }
    (void)fclose(file);
    if (!have_magic || version != DIGS_CHRONICLE_VERSION || !have_sum ||
        stored != sum) {
        return 0;                    /* present but not trustworthy */
    }
    (void)vox_digs_memory_hash(&chronicle->memory);
    return 1;
}

int digs_chronicle_load(digs_chronicle *chronicle, const char *path)
{
    char backup[1032];
    int result;
    if (chronicle == 0 || path == 0) {
        return 0;
    }
    result = digs_chronicle_read_file(chronicle, path);
    if (result == 1) {
        return 1;
    }
    if (result == 0 && strlen(path) + 5U < sizeof(backup)) {
        /*
         * The file was there and did not survive its checksum.  Fall back to
         * the last good copy if one exists, and record that it happened --
         * the bots are told about it in fiction rather than the player being
         * shown an error dialog about their own save.
         */
        sprintf(backup, "%s.bak", path);
        if (digs_chronicle_read_file(chronicle, backup) == 1) {
            chronicle->tampered = 1U;
            return 1;
        }
    }
    digs_chronicle_reset(chronicle);
    chronicle->tampered = (vox_u16)(result == 0 ? 1U : 0U);
    return 0;
}
