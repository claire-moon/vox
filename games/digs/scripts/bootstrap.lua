-- SPDX-License-Identifier: GPL-3.0-or-later
-- Plain Lua 5.1 source: deterministic, data-only DIGS bootstrap.

assert(vox and vox.api_version == 1, "VOX script API 1 is required")
assert(io == nil and os == nil and package == nil and debug == nil,
       "host libraries must not enter the deterministic sandbox")
assert(dofile == nil and loadfile == nil and load == nil and loadstring == nil,
       "dynamic code loading must remain disabled")
assert(require == nil and module == nil and collectgarbage == nil,
       "module and allocator controls must remain host-owned")
assert(print == nil and pairs == nil and next == nil and tostring == nil,
       "process I/O and unordered table traversal must remain disabled")
assert(string.dump == nil and math.random == nil and math.randomseed == nil,
       "bytecode and process-global random state must remain disabled")
assert(math.sin == nil and math.cos == nil and math.sqrt == nil,
       "platform-dependent transcendental math must remain disabled")
assert(vox.random(1448037169, 1000) == vox.random(1448037169, 1000),
       "VOX stateless randomness must be deterministic")

vox.configure {
    schema = 1,
    game_id = "digs",
    title = "DIGS MINER'S INDEX",
    visible_rows = 6
}
