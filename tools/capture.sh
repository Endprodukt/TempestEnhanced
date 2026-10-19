#!/bin/sh
# capture.sh - record a run of Tempest (MAME set tempest) for tpcompare.
#
#   sh tools/capture.sh OUT.tpcap SECONDS ["PLAY SCRIPT"]
#
# MAME and its ROM folder: $TP_MAME (default D:/Mame/mame.exe) and
# $TP_MAME_ROMS (default D:/Mame/roms). MAME runs without picture and
# sound, as fast as it can, with a configuration folder of its own
# (_scratch/mame), so that the owner's MAME settings stay untouched.
# The play script: see tools/mame/tpcapture.lua.
here=$(cd "$(dirname "$0")/.." && { pwd -W 2>/dev/null || pwd; })
out=$1
secs=${2:-60}
play=$3
case "$out" in
    /*|?:*) ;;
    *) out="$({ pwd -W 2>/dev/null || pwd; })/$out" ;;
esac
# (a fresh configuration every time: MAME keeps option switches a script
# has set, and its EAROM file would carry high scores into the next run)
rm -rf "$here/_scratch/mame/cfg" "$here/_scratch/mame/nvram"
mkdir -p "$here/_scratch/mame/cfg" "$here/_scratch/mame/nvram"
# $TPCAP_WAV: MAME's sound of the run as a WAV file too
wav=$TPCAP_WAV
case "$wav" in
    ""|/*|?:*) ;;
    *) wav="$({ pwd -W 2>/dev/null || pwd; })/$wav" ;;
esac
cd "$here/_scratch/mame" || exit 1
TPCAP_OUT="$out" TPCAP_PLAY="$play" "${TP_MAME:-D:/Mame/mame.exe}" tempest \
    -rompath "${TP_MAME_ROMS:-D:/Mame/roms}" -cfg_directory cfg -nvram_directory nvram \
    -autoboot_script "$here/tools/mame/tpcapture.lua" ${wav:+-wavwrite "$wav"} \
    -video none -sound none -nothrottle -skip_gameinfo -seconds_to_run "$secs" 2>&1 | grep -i "tpcapture\|error"
