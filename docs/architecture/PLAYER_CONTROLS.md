<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# DIGS input and bot contract

Movement and combat are submitted through two small deterministic surfaces.
`vox_digs_submit_input` records the held left, right, jump, and steam action
bits for a living player. `vox_digs_fire_weapon` accepts a player, weapon ID,
and integer world target; the game validates life state, arsenal access,
cooldown, and capacity before creating the authoritative action.

At each 60 Hz match tick DIGS applies horizontal run velocity, permits a jump
only when the preceding physics result was grounded, and applies bounded upward
steam acceleration while fuel remains. Steam is a 16-bit resource: it drains
while thrusting and recharges during grounded ticks when not held. The C++98
solver then advances the body against the C89 terrain.

The SDL2 host maps controls as follows:

- `A`/`D` and left/right arrows set horizontal actions;
- `W`, up arrow, and `Space` set jump;
- either `Shift` key sets steam;
- mouse position is transformed through the letterboxed logical viewport and
  active camera exactly once to an integer world target;
- left mouse fires; `1` through `0` select the ten weapon IDs; and
- the mouse wheel changes the player-locked camera zoom from 1x through 4x;
- Shift plus mouse wheel wraps through weapons allowed by the active arsenal
  mask.

Host key repeat, desktop resolution, presentation frame cap, mouse sampling
rate, and Lightfield tier never change the order of authoritative ticks. One
held-input record per player is sampled by the next tick; firing is a bounded
game command with a deterministic success or error result.

Bots produce the same held-action bits consumed by player control and call the
same `vox_digs_fire_weapon` command; they have no alternate physics, damage, or
terrain-edit path. They evaluate on a fixed tick cadence, select targets by
stable player-slot rules, and derive movement, aim, and weapon choice from
match state. This makes the demo bots repeatable acceptance actors rather than
a separate privileged simulation. They intentionally do not provide navigation
meshes, learned behavior, hidden world knowledge, difficulty scaling, or
production competitive AI.

The public input and fire records are ABI-versioned. A future replay or network
transport should serialize these commands plus required setup metadata, not
body transforms or presentation events.
