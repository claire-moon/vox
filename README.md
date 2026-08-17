# DIGS

## v0.0.5 release boundary

This tree builds the v0.0.5 DIGS candidate. It is not a public download merely
because a local or CI package exists: public packages appear only on the
[GitHub Releases page](https://github.com/claire-moon/vox/releases) after the
matching tag has passed the full
[v0.0.5 release checklist](docs/releasing/V0.0.5_RELEASE_CHECKLIST.md).

Artifacts whose filenames end in `-ci` are nonpublishing review evidence. They
must not be offered as a release, linked as a download, or used to infer that a
platform has completed manual acceptance.

When v0.0.5 is accepted, its public release contains:

- a Linux x86-64 bundle;
- a Windows x86-64 ZIP;
- the matching Corresponding Source archive; and
- SHA-256 checksums, SPDX SBOMs, notices, and package evidence.

No legacy Win32 or Android package is part of v0.0.5.

## Build from source

Use a fresh build directory and run the repository gates from the checkout:

```sh
cmake -S . -B build -DVOX_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
git diff --check
```

For the SDL2 desktop demo, configure with `-DVOX_BUILD_SDL2_DEMO=ON` and make
SDL2 development files available on the host. `tools/vox-verify.sh` performs
the stricter native/NASM gate where its dependencies are installed.

## Documentation

| Document | What it covers |
|---|---|
| [CONCERNS.md](CONCERNS.md) | Traps, unproven claims, and decisions most likely to be wrong — read first |
| [docs/DEMO.md](docs/DEMO.md) | Current game behavior, menus, controls, materials, and explicit gaps |
| [docs/ROADMAP_V0.0.5.md](docs/ROADMAP_V0.0.5.md) | The complete v0.0.5 product scope |
| [docs/releasing/V0.0.5_RELEASE_CHECKLIST.md](docs/releasing/V0.0.5_RELEASE_CHECKLIST.md) | Required release, package, hosted, and human gates |
| [docs/releasing/V0.0.5_BRANCH_AUDIT.md](docs/releasing/V0.0.5_BRANCH_AUDIT.md) | Ref retention and post-release cleanup order |
| [ROADMAP.txt](ROADMAP.txt) | Historical release-by-release support states |
| [docs/architecture/](docs/architecture/) | Engine and port design |
| [docs/rfcs/](docs/rfcs/) | Load-bearing design decisions |
| [docs/compatibility/MATRIX.md](docs/compatibility/MATRIX.md) | Measured evidence and its boundaries |
| [qa/](qa/) | Workbook, quick feedback guide, and tester cockpit |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Language, determinism, and RFC requirements |

## License

DIGS and VOX are licensed under [GPL-3.0-or-later](LICENSE).
