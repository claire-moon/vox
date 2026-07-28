DIGS v0.0.3 package-time evidence
========================================

Each named command has an unmodified stdout stream, unmodified stderr stream,
and numeric exit status in this directory. The generated PPM images are the
actual smoke outputs checked by the packager. Lua validate, hash, and headless
streams prove that the shipped catalog was accepted by the same script runtime
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

Archive ordering, ownership, and timestamps are normalized with
SOURCE_DATE_EPOCH. The source archive is reproducible from the same tree.
The evidence-bearing binary archive is intentionally not promised to be
bit-for-bit reproducible: genuine ctest, Cargo, and benchmark output contains
real execution durations. Removing or rewriting those values would make the
logs cease to be raw evidence. The shipped game binaries and every archive are
covered by SHA256SUMS so a particular release artifact can be verified.

QUICK-FEEDBACK.txt is the packaged human-in-the-loop guide. Its results live in
the tester's workbook and cockpit packet, not in these package-time logs. The
automated streams here do not prove a visible desktop, audible device output,
controller mapping, Bluetooth transport, vibration, or player-perceived feel.
