/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <string.h>

#include "vox/vox_script.h"

#define DIGS_SCRIPT_DEFAULT "games/digs/scripts/manifest.txt"

static void digs_script_usage(const char *program)
{
    fprintf(stderr,
            "usage: %s [--validate|--hash|--headless] [manifest.txt]\n",
            program);
}

int main(int argc, char **argv)
{
    const char *command;
    const char *manifest;
    vox_script_runtime runtime;
    vox_script_report report;
    vox_result status;
    command = argc > 1 ? argv[1] : "--validate";
    manifest = argc > 2 ? argv[2] : DIGS_SCRIPT_DEFAULT;
    if (argc > 3 || (strcmp(command, "--validate") != 0 &&
                     strcmp(command, "--hash") != 0 &&
                     strcmp(command, "--headless") != 0)) {
        digs_script_usage(argv[0]);
        return 2;
    }
    status = vox_script_runtime_init(&runtime, 0);
    if (status != VOX_OK) {
        fprintf(stderr, "DIGS script runtime initialization failed (%u)\n",
                (unsigned int)status);
        return 3;
    }
    if (strcmp(command, "--validate") == 0) {
        status = vox_script_validate_manifest(&runtime, manifest, &report);
    } else {
        status = vox_script_reload_manifest(&runtime, manifest, &report);
    }
    if (status != VOX_OK) {
        fprintf(stderr, "%s\n", report.error[0] == '\0' ?
                vox_script_last_error(&runtime) : report.error);
        vox_script_runtime_shutdown(&runtime);
        return 4;
    }
    printf("DIGS Lua source=%08lx catalog=%08lx entries=%u rows=%u",
           (unsigned long)report.source_hash,
           (unsigned long)report.catalog_hash,
           (unsigned int)report.entry_count,
           (unsigned int)report.visible_rows);
    if (strcmp(command, "--headless") == 0) {
        const vox_script_catalog *catalog = vox_script_catalog_get(&runtime);
        vox_u16 index;
        printf(" game=%s\n", catalog == 0 ? "unavailable" :
                                      catalog->game_id);
        if (catalog != 0) {
            for (index = 0U; index < catalog->entry_count; ++index) {
                const vox_script_entry *entry =
                    vox_script_catalog_entry(catalog, index);
                if (entry != 0) {
                    printf("%03u %-18s %s\n", (unsigned int)index,
                           entry->id, entry->title);
                }
            }
        }
    } else {
        printf("\n");
    }
    vox_script_runtime_shutdown(&runtime);
    return 0;
}
