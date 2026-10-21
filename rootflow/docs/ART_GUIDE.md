# Rootflow Art Guide

How to draw new art for Rootflow, by hand or with ChatGPT, so it drops straight into the game.

## 1. Which software

**Short answer on Canva:** you can, but it fights you. Canva has no pixel grid, blurs edges when you resize (anti-aliasing), and cannot limit you to 16 colors. You would spend your time fixing blur. Use a pixel-art editor instead. Good free ones:

| App | Where | Why |
|---|---|---|
| **Piskel** (piskelapp.com) | Browser, free | Easiest start. Grid, animation frames side by side, PNG export. |
| **Pixilart** (pixilart.com) | Browser and phone, free | Same idea, works on your phone. |
| **Pixel Studio** | Phone (iOS/Android), free | Good touch controls, layers, animation. |
| **LibreSprite** | Mac/PC, free | A free version of Aseprite. Full palette control. |
| **Aseprite** | Mac/PC, about $20 | The standard tool for game pixel art. Worth it if you keep going. |

Whatever you use, set the **pencil to 1 pixel, hard edge, no anti-aliasing**, and turn on the pixel grid.

## 2. Rules every image must follow

1. **Exact size.** Characters and tiles are 16 x 16 pixels. The player is 16 x 24 (inside a 16 x 32 frame). Draw at that size, not bigger.
2. **16 colors per sheet, at most.** One of them is the see-through background. Use **pure magenta #FF00FF** for anything that should be invisible.
3. **No soft edges.** No blur, gradients, glow, or semi-transparent pixels. Every pixel is one solid color.
4. **A dark outline** around characters, so they read on gray ground, green grass, and blue water.
5. **Save as PNG.** Never JPG or WebP (they smear colors). Export at 1x (16 x 16 per frame). If your app only exports bigger, export at an exact whole multiple (like 10x) with "nearest neighbor" scaling.
6. **Frames side by side in one row,** in the order listed below, with no gaps.

Send me the PNG. I convert it, check it in the game, and fix small issues.

## 3. What each sheet holds

### Spirits (one file each, 32 x 16)

Two frames: **idle 1** and **idle 2** (a gentle bob: frame 2 is usually the same spirit one pixel lower, or a tiny squish). The game makes the gray "asleep" version by itself.

| File | Spirit | Look today | Ideas to keep |
|---|---|---|---|
| `sprout_sprite.png` | Sprout | Round green plant spirit with two leaf sprouts | Plant, cheerful, the first friend |
| `ripple_sprite.png` | Ripple | Light-blue water drop with a shine | Water, calm. This sheet also holds the "zz" sleep bubble as a 3rd frame (48 x 16). |
| `pebble_sprite.png` | Pebble | Beige mossy stone with two dark eyes | Stone, sturdy, moss cap |
| `breeze_sprite.png` | Breeze | White cloud puff with a curling wind tail | Wind, light, playful |

Each spirit sheet has its **own** 16-color palette, so spirits can use completely different colors from each other.

### Map tiles (`terrain.png`, one row of 16 x 16 tiles)

All tiles share **one** 16-color palette, so plan the colors together. Three palette slots have a special job:

- **Colors 1, 2, 3 = dead ground (dark, mid, light).** The game recolors exactly these three slots per area (gray in the valley, straw in the steppe, blue-gray in the hollow). Use them only for dead land, rocks, and stone.
- **Colors 9, 10, 11 = water (dark, light, foam).**
- **Color 15 = outline.**

Current palette (slot 0 to 15):
`#101418, #4A4440, #6B625A, #8A8078, #8B5E3C, #A8774C, #3F7A3A, #6AA84F, #9CCC65, #2F5F9E, #4F8FD6, #A8D4F0, #5A3A22, #D9603B, #F0E2B6, #1C2420`

