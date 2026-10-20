// Rootflow spirit toy (GBA).
// Walk the dead valley, find the sprout spirit, and bloom life into the land.
// D-pad: move   B: run   A: greet / use spirit   SELECT: swap spirit   START: reset
// Spirits: Sprout blooms life, Ripple bridges water, Pebble breaks rocks, Breeze clears thorns.
// Powers cost energy. Spirits can be housed at home sites; a home spreads life around it.

#include "data.h"

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

#define SCREEN_W 240
#define SCREEN_H 160
#define WORLD_PX (MAP_W * 16)

#define TEXT_SB   26
#define MAP_SB    28

enum { DEAD, DEAD_CRACK, DIRT, GRASS, GRASS_TUFT, FLOWERS, TREE, DEAD_TREE, ROCK, WATER, WATER_B, BRIDGE,
       THORN, HOMESITE };
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
static void sound_init(void) {
    REG_SNDCNT_X = 0x0080;   // master on
    REG_SNDCNT_L = 0x1177;   // channel 1 on both sides, full volume
    REG_SNDCNT_H = 0x0002;   // PSG at 100%
    REG_SND1SWEEP = 0x0008;  // sweep off
}
static void chime(u32 hz, u32 vol) {
    REG_SND1CNT = (vol << 12) | (2 << 8) | (2 << 6);      // decaying envelope, 50% duty
    REG_SND1FREQ = (u16)((2048 - 131072 / hz) | 0x8000);  // restart
}
static const u16 scale[] = { 523, 587, 659, 784, 880, 1047, 1175, 1319 };

// ---------- world ----------
static u8 world[MAP_W * MAP_H];
static u32 restorable_count;

static u8 tile_at(s32 mx, s32 my) {
    if (mx < 0 || my < 0 || mx >= MAP_W || my >= MAP_H) return ROCK;
    return world[my * MAP_W + mx];
}

static void draw_meta(s32 mx, s32 my) {
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
    return t == WATER || t == TREE || t == DEAD_TREE || t == ROCK || t == THORN;
}
static int is_alive(u8 t) {
    return t == GRASS || t == GRASS_TUFT || t == FLOWERS || t == TREE;
}

static void load_world(void) {
    restorable_count = 0;
    for (s32 i = 0; i < MAP_W * MAP_H; i++) {
        world[i] = start_map[i];
        u8 t = world[i];
        if (t != WATER && t != ROCK && t != THORN && t != HOMESITE) restorable_count++;
    }
    for (s32 y = 0; y < MAP_H; y++)
        for (s32 x = 0; x < MAP_W; x++)
            draw_meta(x, y);
}

static u32 life_percent(void) {
    u32 alive = 0;
    for (s32 i = 0; i < MAP_W * MAP_H; i++)
        if (is_alive(world[i])) alive++;
    return alive * 100 / restorable_count;
}

