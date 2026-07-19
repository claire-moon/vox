DIGS v0.0.2 package-time evidence
========================================

Each named command has an unmodified stdout stream, unmodified stderr stream,
and numeric exit status in this directory. The generated PPM images are the
actual smoke outputs checked by the packager. Lua validate, hash, and headless
streams prove that the shipped catalog was accepted by the same script runtime
included in the bundle.

Archive ordering, ownership, and timestamps are normalized with
SOURCE_DATE_EPOCH. The source archive is reproducible from the same tree.
The evidence-bearing binary archive is intentionally not promised to be
bit-for-bit reproducible: genuine ctest, Cargo, and benchmark output contains
real execution durations. Removing or rewriting those values would make the
logs cease to be raw evidence. The shipped game binaries and every archive are
covered by SHA256SUMS so a particular release artifact can be verified.
