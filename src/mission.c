/* Story missions for Maple Bay. */
#include "game.h"

int spawn_ai_car(int type, int k, int l, int dir, int ai, float speed);

enum { MT_CHECK, MT_COLLECT, MT_SMASH, MT_CHASE, MT_REACH, MT_RACE, MT_FLY, MT_SHOOT, MT_KILL, MT_LAND, MT_BOSS };
typedef struct {
    const char *name, *giver, *brief, *step0, *stepN;
    int type;
    float time;
    int reward, needCar, look; /* needCar: 1 car, 2 plane, 3 helicopter */
} MDef;
#define B(i) (1u << (i))
static const unsigned need[NUM_MISSIONS] = {0, B(0), B(0) | B(1), 0x7, 0xF, 0x1F, 0x3F, 0x7F, 0, 0, B(9), B(8), B(8) | B(11), B(10) | B(12), 0, 0};
/* which island each mission's giver lives on */
static const int onMap[NUM_MISSIONS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2};

static const MDef defs[NUM_MISSIONS] = {
    {"WHEELS OF FORTUNE", "GRANDMA GEARBOX",
     "Grandma Gearbox was the fastest courier Maple Bay ever had. She wants to know if you inherited the family talent. Drive through every checkpoint before the clock runs out!",
     "DRIVE THROUGH THE CHECKPOINTS", "CHECKPOINT %d OF %d", MT_CHECK, 80, 150, 1, 0},
    {"PIZZA PANIC", "CHEF TONY",
     "My delivery kid quit and three customers are STARVING! Grab the pizzas from my counter and deliver them all while they're still hot. A car might help...",
     "PICK UP THE PIZZAS AT TONY'S", "DELIVER PIZZA %d OF %d", MT_CHECK, 120, 200, 0, 1},
    {"GIZMO GATHERING", "DR. FIZZ",
     "Kaboom! My teleporter exploded and scattered eight glowing power cells across the east side of town. Collect every one before they fizzle out!",
     "COLLECT THE POWER CELLS", "POWER CELLS %d OF %d", MT_COLLECT, 150, 250, 0, 2},
    {"CRATE EXPECTATIONS", "DOCK BOSS MOE",
     "Somebody stashed contraband in the glowing orange crates around my docks. Smash every single one - kick 'em or ram 'em, I don't care how!",
     "SMASH THE ORANGE CRATES", "CRATES SMASHED %d OF %d", MT_SMASH, 110, 300, 0, 3},
    {"GETAWAY GRAB", "OFFICER PRINGLE",
     "A black van just robbed the Bolt & Bargain! My cruiser is in the shop, so it's up to you. Ram that van until it's wrecked - and don't let it get away!",
     "RAM THE GETAWAY VAN UNTIL IT IS WRECKED", "", MT_CHASE, 120, 400, 1, 4},
    {"TOWER TROUBLE", "COACH HOPS",
     "Fitness test! Climb the floating platforms all the way to the top of the old clock tower before my whistle blows. On foot only - no cars!",
     "CLIMB TO THE TOP OF THE CLOCK TOWER", "", MT_REACH, 75, 350, 0, 5},
    {"MAYOR'S GRAND PRIX", "MAYOR PICKLES",
     "Welcome to the first ever Maple Bay Grand Prix! Beat my three hand-picked racers around the city circuit and the prize money is all yours.",
     "WIN THE RACE", "CHECKPOINT %d OF %d", MT_RACE, 150, 500, 1, 6},
    {"BOLT'S LAST STAND", "MAYOR PICKLES",
     "It was Baron Von Bolt behind ALL the trouble in town! He's escaping in his armored car right now. Smash it to scrap and save Maple Bay!",
     "WRECK BARON VON BOLT'S ARMORED CAR", "", MT_CHASE, 170, 1000, 1, 6},
    {"FIRST FLIGHT", "CAPTAIN SKYE",
     "Welcome to Maple Bay Airport, rookie! Your Sky Hopper is on the runway. Hold THROTTLE to speed up, pull up once you are fast enough, and fly through every ring.",
     "FLY THROUGH THE RINGS", "RING %d OF %d", MT_FLY, 170, 300, 2, 11},
    {"HOT SHOTS", "SARGE BOOMER",
     "So you want to handle a firearm, eh? Here's a pistol on the house. Get out on my range and knock down all twelve targets. Hold AIM for a steadier shot!",
     "SHOOT ALL THE TARGETS", "TARGETS %d OF %d", MT_SHOOT, 90, 200, 0, 7},
    {"TURF TROUBLE", "NANA RUTH",
     "Those Rust Rats have been terrorizing my street, sonny. Go up to their turf north of the clock tower and teach ten of them some manners. Mind the police, dear!",
     "TAKE DOWN THE RUST RATS", "RUST RATS %d OF %d", MT_KILL, 240, 400, 0, 9},
    {"ROOFTOP DROP", "CAPTAIN SKYE",
     "Downtown needs medical supplies and the roads are jammed. Take the helicopter, land on three skyscraper rooftops and drop the crates. Gentle landings!",
     "LAND ON THE ROOFTOP HELIPADS", "HELIPAD %d OF %d", MT_LAND, 210, 450, 3, 11},
    {"BALLOON BUSTERS", "PROFESSOR GALE",
     "My weather drones have gone haywire and are spying on everyone! Shoot down all ten - aircraft guns work wonders, and a sniper rifle is not bad either.",
     "SHOOT DOWN THE ROGUE DRONES", "DRONES %d OF %d", MT_SHOOT, 260, 500, 0, 2},
    {"VIPER KING", "OFFICER PRINGLE",
     "The Neon Vipers' boss has stolen an armed gunship and is shooting up the town! I've left a rocket launcher for you. Bring that thing down - by any means!",
     "SHOOT DOWN THE VIPER GUNSHIP", "", MT_BOSS, 260, 1200, 0, 4},
    {"DESERT DASH", "LUCKY LOU",
     "Welcome to Sunset Sands, high roller! I bet you can't lap my island before the sun sets. Hit every checkpoint and the jackpot is yours.",
     "DRIVE THROUGH THE CHECKPOINTS", "CHECKPOINT %d OF %d", MT_CHECK, 110, 450, 1, 3},
    {"SUMMIT SCRAMBLE", "MAMA MOOSE",
     "Brrr! The blizzard blew my eight trail flags all over Frostpeak. Bring every one back before you freeze - try the ski jump!",
     "COLLECT THE TRAIL FLAGS", "FLAGS %d OF %d", MT_COLLECT, 180, 450, 0, 10},
};

