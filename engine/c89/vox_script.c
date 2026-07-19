/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vox/vox_script.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#define VOX_SCRIPT_FNV_OFFSET 2166136261U
#define VOX_SCRIPT_FNV_PRIME 16777619U
#define VOX_SCRIPT_DEFAULT_MEMORY (4U * 1024U * 1024U)
#define VOX_SCRIPT_DEFAULT_BUDGET 500000U
#define VOX_SCRIPT_DEFAULT_HOOK_INTERVAL 1000U
#define VOX_SCRIPT_DEFAULT_SOURCE_LIMIT (512U * 1024U)
#define VOX_SCRIPT_PATH_CAPACITY 512U

typedef struct vox_script_impl {
    lua_State *state;
    vox_script_config config;
    vox_script_catalog catalog;
    size_t lua_bytes;
    vox_u32 source_bytes;
    vox_u32 source_hash;
    vox_u32 instructions_left;
    int configured;
    int budget_exhausted;
    int memory_failed;
    char error[VOX_SCRIPT_ERROR_CAPACITY];
} vox_script_impl;

static char vox_script_registry_key;

static vox_u32 script_hash_update(vox_u32 hash, const void *bytes,
                                  vox_u32 byte_count)
{
    const vox_u8 *data;
    vox_u32 i;

    if (bytes == 0 && byte_count != 0U) {
        return 0U;
    }
    data = (const vox_u8 *)bytes;
    for (i = 0U; i < byte_count; ++i) {
        hash ^= (vox_u32)data[i];
        hash *= VOX_SCRIPT_FNV_PRIME;
    }
    return hash;
}

vox_u32 vox_script_hash_bytes(const void *bytes, vox_u32 byte_count)
{
    return script_hash_update(VOX_SCRIPT_FNV_OFFSET, bytes, byte_count);
}

static vox_u32 script_hash_string(vox_u32 hash, const char *text)
{
    vox_u8 terminator;

    if (text == 0) {
        return 0U;
    }
    hash = script_hash_update(hash, text, (vox_u32)strlen(text));
    terminator = 0U;
    return script_hash_update(hash, &terminator, 1U);
}

static vox_u32 script_hash_u32(vox_u32 hash, vox_u32 value)
{
    vox_u8 bytes[4];

    bytes[0] = (vox_u8)(value & 255U);
    bytes[1] = (vox_u8)((value >> 8) & 255U);
    bytes[2] = (vox_u8)((value >> 16) & 255U);
    bytes[3] = (vox_u8)((value >> 24) & 255U);
    return script_hash_update(hash, bytes, 4U);
}

static void script_copy_text(char *target, vox_u32 capacity,
                             const char *source)
{
    vox_u32 length;

    if (target == 0 || capacity == 0U) {
        return;
    }
    if (source == 0) {
        target[0] = '\0';
        return;
    }
    length = (vox_u32)strlen(source);
    if (length >= capacity) {
        length = capacity - 1U;
    }
    if (length != 0U) {
        memcpy(target, source, (size_t)length);
    }
    target[length] = '\0';
}

static void script_set_error(vox_script_impl *impl, const char *message)
{
    if (impl != 0) {
        script_copy_text(impl->error, VOX_SCRIPT_ERROR_CAPACITY, message);
    }
}

static void *script_allocator(void *user, void *pointer, size_t old_size,
                              size_t new_size)
{
    vox_script_impl *impl;
    void *replacement;
    size_t reduced;

    impl = (vox_script_impl *)user;
    if (new_size == 0U) {
        if (pointer != 0) {
            free(pointer);
        }
        if (old_size <= impl->lua_bytes) {
            impl->lua_bytes -= old_size;
        } else {
            impl->lua_bytes = 0U;
        }
        return 0;
    }

    reduced = impl->lua_bytes;
    if (old_size <= reduced) {
        reduced -= old_size;
    } else {
        reduced = 0U;
    }
    if (reduced > (size_t)impl->config.memory_limit_bytes ||
        new_size > (size_t)impl->config.memory_limit_bytes - reduced) {
        impl->memory_failed = 1;
        return 0;
    }
    replacement = realloc(pointer, new_size);
    if (replacement == 0) {
        impl->memory_failed = 1;
        return 0;
    }
    impl->lua_bytes = reduced + new_size;
    return replacement;
}

static vox_script_impl *script_impl_from_state(lua_State *state)
{
    vox_script_impl *impl;

    lua_pushlightuserdata(state, (void *)&vox_script_registry_key);
    lua_rawget(state, LUA_REGISTRYINDEX);
    impl = (vox_script_impl *)lua_touserdata(state, -1);
    lua_pop(state, 1);
    return impl;
}

static void script_instruction_hook(lua_State *state, lua_Debug *debug_info)
{
    vox_script_impl *impl;

    (void)debug_info;
    impl = script_impl_from_state(state);
    if (impl == 0) {
        luaL_error(state, "VOX script runtime state is unavailable");
        return;
    }
    if (impl->instructions_left <= impl->config.hook_interval) {
        impl->instructions_left = 0U;
        impl->budget_exhausted = 1;
        luaL_error(state, "VOX script instruction budget exhausted");
        return;
    }
    impl->instructions_left -= impl->config.hook_interval;
}

static int script_panic(lua_State *state)
{
    vox_script_impl *impl;
    const char *message;

    impl = script_impl_from_state(state);
    message = lua_tostring(state, -1);
    if (impl != 0) {
        script_set_error(impl, message != 0 ? message : "Lua panic");
    }
    return 0;
}

