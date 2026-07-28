-- SPDX-License-Identifier: GPL-3.0-or-later

vox.define("system", "system.movement", {
    order = 10,
    title = "MINER MOVEMENT",
    category = "SYSTEMS",
    summary = "Responsive acceleration, coyote time, buffered jumps, air steering, and a short steam burst.",
    detail = "Ground traction reaches full run speed quickly without snapping. Releasing movement brakes harder on soil and stone than in air. A six-tick coyote window forgives late jumps, a six-tick input buffer forgives early jumps, and holding jump extends the rise for eight ticks. The steam pack supplies a precise mid-air correction and recharges while grounded.",
    tags = "run,jump,steam,game-feel",
    values = {
        air_accel_q16 = 4096,
        air_decel_q16 = 2048,
        coyote_ticks = 6,
        ground_accel_q16 = 8192,
        ground_decel_q16 = 12288,
        jump_buffer_ticks = 6,
        jump_hold_accel_q16 = 3072,
        jump_hold_ticks = 8,
        jump_speed_q16 = -106496,
        run_speed_q16 = 49152,
        steam_accel_q16 = 11264,
        steam_lateral_q16 = 3072,
        steam_max_rise_q16 = -98304,
        steam_recharge_q16 = 728,
        steam_use_q16 = 728
    }
})

vox.define("system", "system.rope", {
    order = 11,
    title = "PIT ROPE",
    category = "SYSTEMS",
    summary = "An always-carried segmented cable that casts, wraps, unwraps, and preserves swing momentum across broken ground.",
    detail = "Cast toward stable terrain using either the host's Hold or Toggle policy. The swept hook respects cover, then a bounded twelve-point cable wraps and unwraps around terrain while the last segment constrains the miner. Excess tension or destruction of the anchor breaks the line. A hook strike leaves an unshielded enemy at exactly one health with bounded knockback and ends the cast; shielded targets block it and cable contact never damages.",
    tags = "rope,traversal,momentum,anchor",
    values = {
        break_tension_q16 = 327680,
        integrity = 100,
        hook_speed_q16 = 196608,
        max_length_q16 = 3145728,
        max_points = 12,
        min_length_q16 = 196608,
        pull_q16 = 32768
    }
})

vox.define("system", "system.spawn_shield", {
    order = 12,
    title = "SPAWN SHIELD",
    category = "SYSTEMS",
    summary = "Five seconds to read the terrain and choose a route without spawn killing.",
    detail = "A fresh fighter ignores direct damage, blast impulse, fire, fluid hazards, and lava for 300 simulation ticks. Moving, jumping, aiming, and using the rope preserve it. Any offensive tool use ends it immediately, including a missed shot or terrain attack. The shield also ends naturally at zero and never returns until the next spawn.",
    tags = "spawn,invulnerability,fairness",
    values = { duration_ticks = 300, ends_on_attack = 1, blocks_impulse = 1 }
})

vox.define("system", "system.camera", {
    order = 13,
    title = "SHARED CAMERA",
    category = "SYSTEMS",
    summary = "A weighted shared view anticipates movement and keeps active local fighters readable.",
    detail = "The camera follows the weighted center of local humans, expands for separation and high-speed action, and eases back after impact. Aim and velocity add restrained look-ahead. Explosion kick, hit kick, and kill emphasis are local-player events only. Manual wheel zoom biases the automatic field of view without disconnecting it from the fighters.",
    tags = "camera,zoom,momentum,feedback",
    values = { max_zoom_q8 = 640, min_zoom_q8 = 160, smoothing_q8 = 36 }
})

vox.define("system", "system.anatomy", {
    order = 14,
    title = "SYSTEMIC ANATOMY",
    category = "SYSTEMS",
    summary = "Fifteen simulated body segments turn damage, bleeding, severing, and cauterizing into material events.",
    detail = "Head and torso are vital. Limbs carry their own health and can be severed by ballistic, blunt, explosive, or heat profiles. Open parts emit blood voxels until clotted, cauterized, or dead. Lost legs impair movement, lost arms impair tool handling, and vital failure or total-health loss kills. Detached tissue inherits impact velocity and keeps reacting with heat, water, terrain, and blasts.",
    tags = "damage,gore,bleeding,limbs",
    values = { bleedout_health = 0, clot_ticks = 180, part_count = 15 }
})

