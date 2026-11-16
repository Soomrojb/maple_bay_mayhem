/* Aircraft: arcade flight model for planes and helicopters, pilotless crashes, enemy gunship AI. */
#include "game.h"

int is_aircraft(const Car *c) { return g_carTypes[c->type].kind != VK_CAR; }
V3 air_fwd3(const Car *c) {
    float cp = cosf(c->pitch);
    return v3(sinf(c->yaw) * cp, sinf(c->pitch), cosf(c->yaw) * cp);
}
static int is_player(const Car *c) { return g_pl.car >= 0 && &g_cars[g_pl.car] == c; }

float vehicle_fuel_use(const Car *c) {
    const CarType *t = &g_carTypes[c->type];
    float tank = 1.0f + 0.4f * g_prog.airUp[5];
    if (t->kind == VK_HELI) return (0.0015f + 0.0028f * c->throttle) / tank;
    return (0.0008f + 0.0042f * c->throttle) / tank;
}

static void crash(Car *c, float dmg) {
    car_damage(c, dmg);
    fx_burst(vadd(c->pos, v3(0, 1, 0)), 14, 5, 0.6f, 0.25f, 1, 0.8f, 0.3f, 10);
    if (is_player(c)) audio_play(SND_CRASH, 0.9f, 0.8f);
}
static void splash(Car *c) {
    fx_burst(c->pos, 40, 6, 1.2f, 0.5f, 0.75f, 0.85f, 1.0f, 12);
    if (!c->wrecked) car_damage(c, 99999);
}

/* collide the fuselage with buildings; returns impact speed */
static float air_collide(Car *c, float sp) {
    const CarType *t = &g_carTypes[c->type];
    V3 p = c->pos, before = p, n;
    g_collSkipInvis = !c->onGround;
    int hit = collide_circle(&p, t->kind == VK_HELI ? 2.2f : 1.6f, c->onGround ? 0.6f : 0.15f, t->hgt, &n);
    g_collSkipInvis = 0;
    if (!hit) return 0;
    c->pos.x = p.x;
    c->pos.z = p.z;
    (void)before;
    float vn = c->vel.x * n.x + c->vel.z * n.z;
    if (vn < 0) {
        c->vel.x -= n.x * vn * 1.4f;
        c->vel.z -= n.z * vn * 1.4f;
        return -vn;
    }
    return 0;
}

static void bounds(Car *c) {
    float lim = 900;
    if (fabsf(c->pos.x) > lim || fabsf(c->pos.z) > lim) {
        float want = atan2f(-c->pos.x, -c->pos.z);
        c->yaw = wrap_pi(c->yaw + clampf(wrap_pi(want - c->yaw), -1, 1) * 0.02f);
        c->pos.x = clampf(c->pos.x, -lim - 20, lim + 20);
        c->pos.z = clampf(c->pos.z, -lim - 20, lim + 20);
        if (is_player(c) && fmodf(g_gameTime, 4) < 0.02f) hud_toast("TURN BACK - YOU ARE LEAVING MAPLE BAY");
    }
    if (c->pos.y > 260) { c->pos.y = 260; if (c->vy > 0) c->vy = 0; if (c->pitch > 0) c->pitch *= 0.9f; }
}

