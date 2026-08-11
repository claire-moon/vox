<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# CONCERNS

What worries me about this codebase, written for whoever picks it up next.

This is deliberately not a task list — `docs/ROADMAP_V0.0.5.md` is that, and
`docs/releasing/V0.0.4_RELEASE_CHECKLIST.md` is what still needs verifying.
This file is the other thing: the traps, the things proven less thoroughly than
they look, and the decisions most likely to be wrong.

Everything here was verified against the source at the time of writing. If you
are reading this much later, check before acting — a concern that has been
fixed is worse than useless, because it sends you looking for a bug that is not
there.

---

## 1. The headline feature is unverified by any human

**This is the largest gap and it is not a technical one.**

v0.0.4 is named "THE BOTS REMEMBER". `digs_memory_across_sessions` proves the
mechanism: memory crosses a real process boundary and changes the simulation.
That is *all* it proves. Whether a miner reads as somebody holding a grudge —
which is the actual feature — has never been observed by a person.

VOX-QA-089 through 093 (memory across sessions) and 096 through 100
(conversation pacing) are release-blocking and unrecorded. So is VOX-QA-084,
the fifteen-minute soak.

The risk is specific: **a green test suite makes this feel finished when the
part that matters is untested.** The tester form says so in as many words. Do
not let an automated pass substitute for the human lane, and do not let the
next release bury the question under new features.

## 2. There is no relationship UI, on purpose

No nameplate tint, no standings screen, no addressing markers. This was a
deliberate decision, recorded in RFC 0003, and it raises the bar on everything
else: with nothing on screen announcing how a miner feels, the writing and the
pacing carry the entire relationship.

**This is the decision most likely to need reversing.** If relationships read
as invisible in play, the cheapest remedy is the nameplate tint that was
declined — reconsider that before building anything more elaborate. Concern 1
is what answers this question, which is another reason not to skip it.

## 3. Determinism has a blind spot that gates do not cover

A wall clock reached the canonical hash once in this release and **no gate
caught it**. `vox_digs_memory_hash` folded `launch_counter` and
`elapsed_coarse`, both filled from `time(0)` by the port, into a digest that
`vox_digs_hash` consumes. Two matches one launch apart had identical world
hashes, scores and deaths, and different canonical hashes from tick zero.

The reason no gate saw it is structural and still true: **every gate
initialises from a canonical snapshot in which those counters are zero. Only
the shipping demo carries a clock in.** Anything that enters the sim from the
port, and only from the port, is in this blind spot.

The rule worth keeping: *a wall-clock value may be read by the port and written
to the chronicle, but must never reach a digest the canonical hash consumes.*
`test_the_clock_stays_out_of_the_hash` fails if those two fields return, but it
does not generalise to a third field somebody adds later.

Related: memory *does* legitimately affect the hash — it is an opening
condition like a seed. "Memory never touches the hash" is false and believing
it will lead you to fold the wrong thing in. `--load-self-test` is stable
across histories because it deliberately does not read the chronicle, which is
a narrower claim.

## 4. Traps that bite silently

Each of these has already caused a defect, and each fails without an obvious
symptom.

| Trap | What happens |
|---|---|
| **`vox_digs_match_init` has no `memset`** and must not gain one. Every new field needs explicit initialisation. | An uninitialised field holds caller memory and the match hash varies by whatever was on the stack. `test_init_leaves_nothing_uninitialised` covers it. |
| **The settings accepted-version list must name every readable schema.** | Dropping the outgoing number silently discards the settings of everybody upgrading. This already happened to schema 4 when 5 arrived. The comment at the site says so. |
| **Line ids pack `set * DIGS_LINE_SET_STRIDE + index` into a `vox_u16`.** | A stride of 256 overflowed past set 255 and aliased set 282 onto set 26 — wrong lines, no crash. `digs_lines_stride_is_sound` guards it. |
| **`vox_ui_text_wrap` returns lines, not pixels.** | Already misused once as a pixel height. Layout drifts instead of erroring. |
| **Urgency is derived from valence.** | A stimulus worth zero rolls at roughly 0.4‰ and is effectively never spoken unless bumped explicitly. A new stimulus can look implemented and never fire. |
| **Digging costs hazard damage.** | Tuned twice already. Changing dig rates changes how often bots kill themselves. |

## 5. Tests and tools that look like they work

Three of my own measurement instruments produced confident wrong answers during
this release. That is a higher rate than the bugs they were looking for, and it
is the concern I would most want passed on.

- An interrupt test counted *any* next bot line as a "reply", conflating
  replies with unrelated event barks, and produced numbers that could not be
  reproduced.
- An init audit grepped one function for field names that a helper sets, and
  produced a false positive. I nearly committed a redundant "fix" for a bug
  that did not exist.
- A defect-reintroduction check silently did nothing because `str.replace`
  does not error when the target is absent — so the test "passed" against an
  unmodified build.

**The discipline that caught all three: make the test fail on purpose before
trusting it to pass.** For a test, reintroduce the defect. For a script that
edits files, assert the edit landed. For a gate, inject the mistake it claims
to catch. `tools/vox-session-evidence.sh` and `tools/vox-version-check.sh` were
both validated this way, and both had real holes that only appeared when
attacked.

Two specific traps in the tooling:

- **`./tools/package-linux-demo.sh` fails *late* and looks like success.** It
  prints a healthy size report and then exits 1 several steps later. Reading
  the tail of the log is not enough; check the exit status.
