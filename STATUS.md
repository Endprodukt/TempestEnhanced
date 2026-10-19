# Tempest Enhanced: status

## Current state (2026-10-19)

- The game logic runs as the original program from the owner's MAME
  `tempest.zip` (rev 3) on the 6502 core. Board, rendering, sound,
  controls, menu and tools are C. No C translation of Atari's routines.
- Board checks (`tools/checkall.sh`): all six MAME recordings in
  `_scratch/cap` match frame for frame (tpcompare and tplockstep).
- Version 0.1.30 is installed and running on the cabinet
  (`pi@192.168.0.122:/persistent/games/tpenhanced`), Pi binary md5
  `4cd7d171d7b42d728fe474c5a1e37ed3`, 50 fps.
- Package: `build/vpk/tpenhanced-0.1.30.vpk` (vp-pack, without ROMs).

## 0.1.30: the old tube's near rim left out in the flight (fx_rim)

- In the flight down the tube the game drops the old tube's near rim
  once the claw is past it (the tube about +-430 wide) in most levels,
  but in levels 4, 7 and 16 (closed shapes, also 20, 23, 32 ...) it draws
  it on all round the screen until the next level comes; the owner saw
  the old playfield over the flight from level 4 on. The cabinet shows it
  too (`_scratch/wide/rawcmp.png`: MAME's view against ours).
- New enhancement RIM (`fx_rim`, menu NO OLD RIM IN THE FLIGHT): in the
  flight states the near rim (the longer of the tube's two rim chains;
  only with both there) is left out once it is wider than 800 generator
  units (a tube at rest is ~490 at most, so the next tube coming in is
  never touched) (`render/enhance.c`).
- Checked with robot games landed in each level (`flightsim`, `POKE=N`
  sets the level, RAM 0046/009F/00DE, in the first flight; `DEMO=1`
  includes the attract mode's flights; `rimsum2.awk`): flights out of
  levels 1-17 (12 of the 16 shapes in games, the rest in the demo): only
  the 16 near rim lines go, only while the game draws all 48 (levels 4,
  7, 16: from ~0.24 s into the flight; elsewhere at most 1-2 frames before
  the game drops them itself); open shapes (8, 10, 14: 46 lines) are left
  to the game; in state 18 only the old level's last frames are touched.

## 0.1.29: the old level no longer jumps into the new one's place

- After the flight down the tube the game goes on drawing the old level
  (its tube all round the screen, the spikes, without the claw) for about
  0.1 s while already in states 0E and 18. The run-ahead switched the
  layout to the next level's scale and place as soon as the state left
  20, so that rest of the old level stood shifted and scaled for those
  frames (owner: from the spike levels on, where the spikes show it).
  Now the switch waits until the tube shown is no wider than the next
  level's at rest (`tube_width` in `render/nextlevel.c`;
  `tp_nextlevel_update` takes the shown picture). Checked with a robot
  game through `_scratch/wide/flightsim` (the display path of main.c):
  level 4 to 5, the old tube's extent stays -1520..5612 through 0E/18
  until the new tube comes at 2017..2079; its growth into play goes on
  without a step.

## 0.1.28: in the menu a button that turns does not go back

- The owner has the superzapper bound to the cabinet's left and right
  buttons: in the menu they then went back (the superzapper) as well as
  turning, and back won, so AUTO/NONE could not be chosen. Now a push
  that comes with a direction turns only (`nav.back` not while `nav.x`,
  `platform/controls.c`). Back stays with the superzapper elsewhere, Esc,
  or the pages' BACK line.

## 0.1.27: bindings can be set to none

- In the menu's CONTROLS and SPINNER pages left/right on a binding steps
  AUTO, NONE, AUTO (from a binding of its own first to AUTO); FIRE still
  adds, Del still takes the last away (NONE: back to AUTO). SERVICE MENU
  never goes to NONE (the cabinet could not reach the menu any more).
  The hint bar says LEFT RIGHT AUTO OR NONE.
- So that no binding can lock the cabinet out of the menu, its own
  spinner, left/right, fire, start 1 and superzapper always work in the
  menu, whatever is bound (`platform/controls.c`, the menu's `nav`).
- Checked (`_scratch/wide/nonetest`): the steps, NONE saved and loaded
  (`bind_zap = none`), a binding added to NONE replaces it, Del on NONE
  gives AUTO. Not yet tried on the cabinet's menu itself.

## 0.1.26: the attract mode's bottom lines centred

- The game starts its bottom line (CREDITS, then the coinage) at x -191
  whatever follows: with coins it reaches +191, with FREE PLAY only +104,
  so that line stood well left of the middle (owner); the minimum line
  (mid -6) and the copyright (mid -2) a little. In the attract mode each
  line of the bottom band is now moved to be centred (`centre_below` in
  `render/layout.c`, used in the text-only and the demo layout; the
  boxes, shape 34AA, are left alone). Checked
  (`_scratch/wide/botrows 02` and `00`): all three lines at the board's
  middle, 2048.

## 0.1.25: the attract mode's copyright not taken for RECHARGE

- In 0.1.24 every flight took the bottom band's text out of the HUD rows
  as SUPERZAPPER RECHARGE; in the attract mode's demo flights that is the
  copyright and the credits, which then stood large and white in the
  middle (owner). RECHARGE is now only the text between the bands (it lies
  at y 179..191, above the bottom band at 195); the bands stay as they
  are. Checked: the attract mode over 400 s (`_scratch/wide/attflight`,
  its flights at 43, 81, 116, 157 s): no line taken; in `play1` frame
  600 RECHARGE still large in the middle.

## 0.1.24: marquee lettering, recharge in the middle, level select, colours

- **Level select small first** (owner, reproduced): after START the game
  is already in state 16 while for about 0.1 s its picture is still the
  screen before (the high score table, in part); laid out as the choice
  it showed small. Now, in state 16, until the five holes show the
  picture stays dark (`render/layout.c`, `horizontal`). Checked with 14
  start moments (`_scratch/wide/ratesim3`): the choice appears at its
  full size every time. Also: the smoothing no longer slides letters
  between two different texts (`render/smooth.c`: text that changed pairs
  only where it stands still, `TEXT_SAME` 3 units).
- **SUPERZAPPER RECHARGE in the middle** (`fx_recharge`, menu RECHARGE IN
  THE MIDDLE): in the flight into the next level (states 20, 0E, 18) the
  game's text below the tube (y 179..191, colour 7, between the bands; see
  0.1.25) is drawn twice the
  text's scale, centred on the screen, light (215, 235, 255), full
  brightness. Off: small and blue under the tube, as before.
- **Title in the marquee's lettering** (`fx_logo`, menu MARQUEE TITLE
  LETTERING): `render/logo.c` replaces each copy of the game's logo
  (shape 3F4E, 34 lines, scaled about the field's middle) by the TEMPEST
  of the marquee, traced from the owner's picture by
  `tools/logo/logotrace.py` (`tools/logo/tempest_logo.png` →
  `render/logo_shape.h`, 8 outlines, 27 middle lines of the cream sides,
  239 segments): same width and top as the game's logo, down to -26.7.
  The front copy in the marquee's colours (yellow to red) with the cream
  sides, the copies behind as outlines in the game's colours, only every
  other one (the lettering is about twice the beam's way): the stack
  costs up to ~267,000 units, the game's logo ~283,000. ENHANCED moves
  down under it (`tp_logo_bottom`). The game's own logo with the switch
  off.
- **Warp**: stronger colours, the head the colour lightened instead of
  white, the streaks fade out in 0.25 s after the flight (was 0.6 s).
- **Stars beside the tube**: stronger colours (white, blue, pink, gold,
  cyan, violet), twinkling from 0.15 to full.

## 0.1.22/0.1.23: several bindings, warp as Tempest 2000's, initials screen

- **Several bindings an action** (as Battlezone Enhanced 0.1.17): up to
  `TP_BIND_ALTS` 8, joined by " or " in `bind_*` (`TP_BIND_SIZE` 320);
  in the menu FIRE adds what is pressed next (what was bound still does
  it), Del takes the last away (the last one left: auto), left/right back
  to auto. `tp_config_bind_add` / `_drop` (render/config.c),
  `parse_binds` and all of an action's bindings in `held` and the
  spinner (platform/controls.c). Checked: add, again (no double), drop,
  save and load.
- **Warp as Tempest 2000's** (its `draw_stars`, `update_zoom`,
  `update_warpin`): stars standing in space (x, y, depth), the viewer
  flying into the depth, each a streak from where it is to where it was
  0.14 s before (so as long as the speed), white head, tail in a soft
  colour; the vanishing point fixed: the middle of the tube's far rim, as
  last seen before the flight (it no longer follows the game's stars, so
  nothing "searches"); the speed gathers down the tube (state 20) and
  runs out towards the next tube (0E, 18), so the streaks shrink to dots;
  after the flight they fly on and fade (0.6 s). Checked in pictures of
  `play1` frames 530..665.
- **Level select** recognised also by what it shows (its five holes,
  colour 6) besides state 16: the owner saw it in the old layout first;
  not reproduced here (in every start tried the choice appears in state
  16 only).
- **Initials screen** (state 12): a layout of its own (`ENTRY` parts in
  `render/layout.c`): score and high score, HIGH SCORES, the first place
  1.8 times (yellow when it is the table's colour), the others 1.35
  times (as in the attract mode), PLAYER 1 and the instructions 1.4
  times, stacked with room between, the whole fitted to the screen; the
  copyright left out. Before, with the player on place 2 the table's
  detection moved everything down and PRESS FIRE TO SELECT was cut at the
  bottom (the owner's photo).

## 0.1.21: stars softly coloured

The owner: stars may be a little colourful, not loud. The stars beside
the tube each get a soft tint of their own (white, pale blue, pink, pale
yellow, pale cyan, lavender: `STAR_RGB`), anew each time one starts
again; the warp's colours toned down to match (`WARP_RGB`).

## 0.1.20: warp starfield, enemies break apart, twinkling, level select

The owner: sparks not nice yet, enemies hit should break apart; the stars
should twinkle; the flight to the next level jerky (streaks first in one
place, then in the next), and more warp-like in colour (mostly blue);
the lane's three floor lines away; the level select not yet laid out for
4:3 (it jumps once), wider and larger.

- **WARP** (`render/enhance.c`): the game's stars are clouds of ~40 dots
  (a few shape calls) that change from game frame to game frame; the
  smoothing paired dot k of one cloud with dot k of another, so they
  jumped at the game's 26 a second. Now, while the game shows its stars,
  they are left out and the port draws a starfield of its own: 64 streaks
  out from their middle (eased), faster the further out, every picture,
  a white head and a tail in a warp colour (blue, cyan, magenta, violet,
  white, amber), drawn round by angle; laid out as stars (over the width).
  Fades out over 0.4 s when the game's stars end. Menu: WARP STARFIELD.
- **SHATTER** (new switch, ENEMIES BREAK APART): where a new explosion
  shows up, the lines of enemies (2690, 2A54) of the picture before
  within 45 units that are gone now fly apart turning (0.8 s). One pool
  of 96 fragments with the claw.
- **SPARKS**: 6 short ones a burst (tail 0.018 s, 0.45 s), white to
  orange.
- **STARS** twinkle (each its own phase and speed, 3 to 9 a second).
- **LANE**: only the rim segments (the floor lines removed).
- **Level select** (state 16, `tp_layout_set_rating`): a fixed layout of
  its own: x as long as y (as the playfield), the field from PLAYER 1 to
  the TIME (x -233..215, y -76..207) as large as the screen takes it
  (width-limited), the copyright at its top left out. No easing, so no
  jump.
- Costs (`fxreplay`, my measure): the flight down the tube with the warp
  up to ~181,000 (board ~145,000), play up to ~183,000 (~146,000): inside
  50 Hz.

## 0.1.19: no DEPTH, lighter flights

- DEPTH removed altogether (the owner: it looked bad, and the spokes cut
  in pieces got bright dots where the beam stops). No `fx_depth` any
  more (taken out of the owner's settings too).
- The next level is no longer run ahead in the attract mode (its demo
  never plays a next level, so the copy ran 8 s for nothing: Pi frames up
  to 38 ms, 42 to 46 fps during the demo's flight); a slice is now
  200,000 cycles, given up after 5 s of the game.
- The warp stars drawn in the order of their angle round the middle (the
  game's order is at random): the flight into a level from about 90,000
  to about 75,000 units of dark beam in my measure.
- The stars beside the tube only in play (state 04).

## 0.1.18: graphic enhancements, 50 Hz, dark flash, initials screen

The owner wanted the graphics modernised a little, staying the original;
all of eight proposed effects, each to be switched off. Also: the
picture goes dark once at each level change; the initials screen
overlaps (the attract mode's copyright among the instructions); the
brake (cutting effects when the beam is short of time) off for now.

- **`render/enhance.c`**, on the game's picture (generator units, after
  the smoothing, before the layout), switched by `tp_config.fx[]`
  (`fx_*` in `tempest.ini`, the service menu's ENHANCEMENTS page with
  ALL ON / ALL OFF):
  - LANE: the game draws the claw's lane's two spokes in their own
    colour; the lane's rim segments in that colour too, and three faint
    floor lines between the spokes (in play only).
  - DEPTH: spokes in four pieces darker towards the far end (the way the
    game draws them, so the beam's way stays), the far rim at 0.55.
    Spokes, near and far rim told apart by the tube's lines: chains
    joined end to start are the rims (the shorter the far one), single
    lines the spokes.
  - WARP: the stars of the flights (states 20, 0E, 18) as streaks back
    towards their middle (0.2 of the way).
  - ZAP: the tube in changing colours while the superzapper works (RAM
    0125 counts in state 04 then; it also counts in the flight down the
    tube, hence state 04 only; found by trying).
  - TRAILS: the shots (28FE) of three pictures before at 0.45.
  - SPARKS: a new explosion (2CD6) throws 8 sparks (at most 64 at once),
    white to yellow to red.
  - CLAW: the claw (2DBC) gone in state 04 or 06: its lines fly apart
    turning, 1.1 s.
  - STARS (`tp_enhance_screen`, after the layout, horizontal only): 32
    faint stars drifting outwards, drawn only beside the tube, left to
    right.
  - FIREWORKS and TITLE switch the 0.1.1 fireworks and ENHANCED.
  Costs measured by replaying recordings (`_scratch/wide/fxreplay`, my
  measure reads about 1.25 times the board's): game pictures without
  effects up to ~136,000, with all ~190,000 (board about 155,000), too
  much for 60 Hz; sparks were the most (cut from 14 to 8 a burst).
- **Board rate 50** (`board_hz`, package and default; the picture page's
  BOARD RATE 40..60, applied at once). Tempest 2000's port sends 40.
- **Dark flash at a level change** (`render/frames.c`): between the
  flight down the tube and the next level the game shows one empty
  picture; it was shown (black). Now an empty picture leaves the last
  one standing.
- **Initials screen** (state 12, `tp_layout_set_entry`): the copyright
  left out and the text below not grown (it pushed into ENTER YOUR
  INITIALS etc.). After a game the table also shows RANKING FROM 1 TO 99
  and the player under it: then places 2 to 8 keep their size (room).
  Checked on `hiscore.tpcap` (frames 4129, 4136) and the attract mode.
- Effects and the new screens checked in pictures; not yet seen on the
  tube, nor the superzapper flash and the claw breaking in a picture.

## 0.1.17: the score in the attract mode where a game has it

The owner: in the attract mode the score stands elsewhere than in a
game. The score (colour 5, left of `SIDE_SPLIT`, above the playfield;
told before the text grows, as the larger high score reaches left of it)
now goes to the top left corner in the attract mode's demo (its row is
placed as in a game) and in pictures of text only such as the high score
table (in the corner with the scale and strip of the last picture with a
playfield). The rest of the attract mode's text stays in place. Checked
in pictures (attract 7.0 s and 10.0 s, `play1` frame 520).

## 0.1.16: marquee without the frame

The owner's third marquee (the artwork with the logo, no frame, 1665 x
445), made for the 1920 x 360 screen as in 0.1.14: 360 high (1347 wide),
in the middle, black at the left and right. 0.1.15's is in
`_scratch/marquee-alt/`. On the cabinet (md5 checked).

## 0.1.15: START lamps, the next level in place at once

The owner: the start lamps do nothing; the next level, flown into, first
stands a little off and is then moved into place.

- **START lamps** (`main.c`): the game's lamp port (60E0, kept by the
  board as `leds`) goes to the SDK's `vp_lamps_set` each picture: D1
  clear lights START 1, D0 clear START 2 (as MAME's `tempest_led_w`).
  Checked in the emulator: with one credit START 1 blinks, with two both.
- **The next level** (`render/nextlevel.c`, `tp_layout_aim`,
  `tp_layout_set_flight`): the layout fitted the playfield to a tube at
  rest, so through the flight it kept the old level's scale and then
  slid to the new one's for about 1.5 s. Now, when the flight begins
  (state 20, or 18), a copy of the machine runs ahead (500,000 cycles a
  picture, about 6.5 ms on the Pi), until the next level is played (state
  04); its tube gives the scale and place, set when the new tube first
  shows (0E or 18); during the flight (20, 0E, 18) the layout holds.
  Checked by replaying `play1.tpcap` with `tplockstep` plus the layout
  (`_scratch/wide/lockaim`, `AIM=1`): the new tube grows in its final
  scale and does not move once play goes on, except for what the game
  itself still animates. The attract mode's demo ends after its flight,
  so there the copy gives up (after 8 s of the game) and the layout just
  holds.

## 0.1.14: the marquee not stretched

The cabinet's marquee screen is 1920 x 360 (HDMI-A-1) and advmame.rc has
`vector_aux_marquee_display_mode stretch`, so any picture is stretched to
it. `package/marquee.png` is now made for it: 0.1.13's 2000 x 706
picture scaled to 360 high (1020 wide), in the middle, black at the left
and right (`magick SRC -resize x360 -background black -gravity center
-extent 1920x360`). 0.1.13's is in `_scratch/marquee-alt/`. On the
cabinet (md5 checked).

## 0.1.13: the marquee with its frame

The owner preferred the marquee with the cabinet's frame (the black
panel at the left, red and white border), 2000 x 706, now
`package/marquee.png`; 0.1.12's is in
`_scratch/marquee-alt/marquee-0.1.12.png`. On the cabinet (md5 checked).

## 0.1.12: new marquee

From the owner: the "Tempest Enhanced" marquee (the cabinet's artwork
with the logo and Enhanced), 1708 x 627, now `package/marquee.png` (was
1920 x 360; kept in `_scratch/marquee-alt/marquee-0.1.11.png`). The SDK
fits it to the HDMI screen as advmame.rc's marquee mode says. On the
cabinet (md5 checked); not yet looked at.

## 0.1.11: the playfield as large as the claw allows

The owner meant the levels' shapes: as large as possible without the
claw leaving the screen. The playfield's fit (`render/layout.c`) now
takes only the room for the claw beside the tube (`CLAW_*`, 40 units)
and a margin of 0.01 (`PF_MARGIN`, was 0.03), in a game too: the side
strips no longer make it smaller. The text keeps strips of at least
`STRIP_MIN` at the screen's sides and lies over the playfield's corners
where a level is that wide. Level 1 a little larger (margin), wide levels
much larger (the square and the cross now fill the height). Checked on
`play1` (levels 1 to 3) and `two`.

## 0.1.10: the level's number larger

The owner: the level itself could be a little larger. The number is now
`NUMBER_GROW` 1.6 times the word LEVEL, on its baseline (it starts at x
15.94, its letters end at y -179.30); the row's box takes the larger
number. Checked on `play1` and `two`.

## 0.1.9: LEVEL n at the bottom right

The owner: the level at the bottom right, with "LEVEL". In a game the
level's number now makes its own row at the bottom right, after the word
LEVEL in the game's own lettering: the port draws the 14 strokes the
game uses for it during the flight to the next level (`LEVEL_WORD` in
`render/layout.c`, measured on `play1.tpcap`, where the game puts them),
always, so it reads LEVEL 1, LEVEL 2 ... all the time and nothing shows
up between the levels (the game's own word stays left out). `tp_layout`
now returns the number of lines and needs room for `TP_LAYOUT_EXTRA`
more (`main.c`, `tpshot` updated). The row keeps `LEVEL_GROW`, but the
strip's width limits it. Checked on `play1` and `two`.

## 0.1.8: in a game no high score, the level larger

The owner: the high score at the top right is of no use in a game; the
level higher and larger; nothing new may show up between the levels. The
game's HUD is one call (2F60) for all of it, so its parts are told by
colour and place (`render/layout.c`, `left_out`): in a game (not the
attract mode) the high score (colour 5, small letters at y -209..-197
between x -56 and 52) and the word LEVEL of the flight to the next level
(colour 7 left of x 10) are not drawn; the level's number (colour 7) is
`LEVEL_GROW` 2 times the other text, as tall as the score, and with the
high score gone it is the top row on the right. Checked on `play1` (1
player, through a level's end) and `two` (2 players: player 2's score
stays on top on the right, the level below it).

## 0.1.7: the playfield as large as before

The owner: the playfield in a game must not be smaller. `STRIP_MIN` back
to 0.17 (0.1.5 had made it 0.21): level 1's circle fills the height again.
The text in the strips keeps `HUD_GROW`, so it is as large as the strips
let it be (larger than up to 0.1.4 only where they leave room).

## 0.1.6: the high score table's other places larger

Asked: places 2 to 8 larger too, with room left below. Now 1.35 times
(spacing with them, from the second place's top, 8 units lower); the
first place 1.8 times (was 1.5) so that it still stands out. The eighth
ends about 42 units above the (larger) copyright. Checked in a picture
(`_scratch/wide/hshots 10.0`).

## 0.1.5: larger HUD, copyright colour, first place, boxes, trembling

Reported: the HUD trembles; make every HUD entry clearly larger; give
the copyright its own colour (it was the tube's blue); the high score
table's first place bright yellow and larger; the colourful boxes before
the logo over the whole horizontal picture. All in `render/layout.c`.

- **HUD larger** (`HUD_GROW` 1.6): in the attract mode and in pictures of
  text only (high score table) the text bands above and below grow from
  the field's top and bottom; in a game the rows in the strips may be
  1.6 times the playfield's scale, as far as the strips let them (0.1.5
  widened the strips to 0.21 and so made the playfield smaller; undone in
  0.1.7).
- **Copyright** (colour 7 at y 207..219, below the playfield) orange.
- **High score table** (detected by the red HIGH SCORES at y -135..-111):
  the first place (colour 7 at y -92..-80) yellow and 1.5 times as large;
  the table moved 22 units down, clear of the larger INSERT COINS.
- **Boxes before the logo** (shape 34AA of the vector ROM, only there,
  attract 19.4..20.1 s): laid out over the whole screen, the outermost
  box (+-310.5 by +-335.4) to its edges; they no longer count for the
  fit of the rest of the picture.
- **Trembling HUD**: in a simulated game (`_scratch/wide/hudjit`) the
  HUD's corners do not move (13 of 4595 pictures, as many without
  smoothing), so the trembling is probably the beam: the text in the far
  corners was drawn between parts of the playfield, so the beam jumped
  to and fro across the screen, and text drawn right after a long jump
  trembles while the deflection settles. In a game the corner text is now
  drawn after the playfield, corner by corner (top left, top right,
  bottom right, bottom left). Not confirmed on the tube.

## 0.1.4: the attract mode's text in place

Reported: the text moved to the side strips looks wrong in the attract
mode. Now, while the game is in its attract mode (RAM 0005 `QSTATUS` D7
clear, from Atari's `ALCOMN.MAC`, checked: 00 in the attract mode, C0 in
a game), the text above and below the playfield stays where the cabinet
has it, over the playfield, laid out exactly as the high score table
lays it out (the text's field, -209 to 255, as tall as the screen), and
the playfield takes the whole width (no strips). `main.c` tells the
layout (`tp_layout_set_attract`) before each picture of the game. In a
game the strips stay. Checked in pictures (`_scratch/wide/hshots`: the
table and the demo's HUD lie in the same places).

Reported too: the picture trembles ("Zittern"). The cabinet's log shows
a steady 60 fps (0.1.3, 45 minutes). What trembles is not known yet.

## 0.1.3: spinner menu, high scores kept, round playfield, smoothing, HUD

Reported with 0.1.1/0.1.2: too many spinner options; keep more high
scores than the original; on a horizontal screen stretch the playfield
so that the circle is round, as the default; shots and enemies jump; at
the flight to the next level the HUD jumps away and back.

- **Spinner page** of the service menu: only SENSITIVITY and TEST ON
  LEVEL 1. `spin_ppr`, MAME DEFAULT, SET 5 SEGMENTS, the turn display,
  DIRECTION and DIGITAL TURNING are gone from the menu; `spin_ppr` from
  the settings altogether (`spin_reverse`, `key_speed` stay in
  `tempest.ini`). The test shows the segments moved and the sensitivity.
  The obsolete `spin_ppr` line was taken out of the owner's
  `save/tempest.ini` on the cabinet.
- **High scores kept** (`core/hiscore.c`, `main.c`): the port's own table
  of the best 10 in `save/highscores.txt` (also the menu's). Once the
  program has set its table up after power-on (state 04 or 0A), the
  file's best 8 are put back into its RAM, so all 8 places survive a
  power-off, not only the EAROM's 3; places 9 and 10 keep what fell out.
  Each new score of the program's is merged in (an entry in both once),
  never while initials are entered (states 24 and 12). Checked in the
  emulated attract mode (`_scratch/wide/hsinject`: a file of 10 shows in
  the game's table, a new score lands in place 3). The factory's entries
  (10101) count as entries, as in the game.
- **Round playfield** (`render/layout.c`, `PF_ASPECT_X` 1.0): the
  playfield on a horizontal screen with x as long as y, so the circle of
  level 1 (357 by 357 generator units) is round; the HUD text keeps the
  cabinet's proportions. Wide levels are limited by the side strips
  (`STRIP_MIN`) and come out a little smaller than the height allows.
- **Smoothing** (`render/smooth.c`): objects were paired in list order,
  so when an enemy or shot came or went, the others were paired with
  their neighbours and jumped in the pictures between frames; and shots
  change shape every frame (3A40/3A72), so they had no partner at all.
  Now: vectors of the sub-list joined end to start form one figure
  (enemies are drawn so); if a sub-list holds the same objects in both
  pictures they pair in order, otherwise each with the nearest of the
  same size, its own shape first. In a simulated game (`_scratch/wide/
  jerk`, `unp`): shots without a partner 23 % of them -> 4 % of their
  lines. Before, the smoothed picture jerked more than the unsmoothed one
  (shots 19 % against 12.5 %); now less in every group (jerks of 8 or
  more: enemies 1.6 % against 2.2 % unsmoothed, shots 6.3 % against
  11.4 %). Not yet seen on the tube.
- **HUD at the flight to the next level** (`render/layout.c`): the game
  writes LEVEL next to the level number then, 24 units beside the score;
  text rows joined anything within 30, so LEVEL glued the score and the
  high score into one row that went small and to the other side. Rows now
  join only lines of one colour (a text of the game is one colour).
  Checked on `play1.tpcap`: the corners stay, LEVEL 2 shows under the
  high score.

## 0.1.2: high scores on the marquee (extended in 0.1.3)

The menu's hint bar (on the marquee) shows a game's best scores from
`save/highscores.txt` when `game.ini` says `hiscores = yes` (Vectortron
does). Tempest keeps its table in the program's RAM and the EAROM, so
there was nothing to show. Now `core/hiscore.c` reads the table from RAM
(initials at 0606, last letter first; BCD scores at 0606+100h = 0706; the
best place last; addresses from Atari's `ALCOMN.MAC`, checked against
`hiscore.tpcap`: factory table EJD..BEH 10101, after the game AAA 12818
first) and `main.c` writes it to the file once a second when it changed
(from 3 s after start). `game.ini`: `hiscores = yes`. The file appears
with the first start of 0.1.2; the menu reads it after the game exits.
Only the best three survive a power-off (the program keeps only those in
the EAROM); places 4 to 8 are the factory's again after power-on, in the
game as in the file.

## 0.1.1: horizontal width, title, fireworks, 60 Hz

Reported on the cabinet with 0.1.0: the flight to the next level and the
attract mode kept to the 3:4 middle; ENHANCED too far below the title and
faded in too slowly; the high score fireworks only in the middle; the
picture flickers a little.

- **Stars of the flight to the next level** (sub-list 2E22, states 20 and
  18, attract and game alike): spread across the width in
  `render/layout.c`. In space the cloud of stars is stretched in x so that
  it is as wide as the screen as it was as tall as the upright one; while
  the tube is still flown through (its box larger than the screen), only
  the stars further out are stretched, so those in the far end of the tube
  stay in it. Checked in pictures (`_scratch/wide/hshots`).
- **ENHANCED** (`render/title.c`): now directly under TEMPEST (the title
  alone ends at y -98, ENHANCED from -88, 24 tall), and it starts once the
  title stands alone (its stack of copies taken apart, 25.85 s into the
  attract mode, about 2.1 s before the title goes). It flies in the way
  the title does: a stack of seven copies in changing colours out of the
  depth below it (0.35 s), then taken apart from the back (0.3 s). Checked
  with a stand-in for the lettering (`_scratch/wide/titletest`), not yet
  on the tube.
- **Fireworks of a new high score** (state 24; clouds of dots of the shape
  the program writes into both picture buffers, 2D4A and 2692):
  `render/fireworks.c`, horizontal screen only. Larger (their field fills
  the height), each cloud's middle pushed outwards to the screen's edges,
  and two delayed copies (18 and 36 pictures before, the first mirrored
  and higher, the second lower, colours turned): three times as many
  clouds. Dots ordered for a short beam: at most about 119,000 units per
  picture (from 175,000 unordered). Checked against `hiscore.tpcap`
  (`_scratch/wide/fwtest`), not yet on the tube.
- **Flicker**: `board_hz` 60 (was 50; package `tempest.ini` and the
  default in `config.c`). The cabinet's DVG REFRESH (menu, 40) is only the
  rate at which the menu itself sends pictures (its `DvgCmd` has no
  command for it, `bzone2.cfg` calls it "frames a second sent to the
  board"), so the game's own rate is what the tube shows. A picture of
  the game needs about 87,000 to 97,000 units of beam (lit plus blank),
  the fireworks up to 119,000; by Battlezone's measurement (170,000 units
  in 20 ms) that is at most about 14 ms, inside the 16.7 ms of 60 Hz.
  Ruled out in simulation (`_scratch/wide/blink`): lines that vanish for
  one picture (none to speak of) and empty pictures (only where the game
  itself changes scene). Whether 60 Hz is steadier on the tube is not yet
  confirmed.

## Known issues and limitations

- The cabinet's AdvanceMAME `tempest.zip` (`/usr/local/share/advance/rom`)
  is the old 1996 2K-chip set, which the game does not accept. The owner's
  modern set was copied to `/persistent/games/tpenhanced/roms/tempest.zip`.
  Untested: whether INSTALL GAMES (vpk upgrade) keeps that `roms/` folder.
  If not, the ROM has to be copied again after an upgrade (or moved to a
  place that survives, such as `/home/pi/.advance/rom`, if AdvanceMAME
  does not trip over the modern set there).
- `BUILD.md` is referenced by `CMakeLists.txt`, `deploy.sh` and the
  toolchain file but does not exist.
- `tests/` is empty; the checks are the MAME recordings above.
- The Pi build points to the sysroot of the Tempest 2000 port
  (`F:/Tempest 2000 Port/build/pi-sysroot`).
- The PC build (`build/pc`, MSYS2 gcc) fails without a message unless
  `/c/msys64/mingw64/bin` is first in `PATH` (the `gcc` found first is
  the retcomm clang).
- ENHANCED is drawn in the SDK's lettering in board units, which a
  horizontal screen stretches 4:3; its width was chosen with that.
- The fireworks copies are only made on a horizontal screen; vertical
  stays as the cabinet.
- Not yet confirmed by looking at the cabinet: whether the menu lists the
  game under Atari.

## Outstanding tasks

- The owner finds the later levels (level 9, chosen at the start) too
  fast. Measured (2026-10-17, `_scratch/wide/pacetest`: a robot game at
  levels 1, 3 and 9, cabinet timing against steady frames): 27.1 to 27.3
  game frames a second in play either way, the cabinet's timing being the
  one that matches MAME frame for frame; the emulation runs exactly the
  real time passed (`tp_app_poll`), difficulty MEDIUM 1 (MAME's default);
  smoothing adds no delay (the copy runs ahead). Steady frames keep the
  average, but not the cabinet's short slowdowns when much is on the
  screen (it averages over ~2 s). To try: GAME FRAMES AS THE CABINET.

- Look at 0.1.24/0.1.25 on the cabinet: the marquee lettering (sharp enough at
  the copies' small sizes? flicker while it flies in), RECHARGE in the
  middle, the level select after START, the warp's and stars' colours.
- Look at 0.1.3/0.1.4 on the cabinet: the attract mode's text, stars, ENHANCED, fireworks, flicker at
  60 Hz, round playfield and HUD, shots and enemies, the high scores in
  the game and the menu's hint bar after a power-off.
- The attract mode's logo flying in (its stack of copies, up to ~700
  lines, now very large) and the boxes before it (over the whole screen)
  cost the board up to ~285,000 units (lit plus blank) in the log, far
  over a picture's time at 50 Hz (~170,000): the board draws them faster
  and dimmer. Older than the enhancements (the layout of 0.1.1/0.1.5).
- The HUD trembles (owner): check whether drawing it corner by corner
  (0.1.5) cured it; if not, a dark pause before each corner (the beam
  settling), or the board's slew and jump settings.
- Write `BUILD.md` (PC build, Pi build with clang and the sysroot,
  deploy, vpk).
- Settle where the ROM lives on the cabinet so that upgrades keep it.
- If the flicker stays: shorten the beam's dark way of the game's picture
  (about 41,000 units, much of it jumps to the text in the corners), in an
  order that stays the same from picture to picture.

## Belegung wie bei allen Vektorspielen (2026-10-19)

- Menü (`src/platform/menu.c`): START 1/Enter auf einer Zeile, dann ein Knopf; der erste seit der Cursor auf der Zeile steht ersetzt die Belegung, jeder weitere START fügt hinzu (" or "). Während des Wartens: Esc/Enter (bei Spielfunktionen auch START 1) = zurück ohne Änderung, START 2 = NONE (nur Spielfunktionen), Entf (auch Keypad) = AUTO. Links/Rechts = AUTO/NONE (Service-Menü-Zeile nie NONE). Anzeige mit echten Namen ("LEFT CTRL + JOY1 BUTTON 1"), eine Taste darf mehrere Aktionen haben.
- Das Menü geht nur über feste Eingaben (`tp_nav`, über `src/platform/vpbind.[ch]`, Kopie aus dem Tempest-2000-Port): Pfeile, jeder Stick/Hat, Spinner (Maus), START 1/Enter/Keypad-Enter, Esc zurück. Belegte Feuer-/Zapper-Tasten wirken im Menü nicht mehr.
- AUTO wird direkt gelesen (`AUTO_SPEC` in controls.c, die AdvanceMAME-Standards: Pfeile/erster Stick, Left Ctrl/Left Alt + Stick-Knöpfe + Maustasten, 1/2/5/6); neu: Maustasten als Belegung (`mouse button left`). Beim Belegen nie: F2, Esc, Enter, Keypad-Enter, Entf; bei Spielfunktionen auch die START-/COIN-Tasten.
- Am PC geprüft (ersetzen, hinzufügen, NONE/AUTO), Pi-Build am Automaten installiert (alte Datei `tempest.before-vpbind`), dort nicht ausprobiert.