static int cur = -1, phase;
static float timer, cd, msgT;
static int step, npts, total;
static V3 pts[24];
static int got[24];
static int crateSlot[16], ncr;
static int target = -1, mcar = -1;
static int racers[3] = {-1, -1, -1};
static float farT;

/* Maple Bay missions were authored on the original 8x8 town, which now sits BO blocks in */
static V3 R(int k, int l) { return v3(road_c(k + BO), 0, road_c(l + BO)); }
static V3 edge(int i, int j, int side, float off) { V3 p; float y; block_edge_spot(i + BO, j + BO, side, off, &p, &y); return p; }
static V3 Rn(int k, int l) { return v3(road_c(k), 0, road_c(l)); }
static V3 edgen(int i, int j, int side, float off) { V3 p; float y; block_edge_spot(i, j, side, off, &p, &y); return p; }

void mission_init(void) { cur = -1; }
int mission_active(void) { return cur >= 0; }
int mission_frozen(void) { return cur >= 0 && phase == 0; }
const char *mission_name(int i) { return defs[i].name; }
const char *mission_giver(int i) { return defs[i].giver; }
const char *mission_brief(int i) { return defs[i].brief; }
float mission_time(int i) { return defs[i].time; }
int mission_is_needcar(int i) { return defs[i].needCar; }
int mission_reward(int i) { return (g_prog.missions & (1u << i)) ? defs[i].reward / 4 : defs[i].reward; }
int mission_available(int i) { return (g_prog.missions & need[i]) == need[i]; }
static int giver_visible(int i) {
    if (onMap[i] != g_curMap) return 0;
    int d6 = (g_prog.missions >> 6) & 1, d8 = (g_prog.missions >> 8) & 1;
    if (i == 6) return !d6;
    if (i == 7) return d6;
    if (i == 8) return !d8;
    if (i == 11) return d8;
    return 1;
}
int mission_cur(void) { return cur; }
int mission_kill_gang(void) { return cur >= 0 && defs[cur].type == MT_KILL && phase == 1 ? GANG_RATS : -1; }
int mission_near_giver(V3 p) {
    int best = -1;
    for (int i = 0; i < NUM_MISSIONS; i++) {
        if (!mission_available(i) || !giver_visible(i)) continue;
        if (dist2xz(p, g_spots.giver[i]) < 3.4f * 3.4f) {
            if (best < 0 || !(g_prog.missions & (1u << i))) best = i;
        }
    }
    return best;
}
int mission_race_point(int idx, V3 *out) {
    if (cur < 0 || defs[cur].type != MT_RACE || idx >= npts) return 0;
    *out = pts[idx];
    return 1;
}

static void cleanup(void) {
    for (int i = 0; i < ncr; i++) g_crates[crateSlot[i]].alive = 0;
    if (cur >= 0 && defs[cur].type == MT_SHOOT) targets_clear();
    ncr = 0;
    sim_clear_mission_cars();
    if (mcar >= 0 && g_cars[mcar].used) g_cars[mcar].mission = 0;
    target = mcar = -1;
    racers[0] = racers[1] = racers[2] = -1;
    cur = -1;
}
void mission_abort(void) {
    if (cur >= 0) cleanup();
    cur = -1;
}
static void fail(const char *why) {
    hud_msg(4, "MISSION FAILED - %s", why);
    audio_play(SND_FAIL, 0.7f, 1);
    cleanup();
}
static void succeed(void) {
    int i = cur;
    int rw = mission_reward(i);
    add_coins(rw);
    g_prog.missions |= 1u << i;
    unsigned all = (1u << NUM_MISSIONS) - 1;
    if ((g_prog.missions & all) == all && i == NUM_MISSIONS - 1)
        hud_msg(6, "MAPLE BAY IS SAVED!  +%d COINS", rw);
    else
        hud_msg(4, "MISSION COMPLETE!  +%d COINS", rw);
    audio_play(SND_SUCCESS, 0.8f, 1);
    cleanup();
    save_game();
}

