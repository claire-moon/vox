-- SPDX-License-Identifier: GPL-3.0-or-later
-- Flags: melee 1, projectile 2, explosive 4, deposit 8, gravity 16,
-- hitscan 32, penetrating 64.

vox.define("weapon", "weapon.pick", {
    order = 200, title = "PULASKI", category = "TOOLS & WEAPONS",
    summary = "Fast miner axe: tap to carve, hold to throw a returning blade.",
    detail = "A quick swing cuts soil and opens a bleeding anatomy wound. Hold fire to charge a boomerang throw: the Pulaski arcs through terrain and miners, then returns to its owner. Limb hits can sever the struck chain into physical gibs.",
    tags = "melee,bleed,returning,mining",
    values = { blast_radius = 2, charge_ticks = 30, cooldown_ticks = 8, damage = 32, damage_flags = 1, flags = 17, fuse_ticks = 0, projectile_speed_q8 = 1152 }
})

vox.define("weapon", "weapon.blast_charge", {
    order = 201, title = "POPPER", category = "TOOLS & WEAPONS",
    summary = "Tiny rapid pistol with a short hitscan explosive round.",
    detail = "The Popper fires immediately along the aim line. Its little blast chips a confined hole, rattles nearby miners, and deals low damage quickly instead of turning every shot into a suicide crater.",
    tags = "hitscan,rapid,explosive,sidearm",
    values = { blast_radius = 2, cooldown_ticks = 6, damage = 18, damage_flags = 5, flags = 36, fuse_ticks = 0, projectile_speed_q8 = 0 }
})

vox.define("weapon", "weapon.smoke_pot", {
    order = 202, title = "SMOKER", category = "TOOLS & WEAPONS",
    summary = "A bouncing canister that drags a thick rolling smoke trail behind it.",
    detail = "The Smoker does not harm miners unless its metal canister actually clips them. It bounces, emits seeded smoke bands while moving, and leaves a denser cloud when its fuse expires. Use it to break sight lines and hide a rope route.",
    tags = "utility,gas,cover,thrown",
    values = { blast_radius = 5, cooldown_ticks = 32, damage = 1, damage_flags = 2, flags = 26, fuse_ticks = 90, projectile_speed_q8 = 768 }
})

vox.define("weapon", "weapon.cinder_flask", {
    order = 203, title = "HOT RAIL", category = "TOOLS & WEAPONS",
    summary = "Short-range industrial blowtorch that melts a lane through fuel and flesh.",
    detail = "Hold the Hot Rail on a nearby wall to scorch and soften it; biomass and coal can catch. On a miner it cauterizes while it burns, making a short range commitment instead of a thrown hazard.",
    tags = "heat,fire,beam,terrain",
    values = { blast_radius = 2, cooldown_ticks = 5, damage = 10, damage_flags = 8, flags = 40, fuse_ticks = 0, projectile_speed_q8 = 0 }
})

vox.define("weapon", "weapon.pressure_hose", {
    order = 204, title = "HYDROSHOT", category = "TOOLS & WEAPONS",
    summary = "A forceful water burst that cools hazards and throws miners without hurting them.",
    detail = "Each Hydroshot packet carries water into lava, burning terrain, smoke, loose soil, and bodies. It deals no anatomy damage, but a direct hit gives a strong shove for movement, rescue, or a lava setup.",
    tags = "water,utility,cooling,rapid",
    values = { blast_radius = 0, cooldown_ticks = 7, damage = 0, damage_flags = 2, flags = 10, fuse_ticks = 14, projectile_speed_q8 = 1664 }
})

vox.define("weapon", "weapon.sledge", {
    order = 205, title = "GIANT FUCKING HAMMER", category = "TOOLS & WEAPONS",
    summary = "Slow demolition hammer: crater, shockwave, recoil, and catastrophic close hits.",
    detail = "The giant hammer caves a broad hole, shakes loose terrain, and throws nearby miners away from the impact. It can annihilate a miner in its direct path, while its recoil throws the wielder backward too. Mind the ledges.",
    tags = "melee,knockback,structural",
    values = { blast_radius = 7, cooldown_ticks = 50, damage = 100, damage_flags = 2, flags = 5, fuse_ticks = 0, projectile_speed_q8 = 0 }
})

