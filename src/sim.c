/* Simulation: player on foot, car physics, traffic AI, pedestrians, camera, scene rendering. */
#include "game.h"

Player g_pl;
Car g_cars[MAX_CARS];
Ped g_peds[MAX_PEDS];
V3 g_camPos, g_camLook, g_camRight, g_camUp;
float g_camYaw, g_camPitch = 0.32f, g_gameTime;
char g_prompt[160];

int spawn_ai_car(int type, int k, int l, int dir, int ai, float speed);
int mission_race_point(int idx, V3 *out);

CarType g_carTypes[NUM_CAR_TYPES] = {
    {"RUSTY WAGON", 28, 14, 9, 2.3f, 100, 0, CS_SEDAN, {0.55f, 0.72f, 0.45f}, 4.4f, 2.0f, 1.5f},
    {"CITY TAXI", 32, 16, 9.5f, 2.4f, 100, 400, CS_TAXI, {1.0f, 0.82f, 0.1f}, 4.5f, 2.0f, 1.5f},
    {"BIG PICKUP", 30, 14, 8.5f, 2.1f, 180, 700, CS_TRUCK, {0.8f, 0.15f, 0.12f}, 5.0f, 2.2f, 1.9f},
    {"ICE CREAM VAN", 27, 12, 8, 2.0f, 160, 900, CS_ICECREAM, {1.0f, 0.72f, 0.82f}, 5.0f, 2.2f, 2.1f},
    {"SPORT COUPE", 40, 22, 11, 2.6f, 90, 1500, CS_SPORT, {0.15f, 0.4f, 0.95f}, 4.4f, 2.0f, 1.2f},
    {"HOT ROD", 44, 25, 10, 2.5f, 110, 2500, CS_HOTROD, {0.95f, 0.45f, 0.08f}, 4.4f, 2.0f, 1.4f},
    {"ROCKET RACER", 52, 30, 12, 2.8f, 120, 4000, CS_ROCKET, {0.6f, 0.2f, 0.85f}, 4.8f, 2.0f, 1.2f},
    {"GETAWAY VAN", 27, 13, 8, 2.1f, 110, -1, CS_VAN, {0.15f, 0.15f, 0.18f}, 5.0f, 2.2f, 2.1f},
    {"BOLT ARMORED", 31, 15, 9, 2.1f, 240, -1, CS_ARMOR, {0.3f, 0.32f, 0.35f}, 5.2f, 2.4f, 2.2f},
    {"RACER", 38, 20, 10, 2.5f, 100, -1, CS_SPORT, {0.9f, 0.1f, 0.1f}, 4.4f, 2.0f, 1.2f},
    {"POLICE CRUISER", 38, 21, 10.5f, 2.5f, 140, -1, CS_POLICE, {0.95f, 0.95f, 0.97f}, 4.6f, 2.0f, 1.6f},
    /* aircraft: maxSpeed, accel, grip(unused), turn rate, health, price, style, colour, len, wid, hgt, kind, climb, guns, missiles */
    {"SKY HOPPER", 48, 14, 0, 1.0f, 120, 0, CS_PLANE, {0.95f, 0.95f, 0.92f}, 8.0f, 3.2f, 2.6f, VK_PLANE, 10, 1, 0},
    {"STUNT BIPLANE", 44, 16, 0, 1.35f, 110, 900, CS_BIPLANE, {0.85f, 0.2f, 0.15f}, 7.6f, 3.2f, 3.0f, VK_PLANE, 13, 1, 0},
    {"HELI SCOUT", 30, 14, 0, 1.4f, 130, 1500, CS_HELI, {0.2f, 0.45f, 0.85f}, 7.0f, 3.0f, 2.9f, VK_HELI, 11, 1, 0},
    {"JET FIGHTER", 85, 26, 0, 1.2f, 180, 4500, CS_JET, {0.5f, 0.55f, 0.6f}, 10.0f, 3.4f, 2.6f, VK_PLANE, 18, 2, 1},
    {"GUNSHIP", 36, 16, 0, 1.3f, 260, 6000, CS_GUNSHIP, {0.3f, 0.38f, 0.25f}, 8.4f, 3.4f, 3.4f, VK_HELI, 12, 2, 1},
    {"VIPER GUNSHIP", 30, 14, 0, 1.3f, 700, -1, CS_GUNSHIP, {0.45f, 0.15f, 0.6f}, 8.4f, 3.4f, 3.4f, VK_HELI, 12, 2, 1},
};

#define SKIN {0.95f, 0.78f, 0.6f}
Look g_looks[NUM_OUTFITS] = {
    {{0.95f, 0.5f, 0.15f}, {0.2f, 0.3f, 0.65f}, SKIN, {0.85f, 0.15f, 0.15f}, {0.25f, 0.2f, 0.2f}, 0},
    {{0.2f, 0.8f, 0.8f}, {0.85f, 0.75f, 0.5f}, SKIN, {0.95f, 0.85f, 0.4f}, {0.6f, 0.4f, 0.2f}, 1},
    {{0.85f, 0.15f, 0.12f}, {0.25f, 0.2f, 0.15f}, SKIN, {0.15f, 0.4f, 0.2f}, {0.3f, 0.2f, 0.1f}, 0},
    {{0.12f, 0.12f, 0.14f}, {0.1f, 0.1f, 0.12f}, SKIN, {0.1f, 0.1f, 0.1f}, {0.05f, 0.05f, 0.05f}, 2},
    {{0.92f, 0.92f, 0.95f}, {0.75f, 0.75f, 0.8f}, SKIN, {0.85f, 0.88f, 0.95f}, {0.4f, 0.4f, 0.45f}, 3},
    {{1.0f, 0.8f, 0.15f}, {0.95f, 0.7f, 0.1f}, SKIN, {1.0f, 0.85f, 0.2f}, {0.9f, 0.65f, 0.1f}, 7},
    {{0.55f, 0.4f, 0.25f}, {0.35f, 0.33f, 0.28f}, SKIN, {0.45f, 0.3f, 0.2f}, {0.2f, 0.12f, 0.08f}, 3},
    {{0.3f, 0.4f, 0.22f}, {0.25f, 0.3f, 0.18f}, SKIN, {0.25f, 0.32f, 0.18f}, {0.1f, 0.1f, 0.08f}, 0},
};
const char *g_lookNames[NUM_OUTFITS] = {"CLASSIC", "BEACH BUM", "LUMBERJACK", "SECRET AGENT", "SPACE CADET", "GOLDEN HERO", "ACE PILOT", "COMMANDO"};
const int g_lookPrice[NUM_OUTFITS] = {0, 150, 250, 350, 500, 1500, 400, 600};
const char *g_upNames[NUM_UPGRADES] = {"SPRING SHOES", "COIN MAGNET", "ARMOR PLATING", "NITRO BOOST"};
const char *g_upDesc[NUM_UPGRADES] = {
    "Jump again in mid-air. Reach new heights!",
    "Nearby coins fly straight into your pockets.",
    "Your cars take 40% less damage.",
    "Hold SPRINT while driving for a burst of speed."};
const int g_upPrice[NUM_UPGRADES] = {500, 800, 600, 1000};
Look g_npcLooks[12] = {
    {{0.6f, 0.35f, 0.7f}, {0.5f, 0.5f, 0.55f}, {0.92f, 0.8f, 0.7f}, {0.85f, 0.85f, 0.88f}, {0.3f, 0.2f, 0.2f}, 2},
    {{0.97f, 0.97f, 0.97f}, {0.15f, 0.15f, 0.18f}, {0.9f, 0.7f, 0.55f}, {1, 1, 1}, {0.1f, 0.1f, 0.1f}, 5},
    {{0.95f, 0.95f, 0.98f}, {0.3f, 0.3f, 0.35f}, {0.85f, 0.75f, 0.65f}, {0.4f, 0.9f, 0.4f}, {0.2f, 0.2f, 0.2f}, 4},
    {{0.2f, 0.35f, 0.6f}, {0.2f, 0.3f, 0.55f}, {0.8f, 0.6f, 0.45f}, {0.15f, 0.15f, 0.3f}, {0.2f, 0.15f, 0.1f}, 0},
    {{0.15f, 0.25f, 0.55f}, {0.1f, 0.15f, 0.35f}, {0.75f, 0.55f, 0.4f}, {0.1f, 0.15f, 0.4f}, {0.05f, 0.05f, 0.05f}, 6},
    {{0.85f, 0.15f, 0.15f}, {0.85f, 0.15f, 0.15f}, {0.6f, 0.42f, 0.3f}, {1, 1, 1}, {1, 1, 1}, 0},
    {{0.1f, 0.1f, 0.12f}, {0.1f, 0.1f, 0.12f}, {0.95f, 0.8f, 0.68f}, {0.1f, 0.1f, 0.1f}, {0.1f, 0.1f, 0.1f}, 2},
    {{0.3f, 0.7f, 0.4f}, {0.35f, 0.3f, 0.25f}, {0.7f, 0.5f, 0.35f}, {0.3f, 0.2f, 0.1f}, {0.2f, 0.2f, 0.2f}, 4},
    {{0.95f, 0.85f, 0.2f}, {0.3f, 0.3f, 0.6f}, {0.95f, 0.8f, 0.65f}, {0.9f, 0.3f, 0.5f}, {0.9f, 0.9f, 0.9f}, 4},
    {{0.9f, 0.45f, 0.7f}, {0.95f, 0.95f, 0.95f}, {0.55f, 0.38f, 0.28f}, {0.3f, 0.2f, 0.15f}, {0.8f, 0.3f, 0.5f}, 1},
    {{0.5f, 0.5f, 0.52f}, {0.25f, 0.25f, 0.3f}, {0.9f, 0.75f, 0.6f}, {0.6f, 0.3f, 0.1f}, {0.15f, 0.1f, 0.1f}, 0},
    {{0.25f, 0.6f, 0.85f}, {0.9f, 0.85f, 0.75f}, {0.8f, 0.62f, 0.48f}, {0.95f, 0.95f, 0.3f}, {0.6f, 0.4f, 0.2f}, 1},
};

typedef struct { V3 p, v; float life, max, size, grow, grav; float c[3]; } Part;
static Part parts[MAX_PARTS];
static int partNext;
static float mouseIdle;
static int camSnap = 1;
static V3 camSm;
static int calledCar = -1;
static const float TRAFFIC_COL[8][3] = {{0.8f, 0.2f, 0.2f}, {0.2f, 0.45f, 0.8f}, {0.9f, 0.9f, 0.9f}, {0.25f, 0.6f, 0.3f}, {0.9f, 0.7f, 0.2f}, {0.5f, 0.3f, 0.6f}, {0.35f, 0.35f, 0.38f}, {0.6f, 0.8f, 0.9f}};

/* ------------------------------------------------------------ particles */
void spawn_part(V3 p, V3 v, float life, float size, float r, float g, float b, float grav, float grow) {
    Part *q = &parts[partNext];
    partNext = (partNext + 1) % MAX_PARTS;
    q->p = p; q->v = v; q->life = q->max = life; q->size = size; q->grow = grow; q->grav = grav;
    q->c[0] = r; q->c[1] = g; q->c[2] = b;
}
void fx_burst(V3 p, int n, float spd, float life, float size, float r, float g, float b, float grav) {
    for (int i = 0; i < n; i++)
        spawn_part(p, v3(frange(-spd, spd), frange(0, spd * 1.3f), frange(-spd, spd)), life * frange(0.6f, 1.2f), size, r, g, b, grav, 0);
}
static void parts_update(float dt) {
    for (int i = 0; i < MAX_PARTS; i++) {
        Part *q = &parts[i];
        if (q->life <= 0) continue;
        q->life -= dt;
        q->v.y -= q->grav * dt;
        q->p = vadd(q->p, vmul(q->v, dt));
        q->size += q->grow * dt;
        if (q->p.y < 0.05f && q->grav > 0) { q->p.y = 0.05f; q->v.y *= -0.3f; q->v.x *= 0.7f; q->v.z *= 0.7f; }
    }
}
static void parts_draw(void) {
    glBegin(GL_QUADS);
    for (int i = 0; i < MAX_PARTS; i++) {
        Part *q = &parts[i];
        if (q->life <= 0) continue;
        float a = clampf(q->life / q->max, 0, 1);
        glColor4f(q->c[0], q->c[1], q->c[2], a);
        V3 r = vmul(g_camRight, q->size), u = vmul(g_camUp, q->size);
        V3 p = q->p;
        glVertex3f(p.x - r.x - u.x, p.y - r.y - u.y, p.z - r.z - u.z);
        glVertex3f(p.x + r.x - u.x, p.y + r.y - u.y, p.z + r.z - u.z);
        glVertex3f(p.x + r.x + u.x, p.y + r.y + u.y, p.z + r.z + u.z);
        glVertex3f(p.x - r.x + u.x, p.y - r.y + u.y, p.z - r.z + u.z);
    }
    glEnd();
}

