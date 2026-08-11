<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# DIGS input and bot contract

Movement and combat are submitted through three small deterministic surfaces.
`vox_digs_submit_input` records held movement, jump, steam, rope, fire, bark,
and dash action bits plus the selected weapon for a living player. The game validates
life state, arsenal access, cooldown, charge state, and capacity before
creating the authoritative action. `vox_digs_fire_weapon` remains an explicit
headless/tool boundary, but the SDL host does not own rail-charge time.
`vox_digs_request_respawn` handles an eligible dead player in ON FIRE mode.

At each 60 Hz match tick DIGS applies horizontal run velocity, permits a jump
only when the preceding physics result was grounded, and applies bounded lift
toward a reachable target while steam remains. Steam is a 16-bit resource: it
drains while thrusting and recharges during grounded ticks. Thrust grants mild
lateral authority without replacing momentum; release lets gravity turn the
motion into a glide. The solver then advances the body against the C89
terrain.

Every action below is rebindable per device from the in-game CONTROLS screen,
which is authoritative if it ever disagrees with this list. These are the
defaults the host ships:

- `A`/`D` move P1 and left/right arrows move P2;
- `Space` jumps P1 and up arrow jumps P2; holding jump after leaving the
  ground engages the steampack;
- left Shift activates P1 steam and right Shift activates P2 steam;
- **middle mouse** operates the P1 rope and `/` the P2 rope. Grapple is always
  toggle: press to cast or attach, press again while attached to retarget, and
  jump cancels it. P1 has no keyboard rope binding by default, though one
  remains available through CONTROLS. P1 uses `W`/`S` and P2 up/down while
  attached to reel it;
- mouse position is transformed through the letterboxed logical viewport and
  active camera exactly once to an integer world target;
- `E` or left mouse fires; `1` through `0` and `-` select weapon IDs directly,
  and `Z`/`X` for P1 or `,`/`.` for P2 step through the arsenal;
- the mouse wheel changes the player-locked camera zoom from 1x through 4x;
- ZL/left-trigger plus vertical right-stick input changes the shared camera
  zoom on a controller;
- Shift plus mouse wheel wraps through weapons allowed by the active arsenal
  mask; and
- `C` for P1, `M` for P2, or controller R3 barks.

**Bark is not presentation-only.** It was through v0.0.3. From v0.0.4 it is
`VOX_DIGS_ACTION_BARK` in the authoritative input word, it is folded into the
canonical hash like any other action, and it drives the whole conversation
system: one press produces exactly one line, and the aim vector submitted with
it decides who is being addressed. A port that treats it as a cosmetic
keypress will desync.

Host key repeat, desktop resolution, presentation frame cap, mouse sampling
rate, and Lightfield tier never change the order of authoritative ticks. One
held-input record per player is sampled by the next tick; firing, rail
charge/release, and each physical rope rising edge are bounded deterministic
game actions.

At the beginning of an interactive match the host calls
`vox_digs_dropship_begin` once, before the first tick. That stages every live
miner on the authoritative launch ship while preserving the deterministic
ground spawn as their later respawn target. Fire releases an onboard miner;
bots make the same decision through their normal submitted action bits; and
the route auto-releases anyone still aboard at its far in-world endpoint.
Headless setup and analysis tools can intentionally omit this explicit host
operation when they need a terrain-grounded initial condition. In that case
`vox_digs_match_init` leaves the ship departed and `vox_digs_dropship_step`
does nothing: an omitted host call never creates an invisible collision hull.

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
ready. Bark does enter `vox_digs_input` as `VOX_DIGS_ACTION_BARK`, so its
authored selection and relationship effect are replayable and hashed; only the
host's bubble, audio, and camera response remain presentation-only.