vox.define("system", "system.simulation", {
    order = 15,
    title = "VOXEL SIMULATION",
    category = "SYSTEMS",
    summary = "A dense deterministic mini-voxel field drives terrain, debris, fluids, gases, heat, blood, and light.",
    detail = "The world advances at sixty fixed ticks independent of presentation rate. Dirty chunk metadata limits work to awake regions while all occupied cells remain individually addressable. Structural failure wakes unsupported material so tunnels cave in instead of preserving perfect blast circles. Identical seed, inputs, rules, and script hashes produce the same state hash.",
    tags = "voxels,determinism,collapse,materials",
    values = { cells = 1638400, depth = 10, height = 320, ticks_per_second = 60, width = 512 }
})

vox.define("system", "system.input", {
    order = 16,
    title = "LOCAL INPUT",
    category = "SYSTEMS",
    summary = "Two local humans may use keyboard plus controller or one controller each, with complete rebinding.",
    detail = "The first bound device claims Player 1 and the second claims Player 2. Aim is absolute for a pointer and rate-based for a stick, but both feed the same world target contract. Controller loss pauses local play and offers reclaim instead of silently handing a fighter to AI. Menus and the Index support keyboard and Xbox-style gamepads.",
    tags = "keyboard,controller,rebind,local-coop",
    values = { controller_deadzone_q15 = 7000, local_players_max = 2 }
})

vox.define("system", "system.index", {
    order = 17,
    title = "MINER'S INDEX",
    category = "SYSTEMS",
    summary = "Every shipped material, weapon, entity, reaction, and rule is visible from the main menu.",
    detail = "The left rail shows exactly six rows with arrows indicating entries above and below. Up and Down move one item with a short stepped transition; the right pane updates automatically as selection changes. Back returns to the previous menu. The v0.0.3 catalog is fully unlocked so testing never depends on progression state.",
    tags = "menu,catalog,qa,accessibility",
    values = { transition_ticks = 5, unlocked = 1, visible_rows = 6 }
})

vox.define("system", "system.match_clock", {
    order = 18,
    title = "QUICK MATCH CLOCK",
    category = "SYSTEMS",
    summary = "Two- or three-minute regulation matches end with an individual or team winner and permit exact draws.",
    detail = "Normal matches default to two minutes with an optional five, ten, or twenty-point cap. The engine resolves the complete final simulation tick before declaring the highest FFA score or Miners-versus-Machines team total. The lava endgame begins during the final thirty seconds.",
    tags = "timer,score,results,lava",
    values = { default_seconds = 120, lava_final_seconds = 30, maximum_seconds = 180 }
})

vox.define("system", "system.respawn", {
    order = 19,
    title = "RESPAWN MODES",
    category = "SYSTEMS",
    summary = "AUTO and ON FIRE modes add a configurable pause before a defeated miner returns.",
    detail = "The delay may be zero, one, two, three, or five seconds and defaults to three. AUTO returns when ready. ON FIRE waits for a fresh Fire press after the countdown; holding Fire through death cannot skip the gate. Machines request their spawn automatically.",
    tags = "death,respawn,controls,match-rules",
    values = { default_ticks = 180, modes = 2, spawn_shield_ticks = 300 }
})

vox.define("system", "system.combat_feedback", {
    order = 20,
    title = "COMBAT FEEDBACK",
    category = "SYSTEMS",
    summary = "Names, hit confirms, death notices, multikills, and sprees translate simulation events without altering them.",
    detail = "A three-second chain produces Double, Triple, and Multi Kill awards while five uninterrupted kills produce Killing Spree. Local hits tint the target, mark the impact, pulse the crosshair, and play a centered confirmation. Bot-only combat stays in the compact feed instead of triggering large local banners.",
    tags = "hit-marker,killfeed,combo,names",
    values = { chain_ticks = 180, feed_rows = 6, spree_kills = 5 }
})