static int script_open_library(vox_script_impl *impl, const char *name,
                               lua_CFunction open_function)
{
    lua_State *state;

    state = impl->state;
    lua_pushcfunction(state, open_function);
    lua_pushstring(state, name);
    if (lua_pcall(state, 1, 0, 0) != 0) {
        script_set_error(impl, lua_tostring(state, -1));
        lua_pop(state, 1);
        return 0;
    }
    return 1;
}

static void script_remove_global(lua_State *state, const char *name)
{
    lua_pushnil(state);
    lua_setglobal(state, name);
}

static void script_remove_field(lua_State *state, const char *table_name,
                                const char *field_name)
{
    lua_getglobal(state, table_name);
    if (lua_istable(state, -1)) {
        lua_pushnil(state);
        lua_setfield(state, -2, field_name);
    }
    lua_pop(state, 1);
}

static int script_kind_from_name(const char *name, vox_u16 *kind_out)
{
    static const char *names[VOX_SCRIPT_KIND_COUNT] = {
        "material", "weapon", "entity", "reaction", "anatomy",
        "system", "how_to", "mode", "fx", "audio"
    };
    vox_u16 kind;

    for (kind = 0U; kind < VOX_SCRIPT_KIND_COUNT; ++kind) {
        if (strcmp(name, names[kind]) == 0) {
            *kind_out = kind;
            return 1;
        }
    }
    return 0;
}

static int script_id_is_valid(const char *id)
{
    const unsigned char *cursor;

    if (id == 0 || id[0] == '\0') {
        return 0;
    }
    cursor = (const unsigned char *)id;
    while (*cursor != 0U) {
        if (!(islower((int)*cursor) || isdigit((int)*cursor) ||
              *cursor == (unsigned char)'_' ||
              *cursor == (unsigned char)'.' ||
              *cursor == (unsigned char)'-')) {
            return 0;
        }
        ++cursor;
    }
    return 1;
}

static int script_copy_argument(lua_State *state, int index, char *target,
                                vox_u32 capacity)
{
    const char *value;
    size_t length;

    if (lua_type(state, index) != LUA_TSTRING) {
        return 0;
    }
    value = lua_tolstring(state, index, &length);
    if (value == 0 || length == 0U || length >= (size_t)capacity) {
        return 0;
    }
    memcpy(target, value, length);
    target[length] = '\0';
    return 1;
}

static int script_copy_table_string(lua_State *state, int table_index,
                                    const char *field, char *target,
                                    vox_u32 capacity, int required)
{
    const char *value;
    size_t length;
    int absolute_index;

    absolute_index = table_index;
    if (table_index < 0) {
        absolute_index = lua_gettop(state) + table_index + 1;
    }
    lua_getfield(state, absolute_index, field);
    if (lua_isnil(state, -1) && !required) {
        target[0] = '\0';
        lua_pop(state, 1);
        return 1;
    }
    if (lua_type(state, -1) != LUA_TSTRING) {
        lua_pop(state, 1);
        return 0;
    }
    value = lua_tolstring(state, -1, &length);
    if (value == 0 || (required && length == 0U) ||
        length >= (size_t)capacity) {
        lua_pop(state, 1);
        return 0;
    }
    if (length != 0U) {
        memcpy(target, value, length);
    }
    target[length] = '\0';
    lua_pop(state, 1);
    return 1;
}

static int script_number_to_i32(lua_Number number, vox_i32 *value_out)
{
    vox_i32 value;

    if (number < (lua_Number)(-2147483647L - 1L) ||
        number > (lua_Number)2147483647L) {
        return 0;
    }
    value = (vox_i32)number;
    if ((lua_Number)value != number) {
        return 0;
    }
    *value_out = value;
    return 1;
}

static int script_table_integer(lua_State *state, int table_index,
                                const char *field, vox_i32 minimum,
                                vox_i32 maximum, vox_i32 fallback,
                                vox_i32 *value_out)
{
    int absolute_index;
    vox_i32 value;

    absolute_index = table_index;
    if (table_index < 0) {
        absolute_index = lua_gettop(state) + table_index + 1;
    }
    lua_getfield(state, absolute_index, field);
    if (lua_isnil(state, -1)) {
        *value_out = fallback;
        lua_pop(state, 1);
        return 1;
    }
    if (lua_type(state, -1) != LUA_TNUMBER ||
        !script_number_to_i32(lua_tonumber(state, -1), &value) ||
        value < minimum || value > maximum) {
        lua_pop(state, 1);
        return 0;
    }
    *value_out = value;
    lua_pop(state, 1);
    return 1;
}

static int script_field_allowed(const char *key, const char *const *allowed,
                                vox_u16 allowed_count)
{
    vox_u16 index;

    for (index = 0U; index < allowed_count; ++index) {
        if (strcmp(key, allowed[index]) == 0) {
            return 1;
        }
    }
    return 0;
}

static int script_validate_table_fields(lua_State *state, int table_index,
                                        const char *const *allowed,
                                        vox_u16 allowed_count)
{
    int absolute_index;
    const char *key;

    absolute_index = table_index;
    if (table_index < 0) {
        absolute_index = lua_gettop(state) + table_index + 1;
    }
    lua_pushnil(state);
    while (lua_next(state, absolute_index) != 0) {
        if (lua_type(state, -2) != LUA_TSTRING) {
            lua_pop(state, 2);
            return 0;
        }
        key = lua_tostring(state, -2);
        if (key == 0 || !script_field_allowed(key, allowed, allowed_count)) {
            lua_pop(state, 2);
            return 0;
        }
        lua_pop(state, 1);
    }
    return 1;
}

static int script_compare_values(const vox_script_value *left,
                                 const vox_script_value *right)
{
    return strcmp(left->key, right->key);
}

