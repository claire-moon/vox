DIGS v0.0.2 - CG QUICK-START AND TEST GUIDE
===========================================

This is the Linux x86-64 CPU-rendered v0.0.2 demo candidate. DIGS uses the
VOX simulation engine, but the game title is simply DIGS. The game does not
use the GTX 1660 Ti yet; this build is deliberately testing the portable CPU
software path.


1. RUN THE EASY BINARY
----------------------

Extract the complete binary archive. From the extracted directory, run:

  ./run-digs.sh

Keep bin/, share/, libexec/, and the launcher together. The game needs the
Lua manifest and catalog under share/digs/; copying only bin/digs_demo will
produce a safe script-error screen instead of a working title menu.

If Linux reports "Permission denied", run this once and try again:

  chmod +x run-digs.sh smoke-test.sh benchmark.sh qa-cockpit.sh

If SDL2 is missing on Linux Mint, Ubuntu, or Debian:

  sudo apt install libsdl2-2.0-0

The game may continue silently if no usable audio device exists. This does not
change the simulation.


2. START A MATCH
----------------

The title screen contains:

  START MATCH       Configure a deathmatch.
  FOUNDRY LAB       Open the systemic-material sandbox.
  HOW TO PLAY       Read the one-page field manual.
  MINER'S INDEX     Browse every documented game/simulation entity.
  CONTROLS          Inspect or rebind keyboard/controller actions.
  OPTIONS           Set frame cap, effects, accessibility, and display.
  QA FEEDBACK       Open the tester-workbook reminder.
  QUIT              Exit DIGS.

Match setup provides one or two local players, zero through two bots,
Free-for-All or Miners vs Machines, friendly fire, three map styles, a visible
seed, and three arsenal profiles. Four fighters is the current match maximum.

Every spawn receives a five-second shield. Firing or using a weapon ends that
player's protection early.


3. MENU AND CONTROLLER BASICS
-----------------------------

Keyboard menus:

  Arrow keys        Move or change a value.
  Enter             Select.
  Escape            Go back.
  R on seed row     Generate another seed.

Controller menus:

  D-pad/left stick  Move or change a value.
  A or Start        Select.
  B                 Go back.

Controller assignment is automatic and deterministic:

  - With one local player, the first connected controller drives P1.
  - With two local players and one controller, keyboard drives P1 and the
    controller drives P2.
  - With two local players and two controllers, the first two controllers
    drive P1 and P2 respectively.
  - Disconnecting an assigned controller pauses the match safely. Reconnect
    it, then resume to reclaim the slot.

The CONTROLS screen can rebind both keyboard layouts and controller buttons
for this run. Select a row with Enter/A, press the new input, and use RESTORE
DEFAULTS if needed. A conflicting binding swaps with the displaced action.


4. DEFAULT MATCH CONTROLS
-------------------------

Player 1 keyboard and mouse:

  A / D             Move left/right.
  Space             Jump.
  Left Shift        Steam pack.
  Q                 Hold/release rope.
  E or left mouse   Fire/use selected tool.
  Mouse             Aim exactly through the current camera transform.
  W / S             Reel an attached rope shorter/longer.
  1 through 0       Direct-select weapon on key press.
  Mouse wheel       Change camera zoom.
  Shift+wheel       Cycle allowed weapons.

Player 2 keyboard:

  Left / Right      Move left/right.
  Up                Jump.
  Right Shift       Steam pack.
  /                 Hold/release rope.
  Right Ctrl        Fire/use selected tool.
  I / J / K / L     Aim up/left/down/right.
  , / .             Previous/next allowed weapon.

Xbox-style controller defaults:

  Left stick        Move; vertical input reels an attached rope.
  Right stick       Aim.
  A                 Jump.
  X                 Steam pack.
  Left bumper       Hold/release rope.
  Right bumper/RT   Fire/use selected tool.
  Y / B             Previous/next allowed weapon during play.
  Start             Pause.

Global match keys:

  Escape            Pause.
  R                 Restart the current map and seed.
  F1                Toggle the debug overlay.
  F5                Transactionally reload the DIGS Lua data.
  F11               Toggle fullscreen.


