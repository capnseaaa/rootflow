#!/bin/sh
# One test for all.sh: its own resolved script; its values with the addresses taken out (out/<name>.val).
cd "$(dirname "$0")"
python3 resolve.py "$1.txt" ../build/rootflow.elf > "out/$1.resolved"
./harness ../rootflow.gba "out/$1.resolved" "$FRAME" 2>/dev/null | grep -v "^ok" | sed -E "s/\[[0-9A-F]+\]//g" > "out/$1.val"
rm -f "out/$1.resolved"
