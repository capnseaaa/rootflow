# Rootflow

A cozy restoration game for the Game Boy Advance, played in emulators (built for the R36 Ultra handheld).

Vael took the water, and the land spirits lost their homes. Walk a dead gray valley, wake the spirits, use their powers to bring life back, and build each spirit a home.
*Small roots. Big change.*

## Spirits
- **Sprout** blooms life. **Ripple** bridges water. **Pebble** breaks rocks. **Breeze** clears thorns.
- Powers cost energy. Rest on green land or at a home to recharge.
- Build a home with help from two other spirits. A home pulses life around it and gives the whole team a bonus.
- Grow every home to level 2 and something falls from the sky: **Bark**, a grumpy wood spirit, the last piece of the Great Tree. He cuts trees (for wood) and clears stumps and dead trees, and complains about it the whole time.
- Upgrade every home and let all four spirits rest inside at once: from then on blooms grow bigger and homes pulse nonstop.

## World
- The valley, the Windy Steppe to the east, and Stone Hollow to the south. Walk off an open edge to travel.
- Pebble sleeps past a dried-up river in the steppe; Breeze is sealed among the boulders of the hollow, across the river.
- Once Bark joins, the way west opens to the Great Tree: Bark's old home, dead and clamped by Vael, its branches spread over the whole wood. Bark has four jobs for your spirits there.
- The Great Tree's wood is dangerous: blight blobs and Vael drones roam it. With an enemy in front, A makes your spirit attack (Sprout whips, Ripple shoots water, Pebble throws rocks, Breeze blasts a gust, Bark slams the ground). Each of Bark's jobs wakes a mini boss that one spirit hits hardest. You have five hearts; if you faint, you wake at home with nothing lost.
- Beat all four mini bosses and the opening at the tree's foot leads inside, to the Gloomheart: five shields in turn, each cracked only by one spirit. Beat it and the Great Tree wakes, green and leafy, with Bark's face on its trunk. Bark gives you his power: choose YOU with R/L to cut, clear, and slam, no charges.
- Each area has its own grass. Blooms grow trees back from stumps and grass under chests and relics.
- Collect stone, fruit, seeds, and wood; find hidden chests and the rusted machines Vael left behind.

## Vael's pumps
- Once you build a home, Vael starts pumps on your living land. A pump slowly drains the land around it.
- Find it and press A to shut it down (your spirit helps, 1 charge). You get scrap stone.
- Land inside a home's reach is safe, so homes and upgrades protect what you restored.

## Building
- Paths, flower beds, fruit saplings, lanterns, and benches, on any land you brought back to life.
- Flower beds and fruit trees regrow, so you can farm your own materials.
- Upgrade a spirit's home to make it reach farther and its bonus stronger.

## Friendship
- Tap B while facing a spirit to give its favorite item. Spirits also warm up as you use their powers and build their homes.
- At 2 hearts a spirit asks for help; its quest unlocks a new thing to build. At 3 hearts its power grows.
- At 5 hearts you can bond one spirit as your lifelong companion. Choose carefully: it cannot be undone.

## Sound
- A chiptune for the title and for each area (melody and bass).
- Recorded sound effects for powers, homes, chests, gifts, pumps, and the menu (CC0; see `rootflow/sfx/CREDITS.md`). Blooms and home pulses keep their rising chimes.

## Controls
- D-pad: move. Hold B: run. Tap B facing a spirit: give a gift
- A: greet a spirit / use the active spirit's power
- R / L: next / previous spirit
- Hold SELECT: build (a frame shows where; green = OK). SELECT + L/R picks an item, SELECT + A places it, SELECT + B removes it
- START: menu (Spirits, Bag, World, Save). Save from SAVE.
- L + R + START: new game (your save stays until you save again)
- L + R + B: dev mode, for testing: jump to a point in the story (all spirits, homes built or grown, Bark, the Great Tree) with options (materials at 99, endless charges, no pumps, full hearts)

## Build
Needs `arm-none-eabi-gcc` and Python 3 with Pillow.

```sh
sh rootflow/build.sh          # -> rootflow/rootflow.gba
sh rootflow/tools/package.sh  # -> rootflow/release/ (ROM + cover for the SD card)
```

Tests need `libmgba`: `sh rootflow/test/run.sh test/v4.txt` plays a scripted run and saves screenshots.

No external GBA libraries. All art is drawn procedurally by `rootflow/tools/make_art.py`.
