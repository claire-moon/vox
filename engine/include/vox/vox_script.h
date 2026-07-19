/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VOX_SCRIPT_H
#define VOX_SCRIPT_H

#include "vox_types.h"

#define VOX_SCRIPT_SCHEMA_VERSION 1U
#define VOX_SCRIPT_INDEX_VISIBLE_ROWS 6U
#define VOX_SCRIPT_MAX_ENTRIES 128U
#define VOX_SCRIPT_MAX_VALUES 16U
#define VOX_SCRIPT_MAX_SOURCES 32U
#define VOX_SCRIPT_ID_CAPACITY 40U
#define VOX_SCRIPT_TITLE_CAPACITY 64U
#define VOX_SCRIPT_CATEGORY_CAPACITY 32U
#define VOX_SCRIPT_SUMMARY_CAPACITY 192U
#define VOX_SCRIPT_DETAIL_CAPACITY 768U
#define VOX_SCRIPT_TAGS_CAPACITY 128U
#define VOX_SCRIPT_VALUE_KEY_CAPACITY 32U
#define VOX_SCRIPT_GAME_ID_CAPACITY 32U
#define VOX_SCRIPT_ERROR_CAPACITY 256U

#define VOX_SCRIPT_ERR_STATE 256U
#define VOX_SCRIPT_ERR_IO 257U
#define VOX_SCRIPT_ERR_SYNTAX 258U
#define VOX_SCRIPT_ERR_RUNTIME 259U
#define VOX_SCRIPT_ERR_SANDBOX 260U
#define VOX_SCRIPT_ERR_SCHEMA 261U
#define VOX_SCRIPT_ERR_MEMORY 262U
#define VOX_SCRIPT_ERR_BUDGET 263U

typedef enum vox_script_entry_kind {
    VOX_SCRIPT_KIND_MATERIAL = 0,
    VOX_SCRIPT_KIND_WEAPON = 1,
    VOX_SCRIPT_KIND_ENTITY = 2,
    VOX_SCRIPT_KIND_REACTION = 3,
    VOX_SCRIPT_KIND_ANATOMY = 4,
    VOX_SCRIPT_KIND_SYSTEM = 5,
    VOX_SCRIPT_KIND_HOW_TO = 6,
    VOX_SCRIPT_KIND_MODE = 7,
    VOX_SCRIPT_KIND_FX = 8,
    VOX_SCRIPT_KIND_AUDIO = 9,
    VOX_SCRIPT_KIND_COUNT = 10
} vox_script_entry_kind;

typedef struct vox_script_value {
    char key[VOX_SCRIPT_VALUE_KEY_CAPACITY];
    vox_i32 value;
} vox_script_value;

typedef struct vox_script_entry {
    vox_u16 kind;
    vox_u16 order;
    vox_u16 value_count;
    vox_u16 flags;
    char id[VOX_SCRIPT_ID_CAPACITY];
    char title[VOX_SCRIPT_TITLE_CAPACITY];
    char category[VOX_SCRIPT_CATEGORY_CAPACITY];
    char summary[VOX_SCRIPT_SUMMARY_CAPACITY];
    char detail[VOX_SCRIPT_DETAIL_CAPACITY];
    char tags[VOX_SCRIPT_TAGS_CAPACITY];
    vox_script_value values[VOX_SCRIPT_MAX_VALUES];
} vox_script_entry;

typedef struct vox_script_catalog {
    vox_u32 abi_version;
    vox_u32 struct_size;
    vox_u32 schema_version;
    vox_u32 source_hash;
    vox_u32 catalog_hash;
    vox_u16 entry_count;
    vox_u16 visible_rows;
    vox_u16 kind_counts[VOX_SCRIPT_KIND_COUNT];
    vox_u16 reserved;
    char game_id[VOX_SCRIPT_GAME_ID_CAPACITY];
    char title[VOX_SCRIPT_TITLE_CAPACITY];
    vox_script_entry entries[VOX_SCRIPT_MAX_ENTRIES];
} vox_script_catalog;

typedef struct vox_script_config {
    vox_u32 abi_version;
    vox_u32 struct_size;
    vox_u32 memory_limit_bytes;
    vox_u32 instruction_budget;
    vox_u32 hook_interval;
    vox_u32 max_source_bytes;
    vox_u16 max_entries;
    vox_u16 reserved;
} vox_script_config;

typedef struct vox_script_source {
    const char *name;
    const char *bytes;
    vox_u32 byte_count;
} vox_script_source;

typedef struct vox_script_report {
    vox_u32 abi_version;
    vox_u32 struct_size;
    vox_result status;
    vox_u32 source_hash;
    vox_u32 catalog_hash;
    vox_u16 entry_count;
    vox_u16 visible_rows;
    char error[VOX_SCRIPT_ERROR_CAPACITY];
} vox_script_report;

/*
 * The active implementation is opaque so a failed reload can be discarded
 * without disturbing the currently running game.  Callers may stack-allocate
 * this small handle; all Lua and catalog memory is owned by the runtime.
 */
typedef struct vox_script_runtime {
    vox_u32 abi_version;
    vox_u32 struct_size;
    vox_script_config config;
    void *active;
    vox_u32 reload_generation;
    vox_u32 active_source_hash;
    vox_u32 active_catalog_hash;
    char last_error[VOX_SCRIPT_ERROR_CAPACITY];
} vox_script_runtime;

#ifdef __cplusplus
extern "C" {
#endif

void vox_script_config_default(vox_script_config *config);
vox_result vox_script_runtime_init(vox_script_runtime *runtime,
                                   const vox_script_config *config);
void vox_script_runtime_shutdown(vox_script_runtime *runtime);

vox_result vox_script_validate_sources(vox_script_runtime *runtime,
                                       const vox_script_source *sources,
                                       vox_u16 source_count,
                                       vox_script_report *report);
vox_result vox_script_reload_sources(vox_script_runtime *runtime,
                                     const vox_script_source *sources,
                                     vox_u16 source_count,
                                     vox_script_report *report);
vox_result vox_script_validate_manifest(vox_script_runtime *runtime,
                                        const char *manifest_path,
                                        vox_script_report *report);
vox_result vox_script_reload_manifest(vox_script_runtime *runtime,
                                      const char *manifest_path,
                                      vox_script_report *report);

const vox_script_catalog *vox_script_catalog_get(
                                      const vox_script_runtime *runtime);
const vox_script_entry *vox_script_catalog_entry(
                                      const vox_script_catalog *catalog,
                                      vox_u16 ordinal);
const vox_script_entry *vox_script_catalog_find(
                                      const vox_script_catalog *catalog,
                                      const char *id);
const vox_script_value *vox_script_entry_value(
                                      const vox_script_entry *entry,
                                      const char *key);
const char *vox_script_last_error(const vox_script_runtime *runtime);
vox_u32 vox_script_hash_bytes(const void *bytes, vox_u32 byte_count);

#ifdef __cplusplus
}
#endif

#endif