static void plane_physics(Car *c, float dt, float thrIn, float turn, float lift, int pilot) {
    const CarType *t = &g_carTypes[c->type];
    int pl = is_player(c);
    float maxS = t->maxSpeed * (pl ? 1.0f + 0.1f * g_prog.airUp[0] : 1.0f);
    float acc = t->accel * (pl ? 1.0f + 0.15f * g_prog.airUp[1] : 1.0f);
    int engine = c->fuel > 0 && !c->wrecked && pilot;
    if (pilot && !c->wrecked) c->throttle = clampf(c->throttle + thrIn * 0.7f * dt, 0, 1);
    else c->throttle = fmaxf(0, c->throttle - dt * 0.5f);
    float thrust = engine ? acc * c->throttle : 0;
    float sp = c->aspd;
    float drag = acc * (sp / maxS) * (sp / maxS);
    float rotSpeed = maxS * 0.42f;
    c->rotor += (engine ? 0.25f + c->throttle : sp * 0.02f) * 45 * dt;
    float oldY = c->pos.y;
    if (c->onGround) {
        float fr = 1.2f + (thrIn < 0 && c->throttle <= 0.01f ? 9.0f : 0) + (c->wrecked ? 6 : 0);
        sp += (thrust - drag) * dt;
        sp = fmaxf(0, sp - fr * dt);
        c->yaw = wrap_pi(c->yaw - turn * 0.9f * clampf(sp / 6, 0, 1) * clampf(1.4f - sp / maxS, 0.3f, 1) * dt);
        if (lift > 0 && sp > rotSpeed) c->pitch += 0.5f * dt;
        else c->pitch = lerpf(c->pitch, 0, clampf(dt * 4, 0, 1));
        c->roll = lerpf(c->roll, 0, clampf(dt * 4, 0, 1));
        V3 f = fwd_of(c->yaw);
        c->vel = vmul(f, sp);
        c->pos.x += c->vel.x * dt;
        c->pos.z += c->vel.z * dt;
        c->vy = 0;
        float g = ground_height(c->pos.x, c->pos.z, c->pos.y, 0.9f);
        if (g < c->pos.y - 0.4f && sp > rotSpeed * 0.8f) c->onGround = 0; /* rolled off a roof edge */
        else c->pos.y = g;
        if (c->pitch > 0.06f && sp > rotSpeed) { c->onGround = 0; c->vy = 0.5f; }
    } else {
        float auth = clampf(sp / (maxS * 0.45f), 0.25f, 1);
        if (pilot && !c->wrecked) {
            c->pitch += lift * 1.1f * auth * dt;
            if (lift == 0) c->pitch *= 1 - 0.35f * dt;
            c->roll = lerpf(c->roll, turn * 0.85f, clampf(dt * 3, 0, 1));
            c->yaw = wrap_pi(c->yaw - turn * t->steer * clampf(sp / (maxS * 0.5f), 0.3f, 1) * dt);
        } else {
            c->roll = lerpf(c->roll, 0.3f, clampf(dt, 0, 1));
            c->pitch -= 0.25f * dt;
        }
        float stall = maxS * 0.36f;
        float sink = 0;
        if (sp < stall) {
            float k = 1 - sp / stall;
            c->pitch -= k * 1.4f * dt;
            sink = k * 9;
        }
        c->pitch = clampf(c->pitch, -1.2f, 1.0f);
        sp += (thrust - drag - 9.8f * sinf(c->pitch)) * dt;
        if (sp < 0) sp = 0;
        V3 f3 = air_fwd3(c);
        c->vel = v3(f3.x * sp, 0, f3.z * sp);
        c->vy = f3.y * sp - sink;
        c->pos.x += c->vel.x * dt;
        c->pos.z += c->vel.z * dt;
        c->pos.y += c->vy * dt;
        float g = ground_height(c->pos.x, c->pos.z, oldY, 0.5f);
        if (c->pos.y <= g) {
            c->pos.y = g;
            if (!is_land(c->pos.x, c->pos.z)) splash(c);
            else if (c->pitch > -0.3f && c->vy > -9 && fabsf(c->roll) < 0.6f && !c->wrecked) {
                if (c->vy < -5) crash(c, (-c->vy - 5) * 6);
                else if (pl) audio_play(SND_DOOR, 0.6f, 0.6f);
            } else {
                crash(c, 45 + fabsf(c->vy) * 4 + sp * 1.5f);
                sp *= 0.3f;
            }
            c->onGround = 1;
            c->vy = 0;
            c->pitch = 0;
            c->roll = 0;
        }
    }
    float imp = air_collide(c, sp);
    if (imp > 6) {
        crash(c, imp * 1.3f + (c->onGround ? 0 : 15));
        sp *= 0.4f;
    }
    c->aspd = sp;
    bounds(c);
}

