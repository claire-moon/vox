<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# DIGS input and bot contract

Movement and combat are submitted through three small deterministic surfaces.
`vox_digs_submit_input` records held movement, jump, steam, rope, and fire
action bits plus the selected weapon for a living player. The game validates
life state, arsenal access, cooldown, charge state, and capacity before
creating the authoritative action. `vox_digs_fire_weapon` remains an explicit
headless/tool boundary, but the SDL host does not own rail-charge time.
`vox_digs_request_respawn` handles an eligible dead player in ON FIRE mode.

At each 60 Hz match tick DIGS applies horizontal run velocity, permits a jump
only when the preceding physics result was grounded, and applies bounded lift
toward a reachable target while steam remains. Steam is a 16-bit resource: it
drains while thrusting and recharges during grounded ticks. Thrust grants mild
lateral authority without replacing momentum; release lets gravity turn the
motion into a glide. The C++98 solver then advances the body against the C89
terrain.

The SDL2 host maps controls as follows:

- `A`/`D` move P1 and left/right arrows move P2;
- `Space` jumps P1 and up arrow jumps P2;
- left Shift activates P1 steam and right Shift activates P2 steam;
- `Q` for P1 and `/` for P2 operate the rope under per-player Hold/Toggle
  policy; P1 uses `W`/`S` and P2
  uses up/down while attached to reel it;
- mouse position is transformed through the letterboxed logical viewport and
  active camera exactly once to an integer world target;
- left mouse fires; `1` through `0` select the original weapon IDs and `-`
  selects the Mining Rail; and
- the mouse wheel changes the player-locked camera zoom from 1x through 4x;
- ZL/left-trigger plus vertical right-stick input changes the shared camera
  zoom on a controller;
- Shift plus mouse wheel wraps through weapons allowed by the active arsenal
  mask; and
- `C` for P1, `M` for P2, or controller R3 requests a presentation-only bark.

Host key repeat, desktop resolution, presentation frame cap, mouse sampling
rate, and Lightfield tier never change the order of authoritative ticks. One
held-input record per player is sampled by the next tick; firing, rail
charge/release, and rope cast level are bounded deterministic game actions.

Bots produce the same held-action bits consumed by player control and use the
same weapon path; they have no alternate physics, damage, or
terrain-edit path. They evaluate on a fixed tick cadence, select targets by
stable player-slot rules, and derive movement, aim, and weapon choice from
match state. This makes the demo bots repeatable acceptance actors rather than
a separate privileged simulation. They intentionally do not provide navigation
meshes, learned behavior, hidden world knowledge, difficulty scaling, or
production competitive AI.

The public input and fire records are ABI-versioned. A future replay or network
transport should serialize these commands plus required setup metadata, not
body transforms or presentation events.

AUTO and locked keyboard/controller ownership are host policies and remain
outside the match hash. In ON FIRE respawn mode the host requires Fire to be
released after death and pressed again once the authoritative countdown is
ready. Bark input never enters `vox_digs_input`: the portable feedback layer
selects a deterministic phrase from current state without changing gameplay.
