-- SPDX-License-Identifier: GPL-3.0-or-later
-- Material identifiers match vox_material_id; temperatures use Q16 Celsius.

vox.define("material", "material.air", {
    order = 100, title = "AIR", category = "MATERIALS",
    summary = "Empty simulation volume that accepts solids, fluids, gases, particles, and light.",
    detail = "Air contributes no mass or strength. An air cell can receive falling matter, flowing liquid, expanding gas, a deposited effect, or emitted light. Clearing terrain restores air and wakes nearby support checks.",
    tags = "gas,empty,volume",
    values = { conductivity = 0, density = 0, flags = 0, id = 0, ignition_q16 = 0, melt_q16 = 0, strength = 0 }
})

vox.define("material", "material.bedrock", {
    order = 101, title = "BEDROCK", category = "MATERIALS",
    summary = "The immutable boundary and final structural anchor of a generated mine.",
    detail = "Bedrock cannot be mined, melted, displaced, or destroyed by shipped tools. It terminates blast excavation and provides a permanent rope anchor. The bottom two terrain rows are bedrock so collapse and rising lava retain a bounded world.",
    tags = "solid,anchor,indestructible",
    values = { conductivity = 65535, density = 65535, flags = 16, id = 1, ignition_q16 = 0, melt_q16 = 0, strength = 65535 }
})

vox.define("material", "material.stone", {
    order = 102, title = "STONE", category = "MATERIALS",
    summary = "Heavy load-bearing terrain that fractures into dangerous falling fragments.",
    detail = "Stone strongly supports neighboring cells and resists small arms, but focused mining and explosives accumulate damage. Once disconnected from support it wakes, falls, collides, and can crush fighters or weaker terrain. Heat eventually melts it, though no ordinary match reaches that point cheaply.",
    tags = "solid,structural,debris",
    values = { conductivity = 400, density = 2400, flags = 16, id = 2, ignition_q16 = 0, melt_q16 = 65536000, strength = 500 }
})

vox.define("material", "material.soil", {
    order = 103, title = "SOIL", category = "MATERIALS",
    summary = "Common diggable terrain with modest support and thick loose debris.",
    detail = "Soil yields quickly to picks, sledges, bullets, and blast pressure. Unsupported shelves crumble into granular chunks, making irregular craters and secondary cave-ins. Water can carry loose soil down slopes while extreme heat bakes or removes it.",
    tags = "solid,structural,loose",
    values = { conductivity = 120, density = 1400, flags = 16, id = 3, ignition_q16 = 0, melt_q16 = 32768000, strength = 80 }
})

vox.define("material", "material.coal", {
    order = 104, title = "COAL", category = "MATERIALS",
    summary = "Brittle fuel seam that turns heat and poor ventilation into a weapon.",
    detail = "Coal supports light loads but breaks more easily than stone. At ignition temperature it burns, emits heat and smoke, and can ignite adjacent biomass or firedamp. Burning seams undermine terrain over time, producing collapses after the initiating shot has ended.",
    tags = "solid,flammable,fuel",
    values = { conductivity = 180, density = 1300, flags = 17, id = 4, ignition_q16 = 29491200, melt_q16 = 39321600, strength = 120 }
})

vox.define("material", "material.biomass", {
    order = 105, title = "BIOMASS", category = "MATERIALS",
    summary = "Grass, roots, timber, and other light organic structures that burn strand by strand.",
    detail = "Biomass is the weakest solid terrain and breaks into light particles. Heat ignites individual cells, so fire crawls through roots and surface grass rather than deleting a perfect region. Water cools and suppresses the burn; lava consumes it immediately.",
    tags = "solid,flammable,grass,wood",
    values = { conductivity = 100, density = 900, flags = 17, id = 5, ignition_q16 = 19660800, melt_q16 = 29491200, strength = 60 }
})

vox.define("material", "material.sand", {
    order = 106, title = "SAND", category = "MATERIALS",
    summary = "Dense granular terrain that avalanches through openings and buries low ground.",
    detail = "Sand has almost no cohesive strength. Exposed cells fall, spread, and settle, continually re-evaluating after nearby destruction. Blasts throw hot grains through the mine; water redirects them into heavier flows. It is solid for collision but fluid-like for support.",
    tags = "granular,solid,fluid-like",
    values = { conductivity = 80, density = 1600, flags = 18, id = 6, ignition_q16 = 0, melt_q16 = 65536000, strength = 20 }
})

