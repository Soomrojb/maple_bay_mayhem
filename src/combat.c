/* Combat: weapons, bullets and rockets, street gangs and turf, police and the wanted level,
 * player health, knock-outs and arrests, mission shooting targets. */
#include "game.h"

void ped_shot(int i, V3 vel);

const WeaponDef g_weap[NUM_WEAPONS] = {
    {"FISTS", "Kick crates, people and gang members.", 22, 2, 0.35f, 0, 1, 0, 0, 1, 0, 0, 0, 0},
    {"PISTOL", "A trusty sidearm. Accurate and cheap to feed.", 20, 50, 0.28f, 0.022f, 1.45f, 1.1f, 12, 1, 300, 30, 36, 0},
    {"SMG", "Sprays bullets fast. Hold FIRE for full auto.", 10, 42, 0.085f, 0.05f, 1.3f, 1.6f, 30, 1, 800, 50, 90, 1},
    {"SHOTGUN", "Eight pellets per blast. Devastating up close.", 11, 24, 0.85f, 0.1f, 1.2f, 2.0f, 6, 8, 1100, 50, 18, 0},
    {"SNIPER RIFLE", "Huge damage at long range. Aim to use the scope.", 70, 170, 1.1f, 0.004f, 3.2f, 2.2f, 5, 1, 1800, 70, 15, 0},
    {"ROCKET LAUNCHER", "Blows up cars, gangs and helicopters.", 140, 170, 1.5f, 0.01f, 1.6f, 2.4f, 1, 1, 3000, 120, 4, 0},
    {"ASSAULT RIFLE", "Fast, accurate and deadly at medium range. Full auto.", 16, 85, 0.11f, 0.028f, 1.8f, 1.8f, 30, 1, 2200, 70, 90, 1},
    {"GRENADE LAUNCHER", "Lobs bouncing explosives over cover.", 110, 60, 1.0f, 0.01f, 1.3f, 2.6f, 4, 1, 2600, 90, 8, 0},
};
static float wu(int w, int k) { return (float)g_prog.wup[w][k]; }
float weap_dmg(int w) { return g_weap[w].dmg * (1 + 0.3f * wu(w, WU_POWER)); }
float weap_range(int w) { return g_weap[w].range * (1 + 0.3f * wu(w, WU_RANGE)); }
float weap_spread(int w) { return g_weap[w].spread * (1 - 0.22f * wu(w, WU_OPTICS)); }
float weap_zoom(int w) { return g_weap[w].zoom * (1 + 0.45f * wu(w, WU_OPTICS)); }
int weap_mag(int w) { return (int)(g_weap[w].mag * (1 + 0.5f * wu(w, WU_MAG)) + 0.5f); }
float weap_rate(int w) { return g_weap[w].rate * (1 - 0.12f * wu(w, WU_MAG)); }
float weap_reload(int w) { return g_weap[w].reload * (1 - 0.15f * wu(w, WU_MAG)); }
void give_weapon(int w, int ammo) {
    g_prog.weapons |= 1u << w;
    g_prog.ammo[w] += ammo;
    if (g_pl.clip[w] == 0) g_pl.clip[w] = g_prog.ammo[w] < weap_mag(w) ? g_prog.ammo[w] : weap_mag(w);
}

const Turf g_turfs[NUM_TURF] = {{4 + BO, 0 + BO, GANG_RATS}, {5 + BO, 0 + BO, GANG_RATS}, {4 + BO, 1 + BO, GANG_RATS}, {5 + BO, 1 + BO, GANG_RATS},
                                {3 + BO, 0 + BO, GANG_VIPERS}, {2 + BO, 0 + BO, GANG_VIPERS}, {3 + BO, 1 + BO, GANG_VIPERS}, {2 + BO, 1 + BO, GANG_VIPERS}};
const char *g_gangNames[3] = {"RUST RATS", "NEON VIPERS", "POLICE"};
float g_gangHeat[2];
int g_turfProg[NUM_TURF];
float g_wanted;

/* ------------------------------------------------------------- entities */
#define MAX_THUGS 30
typedef struct {
    int active, gang, turf, dir, dead, weapon, tgt, byPlayer;
    float t, speed, yaw, anim, cd, hp, knock, deadT, think, bust, idle, aimP;
    V3 pos, vel;
    int shots;
} Thug;
static Thug thugs[MAX_THUGS];
static const Look gangLook[5] = {
    {{0.28f, 0.2f, 0.16f}, {0.22f, 0.28f, 0.45f}, {0.9f, 0.72f, 0.58f}, {0.8f, 0.12f, 0.1f}, {0.15f, 0.12f, 0.1f}, 0},
    {{0.55f, 0.12f, 0.1f}, {0.1f, 0.1f, 0.12f}, {0.62f, 0.45f, 0.32f}, {0.15f, 0.15f, 0.15f}, {0.3f, 0.1f, 0.1f}, 0},
    {{0.2f, 0.75f, 0.38f}, {0.35f, 0.15f, 0.5f}, {0.85f, 0.68f, 0.52f}, {0.45f, 0.15f, 0.6f}, {0.1f, 0.1f, 0.1f}, 2},
    {{0.45f, 0.2f, 0.65f}, {0.1f, 0.1f, 0.1f}, {0.55f, 0.4f, 0.3f}, {0.2f, 0.8f, 0.4f}, {0.2f, 0.7f, 0.35f}, 0},
    {{0.15f, 0.25f, 0.55f}, {0.1f, 0.12f, 0.25f}, {0.88f, 0.7f, 0.55f}, {0.1f, 0.15f, 0.4f}, {0.05f, 0.05f, 0.05f}, 6},
};

Target g_targets[MAX_TARGETS];

typedef struct { V3 a, b; float life; float c[3]; } Tracer;
#define MAX_TRACERS 96
static Tracer tracers[MAX_TRACERS];
static int tracerNext;

typedef struct { V3 p, v; float life, dmg, rad; int alive, byPlayer, homing, grav; } Proj;
#define MAX_PROJ 24
static Proj projs[MAX_PROJ];

typedef struct { V3 p; float life; int kind, alive; } Pick;
#define MAX_PICKS 20
static Pick picks[MAX_PICKS];

static V3 incidentPos, crimePos;
static float incidentT, noCrimeT, spawnCD, aimHold, incomeT, hurtSnd, bustedFine;

/* --------------------------------------------------------------- helpers */
static float dist_to_cam(V3 p) { return dist3(p, g_camPos); }
static float vol_at(V3 p, float base) { return clampf(base * (1 - dist_to_cam(p) / 140), 0, base); }
void spawn_tracer(V3 a, V3 b, float r, float g, float bl) {
    Tracer *t = &tracers[tracerNext];
    tracerNext = (tracerNext + 1) % MAX_TRACERS;
    t->a = a; t->b = b; t->life = 0.07f;
    t->c[0] = r; t->c[1] = g; t->c[2] = bl;
}
static int proj_spawn(V3 p, V3 v, float dmg, float radius, int byPlayer, int homing) {
    for (int i = 0; i < MAX_PROJ; i++) {
        if (projs[i].alive) continue;
        Proj *q = &projs[i];
        q->p = p; q->v = v; q->life = 5; q->dmg = dmg; q->rad = radius; q->alive = 1; q->byPlayer = byPlayer; q->homing = homing;
        q->grav = 0;
        return i;
    }
    return -1;
}
void spawn_rocket(V3 p, V3 v, float dmg, float radius, int byPlayer, int homing) { proj_spawn(p, v, dmg, radius, byPlayer, homing); }
void spawn_grenade(V3 p, V3 v, float dmg, float radius) {
    int i = proj_spawn(p, v, dmg, radius, 1, 0);
    if (i >= 0) { projs[i].grav = 1; projs[i].life = 3.5f; }
}
static void add_pick(V3 p, int kind) {
    for (int i = 0; i < MAX_PICKS; i++) {
        if (picks[i].alive) continue;
        picks[i].p = v3(p.x, p.y + 0.7f, p.z);
        picks[i].life = 25;
        picks[i].kind = kind;
        picks[i].alive = 1;
        return;
    }
}
int turf_at(V3 p) {
    if (g_curMap != 0) return -1;
    for (int k = 0; k < NUM_TURF; k++) {
        float x0, z0, x1, z1;
        block_rect(g_turfs[k].bi, g_turfs[k].bj, &x0, &z0, &x1, &z1);
        if (p.x > x0 - 4 && p.x < x1 + 4 && p.z > z0 - 4 && p.z < z1 + 4) return k;
    }
    return -1;
}
static int turf_owned(int k) { return (g_prog.turf >> k) & 1; }

