DIGS v0.0.4 package-time evidence
========================================

Each named command has an unmodified stdout stream, unmodified stderr stream,
and numeric exit status in this directory. The generated PPM images are the
actual smoke outputs checked by the packager. The headless streams prove that
the shipped simulation reproduces its canonical hashes under the same runtime
included in the bundle. The QA-workbook-current stream proves that the shipped
template byte-matches a fresh deterministic build from its checkpoint CSV.
The haptic stream proves deterministic mixer ordering/isolation without
claiming a physical motor. The load stream is the complete 600-tick, four-slot
portable regression and validates canonical activity plus state hash without a
wall-clock assertion. A named-bench performance stream is present only when the
packager was explicitly run with VOX_NAMED_BENCH_QUALIFY=1 on the i7-10750H
laptop; that stream records average, p95, maximum, event counts, awake cells,
and state hash against the RFC timing limits. A shorter diagnostic is not
release evidence.

The chronicle stream proves the save layer writes and reads back what it was
given and refuses a file that fails its checksum; it does not prove recovery
from a genuinely corrupted file on disk, which is a manual checkpoint. The
menu stream proves that no screen draws content outside its own frame; it says
nothing about whether those screens are legible, which is judged by eye.

Neither the chronicle nor the memory it carries enters the canonical hash.
The load stream prints the same state hash on a machine with a long history as
on a fresh one, which is the determinism boundary this release rests on.

Archive ordering, ownership, and timestamps are normalized with
SOURCE_DATE_EPOCH. The source archive is reproducible from the same tree.
The evidence-bearing binary archive is intentionally not promised to be
bit-for-bit reproducible: genuine ctest and benchmark output contains
real execution durations. Removing or rewriting those values would make the
logs cease to be raw evidence. The shipped game binaries and every archive are
covered by SHA256SUMS so a particular release artifact can be verified.

QUICK-FEEDBACK.txt is the packaged human-in-the-loop guide. Its results live in
the tester's workbook and cockpit packet, not in these package-time logs. The
automated streams here do not prove a visible desktop, audible device output,
controller mapping, Bluetooth transport, vibration, or player-perceived feel.
