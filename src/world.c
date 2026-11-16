/* World: procedural island city of Maple Bay, collision queries, static rendering. */
#include "game.h"

Box g_boxes[MAX_BOXES];
int g_nboxes;
Ramp g_ramps[MAX_RAMPS];
int g_nramps;
Crate g_crates[MAX_CRATES];
int g_ncrates;
Coin g_coins[MAX_COINS];
int g_ncoins;
Deco g_decos[MAX_DECOS];
int g_ndecos;
int g_blockType[MAXBLK][MAXBLK];
int g_nblk = 12;
float g_cityHalf = 420;
int g_curMap;
float g_skyTop[3] = {0.3f, 0.55f, 0.92f}, g_skyHz[3] = {0.78f, 0.88f, 0.98f};
static int theme; /* 0 city, 1 desert, 2 snow */
V3 g_parkSpots[MAX_PARKS];
float g_parkYaw[MAX_PARKS];
int g_nparks;
V3 g_gizmoPos[NUM_GIZMOS];
WorldSpots g_spots;
int g_collSkipInvis;

typedef struct { float x, z, r; } Pond;
typedef struct { float x0, z0, x1, z1; float col[3]; } Pad;
static Pond ponds[16];
static int nponds;
static Pad pads[160];
static int npads;
static V3 crateHome[MAX_CRATES];
static GLuint dlGround, dlStatic, dlMap;
static const char *mapNames[NUM_MAPS] = {"MAPLE BAY", "SUNSET SANDS", "FROSTPEAK"};
const char *map_name(int m) { return mapNames[m < 0 || m >= NUM_MAPS ? 0 : m]; }

static unsigned rs = 20240611u;
static float rnd(void) { rs = rs * 1664525u + 1013904223u; return (float)(rs >> 8) / 16777216.0f; }
static float rr(float a, float b) { return a + (b - a) * rnd(); }
static int ri(int n) { int v = (int)(rnd() * n); return v >= n ? n - 1 : v; }

static const float PAL_HOUSE[6][3] = {{0.95f, 0.85f, 0.6f}, {0.7f, 0.85f, 0.95f}, {0.95f, 0.72f, 0.7f}, {0.75f, 0.92f, 0.72f}, {0.96f, 0.95f, 0.86f}, {0.85f, 0.76f, 0.95f}};
static const float PAL_ROOF[4][3] = {{0.65f, 0.25f, 0.2f}, {0.35f, 0.32f, 0.33f}, {0.3f, 0.42f, 0.58f}, {0.5f, 0.32f, 0.2f}};
static const float PAL_COM[6][3] = {{0.72f, 0.4f, 0.32f}, {0.82f, 0.74f, 0.56f}, {0.62f, 0.64f, 0.67f}, {0.42f, 0.62f, 0.64f}, {0.86f, 0.83f, 0.76f}, {0.58f, 0.48f, 0.62f}};
static const float PAL_DOWN[4][3] = {{0.55f, 0.66f, 0.78f}, {0.72f, 0.74f, 0.77f}, {0.46f, 0.52f, 0.62f}, {0.82f, 0.79f, 0.7f}};

float road_c(int k) { return -CITY_HALF + CELL * k; }
void block_rect(int i, int j, float *x0, float *z0, float *x1, float *z1) {
    *x0 = road_c(i) + ROAD_HALF;
    *x1 = road_c(i + 1) - ROAD_HALF;
    *z0 = road_c(j) + ROAD_HALF;
    *z1 = road_c(j + 1) - ROAD_HALF;
}
/* side 0:+Z edge 1:+X edge 2:-Z edge 3:-X edge; position on the sidewalk, facing the road */
void block_edge_spot(int i, int j, int side, float off, V3 *p, float *yaw) {
    float x0, z0, x1, z1;
    block_rect(i, j, &x0, &z0, &x1, &z1);
    float cx = (x0 + x1) * 0.5f, cz = (z0 + z1) * 0.5f;
    switch (side) {
    case 0: *p = v3(cx + off, 0, z1 - 1.5f); *yaw = 0; break;
    case 1: *p = v3(x1 - 1.5f, 0, cz + off); *yaw = PI / 2; break;
    case 2: *p = v3(cx + off, 0, z0 + 1.5f); *yaw = PI; break;
    default: *p = v3(x0 + 1.5f, 0, cz + off); *yaw = -PI / 2; break;
    }
}

/* ----------------------------------------------------------- builders */
static Box *add_box(float x0, float y0, float z0, float x1, float y1, float z1, const float *col, int kind) {
    static Box dummy;
    if (g_nboxes >= MAX_BOXES) return &dummy;
    Box *b = &g_boxes[g_nboxes++];
    b->mn = v3(fminf(x0, x1), y0, fminf(z0, z1));
    b->mx = v3(fmaxf(x0, x1), y1, fmaxf(z0, z1));
    b->col[0] = col[0]; b->col[1] = col[1]; b->col[2] = col[2];
    b->kind = kind;
    b->roof = 0;
    return b;
}
static Box *add_boxc(float x0, float y0, float z0, float x1, float y1, float z1, float r, float g, float bl, int kind) {
    float c[3] = {r, g, bl};
    return add_box(x0, y0, z0, x1, y1, z1, c, kind);
}
static void add_ramp(float x0, float z0, float x1, float z1, int dir, float h) {
    if (g_nramps >= MAX_RAMPS) return;
    Ramp *r = &g_ramps[g_nramps++];
    r->mn = v3(fminf(x0, x1), 0, fminf(z0, z1));
    r->mx = v3(fmaxf(x0, x1), h, fmaxf(z0, z1));
    r->dir = dir;
    r->h = h;
    r->col[0] = 0.95f; r->col[1] = 0.72f; r->col[2] = 0.18f;
}
static Deco *add_deco(int kind, float x, float y, float z, float s) {
    static Deco dummy;
    if (g_ndecos >= MAX_DECOS) return &dummy;
    Deco *d = &g_decos[g_ndecos++];
    memset(d, 0, sizeof *d);
    d->kind = kind;
    d->p = v3(x, y, z);
    d->s = s;
    return d;
}
static void add_tree(float x, float z) {
    int pine = rnd() < 0.35f;
    float s = rr(0.85f, 1.3f);
    int kind = pine ? DECO_PINE : DECO_TREE;
    if (theme == 1) kind = rnd() < 0.3f ? DECO_CACTUS : DECO_PALM;
    if (theme == 2) kind = DECO_SNOWPINE;
    Deco *d = add_deco(kind, x, 0, z, s);
    d->yaw = rr(-PI, PI);
    if (kind == DECO_CACTUS) {
        d->col[0] = 0.3f; d->col[1] = 0.55f; d->col[2] = 0.25f;
        add_boxc(x - 0.35f, 0, z - 0.35f, x + 0.35f, 2.6f * s, z + 0.35f, 0.3f, 0.55f, 0.25f, K_TRUNK);
        return;
    }
    d->col[0] = rr(0.18f, 0.32f); d->col[1] = rr(0.5f, 0.68f); d->col[2] = rr(0.15f, 0.25f);
    add_boxc(x - 0.3f, 0, z - 0.3f, x + 0.3f, 2.4f * s, z + 0.3f, 0.45f, 0.3f, 0.18f, K_TRUNK);
}
static void add_sign(float x, float y, float z, float yaw, const char *text, float px, float r, float g, float b) {
    Deco *d = add_deco(DECO_SIGN, x, y, z, px);
    d->yaw = yaw;
    d->text = text;
    d->col[0] = r; d->col[1] = g; d->col[2] = b;
}
static void add_coin(float x, float y, float z) {
    if (g_ncoins >= MAX_COINS - 250) return;
    Coin *c = &g_coins[g_ncoins++];
    memset(c, 0, sizeof *c);
    c->pos = v3(x, y, z);
    c->alive = 1;
    c->value = 1;
}
static void coin_line(V3 a, V3 b, int n) {
    for (int i = 0; i < n; i++) {
        float t = n == 1 ? 0.5f : (float)i / (n - 1);
        add_coin(lerpf(a.x, b.x, t), lerpf(a.y, b.y, t), lerpf(a.z, b.z, t));
    }
}
static void coin_ring(float x, float y, float z, float r, int n) {
    for (int i = 0; i < n; i++) { float a = 2 * PI * i / n; add_coin(x + cosf(a) * r, y, z + sinf(a) * r); }
}
static void add_crate(float x, float y, float z) {
    if (g_ncrates >= MISSION_CRATE_BASE) return;
    Crate *c = &g_crates[g_ncrates];
    crateHome[g_ncrates] = v3(x, y, z);
    g_ncrates++;
    c->pos = v3(x, y, z);
    c->alive = 1;
    c->canRespawn = 1;
    c->mission = 0;
    c->vy = 0;
}
int add_crate_at(int slot, V3 p, int mission) {
    if (slot < 0 || slot >= MAX_CRATES) return -1;
    Crate *c = &g_crates[slot];
    c->pos = p;
    c->alive = 1;
    c->mission = mission;
    c->canRespawn = 0;
    c->vy = 0;
    crateHome[slot] = p;
    return slot;
}
static void add_pad(float x0, float z0, float x1, float z1, float r, float g, float b) {
    if (npads >= 160) return;
    Pad *p = &pads[npads++];
    p->x0 = x0; p->z0 = z0; p->x1 = x1; p->z1 = z1;
    p->col[0] = r; p->col[1] = g; p->col[2] = b;
}

/* ------------------------------------------------------- block types */
static void gen_res(float ax0, float az0) {
    for (int lx = 0; lx < 2; lx++)
        for (int lz = 0; lz < 2; lz++) {
            float lx0 = ax0 + lx * 24, lz0 = az0 + lz * 24;
            float w = rr(9, 13), d = rr(9, 13), h = rr(4.5f, 6.5f);
            float cx = lx0 + 12 + rr(-1.5f, 1.5f), cz = lz0 + 12 + rr(-1.5f, 1.5f);
            Box *b = add_box(cx - w / 2, 0, cz - d / 2, cx + w / 2, h, cz + d / 2, PAL_HOUSE[ri(6)], K_BUILDING);
            b->roof = 1 + ri(4);
            if (rnd() < 0.75f) {
                float tx = lx0 + (rnd() < 0.5f ? 1.8f : 22.2f), tz = lz0 + (rnd() < 0.5f ? 1.8f : 22.2f);
                add_tree(tx, tz);
            }
        }
}
static void gen_com(float ax0, float az0) {
    for (int lx = 0; lx < 2; lx++)
        for (int lz = 0; lz < 2; lz++) {
            float lx0 = ax0 + lx * 24, lz0 = az0 + lz * 24;
            if (rnd() < 0.2f) {
                add_pad(lx0 + 1, lz0 + 1, lx0 + 23, lz0 + 23, 0.36f, 0.36f, 0.38f);
                add_crate(lx0 + 5, 0, lz0 + 5);
                add_crate(lx0 + 6.3f, 0, lz0 + 5);
                add_crate(lx0 + 5.6f, 1.2f, lz0 + 5);
                coin_line(v3(lx0 + 8, 1, lz0 + 12), v3(lx0 + 20, 1, lz0 + 12), 5);
                continue;
            }
            float w = rr(15, 21), d = rr(15, 21), h = rr(9, 22);
            float cx = lx0 + 12, cz = lz0 + 12;
            add_box(cx - w / 2, 0, cz - d / 2, cx + w / 2, h, cz + d / 2, PAL_COM[ri(6)], K_BUILDING);
        }
}
static void gen_down(float ax0, float az0) {
    int flip = rnd() < 0.5f;
    for (int lx = 0; lx < 2; lx++)
        for (int lz = 0; lz < 2; lz++) {
            float lx0 = ax0 + lx * 24, lz0 = az0 + lz * 24;
            int diag = ((lx == lz) != flip);
            float cx = lx0 + 12, cz = lz0 + 12;
            if (diag) {
                float w = rr(18, 22), h = (lx == 0) ? rr(36, 64) : rr(24, 44);
                add_box(cx - w / 2, 0, cz - w / 2, cx + w / 2, h, cz + w / 2, PAL_DOWN[ri(4)], K_BUILDING);
            } else if (rnd() < 0.4f) {
                add_tree(cx - 6, cz - 6); add_tree(cx + 6, cz + 6); add_tree(cx + 6, cz - 6);
                add_deco(DECO_BENCH, cx, 0, cz, 1)->yaw = 0;
                coin_ring(cx, 1, cz, 4, 6);
            } else {
                float w = rr(15, 19), h = rr(12, 22);
                add_box(cx - w / 2, 0, cz - w / 2, cx + w / 2, h, cz + w / 2, PAL_COM[ri(6)], K_BUILDING);
            }
        }
}
static void gen_park(float x0, float z0, float x1, float z1, int pond) {
    float cx = (x0 + x1) / 2, cz = (z0 + z1) / 2;
    float pr = 0;
    if (pond && nponds < 16) {
        pr = rr(6, 8);
        ponds[nponds].x = cx; ponds[nponds].z = cz; ponds[nponds].r = pr; nponds++;
        coin_ring(cx, 1, cz, pr + 2.5f, 10);
        for (int i = 0; i < 4; i++) {
            float a = PI / 4 + i * PI / 2;
            Deco *b = add_deco(DECO_BENCH, cx + cosf(a) * (pr + 5), 0, cz + sinf(a) * (pr + 5), 1);
            b->yaw = -a - PI / 2;
        }
    }
    for (int n = 0; n < 16; n++) {
        float x = rr(x0 + 5, x1 - 5), z = rr(z0 + 5, z1 - 5);
        float dd = sqrtf((x - cx) * (x - cx) + (z - cz) * (z - cz));
        if (dd < pr + 7) continue;
        add_tree(x, z);
    }
}

