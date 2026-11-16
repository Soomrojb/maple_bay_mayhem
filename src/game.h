/* Maple Bay Mayhem - an original open-world driving & platforming game.
 * Shared declarations. */
#ifndef GAME_H
#define GAME_H
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <GL/gl.h>
#include <X11/keysym.h>

#define PI 3.14159265358979f
#define DEG (180.0f / PI)

/* ------------------------------------------------------------------ math */
typedef struct { float x, y, z; } V3;
static inline V3 v3(float x, float y, float z) { V3 r; r.x = x; r.y = y; r.z = z; return r; }
static inline V3 vadd(V3 a, V3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline V3 vsub(V3 a, V3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline V3 vmul(V3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static inline float vdot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline V3 vcross(V3 a, V3 b) { return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static inline float vlen(V3 a) { return sqrtf(vdot(a, a)); }
static inline V3 vnorm(V3 a) { float l = vlen(a); return l > 1e-6f ? vmul(a, 1.0f / l) : v3(0, 0, 0); }
static inline float dist2xz(V3 a, V3 b) { float dx = a.x - b.x, dz = a.z - b.z; return dx * dx + dz * dz; }
static inline float distxz(V3 a, V3 b) { return sqrtf(dist2xz(a, b)); }
static inline float dist3(V3 a, V3 b) { return vlen(vsub(a, b)); }
static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float frand(void) { return (float)rand() / (float)RAND_MAX; }
static inline float frange(float a, float b) { return a + (b - a) * frand(); }
static inline float signf(float v) { return v < 0 ? -1.0f : 1.0f; }
static inline float wrap_pi(float a) { while (a > PI) a -= 2 * PI; while (a < -PI) a += 2 * PI; return a; }
/* yaw convention: forward = (sin yaw, 0, cos yaw) */
static inline V3 fwd_of(float yaw) { return v3(sinf(yaw), 0, cosf(yaw)); }
static inline V3 right_of(float yaw) { return v3(-cosf(yaw), 0, sinf(yaw)); }

/* ------------------------------------------------------------ constants */
#define CELL 70.0f
#define MAXBLK 12
extern int g_nblk;          /* city blocks per side on the current island */
extern float g_cityHalf;
extern int g_curMap;        /* 0 Maple Bay, 1 Sunset Sands, 2 Frostpeak */
#define NUM_MAPS 3
#define NBLK g_nblk
#define ROAD_HALF 8.0f
#define CITY_HALF g_cityHalf
#define ISLAND (g_cityHalf + 50.0f)
#define WORLD_LIMIT (g_cityHalf + 42.0f)
#define BO 2                /* Maple Bay: offset of the original 8x8 town inside the 12x12 grid */
#define AZ(z) ((z) + g_cityHalf - 280.0f) /* airport layout is authored for a 280 m city half */
#define FAR_AWAY v3(99999, 0, 99999)
#define GRAV 28.0f
#define JUMP_V 10.5f
#define LANE 3.6f

#define MAX_BOXES 7000
#define MAX_RAMPS 96
#define MAX_CRATES 220
#define MISSION_CRATE_BASE 200
#define MAX_COINS 2800
#define MAX_DECOS 4500
#define MAX_PARKS 40
#define MAX_CARS 80
#define MAX_PEDS 40
#define MAX_PARTS 1000
#define NUM_GIZMOS 20 /* 0-9 Maple Bay, 10-14 Sunset Sands, 15-19 Frostpeak */
#define NUM_GAS 8
#define NUM_STANDS 7
#define AIR_Z0 (g_cityHalf + 42.0f)
#define AIR_Z1 (g_cityHalf + 282.0f)
#define AIR_X 312.0f
#define NUM_MISSIONS 16

/* ---------------------------------------------------------------- input */
#define MOUSE_BASE 0x7F000000
enum {
    ACT_FORWARD, ACT_BACK, ACT_LEFT, ACT_RIGHT, ACT_JUMP, ACT_SPRINT, ACT_INTERACT, ACT_KICK,
    ACT_FIRE, ACT_AIM, ACT_RELOAD, ACT_NEXTWEAP, ACT_PREVWEAP,
    ACT_HANDBRAKE, ACT_HORN, ACT_CALLCAR, ACT_RESETCAR, ACT_ASCEND, ACT_DESCEND, ACT_MISSILE,
    ACT_CAM_LEFT, ACT_CAM_RIGHT, ACT_CAM_UP, ACT_CAM_DOWN, ACT_MAP, ACT_PAUSE, ACT_COUNT
};
/* binding contexts are bit masks: an action is live on foot, in a car and/or in an aircraft */
#define CTX_FOOT 1
#define CTX_CAR 2
#define CTX_AIR 4
extern const char *g_actNames[ACT_COUNT];
extern const char *g_actIds[ACT_COUNT];
extern const int g_actCtx[ACT_COUNT]; /* CTX_* mask */

typedef struct {
    float sens;
    int invertY, volume, music, fov, fullscreen, showFps;
    int binds[ACT_COUNT][2];
} Settings;
extern Settings g_set;
void settings_default_binds(void);
void settings_defaults(void);

typedef struct {
    int coins;
    unsigned missions, cars;
    int curCar;
    unsigned outfits;
    int outfit;
    unsigned upgrades, gizmos;
    int earned;
    float playTime;
    /* aircraft, weapons, tuning, territory */
    unsigned aircraft;
    int curAir;
    unsigned weapons;
    int ammo[8];
    unsigned char wup[8][4];
    int carUp[4];  /* engine, turbo, armor, fuel tank */
    int airUp[6];  /* engine, power, armor, guns, missiles, fuel tank */
    int carPaint, airPaint;
    unsigned turf;
    int vitality, bodyArmor;
    int map;
} Progress;
extern Progress g_prog;
enum { UP_DOUBLEJUMP = 1, UP_MAGNET = 2, UP_ARMOR = 4, UP_NITRO = 8 };
#define NUM_UPGRADES 4

extern int g_winW, g_winH, g_quit;
extern float g_mouseDX, g_mouseDY, g_wheel;
extern int g_mouseX, g_mouseY, g_mouseClick, g_lastKey;
extern char g_baseDir[1024];

double plat_time(void);
void plat_set_fullscreen(int on);
void plat_grab_mouse(int on);
int key_down(int code);
int key_pressed(int code);
int act_down(int a);
int act_pressed(int a);
const char *key_name(int code);
const char *act_key(int a); /* label of primary binding, for prompts */
void config_save(void);
void config_load(void);
int save_game(void);
int load_game(void);
int save_exists(void);
void progress_reset(void);
void take_screenshot(const char *path);
void base_path(char *out, size_t n, const char *name);

/* ---------------------------------------------------------------- audio */
enum {
    SND_COIN, SND_JUMP, SND_CRASH, SND_BREAK, SND_SUCCESS, SND_FAIL, SND_CLICK, SND_HORN,
    SND_BUY, SND_CHECK, SND_KICK, SND_EXPLODE, SND_BEEP, SND_GO, SND_DOOR, SND_GIZMO, SND_DENY,
    SND_SHOT, SND_SHOTGUN, SND_RIFLE, SND_MG, SND_ROCKET, SND_HURT, SND_RELOAD, SND_EMPTY, SND_HEAL,
    SND_FUEL, SND_SIREN, SND_BUSTED, SND_COUNT
};
void audio_init(void);
void audio_shutdown(void);
void audio_play(int id, float vol, float pitch);
void audio_engine(float rpm01, float vol);
void audio_engine_kind(int kind); /* 0 car 1 propeller/jet 2 rotor */

/* ------------------------------------------------------------- entities */
typedef struct {
    float shirt[3], pants[3], skin[3], hat[3], shoes[3];
    int hat_style; /* 0 cap 1 straw 2 fedora 3 helmet 4 none 5 chef 6 police 7 crown */
} Look;

enum { CS_SEDAN, CS_TAXI, CS_TRUCK, CS_VAN, CS_SPORT, CS_HOTROD, CS_ROCKET, CS_ARMOR, CS_ICECREAM, CS_POLICE,
       CS_PLANE, CS_BIPLANE, CS_JET, CS_HELI, CS_GUNSHIP };
enum { VK_CAR, VK_PLANE, VK_HELI };
typedef struct {
    const char *name;
    float maxSpeed, accel, grip, steer, health;
    int price; /* -1 = not for sale */
    int style;
    float col[3];
    float len, wid, hgt;
    int kind;      /* VK_* */
    float climb;   /* aircraft: climb rate */
    int guns, missiles; /* built-in weapon levels for aircraft */
} CarType;
#define NUM_SHOP_CARS 7
#define CAR_GETAWAY 7
#define CAR_BOSS 8
#define CAR_RACER 9
#define CAR_POLICE 10
#define AIR_FIRST 11
#define NUM_AIR_SHOP 5
#define AIR_BOSS 16
#define NUM_CAR_TYPES 17
extern CarType g_carTypes[NUM_CAR_TYPES];

enum { AI_NONE, AI_PARKED, AI_TRAFFIC, AI_FLEE, AI_RACE, AI_POLICE, AI_BOSS };
typedef struct Car {
    int used, type;
    V3 pos;
    float yaw;
    V3 vel;
    float vy;
    int onGround;
    float health, maxHealth;
    int wrecked;
    float wreckT;
    int ai;
    int node[2];
    int dir, ndir;
    V3 wp;
    float stuck, stun, topSpeed;
    int raceIdx;
    float raceOff;
    float wheelRot, pitch, roll, steerVis;
    float col[3];
    int mission, called;
    float smokeT, lastHit;
    /* fuel, flight and weapons */
    float fuel, throttle, rotor, aspd, gunCD, misCD, aiT, aiT2, siren;
    int stand, locked, hitByPlayer, crew;
} Car;

typedef struct {
    V3 pos, vel;
    float yaw;
    int onGround, jumps, car;
    float anim, kick, knock, callCD, nitro, idle;
    /* combat */
    float health, armor, hurtT, flash, deadT, reloadT, fireCD, chute, recoil, refuelAcc;
    int dead, busted, weapon, clip[8], aiming;
} Player;

typedef struct {
    int active, bi, bj, dir, look, down;
    float t, speed;
    V3 pos, vel;
    float yaw, anim, knock, spin, respawn;
} Ped;

extern Player g_pl;
extern Car g_cars[MAX_CARS];
extern Ped g_peds[MAX_PEDS];
extern V3 g_camPos, g_camLook, g_camRight, g_camUp;
extern float g_camYaw, g_camPitch, g_gameTime;
extern char g_prompt[160];
#define NUM_OUTFITS 8
extern Look g_looks[NUM_OUTFITS];
extern const char *g_lookNames[NUM_OUTFITS];
extern const int g_lookPrice[NUM_OUTFITS];
extern const char *g_upNames[NUM_UPGRADES];
extern const char *g_upDesc[NUM_UPGRADES];
extern const int g_upPrice[NUM_UPGRADES];
extern Look g_npcLooks[12];

void sim_init(void);
void sim_new_game(int fromSave);
void sim_update(float dt);
void sim_ambient(float dt);
void sim_render(void);
int spawn_car(int type, V3 pos, float yaw, int ai);
void remove_car(int i);
V3 player_pos(void);
Car *player_car(void);
void put_player_in_car(int ci);
void add_coins(int n);
void spawn_part(V3 p, V3 v, float life, float size, float r, float g, float b, float grav, float grow);
void break_crate(int i);
int add_dyn_coin(V3 p, V3 v);
int car_spot_free(V3 p, float r);
int spawn_player_car_near(V3 p);
void sim_call_car(void);
void sim_clear_mission_cars(void);
void car_damage(Car *c, float d);
float car_speed(const Car *c);
void fx_burst(V3 p, int n, float spd, float life, float size, float r, float g, float b, float grav);
int nearest_vehicle(V3 p, float maxd);
float player_max_health(void);
void player_respawn(V3 p);
void exit_vehicle(void);
int spawn_ai_car(int type, int k, int l, int dir, int ai, float speed);
V3 cam_forward(void);
int is_land(float x, float z);
extern int g_godMode;
extern float g_fovMul;
void sim_respawn_stands(void);
void travel_to(int map, int byPlane);
V3 nearest_hospital(V3 p);
extern int g_wpOn;
extern V3 g_wpPos;

/* ---------------------------------------------------------------- world */
enum { K_SOLID, K_BUILDING, K_INVIS, K_PLATFORM, K_CONTAINER, K_TRUNK, K_STEP };
typedef struct { V3 mn, mx; float col[3]; int kind, roof; } Box;
typedef struct { V3 mn, mx; int dir; float h; float col[3]; } Ramp; /* dir 0:+x 1:-x 2:+z 3:-z rising */
typedef struct { V3 pos; int alive, mission, canRespawn; float respawn, vy; } Crate;
typedef struct { V3 pos, vel; int alive, dyn, value; float respawn, life; } Coin;
enum { DECO_TREE, DECO_PINE, DECO_LAMP, DECO_BENCH, DECO_FOUNTAIN, DECO_CLOCK, DECO_DOME, DECO_SIGN, DECO_POND, DECO_AWNING, DECO_ANTENNA,
       DECO_PALM, DECO_CACTUS, DECO_SNOWPINE, DECO_CROSS };
typedef struct { V3 p; int kind; float s, yaw; float col[3]; const char *text; } Deco;
enum { BT_RES, BT_COM, BT_DOWN, BT_PARK, BT_TOWER, BT_STUNT, BT_PLAZA, BT_HOME, BT_DOCK, BT_PIZZA, BT_LAB,
       BT_ARMORY, BT_CLINIC, BT_TUNER, BT_GAS, BT_HOSPITAL, BT_MALL, BT_STADIUM, BT_FIRE, BT_BUILD,
       BT_CANYON, BT_CASINO, BT_RESORT, BT_ADOBE, BT_OASIS, BT_GUNX, BT_SKI, BT_LAKE, BT_LODGE, BT_CABIN };

#define CRATE_H 0.6f
extern Box g_boxes[MAX_BOXES];
extern int g_nboxes;
extern Ramp g_ramps[MAX_RAMPS];
extern int g_nramps;
extern Crate g_crates[MAX_CRATES];
extern int g_ncrates;
extern Coin g_coins[MAX_COINS];
extern int g_ncoins;
extern Deco g_decos[MAX_DECOS];
extern int g_ndecos;
extern int g_blockType[MAXBLK][MAXBLK];
extern V3 g_parkSpots[MAX_PARKS];
extern float g_parkYaw[MAX_PARKS];
extern int g_nparks;
extern V3 g_gizmoPos[NUM_GIZMOS];

typedef struct {
    V3 home; float homeYaw;
    V3 homeCar; float homeCarYaw;
    V3 shop; float shopYaw;
    V3 giver[NUM_MISSIONS]; float giverYaw[NUM_MISSIONS];
    V3 towerTop;
    V3 armory, hangar, tuner, clinic, police; float armoryYaw, hangarYaw, tunerYaw, clinicYaw;
    V3 gunx; float gunxYaw;
    V3 travel;              /* airline desk at the terminal */
    V3 hosp[4]; int nhosp;  /* every place that heals you; hosp[0] is the clinic */
    V3 gas[NUM_GAS]; int ngas;
    V3 stand[NUM_STANDS]; float standYaw[NUM_STANDS]; int standType[NUM_STANDS];
    V3 runway; float runwayYaw;
    V3 range[12];
} WorldSpots;
extern WorldSpots g_spots;

float road_c(int k);
void block_rect(int i, int j, float *x0, float *z0, float *x1, float *z1);
void block_edge_spot(int i, int j, int side, float off, V3 *p, float *yaw);
void world_build(int map);
const char *map_name(int m);
extern float g_skyTop[3], g_skyHz[3];
int road_route(V3 a, V3 b, V3 *out, int max); /* GPS: road path from a to b */
void world_draw_ground(void);
void world_draw_static(void);
void world_draw_dynamic(float t, V3 cam);
void world_draw_map(void);
float ground_height(float x, float z, float y, float step);
float ground_height_excl(float x, float z, float y, float step, int crateExcl);
float ceiling_height(float x, float z, float yHead);
int collide_circle(V3 *p, float r, float step, float h, V3 *n);
float ray_clear(V3 a, V3 b);
float ray_world(V3 a, V3 b);
extern int g_collSkipInvis;
float ramp_h(const Ramp *r, float x, float z);
void road_spot_near(V3 p, V3 *out, float *yaw);
void world_reset_pickups(void);
int add_crate_at(int slot, V3 p, int mission);

/* ----------------------------------------------------------------- gfx */
void gl_box(float x0, float y0, float z0, float x1, float y1, float z1);
void gl_cube(float cx, float cy, float cz, float hx, float hy, float hz);
void gl_frustum_y(float cx, float y0, float cz, float r0, float r1, float h, int n);
void gl_cyl_x(float cx, float cy, float cz, float r, float hw, int n);
void gl_disc_y(float cx, float y, float cz, float r, int n);
void text3d(const char *s, V3 o, V3 right, V3 up, float px);
float text3d_width(const char *s, float px);
void set_perspective(float fovDeg, float aspect, float n, float f);
void look_at(V3 eye, V3 at);
void ui_begin(void);
void ui_end(void);
void ui_rect(float x, float y, float w, float h, float r, float g, float b, float a);
void ui_frame_rect(float x, float y, float w, float h, float t, float r, float g, float b, float a);
float ui_text_w(const char *s, float sc);
void ui_text(float x, float y, float sc, const char *s, float r, float g, float b, float a);
void ui_text_c(float cx, float y, float sc, const char *s, float r, float g, float b, float a);
void ui_text_r(float rx, float y, float sc, const char *s, float r, float g, float b, float a);
float ui_text_wrap(float x, float y, float maxw, float sc, const char *s, float r, float g, float b, float a);
void ui_circle(float cx, float cy, float rad, float r, float g, float b, float a);
void save_png(const char *path, int w, int h, const unsigned char *rgb);
void draw_character(V3 p, float yaw, float anim, float move, const Look *L, float kick, int air);
void draw_car(const Car *c);
void draw_aircraft_body(int type, const float *col, float rotor, int wrecked, float gear);
void draw_character_ex(V3 p, float yaw, float anim, float move, const Look *L, float kick, int air, int weapon, float aimPitch);
void draw_weapon_model(int w);
void draw_car_body(int type, const float *col, float wheelRot, float steer, int wrecked);
void blob_shadow(float x, float y, float z, float rx, float rz, float yaw, float a);
void setup_light(void);

/* ------------------------------------------------------------- missions */
void mission_init(void);
void mission_update(float dt);
void mission_draw3d(float t);
void mission_hud(float s);
int mission_active(void);
int mission_frozen(void);
int mission_near_giver(V3 p);
void mission_start(int idx);
void mission_abort(void);
int mission_target(V3 *out);
const char *mission_name(int i);
const char *mission_giver(int i);
const char *mission_brief(int i);
int mission_reward(int i);
float mission_time(int i);
int mission_available(int i);
void mission_on_crate(int crate);
int mission_is_needcar(int i);
void mission_map_markers(void (*cb)(V3 p, int kind));
void mission_fail_reason(const char *why);
int mission_cur(void);
int mission_kill_gang(void); /* gang the current mission wants killed, or -1 */
void mission_on_kill(int gang);

/* ------------------------------------------------------------------ ui */
void ui_init(const char *autoshot);
void ui_frame(float dt);
void hud_msg(float secs, const char *fmt, ...);
void hud_toast(const char *fmt, ...);
void ui_open_dialog(int mission);
void ui_open_shop(int which);
void ui_open_travel(int byPlane);
extern int g_autoshotFrames;

/* --------------------------------------------------------------- combat */
enum { W_FISTS, W_PISTOL, W_SMG, W_SHOTGUN, W_RIFLE, W_ROCKET, W_ASSAULT, W_GRENADE, NUM_WEAPONS };
typedef struct {
    const char *name, *desc;
    float dmg, range, rate, spread, zoom, reload;
    int mag, pellets, price, ammoPrice, ammoPack, autoFire;
} WeaponDef;
extern const WeaponDef g_weap[NUM_WEAPONS];
enum { WU_POWER, WU_RANGE, WU_OPTICS, WU_MAG };
float weap_dmg(int w);
float weap_range(int w);
float weap_spread(int w);
float weap_zoom(int w);
int weap_mag(int w);
float weap_rate(int w);
float weap_reload(int w);
void give_weapon(int w, int ammo);

enum { GANG_RATS, GANG_VIPERS, GANG_POLICE };
#define NUM_TURF 8
typedef struct { int bi, bj, gang; } Turf;
extern const Turf g_turfs[NUM_TURF];
extern const char *g_gangNames[3];
extern float g_gangHeat[2];
extern int g_turfProg[NUM_TURF];
#define TURF_KILLS 8
int turf_at(V3 p);

#define MAX_TARGETS 24
typedef struct { V3 pos, base; int alive, kind; float hp, t; } Target;
extern Target g_targets[MAX_TARGETS];
void targets_clear(void);
int target_add(V3 p, int kind, float hp);
int targets_alive(void);

extern float g_wanted;      /* 0..5 stars (fractional heat) */
int wanted_stars(void);
void wanted_crime(float amount, V3 where);
void wanted_clear(void);

void combat_reset(void);
void combat_update(float dt);
void combat_render(void);
void combat_render_fx(void);
void combat_map_markers(void (*cb)(V3 p, int kind));
void turf_draw_map(void);
void player_damage(float d, V3 from);
void explode(V3 p, float radius, float dmg, int byPlayer);
int fire_hitscan(V3 o, V3 dir, float range, float dmg, int byPlayer, int ignoreCar, V3 *hitp);
void spawn_tracer(V3 a, V3 b, float r, float g, float bl);
void spawn_rocket(V3 p, V3 v, float dmg, float radius, int byPlayer, int homing);
void spawn_grenade(V3 p, V3 v, float dmg, float radius);
void gunfire_noise(V3 p, int byPlayer);
int thugs_near(V3 p, float r, int gang);
V3 thug_nearest(V3 p, int gang, int *found);
void police_on_car_destroyed(Car *c);
int police_incident(V3 *p);
int spawn_thug(int gang, V3 p, int turf);

/* ---------------------------------------------------------------- air */
void air_control(Car *c, float dt, int frozen);
void air_physics(Car *c, float dt, float thr, float turn, float lift, int pilot);
void air_ai(Car *c, float dt, int frozen);
V3 air_fwd3(const Car *c);
int is_aircraft(const Car *c);
float vehicle_fuel_use(const Car *c);
int spawn_boss_heli(V3 p);

/* ---------------------------------------------------------------- shops */
enum { SHOP_BOLT, SHOP_ARMORY, SHOP_HANGAR, SHOP_TUNER, SHOP_GUNX, NUM_SHOPS };
int weapon_sell_value(int w);
typedef struct {
    char name[48], info[48];
    const char *desc;
    int price;     /* -1 = not purchasable */
    int status;    /* 0 buy, 1 owned, 2 equipped, 3 maxed, 4 locked */
    int preview;   /* 0 icon, 1 car, 2 outfit, 3 aircraft, 4 weapon, 5 paint */
    int pidx;
    float pcol[3];
    int nstats;
    const char *statName[4];
    float stat[4];
    const char *icon;
} ShopItem;
const char *shop_title(int shop);
int shop_tabs(int shop, const char **names);
int shop_items(int shop, int tab, ShopItem *out, int max);
void shop_buy(int shop, int tab, int idx);
extern const float g_paints[9][3];
extern const char *g_paintNames[9];
float car_up_mul(int which);

#endif