static void heli_physics(Car *c, float dt, float thrIn, float turn, float lift, int pilot) {
    const CarType *t = &g_carTypes[c->type];
    int pl = is_player(c);
    float maxS = t->maxSpeed * (pl ? 1.0f + 0.1f * g_prog.airUp[0] : 1.0f);
    float acc = t->accel * (pl ? 1.0f + 0.15f * g_prog.airUp[1] : 1.0f);
    float climb = t->climb * (pl ? 1.0f + 0.15f * g_prog.airUp[1] : 1.0f);
    int engine = c->fuel > 0 && !c->wrecked && pilot;
    c->throttle = clampf(c->throttle + (engine ? 0.7f : -0.25f) * dt, 0, 1);
    c->rotor += c->throttle * 40 * dt;
    float oldY = c->pos.y;
    float tvy;
    if (engine && c->throttle > 0.85f) tvy = lift * climb;
    else if (engine) tvy = fminf(lift * climb, 0) - 0.5f;
    else tvy = c->wrecked ? -25 : -7;
    V3 f = fwd_of(c->yaw), r = right_of(c->yaw);
    float vf = vdot(c->vel, f), vl = vdot(c->vel, r);
    if (c->onGround) {
        vf *= expf(-4 * dt);
        vl *= expf(-4 * dt);
        if (tvy > 0.5f) { c->onGround = 0; c->vy = 1; }
        else c->vy = 0;
        if (engine && c->throttle > 0.5f) c->yaw = wrap_pi(c->yaw - turn * t->steer * 0.6f * dt);
    } else {
        float tf = engine ? (thrIn > 0 ? thrIn * maxS : thrIn * maxS * 0.4f) : vf * 0.98f;
        vf += clampf(tf - vf, -acc * dt, acc * dt);
        vl *= expf(-1.5f * dt);
        c->vy += (tvy - c->vy) * clampf(3.5f * dt, 0, 1);
        if (engine) c->yaw = wrap_pi(c->yaw - turn * t->steer * dt);
        else if (c->wrecked) c->yaw = wrap_pi(c->yaw + 4 * dt);
    }
    V3 f2 = fwd_of(c->yaw), r2 = right_of(c->yaw);
    c->vel = vadd(vmul(f2, vf), vmul(r2, vl));
    c->pos.x += c->vel.x * dt;
    c->pos.z += c->vel.z * dt;
    c->pos.y += c->vy * dt;
    float g = ground_height(c->pos.x, c->pos.z, oldY, 0.5f);
    if (c->pos.y <= g) {
        float land = -c->vy;
        c->pos.y = g;
        if (!c->onGround) {
            if (!is_land(c->pos.x, c->pos.z)) splash(c);
            else if (land > 12) crash(c, (land - 12) * 8);
            else if (land > 3 && pl) audio_play(SND_DOOR, 0.5f, 0.7f);
        }
        c->onGround = 1;
        c->vy = 0;
    } else if (c->onGround && c->pos.y > g + 0.3f) c->onGround = 0;
    float imp = air_collide(c, vf);
    if (imp > 7) crash(c, imp * 2.0f);
    c->pitch = lerpf(c->pitch, c->onGround ? 0 : -vf / maxS * 0.28f, clampf(dt * 3, 0, 1));
    c->roll = lerpf(c->roll, c->onGround ? 0 : turn * 0.18f + vl * 0.02f, clampf(dt * 3, 0, 1));
    c->aspd = fabsf(vf);
    bounds(c);
}

void air_physics(Car *c, float dt, float thr, float turn, float lift, int pilot) {
    if (g_carTypes[c->type].kind == VK_HELI) heli_physics(c, dt, thr, turn, lift, pilot);
    else plane_physics(c, dt, thr, turn, lift, pilot);
}

void air_control(Car *c, float dt, int frozen) {
    float thr = (act_down(ACT_FORWARD) ? 1.0f : 0.0f) - (act_down(ACT_BACK) ? 1.0f : 0.0f);
    float turn = (act_down(ACT_RIGHT) ? 1.0f : 0.0f) - (act_down(ACT_LEFT) ? 1.0f : 0.0f);
    float lift = (act_down(ACT_ASCEND) ? 1.0f : 0.0f) - (act_down(ACT_DESCEND) ? 1.0f : 0.0f);
    if (frozen) { thr = turn = 0; lift = c->onGround ? 0 : lift; if (c->onGround) { c->aspd *= 0.9f; c->vel = vmul(c->vel, 0.9f); } }
    air_physics(c, dt, thr, turn, lift, 1);
}

