#!/bin/sh
# Build the headless test runner (needs libmgba) and play a script.
# Usage: sh test/run.sh test/v2.txt   (screenshots land in test/)
set -e
cd "$(dirname "$0")"
# Prefer the local source build in third_party/mgba (macOS); else use the system libmgba.
MGBA="../../third_party/mgba"
if [ -d "$MGBA/lib" ]; then
    gcc harness.c -o harness -I"$MGBA/include" -L"$MGBA/lib" -Wl,-rpath,"$(cd "$MGBA/lib" && pwd)" -lmgba
else
    gcc harness.c -o harness -lmgba
fi
# Step by game loops, not emulator frames: pass the address of the game's frame counter.
FRAME=$(arm-none-eabi-nm ../build/rootflow.elf | awk '$3 == "frame" { print $1 }')
# Scripts may name variables ($px, $spirits+72); resolve them against this build.
python3 resolve.py "$(basename "$1")" ../build/rootflow.elf > .resolved.txt
./harness ../rootflow.gba .resolved.txt "$FRAME"