vox.define("material", "material.water", {
    order = 107, title = "WATER", category = "MATERIALS",
    summary = "Cooling fluid that flows through damage, suppresses fire, and hardens lava.",
    detail = "Water seeks open cells below and then sideways. It carries heat away from terrain, extinguishes biomass and coal, dilutes hot blood, and reacts with lava into stone plus steam. Pools change traversal and can redirect loose sand and soil.",
    tags = "fluid,cooling,reaction",
    values = { conductivity = 600, density = 1000, flags = 2, id = 7, ignition_q16 = 0, melt_q16 = 6553600, strength = 0 }
})

vox.define("material", "material.lava", {
    order = 108, title = "LAVA", category = "MATERIALS",
    summary = "Rising emissive fluid that burns fighters and turns the match into a shrinking route puzzle.",
    detail = "Lava flows slowly, emits local light, ignites fuel, cauterizes flesh, and fries an unshielded fighter on contact. Water cools it into stone while generating steam and smoke. Match lava rises after the configured delay, invalidating camps and low tunnels.",
    tags = "fluid,emissive,heat,hazard",
    values = { conductivity = 900, density = 3000, flags = 10, id = 8, ignition_q16 = 0, melt_q16 = 42598400, strength = 0 }
})

vox.define("material", "material.metal", {
    order = 109, title = "METAL", category = "MATERIALS",
    summary = "Dense conductive solid used by nails, tools, machinery, and reinforced minework.",
    detail = "Metal is heavy, strong, and difficult to excavate. It transfers heat efficiently, allowing hot fragments and embedded nails to ignite nearby fuel or cauterize tissue. Metal debris preserves momentum and is especially dangerous after a nail bomb.",
    tags = "solid,conductive,projectile",
    values = { conductivity = 800, density = 7800, flags = 16, id = 9, ignition_q16 = 0, melt_q16 = 98304000, strength = 500 }
})

vox.define("material", "material.flesh", {
    order = 110, title = "FLESH", category = "MATERIALS",
    summary = "Segmented body matter that can tear free, burn, collide, and continue through the material simulation.",
    detail = "Living anatomy maps damage into flesh segments. Severed tissue becomes physical debris with inherited velocity, can block tiny spaces, floats or sinks according to local flow, chars near heat, and emits blood from open wounds until its source clots or cauterizes.",
    tags = "solid,flammable,anatomy,gore",
    values = { conductivity = 80, density = 1050, flags = 17, id = 10, ignition_q16 = 26214400, melt_q16 = 32768000, strength = 40 }
})

vox.define("material", "material.blood", {
    order = 111, title = "BLOOD", category = "MATERIALS",
    summary = "Systemic fluid emitted by wounds that stains terrain and changes traction.",
    detail = "Blood particles collide, pool, run downhill, stain contacted solids, and darken as they age. Wet blood lowers traction; water thins and transports it; high heat darkens, evaporates, or cauterizes the source. It is simulation matter rather than a screen-space decal.",
    tags = "fluid,gore,stain,traction",
    values = { conductivity = 120, density = 1050, flags = 2, id = 11, ignition_q16 = 0, melt_q16 = 6553600, strength = 10 }
})

vox.define("material", "material.smoke", {
    order = 112, title = "SMOKE", category = "MATERIALS",
    summary = "Light gas that rises, spreads, obscures sight, and records where heat has moved.",
    detail = "Smoke expands into air, drifts upward, and thins over time. Dense smoke blocks bot line of sight and gives players temporary visual cover. It is displaced by pressure, created by fire and cooled lava, and illuminated by nearby emissive particles.",
    tags = "gas,visibility,fire",
    values = { conductivity = 20, density = 10, flags = 4, id = 12, ignition_q16 = 0, melt_q16 = 0, strength = 0 }
})

vox.define("material", "material.firedamp", {
    order = 113, title = "FIREDAMP", category = "MATERIALS",
    summary = "Nearly weightless flammable mine gas that turns enclosed sparks into chain blasts.",
    detail = "Firedamp occupies open cave pockets and moves like a gas. Heat, flame, hot metal, or a nearby explosion ignites it above threshold, consuming the pocket and producing pressure, smoke, and secondary terrain damage. Venting a pocket before firing is often the safer play.",
    tags = "gas,flammable,explosive,mine",
    values = { conductivity = 10, density = 2, flags = 5, id = 13, ignition_q16 = 16384000, melt_q16 = 0, strength = 0 }
})