static void script_sort_values(vox_script_entry *entry)
{
    vox_u16 outer;

    for (outer = 1U; outer < entry->value_count; ++outer) {
        vox_script_value moving;
        vox_u16 inner;

        moving = entry->values[outer];
        inner = outer;
        while (inner > 0U &&
               script_compare_values(&moving, &entry->values[inner - 1U]) < 0) {
            entry->values[inner] = entry->values[inner - 1U];
            --inner;
        }
        entry->values[inner] = moving;
    }
}

static int script_read_values(lua_State *state, int table_index,
                              vox_script_entry *entry)
{
    int absolute_index;
    const char *key;
    size_t key_length;
    vox_i32 value;

    absolute_index = table_index;
    if (table_index < 0) {
        absolute_index = lua_gettop(state) + table_index + 1;
    }
    lua_getfield(state, absolute_index, "values");
    if (lua_isnil(state, -1)) {
        lua_pop(state, 1);
        return 1;
    }
    if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        return 0;
    }
    lua_pushnil(state);
    while (lua_next(state, -2) != 0) {
        if (entry->value_count >= VOX_SCRIPT_MAX_VALUES ||
            lua_type(state, -2) != LUA_TSTRING ||
            lua_type(state, -1) != LUA_TNUMBER ||
            !script_number_to_i32(lua_tonumber(state, -1), &value)) {
            lua_pop(state, 3);
            return 0;
        }
        key = lua_tolstring(state, -2, &key_length);
        if (key == 0 || key_length == 0U ||
            key_length >= VOX_SCRIPT_VALUE_KEY_CAPACITY ||
            !script_id_is_valid(key)) {
            lua_pop(state, 3);
            return 0;
        }
        memcpy(entry->values[entry->value_count].key, key, key_length);
        entry->values[entry->value_count].key[key_length] = '\0';
        entry->values[entry->value_count].value = value;
        ++entry->value_count;
        lua_pop(state, 1);
    }
    lua_pop(state, 1);
    script_sort_values(entry);
    return 1;
}

static int script_entry_id_exists(const vox_script_catalog *catalog,
                                  const char *id)
{
    vox_u16 index;

    for (index = 0U; index < catalog->entry_count; ++index) {
        if (strcmp(catalog->entries[index].id, id) == 0) {
            return 1;
        }
    }
    return 0;
}

static int script_lua_configure(lua_State *state)
{
    static const char *const allowed[] = {
        "schema", "game_id", "title", "visible_rows"
    };
    vox_script_impl *impl;
    vox_i32 schema;
    vox_i32 visible_rows;

    impl = script_impl_from_state(state);
    if (impl == 0 || impl->configured) {
        return luaL_error(state, "vox.configure may be called exactly once");
    }
    if (!lua_istable(state, 1) ||
        !script_validate_table_fields(state, 1, allowed, 4U) ||
        !script_table_integer(state, 1, "schema", 1, 1, -1, &schema) ||
        schema != (vox_i32)VOX_SCRIPT_SCHEMA_VERSION ||
        !script_table_integer(state, 1, "visible_rows", 1, 12,
                              (vox_i32)VOX_SCRIPT_INDEX_VISIBLE_ROWS,
                              &visible_rows) ||
        !script_copy_table_string(state, 1, "game_id",
                                  impl->catalog.game_id,
                                  VOX_SCRIPT_GAME_ID_CAPACITY, 1) ||
        !script_id_is_valid(impl->catalog.game_id) ||
        !script_copy_table_string(state, 1, "title", impl->catalog.title,
                                  VOX_SCRIPT_TITLE_CAPACITY, 1)) {
        return luaL_error(state, "invalid vox.configure schema");
    }
    impl->catalog.schema_version = (vox_u32)schema;
    impl->catalog.visible_rows = (vox_u16)visible_rows;
    impl->configured = 1;
    return 0;
}

static int script_lua_define(lua_State *state)
{
    static const char *const allowed[] = {
        "order", "title", "category", "summary", "detail", "tags",
        "flags", "values"
    };
    vox_script_impl *impl;
    vox_script_entry *entry;
    const char *kind_name;
    vox_u16 kind;
    vox_i32 order;
    vox_i32 flags;

    impl = script_impl_from_state(state);
    if (impl == 0 || !impl->configured) {
        return luaL_error(state, "vox.configure must precede vox.define");
    }
    if (lua_type(state, 1) != LUA_TSTRING ||
        !script_kind_from_name(lua_tostring(state, 1), &kind) ||
        !lua_istable(state, 3)) {
        return luaL_error(state, "vox.define expects kind, id, and table");
    }
    kind_name = lua_tostring(state, 1);
    (void)kind_name;
    if (impl->catalog.entry_count >= impl->config.max_entries) {
        return luaL_error(state, "VOX catalog entry capacity exceeded");
    }
    entry = &impl->catalog.entries[impl->catalog.entry_count];
    memset(entry, 0, sizeof(*entry));
    if (!script_copy_argument(state, 2, entry->id,
                              VOX_SCRIPT_ID_CAPACITY) ||
        !script_id_is_valid(entry->id) ||
        script_entry_id_exists(&impl->catalog, entry->id) ||
        !script_validate_table_fields(state, 3, allowed, 8U) ||
        !script_table_integer(state, 3, "order", 0, 65535, -1, &order) ||
        order < 0 ||
        !script_table_integer(state, 3, "flags", 0, 65535, 0, &flags) ||
        !script_copy_table_string(state, 3, "title", entry->title,
                                  VOX_SCRIPT_TITLE_CAPACITY, 1) ||
        !script_copy_table_string(state, 3, "category", entry->category,
                                  VOX_SCRIPT_CATEGORY_CAPACITY, 1) ||
        !script_copy_table_string(state, 3, "summary", entry->summary,
                                  VOX_SCRIPT_SUMMARY_CAPACITY, 1) ||
        !script_copy_table_string(state, 3, "detail", entry->detail,
                                  VOX_SCRIPT_DETAIL_CAPACITY, 1) ||
        !script_copy_table_string(state, 3, "tags", entry->tags,
                                  VOX_SCRIPT_TAGS_CAPACITY, 0) ||
        !script_read_values(state, 3, entry)) {
        return luaL_error(state, "invalid or duplicate VOX catalog entry");
    }
    entry->kind = kind;
    entry->order = (vox_u16)order;
    entry->flags = (vox_u16)flags;
    ++impl->catalog.entry_count;
    return 0;
}