vox.define("weapon", "weapon.nail_gun", {
    order = 206, title = "BOLT ACTION", category = "TOOLS & WEAPONS",
    summary = "Hold for a scalding, terrain-boring bolt that can split a miner in one hit.",
    detail = "Charge for half a second, then release a hot industrial bolt. It bores through several soft cells and resolves an exact anatomy hit with lethal force. Four consecutive bolts overheat the mechanism for a longer cooldown.",
    tags = "ballistic,rapid,precision,metal",
    values = { blast_radius = 2, charge_ticks = 30, cooldown_ticks = 38, damage = 100, damage_flags = 1, flags = 96, fuse_ticks = 0, projectile_speed_q8 = 0 }
})

vox.define("weapon", "weapon.boiler_shotgun", {
    order = 207, title = "SCATTERBRAIN", category = "TOOLS & WEAPONS",
    summary = "A monster double-barrel burst for point-blank terrain holes and red-cloud kills.",
    detail = "Nine seeded pellets tear a wide cone through a nearby wall or miner. The long two-second reload is deliberate: the Scatterbrain should feel like a commitment, not a rapid-fire blanket.",
    tags = "ballistic,spread,close-range",
    values = { blast_radius = 3, cooldown_ticks = 120, damage = 28, damage_flags = 1, flags = 2, fuse_ticks = 20, pellet_count = 9, projectile_speed_q8 = 2048 }
})

vox.define("weapon", "weapon.concussion_grenade", {
    order = 208, title = "FIRECRACKER", category = "TOOLS & WEAPONS",
    summary = "A charged lob grenade whose blast leaves a seeded spread of hot cinders.",
    detail = "Release early for a short toss or hold for a far throw. The Firecracker cracks terrain, pushes bodies, and scatters brief burning cinders that can set fuel at the crater edge alight.",
    tags = "explosive,impulse,traversal,collapse",
    values = { blast_radius = 8, charge_ticks = 60, cooldown_ticks = 48, damage = 42, damage_flags = 6, flags = 22, fuse_ticks = 50, projectile_speed_q8 = 768 }
})

vox.define("weapon", "weapon.nail_bomb", {
    order = 209, title = "BORE DRILL", category = "TOOLS & WEAPONS",
    summary = "A pneumatic down-drill for fast burrows and falling crush kills.",
    detail = "The Bore Drill is aimed straight down. It makes a narrow escape shaft through support material, and a descending miner can drive it directly through an opponent below for a lethal hit.",
    tags = "explosive,shrapnel,gore,terrain",
    values = { blast_radius = 3, cooldown_ticks = 10, damage = 100, damage_flags = 2, flags = 65, fuse_ticks = 0, projectile_speed_q8 = 0 }
})

vox.define("weapon", "weapon.mining_rail", {
    order = 210, title = "MINING RAIL", category = "TOOLS & WEAPONS",
    summary = "A charged industrial rail that drills soft strata and resolves every pierced miner against exact anatomy.",
    detail = "Hold fire for up to 1.2 seconds, then release a deterministic line through loose soil, sand, coal, biomass, bounded stone strata, and exposed miners. Energy falls after every body and material column. Bedrock, metal, and sufficient stone stop the trace. A maximum-charge hit is precision-lethal after exact anatomy resolves; a direct uncharged anatomy primitive can still sever a limb without inventing charge state.",
    tags = "hitscan,charged,penetrating,precision,mining",
    values = { charge_ticks = 72, cooldown_ticks = 75, damage = 100, damage_flags = 1, flags = 96, minimum_damage = 20, penetration = 180 }
})