static void gen_tower(float x0, float z0, float x1, float z1) {
    float cx = (x0 + x1) / 2, cz = (z0 + z1) / 2;
    float th = 27.5f;
    add_boxc(cx - 3.5f, 0, cz - 3.5f, cx + 3.5f, th, cz + 3.5f, 0.78f, 0.66f, 0.5f, K_BUILDING)->roof = 9;
    add_deco(DECO_CLOCK, cx, th - 4.5f, cz, 3.5f);
    g_spots.towerTop = v3(cx, th, cz);
    for (int k = 0; k < 19; k++) {
        float a = k * 0.55f;
        float px = cx + cosf(a) * 8.5f, pz = cz + sinf(a) * 8.5f, top = 1.4f * (k + 1);
        float col[3] = {0.35f + 0.03f * (k % 3), 0.55f, 0.85f - 0.02f * k};
        add_box(px - 1.5f, top - 0.6f, pz - 1.5f, px + 1.5f, top, pz + 1.5f, col, K_PLATFORM);
        add_coin(px, top + 1.1f, pz);
    }
    g_gizmoPos[0] = v3(cx, th + 1.2f, cz);
    for (int i = 0; i < 4; i++) {
        float tx = (i & 1) ? x1 - 6 : x0 + 6, tz = (i & 2) ? z1 - 6 : z0 + 6;
        add_tree(tx, tz);
        add_tree(tx + ((i & 1) ? -5 : 5), tz);
    }
}
static void gen_stunt(float x0, float z0) {
#define LX(v) (x0 + (v))
#define LZ(v) (z0 + (v))
    add_ramp(LX(4), LZ(6), LX(20), LZ(14), 0, 4.5f);
    static const float arc[7][2] = {{22.5f, 5.06f}, {25, 5.34f}, {27.5f, 5.34f}, {30, 5.06f}, {32.5f, 4.5f}, {35, 3.66f}, {37.5f, 2.54f}};
    for (int i = 0; i < 7; i++) add_coin(LX(arc[i][0]), arc[i][1] + 1.0f, LZ(10));
    g_gizmoPos[9] = v3(LX(27), 6.6f, LZ(10));
    add_ramp(LX(43), LZ(22), LX(51), LZ(36), 2, 6);
    add_boxc(LX(43), 0, LZ(36), LX(51), 6, LZ(51), 0.55f, 0.55f, 0.6f, K_PLATFORM);
    coin_line(v3(LX(47), 7, LZ(38)), v3(LX(47), 7, LZ(47)), 5);
    g_gizmoPos[1] = v3(LX(47), 7.2f, LZ(49.5f));
    add_ramp(LX(24), LZ(40), LX(32), LZ(47), 1, 2.2f);
    add_ramp(LX(6), LZ(30), LX(13), LZ(44), 3, 3.0f);
    coin_line(v3(LX(9.5f), 4.5f, LZ(26)), v3(LX(9.5f), 2, LZ(18)), 4);
    add_crate(LX(28.8f), 0, LZ(26)); add_crate(LX(30), 0, LZ(26)); add_crate(LX(31.2f), 0, LZ(26));
    add_crate(LX(29.4f), 1.2f, LZ(26)); add_crate(LX(30.6f), 1.2f, LZ(26));
    add_crate(LX(30), 2.4f, LZ(26));
    add_sign(LX(27), 0.2f, LZ(52.8f), 0, "STUNT PARK", 0.18f, 1, 0.5f, 0.1f);
}
static void gen_plaza(float x0, float z0) {
    float shopC[3] = {0.3f, 0.7f, 0.65f};
    add_box(LX(12), 0, LZ(32), LX(42), 9, LZ(48), shopC, K_BUILDING)->roof = 8;
    add_sign(LX(27), 6.2f, LZ(32), PI, "BOLT & BARGAIN", 0.16f, 1, 0.9f, 0.2f);
    Deco *aw = add_deco(DECO_AWNING, LX(27), 3.4f, LZ(32), 8);
    aw->yaw = PI;
    g_spots.shop = v3(LX(27), 0, LZ(29.5f));
    g_spots.shopYaw = PI;
    for (int k = 0; k < 5; k++)
        add_boxc(LX(42), 0, LZ(32 + 3 * k), LX(46), 1.5f * (k + 1), LZ(35 + 3 * k), 0.75f, 0.7f, 0.62f, K_STEP);
    coin_line(v3(LX(44), 2.5f, LZ(33.5f)), v3(LX(44), 8.5f, LZ(45.5f)), 5);
    coin_line(v3(LX(16), 10, LZ(44)), v3(LX(36), 10, LZ(44)), 6);
    g_gizmoPos[2] = v3(LX(38), 10.2f, LZ(36));
    add_boxc(LX(24), 0, LZ(12), LX(30), 0.9f, LZ(18), 0.6f, 0.6f, 0.6f, K_INVIS);
    add_deco(DECO_FOUNTAIN, LX(27), 0, LZ(15), 3);
    coin_ring(LX(27), 1, LZ(15), 6, 8);
    add_tree(LX(6), LZ(6)); add_tree(LX(48), LZ(6)); add_tree(LX(6), LZ(26)); add_tree(LX(48), LZ(26));
    for (int i = 0; i < 2; i++) add_deco(DECO_BENCH, LX(15 + i * 24), 0, LZ(22), 1)->yaw = 0;
}
static void gen_home(float x0, float z0) {
    float hc[3] = {0.97f, 0.86f, 0.55f};
    add_box(LX(8), 0, LZ(8), LX(26), 7, LZ(24), hc, K_BUILDING)->roof = 1;
    add_boxc(LX(30), 0, LZ(8), LX(40), 4, LZ(18), 0.82f, 0.8f, 0.74f, K_BUILDING);
    add_boxc(LX(41), 0, LZ(10), LX(42.5f), 1.3f, LZ(11.5f), 0.3f, 0.45f, 0.3f, K_SOLID);
    add_boxc(LX(41), 0, LZ(12.2f), LX(43), 2.7f, LZ(14.2f), 0.7f, 0.7f, 0.72f, K_SOLID);
    g_gizmoPos[4] = v3(LX(35), 5.2f, LZ(13));
    add_pad(LX(30), LZ(19), LX(54), LZ(25), 0.55f, 0.55f, 0.55f);
    g_spots.homeCar = v3(LX(44), 0, LZ(22));
    g_spots.homeCarYaw = PI / 2;
    g_spots.home = v3(LX(36), 0, LZ(30));
    g_spots.homeYaw = PI / 2;
    add_sign(LX(17), 4.0f, LZ(24), 0, "HOME", 0.2f, 0.9f, 0.3f, 0.2f);
    add_tree(LX(5), LZ(40)); add_tree(LX(20), LZ(44)); add_tree(LX(46), LZ(44)); add_tree(LX(48), LZ(5));
    coin_line(v3(LX(10), 1, LZ(34)), v3(LX(26), 1, LZ(34)), 5);
}
static void gen_dock(float x0, float z0) {
    float wa[3] = {0.55f, 0.6f, 0.68f}, wb[3] = {0.72f, 0.56f, 0.4f};
    add_box(LX(4), 0, LZ(4), LX(30), 10, LZ(20), wa, K_BUILDING);
    add_box(LX(4), 0, LZ(34), LX(24), 8, LZ(50), wb, K_BUILDING);
    add_sign(LX(17), 6.5f, LZ(20), 0, "MOE'S DOCKS", 0.16f, 0.95f, 0.95f, 0.95f);
    static const float cc[6][3] = {{0.8f, 0.2f, 0.15f}, {0.15f, 0.4f, 0.8f}, {0.2f, 0.65f, 0.3f}, {0.9f, 0.6f, 0.1f}, {0.6f, 0.2f, 0.6f}, {0.3f, 0.6f, 0.65f}};
    add_box(LX(36), 0, LZ(6), LX(42), 2.6f, LZ(8.5f), cc[0], K_CONTAINER);
    add_box(LX(36), 0, LZ(8.5f), LX(42), 2.6f, LZ(11), cc[1], K_CONTAINER);
    add_box(LX(36), 0, LZ(11), LX(42), 2.6f, LZ(13.5f), cc[2], K_CONTAINER);
    add_box(LX(36), 2.6f, LZ(6), LX(42), 5.2f, LZ(8.5f), cc[3], K_CONTAINER);
    add_box(LX(36), 2.6f, LZ(8.5f), LX(42), 5.2f, LZ(11), cc[4], K_CONTAINER);
    add_box(LX(36), 5.2f, LZ(6), LX(42), 7.8f, LZ(8.5f), cc[5], K_CONTAINER);
    add_crate(LX(39), 0, LZ(14.8f));
    g_gizmoPos[3] = v3(LX(39), 9.0f, LZ(7.25f));
    for (int i = 0; i < 3; i++) add_box(LX(46), 0, LZ(22 + i * 8), LX(48.5f), 2.6f, LZ(28 + i * 8), cc[(i + 2) % 6], K_CONTAINER);
    for (int a = 0; a < 3; a++)
        for (int b = 0; b < 3; b++) add_crate(LX(29 + a * 1.25f), 0, LZ(25 + b * 1.25f));
    add_crate(LX(29.6f), 1.2f, LZ(25.6f));
    add_crate(LX(30.9f), 1.2f, LZ(26.9f));
    add_crate(LX(33), 0, LZ(40)); add_crate(LX(34.3f), 0, LZ(40)); add_crate(LX(33.6f), 1.2f, LZ(40));
    coin_line(v3(LX(30), 1, LZ(45)), v3(LX(44), 1, LZ(45)), 6);
}
static void gen_pizza(float x0, float z0) {
    float pc[3] = {0.86f, 0.28f, 0.22f};
    add_box(LX(26), 0, LZ(12), LX(44), 7, LZ(32), pc, K_BUILDING)->roof = 8;
    add_sign(LX(44), 4.8f, LZ(22), PI / 2, "TONY'S PIZZA", 0.15f, 1, 1, 0.85f);
    Deco *aw = add_deco(DECO_AWNING, LX(44), 3.2f, LZ(22), 7);
    aw->yaw = PI / 2;
    add_box(LX(4), 0, LZ(4), LX(20), 9, LZ(20), PAL_COM[1], K_BUILDING);
    Box *b = add_box(LX(6), 0, LZ(36), LX(18), 5.5f, LZ(48), PAL_HOUSE[2], K_BUILDING); b->roof = 2;
    b = add_box(LX(30), 0, LZ(37), LX(42), 5.5f, LZ(49), PAL_HOUSE[4], K_BUILDING); b->roof = 3;
    add_tree(LX(24), LZ(42)); add_tree(LX(48), LZ(6));
}
static void gen_lab(float x0, float z0) {
    float lc[3] = {0.92f, 0.93f, 0.96f};
    add_box(LX(12), 0, LZ(22), LX(42), 10, LZ(48), lc, K_BUILDING)->roof = 8;
    add_deco(DECO_DOME, LX(27), 10, LZ(35), 6);
    add_deco(DECO_ANTENNA, LX(17), 10, LZ(44), 1);
    add_sign(LX(27), 7.0f, LZ(22), PI, "FIZZ LABS", 0.17f, 0.2f, 0.9f, 0.5f);
    for (int k = 0; k < 6; k++)
        add_boxc(LX(8), 0, LZ(22 + 3 * k), LX(12), 1.5f * (k + 1), LZ(25 + 3 * k), 0.6f, 0.65f, 0.75f, K_STEP);
    g_gizmoPos[5] = v3(LX(38), 11.2f, LZ(44));
    Box *b = add_box(LX(4), 0, LZ(3), LX(16), 5, LZ(15), PAL_HOUSE[1], K_BUILDING); b->roof = 3;
    b = add_box(LX(36), 0, LZ(3), LX(48), 5, LZ(15), PAL_HOUSE[3], K_BUILDING); b->roof = 1;
    add_tree(LX(26), LZ(8)); add_tree(LX(48), LZ(50));
    coin_line(v3(LX(10), 10, LZ(41)), v3(LX(10), 10, LZ(46)), 3);
}
static void gen_gas(float x0, float z0, int idx, int store) {
    add_pad(LX(4), LZ(4), LX(50), LZ(50), 0.4f, 0.4f, 0.43f);
    for (int a = 0; a < 2; a++)
        for (int b = 0; b < 2; b++)
            add_boxc(LX(14 + a * 26) - 0.3f, 0, LZ(14 + b * 26) - 0.3f, LX(14 + a * 26) + 0.3f, 5.2f, LZ(14 + b * 26) + 0.3f, 0.85f, 0.85f, 0.88f, K_SOLID);
    add_boxc(LX(12), 5.2f, LZ(12), LX(42), 5.8f, LZ(42), 0.95f, 0.35f, 0.15f, K_SOLID);
    add_boxc(LX(26.4f), 0, LZ(22), LX(27.6f), 1.7f, LZ(23), 0.9f, 0.9f, 0.3f, K_SOLID);
    add_boxc(LX(26.4f), 0, LZ(31), LX(27.6f), 1.7f, LZ(32), 0.9f, 0.9f, 0.3f, K_SOLID);
    add_sign(LX(27), 6.0f, LZ(42), 0, "GAS", 0.18f, 1, 0.95f, 0.3f);
    add_sign(LX(27), 6.0f, LZ(12), PI, "GAS", 0.18f, 1, 0.95f, 0.3f);
    (void)idx;
    if (g_spots.ngas < NUM_GAS) g_spots.gas[g_spots.ngas++] = v3(LX(27), 0, LZ(27));
    if (store) {
        Box *b = add_box(LX(42), 0, LZ(44), LX(52), 4, LZ(52), PAL_COM[4], K_BUILDING);
        b->roof = 8;
    }
}
static void gen_tuner(float x0, float z0) {
    add_boxc(LX(4), 0, LZ(4), LX(28), 7, LZ(26), 0.55f, 0.55f, 0.6f, K_BUILDING)->roof = 8;
    add_sign(LX(28), 5.2f, LZ(15), PI / 2, "TURBO TUNERS", 0.13f, 1, 0.6f, 0.1f);
    Deco *aw = add_deco(DECO_AWNING, LX(28), 3.0f, LZ(15), 8);
    aw->yaw = PI / 2;
    g_spots.tuner = v3(LX(30.5f), 0, LZ(15));
    g_spots.tunerYaw = PI / 2;
    add_pad(LX(30), LZ(2), LX(52), LZ(28), 0.45f, 0.45f, 0.47f);
    /* small gas station in the south half */
    add_pad(LX(4), LZ(30), LX(50), LZ(52), 0.4f, 0.4f, 0.43f);
    add_boxc(LX(14), 5.2f, LZ(33), LX(40), 5.8f, LZ(49), 0.95f, 0.35f, 0.15f, K_SOLID);
    for (int a = 0; a < 2; a++)
        for (int b = 0; b < 2; b++)
            add_boxc(LX(15 + a * 24) - 0.3f, 0, LZ(34 + b * 14) - 0.3f, LX(15 + a * 24) + 0.3f, 5.2f, LZ(34 + b * 14) + 0.3f, 0.85f, 0.85f, 0.88f, K_SOLID);
    add_boxc(LX(26.4f), 0, LZ(40.5f), LX(27.6f), 1.7f, LZ(41.5f), 0.9f, 0.9f, 0.3f, K_SOLID);
    add_sign(LX(27), 6.0f, LZ(49), 0, "GAS", 0.18f, 1, 0.95f, 0.3f);
    if (g_spots.ngas < NUM_GAS) g_spots.gas[g_spots.ngas++] = v3(LX(27), 0, LZ(41));
    add_tree(LX(48), LZ(4));
}
static void gen_armory(float x0, float z0) {
    add_boxc(LX(4), 0, LZ(6), LX(26), 8, LZ(24), 0.42f, 0.36f, 0.3f, K_BUILDING)->roof = 8;
    add_sign(LX(15), 5.6f, LZ(6), PI, "BOOM BROS ARMORY", 0.11f, 1, 0.35f, 0.2f);
    Deco *aw = add_deco(DECO_AWNING, LX(15), 3.2f, LZ(6), 8);
    aw->yaw = PI;
    g_spots.armory = v3(LX(15), 0, LZ(3.5f));
    g_spots.armoryYaw = PI;
    /* outdoor shooting range */
    add_pad(LX(30), LZ(4), LX(52), LZ(52), 0.62f, 0.55f, 0.42f);
    add_boxc(LX(29), 0, LZ(14), LX(30), 1.4f, LZ(52), 0.5f, 0.42f, 0.3f, K_SOLID);
    add_boxc(LX(52), 0, LZ(14), LX(53), 1.4f, LZ(52), 0.5f, 0.42f, 0.3f, K_SOLID);
    add_boxc(LX(29), 0, LZ(51), LX(53), 4.0f, LZ(53), 0.5f, 0.42f, 0.3f, K_SOLID);
    add_boxc(LX(31), 0, LZ(10), LX(51), 1.0f, LZ(11), 0.75f, 0.6f, 0.35f, K_SOLID);
    add_sign(LX(41), 4.2f, LZ(51), PI, "SHOOTING RANGE", 0.1f, 1, 0.85f, 0.3f);
    int n = 0;
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 4; c++) g_spots.range[n++] = v3(LX(34 + c * 5), 1.3f, LZ(22 + r * 10));
    coin_line(v3(LX(10), 9.2f, LZ(12)), v3(LX(22), 9.2f, LZ(18)), 4);
}
static void gen_clinic(float x0, float z0) {
    float wc[3] = {0.95f, 0.95f, 0.96f};
    add_box(LX(4), 0, LZ(14), LX(26), 11, LZ(42), wc, K_BUILDING)->roof = 8;
    add_boxc(LX(13), 11, LZ(25), LX(17), 11.3f, LZ(31), 0.9f, 0.15f, 0.15f, K_SOLID);
    add_boxc(LX(12), 11, LZ(26.5f), LX(18), 11.3f, LZ(29.5f), 0.9f, 0.15f, 0.15f, K_SOLID);
    add_sign(LX(15), 7.5f, LZ(42), 0, theme == 0 ? "MAPLE MEDICAL" : theme == 1 ? "DESERT CLINIC" : "ALPINE CLINIC", 0.12f, 0.9f, 0.15f, 0.15f);
    g_spots.clinic = v3(LX(15), 0, LZ(44.5f));
    if (g_spots.nhosp < 4) g_spots.hosp[g_spots.nhosp++] = g_spots.clinic;
    g_spots.clinicYaw = 0;
    add_boxc(LX(30), 0, LZ(16), LX(50), 9, LZ(42), 0.35f, 0.42f, 0.6f, K_BUILDING)->roof = 8;
    add_sign(LX(40), 6.5f, LZ(42), 0, "POLICE", 0.16f, 0.85f, 0.9f, 1);
    g_spots.police = v3(LX(40), 0, LZ(44.5f));
    add_pad(LX(30), LZ(2), LX(52), LZ(14), 0.45f, 0.45f, 0.47f);
    add_tree(LX(28), LZ(48)); add_tree(LX(2), LZ(48));
}
static void gen_hospital(float x0, float z0) {
    float wc[3] = {0.93f, 0.95f, 0.97f};
    add_box(LX(6), 0, LZ(8), LX(48), 16, LZ(38), wc, K_BUILDING)->roof = 8;
    add_boxc(LX(14), 0, LZ(38), LX(40), 5, LZ(44), 0.82f, 0.86f, 0.9f, K_BUILDING)->roof = 8;
    add_boxc(LX(25), 16, LZ(18), LX(29), 16.3f, LZ(28), 0.9f, 0.15f, 0.15f, K_SOLID);
    add_boxc(LX(22), 16, LZ(21), LX(32), 16.3f, LZ(25), 0.9f, 0.15f, 0.15f, K_SOLID);
    add_sign(LX(27), 11.5f, LZ(38), 0, "BAYSIDE HOSPITAL", 0.14f, 0.9f, 0.15f, 0.15f);
    V3 sp = v3(LX(27), 0, LZ(46.5f));
    if (g_spots.nhosp < 4) g_spots.hosp[g_spots.nhosp++] = sp;
    add_pad(LX(4), LZ(44), LX(50), LZ(52), 0.45f, 0.45f, 0.47f);
    add_tree(LX(3), LZ(3)); add_tree(LX(51), LZ(3)); add_tree(LX(3), LZ(42)); add_tree(LX(51), LZ(42));
    coin_line(v3(LX(10), 17.2f, LZ(12)), v3(LX(44), 17.2f, LZ(12)), 8);
}
static void gun_store(float x0, float z0, float sx, float sz) {
    add_boxc(LX(sx), 0, LZ(sz), LX(sx + 18), 6.5f, LZ(sz + 16), 0.22f, 0.22f, 0.25f, K_BUILDING)->roof = 8;
    add_sign(LX(sx + 18), 4.6f, LZ(sz + 8), PI / 2, "LOCK & LOAD", 0.14f, 1, 0.75f, 0.1f);
    Deco *aw = add_deco(DECO_AWNING, LX(sx + 18), 2.9f, LZ(sz + 8), 7);
    aw->yaw = PI / 2;
    g_spots.gunx = v3(LX(sx + 20.5f), 0, LZ(sz + 8));
    g_spots.gunxYaw = PI / 2;
}
static void gen_mall(float x0, float z0) {
    add_box(LX(4), 0, LZ(4), LX(50), 12, LZ(22), PAL_COM[4], K_BUILDING)->roof = 8;
    add_sign(LX(27), 8.5f, LZ(22), 0, "BAYVIEW MALL", 0.16f, 1, 0.8f, 0.3f);
    Deco *aw = add_deco(DECO_AWNING, LX(27), 3.2f, LZ(22), 14);
    aw->yaw = 0;
    gun_store(x0, z0, 4, 32);
    add_pad(LX(26), LZ(26), LX(52), LZ(52), 0.84f, 0.79f, 0.68f);
    add_boxc(LX(36), 0, LZ(36), LX(42), 0.9f, LZ(42), 0.6f, 0.6f, 0.6f, K_INVIS);
    add_deco(DECO_FOUNTAIN, LX(39), 0, LZ(39), 3);
    coin_ring(LX(39), 1, LZ(39), 6, 8);
    add_tree(LX(30), LZ(48)); add_tree(LX(48), LZ(48)); add_tree(LX(48), LZ(28));
    for (int k = 0; k < 4; k++) add_boxc(LX(50), 0, LZ(6 + 3 * k), LX(53), 3.0f * (k + 1), LZ(9 + 3 * k), 0.75f, 0.7f, 0.62f, K_STEP);
    coin_line(v3(LX(10), 13, LZ(13)), v3(LX(46), 13, LZ(13)), 8);
}
static void gen_stadium(float x0, float z0) {
    add_pad(LX(12), LZ(12), LX(42), LZ(42), 0.3f, 0.62f, 0.27f);
    for (int k = 0; k < 4; k++) {
        float a = 3 + 2.25f * (3 - k), b = a + 2.25f, h = 1.6f * (k + 1);
        float col[3] = {0.7f - 0.05f * k, 0.72f - 0.05f * k, 0.78f - 0.04f * k};
        add_box(LX(a), 0, LZ(a), LX(54 - a), h, LZ(b), col, K_STEP);
        add_box(LX(a), 0, LZ(54 - b), LX(22), h, LZ(54 - a), col, K_STEP);
        add_box(LX(32), 0, LZ(54 - b), LX(54 - a), h, LZ(54 - a), col, K_STEP);
        add_box(LX(a), 0, LZ(b), LX(b), h, LZ(54 - b), col, K_STEP);
        add_box(LX(54 - b), 0, LZ(b), LX(54 - a), h, LZ(54 - b), col, K_STEP);
    }
    for (int i = 0; i < 4; i++) {
        float x = (i & 1) ? LX(51) : LX(3), z = (i & 2) ? LZ(51) : LZ(3);
        add_boxc(x - 0.3f, 0, z - 0.3f, x + 0.3f, 16, z + 0.3f, 0.5f, 0.5f, 0.55f, K_SOLID);
        add_boxc(x - 1.5f, 16, z - 1.5f, x + 1.5f, 17.2f, z + 1.5f, 1, 0.95f, 0.75f, K_SOLID);
    }
    add_sign(LX(27), 7.5f, LZ(3), PI, "MAPLE STADIUM", 0.16f, 1, 0.85f, 0.2f);
    coin_ring(LX(27), 1, LZ(27), 9, 14);
    coin_line(v3(LX(5), 7.6f, LZ(10)), v3(LX(5), 7.6f, LZ(44)), 8);
}
static void gen_fire(float x0, float z0) {
    add_boxc(LX(6), 0, LZ(8), LX(36), 9, LZ(30), 0.8f, 0.2f, 0.15f, K_BUILDING)->roof = 8;
    for (int i = 0; i < 3; i++) add_boxc(LX(9 + i * 9), 0, LZ(29.95f), LX(15 + i * 9), 5, LZ(30.1f), 0.85f, 0.85f, 0.8f, K_SOLID);
    add_boxc(LX(38), 0, LZ(10), LX(44), 20, LZ(16), 0.75f, 0.2f, 0.15f, K_BUILDING)->roof = 8;
    add_sign(LX(21), 6.5f, LZ(30.2f), 0, "FIRE STATION 2", 0.11f, 1, 1, 1);
    add_pad(LX(6), LZ(30), LX(36), LZ(50), 0.45f, 0.45f, 0.47f);
    add_tree(LX(46), LZ(40)); add_tree(LX(46), LZ(48));
    coin_line(v3(LX(41), 21.2f, LZ(13)), v3(LX(41), 21.2f, LZ(13)), 1);
}
static void gen_build(float x0, float z0) {
    float steel[3] = {0.85f, 0.55f, 0.15f};
    add_pad(LX(2), LZ(2), LX(52), LZ(52), 0.62f, 0.52f, 0.38f);
    for (int f = 1; f <= 4; f++) {
        float y = f * 5.0f;
        add_box(LX(10), y - 0.4f, LZ(10), LX(40), y, LZ(40), steel, K_PLATFORM);
        coin_line(v3(LX(14), y + 1, LZ(25)), v3(LX(36), y + 1, LZ(25)), 6);
    }
    for (int i = 0; i < 4; i++) {
        float x = (i & 1) ? LX(39.5f) : LX(10.5f), z = (i & 2) ? LZ(39.5f) : LZ(10.5f);
        add_boxc(x - 0.4f, 0, z - 0.4f, x + 0.4f, 20, z + 0.4f, 0.55f, 0.35f, 0.15f, K_SOLID);
    }
    for (int k = 0; k < 20; k++)
        add_boxc(LX(41 + (k % 2) * 3), 0, LZ(10 + k * 1.5f), LX(44 + (k % 2) * 3), 1.0f * (k + 1), LZ(11.5f + k * 1.5f), 0.6f, 0.6f, 0.62f, K_STEP);
    add_boxc(LX(4), 0, LZ(44), LX(6), 34, LZ(46), 0.95f, 0.75f, 0.1f, K_SOLID);
    add_boxc(LX(4), 32, LZ(25), LX(6), 33.5f, LZ(50), 0.95f, 0.75f, 0.1f, K_SOLID);
    for (int k = 0; k < 6; k++) add_crate(LX(16 + k * 1.3f), 0, LZ(46));
    add_sign(LX(27), 2.5f, LZ(2), PI, "UNDER CONSTRUCTION", 0.08f, 1, 0.8f, 0.1f);
}
/* ----------------------------------------------- Sunset Sands (desert) */
static void gen_adobe(float ax0, float az0) {
    static const float pal[4][3] = {{0.86f, 0.66f, 0.45f}, {0.92f, 0.78f, 0.6f}, {0.78f, 0.5f, 0.35f}, {0.95f, 0.88f, 0.72f}};
    for (int lx = 0; lx < 2; lx++)
        for (int lz = 0; lz < 2; lz++) {
            float lx0 = ax0 + lx * 24, lz0 = az0 + lz * 24;
            float w = rr(9, 14), d = rr(9, 14), h = rr(3.5f, 6.5f);
            float cx = lx0 + 12, cz = lz0 + 12;
            Box *b = add_box(cx - w / 2, 0, cz - d / 2, cx + w / 2, h, cz + d / 2, pal[ri(4)], K_BUILDING);
            b->roof = 8;
            if (rnd() < 0.5f) add_box(cx - w / 4, h, cz - d / 4, cx + w / 4, h + 2.5f, cz + d / 4, pal[ri(4)], K_BUILDING)->roof = 8;
            add_tree(lx0 + 2, lz0 + 2);
        }
}
static void gen_resort(float x0, float z0) {
    static const float pal[3][3] = {{0.98f, 0.75f, 0.7f}, {0.7f, 0.9f, 0.88f}, {0.98f, 0.92f, 0.7f}};
    add_box(LX(6), 0, LZ(6), LX(24), rr(30, 45), LZ(24), pal[ri(3)], K_BUILDING);
    add_box(LX(30), 0, LZ(30), LX(48), rr(22, 38), LZ(48), pal[ri(3)], K_BUILDING);
    add_pad(LX(30), LZ(8), LX(48), LZ(22), 0.25f, 0.65f, 0.9f);
    add_pad(LX(8), LZ(30), LX(22), LZ(46), 0.25f, 0.65f, 0.9f);
    add_tree(LX(28), LZ(6)); add_tree(LX(50), LZ(6)); add_tree(LX(4), LZ(28)); add_tree(LX(26), LZ(50));
}
static void gen_casino(float x0, float z0) {
    add_boxc(LX(6), 0, LZ(6), LX(48), 14, LZ(36), 0.55f, 0.15f, 0.45f, K_BUILDING)->roof = 8;
    add_boxc(LX(5.5f), 13, LZ(5.5f), LX(48.5f), 14.5f, LZ(36.5f), 0.95f, 0.78f, 0.2f, K_SOLID);
    add_sign(LX(27), 9.5f, LZ(36), 0, "LUCKY SANDS CASINO", 0.15f, 1, 0.85f, 0.2f);
    Deco *aw = add_deco(DECO_AWNING, LX(27), 3.5f, LZ(36), 14);
    aw->yaw = 0;
    g_spots.giver[14] = v3(LX(18), 0, LZ(39)); g_spots.giverYaw[14] = 0;
    add_pad(LX(4), LZ(38), LX(50), LZ(52), 0.45f, 0.42f, 0.4f);
    for (int i = 0; i < 6; i++) add_tree(LX(6 + i * 8.5f), LZ(50));
    g_gizmoPos[12] = v3(LX(44), 15.8f, LZ(10));
    coin_line(v3(LX(10), 15.5f, LZ(20)), v3(LX(40), 15.5f, LZ(20)), 8);
}
static void gen_canyon(float x0, float z0) {
    float rock[3] = {0.72f, 0.42f, 0.28f}, rock2[3] = {0.8f, 0.52f, 0.33f};
    add_box(LX(4), 0, LZ(4), LX(18), 9, LZ(18), rock, K_SOLID);
    add_box(LX(32), 0, LZ(6), LX(50), 14, LZ(20), rock2, K_SOLID);
    add_box(LX(6), 0, LZ(34), LX(22), 6, LZ(50), rock2, K_SOLID);
    add_box(LX(36), 0, LZ(36), LX(48), 18, LZ(48), rock, K_SOLID);
    add_ramp(LX(18), LZ(6), LX(30), LZ(16), 0, 9);
    add_ramp(LX(22), LZ(36), LX(34), LZ(48), 1, 6);
    add_ramp(LX(36), LZ(20), LX(48), LZ(34), 3, 14);
    for (int k = 0; k < 6; k++) add_boxc(LX(30), 0, LZ(36 + 2 * k), LX(34), 3.0f * (k + 1), LZ(38 + 2 * k), 0.75f, 0.48f, 0.3f, K_STEP);
    coin_line(v3(LX(8), 10, LZ(11)), v3(LX(15), 10, LZ(11)), 4);
    coin_line(v3(LX(36), 15, LZ(13)), v3(LX(46), 15, LZ(13)), 5);
    coin_line(v3(LX(26), 12, LZ(27)), v3(LX(26), 8, LZ(40)), 5);
    g_gizmoPos[10] = v3(LX(42), 19.2f, LZ(42));
    g_gizmoPos[11] = v3(LX(27), 13, LZ(26));
    add_sign(LX(27), 0.3f, LZ(52.5f), 0, "DEVIL'S CANYON", 0.14f, 1, 0.6f, 0.2f);
}
static void gen_oasis(float x0, float z0) {
    gen_park(x0 + 3, z0 + 3, x0 + 51, z0 + 51, 1);
    g_gizmoPos[13] = v3(x0 + 27, 1.2f, z0 + 27);
}
/* ---------------------------------------------------- Frostpeak (snow) */
static void gen_cabin(float ax0, float az0) {
    static const float pal[3][3] = {{0.45f, 0.3f, 0.18f}, {0.55f, 0.38f, 0.22f}, {0.6f, 0.25f, 0.2f}};
    for (int lx = 0; lx < 2; lx++)
        for (int lz = 0; lz < 2; lz++) {
            float lx0 = ax0 + lx * 24, lz0 = az0 + lz * 24;
            float w = rr(8, 12), d = rr(8, 12), h = rr(3.5f, 5);
            float cx = lx0 + 12, cz = lz0 + 12;
            Box *b = add_box(cx - w / 2, 0, cz - d / 2, cx + w / 2, h, cz + d / 2, pal[ri(3)], K_BUILDING);
            b->roof = 1 + ri(4);
            add_tree(lx0 + 2, lz0 + 2);
            if (rnd() < 0.5f) add_tree(lx0 + 21, lz0 + 3);
        }
}
static void gen_lodge(float x0, float z0) {
    add_boxc(LX(6), 0, LZ(10), LX(48), 10, LZ(34), 0.5f, 0.33f, 0.2f, K_BUILDING)->roof = 2;
    add_sign(LX(27), 6.5f, LZ(34), 0, "FROSTPEAK LODGE", 0.15f, 1, 0.95f, 0.8f);
    g_spots.giver[15] = v3(LX(18), 0, LZ(37)); g_spots.giverYaw[15] = 0;
    add_pad(LX(4), LZ(36), LX(50), LZ(52), 0.55f, 0.55f, 0.58f);
    g_gizmoPos[18] = v3(LX(27), 13.6f, LZ(22));
    for (int i = 0; i < 5; i++) add_tree(LX(5 + i * 11), LZ(4));
}
static void gen_ski(float x0, float z0) {
    /* a huge ski-jump ramp with stairs to the top */
    add_boxc(LX(2), 0, LZ(18), LX(10), 16, LZ(36), 0.85f, 0.88f, 0.92f, K_PLATFORM);
    add_ramp(LX(10), LZ(20), LX(40), LZ(34), 1, 16);
    for (int k = 0; k < 10; k++) add_boxc(LX(2 + k * 0.8f), 0, LZ(36), LX(3 + k * 0.8f), 1.6f * (k + 1), LZ(40), 0.6f, 0.62f, 0.68f, K_STEP);
    add_ramp(LX(40), LZ(20), LX(52), LZ(34), 1, 0.01f);
    coin_line(v3(LX(12), 18, LZ(27)), v3(LX(38), 3, LZ(27)), 8);
    add_ramp(LX(12), LZ(42), LX(26), LZ(50), 0, 4);
    coin_line(v3(LX(30), 7, LZ(46)), v3(LX(46), 3, LZ(46)), 5);
    g_gizmoPos[15] = v3(LX(6), 17.2f, LZ(27));
    g_gizmoPos[16] = v3(LX(36), 9.5f, LZ(46));
    add_sign(LX(27), 0.3f, LZ(52.5f), 0, "SKI JUMP", 0.14f, 0.2f, 0.4f, 0.9f);
    add_tree(LX(48), LZ(5)); add_tree(LX(30), LZ(8));
}
static void gen_lake(float x0, float z0) {
    gen_park(x0 + 3, z0 + 3, x0 + 51, z0 + 51, 1);
    g_gizmoPos[17] = v3(x0 + 27, 1.2f, z0 + 27);
}