static int script_lua_hash32(lua_State *state)
{
    const char *bytes;
    size_t byte_count;
    vox_u32 hash;

    bytes = luaL_checklstring(state, 1, &byte_count);
    if (byte_count > 0xffffffffUL) {
        return luaL_error(state, "vox.hash32 input is too large");
    }
    hash = vox_script_hash_bytes(bytes, (vox_u32)byte_count);
    lua_pushnumber(state, (lua_Number)hash);
    return 1;
}

static int script_lua_random(lua_State *state)
{
    lua_Number seed_number;
    lua_Number bound_number;
    vox_u32 seed;
    vox_u32 bound;

    seed_number = luaL_checknumber(state, 1);
    bound_number = luaL_checknumber(state, 2);
    if (seed_number < 0.0 || seed_number > 4294967295.0 ||
        bound_number < 1.0 || bound_number > 1000000.0) {
        return luaL_error(state, "vox.random arguments are out of range");
    }
    seed = (vox_u32)seed_number;
    bound = (vox_u32)bound_number;
    if ((lua_Number)seed != seed_number || (lua_Number)bound != bound_number) {
        return luaL_error(state, "vox.random arguments must be integers");
    }
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    lua_pushnumber(state, (lua_Number)(seed % bound));
    return 1;
}

static int script_register_api(vox_script_impl *impl)
{
    lua_State *state;

    state = impl->state;
    lua_newtable(state);
    lua_pushlightuserdata(state, (void *)impl);
    lua_pushcclosure(state, script_lua_configure, 1);
    lua_setfield(state, -2, "configure");
    lua_pushlightuserdata(state, (void *)impl);
    lua_pushcclosure(state, script_lua_define, 1);
    lua_setfield(state, -2, "define");
    lua_pushcfunction(state, script_lua_hash32);
    lua_setfield(state, -2, "hash32");
    lua_pushcfunction(state, script_lua_random);
    lua_setfield(state, -2, "random");
    lua_pushnumber(state, (lua_Number)VOX_SCRIPT_SCHEMA_VERSION);
    lua_setfield(state, -2, "api_version");
    lua_setglobal(state, "vox");
    return 1;
}

static int script_create_sandbox(vox_script_impl *impl)
{
    static const char *const unsafe_globals[] = {
        "io", "os", "package", "debug", "coroutine", "dofile",
        "loadfile", "load", "loadstring", "require", "module",
        "collectgarbage", "getfenv", "setfenv", "print", "gcinfo",
        "newproxy", "pairs", "next", "tostring", "tonumber"
    };
    static const char *const unstable_math[] = {
        "acos", "asin", "atan", "atan2", "cos", "cosh", "exp", "log",
        "log10", "pow", "sin", "sinh", "sqrt", "tan", "tanh"
    };
    lua_State *state;
    vox_u16 index;

    state = impl->state;
    if (!script_open_library(impl, "", luaopen_base) ||
        !script_open_library(impl, LUA_TABLIBNAME, luaopen_table) ||
        !script_open_library(impl, LUA_STRLIBNAME, luaopen_string) ||
        !script_open_library(impl, LUA_MATHLIBNAME, luaopen_math)) {
        return 0;
    }
    for (index = 0U;
         index < (vox_u16)(sizeof(unsafe_globals) / sizeof(unsafe_globals[0]));
         ++index) {
        script_remove_global(state, unsafe_globals[index]);
    }
    script_remove_field(state, "string", "dump");
    script_remove_field(state, "string", "format");
    script_remove_field(state, "string", "lower");
    script_remove_field(state, "string", "upper");
    script_remove_field(state, "math", "random");
    script_remove_field(state, "math", "randomseed");
    for (index = 0U;
         index < (vox_u16)(sizeof(unstable_math) / sizeof(unstable_math[0]));
         ++index) {
        script_remove_field(state, "math", unstable_math[index]);
    }
    return script_register_api(impl);
}

static vox_script_impl *script_impl_create(const vox_script_config *config)
{
    vox_script_impl *impl;

    impl = (vox_script_impl *)malloc(sizeof(*impl));
    if (impl == 0) {
        return 0;
    }
    memset(impl, 0, sizeof(*impl));
    impl->config = *config;
    impl->source_hash = VOX_SCRIPT_FNV_OFFSET;
    impl->catalog.abi_version = VOX_ABI_VERSION;
    impl->catalog.struct_size = (vox_u32)sizeof(impl->catalog);
    impl->catalog.schema_version = VOX_SCRIPT_SCHEMA_VERSION;
    impl->catalog.visible_rows = VOX_SCRIPT_INDEX_VISIBLE_ROWS;
    impl->state = lua_newstate(script_allocator, impl);
    if (impl->state == 0) {
        free(impl);
        return 0;
    }
    lua_atpanic(impl->state, script_panic);
    lua_pushlightuserdata(impl->state, (void *)&vox_script_registry_key);
    lua_pushlightuserdata(impl->state, (void *)impl);
    lua_rawset(impl->state, LUA_REGISTRYINDEX);
    if (!script_create_sandbox(impl)) {
        lua_close(impl->state);
        free(impl);
        return 0;
    }
    return impl;
}

