/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vox/vox_script.h"

#include <stdio.h>
#include <string.h>

#define DIGS_MANIFEST "games/digs/scripts/manifest.txt"
#define DIGS_ENTRY_COUNT 87U

static vox_u32 tested_source_hash;
static vox_u32 tested_catalog_hash;

static int test_catalog_shape(const vox_script_catalog *catalog)
{
    const vox_script_entry *entry;
    const vox_script_value *value;

    if (catalog == 0 || catalog->schema_version != 1U ||
        catalog->entry_count != DIGS_ENTRY_COUNT ||
        catalog->visible_rows != VOX_SCRIPT_INDEX_VISIBLE_ROWS ||
        strcmp(catalog->game_id, "digs") != 0 ||
        strcmp(catalog->title, "DIGS MINER'S INDEX") != 0 ||
        catalog->kind_counts[VOX_SCRIPT_KIND_MATERIAL] != 14U ||
        catalog->kind_counts[VOX_SCRIPT_KIND_WEAPON] != 11U ||
        catalog->kind_counts[VOX_SCRIPT_KIND_ENTITY] != 9U ||
        catalog->kind_counts[VOX_SCRIPT_KIND_REACTION] != 12U ||
        catalog->kind_counts[VOX_SCRIPT_KIND_ANATOMY] != 15U ||
        catalog->kind_counts[VOX_SCRIPT_KIND_SYSTEM] != 11U ||
        catalog->kind_counts[VOX_SCRIPT_KIND_HOW_TO] != 1U ||
        catalog->kind_counts[VOX_SCRIPT_KIND_MODE] != 2U ||
        catalog->kind_counts[VOX_SCRIPT_KIND_FX] != 5U ||
        catalog->kind_counts[VOX_SCRIPT_KIND_AUDIO] != 7U) {
        return 1;
    }
    entry = vox_script_catalog_entry(catalog, 0U);
    if (entry == 0 || strcmp(entry->id, "how_to.play") != 0) {
        return 2;
    }
    entry = vox_script_catalog_find(catalog, "weapon.nail_bomb");
    if (entry == 0 || entry->kind != VOX_SCRIPT_KIND_WEAPON ||
        strcmp(entry->title, "BORE DRILL") != 0) {
        return 3;
    }
    value = vox_script_entry_value(entry, "damage");
    if (value == 0 || value->value != 100) {
        return 4;
    }
    entry = vox_script_catalog_find(catalog, "system.index");
    value = vox_script_entry_value(entry, "visible_rows");
    if (entry == 0 || value == 0 || value->value != 6) {
        return 5;
    }
    if (vox_script_catalog_find(catalog, "not.present") != 0 ||
        vox_script_catalog_entry(catalog, catalog->entry_count) != 0) {
        return 6;
    }
    return 0;
}