// ---------- text (BG0) ----------
static u16 glyph_entry(char c) {
    for (u32 i = 0; FONT_CHARS[i]; i++)
        if (FONT_CHARS[i] == c) return (1 + i) | (1 << 12);   // palette bank 1
    return 1 | (1 << 12);
}
static void put_text(s32 col, s32 row, const char *s) {
    for (; *s && col < 30; s++, col++) SCREENBLOCK(TEXT_SB)[row * 32 + col] = glyph_entry(*s);
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
#define MSGQ 4
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
static void sprite_set(s32 id, s32 x, s32 y, u32 tile, u32 hflip, u32 pal) {
    u16 *o = &oam_shadow[id * 4];
    if (x < -16 || x > SCREEN_W || y < -16 || y > SCREEN_H) { o[0] = 0x0200; return; }
    o[0] = (y & 0xFF);                                        // square shape
    o[1] = (x & 0x1FF) | (1 << 14) | (hflip ? 1 << 12 : 0);   // 16x16
    o[2] = (tile * 4) | (1 << 10) | (pal << 12);              // priority 1
}
static void sprite_hide(s32 id) { oam_shadow[id * 4] = 0x0200; }
static void oam_flush(void) {
    for (u32 i = 0; i < 128 * 4; i++) OAM[i] = oam_shadow[i];
}

// Sprite sheet tile numbers (16x16 cells)
#define SPR_SPROUT 6
#define SPR_SPARKLE 8
#define SPR_RIPPLE 9
#define SPR_ZZZ 11
#define SPR_STONE 12
#define SPR_BREEZE 14
#define PAL_MAIN 0
#define PAL_RIPPLE 1
#define PAL_SLEEP 2
#define PAL_STONE 3
#define PAL_STONE_SLEEP 4
#define PAL_BREEZE 5
#define PAL_BREEZE_SLEEP 6

// OAM slots
#define OAM_PLAYER  0
#define OAM_SPIRIT  1   // 1..NUM_SPIRITS
#define OAM_ZZZ     5   // 5..5+NUM_SPIRITS-1
#define OAM_SPARKLE 9

// ---------- game state ----------
static s32 px, py, pdir, pframe;        // player top-left (pixels), facing, anim frame
static s32 cam_x, cam_y;
static u32 frame;

#define HIST 128   // power of two; the last follower trails 18*NUM_SPIRITS entries
static s32 hist_x[HIST], hist_y[HIST], hist_head;
static const signed char bob[16] = { 0, 1, 1, 2, 2, 2, 1, 1, 0, -1, -1, -2, -2, -2, -1, -1 };

// ---------- spirits ----------
enum { SP_ASLEEP, SP_AWAKE, SP_JOINED, SP_HOME };
enum { KIND_SPROUT, KIND_RIPPLE, KIND_STONE, KIND_BREEZE };
// housed: the spirit has a home. While it travels with you its power is weaker (fewer charges).
typedef struct { s32 x, y, state, kind, energy, housed; } Spirit;
#define NUM_SPIRITS 4
#define ENERGY_MAX 3
#define ENERGY_MAX_HOUSED 2
#define REGEN_FRAMES 300          // one charge back per 5 s while standing on living ground
static Spirit spirits[NUM_SPIRITS];   // index == kind
static s32 party[NUM_SPIRITS], party_count, active;   // party holds spirit indexes in join order
static const char *spirit_name[] = { "SPROUT", "RIPPLE", "PEBBLE", "BREEZE" };
static const char *spirit_skill[] = { "SPROUT: BLOOM", "RIPPLE: BRIDGE", "PEBBLE: BREAK ROCK",
                                      "BREEZE: CLEAR THORNS" };
static const char *spirit_joined[] = { "SPROUT JOINED! A: BLOOM", "RIPPLE JOINED! SELECT: SWAP",
                                       "PEBBLE JOINED! SELECT: SWAP", "BREEZE JOINED! SELECT: SWAP" };
static const char *spirit_tired[] = { "SPROUT IS TIRED", "RIPPLE IS TIRED", "PEBBLE IS TIRED",
                                      "BREEZE IS TIRED" };
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
static u8 home_done[NUM_SPIRITS];
static u32 told_ending;
// per kind: sprite cell, awake palette, asleep palette
static const u8 spirit_tile[] = { SPR_SPROUT, SPR_RIPPLE, SPR_STONE, SPR_BREEZE };
static const u8 spirit_pal[] = { PAL_MAIN, PAL_RIPPLE, PAL_STONE, PAL_BREEZE };
static const u8 spirit_sleep_pal[] = { PAL_MAIN, PAL_SLEEP, PAL_STONE_SLEEP, PAL_BREEZE_SLEEP };

static s32 energy_max(const Spirit *s) { return s->housed ? ENERGY_MAX_HOUSED : ENERGY_MAX; }
static int in_party(s32 kind) {
    for (s32 p = 0; p < party_count; p++) if (party[p] == kind) return 1;
    return 0;
}

// ---------- blooms ----------
#define BR 4                      // max bloom radius in tiles
#define BGRID (BR * 2 + 1)
typedef struct { s32 cx, cy, r, maxr, timer, active; u8 dist[BGRID * BGRID]; } Bloom;
#define MAX_BLOOMS 6
static Bloom blooms[MAX_BLOOMS];
static s32 sparkle_x, sparkle_y, sparkle_life;
static u32 told_river, told_spring;
static u32 regen_timer, home_timer;
static s32 just_called = -1;   // spirit just called back from home (see handle_action)

static int blocked_at(s32 x, s32 y) {
    // feet box: x+3..x+12, y+10..y+15
    s32 xs[2] = { x + 3, x + 12 }, ys[2] = { y + 10, y + 15 };
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++)
            if (is_solid(tile_at(xs[i] >> 4, ys[j] >> 4))) return 1;
    return x < 0 || y < 0 || x > WORLD_PX - 16 || y > WORLD_PX - 16;
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
    s->energy = ENERGY_MAX; s->housed = 0;
}

