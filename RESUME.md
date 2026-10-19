# Tempest Enhanced: continuing

- Builds: `cmake --build build/pc` (MSYS2 gcc, SDL2; in Git Bash first
  `export PATH="/c/msys64/mingw64/bin:$PATH"`, else it fails without a
  message) and `cmake --build build/pi` (clang + lld,
  `cmake/toolchain-pi-clang.cmake`). Both are configured.
- Check the board: `sh tools/checkall.sh` (Git Bash; ROMs in
  `D:/Mame/roms`, recordings in `_scratch/cap`). It should print
  `ALL THE SAME`.
- To the cabinet: `sh tools/deploy.sh` (ssh key `~/.ssh/id_ed25519_vectorpie`,
  `pi@192.168.0.122`). It installs to `/persistent/games/tpenhanced`,
  restarts the game and shows `run.log`; if another game has the board it
  only installs.
- Package: stage `build/pi/tempest` with `package/*` in a folder named
  `tpenhanced`, then
  `sh external/VectorPie/sdk/tools/vp-pack/vp-pack.sh <folder> build/vpk`.
  Never put the ROMs in the package. Bump `version` in `package/game.ini`.
- The ROM on the cabinet: `/persistent/games/tpenhanced/roms/tempest.zip`
  (see STATUS.md).
- The game's routines stay in the ROM; enhancements hook onto RAM state,
  program addresses and what the picture holds (sub-lists and shapes of
  `tp_line`: `sub`, `item`, `ikey`).
- Looking at pictures without the cabinet: the probes in `_scratch/wide`
  (built with `gcc -I src ... build/pc/libtpcore.a build/pc/libminiz.a`):
  `probe` (attract mode, picture by picture: state, extents, sub-lists),
  `hshots T...` (horizontal layout at times of the attract mode, SVG),
  `capshots`/`dump`/`ext` (a MAME recording's pictures, lines, extents of
  a sub-list or shape), `titletest` (ENHANCED with a stand-in for the
  lettering), `fwtest` (fireworks of `hiscore.tpcap`, with the beam's dark
  way), `blink` (the display loop at a rate: blinking lines, empty
  pictures). SVG to PNG: `magick -density 48 F.svg -background black
  -flatten F.png`; recordings are drawn without the mirror bits (upside
  down).
- The title's lettering: `python tools/logo/logotrace.py` (needs `magick`)
  writes `src/render/logo_shape.h` from `tools/logo/tempest_logo.png`;
  `_scratch/logo/preview.py H OUT.svg` draws a header. `_scratch/wide/logotest`
  (title flight laid out, SVG, worst beam way; `NOLOGO=1` the game's
  logo), `ratesim3` (level select at 14 start moments), `fxreplay` with
  `SHOTS=",n,"`, `STATES=1`, `ROWS=1` (a recording with the enhancements).
  `flightsim STEPS SECS` (a robot game from a level chosen by spinner
  steps, 10: level 3, 40: level 9; five lives; main.c's display path at 50
  Hz; per flight picture states, tube extent, sub-lists; `FLIGHT=n` writes
  `fl_NNN.svg` of the n-th flight), `pacetest STEPS STEADY SECS` (game
  frames a second in play).
- Useful states (RAM 0000): 04 play, 0A pause/attract pictures, 18 flight
  into a level, 20 flight down the tube, 24 high score fireworks. Sub-lists:
  2004 text, 240A tube, 2DBC claw, 2E22 stars; shapes 2D4A/2692 the
  fireworks' clouds.
- High scores: `core/hiscore.c` (RAM 0606 initials, 0706 BCD scores,
  best last). `save/highscores.txt` is the port's table of 10 and the
  menu's; `main.c` puts its best 8 back into RAM after power-on and merges
  the program's table into it once a second (states 04, 0A only).
  `_scratch/wide/hstest`, `hsinject` test it.
- Smoothing checks: `_scratch/wide/jerk HZ SMOOTH` (accelerations per
  sub-list in a simulated game), `unp` (lines without a partner),
  `pairchk`. HUD through a transition: `hudcap` on `play1.tpcap` (frames
  520..650; recordings need mirror bits 0,1 to be upright).
- Sub-lists in play: 2690 enemies (figures of plain vectors), 28FE shots
  (shapes 3A40/3A72 alternate, 3594), 2A54 dots, 2200 spikes, 2CD6
  explosions.
- Enhancements: `render/enhance.c` (on the game's picture before the
  layout; switches `tp_config.fx[]`, menu page ENHANCEMENTS). Costs and
  pictures by replaying a recording: `_scratch/wide/fxreplay
  FILE.tpcap --roms D:/Mame/roms` (env `FXOFF=1`, `FXONLY=n`,
  `SHOTS=",f1,f2,"`, `TRACE=1`).
- The next level run ahead: `render/nextlevel.c` (`tp_layout_aim`).
- Next: look at the latest version on the cabinet (STATUS.md).
