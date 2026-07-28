DIGS v0.0.3 - CG QUICK-START AND TEST GUIDE
===========================================

This is the Linux x86-64 CPU-rendered v0.0.3 demo candidate. DIGS uses the
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
CUSTOMIZE GAME names every active slot and selects a two- or three-minute
clock, optional Off/5/10/20 score cap, AUTO or ON FIRE respawn, and a
0/1/2/3/5-second delay. The defaults are two minutes, score cap Off, AUTO, and
three seconds. Highest individual FFA score or Miners-versus-Machines team
total wins; an exact tie is a draw.

The 512 x 320 x 10 arena is four times the area of the previous dense demo.
Each visible seed selects an ARCHIPELAGO, CONTINENT, or TWIN HILLS macro
landform inside the chosen material style:

  COAL RIDGE       Broad rolling ground, coal seams, sand drifts, sparse rails.
  DEEPWORKS        Connected chambers, open shafts, firedamp, natural anchors.
  FURNACE YARD     Industrial terraces, broken gantries, contained hot pockets.

The outer drop-offs and island gaps expose the permanent lava basin. Lava is
visible from match start and rises during the endgame. Suspended rope anchors
are sparse and sit 36 through 42 cells above terrain, beyond the complete
walk/jump envelope; they have no ground-to-rail legs and cannot become spawn
floors.

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

Open OPTIONS, then INPUT & CONTROLLER, to choose AUTO, KEYBOARD, or CONTROLLER
for each player. P1's keyboard mode includes the mouse. Menus remain available
from keyboard or controller regardless of gameplay ownership.

AUTO is the default. It switches only after deliberate input and uses a short
hysteresis so stick noise cannot continually steal control. A source change
clears held action edges. Keyboard/mouse and controller gameplay input are never
mixed for one player, so each local player receives exactly one crosshair.

Default controller claims remain deterministic:

  - With one local player, the first pad is available to P1 AUTO.
  - With two local players and one pad, P1 defaults to keyboard/mouse and the
    pad is available to P2.
  - With two local players and two pads, P1 and P2 receive separate pads.
  - AUTO falls back to keyboard when its pad disappears. A player locked to
    CONTROLLER pauses safely on disconnect instead of accepting stray input.

INPUT & CONTROLLER also provides Low/Normal/High sensitivity, Auto/Small/
Normal/Large radial deadzones, Off/Low/Medium aim slowdown, calibration, and
input-default restoration. These preferences are stored in DIGS's per-user SDL
settings directory and survive a restart. Keep all sticks still during the
750 ms calibration sample.

DIGS loads the bundled SDL mapping database before device enumeration. Known
pads use SDL's logical layout; unknown devices open through the raw joystick
fallback. For the Logitech F310, test both rear-switch positions: unplug it,
select X or D, reconnect it, calibrate with the sticks centered, and record the
mode in QA feedback.

The CONTROLS screen can rebind both keyboard layouts and controller buttons.
Bindings persist in DIGS's per-user settings file. Select a row with Enter/A,
press the new input, and use RESTORE DEFAULTS if needed. A conflicting binding
swaps with the displaced action.


4. DEFAULT MATCH CONTROLS
-------------------------

Player 1 keyboard and mouse:

  A / D             Move left/right.
  Space             Jump.
  Left Shift        Steam pack.
  Right mouse       Operate rope in the selected Hold/Toggle mode.
  E or left mouse   Fire/use selected tool.
  Mouse             Aim exactly through the current camera transform.
  W / S             Reel an attached rope shorter/longer.
  1 through 0       Direct-select the ten tools.
  -                 Direct-select the charged Mining Rail.
  Mouse wheel       Change camera zoom.
  Shift+wheel       Cycle allowed weapons.
  C                 Speak a contextual miner bark.

Player 2 keyboard:

  Left / Right      Move left/right.
  Up                Jump; reel shorter while attached.
  Down              Reel longer while attached.
  Right Shift       Steam pack.
  /                 Operate rope in the selected Hold/Toggle mode.
  Right Ctrl        Fire/use selected tool.
  I / J / K / L     Aim up/left/down/right.
  , / .             Previous/next allowed weapon.
  M                 Speak a contextual miner bark.