void mission_fail_reason(const char *why) {
    if (cur >= 0) fail(why);
}
void mission_on_kill(int gang) {
    if (cur < 0 || defs[cur].type != MT_KILL || phase == 0 || gang != GANG_RATS) return;
    step++;
    audio_play(SND_CHECK, 0.6f, 1 + step * 0.03f);
    if (step >= total) succeed();
}
static void give_air(int type, V3 p, float yaw) {
    if (g_pl.car >= 0) exit_vehicle();
    for (int i = 0; i < MAX_CARS; i++)
        if (g_cars[i].used && i != g_pl.car && dist2xz(g_cars[i].pos, p) < 100) g_cars[i].used = 0;
    int ci = spawn_car(type, p, yaw, AI_PARKED);
    if (ci < 0) return;
    if (g_prog.airPaint > 0) memcpy(g_cars[ci].col, g_paints[g_prog.airPaint], sizeof g_cars[ci].col);
    put_player_in_car(ci);
    g_cars[ci].mission = 1;
    mcar = ci;
}
/* pick three tall downtown towers in different quarters for helicopter landings */
static void find_rooftops(void) {
    int qd[3][2] = {{-1, -1}, {1, -1}, {1, 1}};
    for (int q = 0; q < 3; q++) {
        int best = -1;
        float bh = 0;
        for (int i = 0; i < g_nboxes; i++) {
            Box *b = &g_boxes[i];
            if (b->kind != K_BUILDING || b->roof != 0) continue;
            float cx = (b->mn.x + b->mx.x) / 2, cz = (b->mn.z + b->mx.z) / 2;
            if (fabsf(cx) > 150 || fabsf(cz) > 150 || cx * qd[q][0] < 10 || cz * qd[q][1] < 10) continue;
            if (b->mx.x - b->mn.x < 14) continue;
            if (b->mx.y > bh) { bh = b->mx.y; best = i; }
        }
        if (best < 0) { pts[npts++] = v3(g_spots.clinic.x, 11, g_spots.clinic.z - 16); continue; }
        Box *b = &g_boxes[best];
        pts[npts++] = v3((b->mn.x + b->mx.x) / 2 + 3, b->mx.y, (b->mn.z + b->mx.z) / 2 - 3);
    }
}

static void give_car(V3 near) {
    Car *pc = player_car();
    if (pc && !pc->wrecked && !is_aircraft(pc)) { mcar = g_pl.car; return; }
    if (pc) exit_vehicle();
    int ci = spawn_player_car_near(near);
    if (ci >= 0) {
        put_player_in_car(ci);
        mcar = ci;
    }
}


/* spawn a fleeing car on the road network a short distance from the player */
static int spawn_chase_target(int type, float speed) {
    static const int DXs[4] = {1, 0, -1, 0}, DZs[4] = {0, 1, 0, -1};
    V3 pp = player_pos();
    int k = (int)floorf((pp.x + CITY_HALF) / CELL + 0.5f), l = (int)floorf((pp.z + CITY_HALF) / CELL + 0.5f);
    if (k < 0) k = 0;
    if (k > NBLK) k = NBLK;
    if (l < 0) l = 0;
    if (l > NBLK) l = NBLK;
    int best = -1;
    float bd = -1;
    for (int d = 0; d < 4; d++) {
        int nk = k + DXs[d], nl = l + DZs[d];
        if (nk < 0 || nk > NBLK || nl < 0 || nl > NBLK) continue;
        V3 sp = v3(road_c(k) + DXs[d] * 14.0f, 0, road_c(l) + DZs[d] * 14.0f);
        float dd = distxz(sp, pp);
        if (dd > bd) { bd = dd; best = d; }
    }
    if (best < 0) best = 0;
    return spawn_ai_car(type, k, l, best, AI_FLEE, speed);
}

