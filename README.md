# DIGS

## Download and run

Download a package from the [DIGS v0.0.4 release](https://github.com/claire-moon/vox/releases/tag/v0.0.4).

### Windows x86-64

1. Download `vox-digs-v0.0.4-windows-x86_64.zip`.
2. Extract the complete ZIP file.
3. Open the extracted folder.
4. Double-click `run-digs.bat`.

Keep `bin/` and `share/` in the extracted folder.

### Linux x86-64

1. Download `vox-digs-v0.0.4-linux-x86_64.tar.gz`.
2. Open a terminal in the download folder.
3. Run:

```sh
tar -xzf vox-digs-v0.0.4-linux-x86_64.tar.gz
cd vox-digs-v0.0.4-linux-x86_64
./run-digs.sh
```

Keep `bin/`, `bin/share`, `share/`, and `libexec/` in the extracted folder.

If the launcher says SDL2 is missing, install the SDL2 runtime from your Linux
distribution and run `./run-digs.sh` again.

## Documentation

| Document | What it covers |
|---|---|
| [CONCERNS.md](CONCERNS.md) | Traps, unproven claims, and decisions most likely to be wrong — read first |
| [docs/DEMO.md](docs/DEMO.md) | What the current build does: menus, controls, arsenal, the miners |
| [docs/ROADMAP_V0.0.5.md](docs/ROADMAP_V0.0.5.md) | What is planned next, and in what order |
| [ROADMAP.txt](ROADMAP.txt) | Release-by-release support states |
| [docs/architecture/](docs/architecture/) | How each subsystem works |
| [docs/rfcs/](docs/rfcs/) | Why the load-bearing decisions were made |
| [docs/compatibility/MATRIX.md](docs/compatibility/MATRIX.md) | What has actually been measured, and on what |
| [docs/releasing/](docs/releasing/) | Per-release checklists |
| [qa/](qa/) | The tester lane: checkpoints, workbook, quick-feedback guide |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Language boundaries, determinism rules, when an RFC is required |

Start with `docs/DEMO.md` to know what the game is, and
`docs/architecture/FOUNDATION.md` to know how it is put together.

## License

DIGS and VOX are licensed under [GPL-3.0-or-later](LICENSE).
