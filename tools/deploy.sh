#!/bin/sh
# deploy.sh - copy the Pi build to the cabinet, put the new program in place
# and start it there, so that what is on the screen is the new build. A
# game that is running is ended first. When the game is left, the cabinet's
# menu comes back (package/run.sh). If another game has the board, the new
# build is only installed. Run from the project's folder, in Git Bash,
# after the Pi build (BUILD.md). Arguments go to the game.
K="-i $HOME/.ssh/id_ed25519_vectorpie -o ConnectTimeout=8 -o BatchMode=yes"
H=pi@192.168.0.122
D=/persistent/games/tpenhanced
ssh $K $H "mkdir -p $D/save" || exit 1
scp -q $K build/pi/tempest $H:$D/tempest.new || exit 1
scp -q $K package/game.ini package/tempest.ini package/marquee.png package/run.sh $H:$D/ || exit 1
# Another game on the board (started from the cabinet's menu): install only,
# do not start ours over it (two programs on one board slow both down).
other=$(ssh $K $H 'for p in /proc/[0-9]*; do ls -l $p/fd 2>/dev/null | grep -q ttyACM && tr "\000" " " < $p/cmdline && echo; done' | grep -a -v -e vector_pie_menu -e tempest)
if [ -n "$other" ]; then
    ssh $K $H "cd $D && chmod 755 tempest.new run.sh && mv -f tempest.new tempest && md5sum tempest && grep '^version' game.ini"
    echo "deploy.sh: installed, not started: another program has the board: $other"
    exit 0
fi
ssh $K $H "cd $D && pkill -INT -x tempest; for i in 1 2 3 4 5 6; do pgrep -x tempest > /dev/null || break; sleep 0.5; done; pkill -KILL -x tempest; chmod 755 tempest.new run.sh && mv -f tempest.new tempest && md5sum tempest && grep '^version' game.ini && (setsid nohup ./run.sh $* > /dev/null 2>&1 &) ; sleep 6; pgrep -x tempest > /dev/null && echo game-running || echo GAME-NOT-RUNNING; tail -n 5 run.log"
md5sum build/pi/tempest