void mission_start(int i) {
    if (cur >= 0) cleanup();
    cur = i;
    phase = 0;
    cd = 3.0f;
    timer = defs[i].time;
    step = 0;
    npts = 0;
    ncr = 0;
    total = 0;
    target = mcar = -1;
    farT = 0;
    msgT = 0;
    memset(got, 0, sizeof got);
    V3 gp = g_spots.giver[i];
    switch (i) {
    case 0: {
        static const int seq[7][2] = {{1, 1}, {1, 3}, {3, 3}, {3, 5}, {5, 5}, {5, 2}, {2, 2}};
        for (int k = 0; k < 7; k++) pts[npts++] = R(seq[k][0], seq[k][1]);
        give_car(gp);
    } break;
    case 1:
        pts[npts++] = v3(-155, 0, 170);
        pts[npts++] = edge(0, 4, 1, 4);
        pts[npts++] = edge(0, 7, 1, -6);
        pts[npts++] = edge(2, 7, 2, 4);
        break;
    case 2: {
        pts[npts++] = edge(5, 3, 0, 10);
        pts[npts++] = edge(6, 4, 1, -5);
        pts[npts++] = edge(7, 5, 3, 8);
        pts[npts++] = edge(5, 6, 2, -12);
        pts[npts++] = edge(6, 7, 0, 0);
        pts[npts++] = edge(7, 3, 1, 0);
        pts[npts++] = edge(5, 5, 3, 15);
        pts[npts++] = edge(6, 5, 2, 0);
        for (int k = 0; k < npts; k++) pts[k].y = 1.2f;
        total = npts;
    } break;
    case 3: {
        static const float cp[10][2] = {{219.5f, -260}, {219.5f, -240}, {200.5f, -250}, {200.5f, -225}, {235, -219.5f},
                                        {260, -219.5f}, {230, -200.5f}, {255, -200.5f}, {292, -245}, {292, -265}};
        for (int k = 0; k < 10; k++) {
            crateSlot[ncr] = add_crate_at(MISSION_CRATE_BASE + k, v3(cp[k][0], 0, cp[k][1]), 1);
            ncr++;
        }
        total = ncr;
    } break;
    case 4:
        give_car(gp);
        target = spawn_chase_target(CAR_GETAWAY, 25);
        break;
    case 5:
        pts[npts++] = vadd(g_spots.towerTop, v3(0, 0.2f, 0));
        break;
    case 6: {
        static const int seq[6][2] = {{4, 2}, {6, 2}, {6, 6}, {2, 6}, {2, 2}, {4, 2}};
        for (int k = 0; k < 6; k++) pts[npts++] = R(seq[k][0], seq[k][1]);
        float z = road_c(3 + BO) - 14;
        static const float xs[4] = {-5.4f, -1.8f, 5.4f, 1.8f};
        for (int r = 0; r < 3; r++) {
            racers[r] = spawn_car(CAR_RACER, v3(xs[r], 0, z), PI, AI_RACE);
            if (racers[r] >= 0) {
                Car *c = &g_cars[racers[r]];
                static const float rc[3][3] = {{0.9f, 0.1f, 0.1f}, {0.1f, 0.75f, 0.2f}, {0.95f, 0.95f, 0.95f}};
                memcpy(c->col, rc[r], sizeof c->col);
                c->mission = 1;
                c->topSpeed = 33 + r * 1.5f;
                c->raceOff = (r - 1) * 2.5f;
            }
        }
        Car *pc = player_car();
        if (pc && is_aircraft(pc)) { exit_vehicle(); pc = NULL; }
        int ci = (pc && !pc->wrecked) ? g_pl.car : spawn_car(g_prog.curCar, v3(xs[3], 0, z), PI, AI_NONE);
        if (ci >= 0) {
            g_cars[ci].pos = v3(xs[3], 0, z);
            g_cars[ci].yaw = PI;
            g_cars[ci].vel = v3(0, 0, 0);
            g_cars[ci].vy = 0;
            g_cars[ci].onGround = 1;
            put_player_in_car(ci);
            mcar = ci;
        }
    } break;
    case 7:
        give_car(gp);
        target = spawn_chase_target(CAR_BOSS, 29);
        break;
    case 8: {
        static const float rp[8][3] = {{-140, 14, 487}, {-20, 26, 487}, {100, 40, 480}, {200, 52, 420}, {150, 58, 320},
                                       {20, 60, 260}, {-130, 50, 320}, {-230, 35, 430}};
        for (int k = 0; k < 8; k++) pts[npts++] = v3(rp[k][0], rp[k][1], AZ(rp[k][2]));
        give_air(AIR_FIRST, g_spots.runway, g_spots.runwayYaw);
    } break;
    case 9:
        if (!((g_prog.weapons >> W_PISTOL) & 1)) give_weapon(W_PISTOL, 36);
        else if (g_prog.ammo[W_PISTOL] < 30) give_weapon(W_PISTOL, 30 - g_prog.ammo[W_PISTOL]);
        if (g_pl.weapon == W_FISTS) g_pl.weapon = W_PISTOL;
        targets_clear();
        for (int k = 0; k < 12; k++) target_add(g_spots.range[k], 0, 1);
        total = 12;
        break;
    case 10:
        if (!(g_prog.weapons & ~1u)) { give_weapon(W_PISTOL, 36); g_pl.weapon = W_PISTOL; hud_toast("NANA GAVE YOU A PISTOL"); }
        total = 10;
        break;
    case 11:
        find_rooftops();
        give_air(AIR_FIRST + 2, v3(-80, 0, AZ(432)), 0);
        break;
    case 12: {
        targets_clear();
        static const float dp[10][3] = {{-120, 34, -130}, {-40, 48, -150}, {60, 40, -120}, {140, 52, -60}, {120, 36, 40},
                                        {40, 55, 120}, {-60, 38, 140}, {-150, 46, 60}, {-20, 60, 0}, {200, 40, 200}};
        for (int k = 0; k < 10; k++) target_add(v3(dp[k][0], dp[k][1], dp[k][2]), 1, 30);
        total = 10;
    } break;
    case 14: {
        static const int seq[8][2] = {{1, 1}, {5, 1}, {5, 3}, {3, 3}, {3, 5}, {5, 5}, {1, 5}, {1, 3}};
        for (int k = 0; k < 8; k++) pts[npts++] = Rn(seq[k][0], seq[k][1]);
        give_car(gp);
    } break;
    case 15: {
        pts[npts++] = v3(g_gizmoPos[15].x + 1, 17.2f, g_gizmoPos[15].z + 3);
        pts[npts++] = edgen(1, 1, 0, 0);
        pts[npts++] = edgen(3, 0, 1, 8);
        pts[npts++] = edgen(5, 2, 3, -6);
        pts[npts++] = edgen(4, 5, 2, 10);
        pts[npts++] = edgen(0, 4, 1, 0);
        pts[npts++] = edgen(2, 3, 0, -12);
        pts[npts++] = v3(g_gizmoPos[17].x + 4, 0, g_gizmoPos[17].z);
        for (int k = 1; k < npts; k++) pts[k].y = 1.2f;
        total = npts;
    } break;
    case 13: {
        if (!((g_prog.weapons >> W_ROCKET) & 1)) give_weapon(W_ROCKET, 6);
        else if (g_prog.ammo[W_ROCKET] < 6) give_weapon(W_ROCKET, 6 - g_prog.ammo[W_ROCKET]);
        V3 pp = player_pos();
        target = spawn_boss_heli(v3(pp.x + 60, pp.y + 35, pp.z + 20));
        g_gangHeat[GANG_VIPERS] = 60;
    } break;
    }
    if (target >= 0) g_cars[target].mission = 1;
    audio_play(SND_BEEP, 0.6f, 1);
}