static void script_impl_destroy(vox_script_impl *impl)
{
    if (impl != 0) {
        if (impl->state != 0) {
            lua_close(impl->state);
        }
        free(impl);
    }
}

static vox_result script_capture_lua_error(vox_script_impl *impl,
                                           int lua_status,
                                           vox_result load_status)
{
    const char *message;

    message = lua_tostring(impl->state, -1);
    script_set_error(impl, message != 0 ? message : "Lua execution failed");
    lua_pop(impl->state, 1);
    if (impl->budget_exhausted) {
        return VOX_SCRIPT_ERR_BUDGET;
    }
    if (impl->memory_failed || lua_status == LUA_ERRMEM) {
        return VOX_SCRIPT_ERR_MEMORY;
    }
    return load_status;
}

static vox_result script_execute(vox_script_impl *impl, const char *name,
                                 const char *bytes, vox_u32 byte_count)
{
    vox_u8 separator;
    vox_u32 index;
    int status;

    if (name == 0 || name[0] == '\0' || bytes == 0 || byte_count == 0U ||
        byte_count > impl->config.max_source_bytes - impl->source_bytes) {
        script_set_error(impl, "invalid or oversized VOX script source");
        return VOX_SCRIPT_ERR_SCHEMA;
    }
    if ((vox_u8)bytes[0] == 27U) {
        script_set_error(impl, "precompiled Lua bytecode is not permitted");
        return VOX_SCRIPT_ERR_SANDBOX;
    }
    for (index = 0U; index < byte_count; ++index) {
        if (bytes[index] == '\0') {
            script_set_error(impl, "embedded NUL in Lua source is not permitted");
            return VOX_SCRIPT_ERR_SANDBOX;
        }
    }

    impl->source_hash = script_hash_string(impl->source_hash, name);
    impl->source_hash = script_hash_update(impl->source_hash, bytes,
                                           byte_count);
    separator = 255U;
    impl->source_hash = script_hash_update(impl->source_hash, &separator, 1U);
    impl->source_bytes += byte_count;
    impl->instructions_left = impl->config.instruction_budget;
    impl->budget_exhausted = 0;
    impl->memory_failed = 0;
    lua_sethook(impl->state, script_instruction_hook, LUA_MASKCOUNT,
                (int)impl->config.hook_interval);
    status = luaL_loadbuffer(impl->state, bytes, (size_t)byte_count, name);
    if (status != 0) {
        lua_sethook(impl->state, 0, 0, 0);
        return script_capture_lua_error(impl, status,
                                        VOX_SCRIPT_ERR_SYNTAX);
    }
    status = lua_pcall(impl->state, 0, 0, 0);
    lua_sethook(impl->state, 0, 0, 0);
    if (status != 0) {
        return script_capture_lua_error(impl, status,
                                        VOX_SCRIPT_ERR_RUNTIME);
    }
    return VOX_OK;
}

static int script_compare_entries(const vox_script_entry *left,
                                  const vox_script_entry *right)
{
    if (left->order < right->order) {
        return -1;
    }
    if (left->order > right->order) {
        return 1;
    }
    if (left->kind < right->kind) {
        return -1;
    }
    if (left->kind > right->kind) {
        return 1;
    }
    return strcmp(left->id, right->id);
}

static void script_sort_entries(vox_script_catalog *catalog)
{
    vox_u16 outer;

    for (outer = 1U; outer < catalog->entry_count; ++outer) {
        vox_script_entry moving;
        vox_u16 inner;

        moving = catalog->entries[outer];
        inner = outer;
        while (inner > 0U &&
               script_compare_entries(&moving,
                                      &catalog->entries[inner - 1U]) < 0) {
            catalog->entries[inner] = catalog->entries[inner - 1U];
            --inner;
        }
        catalog->entries[inner] = moving;
    }
}

static vox_u32 script_catalog_hash(const vox_script_catalog *catalog)
{
    vox_u32 hash;
    vox_u16 entry_index;

    hash = VOX_SCRIPT_FNV_OFFSET;
    hash = script_hash_u32(hash, catalog->schema_version);
    hash = script_hash_u32(hash, (vox_u32)catalog->visible_rows);
    hash = script_hash_string(hash, catalog->game_id);
    hash = script_hash_string(hash, catalog->title);
    hash = script_hash_u32(hash, (vox_u32)catalog->entry_count);
    for (entry_index = 0U; entry_index < catalog->entry_count;
         ++entry_index) {
        const vox_script_entry *entry;
        vox_u16 value_index;

        entry = &catalog->entries[entry_index];
        hash = script_hash_u32(hash, (vox_u32)entry->kind);
        hash = script_hash_u32(hash, (vox_u32)entry->order);
        hash = script_hash_u32(hash, (vox_u32)entry->flags);
        hash = script_hash_string(hash, entry->id);
        hash = script_hash_string(hash, entry->title);
        hash = script_hash_string(hash, entry->category);
        hash = script_hash_string(hash, entry->summary);
        hash = script_hash_string(hash, entry->detail);
        hash = script_hash_string(hash, entry->tags);
        hash = script_hash_u32(hash, (vox_u32)entry->value_count);
        for (value_index = 0U; value_index < entry->value_count;
             ++value_index) {
            hash = script_hash_string(hash, entry->values[value_index].key);
            hash = script_hash_u32(hash,
                                  (vox_u32)entry->values[value_index].value);
        }
    }
    return hash;
}

