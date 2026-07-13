<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Chunked world contract

The v0.0.1 dense development profile uses a bounded `256 x 160 x 10`
mini-voxel slab: 409,600 cells, exactly ten times the original 40,960-cell
profile. Cells are stored in stable `z/y/x` order, while a parallel `16 x 10`
grid of `16 x 16 x 10` chunks supplies scheduling and renderer-upload metadata.

Each chunk owns occupied and awake counts. `VOX_CHUNK_ACTIVE` is set exactly
when its awake count is nonzero. A set, clear, phase change, or move marks a
chunk dirty and increments its revision; cross-boundary movement marks both
the source and destination chunks. `vox_world_clear_dirty` clears only the
dirty bit, never a revision or simulation state.

The canonical world hash combines the simulation tick, global counts, and an
incrementally maintained cell signature for each chunk. It includes active
scheduler metadata and excludes dirty/revision metadata, so presentation
uploads cannot alter deterministic replays. Kernel tests recompute the chunk
counts and signatures from every cell after material, motion, and sleep
scenarios.

This is not a promise that the fixed array is the final shipping-world size.
It establishes the C89 contract used by later cold-chunk compression,
streaming, worker-local proposals, GPU uploads, and smaller compatibility
profiles.

The dense development-world structure is roughly 4.7 MiB on the current ABI. Tools
and tests therefore keep it in static storage; a constrained compatibility
host must use a smaller profile or caller-owned arena rather than relying on
a retro default stack.
