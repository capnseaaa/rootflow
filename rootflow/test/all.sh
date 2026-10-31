#!/bin/sh
# Run every regression test at once (8 side by side) and compare what each prints with its known-good
# values in test/expect/. Usage:
#   sh test/all.sh            build, run all, report "same" / "DIFF" per test (exit 1 on any DIFF)
#   sh test/all.sh v4 marsh   only these tests
#   sh test/all.sh --update   ... and store what they print now as the new known-good values
# The load_* tests need a save file made by an old build (HARNESS_SAV), so they are run by hand.
cd "$(dirname "$0")"
TESTS="v2 lr v4 perks rewards save builds menu friends stuck v13 tips v14 dev combat v16 gloom gloom_melee cam
swap_text jump_gloom west_exit aura gloom_intro music_story omen leafen chatter west_entry rock_grass marsh
swim_orbs river_narrow shores marsh_views picker thirds lily_attack pouch orchard flick smoke cook orchard2 chirp roots grove shrink home decor meadow glimmer nox night meadow_leafen"
UPDATE=0
if [ "$1" = "--update" ]; then UPDATE=1; shift; fi
[ $# -gt 0 ] && TESTS="$*"
(cd .. && sh build.sh >/dev/null 2>&1) || { echo "build failed"; exit 2; }
MGBA="../../third_party/mgba"
if [ -d "$MGBA/lib" ]; then
    gcc harness.c -o harness -I"$MGBA/include" -L"$MGBA/lib" -Wl,-rpath,"$(cd "$MGBA/lib" && pwd)" -lmgba
else
    gcc harness.c -o harness -lmgba
fi
FRAME=$(arm-none-eabi-nm ../build/rootflow.elf | awk '$3 == "frame" { print $1 }')
mkdir -p out expect
export FRAME
for t in $TESTS; do echo "$t"; done | xargs -P 8 -n 1 sh one.sh
FAIL=0
for t in $TESTS; do
    if [ $UPDATE = 1 ]; then cp out/$t.val expect/$t.val; echo "$t stored ($(wc -l < out/$t.val | tr -d ' ') lines)"; continue; fi
    if [ ! -f expect/$t.val ]; then echo "$t NEW (no known-good values yet)"; FAIL=1; continue; fi
    if cmp -s out/$t.val expect/$t.val; then echo "$t same"; else echo "$t DIFF"; diff expect/$t.val out/$t.val | head -8; FAIL=1; fi
done
exit $FAIL