Tile order (56 tiles, keep this order):
dead, dead_crack, dirt, grass, grass_tuft, flowers, tree, dead_tree, rock, water, water_b (2nd water frame), bridge, thorn, homesite, chest, chest_open, wreck (rusted Vael pump), dry_grass, pebbles, menhir (standing stone), dry_river, dry_bridge, path, flowerbed, flowerbed_empty, sapling, fruit_tree, fruit_tree_bare, lantern, bench, dam, lever, lever_pulled, lost_stone, blossom, fountain, statue, pinwheel, vael_pump, vael_pump_b (its 2nd animation frame), then the homes: for each of grove, spring, cairn, nook: level 1 roof, level 1 base, level 2 roof, level 2 base.

Homes are two tiles tall: the roof sits on the tile above the home site and the base on the site, where the spirit rests. The bases share one look (a mossy pad) with small touches per spirit; the roofs are each spirit's own design. Roofs should sit on grass (the spring's roof sits on water).

You do not have to redo all of them at once. Start with the ground (dead, grass, dirt, water) and trees; they cover most of the screen.

## 4. ChatGPT prompts

These prompts worked for the wizard. Paste one as a single message. Afterwards, download the image with the download button **as PNG** and send it to me.

### Spirit prompt (fill in the [brackets])

```
Create a pixel art sprite sheet for a Game Boy Advance game. Follow every rule exactly.

CHARACTER
[Name], a small nature spirit: [description, e.g. "a round mossy stone creature with a
green moss cap and two dark eyes, sturdy and friendly"]. Cute, chunky, readable at tiny size.
Clean 1-pixel dark outline around the whole character.

PIXEL GRID (most important)
- Each frame is exactly 16 pixels wide and 16 pixels tall.
- Draw every pixel as a solid 10 x 10 square block, so each frame is 160 x 160 on screen.
- Hard pixel edges only. No anti-aliasing, blur, gradients, soft shading, dithering, glow,
  or shadows.
- Use ONLY these colors plus the background: [list up to 15 hex colors, e.g.
  #160E1E outline, #C4B8A4 stone light, #968A7A stone mid, #685E56 stone dark,
  #B8F08A moss light, #6AC46A moss mid, #FAF6EC white]
- Background: flat pure magenta #FF00FF everywhere that is not the character.

LAYOUT
- 2 frames side by side in one row, with 40 pixels of magenta between and around them.
- Frame 1: idle. Frame 2: the same pose, bobbed 1 pixel lower (a gentle breathing bob).
- No text, labels, grid lines, or borders.

CONSISTENCY
- Both frames are the same character, same colors, same size. Only the small bob changes.
```

### Map tile prompt (a few tiles at a time)

```
Create pixel art map tiles for a Game Boy Advance game, top-down view. Follow every rule exactly.

TILES (in this order, left to right):
[e.g. 1. dead gray ground, 2. dead ground with a crack, 3. bare brown dirt, 4. green grass]

PIXEL GRID
- Each tile is exactly 16 x 16 pixels; draw every pixel as a solid 10 x 10 block.
- Tiles must repeat seamlessly when placed next to themselves (no border around each tile).
- Hard pixel edges only. No anti-aliasing, gradients, or soft shading.
- Use ONLY these 16 colors: #101418, #4A4440, #6B625A, #8A8078, #8B5E3C, #A8774C, #3F7A3A,
  #6AA84F, #9CCC65, #2F5F9E, #4F8FD6, #A8D4F0, #5A3A22, #D9603B, #F0E2B6, #1C2420
- Dead ground, rocks, and stone use only #4A4440, #6B625A, #8A8078 (plus the outline).

LAYOUT
- The tiles in one row, 40 pixels of magenta #FF00FF between and around them.
- No text, labels, or grid lines.
```

### If the result is wrong, reply with one of these

- Blurry: `Redo it with hard square pixels only, no anti-aliasing, each pixel a 10x10 block.`
- Wrong colors: `Use only the hex colors I listed, nothing else.`
- Frames differ too much: `Keep both frames identical except the 1-pixel bob.`
- Tiles show seams: `Make each tile repeat seamlessly; remove the borders.`

## 5. What happens after you send it

1. I convert the image to the exact pixels and palette (`tools/import_sheet.py`).
2. I put it in the game and check it on screen in every area.
3. I send you a screenshot. If something looks off, we adjust.

Note: the converter's exact "grid" mode is set up for the player sheet today. When your first spirit or tile image arrives, I will extend it for that layout.