static int test_manifest_and_atomic_reload(void)
{
    vox_script_runtime runtime;
    vox_script_report validate_report;
    vox_script_report first_report;
    vox_script_report second_report;
    const vox_script_catalog *catalog;
    const vox_script_catalog *active_before_failure;
    vox_u32 generation;
    vox_u32 source_hash;
    vox_u32 catalog_hash;
    vox_result status;
    int shape_status;

    if (vox_script_runtime_init(&runtime, 0) != VOX_OK) {
        return 10;
    }
    status = vox_script_validate_manifest(&runtime, DIGS_MANIFEST,
                                          &validate_report);
    if (status != VOX_OK || validate_report.status != VOX_OK ||
        validate_report.entry_count != DIGS_ENTRY_COUNT ||
        validate_report.visible_rows != 6U || runtime.active != 0 ||
        runtime.reload_generation != 0U) {
        fprintf(stderr, "manifest validation: %s\n", validate_report.error);
        vox_script_runtime_shutdown(&runtime);
        return 11;
    }
    status = vox_script_reload_manifest(&runtime, DIGS_MANIFEST,
                                        &first_report);
    catalog = vox_script_catalog_get(&runtime);
    shape_status = test_catalog_shape(catalog);
    if (status != VOX_OK || first_report.status != VOX_OK ||
        runtime.reload_generation != 1U || shape_status != 0) {
        fprintf(stderr, "manifest reload: %s shape=%d\n",
                first_report.error, shape_status);
        vox_script_runtime_shutdown(&runtime);
        return 12;
    }
    generation = runtime.reload_generation;
    source_hash = runtime.active_source_hash;
    catalog_hash = runtime.active_catalog_hash;
    active_before_failure = catalog;
    if (source_hash == 0U || catalog_hash == 0U ||
        source_hash != validate_report.source_hash ||
        catalog_hash != validate_report.catalog_hash ||
        source_hash != catalog->source_hash ||
        catalog_hash != catalog->catalog_hash) {
        vox_script_runtime_shutdown(&runtime);
        return 13;
    }
    tested_source_hash = source_hash;
    tested_catalog_hash = catalog_hash;
    status = vox_script_reload_manifest(&runtime, DIGS_MANIFEST,
                                        &second_report);
    if (status != VOX_OK || runtime.reload_generation != generation + 1U ||
        second_report.source_hash != source_hash ||
        second_report.catalog_hash != catalog_hash ||
        test_catalog_shape(vox_script_catalog_get(&runtime)) != 0) {
        vox_script_runtime_shutdown(&runtime);
        return 14;
    }

    generation = runtime.reload_generation;
    active_before_failure = vox_script_catalog_get(&runtime);
    {
        static const char invalid_lua[] = "this is not valid Lua source";
        vox_script_source source;
        vox_script_report report;

        source.name = "invalid.lua";
        source.bytes = invalid_lua;
        source.byte_count = (vox_u32)(sizeof(invalid_lua) - 1U);
        status = vox_script_reload_sources(&runtime, &source, 1U, &report);
        if (status != VOX_SCRIPT_ERR_SYNTAX ||
            report.status != VOX_SCRIPT_ERR_SYNTAX ||
            report.error[0] == '\0') {
            vox_script_runtime_shutdown(&runtime);
            return 15;
        }
    }
    if (runtime.reload_generation != generation ||
        vox_script_catalog_get(&runtime) != active_before_failure ||
        runtime.active_source_hash != source_hash ||
        runtime.active_catalog_hash != catalog_hash) {
        vox_script_runtime_shutdown(&runtime);
        return 16;
    }
    vox_script_runtime_shutdown(&runtime);
    return 0;
}

