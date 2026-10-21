# Rootflow

A cozy restoration game for the Game Boy Advance, played in emulators (built for the R36 Ultra handheld).

Vael took the water, and the land spirits lost their homes. Walk a dead gray valley, wake the spirits, use their powers to bring life back, and build each spirit a home.
*Small roots. Big change.*

## Spirits
- **Sprout** blooms life. **Ripple** bridges water. **Pebble** breaks rocks. **Breeze** clears thorns.
- Powers cost energy. Rest on green land or at a home to recharge.
- Build a home with help from two other spirits. A home pulses life around it and gives the whole team a bonus.

## World
- The valley, the Windy Steppe to the east, and Stone Hollow to the south. Walk off an open edge to travel.
- Pebble sleeps past a dried-up river in the steppe; Breeze is sealed among the boulders of the hollow.
- Collect stone, fruit, and seeds; find hidden chests and the rusted machines Vael left behind.

## Building
- Paths, flower beds, fruit saplings, lanterns, and benches, on any land you brought back to life.
- Flower beds and fruit trees regrow, so you can farm your own materials.
- Upgrade a spirit's home to make it reach farther and its bonus stronger.

## Friendship
- Tap B while facing a spirit to give its favorite item. Spirits also warm up as you use their powers and build their homes.
- At 2 hearts a spirit asks for help; its quest unlocks a new thing to build. At 3 hearts its power grows.
- At 5 hearts you can bond one spirit as your lifelong companion. Choose carefully: it cannot be undone.

## Controls
- D-pad: move. Hold B: run. Tap B facing a spirit: give a gift
- A: greet a spirit / use the active spirit's power
- R / L: next / previous spirit
- Hold SELECT: build. SELECT + L/R picks an item, SELECT + A places it, SELECT + B removes it
- START: menu (Spirits, Bag, World, Save). Save from SAVE.
- L + R + START: new game (your save stays until you save again)

## Build
Needs `arm-none-eabi-gcc` and Python 3 with Pillow.

```sh
sh rootflow/build.sh          # -> rootflow/rootflow.gba
sh rootflow/tools/package.sh  # -> rootflow/release/ (ROM + cover for the SD card)
```

Tests need `libmgba`: `sh rootflow/test/run.sh test/v4.txt` plays a scripted run and saves screenshots.

No external GBA libraries. All art is drawn procedurally by `rootflow/tools/make_art.py`.