5. USING THE ROPE
-----------------

Aim at solid rock, metal, bedrock, or another valid support and hold the rope
action. The line attaches to the first valid anchor in range. Keep moving to
swing; P1 W/S or a controller's vertical left-stick input changes rope length.
Release the rope action to detach while preserving momentum. A destroyed
anchor breaks the rope, so blasts can cut both escape routes and bot routes.

The generated maps include extra rock and beam anchors. You can also use the
terrain tools to open, close, or reshape traversal routes.


6. COMBAT, EFFECTS, AND ACCESSIBILITY
-------------------------------------

The ten-tool Full Works arsenal remains:

  1  Pick                         6  Sledge
  2  Blast Charge                 7  Nail Gun
  3  Smoke Pot                    8  Boiler Shotgun
  4  Cinder Flask                 9  Concussion Grenade
  5  Pressure Hose                0  Nail Bomb

Explosions, impacts, deaths, blood, smoke, embers, fragments, and bot barks
use seeded variants. RETRO, STANDARD, and CARNAGE set explicit bounded pools
of 768, 1536, or 3072 simulated FX voxels. The chosen profile is part of the
match rules and state hash, so compare deterministic results only when the
seed, inputs, rules, scripts, and FX profile all match. Lower profiles trade
particle density for CPU headroom; they are not competitive replay equivalents.

OPTIONS also provides Full/Reduced/Off local-event flashes, Full/Reduced/Off
gore, camera shake, damage numbers, number size/color, Lightfield tier, and
30/60/90/120/144/Unlimited presentation caps. White explosion flashes and red
kill flashes are limited to events involving a local player. Choose Reduced
or Off if flashes are uncomfortable.

The shared camera follows all local players, adds bounded action/movement
look-ahead, and uses momentum, contextual zoom, and optional shake. Mouse aim
is transformed through the same camera and shake values used to draw the
software cursor.


7. MINER'S INDEX AND LUA DATA
-----------------------------

MINER'S INDEX is a read-only field manual generated from the shipped Lua
catalog. Up/Down browses six visible entries at a time; Escape/B returns. The
right pane shows the selected material, weapon, entity, anatomy part, reaction,
mode, AI state, effects/audio system, or rule without another selection step.

The runtime is a bounded Lua 5.1 sandbox. It has no file, operating-system,
package, debug, random, or dynamic-code APIs. F5 validates a replacement data
set before swapping it in; a rejected reload leaves the last good catalog
active. If initial data cannot load, F5 retries and Escape exits safely.

Validate the exact packaged data from a terminal:

  ./bin/digs_script --validate share/digs/scripts/manifest.txt
  ./bin/digs_script --hash share/digs/scripts/manifest.txt
  ./bin/digs_script --headless share/digs/scripts/manifest.txt

The first two commands should report the same source/catalog hashes and 79
catalog entries. --headless also prints the complete entry list.


8. QUICK AUTOMATED TESTS
------------------------

Run these from the extracted demo directory:

  ./smoke-test.sh
  ./benchmark.sh 240
  ./bin/vox_headless
  ./bin/digs_headless
  ./bin/vox_render_demo qa/out/vox-render-test.ppm

The smoke test is non-windowed and writes qa/out/digs-demo-smoke.ppm. The
benchmark reports Compatibility, Balanced, and Showcase CPU Lightfield
results. Hashes must repeat for the same frozen build; use the values printed
by this archive's evidence logs rather than old v0.0.1 values.

The package-time versions of these checks, CTest, Cargo tests, script
validation, and raw exit statuses are under evidence/.


9. MANUAL v0.0.2 PLAYTEST
-------------------------

