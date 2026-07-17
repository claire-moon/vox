DIGS v0.0.1 - CG QUICK-START AND TEST GUIDE
===========================================

This is the Linux x86-64 CPU-rendered demo build. The game does not use the
GTX 1660 Ti yet; the GPU path is planned for a later VOX version.


1. RUN THE GAME NOW
-------------------

Open a terminal and run:

  cd /home/cg/jebby/paragould/casebox_local/ppd/.vox-demo-work/dist/vox-digs-v0.0.1-linux-x86_64
  ./run-digs.sh

If Linux reports "Permission denied", run this once and try again:

  chmod +x run-digs.sh smoke-test.sh benchmark.sh qa-cockpit.sh

If SDL2 is missing on Linux Mint, Ubuntu, or Debian:

  sudo apt install libsdl2-2.0-0


2. START A MATCH
----------------

At the title screen:

  START MATCH       Configure a normal deathmatch.
  FOUNDRY LAB       Open the systemic-material sandbox.
  OPTIONS           Change frame cap, lighting, debug, and fullscreen.
  QA FEEDBACK       Show the testing-cockpit reminder.
  QUIT              Exit the demo.

Match setup lets you choose:

  - zero through three bots;
  - Coal Ridge, Deepworks, or Furnace Yard;
  - a visible procedural map seed; and
  - Miner Kit, Powder Keg, or the complete Full Works arsenal.


3. CONTROLS
-----------

Menus:

  Arrow keys        Move or change a value.
  Enter             Select.
  Escape            Go back.
  R on seed row     Generate another seed.

Match:

  A / D             Move left or right.
  W / Up / Space    Jump.
  Shift             Use the rechargeable steam jet.
  Mouse             Aim.
  Left mouse        Fire or use the selected tool.
  Mouse wheel       Zoom the player-locked camera from 1x through 4x.
  Shift+wheel       Cycle weapons allowed by the match arsenal.
  1 through 0       Select one of the ten weapons.
  Escape            Pause.
  R                 Restart the current map and seed.
  F1                Toggle the debug overlay.
  F11               Toggle fullscreen.

Weapons:

  1  Pick                         6  Sledge
  2  Blast Charge                 7  Nail Gun
  3  Smoke Pot                    8  Boiler Shotgun
  4  Cinder Flask                 9  Concussion Grenade
  5  Pressure Hose                0  Nail Bomb


4. QUICK AUTOMATED TESTS
------------------------

Run these from the extracted demo directory.

Deterministic smoke test:

  ./smoke-test.sh

A good result exits successfully and prints these values:

  state hash: d49e20fd
  frame hash: 4ed9909b

CPU Lightfield benchmark:

  ./benchmark.sh 240

This must report Compatibility, Balanced, and Showcase results. FPS will vary
with CPU load, temperature, power mode, and build environment. The three frame
hashes should remain stable:

  Compatibility: e5d806dd
  Balanced:      3104be39
  Showcase:      03e642c4

Minimal engine and match proofs:

  ./bin/vox_headless
  ./bin/digs_headless

Software-rendered image proof:

  ./bin/vox_render_demo qa/out/vox-render-test.ppm


5. MANUAL PLAYTEST CHECKLIST
----------------------------

Please check at least these items:

  [ ] The title, setup, options, pause, results, and QA screens are readable.
  [ ] Bot counts 0, 1, 2, and 3 can be selected.
  [ ] All three maps generate and the same seed regenerates the same terrain.
  [ ] Movement, jumping, aiming, and the steam jet respond correctly.
  [ ] All ten weapon keys work.
  [ ] Blasts have varied fracture edges instead of identical round holes.
  [ ] Unsupported structural terrain collapses under gravity.
  [ ] Explosions throw mixed terrain, smoke, ember, and weapon debris.
  [ ] Camera stays centered on P1; wheel zoom and aim remain aligned.
  [ ] Sand falls; smoke and firedamp rise; water cools lava into stone.
  [ ] Bots move, attack, take damage, die, score, and respawn.
  [ ] Rising lava enters the world and damages a miner.
  [ ] Frame caps 30, 60, 90, 120, 144, and Unlimited are selectable.
  [ ] Compatibility, Balanced, and Showcase lighting all render correctly.
  [ ] F1 debug, F11 fullscreen, pause, restart, and quit work.
  [ ] Audio works, or the demo continues safely in silent mode.


6. RECORD FEEDBACK
------------------

The included workbook is:

  qa/VOX_QA_FEEDBACK.xlsx

Open it with LibreOffice, Excel, OnlyOffice, or another XLSX editor. Fill in:

  Checkpoints   One result for each test you performed.
  Issues        One row for each distinct problem.
  Environment   The exact build and general test-bench information.

Use PASS, FAIL, BLOCKED, SKIP, or NOT RUN honestly. For defects, use severity
BLOCKER, HIGH, MEDIUM, LOW, or NOTE.

If xleak is installed, you can inspect the workbook in the terminal:

  xleak qa/VOX_QA_FEEDBACK.xlsx -i

Create a complete local evidence packet with:

  ./qa-cockpit.sh

The report and tar.gz packet will appear under:

  qa/out/

Nothing is uploaded automatically. Review the report, logs, workbook, and any
screenshots for private information before sharing them.

Submit reviewed feedback here:

  https://github.com/claire-moon/vox/issues/new?template=demo-feedback.yml


7. VERIFY THE DOWNLOAD
----------------------

From the parent dist directory:

  sha256sum -c SHA256SUMS

Both archive lines must report OK. The binary bundle contains SBOM.spdx.json,
raw package-time test evidence, GPL notices, and launch scripts. A matching
Corresponding Source archive is distributed beside it.


8. TEST FROM THE SOURCE CHECKOUT
--------------------------------

From the VOX repository root:

  tools/vox-verify.sh

That builds the strict C89/C++98 core, Rust boundary, SDL2 demo, and optional
NASM parity path; runs all eight native tests and Rust tests; and executes the
headless and graphical smoke scenarios.

To create a new clean Linux tester bundle:

  tools/package-linux-demo.sh


9. CURRENT v0.0.1 LIMITS
------------------------

This release demonstrates the Linux CPU software path. Windows, macOS,
historical operating systems, a GPU renderer, network multiplayer, full rigid
body/Havok-scale physics, and production multithreaded scaling remain planned
work. Record only behavior you actually observe; do not infer GPU acceleration
from the laptop containing a GTX 1660 Ti.
