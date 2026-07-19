-- SPDX-License-Identifier: GPL-3.0-or-later

vox.define("how_to", "how_to.play", {
    order = 0,
    title = "HOW TO PLAY",
    category = "FIELD MANUAL",
    summary = "Move fast, reshape the mine, and reach the score limit before the lava takes the works.",
    detail = [[DIGS is a live side-view deathmatch, not a turn-based duel. Run, jump, feather the steam pack, and hold the rope to swing from stable terrain. Aim with the pointer or right stick and fire tools into miners, machines, and the world. Every material has weight, heat, strength, and reactions: water cools lava, fire ignites coal and firedamp, blasts undermine supports, and loose terrain collapses. New spawns are protected for five seconds unless they attack. FFA scores individual kills. Miners vs Machines joins the human miners against adaptive machine bots; friendly fire starts off. The match ends at the score limit or timer. Rising lava makes every route temporary.]],
    tags = "controls,objective,spawn,rope,lava",
    values = {
        local_players_max = 2,
        match_slots = 4,
        spawn_shield_ticks = 300,
        ticks_per_second = 60,
        visible_rows = 6
    }
})