Please check at least these paths:

  [ ] Keyboard-only P1 can cross each map, jump, steam, rope, aim, and fire.
  [ ] One controller works for P1; one pad plus keyboard works for P1/P2.
  [ ] Two controllers remain isolated to their assigned local players.
  [ ] Controller disconnect pauses safely and reconnect restores control.
  [ ] Rebinding, conflict swaps, and RESTORE DEFAULTS work for the run.
  [ ] Rope attaches only to valid solid material, reels, swings, detaches,
      preserves release momentum, and breaks when its anchor is destroyed.
  [ ] Five-second spawn protection is visible and attacking cancels it.
  [ ] Free-for-All and Miners vs Machines honor friendly-fire selection.
  [ ] Bot roaming/searching/attacking/retreating behavior is observable.
  [ ] Camera frames both humans and remains smooth at every frame cap.
  [ ] Hardware pointer, software cursor, and P1 world aim stay aligned.
  [ ] Each FX profile changes density without changing terrain or damage.
  [ ] Repeated explosions and deaths visibly vary and collapse supports.
  [ ] Gore, flashes, camera shake, and damage-number options work.
  [ ] Local hit/kill feedback triggers; unrelated bot fights do not flash.
  [ ] Dual-chip 8-bit sound varies by event or fails safely to silence.
  [ ] MINER'S INDEX has 79 readable entries, six rows, arrows, and back only.
  [ ] F5 reloads valid Lua data and rejects invalid data without corruption.
  [ ] Rising lava, water cooling, fire, firedamp, liquids, and collapse work.
  [ ] Pause/results/restart/fullscreen/quit paths remain stable.


10. RECORD AND SHARE QA FEEDBACK
--------------------------------

The included 51-checkpoint workbook is:

  qa/VOX_QA_FEEDBACK.xlsx

Open it with LibreOffice, Excel, OnlyOffice, or another XLSX editor. Fill in:

  Checkpoints   One result for each test you actually performed.
  Issues        One row for each distinct problem.
  Environment   Build and general test-bench information.

Use the workbook choices Pass, Fail, Blocked, Skip, or Not Run honestly. For
defects, use severity BLOCKER, HIGH, MEDIUM, LOW, or NOTE. Include the seed,
tick/debug values,
controller model and assignment, expected result, actual result, and minimal
reproduction steps whenever they matter.

If xleak is installed, inspect the workbook in a terminal:

  xleak qa/VOX_QA_FEEDBACK.xlsx -i

Create a complete local evidence packet with:

  ./qa-cockpit.sh

The report and tar.gz packet appear under qa/out/. Nothing is uploaded
automatically. Review the report, logs, workbook, screenshots, and system
summary for private information before sharing them.

Submit reviewed feedback here:

  https://github.com/claire-moon/vox/issues/new?template=demo-feedback.yml


11. BUILD AND TEST FROM SOURCE
------------------------------

From the VOX repository root:

  cmake -S . -B build-demo \
    -DVOX_BUILD_TESTS=ON \
    -DVOX_BUILD_SDL2_DEMO=ON
  cmake --build build-demo --parallel
  ctest --test-dir build-demo --output-on-failure
  cargo test --workspace
  ./build-demo/digs_script --validate \
    ./build-demo/share/digs/scripts/manifest.txt
  ./build-demo/digs_demo

Run the complete strict-language verification gate with:

  tools/vox-verify.sh

Create clean Linux tester archives with:

  tools/package-linux-demo.sh

For an explicitly non-release package from a dirty development tree only:

  VOX_PACKAGE_ALLOW_DIRTY=1 \
  VOX_PACKAGE_VERSION=v0.0.2-dev \
  tools/package-linux-demo.sh


12. VERIFY THE DOWNLOAD AND KNOW THE LIMITS
--------------------------------------------

From the parent dist directory:

  sha256sum -c SHA256SUMS

Both archives must report OK. The binary bundle contains SBOM.spdx.json, raw
package-time evidence, GPL notices, the Lua catalog, and launch/test scripts.
The matching Corresponding Source archive is distributed beside it.

v0.0.2 is still the Linux CPU-software vertical slice. Windows, macOS,
historical operating systems, a GPU renderer, network multiplayer, a complete
rigid-body solver, persistent input settings, and final competitive bot
quality remain planned/evidence-gated work. Record only what you observe and
do not infer GPU acceleration from the laptop containing a GTX 1660 Ti.
