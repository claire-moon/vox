-- SPDX-License-Identifier: GPL-3.0-or-later
-- Flags: melee 1, projectile 2, explosive 4, deposit 8, gravity 16.

vox.define("weapon", "weapon.pick", {
    order = 200, title = "PICK", category = "TOOLS & WEAPONS",
    summary = "Fast close-range mining tool for carving footholds and opening weak anatomy.",
    detail = "The pick applies a narrow blunt cut with little knockback and no projectile. It removes small, irregular clusters of soil, coal, biomass, and weakened stone without spending ammunition. Against fighters it favors the aimed body segment and can turn an existing wound into a faster bleed.",
    tags = "melee,mining,precision",
    values = { blast_radius = 2, cooldown_ticks = 10, damage = 28, damage_flags = 2, flags = 1, fuse_ticks = 0, projectile_speed_q8 = 0 }
})

vox.define("weapon", "weapon.blast_charge", {
    order = 201, title = "BLAST CHARGE", category = "TOOLS & WEAPONS",
    summary = "A timed demolition puck whose pressure excavates an uneven cavity and wakes unsupported terrain.",
    detail = "The charge follows gravity, settles, and detonates after one second. Its core pulverizes weak terrain while the pressure skirt damages a noisy outer region, throws sampled debris, wakes structural checks, and delivers heavy explosive anatomy damage. Firedamp and coal can extend the event well beyond the first crater.",
    tags = "explosive,terrain,collapse,timed",
    values = { blast_radius = 8, cooldown_ticks = 45, damage = 72, damage_flags = 4, debris_samples = 96, flags = 22, fuse_ticks = 60, projectile_speed_q8 = 512 }
})

vox.define("weapon", "weapon.smoke_pot", {
    order = 202, title = "SMOKE POT", category = "TOOLS & WEAPONS",
    summary = "A thrown canister that fills broken space with moving sight-blocking gas.",
    detail = "After a short fuse the pot deposits smoke rather than a damaging blast. Pressure sends several density bands through nearby air and tunnels; the cloud then rises and thins under normal gas rules. Use it to interrupt bot sight, conceal a rope route, or reveal airflow around hidden cavities.",
    tags = "utility,gas,cover,thrown",
    values = { blast_radius = 4, cooldown_ticks = 35, damage = 0, damage_flags = 0, flags = 26, fuse_ticks = 45, projectile_speed_q8 = 640 }
})

vox.define("weapon", "weapon.cinder_flask", {
    order = 203, title = "CINDER FLASK", category = "TOOLS & WEAPONS",
    summary = "A thrown vessel of hot coals that spreads irregular fire through fuel and wounds.",
    detail = "The flask breaks into multiple hot emissive deposits. Each cinder keeps its own motion and temperature, igniting biomass, coal, firedamp, flesh, or blood it reaches. Water cools individual cinders. Heat damage can cauterize a wound even while it injures the victim, changing the later bleedout.",
    tags = "heat,fire,deposit,thrown",
    values = { blast_radius = 4, cooldown_ticks = 30, damage = 12, damage_flags = 8, flags = 26, fuse_ticks = 40, projectile_speed_q8 = 768 }
})

vox.define("weapon", "weapon.pressure_hose", {
    order = 204, title = "PRESSURE HOSE", category = "TOOLS & WEAPONS",
    summary = "Rapid water jets cool hazards, push light matter, reveal flow, and chip exposed targets.",
    detail = "Each pulse launches a short-lived water parcel without ballistic drop. Repeated jets extinguish fuel, harden the edge of lava, displace smoke, carry blood and loose soil, and apply small impact damage. It is a traversal and rescue tool as often as a weapon.",
    tags = "water,utility,cooling,rapid",
    values = { blast_radius = 0, cooldown_ticks = 4, damage = 4, damage_flags = 2, flags = 10, fuse_ticks = 12, projectile_speed_q8 = 1536 }
})

vox.define("weapon", "weapon.sledge", {
    order = 205, title = "SLEDGE", category = "TOOLS & WEAPONS",
    summary = "Slow close-range impact with brutal knockback and broad structural damage.",
    detail = "The sledge transfers a wide blunt impulse into fighters and terrain. It is ideal for launching an enemy into lava, breaking a narrow support, or converting cracked stone into a collapse. Limb hits carry a high sever threshold without adding heat or a projectile cloud.",
    tags = "melee,knockback,structural",
    values = { blast_radius = 4, cooldown_ticks = 28, damage = 48, damage_flags = 2, flags = 1, fuse_ticks = 0, projectile_speed_q8 = 0 }
})

vox.define("weapon", "weapon.nail_gun", {
    order = 206, title = "NAIL GUN", category = "TOOLS & WEAPONS",
    summary = "Precise rapid metal projectiles for anatomy damage, hot ricochets, and pinning pressure.",
    detail = "A nail flies nearly flat and resolves against the exact body segment or terrain cell it reaches. Nails can remain as hot metal debris after impact and may ignite firedamp or fuel if heated by another event. Sustained fire is accurate but gives up the area control of explosives.",
    tags = "ballistic,rapid,precision,metal",
    values = { blast_radius = 0, cooldown_ticks = 5, damage = 18, damage_flags = 1, flags = 2, fuse_ticks = 40, projectile_speed_q8 = 2048 }
})

vox.define("weapon", "weapon.boiler_shotgun", {
    order = 207, title = "BOILER SHOTGUN", category = "TOOLS & WEAPONS",
    summary = "A close-range fan of deterministic pellets with varied spread, debris, and limb outcomes.",
    detail = "Six pellets derive their spread from match seed, shot sequence, and pellet index, so the pattern varies without desynchronizing. Near targets can absorb hits across several anatomy parts; missed pellets chip terrain and seed metal sparks. Recoil gives the shooter a small opposite impulse.",
    tags = "ballistic,spread,close-range",
    values = { blast_radius = 2, cooldown_ticks = 36, damage = 14, damage_flags = 1, flags = 2, fuse_ticks = 20, pellet_count = 6, projectile_speed_q8 = 1792 }
})

vox.define("weapon", "weapon.concussion_grenade", {
    order = 208, title = "CONCUSSION GRENADE", category = "TOOLS & WEAPONS",
    summary = "Large pressure radius, modest damage, severe knockback, and extensive support wake-up.",
    detail = "The grenade follows a high arc and detonates after its fuse. The blast prioritizes momentum over tissue damage: fighters, debris, loose terrain, fluids, and gases are pushed outward through several deterministic bands. It can break rope tension, peel a roof loose, or launch a protected route into lava.",
    tags = "explosive,impulse,traversal,collapse",
    values = { blast_radius = 10, cooldown_ticks = 46, damage = 38, damage_flags = 6, flags = 22, fuse_ticks = 50, impulse_q16 = 393216, projectile_speed_q8 = 768 }
})

vox.define("weapon", "weapon.nail_bomb", {
    order = 209, title = "NAIL BOMB", category = "TOOLS & WEAPONS",
    summary = "A demolition core wrapped in seeded metal fragments for violent terrain and anatomy outcomes.",
    detail = "The core produces an irregular excavation and pressure wave, then launches deterministic nails across open paths. Fragment count, velocity bands, sampled debris, heat, and anatomy targets derive from the event seed, giving each use a distinct but replayable signature. Metal persists after the smoke clears.",
    tags = "explosive,shrapnel,gore,terrain",
    values = { blast_radius = 8, cooldown_ticks = 56, damage = 62, damage_flags = 5, flags = 22, fragment_count = 24, fuse_ticks = 55, projectile_speed_q8 = 640 }
})
