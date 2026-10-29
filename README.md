# Sparta: Ancient Wars - 1440p / widescreen UI fix

A `d3d9.dll` proxy for Sparta: Ancient Wars (AWE engine, build 616, 32-bit). It forwards to the
system Direct3D 9 and patches two things in memory. Files on disk stay untouched.

## What it fixes

- **Menu/GUI offset at widths above 2048.** The base GUI window constructor in `Sparta.dll`
  clamps windows to 2048x1600, which pushed the whole 2D layer right by (width - 2048) pixels
  at 2560x1440. The proxy raises the two limits.
- **HUD size.** `gui_scale.ini` (`[scale] width_div`, `height_div`; defaults 1600/1200) overrides
  the two scale divisors, so the in-battle HUD can be made smaller. Example: `width_div=2133`
  gives a scale of 1.2 at 2560 wide.

Patches are byte-checked against build 616 and skipped on mismatch.

## Status

Verified in a lab copy at 2560x1440 borderless: main menu and in-battle HUD. Not tested:
other aspect ratios, true fullscreen, every in-game dialog. The bottom HUD bar stays left-aligned.

## Use

1. Build with `build_fix.bat` (MSVC x86 prompt), producing `d3d9fix.dll`; name it `d3d9.dll`.
2. Put `d3d9.dll` and `gui_scale.ini` next to `AWE.exe` in your own install. Back up what you replace.
3. Set your resolution in the profile (`Profiles\...\profile.ini`, `ResolutionX/Y`); with
   `g_window_popup_mode 1` in `AWE.ini` the window is borderless.

`d3d9log.c` is a logging variant I used to find the cause; `tools/pakls.py` lists the contents of
the game's `.pak` archives. No game files are included.
