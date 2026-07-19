<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Software Lightfield renderer

The software renderer is VOX's portable presentation oracle. It projects the
ten-layer mini-voxel slab into a caller-owned RGB24 framebuffer without an
OS, window, GPU, allocator, floating-point calculation, or external asset.

For each `x/y` world location, one ascending depth scan caches the frontmost
non-air material while collecting emission from every layer. It initializes a
`512 x 320` RGB lightfield with vertical ambient skylight, injects colored
emission from lava and sufficiently hot material, and propagates the maximum
attenuated neighbor light. Air transmits farther than occupied cells. The final
pass reuses the cached surface material instead of rescanning depth, then
samples palette color and the lightfield while scaling to the target.

This is a bounded diffuse Lightfield approximation designed for cheap CPUs,
not physically based global illumination. There are no rays, visibility cones,
normals, BRDFs, temporal history, denoising, or floating-point HDR buffers.
Its value is systemic coupling: the same simulated hot/lava voxels that affect
materials also emit visible colored light, at predictable fixed cost.

## Quality tiers

| API value | Passes | Contract |
|---|---:|---|
| `VOX_GI_COMPATIBILITY` | 1 | Minimum propagation work |
| `VOX_GI_BALANCED` | 3 | Default demo presentation |
| `VOX_GI_SHOWCASE` | 5 | Wider propagation where CPU time permits |

Quality is a presentation setting and is excluded from authoritative state.
All tiers share surface visibility, emission injection, integer arithmetic,
and RGB output rules. The headless renderer test checks repeatable frame hashes
and verifies that the tiers produce distinct results.

The SDL2 host constructs a render-only snapshot of the canonical terrain and
voxelizes living miners, active projectiles, and transient effects into it.
That lets every visible world element participate in the Lightfield while the
canonical material array and match hash remain untouched. Custom menu/HUD text
is composited afterward as a separate RGB layer.

The renderer owns two fixed world-sized light fields plus solid and surface
caches, so this v0.0.2 scalar implementation is allocation-free but not
reentrant. A host must serialize calls. A future worker implementation may use
caller-owned scratch arenas and chunk-dirty recomputation after proving
byte-stable results against this path.

## Host and future backends

SDL2 receives the finished `320 x 200` RGB24 frame, uploads it to a streaming
texture, and scales/letterboxes it. SDL2 is not a GPU Lightfield backend and
the presence of a graphics card does not accelerate this algorithm.

`digs_demo --benchmark` times the scalar renderer by tier. Results describe the
recorded CPU, compiler, build, and scene only. The initial GTX 1660 Ti laptop
cannot provide GPU evidence in the current verification environment because no
working NVIDIA driver/device node is available there.

Future OpenGL, Direct3D, Metal, GPU-compute, SIMD, NASM, and historical
framebuffer adapters must consume a renderer-neutral snapshot and preserve the
authoritative material visibility rules. A visually enhanced backend may not
silently become a simulation dependency; the scalar software path remains the
fallback and verification oracle.