/* ------------------------------------------------------- enemy gunship */
static void boss_ai(Car *c, float dt, int frozen) {
    if (c->wrecked) { air_physics(c, dt, 0, 0, 0, 0); return; }
    c->throttle = 1;
    c->rotor += 40 * dt;
    V3 pp = player_pos();
    c->aiT += dt;
    float a = c->aiT * 0.22f;
    float rad = 42 + 10 * sinf(c->aiT * 0.37f);
    float roof = ground_height(c->pos.x, c->pos.z, 1e4f, 0);
    V3 want = v3(pp.x + cosf(a) * rad, fmaxf(pp.y + 26, roof + 14), pp.z + sinf(a) * rad);
    if (frozen) want = c->pos;
    V3 d = vsub(want, c->pos);
    float dl = vlen(d);
    V3 tv = dl > 0.1f ? vmul(d, fminf(22, dl * 0.8f) / dl) : v3(0, 0, 0);
    c->vel.x += clampf(tv.x - c->vel.x, -14 * dt, 14 * dt);
    c->vel.z += clampf(tv.z - c->vel.z, -14 * dt, 14 * dt);
    c->vy += clampf(tv.y - c->vy, -10 * dt, 10 * dt);
    c->pos = vadd(c->pos, v3(c->vel.x * dt, c->vy * dt, c->vel.z * dt));
    float face = atan2f(pp.x - c->pos.x, pp.z - c->pos.z);
    c->yaw = wrap_pi(c->yaw + clampf(wrap_pi(face - c->yaw), -1.5f * dt, 1.5f * dt));
    c->pitch = lerpf(c->pitch, -0.15f, clampf(dt, 0, 1));
    c->roll = lerpf(c->roll, 0, clampf(dt, 0, 1));
    c->onGround = 0;
    air_collide(c, 0);
    if (frozen || g_pl.dead) return;
    /* weapons: machine-gun bursts and the odd rocket */
    V3 gun = vadd(c->pos, vadd(vmul(fwd_of(c->yaw), 3.2f), v3(0, 0.6f, 0)));
    V3 tgt = vadd(pp, v3(0, g_pl.car >= 0 ? 1.0f : 1.2f, 0));
    float dist = dist3(gun, tgt);
    int los = ray_world(gun, tgt) > 0.97f;
    c->gunCD -= dt;
    c->misCD -= dt;
    if (c->gunCD <= 0 && los && dist < 95) {
        c->crew++;
        c->gunCD = (c->crew % 6) ? 0.13f : 2.4f;
        float miss = frand() < 0.32f ? 0 : 1;
        V3 hit = vadd(tgt, v3(frange(-3, 3) * miss, frange(-1.5f, 1.5f) * miss, frange(-3, 3) * miss));
        spawn_tracer(gun, hit, 1, 0.5f, 0.9f);
        audio_play(SND_MG, clampf(1 - dist / 120, 0.1f, 0.6f), 0.8f);
        if (!miss) {
            Car *pc = player_car();
            if (pc) car_damage(pc, 5);
            else player_damage(5, c->pos);
        }
    }
    if (c->misCD <= 0 && los && dist < 120) {
        c->misCD = 7.5f;
        spawn_rocket(gun, vmul(vnorm(vsub(tgt, gun)), 40), 30, 5, 0, 0);
        audio_play(SND_ROCKET, 0.5f, 0.9f);
    }
}

void air_ai(Car *c, float dt, int frozen) {
    if (c->ai == AI_BOSS) { boss_ai(c, dt, frozen); return; }
    if (c->onGround && c->aspd < 0.05f && car_speed(c) < 0.05f && c->vy == 0 && !c->wrecked) {
        float g = ground_height(c->pos.x, c->pos.z, c->pos.y, 0.5f);
        if (g < c->pos.y - 0.05f) c->onGround = 0; /* support vanished */
        return;
    }
    air_physics(c, dt, 0, 0, 0, 0);
}

int spawn_boss_heli(V3 p) {
    int i = spawn_car(AIR_BOSS, p, 0, AI_BOSS);
    if (i < 0) return -1;
    Car *c = &g_cars[i];
    c->onGround = 0;
    c->throttle = 1;
    c->mission = 1;
    return i;
}