static int test_sandbox_guards(void)
{
    static const char escape_attempt[] =
        "assert(io == nil and os == nil and package == nil and debug == nil)\n"
        "assert(dofile == nil and loadfile == nil and loadstring == nil)\n"
        "assert(require == nil and string.dump == nil and math.random == nil)\n"
        "assert(print == nil and pairs == nil and next == nil)\n"
        "assert(math.sin == nil and math.cos == nil and math.sqrt == nil)\n"
        "vox.configure{schema=1,game_id='guard',title='GUARD',visible_rows=6}\n"
        "vox.define('system','guard.entry',{order=1,title='GUARD',"
        "category='TEST',summary='guard',detail='guard'})\n";
    static const char infinite_loop[] = "while true do end\n";
    static const char embedded_nul[] = "return\0true";
    static const char bytecode[] = { 27, 'L', 'u', 'a' };
    static const char memory_bomb[] =
        "local huge = string.rep('x', 8000000)\n";
    static const char duplicate_id[] =
        "vox.configure{schema=1,game_id='dup',title='DUP',visible_rows=6}\n"
        "local e={order=1,title='A',category='T',summary='A',detail='A'}\n"
        "vox.define('system','same.id',e)\n"
        "vox.define('system','same.id',e)\n";
    vox_script_runtime runtime;
    vox_script_source source;
    vox_script_report report;
    vox_result status;

    if (vox_script_runtime_init(&runtime, 0) != VOX_OK) {
        return 20;
    }
    source.name = "guard.lua";
    source.bytes = escape_attempt;
    source.byte_count = (vox_u32)(sizeof(escape_attempt) - 1U);
    status = vox_script_reload_sources(&runtime, &source, 1U, &report);
    if (status != VOX_OK || report.entry_count != 1U ||
        vox_script_catalog_find(vox_script_catalog_get(&runtime),
                                "guard.entry") == 0) {
        fprintf(stderr, "sandbox guard: %s\n", report.error);
        vox_script_runtime_shutdown(&runtime);
        return 21;
    }

    source.name = "loop.lua";
    source.bytes = infinite_loop;
    source.byte_count = (vox_u32)(sizeof(infinite_loop) - 1U);
    status = vox_script_validate_sources(&runtime, &source, 1U, &report);
    if (status != VOX_SCRIPT_ERR_BUDGET || report.error[0] == '\0') {
        vox_script_runtime_shutdown(&runtime);
        return 22;
    }

    source.name = "nul.lua";
    source.bytes = embedded_nul;
    source.byte_count = (vox_u32)(sizeof(embedded_nul) - 1U);
    status = vox_script_validate_sources(&runtime, &source, 1U, &report);
    if (status != VOX_SCRIPT_ERR_SANDBOX) {
        vox_script_runtime_shutdown(&runtime);
        return 23;
    }

    source.name = "bytecode.lua";
    source.bytes = bytecode;
    source.byte_count = (vox_u32)sizeof(bytecode);
    status = vox_script_validate_sources(&runtime, &source, 1U, &report);
    if (status != VOX_SCRIPT_ERR_SANDBOX) {
        vox_script_runtime_shutdown(&runtime);
        return 24;
    }

    source.name = "memory.lua";
    source.bytes = memory_bomb;
    source.byte_count = (vox_u32)(sizeof(memory_bomb) - 1U);
    status = vox_script_validate_sources(&runtime, &source, 1U, &report);
    if (status != VOX_SCRIPT_ERR_MEMORY) {
        vox_script_runtime_shutdown(&runtime);
        return 25;
    }

    source.name = "duplicate.lua";
    source.bytes = duplicate_id;
    source.byte_count = (vox_u32)(sizeof(duplicate_id) - 1U);
    status = vox_script_validate_sources(&runtime, &source, 1U, &report);
    if (status != VOX_SCRIPT_ERR_RUNTIME || report.error[0] == '\0') {
        vox_script_runtime_shutdown(&runtime);
        return 26;
    }
    vox_script_runtime_shutdown(&runtime);
    return 0;
}

static int test_config_validation(void)
{
    vox_script_config config;
    vox_script_runtime runtime;

    vox_script_config_default(&config);
    if (config.abi_version != VOX_ABI_VERSION ||
        config.max_entries != VOX_SCRIPT_MAX_ENTRIES ||
        config.instruction_budget < config.hook_interval) {
        return 30;
    }
    config.max_entries = VOX_SCRIPT_MAX_ENTRIES + 1U;
    if (vox_script_runtime_init(&runtime, &config) != VOX_ERR_INVALID) {
        return 31;
    }
    if (vox_script_hash_bytes("hello", 5U) != 0x4f9f2cabU ||
        vox_script_hash_bytes(0, 1U) != 0U) {
        return 32;
    }
    return 0;
}

int main(void)
{
    int status;

    status = test_manifest_and_atomic_reload();
    if (status != 0) {
        fprintf(stderr, "script manifest/atomic test failed: %d\n", status);
        return status;
    }
    status = test_sandbox_guards();
    if (status != 0) {
        fprintf(stderr, "script sandbox test failed: %d\n", status);
        return status;
    }
    status = test_config_validation();
    if (status != 0) {
        fprintf(stderr, "script config/hash test failed: %d\n", status);
        return status;
    }
    printf("Lua 5.1 DIGS source=%08x catalog=%08x entries=%u\n",
           (unsigned int)tested_source_hash,
           (unsigned int)tested_catalog_hash,
           (unsigned int)DIGS_ENTRY_COUNT);
    return 0;
}