void mission_on_crate(int crate) {
    if (cur < 0 || defs[cur].type != MT_SMASH) return;
    for (int k = 0; k < ncr; k++)
        if (crateSlot[k] == crate) {
            step++;
            audio_play(SND_CHECK, 0.6f, 1);
            if (step >= total) succeed();
            return;
        }
}

void mission_update(float dt) {
    if (cur < 0) return;
    if (msgT > 0) msgT -= dt;
    if (phase == 0) {
        float before = cd;
        cd -= dt;
        if (floorf(before) != floorf(cd) && cd > 0) audio_play(SND_BEEP, 0.6f, 1);
        if (cd <= 0) { phase = 1; audio_play(SND_GO, 0.6f, 1); }
        return;
    }
    timer -= dt;
    if (timer <= 0) { fail("OUT OF TIME"); return; }
    const MDef *d = &defs[cur];
    if (mcar >= 0 && (!g_cars[mcar].used || g_cars[mcar].wrecked)) { fail("YOUR VEHICLE WAS WRECKED"); return; }
    V3 pp = player_pos();
    switch (d->type) {
    case MT_CHECK:
        if (distxz(pp, pts[step]) < (g_pl.car >= 0 ? 8.0f : 3.0f) && fabsf(pp.y - pts[step].y) < 6) {
            step++;
            audio_play(SND_CHECK, 0.6f, 1 + step * 0.03f);
            if (step >= npts) succeed();
        }
        break;
    case MT_COLLECT:
        for (int k = 0; k < npts; k++) {
            if (got[k]) continue;
            if (dist3(vadd(pp, v3(0, 1, 0)), pts[k]) < (g_pl.car >= 0 ? 3.6f : 2.2f)) {
                got[k] = 1;
                step++;
                audio_play(SND_CHECK, 0.6f, 1 + step * 0.03f);
                if (step >= total) { succeed(); return; }
            }
        }
        break;
    case MT_SMASH: break;
    case MT_CHASE: {
        if (target < 0 || !g_cars[target].used) { fail("THE TARGET VANISHED"); return; }
        Car *t = &g_cars[target];
        if (t->wrecked) { succeed(); return; }
        float dd = distxz(pp, t->pos);
        if (dd > 200) farT += dt; else farT = 0;
        if (farT > 5) { fail("IT GOT AWAY"); return; }
    } break;
    case MT_REACH:
        if (g_pl.car < 0 && dist3(pp, pts[0]) < 3.0f) { succeed(); return; }
        if (g_pl.car >= 0 && msgT <= 0) { hud_toast("GET OUT OF THE CAR - THIS IS A CLIMBING CHALLENGE"); msgT = 6; }
        break;
    case MT_FLY: {
        Car *pc = player_car();
        if (pc && is_aircraft(pc) && step < npts && dist3(pp, pts[step]) < 11) {
            step++;
            audio_play(SND_CHECK, 0.6f, 1 + step * 0.04f);
            if (step >= npts) { succeed(); return; }
        }
    } break;
    case MT_SHOOT:
        step = total - targets_alive();
        if (step >= total) { succeed(); return; }
        break;
    case MT_KILL: break;
    case MT_LAND: {
        Car *pc = player_car();
        if (pc && g_carTypes[pc->type].kind == VK_HELI && pc->onGround && step < npts && distxz(pp, pts[step]) < 7 && fabsf(pp.y - pts[step].y) < 2) {
            step++;
            add_coins(25);
            audio_play(SND_CHECK, 0.6f, 1 + step * 0.04f);
            hud_toast("SUPPLIES DELIVERED! +25 COINS");
            if (step >= npts) { succeed(); return; }
        }
        if ((!pc || g_carTypes[pc->type].kind != VK_HELI) && msgT <= 0) { hud_toast("GET IN A HELICOPTER TO LAND ON THE ROOFTOPS"); msgT = 6; }
    } break;
    case MT_BOSS: {
        if (target < 0 || !g_cars[target].used) { fail("THE GUNSHIP ESCAPED"); return; }
        if (g_cars[target].wrecked) { succeed(); return; }
    } break;
    case MT_RACE:
        if (step < npts && distxz(pp, pts[step]) < 10) {
            step++;
            audio_play(SND_CHECK, 0.6f, 1 + step * 0.03f);
            if (step >= npts) { succeed(); return; }
        }
        for (int r = 0; r < 3; r++)
            if (racers[r] >= 0 && g_cars[racers[r]].used && g_cars[racers[r]].raceIdx >= npts) { fail("YOU LOST THE RACE"); return; }
        break;
    }
}

