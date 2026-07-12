<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# DIGS player-control foundation

`vox_digs_submit_input` records held actions for a living player. Inputs are
versioned C records and only accept the four deterministic actions: left,
right, jump, and steam.

At each authoritative 60 Hz match tick DIGS applies horizontal run velocity,
allows a jump only when the preceding physics result is grounded, and applies
bounded upward steam acceleration while fuel remains. Steam is a 16-bit
resource: it drains while thrusting and recharges on grounded ticks when the
jet is not held. The C++98 body solver then performs collision against the
C89 voxel terrain.

This is intentionally a small, replayable control surface. Aim, weapons,
camera-relative input, controller bindings, bot decision logic, animation,
and UI binding remain host/game layers. Frame-rate options must never alter
this input-to-simulation sequence.
