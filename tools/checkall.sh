#!/bin/sh
# checkall.sh - hold the board against every MAME recording of a folder.
#
#   sh tools/checkall.sh [FOLDER]
#
# Twice per recording: tpcompare (each frame from MAME's state, with
# MAME's reads) and tplockstep (the board on its own from power-on, only
# the inputs and the random numbers MAME's). Recordings: tools/captures.sh.
# The programs: build/pc. The ROMs: $TP_MAME_ROMS (default D:/Mame/roms).
cd "$(dirname "$0")/.." || exit 1
dir=${1:-_scratch/cap}
roms=${TP_MAME_ROMS:-D:/Mame/roms}
bad=0
for f in "$dir"/*.tpcap; do
    ./build/pc/tpcompare "$f" --roms "$roms" --max 3 --show 8 --quiet || bad=1
    ./build/pc/tplockstep "$f" --roms "$roms" | grep -v "^generator" | tail -3 || bad=1
    ./build/pc/tplockstep "$f" --roms "$roms" > /dev/null || bad=1
done
[ $bad = 0 ] && echo "ALL THE SAME" || echo "DIFFERENCES"
exit $bad