Detected controller defaults (labels follow Nintendo/Xbox/PlayStation/generic):

  Left stick        Move; vertical input reels an attached rope.
  Right stick       Radial player-relative aim.
  Shown face button Jump.
  Shown face button Steam pack.
  Left shoulder     Operate rope in the selected Hold/Toggle mode.
  Right shoulder/ZR Fire/use selected tool.
  Shown face buttons Previous/next allowed weapon during play.
  Hold ZL + R-stick Zoom out/in with vertical stick input.
  Right stick click Speak a contextual miner bark.
  Start             Pause.

Right-stick deadzone is circular rather than per-axis. Aim stays attached to
the moving miner when the stick centers, uses the selected response curve, and
limits the visible reach to the current tool: 8 cells for close tools, 24 for
guns/thrown weapons, and 48 for the rope. Low aim slowdown is the default; it
slows travel near a visible target but never snaps or changes the shot vector.

Global match keys:

  Escape            Pause.
  R                 Restart the current map and seed.
  F1                Toggle the debug overlay.
  F5                Transactionally reload the DIGS Lua data.
  F11               Toggle fullscreen.


5. USING THE ROPE
-----------------

Aim at stable rock, metal, bedrock, coal, or another valid support and cast the
rope. The swept hook stops at blockers, attaches to the first eligible anchor,
and the segmented cable wraps and unwraps around terrain corners. Keep moving
to swing; P1 W/S or a controller's vertical left-stick input changes rope
length. Hold mode detaches on release; Toggle mode detaches on the next press.
A destroyed anchor breaks the rope, so blasts can cut escape and bot routes.

A hook-head strike against an unshielded enemy sets that miner to exactly one
health, applies bounded knockback, and ends the cast without killing or
severing them. The cable itself never damages. A spawn shield fully blocks the
hook strike, knockback, and attribution and gives distinct shield feedback.

The generated maps include sparse high rail, gantry, and natural-rock anchors
within the 48-cell rope reach without placing solid legs or low ceilings across
the walk lane. You can also use terrain tools to open, close, or reshape
traversal routes.


6. COMBAT, EFFECTS, AND ACCESSIBILITY
-------------------------------------

The eleven-tool Full Works arsenal is:

  1  Pulaski                      6  Giant Fucking Hammer
  2  Popper                       7  Bolt Action
  3  Smoker                       8  Scatterbrain
  4  Hot Rail                     9  Firecracker
  5  Hydroshot                    0  Bore Drill
  -  Mining Rail (hold to charge, release to fire)

The Mining Rail loses energy while piercing soft terrain and aligned miners;
sufficient stone and bedrock stop it. A maximum-charge head or torso hit is
lethal, limbs can sever, and later targets receive less energy. Its charge
tell, recoil, cooldown, beam afterimage, and haptics make the shot readable.

Explosions, impacts, deaths, blood, smoke, embers, fragments, and bot barks
use seeded variants. RETRO, STANDARD, and CARNAGE set explicit bounded pools
of 768, 1536, or 3072 simulated FX voxels. The chosen profile is part of the
match rules and state hash, so compare deterministic results only when the
seed, inputs, rules, scripts, and FX profile all match. Lower profiles trade
particle density for CPU headroom; they are not competitive replay equivalents.

OPTIONS also provides Full/Reduced/Off local-event flashes, Full/Reduced/Off
gore, camera shake, damage numbers, number size/color, Lightfield tier, and
15/30/60/90/120/144/Unlimited presentation caps, Laptop Mode, per-player rope
mode, Dummy Mode, and Off/Low/Normal/Heavy haptics. White explosion flashes and red
kill flashes are limited to events involving a local player. Choose Reduced
or Off if flashes are uncomfortable.

The shared camera follows all local players, adds bounded action/movement
look-ahead, and uses momentum, contextual zoom, and optional shake. Mouse aim
is transformed through the same camera and shake values used to draw the
software cursor.

Projectiles start at a body-relative muzzle. Their launch-envelope clearance
is separate from 45 fixed ticks (0.75 seconds) of owner-only hit and splash
immunity, so firing a weapon cannot instantly kill its miner while terrain and
other miners still resolve normally. The bottom-left match clock and names
above every living miner make the match state visible. Local hits tint the
victim red, mark the impact, pulse the attacker crosshair, and play a hit
confirm. A compact boxed feed records every death; local players receive named
kill/death banners. Kills inside three
seconds produce DOUBLE KILL, TRIPLE KILL, and MULTI KILL; five uninterrupted
kills produce KILLING SPREE.

