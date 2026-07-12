# VOX + DIGS

VOX is a portable, CPU-authoritative mini-voxel engine. DIGS is its first
game: a side-view steampunk-miner deathmatch built around destructible
terrain, systemic materials, and deterministic replays.

The repository is in foundation phase. The first public milestone is the
VOX + DIGS `v0.0.1` demo, verified initially on Linux x86-64. Windows,
macOS, and historical platforms are tracked in `ROADMAP.txt` and are not
claimed as supported until reproducible acceptance evidence exists.

## License

Original VOX and DIGS code, documentation, shaders, and assets are released
under the GNU General Public License version 3 or later. See `LICENSE` and
`LICENSES/` for the full notice and third-party policy.

## Current foundation proof

The first implementation slice is a strict C89 headless kernel with:

- fixed-width integer state and deterministic stepping;
- a shallow voxel slab with activity/sleep flags;
- material definitions for air, stone, soil, water, lava, and bedrock;
- canonical state hashing and a repeatable scenario test.

Run it with:

```text
cmake -S . -B build -DVOX_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The renderer, Rust host, physics layer, Foundry Lab, and DIGS vertical slice
are added behind the same versioned C ABI.
