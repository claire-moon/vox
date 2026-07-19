-- SPDX-License-Identifier: GPL-3.0-or-later

vox.define("fx", "fx.retro", {
    order = 600, title = "FX PROFILE: RETRO", category = "PRESENTATION",
    summary = "A 768-voxel ceiling for older CPUs and intentionally sparse 1990s presentation.",
    detail = "Retro preserves gameplay events, damage, terrain work, and deterministic event variants while reducing cosmetic effect emission. When the pool is full, replacement follows a deterministic cursor rather than allocation timing.",
    tags = "particles,performance,retro",
    values = { voxel_budget = 768 }
})

vox.define("fx", "fx.standard", {
    order = 601, title = "FX PROFILE: STANDARD", category = "PRESENTATION",
    summary = "The default 1536-voxel balance for clear explosions, gore, smoke, sparks, and debris.",
    detail = "Standard exposes the full seeded event grammar at a budget suitable for the GTX 1660 Ti test bench and ordinary cross-generation PCs. Gameplay remains identical to the other profiles.",
    tags = "particles,default,balanced",
    values = { default = 1, voxel_budget = 1536 }
})

vox.define("fx", "fx.carnage", {
    order = 602, title = "FX PROFILE: CARNAGE", category = "PRESENTATION",
    summary = "A 3072-voxel ceiling emphasizing long debris tails, fluid gore, sparks, and secondary smoke.",
    detail = "Carnage increases cosmetic samples and lifetimes without introducing nondeterministic randomness or additional gameplay damage. It is the stress and spectacle setting for modern hardware.",
    tags = "particles,gore,stress-test",
    values = { voxel_budget = 3072 }
})

vox.define("fx", "fx.flashes", {
    order = 603, title = "LOCAL EVENT FLASHES", category = "PRESENTATION",
    summary = "White explosion and red kill feedback trigger only for events involving a local human.",
    detail = "Full, Reduced, and Off profiles scale or suppress the overlay. Bot-on-bot events never flash the display. Explosion white decays quickly; kill red carries more weight. The rule prevents distant AI fights from becoming repetitive eye strain.",
    tags = "accessibility,feedback,local-player",
    values = { full = 2, off = 0, reduced = 1 }
})

vox.define("fx", "fx.damage_numbers", {
    order = 604, title = "DAMAGE NUMBERS", category = "PRESENTATION",
    summary = "Local-human damage is aggregated into a readable 100 ms pop with simple size and color controls.",
    detail = "Numbers appear only when a local human deals or receives damage. Hits sharing source and target inside six simulation ticks combine, preventing shotgun pellets and bleed ticks from flooding the view. The setting defaults on and does not affect simulation hashes.",
    tags = "feedback,damage,accessibility",
    values = { aggregate_ticks = 6, default = 1 }
})

vox.define("audio", "audio.dual_pokey", {
    order = 700, title = "DUAL CHIP SOUND", category = "AUDIO",
    summary = "Eight deterministic stereo voices synthesize every effect and bark from a POKEY-inspired register palette.",
    detail = "VOX uses an original GPL implementation inspired by late-1970s polynomial noise and divider-channel techniques; it does not copy Atari800 code. Two four-voice banks provide square, polynomial noise, metallic, and stepped-envelope shapes at the audio callback rate. There is no music in v0.0.2.",
    tags = "8-bit,synthesis,stereo,sfx",
    values = { banks = 2, music = 0, voices = 8 }
})

vox.define("audio", "audio.event_mapping", {
    order = 701, title = "SOUND EVENT MAPPING", category = "AUDIO",
    summary = "Simulation event type, magnitude, material, and seeded variant select a bounded chip patch.",
    detail = "Shots, impacts, explosions, rope events, hurt, death, spawn, lava, and UI navigation map to short register envelopes. Magnitude changes divider and decay; material selects noise character; deterministic variant changes a small patch range so repeated events remain recognizable without becoming identical.",
    tags = "events,variation,determinism",
    values = { patch_count = 24, voices = 8 }
})

vox.define("audio", "audio.bot_barks", {
    order = 702, title = "MACHINE BARKS", category = "AUDIO",
    summary = "Sparse contextual synthetic chirps communicate bot state without recorded speech.",
    detail = "A machine may bark on acquiring a target, losing sight, retreating, attaching a rope, taking a severe hit, or scoring a kill. Cooldowns and event priority keep chatter sparse. Pitch and rhythm derive from bot slot, state, and event sequence, making each machine legible but deterministic.",
    tags = "ai,barks,chip-speech,feedback",
    values = { cooldown_ticks = 180, variants = 16 }
})