int mission_target(V3 *out) {
    if (cur < 0) return 0;
    const MDef *d = &defs[cur];
    V3 pp = player_pos();
    switch (d->type) {
    case MT_CHECK: case MT_RACE: case MT_FLY: case MT_LAND:
        if (step >= npts) return 0;
        *out = pts[step];
        return 1;
    case MT_REACH: *out = pts[0]; return 1;
    case MT_SHOOT: {
        float bd = 1e18f;
        int bi = -1;
        for (int k = 0; k < MAX_TARGETS; k++) if (g_targets[k].alive && dist2xz(pp, g_targets[k].pos) < bd) { bd = dist2xz(pp, g_targets[k].pos); bi = k; }
        if (bi < 0) return 0;
        *out = g_targets[bi].pos;
        return 1;
    }
    case MT_KILL: {
        int f;
        V3 p = thug_nearest(pp, GANG_RATS, &f);
        if (!f) { float x0, z0, x1, z1; block_rect(4 + BO, 0 + BO, &x0, &z0, &x1, &z1); p = v3((x0 + x1) / 2, 0, (z0 + z1) / 2); }
        *out = p;
        return 1;
    }
    case MT_BOSS:
        if (target < 0) return 0;
        *out = g_cars[target].pos;
        return 1;
    case MT_COLLECT: {
        float bd = 1e18f;
        int bi = -1;
        for (int k = 0; k < npts; k++) if (!got[k] && dist2xz(pp, pts[k]) < bd) { bd = dist2xz(pp, pts[k]); bi = k; }
        if (bi < 0) return 0;
        *out = pts[bi];
        return 1;
    }
    case MT_SMASH: {
        float bd = 1e18f;
        int bi = -1;
        for (int k = 0; k < ncr; k++) {
            Crate *c = &g_crates[crateSlot[k]];
            if (c->alive && dist2xz(pp, c->pos) < bd) { bd = dist2xz(pp, c->pos); bi = k; }
        }
        if (bi < 0) return 0;
        *out = g_crates[crateSlot[bi]].pos;
        return 1;
    }
    case MT_CHASE:
        if (target < 0) return 0;
        *out = g_cars[target].pos;
        return 1;
    }
    return 0;
}

void mission_map_markers(void (*cb)(V3 p, int kind)) {
    for (int i = 0; i < NUM_MISSIONS; i++) {
        if (!giver_visible(i) || !mission_available(i)) continue;
        cb(g_spots.giver[i], (g_prog.missions & (1u << i)) ? 2 : 1);
    }
    if (cur < 0) return;
    const MDef *d = &defs[cur];
    if (d->type == MT_COLLECT) {
        for (int k = 0; k < npts; k++) if (!got[k]) cb(pts[k], 0);
    } else if (d->type == MT_SMASH) {
        for (int k = 0; k < ncr; k++) if (g_crates[crateSlot[k]].alive) cb(g_crates[crateSlot[k]].pos, 0);
    } else if (d->type == MT_SHOOT) {
        for (int k = 0; k < MAX_TARGETS; k++) if (g_targets[k].alive) cb(g_targets[k].pos, 0);
    } else if (d->type == MT_RACE) {
        V3 p;
        if (mission_target(&p)) cb(p, 0);
        for (int r = 0; r < 3; r++) if (racers[r] >= 0 && g_cars[racers[r]].used) cb(g_cars[racers[r]].pos, 3);
    } else {
        V3 p;
        if (mission_target(&p)) cb(p, 0);
    }
}

static void beacon(V3 p, float r, float h, float cr, float cg, float cb, float a) {
    glColor4f(cr, cg, cb, a);
    glBegin(GL_QUADS);
    for (int i = 0; i < 16; i++) {
        float a0 = 2 * PI * i / 16, a1 = 2 * PI * (i + 1) / 16;
        glColor4f(cr, cg, cb, a);
        glVertex3f(p.x + cosf(a0) * r, p.y, p.z + sinf(a0) * r);
        glVertex3f(p.x + cosf(a1) * r, p.y, p.z + sinf(a1) * r);
        glColor4f(cr, cg, cb, 0);
        glVertex3f(p.x + cosf(a1) * r, p.y + h, p.z + sinf(a1) * r);
        glVertex3f(p.x + cosf(a0) * r, p.y + h, p.z + sinf(a0) * r);
    }
    glEnd();
}
static void sky_ring(V3 c, V3 n, float r, float cr, float cg, float cb, float a) {
    V3 up = fabsf(n.y) > 0.9f ? v3(1, 0, 0) : v3(0, 1, 0);
    V3 u = vnorm(vcross(n, up)), w = vcross(u, n);
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= 32; i++) {
        float t = 2 * PI * i / 32;
        V3 d = vadd(vmul(u, cosf(t)), vmul(w, sinf(t)));
        glColor4f(cr, cg, cb, a);
        V3 p0 = vadd(c, vmul(d, r)), p1 = vadd(c, vmul(d, r + 1.2f));
        glVertex3f(p0.x, p0.y, p0.z);
        glVertex3f(p1.x, p1.y, p1.z);
    }
    glEnd();
}
static void marker_text(const char *s, V3 p, float px, float r, float g, float b) {
    float w = text3d_width(s, px);
    V3 o = vsub(p, vmul(g_camRight, w / 2));
    glColor4f(r, g, b, 1);
    text3d(s, o, g_camRight, g_camUp, px);
}

