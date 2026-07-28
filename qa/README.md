<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# VOX + DIGS testing cockpit

`VOX_QA_FEEDBACK.xlsx` is the portable feedback workbook for the `v0.0.3`
demo. It can be filled in with Excel, LibreOffice, OnlyOffice, or another
application that preserves `.xlsx` files. The repository builds it
deterministically from `VOX_QA_CHECKPOINTS.csv` with
`tools/build-qa-workbook.py`.

The workbook has three sheets:

- **Checkpoints** contains the repeatable acceptance pass. Fill the yellow
  cells for every test you run. `Not Run` is an honest result and is better
  than an assumed pass.
- **Issues** is one row per distinct defect. Reference the checkpoint ID when
  possible and keep one observable problem in each row.
- **Environment** identifies the build and test bench. Use a public tester
  alias and general hardware labels; never enter passwords, access tokens,
  email addresses, user names, host names, network addresses, hardware serial
  numbers, or private file paths.

For a fast candidate check, follow `V0.0.3-QUICK-FEEDBACK.txt`. Its base
keyboard/mouse lane takes about 20 minutes and references the exact workbook
checkpoint IDs to fill. Controller USB/Bluetooth passes are separate add-ons;
the quick lane is defect discovery and does not replace the full 88-checkpoint
release pass or the 15-minute performance soak.

## Run the cockpit

Install [xleak](https://github.com/bgreenwell/xleak) and make sure `xleak` is
available on `PATH`. From a source checkout, run:

```sh
tools/vox-test-cockpit.sh --binary ./build-demo/digs_demo \
  qa/VOX_QA_FEEDBACK.xlsx
```

From the extracted Linux tester bundle, the short form uses the same workbook
and the packaged binary automatically:

```sh
./qa-cockpit.sh
```

Multiple testers' workbooks can be combined in one packet:

```sh
tools/vox-test-cockpit.sh tester-a.xlsx tester-b.xlsx
```

The script exports all three known sheets through xleak, combines like sheets,
records a privacy-conscious system profile, and optionally runs the demo's
non-windowed smoke, CPU renderer benchmark, input, cap, audio-cadence, bark,
haptic mixer, portable 600-tick deterministic load, settings, camera, and adjacent
headless checks. It writes a Markdown report,
raw CSV exports, copied workbooks, logs, and a compressed evidence packet under
`qa/out/`. Review the report and every attachment before sharing the packet.

The script does not upload anything. Attach the reviewed `.tar.gz` packet to a
GitHub issue or release-feedback thread. For a single problem, also copy its
Issues row into the repository's **Demo feedback** issue form so maintainers can
triage it directly.

## Controller and input acceptance

Record the exact controller name, SDL version, connection mode, selected P1/P2
input modes, sensitivity, deadzone, and aim-slow setting. For the Logitech F310,
run the pass once with its rear switch in `X` mode and once in `D` mode; unplug
the pad before changing the switch. The pinned mapping database should cover
known SDL controller GUIDs, while an unmapped D-mode device exercises DIGS's raw
joystick fallback.

Test `AUTO`, `KEYBOARD`, and `CONTROLLER` separately. AUTO must switch only
after deliberate activity, an idle or noisy stick must not steal ownership, and
changing source must not carry a held fire, jump, or rope edge into the next
simulation tick. Locked modes must ignore the other gameplay source. Menus stay
available from either source so a player cannot lock themselves out.

For aim, check a centered stick, slow circles just beyond the deadzone, full
diagonals, movement while the stick is centered, and transitions across an
enemy. Expect a player-relative radial reticle with no jump at the deadzone,
exactly one crosshair per local player, tool-aware reach, and gentle slowdown
without snapping or changing the fired direction. Use `CALIBRATE PADS` with the
sticks untouched, then repeat these checks at Small, Normal, Large, and Auto
deadzone settings.

With two local players, test no pads, one pad, and two pads. One pad defaults to
P2 while P1 remains keyboard/mouse; two pads are exclusive to their claimed
slots. In a locked controller mode, unplugging the pad must pause safely. In
AUTO, it must fall back without generating an action. Reconnect and confirm the
same slot is usable again.

Record Nintendo, Xbox, PlayStation, or generic as the observed prompt family,
not as an inference from button position. A Switch Pro USB result does not
cover Bluetooth. At each transport, capture Off/Low/Normal/Heavy haptic level,
whether SDL/driver rumble is available, and whether nearby/distant events reach
the correct local controller. Driver-unavailable vibration is `Blocked`, not a
pass. The Environment sheet also carries rope mode, Laptop/Dummy settings,
cap-qualification result, audio cadence diagnostics, seed, and state/frame
hashes so otherwise-similar runs remain distinguishable. Record the automated
haptic mixer values separately from physical-pad vibration. The default
`--load-self-test 600` evidence records canonical slots, activity, awake cells,
and hash without a wall-clock gate. Only on the named i7-10750H laptop bench,
set `VOX_NAMED_BENCH_QUALIFY=1` when running the cockpit to additionally record
the strict average, p95, and maximum qualification. A reduced 180-tick
diagnostic cannot pass either 600-tick checkpoint.

## Result and severity rules

- **Pass**: observed behavior matches the checkpoint on the recorded build.
- **Fail**: the checkpoint was executed and produced a reproducible mismatch.
- **Blocked**: a dependency or environment condition prevented the test.
- **Skip**: the tester intentionally excluded the checkpoint and explained why.
- **Not Run**: no result was observed.

Severity uses the same labels as the GitHub demo-feedback form and describes
player and test impact, not how difficult a fix appears:

- **BLOCKER**: cannot install, launch, start a match, or complete the required
  acceptance path; unrecoverable crash or data loss.
- **HIGH**: a core mechanic is broken or frequently crashes, with no practical
  workaround.
- **MEDIUM**: behavior is wrong but the demo remains testable with a workaround.
- **LOW**: presentation or localized behavior issue with no material block.
- **NOTE**: observation or successful checkpoint with no defect severity.

## A useful issue report

Include the exact build ID or binary SHA-256, checkpoint ID, platform, minimal
reproduction steps, expected behavior, actual behavior, and the smallest useful
evidence file. Record seeds and debug ticks for simulation defects. Do not
bundle unrelated problems into one row.

Before sharing evidence:

1. Open the generated Markdown report and all logs.
2. Inspect screenshots and videos for notifications or personal content.
3. Remove credentials, private paths, host names, network identifiers, and
   serial numbers.
4. Confirm the Environment sheet's evidence-sharing consent field.

## Rebuild and verify the workbook

The generator requires Python 3 and `openpyxl`:

```sh
python3 tools/build-qa-workbook.py
python3 tools/build-qa-workbook.py --check
```

`--check` creates a fresh workbook in a temporary directory and byte-compares
it with the committed artifact. A mismatch means the `.xlsx` is stale or was
edited directly; preserve tester-filled copies elsewhere and regenerate the
repository template from the CSV.
