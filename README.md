# DIGS

## Download and run

Download a package from the [DIGS v0.0.3 release](https://github.com/claire-moon/vox/releases/tag/v0.0.3).

### Windows x86-64

1. Download `vox-digs-v0.0.3-windows-x86_64.zip`.
2. Extract the complete ZIP file.
3. Open the extracted folder.
4. Double-click `run-digs.bat`.

Keep `bin/` and `share/` in the extracted folder.

### Linux x86-64

1. Download `vox-digs-v0.0.3-linux-x86_64.tar.gz`.
2. Open a terminal in the download folder.
3. Run:

```sh
tar -xzf vox-digs-v0.0.3-linux-x86_64.tar.gz
cd vox-digs-v0.0.3-linux-x86_64
./run-digs.sh
```

Keep `bin/`, `bin/share`, `share/`, and `libexec/` in the extracted folder.

If the launcher says SDL2 is missing, install the SDL2 runtime from your Linux
distribution and run `./run-digs.sh` again.

## Experimental Android port

The released `v0.0.3` game has an Android source port under
[`ports/android`](ports/android/README.md). It compiles the same SDL2 host,
deterministic core, scripts, renderer, and audio path with accessible,
on-screen controls for phone users. It is not yet a released or
hardware-verified Android package.

## License

DIGS and VOX are licensed under [GPL-3.0-or-later](LICENSE).
