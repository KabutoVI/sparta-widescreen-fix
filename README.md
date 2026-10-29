# Sparta: Ancient Wars - 1440p / widescreen UI fix

A `d3d9.dll` proxy for Sparta: Ancient Wars (AWE engine, v1.2 build 616, 32-bit, Spanish/Italian
release). It forwards to the system Direct3D 9 and patches two things in memory. Files on disk
stay untouched.

## What it fixes

- **Menu and GUI offset at widths above 2048.** The base GUI window constructor in `Sparta.dll`
  clamps windows to 2048x1600, which pushed the whole 2D layer right by (width - 2048) pixels
  at 2560x1440. The proxy raises the two limits.
- **HUD size.** `gui_scale.ini` (`[scale]` with `width_div` and `height_div`; the game's own
  defaults are 1600 and 1200) overrides the two scale divisors, so the in-battle HUD can be made
  smaller. A larger divisor gives a smaller HUD.

The `gui_scale.ini` in the release is set for 2560x1440 (`width_div=2133`, a scale of 1.2 at
2560 wide). For the game's stock HUD size, set `width_div=1600`.

Patches are byte-checked against build 616 and skipped on mismatch. Tested against a
`Sparta.dll` with SHA-256 `3c4215d639a11f358bad70114ff0c43bbda428100a5676c81cf65aa57830d5a4`.

## Status

Checked in a copy of the game folder at 2560x1440 borderless: the main menu and the in-battle
HUD. Not tested: other aspect ratios, true fullscreen, and every in-game dialog. The bottom HUD
bar stays left-aligned.

## Install

1. Download `sparta-widescreen-fix-v0.1.0.zip` from the Releases page, or build it yourself.
2. Copy `d3d9.dll` and `gui_scale.ini` next to `AWE.exe` in your own install. Keep a copy of
   anything you replace.
3. Set your resolution in the profile (`Profiles\...\profile.ini`, `ResolutionX` and
   `ResolutionY`). With `g_window_popup_mode 1` in `AWE.ini` the window is borderless.

## Uninstall

Delete `d3d9.dll` and `gui_scale.ini` from the game folder.

## Build

From an MSVC x86 command prompt, run `build_fix.bat`. It produces `d3d9fix.dll`; name it
`d3d9.dll` to use it.

`d3d9log.c` is a logging variant I used to find the cause (built with `build_log.bat`).
`tools/pakls.py` lists the contents of the game's `.pak` archives. No game files are included.

## License

[MIT](LICENSE)