/* ------------------------------------------------------------- helpers */
V3 player_pos(void) { return g_pl.car >= 0 ? g_cars[g_pl.car].pos : g_pl.pos; }
Car *player_car(void) { return g_pl.car >= 0 ? &g_cars[g_pl.car] : NULL; }
void add_coins(int n) {
    g_prog.coins += n;
    if (n > 0) g_prog.earned += n;
}
int car_spot_free(V3 p, float r) {
    for (int i = 0; i < MAX_CARS; i++)
        if (g_cars[i].used && dist2xz(g_cars[i].pos, p) < r * r) return 0;
    V3 q = p;
    q.y = 0;
    V3 q0 = q;
    collide_circle(&q, 1.3f, 0.5f, 2.0f, NULL);
    return dist2xz(q, q0) < 0.01f;
}
float car_speed(const Car *c) { return sqrtf(c->vel.x * c->vel.x + c->vel.z * c->vel.z); }
static float car_vf(const Car *c) { V3 f = fwd_of(c->yaw); return c->vel.x * f.x + c->vel.z * f.z; }

void remove_car(int i) {
    if (i < 0 || i >= MAX_CARS) return;
    if (g_pl.car == i) return;
    g_cars[i].used = 0;
    if (calledCar == i) calledCar = -1;
}
int spawn_car(int type, V3 pos, float yaw, int ai) {
    int slot = -1;
    for (int i = 0; i < MAX_CARS; i++) if (!g_cars[i].used) { slot = i; break; }
    if (slot < 0) {
        float best = -1;
        for (int i = 0; i < MAX_CARS; i++) {
            Car *c = &g_cars[i];
            if (i == g_pl.car || c->mission || (c->ai != AI_PARKED && c->ai != AI_TRAFFIC)) continue;
            float d = dist2xz(c->pos, player_pos());
            if (d > best) { best = d; slot = i; }
        }
        if (slot < 0) return -1;
    }
    Car *c = &g_cars[slot];
    memset(c, 0, sizeof *c);
    c->used = 1;
    c->type = type;
    c->pos = pos;
    c->yaw = yaw;
    c->onGround = 1;
    c->health = c->maxHealth = g_carTypes[type].health;
    c->ai = ai;
    c->fuel = 1;
    c->stand = -1;
    c->crew = type == CAR_POLICE ? 2 : 0;
    memcpy(c->col, g_carTypes[type].col, sizeof c->col);
    return slot;
}

/* -------------------------------------------------------- road network */
static const int DX[4] = {1, 0, -1, 0}, DZ[4] = {0, 1, 0, -1};
static int node_ok(int k, int l) { return k >= 0 && k <= NBLK && l >= 0 && l <= NBLK; }
static V3 node_pos(int k, int l) { return v3(road_c(k), 0, road_c(l)); }
static V3 lane_r(int d) { return v3((float)-DZ[d], 0, (float)DX[d]); }
static int pick_dir(int k, int l, int from, V3 *away) {
    int opts[4], n = 0;
    for (int d = 0; d < 4; d++) {
        if (from >= 0 && d == ((from + 2) & 3)) continue;
        if (node_ok(k + DX[d], l + DZ[d])) opts[n++] = d;
    }
    if (!n) return (from + 2) & 3;
    if (away && rand() % 4) {
        int best = opts[0];
        float bd = -1e9f;
        for (int i = 0; i < n; i++) {
            float s = DX[opts[i]] * away->x + DZ[opts[i]] * away->z + frange(-0.3f, 0.3f);
            if (s > bd) { bd = s; best = opts[i]; }
        }
        return best;
    }
    return opts[rand() % n];
}
static V3 corner_wp(int k, int l, int din, int dout) {
    V3 c = node_pos(k, l);
    if (din == dout) return vadd(c, vmul(lane_r(din), LANE));
    return vadd(c, vadd(vmul(lane_r(din), LANE), vmul(lane_r(dout), LANE)));
}
static void ai_setup_route(Car *c, int k, int l, int dir) {
    c->dir = dir;
    c->node[0] = k + DX[dir];
    c->node[1] = l + DZ[dir];
    c->ndir = pick_dir(c->node[0], c->node[1], dir, NULL);
    c->wp = corner_wp(c->node[0], c->node[1], c->dir, c->ndir);
}
int spawn_ai_car(int type, int k, int l, int dir, int ai, float speed) {
    V3 p = vadd(vadd(node_pos(k, l), vmul(v3((float)DX[dir], 0, (float)DZ[dir]), 14)), vmul(lane_r(dir), LANE));
    int i = spawn_car(type, p, atan2f((float)DX[dir], (float)DZ[dir]), ai);
    if (i < 0) return -1;
    ai_setup_route(&g_cars[i], k, l, dir);
    g_cars[i].topSpeed = speed;
    return i;
}
static int place_traffic(Car *c, float minDist) {
    for (int tries = 0; tries < 30; tries++) {
        int k = rand() % (NBLK + 1), l = rand() % (NBLK + 1);
        int d = pick_dir(k, l, -1, NULL);
        V3 p = vadd(vadd(node_pos(k, l), vmul(v3((float)DX[d], 0, (float)DZ[d]), frange(15, 50))), vmul(lane_r(d), LANE));
        if (distxz(p, player_pos()) < minDist) continue;
        if (!car_spot_free(p, 6)) continue;
        c->pos = p;
        c->yaw = atan2f((float)DX[d], (float)DZ[d]);
        c->vel = v3(0, 0, 0);
        c->vy = 0;
        c->onGround = 1;
        c->stuck = 0;
        ai_setup_route(c, k, l, d);
        return 1;
    }
    return 0;
}
static void spawn_traffic(int n, float minDist) {
    for (int i = 0; i < n; i++) {
        int type = rand() % 4;
        int s = spawn_car(type, v3(0, 0, 0), 0, AI_TRAFFIC);
        if (s < 0) return;
        Car *c = &g_cars[s];
        if (type != 1 && type != 3) memcpy(c->col, TRAFFIC_COL[rand() % 8], sizeof c->col);
        c->topSpeed = frange(11, 15);
        if (!place_traffic(c, minDist)) c->used = 0;
    }
}

/* ------------------------------------------------------------ car physics */
static void car_wreck(Car *c) {
    c->wrecked = 1;
    c->wreckT = 0;
    c->health = 0;
    audio_play(SND_EXPLODE, 0.8f, 1);
    V3 p = vadd(c->pos, v3(0, 1, 0));
    fx_burst(p, 40, 9, 1.0f, 0.7f, 1, 0.6f, 0.1f, 4);
    fx_burst(p, 25, 4, 2.2f, 1.2f, 0.25f, 0.25f, 0.25f, -1);
    police_on_car_destroyed(c);
    if (g_pl.car >= 0 && &g_cars[g_pl.car] == c) {
        int air = is_aircraft(c);
        g_pl.car = -1;
        g_pl.pos = vadd(c->pos, vmul(right_of(c->yaw), -2.5f));
        g_pl.pos.y = c->pos.y + 0.5f;
        g_pl.vel = v3(0, 8, 0);
        g_pl.knock = 1.0f;
        g_pl.onGround = 0;
        collide_circle(&g_pl.pos, 0.4f, 0.45f, 1.8f, NULL);
        if (c->pos.y > 6) g_pl.chute = 0.01f; /* emergency parachute */
        player_damage(air ? 35.0f : 20.0f, c->pos);
        hud_msg(3, air ? "YOUR AIRCRAFT IS WRECKED!" : "YOUR CAR IS WRECKED!  PRESS [%s] TO CALL A NEW ONE", act_key(ACT_CALLCAR));
    }
}
float car_up_mul(int which) {
    /* tuning multipliers for the vehicle the player is driving */
    switch (which) {
    case 0: return 1.0f + 0.08f * g_prog.carUp[0];
    case 1: return 1.0f + 0.15f * g_prog.carUp[1];
    case 2: return 1.0f / (1.0f + 0.3f * g_prog.carUp[2]);
    case 3: return 1.0f / (1.0f + 0.4f * g_prog.carUp[3]);
    }
    return 1;
}
void car_damage(Car *c, float d) {
    if (c->wrecked) return;
    if (g_pl.car >= 0 && &g_cars[g_pl.car] == c) {
        if (g_prog.upgrades & UP_ARMOR) d *= 0.6f;
        if (is_aircraft(c)) d /= 1.0f + 0.3f * g_prog.airUp[2];
        else d *= car_up_mul(2);
        if (g_godMode) d = 0;
    }
    c->health -= d;
    c->lastHit = 0.3f;
    if (c->health <= 0) car_wreck(c);
}
static float car_physics(Car *c, float dt, float thr, float steer, int hb, int boost) {
    const CarType *t = &g_carTypes[c->type];
    if (c->wrecked) { thr = 0; steer = 0; hb = 1; }
    V3 f = fwd_of(c->yaw), r = right_of(c->yaw);
    float vf = c->vel.x * f.x + c->vel.z * f.z, vl = c->vel.x * r.x + c->vel.z * r.z;
    float maxS = t->maxSpeed * (boost ? 1.3f : 1.0f);
    float acc = t->accel * (boost ? 1.7f : 1.0f);
    if (g_pl.car >= 0 && &g_cars[g_pl.car] == c) { maxS *= car_up_mul(0); acc *= car_up_mul(1); }
    if (c->onGround) {
        if (thr > 0) {
            if (vf < -0.5f) vf += acc * 2.2f * thr * dt;
            else if (vf < maxS) vf += acc * thr * dt * (1.0f - 0.5f * vf / maxS);
        } else if (thr < 0) {
            if (vf > 0.5f) vf += thr * acc * 2.4f * dt;
            else if (vf > -maxS * 0.35f) vf += thr * acc * 0.7f * dt;
        } else {
            float d = 3.5f * dt;
            vf = fabsf(vf) < d ? 0 : vf - d * signf(vf);
        }
        if (hb) { float d = 11 * dt; vf = fabsf(vf) < d ? 0 : vf - d * signf(vf); }
        if (vf > maxS) vf = lerpf(vf, maxS, clampf(2 * dt, 0, 1));
        float sp = fabsf(vf);
        float sa = steer * t->steer * clampf(sp / 5.0f, 0, 1) / (1.0f + sp * 0.018f);
        if (hb) sa *= 1.45f;
        c->yaw = wrap_pi(c->yaw - sa * (vf >= 0 ? 1 : -1) * dt);
        float grip = hb ? 1.3f : t->grip;
        vl *= expf(-grip * dt);
        V3 f2 = fwd_of(c->yaw), r2 = right_of(c->yaw);
        c->vel.x = f2.x * vf + r2.x * vl;
        c->vel.z = f2.z * vf + r2.z * vl;
        if (fabsf(vl) > 4 && rand() % 6 == 0) {
            V3 bp = vsub(c->pos, vmul(f2, t->len * 0.35f));
            spawn_part(vadd(bp, v3(0, 0.3f, 0)), v3(frange(-1, 1), 1, frange(-1, 1)), 0.9f, 0.5f, 0.85f, 0.85f, 0.85f, -0.5f, 1.2f);
        }
    } else {
        c->yaw = wrap_pi(c->yaw - steer * 0.9f * dt);
    }
    c->steerVis = lerpf(c->steerVis, -steer * 0.45f, clampf(10 * dt, 0, 1));
    c->wheelRot += vf * dt / 0.38f;

    float oldY = c->pos.y;
    c->pos.x += c->vel.x * dt;
    c->pos.z += c->vel.z * dt;
    float spd = car_speed(c);
    float rr2 = (t->len * 0.5f + 0.6f) * (t->len * 0.5f + 0.6f);
    for (int i = 0; i < MAX_CRATES; i++) {
        Crate *cr = &g_crates[i];
        if (!cr->alive || fabsf(cr->pos.y - c->pos.y) > 1.6f) continue;
        if (dist2xz(cr->pos, c->pos) < rr2 && spd > 3) {
            break_crate(i);
            c->vel.x *= 0.88f;
            c->vel.z *= 0.88f;
        }
    }
    float impact = 0;
    V3 fw = fwd_of(c->yaw);
    float off = t->len * 0.28f, R = t->wid * 0.55f;
    for (int s = -1; s <= 1; s += 2) {
        V3 cp = vadd(c->pos, vmul(fw, off * s)), before = cp, n;
        if (collide_circle(&cp, R, 0.75f, t->hgt, &n)) {
            c->pos.x += cp.x - before.x;
            c->pos.z += cp.z - before.z;
            float vn = c->vel.x * n.x + c->vel.z * n.z;
            if (vn < 0) {
                c->vel.x -= n.x * vn * 1.35f;
                c->vel.z -= n.z * vn * 1.35f;
                if (-vn > impact) impact = -vn;
            }
        }
    }
    if (impact > 8) {
        car_damage(c, (impact - 8) * 2.0f);
        if (g_pl.car >= 0 && &g_cars[g_pl.car] == c) audio_play(SND_CRASH, clampf(impact / 25, 0.3f, 1), frange(0.9f, 1.1f));
        fx_burst(vadd(c->pos, v3(0, 0.8f, 0)), 8, 4, 0.5f, 0.15f, 1, 0.85f, 0.3f, 10);
    }
    float g = ground_height(c->pos.x, c->pos.z, oldY, 0.9f);
    if (c->onGround) {
        if (g >= oldY - 0.35f) {
            c->vy = clampf((g - oldY) / dt, -15, 15);
            c->pos.y = g;
        } else c->onGround = 0;
    }
    if (!c->onGround) {
        c->vy -= GRAV * dt;
        c->pos.y = oldY + c->vy * dt;
        if (c->pos.y <= g) {
            float land = -c->vy;
            c->pos.y = g;
            c->vy = 0;
            c->onGround = 1;
            if (land > 16) car_damage(c, (land - 16) * 2.0f);
            if (land > 7) {
                fx_burst(c->pos, 10, 3, 0.6f, 0.4f, 0.8f, 0.75f, 0.6f, -0.5f);
                if (g_pl.car >= 0 && &g_cars[g_pl.car] == c) audio_play(SND_DOOR, 0.6f, 0.7f);
            }
        }
    }
    if (c->onGround) {
        float hf = ground_height(c->pos.x + fw.x * off, c->pos.z + fw.z * off, c->pos.y, 0.9f);
        float hb2 = ground_height(c->pos.x - fw.x * off, c->pos.z - fw.z * off, c->pos.y, 0.9f);
        c->pitch = lerpf(c->pitch, atan2f(hf - hb2, off * 2), clampf(dt * 12, 0, 1));
    } else {
        c->pitch = lerpf(c->pitch, clampf(atan2f(c->vy, fmaxf(spd, 6)) * 0.6f, -0.6f, 0.6f), clampf(dt * 3, 0, 1));
    }
    c->roll = lerpf(c->roll, clampf(vl * 0.012f, -0.1f, 0.1f), clampf(dt * 6, 0, 1));
    if (c->lastHit > 0) c->lastHit -= dt;
    return impact;
}

