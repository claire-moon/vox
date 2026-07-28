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
    summary = "Eight deterministic stereo voices synthesize effects, interface notes, ambience, and phonetic speech.",
    detail = "VOX Audio v2 is an original allocation-free GPL implementation inspired by late-1970s divider channels and polynomial noise. Two four-voice banks remain bounded while a sample-time queue schedules short notes, confirms, speech, and world ambience without recorded samples.",
    tags = "8-bit,synthesis,stereo,sfx,speech",
    values = { api_version = 2, banks = 2, note_queue = 32, voices = 8 }
})

vox.define("audio", "audio.event_mapping", {
    order = 701, title = "SOUND EVENT MAPPING", category = "AUDIO",
    summary = "World events and local confirmations select bounded chip patches without changing simulation state.",
    detail = "Shots, impacts, explosions, rope events, hurt, death, spawn, hit confirms, and kill confirms use separate priorities and stereo placement. Magnitude, material, and deterministic event variants change divider, noise, and decay while preserving a recognizable sound family.",
    tags = "events,variation,determinism",
    values = { patch_count = 30, voices = 8 }
})

vox.define("audio", "audio.bot_barks", {
    order = 702, title = "CONTEXTUAL MINER BARKS", category = "AUDIO",
    summary = "Humans and machines share six deep phrase pools plus seeded context grammar synthesized from authored phonemes.",
    detail = "Idle, searching, attacking, retreating, hurt, and victory each provide fifty curated lines plus deterministic rival, tool, material, and hazard grammar. Humans speak only on Bark with a three-second cooldown; Dummy Mode bypasses only that local-human cooldown. Machines pass a sparse deterministic eligibility gate, a twelve-second cooldown, and global spacing. Speech remains presentation feedback while authoritative AI bark/state events carry only bounded context.",
    tags = "ai,barks,chip-speech,feedback",
    values = { bot_cooldown_ticks = 720, contexts = 6, human_cooldown_ticks = 180, phrases_per_context = 50 }
})

vox.define("audio", "audio.announcer", {
    order = 703, title = "PHONETIC ANNOUNCER", category = "AUDIO",
    summary = "An original fixed-point formant voice announces DIGS, match start, multikills, and sprees.",
    detail = "Forty stable allophones feed deep-announcer and high-miner profiles at an internal eight-kilohertz speech rate. Curated phrases plus deterministic grammar and letter-to-sound or spelled fallback keep contextual speech and custom names portable in v0.0.3.",
    tags = "speech,allophone,announcer,portable",
    values = { allophones = 40, internal_rate = 8000, phrase_queue = 4, profiles = 2 }
})

vox.define("audio", "audio.menu_motif", {
    order = 704, title = "MINING MENU MOTIF", category = "AUDIO",
    summary = "Successful menu movement walks an eight-note minor-pentatonic work-song instead of repeating one beep.",
    detail = "Navigation schedules one short note on a sixty-millisecond grid and resets after one and a half seconds idle. Accept and Back use rising and falling cadences. Mouse-wheel zoom has its own quiet click and stays silent at zoom limits.",
    tags = "ui,motif,zoom,feedback",
    values = { idle_reset_ms = 1500, notes = 8, note_grid_ms = 60 }
})

vox.define("audio", "audio.ambience", {
    order = 705, title = "WORLD AWARE AMBIENCE", category = "AUDIO",
    summary = "Wind, water, and lava layers follow the terrain visible around the shared camera.",
    detail = "A bounded five-hertz scene mapper samples a fixed eight-by-four grid through all ten depth cells. Open sky, water, lava, and material centroids drive smoothed gain and pan while the two strongest layers mix below gameplay and speech priority.",
    tags = "ambience,wind,water,lava,bounded",
    values = { depth_reads_per_update = 320, sources = 3, update_hz = 5 }
})

vox.define("audio", "audio.master_volume", {
    order = 706, title = "MASTER VOLUME", category = "AUDIO",
    summary = "One persistent master control adjusts every generated sound from zero through one hundred percent.",
    detail = "The Options menu exposes eleven ten-percent steps and no per-category sliders. A short gain ramp avoids clicks. At zero the output is silent while deterministic note, speech, and ambience clocks continue to advance.",
    tags = "volume,options,accessibility",
    values = { default_percent = 80, maximum_percent = 100, step_percent = 10 }
})
