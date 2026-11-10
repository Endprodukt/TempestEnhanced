# Tempest Enhanced

Atari's *Tempest* (1981) on a VectorPie cabinet's vector monitor, with a
widescreen layout and optional modern effects.

The game itself is the original: Atari's program runs unchanged from your
own ROM set on an emulated 6502, together with the cabinet's vector
generator, its two POKEY sound chips and its EAROM, all written in C and
checked frame by frame against MAME. Everything around the game is new:
the picture laid out for a horizontal 4:3 tube, smooth 50 Hz motion, the
effects below, a service menu, high scores kept for the VectorPie menu,
and the controls of the VectorPie cabinet.

## What you need

- A VectorPie cabinet (Raspberry Pi with a USB-DVG vector board) with the
  VectorPie menu 1.1.9 or newer.
- The Tempest ROMs. They are **not** included. The game looks for MAME's
  `tempest.zip` (revision 3) in `/persistent/games/tpenhanced/roms/`, in
  the ROM folders of the cabinet's `advmame.rc` (`dir_rom`) and in
  `/usr/local/share/advance/rom`. The older set in 2K chips that
  AdvanceMAME uses works too; the current MAME set is preferred, because
  its PROM `136002-125` gives the vector generator's exact timing.

## Installing

Put `tpenhanced-<version>.vpk` in the `games/` folder of a USB drive (or
copy it to `/persistent/games/packages/`), then choose **Settings → GAMES →
INSTALL GAMES** in the VectorPie menu. The game is listed under Atari.

## Playing

The cabinet's spinner, fire and superzapper buttons, START and COIN work
as on the original. Hold **START 1 and FIRE** together (or press the
service key, F2 on a keyboard) to open the service menu:

- **Controls**: any key, button, stick or hat for every action, several
  at once; Del takes the last one away, left/right steps AUTO or NONE.
- **Spinner**: sensitivity and a test (the direction: `spin_reverse` in
  `save/tempest.ini`).
- **Picture**: orientation, smoothing, steady game frames, the vector
  board's frame rate, size, position, mirroring, brightness and a test
  pattern.
- **Game settings**: the cabinet's DIP switches (lives, bonus life,
  difficulty, coinage, language...).
- **Enhancements**: every effect on its own switch, or all off for the
  original look.
- On the first page: the sound volume and resetting the high scores.

Settings are kept in `save/tempest.ini`, high scores in `save/`.

## Enhancements

All can be switched off one by one:

- the claw's lane lit
- a 3D warp starfield in the flight to the next level
- the tube flashing with the superzapper
- shots leaving short trails
- explosions throwing sparks
- the claw and hit enemies breaking apart
- the old tube's near rim left out in the flight once passed
- twinkling, softly coloured stars beside the tube
- the high score fireworks over the whole width
- ENHANCED flying in under the title
- SUPERZAPPER RECHARGE large in the middle of the screen
- the title drawn in the lettering of the cabinet's marquee

## Sound

The two POKEYs are emulated and mixed by the VectorPie SDK, so the
cabinet menu's volume applies (the SDK counts a game's own sound stream
as music: **MUSIC VOLUME**), on top of the game's own VOLUME setting.

## Building

- PC (MSYS2, MinGW-w64 gcc, SDL2): `cmake -B build/pc` then
  `cmake --build build/pc`; in Git Bash put `/c/msys64/mingw64/bin` first
  in `PATH`.
- Raspberry Pi (clang and lld with a copy of the cabinet's system):
  `cmake -B build/pi -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-pi-clang.cmake`
  then `cmake --build build/pi`.
- The VectorPie SDK goes in `external/VectorPie`
  (https://github.com/Goldfinch2/VectorPie); this version was built with
  SDK 0.9.0, commit `b00d537`.
- Package: `external/VectorPie/sdk/tools/vp-pack/vp-pack.sh <folder>`
  on a folder named `tpenhanced` holding the Pi program and `package/*`.

`STATUS.md` holds the state of the work, `RESUME.md` notes for going on.

## Legal

*Tempest* is a trademark of Atari. This project holds no Atari code or
data: the program and the vector ROM are read from your own ROM set at
start-up.