static vox_result script_finalize(vox_script_impl *impl)
{
    vox_u16 entry_index;

    if (!impl->configured || impl->catalog.entry_count == 0U) {
        script_set_error(impl, "catalog must configure DIGS and define entries");
        return VOX_SCRIPT_ERR_SCHEMA;
    }
    script_sort_entries(&impl->catalog);
    memset(impl->catalog.kind_counts, 0, sizeof(impl->catalog.kind_counts));
    for (entry_index = 0U; entry_index < impl->catalog.entry_count;
         ++entry_index) {
        vox_u16 kind;

        kind = impl->catalog.entries[entry_index].kind;
        if (kind >= VOX_SCRIPT_KIND_COUNT) {
            script_set_error(impl, "catalog entry has invalid kind");
            return VOX_SCRIPT_ERR_SCHEMA;
        }
        ++impl->catalog.kind_counts[kind];
    }
    impl->catalog.source_hash = impl->source_hash;
    impl->catalog.catalog_hash = script_catalog_hash(&impl->catalog);
    return VOX_OK;
}

static void script_report_reset(vox_script_report *report)
{
    if (report != 0) {
        memset(report, 0, sizeof(*report));
        report->abi_version = VOX_ABI_VERSION;
        report->struct_size = (vox_u32)sizeof(*report);
    }
}

static void script_report_fill(vox_script_report *report,
                               const vox_script_impl *impl,
                               vox_result status)
{
    if (report != 0) {
        report->status = status;
        if (impl != 0) {
            report->source_hash = impl->source_hash;
            report->catalog_hash = impl->catalog.catalog_hash;
            report->entry_count = impl->catalog.entry_count;
            report->visible_rows = impl->catalog.visible_rows;
            script_copy_text(report->error, VOX_SCRIPT_ERROR_CAPACITY,
                             impl->error);
        }
    }
}

static int script_runtime_is_valid(const vox_script_runtime *runtime)
{
    return runtime != 0 && runtime->abi_version == VOX_ABI_VERSION &&
           runtime->struct_size == (vox_u32)sizeof(*runtime) &&
           runtime->config.abi_version == VOX_ABI_VERSION &&
           runtime->config.struct_size == (vox_u32)sizeof(runtime->config);
}

static void script_runtime_error(vox_script_runtime *runtime,
                                 const char *message)
{
    if (runtime != 0) {
        script_copy_text(runtime->last_error, VOX_SCRIPT_ERROR_CAPACITY,
                         message);
    }
}

void vox_script_config_default(vox_script_config *config)
{
    if (config != 0) {
        memset(config, 0, sizeof(*config));
        config->abi_version = VOX_ABI_VERSION;
        config->struct_size = (vox_u32)sizeof(*config);
        config->memory_limit_bytes = VOX_SCRIPT_DEFAULT_MEMORY;
        config->instruction_budget = VOX_SCRIPT_DEFAULT_BUDGET;
        config->hook_interval = VOX_SCRIPT_DEFAULT_HOOK_INTERVAL;
        config->max_source_bytes = VOX_SCRIPT_DEFAULT_SOURCE_LIMIT;
        config->max_entries = VOX_SCRIPT_MAX_ENTRIES;
    }
}

static int script_config_is_valid(const vox_script_config *config)
{
    return config != 0 && config->abi_version == VOX_ABI_VERSION &&
           config->struct_size == (vox_u32)sizeof(*config) &&
           config->memory_limit_bytes >= 65536U &&
           config->memory_limit_bytes <= (64U * 1024U * 1024U) &&
           config->instruction_budget >= config->hook_interval &&
           config->hook_interval >= 100U &&
           config->hook_interval <= 10000U &&
           config->max_source_bytes >= 1024U &&
           config->max_source_bytes <= (16U * 1024U * 1024U) &&
           config->max_entries > 0U &&
           config->max_entries <= VOX_SCRIPT_MAX_ENTRIES;
}

vox_result vox_script_runtime_init(vox_script_runtime *runtime,
                                   const vox_script_config *config)
{
    vox_script_config defaults;
    const vox_script_config *selected;

    if (runtime == 0) {
        return VOX_ERR_INVALID;
    }
    vox_script_config_default(&defaults);
    selected = config != 0 ? config : &defaults;
    if (!script_config_is_valid(selected)) {
        memset(runtime, 0, sizeof(*runtime));
        return VOX_ERR_INVALID;
    }
    memset(runtime, 0, sizeof(*runtime));
    runtime->abi_version = VOX_ABI_VERSION;
    runtime->struct_size = (vox_u32)sizeof(*runtime);
    runtime->config = *selected;
    return VOX_OK;
}

void vox_script_runtime_shutdown(vox_script_runtime *runtime)
{
    if (runtime != 0) {
        if (script_runtime_is_valid(runtime)) {
            script_impl_destroy((vox_script_impl *)runtime->active);
        }
        memset(runtime, 0, sizeof(*runtime));
    }
}

static vox_result script_build_sources(vox_script_runtime *runtime,
                                       const vox_script_source *sources,
                                       vox_u16 source_count,
                                       vox_script_impl **impl_out)
{
    vox_script_impl *impl;
    vox_u16 source_index;
    vox_result status;

    *impl_out = 0;
    if (!script_runtime_is_valid(runtime) || sources == 0 ||
        source_count == 0U || source_count > VOX_SCRIPT_MAX_SOURCES) {
        script_runtime_error(runtime, "invalid VOX script source list");
        return VOX_ERR_INVALID;
    }
    impl = script_impl_create(&runtime->config);
    if (impl == 0) {
        script_runtime_error(runtime, "unable to allocate Lua 5.1 runtime");
        return VOX_SCRIPT_ERR_MEMORY;
    }
    status = VOX_OK;
    for (source_index = 0U; source_index < source_count; ++source_index) {
        status = script_execute(impl, sources[source_index].name,
                                sources[source_index].bytes,
                                sources[source_index].byte_count);
        if (status != VOX_OK) {
            break;
        }
    }
    if (status == VOX_OK) {
        status = script_finalize(impl);
    }
    if (status != VOX_OK) {
        script_runtime_error(runtime, impl->error);
    }
    *impl_out = impl;
    return status;
}

