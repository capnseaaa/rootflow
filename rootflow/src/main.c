// Rootflow spirit toy (GBA).
// Walk the dead valley, find the sprout spirit, and bloom life into the land.
// D-pad: move   B: run   A: greet / use spirit   R / L: next / previous spirit   START: reset
// Hold SELECT to build: L/R pick an item, A places it, B removes one.
// Spirits: Sprout blooms life, Ripple bridges water, Pebble breaks rocks, Breeze clears thorns,
// Bark (falls from the sky once all four homes are level 2) cuts trees and clears stumps, grumbling.
// Powers cost energy. Spirits can be housed at home sites; a home spreads life around it.

#include "data.h"
#include "music.h"
#include "sfx.h"

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef signed int     s32;

// ---------- hardware ----------
#define REG16(a) (*(volatile u16*)(a))
#define REG_DISPCNT   REG16(0x04000000)
#define REG_VCOUNT    REG16(0x04000006)
#define REG_BG0CNT    REG16(0x04000008)
#define REG_BG1CNT    REG16(0x0400000A)
#define REG_BG1HOFS   REG16(0x04000014)
#define REG_BG1VOFS   REG16(0x04000016)
#define REG_BG2CNT    REG16(0x0400000C)
#define REG_BG2HOFS   REG16(0x04000018)
#define REG_BG2VOFS   REG16(0x0400001A)
#define REG_BG3CNT    REG16(0x0400000E)
#define REG_BG3HOFS   REG16(0x0400001C)
#define REG_BG3VOFS   REG16(0x0400001E)
#define REG_WIN0H     REG16(0x04000040)
#define REG_WIN0V     REG16(0x04000044)
#define REG_WININ     REG16(0x04000048)
#define REG_WINOUT    REG16(0x0400004A)
#define REG_WIN1H     REG16(0x04000042)
#define REG_WIN1V     REG16(0x04000046)
#define REG_BLDY      REG16(0x04000054)
#define REG_BLDCNT    REG16(0x04000050)
#define REG_BLDALPHA  REG16(0x04000052)
#define REG_KEYINPUT  REG16(0x04000130)
#define REG_SND1SWEEP REG16(0x04000060)
#define REG_SND1CNT   REG16(0x04000062)
#define REG_SND1FREQ  REG16(0x04000064)
#define REG_SNDCNT_L  REG16(0x04000080)
#define REG_SNDCNT_H  REG16(0x04000082)
#define REG_SNDCNT_X  REG16(0x04000084)

#define PAL_BG   ((volatile u16*)0x05000000)
#define PAL_OBJ  ((volatile u16*)0x05000200)
#define CHARBLOCK(n)  ((volatile u32*)(0x06000000 + (n) * 0x4000))
#define SCREENBLOCK(n) ((volatile u16*)(0x06000000 + (n) * 0x800))
#define OBJ_TILES ((volatile u32*)0x06010000)
#define OAM       ((volatile u16*)0x07000000)

#define KEY_A      0x0001
#define KEY_B      0x0002
#define KEY_SELECT 0x0004
#define KEY_START  0x0008
#define KEY_RIGHT  0x0010
#define KEY_LEFT   0x0020
#define KEY_UP     0x0040
#define KEY_DOWN   0x0080
#define KEY_R      0x0100
#define KEY_L      0x0200

#define SCREEN_W 240
#define SCREEN_H 160
#define WORLD_PX (MAP_W * 16)

// VRAM (v0.14): charblocks 0-1 = map metatiles, then the Great Tree's tiles (from TREE_TILE_BASE;
// trunk and branches share them); screenblocks 16-19 = the trunk map (BG3, under the sprites),
// 20-23 = the branch map (BG2, over the sprites), 24-25 = font and title banner (BG0's charblock 3),
// 26 = text map, 28-31 = the 64x64 map (BG1).
#define TEXT_SB   26
#define MAP_SB    28
#define TRUNK_SB  16
#define CANOPY_SB 20
#define DISP_TREE ((1 << 10) | (1 << 11) | (1 << 13))   // BG2, BG3, and window 0 (the clear box)

enum { DEAD, DEAD_CRACK, DIRT, GRASS, GRASS_TUFT, FLOWERS, TREE, DEAD_TREE, ROCK, WATER, WATER_B, BRIDGE,
       THORN, HOMESITE, CHEST, CHEST_OPEN, WRECK, DRY_GRASS, PEBBLES, MENHIR, DRY_RIVER, DRY_BRIDGE,
       PATH, FLOWERBED, FLOWERBED_EMPTY, SAPLING, FRUIT_TREE, FRUIT_TREE_BARE, LANTERN, BENCH,
       DAM, LEVER, LEVER_PULLED, LOST_STONE, BLOSSOM, FOUNTAIN, STATUE, PINWHEEL, VAEL_PUMP, VAEL_PUMP_B,
       HOME_FIRST };   // then 16 home tiles: per spirit, level 1 roof + base, level 2 roof + base
#define NUM_HOME_TILES 16
// After the homes: animation frames, never placed on the map (animate_homes copies them over a home).
enum { SPRING1_RING1 = HOME_FIRST + NUM_HOME_TILES, SPRING1_RING2, SPRING2_RING1, SPRING2_RING2, SPRING2_SPRAY,
       CAIRN1_GLOW, CAIRN2_GLOW, NOOK1_SWING, NOOK2_SPIN,
       // v0.13: stumps (on dead land; on grass, where Bark cut a tree) and chests / relics on grass
       STUMP, STUMP_GRASS, CHEST_GRASS, CHEST_OPEN_GRASS, WRECK_GRASS,
       // v0.14: solid ground under the Great Tree's trunk (the trunk is its own layer), its roots,
       // and Bark's quest pieces
       TRUNK_BLOCK, ROOT_TIP_DEAD, ROOT_TIP_ALIVE, CLAMP, DRY_POOL,   // CLAMP: solid; a sprite shows it
       TRUNK_GRASS,     // v0.16.3: the healed trunk's cells, drawn as grass so its foot stands on green
       MUSH_LAMP, LEAF_SWING, ACORN_BENCH, FLOWER_ARCH, PEBBLE_PATH, DEW_POND,   // v0.17: the Leafen's gifts
       ROCK_GRASS,      // v0.17.3: a rock with grass round it; acts as a rock in every way
       SLUDGE, LILY_PAD, REEDS, VAEL_PIPE,     // v0.18 the drowned marsh
       // v0.18.1 the marsh's own (MARSH_META_FIRST on): in video memory only while you are in the marsh
       BOG_WATER = MARSH_META_FIRST, BOG_WATER_B, BOG_PAD, MOSS, MUSHROOMS, SWAMP_TREE, SWAMP_TREE_ALIVE, LOG,
       SWAMP_TOP, SWAMP_TOP_ALIVE,   // a swamp tree's crown, the tile above its foot
       BOG_EDGE,        // + 0..14: bog water's ragged shores (never in a map; draw_meta picks them by the neighbors)
       WATER_EDGE = BOG_EDGE + 15,     // + 0..14: the same for clear water, outside the Great Tree's region
       // v0.19 the burnt orchard's own (loaded like the marsh's); DARKNESS is never on a map: BG2 draws the dark
       BURNT_TREE = WATER_EDGE + 15, BURNT_FRUIT_TREE, BURNT_LOG, LANTERN_COLD, LANTERN_LIT, CAMPFIRE_COLD,
       CAMPFIRE_LIT, LOG_SEAT, SMOKE_STACK, BURNT_FENCE, DARKNESS,
       ASH_LANE, CHARRED_BRUSH };   // dark ash lanes (walkable, they stay lanes); burnt brush (walkable, it blooms)
static int is_pad(u8 t) { return t == LILY_PAD || t == BOG_PAD; }
static int is_bog(u8 t) { return t == BOG_WATER || t == BOG_WATER_B; }
static int is_rock(u8 t) { return t == ROCK || t == ROCK_GRASS; }
static int is_trunk(u8 t) { return t == TRUNK_BLOCK || t == TRUNK_GRASS; }
static int is_tree_part(u8 t) { return (t >= TRUNK_BLOCK && t <= DRY_POOL) || t == TRUNK_GRASS; }
#define WEST 3        // the Great Tree's area
#define HOLLOW_IN 4   // the hollow inside the Great Tree (v0.16)
#define MARSH 5       // the drowned marsh, south-west (v0.18)
#define ORCHARD 6     // the burnt orchard, north-west (v0.19)
#define ORCHARD_WALL_ROW 27   // the fallen burnt trunk across its way in (only YOU clears it)
static int is_orchard_piece(u8 t) { return t >= BURNT_TREE && t <= BURNT_FENCE; }   // all solid
// none restorable but the burnt trees, which grow back (v0.19.4)
static int is_burnt_tree(u8 t) { return t == BURNT_TREE || t == BURNT_FRUIT_TREE; }
#define MARSH_BANK_ROW 19   // v0.18.1: its way to Stone Hollow, along the river's north bank
static int is_home(u8 t) { return t >= HOME_FIRST && t < HOME_FIRST + NUM_HOME_TILES; }
// The home tile for spirit k at a level (1 or 2): the roof sits on the tile above the site.
static u8 home_tile(s32 k, s32 level, int roof) { return HOME_FIRST + k * 4 + (level - 1) * 2 + (roof ? 0 : 1); }
enum { DIR_DOWN, DIR_UP, DIR_LEFT, DIR_RIGHT };

// ---------- small helpers ----------
static void copy32(volatile u32 *dst, const u32 *src, u32 words) {
    while (words--) *dst++ = *src++;
}

static u32 rng_state = 1;
static u32 rng(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state >> 16;
}

static u16 keys_now, keys_prev;
static void poll_keys(void) {
    keys_prev = keys_now;
    keys_now = ~REG_KEYINPUT & 0x03FF;
}
#define HELD(k) (keys_now & (k))
#define HIT(k)  ((keys_now & ~keys_prev) & (k))

static void vsync(void) {
    while (REG_VCOUNT >= 160);
    while (REG_VCOUNT < 160);
}

// ---------- sound: short chimes on channel 1 ----------
#define REG_SND2CNT   REG16(0x04000068)
#define REG_SND2FREQ  REG16(0x0400006C)
#define REG_SND3SEL   REG16(0x04000070)
#define REG_SND3CNT   REG16(0x04000072)
#define REG_SND3FREQ  REG16(0x04000074)
#define WAVE_RAM      ((volatile u16*)0x04000090)

// ---------- sound effects: recorded samples on DirectSound A (timer 0 + DMA 1), from sfx.h ----------
#define REG32(a) (*(volatile u32*)(a))
#define REG_TM0CNT_L  REG16(0x04000100)
#define REG_TM0CNT_H  REG16(0x04000102)
#define REG_DMA1SAD   REG32(0x040000BC)
#define REG_DMA1DAD   REG32(0x040000C0)
#define REG_DMA1CNT_H REG16(0x040000C6)
#define SNDH_BASE     (0x0002 | (1 << 2) | (1 << 8) | (1 << 9))
static u32 sfx_frames;   // frames until the playing sample ends
static void sfx_stop(void) {
    REG_DMA1CNT_H = 0;
    REG_SNDCNT_H = SNDH_BASE | 0x0800;   // empty the FIFO so it goes quiet at once
}
static void sfx_play(u32 id) {
    sfx_stop();
    REG_DMA1SAD = (u32)sfx_data[id];
    REG_DMA1DAD = 0x040000A0;            // FIFO A
    REG_DMA1CNT_H = 0xB640;              // on, refill when the FIFO asks, repeat, 32-bit, fixed destination
    sfx_frames = sfx_len[id] * 60 / SFX_RATE + 2;
}
static void sfx_tick(void) {
    if (sfx_frames && --sfx_frames == 0) sfx_stop();
}

static void sound_init(void) {
    REG_SNDCNT_X = 0x0080;   // master on
    REG_SNDCNT_L = 0x7777;   // channels 1-3 on both sides, full volume
    REG_SNDCNT_H = SNDH_BASE | 0x0800;   // PSG at 100%, DMA sound A at 100% on both sides, reset its FIFO
    REG_TM0CNT_L = 65536 - 16777216 / SFX_RATE;   // timer 0 ticks once per sample
    REG_TM0CNT_H = 0x0080;
    REG_SND1SWEEP = 0x0008;  // sweep off
    // Channel 3 plays the bass from a 32-sample wave. Write it into the bank that is not playing.
    REG_SND3SEL = 0x0040;
    for (u32 i = 0; i < 8; i++) WAVE_RAM[i] = bass_wave[2 * i] | (bass_wave[2 * i + 1] << 8);
    REG_SND3SEL = 0x0000;
}
static void chime(u32 hz, u32 vol) {
    REG_SND1CNT = (vol << 12) | (2 << 8) | (2 << 6);      // decaying envelope, 50% duty
    REG_SND1FREQ = (u16)((2048 - 131072 / hz) | 0x8000);  // restart
}
// v0.19.5 a weak little chirp (Flick, cold): the thinnest tone (12.5%), quiet, fading fast
static void weak_chirp(u32 hz, u32 vol) {
    REG_SND1CNT = (vol << 12) | (1 << 8) | (0 << 6);
    REG_SND1FREQ = (u16)((2048 - 131072 / hz) | 0x8000);
}
static const u16 scale[] = { 523, 587, 659, 784, 880, 1047, 1175, 1319 };

// ---------- music: melody on channel 2, bass on channel 3, harmony on channel 1 ----------
// The chimes (sound effects) share channel 1, so a chime cuts into a song's harmony for a moment.
// Each song carries its own tone (song_mel_env, song_har_env, song_bass_cnt) and loops or plays
// once (song_loop); a song that played once leaves song at -1 (silence).
static s32 song = -1;
static u32 voice_pos[3], voice_left[3], voice_done[3];   // note index, frames left, finished (play once)
static void music_note(s32 v, u16 freq) {
    if (v == 0) {
        if (!freq) { REG_SND2CNT = 0; REG_SND2FREQ = 0x8000; return; }      // rest: silence
        REG_SND2CNT = song_mel_env[song];
        REG_SND2FREQ = freq | 0x8000;
    } else if (v == 1) {
        if (!freq) { REG_SND3SEL = 0; return; }
        REG_SND3SEL = 0x0080;                                             // on, 32-sample bank 0
        REG_SND3CNT = song_bass_cnt[song];
        REG_SND3FREQ = freq | 0x8000;
    } else {
        if (!freq) { REG_SND1CNT = 0; REG_SND1FREQ = 0x8000; return; }
        REG_SND1CNT = song_har_env[song];
        REG_SND1FREQ = freq | 0x8000;
    }
}
static void music_silence(void) {
    REG_SND2CNT = 0; REG_SND2FREQ = 0x8000;
    REG_SND3SEL = 0;
    REG_SND1CNT = 0; REG_SND1FREQ = 0x8000;
}
static void music_play(s32 s) {                        // -1: no music
    if (s == song) return;
    if (song >= 0 && song_count[song][2]) { REG_SND1CNT = 0; REG_SND1FREQ = 0x8000; }   // the old harmony stops
    song = s;
    for (s32 v = 0; v < 3; v++) { voice_pos[v] = 0; voice_left[v] = 0; voice_done[v] = 0; }
    if (s < 0) music_silence();
}
static int music_once_playing(void) { return song >= 0 && !song_loop[song]; }
static void music_tick(void) {
    sfx_tick();
    if (song < 0) return;
    int busy = 0;
    for (s32 v = 0; v < 3; v++) {
        if (!song_count[song][v] || voice_done[v]) continue;
        busy = 1;
        if (voice_left[v]) { voice_left[v]--; continue; }
        u32 i = voice_pos[v];
        if (i >= song_count[song][v]) {                 // the end: loop, or this voice is done
            if (!song_loop[song]) { voice_done[v] = 1; continue; }
            i = 0;
        }
        music_note(v, song_freq[song][v][i]);
        voice_left[v] = song_len[song][v][i] * song_fps[song] - 1;
        voice_pos[v] = i + 1;
    }
    if (!song_loop[song] && !busy) music_play(-1);     // a jingle ended: silence
}

// ---------- world ----------
// Three areas, each a 32x32 map kept in memory so restored land stays restored.
// cur_area is where the player is (and what the screen shows). work_area is the map the tile
// functions act on; it differs from cur_area only while a home in another area pulses.
static u8 maps[NUM_AREAS][MAP_W * MAP_H];
static u8 *world = maps[0];
static s32 cur_area, work_area;
static u32 restorable[NUM_AREAS];   // tiles that can become living, per area

static void use_area(s32 a) { work_area = a; world = maps[a]; }

static u8 tile_at(s32 mx, s32 my) {
    if (mx < 0 || my < 0 || mx >= MAP_W || my >= MAP_H) return ROCK;
    return world[my * MAP_W + mx];
}

// v0.17: the nine home animation frames never sit on the map (they are copied from ROM), so they get no
// room in video memory; metatiles after them move down nine places there. This left room for the
// Leafen's gifts next to the Great Tree's art.
static u32 meta_vram(u32 m) {
    if (m >= MARSH_META_FIRST) return TREE_TILE_BASE / 4 + (m - MARSH_META_FIRST);   // v0.18.1: the marsh's own
    return m > NOOK2_SPIN ? m - (NOOK2_SPIN + 1 - SPRING1_RING1) : m;
}
enum { TS_NONE, TS_DEAD, TS_HOLLOW, TS_ALIVE, TS_EXTRA };   // TS_EXTRA: the marsh's metatiles and the shores (v0.18.1)
static s32 tileset_now = TS_NONE;
#define SLUDGE_BANK 5   // v0.18: sludge draws with its own colors (BG bank 5): murky purple-gray, an oily sheen
#define BOG_BANK 6      // v0.18.1: bog water and its lily pads (BG bank 6): dark green-black water
static void draw_meta(s32 mx, s32 my) {
    if (work_area != cur_area) return;   // off-screen area: memory only
    u8 m = world[my * MAP_W + mx];
    u32 look = m == SLUDGE && cur_area == MARSH ? BOG_WATER : m;   // sludge: bog water's ripples, in sludge colors
    int shores = tileset_now == TS_EXTRA && (m == BOG_WATER || m == WATER || (m == SLUDGE && cur_area == MARSH));
    if (shores) {                                 // v0.18.1: a ragged shore on each side with land
        u32 mask = 0;
        static const signed char ex[4] = { 0, 1, 0, -1 }, ey[4] = { -1, 0, 1, 0 };
        for (s32 k = 0; k < 4; k++) {
            s32 nx = mx + ex[k], ny = my + ey[k];
            if (nx < 0 || ny < 0 || nx >= MAP_W || ny >= MAP_H) continue;
            u8 n = world[ny * MAP_W + nx];
            if (!(is_bog(n) || n == SLUDGE || n == WATER || is_pad(n) || n == VAEL_PIPE || n == BRIDGE || n == DRY_BRIDGE ||
                  (n >= HOME_FIRST + 4 && n < HOME_FIRST + 8))) mask |= 1 << k;   // bridges and Ripple's spring sit on water
        }
        if (mask) look = (m == WATER ? WATER_EDGE : BOG_EDGE) + mask - 1;
    }
    u16 base = meta_vram(look) * 4 | ((m == SLUDGE || m == VAEL_PIPE || m == SMOKE_STACK) ? SLUDGE_BANK << 12 : (is_bog(m) || m == BOG_PAD) ? BOG_BANK << 12 : 0);
    for (s32 k = 0; k < 4; k++) {
        s32 tx = mx * 2 + (k & 1);
        s32 ty = my * 2 + (k >> 1);
        u32 sb = MAP_SB + (tx >= 32) + (ty >= 32) * 2;
        SCREENBLOCK(sb)[(ty & 31) * 32 + (tx & 31)] = base + k;
    }
}

static void set_tile(s32 mx, s32 my, u8 t) {
    world[my * MAP_W + mx] = t;
    draw_meta(mx, my);
    if (tileset_now == TS_EXTRA && work_area == cur_area) {   // watery neighbors may need a new shore
        static const signed char nx[4] = { -1, 1, 0, 0 }, ny[4] = { 0, 0, -1, 1 };
        for (s32 k = 0; k < 4; k++) {
            s32 x = mx + nx[k], y = my + ny[k];
            if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) continue;
            u8 n = world[y * MAP_W + x];
            if (n == WATER || n == BOG_WATER || n == SLUDGE) draw_meta(x, y);
        }
    }
}

static int is_solid(u8 t) {
    return t == WATER || t == TREE || t == DEAD_TREE || is_rock(t) || t == THORN ||
           t == CHEST || t == CHEST_OPEN || t == WRECK || t == MENHIR || t == DRY_RIVER ||
           t == SAPLING || t == FRUIT_TREE || t == FRUIT_TREE_BARE || t == LANTERN || t == BENCH ||
           t == DAM || t == LEVER || t == LEVER_PULLED || t == LOST_STONE || t == BLOSSOM || t == FOUNTAIN ||
           t == STATUE || t == PINWHEEL || t == VAEL_PUMP || is_home(t) ||
           t == STUMP || t == STUMP_GRASS || t == CHEST_GRASS || t == CHEST_OPEN_GRASS || t == WRECK_GRASS ||
           is_trunk(t) || t == CLAMP || t == DRY_POOL ||   // roots can be walked on
           t == MUSH_LAMP || t == LEAF_SWING || t == ACORN_BENCH || t == FLOWER_ARCH || t == DEW_POND ||
           t == SLUDGE || t == REEDS || t == VAEL_PIPE ||   // lily pads can be walked on
           is_bog(t) || t == SWAMP_TREE || t == SWAMP_TREE_ALIVE || t == LOG || t == SWAMP_TOP || t == SWAMP_TOP_ALIVE ||
           is_orchard_piece(t);
}
// Built things stand on living land, so they count as living (LIFE %, resting, waking).
static int is_built(u8 t) {
    return t == PATH || t == FLOWERBED || t == FLOWERBED_EMPTY || t == SAPLING || t == FRUIT_TREE ||
           t == FRUIT_TREE_BARE || t == LANTERN || t == BENCH || t == BLOSSOM || t == FOUNTAIN ||
           t == STATUE || t == PINWHEEL || is_home(t) || (t >= MUSH_LAMP && t <= DEW_POND);
}
static int is_alive(u8 t) {
    return t == GRASS || t == GRASS_TUFT || t == FLOWERS || t == TREE || t == STUMP_GRASS || is_built(t) ||
           t == SWAMP_TREE_ALIVE || t == SWAMP_TOP_ALIVE;
}

static int is_restorable(u8 t) {
    return !(t == WATER || is_rock(t) || t == THORN || t == HOMESITE || t == CHEST || t == CHEST_OPEN ||
             t == WRECK || t == MENHIR || t == DRY_RIVER || t == DRY_BRIDGE || t == DAM || t == LEVER ||
             t == LEVER_PULLED || t == LOST_STONE || is_tree_part(t) ||
             t == SLUDGE || t == LILY_PAD || t == REEDS || t == VAEL_PIPE ||   // cleared reeds and pipes add ground
             is_bog(t) || t == BOG_PAD || t == LOG || (is_orchard_piece(t) && !is_burnt_tree(t)) || t == ASH_LANE);
}

static void draw_area(void) {
    for (s32 y = 0; y < MAP_H; y++)
        for (s32 x = 0; x < MAP_W; x++)
            draw_meta(x, y);
}

static void load_world(void) {
    for (s32 a = 0; a < NUM_AREAS; a++) {
        restorable[a] = 0;
        for (s32 i = 0; i < MAP_W * MAP_H; i++) {
            u8 t = start_maps[a * MAP_W * MAP_H + i];
            maps[a][i] = t;
            if (is_restorable(t)) restorable[a]++;
        }
    }
    cur_area = 0;
    use_area(0);
    draw_area();
}

// LIFE % of one area (the HUD shows the area you are in; the pause panel shows all three).
static u32 life_percent_of(s32 a) {
    u32 alive = 0;
    for (s32 i = 0; i < MAP_W * MAP_H; i++)
        if (is_alive(maps[a][i])) alive++;
    if (!restorable[a]) return 0;   // the hollow inside the tree has nothing to restore
    u32 p = alive * 100 / restorable[a];
    return p > 100 ? 100 : p;   // homes count as living but were not restorable ground
}
static u32 life_percent(void) { return life_percent_of(cur_area); }

#define RGB5(r, g, b) (((r) >> 3) | (((g) >> 3) << 5) | (((b) >> 3) << 10))
// Each area tints its dead ground (BG colors 1-3): gray valley, straw steppe, blue-gray hollow.
static const u16 ground_pal[NUM_AREAS][3] = {
    { 0, 0, 0 },   // valley: the colors from terrain.png (filled in at startup)
    { (96 >> 3) | ((86 >> 3) << 5) | ((56 >> 3) << 10), (140 >> 3) | ((126 >> 3) << 5) | ((82 >> 3) << 10),
      (178 >> 3) | ((164 >> 3) << 5) | ((110 >> 3) << 10) },
    { (62 >> 3) | ((66 >> 3) << 5) | ((80 >> 3) << 10), (94 >> 3) | ((100 >> 3) << 5) | ((116 >> 3) << 10),
      (132 >> 3) | ((138 >> 3) << 5) | ((154 >> 3) << 10) },
    { (54 >> 3) | ((44 >> 3) << 5) | ((40 >> 3) << 10), (82 >> 3) | ((68 >> 3) << 5) | ((60 >> 3) << 10),
      (110 >> 3) | ((94 >> 3) << 5) | ((82 >> 3) << 10) },   // the Great Tree: dark, ashy brown
    { 0, 0, 0 },   // the hollow: its own layer covers the map
    { RGB5(34, 40, 28), RGB5(52, 60, 40), RGB5(78, 86, 56) },     // the marsh: dark, mossy mud (v0.18.1)
    { RGB5(46, 44, 48), RGB5(74, 70, 72), RGB5(108, 102, 100) },  // the orchard: grey ash (v0.19)
};
// And its grass (BG colors 6-8, also trees and moss): the valley's own green, a warm light green
// in the steppe, a deep blue-green in the hollow (Caleb, v0.13).
static const u16 grass_pal[NUM_AREAS][3] = {
    { 0, 0, 0 },   // valley: terrain.png
    { RGB5(96, 140, 54), RGB5(150, 190, 82), RGB5(206, 224, 124) },
    { RGB5(28, 82, 68), RGB5(46, 118, 94), RGB5(84, 158, 122) },
    { RGB5(40, 112, 44), RGB5(70, 160, 64), RGB5(128, 200, 92) },   // the Great Tree: deep, lush
    { 0, 0, 0 },
    { RGB5(40, 116, 78), RGB5(72, 166, 98), RGB5(150, 214, 118) },  // the marsh: a fresh bog green (v0.18.2)
    { RGB5(70, 130, 50), RGB5(112, 176, 70), RGB5(172, 214, 110) },  // the orchard: a warm orchard green (v0.19)
};
// The Great Tree's tile sets share the slots after the map's tiles (TREE_TILE_BASE) and the layer
// maps in screenblocks 16-23; each area that needs one loads it (v0.16): the dead tree, the
// hollow inside it, or the living tree once it is healed.
static u32 tree_alive;                     // the Great Tree is healed (saved)
static void load_tileset(s32 ts) {
    if (ts == tileset_now) return;
    tileset_now = ts;
    if (ts == TS_EXTRA) {                         // the marsh's metatiles and the shores go where the tree's would be
        copy32(CHARBLOCK(0) + TREE_TILE_BASE * 8, terrain_tiles + MARSH_META_FIRST * 32,
               sizeof(terrain_tiles) / 4 - MARSH_META_FIRST * 32);
        return;
    }
    const u32 *tiles = ts == TS_DEAD ? tiles_dead : ts == TS_HOLLOW ? tiles_hollow : tiles_alive;
    u32 words = ts == TS_DEAD ? sizeof(tiles_dead) / 4 : ts == TS_HOLLOW ? sizeof(tiles_hollow) / 4 : sizeof(tiles_alive) / 4;
    const u16 *trunk = ts == TS_DEAD ? map_dead_trunk : ts == TS_HOLLOW ? map_hollow_trunk : map_alive_trunk;
    const u16 *top = ts == TS_DEAD ? map_dead_top : ts == TS_ALIVE ? map_alive_top : 0;
    const u16 *pal = ts == TS_DEAD ? pal_dead : ts == TS_HOLLOW ? pal_hollow : pal_alive;
    copy32(CHARBLOCK(0) + TREE_TILE_BASE * 8, tiles, words);
    for (u32 i = 0; i < 4096; i++) SCREENBLOCK(TRUNK_SB)[i] = trunk[i];        // 16-bit data
    for (u32 i = 0; i < 4096; i++) SCREENBLOCK(CANOPY_SB)[i] = top ? top[i] : 0;
    for (u32 i = 0; i < 16; i++) PAL_BG[TREE_BANK * 16 + i] = pal[i];
}
static void load_region_sprites(s32 a);   // v0.18.3, after the palette banks
static void dark_enter(s32 a);   // v0.19 the orchard's dark (below)
static void pool_clear(void);    // v0.19 the shared sprite slots (below)
static void smoke_fade(void);    // v0.19.4 (below)
static void set_ground_palette(s32 a) {
    load_region_sprites(a);   // and, once the tree is healed, Flick's colors
    pool_clear();                                      // a new region: its own things take the shared slots
    for (s32 i = 0; i < 3; i++) PAL_BG[1 + i] = a == 0 || a == HOLLOW_IN ? bg_pal[1 + i] : ground_pal[a][i];
    for (s32 i = 0; i < 3; i++) PAL_BG[6 + i] = a == 0 || a == HOLLOW_IN ? bg_pal[6 + i] : grass_pal[a][i];
    // the tree's layers show only in the Great Tree's area and its hollow
    REG_DISPCNT &= ~DISP_TREE;
    if (a == WEST) { load_tileset(tree_alive ? TS_ALIVE : TS_DEAD); REG_DISPCNT |= DISP_TREE; }
    if (a == HOLLOW_IN) { load_tileset(TS_HOLLOW); REG_DISPCNT |= 1 << 11; }   // its room only
    if (a != WEST && a != HOLLOW_IN) load_tileset(TS_EXTRA);   // every area without the Great Tree's own tiles
    for (s32 i = 0; i < 3; i++)                   // v0.18.1: in the marsh the river's water turns a murky teal
        PAL_BG[9 + i] = a == MARSH ? (i == 0 ? RGB5(36, 86, 90) : i == 1 ? RGB5(64, 128, 120) : RGB5(140, 190, 176))
                                   : bg_pal[9 + i];
    static const u16 mud[2][3] = { { RGB5(34, 40, 28), RGB5(52, 60, 40), RGB5(78, 86, 56) },     // the marsh's
                                   { RGB5(46, 44, 48), RGB5(74, 70, 72), RGB5(108, 102, 100) } };  // the orchard's
    for (s32 i = 0; i < 3; i++) PAL_BG[SLUDGE_BANK * 16 + 9 + i] = mud[a == ORCHARD][i];   // under pipes and stacks
    dark_enter(a);
}

// ---------- text (BG0) ----------
// Text palette banks: 1 = HUD and messages (white on slate), 2 = menu windows (dark on cream).
static u16 glyph_bank(char c, u32 bank) {
    for (u32 i = 0; FONT_CHARS[i]; i++)
        if (FONT_CHARS[i] == c) return (1 + i) | (bank << 12);
    return 1 | (bank << 12);
}
static u16 glyph_entry(char c) { return glyph_bank(c, 1); }
static void put_text(s32 col, s32 row, const char *s) {
    for (; *s && col < 30; s++, col++) SCREENBLOCK(TEXT_SB)[row * 32 + col] = glyph_entry(*s);
}
static void put_text_win(s32 col, s32 row, const char *s) {
    for (; *s && col < 30; s++, col++) SCREENBLOCK(TEXT_SB)[row * 32 + col] = glyph_bank(*s, 2);
}
static void clear_row(s32 row) {
    for (s32 c = 0; c < 32; c++) SCREENBLOCK(TEXT_SB)[row * 32 + c] = 0;
}

static u32 msg_timer;
static u32 msg_is_prompt;   // the message on screen is a repeating prompt (see prompt())
// v0.16.6 (Caleb): the box spans the whole screen and the line sits in its middle.
static void show_message(const char *s) {
    msg_is_prompt = 0;
    s32 n = 0; while (s[n]) n++;
    for (s32 c = 0; c < 30; c++) SCREENBLOCK(TEXT_SB)[19 * 32 + c] = glyph_entry(' ');
    put_text((30 - n) / 2, 19, s);
    msg_timer = 150;
}

// Messages that wait their turn (lore, endings). Shown when the current message ends.
#define MSGQ 8   // quest offers, rewards, and the ending can stack up
static const char *msg_queue[MSGQ];
static u32 msgq_len;
static void queue_message(const char *s) {
    if (msg_timer == 0 && msgq_len == 0) { show_message(s); return; }
    if (msgq_len < MSGQ) msg_queue[msgq_len++] = s;
}
// A repeating prompt ("A: ..."). It never pushes queued messages aside, and it stays quiet
// while a scene plays (Bark's arrival), so the scene's lines get the message row.
static u32 scene_on, prompt_quiet;   // prompt_quiet: frames left with no prompts (a tip is showing)
static void prompt(const char *s) {
    if (msgq_len == 0 && msg_timer < 100 && !scene_on && !prompt_quiet) { show_message(s); msg_is_prompt = 1; }
}
static void update_message(void) {
    if (msg_timer && --msg_timer == 0) {
        clear_row(19);
        if (msgq_len) {
            show_message(msg_queue[0]);
            for (u32 i = 1; i < msgq_len; i++) msg_queue[i - 1] = msg_queue[i];
            msgq_len--;
        }
    }
}

static void draw_life(void) {
    char buf[12] = "LIFE    % ";
    u32 p = life_percent();
    buf[5] = p >= 100 ? '1' : ' ';
    buf[6] = p >= 10 ? '0' + (p / 10) % 10 : ' ';
    buf[7] = '0' + p % 10;
    put_text(0, 0, " ");
    put_text(1, 0, buf);
}

// ---------- sprites ----------
static u16 oam_shadow[128 * 4];
// tile8 is in 8x8 tiles. tall = 16x32 (vertical shape, size 2), else 16x16 (square, size 1).
static void obj_set(s32 id, s32 x, s32 y, u32 tile8, u32 hflip, u32 pal, int tall) {
    if (id >= 128) return;                              // OAM_NONE
    u16 *o = &oam_shadow[id * 4];
    if (x < -16 || x > SCREEN_W || y < (tall ? -32 : -16) || y > SCREEN_H) { o[0] = 0x0200; return; }
    o[0] = (y & 0xFF) | (tall ? 2 << 14 : 0);
    o[1] = (x & 0x1FF) | ((tall ? 2 : 1) << 14) | (hflip ? 1 << 12 : 0);
    o[2] = tile8 | (1 << 10) | (pal << 12);                   // priority 1
}
// 16x16 sprite; tile is in 16x16 cells (4 tiles each).
static void sprite_set(s32 id, s32 x, s32 y, u32 tile, u32 hflip, u32 pal) {
    obj_set(id, x, y, tile * 4, hflip, pal, 0);
}
static void sprite_hide(s32 id) { if (id < 128) oam_shadow[id * 4] = 0x0200; }
static void oam_flush(void) {
    for (u32 i = 0; i < 128 * 4; i++) OAM[i] = oam_shadow[i];
}

// Sprite sheet cells (16x16 cells = 4 tiles); offsets come from build_data.py.
// The player is separate: 16x32 frames of 8 tiles from TILE_PLAYER.
#define SPR_SPROUT (TILE_SPROUT / 4)
#define SPR_SPARKLE (TILE_FX / 4)
#define SPR_RIPPLE (TILE_RIPPLE / 4)
#define SPR_ZZZ (SPR_RIPPLE + 2)
#define SPR_STONE (TILE_PEBBLE / 4)
#define SPR_BREEZE (TILE_BREEZE / 4)
#define SPR_BARK (TILE_BARK / 4)
#define SPR_LILY (TILE_LILY / 4)
#define SPR_BALL (TILE_BALL / 4)   // v0.18.4: each spirit as a ball, in the pouch (its own palette)
#define SPR_FLICK (TILE_FLICK / 4)   // v0.19: awake x2, cold, its ember shot, cold and shivering
#define PAL_MAIN 0
#define PAL_RIPPLE 1
#define PAL_SLEEP 2
#define PAL_STONE 3
#define PAL_STONE_SLEEP 4
#define PAL_BREEZE 5
#define PAL_BREEZE_SLEEP 6
#define PAL_SPROUT 7
#define PAL_FX 8
#define PAL_BARK 9
#define PAL_LILY 10       // v0.18: Bark never sleeps, so his gray copy's bank went to Lily
// v0.19 Flick: Breeze's sleeping colors' bank, which nothing needs once every spirit is awake (they are,
// before the orchard can be reached). Loaded in set_ground_palette once the tree is healed. The other two
// sleeping banks (PAL_SLEEP, PAL_STONE_SLEEP) are free the same way, for the corner spirits still to come.
#define PAL_FLICK PAL_BREEZE_SLEEP
#define PAL_CLAMP 11
#define PAL_COMBAT 12
#define PAL_BOSS 13
#define PAL_FLASH 14   // all white: something just got hit

// OAM slots (v0.19: reorganized, Caleb: room for what comes next). A lower slot draws on top.
// Fixed slots for what can show anywhere; then one shared block for what belongs to one region only:
// those systems draw (and hide their slots) only in their own region, and the block is cleared on every
// change of region (set_ground_palette), so they never fight over a slot.
#define OAM_PLAYER  0
#define OAM_SPIRIT  1   // 1..9: one per spirit, by kind (room for nine)
#define OAM_ZZZ     10  // 10..14: sleep bubbles for the four that sleep, and Flick's spark while cold (zzz_slot)
#define OAM_SPARKLE 15
#define OAM_GLOW    16
#define OAM_CURSOR  17
#define OAM_ATTACK  18  // 18..21: the active spirit's attack (Bark's slam uses all four)
#define OAM_BOSS    22
#define OAM_HIT     23
#define OAM_SHIELD  24  // Gloomheart's shield ring
#define OAM_LEAF    25  // 25..30: leaves swirling round the wizard as Bark gives his power
#define OAM_SPORE   31  // 31..38: Gloomheart's spores (the picker and the pouch page reuse them)
#define OAM_MOTE    39  // 39..54: rot motes (warnings, dust, shards); the pouch page reuses 39..45
#define OAM_SHADOW  55  // where it will land
#define OAM_STARS   56  // dazed
#define OAM_LEAFEN  57  // 57..62: Leafen waiting in this area, or the three of a rescue
#define OAM_SHINY   63  // Dewdrop's shiny pebble
#define OAM_POOL    64  // 64..124: the shared block, by region:
#define OAM_ENEMY   (OAM_POOL + 0)   //   +0..7: blobs and drones (combat areas)
#define OAM_CLAMP   (OAM_POOL + 8)   //   +8..14: Vael clamps on the Great Tree's roots (its region, before the heal)
#define OAM_RING    (OAM_POOL + 8)   //   +8..23: the Gloomheart's rot ring (inside the tree)
#define OAM_FIREFLY (OAM_POOL + 8)   //   +8..11: the marsh's fireflies
#define OAM_LHOME   (OAM_POOL + 8)   //   +8..27: the Leafen's tiny homes (the Great Tree's region, healed)
#define OAM_FOLK    (OAM_POOL + 28)  //   +28..60: the Leafen living round Bark's tree (33: three each for 11)
#define POOL_SIZE   61
#define OAM_SWIMHANDS 125 // swimming, front to back: ripples (slot 0), his hands, him, his wake
#define OAM_SWIMRING 126  // (named for the ring's old slot: now his head and shoulders while he swims)
#define OAM_WAKE    127
#define OAM_NONE    128   // no slot: sprite_set and sprite_hide ignore it
#define SPIRIT_OAM(i) (OAM_SPIRIT + (i))
static const u8 zzz_slot[9] = { OAM_ZZZ, OAM_ZZZ + 1, OAM_ZZZ + 2, OAM_ZZZ + 3, OAM_NONE, OAM_NONE, OAM_ZZZ + 4,
                                OAM_NONE, OAM_NONE };
#define ZZZ_OAM(i) (zzz_slot[i])
static void pool_clear(void) { for (s32 i = 0; i < POOL_SIZE; i++) oam_shadow[(OAM_POOL + i) * 4] = 0x0200; }
#define PAL_SHIELD  15
// v0.18.3: each region's own sprites load at TILE_REGION as you walk in (bosses, region enemies, a corner's
// spirit attack), with its colors in OBJ bank PAL_BOSS; areas without a set keep whatever is there.
enum { RS_NONE, RS_TREE, RS_ORCH };
static s32 region_set_now = -1;
static void load_region_sprites(s32 a) {
    if (tree_alive)                                    // v0.19: Flick's colors take Breeze's sleeping bank
        for (s32 i = 0; i < 16; i++) PAL_OBJ[PAL_FLICK * 16 + i] = obj_pal_flick[i];
    s32 rs = a == WEST || a == HOLLOW_IN ? RS_TREE : a == ORCHARD ? RS_ORCH : RS_NONE;
    if (rs == region_set_now || rs == RS_NONE) return;
    region_set_now = rs;
    if (rs == RS_TREE) {
        copy32(OBJ_TILES + TILE_REGION * 8, region_tree_tiles, sizeof(region_tree_tiles) / 4);
        for (s32 i = 0; i < 16; i++) PAL_OBJ[PAL_BOSS * 16 + i] = obj_pal_boss[i];
    } else {                                           // v0.19 the orchard: its smoke creatures
        copy32(OBJ_TILES + TILE_REGION * 8, region_orch_tiles, sizeof(region_orch_tiles) / 4);
        for (s32 i = 0; i < 16; i++) PAL_OBJ[PAL_BOSS * 16 + i] = obj_pal_orch[i];
    }
}

// ---------- game state ----------
static s32 px, py, pdir, pstep;         // player top-left of the 16x16 feet cell (pixels), facing, walk step
static s32 cam_x, cam_y;
static u32 frame;

#define HIST 128   // power of two; the last follower trails 18*NUM_SPIRITS entries
static s32 hist_x[HIST], hist_y[HIST], hist_head;
static const signed char bob[16] = { 0, 1, 1, 2, 2, 2, 1, 1, 0, -1, -1, -2, -2, -2, -1, -1 };

// A spirit at home lives inside it and makes its home move (draw_home_spirit, animate_homes).
// Sprout sits on its pad; in the level 2 tree house it peeks out of the hole now and then (frames
// cut to the hole by build_data.py). Ripple floats in its pool, dives (rings spread), and comes back
// up (at level 2 the dome's jet sprays). Pebble sits on top of its cairn and hops; when it lands the
// rune glows. Breeze perches on its chime bar and puffs (the chimes swing); at level 2 it is the
// windmill's hub and the blades spin faster when it puffs.
#define SPR_PEEK (TILE_PEEK / 4)
static const u8 peek_frame[16] = { 9, 9, 9, 9, 9, 9, 9, 9, 0, 1, 2, 3, 2, 4, 2, 1 };   // 9 = hidden
static const u8 peek_len[16]   = { 30, 30, 30, 30, 30, 30, 30, 30, 8, 8, 60, 40, 20, 40, 30, 8 };
#define RIPPLE_CYCLE 360   // float until 200, dive 200-230 (rings), under until 290, up 290-320 (rings)
#define PEBBLE_CYCLE 280   // sit until 200, hop 200-224, rune glows 224-270
#define BREEZE_CYCLE 300   // calm until 240, puff 240-300
static u32 home_t(s32 k, u32 cycle) { return (frame + k * 71) % cycle; }
static void draw_home_spirit(s32 i, s32 x, s32 y, s32 b, s32 anim);

// ---------- spirits ----------
enum { SP_ASLEEP, SP_AWAKE, SP_JOINED, SP_HOME, SP_HIDDEN, SP_POUCH };   // hidden: not in the world yet (Bark)
// SP_POUCH (v0.18.4): joined but not carried; it rides in the spirit pouch as a tiny orb (not selectable)
enum { KIND_SPROUT, KIND_RIPPLE, KIND_STONE, KIND_BREEZE, KIND_BARK, KIND_LILY, KIND_FLICK };
// housed: the spirit has a home. While it travels with you its power is weaker (fewer charges).
typedef struct { s32 x, y, state, kind, energy, housed, area; } Spirit;   // area: where it is
#define NUM_SPIRITS 7   // v0.18: Lily, summoned by Bark; v0.19: Flick, found in the burnt orchard
#define OLD_SPIRITS 5   // saves before v0.18 hold five
#define NUM_SHIELDS 5   // the Gloomheart's shields: Sprout, Ripple, Pebble, Breeze, Bark
#define NUM_HOMES 4   // Sprout, Ripple, Pebble, Breeze have homes; Bark has none yet
#define ENERGY_MAX 3
#define ENERGY_MAX_HOUSED 2
#define REGEN_FRAMES 300          // one charge back per 5 s while standing on living ground
static Spirit spirits[NUM_SPIRITS];   // index == kind
static s32 party[NUM_SPIRITS], party_count, active;   // party holds spirit indexes in join order
static u32 pouch_on;                       // v0.18.4: Bark gave you the spirit pouch (saved)
static const char *spirit_name[] = { "SPROUT", "RIPPLE", "PEBBLE", "BREEZE", "BARK", "LILY", "FLICK" };
static const char *spirit_skill[] = { "SPROUT: BLOOM", "RIPPLE: BRIDGE", "PEBBLE: BREAK ROCK",
                                      "BREEZE: CLEAR THORNS", "BARK: CUT AND CLEAR", "LILY: CLEAN WATER, PADS",
                                      "FLICK: LIGHT LANTERNS, FIRES" };
static const char *spirit_joined[] = { "SPROUT JOINED! A: BLOOM", "RIPPLE JOINED! R: SWAP",
                                       "PEBBLE JOINED! R: SWAP", "BREEZE JOINED! R: SWAP",
                                       "BARK JOINED... HMPH.", "LILY JOINED! WHEE!", "FLICK JOINED! CHEEP!" };
static const char *spirit_tired[] = { "SPROUT IS TIRED", "RIPPLE IS TIRED", "PEBBLE IS TIRED",
                                      "BREEZE IS TIRED", "BARK: NO. I'M TIRED.", "LILY IS SLEEPY...", "FLICK IS SLEEPY..." };
static const char *home_built[] = { "SPROUT'S GROVE IS BUILT!", "RIPPLE'S SPRING IS BUILT!",
                                    "PEBBLE'S CAIRN IS BUILT!", "BREEZE'S NOOK IS BUILT!" };
static const char *home_prompt[] = { "A: BUILD SPROUT'S GROVE", "A: BUILD RIPPLE'S SPRING",
                                     "A: BUILD PEBBLE'S CAIRN", "A: BUILD BREEZE'S NOOK" };
static const char *home_hint[] = { "A HOME SITE FOR SPROUT", "A HOME SITE FOR RIPPLE",
                                   "A HOME SITE FOR PEBBLE", "A HOME SITE FOR BREEZE" };
static const char *home_needs[] = { "NEEDS RIPPLE AND BREEZE", "NEEDS SPROUT AND PEBBLE",
                                    "NEEDS SPROUT AND RIPPLE", "NEEDS PEBBLE AND SPROUT" };
static const char *go_home[] = { "A: SPROUT GOES HOME", "A: RIPPLE GOES HOME", "A: PEBBLE GOES HOME",
                                 "A: BREEZE GOES HOME" };
static const char *call_back[] = { "A: CALL SPROUT BACK", "A: CALL RIPPLE BACK", "A: CALL PEBBLE BACK",
                                   "A: CALL BREEZE BACK" };
static const char *called_back[] = { "SPROUT JOINS. MAX 2 USES", "RIPPLE JOINS. MAX 2 USES",
                                     "PEBBLE JOINS. MAX 2 USES", "BREEZE JOINS. MAX 2 USES" };
// Building a home for kind k spends one charge from each of these two helpers.
static const u8 home_helper[NUM_SPIRITS][2] = {
    { KIND_RIPPLE, KIND_BREEZE }, { KIND_SPROUT, KIND_STONE },
    { KIND_SPROUT, KIND_RIPPLE }, { KIND_STONE, KIND_SPROUT },
};
static const char *home_bonus[] = { "BONUS: BLOOMS GROW BIGGER", "BONUS: CHARGES REFILL FASTER",
                                    "BONUS: BREAK TWO ROCKS", "BONUS: YOU WALK FASTER" };
#define REST_TIP "REST ON GREEN LAND TO REFILL"
static u8 home_done[NUM_SPIRITS];
// Homes pulse life outward: ring pulse_r[k] grows every HOME_PULSE frames, up to the home's reach
// (8 tiles; 12 after the level-2 upgrade).
#define HOME_REACH 12                  // largest reach (sizes the distance maps)
#define HOME_REACH_L1 8
static u8 home_level[NUM_SPIRITS];     // 0 = not built, 1, 2
// Build items (hold SELECT). The last four are quest rewards, shown once unlocked.
enum { B_PATH, B_BED, B_SAPLING, B_LANTERN, B_BENCH, B_UPGRADE, B_BLOSSOM, B_FOUNTAIN, B_STATUE, B_PINWHEEL,
       B_DEWPOND, B_ACORNBENCH, B_LEAFSWING, B_FLOWERARCH, B_PEBBLEPATH, B_MUSHLAMP,   // v0.17: one per Leafen
       NUM_BUILDS };
static s32 build_item;                 // the item SELECT-building places
static s32 cursor_mx = -1, cursor_my, cursor_ok;   // build preview: faced tile (mx -1 = hidden)
static u32 grow_timer;                 // built things grow every 10 s
static s32 home_reach(s32 k) { return home_level[k] >= 2 ? HOME_REACH : HOME_REACH_L1; }
#define HOME_PULSE 360
static s32 pulse_r[NUM_SPIRITS], pulse_timer[NUM_SPIRITS];
// Each home's distance map is computed once, when it is built; pulses only paint rings.
static u8 home_dist[NUM_SPIRITS][(HOME_REACH * 2 + 1) * (HOME_REACH * 2 + 1)];
// Tips told once, as bits: TIP_REST "rest on green land", TIP_UPGRADE "homes can grow",
// TIP_ALL "upgrade all four". (One u32, so saves keep their layout.)
static u32 told_tips;
#define TIP_REST    1
#define TIP_UPGRADE 2
#define TIP_ALL     4
static u32 told_ending;
// per kind: sprite cell, awake palette, asleep palette
static const u8 spirit_tile[] = { SPR_SPROUT, SPR_RIPPLE, SPR_STONE, SPR_BREEZE, SPR_BARK, SPR_LILY, SPR_FLICK };
static const u8 spirit_pal[] = { PAL_SPROUT, PAL_RIPPLE, PAL_STONE, PAL_BREEZE, PAL_BARK, PAL_LILY, PAL_FLICK };
static const u8 spirit_sleep_pal[] = { PAL_SPROUT, PAL_SLEEP, PAL_STONE_SLEEP, PAL_BREEZE_SLEEP, PAL_BARK, PAL_LILY,
                                       PAL_FLICK };   // Flick, cold in its nest, has its own cold frame
static void draw_home_spirit(s32 i, s32 x, s32 y, s32 b, s32 anim) {
    s32 id = SPIRIT_OAM(i), lv = home_level[i];
    switch (i) {
    case KIND_SPROUT:
        if (lv < 2) { sprite_set(id, x, y - 4 + b, SPR_SPROUT + anim, 0, PAL_SPROUT); return; }
        {
            u32 t = frame % 454, k = 0;
            while (t >= peek_len[k]) t -= peek_len[k++];
            if (peek_frame[k] == 9) sprite_hide(id);
            else sprite_set(id, x, y, SPR_PEEK + peek_frame[k], 0, PAL_SPROUT);
        }
        return;
    case KIND_RIPPLE: {
        u32 t = home_t(i, RIPPLE_CYCLE);
        if (t >= 200 && t < 290) { sprite_hide(id); return; }            // under the water
        s32 dy = t >= 290 && t < 300 ? 4 - (s32)(t - 290) / 2 : 0;       // rising out of the pool
        if (t >= 190 && t < 200) dy = (t - 190) / 2;                     // sinking in
        sprite_set(id, x, y - 2 + (b >> 1) + dy, SPR_RIPPLE + anim, 0, PAL_RIPPLE);
        return;
    }
    case KIND_STONE: {
        u32 t = home_t(i, PEBBLE_CYCLE);
        s32 hop = t >= 200 && t < 224 ? (s32)((t - 200) * (224 - t)) / 24 : 0;   // up to 6 px
        sprite_set(id, x, y - 16 - (lv < 2 ? 7 : 14) - hop, SPR_STONE + (hop ? 1 : 0), 0, PAL_STONE);
        return;
    }
    default: {
        if (lv >= 2) { sprite_hide(id); return; }                          // it is the windmill's hub
        u32 t = home_t(i, BREEZE_CYCLE);
        s32 puff = t >= 240 && ((t >> 3) & 1);
        sprite_set(id, x + puff, y - 16 - 7 - puff, SPR_BREEZE + (t >= 240 ? 1 : anim), 0, PAL_BREEZE);
    }
    }
}

// Copy a 16x16 metatile's graphics into another metatile's slot in VRAM.
static void set_meta_gfx(u32 slot, u32 src) { copy32(CHARBLOCK(0) + meta_vram(slot) * 32, terrain_tiles + src * 32, 32); }

// Run every frame: each home whose spirit is inside shows that spirit's animation frame.
static void animate_homes(void) {
    for (s32 k = 0; k < NUM_HOMES; k++) {
        if (!home_done[k]) continue;
        s32 lv = home_level[k];
        u32 roof = home_tile(k, lv, 1), base = home_tile(k, lv, 0);
        u32 r = roof, bs = base;
        if (spirits[k].state == SP_HOME) {
            if (k == KIND_RIPPLE) {
                u32 t = home_t(k, RIPPLE_CYCLE);
                u32 ring1 = lv < 2 ? SPRING1_RING1 : SPRING2_RING1;
                if ((t >= 200 && t < 215) || (t >= 290 && t < 305)) bs = ring1;
                else if ((t >= 215 && t < 230) || (t >= 305 && t < 320)) bs = ring1 + 1;
                if (lv >= 2 && t >= 290 && t < 350) r = SPRING2_SPRAY;
            } else if (k == KIND_STONE) {
                u32 t = home_t(k, PEBBLE_CYCLE);
                if (t >= 224 && t < 270 && ((t - 224) < 30 || (t & 4))) r = lv < 2 ? CAIRN1_GLOW : CAIRN2_GLOW;
            } else if (k == KIND_BREEZE) {
                u32 t = home_t(k, BREEZE_CYCLE);
                if (lv < 2) { if (t >= 240 && ((t >> 3) & 1)) r = NOOK1_SWING; }
                else if ((frame >> (t >= 240 ? 2 : 4)) & 1) r = NOOK2_SPIN;   // fast while it puffs
            }
        }
        set_meta_gfx(roof, r);
        set_meta_gfx(base, bs);
    }
}

static s32 bonus_charge;   // +1 max charge for everyone after the steppe chest
static s32 energy_max(const Spirit *s) { return (s->housed ? ENERGY_MAX_HOUSED : ENERGY_MAX) + bonus_charge; }
static int in_party(s32 kind) {
    for (s32 p = 0; p < party_count; p++) if (party[p] == kind) return 1;
    return 0;
}

// ---------- friendship ----------
// Friendship points 0..100 per spirit; every 20 is a heart (max 5).
static u8 fp[NUM_SPIRITS];
static u8 quest[NUM_SPIRITS];          // 0 not offered, 1 active, 2 done
static u8 unlocked[NUM_SPIRITS];       // quest reward build item unlocked
static s32 bonded = -1;                // the bonded companion's kind, or -1
static u32 thorn_quest;                // thorns cleared since Breeze asked
static s32 hearts(s32 k) { return fp[k] / 20; }
static int boosted(s32 k) { return hearts(k) >= 3; }   // the 3-heart power boost

// ---------- Vael pumps ----------
// After the first home is built, Vael pumps start up on living land (one per area at most),
// outside every home's reach. Each drains the nearest living tile within PUMP_REACH every
// PUMP_DRAIN frames. Pressing A on one (with a spirit's help: 1 charge) shuts it down.
#define PUMP_REACH 6
#define PUMP_DRAIN 240
#define PUMP_FIRST 7200     // 2 minutes after the first home
#define PUMP_EVERY 10800    // then every 3 minutes, while an area has none
static u8 pump_on[NUM_AREAS], pump_x[NUM_AREAS], pump_y[NUM_AREAS];
static u32 pump_drain[NUM_AREAS], pump_timer, pumps_stopped;

// ---------- materials and relics ----------
static u32 stone, fruit, seeds, wood;   // wood: from Bark's cuts (v0.13; for builds to come)
static u8 chest_open[NUM_CHESTS], wreck_read[NUM_WRECKS];
static u8 picked[NUM_AREAS][MAP_W * MAP_H / 8];   // one bit per tile: a tree already shaken
static s32 pulse_div = 1;                          // 2 after the hollow chest: homes pulse twice as fast
// Harmony (v0.13): the first time all four homes are level 2 with every spirit inside at once.
// From then on, for good: blooms +2 radius, and homes pulse twice as often and never stop.
static u32 harmony;
static u8 told_full[NUM_AREAS];                    // "... IS FULLY ALIVE!" shown
static u32 told_grown;                             // "EVERY HOME IS FULLY GROWN!" shown
// Bark's arrival (v0.13): bark_step walks through bark_scene once every home is level 2.
// bark_wait counts frames before the next step; bark_fall > 0 while Bark drops in from above.
static u32 bark_step, bark_wait, bark_fall, bark_line;
// The Great Tree (v0.14): tree_step walks Bark's scene there; tree_done holds the finished tasks
// (TASK_* bits). Both are saved.
static u32 tree_step, tree_wait, tree_done;
#define TASK_CLAMPS 1
#define TASK_THORNS 2
#define TASK_POOL   4
#define TASK_TIPS   8
#define TASKS_ALL  15
// Combat (v0.15; see the combat section): the wizard's hearts, the hit blink, the dev mode's
// invincibility, and the mini bosses beaten (a bit each, saved).
#define MAX_HEARTS 5
static u32 hearts_now = MAX_HEARTS, hurt_timer, dev_god, boss_beaten;
// v0.19 (step 4) cooking at a lit campfire (Caleb: one meal at a time, 45 seconds; no faster walking)
enum { MEAL_NONE, MEAL_HEART, MEAL_LIGHT };
#define MEAL_FRAMES (45 * 60)
static u32 meal_kind, meal_timer, mushrooms;      // mushrooms: picked in the marsh (saved); the meal is not saved
static u32 hearts_max(void) { return MAX_HEARTS + (meal_kind == MEAL_HEART); }
static u32 revive_step, revive_wait, shake_timer;   // the Great Tree coming back (v0.16; steps saved)
static u32 leaf_timer;                              // > 0 while leaves swirl round the wizard
// ---------- the Leafen (v0.17) ----------
// Six Leafen hide in the valley, the steppe, and Stone Hollow once the Great Tree is healed. Help one
// and it calls up two friends; they walk to Bark's tree, where they build tiny homes. Every two that
// Bark welcomes, he teaches YOU a skill: LEAF STEP, LEAF SHIELD, SEEDFALL.
enum { LF_PIP, LF_MOSSY, LF_TWIG, LF_POSY, LF_NUB, LF_DEW, LF_BOG, LF_REEDY, LF_PUDDLE, LF_WICK, LF_SOOT, NUM_LEAFEN };
#define FIRST_LEAFEN 6   // the valley, steppe and hollow ones; v0.18 adds three in the marsh
#define FIRST_ORCH_LEAFEN 9   // v0.19.4: two in the burnt orchard (they need Flick's world: light, the stacks gone)
// v0.18 Bark summons Lily once the first six are home; then the marsh opens (lily_step past the scene).
static u32 lily_step;
#define LILY_SCENE_LEN 10
static u32 flick_step, flick_flags;   // v0.19 Flick's wake-up scene; flags: 1 the warm ember, 2 the chirp told,
                                      // 4 its first shiver heard
#define FLICK_SCENE_LEN 8
static u32 swim_step;                                 // v0.18.1: Ripple's swimming lesson (saved; swim_skill after)
enum { LS_NONE, LS_WAIT, LS_MET, LS_FREE, LS_HOME };   // FREE: on the way to the tree; HOME: welcomed
static u8 leafen_state[NUM_LEAFEN];
static u32 leafen_init, has_shiny, pip_cell, shield_wait;
static s32 lf_scene, welcome_step;                    // a rescue scene, the welcome at the tree (not saved)
static u32 leafen_home_count(void) {
    u32 n = 0;
    for (s32 k = 0; k < NUM_LEAFEN; k++) n += leafen_state[k] == LS_HOME;
    return n;
}
// v0.17.3 (Caleb: space them out, more Leafen come in the corners): LEAF STEP at 2 Leafen home, LEAF
// SHIELD at all 6. SEEDFALL (level 3) waits for the corner regions; its code stays, unreachable for now.
static u32 skill_lv(void) { u32 n = leafen_home_count(); return n >= FIRST_LEAFEN ? 2 : n >= 2 ? 1 : 0; }
// v0.16.2 revival inside the tree: Bark floats (float_on 1; 2 = shot up and gone), the screen goes white
// (white_mode 1 opening like an old TV turning on, in reverse: a line, then the whole screen; 2 white;
// 3 fading back; 4 fading to white).
static s32 float_on, float_x, float_y, float_h, float_up, float_tx, float_ty, white_mode, white_t, white_slow;
static void combat_enter(void);   // enemies and the boss of the area you enter
static void draw_hearts_hud(void);

// ---------- blooms ----------
#define BR 10                     // max bloom radius: 4 near water, +2 grove level 2, +1 at 3 hearts, +1 bonded, +2 harmony
#define BGRID (BR * 2 + 1)
typedef struct { s32 cx, cy, r, maxr, timer, active; u8 dist[BGRID * BGRID]; } Bloom;
#define MAX_BLOOMS 6
static Bloom blooms[MAX_BLOOMS];
static s32 sparkle_x, sparkle_y, sparkle_life;
static u32 told_river, told_spring;
static u32 regen_timer;
static s32 just_called = -1;   // spirit just called back from home (see handle_action)

// v0.18.1 (Caleb): Ripple teaches the wizard to swim in the marsh. Then, with Ripple chosen, B by deep
// water jumps in (any water body of at least SWIM_MIN tiles; not little ponds). In the water you keep
// swimming whoever is chosen (Lily can clean as you go); walking onto land climbs out. Never in sludge.
#define SWIM_MIN 16
static u32 swim_skill, in_water, orb_morph;   // orb_morph: frames left of the spirits changing form
static int water_tile(u8 t) { return t == WATER || is_bog(t); }
static int liquid(u8 t) { return water_tile(t) || t == SLUDGE || t == LILY_PAD || t == BOG_PAD; }
// Caleb: no slipping across a two-wide river. A body of water can be swum only if it is at least three
// wide somewhere (a 3x3 square of liquid; sludge counts for the width, but is never swum) and has at least
// SWIM_MIN tiles. A two-wide river never has such a square, so it cannot be swum anywhere along it.
static int wide_water(s32 mx, s32 my) {
    for (s32 oy = -2; oy <= 0; oy++)
        for (s32 ox = -2; ox <= 0; ox++) {
            int all = 1;
            for (s32 y = 0; y < 3 && all; y++)
                for (s32 x = 0; x < 3 && all; x++) {
                    s32 tx = mx + ox + x, ty = my + oy + y;
                    if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H || !liquid(world[ty * MAP_W + tx])) all = 0;
                }
            if (all) return 1;
        }
    return 0;
}
static int on_solid(s32 x, s32 y) {
    // feet box: x+3..x+12, y+10..y+15
    s32 xs[2] = { x + 3, x + 12 }, ys[2] = { y + 10, y + 15 };
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++) {
            s32 tx = xs[i] >> 4, ty = ys[j] >> 4;
            u8 t = tile_at(tx, ty);
            if (is_solid(t) && !(in_water && water_tile(t))) return 1;
        }
    return 0;
}
static int swimming(void) { return in_water && water_tile(tile_at((px + 8) >> 4, (py + 12) >> 4)); }
// Can the water body at (mx, my) be swum? At least SWIM_MIN tiles of liquid, three wide somewhere.
static int deep_water(s32 mx, s32 my) {
    static u16 stack[MAP_W * MAP_H];
    static u8 seen[MAP_W * MAP_H / 8];
    for (u32 i = 0; i < sizeof(seen); i++) seen[i] = 0;
    s32 n = 0, count = 0, wide = 0;
    stack[n++] = my * MAP_W + mx; seen[stack[0] >> 3] |= 1 << (stack[0] & 7);
    while (n) {
        s32 c = stack[--n], x = c % MAP_W, y = c / MAP_W;
        count++;
        if (!wide && wide_water(x, y)) wide = 1;
        if (wide && count >= SWIM_MIN) return 1;
        static const signed char dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
        for (s32 k = 0; k < 4; k++) {
            s32 nx = x + dx[k], ny = y + dy[k], id = ny * MAP_W + nx;
            if (nx < 0 || ny < 0 || nx >= MAP_W || ny >= MAP_H || (seen[id >> 3] & (1 << (id & 7)))) continue;
            if (!liquid(tile_at(nx, ny))) continue;
            seen[id >> 3] |= 1 << (id & 7);
            stack[n++] = id;
        }
    }
    return 0;
}
static int in_world(s32 x, s32 y) { return x >= 0 && y >= 0 && x <= WORLD_PX - 16 && y <= WORLD_PX - 16; }
static int blocked_at(s32 x, s32 y) { return on_solid(x, y) || !in_world(x, y); }
// A tile can turn solid under the player (a home appears). Move the player to the nearest free spot.
static void unstick_player(void) {
    if (!blocked_at(px, py)) return;
    static const signed char dir_x[4] = { 0, -1, 1, 0 }, dir_y[4] = { 1, 0, 0, -1 };   // down first
    for (s32 d = 1; d <= 24; d++)
        for (s32 k = 0; k < 4; k++)
            if (!blocked_at(px + dir_x[k] * d, py + dir_y[k] * d)) { px += dir_x[k] * d; py += dir_y[k] * d; return; }
}

// Once the Great Tree is healed, Bark lives in it and his slot is the wizard's own: "YOU".
static int is_you(s32 kind) { return kind == KIND_BARK && tree_alive; }
static void draw_active(void) {
    for (s32 c = 17; c < 30; c++) SCREENBLOCK(TEXT_SB)[c] = glyph_entry(' ');   // the bar spans the screen (16: a sixth heart)
    if (party_count == 0) return;
    Spirit *s = &spirits[party[active]];
    const char *n = is_you(s->kind) ? "YOU" : spirit_name[s->kind];
    s32 len = 0; while (n[len]) len++;
    char num[2] = { (char)('0' + s->energy), 0 };
    put_text(30 - len - 4, 0, " ");
    put_text(30 - len - 3, 0, n);
    put_text(27, 0, " ");
    put_text(28, 0, is_you(s->kind) || s->kind == KIND_FLICK ? " " : num);   // the wizard and Flick need no charges
    put_text(29, 0, " ");
}

// Field by field: a struct literal makes gcc call memset, and there is no libc.
static void spirit_init(s32 kind, s32 mx, s32 my, s32 state) {
    Spirit *s = &spirits[kind];
    s->x = mx * 16; s->y = my * 16; s->state = state; s->kind = kind;
    s->energy = ENERGY_MAX; s->housed = 0; s->area = 0;
}

static void reset_game(void) {
    music_play(SONG_VALLEY);
    load_world();
    set_ground_palette(0);
    px = PLAYER_START_X * 16; py = PLAYER_START_Y * 16; pdir = DIR_DOWN;
    spirit_init(KIND_SPROUT, SPIRIT_SPOT_X, SPIRIT_SPOT_Y, SP_AWAKE);
    spirit_init(KIND_RIPPLE, RIPPLE_SPOT_X, RIPPLE_SPOT_Y, SP_ASLEEP);
    spirit_init(KIND_STONE, STONE_SPOT_X, STONE_SPOT_Y, SP_ASLEEP);
    spirit_init(KIND_BREEZE, BREEZE_SPOT_X, BREEZE_SPOT_Y, SP_ASLEEP);
    spirit_init(KIND_BARK, 0, 0, SP_HIDDEN);   // falls in once all four homes are level 2 (bark_step)
    spirit_init(KIND_LILY, 0, 0, SP_HIDDEN);   // v0.18: Bark summons her once six Leafen are home
    spirit_init(KIND_FLICK, ORCHARD_NEST_X, ORCHARD_NEST_Y, SP_ASLEEP);   // v0.19: cold in its nest in the orchard
    spirits[KIND_FLICK].area = ORCHARD;
    flick_step = flick_flags = 0;
    lily_step = 0; swim_skill = swim_step = 0; in_water = 0; pouch_on = 0;
    bark_step = bark_wait = bark_fall = bark_line = 0;
    tree_step = tree_wait = tree_done = 0;
    boss_beaten = 0;
    tree_alive = revive_step = revive_wait = shake_timer = leaf_timer = 0;
    for (s32 k = 0; k < NUM_LEAFEN; k++) leafen_state[k] = LS_NONE;
    leafen_init = has_shiny = pip_cell = shield_wait = 0;
    lf_scene = welcome_step = 0;
    spirits[KIND_STONE].area = STONE_AREA;     // Pebble sleeps in the Windy Steppe
    spirits[KIND_BREEZE].area = BREEZE_AREA;   // Breeze sleeps in Stone Hollow
    for (s32 i = 0; i < NUM_SPIRITS; i++) home_done[i] = 0;
    told_ending = 0;
    for (s32 i = 0; i < NUM_SPIRITS; i++) home_level[i] = 0;
    build_item = B_UPGRADE;   // homes come first in the build list
    grow_timer = 0;
    for (s32 i = 0; i < NUM_SPIRITS; i++) fp[i] = quest[i] = unlocked[i] = 0;
    bonded = -1;
    thorn_quest = 0;
    for (s32 a = 0; a < NUM_AREAS; a++) pump_on[a] = 0, pump_drain[a] = 0;
    pump_timer = pumps_stopped = 0;
    msgq_len = 0;
    party_count = 0; active = 0;
    for (s32 i = 0; i < HIST; i++) { hist_x[i] = px; hist_y[i] = py; }
    for (s32 i = 0; i < MAX_BLOOMS; i++) blooms[i].active = 0;
    sparkle_life = 0;
    told_river = 0;
    told_spring = 0;
    regen_timer = 0;
    told_tips = 0;
    for (s32 i = 0; i < NUM_SPIRITS; i++) pulse_r[i] = pulse_timer[i] = 0;
    just_called = -1;
    stone = fruit = seeds = wood = 0;
    harmony = told_grown = 0;
    for (s32 a = 0; a < NUM_AREAS; a++) told_full[a] = 0;
    for (s32 i = 0; i < NUM_CHESTS; i++) chest_open[i] = 0;
    for (s32 i = 0; i < NUM_WRECKS; i++) wreck_read[i] = 0;
    for (s32 a = 0; a < NUM_AREAS; a++)
        for (s32 i = 0; i < MAP_W * MAP_H / 8; i++) picked[a][i] = 0;
    bonus_charge = 0;
    pulse_div = 1;
    for (s32 r = 0; r < 20; r++) clear_row(r);
    draw_life();
    draw_active();
    combat_enter();
    show_message("VAEL TOOK THE WATER.");
    queue_message("THE SPIRITS LOST THEIR HOMES.");
    queue_message("FIND THE SPIRIT");
}

static int water_near(s32 mx, s32 my, s32 dist) {
    for (s32 y = my - dist; y <= my + dist; y++)
        for (s32 x = mx - dist; x <= mx + dist; x++)
            if (tile_at(x, y) == WATER) return 1;
    return 0;
}

// Distance map around (cx, cy) in a (2R+1)^2 grid: life spreads around obstacles but never
// across water, rock, or thorns. Costs: 10 per straight step, 14 per diagonal, so rings come out round.
// Two-sweep relaxation (forward, then backward) repeated until nothing changes: same result as
// relaxing every cell g times, at a fraction of the cost (a radius-8 pulse used to stall ~10 frames).
#define FLOOD_MAX (HOME_REACH * 2 + 1)
static u8 flood_open[FLOOD_MAX * FLOOD_MAX];
static void flood_dist(u8 *dist, s32 cx, s32 cy, s32 R) {
    static const signed char fdx[4] = { -1, 0, 1, -1 }, fdy[4] = { -1, -1, -1, 0 };   // already-visited side
    s32 g = R * 2 + 1;
    for (s32 gy = 0; gy < g; gy++)
        for (s32 gx = 0; gx < g; gx++) {
            u8 t = tile_at(cx + gx - R, cy + gy - R);
            flood_open[gy * g + gx] = !(t == WATER || is_rock(t) || t == THORN || t == MENHIR || t == DRY_RIVER ||
                                        t == SLUDGE || t == REEDS || t == VAEL_PIPE || is_bog(t) || t == LOG ||
                                        t == DAM || t == LOST_STONE || is_trunk(t) || t == CLAMP || t == DRY_POOL);
            dist[gy * g + gx] = 255;
        }
    dist[R * g + R] = 0;
    for (int changed = 1; changed; ) {
        changed = 0;
        for (s32 dir = 1; dir >= -1; dir -= 2) {          // forward sweep, then backward sweep
            s32 y0 = dir > 0 ? 0 : g - 1, y1 = dir > 0 ? g : -1;
            for (s32 gy = y0; gy != y1; gy += dir)
                for (s32 gx = y0; gx != y1; gx += dir) {
                    s32 i = gy * g + gx;
                    if (!flood_open[i]) continue;
                    s32 best = dist[i];
                    for (s32 k = 0; k < 4; k++) {
                        s32 nx = gx + fdx[k] * dir, ny = gy + fdy[k] * dir;
                        if (nx < 0 || ny < 0 || nx >= g || ny >= g) continue;
                        s32 d = dist[ny * g + nx];
                        if (d == 255) continue;
                        d += (fdx[k] && fdy[k]) ? 14 : 10;
                        if (d < best) best = d;
                    }
                    if (best < dist[i]) { dist[i] = best > 255 ? 255 : best; changed = 1; }
                }
        }
    }
}

// Blooms use a 9x9 grid (as before v0.4.2) unless the bloom is bigger than radius 4.
static s32 bloom_grid_r(const Bloom *b) { return b->maxr > 4 ? BR : 4; }
static void bloom_distances(Bloom *b) { flood_dist(b->dist, b->cx, b->cy, bloom_grid_r(b)); }

static void start_bloom_r(s32 mx, s32 my, s32 reach) {   // reach < 0: Sprout's own (water, homes, hearts)
    for (s32 i = 0; i < MAX_BLOOMS; i++) {
        if (blooms[i].active) continue;
        Bloom *b = &blooms[i];
        b->cx = mx; b->cy = my; b->r = -1; b->timer = 0; b->active = 1;
        int wet = water_near(mx, my, 3);
        b->maxr = reach >= 0 ? reach :
                  (wet ? 4 : 2) + home_level[KIND_SPROUT] + boosted(KIND_SPROUT) + (bonded == KIND_SPROUT) +
                  (harmony ? 2 : 0);
        bloom_distances(b);
        sparkle_x = mx * 16; sparkle_y = my * 16; sparkle_life = 40;
        sfx_play(SFX_BLOOM);
        if (wet && !told_river) { show_message("THE RIVER FEEDS THE ROOTS"); told_river = 1; }
        return;
    }
}

static void start_bloom(s32 mx, s32 my) { start_bloom_r(mx, my, -1); }

static int player_on_tile(s32 mx, s32 my) {
    return (px + 3) >> 4 <= mx && mx <= (px + 12) >> 4 &&
           (py + 10) >> 4 <= my && my <= (py + 15) >> 4;
}

// The Great Tree region's way back east (x 24..31, rows 12..14): kept free of trees.
// v0.17.3: the valley's side of that gate too (x 0..5, rows 12..14), where blooms could grow trees.
static int in_west_gate(s32 mx, s32 my) {
    if (work_area == WEST && my >= 28 && mx >= 13 && mx <= 18) return 1;   // v0.18: the way south, to the marsh
    if (my < 12 || my > 14) return 0;
    return (work_area == WEST && mx >= 24) || (work_area == 0 && mx <= 5);
}
static void clear_west_gate(void) {
    for (s32 y = 12; y <= 14; y++)
        for (s32 x = 24; x < MAP_W; x++) {
            u8 *c = &maps[WEST][y * MAP_W + x];
            if (*c == TREE || *c == DEAD_TREE || *c == STUMP || *c == STUMP_GRASS) *c = tree_alive ? GRASS : DEAD;
        }
    for (s32 y = 28; y < MAP_H; y++)                       // v0.18: the way south to the marsh
        for (s32 x = 13; x <= 18; x++) {
            u8 *c = &maps[WEST][y * MAP_W + x];
            if (*c == TREE || *c == DEAD_TREE || *c == STUMP || *c == STUMP_GRASS) *c = tree_alive ? GRASS : DEAD;
        }
    for (s32 y = 12; y <= 14; y++)                         // the valley side: only trees that grew there
        for (s32 x = 0; x <= 5; x++)
            if (maps[0][y * MAP_W + x] == TREE) maps[0][y * MAP_W + x] = GRASS;
}
static void grow_tile(s32 mx, s32 my, int edge) {
    if (mx < 0 || my < 0 || mx >= MAP_W || my >= MAP_H) return;
    u8 t = tile_at(mx, my);
    int here = work_area == cur_area;   // the player only stands in the area on screen
    if (t == SWAMP_TREE) { set_tile(mx, my, SWAMP_TREE_ALIVE); return; }   // v0.18.1: swamp trees leaf out
    if (t == BURNT_FRUIT_TREE) { set_tile(mx, my, FRUIT_TREE); return; }   // v0.19.4: the orchard bears fruit again
    if (t == BURNT_TREE) { set_tile(mx, my, TREE); return; }
    if (t == SWAMP_TOP) { set_tile(mx, my, SWAMP_TOP_ALIVE); return; }
    if (t == DEAD_TREE || t == STUMP || t == STUMP_GRASS) {   // dead trees and stumps grow back
        if (!(here && player_on_tile(mx, my))) set_tile(mx, my, TREE);
        return;
    }
    if (t == ROOT_TIP_DEAD) { set_tile(mx, my, ROOT_TIP_ALIVE); return; }   // a bloom wakes the Great Tree's roots
    if (!edge && (t == CHEST || t == CHEST_OPEN || t == WRECK)) {   // grass grows in under chests and relics
        set_tile(mx, my, t == CHEST ? CHEST_GRASS : t == CHEST_OPEN ? CHEST_OPEN_GRASS : WRECK_GRASS);
        return;
    }
    if (t != DEAD && t != DEAD_CRACK && t != DIRT && t != DRY_GRASS && t != PEBBLES && t != MOSS && t != MUSHROOMS &&
        t != CHARRED_BRUSH) return;
    if (edge && t != DIRT) { set_tile(mx, my, DIRT); return; }
    u32 r = rng() % 100;
    static const signed char nbx[4] = { 1, -1, 0, 0 }, nby[4] = { 0, 0, 1, -1 };
    for (s32 k = 0; k < 4; k++)                    // v0.17.3 (Caleb): rocks beside new life get grass round them
        if (tile_at(mx + nbx[k], my + nby[k]) == ROCK) set_tile(mx + nbx[k], my + nby[k], ROCK_GRASS);
    u8 nt = r < 55 ? GRASS : r < 80 ? GRASS_TUFT : r < 95 ? FLOWERS : TREE;
    if (nt == TREE && here && player_on_tile(mx, my)) nt = FLOWERS;
    if (nt == TREE && in_west_gate(mx, my)) nt = FLOWERS;   // never close the way back to the valley
    set_tile(mx, my, nt);
}

// A sleeping spirit wakes once living ground reaches the tiles around it.
static void check_wake(void) {
    for (s32 i = 0; i < NUM_SPIRITS; i++) {
        Spirit *s = &spirits[i];
        if (s->state != SP_ASLEEP || s->area != work_area || i == KIND_FLICK) continue;   // Flick needs the ember
        s32 mx = (s->x + 8) >> 4, my = (s->y + 8) >> 4;
        for (s32 y = my - 1; y <= my + 1; y++)
            for (s32 x = mx - 1; x <= mx + 1; x++)
                if (is_alive(tile_at(x, y)) && s->state == SP_ASLEEP) {
                    s->state = SP_AWAKE;
                    show_message("A SPIRIT WOKE UP!");
                    sfx_play(SFX_WAKE);
                }
    }
}

static void update_blooms(void) {
    int changed = 0;
    for (s32 i = 0; i < MAX_BLOOMS; i++) {
        Bloom *b = &blooms[i];
        if (!b->active || ++b->timer < 6) continue;
        b->timer = 0;
        b->r++;
        s32 r = b->r;
        s32 lo = r == 0 ? -1 : 10 * (r - 1) + 5, hi = r == 0 ? 0 : 10 * r + 5;
        s32 R = bloom_grid_r(b), g = R * 2 + 1;
        for (s32 gy = 0; gy < g; gy++)
            for (s32 gx = 0; gx < g; gx++) {
                s32 d = b->dist[gy * g + gx];
                if (d > lo && d <= hi) grow_tile(b->cx + gx - R, b->cy + gy - R, r == b->maxr);
            }
        chime(scale[r < 8 ? r : 7], r == 0 ? 12 : 9);
        changed = 1;
        if (b->r >= b->maxr) b->active = 0;
    }
    if (changed) { draw_life(); check_wake(); }
}

// ---------- v0.19 the dark (the burnt orchard) ----------
// The Great Tree's branch layer (BG2) is free outside its region: in the orchard it holds the dark, one 8x8
// cell each, blended over the map and the sprites (BLDCNT/BLDALPHA as for the branches: black lets a quarter
// of the light through). The darkness metatile's quarters: full, half, a quarter, none. Light comes from you
// (a small circle, Caleb), the open ground south of the fallen trunk, and lit lanterns and campfires.
#define DARK_R 26          // your own light (pixels)
#define DARK_R_FLICK 31    // a little bigger with Flick carried (Caleb: slightly)
#define FIRE_R 44          // a lit lantern's or campfire's
#define MIN(a, b) ((a) < (b) ? (a) : (b))
static s32 dark_on, light_n, light_x[24], light_y[24];
static s32 dark_r(void) { return (in_party(KIND_FLICK) ? DARK_R_FLICK : DARK_R) + (meal_kind == MEAL_LIGHT ? 12 : 0); }
// Is (x, y) in a lantern's or campfire's light (its index), or the daylight past the trunk (-2), or dark (-1)?
static s32 stack_dark(s32 wx, s32 wy);
static s32 fire_lit(s32 x, s32 y) {
    if (y >= ORCHARD_WALL_ROW * 16 - 4) return -2;
    if (stack_dark(x, y) == 0) return -2;               // a cleared part (its stack smashed)
    for (s32 i = 0; i < light_n; i++) {
        s32 ddx = x - light_x[i], ddy = y - light_y[i];
        if (ddx * ddx + ddy * ddy < (FIRE_R - 4) * (FIRE_R - 4)) return i;
    }
    return -1;
}
static s32 dark_lv(s32 dx, s32 dy, s32 r) {        // 0 lit, 1 a quarter dark, 2 half, 3 dark
    s32 d2 = dx * dx + dy * dy;
    return d2 < (r - 10) * (r - 10) ? 0 : d2 < (r - 5) * (r - 5) ? 1 : d2 < r * r ? 2 : 3;
}
// v0.19.4: each smoke stack keeps its part of the orchard dark (the cells nearer it than any other stack);
// once Pebble smashes it, that part stays clear. 0 clear, 2 the dithered edge next to a clear part, 3 dark.
static s32 stack_dark(s32 wx, s32 wy) {
    s32 best = -1, bd = 1 << 30, cd = 1 << 30;           // the nearest stack, and the nearest smashed one
    for (s32 k = 0; k < NUM_STACKS; k++) {
        s32 ddx = wx - stack_x[k] * 16 - 8, ddy = wy - stack_y[k] * 16 - 8, d = ddx * ddx + ddy * ddy;
        int gone = maps[ORCHARD][stack_y[k] * MAP_W + stack_x[k]] != SMOKE_STACK;
        if (d < bd) { bd = d; best = k; }
        if (gone && d < cd) cd = d;
    }
    if (maps[ORCHARD][stack_y[best] * MAP_W + stack_x[best]] != SMOKE_STACK) return 0;
    return cd < bd + 2600 ? 2 : 3;                       // about a tile from the line between the two parts
}
static s32 dark_cell(s32 cx, s32 cy) {
    s32 wx = cx * 8 + 4, wy = cy * 8 + 4, y0 = ORCHARD_WALL_ROW * 16;
    s32 lv = wy >= y0 + 16 ? 0 : wy >= y0 + 10 ? 1 : wy >= y0 + 4 ? 2 : 3;   // daylight past the trunk
    if (lv) lv = MIN(lv, stack_dark(wx, wy));
    if (lv) lv = MIN(lv, dark_lv(wx - (px + 8), wy - (py + 4), dark_r()));
    for (s32 i = 0; i < light_n && lv; i++) lv = MIN(lv, dark_lv(wx - light_x[i], wy - light_y[i], FIRE_R));
    return lv;
}
static void dark_put(s32 cx, s32 cy) {
    if (cx < 0 || cy < 0 || cx >= 64 || cy >= 64) return;
    SCREENBLOCK(CANOPY_SB)[((cx >> 5) + ((cy >> 5) << 1)) * 1024 + (cy & 31) * 32 + (cx & 31)] =
        (meta_vram(DARKNESS) * 4 + 3 - dark_cell(cx, cy)) | (TREE_BANK << 12);
}
static void dark_enter(s32 a) {
    dark_on = a == ORCHARD;
    REG_BLDALPHA = dark_on ? 16 | (2 << 8) : 12 | (4 << 8);   // the dark lets an eighth through; branches 12/4
    if (!dark_on) return;
    light_n = 0;                                    // the lit ones on this map
    for (s32 i = 0; i < MAP_W * MAP_H && light_n < 24; i++)
        if (maps[ORCHARD][i] == LANTERN_LIT || maps[ORCHARD][i] == CAMPFIRE_LIT) {
            light_x[light_n] = (i % MAP_W) * 16 + 8; light_y[light_n++] = (i / MAP_W) * 16 + 6;
        }
    PAL_BG[TREE_BANK * 16 + 1] = RGB5(6, 6, 12);   // a smoky black
    for (s32 cy = 0; cy < 64; cy++)
        for (s32 cx = 0; cx < 64; cx++) dark_put(cx, cy);
    REG_DISPCNT |= 1 << 10;
}
static void dark_update(void) {                    // each frame: the cells round you (you move a few pixels at most)
    if (!dark_on) return;
    s32 cx = (px + 8) >> 3, cy = (py + 4) >> 3, n = (DARK_R_FLICK + 24) >> 3;
    for (s32 y = cy - n; y <= cy + n; y++)
        for (s32 x = cx - n; x <= cx + n; x++) dark_put(x, y);
}

// ---------- areas ----------
static const signed char area_east[NUM_AREAS] = { 1, -1, -1, 0, -1, 2, -1 }, area_west[NUM_AREAS] = { 3, 0, MARSH, -1, -1, -1, -1 };
static const signed char area_south[NUM_AREAS] = { 2, -1, -1, MARSH, -1, -1, WEST };
static const signed char area_north[NUM_AREAS] = { -1, -1, 0, ORCHARD, -1, WEST, -1 };
static const char *area_name[NUM_AREAS] = { "THE VALLEY", "THE WINDY STEPPE", "STONE HOLLOW", "THE GREAT TREE",
                                            "INSIDE THE GREAT TREE", "THE DROWNED MARSH", "THE BURNT ORCHARD" };
// Each area's tune; the Great Tree's two follow its story (area_song, after the Gloomheart's code).
// A scene may set its own (scene_song; -1 = silence).
#define NO_SCENE_SONG (-2)
static s32 scene_song = NO_SCENE_SONG;
static u32 revive_slow;                 // the revival's middle lines stay up longer
static u32 omen_step;                   // the last mini boss's jingle is playing (update_music)
static s32 area_song(s32 a);

// Walk into area a, standing at (nx, ny). Fails (returns 0) if that spot is blocked.
static int enter_area(s32 a, s32 nx, s32 ny) {
    use_area(a);
    if (blocked_at(nx, ny)) { use_area(cur_area); return 0; }
    cur_area = a;
    music_play(area_song(a));
    for (s32 i = 0; i < MAX_BLOOMS; i++) blooms[i].active = 0;   // blooms finish where they started
    sparkle_life = 0;
    px = nx; py = ny;
    for (s32 i = 0; i < HIST; i++) { hist_x[i] = px; hist_y[i] = py; }
    for (s32 p = 0; p < party_count; p++) {
        Spirit *s = &spirits[party[p]];
        s->area = a; s->x = px; s->y = py;
    }
    REG_DISPCNT |= 0x0080;              // blank the screen while the new map is drawn
    set_ground_palette(a);
    draw_area();
    draw_life();
    REG_DISPCNT &= ~0x0080;
    show_message(area_name[a]);
    chime(523, 8);
    combat_enter();
    return 1;
}

// The opening at the Great Tree's foot (2 x 2 cells): solid until all four mini bosses are beaten.
#define DOOR_X 15
#define DOOR_Y 19

// The valley's closed edges (north, west) point you to the regions still without a home:
// the east (Breeze's nook, in the steppe) and the south (Pebble's cairn, in the hollow).
static void region_hint(void) {
    s32 east = !home_done[KIND_BREEZE], south = !home_done[KIND_STONE];
    if (msg_timer || msgq_len) return;
    if (!east && !south) {                 // both have homes: the west waits for Bark
        if (spirits[KIND_BARK].state != SP_JOINED) show_message("A DARK, DEAD WOOD LIES WEST.");
        return;
    }
    show_message("I WONDER WHAT LIES");
    queue_message(east && south ? "IN THE EAST AND SOUTH..." : east ? "IN THE EAST..." : "IN THE SOUTH...");
}

// Pressing against an open map edge leads to the next area. v0.18.2 (Caleb: often you could not cross):
// each edge is cut into thirds, and leaving through a third arrives in the same third of the other
// area's facing edge, at the nearest spot you can stand on (searched along the edge and inward).
enum { SIDE_WEST, SIDE_EAST, SIDE_NORTH, SIDE_SOUTH };
static int try_arrive(s32 a, s32 side, s32 third) {
    s32 c = (third * 2 + 1) * WORLD_PX / 6 - 8;          // the third's middle, along the edge
    use_area(a);
    for (s32 in = 0; in < 8; in++)
        for (s32 j = 0; j < 11; j++) {
            s32 along = c + ((j + 1) / 2) * 16 * ((j & 1) ? 1 : -1);
            if (along < 0 || along > WORLD_PX - 16) continue;
            s32 x = side == SIDE_WEST ? 2 + in * 16 : side == SIDE_EAST ? WORLD_PX - 18 - in * 16 : along;
            s32 y = side == SIDE_NORTH ? 2 + in * 16 : side == SIDE_SOUTH ? WORLD_PX - 18 - in * 16 : along - 6;
            if (!blocked_at(x, y)) { use_area(cur_area); return enter_area(a, x, y); }
        }
    use_area(cur_area);
    return 0;
}
static void cross_to(s32 a, s32 side, s32 along) {
    s32 third = along < WORLD_PX / 3 ? 0 : along < WORLD_PX * 2 / 3 ? 1 : 2;
    if (try_arrive(a, side, third)) return;
    for (s32 k = 0; k < 3; k++) if (k != third && try_arrive(a, side, k)) return;   // that third is shut: another
}

static void check_exits(void) {
    int lily = lily_step > LILY_SCENE_LEN;                 // the marsh opens once Lily has joined (v0.18)
    if (HELD(KEY_RIGHT) && px >= WORLD_PX - 16 && area_east[cur_area] >= 0)
        cross_to(area_east[cur_area], SIDE_WEST, py + 8);
    else if (HELD(KEY_LEFT) && px <= 0 && area_west[cur_area] >= 0 &&
             (cur_area != 0 || spirits[KIND_BARK].state == SP_JOINED || tree_alive) &&
             (cur_area != 2 || lily))                         // the west opens with Bark
        cross_to(area_west[cur_area], SIDE_EAST, py + 8);
    else if (HELD(KEY_DOWN) && py >= WORLD_PX - 16 && area_south[cur_area] >= 0 && (cur_area != WEST || lily))
        cross_to(area_south[cur_area], SIDE_NORTH, px + 8);
    else if (HELD(KEY_UP) && py <= 0 && area_north[cur_area] >= 0)
        cross_to(area_north[cur_area], SIDE_SOUTH, px + 8);
    else if (cur_area == 0 && ((HELD(KEY_UP) && py <= 0) || (HELD(KEY_LEFT) && px <= 0)))
        region_hint();
    else if (cur_area == HOLLOW_IN && HELD(KEY_DOWN) && py >= WORLD_PX - 18)
        enter_area(WEST, DOOR_X * 16 + 8, (DOOR_Y + 2) * 16 - 6);   // out, below the opening
    else if (cur_area == WEST && maps[WEST][DOOR_Y * MAP_W + DOOR_X] == DRY_BRIDGE &&
             (py + 12) >> 4 <= DOOR_Y + 1 && ((px + 8) >> 4) >= DOOR_X && ((px + 8) >> 4) <= DOOR_X + 1)
        enter_area(HOLLOW_IN, DOOR_X * 16 + 8, WORLD_PX - 42);      // into the tree
}

static s32 pick_hold, pick_on, pick_sel;   // the spirit picker (hold R or L)
static u32 pick_key;
static void update_player(void) {
    if (pick_on) { pstep = 0; return; }       // standing still while you pick
    s32 speed = HELD(KEY_B) ? 2 : 1;
    if (home_level[KIND_BREEZE] == 1) speed += HELD(KEY_B) ? 1 : (frame & 1);   // nook bonus: 1.5x walk, 3 run
    if (home_level[KIND_BREEZE] == 2) speed += 1;                              // level 2: 2x walk, 3 run
    u8 under = tile_at((px + 8) >> 4, (py + 12) >> 4);
    if (under == PATH || under == PEBBLE_PATH) speed += 1;                     // paths are quick
    if (swimming()) speed = HELD(KEY_B) ? 2 : 1;                               // swimming is slow
    s32 dx = 0, dy = 0;
    if (HELD(KEY_LEFT))  { dx = -speed; pdir = DIR_LEFT; }
    if (HELD(KEY_RIGHT)) { dx =  speed; pdir = DIR_RIGHT; }
    if (HELD(KEY_UP))    { dy = -speed; pdir = DIR_UP; }
    if (HELD(KEY_DOWN))  { dy =  speed; pdir = DIR_DOWN; }

    // If the player is ever inside something solid, let them walk out instead of being stuck.
    int moved = 0, stuck = on_solid(px, py);
    if (dx && (stuck ? in_world(px + dx, py) : !blocked_at(px + dx, py))) { px += dx; moved = 1; }
    if (dy && (stuck ? in_world(px, py + dy) : !blocked_at(px, py + dy))) { py += dy; moved = 1; }

    if (in_water && !water_tile(tile_at((px + 8) >> 4, (py + 12) >> 4))) in_water = 0;   // climbed out
    if (moved) {
        if ((frame & 7) == 0) pstep++;
        if (skill_lv() >= 1 && cur_area != HOLLOW_IN && party_count && is_you(spirits[party[active]].kind)) {
            s32 fx = (px + 8) >> 4, fy = (py + 12) >> 4;     // LEAF STEP (v0.17): dead ground greens underfoot
            u8 g = tile_at(fx, fy);
            if (g == DEAD || g == DEAD_CRACK || g == DIRT || g == DRY_GRASS || g == PEBBLES) {
                set_tile(fx, fy, (rng() & 3) ? GRASS : GRASS_TUFT);
                draw_life();
            }
        }
        hist_head = (hist_head + 1) & (HIST - 1);
        hist_x[hist_head] = px; hist_y[hist_head] = py;
    } else {
        pstep = 0;
    }
    check_exits();
}

static void update_spirits(void) {
    // Party members follow the player's trail, one behind the other. The active spirit walks
    // first; the others keep their party order behind it (they glide to new places on a swap).
    for (s32 p = 0; p < party_count; p++) {
        Spirit *s = &spirits[party[p]];
        s32 place = p == active ? 0 : (p < active ? p + 1 : p);
        s32 idx = (hist_head - 18 * (place + 1)) & (HIST - 1);
        s->x += (hist_x[idx] - s->x) / 4;
        s->y += (hist_y[idx] - s->y) / 4;
    }
    for (s32 i = 0; i < NUM_SPIRITS; i++)                 // the pouch goes where you go
        if (spirits[i].state == SP_POUCH) { spirits[i].area = cur_area; spirits[i].x = px; spirits[i].y = py; }
}

static void sparkle_at(s32 mx, s32 my) { sparkle_x = mx * 16; sparkle_y = my * 16; sparkle_life = 30; }

static const char *quest_ask[NUM_SPIRITS][2] = {
    { "SPROUT: PLANT 3 FLOWER", "BEDS BY MY GROVE?" },
    { "RIPPLE: A VAEL DAM UP NORTH", "IN THE STEPPE HOLDS MY RIVER." },
    { "PEBBLE: I LOST A STANDING", "STONE IN THE VALLEY..." },
    { "BREEZE: HELP ME CLEAR", "30 THORNS WITH MY GUSTS!" },
};
static const char *quest_reward[NUM_SPIRITS] = {
    "NEW BUILD: BLOSSOM TREE", "NEW BUILD: FOUNTAIN", "NEW BUILD: PEBBLE STATUE", "NEW BUILD: PINWHEEL",
};
static const char *grew_msg[NUM_SPIRITS] = {
    "SPROUT GREW STRONGER!", "RIPPLE GREW STRONGER!", "PEBBLE GREW STRONGER!", "BREEZE GREW STRONGER!",
    "BARK GREW STRONGER!", "LILY GREW STRONGER!", "FLICK GREW STRONGER!",   // v0.19: Lily's was missing
};
static const char *trust_msg[NUM_SPIRITS] = {
    "SPROUT TRUSTS YOU FULLY!", "RIPPLE TRUSTS YOU FULLY!", "PEBBLE TRUSTS YOU FULLY!", "BREEZE TRUSTS YOU FULLY!",
    "BARK TRUSTS YOU FULLY!", "LILY TRUSTS YOU FULLY!", "FLICK TRUSTS YOU FULLY!",
};

static void add_fp(s32 k, s32 n) {
    s32 before = hearts(k);
    s32 v = fp[k] + n;
    fp[k] = v > 100 ? 100 : v;
    s32 after = hearts(k);
    if (before < 2 && after >= 2 && quest[k] == 0 && k < NUM_HOMES) {   // Bark has no quest yet
        quest[k] = 1;
        queue_message(quest_ask[k][0]);
        queue_message(quest_ask[k][1]);
    }
    if (before < 3 && after >= 3) queue_message(grew_msg[k]);
    if (before < 5 && after >= 5) { queue_message(trust_msg[k]); queue_message("BOND IN THE SPIRITS MENU."); }
}

static void complete_quest(s32 k) {
    if (quest[k] != 1) return;
    quest[k] = 2;
    unlocked[k] = 1;
    sfx_play(SFX_QUEST);
    queue_message("QUEST DONE!");
    queue_message(quest_reward[k]);
    add_fp(k, 30);
}

// Bark grumbles every time he works: he was part of the Great Tree, and he wants to go back.
// Lines rotate per kind of job; a line only queues when no other messages are waiting.
static const char *bark_cut[][2] = {
    { "BARK: FINE. CUT IT DOWN.", "LIKE THEY CUT ME DOWN." },
    { "BARK: I WAS A GREAT TREE.", "TALLER THAN ALL OF THESE." },
    { "BARK: BIRDS NESTED IN ME.", "NOW LOOK AT ME. A STUMP." },
    { "BARK: THAT TREE NEVER HURT", "ANYONE. JUST SAYING." },
    { "BARK: ONE DAY I'LL GO HOME", "TO MY TREE. YOU'LL SEE." },
};
static const char *bark_stump[][2] = {
    { "BARK: A STUMP. LIKE ME.", "HOW DO YOU THINK I FEEL?" },
    { "BARK: I HATE BEING A STUMP.", "I HAD LEAVES ONCE. SO MANY." },
    { "BARK: I MISS MY DEEP ROOTS.", "YOU WOULDN'T GET IT." },
};
static const char *bark_dead[][2] = {
    { "BARK: POOR OLD THING.", "VAEL GOT IT TOO." },
    { "BARK: IT WAS GLORIOUS ONCE.", "LIKE ME. BEFORE VAEL." },
};
static void bark_grumble(s32 kind_of) {
    if (msgq_len || tree_alive) return;                   // healed, Bark is not grumbling at you
    const char *(*lines)[2] = kind_of == 0 ? bark_cut : kind_of == 1 ? bark_stump : bark_dead;
    u32 n = kind_of == 0 ? sizeof(bark_cut) / sizeof(bark_cut[0])
          : kind_of == 1 ? sizeof(bark_stump) / sizeof(bark_stump[0]) : sizeof(bark_dead) / sizeof(bark_dead[0]);
    u32 i = bark_line++ % n;
    queue_message(lines[i][0]);
    queue_message(lines[i][1]);
}

// Returns 1 if the power did something (and so costs a charge).
// v0.18: is the sludge pool at (mx, my) still fed by a Vael pipe? (A pipe touching any of its cells.)
static u8 sludge_seen[MAP_W * MAP_H / 8];
static int sludge_fed(s32 mx, s32 my) {
    static u16 stack[MAP_W * MAP_H];
    for (u32 i = 0; i < sizeof(sludge_seen); i++) sludge_seen[i] = 0;
    s32 n = 0;
    stack[n++] = my * MAP_W + mx;
    sludge_seen[stack[0] >> 3] |= 1 << (stack[0] & 7);
    while (n) {
        s32 c = stack[--n], x = c % MAP_W, y = c / MAP_W;
        static const signed char dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
        for (s32 k = 0; k < 4; k++) {
            s32 nx = x + dx[k], ny = y + dy[k];
            if (nx < 0 || ny < 0 || nx >= MAP_W || ny >= MAP_H) continue;
            u8 tt = tile_at(nx, ny);
            if (tt == VAEL_PIPE) return 1;
            s32 id = ny * MAP_W + nx;
            if (tt != SLUDGE || (sludge_seen[id >> 3] & (1 << (id & 7)))) continue;
            sludge_seen[id >> 3] |= 1 << (id & 7);
            stack[n++] = id;
        }
    }
    return 0;
}

static int use_power(s32 kind, s32 mx, s32 my) {
    u8 t = tile_at(mx, my);
    int in_map = mx >= 0 && my >= 0 && mx < MAP_W && my < MAP_H;
    if (!in_map) return 0;
    if (kind == KIND_SPROUT) {
        if (liquid(t) || is_rock(t) || t == BRIDGE || t == THORN || t == MENHIR || t == VAEL_PIPE) return 0;   // v0.18.2: not into water
        start_bloom(mx, my);
    } else if (kind == KIND_RIPPLE && t == DRY_POOL) {
        // Ripple fills the Great Tree's dried root pool, all of it at once
        for (s32 y = 0; y < MAP_H; y++)
            for (s32 x = 0; x < MAP_W; x++)
                if (tile_at(x, y) == DRY_POOL) { set_tile(x, y, WATER); sparkle_at(x, y); }
        sfx_play(SFX_BRIDGE);
    } else if (kind == KIND_RIPPLE) {
        if (t != WATER && t != DRY_RIVER) return 0;
        // 1 plank; 2 at 3 hearts; 3 when bonded.
        s32 sx = pdir == DIR_LEFT ? -1 : pdir == DIR_RIGHT ? 1 : 0;
        s32 sy = pdir == DIR_UP ? -1 : pdir == DIR_DOWN ? 1 : 0;
        s32 n = bonded == KIND_RIPPLE ? 3 : 1 + boosted(KIND_RIPPLE);
        for (s32 k = 0; k < n; k++, mx += sx, my += sy) {
            u8 w = tile_at(mx, my);
            if (w != WATER && w != DRY_RIVER) break;
            set_tile(mx, my, w == WATER ? BRIDGE : DRY_BRIDGE);
            sparkle_at(mx, my);
        }
        sfx_play(SFX_BRIDGE);
    } else if (kind == KIND_LILY) {
        // v0.18 Lily: sludge becomes clean water (a cross of five) unless a Vael pipe still feeds that
        // pool; on clean water she grows a lily pad you can walk on.
        if (t == SLUDGE) {
            if (sludge_fed(mx, my)) { show_message("THE PIPE KEEPS POURING SLUDGE!"); queue_message("PEBBLE CAN SMASH THE PIPE."); return 0; }
            static const signed char cx[5] = { 0, 1, -1, 0, 0 }, cy[5] = { 0, 0, 0, 1, -1 };
            for (s32 k = 0; k < 5; k++)
                if (tile_at(mx + cx[k], my + cy[k]) == SLUDGE) { set_tile(mx + cx[k], my + cy[k], BOG_WATER); sparkle_at(mx + cx[k], my + cy[k]); }
            sfx_play(SFX_BRIDGE);
        } else if (t == WATER || is_bog(t)) {
            set_tile(mx, my, t == WATER ? LILY_PAD : BOG_PAD);
            sparkle_at(mx, my);
            sfx_play(SFX_BLOOM);
        } else return 0;
    } else if (kind == KIND_FLICK) {
        // v0.19 Flick lights the old lanterns and campfires: they stay lit, and light the dark round them
        if (t == LANTERN_COLD) { set_tile(mx, my, LANTERN_LIT); show_message("THE LANTERN GLOWS!"); }
        else if (t == CAMPFIRE_COLD) { set_tile(mx, my, CAMPFIRE_LIT); show_message("THE CAMPFIRE CRACKLES!"); }
        else return 0;
        sparkle_at(mx, my);
        sfx_play(SFX_WAKE);
        if (cur_area == ORCHARD) dark_enter(ORCHARD);   // the new light (all of the dark is redrawn)
    } else if (kind == KIND_STONE && t == SMOKE_STACK) {
        set_tile(mx, my, DEAD);                // v0.19.4: a Vael smoke stack falls; its part of the orchard clears
        restorable[work_area]++;
        stone += 2;
        sparkle_at(mx, my);
        sfx_play(SFX_PUMP_OFF);
        show_message("THE SMOKE STACK FALLS! +2 STONE");
        s32 left = 0;
        for (s32 k = 0; k < NUM_STACKS; k++) left += maps[ORCHARD][stack_y[k] * MAP_W + stack_x[k]] == SMOKE_STACK;
        queue_message(left ? "THE SMOKE HERE THINS AWAY." : "THE LAST OF THE SMOKE CLEARS!");
        if (cur_area == ORCHARD) { dark_enter(ORCHARD); smoke_fade(); }
    } else if (kind == KIND_STONE && t == VAEL_PIPE) {
        set_tile(mx, my, DEAD);                // a Vael pipe cracks: its sludge stops coming
        restorable[work_area]++;
        stone += 2;
        sparkle_at(mx, my);
        show_message("THE PIPE BREAKS! +2 STONE");
        sfx_play(SFX_PUMP_OFF);
    } else if (kind == KIND_STONE && t == CLAMP) {
        set_tile(mx, my, DEAD);                // a Vael clamp cracks off the Great Tree's root
        stone += 2;
        sparkle_at(mx, my);
        show_message("+2 STONE");
        sfx_play(SFX_ROCK);
    } else if (kind == KIND_STONE) {
        if (!is_rock(t)) return 0;
        // Broken rock leaves bare ground that can bloom. With Pebble's cairn, two rocks in a line break.
        s32 sx = pdir == DIR_LEFT ? -1 : pdir == DIR_RIGHT ? 1 : 0;
        s32 sy = pdir == DIR_UP ? -1 : pdir == DIR_DOWN ? 1 : 0;
        s32 reach = 1 + home_level[KIND_STONE] + boosted(KIND_STONE) + (bonded == KIND_STONE ? 2 : 0);
        for (s32 k = 0; k < reach && is_rock(tile_at(mx, my)); k++, mx += sx, my += sy) {
            if (mx < 0 || my < 0 || mx >= MAP_W || my >= MAP_H) break;
            set_tile(mx, my, tile_at(mx, my) == ROCK_GRASS ? GRASS : DIRT);   // grass stays grass
            restorable[work_area]++;
            stone++;
            sparkle_at(mx, my);
        }
        draw_life();
        sfx_play(SFX_ROCK);
    } else if (kind == KIND_BARK) {
        // Bark cuts a living tree to a stump (+1 wood), clears stumps, and fells dead trees (+1 wood).
        s32 kind_of = t == TREE ? 0 : t == DEAD_TREE ? 2 : 1;   // a living tree, a stump, a dead tree
        if (t == TREE) { set_tile(mx, my, STUMP_GRASS); wood++; show_message("+1 WOOD"); }
        else if (t == STUMP_GRASS) { set_tile(mx, my, GRASS); show_message("STUMP CLEARED"); }
        else if (t == STUMP) { set_tile(mx, my, DIRT); show_message("STUMP CLEARED"); }
        else if (t == DEAD_TREE) { set_tile(mx, my, DIRT); wood++; show_message("+1 WOOD"); }
        else if (t == REEDS) { set_tile(mx, my, DEAD); restorable[work_area]++; show_message("REEDS CLEARED"); }   // v0.18
        else if (t == LOG) { set_tile(mx, my, MOSS); restorable[work_area]++; wood++; show_message("+1 WOOD"); }
        else if (t == BURNT_LOG && !tree_alive) { show_message("BARK: HMPH. TOO BIG FOR ME."); return 0; }
        else if (t == BURNT_LOG || t == BURNT_TREE) {   // v0.19: the orchard's fallen trunk (YOU) and burnt trees
            set_tile(mx, my, DEAD); restorable[work_area]++; wood++; show_message("+1 WOOD");
            if (t == BURNT_LOG && !(flick_flags & 1)) {   // deep in the trunk, still glowing: it will warm Flick
                flick_flags |= 1;
                queue_message("A WARM EMBER FALLS OUT!");
                queue_message("YOU TUCK IT IN YOUR POCKET.");
            }
        }
        else if (t == SWAMP_TREE || t == SWAMP_TREE_ALIVE || t == SWAMP_TOP || t == SWAMP_TOP_ALIVE) {
            s32 foot = (t == SWAMP_TOP || t == SWAMP_TOP_ALIVE) ? my + 1 : my;   // the whole tree comes down
            set_tile(mx, foot, MOSS);
            if (foot > 0 && (tile_at(mx, foot - 1) == SWAMP_TOP || tile_at(mx, foot - 1) == SWAMP_TOP_ALIVE)) set_tile(mx, foot - 1, MOSS);
            wood++; show_message("+1 WOOD");
        }
        else return 0;
        sparkle_at(mx, my);
        draw_life();
        sfx_play(SFX_REMOVE);
        bark_grumble(kind_of);
    } else {
        // A gust clears up to three thorn tiles in a line.
        if (t != THORN) return 0;
        s32 sx = pdir == DIR_LEFT ? -1 : pdir == DIR_RIGHT ? 1 : 0;
        s32 sy = pdir == DIR_UP ? -1 : pdir == DIR_DOWN ? 1 : 0;
        s32 n = 3 + 2 * boosted(KIND_BREEZE) + (bonded == KIND_BREEZE ? 3 : 0);
        for (s32 k = 0; k < n && tile_at(mx, my) == THORN; k++, mx += sx, my += sy) {
            set_tile(mx, my, DEAD);
            restorable[work_area]++;
            sparkle_at(mx, my);
            if (quest[KIND_BREEZE] == 1 && ++thorn_quest >= 30) complete_quest(KIND_BREEZE);
        }
        draw_life();
        sfx_play(SFX_GUST);
    }
    return 1;
}

// ---------- materials, chests, lore ----------
static char num_msg[32];

static u32 count_set(const u8 *a, s32 n) { u32 c = 0; for (s32 i = 0; i < n; i++) c += a[i] != 0; return c; }

// "+1 FRUIT" style messages; numbers stay small (0..999).
static char *fmt_num(char *p, u32 v) {
    if (v >= 100) *p++ = '0' + v / 100 % 10;
    if (v >= 10) *p++ = '0' + v / 10 % 10;
    *p++ = '0' + v % 10;
    return p;
}
static void str_cat(char **p, const char *s) { while (*s) *(*p)++ = *s++; }

static const char *lore[NUM_WRECKS][3] = {
    { "A RUSTED VAEL PUMP.", "IT DRANK THE RIVER DOWN", "UNTIL THE VALLEY WENT GRAY." },
    { "A FALLEN VAEL WIND TOWER.", "VAEL PUSHED THE RAIN CLOUDS", "AWAY TO SELL THEM NORTH." },
    { "A COLD VAEL DRILL.", "IT DUG FOR WATER BELOW", "AND THE SPRINGS WENT SILENT." },
};
static const char *chest_reward[NUM_CHESTS] = {
    "+5 SEEDS AND +3 FRUIT!", "EVERY SPIRIT: +1 MAX CHARGE!", "HOMES NOW PULSE TWICE AS FAST!",
    "+6 STONE FOR BUILDING!",
};

static int find_at(const u8 *ax, const u8 *ay, const u8 *aa, s32 n, s32 mx, s32 my) {
    for (s32 i = 0; i < n; i++)
        if (aa[i] == cur_area && ax[i] == mx && ay[i] == my) return i;
    return -1;
}

static void open_chest(s32 i, s32 mx, s32 my) {
    chest_open[i] = 1;
    set_tile(mx, my, tile_at(mx, my) == CHEST_GRASS ? CHEST_OPEN_GRASS : CHEST_OPEN);
    if (i == 0) { seeds += 5; fruit += 3; }
    if (i == 1) {
        bonus_charge = 1;
        for (s32 k = 0; k < NUM_SPIRITS; k++) if (spirits[k].state != SP_HIDDEN) spirits[k].energy++;
        draw_active();
    }
    if (i == 2) pulse_div = 2;
    if (i == 3) stone += 6;
    sfx_play(SFX_CHEST);
    sparkle_at(mx, my);
    show_message("YOU FOUND A RELIC!");
    queue_message(chest_reward[i]);
}

static void read_wreck(s32 i) {
    if (!wreck_read[i]) { wreck_read[i] = 1; chime(330, 10); }
    show_message(lore[i][0]);
    queue_message(lore[i][1]);
    queue_message(lore[i][2]);
    char *p = num_msg;
    str_cat(&p, "LORE FOUND: ");
    p = fmt_num(p, count_set(wreck_read, NUM_WRECKS));
    str_cat(&p, " OF ");
    p = fmt_num(p, NUM_WRECKS);
    *p = 0;
    queue_message(num_msg);
}

// A on a chest, a pump, a fruit tree, or flowers does that instead of a spirit power.
// Returns 1 if it handled the press.
static void open_cook(void);
static int interact(s32 mx, s32 my) {
    u8 t = tile_at(mx, my);
    s32 i;
    if (t == CAMPFIRE_LIT || (t == LOG_SEAT && (tile_at(mx - 1, my) == CAMPFIRE_LIT || tile_at(mx + 1, my) == CAMPFIRE_LIT))) {
        open_cook(); return 1;                        // v0.19: sit by the fire and cook
    }
    if (t == CAMPFIRE_COLD && !(party_count && spirits[party[active]].kind == KIND_FLICK)) {   // Flick lights it
        show_message("A COLD CAMPFIRE.");
        if (spirits[KIND_FLICK].state == SP_JOINED || spirits[KIND_FLICK].state == SP_POUCH) queue_message("FLICK COULD LIGHT IT.");
        return 1;
    }
    if (t == MUSHROOMS) {                            // v0.19: marsh mushrooms, for cooking
        set_tile(mx, my, MOSS);
        mushrooms++;
        sfx_play(SFX_GIFT);
        show_message("+1 MUSHROOM");
        return 1;
    }
    if ((t == CHEST || t == CHEST_GRASS) && (i = find_at(chest_x, chest_y, chest_area, NUM_CHESTS, mx, my)) >= 0) {
        open_chest(i, mx, my); return 1;
    }
    if ((t == WRECK || t == WRECK_GRASS) && (i = find_at(wreck_x, wreck_y, wreck_area, NUM_WRECKS, mx, my)) >= 0) {
        read_wreck(i); return 1;
    }
    if (t == VAEL_PUMP) {
        if (!party_count) { show_message("IT'S TOO STRONG ALONE."); return 1; }
        Spirit *sp = &spirits[party[active]];
        if (sp->energy == 0) { sfx_play(SFX_FAIL); show_message(spirit_tired[sp->kind]); return 1; }
        sp->energy--;
        draw_active();
        set_tile(mx, my, DEAD);
        pump_on[cur_area] = 0;
        stone += 3;
        add_fp(sp->kind, 5);
        sfx_play(SFX_PUMP_OFF);
        sparkle_at(mx, my);
        show_message("YOU SHUT DOWN THE PUMP!");
        queue_message("+3 STONE OF SCRAP");
        if (pumps_stopped++ == 0) queue_message("VAEL STILL WANTS OUR WATER.");
        return 1;
    }
    if (t == LEVER) {
        if (quest[KIND_RIPPLE] != 1) { show_message("A RUSTED VAEL LEVER."); queue_message("IT IS STUCK FAST."); return 1; }
        if (!in_party(KIND_RIPPLE)) { show_message("IT WON'T MOVE. RIPPLE"); queue_message("SHOULD BE HERE TO HELP."); return 1; }
        // Ripple pushes the water through: the dam opens and the whole steppe river flows again.
        set_tile(mx, my, LEVER_PULLED);
        for (s32 i = 0; i < MAP_W * MAP_H; i++) {
            u8 w = world[i];
            if (w == DRY_RIVER || w == DAM) set_tile(i % MAP_W, i / MAP_W, WATER);
            else if (w == DRY_BRIDGE) set_tile(i % MAP_W, i / MAP_W, BRIDGE);
        }
        chime(330, 14);
        show_message("THE DAM OPENS!");
        queue_message("THE RIVER FLOWS AGAIN!");
        complete_quest(KIND_RIPPLE);
        return 1;
    }
    if (t == LOST_STONE) {
        if (quest[KIND_STONE] != 1) { show_message("A SMALL STANDING STONE."); queue_message("IT HUMS SOFTLY."); return 1; }
        set_tile(mx, my, MENHIR);
        sparkle_at(mx, my);
        show_message("PEBBLE'S LOST STONE!");
        complete_quest(KIND_STONE);
        return 1;
    }
    if (tree_alive && cur_area == WEST && is_trunk(t)) {   // talk to Bark, in his tree
        static const char *tree_talk[][2] = {
            { "BARK: STILL HERE. STILL HUGE.", "DON'T GET USED TO NICE ME." },
            { "BARK: MIND MY ROOTS, WIZARD.", "THEY TICKLE." },
            { "BARK: THE LEAFEN. MY FRIENDS.", "VAEL TOOK THEM. FIND THEM." },
            { "BARK: ...THANKS. AGAIN.", "HMPH. NOW GO." },
        };
        static u32 talk;
        u32 i = talk++ % 4;
        show_message(tree_talk[i][0]);
        queue_message(tree_talk[i][1]);
        return 1;
    }
    u32 bit = my * MAP_W + mx;
    int bark = party_count && spirits[party[active]].kind == KIND_BARK;   // with Bark, A cuts trees instead
    if (t == TREE && !bark && !(picked[cur_area][bit >> 3] & (1 << (bit & 7)))) {
        picked[cur_area][bit >> 3] |= 1 << (bit & 7);
        fruit++;
        sfx_play(SFX_GIFT);
        sparkle_at(mx, my);
        show_message("+1 FRUIT");
        return 1;
    }
    if (t == FLOWERBED) {
        set_tile(mx, my, FLOWERBED_EMPTY);
        seeds++;
        sfx_play(SFX_GIFT);
        show_message("+1 SEED");
        return 1;
    }
    if (t == FRUIT_TREE) {
        set_tile(mx, my, FRUIT_TREE_BARE);
        fruit++;
        sfx_play(SFX_GIFT);
        sparkle_at(mx, my);
        show_message("+1 FRUIT");
        return 1;
    }
    if (t == BENCH) {
        for (s32 p = 0; p < party_count; p++) spirits[party[p]].energy = energy_max(&spirits[party[p]]);
        hearts_now = hearts_max();
        draw_active();
        draw_hearts_hud();
        sfx_play(SFX_JOIN);
        show_message("YOU REST. CHARGES REFILLED.");
        return 1;
    }
    if (t == FLOWERS) {
        set_tile(mx, my, GRASS);
        seeds++;
        sfx_play(SFX_GIFT);
        show_message("+1 SEED");
        return 1;
    }
    return 0;
}

// ---------- combat (v0.15): the Great Tree's region only ----------
// The wizard has hearts. A attacks with the active spirit when an enemy is near in front (else A
// uses its power as before); a spirit fights once its home is level 2 (Bark always). Attacks cost
// nothing but have a cooldown. Blight blobs and Vael drones roam the west; each of Bark's four
// tasks wakes a mini boss that takes 3x damage from one spirit. At 0 hearts the wizard faints and
// wakes at the nearest home with nothing lost; the boss there starts over.
#define SPR_COMBAT (TILE_COMBAT / 4)
enum { C_WHIP_H, C_WHIP_V, C_WATER, C_ROCK, C_GUST, C_SLAM, C_BLOB, C_DRONE = C_BLOB + 2, C_SPARK = C_DRONE + 2,
       C_LEAF, C_SPORE, C_MOTE, C_RINGDOT, C_SHADOW, C_STARS, C_GLINT, C_BUBBLE, C_SPLASH, C_PETAL };
// v0.17 the Leafen's own sheet (leafen_sprite.png): 5 looks x 4 poses, 6 tiny homes, the shiny pebble.
// They use OBJ bank 11 (the clamps' palette until the tree is healed; no clamp shows after that).
#define SPR_LEAFEN (TILE_LEAFEN / 4)
#define PAL_LEAFEN 11
enum { C_LEAFEN = 0, C_LHOME = 5 * 4, C_SHINY = C_LHOME + 6, C_PEEK, C_FIREFLY, C_SWIMRING, C_ORB = C_SWIMRING + 2,
       C_WAKE = C_ORB + 6, C_MINI = C_WAKE + 3 };   // C_WAKE: sideways, moving down, moving up; C_MINI: pouch orbs
static int in_combat_area(void) { return ((cur_area == WEST || cur_area == HOLLOW_IN) && !tree_alive) || cur_area == MARSH ||
                                         cur_area == ORCHARD; }

static void draw_hearts_hud(void) {
    for (s32 i = 0; i < MAX_HEARTS + 1; i++)        // the sixth only while a stew lasts (v0.19)
        SCREENBLOCK(TEXT_SB)[11 + i] = glyph_entry(in_combat_area() && (u32)i < hearts_max() ? ((u32)i < hearts_now ? '*' : '@') : ' ');
}

// Enemies: blobs creep after you, drones dart at you through the air.
enum { E_BLOB, E_DRONE, E_SMOKE };   // E_SMOKE (v0.19): the orchard's smoke creatures
typedef struct { s32 type, x, y, hp, timer, dx, dy, flash, knock, alive; } Enemy;
#define MAX_ENEMIES 8
static Enemy enemies[MAX_ENEMIES];
static const u8 enemy_spawn[][3] = {           // type, tile x, tile y
    { E_BLOB, 4, 6 }, { E_BLOB, 27, 5 }, { E_BLOB, 3, 27 }, { E_BLOB, 28, 26 },
    { E_DRONE, 3, 1 }, { E_DRONE, 28, 1 },   // v0.18.2: two drones, not three (Caleb: too annoying)
};
#define NUM_SPAWNS (sizeof(enemy_spawn) / sizeof(enemy_spawn[0]))

// One attack at a time: a box that moves (shots) or stays by the wizard (whip, slam).
typedef struct { s32 kind, x, y, dx, dy, life, w, h, dmg, push, hit; } Attack;
static Attack atk;
static u32 atk_cooldown, hit_x, hit_y, hit_life;

// Mini bosses: one per task, in task order. weak: the spirit that deals 3x damage.
enum { B_CLAMPER, B_THORN, B_MUD, B_ROT, NUM_BOSSES, B_GLOOM = NUM_BOSSES };
static const char *boss_name[NUM_BOSSES + 1] = { "IRONJAW", "BRAMBLEMAW", "THE DREDGE", "ROTCOIL", "GLOOMHEART" };
// Gloomheart (v0.16), in the hollow: five shields in turn, one per spirit (Sprout, Ripple, Pebble,
// Breeze, Bark); only that spirit's attacks crack it (3 hits). Then its heart is open.
#define GLOOM_SHIELD_HITS 3
static s32 gloom_phase, gloom_shield, gloom_wrong_told;
// v0.16.1 (Caleb: the fight was static and gave no warning): it glides between spots in the room and
// takes turns at three attacks, each with a clear warning. Volley: it shakes while motes gather in,
// then fires spores. Ring: dots blink where the ring will reach, then the ring spreads out. Dive: it
// rises, its shadow chases you, it crashes down there and lies dazed (the window for melee spirits).
enum { G_MOVE, G_VOLLEY, G_RING, G_DIVE, G_DAZED };
#define MAX_SPORES 8
#define MAX_MOTES 16
#define RING_DOTS 16
#define RING_REACH 72
typedef struct { s32 x, y, dx, dy, life, c; } Dot;   // x, y, dx, dy in 1/16 px; c: the sprite cell (motes)
static Dot spores[MAX_SPORES], motes[MAX_MOTES];
static void gloom_burst(s32 x, s32 y, s32 speed, s32 life);
// v0.16.7 (Caleb): entering the hollow is a scene first: the Gloomheart roars and shakes the tree,
// and Bark tells you to squash it. The second time (after fainting) it is short.
static u32 gloom_intro, gloom_intro_wait, gloom_intro_seen;
static s32 gloom_act, gloom_step, gloom_tx, gloom_ty, gloom_ring, gloom_ring_hit, gloom_shake, gloom_told_daze;
static const u16 shield_color[NUM_SHIELDS][2] = {   // dark, light
    { RGB5(40, 120, 40), RGB5(140, 230, 100) }, { RGB5(30, 80, 190), RGB5(130, 200, 250) },
    { RGB5(100, 92, 84), RGB5(200, 190, 170) }, { RGB5(150, 190, 210), RGB5(240, 250, 255) },
    { RGB5(110, 70, 40), RGB5(200, 150, 90) },
};
static void gloom_shield_color(void) {
    if (gloom_phase >= NUM_SHIELDS) return;
    PAL_OBJ[PAL_SHIELD * 16 + 1] = RGB5(20, 16, 20);
    PAL_OBJ[PAL_SHIELD * 16 + 2] = shield_color[gloom_phase][0];
    PAL_OBJ[PAL_SHIELD * 16 + 3] = shield_color[gloom_phase][1];
}
static const u8 boss_weak[NUM_BOSSES] = { KIND_STONE, KIND_BREEZE, KIND_RIPPLE, KIND_SPROUT };
static const u16 boss_home[NUM_BOSSES][2] = { { 48, 340 }, { 240, 372 }, { 224, 440 }, { 352, 440 } };
static const char *boss_intro[NUM_BOSSES][2] = {
    { "VAEL'S IRONJAW WAKES UP!", "BARK: PEBBLE'LL CRACK IT." },
    { "A BRAMBLEMAW BURSTS OUT!", "BARK: BREEZE. BLOW IT AWAY." },
    { "THE DREDGE RISES FROM MUD!", "BARK: RIPPLE. WASH IT OFF." },
    { "A ROTCOIL CRAWLS UP!", "BARK: SPROUT. ROOT IT OUT." },
};
#define BOSS_HP 9   // v0.15.1: was 12 (Caleb: too hard)
typedef struct { s32 kind, x, y, hp, timer, state, dx, dy, flash, alive, hop; } Boss;
static Boss boss;

static int open_at(s32 x, s32 y) {             // a 12x12 body at (x+2, y+2) fits here
    s32 xs[2] = { x + 2, x + 13 }, ys[2] = { y + 2, y + 13 };
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++)
            if (is_solid(tile_at(xs[i] >> 4, ys[j] >> 4))) return 0;
    return in_world(x, y);
}

// v0.18: the marsh has a few of its own (light: three blobs in the mud, two drones over the sludge)
static const u8 marsh_spawn[][3] = { { E_BLOB, 10, 15 }, { E_BLOB, 21, 16 }, { E_BLOB, 12, 29 },
                                     { E_DRONE, 22, 11 } };   // v0.18.2: one drone
// v0.19 smoke creatures drifting in the orchard's dark, away from the lanes, the nest and the way in
static const u8 orchard_spawn[][3] = { { E_SMOKE, 8, 19 }, { E_SMOKE, 22, 14 }, { E_SMOKE, 26, 24 },
                                       { E_SMOKE, 12, 6 }, { E_SMOKE, 21, 4 }, { E_SMOKE, 6, 24 } };
static void spawn_enemies(void) {
    for (s32 i = 0; i < MAX_ENEMIES; i++) enemies[i].alive = 0;
    const u8 (*sp)[3] = cur_area == MARSH ? marsh_spawn : cur_area == ORCHARD ? orchard_spawn : enemy_spawn;
    u32 count = cur_area == MARSH ? sizeof(marsh_spawn) / sizeof(marsh_spawn[0]) :
                cur_area == ORCHARD ? sizeof(orchard_spawn) / sizeof(orchard_spawn[0]) : NUM_SPAWNS;
    for (u32 i = 0; i < count && i < MAX_ENEMIES; i++) {
        Enemy *e = &enemies[i];
        e->type = sp[i][0];
        e->x = sp[i][1] * 16; e->y = sp[i][2] * 16;
        e->hp = e->type == E_DRONE ? 2 : 3;
        e->timer = i * 37; e->dx = e->dy = 0; e->flash = e->knock = 0;
        e->alive = 1;
        if (e->type == E_SMOKE && stack_dark(e->x + 8, e->y + 8) == 0) e->alive = 0;   // not in the clear air
    }
}

static void boss_start(s32 k) {
    boss.kind = k;
    if (k < NUM_BOSSES) { boss.x = boss_home[k][0]; boss.y = boss_home[k][1]; }
    boss.hp = BOSS_HP; boss.timer = 0; boss.state = 0; boss.dx = boss.dy = 0; boss.flash = 0; boss.hop = 0;
    boss.alive = 1;
}

// Enter the west: enemies return; a boss whose task is done (and not beaten) waits there.
static void combat_enter(void) {
    hearts_now = hearts_max();
    draw_hearts_hud();
    boss.alive = 0;
    atk.life = 0;
    for (s32 i = 0; i < MAX_ENEMIES; i++) enemies[i].alive = 0;
    for (s32 i = 0; i < MAX_SPORES; i++) spores[i].life = 0;
    for (s32 i = 0; i < MAX_MOTES; i++) motes[i].life = 0;
    gloom_ring = 0;
    if (!in_combat_area()) return;
    if (cur_area == HOLLOW_IN) {                     // Gloomheart waits in the middle of the room
        boss_start(B_GLOOM);
        boss.x = 240; boss.y = 192; boss.hp = BOSS_HP;
        gloom_phase = 0; gloom_shield = GLOOM_SHIELD_HITS; gloom_wrong_told = 0;
        gloom_act = G_MOVE; gloom_step = 0; gloom_tx = 240; gloom_ty = 150; gloom_ring = 0; gloom_told_daze = 0;
        gloom_shield_color();
        gloom_intro = 1; gloom_intro_wait = 40;        // the fight waits for its roar and Bark (update_gloom_intro)
        return;
    }
    spawn_enemies();
}

// The nearest built home: wake up beside it, hearts full.
static void faint(void) {
    s32 best = -1, bd = 1 << 30;
    for (s32 k = 0; k < NUM_HOMES; k++) {
        if (!home_done[k]) continue;
        s32 d = home_site_area[k] == cur_area ? 0 : 1000 + k;   // homes in this area first
        if (d < bd) { bd = d; best = k; }
    }
    show_message("YOU FAINTED...");
    queue_message("YOU WAKE UP AT HOME.");
    hurt_timer = 90;
    meal_kind = MEAL_NONE; meal_timer = 0;             // fainting ends a meal
    if (best < 0) { hearts_now = MAX_HEARTS; return; }
    s32 hx = home_site_x[best], hy = home_site_y[best] + 1;
    static const signed char near_x[9] = { 0, -1, 1, 0, -1, 1, -2, 2, 0 }, near_y[9] = { 0, 0, 0, 1, 1, 1, 0, 0, 2 };
    for (s32 r = 0; r < 9; r++)                          // its doorstep, or the nearest open spot (v0.18.2: regrowth can fill it)
        if (enter_area(home_site_area[best], (hx + near_x[r]) * 16, (hy + near_y[r]) * 16 - 6)) break;
    unstick_player();
    hearts_now = MAX_HEARTS;
    draw_hearts_hud();
}

static void hurt(s32 n, s32 fromx, s32 fromy) {
    if (hurt_timer || dev_god) return;
    if (skill_lv() >= 2 && !shield_wait) {                    // LEAF SHIELD (v0.17): leaves take the hit
        shield_wait = 600;                                    // back after 10 s
        hurt_timer = 40;
        leaf_timer = 60;
        chime(1319, 10);
        show_message("THE LEAF SHIELD HELD!");
        return;
    }
    hearts_now = (u32)n >= hearts_now ? 0 : hearts_now - n;
    hurt_timer = 60;
    sfx_play(SFX_FAIL);
    s32 kx = px + 8 - fromx, ky = py + 8 - fromy;             // pushed away from the hit
    s32 sx = kx > 6 ? 12 : kx < -6 ? -12 : 0, sy = ky > 6 ? 12 : ky < -6 ? -12 : 0;
    if (!blocked_at(px + sx, py)) px += sx;
    if (!blocked_at(px, py + sy)) py += sy;
    draw_hearts_hud();
    if (hearts_now == 0) faint();
}

static int can_fight(s32 kind) { return kind == KIND_BARK || kind == KIND_LILY || kind == KIND_FLICK || home_level[kind] >= 2; }

// Is something to hit within reach, in the direction the wizard faces?
static int enemy_ahead(void) {
    if (!in_combat_area()) return 0;
    s32 cx = px + 8, cy = py + 8;
    s32 fx = pdir == DIR_LEFT ? -1 : pdir == DIR_RIGHT ? 1 : 0, fy = pdir == DIR_UP ? -1 : pdir == DIR_DOWN ? 1 : 0;
    for (s32 i = 0; i <= MAX_ENEMIES; i++) {
        s32 ex, ey;
        if (i < MAX_ENEMIES) { if (!enemies[i].alive) continue; ex = enemies[i].x + 8; ey = enemies[i].y + 8; }
        else { if (!boss.alive || boss.state == 3) continue; ex = boss.x + 16; ey = boss.y + 16; }
        s32 dx = ex - cx, dy = ey - cy;
        s32 reach = i < MAX_ENEMIES ? 72 : 96;
        if (dx * dx + dy * dy > reach * reach) continue;
        if (dx * fx + dy * fy >= -4) return 1;               // in front (or right beside)
    }
    return 0;
}

static void start_attack(s32 kind) {
    s32 fx = pdir == DIR_LEFT ? -1 : pdir == DIR_RIGHT ? 1 : 0, fy = pdir == DIR_UP ? -1 : pdir == DIR_DOWN ? 1 : 0;
    atk.kind = kind; atk.hit = 0; atk.dx = atk.dy = 0;
    atk.x = px + fx * 14; atk.y = py + fy * 14;
    switch (kind) {
    case KIND_SPROUT: atk.life = 10; atk.w = fx ? 24 : 28; atk.h = fx ? 28 : 24; atk.dmg = 1; atk.push = 8;
        if (fx < 0) atk.x -= 8;
        if (fy < 0) atk.y -= 8;
        if (fx) atk.y -= 6; else atk.x -= 6;               // a wide lash (v0.16: it missed moving targets)
        sfx_play(SFX_REMOVE); break;
    case KIND_RIPPLE: atk.life = 26; atk.w = atk.h = 10; atk.dmg = 1; atk.push = 6; atk.dx = fx * 4; atk.dy = fy * 4;
        sfx_play(SFX_BRIDGE); break;
    case KIND_STONE: atk.life = 24; atk.w = atk.h = 12; atk.dmg = 2; atk.push = 10; atk.dx = fx * 3; atk.dy = fy * 3;
        sfx_play(SFX_ROCK); break;
    case KIND_BREEZE: atk.life = 28; atk.w = atk.h = 16; atk.dmg = 1; atk.push = 28; atk.dx = fx * 3; atk.dy = fy * 3;
        sfx_play(SFX_GUST); break;
    case KIND_LILY: atk.life = 20; atk.w = atk.h = 12; atk.dmg = 1; atk.push = 6; atk.dx = fx * 3; atk.dy = fy * 3;
        chime(1319, 6); break;                            // v0.18.2: a spinning lily blossom
    case KIND_FLICK: atk.life = 22; atk.w = atk.h = 10; atk.dmg = 1; atk.push = 6; atk.dx = fx * 3; atk.dy = fy * 3;
        chime(988, 5); break;                             // v0.19: an ember shot
    default: atk.life = 14; atk.x = px - 16; atk.y = py - 16; atk.w = atk.h = 48; atk.dmg = 2; atk.push = 14;
        sfx_play(SFX_BUILD); break;                       // Bark slams the ground all around
    }
    // A shot starts at the wizard and its first move takes it to its spot in front: a target
    // right against him is still hit (v0.16; shots used to start past it).
    if (atk.dx || atk.dy) { atk.x = px + 3 - atk.dx; atk.y = py + 3 - atk.dy; }
    atk_cooldown = 24;
}

static int boxes_meet(s32 ax, s32 ay, s32 aw, s32 ah, s32 bx, s32 by, s32 bw, s32 bh) {
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

static void spark(s32 x, s32 y) { hit_x = x; hit_y = y; hit_life = 10; }
static void open_door(void);   // the Great Tree's opening, once all four mini bosses are beaten
// a circle of radius 24 (x) by 16 (y) in 48 steps, for the leaves
static const signed char loop_x48[48] = { 24, 24, 23, 22, 21, 19, 17, 15, 12, 9, 6, 3, 0, -3, -6, -9, -12, -15, -17,
    -19, -21, -22, -23, -24, -24, -24, -23, -22, -21, -19, -17, -15, -12, -9, -6, -3, 0, 3, 6, 9, 12, 15, 17, 19, 21,
    22, 23, 24 };
static const signed char loop_y48[48] = { 0, 2, 4, 6, 8, 9, 11, 12, 14, 15, 15, 16, 16, 16, 15, 15, 14, 12, 11, 9,
    8, 6, 4, 2, 0, -2, -4, -6, -8, -9, -11, -12, -14, -15, -15, -16, -16, -16, -15, -15, -14, -12, -11, -9, -8, -6,
    -4, -2 };

static void update_attack(void) {
    if (atk_cooldown) atk_cooldown--;
    if (!atk.life) return;
    atk.life--;
    atk.x += atk.dx; atk.y += atk.dy;
    if (atk.dx || atk.dy) {                                  // shots stop at walls and the trunk
        u8 t = tile_at((atk.x + atk.w / 2) >> 4, (atk.y + atk.h / 2) >> 4);
        if (is_solid(t) && t != WATER && t != THORN) { atk.life = 0; return; }
    }
    s32 fx = atk.dx > 0 ? 1 : atk.dx < 0 ? -1 : pdir == DIR_RIGHT ? 1 : pdir == DIR_LEFT ? -1 : 0;
    s32 fy = atk.dy > 0 ? 1 : atk.dy < 0 ? -1 : pdir == DIR_DOWN ? 1 : pdir == DIR_UP ? -1 : 0;
    for (s32 i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &enemies[i];
        if (!e->alive || (atk.hit & (1 << i)) || !boxes_meet(atk.x, atk.y, atk.w, atk.h, e->x + 2, e->y + 2, 12, 12)) continue;
        atk.hit |= 1 << i;
        e->hp -= atk.dmg * (e->type == E_SMOKE && atk.kind == KIND_FLICK ? 2 : 1);   // embers burn smoke away
        e->flash = 8;
        s32 kx = atk.kind == KIND_BARK ? (e->x > px ? 1 : -1) : fx, ky = atk.kind == KIND_BARK ? (e->y > py ? 1 : -1) : fy;
        e->dx = kx; e->dy = ky; e->knock = atk.push / 2;
        spark(e->x, e->y);
        chime(880, 8);
        if (e->hp <= 0) { e->alive = 0; sfx_play(SFX_REMOVE); }
        if (atk.dx || atk.dy) atk.life = 0;                  // a shot stops at what it hits
    }
    if (boss.alive && boss.state != 3 && boss.hop < 8 && !(atk.hit & (1 << 30)) &&
        boxes_meet(atk.x, atk.y, atk.w, atk.h, boss.x + 3, boss.y + 3, 26, 26)) {
        atk.hit |= 1 << 30;
        if (atk.dx || atk.dy) atk.life = 0;
        if (boss.kind == B_GLOOM && gloom_phase < NUM_SHIELDS) {   // a shield: only its spirit cracks it
            if (atk.kind != gloom_phase) {
                chime(196, 10);
                if (!gloom_wrong_told) { gloom_wrong_told = 1; show_message("ONLY THAT COLOR'S SPIRIT"); queue_message("CAN CRACK THE SHIELD!"); }
                return;
            }
            boss.flash = 10;
            spark(boss.x + 8, boss.y + 8);
            chime(1175, 10);
            if (--gloom_shield <= 0) {
                gloom_burst(boss.x + 16, boss.y + 16, 32, 24);   // shards fly off
                gloom_phase++;
                gloom_shield = GLOOM_SHIELD_HITS;
                gloom_wrong_told = 0;
                sfx_play(SFX_ROCK);
                if (gloom_phase < NUM_SHIELDS) {
                    gloom_shield_color();
                    show_message("THE SHIELD SHATTERS!");
                    queue_message(spirit_skill[gloom_phase]);
                } else {
                    show_message("ITS HEART IS OPEN!");
                    queue_message("HIT IT WITH EVERYTHING!");
                }
            }
            return;
        }
        s32 dmg = boss.kind == B_GLOOM ? atk.dmg : atk.dmg * (atk.kind == boss_weak[boss.kind] ? 3 : 1);
        boss.hp -= dmg;
        boss.flash = 10;
        spark(boss.x + 8, boss.y + 8);
        chime(boss.kind == B_GLOOM || atk.kind == boss_weak[boss.kind] ? 1175 : 660, 10);
        if (boss.hp <= 0 && boss.kind == B_GLOOM) {
            boss.alive = 0;
            for (s32 i = 0; i < MAX_SPORES; i++) spores[i].life = 0;
            for (s32 i = 0; i < MAX_ENEMIES; i++) enemies[i].alive = 0;   // its blobs die with it
            gloom_ring = 0;
            gloom_burst(boss.x + 16, boss.y + 16, 32, 30);
            music_play(SONG_WIN_WARM);                       // the victory jingle (plays once)
            sfx_play(SFX_QUEST);
            spark(boss.x + 8, boss.y + 8);
            revive_step = 1;                                  // the tree comes back (update_revive)
            revive_wait = 30;
            return;
        }
        if (boss.hp <= 0) {
            boss.alive = 0;
            boss_beaten |= 1 << boss.kind;
            sfx_play(SFX_QUEST);
            spark(boss.x + 8, boss.y + 8);
            show_message(boss_name[boss.kind]);
            queue_message("IS BEATEN!");
            if (boss_beaten == (1 << NUM_BOSSES) - 1) {
                queue_message("BARK: HUH. THE OUTSIDE'S");
                queue_message("CLEAN. BUT THE ROT... IT'S");
                queue_message("IN MY HEART. IN THE HOLLOW.");
                open_door();
                queue_message("THE WAY INTO THE TREE OPENS.");
                omen_step = 1;                                // a quiet, open jingle, then the tree shakes
                music_play(SONG_OMEN);
            }
        }
    }
}

static void move_enemy(Enemy *e, s32 dx, s32 dy, int flies) {
    if (flies) { if (in_world(e->x + dx, e->y)) e->x += dx; if (in_world(e->x, e->y + dy)) e->y += dy; return; }
    if (dx && open_at(e->x + dx, e->y)) e->x += dx;
    if (dy && open_at(e->x, e->y + dy)) e->y += dy;
}

static s32 sgn(s32 v) { return v > 0 ? 1 : v < 0 ? -1 : 0; }
static u32 dx_flip(s32 dx) { return dx < 0; }

static void update_enemies(void) {
    if (!in_combat_area()) return;
    s32 pcx = px + 8, pcy = py + 8;
    for (s32 i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &enemies[i];
        if (!e->alive) continue;
        if (e->flash) e->flash--;
        e->timer++;
        if (e->knock) { e->knock--; move_enemy(e, e->dx * 2, e->dy * 2, e->type != E_BLOB); continue; }
        s32 dx = pcx - (e->x + 8), dy = pcy - (e->y + 8);
        s32 near = dx * dx + dy * dy < 100 * 100;
        if (e->type == E_SMOKE) {                            // v0.19: drifts in the dark; comes for you once it sees
            s32 cx = e->x + 8, cy = e->y + 8, mx = 0, my = 0, lit = fire_lit(cx, cy);   // your light; shuns fire
            s32 see = dark_r() + 40;
            if (lit != -1) {                                 // caught in a lantern's or fire's light: away from it
                mx = lit >= 0 ? sgn(cx - light_x[lit]) : 0; my = lit >= 0 ? sgn(cy - light_y[lit]) : -1;
                if (!mx && !my) my = -1;
            } else if (dx * dx + dy * dy < see * see) {      // it sees you: after you, a little slower than a walk
                if (e->timer & 3) { mx = sgn(dx); my = sgn(dy); }
            } else {                                         // wandering slowly
                if ((e->timer & 127) == 0) { e->dx = (s32)(rng() % 3) - 1; e->dy = (s32)(rng() % 3) - 1; }
                if (e->timer & 1) { mx = e->dx; my = e->dy; }
            }
            if (lit == -1) {                                 // never into the light
                if (mx && fire_lit(cx + mx * 8, cy) != -1) mx = 0;
                if (my && fire_lit(cx, cy + my * 8) != -1) my = 0;
            }
            move_enemy(e, mx, my, 1);
        } else if (e->type == E_BLOB) {                      // creeps after you, slowly
            if (e->timer & 1) continue;
            if (near) move_enemy(e, sgn(dx), sgn(dy), 0);
            else {
                if ((e->timer & 127) == 0) { e->dx = (s32)(rng() % 3) - 1; e->dy = (s32)(rng() % 3) - 1; }
                move_enemy(e, e->dx, e->dy, 0);
            }
        } else {                                             // darts at you, rests, darts again
            u32 t = e->timer % 160;                          // v0.18.2: darts less often, and shorter
            if (t == 0) { e->dx = near ? sgn(dx) : (s32)(rng() % 3) - 1; e->dy = near ? sgn(dy) : (s32)(rng() % 3) - 1; }
            if (t < 22) move_enemy(e, e->dx * 2, e->dy * 2, 1);
            else if (t > 100 && (e->timer & 3) == 0) move_enemy(e, e->dx, e->dy, 1);   // a slow drift between
        }
        if (boxes_meet(px + 3, py + 4, 10, 12, e->x + 3, e->y + 3, 10, 10)) hurt(1, e->x + 8, e->y + 8);
    }
}

// 16 directions at 2 px per frame (1/16 px): x = dir16[k], y = dir16[(k + 12) & 15]
static const signed char dir16[16] = { 32, 30, 23, 12, 0, -12, -23, -30, -32, -30, -23, -12, 0, 12, 23, 30 };
#define DIR16_Y(k) dir16[((k) + 12) & 15]
static const s32 gloom_spot[8][2] = { { 240, 112 }, { 140, 160 }, { 340, 160 }, { 120, 236 },
                                      { 360, 236 }, { 170, 296 }, { 310, 296 }, { 240, 210 } };
// The attack order per shield (Sprout, Ripple, Pebble, Breeze, Bark) and the open heart. The melee
// spirits' rounds (Sprout's whip, Bark's slam) dive more, so there is always a safe time to hit.
static const u8 gloom_plan[NUM_SHIELDS + 1][4] = {
    { G_DIVE, G_VOLLEY, G_DIVE, G_RING }, { G_VOLLEY, G_RING, G_VOLLEY, G_DIVE },
    { G_RING, G_VOLLEY, G_DIVE, G_VOLLEY }, { G_VOLLEY, G_DIVE, G_RING, G_VOLLEY },
    { G_DIVE, G_RING, G_DIVE, G_VOLLEY }, { G_VOLLEY, G_DIVE, G_RING, G_VOLLEY },
};
static int gloom_melee(void) { return gloom_phase == KIND_SPROUT || gloom_phase == KIND_BARK; }

static void add_mote_c(s32 x, s32 y, s32 dx, s32 dy, s32 life, s32 c) {
    for (s32 i = 0; i < MAX_MOTES; i++)
        if (!motes[i].life) {
            motes[i].x = x << 4; motes[i].y = y << 4; motes[i].dx = dx; motes[i].dy = dy; motes[i].life = life;
            motes[i].c = c;
            return;
        }
}
static void add_mote(s32 x, s32 y, s32 dx, s32 dy, s32 life) { add_mote_c(x, y, dx, dy, life, C_MOTE); }
static void smoke_fade(void) {                        // v0.19.4: smoke creatures caught in cleared air fade away
    for (s32 i = 0; i < MAX_ENEMIES; i++)
        if (enemies[i].alive && enemies[i].type == E_SMOKE && stack_dark(enemies[i].x + 8, enemies[i].y + 8) == 0) {
            enemies[i].alive = 0;
            add_mote_c(enemies[i].x + 4, enemies[i].y, 0, -6, 20, C_GLINT);
        }
}
// Motes fly out from (x, y): speed in 1/32 of the dir16 speed (32 = 2 px per frame).
static void gloom_burst(s32 x, s32 y, s32 speed, s32 life) {
    for (s32 k = 0; k < 8; k++)
        add_mote(x - 8, y - 8, dir16[k * 2] * speed >> 5, DIR16_Y(k * 2) * speed >> 5, life);
}
static void fire_spore(s32 k, s32 speed) {
    for (s32 i = 0; i < MAX_SPORES; i++)
        if (!spores[i].life) {
            Dot *s = &spores[i];
            s->x = (boss.x + 8) << 4; s->y = (boss.y + 8) << 4;
            s->dx = dir16[k & 15] * speed >> 5; s->dy = DIR16_Y(k & 15) * speed >> 5;
            s->life = 150;
            return;
        }
}

static void gloom_next(s32 act) { gloom_act = act; boss.timer = 0; boss.state = 0; gloom_shake = 0; gloom_ring = 0; boss.hop = 0; }

static void update_gloom(s32 dx, s32 dy) {
    s32 heart = gloom_phase >= NUM_SHIELDS, t = boss.timer;
    s32 cx = boss.x + 16, cy = boss.y + 16;
    gloom_shake = 0;
    switch (gloom_act) {
    case G_MOVE: {                                   // glide to the next spot, then attack
        s32 sp = heart ? 2 : 1;
        s32 mx = gloom_tx - boss.x, my = gloom_ty - boss.y;
        boss.x += mx > sp ? sp : mx < -sp ? -sp : mx;
        boss.y += my > sp ? sp : my < -sp ? -sp : my;
        if ((!mx && !my) || t > 120) {
            gloom_next(gloom_plan[gloom_phase][gloom_step & 3]);
            gloom_step++;
        }
        break;
    }
    case G_VOLLEY:                                   // shakes, motes gather in, spores fly
        if (t < 48) {
            gloom_shake = (t >> 1) & 1 ? 1 : -1;
            boss.state = t >= 30;
            if (t == 1) {
                chime(330, 30);
                for (s32 k = 0; k < 8; k++)   // 64 px out, in over 46 frames
                    add_mote(cx - 8 + dir16[k * 2] * 2, cy - 8 + DIR16_Y(k * 2) * 2,
                             -(dir16[k * 2] * 21 >> 5), -(DIR16_Y(k * 2) * 21 >> 5), 46);
            }
        } else if (t == 48) {
            boss.state = 0;
            sfx_play(SFX_GLOOM_SHOOT);
            if (gloom_melee()) {                     // three slow spores, aimed at you
                s32 best = 0, bd = -(1 << 30);
                for (s32 k = 0; k < 16; k++) {
                    s32 d = dx * dir16[k] + dy * DIR16_Y(k);
                    if (d > bd) { bd = d; best = k; }
                }
                for (s32 k = -1; k <= 1; k++) fire_spore(best + k, 22);
            } else
                for (s32 k = 0; k < 8; k++) fire_spore(k * 2, 26);   // a ring of eight
        } else if (t == 72 && heart) {
            sfx_play(SFX_GLOOM_SHOOT);
            for (s32 k = 0; k < 8; k++) fire_spore(k * 2 + 1, 26);   // the open heart fires twice
        } else if (t >= 90) {
            gloom_tx = gloom_spot[rng() & 7][0]; gloom_ty = gloom_spot[rng() & 7][1];
            gloom_next(G_MOVE);
        }
        break;
    case G_RING:                                     // dots blink at its reach, then the ring spreads
        if (t < 54) {
            boss.state = 1;
            gloom_ring = -RING_REACH;                // negative: the warning
            if (t == 1) chime(262, 40);
            gloom_ring_hit = 0;
        } else if (t < 54 + (RING_REACH - 16) / 2) {
            boss.state = 0;
            gloom_ring = 16 + (t - 54) * 2;
            if (t == 54) sfx_play(SFX_GLOOM_ROAR);
            s32 d2 = dx * dx + dy * dy, lo = gloom_ring - 10, hi = gloom_ring + 10;
            if (!gloom_ring_hit && d2 >= lo * lo && d2 <= hi * hi) { gloom_ring_hit = 1; hurt(1, cx, cy); }
        } else {
            gloom_ring = 0;
            gloom_tx = gloom_spot[rng() & 7][0]; gloom_ty = gloom_spot[rng() & 7][1];
            gloom_next(G_MOVE);
        }
        break;
    case G_DIVE:                                     // up, chase you as a shadow, crash down, dazed
        if (t < 24) boss.hop = t * 4;
        else if (t < 90) {                           // its shadow follows you
            s32 sp = 1;                              // 1 px a frame, a little faster later on
            if ((heart && (t & 1)) || (!gloom_melee() && (t & 3) == 0)) sp++;
            boss.x += sgn(dx) * sp; boss.y += sgn(dy) * sp;
        } else if (t < 110) {
            boss.state = 1;                          // locked: the shadow blinks
        } else if (t < 118) {
            boss.state = 0;
            boss.hop = 96 - (t - 109) * 12;
            if (boss.hop < 0) boss.hop = 0;
        } else {
            boss.hop = 0;
            sfx_play(SFX_GLOOM_SLAM);
            gloom_burst(cx, cy + 8, 20, 20);         // dust
            if (dx * dx + dy * dy < 28 * 28) hurt(1, cx, cy);
            if (!gloom_told_daze) { gloom_told_daze = 1; show_message("IT'S DAZED! HIT IT NOW!"); }
            gloom_next(G_DAZED);
        }
        break;
    default:                                         // dazed on the floor: no attacks, no touch damage
        if (t >= (gloom_melee() ? 150 : 100)) {
            gloom_tx = gloom_spot[rng() & 7][0]; gloom_ty = gloom_spot[rng() & 7][1];
            gloom_next(G_MOVE);
        }
        break;
    }
    if (gloom_phase >= 2 && frame % 600 == 300) {    // a blob crawls out of the heart
        for (s32 i = 0; i < 3; i++)
            if (!enemies[i].alive) {
                Enemy *e = &enemies[i];
                e->type = E_BLOB; e->x = boss.x + 8; e->y = boss.y + 36; e->hp = 2;
                e->timer = 0; e->dx = e->dy = 0; e->flash = e->knock = 0; e->alive = 1;
                break;
            }
    }
    if (gloom_act != G_DIVE && frame % 24 == 0)      // rot drips off it
        add_mote(boss.x + 4 + (s32)(rng() % 24), boss.y + 22, 0, 6, 28);
}

// Spores fly until they hit you, a wall, or an attack (the whip and the slam swat them).
static void update_gloom_dots(void) {
    for (s32 i = 0; i < MAX_SPORES; i++) {
        Dot *s = &spores[i];
        if (!s->life) continue;
        s->life--;
        s->x += s->dx; s->y += s->dy;
        s32 x = s->x >> 4, y = s->y >> 4;
        if (is_solid(tile_at((x + 8) >> 4, (y + 8) >> 4)) || !in_world(x, y)) { s->life = 0; continue; }
        if (atk.life && boxes_meet(atk.x, atk.y, atk.w, atk.h, x + 4, y + 4, 8, 8)) {
            s->life = 0; spark(x, y); chime(988, 6); continue;
        }
        if (boxes_meet(px + 3, py + 4, 10, 12, x + 5, y + 5, 6, 6)) { s->life = 0; hurt(1, x + 8, y + 8); }
    }
}
// Motes drift and fade, wherever they are (v0.18: the rescues' glints outside the tree showed none before).
static void update_motes(void) {
    for (s32 i = 0; i < MAX_MOTES; i++) {
        Dot *m = &motes[i];
        if (!m->life) continue;
        m->life--;
        m->x += m->dx; m->y += m->dy;
    }
}

// The mini bosses. state: 0 move, 1 wind up (flashing), 2 strike, 3 hidden (the Rot Root underground).
static void update_boss(void) {
    if (!in_combat_area()) return;
    if (!boss.alive) {                                       // a task done, its boss not beaten yet: it appears
        if (scene_on || msgq_len || cur_area != WEST) return;
        for (s32 k = 0; k < NUM_BOSSES; k++)
            if ((tree_done & (1 << k)) && !(boss_beaten & (1 << k))) {
                boss_start(k);
                sfx_play(SFX_PUMP_ON);
                show_message(boss_intro[k][0]);
                queue_message(boss_intro[k][1]);
                return;
            }
        return;
    }
    if (boss.flash) boss.flash--;
    boss.timer++;
    s32 dx = px + 8 - (boss.x + 16), dy = py + 8 - (boss.y + 16);
    s32 speed_up = boss.hp <= BOSS_HP / 2;                   // faster when hurt
    switch (boss.kind) {
    case B_CLAMPER:                                          // stalks you, winds up, charges
        if (boss.state == 0) {
            if (boss.timer & 1) { boss.x += sgn(dx); boss.y += sgn(dy); }
            if (boss.timer >= (speed_up ? 120u : 150u)) { boss.state = 1; boss.timer = 0; }
        } else if (boss.state == 1) {
            if (boss.timer >= 30) { boss.state = 2; boss.timer = 0; boss.dx = sgn(dx) * 3; boss.dy = sgn(dy) * 3; }
        } else if (boss.state == 2) {
            boss.x += boss.dx; boss.y += boss.dy;
            if (boss.timer >= 24) { boss.state = 0; boss.timer = 0; }
        }
        break;
    case B_THORN:                                            // rolls and bounces, then stops to rest (hit it then)
        if (!boss.dx) { boss.dx = 1; boss.dy = 1; }
        if (boss.timer % 220 >= (speed_up ? 170u : 150u)) break;   // resting
        boss.x += boss.dx * (speed_up && (boss.timer & 1) ? 2 : 1); boss.y += boss.dy;
        if (boss.x < 0 || boss.x > WORLD_PX - 32) { boss.dx = -boss.dx; boss.x += boss.dx * 2; }
        if (boss.y < 200 || boss.y > WORLD_PX - 32) { boss.dy = -boss.dy; boss.y += boss.dy * 2; }
        break;
    case B_MUD:                                              // creeps, then leaps and slams down
        if (boss.state == 0) {
            if ((boss.timer & 1) == 0) { boss.x += sgn(dx); boss.y += sgn(dy); }
            if (boss.timer >= (speed_up ? 150u : 180u)) { boss.state = 2; boss.timer = 0; }
        } else {
            boss.hop = boss.timer < 20 ? boss.timer : 40 - boss.timer;
            if (boss.timer < 40) { boss.x += sgn(dx); boss.y += sgn(dy); }
            if (boss.timer == 40) {                          // the landing shakes the ground
                boss.hop = 0;
                sfx_play(SFX_ROCK);
                if (dx * dx + dy * dy < 44 * 44) hurt(2, boss.x + 16, boss.y + 16);
                boss.state = 0; boss.timer = 0;
            }
        }
        break;
    case B_GLOOM:
        update_gloom(dx, dy);
        break;
    default:                                                 // burrows toward you, bursts up where you stood
        if (boss.state == 0) {
            if (boss.timer >= 90) { boss.state = 3; boss.timer = 0; }
        } else if (boss.state == 3) {
            if (boss.timer & 1) { boss.x += sgn(dx) * 2; boss.y += sgn(dy) * 2; }
            if (boss.timer >= (speed_up ? 50u : 60u)) { boss.state = 1; boss.timer = 0; }
        } else if (boss.state == 1) {
            if (boss.timer >= 24) { boss.state = 0; boss.timer = 0; sfx_play(SFX_ROCK); }
        }
        break;
    }
    if (boss.x < 0) boss.x = 0;
    if (boss.y < 0) boss.y = 0;
    if (boss.x > WORLD_PX - 32) boss.x = WORLD_PX - 32;
    if (boss.y > WORLD_PX - 32) boss.y = WORLD_PX - 32;
    if (boss.kind == B_GLOOM) update_gloom_dots();
    if (boss.state != 3 && boss.hop < 8 && !(boss.kind == B_GLOOM && gloom_act == G_DAZED) &&
        boxes_meet(px + 3, py + 4, 10, 12, boss.x + 4, boss.y + 4, 24, 24)) hurt(1, boss.x + 16, boss.y + 16);
}

// The boss's name and health, on the row under the HUD (redrawn twice a second).
static void draw_boss_bar(void) {
    if (frame % 30) return;
    if (!boss.alive || !in_combat_area()) { if (frame % 60 == 0) clear_row(1); return; }
    char bar[32], *p = bar;
    str_cat(&p, boss_name[boss.kind]);
    *p++ = ' ';
    if (boss.kind == B_GLOOM && gloom_phase < NUM_SHIELDS) {   // which spirit's shield is up
        str_cat(&p, spirit_name[gloom_phase]);
        str_cat(&p, gloom_shield == 3 ? " ***" : gloom_shield == 2 ? " **" : " *");
    } else
    for (s32 i = 0; i < BOSS_HP; i += 2) *p++ = i < boss.hp ? '*' : '@';
    *p = 0;
    clear_row(1);
    put_text(1, 1, " ");
    put_text(2, 1, bar);
}

// Spores, motes, the rot ring, the landing shadow, and the dazed stars.
static void draw_gloom_fx(void) {
    int on = boss.kind == B_GLOOM && cur_area == HOLLOW_IN;
    for (s32 i = 0; i < MAX_SPORES; i++) {
        Dot *s = &spores[i];
        if (on && s->life) sprite_set(OAM_SPORE + i, (s->x >> 4) - cam_x, (s->y >> 4) - cam_y, SPR_COMBAT + C_SPORE,
                                      (frame >> 3) & 1, PAL_COMBAT);
        else sprite_hide(OAM_SPORE + i);
    }
    for (s32 i = 0; i < MAX_MOTES; i++) {
        Dot *m = &motes[i];
        if (m->life) sprite_set(OAM_MOTE + i, (m->x >> 4) - cam_x, (m->y >> 4) - cam_y, SPR_COMBAT + m->c, 0, PAL_COMBAT);   // anywhere
        else sprite_hide(OAM_MOTE + i);
    }
    s32 r = gloom_ring < 0 ? -gloom_ring : gloom_ring;
    int ring = on && boss.alive && gloom_ring;
    for (s32 k = 0; k < RING_DOTS && cur_area == HOLLOW_IN; k++) {   // its slots are shared (OAM_POOL)
        if (!ring) { sprite_hide(OAM_RING + k); continue; }
        s32 x = boss.x + 16 + (dir16[k] * r >> 5) - 8, y = boss.y + 16 + (DIR16_Y(k) * r >> 5) - 8;
        sprite_set(OAM_RING + k, x - cam_x, y - cam_y, SPR_COMBAT + (gloom_ring < 0 && (frame & 8) ? C_MOTE : C_RINGDOT), 0, PAL_COMBAT);
    }
    if (on && boss.alive && boss.hop && !(boss.state == 1 && (frame & 4))) {
        u16 *o = &oam_shadow[OAM_SHADOW * 4];
        s32 x = boss.x + 8 - cam_x, y = boss.y + 20 - cam_y;
        if (x > -16 && x < SCREEN_W && y > -16 && y < SCREEN_H) {
            o[0] = (y & 0xFF) | (1 << 10);           // see-through
            o[1] = (x & 0x1FF) | (1 << 14);
            o[2] = (SPR_COMBAT + C_SHADOW) * 4 | (1 << 10) | (PAL_COMBAT << 12);
        } else sprite_hide(OAM_SHADOW);
    } else sprite_hide(OAM_SHADOW);
    if (on && boss.alive && gloom_act == G_DAZED)
        sprite_set(OAM_STARS, boss.x + 8 - cam_x + bob[(frame >> 2) & 15], boss.y - 8 - cam_y, SPR_COMBAT + C_STARS,
                   (frame >> 4) & 1, PAL_COMBAT);
    else sprite_hide(OAM_STARS);
}

static void draw_combat(void) {
    for (s32 i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &enemies[i];
        if (!e->alive || !in_combat_area()) { sprite_hide(OAM_ENEMY + i); continue; }
        if (e->type == E_SMOKE) {                        // the orchard's own sprite (its region set, bank PAL_BOSS)
            sprite_set(OAM_ENEMY + i, e->x - cam_x, e->y - cam_y, TILE_SMOKE / 4 + ((e->timer >> 4) & 1), e->dx < 0,
                       e->flash ? PAL_FLASH : PAL_BOSS);
            continue;
        }
        u32 cell = (e->type == E_BLOB ? C_BLOB : C_DRONE) + ((e->timer >> 4) & 1);
        sprite_set(OAM_ENEMY + i, e->x - cam_x, e->y - cam_y, SPR_COMBAT + cell, e->dx < 0, e->flash ? PAL_FLASH : PAL_COMBAT);
    }
    for (s32 k = 0; k < 4; k++) sprite_hide(OAM_ATTACK + k);
    if (atk.life) {
        s32 x = atk.x - cam_x, y = atk.y - cam_y;
        switch (atk.kind) {
        case KIND_SPROUT:
            if (atk.w > atk.h) sprite_set(OAM_ATTACK, x + 4, y, SPR_COMBAT + C_WHIP_H, pdir == DIR_LEFT, PAL_COMBAT);
            else sprite_set(OAM_ATTACK, x, y + 4, SPR_COMBAT + C_WHIP_V, 0, PAL_COMBAT);
            break;
        case KIND_RIPPLE: sprite_set(OAM_ATTACK, x - 3, y - 3, SPR_COMBAT + C_WATER, 0, PAL_COMBAT); break;
        case KIND_STONE: sprite_set(OAM_ATTACK, x - 2, y - 2, SPR_COMBAT + C_ROCK, 0, PAL_COMBAT); break;
        case KIND_BREEZE: sprite_set(OAM_ATTACK, x, y, SPR_COMBAT + C_GUST, (frame >> 2) & 1, PAL_COMBAT); break;
        case KIND_LILY: sprite_set(OAM_ATTACK, x - 2, y - 2, SPR_COMBAT + C_PETAL, (frame >> 2) & 1, PAL_COMBAT); break;
        case KIND_FLICK: sprite_set(OAM_ATTACK, x - 3, y - 3, SPR_FLICK + 3, (frame >> 2) & 1, PAL_FLICK); break;
        default:                                         // four quarters of the ring, flipped into place
            for (s32 k = 0; k < 4; k++) {
                u16 *o = &oam_shadow[(OAM_ATTACK + k) * 4];
                s32 qx = x + (k & 1) * 32 - 8 + 8, qy = y + (k >> 1) * 32 - 8 + 8;
                if (qx < -16 || qx > SCREEN_W || qy < -16 || qy > SCREEN_H) continue;
                o[0] = (qy & 0xFF);
                o[1] = (qx & 0x1FF) | (1 << 14) | ((k & 1) << 12) | ((k >> 1) << 13);
                o[2] = (SPR_COMBAT + C_SLAM) * 4 | (1 << 10) | (PAL_COMBAT << 12);
            }
        }
    }
    draw_gloom_fx();
    if (boss.alive && in_combat_area() && !(boss.state == 3 && (frame & 2))) {
        s32 x = boss.x - cam_x + (boss.kind == B_GLOOM ? gloom_shake : 0), y = boss.y - cam_y - boss.hop;
        u16 *o = &oam_shadow[OAM_BOSS * 4];
        if (x > -32 && x < SCREEN_W && y > -32 && y < SCREEN_H) {
            u32 f = boss.state == 1 || boss.state == 2 || boss.hop || ((frame >> 4) & 1);
            u32 pal = boss.flash || (boss.state == 1 && (frame & 4) && !(boss.kind == B_GLOOM && boss.hop)) ? PAL_FLASH : PAL_BOSS;
            o[0] = (y & 0xFF) | (boss.state == 3 ? (1 << 10) : 0);   // underground: see-through
            o[1] = (x & 0x1FF) | (2 << 14) | (dx_flip(boss.dx) << 12);
            o[2] = (TILE_BOSS + (boss.kind * 2 + f) * 16) | (1 << 10) | (pal << 12);
        } else sprite_hide(OAM_BOSS);
    } else sprite_hide(OAM_BOSS);
    s32 shx = boss.x - cam_x + gloom_shake, shy = boss.y - cam_y - boss.hop;
    if (boss.alive && boss.kind == B_GLOOM && gloom_phase < NUM_SHIELDS &&
        shx > -32 && shx < SCREEN_W && shy > -32 && shy < SCREEN_H) {   // off screen: hidden, not wrapped round
        u16 *o = &oam_shadow[OAM_SHIELD * 4];
        o[0] = (shy & 0xFF);
        o[1] = (shx & 0x1FF) | (2 << 14);
        o[2] = (TILE_BOSS + 10 * 16) | (1 << 10) | (PAL_SHIELD << 12);
    } else sprite_hide(OAM_SHIELD);
    int aura = !leaf_timer && party_count && is_you(spirits[party[active]].kind) && !swimming();   // v0.16.6: YOU is chosen (not while swimming)
    for (s32 k = 0; k < 6; k++) {                    // leaves swirl round the wizard (Bark's gift)
        if (aura && k < 3) {                         // three small leaves circle him while YOU is chosen
            u32 a = ((frame >> 1) + k * 16) % 48;
            s32 lx = px - cam_x + ((s32)loop_x48[a] * 9 >> 4), ly = py - cam_y + 4 + ((s32)loop_y48[a] * 5 >> 4);   // round his feet
            sprite_set(OAM_LEAF + k, lx, ly, SPR_COMBAT + C_LEAF, (a >> 4) & 1, PAL_COMBAT);
            continue;
        }
        if (!leaf_timer) { sprite_hide(OAM_LEAF + k); continue; }
        u32 a = ((leaf_timer >> 1) + k * 8) & 47;
        s32 ox = float_on == 1 ? float_x : px, oy = float_on == 1 ? float_y - float_h : py;   // round Bark, or you
        s32 lx = ox - cam_x + (s32)loop_x48[a] - 8 + 8, ly = oy - cam_y - 8 + (s32)loop_y48[a];
        sprite_set(OAM_LEAF + k, lx, ly - (s32)(120 - leaf_timer) / 8, SPR_COMBAT + C_LEAF, k & 1, PAL_COMBAT);
    }
    if (hit_life) {
        hit_life--;
        sprite_set(OAM_HIT, hit_x - cam_x, hit_y - cam_y, SPR_COMBAT + C_SPARK, 0, PAL_COMBAT);
    } else sprite_hide(OAM_HIT);
}

// The spirit picker's row: each party spirit (YOU as a leaf) at the top of the screen, a bracket on the
// chosen one. It borrows the spore slots (OAM 42-47), which only the Gloomheart's fight uses.
static void draw_picker(void) {
    static int shown;
    s32 x0 = SCREEN_W / 2 - party_count * 10;
    if (!pick_on) {                                // the build preview keeps the cursor otherwise
        if (shown) { shown = 0; sprite_hide(OAM_CURSOR); for (s32 r = 2; r <= 4; r++) clear_row(r); }
        return;
    }
    if (!shown)                                    // a dark strip behind the row (the text layer's box)
        for (s32 r = 2; r <= 4; r++)
            for (s32 c = x0 / 8 - 1; c <= (x0 + party_count * 20) / 8; c++) SCREENBLOCK(TEXT_SB)[r * 32 + c] = glyph_entry(' ');
    shown = 1;
    for (s32 p = 0; p < party_count && p < 6; p++) {
        s32 k = spirits[party[p]].kind, x = x0 + p * 20, y = 20 + (p == pick_sel ? -2 : 0);
        if (is_you(k)) sprite_set(OAM_SPORE + p, x, y, SPR_COMBAT + C_LEAF, 0, PAL_COMBAT);
        else sprite_set(OAM_SPORE + p, x, y, spirit_tile[k], 0, spirit_pal[k]);
    }
    sprite_set(OAM_CURSOR, x0 + pick_sel * 20, 20, SPR_SPARKLE + 1, 0, PAL_FX);
    for (s32 p = 0; p < party_count && p < 6; p++) oam_shadow[(OAM_SPORE + p) * 4 + 2] &= ~(3 << 10);   // above the strip
    oam_shadow[OAM_CURSOR * 4 + 2] &= ~(3 << 10);
}

static int leafen_talk(s32 fmx, s32 fmy);
static void use_ability(void) {
    s32 fx = px + 8, fy = py + 12;
    if (pdir == DIR_LEFT)  fx -= 14;
    if (pdir == DIR_RIGHT) fx += 14;
    if (pdir == DIR_UP)    fy -= 14;
    if (pdir == DIR_DOWN)  fy += 14;
    // An enemy in front: the active spirit attacks instead (free, with a short cooldown). Not Flick facing a
    // cold lantern or campfire (v0.19): lighting it is what drives the smoke off.
    u8 ahead = tile_at(fx >> 4, fy >> 4);
    int light_first = party_count && spirits[party[active]].kind == KIND_FLICK && (ahead == LANTERN_COLD || ahead == CAMPFIRE_COLD);
    if (party_count && can_fight(spirits[party[active]].kind) && enemy_ahead() && !light_first) {
        if (!atk_cooldown) start_attack(spirits[party[active]].kind);
        return;
    }
    if (leafen_talk(fx >> 4, fy >> 4)) return;               // a Leafen in need comes first
    if (interact(fx >> 4, fy >> 4)) return;
    if (!party_count) return;
    Spirit *s = &spirits[party[active]];
    if (is_you(s->kind)) {                                   // the wizard: Bark's power, no charges
        if (use_power(KIND_BARK, fx >> 4, fy >> 4)) return;
        u8 t = tile_at(fx >> 4, fy >> 4);
        if (skill_lv() >= 3 && (t == DEAD || t == DEAD_CRACK || t == DIRT || t == DRY_GRASS || t == PEBBLES))
            start_bloom_r(fx >> 4, fy >> 4, 2);              // SEEDFALL (v0.17): a small bloom, free
        else sfx_play(SFX_FAIL);
        return;
    }
    int free_use = (bonded == KIND_SPROUT && s->kind == KIND_SPROUT) || s->kind == KIND_FLICK;   // Flick: no charges
    if (s->energy == 0 && !free_use) {
        sfx_play(SFX_FAIL);
        show_message(spirit_tired[s->kind]);
        if (msgq_len == 0) queue_message(REST_TIP);
        return;
    }
    if (!use_power(s->kind, fx >> 4, fy >> 4)) {
        sfx_play(SFX_FAIL);
        if (tile_at(fx >> 4, fy >> 4) == VAEL_PIPE && !msg_timer) show_message("A VAEL PIPE! PEBBLE CAN SMASH IT.");
        if (tile_at(fx >> 4, fy >> 4) == SMOKE_STACK && !msg_timer) show_message("A VAEL SMOKE STACK! PEBBLE CAN SMASH IT.");
        return;
    }
    if (!free_use) s->energy--;                              // a bonded Sprout blooms for free
    add_fp(s->kind, 2);
    draw_active();
    if (!(told_tips & TIP_REST)) { told_tips |= TIP_REST; queue_message(REST_TIP); }
}

// v0.18.4 (Caleb): you carry four spirits; Bark's gift lets you carry a fifth (Bark himself, later YOU).
// Any more ride in the pouch, once Bark has given it.
// YOU (Bark's power, once the tree is healed) is the wizard himself, not a spirit: it never counts.
static s32 party_cap(void) { return spirits[KIND_BARK].state != SP_HIDDEN || tree_alive ? 5 : 4; }
static s32 carried(void) { return party_count - (tree_alive && in_party(KIND_BARK)); }
static const char *pouch_msg[NUM_SPIRITS] = { "SPROUT HOPS IN THE POUCH.", "RIPPLE HOPS IN THE POUCH.",
    "PEBBLE HOPS IN THE POUCH.", "BREEZE HOPS IN THE POUCH.", "", "LILY HOPS IN THE POUCH.",
    "FLICK HOPS IN THE POUCH." };
static void party_add(s32 i) {
    if (carried() >= party_cap() && pouch_on && !is_you(i)) {
        spirits[i].state = SP_POUCH;
        spirits[i].area = cur_area;
        queue_message(pouch_msg[i]);
        return;
    }
    party[party_count++] = i;
    active = party_count - 1;
    spirits[i].state = SP_JOINED;
    draw_active();
}

static void party_remove(s32 i) {
    s32 n = 0;
    for (s32 p = 0; p < party_count; p++) if (party[p] != i) party[n++] = party[p];
    party_count = n;
    if (active >= party_count) active = party_count ? party_count - 1 : 0;
    draw_active();
}

// Living tiles in the 5x5 around a home site.
static s32 life_around(s32 mx, s32 my) {
    s32 n = 0;
    for (s32 y = my - 2; y <= my + 2; y++)
        for (s32 x = mx - 2; x <= mx + 2; x++)
            if (is_alive(tile_at(x, y))) n++;
    return n;
}

static void send_home(s32 k) {
    Spirit *s = &spirits[k];
    party_remove(k);
    s->state = SP_HOME;
    s->x = home_site_x[k] * 16; s->y = home_site_y[k] * 16;   // drawn inside its home (draw_home_spirit)
    s->area = home_site_area[k];
    s->energy = ENERGY_MAX_HOUSED + bonus_charge;
}

// Draw spirit k's home at its current level: the base on the site, the roof on the tile above.
static void place_home_tiles(s32 k) {
    s32 hx = home_site_x[k], hy = home_site_y[k];
    use_area(home_site_area[k]);
    set_tile(hx, hy, home_tile(k, home_level[k], 0));
    if (hy > 0) set_tile(hx, hy - 1, home_tile(k, home_level[k], 1));
    use_area(cur_area);
}

static void try_build_home(s32 k) {
    s32 hx = home_site_x[k], hy = home_site_y[k];
    const u8 *h = home_helper[k];
    if (life_around(hx, hy) < 4) { sfx_play(SFX_FAIL); show_message("BLOOM THE LAND HERE FIRST"); return; }
    if (!in_party(h[0]) || !in_party(h[1]) || spirits[h[0]].energy == 0 || spirits[h[1]].energy == 0) {
        sfx_play(SFX_FAIL); show_message(home_needs[k]); return;
    }
    spirits[h[0]].energy--;
    spirits[h[1]].energy--;
    home_done[k] = 1;
    spirits[k].housed = 1;
    send_home(k);
    sfx_play(SFX_HOME);
    show_message(home_built[k]);
    queue_message(home_bonus[k]);
    sparkle_at(hx, hy);
    pulse_r[k] = 0;
    pulse_timer[k] = HOME_PULSE - 120;   // first pulse two seconds after building
    home_level[k] = 1;
    place_home_tiles(k);
    unstick_player();
    flood_dist(home_dist[k], hx, hy, HOME_REACH_L1);
    add_fp(k, 20);
    s32 all = 1;
    for (s32 i = 0; i < NUM_HOMES; i++) all &= home_done[i];
    if (all && !told_ending) {
        told_ending = 1;
        queue_message("EVERY SPIRIT HAS A HOME.");
        queue_message("BUT THE RIVER STILL RUNS LOW.");
        queue_message("VAEL'S DAM LIES NORTH...");
    }
}

static int near_px(s32 x, s32 y, s32 r) {
    s32 dx = x - px, dy = y - py;
    return dx * dx + dy * dy < r * r;
}

// ---------- building (hold SELECT) ----------
// The last four items are quest rewards and appear once their quest is done (unlocked[]).
static const u8 build_tile[NUM_BUILDS] = { PATH, FLOWERBED, SAPLING, LANTERN, BENCH, 0,
                                           BLOSSOM, FOUNTAIN, STATUE, PINWHEEL,
                                           DEW_POND, ACORN_BENCH, LEAF_SWING, FLOWER_ARCH, PEBBLE_PATH, MUSH_LAMP };
static const u8 build_cost[NUM_BUILDS][3] = {   // stone, seeds, fruit
    { 1, 0, 0 }, { 0, 2, 0 }, { 0, 1, 1 }, { 3, 0, 0 }, { 2, 0, 1 }, { 5, 5, 3 },
    { 0, 3, 2 }, { 4, 0, 2 }, { 6, 0, 0 }, { 2, 2, 0 },
    { 3, 0, 1 }, { 2, 0, 2 }, { 1, 2, 0 }, { 0, 4, 0 }, { 1, 0, 0 }, { 2, 1, 0 },
};
static const char *build_text[NUM_BUILDS] = {
    "PATH: 1 STONE", "FLOWER BED: 2 SEEDS", "SAPLING: 1 SEED 1 FRUIT", "LANTERN: 3 STONE",
    "BENCH: 2 STONE 1 FRUIT", "HOME: 5 STONE 5 SEED 3 FRUIT",
    "BLOSSOM: 3 SEEDS 2 FRUIT", "FOUNTAIN: 4 STONE 2 FRUIT", "STATUE: 6 STONE", "PINWHEEL: 2 STONE 2 SEEDS",
    "DEW POND: 3 STONE 1 FRUIT", "ACORN BENCH: 2 STONE 2 FRUIT", "LEAF SWING: 1 STONE 2 SEEDS",
    "FLOWER ARCH: 4 SEEDS", "PEBBLE PATH: 1 STONE", "MUSHROOM LAMP: 2 STONE 1 SEED",
};
static int build_ok(s32 b) {
    if (b >= B_DEWPOND) return leafen_state[b - B_DEWPOND] >= LS_FREE;   // a Leafen's gift, once it is free
    return b < B_BLOSSOM || unlocked[b - B_BLOSSOM];
}
// The order L/R walks the list in: the home upgrade first (Caleb, v0.13.1), then the rest.
static const u8 build_order[NUM_BUILDS] = { B_UPGRADE, B_PATH, B_BED, B_SAPLING, B_LANTERN, B_BENCH,
                                            B_BLOSSOM, B_FOUNTAIN, B_STATUE, B_PINWHEEL,
                                            B_DEWPOND, B_ACORNBENCH, B_LEAFSWING, B_FLOWERARCH, B_PEBBLEPATH, B_MUSHLAMP };
static void build_step(s32 dir) {
    s32 i = 0;
    while (build_order[i] != build_item) i++;
    do { i = (i + dir + NUM_BUILDS) % NUM_BUILDS; } while (!build_ok(build_order[i]));
    build_item = build_order[i];
}

static s32 build_of_tile(u8 t) {
    switch (t) {
    case PATH: return B_PATH;
    case FLOWERBED: case FLOWERBED_EMPTY: return B_BED;
    case SAPLING: case FRUIT_TREE: case FRUIT_TREE_BARE: return B_SAPLING;
    case LANTERN: return B_LANTERN;
    case BENCH: return B_BENCH;
    case BLOSSOM: return B_BLOSSOM;
    case FOUNTAIN: return B_FOUNTAIN;
    case STATUE: return B_STATUE;
    case PINWHEEL: return B_PINWHEEL;
    case DEW_POND: return B_DEWPOND;
    case ACORN_BENCH: return B_ACORNBENCH;
    case LEAF_SWING: return B_LEAFSWING;
    case FLOWER_ARCH: return B_FLOWERARCH;
    case PEBBLE_PATH: return B_PEBBLEPATH;
    case MUSH_LAMP: return B_MUSHLAMP;
    }
    return -1;
}
static int can_pay(s32 b) {
    return stone >= build_cost[b][0] && seeds >= build_cost[b][1] && fruit >= build_cost[b][2];
}
static void pay(s32 b, s32 sign) {
    stone += sign * build_cost[b][0]; seeds += sign * build_cost[b][1]; fruit += sign * build_cost[b][2];
}

// Sprout's quest: three flower beds within 5 tiles of its grove site (in the valley).
static void check_sprout_quest(void) {
    if (quest[KIND_SPROUT] != 1) return;
    s32 n = 0, hx = home_site_x[KIND_SPROUT], hy = home_site_y[KIND_SPROUT];
    u8 *m = maps[home_site_area[KIND_SPROUT]];
    for (s32 y = hy - 5; y <= hy + 5; y++)
        for (s32 x = hx - 5; x <= hx + 5; x++)
            if (x >= 0 && y >= 0 && x < MAP_W && y < MAP_H &&
                (m[y * MAP_W + x] == FLOWERBED || m[y * MAP_W + x] == FLOWERBED_EMPTY)) n++;
    if (n >= 3) complete_quest(KIND_SPROUT);
}

// Upgrade the built home next to the player to level 2: it reaches 12 tiles and its bonus grows.
static void upgrade_home(void) {
    for (s32 k = 0; k < NUM_HOMES; k++) {
        if (home_site_area[k] != cur_area || !near_px(home_site_x[k] * 16, home_site_y[k] * 16, 28)) continue;
        if (!home_done[k]) { sfx_play(SFX_FAIL); show_message("BUILD THIS HOME FIRST"); return; }
        if (home_level[k] >= 2) { sfx_play(SFX_FAIL); show_message("THIS HOME IS FULLY GROWN"); return; }
        if (!can_pay(B_UPGRADE)) { sfx_play(SFX_FAIL); show_message("NOT ENOUGH MATERIALS"); return; }
        pay(B_UPGRADE, -1);
        home_level[k] = 2;
        place_home_tiles(k);
        unstick_player();
        add_fp(k, 20);
        flood_dist(home_dist[k], home_site_x[k], home_site_y[k], HOME_REACH);   // pulses go on to 12
        sfx_play(SFX_HOME);
        sparkle_at(home_site_x[k], home_site_y[k]);
        show_message("THE HOME GREW STRONGER!");
        s32 grown = 1;
        for (s32 i = 0; i < NUM_HOMES; i++) grown &= home_level[i] >= 2;
        if (grown && !told_grown) {
            told_grown = 1;
            queue_message("EVERY HOME IS FULLY GROWN!");
            queue_message("YOU DID IT, WIZARD!");
            bark_step = 1;                     // then, after a pause: crack... thump... Bark (update_bark)
            bark_wait = 120;
        }
        return;
    }
    sfx_play(SFX_FAIL);
    show_message("STAND BY A HOME TO UPGRADE");
}

static void build_mode(void) {
    if (HIT(KEY_R)) build_step(1);
    if (HIT(KEY_L)) build_step(-1);
    if (HIT(KEY_SELECT | KEY_R | KEY_L) || msg_timer < 30) { show_message(build_text[build_item]); msg_timer = 40; }
    s32 fx = px + 8, fy = py + 12;
    if (pdir == DIR_LEFT)  fx -= 14;
    if (pdir == DIR_RIGHT) fx += 14;
    if (pdir == DIR_UP)    fy -= 14;
    if (pdir == DIR_DOWN)  fy += 14;
    s32 mx = fx >> 4, my = fy >> 4;
    u8 t = tile_at(mx, my);
    // preview: green when this item can go here and you can afford it
    cursor_mx = mx; cursor_my = my;
    cursor_ok = build_item == B_UPGRADE ? can_pay(B_UPGRADE)
                                        : (t == GRASS || t == GRASS_TUFT || t == FLOWERS) && can_pay(build_item);
    if (HIT(KEY_A)) {
        if (build_item == B_UPGRADE) { upgrade_home(); return; }
        if (!(t == GRASS || t == GRASS_TUFT || t == FLOWERS)) { sfx_play(SFX_FAIL); show_message("BUILD ON GREEN LAND"); return; }
        if (!can_pay(build_item)) { sfx_play(SFX_FAIL); show_message("NOT ENOUGH MATERIALS"); return; }
        pay(build_item, -1);
        set_tile(mx, my, build_tile[build_item]);
        sfx_play(SFX_BUILD);
        sparkle_at(mx, my);
        if (build_item == B_BED) check_sprout_quest();
    }
    if (HIT(KEY_B)) {
        s32 b = build_of_tile(t);
        if (b < 0) { sfx_play(SFX_FAIL); return; }
        pay(b, 1);                                  // materials come back
        set_tile(mx, my, GRASS);
        sfx_play(SFX_REMOVE);
        show_message("REMOVED. MATERIALS BACK.");
    }
}

// Every 10 s, built things grow: picked beds regrow flowers, saplings become fruit trees,
// and picked fruit trees ripen again. Works in all three areas.
static void update_growth(void) {
    if (++grow_timer < 600) return;
    grow_timer = 0;
    for (s32 a = 0; a < NUM_AREAS; a++) {
        use_area(a);
        for (s32 i = 0; i < MAP_W * MAP_H; i++) {
            u8 t = world[i], nt = t;
            if (t == FLOWERBED_EMPTY && (rng() & 1)) nt = FLOWERBED;
            else if (t == FRUIT_TREE_BARE && (rng() & 1)) nt = FRUIT_TREE;
            else if (t == SAPLING && rng() % 3 == 0) nt = FRUIT_TREE;
            if (nt != t) set_tile(i % MAP_W, i / MAP_W, nt);
        }
    }
    use_area(cur_area);
    check_sprout_quest();
}

// A tile is safe when a built home in that area reaches it.
static int home_protects(s32 a, s32 x, s32 y) {
    for (s32 k = 0; k < NUM_HOMES; k++) {
        if (!home_done[k] || home_site_area[k] != a) continue;
        s32 dx = x - home_site_x[k], dy = y - home_site_y[k], r = home_reach(k);
        if (dx * dx + dy * dy <= r * r) return 1;
    }
    return 0;
}
static int is_grassy(u8 t) { return t == GRASS || t == GRASS_TUFT || t == FLOWERS || t == TREE || t == STUMP_GRASS; }

static int spawn_pump(s32 a) {
    use_area(a);
    for (s32 tries = 0; tries < 80; tries++) {
        s32 x = 1 + rng() % (MAP_W - 2), y = 1 + rng() % (MAP_H - 2);
        u8 t = world[y * MAP_W + x];
        if (!(t == GRASS || t == GRASS_TUFT || t == FLOWERS) || home_protects(a, x, y)) continue;
        if (a == cur_area) {                               // never right next to the player
            s32 dx = x - ((px + 8) >> 4), dy = y - ((py + 12) >> 4);
            if (dx * dx + dy * dy < 36) continue;
        }
        // reachable: at least one walkable neighbor
        if (is_solid(tile_at(x + 1, y)) && is_solid(tile_at(x - 1, y)) &&
            is_solid(tile_at(x, y + 1)) && is_solid(tile_at(x, y - 1))) continue;
        set_tile(x, y, VAEL_PUMP);
        pump_on[a] = 1; pump_x[a] = x; pump_y[a] = y; pump_drain[a] = 0;
        use_area(cur_area);
        return 1;
    }
    use_area(cur_area);
    return 0;
}

static u32 dev_no_pumps;   // dev mode: pumps never start
static void update_pumps(void) {
    if (dev_no_pumps) return;
    s32 any_home = 0;
    for (s32 k = 0; k < NUM_HOMES; k++) any_home |= home_done[k];
    if (!any_home) return;
    // start a new pump now and then, in an area that has none
    s32 running = 0;
    for (s32 a = 0; a < NUM_AREAS; a++) running += pump_on[a];
    u32 wait = pumps_stopped || running ? PUMP_EVERY : PUMP_FIRST;
    if (++pump_timer >= wait) {
        pump_timer = 0;
        s32 start = rng() % NUM_AREAS;
        for (s32 i = 0; i < NUM_AREAS; i++) {
            s32 a = (start + i) % NUM_AREAS;
            if (pump_on[a] || !spawn_pump(a)) continue;
            sfx_play(SFX_PUMP_ON);
            queue_message("A VAEL PUMP STARTED UP:");
            queue_message(area_name[a]);
            break;
        }
    }
    // each running pump drains the nearest living tile it can reach
    for (s32 a = 0; a < NUM_AREAS; a++) {
        if (!pump_on[a] || ++pump_drain[a] < PUMP_DRAIN) continue;
        pump_drain[a] = 0;
        use_area(a);
        s32 best = 1 << 30, bx = -1, by = -1, cx = pump_x[a], cy = pump_y[a];
        for (s32 y = cy - PUMP_REACH; y <= cy + PUMP_REACH; y++)
            for (s32 x = cx - PUMP_REACH; x <= cx + PUMP_REACH; x++) {
                if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) continue;
                s32 d = (x - cx) * (x - cx) + (y - cy) * (y - cy);
                if (d > PUMP_REACH * PUMP_REACH || d >= best) continue;
                if (!is_grassy(world[y * MAP_W + x]) || home_protects(a, x, y)) continue;
                best = d; bx = x; by = y;
            }
        if (bx >= 0) {
            u8 was = world[by * MAP_W + bx];
            set_tile(bx, by, was == TREE ? DEAD_TREE : was == STUMP_GRASS ? STUMP : DEAD);
            if (a == cur_area) draw_life();
        }
        use_area(cur_area);
    }
}

// ---------- gifts: tap B while facing a spirit ----------
static const u8 favorite[NUM_SPIRITS] = { 2, 1, 0, 2, 1, 1, 1 };    // 0 stone, 1 fruit, 2 seeds
static const char *favorite_name[3] = { "STONE", "FRUIT", "SEEDS" };
static const char *loves_msg[NUM_SPIRITS] = {
    "SPROUT LOVES THE SEEDS!", "RIPPLE LOVES THE FRUIT!", "PEBBLE LOVES THE STONE!", "BREEZE LOVES THE SEEDS!",
    "BARK: ...FINE. THANKS.", "LILY LOVES THE FRUIT!", "FLICK LOVES THE FRUIT!",
};
static u32 b_hold;
// B by deep water with Ripple chosen (and the skill learned): jump in (v0.18.1).
static int try_jump_in(void) {
    if (!swim_skill || in_water || !party_count || spirits[party[active]].kind != KIND_RIPPLE) return 0;
    s32 sx = pdir == DIR_LEFT ? -1 : pdir == DIR_RIGHT ? 1 : 0, sy = pdir == DIR_UP ? -1 : pdir == DIR_DOWN ? 1 : 0;
    s32 mx = (px + 8 + sx * 14) >> 4, my = (py + 12 + sy * 14) >> 4;
    u8 t = tile_at(mx, my);
    if (t == SLUDGE) { show_message("NOT IN THE SLUDGE! EW!"); sfx_play(SFX_FAIL); return 1; }
    if (!water_tile(t)) return 0;
    if (!deep_water(mx, my)) { show_message("TOO SMALL TO SWIM IN."); sfx_play(SFX_FAIL); return 1; }
    in_water = 1;
    px = mx * 16; py = my * 16 - 8;               // in with a splash
    sfx_play(SFX_BRIDGE);
    for (s32 k = 0; k < 6; k++) add_mote_c(px, py, dir16[(k * 3 + 9) & 15] >> 1, DIR16_Y((k * 3 + 9) & 15) >> 1, 14, C_SPLASH);   // splash
    return 1;
}

static void try_gift(void) {
    s32 fx = px + 8, fy = py + 12;
    if (pdir == DIR_LEFT)  fx -= 14;
    if (pdir == DIR_RIGHT) fx += 14;
    if (pdir == DIR_UP)    fy -= 14;
    if (pdir == DIR_DOWN)  fy += 14;
    for (s32 k = 0; k < NUM_SPIRITS; k++) {
        Spirit *sp = &spirits[k];
        if (sp->area != cur_area || (sp->state != SP_JOINED && sp->state != SP_HOME) || is_you(k)) continue;
        s32 dx = sp->x + 8 - fx, dy = sp->y + 8 - fy;
        if (dx * dx + dy * dy > 12 * 12) continue;
        u32 *have = favorite[k] == 0 ? &stone : favorite[k] == 1 ? &fruit : &seeds;
        if (*have == 0) {
            char *p = num_msg;
            str_cat(&p, spirit_name[k]);
            str_cat(&p, " WANTS ");
            str_cat(&p, favorite_name[favorite[k]]);
            *p = 0;
            sfx_play(SFX_FAIL);
            show_message(num_msg);
            return;
        }
        (*have)--;
        sfx_play(SFX_GIFT);
        show_message(loves_msg[k]);
        sparkle_at((sp->x + 8) >> 4, (sp->y + 8) >> 4);
        add_fp(k, 10);
        return;
    }
}

static void handle_action(void) {
    cursor_mx = -1;   // build_mode() sets it again while SELECT is held
    // A short tap of B (released within 10 frames) is a gift; holding B is running.
    if (HELD(KEY_B)) b_hold++;
    else {
        if (b_hold && b_hold < 10 && !HELD(KEY_SELECT) && !try_jump_in()) try_gift();
        b_hold = 0;
    }
    // R swaps to the next spirit, L to the previous one.
    if (HELD(KEY_SELECT)) { build_mode(); return; }   // SELECT is the builder
    // v0.18.2 (Caleb: cycling through six was tedious): a tap of R or L steps once; holding either opens
    // a row of every spirit at the top (pick_on); the D-pad moves along it; letting go chooses.
    s32 step = 0;
    if (HELD(KEY_R | KEY_L) && party_count > 1) {
        if (!pick_hold) pick_key = HELD(KEY_R) ? KEY_R : KEY_L;
        if (++pick_hold == 12) { pick_on = 1; pick_sel = active; sfx_play(SFX_MENU_OPEN); }
        if (pick_on && (HIT(KEY_RIGHT) || HIT(KEY_LEFT))) {
            pick_sel = (pick_sel + (HIT(KEY_RIGHT) ? 1 : party_count - 1)) % party_count;
            sfx_play(SFX_MENU_MOVE);
        }
        if (pick_on && !scene_on) {
            show_message(is_you(spirits[party[pick_sel]].kind) ? "YOU" : spirit_name[spirits[party[pick_sel]].kind]);
            msg_is_prompt = 1; msg_timer = 30;
        }
    } else if (pick_hold) {
        if (pick_on) step = (pick_sel - active + party_count) % party_count;
        else step = pick_key == KEY_R ? 1 : party_count - 1;
        pick_hold = 0; pick_on = 0;
    }
    if (party_count > 1 && step) {
        active = (active + step) % party_count;
        sfx_play(SFX_MENU_MOVE);
        if (!scene_on && !msgq_len && !(msg_timer && !msg_is_prompt)) {   // dialogue comes first (Caleb)
            show_message(is_you(spirits[party[active]].kind) ? "YOU: CUT, CLEAR, SLAM" : spirit_skill[spirits[party[active]].kind]);
            msg_is_prompt = 1;                         // and the next line of dialogue may replace it
        }
        draw_active();
    }

    // Spirits not in the party react when the player is close. Two passes: a spirit you can
    // greet comes before one you can call back from its home (Bark lands right beside a home).
    for (s32 pass = 0; pass < 2; pass++)
    for (s32 i = 0; i < NUM_SPIRITS; i++) {
        Spirit *s = &spirits[i];
        if (s->state == SP_JOINED || s->state == SP_HIDDEN || s->state == SP_POUCH || s->area != cur_area) continue;
        if ((s->state == SP_HOME) != (pass == 1)) continue;
        // A housed spirit is drawn inside its home (the roof for Pebble and Breeze); it also answers near the site.
        if (!near_px(s->x, s->y, 28) && !(s->state == SP_HOME && near_px(home_site_x[i] * 16, home_site_y[i] * 16, 24))) continue;
        if (s->state == SP_ASLEEP && i == KIND_FLICK) {   // v0.19: cold in its nest; the ember warms it
            prompt("A: LOOK AT THE LITTLE BIRD");
            if (HIT(KEY_A)) {
                if (flick_flags & 1) { flick_step = 1; return; }
                show_message("A TINY BIRD OF ASH, CURLED UP.");
                queue_message("BRRRRR... SO COLD...");
                queue_message("IT NEEDS SOMETHING WARM.");
            }
            return;
        }
        if (s->state == SP_ASLEEP) {
            // Only a hint; A still uses your spirit so you can bloom beside it.
            if (msg_timer == 0) show_message("IT SLEEPS. BLOOM NEAR IT");
            continue;
        }
        if (s->state == SP_HOME) {
            prompt(call_back[i]);
            if (HIT(KEY_A)) { party_add(i); sfx_play(SFX_JOIN); show_message(called_back[i]); just_called = i; }
            return;
        }
        prompt("A: GREET THE SPIRIT");
        if (HIT(KEY_A)) {
            party_add(i);
            sfx_play(SFX_JOIN);
            show_message(spirit_joined[s->kind]);
            s32 found = 0;                            // v0.18.4 (Caleb): a nudge once the fourth joins
            for (s32 k = 0; k < NUM_HOMES; k++) found += spirits[k].state >= SP_JOINED && spirits[k].state != SP_HIDDEN;
            if (i < NUM_HOMES && found == NUM_HOMES) {
                queue_message("ALL FOUR SPIRITS FOUND!");
                queue_message("BUILD UP THEIR HOMES,");
                queue_message("UPGRADE THEM, AND RESTORE");
                queue_message("THE LANDS.");
            }
        }
        return;
    }

    // Home sites: build one, or send a housed spirit back to it. A site only takes the
    // A button when the action can happen; otherwise A still uses your spirit (so you can
    // bloom the land around a site).
    for (s32 k = 0; k < NUM_HOMES; k++) {
        if (home_site_area[k] != cur_area || !near_px(home_site_x[k] * 16, home_site_y[k] * 16, 24)) {
            if (just_called == k) just_called = -1;
            continue;
        }
        if (!in_party(k)) {
            if (!home_done[k] && msg_timer == 0) show_message(home_hint[k]);
            break;
        }
        if (home_done[k]) {
            // Only the active spirit can be sent home, so A near a home still uses the others.
            // A spirit you just called back stays until you walk away from its home.
            if (party[active] != k || just_called == k) break;
            prompt(go_home[k]);
            if (HIT(KEY_A)) { send_home(k); chime(659, 10); }
            return;
        }
        if (life_around(home_site_x[k], home_site_y[k]) < 4) {
            if (msg_timer == 0) show_message("BLOOM THE LAND HERE FIRST");
            break;
        }
        const u8 *h = home_helper[k];
        if (!in_party(h[0]) || !in_party(h[1]) || !spirits[h[0]].energy || !spirits[h[1]].energy) {
            if (msg_timer == 0) show_message(home_needs[k]);
            break;
        }
        prompt(home_prompt[k]);
        if (HIT(KEY_A)) try_build_home(k);
        return;
    }

    if (HIT(KEY_A)) use_ability();
}

// Energy: standing on living ground slowly recharges the party (twice as fast with Ripple's
// spring); standing at a built home refills it. Each home also pulses life outward in rings.
static void update_homes(void) {
    int changed = 0;
    s32 fx = (px + 8) >> 4, fy = (py + 12) >> 4;
    u32 regen = REGEN_FRAMES / (1 + home_level[KIND_RIPPLE]);   // spring bonus: 2x, 3x at level 2
    if ((is_alive(tile_at(fx, fy)) || is_pad(tile_at(fx, fy))) && ++regen_timer >= regen) {
        regen_timer = 0;
        int got = 0;
        for (s32 p = 0; p < party_count; p++) {
            Spirit *s = &spirits[party[p]];
            if (s->energy < energy_max(s)) { s->energy++; got = 1; }
        }
        if (got) { chime(1319, 5); changed = 1; }
    }
    for (s32 k = 0; k < NUM_HOMES; k++) {
        if (!home_done[k] || home_site_area[k] != cur_area) continue;
        s32 dx = fx - home_site_x[k], dy = fy - home_site_y[k];
        if (dx * dx + dy * dy > 4) continue;
        for (s32 p = 0; p < party_count; p++) {
            Spirit *s = &spirits[party[p]];
            if (s->energy < energy_max(s)) { s->energy = energy_max(s); changed = 1; }
        }
        if (hearts_now < hearts_max()) { hearts_now = hearts_max(); draw_hearts_hud(); }
    }
    if (changed) draw_active();

    // Pulses: ring r turns living, ring r+1 becomes the dirt edge. Rings follow the same
    // distance rules as blooms, so rock walls and water stop them.
    // Homes in other areas keep pulsing: their map changes in memory only.
    int grew = 0;
    for (s32 k = 0; k < NUM_HOMES; k++) {
        s32 reach = home_reach(k), g = reach * 2 + 1;
        if (!home_done[k]) continue;
        if (pulse_r[k] >= reach) {            // done; in harmony the waves start over and never stop
            if (!harmony) continue;
            pulse_r[k] = 0;
        }
        if (++pulse_timer[k] < HOME_PULSE / pulse_div / (harmony ? 2 : 1)) continue;
        pulse_timer[k] = 0;
        s32 r = ++pulse_r[k];
        s32 hx = home_site_x[k], hy = home_site_y[k];
        use_area(home_site_area[k]);
        for (s32 gy = 0; gy < g; gy++)
            for (s32 gx = 0; gx < g; gx++) {
                s32 d = home_dist[k][gy * g + gx];
                if (d <= 10 * (r - 1) + 5 || d > 10 * (r + 1) + 5) continue;
                grow_tile(hx + gx - reach, hy + gy - reach, d > 10 * r + 5);
            }
        check_wake();
        if (work_area == cur_area) { sparkle_at(hx, hy); chime(scale[r & 7], 7); }
        use_area(cur_area);
        grew = 1;
    }
    if (grew) draw_life();
}

// Bark's arrival, once every home is level 2: a pause, a crack, Bark drops in beside the player
// (bark_fall animates the drop in draw_sprites), a thump, and his first words. Then A greets him.
enum { BS_SAY, BS_CRACK, BS_FALL, BS_THUMP, BS_POWER };
static const struct { u8 act; const char *msg; } bark_scene[] = {
    { BS_CRACK, "CRACK...!" }, { BS_FALL, 0 }, { BS_THUMP, "THUMP." }, { BS_SAY, "DID SOMETHING FALL?" },
    { BS_SAY, "BARK: OW. OW. MY ROOTS." }, { BS_SAY, "BARK: WHAT? STOP STARING." },
    { BS_SAY, "BARK: I'M BARK. I WAS PART" }, { BS_SAY, "OF THE GREAT TREE." },
    { BS_SAY, "VAEL CUT IT DOWN. NOW I'M..." }, { BS_SAY, "...THIS. A STUMP." },
    { BS_SAY, "BARK: THOSE FOUR WITH YOU?" }, { BS_SAY, "I MADE THEM. LONG AGO," },
    { BS_SAY, "BEFORE VAEL CUT ME DOWN." }, { BS_SAY, "YOU CAN ONLY CARRY FOUR." },
    { BS_POWER, "HERE. A BIT OF MY POWER." }, { BS_SAY, "NOW YOU CAN CARRY FIVE." },
    { BS_SAY, "BARK: HMPH. FINE. I'LL HELP." }, { BS_SAY, "BUT I WANT MY TREE BACK." },
    { BS_SAY, "LET ALL FOUR REST AT HOME..." },
};
#define BARK_SCENE_LEN (sizeof(bark_scene) / sizeof(bark_scene[0]))
// Bark at his old tree, the first time you bring him west: he gives his quest.
static const char *tree_scene[] = {
    "BARK: ...", "BARK: YEP. THAT'S IT.", "THE GREAT TREE. THAT WAS ME.", "NOW I'M THIS JUNK.",
    "VAEL CLAMPED MY ROOTS,", "CHOKED ME WITH THORNS,", "AND DRANK MY POOL DRY.",
    "WIZARD. YOU AND YOUR SPIRITS", "COULD HELP. MAYBE. HMPH.",
    "BREAK THE CLAMPS. CUT THORNS.", "FILL THE POOL. WAKE MY ROOTS.",
};
#define TREE_SCENE_LEN (sizeof(tree_scene) / sizeof(tree_scene[0]))
static void update_tree_scene(void) {
    if (!tree_step && cur_area == WEST && in_party(KIND_BARK)) { tree_step = 1; tree_wait = 60; }
    if (!tree_step || tree_step > TREE_SCENE_LEN) return;
    scene_on = 1;
    if (tree_wait) { tree_wait--; return; }
    if (msg_timer || msgq_len) return;
    show_message(tree_scene[tree_step - 1]);
    if (tree_step == TREE_SCENE_LEN) quest[KIND_BARK] = 1;   // "HEAL MY TREE" on Bark's page
    tree_step++;
    tree_wait = 20;
}

// Bark's four tasks at the Great Tree, checked twice a second (they can be done in any order).
static u32 count_west(u8 t, s32 x0, s32 y0, s32 x1, s32 y1) {
    u32 n = 0;
    for (s32 y = y0; y <= y1; y++)
        for (s32 x = x0; x <= x1; x++)
            n += maps[WEST][y * MAP_W + x] == t;
    return n;
}
static void check_tree(void) {
    if (frame % 30 || tree_done == TASKS_ALL) return;
    static const char *done_msg[4][2] = {
        { "THE CLAMPS ARE BROKEN!", "BARK: MY ROOTS CAN BREATHE." },
        { "THE THORNS ARE GONE!", "BARK: THAT ITCHED. FOR YEARS." },
        { "THE ROOT POOL IS FULL!", "BARK: ...COLD. GOOD." },
        { "THE ROOT TIPS WAKE UP!", "BARK: I FEEL MY TOES. EW." },
    };
    u32 left[4] = {
        count_west(CLAMP, 0, 0, MAP_W - 1, MAP_H - 1),
        count_west(THORN, 12, 21, 19, 21),       // the brambles in front of the trunk
        count_west(DRY_POOL, 0, 0, MAP_W - 1, MAP_H - 1),
        count_west(ROOT_TIP_DEAD, 0, 0, MAP_W - 1, MAP_H - 1),
    };
    for (s32 i = 0; i < 4; i++) {
        if (left[i] || (tree_done & (1 << i))) continue;
        tree_done |= 1 << i;
        sfx_play(SFX_QUEST);
        queue_message(done_msg[i][0]);
        if (in_party(KIND_BARK)) queue_message(done_msg[i][1]);
    }
    if (tree_done == TASKS_ALL) {
        queue_message("BARK: HUH. I FEEL A LITTLE");
        queue_message("LESS DEAD. BUT SOMETHING");
        queue_message("ROTS IN MY HEART. INSIDE.");
        queue_message("I CAN FEEL IT. UGH.");
    }
}

static const char *gloom_intro_lines[] = {   // Bark's words (Caleb)
    "THE GLOOMHEART ROARS!", "BARK: GRRR...", "SO THAT'S WHAT WAS OFF..", "LOOKS LIKE IT MADE ITSELF",
    "AT HOME.", "SQUISH IT, WIZARD.",
};
static const char *gloom_again_lines[] = { "THE GLOOMHEART ROARS!", "BARK: AGAIN. SQUISH IT." };
static void update_gloom_intro(void) {
    if (!gloom_intro) return;
    if (!boss.alive || cur_area != HOLLOW_IN) { gloom_intro = 0; return; }
    u32 n = gloom_intro_seen ? 2 : sizeof(gloom_intro_lines) / sizeof(gloom_intro_lines[0]);
    scene_on = 1;
    if (gloom_intro == 1 && py > 290) {                      // first the wizard walks up into the room
        py--; pdir = DIR_UP; pstep = 4 + ((frame >> 3) & 3);
        hist_head = (hist_head + 1) & (HIST - 1);            // the spirits follow him in
        hist_x[hist_head] = px; hist_y[hist_head] = py;
        return;
    }
    if (gloom_intro_wait) { gloom_intro_wait--; return; }
    if (msg_timer || msgq_len) return;
    if (gloom_intro > n) {                                   // the fight begins
        gloom_intro = 0; gloom_intro_seen = 1;
        boss.state = 0; boss.timer = 0;
        return;
    }
    if (gloom_intro == 1) {                                  // the roar shakes the whole tree
        shake_timer = 90; boss.state = 1;
        sfx_play(SFX_GLOOM_ROAR);
    }
    if (gloom_intro == 2) boss.state = 0;
    show_message(gloom_intro_seen ? gloom_again_lines[gloom_intro - 1] : gloom_intro_lines[gloom_intro - 1]);
    gloom_intro++;
    gloom_intro_wait = 20;
}

// v0.16.8 (Caleb's picks): the Great Tree's region is withered while dead, rootward once Bark's tasks are
// done, uneasy after the four mini bosses (and inside until the Gloomheart's roar is over), heartwood
// once healed; the fight is gloomfight.
static s32 area_song(s32 a) {
    static const u8 song_of[NUM_AREAS] = { SONG_VALLEY, SONG_STEPPE, SONG_HOLLOW, SONG_UNEASY, SONG_UNEASY,
                                           SONG_WITHERED, SONG_WITHERED };   // the marsh, the orchard: for now (music later)
    if (scene_song != NO_SCENE_SONG) return scene_song;
    if (a == WEST)
        return tree_alive ? SONG_HEARTWOOD : boss_beaten == (1 << NUM_BOSSES) - 1 ? SONG_UNEASY :
               tree_done == 15 ? SONG_ROOTWARD : SONG_WITHERED;
    if (a == HOLLOW_IN) return boss.alive && !gloom_intro ? SONG_GLOOMFIGHT : SONG_UNEASY;
    return song_of[a];
}
// ---------- the Leafen (v0.17): where they wait, what they need, the rescue, the welcome ----------
enum { LOOK_SPROUT, LOOK_LEAF, LOOK_BUD, LOOK_ACORN, LOOK_DEW };
enum { POSE_STAND, POSE_STEP, POSE_SWIM, POSE_CLIMB };
static const u8 leafen_area[NUM_LEAFEN] = { 0, 0, 1, 1, 2, 2, MARSH, MARSH, MARSH, ORCHARD, ORCHARD };
static const u8 leafen_cx[NUM_LEAFEN] = { 10, 26, 6, 20, 16, 28, 17, 26, 7, 27, 23 };   // Wick: by the lantern at (26,8)
static const u8 leafen_cy[NUM_LEAFEN] = { 25, 22, 20, 28, 4, 22, 26, 26, 12, 10, 21 };
static const u8 leafen_look[NUM_LEAFEN] = { LOOK_SPROUT, LOOK_ACORN, LOOK_LEAF, LOOK_BUD, LOOK_SPROUT, LOOK_DEW,
                                            LOOK_DEW, LOOK_LEAF, LOOK_BUD, LOOK_ACORN, LOOK_LEAF };
static const char *leafen_name[NUM_LEAFEN] = { "PIP", "MOSSY", "TWIG", "POSY", "NUB", "DEWDROP", "BOG", "REEDY",
                                               "PUDDLE", "WICK", "SOOT" };
static const char *leafen_ask[NUM_LEAFEN][2] = {   // what each says when you meet it (its problem, not the fix)
    { "PIP: HELP! I'M STUCK ON THIS", "LILY PAD. I CAN'T SWIM YET!" },
    { "MOSSY: SO... HUNGRY...", "SOME FRUIT? THREE, MAYBE?" },
    { "TWIG: OW! THORNS! EVERYWHERE!", "I CAN'T GET OUT!" },
    { "POSY: IT'S ALL SO GRAY...", "I WANT FLOWERS ALL AROUND ME!" },
    { "...MMF! HELP! UNDER HERE!", "THE ROCKS ARE SO HEAVY!" },
    { "DEWDROP: MY SHINY PEBBLE!", "I LOST IT IN THE VALLEY..." },
    { "BOG: THE GOO IS ALL ROUND ME!", "I'M STUCK ON THIS LITTLE ISLE!" },
    { "REEDY: I'M ALL TANGLED UP!", "THESE REEDS WON'T LET GO!" },
    { "PUDDLE: EW. EW! STINKY GOO!", "I WANT CLEAN WATER BY ME!" },
    { "WICK: IT'S SO DARK! SO SCARY!", "I WON'T MOVE TILL I CAN SEE!" },
    { "SOOT: COUGH! THE SMOKE! COUGH!", "THAT STACK WON'T STOP PUFFING!" },
};
static const char *leafen_thanks[NUM_LEAFEN] = {
    "PIP: A BRIDGE! YOU'RE GREAT!", "MOSSY: MMM! FRUIT! THANK YOU!", "TWIG: NO MORE THORNS! YAY!",
    "POSY: FLOWERS! SO PRETTY!", "NUB: PHEW! I CAN BREATHE!", "DEWDROP: MY SHINY! THANK YOU!",
    "BOG: THE GOO'S GONE! HOP HOP!", "REEDY: FREE! I CAN WIGGLE!", "PUDDLE: AH! CLEAN! SPLASH!",
    "WICK: LIGHT! I CAN SEE YOU!", "SOOT: FRESH AIR! AAH! YAY!",
};
static const char *leafen_gift[NUM_LEAFEN] = {
    "NEW BUILD: DEW POND", "NEW BUILD: ACORN BENCH", "NEW BUILD: LEAF SWING",
    "NEW BUILD: FLOWER ARCH", "NEW BUILD: PEBBLE PATH", "NEW BUILD: MUSHROOM LAMP",
    "+5 SEEDS +3 FRUIT", "+5 SEEDS +3 FRUIT", "+5 SEEDS +3 FRUIT",   // the marsh Leafen give a little haul
    "+3 FRUIT +2 MUSHROOMS", "+3 FRUIT +2 MUSHROOMS",                    // the orchard's: things to cook
};
#define SHINY_X 2    // Dewdrop's shiny pebble lies in the valley, far west
#define SHINY_Y 15
static s32 leafen_x(s32 k) { return k == LF_PIP ? (s32)(pip_cell % MAP_W) : leafen_cx[k]; }
static s32 leafen_y(s32 k) { return k == LF_PIP ? (s32)(pip_cell / MAP_W) : leafen_cy[k]; }

static int plain_ground(u8 t) {
    return t == DEAD || t == DEAD_CRACK || t == DIRT || t == DRY_GRASS || t == PEBBLES || t == GRASS ||
           t == GRASS_TUFT || t == FLOWERS;
}
static void leafen_stamp(s32 a, s32 x, s32 y, u8 nt) {   // only over open ground (never a home or a build)
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) return;
    u8 *c = &maps[a][y * MAP_W + x];
    if (!plain_ground(*c)) return;
    if (is_restorable(*c) && !is_restorable(nt)) restorable[a]--;   // clearing it later adds it back
    *c = nt;
    if (a == cur_area) { use_area(a); draw_meta(x, y); }
}
static void nub_clear_ring(void);
// Once the tree is healed: the Leafen take their places, with their troubles drawn into the maps.
static void leafen_setup(void) {
    leafen_init = 1;
    for (s32 k = 0; k < NUM_LEAFEN; k++) leafen_state[k] = LS_WAIT;
    static const u8 pond[][2] = { { 10, 25 }, { 11, 25 }, { 10, 24 }, { 11, 24 }, { 10, 26 }, { 11, 26 } };
    pip_cell = 25 * MAP_W + 10;                          // Pip: on a lily pad in the valley's pond
    for (u32 i = 0; i < sizeof(pond) / sizeof(pond[0]); i++)
        if (maps[0][pond[i][1] * MAP_W + pond[i][0]] == WATER) { pip_cell = pond[i][1] * MAP_W + pond[i][0]; break; }
    for (s32 dy = -1; dy <= 1; dy++)                     // Twig: inside a ring of thorns, in the steppe
        for (s32 dx = -1; dx <= 1; dx++)
            if (dx || dy) leafen_stamp(1, leafen_cx[LF_TWIG] + dx, leafen_cy[LF_TWIG] + dy, THORN);
    s32 nx = leafen_cx[LF_NUB], ny = leafen_cy[LF_NUB];  // Nub: under a pile of rocks, in Stone Hollow
    leafen_stamp(2, nx, ny, ROCK);
    leafen_stamp(2, nx - 1, ny, ROCK); leafen_stamp(2, nx + 1, ny, ROCK);
    leafen_stamp(2, nx, ny - 1, ROCK); leafen_stamp(2, nx, ny + 1, ROCK);
    nub_clear_ring();
    use_area(cur_area);
}

// v0.17.3 (Caleb): nothing stands in the ring round Nub's rock pile, so the pile stands out.
static void nub_clear_ring(void) {
    s32 nx = leafen_cx[LF_NUB], ny = leafen_cy[LF_NUB];
    for (s32 y = ny - 2; y <= ny + 2; y++)
        for (s32 x = nx - 2; x <= nx + 2; x++) {
            if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) continue;
            s32 cx = x - nx, cy = y - ny;
            if ((cx == 0 && cy >= -1 && cy <= 1) || (cy == 0 && cx >= -1 && cx <= 1)) continue;   // the pile
            u8 *c = &maps[2][y * MAP_W + x];
            if (is_rock(*c) || *c == MENHIR || *c == DEAD_TREE || *c == STUMP || *c == STUMP_GRASS || *c == TREE) {
                if (!is_restorable(*c)) restorable[2]++;
                *c = PEBBLES;
                if (cur_area == 2) { use_area(2); draw_meta(x, y); use_area(cur_area); }
            }
        }
}

static int leafen_ready(s32 k) {
    s32 x = leafen_x(k), y = leafen_y(k);
    const u8 *m = maps[leafen_area[k]];
#define LF_AT(xx, yy) (((xx) < 0 || (yy) < 0 || (xx) >= MAP_W || (yy) >= MAP_H) ? ROCK : m[(yy) * MAP_W + (xx)])
    switch (k) {
    case LF_PIP:
        return LF_AT(x, y) == BRIDGE || LF_AT(x - 1, y) == BRIDGE || LF_AT(x + 1, y) == BRIDGE ||
               LF_AT(x, y - 1) == BRIDGE || LF_AT(x, y + 1) == BRIDGE;
    case LF_MOSSY: return fruit >= 3;
    case LF_TWIG:
        for (s32 dy = -1; dy <= 1; dy++)
            for (s32 dx = -1; dx <= 1; dx++) if (LF_AT(x + dx, y + dy) == THORN) return 0;
        return 1;
    case LF_POSY: {
        s32 n = 0;
        for (s32 dy = -1; dy <= 1; dy++)
            for (s32 dx = -1; dx <= 1; dx++) {
                u8 c = LF_AT(x + dx, y + dy);
                if ((dx || dy) && (c == FLOWERS || c == FLOWERBED || c == BLOSSOM || c == FLOWER_ARCH)) n++;
            }
        return n >= 4;
    }
    case LF_NUB:
        return !is_rock(LF_AT(x, y)) && !is_rock(LF_AT(x - 1, y)) && !is_rock(LF_AT(x + 1, y)) &&
               !is_rock(LF_AT(x, y - 1)) && !is_rock(LF_AT(x, y + 1));
    case LF_DEW: return has_shiny;
    case LF_BOG:                                            // the goo round his isle is gone (swim to him), or a pad
        return is_pad(LF_AT(x - 1, y)) || is_pad(LF_AT(x + 1, y)) || is_pad(LF_AT(x, y - 1)) || is_pad(LF_AT(x, y + 1)) ||
               (LF_AT(x - 1, y) != SLUDGE && LF_AT(x + 1, y) != SLUDGE && LF_AT(x, y - 1) != SLUDGE && LF_AT(x, y + 1) != SLUDGE);
    case LF_WICK: return fire_lit(x * 16 + 8, y * 16 + 8) != -1;   // a lantern lit by it (or its smoke gone)
    case LF_SOOT: return maps[ORCHARD][stack_y[0] * MAP_W + stack_x[0]] != SMOKE_STACK;   // the stack beside it
    default: {                                              // Reedy: no reeds round; Puddle: no sludge round
        u8 bad = k == LF_REEDY ? REEDS : SLUDGE;
        for (s32 dy = -1; dy <= 1; dy++)
            for (s32 dx = -1; dx <= 1; dx++) if (LF_AT(x + dx, y + dy) == bad) return 0;
        return 1;
    }
    }
#undef LF_AT
}
static int leafen_waiting(s32 k) { return leafen_state[k] == LS_WAIT || leafen_state[k] == LS_MET; }
// How many Leafen you know of: the first six, the marsh's once Lily has joined, the orchard's once you are in it.
static s32 leafen_total(void) {
    return FIRST_LEAFEN + (lily_step > LILY_SCENE_LEN ? FIRST_ORCH_LEAFEN - FIRST_LEAFEN : 0) +
           ((flick_flags & 2) || spirits[KIND_FLICK].state != SP_ASLEEP ? NUM_LEAFEN - FIRST_ORCH_LEAFEN : 0);
}
static int near_leafen(s32 k, s32 r) {
    s32 dx = px + 8 - (leafen_x(k) * 16 + 8), dy = py + 8 - (leafen_y(k) * 16 + 8);
    return dx * dx + dy * dy < r * r;
}

// The rescue scene: thanks, friends pop up out of the ground, and all three walk off to the tree.
static s32 lf_k = -1, lf_wait;
static s32 lf_sx[3], lf_sy[3], lf_rise;               // the three walkers (pixels) and the friends' rise
static char lf_line[32];
static void leafen_rescue(s32 k) {
    if (k == LF_MOSSY) fruit -= 3;
    if (k == LF_DEW) has_shiny = 0;
    lf_k = k; lf_scene = 1; lf_wait = 10; lf_rise = 0;
    s32 x = leafen_x(k) * 16, y = leafen_y(k) * 16;
    lf_sx[0] = x; lf_sy[0] = y;
    lf_sx[1] = x - 18; lf_sy[1] = y + 6;
    lf_sx[2] = x + 18; lf_sy[2] = y + 6;
    if (k == LF_PIP) { lf_sx[1] = px - 14; lf_sy[1] = py; lf_sx[2] = px + 14; lf_sy[2] = py; }   // on land
}
// v0.17.2 (Caleb): the Leafen at Bark's tree chatter when you press A by one; they all share these.
static const char *leafen_chatter[] = {
    "HI! HI! HI!", "I FOUND A ROCK! IT'S A ROCK!", "BARK IS GRUMPY. WE LOVE HIM!",
    "DO YOU HAVE ANY SUNSHINE?", "I WAVED AT A CLOUD TODAY!", "MY LEAF IS EXTRA SHINY TODAY!",
    "I TRIED TO SWIM UPWARDS!", "WHAT'S A WIZARD? IS IT A HAT?", "THANK YOU FOR FINDING US!",
    "I NAPPED IN A FLOWER! COMFY!", "DIRT TASTES LIKE... DIRT!", "WE BUILT A HOUSE! IT'S TINY!",
    "I'M GROWING! I THINK! MAYBE!", "BARK SNORES. SO LOUD!", "WANNA SEE ME HOP? HOP! HOP!",
    "I COUNTED TO THREE! TWICE!", "RAIN IS SKY WATER! WOW!", "I LIKE YOUR HAT! SO POINTY!",
    "WE MISS OUR FRIENDS UP NORTH.", "A BUG SAID HI TO ME! I THINK!",
};
#define NUM_CHATTER (sizeof(leafen_chatter) / sizeof(leafen_chatter[0]))
static u32 chatter_at;
static short folk_sx[NUM_LEAFEN * 3], folk_sy[NUM_LEAFEN * 3];   // where each was last drawn (world pixels)
static u8 folk_hop[NUM_LEAFEN * 3];                               // a little hop after you talk to it

// A at a waiting Leafen: its trouble, or (when the trouble is fixed) the rescue. Returns 1 if it talked.
// Facing a rock, thorn or water next to it, A is the spirit's power instead (to break Nub out, and so on).
static int leafen_talk(s32 fmx, s32 fmy) {
    if (!leafen_init || lf_scene) return 0;
    u8 faced = tile_at(fmx, fmy);
    int trouble = is_rock(faced) || faced == THORN || faced == WATER || faced == SLUDGE || faced == REEDS ||
                  faced == VAEL_PIPE || is_bog(faced);
    for (s32 k = 0; k < NUM_LEAFEN && !trouble; k++) {
        if (leafen_area[k] != cur_area || !leafen_waiting(k) || !near_leafen(k, k == LF_NUB ? 34 : 28)) continue;
        if (leafen_ready(k)) { leafen_rescue(k); return 1; }
        leafen_state[k] = LS_MET;
        show_message(leafen_ask[k][0]);
        queue_message(leafen_ask[k][1]);
        sfx_play(SFX_GIFT);
        return 1;
    }
    if (cur_area == WEST)                                 // a Leafen of the village: a bit of chatter
        for (s32 n = 0; n < NUM_LEAFEN * 3; n++) {
            if (leafen_state[n / 3] != LS_HOME) continue;
            s32 dx = px + 8 - (folk_sx[n] + 8), dy = py + 8 - (folk_sy[n] + 8);
            if (dx * dx + dy * dy >= 24 * 24) continue;
            chatter_at = (chatter_at + 1 + rng() % 3) % NUM_CHATTER;   // never the same line twice in a row
            show_message(leafen_chatter[chatter_at]);
            folk_hop[n] = 16;
            sfx_play(SFX_GIFT);
            return 1;
        }
    return 0;
}

// The welcome at Bark's tree, and the skills he teaches every second Leafen.
static const char *skill_name[3] = { "NEW SKILL: LEAF STEP!", "NEW SKILL: LEAF SHIELD!", "NEW SKILL: SEEDFALL!" };
static const char *skill_help[3] = { "AS YOU, DEAD GROUND GREENS.", "A LEAF RING BLOCKS A HIT.",
                                     "AS YOU, A ON DEAD LAND BLOOMS." };
static s32 welcome_skill_from;

static void update_leafen(void) {
    if (!tree_alive) return;
    if (!leafen_init) leafen_setup();
    if (shield_wait) shield_wait--;
    // the rescue scene
    if (lf_scene) {
        scene_on = 1;
        if (lf_scene == 3 && lf_rise < 12) {             // the friends pop up out of the ground
            lf_rise++;
            if ((lf_rise & 3) == 0) {
                add_mote_c(lf_sx[1], lf_sy[1] + 8, 0, -8, 20, C_GLINT);
                add_mote_c(lf_sx[2], lf_sy[2] + 8, 0, -8, 20, C_GLINT);
            }
            return;
        }
        if (lf_scene == 5) {                             // walking off toward the tree
            int north = leafen_area[lf_k] == 2 || leafen_area[lf_k] == MARSH;   // the tree is north of them
            s32 dx = north || leafen_area[lf_k] == ORCHARD ? 0 : -1, dy = north ? -1 : leafen_area[lf_k] == ORCHARD ? 1 : 0;
            for (s32 i = 0; i < 3; i++) { lf_sx[i] += dx; lf_sy[i] += dy; }
            if (++lf_wait < 150) return;
            lf_scene = 6; lf_wait = 0;
        }
        if (lf_wait) { lf_wait--; return; }
        if (msg_timer || msgq_len) return;
        char *p = lf_line;
        switch (lf_scene) {
        case 1: show_message(leafen_thanks[lf_k]); sfx_play(SFX_QUEST); lf_scene = 2; lf_wait = 10; break;
        case 2:
            str_cat(&p, leafen_name[lf_k]); str_cat(&p, " CALLS ITS FRIENDS!"); *p = 0;
            show_message(lf_line); sfx_play(SFX_WAKE); lf_scene = 3; lf_wait = 0;
            break;
        case 3: show_message("WE'RE GOING HOME! TO BARK!"); lf_scene = 4; lf_wait = 20; break;
        case 4: lf_scene = 5; lf_wait = 0; break;
        default: {
            leafen_state[lf_k] = LS_FREE;
            if (lf_k >= FIRST_ORCH_LEAFEN) { fruit += 3; mushrooms += 2; }
            else if (lf_k >= FIRST_LEAFEN) { seeds += 5; fruit += 3; }
            u32 n = 0;
            for (s32 k = 0; k < NUM_LEAFEN; k++) n += leafen_state[k] >= LS_FREE;
            str_cat(&p, "LEAFEN FREED: "); p = fmt_num(p, n);
            str_cat(&p, " OF "); p = fmt_num(p, leafen_total()); *p = 0;
            show_message(lf_line);
            queue_message(leafen_gift[lf_k]);
            sfx_play(SFX_CHEST);
            lf_scene = 0; lf_k = -1;
        }
        }
        return;
    }
    // the welcome at the tree (the next time you come back to it)
    if (welcome_step) {
        scene_on = 1;
        if (msg_timer || msgq_len) return;
        static const char *lines[] = { "THE LEAFEN MADE IT HOME!", "BARK: HMPH. LOUD LITTLE THINGS.",
                                       "...GOOD TO HAVE THEM BACK." };
        if (welcome_step <= 3) { show_message(lines[welcome_step - 1]); welcome_step++; return; }
        s32 lv = (s32)skill_lv();
        if (welcome_skill_from < lv) {                   // a new skill for each two Leafen home
            s32 s = welcome_skill_from++;
            show_message(s == 0 ? "BARK: WIZARD. HOLD STILL." : "BARK: HERE. ANOTHER ONE.");
            queue_message(skill_name[s]);
            queue_message(skill_help[s]);
            leaf_timer = 150;
            sfx_play(SFX_QUEST);
            return;
        }
        if (leafen_home_count() == FIRST_LEAFEN && welcome_step == 4) {
            show_message("BARK: THAT'S THESE ONES.");
            queue_message("MORE WANDERED TO THE CORNERS.");
            queue_message("AND VAEL HAS THE REST, NORTH.");
        }
        welcome_step = 0;
        return;
    }
    // v0.18.1: the first time in the marsh with Ripple along, she teaches the wizard to swim
    if (swim_step && swim_step <= 6) {
        scene_on = 1;
        if (msg_timer || msgq_len) return;
        static const char *lines[6] = { "RIPPLE: OOH! SO MUCH WATER!", "WIZARD, HOLD ON TO ME!",
                                        "KICK YOUR FEET... LIKE THIS!", "NEW SKILL: SWIM!",
                                        "PICK ME, PRESS B BY THE WATER.", "BUT NEVER IN THE SLUDGE!" };
        if (swim_step == 4) { swim_skill = 1; leaf_timer = 120; sfx_play(SFX_QUEST); }
        show_message(lines[swim_step - 1]);
        swim_step++;
        return;
    }
    if (cur_area == MARSH && !swim_skill && !swim_step && in_party(KIND_RIPPLE) && !scene_on && !msg_timer && !msgq_len) {
        swim_step = 1;
        return;
    }
    // v0.19 Flick wakes: you hold out the warm ember from the fallen trunk
    if (flick_step && flick_step <= FLICK_SCENE_LEN) {
        scene_on = 1;
        if (msg_timer || msgq_len) return;
        static const char *lines[FLICK_SCENE_LEN] = {
            "YOU HOLD OUT THE WARM EMBER...", "THE LITTLE BIRD STIRS...", "FLICK: CHEEP! WARM! SO WARM!",
            "FLICK: I'M FLICK! I WAS COLD.", "I LOVE LIGHT! LANTERNS! FIRES!", "FLICK JOINED! CHEEP!",
            "PICK FLICK, PRESS A BY A", "LANTERN OR A CAMPFIRE.",
        };
        Spirit *s = &spirits[KIND_FLICK];
        if (flick_step == 2) {                                   // it glows back to life
            s->state = SP_AWAKE;
            for (s32 k = 0; k < 8; k++) add_mote_c(s->x + 4, s->y, dir16[k * 2] >> 1, DIR16_Y(k * 2) >> 1, 24, C_GLINT);
            sfx_play(SFX_WAKE);
        }
        if (flick_step == 6) { s->energy = 3; party_add(KIND_FLICK); sfx_play(SFX_JOIN); dark_enter(cur_area); }
        show_message(lines[flick_step - 1]);
        flick_step++;
        return;
    }
    // v0.19: past the fallen trunk, Flick's chirp leads you to its nest (Caleb: by how near you are). The nearer,
    // the more often, the louder and the higher it chirps; now and then you wonder which way it comes from.
    if (cur_area == ORCHARD && spirits[KIND_FLICK].state == SP_ASLEEP && py < ORCHARD_WALL_ROW * 16 - 8) {
        static u32 chirp_t, chirp2, chirp_vol, hint_t;
        s32 dx = ORCHARD_NEST_X * 16 + 8 - (px + 8), dy = ORCHARD_NEST_Y * 16 + 8 - (py + 8);
        s32 ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, d = (ax + ay) >> 4;   // about how many tiles away
        if (!(flick_flags & 2) && !scene_on && !msg_timer && !msgq_len) {
            flick_flags |= 2;
            show_message("...CHEEP?");
            queue_message("WHAT'S THAT?");
            hint_t = 25 * 60 - 360;                    // which way it seems to come from, a little later
        }
        if (++chirp_t >= (u32)(40 + d * 6)) {          // a weak two-note chirp, falling (Caleb: it sounds weak)
            chirp_t = 0;
            chirp_vol = d < 6 ? 7 : d < 14 ? 5 : d < 24 ? 3 : 2;
            weak_chirp(2349, chirp_vol);
            chirp2 = 7;
        }
        if (chirp2 && --chirp2 == 0) weak_chirp(1976, chirp_vol > 1 ? chirp_vol - 1 : 1);
        if (d < 4 && !(flick_flags & 4) && !scene_on && !msg_timer && !msgq_len) {   // close: it shivers
            flick_flags |= 4;
            show_message("BRRRRR...");
        }
        if ((flick_flags & 2) && ++hint_t >= 25 * 60 && !scene_on && !msg_timer && !msgq_len) {
            hint_t = 0;
            // which way it seems to come from, by the way you face (Caleb: "ahead", not compass points)
            static const char *from[4] = { "IS IT COMING FROM AHEAD?", "IS IT COMING FROM BEHIND?",
                                           "IS IT OFF TO THE LEFT?", "IS IT OFF TO THE RIGHT?" };
            s32 fx = pdir == DIR_LEFT ? -1 : pdir == DIR_RIGHT ? 1 : 0, fy = pdir == DIR_UP ? -1 : pdir == DIR_DOWN ? 1 : 0;
            s32 along = dx * fx + dy * fy, side = -dx * fy + dy * fx;   // side > 0: to your right
            s32 k = (along < 0 ? -along : along) >= (side < 0 ? -side : side) ? (along >= 0 ? 0 : 1) : (side > 0 ? 3 : 2);
            static u32 close_n;
            show_message(d >= 5 ? from[k] : (close_n++ & 1) ? "BRRRRR..." : "IT'S RIGHT AROUND HERE...");
        }
    }
    // v0.18: with the first six home, Bark summons Lily and points you south to the marsh
    if (lily_step && lily_step <= LILY_SCENE_LEN) {
        scene_on = 1;
        if (msg_timer || msgq_len || leaf_timer > 60) return;
        static const char *lines[LILY_SCENE_LEN] = {
            "BARK: HMPH. SOME LEAFEN WENT", "SOUTH. INTO THE OLD MARSH.", "VAEL DROWNED IT IN SLUDGE.",
            "YOU'LL NEED HELP. HOLD ON...", "LILY: OH! HELLO! I'M LILY!", "I CLEAN DIRTY WATER!",
            "AND I GROW LILY PADS! WHEE!", "LILY JOINED! R: SWAP", "BARK: THE MARSH IS SOUTH,",
            "PAST MY ROOTS. GO ON, THEN.",
        };
        if (lily_step == 4) { leaf_timer = 150; sfx_play(SFX_QUEST); }   // leaves swirl: Bark calls her
        if (lily_step == 5) {                                    // Lily appears beside you
            Spirit *s = &spirits[KIND_LILY];
            s->x = px + 18; s->y = py; s->area = cur_area; s->energy = 3;
            party_add(KIND_LILY);                               // into the pouch if you carry five
            for (s32 k = 0; k < 8; k++) add_mote_c(px + 18, py, dir16[k * 2] >> 1, DIR16_Y(k * 2) >> 1, 24, C_GLINT);
            sfx_play(SFX_WAKE);
            draw_active();
        }
        show_message(lines[lily_step - 1]);
        lily_step++;
        return;
    }
    if (cur_area == WEST && !scene_on && !msg_timer && !msgq_len && !welcome_step && !lily_step) {
        u32 home6 = 0;
        for (s32 k = 0; k < FIRST_LEAFEN; k++) home6 += leafen_state[k] == LS_HOME;
        if (home6 == FIRST_LEAFEN) { lily_step = 1; return; }
    }
    if (cur_area == WEST && !scene_on && !msg_timer && !msgq_len) {
        s32 any = 0;
        for (s32 k = 0; k < NUM_LEAFEN; k++) if (leafen_state[k] == LS_FREE) any = 1;
        if (any) {
            welcome_skill_from = (s32)skill_lv();
            for (s32 k = 0; k < NUM_LEAFEN; k++) if (leafen_state[k] == LS_FREE) leafen_state[k] = LS_HOME;
            welcome_step = 1;
            sfx_play(SFX_JOIN);
            return;
        }
    }
    if (scene_on) return;
    // Dewdrop's shiny pebble: picked up by walking onto it
    if (cur_area == 0 && !has_shiny && leafen_waiting(LF_DEW) &&
        ((px + 8) >> 4) == SHINY_X && ((py + 12) >> 4) == SHINY_Y) {
        has_shiny = 1;
        sparkle_at(SHINY_X, SHINY_Y);
        sfx_play(SFX_GIFT);
        show_message("YOU FOUND A SHINY PEBBLE!");
    }
    // Nub, still buried, calls out the first time you come near his rock pile
    if (cur_area == 2 && leafen_state[LF_NUB] == LS_WAIT && near_leafen(LF_NUB, 44) &&
        is_rock(tile_at(leafen_cx[LF_NUB], leafen_cy[LF_NUB])) && !msg_timer && !msgq_len) {
        leafen_state[LF_NUB] = LS_MET;
        show_message(leafen_ask[LF_NUB][0]);
        queue_message(leafen_ask[LF_NUB][1]);
        sfx_play(SFX_GIFT);
    }
    // a trouble fixed on the map: the Leafen notices as soon as you come close
    for (s32 k = 0; k < NUM_LEAFEN; k++)
        if (leafen_area[k] == cur_area && leafen_waiting(k) && k != LF_MOSSY && k != LF_DEW &&
            leafen_ready(k) && near_leafen(k, k == LF_BOG ? 96 : 48)) { leafen_rescue(k); return; }
}

// The Leafen village round Bark's tree: per freed Leafen, two tiny homes and three Leafen (itself and
// its two friends) who walk round them, swim in the root pool, or climb on Bark's trunk.
enum { V_WALK, V_SWIM, V_CLIMB };
static const short village_home[NUM_LEAFEN][2][3] = {   // x, y, home style (0 mushroom .. 5 dewhut)
    { { 210, 406, 5 }, { 288, 402, 2 } },
    { { 40, 356, 0 }, { 66, 368, 1 } },
    { { 430, 356, 1 }, { 456, 368, 0 } },
    { { 62, 416, 3 }, { 30, 432, 2 } },
    { { 436, 420, 4 }, { 452, 418, 3 } },
    { { 204, 446, 0 }, { 294, 446, 5 } },
    { { 134, 346, 5 }, { 0, 0, 0 } },
    { { 362, 346, 0 }, { 0, 0, 0 } },
    { { 470, 470, 1 }, { 0, 0, 0 } },   // the marsh Leafen: one home each
    { { 70, 150, 2 }, { 0, 0, 0 } },    // v0.19.4 the orchard's, north-west and north-east of the tree
    { { 432, 150, 4 }, { 0, 0, 0 } },
};
static const short village_folk[NUM_LEAFEN][3][3] = {   // x, y, what it does
    { { 242, 422, V_SWIM }, { 214, 470, V_WALK }, { 168, 262, V_CLIMB } },
    { { 44, 378, V_WALK }, { 28, 380, V_WALK }, { 182, 236, V_CLIMB } },
    { { 452, 384, V_WALK }, { 258, 432, V_SWIM }, { 338, 244, V_CLIMB } },
    { { 42, 470, V_WALK }, { 106, 454, V_WALK }, { 156, 286, V_CLIMB } },
    { { 456, 436, V_WALK }, { 392, 456, V_WALK }, { 352, 280, V_CLIMB } },
    { { 296, 474, V_WALK }, { 162, 472, V_WALK }, { 330, 300, V_CLIMB } },
    { { 144, 368, V_WALK }, { 136, 390, V_WALK }, { 180, 300, V_CLIMB } },
    { { 356, 382, V_WALK }, { 346, 402, V_WALK }, { 332, 286, V_CLIMB } },
    { { 450, 478, V_WALK }, { 430, 480, V_WALK }, { 176, 312, V_CLIMB } },
    { { 92, 172, V_WALK }, { 52, 184, V_WALK }, { 214, 232, V_CLIMB } },
    { { 410, 172, V_WALK }, { 452, 186, V_WALK }, { 300, 234, V_CLIMB } },
};
static short folk_x[NUM_LEAFEN * 3], folk_y[NUM_LEAFEN * 3];
static u8 folk_look(s32 k, s32 i) { return i == 0 ? leafen_look[k] : (u8)((leafen_look[k] + i * 2 + k) % 5); }

static void draw_leafen(void) {
    for (s32 i = 0; i < 6; i++) sprite_hide(OAM_LEAFEN + i);
    sprite_hide(OAM_SHINY);
    if (cur_area == WEST && tree_alive) {           // the village's slots are shared (OAM_POOL)
        for (s32 i = 0; i < NUM_LEAFEN * 3; i++) sprite_hide(OAM_FOLK + i);
        for (s32 i = 0; i < FIRST_LEAFEN * 2 + NUM_LEAFEN - FIRST_LEAFEN; i++) sprite_hide(OAM_LHOME + i);   // 15 homes
    }
    if (!tree_alive || !leafen_init) return;
    for (s32 i = 0; i < 16; i++) PAL_OBJ[PAL_LEAFEN * 16 + i] = obj_pal_leafen[i];   // the clamps' bank is free now
    s32 slot = 0;
    u32 hop = (frame >> 4) & 1;
    // the waiting Leafen in this area (and the rescue's three walkers)
    for (s32 k = 0; k < NUM_LEAFEN && slot < 6; k++) {
        if (k == lf_k && lf_scene) {
            for (s32 i = 0; i < 3; i++) {
                if (i && lf_scene < 3) continue;
                s32 sink = i && lf_scene == 3 ? 12 - lf_rise : 0;   // friends rising out of the ground
                u32 pose = lf_scene == 5 ? ((frame >> 3) & 1) : (k == LF_PIP && i == 0 && lf_scene < 5) ? POSE_SWIM : hop;
                sprite_set(OAM_LEAFEN + slot++, lf_sx[i] - cam_x, lf_sy[i] - cam_y + sink,
                           SPR_LEAFEN + C_LEAFEN + folk_look(k, i) * 4 + pose, lf_scene == 5, PAL_LEAFEN);
            }
            continue;
        }
        if (leafen_area[k] != cur_area || !leafen_waiting(k)) continue;
        s32 x = leafen_x(k) * 16, y = leafen_y(k) * 16;
        if (k == LF_NUB && is_rock(tile_at(leafen_x(k), leafen_y(k)))) {   // still buried: his leaves poke out
            sprite_set(OAM_LEAFEN + slot++, x - cam_x, y - cam_y - 9, SPR_LEAFEN + C_PEEK, (frame >> 5) & 1, PAL_LEAFEN);
            continue;
        }
        u32 pose = k == LF_PIP ? POSE_SWIM : hop;
        sprite_set(OAM_LEAFEN + slot++, x - cam_x, y - cam_y, SPR_LEAFEN + C_LEAFEN + leafen_look[k] * 4 + pose,
                   px < x, PAL_LEAFEN);
    }
    if (cur_area == 0 && !has_shiny && leafen_waiting(LF_DEW) && ((frame >> 3) & 7))   // glinting
        sprite_set(OAM_SHINY, SHINY_X * 16 - cam_x, SHINY_Y * 16 - cam_y, SPR_LEAFEN + C_SHINY, 0, PAL_LEAFEN);
    for (s32 i = 0; i < 4 && cur_area == MARSH; i++) {   // the marsh's fireflies: drifting loops, a slow blink
        static const short fly_at[5][2] = { { 120, 140 }, { 300, 90 }, { 380, 260 }, { 180, 330 }, { 260, 440 } };
        u32 tt = frame + i * 97, a = (tt >> 2) % 48, b = (tt >> 3) % 48;
        if (cur_area != MARSH || ((tt >> 5) % 5) == 0) { sprite_hide(OAM_FIREFLY + i); continue; }
        s32 x = fly_at[i][0] + loop_x48[a] + (loop_x48[b] >> 1), y = fly_at[i][1] + loop_y48[a] + (loop_y48[(b + 12) % 48] >> 1);
        sprite_set(OAM_FIREFLY + i, x - cam_x, y - cam_y, SPR_LEAFEN + C_FIREFLY, 0, PAL_LEAFEN);
    }
    if ((frame & 15) == 0)                             // the pipes' red lights blink, so they catch the eye (v0.18.2)
        PAL_BG[SLUDGE_BANK * 16 + 12] = (frame & 32) ? RGB5(238, 60, 52) : RGB5(255, 170, 120);
    if (cur_area == MARSH && (frame % 24) == 0) {    // sludge drips from every pipe on screen (v0.18.2)
        for (s32 y = cam_y >> 4; y <= (cam_y + SCREEN_H) >> 4 && y < MAP_H; y++)
            for (s32 x = cam_x >> 4; x <= (cam_x + SCREEN_W) >> 4 && x < MAP_W; x++)
                if (maps[MARSH][y * MAP_W + x] == VAEL_PIPE) add_mote(x * 16 + (s32)(rng() % 6), y * 16 - 4, 0, 6, 18);
    }
    if (cur_area != WEST) return;
    for (s32 k = 0; k < NUM_LEAFEN; k++) {
        if (leafen_state[k] != LS_HOME) continue;
        for (s32 h = 0; h < 2; h++) {
            if (!village_home[k][h][0]) continue;                 // the marsh groups have one home
            s32 slot = k < FIRST_LEAFEN ? k * 2 + h : FIRST_LEAFEN * 2 + (k - FIRST_LEAFEN);
            sprite_set(OAM_LHOME + slot, village_home[k][h][0] - cam_x, village_home[k][h][1] - cam_y,
                       SPR_LEAFEN + C_LHOME + village_home[k][h][2], 0, PAL_LEAFEN);
        }
        for (s32 i = 0; i < 3; i++) {
            s32 n = k * 3 + i, ax = village_folk[k][i][0], ay = village_folk[k][i][1], mode = village_folk[k][i][2];
            s32 x = ax, y = ay;
            u32 pose = POSE_STAND, flip = 0;
            u32 tt = frame + n * 53;
            if (mode == V_SWIM) {                        // paddling round the root pool
                u32 a = (tt >> 2) % 48;
                x += loop_x48[a] >> 1; y += loop_y48[a] >> 1;
                pose = POSE_SWIM; flip = a >= 24;
            } else if (mode == V_CLIMB) {                // up Bark's bark, a rest, and down again
                u32 c = tt % 320;
                s32 up = c < 100 ? (s32)c / 2 : c < 160 ? 50 : c < 260 ? 50 - (s32)(c - 160) / 2 : 0;
                y -= up;
                pose = POSE_CLIMB; flip = (c < 100 || (c >= 160 && c < 260)) && ((tt >> 3) & 1);
            } else {                                      // pottering round their homes
                if (!folk_x[n]) { folk_x[n] = ax; folk_y[n] = ay; }
                u32 seg = tt >> 7;
                s32 tx = ax + (s32)((seg * 37 + n * 11) % 33) - 16, ty = ay + (s32)((seg * 23 + n * 7) % 17) - 8;
                if ((frame & 1) == 0) { folk_x[n] += sgn(tx - folk_x[n]); folk_y[n] += sgn(ty - folk_y[n]); }
                int moving = folk_x[n] != tx || folk_y[n] != ty;
                x = folk_x[n]; y = folk_y[n];
                pose = moving ? ((tt >> 3) & 1) : POSE_STAND;
                flip = tx < x;
            }
            folk_sx[n] = x; folk_sy[n] = y;
            s32 hop = 0;
            if (folk_hop[n]) { folk_hop[n]--; hop = folk_hop[n] > 8 ? 16 - folk_hop[n] : folk_hop[n]; }
            sprite_set(OAM_FOLK + n, x - cam_x, y - cam_y - hop, SPR_LEAFEN + C_LEAFEN + folk_look(k, i) * 4 + pose, flip,
                       PAL_LEAFEN);
        }
    }
}

static void update_bark(void) {
    if (bark_fall) bark_fall--;
    scene_on = bark_step && bark_step <= BARK_SCENE_LEN;
    update_tree_scene();
    update_gloom_intro();
    update_leafen();
    if (!(bark_step && bark_step <= BARK_SCENE_LEN)) return;
    if (bark_wait) { bark_wait--; return; }
    if (msg_timer || msgq_len) return;                      // let the messages before it finish
    u32 act = bark_scene[bark_step - 1].act;
    const char *m = bark_scene[bark_step - 1].msg;
    bark_step++;
    bark_wait = 20;
    if (act == BS_CRACK) sfx_play(SFX_ROCK);
    if (act == BS_FALL) {
        // land on a free tile beside the player: right, left, below, above
        static const signed char ox[4] = { 1, -1, 0, 0 }, oy[4] = { 0, 0, 1, -1 };
        s32 fx = (px + 8) >> 4, fy = (py + 12) >> 4, bx = fx, by = fy;
        for (s32 k = 0; k < 4; k++)
            if (!is_solid(tile_at(fx + ox[k], fy + oy[k]))) { bx = fx + ox[k]; by = fy + oy[k]; break; }
        Spirit *b = &spirits[KIND_BARK];
        b->x = bx * 16; b->y = by * 16; b->area = cur_area; b->state = SP_AWAKE;
        bark_fall = 30;
        bark_wait = 30;
    }
    if (act == BS_POWER) { leaf_timer = 150; sfx_play(SFX_QUEST); }   // v0.18.4: room for a fifth spirit
    if (act == BS_THUMP) { sfx_play(SFX_BUILD); sparkle_at(spirits[KIND_BARK].x >> 4, spirits[KIND_BARK].y >> 4); }
    if (m) show_message(m);
}

// Building tips, each told once, 3 s after the messages before it end: after the first home,
// how to upgrade; once all four homes stand, a nudge to upgrade every one (Bark comes then).
static u32 tip_wait;
static void update_tips(void) {
    s32 built = 0, grown = 0;
    for (s32 k = 0; k < NUM_HOMES; k++) { built += home_done[k]; grown += home_level[k] >= 2; }
    u32 tip = 0;
    if (built >= 1 && grown == 0 && !(told_tips & TIP_UPGRADE)) tip = TIP_UPGRADE;
    else if (built == NUM_HOMES && grown < NUM_HOMES && !(told_tips & TIP_ALL)) tip = TIP_ALL;
    if (prompt_quiet) prompt_quiet--;
    // a repeating prompt (you stand by a home) does not count as busy
    if (!tip || scene_on || (msg_timer && !msg_is_prompt) || msgq_len) { tip_wait = 0; return; }
    if (++tip_wait < 180) return;
    told_tips |= tip;
    prompt_quiet = 3 * 150;
    if (tip == TIP_UPGRADE) {
        show_message("HOMES CAN GROW STRONGER!");
        queue_message("HOLD SELECT, PICK HOME,");
        queue_message("AND PRESS A BY A HOME.");
    } else {
        show_message("EVERY HOME STANDS. NOW...");
        queue_message("GROW ALL FOUR TO LEVEL 2");
        queue_message("AND SEE WHAT HAPPENS...");
    }
}

// The Great Tree's opening: solid until all four mini bosses are beaten, then the way into its hollow.
static void open_door(void) {
    use_area(WEST);
    for (s32 y = DOOR_Y; y <= DOOR_Y + 1; y++)
        for (s32 x = DOOR_X; x <= DOOR_X + 1; x++) set_tile(x, y, DRY_BRIDGE);
    use_area(cur_area);
}

// The healed tree's whole region turns green at once: dead ground grows grass and flowers, dead
// trees and stumps grow back, the brambles are gone, and the opening into the hollow closes.
static void revive_world(void) {
    u8 *m = maps[WEST];
    for (s32 i = 0; i < MAP_W * MAP_H; i++) {
        u8 t = m[i];
        u32 r = rng() % 100;
        if (t == DEAD || t == DEAD_CRACK || t == DIRT || t == THORN) m[i] = r < 60 ? GRASS : r < 85 ? GRASS_TUFT : FLOWERS;
        else if (t == DEAD_TREE || t == STUMP || t == STUMP_GRASS) m[i] = TREE;
        else if (t == CHEST) m[i] = CHEST_GRASS;
        else if (t == CHEST_OPEN) m[i] = CHEST_OPEN_GRASS;
        else if (t == WRECK) m[i] = WRECK_GRASS;
        else if (t == ROCK) m[i] = ROCK_GRASS;               // v0.17.3: grass round the rocks too
    }
    for (s32 y = DOOR_Y; y <= DOOR_Y + 1; y++)                // the opening grows shut (Caleb: no way back in)
        for (s32 x = DOOR_X; x <= DOOR_X + 1; x++) m[y * MAP_W + x] = TRUNK_BLOCK;
    for (s32 i = 0; i < MAP_W * MAP_H; i++)                   // the trunk stands on grass now
        if (m[i] == TRUNK_BLOCK) m[i] = TRUNK_GRASS;
    clear_west_gate();
}

// Gloomheart beaten (v0.16.2, Caleb's scene): inside the tree it shakes; Bark floats up among green
// sparks; the shaking stops and white light fills the screen; the room is clean again (the rot veins
// turn to green moss); Bark feels tingly, leaves swirl round him, and he shoots up. White again, and
// the team stands outside the healed tree, where Bark gives the wizard his power ("YOU" in R/L).
enum { RV_SAY, RV_SHAKE, RV_FLOAT, RV_WHITE, RV_CLEAN, RV_LEAFUP, RV_FADE, RV_OUT, RV_UNFADE, RV_STILL, RV_LEAVES, RV_POUCH };
// v0.16.3 (Caleb): the white stays while Bark turns back into his tree; it fades slowly into the
// healed tree shaking outside, the shaking stops as the light is gone, and then Bark speaks.
static const struct { u8 act; const char *msg; } revive_scene[] = {
    { RV_SAY, "THE GLOOMHEART CRUMBLES!" }, { RV_SHAKE, "WHAT'S HAPPENING?..." },
    { RV_FLOAT, "BARK: HMPH... SIGH..." }, { RV_WHITE, 0 }, { RV_CLEAN, 0 },
    { RV_SAY, "BARK: OH.. THIS LOOKS BETTER" }, { RV_SAY, "BARK: HMPH... WHAT'S THIS" },
    { RV_SAY, "TINGLY FEELING?..." }, { RV_SAY, "I DON'T LIKE IT..." }, { RV_LEAFUP, 0 }, { RV_FADE, 0 },
    { RV_OUT, 0 }, { RV_SAY, "BARK: WHOA. I FEEL... ROOTS." }, { RV_SAY, "MY BARK... MY BRANCHES..." },
    { RV_SAY, "I'M TURNING BACK INTO A TREE!" }, { RV_SAY, "HMPH. ABOUT TIME." },
    { RV_UNFADE, 0 }, { RV_STILL, "YOU BROUGHT BACK BARK!" },
    { RV_SAY, "BARK: OH. OH! MY LEAVES!" }, { RV_SAY, "I'M HOME. I'M ME AGAIN." },
    { RV_SAY, "HMPH. THANK YOU, WIZARD." }, { RV_SAY, "TAKE MY POWER. YOU EARNED IT." },
    { RV_LEAVES, "BARK'S POWER IS YOURS!" }, { RV_SAY, "R/L TO YOU: CUT, CLEAR, SLAM." },
    { RV_SAY, "NO CHARGES NEEDED." },
    { RV_SAY, "BARK: NOW. MY FRIENDS," }, { RV_SAY, "THE LEAFEN. TINY SPROUT FOLK." },
    { RV_SAY, "CHEERY. NOT VERY BRIGHT." }, { RV_SAY, "VAEL TOOK THEM AWAY." },
    { RV_SAY, "THEY'RE SCATTERED ALL OVER." }, { RV_SAY, "SOME NEARBY. SOME FAR OFF." },
    { RV_SAY, "SOME MIGHT NEED PERSUADING." },
    { RV_SAY, "AND MORE SPIRITS ARE OUT" }, { RV_SAY, "THERE. SOME I CAN CALL." },
    { RV_SAY, "SOME YOU'LL HAVE TO FIND." }, { RV_SAY, "SOME LEAFEN YOU CAN'T FREE" },
    { RV_SAY, "UNTIL YOU FIND THEIR SPIRIT." }, { RV_SAY, "SO GO FURTHER, WIZARD." },
    { RV_SAY, "FURTHER THAN YOU'VE BEEN." }, { RV_SAY, "BRING THEM HOME. PLEASE." },
    { RV_SAY, "HMPH. DO THAT, AND I'LL" }, { RV_SAY, "TEACH YOU A THING OR TWO." },
    { RV_SAY, "OH. AND TAKE THIS." }, { RV_POUCH, "YOU GOT THE SPIRIT POUCH!" },
    { RV_SAY, "SPIRITS YOU DON'T CARRY RIDE" }, { RV_SAY, "IN IT AS TINY ORBS." },
    { RV_SAY, "START, POUCH: SWAP THEM." },
};
#define REVIVE_LEN (sizeof(revive_scene) / sizeof(revive_scene[0]))
#define BLD_NORMAL ((1 << 2) | (1 << 6) | (1 << 9) | (1 << 11) | (1 << 12) | (1 << 13))   // BG2 see-through

// The white light, set once a frame in vblank. Brightening is the GBA's own fade to white.
static void apply_white(void) {
    if (!white_mode) return;
    white_t++;
    REG_BLDCNT = 0x3E | (2 << 6);                    // brighten every layer but the text (lines stay readable)
    REG_DISPCNT &= ~(1 << 13);                       // no clear box meanwhile (window 0)
    if (white_mode == 1) {                           // a line grows across, then opens to the full screen
        s32 w = white_t < 16 ? white_t * 8 : SCREEN_W / 2, h = white_t < 16 ? 1 : (white_t - 15) * 4;
        if (h > SCREEN_H / 2) h = SCREEN_H / 2;
        REG_DISPCNT |= 1 << 14;
        REG_WIN1H = ((SCREEN_W / 2 - w) << 8) | (SCREEN_W / 2 + w);
        REG_WIN1V = ((SCREEN_H / 2 - h) << 8) | (SCREEN_H / 2 + h);
        REG_WININ = (REG_WININ & 0xFF) | (0x3F << 8);
        REG_WINOUT = 0x1F;                           // outside the light: no effect
        REG_BLDY = 16;
        if (h >= SCREEN_H / 2) { white_mode = 2; white_t = 0; }
        return;
    }
    REG_DISPCNT &= ~(1 << 14);
    REG_WINOUT = 0x3F;
    s32 sh = white_slow ? 3 : 1, len = 16 << sh;     // a slow fade takes 128 frames
    s32 y = white_mode == 2 ? 16 : white_mode == 3 ? 16 - (white_t >> sh) : white_t >> 1;
    REG_BLDY = y < 0 ? 0 : y > 16 ? 16 : y;
    if (white_mode == 4 && white_t >= 32) { white_mode = 2; white_t = 0; }
    if (white_mode == 3 && white_t >= len) {         // back to normal
        white_slow = 0;
        white_mode = 0;
        REG_BLDY = 0;
        REG_BLDCNT = BLD_NORMAL;
        if (cur_area == WEST) REG_DISPCNT |= 1 << 13;
    }
}

static void green_motes(s32 x, s32 y, s32 n, s32 up) {   // sparks of life round (x, y)
    for (s32 k = 0; k < n; k++)
        add_mote_c(x - 8 + (s32)(rng() % 24) - 12, y - 8 + (s32)(rng() % 16) - 8,
                   up ? 0 : dir16[(k * 2) & 15] >> 1, up ? -10 : DIR16_Y((k * 2) & 15) >> 1, up ? 40 : 30, C_GLINT);
}

static void update_revive(void) {
    if (shake_timer) shake_timer--;
    if (leaf_timer) leaf_timer--;
    if (!revive_step || revive_step > REVIVE_LEN) return;
    scene_on = 1;
    if (cur_area == HOLLOW_IN) update_gloom_dots();
    if (float_on == 1) {                             // Bark floats, sparks rise; at the end he shoots up
        if (leaf_timer && leaf_timer < 40) float_up = 1;
        float_x += sgn(float_tx - float_x); float_y += sgn(float_ty - float_y);   // drifts to its spot
        if (float_up) float_h += 5;
        else if (float_h < 20 && (frame & 1)) float_h++;
        if (!float_up && frame % 8 == 0) green_motes(float_x + 8, float_y + 8 - float_h, 1, 1);
        if (float_h > 220) { float_on = 2; leaf_timer = 0; }
    }
    if (white_mode == 1 || white_mode >= 3) return;  // wait for the light
    if (revive_wait) { revive_wait--; return; }
    if ((msg_timer && !msg_is_prompt) || msgq_len || music_once_playing()) return;   // a jingle plays out
    u32 act = revive_scene[revive_step - 1].act;
    const char *m = revive_scene[revive_step - 1].msg;
    revive_step++;
    revive_wait = 20;
    if (act == RV_SHAKE) {                           // until the light comes
        shake_timer = 2000; sfx_play(SFX_ROCK);
        revive_slow = 1; scene_song = SONG_WHOLETONE; music_play(SONG_WHOLETONE);
    }
    if (act == RV_FLOAT) {
        Spirit *b = &spirits[KIND_BARK];
        float_on = 1; float_x = b->x; float_y = b->y; float_h = 0; float_up = 0;
        float_tx = px; float_ty = py - 20;            // in front of the wizard, above him
        sfx_play(SFX_WAKE);
    }
    if (act == RV_WHITE) { shake_timer = 0; white_mode = 1; white_t = 0; revive_wait = 30; chime(1568, 40); }
    if (act == RV_CLEAN) {                           // the rot veins become moss; light fades back
        PAL_BG[TREE_BANK * 16 + 11] = RGB5(40, 76, 36);
        PAL_BG[TREE_BANK * 16 + 12] = RGB5(92, 150, 66);
        for (s32 i = 0; i < MAX_MOTES; i++) motes[i].life = 0;
        green_motes(float_x + 8, float_y + 8 - float_h, 8, 0);
        white_mode = 3; white_t = 0;
        sfx_play(SFX_QUEST);
        revive_wait = 40;
    }
    if (act == RV_LEAFUP) { leaf_timer = 150; sfx_play(SFX_BUILD); revive_wait = 170; }
    if (act == RV_FADE) {                            // white again, and silence (Caleb)
        white_mode = 4; white_t = 0; revive_wait = 30;
        scene_song = -1; music_play(-1);
    }
    if (act == RV_OUT) {                             // while white: outside, the tree healed, the region green
        float_on = 0;
        tree_alive = 1;
        revive_world();
        enter_area(WEST, DOOR_X * 16 + 8, (DOOR_Y + 3) * 16 - 6);
        draw_life();
        draw_active();
        draw_hearts_hud();
        clear_row(1);
        music_play(area_song(WEST));
        msg_timer = 0; msgq_len = 0; clear_row(19);   // no area name over the scene
        white_mode = 2;                              // still white while Bark turns back into his tree
        revive_wait = 60;
    }
    if (act == RV_UNFADE) {                          // the reveal: the tree shakes until the jingle ends
        shake_timer = 2000; white_slow = 1; white_mode = 3; white_t = 0; revive_wait = 0;
        scene_song = SONG_REVEAL_GRAND; music_play(SONG_REVEAL_GRAND);
    }
    if (act == RV_STILL) {
        shake_timer = 0; sfx_play(SFX_WAKE); revive_wait = 30;
        revive_slow = 0; scene_song = NO_SCENE_SONG; music_play(area_song(WEST));   // the healed tree's own song
    }
    if (act == RV_LEAVES) { leaf_timer = 150; sfx_play(SFX_QUEST); }
    if (act == RV_POUCH) { pouch_on = 1; sfx_play(SFX_CHEST); }   // v0.18.4
    if (m) { show_message(m); if (revive_slow) msg_timer = 240; }   // Caleb: these went by too fast
}

// The last mini boss falls: omen (Bright, quiet and open) plays once, then the tree shakes and the
// music turns uneasy. Otherwise the Great Tree's music follows area_song as the story moves on.
static void update_music(void) {
    if (omen_step == 1 && !music_once_playing()) {
        omen_step = 0;
        shake_timer = 100;
        sfx_play(SFX_GLOOM_ROAR);                              // something inside the tree stirs
    }
    if (omen_step || music_once_playing()) return;
    if (revive_step && revive_step <= REVIVE_LEN) return;     // the revival sets its own music
    if (cur_area == WEST || cur_area == HOLLOW_IN) music_play(area_song(cur_area));
}

// Milestones, checked twice a second: an area reaching 100% LIFE, and harmony.
static const char *area_full[NUM_AREAS] = { "THE VALLEY IS FULLY ALIVE!", "THE STEPPE IS FULLY ALIVE!",
                                            "STONE HOLLOW IS FULLY ALIVE!", "THE GREAT WOOD IS ALIVE!", "",
                                            "THE MARSH IS FULLY ALIVE!", "THE ORCHARD IS FULLY ALIVE!" };
static void check_milestones(void) {
    if (frame % 30 || scene_on) return;                     // not in the middle of a scene
    for (s32 a = 0; a < NUM_AREAS; a++)
        if (!told_full[a] && restorable[a] && life_percent_of(a) >= 100) {
            told_full[a] = 1;
            sfx_play(SFX_QUEST);
            queue_message(area_full[a]);
        }
    if (harmony) return;
    for (s32 k = 0; k < NUM_HOMES; k++)
        if (home_level[k] < 2 || spirits[k].state != SP_HOME) return;
    harmony = 1;
    sfx_play(SFX_QUEST);
    queue_message("ALL FOUR SPIRITS ARE HOME.");
    queue_message("THE LAND SINGS WITH LIFE!");
    queue_message("BLOOMS NOW GROW BIGGER!");
    queue_message("HOMES NOW PULSE NONSTOP!");
}

// The first step into the hidden spring gets a message.
static void check_spring(void) {
    if (told_spring || cur_area != 0) return;
    s32 mx = (px + 8) >> 4, my = (py + 12) >> 4;
    if (mx >= POCKET_X0 && mx <= POCKET_X1 && my >= POCKET_Y0 && my <= POCKET_Y1) {
        show_message("A HIDDEN SPRING!");
        sfx_play(SFX_WAKE);
        told_spring = 1;
    }
}

// v0.16.1 (Caleb: always looking up shoved the wizard to the bottom of the screen): the camera looks
// up only while you stand in front of the Great Tree's trunk, more the closer you get, and eases there.
// In the hollow the camera frames both you and the Gloomheart (halfway between, you kept off the edges).
static s32 cam_ox, cam_oy;
static s32 ease_to(s32 v, s32 want) { return v < want - 1 ? v + 2 : v > want + 1 ? v - 2 : want; }
static s32 clamp_abs(s32 v, s32 m) { return v > m ? m : v < -m ? -m : v; }
static void update_camera(void) {
    s32 wx = 0, wy = 0;
    if (cur_area == WEST && px + 8 >= 120 && px + 8 <= 392 && py < 430) wy = 430 - py > 56 ? -56 : py - 430;
    if (cur_area == HOLLOW_IN && boss.alive && boss.kind == B_GLOOM) {
        wx = clamp_abs((boss.x + 16 - (px + 8)) >> 1, SCREEN_W / 2 - 28);
        wy = clamp_abs((boss.y + 16 - (py + 8)) >> 1, gloom_intro ? SCREEN_H / 2 - 20 : SCREEN_H / 2 - 32);
    }
    if (cur_area != WEST && cur_area != HOLLOW_IN) cam_ox = cam_oy = 0;
    cam_ox = ease_to(cam_ox, wx);
    cam_oy = ease_to(cam_oy, wy);
    cam_x = px + 8 - SCREEN_W / 2 + cam_ox;
    cam_y = py + 8 - SCREEN_H / 2 + cam_oy;
    if (cam_x < 0) cam_x = 0;
    if (cam_y < 0) cam_y = 0;
    if (cam_x > WORLD_PX - SCREEN_W) cam_x = WORLD_PX - SCREEN_W;
    if (cam_y > WORLD_PX - SCREEN_H) cam_y = WORLD_PX - SCREEN_H;
}

static void animate_water(void) {
    if (cur_area == MARSH)                        // v0.18.1: the bog water ripples too
        copy32(CHARBLOCK(0) + meta_vram(BOG_WATER) * 32, terrain_tiles + (((frame >> 5) & 1) ? BOG_WATER_B : BOG_WATER) * 32, 32);
    // Swap the water tile graphics between two frames.
    u32 src = ((frame >> 5) & 1) ? WATER_B : WATER;
    copy32(CHARBLOCK(0) + WATER * 32, terrain_tiles + src * 32, 32);
    // Vael pumps work in the same rhythm: their light blinks and the piston moves.
    u32 pump = ((frame >> 5) & 1) ? VAEL_PUMP_B : VAEL_PUMP;
    copy32(CHARBLOCK(0) + VAEL_PUMP * 32, terrain_tiles + pump * 32, 32);
}

// The clear box: window 0 around the wizard, where the Great Tree's branches are not drawn.
static void clear_box(void) {
    s32 x = px - cam_x, y = py - cam_y - 16;               // the wizard's 16x32 sprite
    s32 x1 = x - 14, x2 = x + 30, y1 = y - 8, y2 = y + 40;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > SCREEN_W) x2 = SCREEN_W;
    if (y2 > SCREEN_H) y2 = SCREEN_H;
    REG_WIN0H = (x1 << 8) | x2;
    REG_WIN0V = (y1 << 8) | y2;
}

static void draw_sprites(void) {
    // Player sheet: per direction (down, up, side) 4 frames: stand, walk1, walk2, walk3.
    // Walking plays 1, 2, 3, 2. Left is the side frames mirrored. The sprite is 16x32 with the
    // feet in the bottom 16 pixels, so it is drawn 16 pixels above the feet cell.
    static const u8 dir_block[4] = { 0, 1, 2, 2 };   // down, up, left, right
    static const u8 walk_seq[4] = { 1, 2, 3, 2 };
    u32 f = dir_block[pdir] * 4 + (pstep ? walk_seq[pstep & 3] : 0);
    if (swimming()) {                              // only his head and shoulders above the water, ripples round him
        // the ring takes the front slot (0) and he the ring's, so the ripples show in front of him
        s32 bob = (frame >> 4) & 1;
        sprite_set(OAM_PLAYER, px - cam_x, py - cam_y + 1 + bob, SPR_LEAFEN + C_SWIMRING + bob, 0, PAL_LEAFEN);
        obj_set(OAM_SWIMRING, px - cam_x, py - cam_y - 6 + bob, TILE_PLAYER + f * 8 + 2, pdir == DIR_LEFT, PAL_MAIN, 0);
        if (hurt_timer & 4) sprite_hide(OAM_SWIMRING);
        u32 tread = (frame >> 3) & 3;              // treading water: hands out, in, out... (faster while moving)
        if (pstep) tread = (frame >> 2) & 3;
        obj_set(OAM_SWIMHANDS, px - cam_x, py - cam_y - 2 + bob, TILE_SWIM + (tread == 1 || tread == 2 ? 4 : 0), 0, PAL_MAIN, 0);
        if (pstep) {                               // swimming along: a wake opens behind him
            s32 wx = px - cam_x, wy = py - cam_y + 1;
            u32 cell = pdir == DIR_DOWN ? 1 : pdir == DIR_UP ? 2 : 0;
            if (pdir == DIR_LEFT) wx += 12; else if (pdir == DIR_RIGHT) wx -= 12;
            else wy += pdir == DIR_DOWN ? -10 : 10;
            sprite_set(OAM_WAKE, wx, wy, SPR_LEAFEN + C_WAKE + cell, pdir == DIR_LEFT, PAL_LEAFEN);
        } else sprite_hide(OAM_WAKE);
        if (pstep && (frame & 15) == 0) {          // tiny splashes at his sides as he swims (Caleb: not bubbles)
            s32 side = (frame >> 4) & 1 ? 7 : -7;
            add_mote_c(px + side + (pdir == DIR_LEFT ? 3 : pdir == DIR_RIGHT ? -3 : 0), py + 2, 0, -2, 10, C_SPLASH);
        }
    } else {
        obj_set(OAM_PLAYER, px - cam_x, py - cam_y - 16, TILE_PLAYER + f * 8, pdir == DIR_LEFT, PAL_MAIN, 1);
        sprite_hide(OAM_SWIMRING);
        sprite_hide(OAM_WAKE);
        sprite_hide(OAM_SWIMHANDS);
    }
    if (hurt_timer & 4) sprite_hide(OAM_PLAYER);   // blinking just after a hit

    // v0.18.1 (Caleb): the spirits change into orbs as you go into the water and back as you come out:
    // a puff of sparkles at each, then a quick flicker between the two forms
    static int was_swimming;
    if (swimming() != was_swimming) {
        was_swimming = swimming();
        orb_morph = 16;
        for (s32 p = 0; p < party_count; p++) {
            Spirit *s = &spirits[party[p]];
            if (is_you(s->kind)) continue;
            for (s32 k = 0; k < 3; k++) add_mote_c(s->x + 4, s->y, dir16[(k * 5 + p) & 15] >> 2, -6 - k * 2, 14, C_SPLASH);
        }
    }
    if (orb_morph) orb_morph--;
    s32 n_pouch = 0, j_pouch = 0;
    for (s32 i = 0; i < NUM_SPIRITS; i++) n_pouch += spirits[i].state == SP_POUCH;
    for (s32 i = 0; i < NUM_SPIRITS; i++) {
        Spirit *s = &spirits[i];
        if (s->area != cur_area) { sprite_hide(SPIRIT_OAM(i)); sprite_hide(ZZZ_OAM(i)); continue; }
        if (s->state == SP_POUCH) {                   // v0.18.4: tiny orbs circling his head, spread evenly
            u32 a = ((frame >> 1) + (u32)(j_pouch++ * 48 / n_pouch)) % 48;
            sprite_set(SPIRIT_OAM(i), px - cam_x + loop_x48[a] * 10 / 24, py - cam_y - 22 + loop_y48[a] * 4 / 16,
                       SPR_LEAFEN + (i < 6 ? C_MINI + i : C_MINI + 7), 0, PAL_LEAFEN);   // Flick's after the rest
            sprite_hide(ZZZ_OAM(i));
            continue;
        }
        s32 anim = (frame >> 4) & 1;
        s32 b = bob[((frame >> 2) + i * 5) & 15];
        s32 sx = s->x - cam_x, sy = s->y - cam_y - 4;
        int orb = swimming();
        if (orb_morph) orb = ((orb_morph >> 1) & 1) ? !orb : orb;   // changing: flicker between the two forms
        if (s->state == SP_JOINED && in_party(i) && orb && tree_alive && !is_you(i)) {   // v0.18.1: orbs while you swim
            sprite_set(SPIRIT_OAM(i), sx, sy - 2 + bob[((frame >> 2) + i * 5) & 15], SPR_LEAFEN + (i < 6 ? C_ORB + i : C_MINI + 6), 0, PAL_LEAFEN);
            sprite_hide(ZZZ_OAM(i));
        } else if (i == KIND_BARK && float_on) {         // the revival: Bark floats, then shoots up
            if (float_on == 2) sprite_hide(SPIRIT_OAM(i));
            else sprite_set(SPIRIT_OAM(i), float_x - cam_x, float_y - cam_y - 4 - float_h + (float_up ? 0 : b),
                            spirit_tile[s->kind] + anim, 0, spirit_pal[s->kind]);
            sprite_hide(ZZZ_OAM(i));
        } else if (s->state == SP_HIDDEN || is_you(i)) {  // Bark lives in his tree once it is healed
            sprite_hide(SPIRIT_OAM(i));
            sprite_hide(ZZZ_OAM(i));
        } else if (s->state == SP_HOME) {
            draw_home_spirit(i, sx, s->y - cam_y, b, anim);
            sprite_hide(ZZZ_OAM(i));
        } else if (s->state == SP_ASLEEP && i == KIND_FLICK) {   // v0.19: cold in its nest, shivering now and then,
            u32 t = frame % 90;                                    // and a little spark that shows through the dark
            sprite_set(SPIRIT_OAM(i), sx, sy + 2, SPR_FLICK + (t < 16 && (frame & 2) ? 4 : 2), 0, PAL_FLICK);
            if (t >= 30 && t < 60) {
                sprite_set(ZZZ_OAM(i), sx + 4, sy - 4 - (s32)(t - 30) / 3, SPR_SPARKLE, 0, PAL_FX);
                oam_shadow[ZZZ_OAM(i) * 4 + 2] &= ~(3 << 10);   // above the dark
            } else sprite_hide(ZZZ_OAM(i));
        } else if (s->state == SP_ASLEEP) {
            // gray, still, with a sleep bubble that drifts up
            sprite_set(SPIRIT_OAM(i), sx, sy + 2, spirit_tile[s->kind], 0, spirit_sleep_pal[s->kind]);
            sprite_set(ZZZ_OAM(i), sx + 8, sy - 10 - ((frame >> 3) & 3), SPR_ZZZ, 0, PAL_RIPPLE);
        } else {
            if (i == KIND_BARK && bark_fall) b = -(s32)(bark_fall * bark_fall) / 6;   // dropping in from above
            sprite_set(SPIRIT_OAM(i), sx, sy + b, spirit_tile[s->kind] + anim, 0, spirit_pal[s->kind]);
            sprite_hide(ZZZ_OAM(i));
        }
    }

    // Vael clamps on the Great Tree's roots: shown while their cell is still a CLAMP.
    for (s32 i = 0; i < NUM_CLAMPS && cur_area == WEST && !tree_alive; i++) {   // shared slots (OAM_POOL)
        u8 t = maps[WEST][(clamp_px_y[i] >> 4) * MAP_W + (clamp_px_x[i] >> 4)];
        s32 x = clamp_px_x[i] - 16 - cam_x, y = clamp_px_y[i] - 16 - cam_y;
        if (cur_area == WEST && t == CLAMP && x > -32 && x < SCREEN_W && y > -32 && y < SCREEN_H) {
            u16 *o = &oam_shadow[(OAM_CLAMP + i) * 4];                // a 32x32 sprite (square, size 2)
            o[0] = (y & 0xFF);
            o[1] = (x & 0x1FF) | (2 << 14);
            o[2] = (TILE_CLAMP + i * 16) | (1 << 10) | (PAL_CLAMP << 12);
        } else {
            sprite_hide(OAM_CLAMP + i);
        }
    }

    // Build preview frame on the faced tile.
    if (cursor_mx >= 0)
        sprite_set(OAM_CURSOR, cursor_mx * 16 - cam_x, cursor_my * 16 - cam_y, SPR_SPARKLE + (cursor_ok ? 1 : 2), 0, PAL_FX);
    else
        sprite_hide(OAM_CURSOR);
    // The bonded companion glows: a soft sparkle that blinks above it.
    if (bonded >= 0 && spirits[bonded].area == cur_area && ((frame >> 4) & 1))
        sprite_set(OAM_GLOW, spirits[bonded].x - cam_x + 4, spirits[bonded].y - cam_y - 14, SPR_SPARKLE, 0, PAL_FX);
    else
        sprite_hide(OAM_GLOW);
    if (sparkle_life > 0) {
        sparkle_life--;
        sprite_set(OAM_SPARKLE, sparkle_x - cam_x, sparkle_y - cam_y - (40 - sparkle_life) / 4,
                   SPR_SPARKLE, (frame >> 3) & 1, PAL_FX);
    } else {
        sprite_hide(OAM_SPARKLE);
    }
}

static int paused;   // the START menu is open

// ---------- saving (manual: SAVE in the START menu) ----------
// Battery-backed SRAM at 0x0E000000, read and written one byte at a time. The "SRAM_V113" tag
// tells emulators (and the R36) which save type this ROM uses; they store it as a .srm file.
#define SRAM ((volatile u8*)0x0E000000)
#define REG_WAITCNT REG16(0x04000204)
__attribute__((used, aligned(4))) static const char save_tag[] = "SRAM_V113";
#define SAVE_MAGIC 0x46534652u   // "RFSF" (v0.19.4: the orchard's Leafen)
#define SAVE_MAGIC_V193 0x45534652u   // "RFSE": v0.19.3 saves still load
#define SAVE_MAGIC_V191 0x44534652u   // "RFSD": v0.19.1-2 saves still load
#define SAVE_MAGIC_V190 0x43534652u   // "RFSC": v0.19.0 saves still load
#define SAVE_MAGIC_V184 0x42534652u   // "RFSB": v0.18.4 saves still load
#define SAVE_MAGIC_V18 0x41534652u   // "RFSA": v0.18 saves still load
#define SAVE_MAGIC_V17 0x39534652u   // "RFS9": v0.17 saves still load
#define SAVE_MAGIC_V16 0x38534652u   // "RFS8": v0.16 saves still load (the Leafen set up on load)
#define SAVE_MAGIC_V15 0x37534652u   // "RFS7": v0.15 saves still load
#define SAVE_MAGIC_V14 0x36534652u   // "RFS6": v0.14 saves still load
#define SAVE_MAGIC_V13 0x35534652u   // "RFS5": v0.13 saves (three areas) still load
#define OLD_AREAS 3

// Everything that makes up a game in progress, in save order.
static const struct { void *p; u32 n; } save_fields[] = {
    { &px, 4 }, { &py, 4 }, { &pdir, 4 }, { &cur_area, 4 },
    { maps, sizeof(maps[0]) * OLD_AREAS }, { restorable, 4 * OLD_AREAS },
    { spirits, sizeof(Spirit) * OLD_SPIRITS }, { party, 4 * OLD_SPIRITS }, { &party_count, 4 }, { &active, 4 },
    { home_done, OLD_SPIRITS }, { pulse_r, 4 * OLD_SPIRITS }, { pulse_timer, 4 * OLD_SPIRITS },
    { &stone, 4 }, { &fruit, 4 }, { &seeds, 4 },
    { chest_open, sizeof(chest_open) }, { wreck_read, sizeof(wreck_read) }, { picked, sizeof(picked[0]) * OLD_AREAS },
    { &bonus_charge, 4 }, { &pulse_div, 4 }, { home_level, OLD_SPIRITS }, { &build_item, 4 },
    { fp, OLD_SPIRITS }, { quest, OLD_SPIRITS }, { unlocked, OLD_SPIRITS }, { &bonded, 4 }, { &thorn_quest, 4 },
    { pump_on, OLD_AREAS }, { pump_x, OLD_AREAS }, { pump_y, OLD_AREAS },
    { pump_drain, 4 * OLD_AREAS }, { &pump_timer, 4 }, { &pumps_stopped, 4 },
    { &told_river, 4 }, { &told_spring, 4 }, { &told_tips, 4 }, { &told_ending, 4 }, { &rng_state, 4 },
    { &wood, 4 }, { &harmony, 4 }, { told_full, OLD_AREAS }, { &told_grown, 4 },
    { &bark_step, 4 }, { &bark_wait, 4 }, { &bark_line, 4 },
    // v0.14 (after everything a v0.13 save holds)
    { maps[WEST], sizeof(maps[0]) }, { &restorable[WEST], 4 }, { picked[WEST], sizeof(picked[0]) },
    { &pump_on[WEST], 1 }, { &pump_x[WEST], 1 }, { &pump_y[WEST], 1 }, { &pump_drain[WEST], 4 },
    { &told_full[WEST], 1 }, { &tree_step, 4 }, { &tree_wait, 4 }, { &tree_done, 4 },
    // v0.15
    { &boss_beaten, 4 },
    // v0.16
    { &tree_alive, 4 }, { &revive_step, 4 }, { &revive_wait, 4 },
    // v0.17: the Leafen
    { leafen_state, FIRST_LEAFEN }, { &leafen_init, 4 }, { &has_shiny, 4 }, { &pip_cell, 4 },
    // v0.18: Lily (the sixth spirit), the marsh and its three Leafen
    { &spirits[KIND_LILY], sizeof(Spirit) }, { &party[OLD_SPIRITS], 4 }, { &fp[KIND_LILY], 1 }, { &quest[KIND_LILY], 1 },
    { &unlocked[KIND_LILY], 1 }, { &leafen_state[FIRST_LEAFEN], FIRST_ORCH_LEAFEN - FIRST_LEAFEN }, { &lily_step, 4 },
    { maps[MARSH], sizeof(maps[0]) }, { &restorable[MARSH], 4 }, { picked[MARSH], sizeof(picked[0]) },
    { &pump_on[MARSH], 1 }, { &pump_x[MARSH], 1 }, { &pump_y[MARSH], 1 }, { &pump_drain[MARSH], 4 },
    { &told_full[MARSH], 1 }, { &swim_skill, 4 }, { &swim_step, 4 },
    // v0.18.4: the spirit pouch
    { &pouch_on, 4 },
    // v0.19: the burnt orchard
    { maps[ORCHARD], sizeof(maps[0]) }, { &restorable[ORCHARD], 4 }, { picked[ORCHARD], sizeof(picked[0]) },
    { &pump_on[ORCHARD], 1 }, { &pump_x[ORCHARD], 1 }, { &pump_y[ORCHARD], 1 }, { &pump_drain[ORCHARD], 4 },
    { &told_full[ORCHARD], 1 },
    // v0.19 (step 2): Flick
    { &spirits[KIND_FLICK], sizeof(Spirit) }, { &party[6], 4 }, { &fp[KIND_FLICK], 1 }, { &quest[KIND_FLICK], 1 },
    { &unlocked[KIND_FLICK], 1 }, { &flick_step, 4 }, { &flick_flags, 4 },
    // v0.19 (step 4): marsh mushrooms (and, on load from older saves, the old regions' campfires)
    { &mushrooms, 4 },
    // v0.19.4: the orchard's two Leafen
    { &leafen_state[FIRST_ORCH_LEAFEN], NUM_LEAFEN - FIRST_ORCH_LEAFEN },
};
#define NUM_SAVE_FIELDS (sizeof(save_fields) / sizeof(save_fields[0]))
#define NUM_V193_FIELDS (NUM_SAVE_FIELDS - 1)
#define NUM_V191_FIELDS (NUM_V193_FIELDS - 1)
#define NUM_V190_FIELDS (NUM_V191_FIELDS - 7)
#define NUM_V184_FIELDS (NUM_V190_FIELDS - 8)
#define NUM_V18_FIELDS (NUM_V184_FIELDS - 1)
#define NUM_V17_FIELDS (NUM_V18_FIELDS - 17)
#define NUM_V16_FIELDS (NUM_V17_FIELDS - 4)
#define NUM_V15_FIELDS (NUM_V17_FIELDS - 7)
#define NUM_V14_FIELDS (NUM_V17_FIELDS - 8)
#define NUM_V13_FIELDS (NUM_V17_FIELDS - 19)

// v0.17.1: the root pool grew into an irregular oval. Older saves get the new cells: water if the pool
// is already filled, dry if not; only over open ground, keeping LIFE % right.
static const u8 root_pool[][2] = { { 15, 25 }, { 16, 25 }, { 14, 26 }, { 15, 26 }, { 16, 26 }, { 17, 26 }, { 14, 27 },
                                   { 15, 27 }, { 16, 27 }, { 17, 27 }, { 18, 27 }, { 15, 28 }, { 16, 28 }, { 17, 28 } };
static void widen_root_pool(void) {
    u8 *m = maps[WEST];
    u8 old = m[26 * MAP_W + 15];
    if (old != WATER && old != DRY_POOL) return;          // the pool is not there (should not happen)
    for (u32 i = 0; i < sizeof(root_pool) / sizeof(root_pool[0]); i++) {
        u8 *c = &m[root_pool[i][1] * MAP_W + root_pool[i][0]];
        if (*c == old || !(*c == DEAD || *c == DEAD_CRACK || *c == DIRT || *c == GRASS || *c == GRASS_TUFT ||
                           *c == FLOWERS)) continue;
        if (is_restorable(*c)) restorable[WEST]--;
        *c = old;
    }
}

static void sram_put32(u32 at, u32 v) { for (u32 k = 0; k < 4; k++) SRAM[at + k] = (u8)(v >> (8 * k)); }
static u32 sram_get32(u32 at) {
    u32 v = 0;
    for (u32 k = 0; k < 4; k++) v |= (u32)SRAM[at + k] << (8 * k);
    return v;
}

static void save_game(void) {
    u32 at = 4, sum = 0x5A5A;
    for (u32 f = 0; f < NUM_SAVE_FIELDS; f++) {
        const u8 *b = save_fields[f].p;
        for (u32 i = 0; i < save_fields[f].n; i++, at++) { SRAM[at] = b[i]; sum += b[i]; }
    }
    sram_put32(at, sum);
    sram_put32(0, SAVE_MAGIC);       // written last: a cut-off save never looks valid
}

// Returns 1 and restores the game if SRAM holds a valid save.
static int load_game(void) {
    u32 magic = sram_get32(0), nf = NUM_SAVE_FIELDS;
    if (magic == SAVE_MAGIC_V13) nf = NUM_V13_FIELDS;    // a v0.13 save: everything but the west
    else if (magic == SAVE_MAGIC_V14) { nf = NUM_V14_FIELDS; boss_beaten = 0; }   // no bosses yet
    else if (magic == SAVE_MAGIC_V15) nf = NUM_V15_FIELDS;
    else if (magic == SAVE_MAGIC_V16) nf = NUM_V16_FIELDS;
    else if (magic == SAVE_MAGIC_V17) nf = NUM_V17_FIELDS;
    else if (magic == SAVE_MAGIC_V18) nf = NUM_V18_FIELDS;
    else if (magic == SAVE_MAGIC_V184) nf = NUM_V184_FIELDS;
    else if (magic == SAVE_MAGIC_V190) nf = NUM_V190_FIELDS;
    else if (magic == SAVE_MAGIC_V191) nf = NUM_V191_FIELDS;
    else if (magic == SAVE_MAGIC_V193) nf = NUM_V193_FIELDS;
    else if (magic != SAVE_MAGIC) return 0;
    u32 at = 4, sum = 0x5A5A;
    for (u32 f = 0; f < nf; f++)
        for (u32 i = 0; i < save_fields[f].n; i++) sum += SRAM[at++];
    if (sram_get32(at) != sum) return 0;
    at = 4;
    for (u32 f = 0; f < nf; f++) {
        u8 *b = save_fields[f].p;
        for (u32 i = 0; i < save_fields[f].n; i++) b[i] = SRAM[at++];
    }
    if (nf < NUM_V16_FIELDS) { tree_alive = revive_step = revive_wait = 0; }
    if (nf < NUM_V17_FIELDS) {                            // no Leafen yet: update_leafen sets them up
        for (s32 k = 0; k < NUM_LEAFEN; k++) leafen_state[k] = LS_NONE;
        leafen_init = has_shiny = pip_cell = 0;
    }
    if (nf < NUM_V184_FIELDS) pouch_on = tree_alive && revive_step > REVIVE_LEN;   // the talk is behind you
    if (nf < NUM_SAVE_FIELDS) {                          // before v0.19.4: the orchard's Leafen wait; burnt trees count
        for (s32 k = FIRST_ORCH_LEAFEN; k < NUM_LEAFEN; k++) leafen_state[k] = leafen_init ? LS_WAIT : LS_NONE;
        restorable[ORCHARD] = 0;                         // as ground that can live again now
        for (s32 i = 0; i < MAP_W * MAP_H; i++)
            restorable[ORCHARD] += is_restorable(maps[ORCHARD][i]) || is_alive(maps[ORCHARD][i]);
    }
    if (nf < NUM_V193_FIELDS) {                          // before v0.19.3: no mushrooms; set the campfires down
        mushrooms = 0;
        for (s32 k = 0; k < NUM_CAMPS; k++) {
            u8 *m = maps[camp_area[k]];
            s32 x = camp_x[k], y = camp_y[k];
            int ok = 1;                                  // only on open ground (never over something built)
            for (s32 dx = -1; dx <= 1; dx++) {
                u8 t = m[y * MAP_W + x + dx];
                if (is_solid(t) || is_built(t) || t == HOMESITE) ok = 0;
            }
            if (!ok) continue;
            for (s32 dx = -1; dx <= 1; dx++)
                if (is_restorable(m[y * MAP_W + x + dx])) restorable[camp_area[k]]--;
            m[y * MAP_W + x] = CAMPFIRE_COLD;
            m[y * MAP_W + x - 1] = m[y * MAP_W + x + 1] = LOG_SEAT;
        }
    }
    if (nf < NUM_V191_FIELDS) {                          // before v0.19 step 2: Flick cold in its nest
        spirit_init(KIND_FLICK, ORCHARD_NEST_X, ORCHARD_NEST_Y, SP_ASLEEP);
        spirits[KIND_FLICK].area = ORCHARD;
        party[6] = 0; fp[KIND_FLICK] = quest[KIND_FLICK] = unlocked[KIND_FLICK] = 0;
        flick_step = flick_flags = 0;
    }
    if (nf < NUM_V190_FIELDS) {                          // before v0.19: a fresh orchard
        restorable[ORCHARD] = 0;
        for (s32 i = 0; i < MAP_W * MAP_H; i++) {
            u8 t = start_maps[ORCHARD * MAP_W * MAP_H + i];
            maps[ORCHARD][i] = t;
            if (is_restorable(t)) restorable[ORCHARD]++;
            if (i < MAP_W * MAP_H / 8) picked[ORCHARD][i] = 0;
        }
        pump_on[ORCHARD] = 0; pump_drain[ORCHARD] = 0; told_full[ORCHARD] = 0;
    }
    if (pouch_on)                                        // carrying more than you can: the last ones ride in the pouch
        for (s32 p = party_count - 1; p >= 0 && carried() > party_cap(); p--)
            if (!is_you(party[p])) { spirits[party[p]].state = SP_POUCH; party_remove(party[p]); }
    if (nf < NUM_V18_FIELDS) {                            // before v0.18: no Lily, a fresh marsh
        spirit_init(KIND_LILY, 0, 0, SP_HIDDEN);
        party[OLD_SPIRITS] = 0; fp[KIND_LILY] = quest[KIND_LILY] = unlocked[KIND_LILY] = 0;
        for (s32 k = FIRST_LEAFEN; k < FIRST_ORCH_LEAFEN; k++) leafen_state[k] = leafen_init ? LS_WAIT : LS_NONE;
        lily_step = 0; swim_skill = swim_step = 0;
        restorable[MARSH] = 0;
        for (s32 i = 0; i < MAP_W * MAP_H; i++) {
            u8 t = start_maps[MARSH * MAP_W * MAP_H + i];
            maps[MARSH][i] = t;
            if (is_restorable(t)) restorable[MARSH]++;
            if (i < MAP_W * MAP_H / 8) picked[MARSH][i] = 0;
        }
        pump_on[MARSH] = 0; pump_drain[MARSH] = 0; told_full[MARSH] = 0;
    }
    for (s32 i = 0; i < MAP_W * MAP_H; i++) maps[HOLLOW_IN][i] = start_maps[HOLLOW_IN * MAP_W * MAP_H + i];
    restorable[HOLLOW_IN] = 0;
    tileset_now = TS_NONE;
    if (nf == NUM_V13_FIELDS) {                           // the west starts fresh
        boss_beaten = 0;
        restorable[WEST] = 0;
        for (s32 i = 0; i < MAP_W * MAP_H; i++) {
            u8 t = start_maps[WEST * MAP_W * MAP_H + i];
            maps[WEST][i] = t;
            if (is_restorable(t)) restorable[WEST]++;
            if (i < MAP_W * MAP_H / 8) picked[WEST][i] = 0;
        }
        pump_on[WEST] = 0; pump_drain[WEST] = 0; told_full[WEST] = 0;
        tree_step = tree_wait = tree_done = 0;
    boss_beaten = 0;
    }
    if (tree_alive)                                       // saves from before v0.16.3: green at the trunk's foot
        for (s32 i = 0; i < MAP_W * MAP_H; i++) if (maps[WEST][i] == TRUNK_BLOCK) maps[WEST][i] = TRUNK_GRASS;
    clear_west_gate();                                    // saves from before v0.16.6: a tree may block the way out
    if (leafen_init && leafen_state[LF_NUB] <= LS_MET) nub_clear_ring();   // saves from before v0.17.3
    for (s32 a = 0; a < HOLLOW_IN; a++) {                 // and their rocks beside living land get grass round them
        u8 *m = maps[a];
        for (s32 i = 0; i < MAP_W * MAP_H; i++) {
            if (m[i] != ROCK) continue;
            s32 x = i % MAP_W, y = i / MAP_W;
            if ((x > 0 && is_alive(m[i - 1])) || (x < MAP_W - 1 && is_alive(m[i + 1])) ||
                (y > 0 && is_alive(m[i - MAP_W])) || (y < MAP_H - 1 && is_alive(m[i + MAP_W]))) m[i] = ROCK_GRASS;
        }
    }
    widen_root_pool();                                    // saves from before v0.17.1: the small square pool
    for (s32 i = 0; i < MAP_W * MAP_H; i++) {             // v0.18.2: the marsh holds only bog water and sludge
        if (maps[MARSH][i] == WATER) maps[MARSH][i] = BOG_WATER;
        if (maps[MARSH][i] == LILY_PAD) maps[MARSH][i] = BOG_PAD;
    }
    // rebuild what is not saved (and draw homes from saves made before homes had their own look)
    for (s32 k = 0; k < NUM_HOMES; k++) if (home_done[k]) place_home_tiles(k);
    for (s32 k = 0; k < NUM_HOMES; k++)
        if (home_done[k]) {
            use_area(home_site_area[k]);
            flood_dist(home_dist[k], home_site_x[k], home_site_y[k], home_reach(k));
        }
    use_area(cur_area);
    for (s32 i = 0; i < HIST; i++) { hist_x[i] = px; hist_y[i] = py; }
    for (s32 p = 0; p < party_count; p++) { spirits[party[p]].x = px; spirits[party[p]].y = py; }
    for (s32 i = 0; i < MAX_BLOOMS; i++) blooms[i].active = 0;
    sparkle_life = 0; msgq_len = 0; regen_timer = 0; just_called = -1; pstep = 0;
    set_ground_palette(cur_area);
    draw_area();
    for (s32 r = 0; r < 20; r++) clear_row(r);
    combat_enter();
    draw_life();
    draw_active();
    show_message("WELCOME BACK!");
    music_play(area_song(cur_area));
    return 1;
}

// ---------- START menu (Emerald style) ----------
// A window on the right lists the options; A opens one in a window on the left, B goes back.
enum { M_SPIRITS, M_POUCH, M_BAG, M_WORLD, M_SAVE, M_EXIT, NUM_MENU };
static const char *menu_item[NUM_MENU] = { "SPIRITS", "POUCH", "BAG", "WORLD", "SAVE", "EXIT" };
static int menu_shown(s32 i) { return i != M_POUCH || pouch_on; }   // POUCH once Bark gives it (v0.18.4)
static s32 menu_cursor, menu_page = -1;   // page -1: only the option list
static const char *home_name[NUM_SPIRITS] = { "GROVE", "SPRING", "CAIRN", "NOOK" };

static void draw_window(s32 x0, s32 y0, s32 x1, s32 y1) {
    for (s32 y = y0; y <= y1; y++)
        for (s32 x = x0; x <= x1; x++) {
            char c = ' ';
            if (y == y0) c = x == x0 ? '[' : x == x1 ? ']' : '^';
            else if (y == y1) c = x == x0 ? '{' : x == x1 ? '}' : '_';
            else if (x == x0) c = '(';
            else if (x == x1) c = ')';
            SCREENBLOCK(TEXT_SB)[y * 32 + x] = glyph_bank(c, 2);
        }
}

// Build "LABEL....N" lines: a label, then a number (with "/of" or "%") ending at column w.
static char line[32];   // v0.19: the cooking line is wider
static const char *num_line(const char *label, u32 v, s32 of, s32 w) {
    char num[12], *q = num;
    q = fmt_num(q, v);
    if (of >= 0) { *q++ = '/'; q = fmt_num(q, of); }
    if (of == -2) *q++ = '%';
    *q = 0;
    s32 n = q - num, i = 0;
    for (; label[i] && i < w; i++) line[i] = label[i];
    for (; i < w - n; i++) line[i] = ' ';
    for (s32 k = 0; k < n; k++) line[i++] = num[k];
    line[i] = 0;
    return line;
}

static void draw_menu_list(void) {
    draw_window(19, 1, 29, pouch_on ? 15 : 13);
    for (s32 i = 0, row = 3; i < NUM_MENU; i++) {
        if (!menu_shown(i)) continue;
        put_text_win(21, row, i == menu_cursor ? ">" : " ");
        put_text_win(22, row, menu_item[i]);
        row += 2;
    }
}

// v0.18.4 the pouch page: every spirit you have, carried or in the pouch (or at home). A moves one
// between the two; with five carried, A on a pouch spirit picks it, then A on a carried one swaps them.
static s32 pouch_cursor, pouch_pick = -1, pouch_ncarried, pouch_drawn;
static s32 pouch_list(s32 *out) {
    s32 n = 0;
    for (s32 k = 0; k < NUM_SPIRITS; k++)
        if (spirits[k].state == SP_JOINED && !is_you(k)) out[n++] = k;   // YOU is not shown (Caleb)
    s32 c = n;
    for (s32 k = 0; k < NUM_SPIRITS; k++) if (spirits[k].state == SP_POUCH) out[n++] = k;
    pouch_ncarried = c;
    return n;
}
// The page's sprites, every frame it is open: the carried spirits in a row, and the pouch below with
// the ones in it peeking out of its mouth (Caleb: icons, a real pouch, not pasted on its front). The
// pouch is two sprites: its front rim (OAM_MOTE) over the spirits (51-55), its back (56) behind them.
#define POUCH_X 44
#define POUCH_Y 46
static void draw_pouch_page(void) {
    s32 list[NUM_SPIRITS], n = pouch_list(list), c = pouch_ncarried, m = n - c, bob = (frame >> 4) & 1;
    s32 st = m < POUCH_STATES ? m : POUCH_STATES - 1;   // Caleb: the pouch fills out as spirits go in
    static s32 st_loaded = 0;                          // the slot starts with the empty pouch
    if (st != st_loaded) {
        st_loaded = st;
        copy32(OBJ_TILES + TILE_POUCH * 8, pouch_tiles + st * 96 * 8, 96 * 8);
    }
    for (s32 i = 0; i < 8; i++) sprite_hide(OAM_SPORE + i);
    for (s32 i = 0; i < 5; i++) sprite_hide(OAM_MOTE + 1 + i);
    sprite_hide(OAM_CURSOR);
    for (s32 i = 0; i < n; i++) {
        s32 k = list[i], x, y, slot;
        if (i < c) { x = 76 - c * 12 + i * 24 + 4; y = 34; slot = OAM_SPORE + i; }
        else {
            s32 j = i - c, gap = m <= 4 ? 12 : 10;     // balls sitting in its mouth (Caleb's reference)
            if (j >= 5) continue;
            x = POUCH_X + 24 + (2 * j - (m - 1)) * gap / 2; y = POUCH_Y + pouch_mouth_y[st] - 10; slot = OAM_MOTE + 1 + j;
        }
        if (k == pouch_pick || i == pouch_cursor) y -= i < c ? 2 + bob : 3 + bob;   // the chosen one pops up
        if (i < c) sprite_set(slot, x, y, spirit_tile[k] + (i == pouch_cursor ? bob : 0), 0, spirit_pal[k]);
        else sprite_set(slot, x, y, SPR_BALL + k, 0, spirit_pal[k]);
        oam_shadow[slot * 4 + 2] &= ~(3 << 10);                         // above the window
        if (i == pouch_cursor) {
            sprite_set(OAM_CURSOR, x, y, SPR_SPARKLE + 1, 0, PAL_FX);
            oam_shadow[OAM_CURSOR * 4 + 2] &= ~(3 << 10);
        }
    }
    u16 *o = &oam_shadow[OAM_MOTE * 4];                                  // the front: 64x32 (wide)
    o[0] = ((POUCH_Y + POUCH_FRONT_Y) & 0xFF) | (1 << 14);
    o[1] = (POUCH_X & 0x1FF) | (3 << 14);
    o[2] = TILE_POUCH_FRONT | (PAL_LEAFEN << 12);
    o = &oam_shadow[(OAM_MOTE + 6) * 4];                                 // the back: 64x64
    o[0] = POUCH_Y & 0xFF;
    o[1] = (POUCH_X & 0x1FF) | (3 << 14);
    o[2] = TILE_POUCH | (PAL_LEAFEN << 12);
    pouch_drawn = 1;
}
static void hide_pouch_page(void) {
    if (!pouch_drawn) return;
    pouch_drawn = 0;
    for (s32 i = 0; i < 8; i++) sprite_hide(OAM_SPORE + i);
    for (s32 i = 0; i < 7; i++) sprite_hide(OAM_MOTE + i);
    sprite_hide(OAM_CURSOR);
}
static void pouch_take(s32 k) {                  // out of the pouch, into the party
    Spirit *s = &spirits[k];
    s->state = SP_JOINED; s->x = px; s->y = py; s->area = cur_area;
    party[party_count++] = k;
    draw_active();
}
static void pouch_put(s32 k) {                   // out of the party, into the pouch
    spirits[k].state = SP_POUCH;
    party_remove(k);
}

static s32 spirit_cursor;   // a kind
// v0.19.1 (Caleb): the SPIRITS page lists only the spirits you have: not ones you have not met yet ("???"),
// and not Bark once he is his tree again (his power is YOU, which is not a spirit you pick here).
static int spirit_listed(s32 k) {
    s32 st = spirits[k].state;
    return (st == SP_JOINED || st == SP_HOME || st == SP_POUCH) && !is_you(k);
}
static void spirit_cursor_step(s32 d) {          // to the next listed spirit (d = 1 or -1), or the first if none
    for (s32 n = 0; n < NUM_SPIRITS; n++) {
        spirit_cursor = (spirit_cursor + d + NUM_SPIRITS) % NUM_SPIRITS;
        if (spirit_listed(spirit_cursor)) return;
    }
}
static const char *quest_short[NUM_SPIRITS][2] = {
    { "PLANT 3 FLOWER", "BEDS BY MY GROVE" }, { "PULL THE VAEL", "LEVER, STEPPE" },
    { "FIND MY LOST", "STONE, VALLEY" }, { "CLEAR 30 THORNS", "WITH MY GUSTS" },
    { "HEAL MY GREAT", "TREE, WEST" },
};
static void draw_hearts(s32 col, s32 row, s32 k) {
    for (s32 i = 0; i < 5; i++) put_text_win(col + i, row, i < hearts(k) ? "*" : "@");
}
static s32 beds_by_grove(void) {
    s32 n = 0, hx = home_site_x[KIND_SPROUT], hy = home_site_y[KIND_SPROUT];
    u8 *m = maps[home_site_area[KIND_SPROUT]];
    for (s32 y = hy - 5; y <= hy + 5; y++)
        for (s32 x = hx - 5; x <= hx + 5; x++)
            if (x >= 0 && y >= 0 && x < MAP_W && y < MAP_H &&
                (m[y * MAP_W + x] == FLOWERBED || m[y * MAP_W + x] == FLOWERBED_EMPTY)) n++;
    return n;
}

// Bond k as your companion for good: it leads the party and its power is supercharged.
static void bond_with(s32 k) {
    bonded = k;
    if (in_party(k)) {
        s32 n = 0, order[NUM_SPIRITS];
        order[n++] = k;
        for (s32 p = 0; p < party_count; p++) if (party[p] != k) order[n++] = party[p];
        for (s32 p = 0; p < party_count; p++) party[p] = order[p];
        active = 0;
        draw_active();
    }
    sfx_play(SFX_QUEST);
    show_message("A BOND FOR LIFE!");
}

static void draw_page(void) {
    draw_window(0, 1, 18, 18);   // rows 1-18; the message row (19) stays free
    s32 r = 2;
    if (menu_page == M_SPIRITS) {
        put_text_win(2, r++, "SPIRITS");
        r++;
        if (!spirit_listed(spirit_cursor)) spirit_cursor_step(1);
        for (s32 k = 0; k < NUM_SPIRITS; k++) {   // two rows each (seven fit in rows 4-17)
            Spirit *sp = &spirits[k];
            if (!spirit_listed(k)) continue;
            put_text_win(1, r, k == spirit_cursor ? ">" : " ");
            put_text_win(2, r, spirit_name[k]);
            draw_hearts(11, r, k);
            r++;
            char *p = line;
            str_cat(&p, "  ");
            if (sp->state == SP_HOME) str_cat(&p, "AT HOME");
            else if (sp->state == SP_POUCH) str_cat(&p, "IN POUCH");
            else if (k != KIND_FLICK) { p = fmt_num(p, sp->energy); *p++ = '/'; p = fmt_num(p, energy_max(sp)); }   // Flick: no charges
            if (bonded == k) str_cat(&p, "  BOND");
            *p = 0;
            put_text_win(2, r, line);
            r += 1;
        }
    } else if (menu_page == M_POUCH) {               // the icons and the pouch are sprites (draw_pouch_page)
        put_text_win(2, r, "SPIRIT POUCH");
        s32 list[NUM_SPIRITS], n = pouch_list(list), k = list[pouch_cursor < n ? pouch_cursor : 0];
        char *p = line;
        str_cat(&p, spirit_name[k]);
        if (spirits[k].state == SP_POUCH) str_cat(&p, ": IN POUCH");
        *p = 0;
        put_text_win(2, 15, line);                  // below the pouch (Caleb: it ran into it)
        put_text_win(2, 17, pouch_pick >= 0 ? "SWAP OUT WHO?" : "A: IN / OUT");
    } else if (menu_page >= 10 && menu_page < 10 + NUM_SPIRITS) {
        s32 k = menu_page - 10;
        put_text_win(2, r, spirit_name[k]);
        draw_hearts(11, r++, k);
        r++;
        char *p = line;
        str_cat(&p, "LOVES ");
        str_cat(&p, favorite_name[favorite[k]]);
        *p = 0;
        put_text_win(2, r++, line);
        if (home_done[k]) {
            p = line;
            str_cat(&p, home_name[k]);
            str_cat(&p, " LV ");
            p = fmt_num(p, home_level[k]);
            *p = 0;
            put_text_win(2, r++, line);
        } else put_text_win(2, r++, "NO HOME YET");
        r++;
        if (quest[k] == 0) put_text_win(2, r++, "NO REQUEST YET");
        else if (quest[k] == 2) put_text_win(2, r++, "QUEST DONE!");
        else {
            put_text_win(2, r++, quest_short[k][0]);
            put_text_win(2, r++, quest_short[k][1]);
            if (k == KIND_SPROUT) put_text_win(2, r++, num_line("BEDS", beds_by_grove(), 3, 16));
            if (k == KIND_BREEZE) put_text_win(2, r++, num_line("CLEARED", thorn_quest, 30, 16));
            if (k == KIND_BARK && !tree_alive) {
                u32 n = 0;
                for (s32 i = 0; i < 4; i++) n += (tree_done >> i) & 1;
                put_text_win(2, r++, num_line("TASKS", n, 4, 16));
            }
            if (k == KIND_BARK && tree_alive) {          // v0.17: his new quest, the Leafen
                u32 n = 0;
                for (s32 i = 0; i < NUM_LEAFEN; i++) n += leafen_state[i] >= LS_FREE;
                put_text_win(2, r - 2, "BRING THE LEAFEN");
                put_text_win(2, r - 1, "HOME             ");
                put_text_win(2, r++, num_line("FREED", n, leafen_total(), 16));
            }
        }
        r++;
        if (bonded == k) put_text_win(2, r++, "YOUR COMPANION");
        else if (bonded >= 0) put_text_win(2, r++, "");
        else if (hearts(k) >= 5) put_text_win(2, r++, "A: BOND");
        else put_text_win(2, r++, "BOND AT 5 HEARTS");
    } else if (menu_page >= 20 && menu_page < 20 + NUM_SPIRITS) {
        s32 k = menu_page - 20;
        char *p = line;
        str_cat(&p, "BOND WITH ");
        str_cat(&p, spirit_name[k]);
        *p++ = '?';
        *p = 0;
        put_text_win(2, r++, line);
        r++;
        put_text_win(2, r++, "THIS CAN NEVER");
        put_text_win(2, r++, "BE UNDONE.");
        r++;
        put_text_win(2, r++, "A: BOND");
        put_text_win(2, r++, "B: NOT YET");
    } else if (menu_page == M_BAG) {
        put_text_win(2, r++, "BAG");
        r++;
        put_text_win(2, r++, num_line("STONE", stone, -1, 16));
        put_text_win(2, r++, num_line("FRUIT", fruit, -1, 16));
        put_text_win(2, r++, num_line("SEEDS", seeds, -1, 16));
        put_text_win(2, r++, num_line("WOOD", wood, -1, 16));
        put_text_win(2, r++, num_line("MUSHROOMS", mushrooms, -1, 16));
        r++;
        put_text_win(2, r++, num_line("RELICS", count_set(chest_open, NUM_CHESTS), NUM_CHESTS, 16));
        r += 2;
        put_text_win(2, r++, "HOLD SELECT TO");
        put_text_win(2, r++, "BUILD WITH THEM.");
    } else if (menu_page == M_WORLD) {
        put_text_win(2, r++, "WORLD");
        r++;
        put_text_win(2, r++, num_line("VALLEY", life_percent_of(0), -2, 16));
        put_text_win(2, r++, num_line("STEPPE", life_percent_of(1), -2, 16));
        put_text_win(2, r++, num_line("HOLLOW", life_percent_of(2), -2, 16));
        if (spirits[KIND_BARK].state == SP_JOINED || tree_step)
            put_text_win(2, r++, num_line("GREAT TREE", life_percent_of(WEST), -2, 16));
        r++;
        put_text_win(2, r++, num_line("LORE FOUND", count_set(wreck_read, NUM_WRECKS), NUM_WRECKS, 16));
        s32 running = 0;
        for (s32 a = 0; a < NUM_AREAS; a++) running += pump_on[a];
        put_text_win(2, r++, num_line("PUMPS RUNNING", running, -1, 16));
        put_text_win(2, r++, num_line("PUMPS STOPPED", pumps_stopped, -1, 16));
        r++;
        put_text_win(2, r++, "YOU ARE IN");
        put_text_win(2, r++, area_name[cur_area]);
    } else if (menu_page == M_SAVE) {
        put_text_win(2, r++, "SAVE");
        r++;
        put_text_win(2, r++, "SAVE THE GAME?");
        r++;
        put_text_win(2, r++, "A: SAVE");
        put_text_win(2, r++, "B: BACK");
        r += 2;
        put_text_win(2, r++, "NEW GAME:");
        put_text_win(2, r++, "HOLD L+R, START");
    }
}

static void hide_pouch_page(void);
static void close_menu(void) {
    hide_pouch_page();
    for (s32 r = 1; r <= 18; r++) clear_row(r);
    paused = 0;
    menu_page = -1;
}

static void open_menu(void) {
    for (s32 k = 0; k < 6; k++) sprite_hide(OAM_LEAF + k);   // the leaf aura draws above the window (back on close)
    paused = 1;
    menu_page = -1;
    draw_menu_list();
    sfx_play(SFX_MENU_OPEN);
}

// ---------- v0.19 cooking at a lit campfire ----------
// One meal at a time (a new one takes the old one's place), 45 seconds (Caleb). The snack is eaten at once.
static s32 cook_open, cook_cursor;
static const struct { const char *name, *how; u8 fruit, mush, seed, meal; } recipe[3] = {
    { "FRUIT SNACK", "2 FRUIT: CHARGES FULL", 2, 0, 0, MEAL_NONE },
    { "MUSHROOM STEW", "2 MUSH, 1 FRUIT: +1 HEART", 1, 2, 0, MEAL_HEART },
    { "EMBER TEA", "1 MUSH, 1 SEED: MORE LIGHT", 0, 1, 1, MEAL_LIGHT },
};
static void draw_cook(void) {
    draw_window(0, 1, 29, 15);
    put_text_win(2, 2, "THE CAMPFIRE CRACKLES.");
    for (s32 i = 0; i < 3; i++) {
        put_text_win(1, 4 + i * 3, i == cook_cursor ? ">" : " ");
        put_text_win(2, 4 + i * 3, recipe[i].name);
        put_text_win(3, 5 + i * 3, recipe[i].how);
    }
    char *p = line;
    str_cat(&p, "FRUIT "); p = fmt_num(p, fruit);
    str_cat(&p, "  MUSH "); p = fmt_num(p, mushrooms);
    str_cat(&p, "  SEEDS "); p = fmt_num(p, seeds);
    *p = 0;
    for (s32 c = 1; c < 29; c++) put_text_win(c, 13, " ");
    put_text_win(2, 13, line);
    put_text_win(2, 14, "A: COOK   B: LEAVE");
}
static void open_cook(void) { cook_open = 1; paused = 1; cook_cursor = 0; draw_cook(); sfx_play(SFX_MENU_OPEN); }
static void update_cook(void) {
    s32 old = cook_cursor;
    if (HIT(KEY_DOWN)) cook_cursor = (cook_cursor + 1) % 3;
    if (HIT(KEY_UP)) cook_cursor = (cook_cursor + 2) % 3;
    if (old != cook_cursor) { draw_cook(); sfx_play(SFX_MENU_MOVE); }
    if (HIT(KEY_B) || HIT(KEY_START)) {
        cook_open = 0; paused = 0;
        for (s32 r = 1; r <= 18; r++) clear_row(r);
        sfx_play(SFX_MENU_BACK);
        return;
    }
    if (!HIT(KEY_A)) return;
    const s32 k = cook_cursor;
    if (fruit < recipe[k].fruit || mushrooms < recipe[k].mush || seeds < recipe[k].seed) {
        sfx_play(SFX_FAIL); show_message("NOT ENOUGH FOR THAT."); return;
    }
    fruit -= recipe[k].fruit; mushrooms -= recipe[k].mush; seeds -= recipe[k].seed;
    if (recipe[k].meal == MEAL_NONE) {
        for (s32 i = 0; i < party_count; i++) spirits[party[i]].energy = energy_max(&spirits[party[i]]);
        show_message("YUM! EVERY SPIRIT PERKS UP.");
    } else {
        meal_kind = recipe[k].meal; meal_timer = MEAL_FRAMES;
        if (meal_kind == MEAL_HEART) { hearts_now++; show_message("A WARM STEW! +1 HEART A WHILE."); }
        else { show_message("A HOT TEA! YOUR LIGHT GROWS."); dark_update(); }
    }
    sfx_play(SFX_QUEST);
    draw_active();
    draw_hearts_hud();
    draw_cook();
}
static void update_meal(void) {                       // each frame: a meal wears off after 45 seconds
    if (!meal_timer || --meal_timer) return;
    show_message(meal_kind == MEAL_HEART ? "THE STEW WEARS OFF." : "THE TEA WEARS OFF.");
    meal_kind = MEAL_NONE;
    if (hearts_now > MAX_HEARTS) hearts_now = MAX_HEARTS;
    draw_hearts_hud();
}

// Called every frame while the menu is open.
static void update_menu(void) {
    if (cook_open) { update_cook(); return; }
    if (menu_page == M_POUCH) draw_pouch_page(); else hide_pouch_page();
    if (HIT(KEY_START)) { close_menu(); return; }
    if (menu_page < 0) {
        s32 old = menu_cursor;
        if (HIT(KEY_DOWN)) do menu_cursor = (menu_cursor + 1) % NUM_MENU; while (!menu_shown(menu_cursor));
        if (HIT(KEY_UP)) do menu_cursor = (menu_cursor + NUM_MENU - 1) % NUM_MENU; while (!menu_shown(menu_cursor));
        if (old != menu_cursor) { draw_menu_list(); sfx_play(SFX_MENU_MOVE); }
        if (HIT(KEY_B)) { close_menu(); return; }
        if (HIT(KEY_A)) {
            if (menu_cursor == M_EXIT) { close_menu(); return; }
            menu_page = menu_cursor;
            draw_page();
            sfx_play(SFX_MENU_OPEN);
        }
        return;
    }
    if (menu_page == M_POUCH) {
        s32 list[NUM_SPIRITS], n = pouch_list(list), old = pouch_cursor, c = pouch_ncarried;
        if (pouch_cursor >= n) pouch_cursor = 0;
        if (HIT(KEY_RIGHT)) pouch_cursor = (pouch_cursor + 1) % n;
        if (HIT(KEY_LEFT)) pouch_cursor = (pouch_cursor + n - 1) % n;
        if (HIT(KEY_DOWN) && pouch_cursor < c && n > c) pouch_cursor = c;              // into the pouch
        else if (HIT(KEY_DOWN) && pouch_cursor >= c && pouch_cursor + 3 < n) pouch_cursor += 3;
        if (HIT(KEY_UP) && pouch_cursor >= c + 3) pouch_cursor -= 3;
        else if (HIT(KEY_UP) && pouch_cursor >= c) pouch_cursor = 0;                  // back to the row
        if (old != pouch_cursor) { draw_page(); sfx_play(SFX_MENU_MOVE); }
        if (HIT(KEY_B) && pouch_pick >= 0) { pouch_pick = -1; draw_page(); sfx_play(SFX_MENU_BACK); return; }
        if (HIT(KEY_B)) pouch_cursor = 0;          // back to the list (below)
        if (HIT(KEY_A)) {
            s32 k = list[pouch_cursor], st = spirits[k].state;
            if (st == SP_POUCH && carried() < party_cap()) pouch_take(k);
            else if (st == SP_POUCH) { pouch_pick = pouch_pick == k ? -1 : k; draw_page(); sfx_play(SFX_MENU_OPEN); return; }
            else if (st == SP_JOINED && !is_you(k) && pouch_pick >= 0) { pouch_put(k); pouch_take(pouch_pick); pouch_pick = -1; }
            else if (st == SP_JOINED && !is_you(k) && carried() > 1) pouch_put(k);   // keep one spirit
            else { sfx_play(SFX_FAIL); return; }
            sfx_play(SFX_JOIN);
            draw_page();
            return;
        }
    } else if (menu_page == M_SPIRITS) {
        s32 old = spirit_cursor;
        if (HIT(KEY_DOWN)) spirit_cursor_step(1);
        if (HIT(KEY_UP)) spirit_cursor_step(-1);
        if (old != spirit_cursor) { draw_page(); sfx_play(SFX_MENU_MOVE); }
        if (HIT(KEY_A) && spirit_listed(spirit_cursor)) {
            menu_page = 10 + spirit_cursor; draw_page(); sfx_play(SFX_MENU_OPEN); return;
        }
    } else if (menu_page >= 10 && menu_page < 10 + NUM_SPIRITS) {
        s32 k = menu_page - 10;
        if (HIT(KEY_A) && bonded < 0 && hearts(k) >= 5) { menu_page = 20 + k; draw_page(); sfx_play(SFX_MENU_OPEN); return; }
        if (HIT(KEY_B)) { menu_page = M_SPIRITS; draw_page(); sfx_play(SFX_MENU_BACK); return; }
        return;
    } else if (menu_page >= 20 && menu_page < 20 + NUM_SPIRITS) {
        s32 k = menu_page - 20;
        if (HIT(KEY_A)) { bond_with(k); menu_page = 10 + k; draw_page(); return; }
        if (HIT(KEY_B)) { menu_page = 10 + k; draw_page(); sfx_play(SFX_MENU_BACK); return; }
        return;
    }
    if (HIT(KEY_B)) {
        for (s32 r = 1; r <= 18; r++) clear_row(r);
        menu_page = -1;
        draw_menu_list();
        sfx_play(SFX_MENU_BACK);
        return;
    }
    if (menu_page == M_SAVE && HIT(KEY_A)) {
        save_game();
        sfx_play(SFX_SAVE);
        show_message("GAME SAVED.");
    }
}

// ---------- dev mode (v0.14.1, Caleb: jump into the story to test) ----------
// L + R + B together while playing opens the DEV MODE window (the game pauses). Up/Down pick a
// line, Left/Right change it, A starts a fresh game at that point with those options, B closes.
enum { DEV_JUMP, DEV_STUFF, DEV_CHARGE, DEV_PUMPS, DEV_HEARTS, DEV_GOD, DEV_ITEMS };
enum { J_NEW, J_SPIRITS, J_HOMES, J_GROWN, J_BARK, J_TREE, J_TASKS, J_BOSSES, J_GLOOM, J_HEALED, J_LILY, NUM_JUMPS };
static const char *dev_jump_name[NUM_JUMPS] = { "NEW GAME", "ALL 4 SPIRITS", "4 HOMES LV1", "4 HOMES LV2",
                                               "BARK JOINED", "AT THE TREE", "TREE TASKS DONE",
                                               "4 BOSSES BEATEN", "GLOOMHEART BEATEN", "TREE HEALED", "LILY JOINED" };
static const s32 dev_max[DEV_ITEMS] = { NUM_JUMPS - 1, 1, 1, 1, 1, 1 };
static s32 dev_open, dev_cursor, dev_val[DEV_ITEMS];
static u32 dev_stuff, dev_charge;   // keep materials at 99; charges never run out

static void draw_dev(void) {
    static const char *label[DEV_ITEMS] = { "JUMP TO", "MATERIALS", "CHARGES", "PUMPS", "HEARTS", "WIZARD" };
    static const char *opt[DEV_ITEMS][2] = { { 0, 0 }, { "NORMAL", "99 ALWAYS" }, { "NORMAL", "ENDLESS" },
                                             { "ON", "OFF" }, { "NORMAL", "ALL 5" }, { "NORMAL", "INVINCIBLE" } };
    draw_window(0, 1, 29, 17);
    put_text_win(10, 2, "DEV MODE");
    for (s32 k = 0; k < DEV_ITEMS; k++) {
        s32 r = 4 + k * 2;
        put_text_win(1, r, k == dev_cursor ? ">" : " ");
        put_text_win(2, r, label[k]);
        const char *v = k == DEV_JUMP ? dev_jump_name[dev_val[k]] : opt[k][dev_val[k]];
        char *p = line;
        str_cat(&p, k == dev_cursor ? "<" : " ");
        str_cat(&p, v);
        str_cat(&p, k == dev_cursor ? ">" : " ");
        while (p < line + 17) *p++ = ' ';
        *p = 0;
        put_text_win(11, r, line);
    }
    put_text_win(2, 16, "A: START   B: CLOSE");
}

static void open_dev(void) {
    dev_open = 1;
    paused = 0;
    menu_page = -1;
    dev_val[DEV_STUFF] = dev_stuff;
    dev_val[DEV_CHARGE] = dev_charge;
    dev_val[DEV_PUMPS] = dev_no_pumps;
    dev_val[DEV_GOD] = dev_god;
    draw_dev();
    sfx_play(SFX_MENU_OPEN);
}

static void close_dev(void) {
    dev_open = 0;
    for (s32 r = 1; r <= 18; r++) clear_row(r);
    sfx_play(SFX_MENU_BACK);
}

// Build a home for spirit k at a level, the way playing would (without the messages).
static void dev_home(s32 k, s32 level) {
    home_done[k] = 1;
    home_level[k] = level;
    spirits[k].housed = 1;
    send_home(k);
    place_home_tiles(k);
    use_area(home_site_area[k]);
    flood_dist(home_dist[k], home_site_x[k], home_site_y[k], home_reach(k));
    use_area(cur_area);
    pulse_r[k] = 0;
    pulse_timer[k] = HOME_PULSE - 120;
}

static void dev_join(s32 k) {
    Spirit *sp = &spirits[k];
    sp->area = cur_area; sp->x = px; sp->y = py;
    party_add(k);
}

// A: a fresh game at the chosen point in the story.
static void dev_start(void) {
    s32 j = dev_val[DEV_JUMP];
    dev_open = 0;
    reset_game();
    msgq_len = 0;
    if (j >= J_SPIRITS)
        for (s32 k = 0; k < NUM_HOMES; k++) dev_join(k);
    if (j >= J_HOMES) {
        for (s32 k = 0; k < NUM_HOMES; k++) dev_home(k, j >= J_GROWN ? 2 : 1);
        told_ending = 1;
        told_tips |= TIP_UPGRADE;
    }
    if (j == J_GROWN) {                      // Bark's arrival plays (he lands beside you)
        told_grown = 1;
        told_tips |= TIP_ALL;
        bark_step = 1;
        bark_wait = 60;
    }
    if (j >= J_BARK) {
        told_grown = 1;
        told_tips |= TIP_ALL;
        bark_step = BARK_SCENE_LEN + 1;
        for (s32 k = 0; k < NUM_HOMES; k++) dev_join(k);   // called back from their homes
        dev_join(KIND_BARK);
    }
    if (j >= J_TREE) {
        if (j >= J_TASKS) {                  // Bark's four jobs, done
            u8 *m = maps[WEST];
            for (s32 i = 0; i < MAP_W * MAP_H; i++) {
                if (m[i] == CLAMP) m[i] = DEAD;
                else if (m[i] == DRY_POOL) m[i] = WATER;
                else if (m[i] == ROOT_TIP_DEAD) m[i] = ROOT_TIP_ALIVE;
                else if (m[i] == THORN && i / MAP_W == 21) m[i] = DEAD;
            }
            tree_done = TASKS_ALL;
            tree_step = TREE_SCENE_LEN + 1;
            quest[KIND_BARK] = 1;
        }
        if (j >= J_BOSSES) {                 // the four mini bosses beaten, the way into the tree open
            boss_beaten = (1 << NUM_BOSSES) - 1;
            u8 *m = maps[WEST];
            for (s32 y = DOOR_Y; y <= DOOR_Y + 1; y++)
                for (s32 x = DOOR_X; x <= DOOR_X + 1; x++) m[y * MAP_W + x] = DRY_BRIDGE;
        }
        if (j >= J_HEALED) {                 // Gloomheart beaten, the tree healed, Bark's power yours
            tree_alive = 1;
            revive_step = REVIVE_LEN + 1;
            pouch_on = 1;
            revive_world();
        }
        if (j >= J_LILY) {                   // v0.18: the first six Leafen home, Lily joined, the marsh open
            leafen_setup();
            for (s32 k = 0; k < FIRST_LEAFEN; k++) leafen_state[k] = LS_HOME;
            Spirit *s = &spirits[KIND_LILY];
            s->energy = 3;
            party_add(KIND_LILY);
            lily_step = LILY_SCENE_LEN + 1;
        }
        if (j == J_BOSSES) enter_area(WEST, DOOR_X * 16 + 8, (DOOR_Y + 3) * 16 - 6);   // at the opening
        else if (j == J_GLOOM) {             // in the hollow, the Gloomheart just beaten: the revival starts
            enter_area(HOLLOW_IN, 248, 260);
            boss.alive = 0; gloom_intro = 0;
            for (s32 i = 0; i < MAX_ENEMIES; i++) enemies[i].alive = 0;
            msg_timer = 0; msgq_len = 0; clear_row(19);
            revive_step = 1;
            revive_wait = 30;
        }
        else if (j == J_LILY) enter_area(WEST, 15 * 16, 29 * 16 - 6);   // at the way south, to the marsh
        else enter_area(WEST, WORLD_PX - 18, 13 * 16 - 6);   // at the way in from the valley
    }
    dev_stuff = dev_val[DEV_STUFF];
    dev_charge = dev_val[DEV_CHARGE];
    dev_no_pumps = dev_val[DEV_PUMPS];
    dev_god = dev_val[DEV_GOD];
    if (dev_val[DEV_HEARTS])
        for (s32 k = 0; k < NUM_SPIRITS; k++) {
            fp[k] = 100;
            if (k < NUM_HOMES && quest[k] == 0) quest[k] = 1;   // the quests they would have offered
        }
    for (s32 r = 1; r <= 18; r++) clear_row(r);
    draw_life();
    draw_active();
    show_message("DEV MODE");
}

static void update_dev(void) {
    s32 old = dev_cursor, d = 0;
    if (HIT(KEY_UP) && dev_cursor > 0) dev_cursor--;
    if (HIT(KEY_DOWN) && dev_cursor < DEV_ITEMS - 1) dev_cursor++;
    if (HIT(KEY_LEFT)) d = -1;
    if (HIT(KEY_RIGHT)) d = 1;
    if (d) {
        s32 k = dev_cursor, v = dev_val[k] + d;
        dev_val[k] = v < 0 ? dev_max[k] : v > dev_max[k] ? 0 : v;   // wraps around
    }
    if (HIT(KEY_A)) { sfx_play(SFX_MENU_OPEN); dev_start(); return; }
    if (HIT(KEY_B)) { close_dev(); return; }
    if (d || old != dev_cursor) { draw_dev(); sfx_play(SFX_MENU_MOVE); }
}

// The combo: L, R, and B all down, one of them just pressed.
static int dev_combo(void) {
    u16 all = KEY_L | KEY_R | KEY_B;
    return (keys_now & all) == all && (keys_now & ~keys_prev & all);
}

// Dev options that hold every frame.
static void dev_tick(void) {
    if (dev_stuff) stone = fruit = seeds = wood = 99;
    if (dev_charge) {
        int changed = 0;
        for (s32 p = 0; p < party_count; p++) {
            Spirit *sp = &spirits[party[p]];
            if (sp->energy < energy_max(sp)) { sp->energy = energy_max(sp); changed = 1; }
        }
        if (changed) draw_active();
    }
}

// ---------- title screen ----------
// Shows the valley (your own, if there is a save) behind a big ROOTFLOW banner. With a save:
// CONTINUE / NEW GAME; without one: PRESS START.
#define BANNER_BASE 64   // first banner tile in the font's charblock (after the font)
static void title_screen(int has_save) {
    REG_DISPCNT &= ~DISP_TREE;             // no tree layers behind the title (set_ground_palette restores them)
    for (u32 i = 0; i < 128; i++) oam_shadow[i * 4] = 0x0200;
    oam_flush();
    for (s32 r = 0; r < 20; r++) clear_row(r);
    msgq_len = 0;
    msg_timer = 0;
    for (s32 ty = 0; ty < 3; ty++)
        for (s32 tx = 0; tx < BANNER_W; tx++)
            SCREENBLOCK(TEXT_SB)[(3 + ty) * 32 + 7 + tx] = (BANNER_BASE + ty * BANNER_W + tx) | (3 << 12);
    put_text(2, 7, " SMALL ROOTS. BIG CHANGE. ");   // 26 wide: columns 2-27, centered
    music_play(SONG_TITLE);
    s32 choice = 0, pan = 0, dir = 1;
    cam_y = WORLD_PX / 2 - SCREEN_H / 2;
    for (;;) {
        vsync();
        music_tick();
        REG_BG1HOFS = pan >> 1;
        REG_BG1VOFS = cam_y;
        if ((frame & 31) == 0) animate_water();
        animate_homes();
        poll_keys();
        frame++;
        pan += dir;                                    // drift slowly across the valley and back
        if (pan >= (WORLD_PX - SCREEN_W) * 2 || pan <= 0) dir = -dir;
        if (has_save) {
            if (HIT(KEY_UP | KEY_DOWN)) { choice ^= 1; sfx_play(SFX_MENU_MOVE); }
            draw_window(9, 11, 20, 16);
            put_text_win(11, 12, choice == 0 ? ">" : " ");
            put_text_win(12, 12, "CONTINUE");
            put_text_win(11, 14, choice == 1 ? ">" : " ");
            put_text_win(12, 14, "NEW GAME");
        } else {
            put_text(8, 14, (frame & 32) ? " PRESS  START " : "              ");   // 14 wide, centered
        }
        if (HIT(KEY_START | KEY_A)) break;
    }
    sfx_play(SFX_MENU_OPEN);
    for (s32 r = 0; r < 20; r++) clear_row(r);
    set_ground_palette(cur_area);
    if (!has_save || choice == 1) {
        reset_game();                     // a new game (a save stays until you save over it)
    } else {
        music_play(area_song(cur_area));
        draw_life();
        draw_active();
        show_message("WELCOME BACK!");
    }
    update_camera();
}

int main(void) {
    REG_DISPCNT = 0x0080;   // forced blank while loading

    for (u32 i = 0; i < 16; i++) {
        PAL_BG[i] = bg_pal[i];
        PAL_OBJ[i] = obj_pal[i];
        PAL_OBJ[16 + i] = obj_pal_ripple[i];
        PAL_OBJ[32 + i] = obj_pal_ripple_sleep[i];
        PAL_OBJ[48 + i] = obj_pal_pebble[i];
        PAL_OBJ[64 + i] = obj_pal_pebble_sleep[i];
        PAL_OBJ[80 + i] = obj_pal_breeze[i];
        PAL_OBJ[96 + i] = obj_pal_breeze_sleep[i];
        PAL_OBJ[112 + i] = obj_pal_sprout[i];
        PAL_OBJ[128 + i] = obj_pal_fx[i];
        PAL_OBJ[144 + i] = obj_pal_bark[i];
        PAL_OBJ[160 + i] = obj_pal_lily[i];
        PAL_OBJ[176 + i] = obj_pal_clamp[i];
        PAL_OBJ[192 + i] = obj_pal_combat[i];
        PAL_OBJ[208 + i] = obj_pal_boss[i];
        PAL_OBJ[224 + i] = i ? 0x7FFF : 0;
    }
    PAL_BG[16 + 1] = 0x7FFF;                         // text color: white
    PAL_BG[16 + 2] = (2) | (3 << 5) | (6 << 10);     // text background: dark slate
    PAL_BG[16 + 4] = (16) | (16 << 5) | (18 << 10);  // HUD hearts: empty outline
    PAL_BG[16 + 5] = (29) | (7 << 5) | (8 << 10);     // and red
    // menu windows (bank 2): dark text on cream, gold border
    PAL_BG[32 + 1] = (7) | (7 << 5) | (8 << 10);
    PAL_BG[32 + 2] = (31) | (31 << 5) | (29 << 10);
    PAL_BG[32 + 3] = (29) | (24 << 5) | (13 << 10);
    PAL_BG[32 + 4] = (22) | (16 << 5) | (7 << 10);
    PAL_BG[32 + 5] = (29) | (7 << 5) | (8 << 10);     // heart red

    copy32(CHARBLOCK(0), terrain_tiles, SPRING1_RING1 * 32);   // the map's tiles, without the animation frames
    copy32(CHARBLOCK(0) + SPRING1_RING1 * 32, terrain_tiles + (NOOK2_SPIN + 1) * 32,
           (MARSH_META_FIRST - NOOK2_SPIN - 1) * 32);   // the marsh's own load with the marsh (load_tileset)
    copy32(CHARBLOCK(3), font_tiles, sizeof(font_tiles) / 4);   // screenblocks 24-25
    // the sludge's colors: drawn with the bog water's tiles (1-3 the water, 15 the rim, 6-11 the shore), so
    // it ripples like water, in an oily purple-gray with a sickly sheen (v0.18.1, Caleb: it looked like ground)
    static const u16 sludge_pal[16] = { 0, RGB5(48, 36, 60), RGB5(90, 68, 104), RGB5(170, 150, 120),
                                        RGB5(176, 182, 192), RGB5(104, 110, 122),   // 4-5: a Vael pipe's metal
                                        RGB5(38, 84, 50), RGB5(62, 120, 64), RGB5(126, 190, 96),
                                        RGB5(34, 40, 28), RGB5(52, 60, 40), RGB5(78, 86, 56),
                                        RGB5(238, 60, 52), RGB5(150, 80, 46), 0, RGB5(20, 14, 26) };   // 12 red light, 13 rust
    for (s32 i = 1; i < 16; i++) PAL_BG[SLUDGE_BANK * 16 + i] = sludge_pal[i];
    static const u16 bog_pal[16] = { 0, RGB5(18, 32, 30), RGB5(32, 56, 50), RGB5(84, 120, 100), 0, 0,
                                     RGB5(38, 84, 50), RGB5(62, 120, 64), RGB5(126, 190, 96),
                                     RGB5(34, 40, 28), RGB5(52, 60, 40), RGB5(78, 86, 56), 0,   // 9-11: the shore's mud
                                     RGB5(236, 150, 170), RGB5(240, 228, 190), RGB5(8, 14, 14) };
    for (s32 i = 1; i < 16; i++) PAL_BG[BOG_BANK * 16 + i] = bog_pal[i];
    copy32(CHARBLOCK(3) + BANNER_BASE * 8, banner_tiles, sizeof(banner_tiles) / 4);
    // the Great Tree's tiles load by area (load_tileset, from set_ground_palette)
    PAL_BG[48 + 1] = (30) | (28 << 5) | (22 << 10);   // title banner: cream letters
    PAL_BG[48 + 3] = (3) | (4 << 5) | (4 << 10);      // and a dark shadow
    copy32(OBJ_TILES, sprite_tiles, sizeof(sprite_tiles) / 4);

    for (u32 i = 0; i < 128; i++) oam_shadow[i * 4] = 0x0200;
    oam_flush();

    REG_BG0CNT = 0 | (3 << 2) | (TEXT_SB << 8);             // text: priority 0, font in charblock 3
    // The Great Tree: its trunk (BG3) over the map but under the sprites; its branches (BG2) over
    // the sprites, mostly solid (alpha blend 12/16 over the rest), with a clear box (window 0)
    // around the wizard so he always shows.
    REG_BG3CNT = 2 | (0 << 2) | (TRUNK_SB << 8) | (3 << 14);
    REG_BG2CNT = 0 | (0 << 2) | (CANOPY_SB << 8) | (3 << 14);
    REG_BLDCNT = (1 << 2) | (1 << 6) | (1 << 9) | (1 << 11) | (1 << 12) | (1 << 13);
    REG_BLDALPHA = 12 | (4 << 8);
    REG_WININ = (1 << 0) | (1 << 1) | (1 << 3) | (1 << 4);   // inside the box: no branches
    REG_WINOUT = 0x3F;                                       // outside: everything, with blending
    REG_BG1CNT = 3 | (0 << 2) | (MAP_SB << 8) | (3 << 14);  // map: priority 3, 64x64 tiles

    sound_init();
    REG_WAITCNT = (REG_WAITCNT & ~3) | 3;   // SRAM: 8 wait states
    int has_save = load_game();
    if (!has_save) reset_game();
    update_camera();

    REG_DISPCNT = 0x0000 | (1 << 6) | (1 << 8) | (1 << 9) | (1 << 12) | (cur_area == WEST ? DISP_TREE : 0);
    title_screen(has_save);

    for (;;) {
        vsync();
        music_tick();
        REG_BG1HOFS = cam_x;
        REG_BG1VOFS = cam_y;
        s32 shake = shake_timer ? ((shake_timer & 2) ? 2 : -2) : 0;   // the Great Tree shaking awake
        REG_BG1HOFS = cam_x + shake;
        REG_BG2HOFS = REG_BG3HOFS = cam_x + shake;
        REG_BG2VOFS = REG_BG3VOFS = cam_y;
        clear_box();
        apply_white();
        oam_flush();
        if ((frame & 31) == 0) animate_water();
        animate_homes();

        poll_keys();
        frame++;
        rng_state += keys_now;

        if (HELD(KEY_L) && HELD(KEY_R) && HIT(KEY_START)) { close_menu(); dev_open = 0; reset_game(); continue; }
        if (!dev_open && dev_combo()) { close_menu(); open_dev(); continue; }
        if (dev_open) { update_dev(); update_message(); continue; }
        dev_tick();
        if (!paused && HIT(KEY_START) && !scene_on) { open_menu(); continue; }
        if (paused) { update_menu(); update_message(); continue; }

        // v0.16.3 (Caleb): during a scene nothing moves: not you, the enemies, the bosses, or the pumps
        if (!scene_on) { update_player(); } else pstep = 0;
        update_spirits();
        if (!scene_on) handle_action();
        check_spring();
        update_homes();
        check_milestones();
        update_bark();
        update_revive();
        update_music();
        update_motes();
        check_tree();
        update_tips();
        update_growth();
        if (!scene_on) update_pumps();
        update_blooms();
        if (!scene_on) { update_attack(); update_enemies(); update_boss(); }
        if (hurt_timer) hurt_timer--;
        update_meal();
        update_camera();
        dark_update();
        draw_sprites();
        draw_combat();
        draw_leafen();
        draw_picker();
        draw_boss_bar();

        update_message();
    }
}
