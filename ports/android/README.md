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
The overlay changes its visible controls when a match begins, so the player
never needs to use a gameplay-labelled button to navigate a menu.

| Screen | On-screen control | Action |
|---|---|---|
| Menu, setup, pause, results | UP / DOWN | Move the selected item |
| Menu, setup, pause, results | LEFT / RIGHT | Change the selected value |
| Menu, setup, pause, results | SELECT | Activate the selected item |
| Menu, setup, pause, results | BACK or Android Back | Go back |
| Match | LEFT / RIGHT | Run |
| Match | JUMP / STEAM | Jump / use steampack |
| Match | AIM | Drag for gamepad-style directional aim |
| Match | Blank screen space | Hold and drag for mouse-style direct cursor aim |
| Match | FIRE | Fire, charge, or respawn |
| Match | ROPE | Hold or toggle rope, following DIGS input settings |
| Match | TOOL | Tap: next weapon; hold: previous weapon |
| Match | PAUSE or Android Back | Pause |

Bluetooth and USB controllers remain available through the same SDL2 controller
path as desktop. The mobile overlay activates the existing keyboard-style
player-one source, so it never creates an alternate simulation or weapon path.