The audio-device-clock engine generates an eight-note menu motif, a quiet click only when
unmodified mouse-wheel zoom really changes, original hit/kill sounds, and
world-aware wind/water/lava ambience. Authored allophones provide a deep
announcer and high miner voices without samples or external TTS. Curated PG-13
lines and seeded lexical pools react to state, rival names, weapons, materials,
and hazards. Human barks use a three-second cooldown; Dummy Mode removes only
that local-human cooldown. Bots react sparsely under global spacing. OPTIONS
exposes one audio setting only: persistent MASTER VOLUME from 0 through 100
percent.


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

The first two commands should report the same source/catalog hashes and 86
catalog entries. --headless also prints the complete entry list.


8. QUICK AUTOMATED TESTS
------------------------

Run these from the extracted demo directory:

  ./smoke-test.sh
  ./benchmark.sh 240
  ./bin/digs_demo --input-self-test
  ./bin/digs_demo --cap-self-test
  ./bin/digs_demo --audio-cadence-self-test
  ./bin/digs_demo --bark-self-test
  ./bin/digs_demo --haptic-self-test
  ./bin/digs_demo --load-self-test 600
  ./bin/digs_demo --performance-self-test 600
  ./bin/digs_demo --settings-self-test qa/out/digs-settings-test.cfg
  ./bin/digs_demo --camera-self-test
  ./bin/digs_demo --render-miner-icon-xpm qa/out/digs-miner-test.xpm
  ./bin/vox_headless
  ./bin/digs_headless
  ./bin/vox_render_demo qa/out/vox-render-test.ppm

The smoke test is non-windowed and writes qa/out/digs-demo-smoke.ppm. The
input self-test checks radial normalization and weapon-aware aim ranges. The
cap test validates deterministic cap qualification, the cadence test proves
audio sample timing is independent of rendered frames, the bark test checks
all curated slots and unknown-word phonetics, the haptic test checks mixer
levels and isolation without requiring a pad, the performance test enforces
the v0.0.3 four-miner simulation budget only on the named laptop bench, and
the settings test checks schema migration without changing real user settings.
The load test is the portable gate: at 600 ticks it validates exact activity
and state hash c37e44b6 without applying machine timing limits. Run the strict
performance test only on the i7-10750H laptop qualification bench. The camera
self-test checks every zoom and world edge for black exposure, verifies
true close zoom plus full-world player/lava framing and the offscreen lava
horizon, and rechecks the mouse transform. The icon command
re-renders the same miner emitter used in play; compare it with
share/digs/icons/digs-miner.xpm. The benchmark reports Compatibility, Balanced,
and Showcase CPU Lightfield results. Hashes must repeat for the same frozen
build; use this archive's evidence logs rather than old v0.0.1 values.

The package-time versions of these checks, CTest, Cargo tests, script
validation, and raw exit statuses are under evidence/.


9. MANUAL v0.0.3 PLAYTEST
-------------------------

For a shorter first report, use qa/V0.0.3-QUICK-FEEDBACK.txt (or the packaged
QUICK-FEEDBACK.txt). It is a guided 20-minute keyboard/mouse lane with separate
F310 and Switch Pro USB/Bluetooth additions and exact workbook checkpoint IDs.
It finds regressions quickly but does not replace the complete list below.