static void gen_airport(void) {
    /* concrete apron, runway and access road are drawn by the ground list */
    float term[3] = {0.62f, 0.72f, 0.82f}, hang[3] = {0.62f, 0.64f, 0.68f};
    static char apName[3][40];
    snprintf(apName[theme], sizeof apName[0], "%s AIRPORT", map_name(g_curMap));
    add_box(-130, 0, AZ(344), -40, 12, AZ(372), term, K_BUILDING)->roof = 8;
    add_sign(-85, 8.5f, AZ(372), 0, apName[theme], 0.15f, 1, 1, 1);
    add_sign(-110, 3.6f, AZ(372), 0, "FLIGHTS", 0.1f, 0.4f, 0.85f, 1);
    g_spots.travel = v3(-110, 0, AZ(375));
    add_deco(DECO_DOME, -115, 12, AZ(358), 4);
    add_boxc(-24, 0, AZ(350), -16, 26, AZ(358), 0.85f, 0.85f, 0.82f, K_BUILDING)->roof = 8;
    add_boxc(-27, 26, AZ(347), -13, 30.5f, AZ(361), 0.3f, 0.45f, 0.6f, K_SOLID);
    add_boxc(-28, 30.5f, AZ(346), -12, 31.2f, AZ(362), 0.8f, 0.8f, 0.8f, K_SOLID);
    add_deco(DECO_ANTENNA, -20, 31.2f, AZ(354), 1);
    add_box(30, 0, AZ(344), 100, 14, AZ(378), hang, K_BUILDING)->roof = 8;
    add_sign(65, 10.0f, AZ(378), 0, "SKYLINE HANGAR", 0.15f, 0.4f, 0.8f, 1);
    g_spots.hangar = v3(65, 0, AZ(380.5f));
    g_spots.hangarYaw = 0;
    add_box(120, 0, AZ(344), 180, 14, AZ(378), hang, K_BUILDING)->roof = 8;
    add_sign(150, 10.0f, AZ(378), 0, "CARGO", 0.15f, 0.9f, 0.9f, 0.9f);
    /* fuel pad */
    add_pad(140, AZ(418), 160, AZ(438), 0.8f, 0.7f, 0.2f);
    add_boxc(148.5f, 0, AZ(412), 151.5f, 2.4f, AZ(414), 0.9f, 0.3f, 0.15f, K_SOLID);
    add_sign(150, 2.8f, AZ(414), 0, "AIR FUEL", 0.08f, 1, 0.95f, 0.3f);
    if (g_spots.ngas < NUM_GAS) g_spots.gas[g_spots.ngas++] = v3(150, 0, AZ(428));
    /* aircraft stands */
    static const float sx[NUM_STANDS] = {-230, -195, -160, -115, -80, -40, 230};
    static const int st[NUM_STANDS] = {AIR_FIRST, AIR_FIRST, AIR_FIRST + 1, AIR_FIRST + 2, AIR_FIRST + 4, AIR_FIRST + 3, -1};
    for (int i = 0; i < NUM_STANDS; i++) {
        g_spots.stand[i] = v3(sx[i], 0, AZ(412));
        g_spots.standYaw[i] = 0;
        g_spots.standType[i] = st[i];
        if (st[i] == AIR_FIRST + 2 || st[i] == AIR_FIRST + 4) add_pad(sx[i] - 7, AZ(405), sx[i] + 7, AZ(419), 0.3f, 0.55f, 0.35f);
        else add_pad(sx[i] - 7, AZ(404), sx[i] + 7, AZ(420), 0.75f, 0.75f, 0.72f);
    }
    add_sign(230, 0.25f, AZ(421), 0, "YOUR STAND", 0.09f, 1, 0.85f, 0.2f);
    g_spots.runway = v3(-255, 0, AZ(487.5f));
    g_spots.runwayYaw = PI / 2;
    /* trees and sky coins */
    for (int i = 0; i < 26; i++) add_tree(-290 + i * 23.0f, AZ(548));
    for (int i = 0; i < 8; i++) { add_tree(-300, AZ(360 + i * 22.0f)); add_tree(300, AZ(360 + i * 22.0f)); }
    coin_ring(0, 40, AZ(487), 14, 12);
    coin_line(v3(-150, 25, AZ(487)), v3(150, 25, AZ(487)), 12);
    coin_ring(-200, 60, AZ(430), 10, 10);
    /* walls around the airport extension */
    add_boxc(-1000, 0, AIR_Z0, -AIR_X, 60, 1000, 0, 0, 0, K_INVIS);
    add_boxc(AIR_X, 0, AIR_Z0, 1000, 60, 1000, 0, 0, 0, K_INVIS);
    add_boxc(-1000, 0, AIR_Z1, 1000, 60, 1000, 0, 0, 0, K_INVIS);
}
int is_land(float x, float z) {
    if (fabsf(x) < ISLAND && fabsf(z) < ISLAND) return 1;
    if (fabsf(x) < AIR_X + 8 && z > 0 && z < AIR_Z1 + 10) return 1;
    return 0;
}
#undef LX
#undef LZ