/* ----------------------------------------------------------------- AI */
static int obstacle_ahead(const Car *c) {
    V3 f = fwd_of(c->yaw), r = right_of(c->yaw);
    V3 pp = player_pos();
    V3 d = vsub(pp, c->pos);
    float a = vdot(d, f), l = fabsf(vdot(d, r));
    if (a > 0 && a < 9 && l < 2.4f && fabsf(d.y) < 3) return 1;
    for (int i = 0; i < MAX_CARS; i++) {
        const Car *o = &g_cars[i];
        if (!o->used || o == c) continue;
        d = vsub(o->pos, c->pos);
        a = vdot(d, f);
        if (a <= 0 || a > 9.5f) continue;
        l = fabsf(vdot(d, r));
        if (l < 2.3f) return 1;
    }
    for (int i = 0; i < MAX_PEDS; i++) {
        const Ped *p = &g_peds[i];
        if (!p->active || p->knock > 0 || p->respawn > 0) continue;
        d = vsub(p->pos, c->pos);
        a = vdot(d, f);
        if (a > 0 && a < 7 && fabsf(vdot(d, r)) < 2.0f) return 1;
    }
    return 0;
}
static void ai_drive(Car *c, V3 target, float speed, float dt, int avoid) {
    float dx = target.x - c->pos.x, dz = target.z - c->pos.z;
    float diff = wrap_pi(atan2f(dx, dz) - c->yaw);
    float steer = clampf(-diff * 2.5f, -1, 1);
    float spd = speed;
    if (fabsf(diff) > 0.5f) spd = fminf(spd, 10);
    if (avoid && obstacle_ahead(c)) spd = 0;
    float vf = car_vf(c);
    float thr = vf < spd - 0.5f ? 1.0f : (vf > spd + 1.5f ? -1.0f : 0.0f);
    if (spd == 0 && vf < 0.5f) thr = 0;
    int hb = 0;
    if (c->stun > 0) { c->stun -= dt; thr = 0; steer = 0; hb = 1; }
    car_physics(c, dt, thr, steer, hb, 0);
    if (fabsf(vf) < 1.0f && spd > 3 && c->stun <= 0) c->stuck += dt;
    else c->stuck = fmaxf(0, c->stuck - dt);
}
static void ai_route_step(Car *c, V3 *away) {
    if (dist2xz(c->pos, c->wp) < 4.5f * 4.5f) {
        c->dir = c->ndir;
        c->node[0] += DX[c->dir];
        c->node[1] += DZ[c->dir];
        if (!node_ok(c->node[0], c->node[1])) {
            c->node[0] -= DX[c->dir];
            c->node[1] -= DZ[c->dir];
            c->dir = (c->dir + 2) & 3;
            c->node[0] += DX[c->dir];
            c->node[1] += DZ[c->dir];
        }
        c->ndir = pick_dir(c->node[0], c->node[1], c->dir, away);
        c->wp = corner_wp(c->node[0], c->node[1], c->dir, c->ndir);
    }
}
static void police_ai(Car *c, float dt, int frozen) {
    V3 pp = player_pos(), inc, goal = pp;
    int hasInc = police_incident(&inc);
    int chase = wanted_stars() > 0 && distxz(c->pos, pp) < 350 && !g_pl.dead;
    int has = chase;
    if (!chase && hasInc && distxz(c->pos, inc) < 300) { goal = inc; has = 1; }
    if (c->crew == 0) {
        c->aiT2 += dt;
        if (c->aiT2 > 25 && !chase) { c->crew = 2; c->aiT2 = 0; }
    }
    if (!has || frozen) {
        ai_route_step(c, NULL);
        ai_drive(c, c->wp, frozen ? 0 : c->topSpeed, dt, 1);
        return;
    }
    c->siren -= dt;
    float dp = distxz(c->pos, pp);
    if (c->siren <= 0) { c->siren = 1.2f; if (dp < 140) audio_play(SND_SIREN, 0.35f * (1 - dp / 140), 1); }
    float d = distxz(c->pos, goal);
    if (d < 45) {
        if (chase && g_pl.car >= 0) ai_drive(c, goal, 32, dt, 0);
        else {
            if (c->crew == 0 && d < 30) { car_physics(c, dt, 0, 0, 1, 0); return; }
            ai_drive(c, goal, d > 15 ? 16 : 0, dt, 0);
            if (d < 17 && c->crew > 0 && car_speed(c) < 3) {
                V3 r = right_of(c->yaw);
                for (int k = 0; k < c->crew; k++) spawn_thug(GANG_POLICE, vadd(c->pos, vmul(r, k ? 2.0f : -2.0f)), -1);
                c->crew = 0;
                c->aiT2 = 0;
                audio_play(SND_DOOR, 0.5f, 1);
            }
        }
        c->wp = c->pos; /* re-plan the route when we leave direct mode */
    } else {
        if (dist2xz(c->pos, c->wp) < 1) {
            int k = (int)floorf((c->pos.x + CITY_HALF) / CELL + 0.5f), l = (int)floorf((c->pos.z + CITY_HALF) / CELL + 0.5f);
            k = k < 0 ? 0 : k > NBLK ? NBLK : k;
            l = l < 0 ? 0 : l > NBLK ? NBLK : l;
            c->node[0] = k; c->node[1] = l;
            c->ndir = pick_dir(k, l, -1, NULL);
            c->wp = corner_wp(k, l, c->dir, c->ndir);
        }
        V3 toward = vsub(goal, c->pos);
        ai_route_step(c, &toward);
        ai_drive(c, c->wp, chase ? 30 : 22, dt, 0);
    }
}
static void ai_update(Car *c, float dt, int frozen) {
    if (g_carTypes[c->type].kind != VK_CAR) { air_ai(c, dt, frozen); return; }
    if (c->wrecked) { car_physics(c, dt, 0, 0, 1, 0); return; }
    switch (c->ai) {
    case AI_POLICE: police_ai(c, dt, frozen); break;
    case AI_TRAFFIC:
        ai_route_step(c, NULL);
        ai_drive(c, c->wp, c->topSpeed, dt, 1);
        break;
    case AI_FLEE: {
        if (frozen) { car_physics(c, dt, 0, 0, 1, 0); break; }
        V3 away = vsub(c->pos, player_pos());
        ai_route_step(c, &away);
        float sp = c->topSpeed;
        float d = distxz(c->pos, player_pos());
        if (d > 90) sp *= 0.8f;
        ai_drive(c, c->wp, sp, dt, 0);
    } break;
    case AI_RACE: {
        if (frozen) { car_physics(c, dt, 0, 0, 1, 0); break; }
        V3 tp;
        if (!mission_race_point(c->raceIdx, &tp)) { car_physics(c, dt, -1, 0, 1, 0); break; }
        V3 f = fwd_of(c->yaw);
        V3 aim = vadd(tp, vmul(right_of(atan2f(tp.x - c->pos.x, tp.z - c->pos.z)), c->raceOff));
        if (distxz(c->pos, tp) < 10) c->raceIdx++;
        float sp = c->topSpeed;
        V3 pp = player_pos();
        float ahead = vdot(vsub(c->pos, pp), f);
        if (ahead < -15) sp *= 1.12f;
        if (ahead > 40) sp *= 0.88f;
        ai_drive(c, aim, sp, dt, 0);
    } break;
    default: /* parked / abandoned */
        if (car_speed(c) > 0.05f || !c->onGround) car_physics(c, dt, 0, 0, 1, 0);
        break;
    }
}

static void cars_collide(void) {
    for (int i = 0; i < MAX_CARS; i++) {
        Car *a = &g_cars[i];
        if (!a->used) continue;
        for (int j = i + 1; j < MAX_CARS; j++) {
            Car *b = &g_cars[j];
            if (!b->used || fabsf(a->pos.y - b->pos.y) > 2.2f) continue;
            if (dist2xz(a->pos, b->pos) > 64) continue;
            const CarType *ta = &g_carTypes[a->type], *tb = &g_carTypes[b->type];
            V3 fa = fwd_of(a->yaw), fb = fwd_of(b->yaw);
            float ra = ta->wid * 0.55f, rb = tb->wid * 0.55f;
            float best = 0;
            V3 bn = v3(0, 0, 0);
            for (int sa = -1; sa <= 1; sa += 2)
                for (int sb = -1; sb <= 1; sb += 2) {
                    V3 ca = vadd(a->pos, vmul(fa, ta->len * 0.28f * sa));
                    V3 cb = vadd(b->pos, vmul(fb, tb->len * 0.28f * sb));
                    float d = distxz(ca, cb);
                    float pen = ra + rb - d;
                    if (pen > best) {
                        best = pen;
                        bn = d > 1e-4f ? v3((ca.x - cb.x) / d, 0, (ca.z - cb.z) / d) : v3(1, 0, 0);
                    }
                }
            if (best <= 0) continue;
            float ma = ta->health / 100.0f, mb = tb->health / 100.0f;
            if (a->ai == AI_PARKED && car_speed(a) < 0.1f) ma *= 1.5f;
            if (b->ai == AI_PARKED && car_speed(b) < 0.1f) mb *= 1.5f;
            float tot = ma + mb;
            a->pos.x += bn.x * best * mb / tot;
            a->pos.z += bn.z * best * mb / tot;
            b->pos.x -= bn.x * best * ma / tot;
            b->pos.z -= bn.z * best * ma / tot;
            float vn = (a->vel.x - b->vel.x) * bn.x + (a->vel.z - b->vel.z) * bn.z;
            if (vn >= 0) continue;
            float jimp = -(1.3f) * vn / (1 / ma + 1 / mb);
            a->vel.x += bn.x * jimp / ma; a->vel.z += bn.z * jimp / ma;
            b->vel.x -= bn.x * jimp / mb; b->vel.z -= bn.z * jimp / mb;
            float impact = -vn;
            int pa = (g_pl.car == i), pb = (g_pl.car == j);
            if (impact > 5) {
                float dmg = (impact - 5) * 1.6f;
                car_damage(a, dmg * (pb && a->mission ? 2.2f : 1.0f) * (pa ? 0.4f : 1));
                car_damage(b, dmg * (pa && b->mission ? 2.2f : 1.0f) * (pb ? 0.4f : 1));
                fx_burst(vmul(vadd(a->pos, b->pos), 0.5f), 10, 5, 0.5f, 0.15f, 1, 0.85f, 0.3f, 10);
            }
            if (a->ai == AI_TRAFFIC && impact > 3) a->stun = 1.5f;
            if (b->ai == AI_TRAFFIC && impact > 3) b->stun = 1.5f;
            if (impact > 6 && (pa || pb)) {
                Car *o = pa ? b : a;
                if (o->ai == AI_POLICE) wanted_crime(0.8f, o->pos);
                else if (o->ai == AI_TRAFFIC || o->ai == AI_PARKED) wanted_crime(0.15f, o->pos);
            }
            if ((pa || pb) && impact > 3) audio_play(SND_CRASH, clampf(impact / 20, 0.25f, 1), frange(0.9f, 1.15f));
        }
    }
}