void mission_draw3d(float t) {
    if (t >= 0) {
        /* opaque pass: mission givers and collectible cells */
        for (int i = 0; i < NUM_MISSIONS; i++) {
            if (!giver_visible(i)) continue;
            if (i == 7 && !giver_visible(7)) continue;
            draw_character(g_spots.giver[i], g_spots.giverYaw[i], t * 2 + i, 0, &g_npcLooks[defs[i].look], 0, 0);
        }
        if (cur >= 0 && defs[cur].type == MT_COLLECT) {
            for (int k = 0; k < npts; k++) {
                if (got[k]) continue;
                glPushMatrix();
                glTranslatef(pts[k].x, pts[k].y + sinf(t * 3 + k) * 0.2f, pts[k].z);
                glRotatef(t * 120, 0, 1, 0);
                glColor3f(0.2f, 1.0f, 0.4f);
                gl_cube(0, 0, 0, 0.3f, 0.5f, 0.3f);
                glColor3f(0.9f, 0.9f, 0.9f);
                gl_cube(0, 0.55f, 0, 0.18f, 0.08f, 0.18f);
                glPopMatrix();
            }
        }
        return;
    }
    /* translucent pass */
    float tt = g_gameTime;
    for (int i = 0; i < NUM_MISSIONS; i++) {
        if (!giver_visible(i) || !mission_available(i)) continue;
        if (cur >= 0) continue;
        V3 p = vadd(g_spots.giver[i], v3(0, 2.6f + sinf(tt * 3 + i) * 0.15f, 0));
        int done = (g_prog.missions & (1u << i)) != 0;
        if (done) marker_text("R", p, 0.09f, 0.4f, 0.7f, 1.0f);
        else {
            marker_text("!", p, 0.14f, 1.0f, 0.85f, 0.1f);
            beacon(g_spots.giver[i], 1.2f, 18, 1.0f, 0.85f, 0.2f, 0.35f);
        }
    }
    if (cur < 0) return;
    const MDef *d = &defs[cur];
    switch (d->type) {
    case MT_CHECK: case MT_RACE:
        if (step < npts) beacon(pts[step], g_pl.car >= 0 || d->type == MT_RACE ? 7.0f : 2.5f, 30, 1.0f, 0.85f, 0.15f, 0.4f);
        if (step + 1 < npts) beacon(pts[step + 1], 5.0f, 18, 1.0f, 0.85f, 0.15f, 0.12f);
        break;
    case MT_REACH: beacon(pts[0], 2.5f, 20, 0.3f, 1.0f, 0.4f, 0.45f); break;
    case MT_FLY:
        if (step < npts) {
            V3 prev = step > 0 ? pts[step - 1] : g_spots.runway;
            sky_ring(pts[step], vnorm(vsub(pts[step], prev)), 9, 1.0f, 0.8f, 0.1f, 0.75f);
            if (step + 1 < npts) sky_ring(pts[step + 1], vnorm(vsub(pts[step + 1], pts[step])), 9, 1.0f, 0.8f, 0.1f, 0.25f);
        }
        break;
    case MT_LAND:
        if (step < npts) beacon(pts[step], 5, 30, 0.3f, 0.8f, 1.0f, 0.45f);
        for (int k = step; k < npts; k++) {
            glColor4f(1, 1, 1, 0.9f);
            glBegin(GL_QUADS);
            V3 c = vadd(pts[k], v3(0, 0.05f, 0));
            glVertex3f(c.x - 2.5f, c.y, c.z - 3); glVertex3f(c.x - 1.7f, c.y, c.z - 3); glVertex3f(c.x - 1.7f, c.y, c.z + 3); glVertex3f(c.x - 2.5f, c.y, c.z + 3);
            glVertex3f(c.x + 1.7f, c.y, c.z - 3); glVertex3f(c.x + 2.5f, c.y, c.z - 3); glVertex3f(c.x + 2.5f, c.y, c.z + 3); glVertex3f(c.x + 1.7f, c.y, c.z + 3);
            glVertex3f(c.x - 1.7f, c.y, c.z - 0.4f); glVertex3f(c.x + 1.7f, c.y, c.z - 0.4f); glVertex3f(c.x + 1.7f, c.y, c.z + 0.4f); glVertex3f(c.x - 1.7f, c.y, c.z + 0.4f);
            glEnd();
        }
        break;
    case MT_SHOOT:
        for (int k = 0; k < MAX_TARGETS; k++) if (g_targets[k].alive && g_targets[k].kind == 1) beacon(v3(g_targets[k].pos.x, 0, g_targets[k].pos.z), 0.6f, g_targets[k].pos.y, 1.0f, 0.3f, 0.3f, 0.25f);
        break;
    case MT_BOSS:
        if (target >= 0 && g_cars[target].used) marker_text("V", vadd(g_cars[target].pos, v3(0, 5.5f + sinf(tt * 5) * 0.2f, 0)), 0.25f, 1, 0.2f, 0.2f);
        break;
    case MT_COLLECT:
        for (int k = 0; k < npts; k++) if (!got[k]) beacon(v3(pts[k].x, 0, pts[k].z), 0.8f, 25, 0.2f, 1.0f, 0.4f, 0.35f);
        break;
    case MT_SMASH:
        for (int k = 0; k < ncr; k++) {
            Crate *c = &g_crates[crateSlot[k]];
            if (c->alive) beacon(c->pos, 1.0f, 25, 1.0f, 0.5f, 0.1f, 0.4f);
        }
        break;
    case MT_CHASE:
        if (target >= 0 && g_cars[target].used) {
            V3 p = vadd(g_cars[target].pos, v3(0, 3.5f + sinf(tt * 5) * 0.2f, 0));
            marker_text("V", p, 0.18f, 1, 0.2f, 0.2f);
        }
        break;
    }
}

