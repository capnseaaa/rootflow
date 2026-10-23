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
       TRUNK_GRASS };   // v0.16.3: the healed trunk's cells, drawn as grass so its foot stands on green
static int is_trunk(u8 t) { return t == TRUNK_BLOCK || t == TRUNK_GRASS; }
static int is_tree_part(u8 t) { return (t >= TRUNK_BLOCK && t <= DRY_POOL) || t == TRUNK_GRASS; }
#define WEST 3        // the Great Tree's area
#define HOLLOW_IN 4   // the hollow inside the Great Tree (v0.16)
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
static const u16 scale[] = { 523, 587, 659, 784, 880, 1047, 1175, 1319 };

// ---------- music: melody on channel 2, bass on channel 3 (sound effects keep channel 1) ----------
static s32 song = -1;
static u32 voice_pos[2], voice_left[2];   // note index and frames left, per voice
static void music_note(s32 v, u16 freq) {
    if (v == 0) {
        if (!freq) { REG_SND2CNT = 0; REG_SND2FREQ = 0x8000; return; }      // rest: silence
        REG_SND2CNT = (10 << 12) | (6 << 8) | (2 << 6);                   // soft pluck that rings ~1 s, 50% duty
        REG_SND2FREQ = freq | 0x8000;
    } else {
        if (!freq) { REG_SND3SEL = 0; return; }
        REG_SND3SEL = 0x0080;                                             // on, 32-sample bank 0
        REG_SND3CNT = 2 << 13;                                            // 50% volume
        REG_SND3FREQ = freq | 0x8000;
    }
}
static void music_play(s32 s) {
    if (s == song) return;
    song = s;
    for (s32 v = 0; v < 2; v++) { voice_pos[v] = 0; voice_left[v] = 0; }
}
static void music_tick(void) {
    sfx_tick();
    if (song < 0) return;
    for (s32 v = 0; v < 2; v++) {
        if (voice_left[v]) { voice_left[v]--; continue; }
        u32 i = voice_pos[v];
        music_note(v, song_freq[song][v][i]);
        voice_left[v] = song_len[song][v][i] * song_fps[song] - 1;
        voice_pos[v] = (i + 1) % song_count[song][v];
    }
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

static void draw_meta(s32 mx, s32 my) {
    if (work_area != cur_area) return;   // off-screen area: memory only
    u16 base = world[my * MAP_W + mx] * 4;
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
}

static int is_solid(u8 t) {
    return t == WATER || t == TREE || t == DEAD_TREE || t == ROCK || t == THORN ||
           t == CHEST || t == CHEST_OPEN || t == WRECK || t == MENHIR || t == DRY_RIVER ||
           t == SAPLING || t == FRUIT_TREE || t == FRUIT_TREE_BARE || t == LANTERN || t == BENCH ||
           t == DAM || t == LEVER || t == LEVER_PULLED || t == LOST_STONE || t == BLOSSOM || t == FOUNTAIN ||
           t == STATUE || t == PINWHEEL || t == VAEL_PUMP || is_home(t) ||
           t == STUMP || t == STUMP_GRASS || t == CHEST_GRASS || t == CHEST_OPEN_GRASS || t == WRECK_GRASS ||
           is_trunk(t) || t == CLAMP || t == DRY_POOL;   // roots can be walked on
}
// Built things stand on living land, so they count as living (LIFE %, resting, waking).
static int is_built(u8 t) {
    return t == PATH || t == FLOWERBED || t == FLOWERBED_EMPTY || t == SAPLING || t == FRUIT_TREE ||
           t == FRUIT_TREE_BARE || t == LANTERN || t == BENCH || t == BLOSSOM || t == FOUNTAIN ||
           t == STATUE || t == PINWHEEL || is_home(t);
}
static int is_alive(u8 t) {
    return t == GRASS || t == GRASS_TUFT || t == FLOWERS || t == TREE || t == STUMP_GRASS || is_built(t);
}

static int is_restorable(u8 t) {
    return !(t == WATER || t == ROCK || t == THORN || t == HOMESITE || t == CHEST || t == CHEST_OPEN ||
             t == WRECK || t == MENHIR || t == DRY_RIVER || t == DRY_BRIDGE || t == DAM || t == LEVER ||
             t == LEVER_PULLED || t == LOST_STONE || is_tree_part(t));
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
};
// And its grass (BG colors 6-8, also trees and moss): the valley's own green, a warm light green
// in the steppe, a deep blue-green in the hollow (Caleb, v0.13).
#define RGB5(r, g, b) (((r) >> 3) | (((g) >> 3) << 5) | (((b) >> 3) << 10))
static const u16 grass_pal[NUM_AREAS][3] = {
    { 0, 0, 0 },   // valley: terrain.png
    { RGB5(96, 140, 54), RGB5(150, 190, 82), RGB5(206, 224, 124) },
    { RGB5(28, 82, 68), RGB5(46, 118, 94), RGB5(84, 158, 122) },
    { RGB5(40, 112, 44), RGB5(70, 160, 64), RGB5(128, 200, 92) },   // the Great Tree: deep, lush
    { 0, 0, 0 },
};
// The Great Tree's tile sets share the slots after the map's tiles (TREE_TILE_BASE) and the layer
// maps in screenblocks 16-23; each area that needs one loads it (v0.16): the dead tree, the
// hollow inside it, or the living tree once it is healed.
enum { TS_NONE, TS_DEAD, TS_HOLLOW, TS_ALIVE };
static s32 tileset_now = TS_NONE;
static u32 tree_alive;                     // the Great Tree is healed (saved)
static void load_tileset(s32 ts) {
    if (ts == tileset_now) return;
    tileset_now = ts;
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
static void set_ground_palette(s32 a) {
    for (s32 i = 0; i < 3; i++) PAL_BG[1 + i] = a == 0 || a == HOLLOW_IN ? bg_pal[1 + i] : ground_pal[a][i];
    for (s32 i = 0; i < 3; i++) PAL_BG[6 + i] = a == 0 || a == HOLLOW_IN ? bg_pal[6 + i] : grass_pal[a][i];
    // the tree's layers show only in the Great Tree's area and its hollow
    REG_DISPCNT &= ~DISP_TREE;
    if (a == WEST) { load_tileset(tree_alive ? TS_ALIVE : TS_DEAD); REG_DISPCNT |= DISP_TREE; }
    if (a == HOLLOW_IN) { load_tileset(TS_HOLLOW); REG_DISPCNT |= 1 << 11; }   // its room only
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
static void sprite_hide(s32 id) { oam_shadow[id * 4] = 0x0200; }
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
#define PAL_BARK_SLEEP 10
#define PAL_CLAMP 11
#define PAL_COMBAT 12
#define PAL_BOSS 13
#define PAL_FLASH 14   // all white: something just got hit

// OAM slots
#define OAM_PLAYER  0
#define OAM_SPIRIT  1   // 1..NUM_SPIRITS
#define OAM_ZZZ     6   // 6..6+NUM_SPIRITS-1
#define OAM_SPARKLE 11
#define OAM_GLOW    12
#define OAM_CURSOR  13
#define OAM_CLAMP   14  // 14..14+NUM_CLAMPS-1 (7): Vael clamps on the Great Tree's roots
#define OAM_ENEMY   21  // 21..28: blobs and drones
#define OAM_ATTACK  29  // 29..32: the active spirit's attack (Bark's slam uses all four)
#define OAM_BOSS    33
#define OAM_HIT     34
#define OAM_SHIELD  35  // Gloomheart's shield ring
#define OAM_LEAF    36  // 36..41: leaves swirling round the wizard as Bark gives his power
#define OAM_SPORE   42  // 42..49: Gloomheart's spores
#define OAM_MOTE    50  // 50..65: rot motes (warnings, dust, shards)
#define OAM_RING    66  // 66..81: the dots of its rot ring
#define OAM_SHADOW  82  // where it will land
#define OAM_STARS   83  // dazed
#define PAL_SHIELD  15

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
enum { SP_ASLEEP, SP_AWAKE, SP_JOINED, SP_HOME, SP_HIDDEN };   // hidden: not in the world yet (Bark)
enum { KIND_SPROUT, KIND_RIPPLE, KIND_STONE, KIND_BREEZE, KIND_BARK };
// housed: the spirit has a home. While it travels with you its power is weaker (fewer charges).
typedef struct { s32 x, y, state, kind, energy, housed, area; } Spirit;   // area: where it is
#define NUM_SPIRITS 5
#define NUM_HOMES 4   // Sprout, Ripple, Pebble, Breeze have homes; Bark has none yet
#define ENERGY_MAX 3
#define ENERGY_MAX_HOUSED 2
#define REGEN_FRAMES 300          // one charge back per 5 s while standing on living ground
static Spirit spirits[NUM_SPIRITS];   // index == kind
static s32 party[NUM_SPIRITS], party_count, active;   // party holds spirit indexes in join order
static const char *spirit_name[] = { "SPROUT", "RIPPLE", "PEBBLE", "BREEZE", "BARK" };
static const char *spirit_skill[] = { "SPROUT: BLOOM", "RIPPLE: BRIDGE", "PEBBLE: BREAK ROCK",
                                      "BREEZE: CLEAR THORNS", "BARK: CUT AND CLEAR" };
static const char *spirit_joined[] = { "SPROUT JOINED! A: BLOOM", "RIPPLE JOINED! R: SWAP",
                                       "PEBBLE JOINED! R: SWAP", "BREEZE JOINED! R: SWAP",
                                       "BARK JOINED... HMPH." };
static const char *spirit_tired[] = { "SPROUT IS TIRED", "RIPPLE IS TIRED", "PEBBLE IS TIRED",
                                      "BREEZE IS TIRED", "BARK: NO. I'M TIRED." };
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
static const u8 spirit_tile[] = { SPR_SPROUT, SPR_RIPPLE, SPR_STONE, SPR_BREEZE, SPR_BARK };
static const u8 spirit_pal[] = { PAL_SPROUT, PAL_RIPPLE, PAL_STONE, PAL_BREEZE, PAL_BARK };
static const u8 spirit_sleep_pal[] = { PAL_SPROUT, PAL_SLEEP, PAL_STONE_SLEEP, PAL_BREEZE_SLEEP, PAL_BARK_SLEEP };
static void draw_home_spirit(s32 i, s32 x, s32 y, s32 b, s32 anim) {
    s32 id = OAM_SPIRIT + i, lv = home_level[i];
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
static void set_meta_gfx(u32 slot, u32 src) { copy32(CHARBLOCK(0) + slot * 32, terrain_tiles + src * 32, 32); }

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
static u32 revive_step, revive_wait, shake_timer;   // the Great Tree coming back (v0.16; steps saved)
static u32 leaf_timer;                              // > 0 while leaves swirl round the wizard
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

static int on_solid(s32 x, s32 y) {
    // feet box: x+3..x+12, y+10..y+15
    s32 xs[2] = { x + 3, x + 12 }, ys[2] = { y + 10, y + 15 };
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++)
            if (is_solid(tile_at(xs[i] >> 4, ys[j] >> 4))) return 1;
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
    for (s32 c = 16; c < 30; c++) SCREENBLOCK(TEXT_SB)[c] = glyph_entry(' ');   // the bar spans the screen
    if (party_count == 0) return;
    Spirit *s = &spirits[party[active]];
    const char *n = is_you(s->kind) ? "YOU" : spirit_name[s->kind];
    s32 len = 0; while (n[len]) len++;
    char num[2] = { (char)('0' + s->energy), 0 };
    put_text(30 - len - 4, 0, " ");
    put_text(30 - len - 3, 0, n);
    put_text(27, 0, " ");
    put_text(28, 0, is_you(s->kind) ? " " : num);       // the wizard needs no charges
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
    bark_step = bark_wait = bark_fall = bark_line = 0;
    tree_step = tree_wait = tree_done = 0;
    boss_beaten = 0;
    tree_alive = revive_step = revive_wait = shake_timer = leaf_timer = 0;
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
            flood_open[gy * g + gx] = !(t == WATER || t == ROCK || t == THORN || t == MENHIR || t == DRY_RIVER ||
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

static void start_bloom(s32 mx, s32 my) {
    for (s32 i = 0; i < MAX_BLOOMS; i++) {
        if (blooms[i].active) continue;
        Bloom *b = &blooms[i];
        b->cx = mx; b->cy = my; b->r = -1; b->timer = 0; b->active = 1;
        int wet = water_near(mx, my, 3);
        b->maxr = (wet ? 4 : 2) + home_level[KIND_SPROUT] + boosted(KIND_SPROUT) + (bonded == KIND_SPROUT) +
                  (harmony ? 2 : 0);
        bloom_distances(b);
        sparkle_x = mx * 16; sparkle_y = my * 16; sparkle_life = 40;
        sfx_play(SFX_BLOOM);
        if (wet && !told_river) { show_message("THE RIVER FEEDS THE ROOTS"); told_river = 1; }
        return;
    }
}

static int player_on_tile(s32 mx, s32 my) {
    return (px + 3) >> 4 <= mx && mx <= (px + 12) >> 4 &&
           (py + 10) >> 4 <= my && my <= (py + 15) >> 4;
}

// The Great Tree region's way back east (x 24..31, rows 12..14): kept free of trees.
static int in_west_gate(s32 mx, s32 my) { return work_area == WEST && mx >= 24 && my >= 12 && my <= 14; }
static void clear_west_gate(void) {
    for (s32 y = 12; y <= 14; y++)
        for (s32 x = 24; x < MAP_W; x++) {
            u8 *c = &maps[WEST][y * MAP_W + x];
            if (*c == TREE || *c == DEAD_TREE || *c == STUMP || *c == STUMP_GRASS) *c = tree_alive ? GRASS : DEAD;
        }
}
static void grow_tile(s32 mx, s32 my, int edge) {
    if (mx < 0 || my < 0 || mx >= MAP_W || my >= MAP_H) return;
    u8 t = tile_at(mx, my);
    int here = work_area == cur_area;   // the player only stands in the area on screen
    if (t == DEAD_TREE || t == STUMP || t == STUMP_GRASS) {   // dead trees and stumps grow back
        if (!(here && player_on_tile(mx, my))) set_tile(mx, my, TREE);
        return;
    }
    if (t == ROOT_TIP_DEAD) { set_tile(mx, my, ROOT_TIP_ALIVE); return; }   // a bloom wakes the Great Tree's roots
    if (!edge && (t == CHEST || t == CHEST_OPEN || t == WRECK)) {   // grass grows in under chests and relics
        set_tile(mx, my, t == CHEST ? CHEST_GRASS : t == CHEST_OPEN ? CHEST_OPEN_GRASS : WRECK_GRASS);
        return;
    }
    if (t != DEAD && t != DEAD_CRACK && t != DIRT && t != DRY_GRASS && t != PEBBLES) return;
    if (edge && t != DIRT) { set_tile(mx, my, DIRT); return; }
    u32 r = rng() % 100;
    u8 nt = r < 55 ? GRASS : r < 80 ? GRASS_TUFT : r < 95 ? FLOWERS : TREE;
    if (nt == TREE && here && player_on_tile(mx, my)) nt = FLOWERS;
    if (nt == TREE && in_west_gate(mx, my)) nt = FLOWERS;   // never close the way back to the valley
    set_tile(mx, my, nt);
}

// A sleeping spirit wakes once living ground reaches the tiles around it.
static void check_wake(void) {
    for (s32 i = 0; i < NUM_SPIRITS; i++) {
        Spirit *s = &spirits[i];
        if (s->state != SP_ASLEEP || s->area != work_area) continue;
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

// ---------- areas ----------
static const signed char area_east[NUM_AREAS] = { 1, -1, -1, 0, -1 }, area_west[NUM_AREAS] = { 3, 0, -1, -1, -1 };
static const signed char area_south[NUM_AREAS] = { 2, -1, -1, -1, -1 }, area_north[NUM_AREAS] = { -1, -1, 0, -1, -1 };
static const char *area_name[NUM_AREAS] = { "THE VALLEY", "THE WINDY STEPPE", "STONE HOLLOW", "THE GREAT TREE",
                                            "INSIDE THE GREAT TREE" };
// Each area's tune (the Great Tree's two use Stone Hollow's quiet one until the tree is healed).
static u32 revive_music, revive_slow;   // the revival's own song; its middle lines stay up longer
static s32 area_song(s32 a) {
    static const u8 song_of[NUM_AREAS] = { SONG_VALLEY, SONG_STEPPE, SONG_HOLLOW, SONG_HOLLOW, SONG_HOLLOW };
    if (revive_music) return SONG_AWAKEN;                 // the Great Tree waking (v0.16.6)
    return a == WEST && tree_alive ? SONG_GREATTREE : song_of[a];
}

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

// Pressing against an open map edge leads to the next area.
static void check_exits(void) {
    if (HELD(KEY_RIGHT) && px >= WORLD_PX - 16 && area_east[cur_area] >= 0)
        enter_area(area_east[cur_area], 2, py);
    else if (HELD(KEY_LEFT) && px <= 0 && area_west[cur_area] >= 0 &&
             (cur_area != 0 || spirits[KIND_BARK].state == SP_JOINED))   // the west opens with Bark
        enter_area(area_west[cur_area], WORLD_PX - 18, py);
    else if (HELD(KEY_DOWN) && py >= WORLD_PX - 16 && area_south[cur_area] >= 0)
        enter_area(area_south[cur_area], px, 2);
    else if (HELD(KEY_UP) && py <= 0 && area_north[cur_area] >= 0)
        enter_area(area_north[cur_area], px, WORLD_PX - 18);
    else if (cur_area == 0 && ((HELD(KEY_UP) && py <= 0) || (HELD(KEY_LEFT) && px <= 0)))
        region_hint();
    else if (cur_area == HOLLOW_IN && HELD(KEY_DOWN) && py >= WORLD_PX - 18)
        enter_area(WEST, DOOR_X * 16 + 8, (DOOR_Y + 2) * 16 - 6);   // out, below the opening
    else if (cur_area == WEST && maps[WEST][DOOR_Y * MAP_W + DOOR_X] == DRY_BRIDGE &&
             (py + 12) >> 4 <= DOOR_Y + 1 && ((px + 8) >> 4) >= DOOR_X && ((px + 8) >> 4) <= DOOR_X + 1)
        enter_area(HOLLOW_IN, DOOR_X * 16 + 8, WORLD_PX - 42);      // into the tree
}

static void update_player(void) {
    s32 speed = HELD(KEY_B) ? 2 : 1;
    if (home_level[KIND_BREEZE] == 1) speed += HELD(KEY_B) ? 1 : (frame & 1);   // nook bonus: 1.5x walk, 3 run
    if (home_level[KIND_BREEZE] == 2) speed += 1;                              // level 2: 2x walk, 3 run
    if (tile_at((px + 8) >> 4, (py + 12) >> 4) == PATH) speed += 1;            // paths are quick
    s32 dx = 0, dy = 0;
    if (HELD(KEY_LEFT))  { dx = -speed; pdir = DIR_LEFT; }
    if (HELD(KEY_RIGHT)) { dx =  speed; pdir = DIR_RIGHT; }
    if (HELD(KEY_UP))    { dy = -speed; pdir = DIR_UP; }
    if (HELD(KEY_DOWN))  { dy =  speed; pdir = DIR_DOWN; }

    // If the player is ever inside something solid, let them walk out instead of being stuck.
    int moved = 0, stuck = on_solid(px, py);
    if (dx && (stuck ? in_world(px + dx, py) : !blocked_at(px + dx, py))) { px += dx; moved = 1; }
    if (dy && (stuck ? in_world(px, py + dy) : !blocked_at(px, py + dy))) { py += dy; moved = 1; }

    if (moved) {
        if ((frame & 7) == 0) pstep++;
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
    "BARK GREW STRONGER!",
};
static const char *trust_msg[NUM_SPIRITS] = {
    "SPROUT TRUSTS YOU FULLY!", "RIPPLE TRUSTS YOU FULLY!", "PEBBLE TRUSTS YOU FULLY!", "BREEZE TRUSTS YOU FULLY!",
    "BARK TRUSTS YOU FULLY!",
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
static int use_power(s32 kind, s32 mx, s32 my) {
    u8 t = tile_at(mx, my);
    int in_map = mx >= 0 && my >= 0 && mx < MAP_W && my < MAP_H;
    if (!in_map) return 0;
    if (kind == KIND_SPROUT) {
        if (t == WATER || t == ROCK || t == BRIDGE || t == THORN || t == MENHIR) return 0;
        start_bloom(mx, my);
    } else if (kind == KIND_RIPPLE && t == DRY_POOL) {
        // Ripple fills the Great Tree's dried root pool, all of it at once
        for (s32 y = my - 2; y <= my + 2; y++)
            for (s32 x = mx - 2; x <= mx + 2; x++)
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
    } else if (kind == KIND_STONE && t == CLAMP) {
        set_tile(mx, my, DEAD);                // a Vael clamp cracks off the Great Tree's root
        stone += 2;
        sparkle_at(mx, my);
        show_message("+2 STONE");
        sfx_play(SFX_ROCK);
    } else if (kind == KIND_STONE) {
        if (t != ROCK) return 0;
        // Broken rock leaves bare ground that can bloom. With Pebble's cairn, two rocks in a line break.
        s32 sx = pdir == DIR_LEFT ? -1 : pdir == DIR_RIGHT ? 1 : 0;
        s32 sy = pdir == DIR_UP ? -1 : pdir == DIR_DOWN ? 1 : 0;
        s32 reach = 1 + home_level[KIND_STONE] + boosted(KIND_STONE) + (bonded == KIND_STONE ? 2 : 0);
        for (s32 k = 0; k < reach && tile_at(mx, my) == ROCK; k++, mx += sx, my += sy) {
            if (mx < 0 || my < 0 || mx >= MAP_W || my >= MAP_H) break;
            set_tile(mx, my, DIRT);
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
static int interact(s32 mx, s32 my) {
    u8 t = tile_at(mx, my);
    s32 i;
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
        hearts_now = MAX_HEARTS;
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
       C_LEAF, C_SPORE, C_MOTE, C_RINGDOT, C_SHADOW, C_STARS, C_GLINT };
static int in_combat_area(void) { return (cur_area == WEST || cur_area == HOLLOW_IN) && !tree_alive; }

static void draw_hearts_hud(void) {
    for (s32 i = 0; i < MAX_HEARTS; i++)
        SCREENBLOCK(TEXT_SB)[11 + i] = glyph_entry(in_combat_area() ? ((u32)i < hearts_now ? '*' : '@') : ' ');
}

// Enemies: blobs creep after you, drones dart at you through the air.
enum { E_BLOB, E_DRONE };
typedef struct { s32 type, x, y, hp, timer, dx, dy, flash, knock, alive; } Enemy;
#define MAX_ENEMIES 8
static Enemy enemies[MAX_ENEMIES];
static const u8 enemy_spawn[][3] = {           // type, tile x, tile y
    { E_BLOB, 4, 6 }, { E_BLOB, 27, 5 }, { E_BLOB, 3, 27 }, { E_BLOB, 28, 26 },
    { E_DRONE, 3, 1 }, { E_DRONE, 28, 1 }, { E_DRONE, 22, 30 },
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
static s32 gloom_act, gloom_step, gloom_tx, gloom_ty, gloom_ring, gloom_ring_hit, gloom_shake, gloom_told_daze;
static const u16 shield_color[NUM_SPIRITS][2] = {   // dark, light
    { RGB5(40, 120, 40), RGB5(140, 230, 100) }, { RGB5(30, 80, 190), RGB5(130, 200, 250) },
    { RGB5(100, 92, 84), RGB5(200, 190, 170) }, { RGB5(150, 190, 210), RGB5(240, 250, 255) },
    { RGB5(110, 70, 40), RGB5(200, 150, 90) },
};
static void gloom_shield_color(void) {
    if (gloom_phase >= NUM_SPIRITS) return;
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

static void spawn_enemies(void) {
    for (s32 i = 0; i < MAX_ENEMIES; i++) enemies[i].alive = 0;
    for (u32 i = 0; i < NUM_SPAWNS && i < MAX_ENEMIES; i++) {
        Enemy *e = &enemies[i];
        e->type = enemy_spawn[i][0];
        e->x = enemy_spawn[i][1] * 16; e->y = enemy_spawn[i][2] * 16;
        e->hp = e->type == E_BLOB ? 3 : 2;
        e->timer = i * 37; e->dx = e->dy = 0; e->flash = e->knock = 0;
        e->alive = 1;
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
    hearts_now = MAX_HEARTS;
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
        queue_message("THE GLOOMHEART THROBS...");
        queue_message("BARK: THE ROT. IN MY HEART.");
        queue_message("ALL OF YOU. ONE AT A TIME.");
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
    if (best < 0) { hearts_now = MAX_HEARTS; return; }
    s32 hx = home_site_x[best], hy = home_site_y[best] + 1;
    enter_area(home_site_area[best], hx * 16, hy * 16 - 6);
    unstick_player();
    hearts_now = MAX_HEARTS;
    draw_hearts_hud();
}

static void hurt(s32 n, s32 fromx, s32 fromy) {
    if (hurt_timer || dev_god) return;
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

static int can_fight(s32 kind) { return kind == KIND_BARK || home_level[kind] >= 2; }

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
        e->hp -= atk.dmg;
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
        if (boss.kind == B_GLOOM && gloom_phase < NUM_SPIRITS) {   // a shield: only its spirit cracks it
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
                if (gloom_phase < NUM_SPIRITS) {
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
        if (e->knock) { e->knock--; move_enemy(e, e->dx * 2, e->dy * 2, e->type == E_DRONE); continue; }
        s32 dx = pcx - (e->x + 8), dy = pcy - (e->y + 8);
        s32 near = dx * dx + dy * dy < 100 * 100;
        if (e->type == E_BLOB) {                             // creeps after you, slowly
            if (e->timer & 1) continue;
            if (near) move_enemy(e, sgn(dx), sgn(dy), 0);
            else {
                if ((e->timer & 127) == 0) { e->dx = (s32)(rng() % 3) - 1; e->dy = (s32)(rng() % 3) - 1; }
                move_enemy(e, e->dx, e->dy, 0);
            }
        } else {                                             // darts at you, rests, darts again
            u32 t = e->timer % 100;
            if (t == 0) { e->dx = near ? sgn(dx) : (s32)(rng() % 3) - 1; e->dy = near ? sgn(dy) : (s32)(rng() % 3) - 1; }
            if (t < 36) move_enemy(e, e->dx * 2, e->dy * 2, 1);
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
static const u8 gloom_plan[NUM_SPIRITS + 1][4] = {
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
    s32 heart = gloom_phase >= NUM_SPIRITS, t = boss.timer;
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
            sfx_play(SFX_ROCK);
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
            sfx_play(SFX_ROCK);
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
            if (t == 54) sfx_play(SFX_ROCK);
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
            sfx_play(SFX_BUILD);
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
    if (boss.kind == B_GLOOM && gloom_phase < NUM_SPIRITS) {   // which spirit's shield is up
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
        if (on && m->life) sprite_set(OAM_MOTE + i, (m->x >> 4) - cam_x, (m->y >> 4) - cam_y, SPR_COMBAT + m->c, 0, PAL_COMBAT);
        else sprite_hide(OAM_MOTE + i);
    }
    s32 r = gloom_ring < 0 ? -gloom_ring : gloom_ring;
    int ring = on && boss.alive && gloom_ring;
    for (s32 k = 0; k < RING_DOTS; k++) {
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
    if (boss.alive && boss.kind == B_GLOOM && gloom_phase < NUM_SPIRITS &&
        shx > -32 && shx < SCREEN_W && shy > -32 && shy < SCREEN_H) {   // off screen: hidden, not wrapped round
        u16 *o = &oam_shadow[OAM_SHIELD * 4];
        o[0] = (shy & 0xFF);
        o[1] = (shx & 0x1FF) | (2 << 14);
        o[2] = (TILE_BOSS + 10 * 16) | (1 << 10) | (PAL_SHIELD << 12);
    } else sprite_hide(OAM_SHIELD);
    int aura = !leaf_timer && party_count && is_you(spirits[party[active]].kind);   // v0.16.6: YOU is chosen
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

static void use_ability(void) {
    s32 fx = px + 8, fy = py + 12;
    if (pdir == DIR_LEFT)  fx -= 14;
    if (pdir == DIR_RIGHT) fx += 14;
    if (pdir == DIR_UP)    fy -= 14;
    if (pdir == DIR_DOWN)  fy += 14;
    // An enemy in front: the active spirit attacks instead (free, with a short cooldown).
    if (party_count && can_fight(spirits[party[active]].kind) && enemy_ahead()) {
        if (!atk_cooldown) start_attack(spirits[party[active]].kind);
        return;
    }
    if (interact(fx >> 4, fy >> 4)) return;
    if (!party_count) return;
    Spirit *s = &spirits[party[active]];
    if (is_you(s->kind)) {                                   // the wizard: Bark's power, no charges
        if (!use_power(KIND_BARK, fx >> 4, fy >> 4)) sfx_play(SFX_FAIL);
        return;
    }
    if (s->energy == 0 && !(bonded == KIND_SPROUT && s->kind == KIND_SPROUT)) {
        sfx_play(SFX_FAIL);
        show_message(spirit_tired[s->kind]);
        if (msgq_len == 0) queue_message(REST_TIP);
        return;
    }
    if (!use_power(s->kind, fx >> 4, fy >> 4)) { sfx_play(SFX_FAIL); return; }
    if (!(bonded == KIND_SPROUT && s->kind == KIND_SPROUT)) s->energy--;   // a bonded Sprout blooms for free
    add_fp(s->kind, 2);
    draw_active();
    if (!(told_tips & TIP_REST)) { told_tips |= TIP_REST; queue_message(REST_TIP); }
}

static void party_add(s32 i) {
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
                                           BLOSSOM, FOUNTAIN, STATUE, PINWHEEL };
static const u8 build_cost[NUM_BUILDS][3] = {   // stone, seeds, fruit
    { 1, 0, 0 }, { 0, 2, 0 }, { 0, 1, 1 }, { 3, 0, 0 }, { 2, 0, 1 }, { 5, 5, 3 },
    { 0, 3, 2 }, { 4, 0, 2 }, { 6, 0, 0 }, { 2, 2, 0 },
};
static const char *build_text[NUM_BUILDS] = {
    "PATH: 1 STONE", "FLOWER BED: 2 SEEDS", "SAPLING: 1 SEED 1 FRUIT", "LANTERN: 3 STONE",
    "BENCH: 2 STONE 1 FRUIT", "HOME: 5 STONE 5 SEED 3 FRUIT",
    "BLOSSOM: 3 SEEDS 2 FRUIT", "FOUNTAIN: 4 STONE 2 FRUIT", "STATUE: 6 STONE", "PINWHEEL: 2 STONE 2 SEEDS",
};
static int build_ok(s32 b) { return b < B_BLOSSOM || unlocked[b - B_BLOSSOM]; }
// The order L/R walks the list in: the home upgrade first (Caleb, v0.13.1), then the rest.
static const u8 build_order[NUM_BUILDS] = { B_UPGRADE, B_PATH, B_BED, B_SAPLING, B_LANTERN, B_BENCH,
                                            B_BLOSSOM, B_FOUNTAIN, B_STATUE, B_PINWHEEL };
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
static const u8 favorite[NUM_SPIRITS] = { 2, 1, 0, 2, 1 };       // 0 stone, 1 fruit, 2 seeds
static const char *favorite_name[3] = { "STONE", "FRUIT", "SEEDS" };
static const char *loves_msg[NUM_SPIRITS] = {
    "SPROUT LOVES THE SEEDS!", "RIPPLE LOVES THE FRUIT!", "PEBBLE LOVES THE STONE!", "BREEZE LOVES THE SEEDS!",
    "BARK: ...FINE. THANKS.",
};
static u32 b_hold;
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
        if (b_hold && b_hold < 10 && !HELD(KEY_SELECT)) try_gift();
        b_hold = 0;
    }
    // R swaps to the next spirit, L to the previous one.
    if (HELD(KEY_SELECT)) { build_mode(); return; }   // SELECT is the builder
    s32 step = HIT(KEY_R) ? 1 : HIT(KEY_L) ? party_count - 1 : 0;
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
        if (s->state == SP_JOINED || s->state == SP_HIDDEN || s->area != cur_area) continue;
        if ((s->state == SP_HOME) != (pass == 1)) continue;
        // A housed spirit is drawn inside its home (the roof for Pebble and Breeze); it also answers near the site.
        if (!near_px(s->x, s->y, 28) && !(s->state == SP_HOME && near_px(home_site_x[i] * 16, home_site_y[i] * 16, 24))) continue;
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
    if (is_alive(tile_at(fx, fy)) && ++regen_timer >= regen) {
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
        if (hearts_now < MAX_HEARTS) { hearts_now = MAX_HEARTS; draw_hearts_hud(); }
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
enum { BS_SAY, BS_CRACK, BS_FALL, BS_THUMP };
static const struct { u8 act; const char *msg; } bark_scene[] = {
    { BS_CRACK, "CRACK...!" }, { BS_FALL, 0 }, { BS_THUMP, "THUMP." }, { BS_SAY, "DID SOMETHING FALL?" },
    { BS_SAY, "BARK: OW. OW. MY ROOTS." }, { BS_SAY, "BARK: WHAT? STOP STARING." },
    { BS_SAY, "BARK: I'M BARK. I WAS PART" }, { BS_SAY, "OF THE GREAT TREE." },
    { BS_SAY, "VAEL CUT IT DOWN. NOW I'M..." }, { BS_SAY, "...THIS. A STUMP." },
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

static void update_bark(void) {
    if (bark_fall) bark_fall--;
    scene_on = bark_step && bark_step <= BARK_SCENE_LEN;
    update_tree_scene();
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
enum { RV_SAY, RV_SHAKE, RV_FLOAT, RV_WHITE, RV_CLEAN, RV_LEAFUP, RV_FADE, RV_OUT, RV_UNFADE, RV_STILL, RV_LEAVES };
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
    { RV_SAY, "A FEW GOT LOOSE. THEY HIDE" }, { RV_SAY, "IN THE VALLEY, THE STEPPE," },
    { RV_SAY, "AND STONE HOLLOW. LOOK AROUND." }, { RV_SAY, "THEY'RE EVERYWHERE, REALLY." },
    { RV_SAY, "THEY WANDER. SEARCH THE" }, { RV_SAY, "FAR CORNERS, TOO." }, { RV_SAY, "SOME MIGHT NEED PERSUADING." },
    { RV_SAY, "THE REST ARE LOCKED UP IN" },
    { RV_SAY, "VAEL'S FACTORY, FAR NORTH." }, { RV_SAY, "BRING THEM HOME. PLEASE." },
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
    if ((msg_timer && !msg_is_prompt) || msgq_len) return;
    u32 act = revive_scene[revive_step - 1].act;
    const char *m = revive_scene[revive_step - 1].msg;
    revive_step++;
    revive_wait = 20;
    if (act == RV_SHAKE) {                           // until the light comes
        shake_timer = 2000; sfx_play(SFX_ROCK);
        revive_music = 1; revive_slow = 1; music_play(SONG_AWAKEN);
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
    if (act == RV_FADE) { white_mode = 4; white_t = 0; revive_wait = 30; chime(1568, 30); }
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
    if (act == RV_UNFADE) { shake_timer = 2000; sfx_play(SFX_ROCK); white_slow = 1; white_mode = 3; white_t = 0; revive_wait = 0; }
    if (act == RV_STILL) {
        shake_timer = 0; sfx_play(SFX_WAKE); revive_wait = 30;
        revive_music = 0; revive_slow = 0; music_play(area_song(WEST));   // the healed tree's own song
    }
    if (act == RV_LEAVES) { leaf_timer = 150; sfx_play(SFX_QUEST); }
    if (m) { show_message(m); if (revive_slow) msg_timer = 240; }   // Caleb: these went by too fast
}

// Milestones, checked twice a second: an area reaching 100% LIFE, and harmony.
static const char *area_full[NUM_AREAS] = { "THE VALLEY IS FULLY ALIVE!", "THE STEPPE IS FULLY ALIVE!",
                                            "STONE HOLLOW IS FULLY ALIVE!", "THE GREAT WOOD IS ALIVE!", "" };
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
        wy = clamp_abs((boss.y + 16 - (py + 8)) >> 1, SCREEN_H / 2 - 32);
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
    obj_set(OAM_PLAYER, px - cam_x, py - cam_y - 16, TILE_PLAYER + f * 8, pdir == DIR_LEFT, PAL_MAIN, 1);
    if (hurt_timer & 4) sprite_hide(OAM_PLAYER);   // blinking just after a hit

    for (s32 i = 0; i < NUM_SPIRITS; i++) {
        Spirit *s = &spirits[i];
        if (s->area != cur_area) { sprite_hide(OAM_SPIRIT + i); sprite_hide(OAM_ZZZ + i); continue; }
        s32 anim = (frame >> 4) & 1;
        s32 b = bob[((frame >> 2) + i * 5) & 15];
        s32 sx = s->x - cam_x, sy = s->y - cam_y - 4;
        if (i == KIND_BARK && float_on) {                // the revival: Bark floats, then shoots up
            if (float_on == 2) sprite_hide(OAM_SPIRIT + i);
            else sprite_set(OAM_SPIRIT + i, float_x - cam_x, float_y - cam_y - 4 - float_h + (float_up ? 0 : b),
                            spirit_tile[s->kind] + anim, 0, spirit_pal[s->kind]);
            sprite_hide(OAM_ZZZ + i);
        } else if (s->state == SP_HIDDEN || is_you(i)) {  // Bark lives in his tree once it is healed
            sprite_hide(OAM_SPIRIT + i);
            sprite_hide(OAM_ZZZ + i);
        } else if (s->state == SP_HOME) {
            draw_home_spirit(i, sx, s->y - cam_y, b, anim);
            sprite_hide(OAM_ZZZ + i);
        } else if (s->state == SP_ASLEEP) {
            // gray, still, with a sleep bubble that drifts up
            sprite_set(OAM_SPIRIT + i, sx, sy + 2, spirit_tile[s->kind], 0, spirit_sleep_pal[s->kind]);
            sprite_set(OAM_ZZZ + i, sx + 8, sy - 10 - ((frame >> 3) & 3), SPR_ZZZ, 0, PAL_RIPPLE);
        } else {
            if (i == KIND_BARK && bark_fall) b = -(s32)(bark_fall * bark_fall) / 6;   // dropping in from above
            sprite_set(OAM_SPIRIT + i, sx, sy + b, spirit_tile[s->kind] + anim, 0, spirit_pal[s->kind]);
            sprite_hide(OAM_ZZZ + i);
        }
    }

    // Vael clamps on the Great Tree's roots: shown while their cell is still a CLAMP.
    for (s32 i = 0; i < NUM_CLAMPS; i++) {
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
#define SAVE_MAGIC 0x38534652u   // "RFS8" (v0.16: the healed tree)
#define SAVE_MAGIC_V15 0x37534652u   // "RFS7": v0.15 saves still load
#define SAVE_MAGIC_V14 0x36534652u   // "RFS6": v0.14 saves still load
#define SAVE_MAGIC_V13 0x35534652u   // "RFS5": v0.13 saves (three areas) still load
#define OLD_AREAS 3

// Everything that makes up a game in progress, in save order.
static const struct { void *p; u32 n; } save_fields[] = {
    { &px, 4 }, { &py, 4 }, { &pdir, 4 }, { &cur_area, 4 },
    { maps, sizeof(maps[0]) * OLD_AREAS }, { restorable, 4 * OLD_AREAS },
    { spirits, sizeof(spirits) }, { party, sizeof(party) }, { &party_count, 4 }, { &active, 4 },
    { home_done, sizeof(home_done) }, { pulse_r, sizeof(pulse_r) }, { pulse_timer, sizeof(pulse_timer) },
    { &stone, 4 }, { &fruit, 4 }, { &seeds, 4 },
    { chest_open, sizeof(chest_open) }, { wreck_read, sizeof(wreck_read) }, { picked, sizeof(picked[0]) * OLD_AREAS },
    { &bonus_charge, 4 }, { &pulse_div, 4 }, { home_level, sizeof(home_level) }, { &build_item, 4 },
    { fp, sizeof(fp) }, { quest, sizeof(quest) }, { unlocked, sizeof(unlocked) }, { &bonded, 4 }, { &thorn_quest, 4 },
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
};
#define NUM_SAVE_FIELDS (sizeof(save_fields) / sizeof(save_fields[0]))
#define NUM_V15_FIELDS (NUM_SAVE_FIELDS - 3)
#define NUM_V14_FIELDS (NUM_SAVE_FIELDS - 4)
#define NUM_V13_FIELDS (NUM_SAVE_FIELDS - 15)

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
    if (nf < NUM_SAVE_FIELDS) { tree_alive = revive_step = revive_wait = 0; }
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
enum { M_SPIRITS, M_BAG, M_WORLD, M_SAVE, M_EXIT, NUM_MENU };
static const char *menu_item[NUM_MENU] = { "SPIRITS", "BAG", "WORLD", "SAVE", "EXIT" };
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
static char line[24];
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
    draw_window(19, 1, 29, 13);
    for (s32 i = 0; i < NUM_MENU; i++) {
        put_text_win(21, 3 + i * 2, i == menu_cursor ? ">" : " ");
        put_text_win(22, 3 + i * 2, menu_item[i]);
    }
}

static s32 spirit_cursor;
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
        for (s32 k = 0; k < NUM_SPIRITS; k++) {   // three rows each: all five fit in rows 4-17
            Spirit *sp = &spirits[k];
            put_text_win(1, r, k == spirit_cursor ? ">" : " ");
            if (sp->state == SP_ASLEEP || sp->state == SP_HIDDEN) { put_text_win(2, r, "???"); r += 3; continue; }
            put_text_win(2, r, spirit_name[k]);
            draw_hearts(11, r, k);
            r++;
            char *p = line;
            str_cat(&p, "  ");
            if (sp->state == SP_HOME) str_cat(&p, "AT HOME");
            else { p = fmt_num(p, sp->energy); *p++ = '/'; p = fmt_num(p, energy_max(sp)); }
            if (bonded == k) str_cat(&p, "  BOND");
            *p = 0;
            put_text_win(2, r, line);
            r += 2;
        }
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
            if (k == KIND_BARK) {
                u32 n = 0;
                for (s32 i = 0; i < 4; i++) n += (tree_done >> i) & 1;
                put_text_win(2, r++, num_line("TASKS", n, 4, 16));
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

static void close_menu(void) {
    for (s32 r = 1; r <= 18; r++) clear_row(r);
    paused = 0;
    menu_page = -1;
}

static void open_menu(void) {
    paused = 1;
    menu_page = -1;
    draw_menu_list();
    sfx_play(SFX_MENU_OPEN);
}

// Called every frame while the menu is open.
static void update_menu(void) {
    if (HIT(KEY_START)) { close_menu(); return; }
    if (menu_page < 0) {
        s32 old = menu_cursor;
        if (HIT(KEY_DOWN)) menu_cursor = (menu_cursor + 1) % NUM_MENU;
        if (HIT(KEY_UP)) menu_cursor = (menu_cursor + NUM_MENU - 1) % NUM_MENU;
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
    if (menu_page == M_SPIRITS) {
        s32 old = spirit_cursor;
        if (HIT(KEY_DOWN)) spirit_cursor = (spirit_cursor + 1) % NUM_SPIRITS;
        if (HIT(KEY_UP)) spirit_cursor = (spirit_cursor + NUM_SPIRITS - 1) % NUM_SPIRITS;
        if (old != spirit_cursor) { draw_page(); sfx_play(SFX_MENU_MOVE); }
        if (HIT(KEY_A) && spirits[spirit_cursor].state != SP_ASLEEP && spirits[spirit_cursor].state != SP_HIDDEN) {
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
enum { J_NEW, J_SPIRITS, J_HOMES, J_GROWN, J_BARK, J_TREE, J_TASKS, J_BOSSES, J_GLOOM, J_HEALED, NUM_JUMPS };
static const char *dev_jump_name[NUM_JUMPS] = { "NEW GAME", "ALL 4 SPIRITS", "4 HOMES LV1", "4 HOMES LV2",
                                               "BARK JOINED", "AT THE TREE", "TREE TASKS DONE",
                                               "4 BOSSES BEATEN", "GLOOMHEART BEATEN", "TREE HEALED" };
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
            revive_world();
        }
        if (j == J_BOSSES) enter_area(WEST, DOOR_X * 16 + 8, (DOOR_Y + 3) * 16 - 6);   // at the opening
        else if (j == J_GLOOM) {             // in the hollow, the Gloomheart just beaten: the revival starts
            enter_area(HOLLOW_IN, 248, 260);
            boss.alive = 0;
            for (s32 i = 0; i < MAX_ENEMIES; i++) enemies[i].alive = 0;
            msg_timer = 0; msgq_len = 0; clear_row(19);
            revive_step = 1;
            revive_wait = 30;
        }
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
        PAL_OBJ[160 + i] = obj_pal_bark_sleep[i];
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

    copy32(CHARBLOCK(0), terrain_tiles, sizeof(terrain_tiles) / 4);
    copy32(CHARBLOCK(3), font_tiles, sizeof(font_tiles) / 4);   // screenblocks 24-25
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
        check_tree();
        update_tips();
        update_growth();
        if (!scene_on) update_pumps();
        update_blooms();
        if (!scene_on) { update_attack(); update_enemies(); update_boss(); }
        if (hurt_timer) hurt_timer--;
        update_camera();
        draw_sprites();
        draw_combat();
        draw_boss_bar();

        update_message();
    }
}
