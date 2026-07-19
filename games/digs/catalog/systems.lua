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
        steam_accel_q16 = 16384,
        steam_recharge_q16 = 512,
        steam_use_q16 = 1024
    }
})

vox.define("system", "system.rope", {
    order = 11,
    title = "PIT ROPE",
    category = "SYSTEMS",
    summary = "An always-carried, hold-to-use cable for swinging across broken ground.",
    detail = "Hold Rope while aiming at stable terrain. A ray selects the first valid anchor, the cable constrains the miner to its current length, and momentum carries into the swing. Release to detach. Excess tension or destruction of the anchor breaks the line. The rope cannot attach to fluids, gas, flesh particles, or unsupported falling debris.",
    tags = "rope,traversal,momentum,anchor",
    values = {
        break_tension_q16 = 327680,
        integrity = 100,
        max_length_q16 = 3145728,
        min_length_q16 = 196608,
        pull_q16 = 32768,
        ray_steps = 96
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
    values = { depth = 10, height = 160, ticks_per_second = 60, width = 256 }
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
    detail = "The left rail shows exactly six rows with arrows indicating entries above and below. Up and Down move one item with a short stepped transition; the right pane updates automatically as selection changes. Back returns to the previous menu. The v0.0.2 catalog is fully unlocked so testing never depends on progression state.",
    tags = "menu,catalog,qa,accessibility",
    values = { transition_ticks = 5, unlocked = 1, visible_rows = 6 }
})