static void reset_game(void) {
    load_world();
    px = PLAYER_START_X * 16; py = PLAYER_START_Y * 16; pdir = DIR_DOWN;
    spirit_init(KIND_SPROUT, SPIRIT_SPOT_X, SPIRIT_SPOT_Y, SP_AWAKE);
    spirit_init(KIND_RIPPLE, RIPPLE_SPOT_X, RIPPLE_SPOT_Y, SP_ASLEEP);
    spirit_init(KIND_STONE, STONE_SPOT_X, STONE_SPOT_Y, SP_ASLEEP);
    spirit_init(KIND_BREEZE, BREEZE_SPOT_X, BREEZE_SPOT_Y, SP_ASLEEP);
    for (s32 i = 0; i < NUM_SPIRITS; i++) home_done[i] = 0;
    told_ending = 0;
    msgq_len = 0;
    party_count = 0; active = 0;
    for (s32 i = 0; i < HIST; i++) { hist_x[i] = px; hist_y[i] = py; }
    for (s32 i = 0; i < MAX_BLOOMS; i++) blooms[i].active = 0;
    sparkle_life = 0;
    told_river = 0;
    told_spring = 0;
    regen_timer = home_timer = 0;
    just_called = -1;
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

// Distance map for a bloom: life spreads around obstacles but never across water, rock, or thorns.
// Costs: 10 per straight step, 14 per diagonal step, so rings come out round.
static void bloom_distances(Bloom *b) {
    for (s32 i = 0; i < BGRID * BGRID; i++) b->dist[i] = 255;
    b->dist[BR * BGRID + BR] = 0;
    for (s32 pass = 0; pass < BGRID; pass++) {
        for (s32 gy = 0; gy < BGRID; gy++)
            for (s32 gx = 0; gx < BGRID; gx++) {
                s32 mx = b->cx + gx - BR, my = b->cy + gy - BR;
                u8 t = tile_at(mx, my);
                if (t == WATER || t == ROCK || t == THORN) continue;
                s32 best = b->dist[gy * BGRID + gx];
                for (s32 dy = -1; dy <= 1; dy++)
                    for (s32 dx = -1; dx <= 1; dx++) {
                        s32 nx = gx + dx, ny = gy + dy;
                        if ((!dx && !dy) || nx < 0 || ny < 0 || nx >= BGRID || ny >= BGRID) continue;
                        s32 d = b->dist[ny * BGRID + nx];
                        if (d == 255) continue;
                        d += (dx && dy) ? 14 : 10;
                        if (d < best) best = d;
                    }
                b->dist[gy * BGRID + gx] = best > 255 ? 255 : best;
            }
    }
}

static void start_bloom(s32 mx, s32 my) {
    for (s32 i = 0; i < MAX_BLOOMS; i++) {
        if (blooms[i].active) continue;
        Bloom *b = &blooms[i];
        b->cx = mx; b->cy = my; b->r = -1; b->timer = 0; b->active = 1;
        int wet = water_near(mx, my, 3);
        b->maxr = wet ? 4 : 2;
        bloom_distances(b);
        sparkle_x = mx * 16; sparkle_y = my * 16; sparkle_life = 40;
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
    if (t == DEAD_TREE) { if (!player_on_tile(mx, my)) set_tile(mx, my, TREE); return; }
    if (t != DEAD && t != DEAD_CRACK && t != DIRT) return;
    if (edge && t != DIRT) { set_tile(mx, my, DIRT); return; }
    u32 r = rng() % 100;
    u8 nt = r < 55 ? GRASS : r < 80 ? GRASS_TUFT : r < 95 ? FLOWERS : TREE;
    if (nt == TREE && player_on_tile(mx, my)) nt = FLOWERS;
    set_tile(mx, my, nt);
}

// A sleeping spirit wakes once living ground reaches the tiles around it.
static void check_wake(void) {
    for (s32 i = 0; i < NUM_SPIRITS; i++) {
        Spirit *s = &spirits[i];
        if (s->state != SP_ASLEEP) continue;
        s32 mx = (s->x + 8) >> 4, my = (s->y + 8) >> 4;
        for (s32 y = my - 1; y <= my + 1; y++)
            for (s32 x = mx - 1; x <= mx + 1; x++)
                if (is_alive(tile_at(x, y)) && s->state == SP_ASLEEP) {
                    s->state = SP_AWAKE;
                    show_message("A SPIRIT WOKE UP!");
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
        for (s32 gy = 0; gy < BGRID; gy++)
            for (s32 gx = 0; gx < BGRID; gx++) {
                s32 d = b->dist[gy * BGRID + gx];
                if (d > lo && d <= hi) grow_tile(b->cx + gx - BR, b->cy + gy - BR, r == b->maxr);
            }
        chime(scale[r < 8 ? r : 7], r == 0 ? 12 : 9);
        changed = 1;
        if (b->r >= b->maxr) b->active = 0;
    }
    if (changed) { draw_life(); check_wake(); }
}

static void update_player(void) {
    s32 speed = HELD(KEY_B) ? 2 : 1;
    s32 dx = 0, dy = 0;
    if (HELD(KEY_LEFT))  { dx = -speed; pdir = DIR_LEFT; }
    if (HELD(KEY_RIGHT)) { dx =  speed; pdir = DIR_RIGHT; }
    if (HELD(KEY_UP))    { dy = -speed; pdir = DIR_UP; }
    if (HELD(KEY_DOWN))  { dy =  speed; pdir = DIR_DOWN; }

    int moved = 0;
    if (dx && !blocked_at(px + dx, py)) { px += dx; moved = 1; }
    if (dy && !blocked_at(px, py + dy)) { py += dy; moved = 1; }

    if (moved) {
        if ((frame & 7) == 0) pframe ^= 1;
        hist_head = (hist_head + 1) & (HIST - 1);
        hist_x[hist_head] = px; hist_y[hist_head] = py;
    } else {
        pframe = 0;
    }
}

static void update_spirits(void) {
    // Party members follow the player's trail, one behind the other.
    for (s32 p = 0; p < party_count; p++) {
        Spirit *s = &spirits[party[p]];
        s32 idx = (hist_head - 18 * (p + 1)) & (HIST - 1);
        s->x += (hist_x[idx] - s->x) / 4;
        s->y += (hist_y[idx] - s->y) / 4;
    }
}

static void sparkle_at(s32 mx, s32 my) { sparkle_x = mx * 16; sparkle_y = my * 16; sparkle_life = 30; }

// Returns 1 if the power did something (and so costs a charge).
static int use_power(s32 kind, s32 mx, s32 my) {
    u8 t = tile_at(mx, my);
    int in_map = mx >= 0 && my >= 0 && mx < MAP_W && my < MAP_H;
    if (!in_map) return 0;
    if (kind == KIND_SPROUT) {
        if (t == WATER || t == ROCK || t == BRIDGE || t == THORN) return 0;
        start_bloom(mx, my);
    } else if (kind == KIND_RIPPLE) {
        if (t != WATER) return 0;
        set_tile(mx, my, BRIDGE);
        chime(392, 12);
        sparkle_at(mx, my);
    } else if (kind == KIND_STONE) {
        if (t != ROCK) return 0;
        set_tile(mx, my, DIRT);     // broken rock leaves bare ground that can bloom
        restorable_count++;
        draw_life();
        chime(196, 14);
        sparkle_at(mx, my);
    } else {
        // A gust clears up to three thorn tiles in a line.
        if (t != THORN) return 0;
        s32 sx = pdir == DIR_LEFT ? -1 : pdir == DIR_RIGHT ? 1 : 0;
        s32 sy = pdir == DIR_UP ? -1 : pdir == DIR_DOWN ? 1 : 0;
        for (s32 k = 0; k < 3 && tile_at(mx, my) == THORN; k++, mx += sx, my += sy) {
            set_tile(mx, my, DEAD);
            restorable_count++;
            sparkle_at(mx, my);
        }
        draw_life();
        chime(880, 10);
    }
    return 1;
}

static void use_ability(void) {
    s32 fx = px + 8, fy = py + 12;
    if (pdir == DIR_LEFT)  fx -= 14;
    if (pdir == DIR_RIGHT) fx += 14;
    if (pdir == DIR_UP)    fy -= 14;
    if (pdir == DIR_DOWN)  fy += 14;
    Spirit *s = &spirits[party[active]];
    if (s->energy == 0) { chime(220, 6); show_message(spirit_tired[s->kind]); return; }
    if (!use_power(s->kind, fx >> 4, fy >> 4)) { chime(262, 6); return; }
    s->energy--;
    draw_active();
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
    s->x = home_site_x[k] * 16; s->y = home_site_y[k] * 16;
    s->energy = ENERGY_MAX_HOUSED;
}

static void try_build_home(s32 k) {
    s32 hx = home_site_x[k], hy = home_site_y[k];
    const u8 *h = home_helper[k];
    if (life_around(hx, hy) < 4) { chime(262, 6); show_message("BLOOM THE LAND HERE FIRST"); return; }
    if (!in_party(h[0]) || !in_party(h[1]) || spirits[h[0]].energy == 0 || spirits[h[1]].energy == 0) {
        chime(262, 6); show_message(home_needs[k]); return;
    }
    spirits[h[0]].energy--;
    spirits[h[1]].energy--;
    home_done[k] = 1;
    spirits[k].housed = 1;
    send_home(k);
    chime(1047, 14);
    show_message(home_built[k]);
    sparkle_at(hx, hy);
    s32 all = 1;
    for (s32 i = 0; i < NUM_SPIRITS; i++) all &= home_done[i];
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

static void handle_action(void) {
    if (party_count > 1 && HIT(KEY_SELECT)) {
        active = (active + 1) % party_count;
        chime(active ? 659 : 523, 8);
        show_message(spirit_skill[spirits[party[active]].kind]);
        draw_active();
    }

    // Spirits not in the party react when the player is close.
    for (s32 i = 0; i < NUM_SPIRITS; i++) {
        Spirit *s = &spirits[i];
        if (s->state == SP_JOINED || !near_px(s->x, s->y, 28)) continue;
        if (s->state == SP_ASLEEP) {
            // Only a hint; A still uses your spirit so you can bloom beside it.
            if (msg_timer == 0) show_message("IT SLEEPS. BLOOM NEAR IT");
            continue;
        }
        if (s->state == SP_HOME) {
            prompt(call_back[i]);
            if (HIT(KEY_A)) { party_add(i); chime(784, 12); show_message(called_back[i]); just_called = i; }
            return;
        }
        prompt("A: GREET THE SPIRIT");
        if (HIT(KEY_A)) {
            party_add(i);
            chime(784, 12);
            show_message(spirit_joined[s->kind]);
        }
        return;
    }

    // Home sites: build one, or send a housed spirit back to it. A site only takes the
    // A button when the action can happen; otherwise A still uses your spirit (so you can
    // bloom the land around a site).
    for (s32 k = 0; k < NUM_SPIRITS; k++) {
        if (!near_px(home_site_x[k] * 16, home_site_y[k] * 16, 24)) {
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

    if (party_count && HIT(KEY_A)) use_ability();
}

// Energy: standing on living ground slowly recharges the party; standing at a built home
// refills it. Each home also spreads life around itself, one tile at a time.
#define HOME_RADIUS 5
static void update_homes(void) {
    int changed = 0;
    s32 fx = (px + 8) >> 4, fy = (py + 12) >> 4;
    if (is_alive(tile_at(fx, fy)) && ++regen_timer >= REGEN_FRAMES) {
        regen_timer = 0;
        for (s32 p = 0; p < party_count; p++) {
            Spirit *s = &spirits[party[p]];
            if (s->energy < energy_max(s)) { s->energy++; changed = 1; }
        }
    }
    for (s32 k = 0; k < NUM_SPIRITS; k++) {
        if (!home_done[k]) continue;
        s32 dx = fx - home_site_x[k], dy = fy - home_site_y[k];
        if (dx * dx + dy * dy > 4) continue;
        for (s32 p = 0; p < party_count; p++) {
            Spirit *s = &spirits[party[p]];
            if (s->energy < energy_max(s)) { s->energy = energy_max(s); changed = 1; }
        }
    }
    if (changed) draw_active();

    if (++home_timer < 90) return;
    home_timer = 0;
    int grew = 0;
    for (s32 k = 0; k < NUM_SPIRITS; k++) {
        if (!home_done[k]) continue;
        for (s32 tries = 0; tries < 8; tries++) {
            s32 x = home_site_x[k] + (s32)(rng() % (2 * HOME_RADIUS + 1)) - HOME_RADIUS;
            s32 y = home_site_y[k] + (s32)(rng() % (2 * HOME_RADIUS + 1)) - HOME_RADIUS;
            s32 ddx = x - home_site_x[k], ddy = y - home_site_y[k];
            if (ddx * ddx + ddy * ddy > HOME_RADIUS * HOME_RADIUS) continue;
            u8 t = tile_at(x, y);
            if (t != DEAD && t != DEAD_CRACK && t != DIRT && t != DEAD_TREE) continue;
            if (!is_alive(tile_at(x + 1, y)) && !is_alive(tile_at(x - 1, y)) &&
                !is_alive(tile_at(x, y + 1)) && !is_alive(tile_at(x, y - 1))) continue;
            grow_tile(x, y, 0);
            grew = 1;
            break;
        }
    }
    if (grew) { draw_life(); check_wake(); }
}

// The first step into the hidden spring gets a message.
static void check_spring(void) {
    if (told_spring) return;
    s32 mx = (px + 8) >> 4, my = (py + 12) >> 4;
    if (mx >= POCKET_X0 && mx <= POCKET_X1 && my >= POCKET_Y0 && my <= POCKET_Y1) {
        show_message("A HIDDEN SPRING!");
        chime(1047, 12);
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
}

static void draw_sprites(void) {
    static const u8 base_tile[4] = { 0, 2, 4, 4 };   // down, up, left, right
    sprite_set(OAM_PLAYER, px - cam_x, py - cam_y, base_tile[pdir] + pframe, pdir == DIR_LEFT, PAL_MAIN);

    for (s32 i = 0; i < NUM_SPIRITS; i++) {
        Spirit *s = &spirits[i];
        s32 anim = (frame >> 4) & 1;
        s32 b = bob[((frame >> 2) + i * 5) & 15];
        s32 sx = s->x - cam_x, sy = s->y - cam_y - 4;
        if (s->state == SP_ASLEEP) {
            // gray, still, with a sleep bubble that drifts up
            sprite_set(OAM_SPIRIT + i, sx, sy + 2, spirit_tile[s->kind], 0, spirit_sleep_pal[s->kind]);
            sprite_set(OAM_ZZZ + i, sx + 8, sy - 10 - ((frame >> 3) & 3), SPR_ZZZ, 0, PAL_RIPPLE);
        } else {
            sprite_set(OAM_SPIRIT + i, sx, sy + b, spirit_tile[s->kind] + anim, 0, spirit_pal[s->kind]);
            sprite_hide(OAM_ZZZ + i);
        }
    }

    if (sparkle_life > 0) {
        sparkle_life--;
        sprite_set(OAM_SPARKLE, sparkle_x - cam_x, sparkle_y - cam_y - (40 - sparkle_life) / 4,
                   SPR_SPARKLE, (frame >> 3) & 1, PAL_MAIN);
    } else {
        sprite_hide(OAM_SPARKLE);
    }
}

int main(void) {
    REG_DISPCNT = 0x0080;   // forced blank while loading

    for (u32 i = 0; i < 16; i++) {
        PAL_BG[i] = bg_pal[i];
        PAL_OBJ[i] = obj_pal[i];
        PAL_OBJ[16 + i] = obj_pal2[i];
        PAL_OBJ[32 + i] = obj_pal_sleep[i];
        PAL_OBJ[48 + i] = obj_pal3[i];
        PAL_OBJ[64 + i] = obj_pal3_sleep[i];
        PAL_OBJ[80 + i] = obj_pal4[i];
        PAL_OBJ[96 + i] = obj_pal4_sleep[i];
    }
    PAL_BG[16 + 1] = 0x7FFF;                         // text color: white
    PAL_BG[16 + 2] = (2) | (3 << 5) | (6 << 10);     // text background: dark slate

    copy32(CHARBLOCK(0), terrain_tiles, sizeof(terrain_tiles) / 4);
    copy32(CHARBLOCK(1), font_tiles, sizeof(font_tiles) / 4);
    copy32(OBJ_TILES, sprite_tiles, sizeof(sprite_tiles) / 4);

    for (u32 i = 0; i < 128; i++) oam_shadow[i * 4] = 0x0200;
    oam_flush();

    REG_BG0CNT = 0 | (1 << 2) | (TEXT_SB << 8);             // text: priority 0
    REG_BG1CNT = 2 | (0 << 2) | (MAP_SB << 8) | (3 << 14);  // map: priority 2, 64x64 tiles

    sound_init();
    reset_game();
    update_camera();

    REG_DISPCNT = 0x0000 | (1 << 6) | (1 << 8) | (1 << 9) | (1 << 12);

    for (;;) {
        vsync();
        REG_BG1HOFS = cam_x;
        REG_BG1VOFS = cam_y;
        oam_flush();
        if ((frame & 31) == 0) animate_water();

        poll_keys();
        frame++;
        rng_state += keys_now;

        if (HIT(KEY_START)) { reset_game(); continue; }

        update_player();
        update_spirits();
        handle_action();
        check_spring();
        update_homes();
        update_blooms();
        update_camera();
        draw_sprites();

        update_message();
    }
}
