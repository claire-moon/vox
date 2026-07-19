-- SPDX-License-Identifier: GPL-3.0-or-later

vox.define("reaction", "reaction.water_lava", {
    order = 500, title = "WATER + LAVA", category = "REACTIONS",
    summary = "Cooling consumes both fluids, creates stone, and vents hot smoke or steam.",
    detail = "Adjacent water drains heat from lava. A cooled lava cell hardens into stone while part of the water becomes rising gas. The new stone may bridge a gap or become an unsupported falling mass, so a pressure-hose rescue can also produce a later collapse.",
    tags = "water,lava,stone,steam",
    values = { hot_threshold_q16 = 20971520, product_material = 2, radius = 1 }
})

vox.define("reaction", "reaction.heat_water", {
    order = 501, title = "HEAT + WATER", category = "REACTIONS",
    summary = "Sufficient heat converts water into rising gas and rapidly cools its source.",
    detail = "Water next to hot cinders, metal, stone, or lava absorbs energy. Above the steam threshold the parcel is replaced by smoke-like vapor and the hot neighbor loses temperature. Confined conversion briefly increases local motion.",
    tags = "heat,water,steam,cooling",
    values = { product_material = 12, steam_q16 = 9175040 }
})

vox.define("reaction", "reaction.fire_biomass", {
    order = 502, title = "FIRE + BIOMASS", category = "REACTIONS",
    summary = "Flame advances cell by cell through grass, roots, and timber while removing support.",
    detail = "A biomass cell above ignition emits heat and smoke, damages adjacent biomass, and weakens itself until consumed. Direction depends on connected fuel and airflow, so narrow strands can carry fire into an otherwise isolated pocket.",
    tags = "fire,biomass,smoke,collapse",
    values = { fuel_material = 5, ignition_q16 = 19660800, smoke_material = 12 }
})

vox.define("reaction", "reaction.fire_coal", {
    order = 503, title = "FIRE + COAL", category = "REACTIONS",
    summary = "A slow, hot seam burn spreads smoke and undermines strong terrain.",
    detail = "Coal needs more heat than biomass but burns longer and hotter. Connected seams propagate the reaction, wake adjacent cells, and may ignite firedamp. Destroyed coal removes structural support, causing delayed cave-ins that follow irregular seam geometry.",
    tags = "fire,coal,heat,collapse",
    values = { fuel_material = 4, ignition_q16 = 29491200, smoke_material = 12 }
})

vox.define("reaction", "reaction.spark_firedamp", {
    order = 504, title = "SPARK + FIREDAMP", category = "REACTIONS",
    summary = "Hot matter ignites a connected gas pocket into a seeded pressure chain.",
    detail = "Any firedamp cell above ignition can consume nearby firedamp and emit pressure, heat, and smoke. Pocket shape controls propagation and open tunnels vent force, so the result is not a repeated circular animation. Nails, cinders, coal fire, lava, and explosions can all initiate it.",
    tags = "firedamp,gas,explosion,chain",
    values = { fuel_material = 13, ignition_q16 = 16384000, product_material = 12 }
})

vox.define("reaction", "reaction.blast_support", {
    order = 505, title = "BLAST + SUPPORT", category = "REACTIONS",
    summary = "Excavation wakes a broad structural skirt so disconnected terrain falls and breaks further ground.",
    detail = "A blast removes cells using seeded radial damage rather than a perfect mask. It then wakes surviving cells beyond the crater. Unsupported stone falls as chunks; soil and sand crumble more freely; collision damage can open a second cavity without another weapon event.",
    tags = "explosion,terrain,collapse,debris",
    values = { max_radius = 16, wake_margin = 4 }
})

vox.define("reaction", "reaction.blood_surface", {
    order = 506, title = "BLOOD + SURFACE", category = "REACTIONS",
    summary = "Blood deposits a darkening stain and temporarily lowers traction on contacted terrain.",
    detail = "A settling blood voxel marks its supporting cell with stain amount and age. Fresh accumulation reduces horizontal traction for fighters and loose objects. The stain darkens as it dries; water can thin and move it while heat accelerates drying.",
    tags = "blood,stain,traction,terrain",
    values = { darken_ticks = 600, traction_q8 = 192 }
})

vox.define("reaction", "reaction.water_blood", {
    order = 507, title = "WATER + BLOOD", category = "REACTIONS",
    summary = "Flow dilutes blood, spreads lighter stains, and transports particles through the mine.",
    detail = "Water reduces local blood concentration and carries it along the fluid path. Diluted cells create weaker stains and less traction loss. This is a material transfer, not a visual fade, so pumping water through a scene physically relocates the evidence.",
    tags = "water,blood,fluid,stain",
    values = { dilution_q8 = 128, stain_q8 = 96 }
})

vox.define("reaction", "reaction.heat_wound", {
    order = 508, title = "HEAT + OPEN WOUND", category = "REACTIONS",
    summary = "High heat can cauterize bleeding anatomy while still applying destructive tissue damage.",
    detail = "An open part exposed above the cautery threshold stops emitting blood and gains the cauterized state. The same event applies heat damage and can destroy a low-health part, so cinders and lava create a tradeoff rather than a free heal.",
    tags = "heat,flesh,bleeding,cautery",
    values = { cautery_q16 = 26214400, stops_bleed = 1 }
})

vox.define("reaction", "reaction.smoke_sight", {
    order = 509, title = "SMOKE + SIGHT", category = "REACTIONS",
    summary = "Dense gas interrupts fair bot line of sight and softens player visibility.",
    detail = "Sight tests sample the same world cells used by rendering. Enough smoke between observer and target breaks direct perception; the bot searches the last seen position until memory expires or a new sound redirects it. No hidden target coordinate is exposed.",
    tags = "smoke,ai,visibility,cover",
    values = { block_samples = 3, memory_ticks = 180 }
})

vox.define("reaction", "reaction.metal_heat", {
    order = 510, title = "METAL + HEAT", category = "REACTIONS",
    summary = "Conductive nails and fragments carry heat into fuel, fluids, and anatomy.",
    detail = "Metal absorbs heat efficiently and retains it while moving. A hot nail can ignite firedamp or biomass, steam a water parcel, or cauterize a wound after the ballistic impact. The interaction allows one event to cross damage paradigms without a scripted special case.",
    tags = "metal,heat,conduction,chain",
    values = { conductivity = 800, material = 9 }
})

vox.define("reaction", "reaction.rope_anchor_loss", {
    order = 511, title = "ROPE + ANCHOR LOSS", category = "REACTIONS",
    summary = "Destroying or destabilizing the anchor immediately releases the cable with preserved momentum.",
    detail = "Each active rope verifies its anchor cell and support every simulation tick. If the cell becomes air, fluid, gas, flesh, or unstable falling terrain, the rope detaches and emits a break event. This makes terrain destruction a direct counterplay tool.",
    tags = "rope,terrain,break,traversal",
    values = { check_ticks = 1, preserve_velocity = 1 }
})
