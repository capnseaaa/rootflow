# Rootflow

A cozy restoration game for the Game Boy Advance, played in emulators (built for the R36 Ultra handheld).

Walk a dead gray valley, befriend land spirits, and use their powers to bring life back.
*Small roots. Big change.*

## Controls
- D-pad: move, B: run
- A: greet a spirit / use the active spirit's power
- SELECT: swap spirit, START: reset

## Build
Needs `arm-none-eabi-gcc` and Python 3 with Pillow.

```sh
sh rootflow/build.sh          # -> rootflow/rootflow.gba
sh rootflow/tools/package.sh  # -> rootflow/release/ (ROM + cover for the SD card)
```

Tests need `libmgba`: `sh rootflow/test/run.sh test/v3.txt` plays a scripted run and saves screenshots.

No external GBA libraries. All art is drawn procedurally by `rootflow/tools/make_art.py`.
