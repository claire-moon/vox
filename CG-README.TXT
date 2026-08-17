DIGS v0.0.5 - TESTER AND SUPPORT GUIDE
=======================================

This guide travels with release-shaped DIGS packages. A package is public only
when it is attached to the matching tag on the project's GitHub Releases page
and its SHA-256 matches the published checksum. A package whose name ends in
`-ci` is review evidence, not a public release or an upgrade recommendation.

DIGS is a deterministic voxel-destruction deathmatch using the VOX simulation
engine. Its simulation is CPU-authoritative and fixed at 60 Hz; SDL2 provides
desktop presentation, input, and optional audio.


1. START FROM THE EXTRACTED PACKAGE
-----------------------------------

Linux x86-64:

  ./run-digs.sh

Keep `bin/`, `bin/share/`, `share/`, and `libexec/` together. Linux uses the
SDL2 runtime supplied by its distribution. If the launcher reports that SDL2
is missing, install the runtime it names and run the launcher again.

Windows x86-64:

  Double-click run-digs.bat

Keep `bin/` and `share/` in the extracted ZIP folder. The Windows package is
self-contained for SDL2; do not replace its executable with a build-tree copy.

The game may run silently if an audio device is unavailable. That is a valid
fallback only when gameplay and the self-tests remain healthy.


2. TITLE SCREEN AND MATCH FLOW
------------------------------

The title menu contains:

  BEGIN       Configure bots, map, arsenal, and custom rules.
  INBOX       Read messages the miners left between sessions.
  LOG         Read the persistent conversation log.
  PRACTICE    Start a no-bot material and weapon sandbox.
  CONTROLS    Rebind keyboard and controller actions.
  OPTIONS     Set presentation, input, haptics, audio, and memory reset.
  QUIT        Leave the game.

The fixed miner identities are RIVET, CINDER, and FLAMEY. Their remembered
relationships, inbox messages, and conversation log survive between sessions;
the options reset is deliberately a two-press destructive action. Record the
state before using it during QA.

BEGIN stages a live match on the dropship. FIRE launches it. PRACTICE uses the
same authoritative materials and tools but has no bots.


3. DEFAULT CONTROLS
-------------------

All controls are rebindable; the in-game CONTROLS page wins if it differs from
this quick reference.

  Action                 Player 1             Player 2        Controller
  Move left/right        A / D                Left / Right    Stick or D-pad
  Jump                   Space                Up              A
  Steam                  Left Shift           Right Shift     X
  Steam Hook             Middle mouse         /               LB
  Fire / use             E or left mouse      Right Ctrl      RB
  Previous / next tool   Z / X                , / .           Y / B
  Bark                   C                    M               Right-stick click

The Steam Hook is always-toggle: press to cast or attach, press again to
retarget, and jump to cancel. It can attach to valid terrain and fixtures.
Mouse wheel changes the player-locked camera zoom. F1 toggles the debug
overlay; F11 toggles fullscreen.


4. PACKAGE SELF-TESTS
---------------------

Run tests from an extracted Linux package, retaining complete output and exit
status. Windows testers can run the same `digs_demo.exe` switches from `bin/`.

  ./smoke-test.sh
  ./benchmark.sh 240
  ./bin/digs_demo --input-self-test
  ./bin/digs_demo --blood-baseline-self-test
  ./bin/digs_demo --load-self-test 600
  ./bin/digs_demo --chronicle-self-test
  ./bin/digs_demo --menu-self-test
  ./bin/vox_headless
  ./bin/digs_headless

The exact candidate's evidence directory is the authority for counters,
hashes, payload size, and timings. Do not compare a current package against
numbers quoted in an older guide or a different CI artifact. A self-test proves
only its named automated boundary; it does not prove visible display, audio,
controller, or player-perceived behavior.


5. HUMAN ACCEPTANCE
-------------------

Start with `QUICK-FEEDBACK.txt`, then complete the workbook and the release
checklist. The quick run is defect discovery only; it cannot close the full
release gates.

Pay special attention to:

  - remembered miner attitudes, inbox state, log pacing, and the memory reset;
  - water, lava, blood, anatomy, debris, and terrain-collapse behavior;
  - always-toggle hook, dash, headshot, replay, and dropship flow;
  - keyboard/controller ownership, display, audio, caps, Laptop Mode, and
    clean shutdown; and
  - the full fifteen-minute four-slot destruction soak on the named bench.

The carried memory/conversation checkpoints and the soak must be observed on
the exact v0.0.5 candidate even if an earlier build passed related automation.
Record `Pass`, `Fail`, `Blocked`, `Skip`, or `Not Run` honestly; a missing
device or platform is not a pass.


6. EVIDENCE, PRIVACY, AND LIMITS
--------------------------------

`qa/README.md` explains the workbook and cockpit. Package evidence is written
under `qa/out/` and the bundle's `evidence/` directory. Review it before
sharing. Do not upload credentials, private paths, email addresses, hardware
serial numbers, or unrelated desktop content.

This package makes no unmeasured claim about Android, historical Windows, GPU
acceleration, physical controller transport, or a machine not listed in its
evidence. See `docs/compatibility/MATRIX.md` and
`docs/releasing/V0.0.5_RELEASE_CHECKLIST.md` for the exact acceptance boundary.