static void assign_types(void) {
    float c = (NBLK - 1) * 0.5f;
    for (int i = 0; i < NBLK; i++)
        for (int j = 0; j < NBLK; j++) {
            float d = fmaxf(fabsf(i - c), fabsf(j - c));
            if (g_curMap == 0) g_blockType[i][j] = d < 2 ? BT_DOWN : d < 3 ? BT_COM : (d > 4 && rnd() < 0.3f) ? BT_COM : BT_RES;
            else if (g_curMap == 1) g_blockType[i][j] = d < 1 ? BT_RESORT : BT_ADOBE;
            else g_blockType[i][j] = d < 1 ? BT_COM : BT_CABIN;
        }
    if (g_curMap == 0) {
        /* the original town, shifted into the middle of the bigger grid */
        g_blockType[3 + BO][3 + BO] = BT_TOWER;
        g_blockType[4 + BO][4 + BO] = BT_STUNT;
        g_blockType[3 + BO][4 + BO] = BT_PLAZA;
        g_blockType[0 + BO][0 + BO] = BT_HOME;
        g_blockType[7 + BO][0 + BO] = BT_DOCK;
        g_blockType[1 + BO][6 + BO] = BT_PIZZA;
        g_blockType[6 + BO][6 + BO] = BT_LAB;
        g_blockType[5 + BO][2 + BO] = BT_PARK;
        g_blockType[2 + BO][5 + BO] = BT_PARK;
        g_blockType[2 + BO][4 + BO] = BT_ARMORY;
        g_blockType[5 + BO][3 + BO] = BT_CLINIC;
        g_blockType[1 + BO][2 + BO] = BT_TUNER;
        g_blockType[6 + BO][4 + BO] = BT_GAS;
        g_blockType[2 + BO][7 + BO] = BT_GAS;
        /* new districts around the old town */
        g_blockType[10][5] = BT_HOSPITAL;
        g_blockType[1][10] = BT_MALL;
        g_blockType[9][10] = BT_STADIUM;
        g_blockType[5][0] = BT_FIRE;
        g_blockType[11][11] = BT_BUILD;
        g_blockType[11][1] = BT_GAS;
        g_blockType[0][6] = BT_GAS;
        g_blockType[6][11] = BT_PARK;
        g_blockType[11][7] = BT_PARK;
        g_blockType[0][1] = BT_PARK;
    } else if (g_curMap == 1) {
        g_blockType[0][0] = BT_CANYON;
        g_blockType[2][4] = BT_CASINO;
        g_blockType[5][1] = BT_CLINIC;
        g_blockType[1][4] = BT_GAS;
        g_blockType[4][4] = BT_GUNX;
        g_blockType[5][5] = BT_OASIS;
        g_blockType[3][1] = BT_PLAZA;
    } else {
        g_blockType[0][0] = BT_SKI;
        g_blockType[5][0] = BT_LAKE;
        g_blockType[2][2] = BT_LODGE;
        g_blockType[4][4] = BT_ARMORY;
        g_blockType[1][4] = BT_CLINIC;
        g_blockType[5][3] = BT_GAS;
        g_blockType[3][5] = BT_PARK;
    }
}

static void gen_outer(void) {
    /* trees on the green belt outside the ring road */
    float H = CITY_HALF, b0 = H + 12, b1 = H + 36, mid = H + 22;
    int n = (int)(220 * H / 280);
    for (int k = 0; k < n; k++) {
        float a = rr(-b1, b1), b = rr(b0, b1) * (rnd() < 0.5f ? -1 : 1);
        float x, z;
        if (rnd() < 0.5f) { x = a; z = b; } else { x = b; z = a; }
        if (fabsf(z - mid) < 14 && x > -110 && x < -40) continue;
        if (fabsf(z + mid) < 14 && x > 40 && x < 110) continue;
        if (fabsf(x) > H + 20 && fabsf(z) > H + 20) continue;
        if (fabsf(x) < 16 && z > H) continue;
        add_tree(x, z);
    }
    add_ramp(-100, mid - 4, -84, mid + 4, 0, 3);
    coin_line(v3(-80, 5, mid), v3(-60, 2.5f, mid), 5);
    add_ramp(84, -mid - 4, 100, -mid + 4, 1, 3);
    coin_line(v3(80, 5, -mid), v3(60, 2.5f, -mid), 5);
    V3 c1 = v3(H + 32, 1.2f, H + 32), c2 = v3(-H - 32, 1.2f, H + 32);
    if (g_curMap == 0) { g_gizmoPos[7] = c1; g_gizmoPos[8] = c2; }
    else if (g_curMap == 1) g_gizmoPos[14] = c1;
    else g_gizmoPos[19] = c1;
    coin_line(v3(-H - 20, 1, -H - 32), v3(-H + 80, 1, -H - 32), 10);
    coin_line(v3(H - 80, 1, H + 32), v3(H + 20, 1, H + 32), 10);
    /* invisible sea walls */
    add_boxc(WORLD_LIMIT, 0, -1000, 1000, 60, 1000, 0, 0, 0, K_INVIS);
    add_boxc(-1000, 0, -1000, -WORLD_LIMIT, 60, 1000, 0, 0, 0, K_INVIS);
    add_boxc(-1000, 0, WORLD_LIMIT, -AIR_X, 60, 1000, 0, 0, 0, K_INVIS);
    add_boxc(AIR_X, 0, WORLD_LIMIT, 1000, 60, 1000, 0, 0, 0, K_INVIS);
    add_boxc(-1000, 0, -1000, 1000, 60, -WORLD_LIMIT, 0, 0, 0, K_INVIS);
}

static void gen_roads_extras(void) {
    /* coins along road lanes */
    for (int axis = 0; axis < 2; axis++)
        for (int k = 0; k <= NBLK; k++)
            for (int l = 0; l < NBLK; l++) {
                if (rnd() > 0.28f) continue;
                float c = road_c(k), off = rnd() < 0.5f ? LANE : -LANE;
                float a = road_c(l) + 14, b = road_c(l + 1) - 14;
                if (axis == 0) coin_line(v3(c + off, 1, a), v3(c + off, 1, b), 6);
                else coin_line(v3(a, 1, c + off), v3(b, 1, c + off), 6);
            }
    /* parked cars along the curbs */
    int tries = 0;
    int wantParks = g_curMap == 0 ? 34 : 14;
    while (g_nparks < wantParks && g_nparks < MAX_PARKS && tries++ < 600) {
        int axis = ri(2), k = ri(NBLK + 1), l = ri(NBLK);
        float c = road_c(k), side = rnd() < 0.5f ? 1.0f : -1.0f;
        float along = road_c(l) + rr(18, 52);
        V3 p;
        float yaw;
        if (axis == 0) { p = v3(c + side * 6.2f, 0, along); yaw = side > 0 ? PI : 0; }
        else { p = v3(along, 0, c + side * 6.2f); yaw = side > 0 ? PI / 2 : -PI / 2; }
        int ok = 1;
        for (int i = 0; i < g_nparks; i++) if (dist2xz(p, g_parkSpots[i]) < 100) ok = 0;
        if (dist2xz(p, g_spots.homeCar) < 400) ok = 0;
        if (!ok) continue;
        g_parkSpots[g_nparks] = p;
        g_parkYaw[g_nparks] = yaw;
        g_nparks++;
    }
}

