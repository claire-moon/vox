-- SPDX-License-Identifier: GPL-3.0-or-later

vox.define("entity", "entity.miner", {
    order = 300, title = "MINER", category = "ENTITIES",
    summary = "Human-controlled fifteen-part fighter with a steam pack, rope, and the complete tool belt.",
    detail = "A miner is a deterministic physics body plus segmented anatomy rather than a single sprite hit box. Two miners may be local. Movement, aim, tool use, rope state, spawn shield, bleed state, score, and device ownership are simulated independently for each slot.",
    tags = "player,human,fighter",
    values = { anatomy_parts = 15, max_health = 100, team = 0 }
})

vox.define("entity", "entity.machine", {
    order = 301, title = "MACHINE", category = "ENTITIES",
    summary = "AI-controlled mining automaton governed by the same movement, damage, perception, and tool rules.",
    detail = "Machines receive no omniscience or physics bonus. They perceive line of sight, sound events, and remembered positions; choose among roaming, searching, attacking, and retreating; and use rope, terrain tools, hazards, and cover. Their anatomy uses equivalent gameplay values with mechanical presentation.",
    tags = "bot,ai,fighter,coop",
    values = { anatomy_parts = 15, max_health = 100, team = 1 }
})

vox.define("entity", "entity.projectile", {
    order = 302, title = "PROJECTILE", category = "ENTITIES",
    summary = "Fixed-tick swept payload carrying owner, tool, motion, fuse, damage, radius, and material.",
    detail = "Projectiles advance in bounded substeps so fast nails do not tunnel through cells or anatomy. Collision resolution is ordered and deterministic. A payload may strike directly, settle under gravity, deposit a material, detonate, emit secondary fragments, or expire without effect.",
    tags = "physics,weapon,payload",
    values = { max_active = 64, substeps = 4 }
})

vox.define("entity", "entity.effect_voxel", {
    order = 303, title = "EFFECT VOXEL", category = "ENTITIES",
    summary = "Budgeted simulated matter used for debris, smoke, cinders, water, blood, flesh, sparks, and fragments.",
    detail = "An effect voxel has material, position, velocity, lifetime, source, and seeded variant. It collides with the world and may deposit into terrain, enter a fluid or gas reaction, stain a surface, emit light, or remain airborne. Presentation budgets change count, never gameplay-critical damage or random order.",
    tags = "particle,voxel,material,fx",
    values = { carnage_budget = 3072, retro_budget = 768, standard_budget = 1536 }
})

vox.define("entity", "entity.rope", {
    order = 304, title = "ROPE CABLE", category = "ENTITIES",
    summary = "Breakable straight constraint linking one fighter to one stable terrain anchor.",
    detail = "The rope stores anchor, length, tension, integrity, and active state. It does not create terrain or pull from empty air. Anchor destruction detaches it, excessive tension breaks it, and ordinary release preserves the fighter's current velocity for a clean swing exit.",
    tags = "traversal,constraint,breakable",
    values = { integrity = 100, owners = 1 }
})

vox.define("entity", "entity.lava_front", {
    order = 305, title = "RISING LAVA", category = "ENTITIES",
    summary = "Match-level hazard surface that steadily converts safe low ground into an active heat simulation.",
    detail = "After the configured start tick, the lava surface rises deterministically and fills reachable cells from below. Contact bypasses ordinary attrition by killing an unshielded fighter rapidly, while the surrounding cells still react normally with water, fuel, flesh, blood, and terrain.",
    tags = "hazard,lava,timer",
    values = { default_start_tick = 12600, material_id = 8 }
})

vox.define("entity", "entity.spawn_shield", {
    order = 306, title = "SPAWN SHIELD FIELD", category = "ENTITIES",
    summary = "Temporary all-hazard protection attached to a newly spawned fighter.",
    detail = "The field blocks direct health damage, anatomy damage, bleed creation, blast impulse, crush force, heat, fluid hazards, and lava. It is a gameplay state, not colliding matter. It expires after five seconds or immediately when its owner performs an offensive action.",
    tags = "spawn,shield,state",
    values = { duration_ticks = 300 }
})

vox.define("entity", "entity.sound_event", {
    order = 307, title = "SOUND EVENT", category = "ENTITIES",
    summary = "Short-lived deterministic perception event shared by audio playback and bot hearing.",
    detail = "Weapon fire, impacts, explosions, rope breaks, pain, deaths, and barks emit an event with source, position, magnitude, and variant. Local audio turns it into chip voices; bots receive only events within material-aware hearing range and remember the last plausible source position.",
    tags = "audio,ai,event,perception",
    values = { max_events = 128 }
})

vox.define("entity", "entity.bot_memory", {
    order = 308, title = "BOT MEMORY", category = "ENTITIES",
    summary = "Fair, decaying record of targets seen or heard by a machine fighter.",
    detail = "A bot stores state, target, last seen position, memory lifetime, decision cooldown, and roam direction. It cannot query hidden fighter positions. Smoke, terrain, distance, fresh damage, health, hazards, rope routes, and recent sounds alter its next state transition.",
    tags = "ai,perception,state-machine",
    values = { decision_ticks = 8, memory_ticks = 180, retreat_health = 28 }
})