static vox_result script_complete_build(vox_script_runtime *runtime,
                                        vox_script_impl *impl,
                                        vox_result status, int activate,
                                        vox_script_report *report)
{
    script_report_fill(report, impl, status);
    if (status != VOX_OK) {
        script_impl_destroy(impl);
        return status;
    }
    runtime->last_error[0] = '\0';
    if (activate) {
        vox_script_impl *previous;

        previous = (vox_script_impl *)runtime->active;
        runtime->active = impl;
        runtime->active_source_hash = impl->catalog.source_hash;
        runtime->active_catalog_hash = impl->catalog.catalog_hash;
        ++runtime->reload_generation;
        script_impl_destroy(previous);
    } else {
        script_impl_destroy(impl);
    }
    return VOX_OK;
}

vox_result vox_script_validate_sources(vox_script_runtime *runtime,
                                       const vox_script_source *sources,
                                       vox_u16 source_count,
                                       vox_script_report *report)
{
    vox_script_impl *impl;
    vox_result status;

    script_report_reset(report);
    status = script_build_sources(runtime, sources, source_count, &impl);
    if (impl == 0) {
        if (report != 0) {
            report->status = status;
            script_copy_text(report->error, VOX_SCRIPT_ERROR_CAPACITY,
                             vox_script_last_error(runtime));
        }
        return status;
    }
    return script_complete_build(runtime, impl, status, 0, report);
}

vox_result vox_script_reload_sources(vox_script_runtime *runtime,
                                     const vox_script_source *sources,
                                     vox_u16 source_count,
                                     vox_script_report *report)
{
    vox_script_impl *impl;
    vox_result status;

    script_report_reset(report);
    status = script_build_sources(runtime, sources, source_count, &impl);
    if (impl == 0) {
        if (report != 0) {
            report->status = status;
            script_copy_text(report->error, VOX_SCRIPT_ERROR_CAPACITY,
                             vox_script_last_error(runtime));
        }
        return status;
    }
    return script_complete_build(runtime, impl, status, 1, report);
}

static void script_manifest_directory(const char *path, char *directory)
{
    const char *cursor;
    const char *separator;
    size_t length;

    separator = 0;
    cursor = path;
    while (*cursor != '\0') {
        if (*cursor == '/' || *cursor == '\\') {
            separator = cursor;
        }
        ++cursor;
    }
    if (separator == 0) {
        script_copy_text(directory, VOX_SCRIPT_PATH_CAPACITY, ".");
        return;
    }
    length = (size_t)(separator - path);
    if (length == 0U) {
        length = 1U;
    }
    if (length >= VOX_SCRIPT_PATH_CAPACITY) {
        length = VOX_SCRIPT_PATH_CAPACITY - 1U;
    }
    memcpy(directory, path, length);
    directory[length] = '\0';
}

static char *script_trim_line(char *line)
{
    char *start;
    char *end;

    start = line;
    while (*start != '\0' && isspace((unsigned char)*start)) {
        ++start;
    }
    end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) {
        --end;
    }
    *end = '\0';
    return start;
}

static vox_result script_execute_file(vox_script_impl *impl,
                                      const char *logical_name,
                                      const char *full_path)
{
    FILE *file;
    long length;
    char *bytes;
    size_t read_count;
    vox_result status;

    file = fopen(full_path, "rb");
    if (file == 0) {
        script_set_error(impl, "unable to open Lua source from manifest");
        return VOX_SCRIPT_ERR_IO;
    }
    if (fseek(file, 0L, SEEK_END) != 0) {
        fclose(file);
        script_set_error(impl, "unable to seek Lua source");
        return VOX_SCRIPT_ERR_IO;
    }
    length = ftell(file);
    if (length <= 0L || length > (long)impl->config.max_source_bytes ||
        fseek(file, 0L, SEEK_SET) != 0) {
        fclose(file);
        script_set_error(impl, "Lua source size is invalid");
        return VOX_SCRIPT_ERR_IO;
    }
    bytes = (char *)malloc((size_t)length);
    if (bytes == 0) {
        fclose(file);
        script_set_error(impl, "unable to allocate Lua source buffer");
        return VOX_SCRIPT_ERR_MEMORY;
    }
    read_count = fread(bytes, 1U, (size_t)length, file);
    fclose(file);
    if (read_count != (size_t)length) {
        free(bytes);
        script_set_error(impl, "unable to read complete Lua source");
        return VOX_SCRIPT_ERR_IO;
    }
    status = script_execute(impl, logical_name, bytes, (vox_u32)length);
    free(bytes);
    return status;
}