/* -------------------------------------------------------------- drawing */
static void gquad(float x0, float z0, float x1, float z1, float y, float r, float g, float b, float step, int checker) {
    int nx = (int)ceilf((x1 - x0) / step), nz = (int)ceilf((z1 - z0) / step);
    if (nx < 1) nx = 1;
    if (nz < 1) nz = 1;
    glBegin(GL_QUADS);
    for (int i = 0; i < nx; i++)
        for (int j = 0; j < nz; j++) {
            float xa = x0 + (x1 - x0) * i / nx, xb = x0 + (x1 - x0) * (i + 1) / nx;
            float za = z0 + (z1 - z0) * j / nz, zb = z0 + (z1 - z0) * (j + 1) / nz;
            float k = (checker && ((i + j) & 1)) ? 0.95f : 1.0f;
            glColor3f(r * k, g * k, b * k);
            glVertex3f(xa, y, za); glVertex3f(xb, y, za); glVertex3f(xb, y, zb); glVertex3f(xa, y, zb);
        }
    glEnd();
}
/* per-theme base colours: sand, grass, sidewalk */
static const float TH_SAND[3][3] = {{0.9f, 0.84f, 0.6f}, {0.95f, 0.82f, 0.56f}, {0.86f, 0.89f, 0.92f}};
static const float TH_GRASS[3][3] = {{0.44f, 0.7f, 0.33f}, {0.88f, 0.72f, 0.47f}, {0.93f, 0.95f, 0.98f}};
static const float TH_WALK[3][3] = {{0.74f, 0.73f, 0.7f}, {0.8f, 0.72f, 0.58f}, {0.8f, 0.82f, 0.86f}};
static void interior_color(int t, float *c) {
    switch (t) {
    case BT_RES: case BT_HOME: case BT_LAB: case BT_ADOBE: case BT_CABIN: case BT_FIRE:
        c[0] = 0.47f; c[1] = 0.73f; c[2] = 0.36f; break;
    case BT_PARK: case BT_TOWER: case BT_OASIS: case BT_LAKE: case BT_HOSPITAL: case BT_RESORT:
        c[0] = 0.38f; c[1] = 0.68f; c[2] = 0.3f; break;
    case BT_PLAZA: case BT_MALL: c[0] = 0.84f; c[1] = 0.79f; c[2] = 0.68f; break;
    case BT_STUNT: case BT_STADIUM: c[0] = 0.5f; c[1] = 0.5f; c[2] = 0.53f; break;
    case BT_DOCK: case BT_GAS: case BT_CASINO: c[0] = 0.46f; c[1] = 0.46f; c[2] = 0.48f; break;
    case BT_CLINIC: case BT_ARMORY: case BT_TUNER: case BT_GUNX: case BT_LODGE: c[0] = 0.55f; c[1] = 0.7f; c[2] = 0.42f; break;
    case BT_CANYON: c[0] = 0.85f; c[1] = 0.6f; c[2] = 0.4f; return;
    case BT_SKI: c[0] = 0.97f; c[1] = 0.98f; c[2] = 1.0f; return;
    default: c[0] = 0.66f; c[1] = 0.65f; c[2] = 0.63f; break;
    }
    /* green turns to sand in the desert and to snow in the mountains */
    if (theme && c[1] > c[0] + 0.1f) {
        float k = 0.95f + (c[1] - 0.68f) * 0.5f;
        for (int i = 0; i < 3; i++) c[i] = TH_GRASS[theme][i] * k;
    }
}
static void build_ground_list(void) {
    glNewList(dlGround, GL_COMPILE);
    gquad(-1400, -1400, 1400, 1400, 0, 0.18f, 0.45f, 0.72f, 100, 0);
    const float *sand = TH_SAND[theme], *grass = TH_GRASS[theme];
    float G = CITY_HALF + 38;
    gquad(-ISLAND, -ISLAND, ISLAND, ISLAND, 0, sand[0], sand[1], sand[2], 24, 0);
    gquad(-G, -G, G, G, 0, grass[0], grass[1], grass[2], 24, 1);
    gquad(-AIR_X - 8, G, AIR_X + 8, AIR_Z1 + 10, 0, sand[0], sand[1], sand[2], 24, 0);
    gquad(-AIR_X, G - 2, AIR_X, AIR_Z1, 0, grass[0], grass[1], grass[2], 24, 1);
    gquad(-300, AZ(340), 300, AZ(552), 0, 0.66f, 0.66f, 0.64f, 24, 0);
    gquad(-8, CITY_HALF, 8, AZ(340), 0, 0.27f, 0.27f, 0.3f, 12, 0);
    gquad(-272, AZ(468), 272, AZ(507), 0, 0.25f, 0.25f, 0.27f, 24, 0);
    glBegin(GL_QUADS);
    glColor3f(0.95f, 0.95f, 0.95f);
    for (float x = -240; x < 240; x += 20) { glVertex3f(x, 0, AZ(487)); glVertex3f(x + 10, 0, AZ(487)); glVertex3f(x + 10, 0, AZ(488)); glVertex3f(x, 0, AZ(488)); }
    for (int e = -1; e <= 1; e += 2)
        for (int i = 0; i < 8; i++) {
            float x = e * 262, z = AZ(472 + i * 4.3f);
            glVertex3f(x - 6, 0, z); glVertex3f(x + 6, 0, z); glVertex3f(x + 6, 0, z + 2); glVertex3f(x - 6, 0, z + 2);
        }
    glColor3f(0.95f, 0.8f, 0.2f);
    for (float x = -250; x < 250; x += 8) { glVertex3f(x, 0, AZ(440)); glVertex3f(x + 4, 0, AZ(440)); glVertex3f(x + 4, 0, AZ(440.5f)); glVertex3f(x, 0, AZ(440.5f)); }
    for (float z = CITY_HALF + 12; z < AZ(338); z += 6) { glVertex3f(-0.15f, 0, z); glVertex3f(0.15f, 0, z); glVertex3f(0.15f, 0, z + 3); glVertex3f(-0.15f, 0, z + 3); }
    glEnd();
    float rl = CITY_HALF + ROAD_HALF;
    for (int k = 0; k <= NBLK; k++) {
        float c = road_c(k);
        gquad(c - ROAD_HALF, -rl, c + ROAD_HALF, rl, 0, 0.27f, 0.27f, 0.3f, 24, 0);
        gquad(-rl, c - ROAD_HALF, rl, c + ROAD_HALF, 0, 0.27f, 0.27f, 0.3f, 24, 0);
    }
    for (int i = 0; i < NBLK; i++)
        for (int j = 0; j < NBLK; j++) {
            float x0, z0, x1, z1, c[3];
            block_rect(i, j, &x0, &z0, &x1, &z1);
            gquad(x0, z0, x1, z1, 0, TH_WALK[theme][0], TH_WALK[theme][1], TH_WALK[theme][2], 27, 0);
            interior_color(g_blockType[i][j], c);
            gquad(x0 + 3, z0 + 3, x1 - 3, z1 - 3, 0, c[0], c[1], c[2], 24, g_blockType[i][j] == BT_PLAZA);
        }
    for (int i = 0; i < npads; i++) gquad(pads[i].x0, pads[i].z0, pads[i].x1, pads[i].z1, 0, pads[i].col[0], pads[i].col[1], pads[i].col[2], 24, 0);
    /* helipad H marks */
    glBegin(GL_QUADS);
    glColor3f(0.95f, 0.95f, 0.95f);
    for (int i = 0; i < NUM_STANDS; i++) {
        int t = g_spots.standType[i];
        if (t != AIR_FIRST + 2 && t != AIR_FIRST + 4) continue;
        float x = g_spots.stand[i].x, z = g_spots.stand[i].z;
        glVertex3f(x - 2.5f, 0, z - 3); glVertex3f(x - 1.7f, 0, z - 3); glVertex3f(x - 1.7f, 0, z + 3); glVertex3f(x - 2.5f, 0, z + 3);
        glVertex3f(x + 1.7f, 0, z - 3); glVertex3f(x + 2.5f, 0, z - 3); glVertex3f(x + 2.5f, 0, z + 3); glVertex3f(x + 1.7f, 0, z + 3);
        glVertex3f(x - 1.7f, 0, z - 0.4f); glVertex3f(x + 1.7f, 0, z - 0.4f); glVertex3f(x + 1.7f, 0, z + 0.4f); glVertex3f(x - 1.7f, 0, z + 0.4f);
    }
    glEnd();
    for (int i = 0; i < nponds; i++) {
        glColor3f(0.75f, 0.7f, 0.55f);
        glBegin(GL_TRIANGLE_FAN);
        glVertex3f(ponds[i].x, 0, ponds[i].z);
        for (int s = 0; s <= 24; s++) { float a = 2 * PI * s / 24; glVertex3f(ponds[i].x + cosf(a) * (ponds[i].r + 1), 0, ponds[i].z + sinf(a) * (ponds[i].r + 1)); }
        glEnd();
        if (theme == 2) glColor3f(0.72f, 0.86f, 0.95f); else glColor3f(0.25f, 0.55f, 0.8f);
        glBegin(GL_TRIANGLE_FAN);
        glVertex3f(ponds[i].x, 0, ponds[i].z);
        for (int s = 0; s <= 24; s++) { float a = 2 * PI * s / 24; glVertex3f(ponds[i].x + cosf(a) * ponds[i].r, 0, ponds[i].z + sinf(a) * ponds[i].r); }
        glEnd();
    }
    /* lane markings and crosswalks */
    glBegin(GL_QUADS);
    for (int k = 0; k <= NBLK; k++)
        for (int l = 0; l < NBLK; l++) {
            float c = road_c(k);
            for (float s = road_c(l) + 12; s < road_c(l + 1) - 12; s += 6) {
                glColor3f(0.95f, 0.8f, 0.2f);
                glVertex3f(c - 0.15f, 0, s); glVertex3f(c + 0.15f, 0, s); glVertex3f(c + 0.15f, 0, s + 3); glVertex3f(c - 0.15f, 0, s + 3);
                glVertex3f(s, 0, c - 0.15f); glVertex3f(s + 3, 0, c - 0.15f); glVertex3f(s + 3, 0, c + 0.15f); glVertex3f(s, 0, c + 0.15f);
            }
        }
    glColor3f(0.92f, 0.92f, 0.9f);
    for (int k = 0; k <= NBLK; k++)
        for (int l = 0; l <= NBLK; l++) {
            float cx = road_c(k), cz = road_c(l);
            for (int side = -1; side <= 1; side += 2) {
                float zz = cz + side * (ROAD_HALF + 1.6f);
                float xx = cx + side * (ROAD_HALF + 1.6f);
                for (float s = -7; s < 7; s += 1.6f) {
                    glVertex3f(cx + s, 0, zz - 1.2f); glVertex3f(cx + s + 0.8f, 0, zz - 1.2f); glVertex3f(cx + s + 0.8f, 0, zz + 1.2f); glVertex3f(cx + s, 0, zz + 1.2f);
                    glVertex3f(xx - 1.2f, 0, cz + s); glVertex3f(xx + 1.2f, 0, cz + s); glVertex3f(xx + 1.2f, 0, cz + s + 0.8f); glVertex3f(xx - 1.2f, 0, cz + s + 0.8f);
                }
            }
        }
    glEnd();
    glEndList();
}