Please check at least these paths:

  [ ] Keyboard-only P1 can cross each map, jump, steam, rope, aim, and fire.
  [ ] AUTO/KEYBOARD/CONTROLLER modes never mix gameplay sources or crosshairs.
  [ ] Logitech F310 X and D modes both work after centered calibration.
  [ ] Switch Pro labels, controls, and haptics work over USB and Bluetooth.
  [ ] Nintendo/Xbox/PlayStation/generic prompts match the detected controller.
  [ ] One pad defaults to P2 in two-player play; two pads remain isolated.
  [ ] AUTO pad loss falls back safely; locked CONTROLLER loss pauses safely.
  [ ] Circular deadzone, response presets, aim slowdown, and tool reach agree.
  [ ] Rebinding, conflict swaps, and RESTORE DEFAULTS work for the run.
  [ ] Hold and Toggle ropes cast once, sweep blockers, wrap/reel/swing,
      preserve release momentum, and break when their anchor is destroyed.
  [ ] Hook hits set unshielded enemies to 1 HP; shields block every side effect.
  [ ] Mining Rail charge, penetration, anatomy damage, recoil, audio, beam, and
      haptics are readable and deterministic.
  [ ] Steampack lift/glide feels controllable and its lit exhaust stays visual.
  [ ] Five-second spawn protection is visible and attacking cancels it.
  [ ] AUTO and ON FIRE honor every delay and ON FIRE requires a fresh press.
  [ ] Two/three-minute clocks, optional caps, FFA/team winners, and draws work.
  [ ] Human names persist; bot names last one match; all nameplates stay clear.
  [ ] Free-for-All and Miners vs Machines honor friendly-fire selection.
  [ ] Bot roaming/searching/attacking/retreating behavior is observable.
  [ ] Camera frames both humans and remains smooth at every frame cap; maximum
      zoom-out still shows sky and the lava horizon without black exposure.
  [ ] Hardware pointer, sole software reticle, and P1 world aim stay aligned.
  [ ] Coal Ridge, Deepworks, and Furnace Yard have visibly distinct topology.
  [ ] Seeds expose archipelago, continent, and twin-hill macro landforms.
  [ ] Suspended anchors remain reachable, stay above the full jump envelope,
      never become spawn floors, and never wall off the walk lane.
  [ ] Each FX profile changes density without changing terrain or damage.
  [ ] Repeated explosions and deaths visibly vary and collapse supports.
  [ ] Digging/collapse never teleports a living miner across the map.
  [ ] Nails, pellets, melee, blasts, and rail shots match visible anatomy.
  [ ] Gore/debris falls or disperses and never builds one-cell vertical towers.
  [ ] Gore, flashes, camera shake, and damage-number options work.
  [ ] Local hit/kill feedback triggers; unrelated bot fights do not flash.
  [ ] Hit markers, crosshair pulses, killfeed, multikills, and sprees are clear.
  [ ] Audio timing/pitch remain stable at 15 through Unlimited presentation.
  [ ] Motif, contextual speech, ambience, confirms, and master volume work.
  [ ] C, M, and controller R3 barks match their visible speech bubbles.
  [ ] Dummy Mode removes only local bark cooldown; bots remain sparse.
  [ ] Laptop Mode and each qualified cap preserve match results and 60 Hz time.
  [ ] MINER'S INDEX has 87 readable entries, six rows, arrows, and back only.
  [ ] F5 reloads valid Lua data and rejects invalid data without corruption.
  [ ] Rising lava, water cooling, fire, firedamp, liquids, and collapse work.
  [ ] Pause/results/restart/fullscreen/quit paths remain stable.


10. RECORD AND SHARE QA FEEDBACK
--------------------------------

The included 88-checkpoint workbook is:

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

On a hot or battery-powered laptop, cap compiler concurrency while retaining
the complete gate:

  VOX_BUILD_JOBS=1 tools/vox-verify.sh

Create clean Linux tester archives with:

  tools/package-linux-demo.sh

The packager requires Python 3 with openpyxl and verifies that the committed
QA workbook byte-matches a fresh deterministic build from its checkpoint CSV.
On Debian-family systems the package name is python3-openpyxl.

For an explicitly non-release package from a dirty development tree only:

  VOX_PACKAGE_ALLOW_DIRTY=1 \
  VOX_PACKAGE_VERSION=v0.0.3-dev \
  tools/package-linux-demo.sh


12. VERIFY THE DOWNLOAD AND KNOW THE LIMITS
--------------------------------------------

From the parent dist directory:

  sha256sum -c SHA256SUMS

Both archives must report OK. The binary bundle contains SBOM.spdx.json, raw
package-time evidence, GPL notices, the Lua catalog, and launch/test scripts.
The matching Corresponding Source archive is distributed beside it.

v0.0.3 is still the Linux CPU-software vertical slice. Windows, macOS,
historical operating systems, a GPU renderer, network multiplayer, a complete
rigid-body solver, device-specific remapping wizards for every unknown pad, and
final competitive bot quality remain planned/evidence-gated work. Record only
what you observe and do not infer GPU acceleration from the laptop containing
a GTX 1660 Ti.