/* ----------------------------------------------------------- pedestrians */
static V3 ped_perim(int bi, int bj, float t) {
    float x0, z0, x1, z1;
    block_rect(bi, bj, &x0, &z0, &x1, &z1);
    x0 += 1.5f; z0 += 1.5f; x1 -= 1.5f; z1 -= 1.5f;
    float w = x1 - x0, d = z1 - z0, P = 2 * (w + d);
    t = fmodf(t, P);
    if (t < 0) t += P;
    if (t < w) return v3(x0 + t, 0, z0);
    if (t < w + d) return v3(x1, 0, z0 + t - w);
    if (t < 2 * w + d) return v3(x1 - (t - w - d), 0, z1);
    return v3(x0, 0, z1 - (t - 2 * w - d));
}
static void ped_spawn(Ped *p) {
    memset(p, 0, sizeof *p);
    p->active = 1;
    for (int tries = 0; tries < 10; tries++) {
        p->bi = rand() % NBLK;
        p->bj = rand() % NBLK;
        p->t = frange(0, 200);
        p->pos = ped_perim(p->bi, p->bj, p->t);
        if (distxz(p->pos, player_pos()) > 40) break;
    }
    p->speed = frange(1.2f, 2.0f);
    p->dir = rand() & 1 ? 1 : -1;
    p->look = 7 + rand() % 5;
}
static void knock_ped(Ped *p, V3 vel) {
    p->knock = 2.5f;
    p->vel = v3(vel.x * 0.7f, 8 + vlen(vel) * 0.15f, vel.z * 0.7f);
    p->spin = 0;
    audio_play(SND_KICK, 0.5f, 0.8f);
}
static void peds_update(float dt) {
    for (int i = 0; i < MAX_PEDS; i++) {
        Ped *p = &g_peds[i];
        if (!p->active) { ped_spawn(p); continue; }
        if (p->respawn > 0) {
            p->respawn -= dt;
            if (p->respawn <= 0) ped_spawn(p);
            continue;
        }
        if (p->knock > 0) {
            p->knock -= dt;
            p->vel.y -= GRAV * dt;
            p->pos = vadd(p->pos, vmul(p->vel, dt));
            float g = ground_height(p->pos.x, p->pos.z, p->pos.y, 0.3f);
            if (p->pos.y <= g) {
                p->pos.y = g;
                p->vel.x *= 0.6f; p->vel.z *= 0.6f;
                p->vel.y = fabsf(p->vel.y) * 0.25f;
                p->down = 1;
            } else p->spin += dt * 9;
            if (p->knock <= 0) p->respawn = 3;
            continue;
        }
        V3 old = p->pos;
        p->t += p->speed * p->dir * dt;
        p->pos = ped_perim(p->bi, p->bj, p->t);
        V3 d = vsub(p->pos, old);
        if (d.x * d.x + d.z * d.z > 1e-6f) p->yaw = atan2f(d.x, d.z);
        p->anim += dt * p->speed * 4;
    }
}

void ped_shot(int i, V3 vel) {
    Ped *p = &g_peds[i];
    if (!p->active || p->knock > 0 || p->respawn > 0) return;
    knock_ped(p, vel);
}

/* ---------------------------------------------------------------- crates */
void break_crate(int i) {
    Crate *c = &g_crates[i];
    if (!c->alive) return;
    c->alive = 0;
    c->respawn = c->canRespawn ? 150 : 0;
    V3 p = vadd(c->pos, v3(0, 0.6f, 0));
    fx_burst(p, 18, 5, 0.9f, 0.22f, 0.65f, 0.45f, 0.22f, 18);
    audio_play(SND_BREAK, 0.6f, frange(0.9f, 1.1f));
    if (!c->mission) {
        int n = 2 + rand() % 4;
        for (int k = 0; k < n; k++) add_dyn_coin(p, v3(frange(-3, 3), frange(6, 9), frange(-3, 3)));
    }
    mission_on_crate(i);
}
int add_dyn_coin(V3 p, V3 v) {
    for (int i = g_ncoins; i < MAX_COINS; i++) {
        Coin *c = &g_coins[i];
        if (c->alive) continue;
        c->pos = p; c->vel = v; c->alive = 1; c->dyn = 1; c->life = 12; c->value = 1;
        return i;
    }
    return -1;
}
static void crates_update(float dt) {
    for (int i = 0; i < MAX_CRATES; i++) {
        Crate *c = &g_crates[i];
        if (!c->alive) {
            if (c->canRespawn && c->respawn > 0 && i < g_ncrates) {
                c->respawn -= dt;
                if (c->respawn <= 0) c->alive = 1;
            }
            continue;
        }
        if (c->pos.y > 0.01f || c->vy != 0) {
            float sup = ground_height_excl(c->pos.x, c->pos.z, c->pos.y, 0.05f, i);
            if (c->pos.y > sup + 0.01f || c->vy != 0) {
                c->vy -= GRAV * dt;
                c->pos.y += c->vy * dt;
                if (c->pos.y <= sup) { c->pos.y = sup; c->vy = 0; }
            }
        }
    }
}

/* ----------------------------------------------------------------- coins */
static void coins_update(float dt) {
    V3 pp = player_pos();
    int inCar = g_pl.car >= 0;
    float rad = inCar ? 3.2f : 1.4f;
    float mag = (g_prog.upgrades & UP_MAGNET) ? 10.0f : 0;
    V3 center = vadd(pp, v3(0, inCar ? 1.0f : 0.9f, 0));
    for (int i = 0; i < MAX_COINS; i++) {
        Coin *c = &g_coins[i];
        if (!c->alive) {
            if (!c->dyn && c->respawn > 0) {
                c->respawn -= dt;
                if (c->respawn <= 0) c->alive = 1;
            }
            continue;
        }
        if (c->dyn) {
            c->life -= dt;
            if (c->life <= 0) { c->alive = 0; continue; }
            c->vel.y -= GRAV * dt;
            c->pos = vadd(c->pos, vmul(c->vel, dt));
            float g = ground_height(c->pos.x, c->pos.z, c->pos.y, 0.2f) + 0.5f;
            if (c->pos.y < g) { c->pos.y = g; c->vel.y = fabsf(c->vel.y) * 0.4f; c->vel.x *= 0.7f; c->vel.z *= 0.7f; }
        }
        if (c->dyn && c->life > 11.6f) continue;
        V3 d = vsub(center, c->pos);
        float d2 = vdot(d, d);
        if (mag > 0 && d2 < mag * mag) {
            float l = sqrtf(d2);
            float stp = fminf(l, 16 * dt);
            if (l > 0.01f) c->pos = vadd(c->pos, vmul(d, stp / l));
            d = vsub(center, c->pos);
            d2 = vdot(d, d);
        }
        if (d2 < rad * rad) {
            c->alive = 0;
            if (!c->dyn) c->respawn = 240;
            add_coins(c->value);
            audio_play(SND_COIN, 0.45f, frange(0.97f, 1.08f));
            fx_burst(c->pos, 5, 2, 0.4f, 0.12f, 1, 0.9f, 0.3f, 0);
        }
    }
    for (int i = 0; i < NUM_GIZMOS; i++) {
        if (g_prog.gizmos & (1u << i)) continue;
        if (dist3(center, g_gizmoPos[i]) < (inCar ? 3.2f : 1.7f)) {
            g_prog.gizmos |= 1u << i;
            add_coins(100);
            int n = 0;
            for (int k = 0; k < NUM_GIZMOS; k++) if (g_prog.gizmos & (1u << k)) n++;
            hud_msg(3, "GOLDEN GIZMO FOUND!  %d OF %d   +100 COINS", n, NUM_GIZMOS);
            audio_play(SND_GIZMO, 0.7f, 1);
            fx_burst(g_gizmoPos[i], 30, 5, 1.0f, 0.2f, 0.8f, 0.5f, 1, 0);
            save_game();
        }
    }
}

/* ---------------------------------------------------------------- player */
float player_max_health(void) { return 100.0f + 25.0f * g_prog.vitality; }
static void foot_update(float dt, int jumpPressed) {
    Player *p = &g_pl;
    float ix = (act_down(ACT_RIGHT) ? 1.0f : 0.0f) - (act_down(ACT_LEFT) ? 1.0f : 0.0f);
    float iz = (act_down(ACT_FORWARD) ? 1.0f : 0.0f) - (act_down(ACT_BACK) ? 1.0f : 0.0f);
    if (p->dead) { ix = iz = 0; jumpPressed = 0; }
    if (p->knock > 0) { ix = iz = 0; p->knock -= dt; jumpPressed = 0; }
    V3 f = fwd_of(g_camYaw), r = right_of(g_camYaw);
    V3 dir = vadd(vmul(f, iz), vmul(r, ix));
    float m = vlen(dir);
    if (m > 1) dir = vmul(dir, 1 / m);
    float spd = act_down(ACT_SPRINT) ? 12.5f : 7.5f;
    if (p->aiming) spd = 4.8f;
    if (p->chute > 0) spd = 7.0f;
    float acc = p->onGround ? 70.0f : 22.0f;
    if (p->knock > 0) acc = p->onGround ? 10 : 0;
    float dvx = dir.x * spd - p->vel.x, dvz = dir.z * spd - p->vel.z;
    float dl = sqrtf(dvx * dvx + dvz * dvz), mx = acc * dt;
    if (dl > mx) { dvx *= mx / dl; dvz *= mx / dl; }
    p->vel.x += dvx;
    p->vel.z += dvz;
    if (p->aiming && !p->dead) {
        p->yaw = wrap_pi(p->yaw + wrap_pi(g_camYaw - p->yaw) * clampf(20 * dt, 0, 1));
    } else if (m > 0.1f && p->knock <= 0) {
        float ty = atan2f(dir.x, dir.z);
        p->yaw = wrap_pi(p->yaw + wrap_pi(ty - p->yaw) * clampf(14 * dt, 0, 1));
    }
    if (jumpPressed) {
        if (p->onGround) {
            p->vel.y = JUMP_V;
            p->onGround = 0;
            p->jumps = 1;
            audio_play(SND_JUMP, 0.4f, 1);
        } else if (p->chute <= 0 && p->vel.y < -9) {
            p->chute = 0.01f;
            audio_play(SND_DOOR, 0.5f, 0.6f);
        } else if ((g_prog.upgrades & UP_DOUBLEJUMP) && p->jumps < 2) {
            p->vel.y = JUMP_V * 0.9f;
            p->jumps = 2;
            audio_play(SND_JUMP, 0.4f, 1.3f);
            fx_burst(p->pos, 8, 2, 0.4f, 0.2f, 0.9f, 0.9f, 1, 0);
        }
    }
    if (p->chute < 0 && p->vel.y < -12) { p->chute = 0.01f; audio_play(SND_DOOR, 0.5f, 0.6f); }
    p->vel.y -= GRAV * dt;
    if (p->chute > 0) { p->chute += dt; if (p->vel.y < -4) p->vel.y = lerpf(p->vel.y, -4, clampf(dt * 6, 0, 1)); }
    float oldY = p->pos.y, vyBefore = p->vel.y;
    p->pos.x += p->vel.x * dt;
    p->pos.z += p->vel.z * dt;
    p->pos.y += p->vel.y * dt;
    collide_circle(&p->pos, 0.4f, 0.45f, 1.8f, NULL);
    if (p->vel.y > 0) {
        float c = ceiling_height(p->pos.x, p->pos.z, oldY + 1.8f);
        if (p->pos.y + 1.8f > c) { p->pos.y = c - 1.8f; p->vel.y = 0; }
    }
    float g = ground_height(p->pos.x, p->pos.z, fmaxf(oldY, p->pos.y), 0.45f);
    if (p->pos.y <= g) {
        if (!p->onGround && vyBefore < -16) fx_burst(p->pos, 6, 2, 0.4f, 0.3f, 0.8f, 0.75f, 0.6f, 0);
        if (!p->onGround && vyBefore < -19 && p->chute <= 0) player_damage((-vyBefore - 19) * 4.5f, vadd(p->pos, v3(0, -1, 0)));
        p->pos.y = g;
        if (p->vel.y < 0) p->vel.y = 0;
        p->onGround = 1;
        p->jumps = 0;
        p->chute = 0;
    } else if (p->onGround && p->vel.y <= 0 && p->pos.y - g < 0.4f) {
        p->pos.y = g;
        p->vel.y = 0;
    } else p->onGround = 0;
    float hs = sqrtf(p->vel.x * p->vel.x + p->vel.z * p->vel.z);
    p->anim += hs * dt * 1.5f;
    if (p->pos.y < -10) { p->pos = g_spots.home; p->vel = v3(0, 0, 0); }
}

