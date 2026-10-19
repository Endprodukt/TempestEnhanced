#!/bin/sh
# captures.sh - make the standard MAME recordings again (_scratch/cap).
#
#   sh tools/captures.sh
#
# Frames are counted from power-on, about 26 a second.
cd "$(dirname "$0")/.." || exit 1
mkdir -p _scratch/cap
sh tools/capture.sh _scratch/cap/attract.tpcap 60
sh tools/capture.sh _scratch/cap/play1.tpcap 240 "60 coin 2;90 start 2;140 auto 5"
# two players: two coins, start 2, both play until the end
sh tools/capture.sh _scratch/cap/two.tpcap 300 "60 coin 2;70 coin 2;90 start2 2;140 auto 5"
# a game played to its end, then the entry of initials for a high score
sh tools/capture.sh _scratch/cap/hiscore.tpcap 360 "60 coin 2;90 start 2;140 auto 4;6500 auto 0;7000 spin 2 20;7100 fire 2;7200 spin -3 10;7300 fire 2;7400 fire 2;7500 fire 2"
# the self test, on and off again
sh tools/capture.sh _scratch/cap/selftest.tpcap 60 "200 test 1;900 test 0"
