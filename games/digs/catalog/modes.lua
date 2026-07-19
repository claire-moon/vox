-- SPDX-License-Identifier: GPL-3.0-or-later

vox.define("mode", "mode.ffa", {
    order = 30,
    title = "FREE-FOR-ALL",
    category = "MATCH MODES",
    summary = "Four-way live deathmatch; each legal kill advances the attacker's score.",
    detail = "Up to two local humans and two adaptive bots share the mine. There are no teams and every other active fighter is hostile. Environmental deaths credit the last valid attacker when possible. First to the score limit wins; otherwise the highest score when the timer expires takes the shift.",
    tags = "ffa,deathmatch,four-player",
    values = { default_score_limit = 10, friendly_fire = 1, slots = 4 }
})

vox.define("mode", "mode.miners_vs_machines", {
    order = 31,
    title = "MINERS VS MACHINES",
    category = "MATCH MODES",
    summary = "Local miners cooperate against a coordinated machine crew.",
    detail = "Human-controlled miners occupy one team and bots occupy the machine team. Friendly fire starts disabled but remains an explicit match option. Both sides use the same physics, perception limits, weapons, rope, anatomy, and spawn protection; machines receive no hidden damage, movement, or vision advantage.",
    tags = "coop,teams,bots,machines",
    values = { default_friendly_fire = 0, teams = 2, slots = 4 }
})