static void foot_vs_cars(void) {
    for (int i = 0; i < MAX_CARS; i++) {
        Car *c = &g_cars[i];
        if (!c->used || fabsf(c->pos.y - g_pl.pos.y) > 1.8f) continue;
        const CarType *t = &g_carTypes[c->type];
        V3 f = fwd_of(c->yaw);
        V3 d = vsub(g_pl.pos, c->pos);
        float a = clampf(vdot(d, f), -t->len * 0.5f, t->len * 0.5f);
        V3 cp = vadd(c->pos, vmul(f, a));
        float dd = distxz(g_pl.pos, cp), rad = t->wid * 0.5f + 0.4f;
        if (dd >= rad) continue;
        float sp = car_speed(c);
        if (sp > 6 && g_pl.knock <= 0) {
            player_damage(sp * 1.1f, c->pos);
            g_pl.vel = v3(c->vel.x * 0.6f, 7, c->vel.z * 0.6f);
            g_pl.knock = 1.2f;
            g_pl.onGround = 0;
            audio_play(SND_KICK, 0.7f, 0.7f);
        }
        V3 n = dd > 1e-4f ? v3((g_pl.pos.x - cp.x) / dd, 0, (g_pl.pos.z - cp.z) / dd) : right_of(c->yaw);
        g_pl.pos.x += n.x * (rad - dd);
        g_pl.pos.z += n.z * (rad - dd);
    }
}
static void cars_vs_peds(void) {
    for (int i = 0; i < MAX_CARS; i++) {
        Car *c = &g_cars[i];
        if (!c->used) continue;
        float sp = car_speed(c);
        if (sp < 4) continue;
        const CarType *t = &g_carTypes[c->type];
        for (int k = 0; k < MAX_PEDS; k++) {
            Ped *p = &g_peds[k];
            if (!p->active || p->knock > 0 || p->respawn > 0) continue;
            if (dist2xz(p->pos, c->pos) > 16) continue;
            V3 f = fwd_of(c->yaw);
            V3 d = vsub(p->pos, c->pos);
            float a = clampf(vdot(d, f), -t->len * 0.5f, t->len * 0.5f);
            if (distxz(p->pos, vadd(c->pos, vmul(f, a))) < t->wid * 0.5f + 0.4f) {
                knock_ped(p, c->vel);
                if (i == g_pl.car) wanted_crime(0.7f, p->pos);
            }
        }
    }
}

int g_godMode;
float g_fovMul = 1;