static void win_face(V3 o, V3 a, V3 n, float L, float H, int house) {
    glNormal3f(n.x, n.y, n.z);
    V3 off = vmul(n, 0.04f);
    if (house) {
        for (int c = 0; c < 2; c++) {
            float cx = L * (c + 1) / 3.0f;
            V3 p = vadd(vadd(o, vmul(a, cx - 0.7f)), off);
            V3 q = vadd(vadd(o, vmul(a, cx + 0.7f)), off);
            glColor3f(0.4f, 0.55f, 0.75f);
            glVertex3f(p.x, 1.5f + o.y, p.z); glVertex3f(q.x, 1.5f + o.y, q.z); glVertex3f(q.x, 2.9f + o.y, q.z); glVertex3f(p.x, 2.9f + o.y, p.z);
        }
        return;
    }
    int ncol = (int)(L / 3.2f);
    if (ncol < 1) return;
    float sp = L / ncol;
    for (float y = 1.6f; y + 1.9f < H - 0.6f; y += 3.4f)
        for (int c = 0; c < ncol; c++) {
            float cx = sp * (c + 0.5f);
            V3 p = vadd(vadd(o, vmul(a, cx - 0.75f)), off);
            V3 q = vadd(vadd(o, vmul(a, cx + 0.75f)), off);
            float r = rnd();
            if (r < 0.12f) glColor3f(0.95f, 0.88f, 0.55f);
            else if (r < 0.55f) glColor3f(0.32f, 0.42f, 0.56f);
            else glColor3f(0.48f, 0.63f, 0.8f);
            float y0 = o.y + y, y1 = o.y + y + 1.9f;
            glVertex3f(p.x, y0, p.z); glVertex3f(q.x, y0, q.z); glVertex3f(q.x, y1, q.z); glVertex3f(p.x, y1, p.z);
        }
}
static void draw_building(const Box *b) {
    int house = b->roof >= 1 && b->roof <= 4;
    glColor3fv(b->col);
    gl_box(b->mn.x, b->mn.y, b->mn.z, b->mx.x, b->mx.y, b->mx.z);
    float W = b->mx.x - b->mn.x, D = b->mx.z - b->mn.z, H = b->mx.y - b->mn.y;
    glBegin(GL_QUADS);
    win_face(v3(b->mn.x, b->mn.y, b->mx.z), v3(1, 0, 0), v3(0, 0, 1), W, H, house);
    win_face(v3(b->mn.x, b->mn.y, b->mn.z), v3(1, 0, 0), v3(0, 0, -1), W, H, house);
    win_face(v3(b->mx.x, b->mn.y, b->mn.z), v3(0, 0, 1), v3(1, 0, 0), D, H, house);
    win_face(v3(b->mn.x, b->mn.y, b->mn.z), v3(0, 0, 1), v3(-1, 0, 0), D, H, house);
    if (house) {
        float cx = (b->mn.x + b->mx.x) / 2;
        glNormal3f(0, 0, 1);
        glColor3f(0.45f, 0.28f, 0.16f);
        glVertex3f(cx - 0.6f, 0, b->mx.z + 0.05f); glVertex3f(cx + 0.6f, 0, b->mx.z + 0.05f);
        glVertex3f(cx + 0.6f, 2.2f, b->mx.z + 0.05f); glVertex3f(cx - 0.6f, 2.2f, b->mx.z + 0.05f);
    }
    glEnd();
    if (house) {
        static const float snowRoof[3] = {0.96f, 0.97f, 1.0f};
        const float *rc = theme == 2 ? snowRoof : PAL_ROOF[(b->roof - 1) & 3];
        glColor3fv(rc);
        float o = 0.5f, y0 = b->mx.y, y1 = b->mx.y + 2.4f;
        float x0 = b->mn.x - o, x1 = b->mx.x + o, z0 = b->mn.z - o, z1 = b->mx.z + o;
        glBegin(GL_QUADS);
        if (W >= D) {
            float zm = (b->mn.z + b->mx.z) / 2;
            V3 n1 = vnorm(v3(0, zm - z0, -(y1 - y0))); n1 = vnorm(v3(0, (zm - z0), -(y1 - y0)));
            glNormal3f(0, 0.7f, -0.7f);
            glVertex3f(x0, y0, z0); glVertex3f(x1, y0, z0); glVertex3f(x1, y1, zm); glVertex3f(x0, y1, zm);
            glNormal3f(0, 0.7f, 0.7f);
            glVertex3f(x0, y0, z1); glVertex3f(x0, y1, zm); glVertex3f(x1, y1, zm); glVertex3f(x1, y0, z1);
            (void)n1;
            glEnd();
            glColor3f(b->col[0] * 0.9f, b->col[1] * 0.9f, b->col[2] * 0.9f);
            glBegin(GL_TRIANGLES);
            glNormal3f(-1, 0, 0);
            glVertex3f(b->mn.x, y0, b->mn.z); glVertex3f(b->mn.x, y0, b->mx.z); glVertex3f(b->mn.x, y1 - 0.4f, zm);
            glNormal3f(1, 0, 0);
            glVertex3f(b->mx.x, y0, b->mn.z); glVertex3f(b->mx.x, y0, b->mx.z); glVertex3f(b->mx.x, y1 - 0.4f, zm);
            glEnd();
        } else {
            float xm = (b->mn.x + b->mx.x) / 2;
            glNormal3f(-0.7f, 0.7f, 0);
            glVertex3f(x0, y0, z0); glVertex3f(xm, y1, z0); glVertex3f(xm, y1, z1); glVertex3f(x0, y0, z1);
            glNormal3f(0.7f, 0.7f, 0);
            glVertex3f(x1, y0, z0); glVertex3f(x1, y0, z1); glVertex3f(xm, y1, z1); glVertex3f(xm, y1, z0);
            glEnd();
            glColor3f(b->col[0] * 0.9f, b->col[1] * 0.9f, b->col[2] * 0.9f);
            glBegin(GL_TRIANGLES);
            glNormal3f(0, 0, -1);
            glVertex3f(b->mn.x, y0, b->mn.z); glVertex3f(b->mx.x, y0, b->mn.z); glVertex3f(xm, y1 - 0.4f, b->mn.z);
            glNormal3f(0, 0, 1);
            glVertex3f(b->mn.x, y0, b->mx.z); glVertex3f(b->mx.x, y0, b->mx.z); glVertex3f(xm, y1 - 0.4f, b->mx.z);
            glEnd();
        }
    } else {
        glColor3f(b->col[0] * 0.7f, b->col[1] * 0.7f, b->col[2] * 0.7f);
        gl_box(b->mn.x - 0.15f, b->mx.y - 0.35f, b->mn.z - 0.15f, b->mx.x + 0.15f, b->mx.y + 0.02f, b->mn.z + 0.3f);
        gl_box(b->mn.x - 0.15f, b->mx.y - 0.35f, b->mx.z - 0.3f, b->mx.x + 0.15f, b->mx.y + 0.02f, b->mx.z + 0.15f);
        gl_box(b->mn.x - 0.15f, b->mx.y - 0.35f, b->mn.z, b->mn.x + 0.3f, b->mx.y + 0.02f, b->mx.z);
        gl_box(b->mx.x - 0.3f, b->mx.y - 0.35f, b->mn.z, b->mx.x + 0.15f, b->mx.y + 0.02f, b->mx.z);
        if (H > 15 && b->roof == 0) {
            glColor3f(0.6f, 0.6f, 0.62f);
            float cx = (b->mn.x + b->mx.x) / 2 + rr(-3, 3), cz = (b->mn.z + b->mx.z) / 2 + rr(-3, 3);
            gl_box(cx - 1.5f, b->mx.y, cz - 1, cx + 1.5f, b->mx.y + 1.4f, cz + 1);
        }
        if (b->roof == 9) {
            float cx = (b->mn.x + b->mx.x) / 2;
            glColor3f(0.55f, 0.25f, 0.2f);
            gl_box(cx - 0.1f, b->mx.y, b->mn.z + 0.4f, cx + 0.1f, b->mx.y + 5, b->mn.z + 0.6f);
            glColor3f(1, 0.3f, 0.2f);
            gl_box(cx + 0.1f, b->mx.y + 3.8f, b->mn.z + 0.45f, cx + 2.0f, b->mx.y + 4.9f, b->mn.z + 0.55f);
        }
    }
}
static void draw_ramp(const Ramp *r) {
    float x0 = r->mn.x, x1 = r->mx.x, z0 = r->mn.z, z1 = r->mx.z;
    float h00 = ramp_h(r, x0, z0), h10 = ramp_h(r, x1, z0), h11 = ramp_h(r, x1, z1), h01 = ramp_h(r, x0, z1);
    int seg = 8;
    glBegin(GL_QUADS);
    for (int i = 0; i < seg; i++) {
        float t0 = (float)i / seg, t1 = (float)(i + 1) / seg;
        if (i & 1) glColor3f(0.2f, 0.2f, 0.22f); else glColor3fv(r->col);
        V3 a, b, c, d;
        if (r->dir < 2) {
            float xa = lerpf(x0, x1, t0), xb = lerpf(x0, x1, t1);
            a = v3(xa, lerpf(h00, h10, t0), z0); b = v3(xb, lerpf(h00, h10, t1), z0);
            c = v3(xb, lerpf(h01, h11, t1), z1); d = v3(xa, lerpf(h01, h11, t0), z1);
        } else {
            float za = lerpf(z0, z1, t0), zb = lerpf(z0, z1, t1);
            a = v3(x0, lerpf(h00, h01, t0), za); b = v3(x1, lerpf(h10, h11, t0), za);
            c = v3(x1, lerpf(h10, h11, t1), zb); d = v3(x0, lerpf(h00, h01, t1), zb);
        }
        V3 n = vnorm(vcross(vsub(c, a), vsub(b, a)));
        if (n.y < 0) n = vmul(n, -1);
        glNormal3f(n.x, n.y, n.z);
        glVertex3f(a.x, a.y, a.z); glVertex3f(b.x, b.y, b.z); glVertex3f(c.x, c.y, c.z); glVertex3f(d.x, d.y, d.z);
    }
    glColor3f(0.55f, 0.5f, 0.45f);
    glNormal3f(0, 0, -1);
    glVertex3f(x0, 0, z0); glVertex3f(x1, 0, z0); glVertex3f(x1, h10, z0); glVertex3f(x0, h00, z0);
    glNormal3f(0, 0, 1);
    glVertex3f(x0, 0, z1); glVertex3f(x1, 0, z1); glVertex3f(x1, h11, z1); glVertex3f(x0, h01, z1);
    glNormal3f(-1, 0, 0);
    glVertex3f(x0, 0, z0); glVertex3f(x0, 0, z1); glVertex3f(x0, h01, z1); glVertex3f(x0, h00, z0);
    glNormal3f(1, 0, 0);
    glVertex3f(x1, 0, z0); glVertex3f(x1, 0, z1); glVertex3f(x1, h11, z1); glVertex3f(x1, h10, z0);
    glEnd();
}
static void draw_deco(const Deco *d) {
    float x = d->p.x, y = d->p.y, z = d->p.z, s = d->s;
    switch (d->kind) {
    case DECO_TREE:
        glColor3fv(d->col);
        gl_frustum_y(x, y + 1.8f * s, z, 1.0f * s, 1.9f * s, 0.9f * s, 7);
        glColor3f(d->col[0] * 1.1f, d->col[1] * 1.1f, d->col[2] * 1.1f);
        gl_frustum_y(x, y + 2.7f * s, z, 1.9f * s, 1.5f * s, 1.0f * s, 7);
        gl_frustum_y(x, y + 3.7f * s, z, 1.5f * s, 0.4f * s, 0.9f * s, 7);
        break;
    case DECO_PINE:
        glColor3f(d->col[0] * 0.8f, d->col[1] * 0.85f, d->col[2] * 0.9f);
        gl_frustum_y(x, y + 1.2f * s, z, 2.0f * s, 0.0f, 2.2f * s, 7);
        gl_frustum_y(x, y + 2.6f * s, z, 1.5f * s, 0.0f, 2.0f * s, 7);
        gl_frustum_y(x, y + 3.9f * s, z, 1.0f * s, 0.0f, 1.6f * s, 7);
        break;
    case DECO_PALM: {
        glPushMatrix();
        glTranslatef(x, y, z);
        glRotatef(d->yaw * DEG, 0, 1, 0);
        glColor3f(0.55f, 0.4f, 0.25f);
        for (int i = 0; i < 6; i++) gl_box(-0.18f + i * 0.08f * s, i * 0.9f * s, -0.18f, 0.18f + i * 0.08f * s, (i + 1) * 0.9f * s, 0.18f);
        glTranslatef(0.48f * s, 5.4f * s, 0);
        glColor3f(0.25f, 0.6f, 0.22f);
        for (int i = 0; i < 6; i++) {
            glPushMatrix();
            glRotatef(i * 60.0f, 0, 1, 0);
            glRotatef(18, 1, 0, 0);
            gl_box(-0.35f, -0.08f, 0, 0.35f, 0.04f, 2.6f * s);
            glPopMatrix();
        }
        glPopMatrix();
    } break;
    case DECO_CACTUS:
        glColor3fv(d->col);
        gl_box(x - 0.3f, 0, z - 0.3f, x + 0.3f, 2.6f * s, z + 0.3f);
        gl_box(x + 0.3f, 1.0f * s, z - 0.2f, x + 0.9f, 1.3f * s, z + 0.2f);
        gl_box(x + 0.6f, 1.0f * s, z - 0.2f, x + 0.9f, 2.0f * s, z + 0.2f);
        gl_box(x - 0.8f, 1.4f * s, z - 0.2f, x - 0.3f, 1.7f * s, z + 0.2f);
        gl_box(x - 0.8f, 1.4f * s, z - 0.2f, x - 0.5f, 2.2f * s, z + 0.2f);
        break;
    case DECO_SNOWPINE:
        glColor3f(0.18f, 0.38f, 0.28f);
        gl_frustum_y(x, y + 1.2f * s, z, 2.0f * s, 0.0f, 2.2f * s, 7);
        gl_frustum_y(x, y + 2.6f * s, z, 1.5f * s, 0.0f, 2.0f * s, 7);
        glColor3f(0.95f, 0.97f, 1.0f);
        gl_frustum_y(x, y + 2.3f * s, z, 1.2f * s, 0.0f, 1.0f * s, 7);
        gl_frustum_y(x, y + 3.9f * s, z, 1.0f * s, 0.0f, 1.6f * s, 7);
        break;
    case DECO_LAMP:
        glColor3f(0.3f, 0.32f, 0.35f);
        gl_box(x - 0.1f, 0, z - 0.1f, x + 0.1f, 5.5f, z + 0.1f);
        glColor3f(1, 0.95f, 0.75f);
        gl_box(x - 0.3f, 5.3f, z - 0.3f, x + 0.3f, 5.7f, z + 0.3f);
        break;
    case DECO_BENCH:
        glPushMatrix();
        glTranslatef(x, y, z);
        glRotatef(d->yaw * DEG, 0, 1, 0);
        glColor3f(0.6f, 0.4f, 0.22f);
        gl_box(-1, 0.45f, -0.3f, 1, 0.55f, 0.3f);
        gl_box(-1, 0.55f, -0.35f, 1, 1.0f, -0.25f);
        glColor3f(0.2f, 0.2f, 0.22f);
        gl_box(-0.9f, 0, -0.25f, -0.8f, 0.45f, 0.25f);
        gl_box(0.8f, 0, -0.25f, 0.9f, 0.45f, 0.25f);
        glPopMatrix();
        break;
    case DECO_FOUNTAIN:
        glColor3f(0.75f, 0.74f, 0.7f);
        gl_frustum_y(x, 0, z, s, s * 1.05f, 0.85f, 14);
        glColor3f(0.3f, 0.6f, 0.9f);
        gl_disc_y(x, 0.87f, z, s * 0.92f, 14);
        glColor3f(0.75f, 0.74f, 0.7f);
        gl_frustum_y(x, 0.85f, z, 0.4f, 0.3f, 1.8f, 8);
        gl_frustum_y(x, 2.6f, z, 0.3f, 1.2f, 0.4f, 10);
        break;
    case DECO_CLOCK:
        for (int f = 0; f < 4; f++) {
            float a = f * PI / 2;
            V3 n = v3(sinf(a), 0, cosf(a));
            V3 c = v3(x + n.x * (s + 0.05f), y, z + n.z * (s + 0.05f));
            glPushMatrix();
            glTranslatef(c.x, c.y, c.z);
            glRotatef(a * DEG, 0, 1, 0);
            glColor3f(0.97f, 0.95f, 0.85f);
            gl_box(-2, -2, 0, 2, 2, 0.08f);
            glColor3f(0.1f, 0.1f, 0.1f);
            gl_box(-0.12f, -0.1f, 0.08f, 0.12f, 1.6f, 0.14f);
            gl_box(-0.1f, -0.12f, 0.08f, 1.1f, 0.12f, 0.14f);
            glPopMatrix();
        }
        break;
    case DECO_DOME:
        glColor3f(0.7f, 0.85f, 0.95f);
        gl_frustum_y(x, y, z, s, s * 0.92f, s * 0.35f, 14);
        gl_frustum_y(x, y + s * 0.35f, z, s * 0.92f, s * 0.7f, s * 0.3f, 14);
        gl_frustum_y(x, y + s * 0.65f, z, s * 0.7f, s * 0.3f, s * 0.25f, 14);
        gl_frustum_y(x, y + s * 0.9f, z, s * 0.3f, 0.0f, s * 0.1f, 14);
        break;
    case DECO_ANTENNA:
        glColor3f(0.5f, 0.5f, 0.55f);
        gl_box(x - 0.08f, y, z - 0.08f, x + 0.08f, y + 7, z + 0.08f);
        glColor3f(1, 0.2f, 0.2f);
        gl_cube(x, y + 7.2f, z, 0.3f, 0.3f, 0.3f);
        break;
    case DECO_AWNING: {
        glPushMatrix();
        glTranslatef(x, y, z);
        glRotatef(d->yaw * DEG, 0, 1, 0);
        int n = 8;
        glBegin(GL_QUADS);
        glNormal3f(0, 0.8f, 0.6f);
        for (int i = 0; i < n; i++) {
            if (i & 1) glColor3f(1, 1, 1); else glColor3f(0.85f, 0.2f, 0.2f);
            float a = -s / 2 + s * i / n, b = -s / 2 + s * (i + 1) / n;
            glVertex3f(a, 0.8f, 0); glVertex3f(b, 0.8f, 0); glVertex3f(b, 0, 1.8f); glVertex3f(a, 0, 1.8f);
        }
        glEnd();
        glPopMatrix();
    } break;
    case DECO_SIGN: {
        V3 f = fwd_of(d->yaw);
        V3 rt = vmul(right_of(d->yaw), -1);
        float w = text3d_width(d->text, s);
        V3 c = vadd(d->p, vmul(f, 0.08f));
        glColor3f(0.12f, 0.12f, 0.16f);
        V3 a0 = vsub(c, vmul(rt, w / 2 + s * 2));
        V3 a1 = vadd(c, vmul(rt, w / 2 + s * 2));
        glBegin(GL_QUADS);
        glNormal3f(f.x, f.y, f.z);
        glVertex3f(a0.x, y - s * 2, a0.z); glVertex3f(a1.x, y - s * 2, a1.z);
        glVertex3f(a1.x, y + s * 9, a1.z); glVertex3f(a0.x, y + s * 9, a0.z);
        glEnd();
        glColor3fv(d->col);
        V3 o = vadd(vsub(c, vmul(rt, w / 2)), vmul(f, 0.05f));
        text3d(d->text, o, rt, v3(0, 1, 0), s);
    } break;
    }
}
static void build_static_list(void) {
    unsigned save = rs;
    rs = 777;
    glNewList(dlStatic, GL_COMPILE);
    for (int i = 0; i < g_nboxes; i++) {
        Box *b = &g_boxes[i];
        switch (b->kind) {
        case K_INVIS: break;
        case K_BUILDING: draw_building(b); break;
        case K_TRUNK:
            glColor3fv(b->col);
            gl_box(b->mn.x + 0.08f, 0, b->mn.z + 0.08f, b->mx.x - 0.08f, b->mx.y, b->mx.z - 0.08f);
            break;
        case K_CONTAINER: {
            glColor3fv(b->col);
            gl_box(b->mn.x, b->mn.y, b->mn.z, b->mx.x, b->mx.y, b->mx.z);
            glColor3f(b->col[0] * 0.75f, b->col[1] * 0.75f, b->col[2] * 0.75f);
            int alongX = (b->mx.x - b->mn.x) > (b->mx.z - b->mn.z);
            float L = alongX ? b->mx.x - b->mn.x : b->mx.z - b->mn.z;
            for (float s = 0.3f; s < L - 0.2f; s += 0.6f) {
                if (alongX) {
                    gl_box(b->mn.x + s, b->mn.y + 0.1f, b->mn.z - 0.04f, b->mn.x + s + 0.15f, b->mx.y - 0.1f, b->mx.z + 0.04f);
                } else {
                    gl_box(b->mn.x - 0.04f, b->mn.y + 0.1f, b->mn.z + s, b->mx.x + 0.04f, b->mx.y - 0.1f, b->mn.z + s + 0.15f);
                }
            }
        } break;
        case K_PLATFORM:
            glColor3fv(b->col);
            gl_box(b->mn.x, b->mn.y, b->mn.z, b->mx.x, b->mx.y, b->mx.z);
            glColor3f(1, 0.85f, 0.2f);
            gl_box(b->mn.x + 0.1f, b->mx.y, b->mn.z + 0.1f, b->mx.x - 0.1f, b->mx.y + 0.03f, b->mn.z + 0.35f);
            gl_box(b->mn.x + 0.1f, b->mx.y, b->mx.z - 0.35f, b->mx.x - 0.1f, b->mx.y + 0.03f, b->mx.z - 0.1f);
            break;
        default:
            glColor3fv(b->col);
            gl_box(b->mn.x, b->mn.y, b->mn.z, b->mx.x, b->mx.y, b->mx.z);
            if (b->kind == K_STEP) {
                glColor3f(b->col[0] * 1.15f, b->col[1] * 1.15f, b->col[2] * 1.1f);
                gl_box(b->mn.x, b->mx.y - 0.05f, b->mn.z, b->mx.x, b->mx.y + 0.02f, b->mx.z);
            }
            break;
        }
    }
    for (int i = 0; i < g_nramps; i++) draw_ramp(&g_ramps[i]);
    for (int i = 0; i < g_ndecos; i++) draw_deco(&g_decos[i]);
    glEndList();
    rs = save;
}
static void build_map_list(void) {
    glNewList(dlMap, GL_COMPILE);
    glBegin(GL_QUADS);
#define MQ(x0, z0, x1, z1) glVertex2f(x0, z0), glVertex2f(x1, z0), glVertex2f(x1, z1), glVertex2f(x0, z1)
    glColor3f(0.2f, 0.42f, 0.65f); MQ(-1400, -1400, 1400, 1400);
    {
        const float *sd = TH_SAND[theme], *gr = TH_GRASS[theme];
        float G = CITY_HALF + 38, R = CITY_HALF + 8;
        glColor3f(sd[0] * 0.95f, sd[1] * 0.95f, sd[2] * 0.95f); MQ(-ISLAND, -ISLAND, ISLAND, ISLAND);
        glColor3f(gr[0] * 0.88f, gr[1] * 0.88f, gr[2] * 0.88f); MQ(-G, -G, G, G);
        glColor3f(sd[0] * 0.95f, sd[1] * 0.95f, sd[2] * 0.95f); MQ(-AIR_X - 8, G, AIR_X + 8, AIR_Z1 + 10);
        glColor3f(gr[0] * 0.88f, gr[1] * 0.88f, gr[2] * 0.88f); MQ(-AIR_X, G - 2, AIR_X, AIR_Z1);
        glColor3f(0.6f, 0.6f, 0.6f); MQ(-300, AZ(340), 300, AZ(552));
        glColor3f(0.22f, 0.22f, 0.25f); MQ(-8, CITY_HALF, 8, AZ(340));
        glColor3f(0.2f, 0.2f, 0.22f); MQ(-272, AZ(468), 272, AZ(507));
        glColor3f(0.22f, 0.22f, 0.25f); MQ(-R, -R, R, R);
    }
    for (int i = 0; i < NBLK; i++)
        for (int j = 0; j < NBLK; j++) {
            float x0, z0, x1, z1, c[3];
            block_rect(i, j, &x0, &z0, &x1, &z1);
            interior_color(g_blockType[i][j], c);
            glColor3f(c[0] * 0.9f, c[1] * 0.9f, c[2] * 0.9f);
            MQ(x0, z0, x1, z1);
        }
    for (int i = 0; i < g_nboxes; i++) {
        Box *b = &g_boxes[i];
        if (b->kind == K_INVIS || b->kind == K_TRUNK) continue;
        float k = b->kind == K_BUILDING ? 0.55f : 0.75f;
        glColor3f(b->col[0] * k, b->col[1] * k, b->col[2] * k);
        MQ(b->mn.x, b->mn.z, b->mx.x, b->mx.z);
    }
    glColor3f(0.95f, 0.75f, 0.2f);
    for (int i = 0; i < g_nramps; i++) MQ(g_ramps[i].mn.x, g_ramps[i].mn.z, g_ramps[i].mx.x, g_ramps[i].mx.z);
    glColor3f(0.25f, 0.55f, 0.8f);
    for (int i = 0; i < nponds; i++) MQ(ponds[i].x - ponds[i].r * 0.8f, ponds[i].z - ponds[i].r * 0.8f, ponds[i].x + ponds[i].r * 0.8f, ponds[i].z + ponds[i].r * 0.8f);
#undef MQ
    glEnd();
    glEndList();
}