/* ------------------------------------------------------------- wanted */
int wanted_stars(void) { int s = (int)ceilf(g_wanted - 0.01f); return s < 0 ? 0 : s > 5 ? 5 : s; }
static int police_witness(V3 where) {
    for (int i = 0; i < MAX_CARS; i++)
        if (g_cars[i].used && g_cars[i].ai == AI_POLICE && !g_cars[i].wrecked && dist2xz(g_cars[i].pos, where) < 90 * 90) return 1;
    for (int i = 0; i < MAX_THUGS; i++)
        if (thugs[i].active && !thugs[i].dead && thugs[i].gang == GANG_POLICE && dist2xz(thugs[i].pos, where) < 90 * 90) return 1;
    return 0;
}
void wanted_crime(float amount, V3 where) {
    if (g_pl.dead) return;
    if (amount < 1.5f && g_wanted <= 0 && !police_witness(where)) return;
    int before = wanted_stars();
    g_wanted = fminf(5, g_wanted + amount);
    if (g_wanted < 1) g_wanted = 1;
    noCrimeT = 0;
    crimePos = where;
    if (wanted_stars() > before) { audio_play(SND_SIREN, 0.5f, 1.1f); hud_toast("WANTED LEVEL %d - THE POLICE ARE AFTER YOU", wanted_stars()); }
}
void wanted_clear(void) { g_wanted = 0; noCrimeT = 0; }
static void wanted_update(float dt) {
    if (g_wanted <= 0) return;
    noCrimeT += dt;
    if (noCrimeT < 12) return;
    V3 pp = player_pos();
    float far = distxz(pp, crimePos);
    float k = far > 350 ? 6 : far > 200 ? 3 : 1;
    if (!police_witness(pp)) k *= 2;
    g_wanted -= dt * 0.06f * k;
    if (g_wanted <= 0) { g_wanted = 0; hud_toast("YOU LOST THE POLICE - NO LONGER WANTED"); audio_play(SND_CHECK, 0.5f, 0.8f); }
}
void gunfire_noise(V3 p, int byPlayer) {
    incidentPos = p;
    incidentT = 40;
    if (byPlayer && !mission_active()) wanted_crime(0.2f, p); /* shots fired on a mission are tolerated */
}
int police_incident(V3 *p) {
    if (incidentT <= 0) return 0;
    *p = incidentPos;
    return 1;
}
void police_on_car_destroyed(Car *c) {
    if (c->hitByPlayer || (g_pl.car >= 0 && &g_cars[g_pl.car] != c && c->lastHit > 0)) {
        if (c->ai == AI_POLICE) wanted_crime(1.5f, c->pos);
        else if (!c->mission) wanted_crime(0.6f, c->pos);
    }
}

/* ------------------------------------------------------------- targets */
void targets_clear(void) { memset(g_targets, 0, sizeof g_targets); }
int target_add(V3 p, int kind, float hp) {
    for (int i = 0; i < MAX_TARGETS; i++) {
        if (g_targets[i].alive) continue;
        Target *t = &g_targets[i];
        memset(t, 0, sizeof *t);
        t->pos = t->base = p; t->kind = kind; t->hp = hp; t->alive = 1; t->t = frange(0, 10);
        return i;
    }
    return -1;
}
int targets_alive(void) { int n = 0; for (int i = 0; i < MAX_TARGETS; i++) n += g_targets[i].alive; return n; }
static void target_damage(int i, float d) {
    Target *t = &g_targets[i];
    if (!t->alive) return;
    t->hp -= d;
    if (t->hp <= 0) {
        t->alive = 0;
        fx_burst(t->pos, 24, 5, 0.8f, 0.2f, t->kind ? 0.6f : 1, t->kind ? 0.6f : 0.3f, t->kind ? 0.65f : 0.2f, 12);
        audio_play(t->kind ? SND_EXPLODE : SND_BREAK, t->kind ? 0.5f : 0.6f, 1.3f);
        audio_play(SND_CHECK, 0.5f, 1.1f);
    }
}