static void bar(float x, float y, float w, float h, float f, float r, float g, float b) {
    ui_rect(x - 2, y - 2, w + 4, h + 4, 0, 0, 0, 0.6f);
    ui_rect(x, y, w * clampf(f, 0, 1), h, r, g, b, 0.95f);
}
void mission_hud(float s) {
    if (cur < 0) return;
    const MDef *d = &defs[cur];
    float cx = g_winW * 0.5f;
    char buf[160];
    float pw = 560 * s;
    ui_rect(cx - pw / 2, 10 * s, pw, 88 * s, 0, 0, 0, 0.45f);
    ui_text_c(cx, 18 * s, 2.2f * s, d->name, 1, 0.85f, 0.2f, 1);
    if (step == 0 || !d->stepN[0]) snprintf(buf, sizeof buf, "%s", d->step0);
    else {
        int shown = step, tot = npts;
        if (cur == 1) { tot = npts - 1; }
        if (d->type == MT_COLLECT || d->type == MT_SMASH || d->type == MT_SHOOT || d->type == MT_KILL) tot = total;
        if (d->type == MT_FLY || d->type == MT_LAND) shown = step + 1;
        if (d->type == MT_CHECK && (cur == 0 || cur == 14)) shown = step + 1;
        if (d->type == MT_RACE) shown = step + 1;
        snprintf(buf, sizeof buf, d->stepN, shown > tot ? tot : shown, tot);
    }
    if (d->type == MT_RACE && step == 0) snprintf(buf, sizeof buf, "WIN THE RACE - CHECKPOINT 1 OF %d", npts);
    if ((cur == 0 || cur == 14) && step == 0) snprintf(buf, sizeof buf, "CHECKPOINT 1 OF %d", npts);
    ui_text_c(cx, 44 * s, 1.9f * s, buf, 1, 1, 1, 1);
    int secs = (int)ceilf(timer);
    snprintf(buf, sizeof buf, "%d:%02d", secs / 60, secs % 60);
    float tr = timer < 10 ? 1.0f : 1, tg = timer < 10 ? 0.3f : 1, tb = timer < 10 ? 0.3f : 1;
    ui_text_c(cx, 66 * s, 2.6f * s, buf, tr, tg, tb, 1);
    if (d->type == MT_BOSS && target >= 0 && g_cars[target].used) {
        Car *t = &g_cars[target];
        ui_text_c(cx, 108 * s, 1.6f * s, "VIPER GUNSHIP", 1, 0.5f, 0.9f, 1);
        bar(cx - 150 * s, 126 * s, 300 * s, 12 * s, t->health / t->maxHealth, 0.8f, 0.25f, 1);
    }
    if (d->type == MT_CHASE && target >= 0 && g_cars[target].used) {
        Car *t = &g_cars[target];
        ui_text_c(cx, 108 * s, 1.6f * s, "TARGET DAMAGE", 1, 0.5f, 0.5f, 1);
        bar(cx - 150 * s, 126 * s, 300 * s, 12 * s, 1.0f - t->health / t->maxHealth, 1, 0.25f, 0.2f);
        float dd = distxz(player_pos(), t->pos);
        snprintf(buf, sizeof buf, "DISTANCE %dM", (int)dd);
        ui_text_c(cx, 144 * s, 1.5f * s, buf, dd > 150 ? 1.0f : 0.8f, dd > 150 ? 0.4f : 0.8f, dd > 150 ? 0.3f : 0.8f, 1);
    }
    if (d->type == MT_RACE) {
        int pos = 1;
        V3 pp = player_pos();
        for (int r = 0; r < 3; r++) {
            if (racers[r] < 0 || !g_cars[racers[r]].used) continue;
            Car *c = &g_cars[racers[r]];
            if (c->raceIdx > step) pos++;
            else if (c->raceIdx == step && step < npts && distxz(c->pos, pts[step]) < distxz(pp, pts[step])) pos++;
        }
        snprintf(buf, sizeof buf, "POSITION %d / 4", pos);
        ui_text_c(cx, 108 * s, 2.2f * s, buf, pos == 1 ? 0.4f : 1, 1, pos == 1 ? 0.4f : 0.4f, 1);
    }
    if (phase == 0) {
        int n = (int)ceilf(cd);
        snprintf(buf, sizeof buf, "%d", n);
        ui_text_c(cx, g_winH * 0.35f, 12 * s, buf, 1, 0.9f, 0.2f, 1);
    } else if (timer > d->time - 1.0f) {
        ui_text_c(cx, g_winH * 0.35f, 12 * s, "GO!", 0.3f, 1, 0.3f, 1);
    }
}
