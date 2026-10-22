// Rootflow spirit toy (GBA).
// Walk the dead valley, find the sprout spirit, and bloom life into the land.
// D-pad: move   B: run   A: greet / use spirit   R / L: next / previous spirit   START: reset
// Hold SELECT to build: L/R pick an item, A places it, B removes one.
// Spirits: Sprout blooms life, Ripple bridges water, Pebble breaks rocks, Breeze clears thorns,
// Bark (appears once all four homes stand) cuts trees and clears stumps.
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

#define TEXT_SB   26
#define MAP_SB    28

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
       STUMP, STUMP_GRASS, CHEST_GRASS, CHEST_OPEN_GRASS, WRECK_GRASS };
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
           t == STUMP || t == STUMP_GRASS || t == CHEST_GRASS || t == CHEST_OPEN_GRASS || t == WRECK_GRASS;
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
             t == LEVER_PULLED || t == LOST_STONE);
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
};
// And its grass (BG colors 6-8, also trees and moss): the valley's own green, a warm light green
// in the steppe, a deep blue-green in the hollow (Caleb, v0.13).
#define RGB5(r, g, b) (((r) >> 3) | (((g) >> 3) << 5) | (((b) >> 3) << 10))
static const u16 grass_pal[NUM_AREAS][3] = {
    { 0, 0, 0 },   // valley: terrain.png
    { RGB5(96, 140, 54), RGB5(150, 190, 82), RGB5(206, 224, 124) },
    { RGB5(28, 82, 68), RGB5(46, 118, 94), RGB5(84, 158, 122) },
};
static void set_ground_palette(s32 a) {
    for (s32 i = 0; i < 3; i++) PAL_BG[1 + i] = a == 0 ? bg_pal[1 + i] : ground_pal[a][i];
    for (s32 i = 0; i < 3; i++) PAL_BG[6 + i] = a == 0 ? bg_pal[6 + i] : grass_pal[a][i];
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
static void show_message(const char *s) {
    clear_row(19);
    s32 n = 0; while (s[n]) n++;
    s32 col = (30 - n - 2) / 2;
    put_text(col, 19, " ");
    put_text(col + 1, 19, s);
    put_text(col + 1 + n, 19, " ");
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
// A repeating prompt ("A: ..."). It never pushes queued messages aside.
static void prompt(const char *s) {
    if (msgq_len == 0 && msg_timer < 100) show_message(s);
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

// OAM slots
#define OAM_PLAYER  0
#define OAM_SPIRIT  1   // 1..NUM_SPIRITS
#define OAM_ZZZ     6   // 6..6+NUM_SPIRITS-1
#define OAM_SPARKLE 11
#define OAM_GLOW    12
#define OAM_CURSOR  13

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
                                       "BARK JOINED! R: SWAP" };
static const char *spirit_tired[] = { "SPROUT IS TIRED", "RIPPLE IS TIRED", "PEBBLE IS TIRED",
                                      "BREEZE IS TIRED", "BARK IS TIRED" };
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
static s32 build_item;                 // the item SELECT-building places
static s32 cursor_mx = -1, cursor_my, cursor_ok;   // build preview: faced tile (mx -1 = hidden)
static u32 grow_timer;                 // built things grow every 10 s
static s32 home_reach(s32 k) { return home_level[k] >= 2 ? HOME_REACH : HOME_REACH_L1; }
#define HOME_PULSE 360
static s32 pulse_r[NUM_SPIRITS], pulse_timer[NUM_SPIRITS];
// Each home's distance map is computed once, when it is built; pulses only paint rings.
static u8 home_dist[NUM_SPIRITS][(HOME_REACH * 2 + 1) * (HOME_REACH * 2 + 1)];
static u32 told_rest;
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

static void draw_active(void) {
    for (s32 c = 16; c < 30; c++) SCREENBLOCK(TEXT_SB)[c] = 0;
    if (party_count == 0) return;
    Spirit *s = &spirits[party[active]];
    const char *n = spirit_name[s->kind];
    s32 len = 0; while (n[len]) len++;
    char num[2] = { (char)('0' + s->energy), 0 };
    put_text(30 - len - 4, 0, " ");
    put_text(30 - len - 3, 0, n);
    put_text(27, 0, " ");
    put_text(28, 0, num);
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
    spirit_init(KIND_BARK, BARK_SPOT_X, BARK_SPOT_Y, SP_HIDDEN);   // appears when all four homes stand
    spirits[KIND_STONE].area = STONE_AREA;     // Pebble sleeps in the Windy Steppe
    spirits[KIND_BREEZE].area = BREEZE_AREA;   // Breeze sleeps in Stone Hollow
    for (s32 i = 0; i < NUM_SPIRITS; i++) home_done[i] = 0;
    told_ending = 0;
    for (s32 i = 0; i < NUM_SPIRITS; i++) home_level[i] = 0;
    build_item = 0;
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
    told_rest = 0;
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
                                        t == DAM || t == LOST_STONE);
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

static void grow_tile(s32 mx, s32 my, int edge) {
    if (mx < 0 || my < 0 || mx >= MAP_W || my >= MAP_H) return;
    u8 t = tile_at(mx, my);
    int here = work_area == cur_area;   // the player only stands in the area on screen
    if (t == DEAD_TREE || t == STUMP || t == STUMP_GRASS) {   // dead trees and stumps grow back
        if (!(here && player_on_tile(mx, my))) set_tile(mx, my, TREE);
        return;
    }
    if (!edge && (t == CHEST || t == CHEST_OPEN || t == WRECK)) {   // grass grows in under chests and relics
        set_tile(mx, my, t == CHEST ? CHEST_GRASS : t == CHEST_OPEN ? CHEST_OPEN_GRASS : WRECK_GRASS);
        return;
    }
    if (t != DEAD && t != DEAD_CRACK && t != DIRT && t != DRY_GRASS && t != PEBBLES) return;
    if (edge && t != DIRT) { set_tile(mx, my, DIRT); return; }
    u32 r = rng() % 100;
    u8 nt = r < 55 ? GRASS : r < 80 ? GRASS_TUFT : r < 95 ? FLOWERS : TREE;
    if (nt == TREE && here && player_on_tile(mx, my)) nt = FLOWERS;
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
static const signed char area_east[NUM_AREAS] = { 1, -1, -1 }, area_west[NUM_AREAS] = { -1, 0, -1 };
static const signed char area_south[NUM_AREAS] = { 2, -1, -1 }, area_north[NUM_AREAS] = { -1, -1, 0 };
static const char *area_name[NUM_AREAS] = { "THE VALLEY", "THE WINDY STEPPE", "STONE HOLLOW" };

// Walk into area a, standing at (nx, ny). Fails (returns 0) if that spot is blocked.
static int enter_area(s32 a, s32 nx, s32 ny) {
    use_area(a);
    if (blocked_at(nx, ny)) { use_area(cur_area); return 0; }
    cur_area = a;
    music_play(SONG_VALLEY + a);   // each area has its own tune
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
    return 1;
}

// The valley's closed edges (north, west) point you to the regions still without a home:
// the east (Breeze's nook, in the steppe) and the south (Pebble's cairn, in the hollow).
static void region_hint(void) {
    s32 east = !home_done[KIND_BREEZE], south = !home_done[KIND_STONE];
    if ((!east && !south) || msg_timer || msgq_len) return;
    show_message("I WONDER WHAT LIES");
    queue_message(east && south ? "IN THE EAST AND SOUTH..." : east ? "IN THE EAST..." : "IN THE SOUTH...");
}

// Pressing against an open map edge leads to the next area.
static void check_exits(void) {
    if (HELD(KEY_RIGHT) && px >= WORLD_PX - 16 && area_east[cur_area] >= 0)
        enter_area(area_east[cur_area], 2, py);
    else if (HELD(KEY_LEFT) && px <= 0 && area_west[cur_area] >= 0)
        enter_area(area_west[cur_area], WORLD_PX - 18, py);
    else if (HELD(KEY_DOWN) && py >= WORLD_PX - 16 && area_south[cur_area] >= 0)
        enter_area(area_south[cur_area], px, 2);
    else if (HELD(KEY_UP) && py <= 0 && area_north[cur_area] >= 0)
        enter_area(area_north[cur_area], px, WORLD_PX - 18);
    else if (cur_area == 0 && ((HELD(KEY_UP) && py <= 0) || (HELD(KEY_LEFT) && px <= 0)))
        region_hint();
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

// Returns 1 if the power did something (and so costs a charge).
static int use_power(s32 kind, s32 mx, s32 my) {
    u8 t = tile_at(mx, my);
    int in_map = mx >= 0 && my >= 0 && mx < MAP_W && my < MAP_H;
    if (!in_map) return 0;
    if (kind == KIND_SPROUT) {
        if (t == WATER || t == ROCK || t == BRIDGE || t == THORN || t == MENHIR) return 0;
        start_bloom(mx, my);
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
        if (t == TREE) { set_tile(mx, my, STUMP_GRASS); wood++; show_message("+1 WOOD"); }
        else if (t == STUMP_GRASS) set_tile(mx, my, GRASS);
        else if (t == STUMP) set_tile(mx, my, DIRT);
        else if (t == DEAD_TREE) { set_tile(mx, my, DIRT); wood++; show_message("+1 WOOD"); }
        else return 0;
        sparkle_at(mx, my);
        draw_life();
        sfx_play(SFX_REMOVE);
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
        draw_active();
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

static void use_ability(void) {
    s32 fx = px + 8, fy = py + 12;
    if (pdir == DIR_LEFT)  fx -= 14;
    if (pdir == DIR_RIGHT) fx += 14;
    if (pdir == DIR_UP)    fy -= 14;
    if (pdir == DIR_DOWN)  fy += 14;
    if (interact(fx >> 4, fy >> 4)) return;
    if (!party_count) return;
    Spirit *s = &spirits[party[active]];
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
    if (!told_rest) { told_rest = 1; queue_message(REST_TIP); }
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
        spirits[KIND_BARK].state = SP_ASLEEP;   // a fifth spirit: asleep among the valley's stumps
        queue_message("SOMETHING STIRS AMONG THE");
        queue_message("OLD STUMPS IN THE VALLEY...");
    }
}

static int near_px(s32 x, s32 y, s32 r) {
    s32 dx = x - px, dy = y - py;
    return dx * dx + dy * dy < r * r;
}

// ---------- building (hold SELECT) ----------
// The last four items are quest rewards and appear once their quest is done (unlocked[]).
enum { B_PATH, B_BED, B_SAPLING, B_LANTERN, B_BENCH, B_UPGRADE, B_BLOSSOM, B_FOUNTAIN, B_STATUE, B_PINWHEEL,
       NUM_BUILDS };
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
            queue_message("LET ALL FOUR REST AT HOME...");
        }
        return;
    }
    sfx_play(SFX_FAIL);
    show_message("STAND BY A HOME TO UPGRADE");
}

static void build_mode(void) {
    if (HIT(KEY_R)) do build_item = (build_item + 1) % NUM_BUILDS; while (!build_ok(build_item));
    if (HIT(KEY_L)) do build_item = (build_item + NUM_BUILDS - 1) % NUM_BUILDS; while (!build_ok(build_item));
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

static void update_pumps(void) {
    s32 any_home = 0;
    for (s32 k = 0; k < NUM_HOMES; k++) any_home |= home_done[k];
    if (!any_home) return;
    // start a new pump now and then, in an area that has none
    u32 wait = pumps_stopped || pump_on[0] || pump_on[1] || pump_on[2] ? PUMP_EVERY : PUMP_FIRST;
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
    "BARK LOVES THE FRUIT!",
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
        if (sp->area != cur_area || (sp->state != SP_JOINED && sp->state != SP_HOME)) continue;
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
        show_message(spirit_skill[spirits[party[active]].kind]);
        draw_active();
    }

    // Spirits not in the party react when the player is close.
    for (s32 i = 0; i < NUM_SPIRITS; i++) {
        Spirit *s = &spirits[i];
        if (s->state == SP_JOINED || s->state == SP_HIDDEN || s->area != cur_area) continue;
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

// Milestones, checked twice a second: an area reaching 100% LIFE, and harmony.
static const char *area_full[NUM_AREAS] = { "THE VALLEY IS FULLY ALIVE!", "THE STEPPE IS FULLY ALIVE!",
                                            "STONE HOLLOW IS FULLY ALIVE!" };
static void check_milestones(void) {
    if (frame % 30) return;
    for (s32 a = 0; a < NUM_AREAS; a++)
        if (!told_full[a] && life_percent_of(a) >= 100) {
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

static void update_camera(void) {
    cam_x = px + 8 - SCREEN_W / 2;
    cam_y = py + 8 - SCREEN_H / 2;
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

static void draw_sprites(void) {
    // Player sheet: per direction (down, up, side) 4 frames: stand, walk1, walk2, walk3.
    // Walking plays 1, 2, 3, 2. Left is the side frames mirrored. The sprite is 16x32 with the
    // feet in the bottom 16 pixels, so it is drawn 16 pixels above the feet cell.
    static const u8 dir_block[4] = { 0, 1, 2, 2 };   // down, up, left, right
    static const u8 walk_seq[4] = { 1, 2, 3, 2 };
    u32 f = dir_block[pdir] * 4 + (pstep ? walk_seq[pstep & 3] : 0);
    obj_set(OAM_PLAYER, px - cam_x, py - cam_y - 16, TILE_PLAYER + f * 8, pdir == DIR_LEFT, PAL_MAIN, 1);

    for (s32 i = 0; i < NUM_SPIRITS; i++) {
        Spirit *s = &spirits[i];
        if (s->area != cur_area) { sprite_hide(OAM_SPIRIT + i); sprite_hide(OAM_ZZZ + i); continue; }
        s32 anim = (frame >> 4) & 1;
        s32 b = bob[((frame >> 2) + i * 5) & 15];
        s32 sx = s->x - cam_x, sy = s->y - cam_y - 4;
        if (s->state == SP_HIDDEN) {
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
            sprite_set(OAM_SPIRIT + i, sx, sy + b, spirit_tile[s->kind] + anim, 0, spirit_pal[s->kind]);
            sprite_hide(OAM_ZZZ + i);
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
#define SAVE_MAGIC 0x35534652u   // "RFS5" (v0.13: Bark, wood, harmony)

// Everything that makes up a game in progress, in save order.
static const struct { void *p; u32 n; } save_fields[] = {
    { &px, 4 }, { &py, 4 }, { &pdir, 4 }, { &cur_area, 4 },
    { maps, sizeof(maps) }, { restorable, sizeof(restorable) },
    { spirits, sizeof(spirits) }, { party, sizeof(party) }, { &party_count, 4 }, { &active, 4 },
    { home_done, sizeof(home_done) }, { pulse_r, sizeof(pulse_r) }, { pulse_timer, sizeof(pulse_timer) },
    { &stone, 4 }, { &fruit, 4 }, { &seeds, 4 },
    { chest_open, sizeof(chest_open) }, { wreck_read, sizeof(wreck_read) }, { picked, sizeof(picked) },
    { &bonus_charge, 4 }, { &pulse_div, 4 }, { home_level, sizeof(home_level) }, { &build_item, 4 },
    { fp, sizeof(fp) }, { quest, sizeof(quest) }, { unlocked, sizeof(unlocked) }, { &bonded, 4 }, { &thorn_quest, 4 },
    { pump_on, sizeof(pump_on) }, { pump_x, sizeof(pump_x) }, { pump_y, sizeof(pump_y) },
    { pump_drain, sizeof(pump_drain) }, { &pump_timer, 4 }, { &pumps_stopped, 4 },
    { &told_river, 4 }, { &told_spring, 4 }, { &told_rest, 4 }, { &told_ending, 4 }, { &rng_state, 4 },
    { &wood, 4 }, { &harmony, 4 }, { told_full, sizeof(told_full) }, { &told_grown, 4 },
};
#define NUM_SAVE_FIELDS (sizeof(save_fields) / sizeof(save_fields[0]))

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
    if (sram_get32(0) != SAVE_MAGIC) return 0;
    u32 at = 4, sum = 0x5A5A;
    for (u32 f = 0; f < NUM_SAVE_FIELDS; f++)
        for (u32 i = 0; i < save_fields[f].n; i++) sum += SRAM[at++];
    if (sram_get32(at) != sum) return 0;
    at = 4;
    for (u32 f = 0; f < NUM_SAVE_FIELDS; f++) {
        u8 *b = save_fields[f].p;
        for (u32 i = 0; i < save_fields[f].n; i++) b[i] = SRAM[at++];
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
    draw_life();
    draw_active();
    show_message("WELCOME BACK!");
    music_play(SONG_VALLEY + cur_area);
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
        r++;
        put_text_win(2, r++, num_line("LORE FOUND", count_set(wreck_read, NUM_WRECKS), NUM_WRECKS, 16));
        put_text_win(2, r++, num_line("PUMPS RUNNING", pump_on[0] + pump_on[1] + pump_on[2], -1, 16));
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

// ---------- title screen ----------
// Shows the valley (your own, if there is a save) behind a big ROOTFLOW banner. With a save:
// CONTINUE / NEW GAME; without one: PRESS START.
#define BANNER_BASE 64   // first banner tile in charblock 1 (after the font)
static void title_screen(int has_save) {
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
    if (!has_save || choice == 1) {
        reset_game();                     // a new game (a save stays until you save over it)
    } else {
        music_play(SONG_VALLEY + cur_area);
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
    }
    PAL_BG[16 + 1] = 0x7FFF;                         // text color: white
    PAL_BG[16 + 2] = (2) | (3 << 5) | (6 << 10);     // text background: dark slate
    // menu windows (bank 2): dark text on cream, gold border
    PAL_BG[32 + 1] = (7) | (7 << 5) | (8 << 10);
    PAL_BG[32 + 2] = (31) | (31 << 5) | (29 << 10);
    PAL_BG[32 + 3] = (29) | (24 << 5) | (13 << 10);
    PAL_BG[32 + 4] = (22) | (16 << 5) | (7 << 10);
    PAL_BG[32 + 5] = (29) | (7 << 5) | (8 << 10);     // heart red

    copy32(CHARBLOCK(0), terrain_tiles, sizeof(terrain_tiles) / 4);
    copy32(CHARBLOCK(1), font_tiles, sizeof(font_tiles) / 4);
    copy32(CHARBLOCK(1) + BANNER_BASE * 8, banner_tiles, sizeof(banner_tiles) / 4);
    PAL_BG[48 + 1] = (30) | (28 << 5) | (22 << 10);   // title banner: cream letters
    PAL_BG[48 + 3] = (3) | (4 << 5) | (4 << 10);      // and a dark shadow
    copy32(OBJ_TILES, sprite_tiles, sizeof(sprite_tiles) / 4);

    for (u32 i = 0; i < 128; i++) oam_shadow[i * 4] = 0x0200;
    oam_flush();

    REG_BG0CNT = 0 | (1 << 2) | (TEXT_SB << 8);             // text: priority 0
    REG_BG1CNT = 2 | (0 << 2) | (MAP_SB << 8) | (3 << 14);  // map: priority 2, 64x64 tiles

    sound_init();
    REG_WAITCNT = (REG_WAITCNT & ~3) | 3;   // SRAM: 8 wait states
    int has_save = load_game();
    if (!has_save) reset_game();
    update_camera();

    REG_DISPCNT = 0x0000 | (1 << 6) | (1 << 8) | (1 << 9) | (1 << 12);
    title_screen(has_save);

    for (;;) {
        vsync();
        music_tick();
        REG_BG1HOFS = cam_x;
        REG_BG1VOFS = cam_y;
        oam_flush();
        if ((frame & 31) == 0) animate_water();
        animate_homes();

        poll_keys();
        frame++;
        rng_state += keys_now;

        if (HELD(KEY_L) && HELD(KEY_R) && HIT(KEY_START)) { close_menu(); reset_game(); continue; }
        if (!paused && HIT(KEY_START)) { open_menu(); continue; }
        if (paused) { update_menu(); update_message(); continue; }

        update_player();
        update_spirits();
        handle_action();
        check_spring();
        update_homes();
        check_milestones();
        update_growth();
        update_pumps();
        update_blooms();
        update_camera();
        draw_sprites();

        update_message();
    }
}