/* -------------------------------------------------------------- thugs */
static V3 perim(int bi, int bj, float t) {
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
int spawn_thug(int gang, V3 p, int turf) {
    for (int i = 0; i < MAX_THUGS; i++) {
        if (thugs[i].active) continue;
        Thug *h = &thugs[i];
        memset(h, 0, sizeof *h);
        h->active = 1;
        h->gang = gang;
        h->turf = turf;
        h->pos = p;
        h->hp = gang == GANG_POLICE ? 80 : 60;
        h->weapon = gang == GANG_VIPERS && (i & 1) ? W_SMG : W_PISTOL;
        h->speed = frange(1.3f, 1.9f);
        h->dir = rand() & 1 ? 1 : -1;
        h->tgt = -1;
        h->cd = frange(0.5f, 1.5f);
        h->yaw = frange(-PI, PI);
        return i;
    }
    return -1;
}
int thugs_near(V3 p, float r, int gang) {
    int n = 0;
    for (int i = 0; i < MAX_THUGS; i++)
        if (thugs[i].active && !thugs[i].dead && (gang < 0 || thugs[i].gang == gang) && dist2xz(thugs[i].pos, p) < r * r) n++;
    return n;
}
V3 thug_nearest(V3 p, int gang, int *found) {
    float bd = 1e18f;
    V3 best = p;
    *found = 0;
    for (int i = 0; i < MAX_THUGS; i++) {
        Thug *h = &thugs[i];
        if (!h->active || h->dead || h->gang != gang) continue;
        float d = dist2xz(h->pos, p);
        if (d < bd) { bd = d; best = h->pos; *found = 1; }
    }
    return best;
}
static void thug_kill(int i, int byPlayer) {
    Thug *h = &thugs[i];
    h->dead = 1;
    h->deadT = 0;
    h->hp = 0;
    audio_play(SND_KICK, vol_at(h->pos, 0.6f), 0.7f);
    V3 p = vadd(h->pos, v3(0, 0.8f, 0));
    if (h->gang != GANG_POLICE) {
        int n = 2 + rand() % 3;
        for (int k = 0; k < n; k++) add_dyn_coin(p, v3(frange(-3, 3), frange(5, 8), frange(-3, 3)));
        float r = frand();
        if (r < 0.3f) add_pick(h->pos, 0);
        else if (r < 0.5f) add_pick(h->pos, 1);
    }
    if (!byPlayer) return;
    if (h->gang == GANG_POLICE) { wanted_crime(1.5f, h->pos); return; }
    wanted_crime(0.3f, h->pos);
    mission_on_kill(h->gang);
    if (h->turf >= 0 && !turf_owned(h->turf)) {
        g_turfProg[h->turf]++;
        if (g_turfProg[h->turf] >= TURF_KILLS) {
            g_prog.turf |= 1u << h->turf;
            add_coins(150);
            int left = 0;
            for (int k = 0; k < NUM_TURF; k++) if (g_turfs[k].gang == h->gang && !turf_owned(k)) left++;
            if (left) hud_msg(4, "TURF CLAIMED FROM THE %s!  +150 COINS", g_gangNames[h->gang]);
            else hud_msg(5, "THE %s HAVE LOST ALL THEIR TURF!  +150 COINS", g_gangNames[h->gang]);
            audio_play(SND_SUCCESS, 0.7f, 1);
            save_game();
        }
    }
}
static void thug_damage(int i, float d, int byPlayer, V3 dir) {
    Thug *h = &thugs[i];
    if (!h->active || h->dead) return;
    h->hp -= d;
    h->vel = vadd(h->vel, vmul(dir, 2));
    fx_burst(vadd(h->pos, v3(0, 1.2f, 0)), 5, 2, 0.3f, 0.12f, 0.9f, 0.2f, 0.15f, 10);
    if (byPlayer) {
        h->byPlayer = 1;
        if (h->gang < 2) g_gangHeat[h->gang] = 45;
        else wanted_crime(0.6f, h->pos);
        h->tgt = -2;
    }
    if (h->hp <= 0) thug_kill(i, byPlayer);
}
static int player_hostile_to(int gang) {
    if (gang == GANG_POLICE) return wanted_stars() > 0;
    if (g_gangHeat[gang] > 0 || mission_kill_gang() == gang) return 1;
    int k = turf_at(player_pos());
    return k >= 0 && g_turfs[k].gang == gang && !turf_owned(k);
}
static int enemies(int a, int b) {
    if (a == b) return 0;
    return 1; /* rats vs vipers, police vs both */
}
static V3 eye(V3 p) { return vadd(p, v3(0, 1.5f, 0)); }
static void thug_shoot(Thug *h, int idx) {
    V3 gun = vadd(eye(h->pos), vmul(fwd_of(h->yaw), 0.6f));
    float dmg = h->weapon == W_SMG ? 4 : (h->gang == GANG_POLICE ? 5 : 6);
    audio_play(h->weapon == W_SMG ? SND_MG : SND_SHOT, vol_at(h->pos, 0.45f), frange(0.9f, 1.1f));
    spawn_part(gun, v3(0, 0.5f, 0), 0.06f, 0.18f, 1, 0.85f, 0.3f, 0, 0);
    gunfire_noise(h->pos, 0);
    if (h->tgt == -2) {
        V3 pp = player_pos();
        V3 tgt = vadd(pp, v3(0, g_pl.car >= 0 ? 1.0f : 1.2f, 0));
        float d = dist3(gun, tgt);
        float hs = sqrtf(g_pl.vel.x * g_pl.vel.x + g_pl.vel.z * g_pl.vel.z);
        float chance = clampf(0.5f - d * 0.009f - (hs > 5 ? 0.15f : 0), 0.08f, 0.6f);
        int hit = frand() < chance;
        V3 end = hit ? tgt : vadd(tgt, v3(frange(-2, 2), frange(-1, 1), frange(-2, 2)));
        spawn_tracer(gun, end, 1, 0.9f, 0.5f);
        if (hit) {
            Car *pc = player_car();
            if (pc) car_damage(pc, dmg * 0.8f);
            else player_damage(dmg, h->pos);
        }
    } else if (h->tgt >= 0) {
        Thug *o = &thugs[h->tgt];
        int hit = frand() < 0.4f;
        V3 tgt = vadd(o->pos, v3(0, 1.2f, 0));
        spawn_tracer(gun, hit ? tgt : vadd(tgt, v3(frange(-2, 2), frange(-1, 1), frange(-2, 2))), 1, 0.9f, 0.5f);
        if (hit) thug_damage(h->tgt, 12, 0, vnorm(vsub(tgt, gun)));
    }
    (void)idx;
}
static void thugs_update(float dt) {
    V3 pp = player_pos();
    /* spawn gang members around their turf when the player is near */
    spawnCD -= dt;
    for (int g = 0; g < 2 && spawnCD <= 0 && g_curMap == 0; g++) {
        int mk = mission_kill_gang() == g;
        float nearest = 1e9f;
        int cand[NUM_TURF], nc = 0;
        for (int k = 0; k < NUM_TURF; k++) {
            if (g_turfs[k].gang != g || turf_owned(k)) continue;
            float x0, z0, x1, z1;
            block_rect(g_turfs[k].bi, g_turfs[k].bj, &x0, &z0, &x1, &z1);
            float d = distxz(pp, v3((x0 + x1) / 2, 0, (z0 + z1) / 2));
            if (d < nearest) nearest = d;
            cand[nc++] = k;
        }
        int want = (nc && nearest < (mk ? 300.0f : 210.0f)) ? (mk ? 8 : 6) : 0;
        if (thugs_near(pp, 1e6f, g) >= want) continue;
        int k = cand[rand() % nc];
        for (int tries = 0; tries < 6; tries++) {
            float t = frange(0, 300);
            V3 p = perim(g_turfs[k].bi, g_turfs[k].bj, t);
            if (distxz(p, pp) < (mk ? 22.0f : 30.0f)) continue;
            int i = spawn_thug(g, p, k);
            if (i >= 0) { thugs[i].t = t; thugs[i].dir = rand() & 1 ? 1 : -1; }
            spawnCD = 0.4f;
            break;
        }
    }
    for (int i = 0; i < MAX_THUGS; i++) {
        Thug *h = &thugs[i];
        if (!h->active) continue;
        float dp = distxz(h->pos, pp);
        if (h->dead) {
            h->deadT += dt;
            if (h->deadT > 8 || dp > 200) h->active = 0;
            continue;
        }
        if (dp > 280) { h->active = 0; continue; }
        if (h->knock > 0) {
            h->knock -= dt;
            h->vel.y -= GRAV * dt;
            h->pos = vadd(h->pos, vmul(h->vel, dt));
            float g = ground_height(h->pos.x, h->pos.z, h->pos.y, 0.3f);
            if (h->pos.y <= g) { h->pos.y = g; h->vel = vmul(h->vel, 0.5f); h->vel.y = 0; }
            continue;
        }
        /* choose a target */
        h->think -= dt;
        if (h->think <= 0) {
            h->think = frange(0.3f, 0.5f);
            int best = -1;
            float bd = h->gang == GANG_POLICE ? 38.0f : 30.0f;
            if (!g_pl.dead && player_hostile_to(h->gang)) {
                float d = dist3(h->pos, pp);
                float reach = h->gang == GANG_POLICE ? 45.0f : (h->byPlayer ? 40.0f : 32.0f);
                if (d < reach && ray_world(eye(h->pos), vadd(pp, v3(0, 1.3f, 0))) > 0.97f) { best = -2; bd = d * 0.8f; }
            }
            for (int j = 0; j < MAX_THUGS; j++) {
                Thug *o = &thugs[j];
                if (j == i || !o->active || o->dead || !enemies(h->gang, o->gang)) continue;
                float d = dist3(h->pos, o->pos);
                if (d >= bd) continue;
                if (ray_world(eye(h->pos), eye(o->pos)) < 0.97f) continue;
                bd = d;
                best = j;
            }
            h->tgt = best;
        }
        if (h->tgt >= 0 && (!thugs[h->tgt].active || thugs[h->tgt].dead)) h->tgt = -1;
        V3 old = h->pos;
        if (h->tgt != -1) {
            V3 tp = h->tgt == -2 ? pp : thugs[h->tgt].pos;
            float d = distxz(h->pos, tp);
            h->yaw = atan2f(tp.x - h->pos.x, tp.z - h->pos.z);
            /* police try to cuff a mildly wanted player on foot instead of shooting */
            int arrest = h->gang == GANG_POLICE && h->tgt == -2 && wanted_stars() <= 2 && g_pl.car < 0;
            if (arrest) {
                if (d > 1.3f) {
                    V3 f = fwd_of(h->yaw);
                    h->pos = vadd(h->pos, vmul(f, 6.2f * dt));
                    h->anim += dt * 8;
                    h->bust = fmaxf(0, h->bust - dt);
                } else {
                    h->bust += dt;
                    if (h->bust > 1.2f && !g_pl.dead) {
                        g_pl.dead = 1;
                        g_pl.busted = 1;
                        g_pl.deadT = 0;
                        audio_play(SND_BUSTED, 0.7f, 1);
                        mission_fail_reason("YOU GOT BUSTED");
                    }
                }
            } else {
                if (h->gang == GANG_POLICE && d > 22) {
                    h->pos = vadd(h->pos, vmul(fwd_of(h->yaw), 5.5f * dt));
                    h->anim += dt * 7;
                }
                h->cd -= dt;
                if (h->cd <= 0) {
                    thug_shoot(h, i);
                    if (h->weapon == W_SMG) { h->shots++; h->cd = (h->shots % 4) ? 0.12f : frange(1.3f, 1.9f); }
                    else h->cd = frange(0.85f, 1.4f);
                }
            }
            h->idle = 0;
        } else if (h->gang != GANG_POLICE) {
            h->t += h->speed * h->dir * dt;
            V3 np = perim(g_turfs[h->turf >= 0 ? h->turf : 0].bi, g_turfs[h->turf >= 0 ? h->turf : 0].bj, h->t);
            V3 d = vsub(np, h->pos);
            float l = sqrtf(d.x * d.x + d.z * d.z);
            if (l > 0.01f) {
                float stp = fminf(l, (l > 3 ? 4.0f : h->speed) * dt);
                h->pos = vadd(h->pos, v3(d.x / l * stp, 0, d.z / l * stp));
                h->yaw = atan2f(d.x, d.z);
            }
            h->anim += dt * h->speed * 4;
        } else {
            h->idle += dt;
            if (h->idle > 25 && dp > 60) h->active = 0;
        }
        collide_circle(&h->pos, 0.4f, 0.45f, 1.8f, NULL);
        h->pos.y = ground_height(h->pos.x, h->pos.z, h->pos.y, 0.5f);
        (void)old;
        /* hit by cars */
        for (int c = 0; c < MAX_CARS; c++) {
            Car *car = &g_cars[c];
            if (!car->used || car_speed(car) < 6 || fabsf(car->pos.y - h->pos.y) > 2) continue;
            const CarType *t = &g_carTypes[car->type];
            V3 f = fwd_of(car->yaw);
            V3 dd = vsub(h->pos, car->pos);
            float a = clampf(vdot(dd, f), -t->len * 0.5f, t->len * 0.5f);
            if (distxz(h->pos, vadd(car->pos, vmul(f, a))) < t->wid * 0.5f + 0.4f) {
                h->knock = 1.2f;
                h->vel = v3(car->vel.x * 0.7f, 7, car->vel.z * 0.7f);
                thug_damage(i, car_speed(car) * 3, c == g_pl.car, vnorm(car->vel));
                break;
            }
        }
    }
}

/* --------------------------------------------------------------- probes */
typedef struct { int kind, idx; float t; } Hit;
enum { H_NONE, H_THUG, H_PED, H_CAR, H_CRATE, H_TARGET };
extern Ped g_peds[MAX_PEDS];
static int ray_sphere(V3 o, V3 d, V3 c, float r, float *t) {
    V3 oc = vsub(c, o);
    float b = vdot(oc, d), c2 = vdot(oc, oc) - b * b;
    if (c2 > r * r || b < -r) return 0;
    float th = sqrtf(r * r - c2);
    *t = b - th < 0 ? 0 : b - th;
    return 1;
}
static int ray_aabb(V3 o, V3 d, V3 mn, V3 mx, float maxd, float *t) {
    float t0 = 0, t1 = maxd;
    float oo[3] = {o.x, o.y, o.z}, dd[3] = {d.x, d.y, d.z}, a[3] = {mn.x, mn.y, mn.z}, b[3] = {mx.x, mx.y, mx.z};
    for (int k = 0; k < 3; k++) {
        if (fabsf(dd[k]) < 1e-7f) { if (oo[k] < a[k] || oo[k] > b[k]) return 0; continue; }
        float inv = 1.0f / dd[k], ta = (a[k] - oo[k]) * inv, tb = (b[k] - oo[k]) * inv;
        if (ta > tb) { float s = ta; ta = tb; tb = s; }
        if (ta > t0) t0 = ta;
        if (tb < t1) t1 = tb;
        if (t0 > t1) return 0;
    }
    *t = t0;
    return 1;
}
static Hit probe(V3 o, V3 d, float maxd, int ignoreCar, int ignorePlayerSide) {
    Hit h = {H_NONE, -1, ray_world(o, vadd(o, vmul(d, maxd))) * maxd};
    float t;
    for (int i = 0; i < MAX_THUGS; i++) {
        Thug *g = &thugs[i];
        if (!g->active || g->dead) continue;
        if (ray_sphere(o, d, vadd(g->pos, v3(0, 0.95f, 0)), 0.62f, &t) && t < h.t) { h.t = t; h.kind = H_THUG; h.idx = i; }
        if (ray_sphere(o, d, vadd(g->pos, v3(0, 1.62f, 0)), 0.36f, &t) && t < h.t) { h.t = t; h.kind = H_THUG; h.idx = i; }
    }
    for (int i = 0; i < MAX_PEDS; i++) {
        Ped *p = &g_peds[i];
        if (!p->active || p->knock > 0 || p->respawn > 0) continue;
        if (ray_sphere(o, d, vadd(p->pos, v3(0, 1.0f, 0)), 0.6f, &t) && t < h.t) { h.t = t; h.kind = H_PED; h.idx = i; }
    }
    for (int i = 0; i < MAX_CARS; i++) {
        Car *c = &g_cars[i];
        if (!c->used || i == ignoreCar) continue;
        const CarType *ct = &g_carTypes[c->type];
        V3 rel = vsub(o, c->pos);
        if (vdot(rel, rel) > (maxd + 12) * (maxd + 12)) continue;
        V3 X = v3(cosf(c->yaw), 0, -sinf(c->yaw)), Z = fwd_of(c->yaw);
        V3 lo = v3(vdot(rel, X), rel.y, vdot(rel, Z)), ld = v3(vdot(d, X), d.y, vdot(d, Z));
        float hw = ct->wid * 0.5f + (ct->kind == VK_PLANE ? 0.0f : 0.1f);
        if (ray_aabb(lo, ld, v3(-hw, 0, -ct->len * 0.5f), v3(hw, ct->hgt, ct->len * 0.5f), h.t, &t) && t < h.t) { h.t = t; h.kind = H_CAR; h.idx = i; }
    }
    for (int i = 0; i < MAX_CRATES; i++) {
        Crate *c = &g_crates[i];
        if (!c->alive || dist2xz(c->pos, o) > (maxd + 2) * (maxd + 2)) continue;
        if (ray_aabb(o, d, v3(c->pos.x - CRATE_H, c->pos.y, c->pos.z - CRATE_H), v3(c->pos.x + CRATE_H, c->pos.y + 1.2f, c->pos.z + CRATE_H), h.t, &t) && t < h.t) { h.t = t; h.kind = H_CRATE; h.idx = i; }
    }
    for (int i = 0; i < MAX_TARGETS; i++) {
        Target *g = &g_targets[i];
        if (!g->alive) continue;
        if (ray_sphere(o, d, g->pos, g->kind ? 1.4f : 1.0f, &t) && t < h.t) { h.t = t; h.kind = H_TARGET; h.idx = i; }
    }
    (void)ignorePlayerSide;
    return h;
}
static void apply_hit(Hit h, V3 p, V3 d, float dmg, int byPlayer) {
    switch (h.kind) {
    case H_THUG: thug_damage(h.idx, dmg, byPlayer, d); break;
    case H_PED:
        ped_shot(h.idx, vmul(d, 5));
        if (byPlayer) wanted_crime(0.8f, p);
        break;
    case H_CAR: {
        Car *c = &g_cars[h.idx];
        if (byPlayer) {
            c->hitByPlayer = 1;
            if (c->ai == AI_POLICE) wanted_crime(0.4f, c->pos);
        }
        car_damage(c, dmg * (is_aircraft(c) ? 0.8f : 0.5f));
        fx_burst(p, 6, 3, 0.3f, 0.1f, 1, 0.8f, 0.4f, 12);
    } break;
    case H_CRATE: break_crate(h.idx); break;
    case H_TARGET: target_damage(h.idx, dmg); break;
    default: fx_burst(p, 4, 2, 0.3f, 0.12f, 0.75f, 0.7f, 0.6f, 8); break;
    }
}
int fire_hitscan(V3 o, V3 dir, float range, float dmg, int byPlayer, int ignoreCar, V3 *hitp) {
    Hit h = probe(o, dir, range, ignoreCar, byPlayer);
    V3 p = vadd(o, vmul(dir, h.t));
    if (h.t < range - 0.01f || h.kind != H_NONE) apply_hit(h, p, dir, dmg, byPlayer);
    if (hitp) *hitp = p;
    return h.kind;
}

void explode(V3 p, float r, float dmg, int byPlayer) {
    if (getenv("MB_DEBUG")) fprintf(stderr, "explode at %.1f %.1f %.1f\n", p.x, p.y, p.z);
    audio_play(SND_EXPLODE, vol_at(p, 0.9f) + 0.1f, frange(0.9f, 1.1f));
    fx_burst(p, 40, 9, 0.9f, 0.6f, 1, 0.6f, 0.1f, 4);
    fx_burst(p, 20, 4, 1.8f, 1.1f, 0.3f, 0.3f, 0.3f, -1);
    if (byPlayer) gunfire_noise(p, 1);
    for (int i = 0; i < MAX_THUGS; i++) {
        Thug *h = &thugs[i];
        if (!h->active || h->dead) continue;
        float d = dist3(vadd(h->pos, v3(0, 1, 0)), p);
        if (d > r) continue;
        h->knock = 1.0f;
        h->vel = vadd(vmul(vnorm(vsub(h->pos, p)), 8), v3(0, 8, 0));
        thug_damage(i, dmg * (1 - d / r * 0.7f), byPlayer, vnorm(vsub(h->pos, p)));
    }
    for (int i = 0; i < MAX_PEDS; i++) {
        Ped *q = &g_peds[i];
        if (!q->active || q->knock > 0 || q->respawn > 0 || dist3(q->pos, p) > r) continue;
        ped_shot(i, vmul(vnorm(vsub(q->pos, p)), 9));
        if (byPlayer) wanted_crime(0.8f, p);
    }
    for (int i = 0; i < MAX_CARS; i++) {
        Car *c = &g_cars[i];
        if (!c->used) continue;
        float d = dist3(c->pos, p);
        if (d > r + 2) continue;
        if (byPlayer) { c->hitByPlayer = 1; if (c->ai == AI_POLICE) wanted_crime(0.8f, c->pos); }
        car_damage(c, dmg * 1.2f * (1 - clampf(d / (r + 2), 0, 1) * 0.7f));
        if (!is_aircraft(c)) { c->vy = 7; c->onGround = 0; }
    }
    for (int i = 0; i < MAX_CRATES; i++) if (g_crates[i].alive && dist3(g_crates[i].pos, p) < r) break_crate(i);
    for (int i = 0; i < MAX_TARGETS; i++) if (g_targets[i].alive && dist3(g_targets[i].pos, p) < r + 1) target_damage(i, dmg);
    if (g_pl.car < 0) {
        float d = dist3(vadd(g_pl.pos, v3(0, 1, 0)), p);
        if (d < r) {
            player_damage(dmg * (1 - d / r * 0.7f) * (byPlayer ? 0.4f : 1.0f), p);
            g_pl.vel = vadd(g_pl.vel, vadd(vmul(vnorm(vsub(g_pl.pos, p)), 7), v3(0, 6, 0)));
            g_pl.onGround = 0;
        }
    }
}

/* -------------------------------------------------------------- player */
void player_damage(float d, V3 from) {
    if (g_pl.dead || g_godMode || d <= 0) return;
    float a = fminf(g_pl.armor, d * 0.6f);
    g_pl.armor -= a;
    g_pl.health -= d - a;
    g_pl.hurtT = 0;
    g_pl.flash = fminf(0.6f, g_pl.flash + 0.25f);
    if (hurtSnd <= 0) { audio_play(SND_HURT, 0.45f, frange(0.9f, 1.1f)); hurtSnd = 0.25f; }
    (void)from;
    if (g_pl.health <= 0) {
        g_pl.health = 0;
        g_pl.dead = 1;
        g_pl.busted = 0;
        g_pl.deadT = 0;
        if (g_pl.car >= 0) exit_vehicle();
        audio_play(SND_FAIL, 0.6f, 0.8f);
        mission_fail_reason("YOU WERE KNOCKED OUT");
    }
}
static void death_update(float dt) {
    if (!g_pl.dead) {
        g_pl.hurtT += dt;
        float half = player_max_health() * 0.5f;
        if (g_pl.hurtT > 6 && g_pl.health < half) g_pl.health = fminf(half, g_pl.health + 3 * dt);
        return;
    }
    g_pl.deadT += dt;
    if (g_pl.busted && g_pl.car >= 0) exit_vehicle();
    if (g_pl.deadT < 3.5f) return;
    int busted = g_pl.busted;
    int fine = (int)fminf(g_prog.coins * (busted ? 0.15f : 0.1f), busted ? 400.0f : 200.0f);
    g_prog.coins -= fine;
    bustedFine = (float)fine;
    wanted_clear();
    for (int i = 0; i < MAX_THUGS; i++) if (thugs[i].gang == GANG_POLICE) thugs[i].active = 0;
    g_gangHeat[0] = g_gangHeat[1] = 0;
    player_respawn(busted ? g_spots.police : nearest_hospital(g_pl.pos));
    g_pl.yaw = 0;
    g_camYaw = 0;
    if (busted) hud_msg(5, "BUSTED!  YOU PAID A FINE OF %d COINS", fine);
    else hud_msg(5, "PATCHED UP AT THE HOSPITAL  -  BILL: %d COINS", fine);
}

static int owned(int w) { return w == W_FISTS || ((g_prog.weapons >> w) & 1); }
static void switch_weapon(int dir) {
    int w = g_pl.weapon;
    for (int k = 0; k < NUM_WEAPONS; k++) {
        w = (w + dir + NUM_WEAPONS) % NUM_WEAPONS;
        if (owned(w)) break;
    }
    if (w != g_pl.weapon) {
        g_pl.weapon = w;
        g_pl.reloadT = 0;
        g_pl.fireCD = 0.2f;
        audio_play(SND_RELOAD, 0.3f, 1.4f);
    }
}
static void start_reload(void) {
    int w = g_pl.weapon;
    if (w == W_FISTS || g_pl.reloadT > 0 || g_pl.clip[w] >= weap_mag(w) || g_prog.ammo[w] <= g_pl.clip[w]) return;
    g_pl.reloadT = weap_reload(w);
    audio_play(SND_RELOAD, 0.5f, 1);
}
static void foot_weapons(float dt) {
    if (act_pressed(ACT_NEXTWEAP)) switch_weapon(1);
    if (act_pressed(ACT_PREVWEAP)) switch_weapon(-1);
    int w = g_pl.weapon;
    if (!owned(w)) { g_pl.weapon = W_FISTS; return; }
    if (g_pl.fireCD > 0) g_pl.fireCD -= dt;
    if (aimHold > 0) aimHold -= dt;
    if (w == W_FISTS) { g_pl.aiming = 0; return; }
    if (g_pl.reloadT > 0) {
        g_pl.reloadT -= dt;
        if (g_pl.reloadT <= 0) {
            g_pl.reloadT = 0;
            int m = weap_mag(w);
            g_pl.clip[w] = g_prog.ammo[w] < m ? g_prog.ammo[w] : m;
        }
    }
    if (act_pressed(ACT_RELOAD)) start_reload();
    g_pl.aiming = act_down(ACT_AIM) || aimHold > 0;
    int trig = g_weap[w].autoFire ? act_down(ACT_FIRE) : act_pressed(ACT_FIRE);
    if (!trig || g_pl.fireCD > 0 || g_pl.reloadT > 0 || g_pl.knock > 0) return;
    if (g_pl.clip[w] <= 0) {
        if (g_prog.ammo[w] > 0) start_reload();
        else { audio_play(SND_EMPTY, 0.5f, 1); hud_toast("OUT OF AMMO - BUY MORE AT BOOM BROS ARMORY"); g_pl.fireCD = 0.4f; }
        return;
    }
    g_pl.clip[w]--;
    g_prog.ammo[w]--;
    g_pl.fireCD = weap_rate(w);
    aimHold = 0.6f;
    g_pl.aiming = 1;
    g_pl.yaw = g_camYaw;
    /* aim where the crosshair points */
    V3 co = g_camPos, cd = cam_forward();
    float camOff = dist3(co, vadd(g_pl.pos, v3(0, 1.6f, 0)));
    Hit ah = probe(vadd(co, vmul(cd, camOff * 0.9f)), cd, weap_range(w) + camOff, -1, 1);
    V3 aimPt = vadd(vadd(co, vmul(cd, camOff * 0.9f)), vmul(cd, ah.t));
    V3 muzzle = vadd(vadd(g_pl.pos, v3(0, 1.45f, 0)), vadd(vmul(fwd_of(g_pl.yaw), 0.75f), vmul(right_of(g_pl.yaw), 0.35f)));
    V3 dir = vnorm(vsub(aimPt, muzzle));
    if (vdot(dir, cd) < 0.5f) dir = cd;
    float spread = weap_spread(w) * (act_down(ACT_AIM) ? 0.5f : 1.0f) * (g_pl.onGround ? 1 : 2);
    spawn_part(muzzle, v3(0, 0.3f, 0), 0.06f, 0.25f, 1, 0.85f, 0.35f, 0, 0);
    if (w == W_ROCKET) {
        spawn_rocket(muzzle, vmul(dir, 65), weap_dmg(w), 6.5f, 1, 0);
        audio_play(SND_ROCKET, 0.6f, 1);
    } else if (w == W_GRENADE) {
        float sp = 26 * (1 + 0.3f * wu(w, WU_RANGE));
        spawn_grenade(muzzle, vadd(vmul(dir, sp), v3(0, 5, 0)), weap_dmg(w), 6.0f);
        audio_play(SND_SHOTGUN, 0.4f, 1.6f);
    } else {
        for (int k = 0; k < g_weap[w].pellets; k++) {
            V3 dd = vnorm(vadd(dir, v3(frange(-spread, spread), frange(-spread, spread), frange(-spread, spread))));
            V3 hp;
            int hk = fire_hitscan(muzzle, dd, weap_range(w), weap_dmg(w), 1, -1, &hp);
            if (getenv("MB_DEBUG")) fprintf(stderr, "shot kind %d at %.1f %.1f %.1f from %.1f %.1f %.1f\n", hk, hp.x, hp.y, hp.z, muzzle.x, muzzle.y, muzzle.z);
            spawn_tracer(muzzle, hp, 1, 0.95f, 0.6f);
        }
        audio_play(w == W_SHOTGUN ? SND_SHOTGUN : w == W_RIFLE ? SND_RIFLE : SND_SHOT, 0.55f, frange(0.95f, 1.05f));
    }
    g_camPitch -= w == W_SHOTGUN || w == W_ROCKET ? 0.025f : w == W_RIFLE ? 0.03f : 0.008f;
    gunfire_noise(g_pl.pos, 1);
    if (g_pl.clip[w] == 0 && g_prog.ammo[w] > 0) start_reload();
}
static int homing_target(V3 p, V3 dir, V3 *out) {
    float best = 0.82f;
    int found = 0;
    for (int i = 0; i < MAX_CARS; i++) {
        Car *c = &g_cars[i];
        if (!c->used || c->wrecked || i == g_pl.car || !(c->ai == AI_BOSS || c->mission || (c->ai == AI_POLICE && wanted_stars()))) continue;
        V3 d = vsub(vadd(c->pos, v3(0, 1, 0)), p);
        float l = vlen(d);
        if (l > 300) continue;
        float s = vdot(d, dir) / l;
        if (s > best) { best = s; *out = vadd(c->pos, v3(0, 1, 0)); found = 1; }
    }
    for (int i = 0; i < MAX_TARGETS; i++) {
        if (!g_targets[i].alive) continue;
        V3 d = vsub(g_targets[i].pos, p);
        float l = vlen(d);
        if (l > 300) continue;
        float s = vdot(d, dir) / l;
        if (s > best) { best = s; *out = g_targets[i].pos; found = 1; }
    }
    for (int i = 0; i < MAX_THUGS; i++) {
        Thug *h = &thugs[i];
        if (!h->active || h->dead || h->gang == GANG_POLICE) continue;
        V3 d = vsub(vadd(h->pos, v3(0, 1, 0)), p);
        float l = vlen(d);
        if (l > 200) continue;
        float s = vdot(d, dir) / l;
        if (s > best) { best = s; *out = vadd(h->pos, v3(0, 1, 0)); found = 1; }
    }
    return found;
}
static void air_weapons(Car *c, float dt) {
    const CarType *t = &g_carTypes[c->type];
    c->gunCD -= dt;
    c->misCD -= dt;
    if (c->wrecked) return;
    int glv = t->guns + g_prog.airUp[3];
    V3 dir = t->kind == VK_PLANE ? air_fwd3(c) : cam_forward();
    if (t->kind == VK_HELI && dir.y > 0.3f) dir = vnorm(v3(dir.x, 0.3f, dir.z));
    V3 r = right_of(c->yaw);
    if (glv > 0 && act_down(ACT_FIRE) && c->gunCD <= 0) {
        c->gunCD = 0.11f / (1 + 0.15f * (glv - 1));
        float dmg = 7 * (1 + 0.35f * (glv - 1));
        c->crew ^= 1;
        V3 m = vadd(vadd(c->pos, v3(0, t->kind == VK_HELI ? 1.0f : 1.4f, 0)), vadd(vmul(air_fwd3(c), t->len * 0.5f), vmul(r, c->crew ? 1.3f : -1.3f)));
        V3 hp;
        V3 dd = vnorm(vadd(dir, v3(frange(-0.012f, 0.012f), frange(-0.012f, 0.012f), frange(-0.012f, 0.012f))));
        fire_hitscan(m, dd, 220, dmg, 1, g_pl.car, &hp);
        spawn_tracer(m, hp, 1, 0.9f, 0.3f);
        audio_play(SND_MG, 0.45f, frange(0.95f, 1.1f));
        gunfire_noise(c->pos, 1);
    }
    if (act_pressed(ACT_MISSILE)) {
        int mlv = t->missiles + g_prog.airUp[4];
        if (mlv <= 0) { hud_toast("NO MISSILES FITTED - UPGRADE AT SKYLINE HANGAR"); audio_play(SND_DENY, 0.5f, 1); }
        else if (c->misCD > 0) audio_play(SND_EMPTY, 0.4f, 1);
        else {
            c->misCD = 2.6f / mlv;
            V3 m = vadd(c->pos, vadd(vmul(air_fwd3(c), t->len * 0.4f), v3(0, 0.6f, 0)));
            spawn_rocket(m, vadd(vmul(dir, 75), v3(c->vel.x, c->vy, c->vel.z)), 150 + 25 * (mlv - 1), 7.5f, 1, 1);
            audio_play(SND_ROCKET, 0.7f, 1.1f);
        }
    }
}

/* ------------------------------------------------------------- update */
static void projs_update(float dt) {
    for (int i = 0; i < MAX_PROJ; i++) {
        Proj *q = &projs[i];
        if (!q->alive) continue;
        q->life -= dt;
        if (q->homing) {
            V3 tp = q->p;
            float sp = vlen(q->v);
            if (homing_target(q->p, vmul(q->v, 1 / sp), &tp)) {
                V3 want = vmul(vnorm(vsub(tp, q->p)), sp);
                q->v = vmul(vnorm(vadd(q->v, vmul(vsub(want, q->v), clampf(dt * 3.5f, 0, 1)))), sp);
            }
        }
        if (q->grav) q->v.y -= GRAV * 0.8f * dt;
        V3 np = vadd(q->p, vmul(q->v, dt));
        V3 d = vsub(np, q->p);
        float l = vlen(d);
        Hit h = probe(q->p, vmul(d, 1 / fmaxf(l, 1e-5f)), l, q->byPlayer ? g_pl.car : -1, q->byPlayer);
        int boom = h.t < l - 1e-4f || q->life <= 0 || np.y < 0;
        if (!q->byPlayer && dist3(np, vadd(player_pos(), v3(0, 1, 0))) < 2.5f) boom = 1;
        if (boom) {
            V3 at = vadd(q->p, vmul(d, fminf(h.t, l) / fmaxf(l, 1e-5f)));
            q->alive = 0;
            explode(at, q->rad, q->dmg, q->byPlayer);
            continue;
        }
        q->p = np;
        if (q->grav) { spawn_part(q->p, v3(0, 0, 0), 0.25f, 0.12f, 0.6f, 0.6f, 0.6f, 0, 0.4f); continue; }
        spawn_part(q->p, v3(frange(-0.5f, 0.5f), frange(-0.5f, 0.5f), frange(-0.5f, 0.5f)), 0.8f, 0.3f, 0.7f, 0.7f, 0.7f, -0.3f, 1.0f);
        spawn_part(q->p, v3(0, 0, 0), 0.08f, 0.35f, 1, 0.6f, 0.15f, 0, 0);
    }
}
static void picks_update(float dt) {
    V3 pp = vadd(player_pos(), v3(0, 0.8f, 0));
    for (int i = 0; i < MAX_PICKS; i++) {
        Pick *k = &picks[i];
        if (!k->alive) continue;
        k->life -= dt;
        if (k->life <= 0) { k->alive = 0; continue; }
        if (g_pl.car >= 0 || g_pl.dead || dist3(pp, k->p) > 1.6f) continue;
        if (k->kind == 0) {
            if (g_pl.health >= player_max_health()) continue;
            g_pl.health = fminf(player_max_health(), g_pl.health + 25);
            audio_play(SND_HEAL, 0.5f, 1.2f);
        } else {
            int w = g_pl.weapon;
            if (w == W_FISTS) {
                for (int q = 1; q < NUM_WEAPONS; q++) if (owned(q)) { w = q; break; }
            }
            if (w == W_FISTS) continue;
            int n = g_weap[w].ammoPack / 3 + 1;
            g_prog.ammo[w] += n;
            hud_toast("+%d %s AMMO", n, g_weap[w].name);
            audio_play(SND_RELOAD, 0.5f, 1.2f);
        }
        k->alive = 0;
    }
}
static void targets_update(float dt) {
    for (int i = 0; i < MAX_TARGETS; i++) {
        Target *t = &g_targets[i];
        if (!t->alive) continue;
        t->t += dt;
        if (t->kind == 1) t->pos = vadd(t->base, v3(sinf(t->t * 0.45f + i) * 7, sinf(t->t * 1.2f + i) * 2, cosf(t->t * 0.38f + i) * 7));
    }
}
void combat_reset(void) {
    memset(thugs, 0, sizeof thugs);
    memset(projs, 0, sizeof projs);
    memset(tracers, 0, sizeof tracers);
    memset(picks, 0, sizeof picks);
    targets_clear();
    g_gangHeat[0] = g_gangHeat[1] = 0;
    memset(g_turfProg, 0, sizeof g_turfProg);
    g_wanted = 0;
    incidentT = 0;
    aimHold = 0;
    g_pl.weapon = W_FISTS;
    for (int w = 1; w < NUM_WEAPONS; w++) {
        int m = weap_mag(w);
        g_pl.clip[w] = g_prog.ammo[w] < m ? g_prog.ammo[w] : m;
        if (!owned(w)) g_pl.clip[w] = 0;
    }
}
void combat_update(float dt) {
    if (hurtSnd > 0) hurtSnd -= dt;
    if (g_pl.flash > 0) g_pl.flash -= dt;
    if (incidentT > 0) incidentT -= dt;
    for (int g = 0; g < 2; g++) if (g_gangHeat[g] > 0) g_gangHeat[g] -= dt;
    for (int i = 0; i < MAX_TRACERS; i++) if (tracers[i].life > 0) tracers[i].life -= dt;
    death_update(dt);
    if (!g_pl.dead) {
        if (g_pl.car < 0) foot_weapons(dt);
        else {
            g_pl.aiming = 0;
            Car *c = &g_cars[g_pl.car];
            if (is_aircraft(c) && !mission_frozen()) air_weapons(c, dt);
        }
    }
    thugs_update(dt);
    projs_update(dt);
    picks_update(dt);
    targets_update(dt);
    wanted_update(dt);
    g_prog.bodyArmor = (int)g_pl.armor;
    incomeT += dt;
    if (incomeT > 60) {
        incomeT = 0;
        int n = 0;
        for (int k = 0; k < NUM_TURF; k++) n += turf_owned(k);
        if (n) { add_coins(10 * n); hud_toast("TURF INCOME +%d COINS", 10 * n); audio_play(SND_COIN, 0.5f, 0.9f); }
    }
}

/* -------------------------------------------------------------- render */
void combat_render(void) {
    for (int i = 0; i < MAX_THUGS; i++) {
        Thug *h = &thugs[i];
        if (!h->active || dist2xz(h->pos, g_camPos) > 170 * 170) continue;
        const Look *L = h->gang == GANG_POLICE ? &gangLook[4] : &gangLook[h->gang * 2 + (i & 1)];
        if (h->dead || h->knock > 0) {
            glPushMatrix();
            glTranslatef(h->pos.x, h->pos.y + 0.2f, h->pos.z);
            glRotatef(h->yaw * DEG, 0, 1, 0);
            glRotatef(-90, 1, 0, 0);
            draw_character(v3(0, -0.9f, 0), 0, 0, 0, L, 0, 1);
            glPopMatrix();
        } else {
            int aimingNow = h->tgt != -1;
            draw_character_ex(h->pos, h->yaw, h->anim, h->tgt == -1 || (h->gang == GANG_POLICE) ? 0.6f : 0, L, 0, 0, aimingNow ? h->weapon : 0, 0);
        }
    }
    for (int i = 0; i < MAX_PICKS; i++) {
        Pick *k = &picks[i];
        if (!k->alive) continue;
        glPushMatrix();
        glTranslatef(k->p.x, k->p.y + sinf(g_gameTime * 3 + i) * 0.15f, k->p.z);
        glRotatef(g_gameTime * 120, 0, 1, 0);
        if (k->kind == 0) {
            glColor3f(0.95f, 0.15f, 0.2f);
            gl_cube(-0.15f, 0.12f, 0, 0.17f, 0.17f, 0.1f);
            gl_cube(0.15f, 0.12f, 0, 0.17f, 0.17f, 0.1f);
            gl_cube(0, -0.08f, 0, 0.2f, 0.2f, 0.1f);
        } else {
            glColor3f(0.3f, 0.55f, 0.25f);
            gl_cube(0, 0, 0, 0.3f, 0.2f, 0.2f);
            glColor3f(0.95f, 0.8f, 0.2f);
            gl_cube(0, 0.21f, 0, 0.2f, 0.02f, 0.12f);
        }
        glPopMatrix();
    }
    for (int i = 0; i < MAX_TARGETS; i++) {
        Target *t = &g_targets[i];
        if (!t->alive) continue;
        glPushMatrix();
        glTranslatef(t->pos.x, t->pos.y, t->pos.z);
        if (t->kind == 0) {
            float face = atan2f(g_camPos.x - t->pos.x, g_camPos.z - t->pos.z);
            glColor3f(0.4f, 0.3f, 0.2f);
            gl_box(-0.06f, -t->pos.y, -0.06f, 0.06f, -0.8f, 0.06f);
            glRotatef(face * DEG, 0, 1, 0);
            glRotatef(90, 0, 1, 0);
            for (int r = 0; r < 4; r++) {
                if (r & 1) glColor3f(1, 1, 1); else glColor3f(0.9f, 0.12f, 0.1f);
                gl_cyl_x(0.02f * r, 0, 0, 0.85f - r * 0.2f, 0.04f, 16);
            }
        } else {
            glRotatef(t->t * 40, 0, 1, 0);
            glColor3f(0.85f, 0.85f, 0.9f);
            gl_cube(0, 0, 0, 0.7f, 0.25f, 0.7f);
            glColor3f(0.2f, 0.2f, 0.22f);
            for (int a = 0; a < 4; a++) {
                glPushMatrix();
                glRotatef(a * 90.0f + 45, 0, 1, 0);
                gl_box(-0.08f, -0.05f, 0.5f, 0.08f, 0.05f, 1.5f);
                glTranslatef(0, 0.12f, 1.5f);
                glRotatef(g_gameTime * 2000, 0, 1, 0);
                gl_box(-0.6f, 0, -0.06f, 0.6f, 0.03f, 0.06f);
                glPopMatrix();
            }
            int blink = ((int)(g_gameTime * 3 + i)) & 1;
            glColor3f(blink ? 1 : 0.4f, 0.15f, 0.1f);
            gl_cube(0, -0.35f, 0, 0.18f, 0.12f, 0.18f);
        }
        glPopMatrix();
    }
    for (int i = 0; i < MAX_PROJ; i++) {
        Proj *q = &projs[i];
        if (!q->alive) continue;
        glColor3f(0.85f, 0.85f, 0.8f);
        gl_cube(q->p.x, q->p.y, q->p.z, 0.18f, 0.18f, 0.18f);
    }
}
void combat_render_fx(void) {
    glBegin(GL_LINES);
    for (int i = 0; i < MAX_TRACERS; i++) {
        Tracer *t = &tracers[i];
        if (t->life <= 0) continue;
        float a = clampf(t->life / 0.07f, 0, 1);
        V3 mid = vadd(t->a, vmul(vsub(t->b, t->a), 0.15f));
        glColor4f(t->c[0], t->c[1], t->c[2], 0.0f);
        glVertex3f(t->a.x, t->a.y, t->a.z);
        glColor4f(t->c[0], t->c[1], t->c[2], a);
        glVertex3f(mid.x, mid.y, mid.z);
        glVertex3f(mid.x, mid.y, mid.z);
        glVertex3f(t->b.x, t->b.y, t->b.z);
    }
    glEnd();
}
void combat_map_markers(void (*cb)(V3 p, int kind)) {
    V3 pp = player_pos();
    for (int i = 0; i < MAX_THUGS; i++) {
        Thug *h = &thugs[i];
        if (!h->active || h->dead || dist2xz(h->pos, pp) > 130 * 130) continue;
        cb(h->pos, h->gang == GANG_RATS ? 3 : h->gang == GANG_VIPERS ? 6 : 7);
    }
    if (wanted_stars())
        for (int i = 0; i < MAX_CARS; i++)
            if (g_cars[i].used && g_cars[i].ai == AI_POLICE && !g_cars[i].wrecked) cb(g_cars[i].pos, 7);
}
void turf_draw_map(void) {
    if (g_curMap != 0) return;
    glBegin(GL_QUADS);
    for (int k = 0; k < NUM_TURF; k++) {
        float x0, z0, x1, z1;
        block_rect(g_turfs[k].bi, g_turfs[k].bj, &x0, &z0, &x1, &z1);
        if (turf_owned(k)) glColor4f(0.2f, 1.0f, 0.3f, 0.35f);
        else if (g_turfs[k].gang == GANG_RATS) glColor4f(1.0f, 0.15f, 0.1f, 0.4f);
        else glColor4f(0.7f, 0.2f, 1.0f, 0.4f);
        glVertex2f(x0, z0); glVertex2f(x1, z0); glVertex2f(x1, z1); glVertex2f(x0, z1);
    }
    glEnd();
}