static void grid_build(void);
void world_build(int map) {
    static const unsigned seeds[NUM_MAPS] = {20240611u, 777001u, 424242u};
    static const float skyT[3][3] = {{0.3f, 0.55f, 0.92f}, {0.32f, 0.55f, 0.9f}, {0.5f, 0.62f, 0.8f}};
    static const float skyH[3][3] = {{0.78f, 0.88f, 0.98f}, {0.98f, 0.86f, 0.68f}, {0.88f, 0.9f, 0.95f}};
    g_curMap = map;
    theme = map;
    g_nblk = map == 0 ? 12 : 6;
    g_cityHalf = CELL * g_nblk * 0.5f;
    memcpy(g_skyTop, skyT[map], sizeof g_skyTop);
    memcpy(g_skyHz, skyH[map], sizeof g_skyHz);
    rs = seeds[map];
    g_nboxes = g_nramps = g_ncrates = g_ncoins = g_ndecos = g_nparks = 0;
    nponds = npads = 0;
    memset(g_crates, 0, sizeof g_crates);
    memset(g_coins, 0, sizeof g_coins);
    memset(g_blockType, 0, sizeof g_blockType);
    /* anything a map does not have lives far out at sea */
    memset(&g_spots, 0, sizeof g_spots);
    g_spots.shop = g_spots.armory = g_spots.hangar = g_spots.tuner = g_spots.clinic = g_spots.police = FAR_AWAY;
    g_spots.gunx = g_spots.travel = g_spots.towerTop = g_spots.home = g_spots.homeCar = FAR_AWAY;
    for (int i = 0; i < NUM_MISSIONS; i++) g_spots.giver[i] = FAR_AWAY;
    for (int i = 0; i < 12; i++) g_spots.range[i] = FAR_AWAY;
    for (int i = 0; i < NUM_GIZMOS; i++) g_gizmoPos[i] = FAR_AWAY;
    assign_types();
    for (int i = 0; i < NBLK; i++)
        for (int j = 0; j < NBLK; j++) {
            float x0, z0, x1, z1;
            block_rect(i, j, &x0, &z0, &x1, &z1);
            float ax0 = x0 + 3, az0 = z0 + 3;
            switch (g_blockType[i][j]) {
            case BT_RES: gen_res(ax0, az0); break;
            case BT_COM: gen_com(ax0, az0); break;
            case BT_DOWN: gen_down(ax0, az0); break;
            case BT_PARK: gen_park(x0 + 3, z0 + 3, x1 - 3, z1 - 3, 1); break;
            case BT_TOWER: gen_tower(x0, z0, x1, z1); break;
            case BT_STUNT: gen_stunt(x0, z0); break;
            case BT_PLAZA: if (map == 0) gen_plaza(x0, z0); else gen_park(x0 + 3, z0 + 3, x1 - 3, z1 - 3, 0); break;
            case BT_HOME: gen_home(x0, z0); break;
            case BT_DOCK: gen_dock(x0, z0); break;
            case BT_PIZZA: gen_pizza(x0, z0); break;
            case BT_LAB: gen_lab(x0, z0); break;
            case BT_ARMORY: gen_armory(x0, z0); break;
            case BT_CLINIC: gen_clinic(x0, z0); break;
            case BT_TUNER: gen_tuner(x0, z0); break;
            case BT_GAS: gen_gas(x0, z0, 0, 1); break;
            case BT_HOSPITAL: gen_hospital(x0, z0); break;
            case BT_MALL: gen_mall(x0, z0); break;
            case BT_STADIUM: gen_stadium(x0, z0); break;
            case BT_FIRE: gen_fire(x0, z0); break;
            case BT_BUILD: gen_build(x0, z0); break;
            case BT_ADOBE: gen_adobe(ax0, az0); break;
            case BT_RESORT: gen_resort(x0, z0); break;
            case BT_CASINO: gen_casino(x0, z0); break;
            case BT_CANYON: gen_canyon(x0, z0); break;
            case BT_OASIS: gen_oasis(x0, z0); break;
            case BT_GUNX: gun_store(x0, z0, 18, 10); add_pad(x0 + 40, z0 + 4, x0 + 52, z0 + 50, 0.45f, 0.45f, 0.47f); add_tree(x0 + 6, z0 + 40); add_tree(x0 + 20, z0 + 44); break;
            case BT_CABIN: gen_cabin(ax0, az0); break;
            case BT_LODGE: gen_lodge(x0, z0); break;
            case BT_SKI: gen_ski(x0, z0); break;
            case BT_LAKE: gen_lake(x0, z0); break;
            }
            add_deco(DECO_LAMP, x0 + 1, 0, z0 + 1, 1);
            add_deco(DECO_LAMP, x1 - 1, 0, z0 + 1, 1);
            add_deco(DECO_LAMP, x0 + 1, 0, z1 - 1, 1);
            add_deco(DECO_LAMP, x1 - 1, 0, z1 - 1, 1);
        }
    if (map == 0) {
        float x0, z0, x1, z1;
        block_rect(5 + BO, 2 + BO, &x0, &z0, &x1, &z1);
        g_gizmoPos[6] = v3((x0 + x1) / 2, 1.2f, (z0 + z1) / 2);
    } else {
        /* no home on the other islands: you arrive at the airport */
        g_spots.home = v3(-60, 0, AZ(392));
        g_spots.homeYaw = 0;
        g_spots.homeCar = v3(-20, 0, AZ(398));
        g_spots.homeCarYaw = PI / 2;
    }
    gen_outer();
    gen_airport();
    gen_roads_extras();
    if (map == 0) {
        /* mission givers */
        block_edge_spot(0 + BO, 0 + BO, 0, -8, &g_spots.giver[0], &g_spots.giverYaw[0]);
        block_edge_spot(1 + BO, 6 + BO, 1, -3, &g_spots.giver[1], &g_spots.giverYaw[1]);
        block_edge_spot(6 + BO, 6 + BO, 2, 6, &g_spots.giver[2], &g_spots.giverYaw[2]);
        block_edge_spot(7 + BO, 0 + BO, 3, 10, &g_spots.giver[3], &g_spots.giverYaw[3]);
        block_edge_spot(3 + BO, 4 + BO, 3, -8, &g_spots.giver[4], &g_spots.giverYaw[4]);
        block_edge_spot(3 + BO, 3 + BO, 2, 8, &g_spots.giver[5], &g_spots.giverYaw[5]);
        block_edge_spot(4 + BO, 3 + BO, 3, -18, &g_spots.giver[6], &g_spots.giverYaw[6]);
        block_edge_spot(4 + BO, 3 + BO, 3, 12, &g_spots.giver[7], &g_spots.giverYaw[7]);
        g_spots.giver[8] = v3(-80, 0, AZ(375)); g_spots.giverYaw[8] = 0;
        g_spots.giver[9] = vadd(g_spots.armory, v3(5, 0, 0)); g_spots.giverYaw[9] = PI;
        block_edge_spot(3 + BO, 2 + BO, 2, 0, &g_spots.giver[10], &g_spots.giverYaw[10]);
        g_spots.giver[11] = g_spots.giver[8]; g_spots.giverYaw[11] = 0;
        g_spots.giver[12] = v3(-5, 0, AZ(362)); g_spots.giverYaw[12] = 0;
        g_spots.giver[13] = vadd(g_spots.police, v3(4, 0, 0)); g_spots.giverYaw[13] = 0;
    }
    if (g_spots.police.x > 50000 && g_spots.nhosp) g_spots.police = vadd(g_spots.hosp[0], v3(10, 0, 0));

    if (dlGround) glDeleteLists(dlGround, 3);
    dlGround = glGenLists(3);
    dlStatic = dlGround + 1;
    dlMap = dlGround + 2;
    build_ground_list();
    build_static_list();
    build_map_list();
    grid_build();
}

void world_reset_pickups(void) {
    for (int i = 0; i < g_ncoins; i++) { g_coins[i].alive = 1; g_coins[i].respawn = 0; }
    for (int i = g_ncoins; i < MAX_COINS; i++) g_coins[i].alive = 0;
    for (int i = 0; i < g_ncrates; i++) { g_crates[i].alive = 1; g_crates[i].pos = crateHome[i]; g_crates[i].vy = 0; g_crates[i].respawn = 0; }
    for (int i = MISSION_CRATE_BASE; i < MAX_CRATES; i++) g_crates[i].alive = 0;
}

void world_draw_ground(void) { glCallList(dlGround); }
void world_draw_static(void) { glCallList(dlStatic); }
void world_draw_map(void) { glCallList(dlMap); }

static void draw_gizmo(V3 p, float t) {
    glPushMatrix();
    glTranslatef(p.x, p.y + sinf(t * 2) * 0.2f, p.z);
    glRotatef(t * 90, 0, 1, 0);
    glRotatef(20, 1, 0, 0);
    glColor3f(0.65f, 0.3f, 0.95f);
    gl_cube(0, 0, 0, 0.35f, 0.35f, 0.15f);
    glColor3f(1, 0.85f, 0.25f);
    for (int i = 0; i < 8; i++) {
        glPushMatrix();
        glRotatef(i * 45.0f, 0, 0, 1);
        gl_cube(0, 0.5f, 0, 0.12f, 0.16f, 0.12f);
        glPopMatrix();
    }
    glPopMatrix();
}
void world_draw_dynamic(float t, V3 cam) {
    for (int i = 0; i < MAX_CRATES; i++) {
        Crate *c = &g_crates[i];
        if (!c->alive || dist2xz(c->pos, cam) > 220 * 220) continue;
        float x = c->pos.x, y = c->pos.y, z = c->pos.z;
        if (c->mission) {
            float k = 0.75f + 0.25f * sinf(t * 6);
            glColor3f(1.0f * k, 0.45f * k, 0.1f);
        } else glColor3f(0.72f, 0.5f, 0.26f);
        gl_box(x - CRATE_H, y, z - CRATE_H, x + CRATE_H, y + 1.2f, z + CRATE_H);
        glColor3f(0.45f, 0.3f, 0.15f);
        float e = CRATE_H + 0.02f;
        gl_box(x - e, y, z - e, x + e, y + 0.15f, z + e);
        gl_box(x - e, y + 1.05f, z - e, x + e, y + 1.2f, z + e);
        gl_box(x - e, y, z - e, x - e + 0.15f, y + 1.2f, z - e + 0.15f);
        gl_box(x + e - 0.15f, y, z + e - 0.15f, x + e, y + 1.2f, z + e);
        gl_box(x + e - 0.15f, y, z - e, x + e, y + 1.2f, z - e + 0.15f);
        gl_box(x - e, y, z + e - 0.15f, x - e + 0.15f, y + 1.2f, z + e);
    }
    for (int i = 0; i < MAX_COINS; i++) {
        Coin *c = &g_coins[i];
        if (!c->alive || dist2xz(c->pos, cam) > 170 * 170) continue;
        glPushMatrix();
        glTranslatef(c->pos.x, c->pos.y + (c->dyn ? 0 : sinf(t * 3 + i) * 0.15f), c->pos.z);
        glRotatef(t * 200 + i * 37.0f, 0, 1, 0);
        glColor3f(1.0f, 0.82f, 0.15f);
        gl_cyl_x(0, 0, 0, 0.45f, 0.06f, 10);
        glColor3f(0.85f, 0.62f, 0.08f);
        gl_cyl_x(0, 0, 0, 0.28f, 0.075f, 8);
        glPopMatrix();
    }
    for (int i = 0; i < NUM_GIZMOS; i++)
        if (!(g_prog.gizmos & (1u << i))) draw_gizmo(g_gizmoPos[i], t + i);
}

