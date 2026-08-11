<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# DIGS for Android (experimental)

This is the Android host for the released `v0.0.3` DIGS game. It uses the
same C89/C++98 simulation, Lua catalog, CPU renderer, SDL2 audio, and 60 Hz
authoritative input path as the desktop demo. The Android-specific code only
packages the existing data and supplies phone controls.

## Build a debug APK

Install Android SDK platform 34, CMake 3.22.1, Android NDK 26.3.11579264, and
JDK 17. Initialize the pinned SDL2 source first:

```sh
git submodule update --init --recursive
cd ports/android
./gradlew assembleDebug
```

The phone APK is written to
`app/build/outputs/apk/debug/app-debug.apk`. It contains ARMv7 and ARM64
native libraries, uses Gradle's debug signing for installable test builds, and
requires Android 6.0 (API 23) or newer. Release signing and hardware
acceptance are intentionally outside this experimental target.

## Phone controls

All controls are at least 48 dp where a button is used, retain a high-contrast
label, and expose a content description to Android accessibility services.

| On-screen control | During a match | In menus |
|---|---|---|
| LEFT / RIGHT | Run | Move left / right |
| JUMP / STEAM | Jump / use steampack | Move up / down |
| AIM | Drag to aim | — |
| FIRE | Fire, charge, or respawn | Select |
| ROPE | Hold or toggle rope, following DIGS input settings | — |
| TOOL | Tap: next weapon; hold: previous weapon | — |
| PAUSE or Android Back | Pause | Go back |

Bluetooth and USB controllers remain available through the same SDL2 controller
path as desktop. The mobile overlay activates the existing keyboard-style
player-one source, so it never creates an alternate simulation or weapon path.