void put_player_in_car(int ci) {
    if (ci < 0) return;
    Car *c = &g_cars[ci];
    if (c->ai == AI_TRAFFIC && c->fuel > 0.99f) c->fuel = frange(0.45f, 1.0f);
    g_pl.car = ci;
    c->ai = AI_NONE;
    g_pl.pos = c->pos;
    g_pl.chute = 0;
    g_camYaw = c->yaw;
    g_camPitch = 0.26f;
    camSnap = 1;
}
void exit_vehicle(void) {
    Car *c = &g_cars[g_pl.car];
    const CarType *t = &g_carTypes[c->type];
    V3 r = right_of(c->yaw);
    if (is_aircraft(c) && !c->onGround) {
        /* bail out: the parachute opens by itself once you are falling fast */
        g_pl.pos = vadd(c->pos, v3(0, -1.5f, 0));
        g_pl.vel = vmul(c->vel, 0.5f);
        g_pl.vel.y = fminf(c->vy, 0);
        g_pl.onGround = 0;
        g_pl.chute = -1;
        g_pl.yaw = c->yaw;
        c->ai = AI_NONE;
        g_pl.car = -1;
        audio_play(SND_DOOR, 0.6f, 0.8f);
        hud_toast("BAILED OUT!  PRESS [%s] TO OPEN YOUR PARACHUTE", act_key(ACT_JUMP));
        return;
    }
    float side = is_aircraft(c) ? t->wid / 2 + 1.8f : t->wid / 2 + 0.9f;
    V3 cand[3] = {vadd(c->pos, vmul(r, -side)), vadd(c->pos, vmul(r, side)), vadd(c->pos, v3(0, t->hgt + 0.3f, 0))};
    V3 pick = cand[2];
    for (int i = 0; i < 2; i++) {
        V3 q = cand[i];
        collide_circle(&q, 0.4f, 0.45f, 1.8f, NULL);
        if (dist2xz(q, cand[i]) < 0.05f) { pick = cand[i]; break; }
    }
    g_pl.pos = pick;
    g_pl.vel = vmul(c->vel, 0.3f);
    g_pl.vel.y = 0;
    g_pl.yaw = c->yaw;
    g_pl.onGround = 0;
    c->ai = AI_PARKED;
    c->throttle = 0;
    g_pl.car = -1;
    audio_play(SND_DOOR, 0.6f, 1);
}
int nearest_vehicle(V3 p, float maxd) {
    int best = -1;
    float bd = maxd * maxd;
    for (int i = 0; i < MAX_CARS; i++) {
        Car *c = &g_cars[i];
        if (!c->used || c->ai == AI_FLEE || c->ai == AI_RACE || c->ai == AI_BOSS) continue;
        float reach = is_aircraft(c) ? 2.0f : 1.0f;
        if (fabsf(c->pos.y - p.y) > 2.5f) continue;
        float d = dist2xz(c->pos, p) / (reach * reach);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}
static int nearest_car(float maxd) { return nearest_vehicle(g_pl.pos, maxd); }

int spawn_player_car_near(V3 p) {
    V3 pos;
    float yaw;
    if (calledCar >= 0 && calledCar != g_pl.car && g_cars[calledCar].used) remove_car(calledCar);
    road_spot_near(p, &pos, &yaw);
    int i = spawn_car(g_prog.curCar, pos, yaw, AI_PARKED);
    if (i >= 0) {
        g_cars[i].called = 1;
        calledCar = i;
        if (g_prog.carPaint > 0) memcpy(g_cars[i].col, g_paints[g_prog.carPaint], sizeof g_cars[i].col);
    }
    return i;
}
void sim_call_car(void) {
    if (g_pl.car >= 0) { hud_toast("YOU ARE ALREADY IN A VEHICLE"); return; }
    if (g_pl.callCD > 0) { hud_toast("WAIT A MOMENT BEFORE CALLING AGAIN"); return; }
    int i = spawn_player_car_near(g_pl.pos);
    if (i >= 0) {
        hud_toast("YOUR %s IS WAITING ON THE ROAD", g_carTypes[g_prog.curCar].name);
        fx_burst(vadd(g_cars[i].pos, v3(0, 1, 0)), 20, 3, 0.8f, 0.3f, 0.9f, 0.9f, 1, 0);
        audio_play(SND_HORN, 0.5f, 1);
    }
    g_pl.callCD = 4;
}
void sim_clear_mission_cars(void) {
    for (int i = 0; i < MAX_CARS; i++)
        if (g_cars[i].used && g_cars[i].mission && i != g_pl.car) g_cars[i].used = 0;
}

/* ---------------------------------------------------------------- camera */
V3 cam_forward(void) { return vnorm(vsub(g_camLook, g_camPos)); }
static void camera_update(float dt) {
    float sens = 0.0026f * g_set.sens;
    Car *pc = player_car();
    int armed = !pc && g_pl.weapon > 0 && !g_pl.dead;
    int aim = armed && act_down(ACT_AIM);
    float zoom = aim ? weap_zoom(g_pl.weapon) : 1.0f;
    sens /= zoom;
    float mdx = g_mouseDX, mdy = g_mouseDY * (g_set.invertY ? -1.0f : 1.0f);
    g_camYaw -= mdx * sens;
    g_camPitch += mdy * sens;
    float kr = 2.2f * dt / zoom;
    int keyCam = 0;
    if (act_down(ACT_CAM_LEFT)) { g_camYaw += kr; keyCam = 1; }
    if (act_down(ACT_CAM_RIGHT)) { g_camYaw -= kr; keyCam = 1; }
    if (act_down(ACT_CAM_UP)) { g_camPitch -= kr * 0.6f; keyCam = 1; }
    if (act_down(ACT_CAM_DOWN)) { g_camPitch += kr * 0.6f; keyCam = 1; }
    if (fabsf(mdx) + fabsf(mdy) > 0.5f || keyCam) mouseIdle = 0;
    else mouseIdle += dt;
    g_camYaw = wrap_pi(g_camYaw);
    float pmin = armed ? -1.25f : (pc && is_aircraft(pc) ? -0.6f : -0.25f);
    g_camPitch = clampf(g_camPitch, pmin, 1.25f);
    V3 target;
    float dist;
    g_fovMul = lerpf(g_fovMul, 1.0f / zoom, clampf(dt * 12, 0, 1));
    if (pc) {
        const CarType *t = &g_carTypes[pc->type];
        float sp = car_speed(pc);
        if (is_aircraft(pc)) {
            target = vadd(pc->pos, v3(0, 2.4f, 0));
            dist = (t->kind == VK_HELI ? 13.0f : 15.0f) + sp * 0.05f;
            float follow = t->kind == VK_PLANE ? 3.0f : 2.0f;
            if (mouseIdle > 1.0f && (sp > 3 || t->kind == VK_PLANE)) {
                g_camYaw = wrap_pi(g_camYaw + wrap_pi(pc->yaw - g_camYaw) * clampf(dt * follow, 0, 1));
                float pt = t->kind == VK_PLANE ? 0.2f - pc->pitch * 0.7f : 0.3f;
                g_camPitch = lerpf(g_camPitch, pt, clampf(dt * 1.5f, 0, 1));
            }
        } else {
            target = vadd(pc->pos, v3(0, 1.6f, 0));
            dist = 8.5f + sp * 0.06f + (t->len - 4.4f);
            if (mouseIdle > 1.0f && sp > 1.5f) {
                g_camYaw = wrap_pi(g_camYaw + wrap_pi(pc->yaw - g_camYaw) * clampf(dt * 2.5f, 0, 1));
                g_camPitch = lerpf(g_camPitch, 0.26f, clampf(dt * 1.5f, 0, 1));
            }
        }
    } else if (armed) {
        target = vadd(vadd(g_pl.pos, v3(0, 1.65f, 0)), vmul(right_of(g_camYaw), aim ? 0.85f : 0.75f));
        dist = aim ? 2.6f : 5.0f;
    } else {
        target = vadd(g_pl.pos, v3(0, 1.55f, 0));
        dist = 6.2f;
    }
    if (camSnap || dist3(camSm, target) > 15) { camSm = target; camSnap = 0; }
    else camSm = vadd(camSm, vmul(vsub(target, camSm), clampf(dt * (armed ? 22 : 14), 0, 1)));
    V3 off = v3(-sinf(g_camYaw) * cosf(g_camPitch) * dist, sinf(g_camPitch) * dist, -cosf(g_camYaw) * cosf(g_camPitch) * dist);
    V3 want = vadd(camSm, off);
    float t = ray_clear(camSm, want);
    if (t < 1) want = vadd(camSm, vmul(off, fmaxf(0.08f, t - 0.04f)));
    float gy = ground_height(want.x, want.z, want.y, 0) + 0.4f;
    if (want.y < gy) want.y = gy;
    g_camPos = want;
    g_camLook = vadd(camSm, v3(0, 0.3f, 0));
    if (armed) g_camLook = vsub(g_camPos, vmul(off, 2.0f)); /* keep the aim direction even when the camera is pushed */
}

/* ------------------------------------------------------------ lifecycle */
static int spawn_police_car(float minDist) {
    int s = spawn_car(CAR_POLICE, v3(0, 0, 0), 0, AI_POLICE);
    if (s < 0) return -1;
    g_cars[s].topSpeed = 13;
    if (!place_traffic(&g_cars[s], minDist)) { g_cars[s].used = 0; return -1; }
    return s;
}
static void spawn_stand(int k) {
    int type = g_spots.standType[k];
    if (type < 0) type = AIR_FIRST + g_prog.curAir;
    int s = spawn_car(type, g_spots.stand[k], g_spots.standYaw[k], AI_PARKED);
    if (s < 0) return;
    Car *c = &g_cars[s];
    c->stand = k;
    c->locked = !((g_prog.aircraft >> (type - AIR_FIRST)) & 1) && g_carTypes[type].price > 0;
    if (g_spots.standType[k] < 0) c->locked = 0;
    if (g_prog.airPaint > 0 && (g_spots.standType[k] < 0 || !c->locked)) memcpy(c->col, g_paints[g_prog.airPaint], sizeof c->col);
}
static int stand_occupied(int k) {
    for (int i = 0; i < MAX_CARS; i++)
        if (g_cars[i].used && g_cars[i].stand == k && !g_cars[i].wrecked) return 1;
    return 0;
}
static int traffic_target(void) { return g_curMap == 0 ? 24 : 10; }
static void populate(void) {
    memset(g_cars, 0, sizeof g_cars);
    calledCar = -1;
    for (int i = 0; i < g_nparks && i < (g_curMap == 0 ? 24 : 12); i++) {
        int s = spawn_car(rand() % 4, g_parkSpots[i], g_parkYaw[i], AI_PARKED);
        if (s >= 0 && g_cars[s].type != 1 && g_cars[s].type != 3) memcpy(g_cars[s].col, TRAFFIC_COL[rand() % 8], sizeof g_cars[s].col);
        if (s >= 0) g_cars[s].fuel = frange(0.35f, 1);
    }
    for (int k = 0; k < NUM_STANDS; k++) spawn_stand(k);
    spawn_traffic(traffic_target(), 25);
    for (int i = 0; i < (g_curMap == 0 ? 4 : 2); i++) spawn_police_car(60);
    for (int i = 0; i < MAX_PEDS; i++) ped_spawn(&g_peds[i]);
}
void sim_respawn_stands(void) {
    for (int i = 0; i < MAX_CARS; i++)
        if (g_cars[i].used && g_cars[i].stand >= 0 && i != g_pl.car) g_cars[i].used = 0;
    for (int k = 0; k < NUM_STANDS; k++) spawn_stand(k);
}
void sim_init(void) {
    memset(&g_pl, 0, sizeof g_pl);
    g_pl.car = -1;
    g_pl.pos = g_spots.home;
    g_pl.health = 100;
    progress_reset();
    populate();
    g_camYaw = g_spots.homeYaw;
    camSnap = 1;
}
void player_respawn(V3 p) {
    if (g_pl.car >= 0) exit_vehicle();
    g_pl.pos = p;
    g_pl.vel = v3(0, 0, 0);
    g_pl.dead = 0;
    g_pl.busted = 0;
    g_pl.knock = 0;
    g_pl.chute = 0;
    g_pl.health = player_max_health();
    g_pl.onGround = 1;
    camSnap = 1;
}
void sim_new_game(int fromSave) {
    if (fromSave) load_game();
    else progress_reset();
    if (g_curMap != g_prog.map) world_build(g_prog.map);
    mission_abort();
    world_reset_pickups();
    memset(&g_pl, 0, sizeof g_pl);
    g_pl.car = -1;
    g_pl.pos = g_spots.home;
    g_pl.yaw = g_spots.homeYaw;
    g_pl.nitro = 1;
    g_pl.health = player_max_health();
    g_pl.armor = (float)g_prog.bodyArmor;
    memset(parts, 0, sizeof parts);
    combat_reset();
    populate();
    g_wpOn = 0;
    int c = spawn_car(g_prog.curCar, g_spots.homeCar, g_spots.homeCarYaw, AI_PARKED);
    if (c >= 0) {
        g_cars[c].called = 1;
        calledCar = c;
        if (g_prog.carPaint > 0) memcpy(g_cars[c].col, g_paints[g_prog.carPaint], sizeof g_cars[c].col);
    }
    g_camYaw = g_spots.homeYaw;
    g_camPitch = 0.3f;
    camSnap = 1;
    mouseIdle = 0;
    camera_update(0.016f);
}

/* places you can use with the interact key */
enum { PL_NONE, PL_BOLT, PL_ARMORY, PL_HANGAR, PL_TUNER, PL_CLINIC, PL_GUNX, PL_TRAVEL };
static int place_near(V3 p) {
    if (dist2xz(p, g_spots.shop) < 3.5f * 3.5f) return PL_BOLT;
    if (dist2xz(p, g_spots.armory) < 3.5f * 3.5f) return PL_ARMORY;
    if (dist2xz(p, g_spots.hangar) < 4.0f * 4.0f) return PL_HANGAR;
    if (dist2xz(p, g_spots.tuner) < 3.5f * 3.5f) return PL_TUNER;
    if (dist2xz(p, g_spots.gunx) < 3.5f * 3.5f) return PL_GUNX;
    if (dist2xz(p, g_spots.travel) < 3.5f * 3.5f) return PL_TRAVEL;
    for (int i = 0; i < g_spots.nhosp; i++)
        if (dist2xz(p, g_spots.hosp[i]) < 3.5f * 3.5f) return PL_CLINIC;
    return PL_NONE;
}
V3 nearest_hospital(V3 p) {
    V3 best = g_spots.home;
    float bd = 1e18f;
    for (int i = 0; i < g_spots.nhosp; i++)
        if (dist2xz(p, g_spots.hosp[i]) < bd) { bd = dist2xz(p, g_spots.hosp[i]); best = g_spots.hosp[i]; }
    return best;
}
#define HEAL_PRICE 40
static int gas_near(const Car *c) {
    for (int i = 0; i < NUM_GAS; i++)
        if (dist2xz(c->pos, g_spots.gas[i]) < 10.0f * 10.0f && c->pos.y - g_spots.gas[i].y < 3) return i;
    return -1;
}
static void update_prompt(void) {
    g_prompt[0] = 0;
    if (g_pl.dead) return;
    if (g_pl.car >= 0) {
        Car *c = &g_cars[g_pl.car];
        if (gas_near(c) >= 0) {
            if (c->fuel >= 0.999f) snprintf(g_prompt, sizeof g_prompt, "TANK IS FULL");
            else if (car_speed(c) > 2) snprintf(g_prompt, sizeof g_prompt, "STOP HERE TO REFUEL");
            else if (g_prog.coins <= 0) snprintf(g_prompt, sizeof g_prompt, "NO COINS FOR FUEL!");
            else snprintf(g_prompt, sizeof g_prompt, "REFUELING... %d%%", (int)(c->fuel * 100));
        }
        return;
    }
    int m = mission_active() ? -1 : mission_near_giver(g_pl.pos);
    if (m >= 0) {
        snprintf(g_prompt, sizeof g_prompt, "[%s] TALK TO %s", act_key(ACT_INTERACT), mission_giver(m));
        return;
    }
    switch (place_near(g_pl.pos)) {
    case PL_BOLT: snprintf(g_prompt, sizeof g_prompt, "[%s] SHOP AT BOLT & BARGAIN", act_key(ACT_INTERACT)); return;
    case PL_ARMORY: snprintf(g_prompt, sizeof g_prompt, "[%s] SHOP AT BOOM BROS ARMORY", act_key(ACT_INTERACT)); return;
    case PL_HANGAR: snprintf(g_prompt, sizeof g_prompt, "[%s] SHOP AT SKYLINE HANGAR", act_key(ACT_INTERACT)); return;
    case PL_TUNER: snprintf(g_prompt, sizeof g_prompt, "[%s] SHOP AT TURBO TUNERS", act_key(ACT_INTERACT)); return;
    case PL_GUNX: snprintf(g_prompt, sizeof g_prompt, "[%s] BUY, SELL & UPGRADE WEAPONS AT LOCK & LOAD", act_key(ACT_INTERACT)); return;
    case PL_TRAVEL: snprintf(g_prompt, sizeof g_prompt, "[%s] BOOK A FLIGHT TO ANOTHER ISLAND", act_key(ACT_INTERACT)); return;
    case PL_CLINIC:
        if (g_pl.health >= player_max_health() - 0.5f) snprintf(g_prompt, sizeof g_prompt, "MAPLE MEDICAL - YOU ARE IN PERFECT HEALTH");
        else snprintf(g_prompt, sizeof g_prompt, "[%s] GET PATCHED UP - %d COINS", act_key(ACT_INTERACT), HEAL_PRICE);
        return;
    }
    int ci = nearest_car(4.2f);
    if (ci >= 0) {
        Car *c = &g_cars[ci];
        if (c->wrecked) snprintf(g_prompt, sizeof g_prompt, "THIS %s IS WRECKED", is_aircraft(c) ? "AIRCRAFT" : "CAR");
        else if (c->locked) snprintf(g_prompt, sizeof g_prompt, "%s - BUY IT AT SKYLINE HANGAR TO FLY", g_carTypes[c->type].name);
        else snprintf(g_prompt, sizeof g_prompt, "[%s] %s %s", act_key(ACT_INTERACT),
                      c->ai == AI_TRAFFIC || c->ai == AI_POLICE ? "BORROW" : (is_aircraft(c) ? "FLY" : "ENTER"), g_carTypes[c->type].name);
    }
}

static void manage_cars(float dt) {
    int traffic = 0, police = 0;
    V3 pp = player_pos();
    for (int i = 0; i < MAX_CARS; i++) {
        Car *c = &g_cars[i];
        if (!c->used) continue;
        if (c->wrecked) {
            c->wreckT += dt;
            c->smokeT -= dt;
            if (c->smokeT <= 0) {
                c->smokeT = 0.08f;
                spawn_part(vadd(c->pos, v3(frange(-0.5f, 0.5f), 1.4f, frange(-0.5f, 0.5f))), v3(frange(-0.5f, 0.5f), 2.5f, frange(-0.5f, 0.5f)), 2.0f, 0.5f, 0.2f, 0.2f, 0.2f, -0.5f, 1.0f);
            }
            if (c->wreckT > 25 && !c->mission && i != g_pl.car && distxz(c->pos, pp) > 40) remove_car(i);
            continue;
        }
        if (c->health < c->maxHealth * 0.35f) {
            c->smokeT -= dt;
            if (c->smokeT <= 0) {
                c->smokeT = 0.15f;
                V3 f = fwd_of(c->yaw);
                spawn_part(vadd(c->pos, vadd(vmul(f, g_carTypes[c->type].len * 0.4f), v3(0, 1.1f, 0))), v3(0, 2, 0), 1.2f, 0.3f, 0.5f, 0.5f, 0.5f, -0.5f, 0.8f);
            }
        }
        if (c->ai == AI_TRAFFIC || c->ai == AI_POLICE) {
            if (c->ai == AI_TRAFFIC) traffic++;
            else police++;
            float d = distxz(c->pos, pp);
            if (c->stuck > 5 || d > 320) {
                if (!place_traffic(c, 90)) c->stuck = 0;
            }
        }
        /* aircraft left lying around far from the airport go back to their stand */
        if (c->stand >= 0 && i != g_pl.car && c->ai != AI_BOSS && dist2xz(c->pos, g_spots.stand[c->stand]) > 400 &&
            distxz(c->pos, pp) > 150 && distxz(g_spots.stand[c->stand], pp) > 120 && c->onGround) {
            c->pos = g_spots.stand[c->stand];
            c->yaw = g_spots.standYaw[c->stand];
            c->vel = v3(0, 0, 0);
            c->vy = 0;
            c->aspd = 0;
            c->pitch = c->roll = 0;
            c->fuel = 1;
            c->ai = AI_PARKED;
        }
    }
    if (traffic < traffic_target()) spawn_traffic(1, 90);
    int wantPolice = (g_curMap == 0 ? 4 : 2) + 2 * wanted_stars();
    if (wantPolice > 11) wantPolice = 11;
    if (police < wantPolice) spawn_police_car(wanted_stars() ? 110 : 90);
    static float standT;
    standT += dt;
    if (standT > 2) {
        standT = 0;
        for (int k = 0; k < NUM_STANDS; k++)
            if (!stand_occupied(k) && distxz(g_spots.stand[k], pp) > 120) {
                for (int i = 0; i < MAX_CARS; i++)
                    if (g_cars[i].used && g_cars[i].stand == k && i != g_pl.car) g_cars[i].stand = -1;
                spawn_stand(k);
            }
    }
}

static void vehicle_fuel(Car *c, float dt, float thr) {
    float use = is_aircraft(c) ? vehicle_fuel_use(c) : (0.0006f + 0.0035f * fabsf(thr)) * car_up_mul(3);
    float before = c->fuel;
    c->fuel = fmaxf(0, c->fuel - use * dt);
    if (before > 0.15f && c->fuel <= 0.15f) { hud_toast("LOW FUEL - FIND A GAS STATION (F ON THE MAP)"); audio_play(SND_BEEP, 0.5f, 0.8f); }
    if (before > 0 && c->fuel <= 0) hud_msg(3, "OUT OF FUEL!");
}
static void refuel_update(float dt) {
    if (g_pl.car < 0) return;
    Car *c = &g_cars[g_pl.car];
    if (gas_near(c) < 0 || car_speed(c) > 2 || c->fuel >= 0.999f || g_prog.coins <= 0) return;
    float add = fminf(0.25f * dt, 1 - c->fuel);
    c->fuel += add;
    g_pl.refuelAcc += add;
    while (g_pl.refuelAcc >= 0.03f) { g_pl.refuelAcc -= 0.03f; g_prog.coins--; }
    static float snd;
    snd -= dt;
    if (snd <= 0) { snd = 0.35f; audio_play(SND_FUEL, 0.35f, 1); }
    if (c->fuel >= 0.999f) { c->fuel = 1; audio_play(SND_CHECK, 0.5f, 1); }
}

void travel_to(int map, int byPlane) {
    Car *pc = player_car();
    int airType = -1;
    float fuel = 1, hp = 1;
    if (byPlane && pc && is_aircraft(pc)) { airType = pc->type; fuel = pc->fuel; hp = pc->health / pc->maxHealth; }
    int weapon = g_pl.weapon, clip[8];
    memcpy(clip, g_pl.clip, sizeof clip);
    float health = g_pl.health, armor = g_pl.armor;
    mission_abort();
    g_pl.car = -1;
    world_build(map);
    world_reset_pickups();
    g_prog.map = map;
    memset(parts, 0, sizeof parts);
    combat_reset();
    g_pl.weapon = weapon;
    memcpy(g_pl.clip, clip, sizeof clip);
    g_pl.health = health;
    g_pl.armor = armor;
    populate();
    g_wpOn = 0;
    g_pl.vel = v3(0, 0, 0);
    g_pl.chute = 0;
    g_pl.dead = 0;
    if (airType >= 0) {
        int ci = spawn_car(airType, v3(0, 85, AIR_Z1 + 140), PI, AI_NONE);
        if (ci >= 0) {
            Car *c = &g_cars[ci];
            c->onGround = 0;
            c->aspd = g_carTypes[airType].maxSpeed * 0.7f;
            c->throttle = 0.7f;
            c->fuel = fmaxf(fuel, 0.35f);
            c->health = c->maxHealth * hp;
            c->vel = vmul(fwd_of(PI), c->aspd);
            put_player_in_car(ci);
            g_camYaw = PI;
        }
    } else {
        g_pl.pos = vadd(g_spots.travel.x < 50000 ? g_spots.travel : g_spots.home, v3(0, 0, 4));
        g_pl.yaw = 0;
        g_camYaw = 0;
    }
    if (g_pl.car < 0) g_pl.onGround = 1;
    g_camPitch = 0.3f;
    camSnap = 1;
    mouseIdle = 0;
    camera_update(0.016f);
    save_game();
    hud_msg(4, "WELCOME TO %s!", map_name(map));
}

void sim_update(float dt) {
    g_gameTime += dt;
    g_prog.playTime += dt;
    int frozen = mission_frozen();
    int jumpP = act_pressed(ACT_JUMP);
    if (g_pl.callCD > 0) g_pl.callCD -= dt;
    if (g_pl.kick > 0) g_pl.kick -= dt;
    if (g_pl.dead) jumpP = 0;
    if (act_pressed(ACT_CALLCAR) && !g_pl.dead) sim_call_car();
    if (g_pl.car >= 0) {
        Car *c = &g_cars[g_pl.car];
        if (act_pressed(ACT_HORN) && !is_aircraft(c)) audio_play(SND_HORN, 0.6f, 1);
        if (act_pressed(ACT_RESETCAR) && !c->wrecked && (c->onGround || !is_aircraft(c))) {
            c->pos.y += 1.5f;
            c->vel = v3(0, 0, 0);
            c->vy = 0;
            c->aspd = 0;
            c->onGround = 0;
            c->pitch = c->roll = 0;
        }
    }
    if (act_pressed(ACT_INTERACT) && !g_pl.dead) {
        if (g_pl.car >= 0) {
            Car *c = &g_cars[g_pl.car];
            if (car_speed(c) < 14 || (is_aircraft(c) && !c->onGround)) exit_vehicle();
        } else {
            int m = mission_active() ? -1 : mission_near_giver(g_pl.pos);
            int pl = place_near(g_pl.pos);
            if (m >= 0) ui_open_dialog(m);
            else if (pl == PL_BOLT) ui_open_shop(SHOP_BOLT);
            else if (pl == PL_ARMORY) ui_open_shop(SHOP_ARMORY);
            else if (pl == PL_HANGAR) ui_open_shop(SHOP_HANGAR);
            else if (pl == PL_TUNER) ui_open_shop(SHOP_TUNER);
            else if (pl == PL_GUNX) ui_open_shop(SHOP_GUNX);
            else if (pl == PL_TRAVEL) ui_open_travel(0);
            else if (pl == PL_CLINIC) {
                if (g_pl.health >= player_max_health() - 0.5f) hud_toast("THE DOCTOR SAYS YOU ARE FINE");
                else if (g_prog.coins < HEAL_PRICE) { hud_toast("YOU NEED %d COINS", HEAL_PRICE); audio_play(SND_DENY, 0.6f, 1); }
                else { g_prog.coins -= HEAL_PRICE; g_pl.health = player_max_health(); audio_play(SND_HEAL, 0.6f, 1); hud_toast("ALL PATCHED UP!"); }
            } else {
                int ci = nearest_car(4.2f);
                if (ci >= 0) {
                    Car *c = &g_cars[ci];
                    if (c->wrecked) hud_toast("IT IS WRECKED");
                    else if (c->locked) { hud_toast("BUY THE %s AT SKYLINE HANGAR FIRST", g_carTypes[c->type].name); audio_play(SND_DENY, 0.5f, 1); }
                    else {
                        if (c->ai == AI_POLICE) wanted_crime(2.0f, c->pos);
                        else if (c->ai == AI_TRAFFIC) wanted_crime(0.6f, c->pos);
                        put_player_in_car(ci);
                        audio_play(SND_DOOR, 0.6f, 1.1f);
                        if (is_aircraft(c)) hud_toast("%s: [%s]/[%s] THROTTLE  [%s]/[%s] CLIMB/DIVE", g_carTypes[c->type].name,
                                                      act_key(ACT_FORWARD), act_key(ACT_BACK), act_key(ACT_ASCEND), act_key(ACT_DESCEND));
                    }
                }
            }
        }
    }
    if (g_pl.car < 0 && !g_pl.dead && (act_pressed(ACT_KICK) || (g_pl.weapon == W_FISTS && act_pressed(ACT_FIRE))) && g_pl.kick <= 0) {
        g_pl.kick = 0.35f;
        audio_play(SND_KICK, 0.35f, 1.3f);
        V3 f = fwd_of(g_pl.yaw);
        for (int i = 0; i < MAX_CRATES; i++) {
            Crate *c = &g_crates[i];
            if (!c->alive) continue;
            V3 d = vsub(c->pos, g_pl.pos);
            if (fabsf(d.y) > 1.4f || d.x * d.x + d.z * d.z > 2.2f * 2.2f) continue;
            if (vdot(vnorm(v3(d.x, 0, d.z)), f) > 0.1f) break_crate(i);
        }
        for (int i = 0; i < MAX_PEDS; i++) {
            Ped *p = &g_peds[i];
            if (!p->active || p->knock > 0 || p->respawn > 0) continue;
            if (dist2xz(p->pos, g_pl.pos) < 2.0f * 2.0f) { knock_ped(p, vmul(f, 6)); wanted_crime(0.4f, p->pos); }
        }
        fire_hitscan(vadd(g_pl.pos, v3(0, 1.0f, 0)), f, 1.9f, 22, 1, -1, NULL);
    }

    int n = (int)ceilf(dt * 120.0f);
    if (n < 1) n = 1;
    if (n > 10) n = 10;
    float h = dt / n;
    for (int s = 0; s < n; s++) {
        if (g_pl.car < 0) {
            foot_update(h, s == 0 && jumpP);
        } else {
            Car *c = &g_cars[g_pl.car];
            if (is_aircraft(c)) {
                air_control(c, h, frozen || g_pl.dead);
                g_pl.pos = c->pos;
            } else {
                float thr = (act_down(ACT_FORWARD) ? 1.0f : 0.0f) - (act_down(ACT_BACK) ? 1.0f : 0.0f);
                float st = (act_down(ACT_RIGHT) ? 1.0f : 0.0f) - (act_down(ACT_LEFT) ? 1.0f : 0.0f);
                int hb = act_down(ACT_HANDBRAKE), boost = 0;
                if (c->fuel <= 0 && thr > 0) thr = 0;
                if ((g_prog.upgrades & UP_NITRO) && act_down(ACT_SPRINT) && g_pl.nitro > 0.02f && thr > 0 && !frozen) {
                    boost = 1;
                    g_pl.nitro -= h * 0.3f;
                    if (s == 0) {
                        V3 bp = vsub(c->pos, vmul(fwd_of(c->yaw), g_carTypes[c->type].len * 0.55f));
                        spawn_part(vadd(bp, v3(0, 0.6f, 0)), vmul(fwd_of(c->yaw), -6), 0.25f, 0.35f, 0.3f, 0.6f, 1, 0, -0.5f);
                        spawn_part(vadd(bp, v3(0, 0.6f, 0)), vmul(fwd_of(c->yaw), -4), 0.3f, 0.25f, 1, 0.7f, 0.2f, 0, -0.3f);
                    }
                } else g_pl.nitro = fminf(1, g_pl.nitro + h * 0.07f);
                if (frozen || g_pl.dead) { thr = 0; hb = 1; }
                car_physics(c, h, thr, st, hb, boost);
                if (!frozen) vehicle_fuel(c, h, thr);
                g_pl.pos = c->pos;
            }
        }
        for (int i = 0; i < MAX_CARS; i++) {
            Car *c = &g_cars[i];
            if (!c->used || i == g_pl.car) continue;
            ai_update(c, h, frozen);
        }
        cars_collide();
        if (g_pl.car < 0) foot_vs_cars();
        else g_pl.pos = g_cars[g_pl.car].pos;
    }
    if (g_pl.car >= 0 && is_aircraft(&g_cars[g_pl.car]) && !frozen) vehicle_fuel(&g_cars[g_pl.car], dt, g_cars[g_pl.car].throttle);
    refuel_update(dt);
    cars_vs_peds();
    peds_update(dt);
    coins_update(dt);
    crates_update(dt);
    parts_update(dt);
    manage_cars(dt);
    combat_update(dt);
    {
        /* flying out over the open sea offers a flight to another island */
        static int armed = 1;
        Car *pc = player_car();
        V3 pp = player_pos();
        float H = CITY_HALF;
        int inside = fabsf(pp.x) < H + 250 && pp.z > -H - 250 && pp.z < AIR_Z1 + 150;
        int out = fabsf(pp.x) > H + 380 || pp.z < -H - 380 || pp.z > AIR_Z1 + 170;
        if (inside) armed = 1;
        if (armed && out && pc && is_aircraft(pc) && !pc->onGround && !pc->wrecked && !mission_active()) {
            armed = 0;
            ui_open_travel(1);
        }
        if (g_wpOn && distxz(pp, g_wpPos) < 12) { g_wpOn = 0; hud_toast("YOU REACHED YOUR WAYPOINT"); audio_play(SND_CHECK, 0.5f, 1); }
    }
    mission_update(dt);
    camera_update(dt);
    update_prompt();
    if (g_pl.car >= 0) {
        Car *c = &g_cars[g_pl.car];
        const CarType *t = &g_carTypes[c->type];
        if (is_aircraft(c)) {
            audio_engine_kind(t->kind == VK_HELI ? 2 : 1);
            float rpm = c->fuel > 0 ? 0.3f + c->throttle * 0.9f : 0;
            audio_engine(c->wrecked ? 0 : rpm, c->wrecked || c->fuel <= 0 ? 0 : 0.8f);
        } else {
            audio_engine_kind(0);
            float rpm = fabsf(car_vf(c)) / t->maxSpeed;
            if (act_down(ACT_FORWARD) && !c->wrecked && c->fuel > 0) rpm += 0.15f;
            audio_engine(c->wrecked ? 0 : rpm, c->wrecked || c->fuel <= 0 ? 0 : 0.7f);
        }
    } else audio_engine(0, 0);
}

void sim_ambient(float dt) {
    int n = (int)ceilf(dt * 60.0f);
    if (n < 1) n = 1;
    if (n > 5) n = 5;
    float h = dt / n;
    for (int s = 0; s < n; s++) {
        for (int i = 0; i < MAX_CARS; i++)
            if (g_cars[i].used && i != g_pl.car) ai_update(&g_cars[i], h, 0);
        cars_collide();
    }
    cars_vs_peds();
    peds_update(dt);
    parts_update(dt);
    manage_cars(dt);
    g_gameTime += dt;
    audio_engine(0, 0);
}

/* ---------------------------------------------------------------- render */
static void billboard_text(const char *s, V3 p, float px, float r, float g, float b) {
    float w = text3d_width(s, px);
    V3 o = vsub(p, vmul(g_camRight, w / 2));
    glColor4f(0, 0, 0, 0.6f);
    text3d(s, vadd(o, vadd(vmul(g_camRight, px * 0.5f), vmul(g_camUp, -px * 0.5f))), g_camRight, g_camUp, px);
    glColor4f(r, g, b, 1);
    text3d(s, o, g_camRight, g_camUp, px);
}
static void draw_clouds(void) {
    static const float cl[10][4] = {{-200, 150, -300, 40}, {150, 160, -250, 55}, {350, 140, 100, 45}, {-350, 155, 200, 50}, {0, 170, 380, 60}, {250, 150, 350, 40}, {-150, 165, -500, 70}, {500, 160, -100, 60}, {-500, 150, -50, 55}, {80, 175, 50, 35}};
    glColor3f(1, 1, 1);
    for (int i = 0; i < 10; i++) {
        float x = cl[i][0] + g_gameTime * 1.5f, y = cl[i][1], z = cl[i][2], s = cl[i][3];
        x = fmodf(x + 700, 1400) - 700;
        gl_cube(x, y, z, s, s * 0.12f, s * 0.5f);
        gl_cube(x + s * 0.3f, y + s * 0.1f, z + s * 0.1f, s * 0.5f, s * 0.12f, s * 0.35f);
        gl_cube(x - s * 0.4f, y - s * 0.02f, z - s * 0.15f, s * 0.45f, s * 0.1f, s * 0.3f);
    }
}
static void draw_parachute(V3 p, float open) {
    float k = clampf(open * 3, 0.2f, 1);
    glPushMatrix();
    glTranslatef(p.x, p.y, p.z);
    for (int i = 0; i < 6; i++) {
        glColor3f(i & 1 ? 1.0f : 0.95f, i & 1 ? 1.0f : 0.35f, i & 1 ? 1.0f : 0.2f);
        float a0 = i * PI / 3, a1 = (i + 1) * PI / 3;
        glBegin(GL_TRIANGLES);
        glNormal3f(0, 1, 0);
        glVertex3f(0, 6.2f, 0);
        glVertex3f(cosf(a0) * 3.2f * k, 5.0f, sinf(a0) * 3.2f * k);
        glVertex3f(cosf(a1) * 3.2f * k, 5.0f, sinf(a1) * 3.2f * k);
        glEnd();
    }
    glColor3f(0.2f, 0.2f, 0.2f);
    glBegin(GL_LINES);
    for (int i = 0; i < 6; i++) { float a = i * PI / 3; glVertex3f(0, 1.5f, 0); glVertex3f(cosf(a) * 3.2f * k, 5.0f, sinf(a) * 3.2f * k); }
    glEnd();
    glPopMatrix();
}
static void shopkeeper(V3 p, float yaw, int look) {
    draw_character(p, yaw, g_gameTime * 2, 0.0f, &g_npcLooks[look], 0, 0);
}
void sim_render(void) {
    float aspect = (float)g_winW / (float)(g_winH > 0 ? g_winH : 1);
    const float *hz = g_skyHz;
    ui_begin();
    glBegin(GL_QUADS);
    glColor3f(g_skyTop[0], g_skyTop[1], g_skyTop[2]);
    glVertex2f(0, 0); glVertex2f((float)g_winW, 0);
    glColor3f(hz[0], hz[1], hz[2]);
    glVertex2f((float)g_winW, (float)g_winH * 0.6f); glVertex2f(0, (float)g_winH * 0.6f);
    glVertex2f(0, (float)g_winH * 0.6f); glVertex2f((float)g_winW, (float)g_winH * 0.6f);
    glVertex2f((float)g_winW, (float)g_winH); glVertex2f(0, (float)g_winH);
    glEnd();
    glDisable(GL_BLEND);
    glClear(GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    set_perspective((float)g_set.fov * g_fovMul, aspect, 0.3f, 1500);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    look_at(g_camPos, g_camLook);
    setup_light();
    glDisable(GL_CULL_FACE);
    glShadeModel(GL_SMOOTH);

    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    draw_clouds();
    GLfloat fc[4] = {hz[0], hz[1], hz[2], 1};
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogfv(GL_FOG_COLOR, fc);
    glFogf(GL_FOG_START, 180 + fmaxf(0, g_camPos.y - 20) * 2);
    glFogf(GL_FOG_END, 560 + fmaxf(0, g_camPos.y - 20) * 3);
    glEnable(GL_FOG);
    glDisable(GL_DEPTH_TEST);
    world_draw_ground();
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_LIGHTING);
    world_draw_static();
    world_draw_dynamic(g_gameTime, g_camPos);
    for (int i = 0; i < MAX_CARS; i++) {
        Car *c = &g_cars[i];
        if (!c->used || dist2xz(c->pos, g_camPos) > (is_aircraft(c) ? 600 * 600 : 330 * 330)) continue;
        draw_car(c);
    }
    if (g_pl.car < 0) {
        float hs = sqrtf(g_pl.vel.x * g_pl.vel.x + g_pl.vel.z * g_pl.vel.z);
        if (g_pl.dead) {
            glPushMatrix();
            glTranslatef(g_pl.pos.x, g_pl.pos.y + 0.2f, g_pl.pos.z);
            glRotatef(g_pl.yaw * DEG, 0, 1, 0);
            glRotatef(-90, 1, 0, 0);
            draw_character(v3(0, -0.9f, 0), 0, 0, 0, &g_looks[g_prog.outfit], 0, 1);
            glPopMatrix();
        } else {
            float ap = g_pl.aiming ? asinf(clampf(cam_forward().y, -1, 1)) : 0;
            draw_character_ex(g_pl.pos, g_pl.yaw, g_pl.anim, clampf(hs / 7.5f, 0, 1.2f), &g_looks[g_prog.outfit], g_pl.kick, !g_pl.onGround,
                              g_pl.weapon, ap);
            if (g_pl.chute > 0) draw_parachute(g_pl.pos, g_pl.chute);
        }
    }
    for (int i = 0; i < MAX_PEDS; i++) {
        Ped *p = &g_peds[i];
        if (!p->active || p->respawn > 0 || dist2xz(p->pos, g_camPos) > 150 * 150) continue;
        if (p->knock > 0) {
            glPushMatrix();
            glTranslatef(p->pos.x, p->pos.y + 0.9f, p->pos.z);
            glRotatef(p->yaw * DEG, 0, 1, 0);
            glRotatef(p->down ? -90.0f : p->spin * DEG, 1, 0, 0);
            draw_character(v3(0, p->down ? -0.75f : -0.9f, 0), 0, 0, 0, &g_npcLooks[p->look], 0, 1);
            glPopMatrix();
        } else draw_character(p->pos, p->yaw, p->anim, 0.6f, &g_npcLooks[p->look], 0, 0);
    }
    shopkeeper(g_spots.shop, g_spots.shopYaw, 8);
    shopkeeper(g_spots.armory, g_spots.armoryYaw, 10);
    shopkeeper(g_spots.hangar, g_spots.hangarYaw, 11);
    shopkeeper(g_spots.tuner, g_spots.tunerYaw, 3);
    shopkeeper(g_spots.clinic, g_spots.clinicYaw, 1);
    shopkeeper(g_spots.gunx, g_spots.gunxYaw, 6);
    shopkeeper(g_spots.travel, 0, 11);
    combat_render();
    mission_draw3d(g_gameTime);

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    if (g_pl.car < 0) blob_shadow(g_pl.pos.x, ground_height(g_pl.pos.x, g_pl.pos.z, g_pl.pos.y + 0.1f, 0.2f), g_pl.pos.z, 0.6f, 0.6f, 0, 0.45f);
    for (int i = 0; i < MAX_CARS; i++) {
        Car *c = &g_cars[i];
        if (!c->used || dist2xz(c->pos, g_camPos) > 150 * 150) continue;
        const CarType *t = &g_carTypes[c->type];
        float gy = ground_height(c->pos.x, c->pos.z, c->pos.y + 0.1f, 0.3f);
        float a = clampf(0.45f - (c->pos.y - gy) * 0.006f, 0.1f, 0.45f);
        float sw = is_aircraft(c) ? (t->kind == VK_PLANE ? 4.5f : 3.0f) : t->wid * 0.75f;
        blob_shadow(c->pos.x, gy, c->pos.z, sw, t->len * 0.62f, c->yaw, a);
    }
    for (int i = 0; i < MAX_PEDS; i++) {
        Ped *p = &g_peds[i];
        if (!p->active || p->respawn > 0 || dist2xz(p->pos, g_camPos) > 100 * 100) continue;
        blob_shadow(p->pos.x, 0, p->pos.z, 0.55f, 0.55f, 0, 0.35f);
    }
    {
        struct { V3 p; const char *t; float r, g, b; } sk[7] = {
            {g_spots.shop, "$", 0.4f, 1.0f, 0.4f}, {g_spots.armory, "G", 1.0f, 0.4f, 0.3f}, {g_spots.hangar, "A", 0.4f, 0.75f, 1.0f},
            {g_spots.tuner, "T", 1.0f, 0.6f, 0.15f}, {g_spots.clinic, "+", 1.0f, 0.3f, 0.3f}, {g_spots.gunx, "W", 1.0f, 0.75f, 0.1f},
            {g_spots.travel, "FLIGHTS", 0.4f, 0.85f, 1.0f}};
        for (int i = 0; i < 7; i++) {
            blob_shadow(sk[i].p.x, 0, sk[i].p.z, 0.55f, 0.55f, 0, 0.35f);
            if (dist2xz(sk[i].p, g_camPos) < 160 * 160)
                billboard_text(sk[i].t, vadd(sk[i].p, v3(0, 2.9f + sinf(g_gameTime * 3 + i) * 0.15f, 0)), 0.12f, sk[i].r, sk[i].g, sk[i].b);
        }
    }
    mission_draw3d(-1);
    combat_render_fx();
    if (g_wpOn) {
        /* waypoint: a tall blue light column */
        V3 w = g_wpPos;
        glBegin(GL_QUADS);
        for (int i = 0; i < 12; i++) {
            float a0 = 2 * PI * i / 12, a1 = 2 * PI * (i + 1) / 12;
            glColor4f(0.3f, 0.65f, 1.0f, 0.5f);
            glVertex3f(w.x + cosf(a0) * 1.5f, w.y, w.z + sinf(a0) * 1.5f);
            glVertex3f(w.x + cosf(a1) * 1.5f, w.y, w.z + sinf(a1) * 1.5f);
            glColor4f(0.3f, 0.65f, 1.0f, 0);
            glVertex3f(w.x + cosf(a1) * 1.5f, w.y + 60, w.z + sinf(a1) * 1.5f);
            glVertex3f(w.x + cosf(a0) * 1.5f, w.y + 60, w.z + sinf(a0) * 1.5f);
        }
        glEnd();
        V3 base = vadd(player_pos(), v3(0, g_pl.car >= 0 ? 4.2f : 3.3f, 0));
        float a = atan2f(w.x - base.x, w.z - base.z);
        glPushMatrix();
        glTranslatef(base.x, base.y, base.z);
        glRotatef(a * DEG, 0, 1, 0);
        glColor4f(0.35f, 0.7f, 1.0f, 0.9f);
        glBegin(GL_TRIANGLES);
        glVertex3f(0, 0, 1.0f); glVertex3f(-0.5f, 0, 0.2f); glVertex3f(0.5f, 0, 0.2f);
        glEnd();
        glPopMatrix();
    }
    if (g_curMap == 2) {
        /* falling snow around the camera */
        glBegin(GL_QUADS);
        glColor4f(1, 1, 1, 0.85f);
        for (int i = 0; i < 400; i++) {
            unsigned h = (unsigned)i * 2654435761u;
            float x = g_camPos.x + fmodf((float)(h & 1023) * 0.06f + g_gameTime * 0.7f, 60) - 30;
            float z = g_camPos.z + fmodf((float)((h >> 10) & 1023) * 0.06f + g_gameTime * 0.4f, 60) - 30;
            float y = g_camPos.y + 25 - fmodf((float)((h >> 20) & 1023) * 0.05f + g_gameTime * 2.5f, 40);
            V3 r = vmul(g_camRight, 0.07f), u = vmul(g_camUp, 0.07f);
            glVertex3f(x - r.x - u.x, y - r.y - u.y, z - r.z - u.z);
            glVertex3f(x + r.x - u.x, y + r.y - u.y, z + r.z - u.z);
            glVertex3f(x + r.x + u.x, y + r.y + u.y, z + r.z + u.z);
            glVertex3f(x - r.x + u.x, y - r.y + u.y, z - r.z + u.z);
        }
        glEnd();
    }
    V3 tp;
    if (mission_target(&tp)) {
        V3 base = vadd(player_pos(), v3(0, g_pl.car >= 0 ? 3.4f : 2.7f, 0));
        float a = atan2f(tp.x - base.x, tp.z - base.z);
        glPushMatrix();
        glTranslatef(base.x, base.y + sinf(g_gameTime * 4) * 0.1f, base.z);
        glRotatef(a * DEG, 0, 1, 0);
        glColor4f(0.3f, 1.0f, 0.3f, 0.9f);
        glBegin(GL_TRIANGLES);
        glVertex3f(0, 0, 1.3f); glVertex3f(-0.7f, 0, 0.3f); glVertex3f(0.7f, 0, 0.3f);
        glEnd();
        glBegin(GL_QUADS);
        glVertex3f(-0.25f, 0, 0.3f); glVertex3f(0.25f, 0, 0.3f); glVertex3f(0.25f, 0, -0.7f); glVertex3f(-0.25f, 0, -0.7f);
        glEnd();
        glPopMatrix();
    }
    parts_draw();
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_FOG);
}
