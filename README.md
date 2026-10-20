# Rootflow

A cozy restoration game for the Game Boy Advance, played in emulators (built for the R36 Ultra handheld).

Vael took the water, and the land spirits lost their homes. Walk a dead gray valley, wake the spirits, use their powers to bring life back, and build each spirit a home.
*Small roots. Big change.*

## Spirits
- **Sprout** blooms life. **Ripple** bridges water. **Pebble** breaks rocks. **Breeze** clears thorns.
- Powers cost energy. Rest on green land or at a home to recharge.
- Build a home with help from two other spirits. A home pulses life around it and gives the whole team a bonus.

## Controls
- D-pad: move, B: run
- A: greet a spirit / use the active spirit's power
- R / L: next / previous spirit (SELECT also works), START: reset

## Build
Needs `arm-none-eabi-gcc` and Python 3 with Pillow.

```sh
sh rootflow/build.sh          # -> rootflow/rootflow.gba
sh rootflow/tools/package.sh  # -> rootflow/release/ (ROM + cover for the SD card)
```

Tests need `libmgba`: `sh rootflow/test/run.sh test/v4.txt` plays a scripted run and saves screenshots.

No external GBA libraries. All art is drawn procedurally by `rootflow/tools/make_art.py`.