static vox_result script_build_manifest(vox_script_runtime *runtime,
                                        const char *manifest_path,
                                        vox_script_impl **impl_out)
{
    FILE *manifest;
    vox_script_impl *impl;
    char directory[VOX_SCRIPT_PATH_CAPACITY];
    char line[VOX_SCRIPT_PATH_CAPACITY];
    char full_path[VOX_SCRIPT_PATH_CAPACITY];
    vox_u16 source_count;
    vox_result status;

    *impl_out = 0;
    if (!script_runtime_is_valid(runtime) || manifest_path == 0 ||
        manifest_path[0] == '\0') {
        script_runtime_error(runtime, "invalid VOX script manifest path");
        return VOX_ERR_INVALID;
    }
    manifest = fopen(manifest_path, "r");
    if (manifest == 0) {
        script_runtime_error(runtime, "unable to open VOX script manifest");
        return VOX_SCRIPT_ERR_IO;
    }
    impl = script_impl_create(&runtime->config);
    if (impl == 0) {
        fclose(manifest);
        script_runtime_error(runtime, "unable to allocate Lua 5.1 runtime");
        return VOX_SCRIPT_ERR_MEMORY;
    }
    script_manifest_directory(manifest_path, directory);
    source_count = 0U;
    status = VOX_OK;
    while (fgets(line, (int)sizeof(line), manifest) != 0) {
        char *logical_name;
        size_t directory_length;
        size_t name_length;

        if (strchr(line, '\n') == 0 && !feof(manifest)) {
            script_set_error(impl, "manifest line exceeds path capacity");
            status = VOX_SCRIPT_ERR_SCHEMA;
            break;
        }
        logical_name = script_trim_line(line);
        if (logical_name[0] == '\0' || logical_name[0] == '#') {
            continue;
        }
        if (source_count >= VOX_SCRIPT_MAX_SOURCES) {
            script_set_error(impl, "manifest contains too many Lua sources");
            status = VOX_SCRIPT_ERR_SCHEMA;
            break;
        }
        directory_length = strlen(directory);
        name_length = strlen(logical_name);
        if (directory_length + 1U + name_length >= sizeof(full_path)) {
            script_set_error(impl, "manifest source path exceeds capacity");
            status = VOX_SCRIPT_ERR_SCHEMA;
            break;
        }
        memcpy(full_path, directory, directory_length);
        full_path[directory_length] = '/';
        memcpy(full_path + directory_length + 1U, logical_name,
               name_length + 1U);
        status = script_execute_file(impl, logical_name, full_path);
        if (status != VOX_OK) {
            break;
        }
        ++source_count;
    }
    fclose(manifest);
    if (status == VOX_OK && source_count == 0U) {
        script_set_error(impl, "manifest contains no Lua sources");
        status = VOX_SCRIPT_ERR_SCHEMA;
    }
    if (status == VOX_OK) {
        status = script_finalize(impl);
    }
    if (status != VOX_OK) {
        script_runtime_error(runtime, impl->error);
    }
    *impl_out = impl;
    return status;
}

vox_result vox_script_validate_manifest(vox_script_runtime *runtime,
                                        const char *manifest_path,
                                        vox_script_report *report)
{
    vox_script_impl *impl;
    vox_result status;

    script_report_reset(report);
    status = script_build_manifest(runtime, manifest_path, &impl);
    if (impl == 0) {
        if (report != 0) {
            report->status = status;
            script_copy_text(report->error, VOX_SCRIPT_ERROR_CAPACITY,
                             vox_script_last_error(runtime));
        }
        return status;
    }
    return script_complete_build(runtime, impl, status, 0, report);
}

vox_result vox_script_reload_manifest(vox_script_runtime *runtime,
                                      const char *manifest_path,
                                      vox_script_report *report)
{
    vox_script_impl *impl;
    vox_result status;

    script_report_reset(report);
    status = script_build_manifest(runtime, manifest_path, &impl);
    if (impl == 0) {
        if (report != 0) {
            report->status = status;
            script_copy_text(report->error, VOX_SCRIPT_ERROR_CAPACITY,
                             vox_script_last_error(runtime));
        }
        return status;
    }
    return script_complete_build(runtime, impl, status, 1, report);
}

const vox_script_catalog *vox_script_catalog_get(
                                      const vox_script_runtime *runtime)
{
    const vox_script_impl *impl;

    if (!script_runtime_is_valid(runtime) || runtime->active == 0) {
        return 0;
    }
    impl = (const vox_script_impl *)runtime->active;
    return &impl->catalog;
}

const vox_script_entry *vox_script_catalog_entry(
                                      const vox_script_catalog *catalog,
                                      vox_u16 ordinal)
{
    if (catalog == 0 || catalog->abi_version != VOX_ABI_VERSION ||
        catalog->struct_size != (vox_u32)sizeof(*catalog) ||
        ordinal >= catalog->entry_count) {
        return 0;
    }
    return &catalog->entries[ordinal];
}

const vox_script_entry *vox_script_catalog_find(
                                      const vox_script_catalog *catalog,
                                      const char *id)
{
    vox_u16 index;

    if (catalog == 0 || id == 0 ||
        catalog->abi_version != VOX_ABI_VERSION ||
        catalog->struct_size != (vox_u32)sizeof(*catalog)) {
        return 0;
    }
    for (index = 0U; index < catalog->entry_count; ++index) {
        if (strcmp(catalog->entries[index].id, id) == 0) {
            return &catalog->entries[index];
        }
    }
    return 0;
}

const vox_script_value *vox_script_entry_value(
                                      const vox_script_entry *entry,
                                      const char *key)
{
    vox_u16 index;

    if (entry == 0 || key == 0) {
        return 0;
    }
    for (index = 0U; index < entry->value_count; ++index) {
        if (strcmp(entry->values[index].key, key) == 0) {
            return &entry->values[index];
        }
    }
    return 0;
}

const char *vox_script_last_error(const vox_script_runtime *runtime)
{
    static const char invalid[] = "invalid VOX script runtime";

    if (!script_runtime_is_valid(runtime)) {
        return invalid;
    }
    return runtime->last_error;
}
