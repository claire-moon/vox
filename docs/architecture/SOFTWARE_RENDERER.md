<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Software renderer foundation

The software renderer is VOX's portability and visual-determinism oracle. It
projects the shallow four-layer voxel slab into a caller-owned RGB framebuffer
without an OS, window, GPU, or allocator dependency.

The first slice uses material palette shading, depth selection, vertical
ambient light, temperature response, and bounded local lava glow. It is not
the final Lightfield GI implementation. Future GPU and historical backends
must consume the same renderer-neutral snapshot and match the authoritative
material visibility rules.