/* ----------------------------------------------------------- spatial grid */
/* boxes are bucketed into 16 m cells so collision queries only test nearby boxes */
#define GRID_N 128
#define GRID_CS 16.0f
#define GRID_MIN -1024.0f
static int gStart[GRID_N * GRID_N + 1];
static int *gItems;
static unsigned gStamp[MAX_BOXES], gStampCur;
static int gcell(float v) { int c = (int)floorf((v - GRID_MIN) / GRID_CS); return c < 0 ? 0 : c >= GRID_N ? GRID_N - 1 : c; }
static void grid_build(void) {
    static int cnt[GRID_N * GRID_N];
    memset(cnt, 0, sizeof cnt);
    for (int pass = 0; pass < 2; pass++) {
        if (pass == 1) {
            gStart[0] = 0;
            for (int c = 0; c < GRID_N * GRID_N; c++) gStart[c + 1] = gStart[c] + cnt[c];
            free(gItems);
            gItems = malloc(sizeof(int) * (size_t)(gStart[GRID_N * GRID_N] + 1));
            memset(cnt, 0, sizeof cnt);
        }
        for (int i = 0; i < g_nboxes; i++) {
            const Box *b = &g_boxes[i];
            int x0 = gcell(b->mn.x), x1 = gcell(b->mx.x), z0 = gcell(b->mn.z), z1 = gcell(b->mx.z);
            for (int x = x0; x <= x1; x++)
                for (int z = z0; z <= z1; z++) {
                    int c = z * GRID_N + x;
                    if (pass == 1) gItems[gStart[c] + cnt[c]] = i;
                    cnt[c]++;
                }
        }
    }
    memset(gStamp, 0, sizeof gStamp);
    gStampCur = 0;
}
#define GRID_POINT(x, z, i, ...) do { \
        int c_ = gcell(z) * GRID_N + gcell(x); \
        for (int k_ = gStart[c_]; k_ < gStart[c_ + 1]; k_++) { \
            int i = gItems[k_]; \
            __VA_ARGS__ \
        } \
    } while (0)
#define GRID_AREA(xa, za, xb, zb, i, ...) do { \
        if (++gStampCur == 0) { \
            memset(gStamp, 0, sizeof gStamp); \
            gStampCur = 1; \
        } \
        int x0_ = gcell(xa), x1_ = gcell(xb), z0_ = gcell(za), z1_ = gcell(zb); \
        for (int gz_ = z0_; gz_ <= z1_; gz_++) \
            for (int gx_ = x0_; gx_ <= x1_; gx_++) { \
                int c_ = gz_ * GRID_N + gx_; \
                for (int k_ = gStart[c_]; k_ < gStart[c_ + 1]; k_++) { \
                    int i = gItems[k_]; \
                    if (gStamp[i] == gStampCur) continue; \
                    gStamp[i] = gStampCur; \
                    __VA_ARGS__ \
                } \
            } \
    } while (0)

/* ----------------------------------------------------------- collision */
float ramp_h(const Ramp *r, float x, float z) {
    float t;
    switch (r->dir) {
    case 0: t = (x - r->mn.x) / (r->mx.x - r->mn.x); break;
    case 1: t = (r->mx.x - x) / (r->mx.x - r->mn.x); break;
    case 2: t = (z - r->mn.z) / (r->mx.z - r->mn.z); break;
    default: t = (r->mx.z - z) / (r->mx.z - r->mn.z); break;
    }
    return clampf(t, 0, 1) * r->h;
}
float ground_height_excl(float x, float z, float y, float step, int excl) {
    float lim = y + step, g = 0;
    GRID_POINT(x, z, i, {
        const Box *b = &g_boxes[i];
        if (x < b->mn.x || x > b->mx.x || z < b->mn.z || z > b->mx.z) continue;
        float t = b->mx.y;
        if (t <= lim && t > g) g = t;
    });
    for (int i = 0; i < g_nramps; i++) {
        const Ramp *r = &g_ramps[i];
        if (x < r->mn.x || x > r->mx.x || z < r->mn.z || z > r->mx.z) continue;
        float h = ramp_h(r, x, z);
        if (h <= lim && h > g) g = h;
    }
    for (int i = 0; i < MAX_CRATES; i++) {
        const Crate *c = &g_crates[i];
        if (!c->alive || i == excl) continue;
        if (fabsf(x - c->pos.x) > CRATE_H || fabsf(z - c->pos.z) > CRATE_H) continue;
        float t = c->pos.y + 1.2f;
        if (t <= lim && t > g) g = t;
    }
    return g;
}
float ground_height(float x, float z, float y, float step) { return ground_height_excl(x, z, y, step, -1); }
float ceiling_height(float x, float z, float yHead) {
    float c = 1e9f;
    GRID_POINT(x, z, i, {
        const Box *b = &g_boxes[i];
        if (b->mn.y < 0.01f) continue;
        if (x < b->mn.x || x > b->mx.x || z < b->mn.z || z > b->mx.z) continue;
        if (b->mn.y >= yHead - 0.05f && b->mn.y < c) c = b->mn.y;
    });
    return c;
}
static int push_rect(V3 *p, float r, float x0, float z0, float x1, float z1, V3 *acc) {
    if (p->x + r < x0 || p->x - r > x1 || p->z + r < z0 || p->z - r > z1) return 0;
    float cx = clampf(p->x, x0, x1), cz = clampf(p->z, z0, z1);
    float dx = p->x - cx, dz = p->z - cz, d2 = dx * dx + dz * dz;
    float nx, nz, push;
    if (d2 > 1e-8f) {
        if (d2 >= r * r) return 0;
        float d = sqrtf(d2);
        nx = dx / d; nz = dz / d;
        push = r - d;
    } else {
        float l = p->x - x0, rt = x1 - p->x, f = p->z - z0, bk = z1 - p->z;
        float m = fminf(fminf(l, rt), fminf(f, bk));
        if (m == l) { nx = -1; nz = 0; }
        else if (m == rt) { nx = 1; nz = 0; }
        else if (m == f) { nx = 0; nz = -1; }
        else { nx = 0; nz = 1; }
        push = m + r;
    }
    p->x += nx * push;
    p->z += nz * push;
    acc->x += nx;
    acc->z += nz;
    return 1;
}
int collide_circle(V3 *p, float r, float step, float h, V3 *n) {
    int hit = 0;
    V3 acc = v3(0, 0, 0);
    float lim = p->y + step;
    GRID_AREA(p->x - r, p->z - r, p->x + r, p->z + r, i, {
        const Box *b = &g_boxes[i];
        if (b->mx.y <= lim || b->mn.y >= p->y + h) continue;
        if (g_collSkipInvis && b->kind == K_INVIS) continue;
        hit |= push_rect(p, r, b->mn.x, b->mn.z, b->mx.x, b->mx.z, &acc);
    });
    for (int i = 0; i < MAX_CRATES; i++) {
        const Crate *c = &g_crates[i];
        if (!c->alive || c->pos.y + 1.2f <= lim || c->pos.y >= p->y + h) continue;
        hit |= push_rect(p, r, c->pos.x - CRATE_H, c->pos.z - CRATE_H, c->pos.x + CRATE_H, c->pos.z + CRATE_H, &acc);
    }
    for (int i = 0; i < g_nramps; i++) {
        const Ramp *rp = &g_ramps[i];
        if (p->x + r < rp->mn.x || p->x - r > rp->mx.x || p->z + r < rp->mn.z || p->z - r > rp->mx.z) continue;
        float cx = clampf(p->x, rp->mn.x, rp->mx.x), cz = clampf(p->z, rp->mn.z, rp->mx.z);
        if (ramp_h(rp, cx, cz) <= lim) continue;
        hit |= push_rect(p, r, rp->mn.x, rp->mn.z, rp->mx.x, rp->mx.z, &acc);
    }
    if (n) *n = vnorm(acc);
    return hit;
}
static int ray_box(V3 o, V3 d, V3 mn, V3 mx, float *t) {
    float t0 = 0, t1 = 1;
    float oo[3] = {o.x, o.y, o.z}, dd[3] = {d.x, d.y, d.z}, a[3] = {mn.x, mn.y, mn.z}, b[3] = {mx.x, mx.y, mx.z};
    for (int k = 0; k < 3; k++) {
        if (fabsf(dd[k]) < 1e-7f) {
            if (oo[k] < a[k] || oo[k] > b[k]) return 0;
        } else {
            float inv = 1.0f / dd[k];
            float ta = (a[k] - oo[k]) * inv, tb = (b[k] - oo[k]) * inv;
            if (ta > tb) { float s = ta; ta = tb; tb = s; }
            if (ta > t0) t0 = ta;
            if (tb < t1) t1 = tb;
            if (t0 > t1) return 0;
        }
    }
    *t = t0;
    return 1;
}
float ray_clear(V3 a, V3 b) {
    V3 d = vsub(b, a);
    float best = 1;
    GRID_AREA(fminf(a.x, b.x) - 1, fminf(a.z, b.z) - 1, fmaxf(a.x, b.x) + 1, fmaxf(a.z, b.z) + 1, i, {
        const Box *bx = &g_boxes[i];
        if (bx->kind == K_INVIS || bx->kind == K_TRUNK) continue;
        V3 mn = vsub(bx->mn, v3(0.25f, 0.25f, 0.25f)), mx = vadd(bx->mx, v3(0.25f, 0.25f, 0.25f));
        float t;
        if (ray_box(a, d, mn, mx, &t) && t > 0.0f && t < best) best = t;
    });
    return best;
}

float ray_world(V3 a, V3 b) {
    V3 d = vsub(b, a);
    float best = 1;
    GRID_AREA(fminf(a.x, b.x), fminf(a.z, b.z), fmaxf(a.x, b.x), fmaxf(a.z, b.z), i, {
        const Box *bx = &g_boxes[i];
        if (bx->kind == K_INVIS) continue;
        if (fminf(a.x, b.x) > bx->mx.x || fmaxf(a.x, b.x) < bx->mn.x || fminf(a.z, b.z) > bx->mx.z || fmaxf(a.z, b.z) < bx->mn.z) continue;
        float t;
        if (ray_box(a, d, bx->mn, bx->mx, &t) && t >= 0.0f && t < best) best = t;
    });
    if (d.y < -1e-5f && a.y > 0) {
        float t = -a.y / d.y;
        if (t < best) best = t;
    }
    return best;
}

void road_spot_near(V3 p, V3 *out, float *yaw) {
    if (p.z > CITY_HALF + 20 && p.z < AIR_Z1) { /* at the airport: use the access road / apron */
        int apron = p.z > AZ(345);
        V3 c = v3(LANE, 0, clampf(p.z, CITY_HALF + 16, AZ(336)));
        if (apron) c = v3(clampf(p.x, -250, 250), 0, AZ(445));
        *yaw = apron ? PI / 2 : PI;
        for (int i = 0; i < 10; i++) {
            V3 q = vadd(c, v3(apron ? i * 7.0f : 0, 0, apron ? 0 : -i * 7.0f));
            if (car_spot_free(q, 3.5f)) { *out = q; return; }
        }
        *out = c;
        return;
    }
    int k = (int)floorf((p.x + CITY_HALF) / CELL + 0.5f);
    int l = (int)floorf((p.z + CITY_HALF) / CELL + 0.5f);
    if (k < 0) k = 0;
    if (k > NBLK) k = NBLK;
    if (l < 0) l = 0;
    if (l > NBLK) l = NBLK;
    float dx = p.x - road_c(k), dz = p.z - road_c(l);
    V3 base, along;
    if (fabsf(dx) < fabsf(dz)) {
        base = v3(road_c(k) + (dx > 0 ? LANE : -LANE), 0, clampf(p.z, -CITY_HALF, CITY_HALF));
        *yaw = dx > 0 ? PI : 0;
        along = v3(0, 0, 1);
    } else {
        base = v3(clampf(p.x, -CITY_HALF, CITY_HALF), 0, road_c(l) + (dz > 0 ? LANE : -LANE));
        *yaw = dz > 0 ? PI / 2 : -PI / 2;
        along = v3(1, 0, 0);
    }
    for (int i = 0; i < 12; i++) {
        float o = (i & 1 ? 1 : -1) * 6.0f * ((i + 1) / 2);
        V3 c = vadd(base, vmul(along, o));
        if (fabsf(c.x) > CITY_HALF + 4 || fabsf(c.z) > CITY_HALF + 4) continue;
        if (car_spot_free(c, 3.5f)) { *out = c; return; }
    }
    *out = base;
}

/* GPS: breadth-first search over the road grid; returns points from a to b */
static void nearest_node(V3 p, int *k, int *l) {
    *k = (int)floorf((p.x + CITY_HALF) / CELL + 0.5f);
    *l = (int)floorf((p.z + CITY_HALF) / CELL + 0.5f);
    *k = *k < 0 ? 0 : *k > NBLK ? NBLK : *k;
    *l = *l < 0 ? 0 : *l > NBLK ? NBLK : *l;
    if (p.z > CITY_HALF + 12) { *k = NBLK / 2; *l = NBLK; } /* airport: via the access road */
}
int road_route(V3 a, V3 b, V3 *out, int max) {
    enum { NN = (MAXBLK + 1) * (MAXBLK + 1) };
    int prev[NN], queue[NN], qh = 0, qt = 0, W = NBLK + 1;
    int ka, la, kb, lb;
    nearest_node(a, &ka, &la);
    nearest_node(b, &kb, &lb);
    for (int i = 0; i < W * W; i++) prev[i] = -2;
    int s0 = la * W + ka, g = lb * W + kb;
    prev[s0] = -1;
    queue[qt++] = s0;
    static const int DXr[4] = {1, -1, 0, 0}, DZr[4] = {0, 0, 1, -1};
    while (qh < qt) {
        int c = queue[qh++];
        if (c == g) break;
        int k = c % W, l = c / W;
        for (int d = 0; d < 4; d++) {
            int nk = k + DXr[d], nl = l + DZr[d];
            if (nk < 0 || nl < 0 || nk >= W || nl >= W) continue;
            int n = nl * W + nk;
            if (prev[n] != -2) continue;
            prev[n] = c;
            queue[qt++] = n;
        }
    }
    int path[NN], np = 0;
    for (int c = g; c >= 0 && np < NN; c = prev[c]) path[np++] = c;
    int n = 0;
    if (n < max) out[n++] = a;
    if (a.z > CITY_HALF + 12 && n < max) out[n++] = v3(0, 0, fminf(a.z, AZ(445)));
    for (int i = np - 1; i >= 0 && n < max; i--) out[n++] = v3(road_c(path[i] % W), 0, road_c(path[i] / W));
    if (b.z > CITY_HALF + 12 && n < max) out[n++] = v3(0, 0, fminf(b.z, AZ(445)));
    if (n < max) out[n++] = b;
    return n;
}