- **`if ! cmd; then status=$?` captures the exit of the negation**, which is
  always 0. This made `./dev.sh check` print "All green" over a failed
  benchmark for an unknown period. Capture directly.

## 6. The version gate is narrower than it looks

`tools/vox-version-check.sh` covers **product-identity surfaces by path** — the
build, packagers, the text they ship, CI artefact names, ports, and the
documents telling a player what to download. Prose about project history is out
of scope on purpose, and so are planning documents and RFCs.

Known limits, none of them accidental but all of them worth knowing:

- **Untracked files are not scanned.** It reads `git ls-files`, so a new
  document is invisible until staged.
- **Exemptions are matched on content.** Two lines with identical text in
  different sections cannot be told apart. An exemption is deliberately
  required to contain the version it excuses, which limits the damage.
- **A whole-file exemption disables checking for that file entirely.**
  `ROADMAP.txt` has one because it is a version-by-version document.

The gate fails on exemptions that stop matching anything, so the list cannot
quietly rot. Two of its own rules were over-broad when written and hid real
mistakes until I injected them deliberately.

## 7. v0.0.3 is public and must stay untouched

A standing constraint from the original handoff. Its release assets must not be
moved, overwritten, or rebuilt.

`.github/workflows/publish-release.yml` enforces this structurally rather than
by convention: the tag must equal `v` + the `VERSION` file at that commit, and
commits older than the VERSION file are refused outright — so it cannot run
against v0.0.3 even if that tag is pushed again. A release already carrying
assets is never modified by a tag push; replacing them needs a manual dispatch
and a typed confirmation.

**Do not reuse `.github/workflows/publish-legacy-win32-addendum.yml`.** It
augments an existing release in place, which is precisely the operation the
guard exists to keep away from a tag push.
`tools/package-win32-legacy.sh` still carries an assertion for a file v0.0.4
deleted; it is left alone deliberately because it builds from the v0.0.3 tag's
source.

## 8. What v0.0.5 asks for argues with what v0.0.4 promised

Three collisions, stated so they are decided rather than discovered:

- **The 1.44 MB ceiling.** 960 KB of headroom against fluid simulation,
  ragdolls, skeletal animation, a raymarched sky, a 256-colour palette and
  voxel weapon models. Budget per feature and measure as you go. If it comes to
  a choice, somebody has to say which features *are* the release.
- **Ragdolls remain research-scale under these constraints.** The in-tree
  bounded angular rigid bodies and joints satisfy the v0.0.5 direction, but
  they are not a general convex solver and still need post-change performance
  and human-acceptance evidence. Do not replace them with particle chains: the
  locked v0.0.5 contract explicitly rejects that scope reduction.
- **A fourth bot is a format change, not a constant.** Five slots means ten
  pairs, not six. It breaks chronicle format 1 and every existing player's
  history. Worth doing in a release that already breaks the format, not on its
  own.

## 9. Smaller things worth knowing

- **The dropship launch has a spatial ordering invariant.** A rider stages on
  top of the hull, but FIRE must eject that body below the hull before the
  next collision query. Giving the deck position a downward velocity instead
  turns the ship into an invisible first-tick splatter. The core and SDL host
  regressions deliberately step once after FIRE; preserve that test when
  changing the launch deck, hull size, or collision rule.
- **The dropship has an explicit lifecycle boundary.** `vox_digs_match_init`
  starts with the virtual hull departed. Only `vox_digs_dropship_begin` at
  tick zero activates and stages it. Restoring LAUNCH as the default makes
  ground-based tools and tests acquire an invisible moving collision surface;
  forgetting the begin call in a real host means there must be no ship at all.
- **The Options memory reset contradicts RFC 0003's own design intent.** The
  RFC says the history should be unrepeatable; a reset makes it a save file.
  It exists at the lead's explicit request, behind a two-press confirm, and is
  recorded as a deliberate exception in both the RFC and the checklist. Do not
  "fix" it back.
- **The chronicle is personal data.** It records what the player and the miners
  said to each other. QA packets and bug reports should treat it as they would
  any other personal file.
- **Optimisation invariance is load-bearing for the size diet.** The `-Os` and
  `--gc-sections` work is only safe because hashes are identical across `-O0`,
  `-O1`, `-O2` and `-Os`. `tools/vox-optimisation-invariance.sh` proves it each
  run rather than assuming it.
- **The portable-core CI matrix is now the whole ISO C portability claim.**
  Through v0.0.3 a Rust boundary and a C++98 translation unit corroborated it.
  Neither exists. A red lane there matters more than it used to.
- **The Win32 legacy host cannot be built on this machine** (no i686 MinGW).
  `digs_legacy_headless_scenario` passes and `vox_sdl_ui.c` compiles under the
  legacy `-m32 -march=i386` profile, but the PE32 binary itself is unproven for
  this release.
- **Charge weapons fire on release, not on press.** Holding FIRE charges
  forever and never shoots. This cost four debugging cycles when bots would not
  dig.
- **`vox_digs_personality_get` returns null for anyone who is not a bot.** The
  player's miner has a middling trait row precisely because speech pacing needs
  a temperament for every speaker. Three segfaults came from assuming otherwise.

---

## How to use this file

When you fix something here, delete the entry — a stale concern costs more than
no concern. When you find a new trap that fails silently, add one. The test is
whether somebody arriving cold would waste a day without it.
