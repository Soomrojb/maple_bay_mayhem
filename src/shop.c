/* Shop catalogues and purchases: Bolt & Bargain, Boom Bros Armory, Skyline Hangar, Turbo Tuners. */
#include "game.h"

const float g_paints[9][3] = {{1, 1, 1}, {0.85f, 0.1f, 0.12f}, {0.12f, 0.35f, 0.85f}, {0.45f, 0.85f, 0.2f}, {0.98f, 0.55f, 0.1f},
                              {0.5f, 0.2f, 0.75f}, {0.12f, 0.12f, 0.14f}, {0.95f, 0.95f, 0.92f}, {0.95f, 0.78f, 0.2f}};
const char *g_paintNames[9] = {"FACTORY COLOURS", "CHERRY RED", "OCEAN BLUE", "LIME GREEN", "SUNSET ORANGE", "ROYAL PURPLE", "MIDNIGHT BLACK", "PEARL WHITE", "SHOWROOM GOLD"};

static const char *T_BOLT[] = {"CARS", "OUTFITS", "UPGRADES"};
static const char *T_ARMORY[] = {"WEAPONS", "POWER", "RANGE", "OPTICS", "MAGAZINE", "GEAR"};
static const char *T_HANGAR[] = {"AIRCRAFT", "UPGRADES", "PAINT"};
static const char *T_TUNER[] = {"UPGRADES", "PAINT"};
static const char *T_GUNX[] = {"BUY", "SELL", "POWER", "RANGE", "OPTICS", "MAGAZINE"};

static const char *AIR_UP_NAMES[6] = {"JET ENGINE", "POWER & CLIMB", "ARMOR PLATING", "MACHINE GUNS", "MISSILE PODS", "LONG-RANGE TANK"};
static const char *AIR_UP_DESC[6] = {
    "+10% TOP SPEED PER LEVEL FOR EVERY AIRCRAFT YOU FLY.",
    "+15% ACCELERATION AND CLIMB RATE PER LEVEL.",
    "AIRCRAFT TAKE 30% LESS DAMAGE PER LEVEL.",
    "HARDER-HITTING, FASTER-FIRING NOSE GUNS. FIRE WITH THE FIRE KEY.",
    "HOMING MISSILES! EACH LEVEL HITS HARDER AND RELOADS FASTER. FIRE WITH THE MISSILE KEY.",
    "USE 40% LESS FUEL PER LEVEL."};
static const int AIR_UP_BASE[6] = {400, 400, 500, 600, 900, 300};
static const char *CAR_UP_NAMES[4] = {"STREET ENGINE", "TURBO CHARGER", "REINFORCED BODY", "BIG FUEL TANK"};
static const char *CAR_UP_DESC[4] = {
    "+8% TOP SPEED PER LEVEL FOR ANY CAR YOU DRIVE.",
    "+15% ACCELERATION PER LEVEL.",
    "CARS TAKE LESS DAMAGE FROM CRASHES, BULLETS AND EXPLOSIONS.",
    "USE 40% LESS FUEL PER LEVEL."};
static const int CAR_UP_BASE[4] = {300, 300, 400, 200};
static const char *WSTAT_NAMES[4] = {"POWER", "RANGE", "OPTICS", "MAGAZINE"};
static const char *WSTAT_DESC[4] = {
    "+30% DAMAGE PER LEVEL.",
    "+30% EFFECTIVE RANGE PER LEVEL.",
    "BETTER SCOPE ZOOM AND TIGHTER SPREAD WHEN AIMING.",
    "BIGGER MAGAZINE, FASTER FIRE RATE AND QUICKER RELOADS."};
static const int WSTAT_BASE[4] = {250, 150, 200, 200};

const char *shop_title(int shop) {
    static const char *t[NUM_SHOPS] = {"BOLT & BARGAIN", "BOOM BROS ARMORY", "SKYLINE HANGAR", "TURBO TUNERS", "LOCK & LOAD WEAPON EXCHANGE"};
    return t[shop];
}
int shop_tabs(int shop, const char **names) {
    const char **src = shop == SHOP_BOLT ? T_BOLT : shop == SHOP_ARMORY ? T_ARMORY : shop == SHOP_HANGAR ? T_HANGAR : shop == SHOP_GUNX ? T_GUNX : T_TUNER;
    int n = shop == SHOP_BOLT ? 3 : shop == SHOP_ARMORY ? 6 : shop == SHOP_HANGAR ? 3 : shop == SHOP_GUNX ? 6 : 2;
    for (int i = 0; i < n; i++) names[i] = src[i];
    return n;
}
static int wstat_price(int w, int k) { return (int)(WSTAT_BASE[k] * (1 + 0.5f * (w - 1)) * (g_prog.wup[w][k] + 1)); }
/* what the weapon exchange pays: half the gun, half the upgrades, half the ammo */
int weapon_sell_value(int w) {
    if (w <= 0 || w >= NUM_WEAPONS || !((g_prog.weapons >> w) & 1)) return 0;
    float v = g_weap[w].price * 0.5f;
    for (int k = 0; k < 4; k++) {
        int L = g_prog.wup[w][k];
        v += 0.5f * WSTAT_BASE[k] * (1 + 0.5f * (w - 1)) * L * (L + 1) / 2.0f;
    }
    if (g_weap[w].ammoPack > 0) v += 0.5f * g_prog.ammo[w] * g_weap[w].ammoPrice / (float)g_weap[w].ammoPack;
    return (int)v;
}
static void level_item(ShopItem *it, const char *name, int lvl, int max, int price, const char *desc) {
    snprintf(it->name, sizeof it->name, "%s", name);
    if (lvl >= max) { it->status = 3; snprintf(it->info, sizeof it->info, "LEVEL %d/%d - MAXED", lvl, max); }
    else { it->status = 0; it->price = price; snprintf(it->info, sizeof it->info, "LEVEL %d/%d  -  %d COINS", lvl, max, price); }
    it->desc = desc;
}
static void car_stats(ShopItem *it, int type) {
    const CarType *t = &g_carTypes[type];
    it->nstats = 4;
    if (t->kind == VK_CAR) {
        static const char *n[4] = {"TOP SPEED", "ACCELERATION", "HANDLING", "TOUGHNESS"};
        for (int i = 0; i < 4; i++) it->statName[i] = n[i];
        it->stat[0] = t->maxSpeed / 52; it->stat[1] = t->accel / 30; it->stat[2] = t->grip / 12; it->stat[3] = t->health / 180;
    } else {
        static const char *n[4] = {"TOP SPEED", "CLIMB", "TOUGHNESS", "WEAPONS"};
        for (int i = 0; i < 4; i++) it->statName[i] = n[i];
        it->stat[0] = t->maxSpeed / 85; it->stat[1] = t->climb / 18; it->stat[2] = t->health / 260; it->stat[3] = (t->guns + t->missiles) / 3.0f;
    }
}
static void weap_stats(ShopItem *it, int w) {
    static const char *n[4] = {"DAMAGE", "RANGE", "FIRE RATE", "MAGAZINE"};
    it->nstats = 4;
    for (int i = 0; i < 4; i++) it->statName[i] = n[i];
    it->stat[0] = weap_dmg(w) * g_weap[w].pellets / 180;
    it->stat[1] = weap_range(w) / 220;
    it->stat[2] = 0.085f / weap_rate(w);
    it->stat[3] = weap_mag(w) / 45.0f;
}

int shop_items(int shop, int tab, ShopItem *out, int max) {
    int n = 0;
#define NEW() (memset(&out[n], 0, sizeof out[n]), &out[n])
    if (shop == SHOP_BOLT) {
        if (tab == 0)
            for (int i = 0; i < NUM_SHOP_CARS && n < max; i++, n++) {
                ShopItem *it = NEW();
                snprintf(it->name, sizeof it->name, "%s", g_carTypes[i].name);
                it->price = g_carTypes[i].price;
                it->status = g_prog.curCar == i ? 2 : ((g_prog.cars >> i) & 1);
                if (it->status == 0) snprintf(it->info, sizeof it->info, "%d COINS", it->price);
                else snprintf(it->info, sizeof it->info, it->status == 2 ? "EQUIPPED" : "OWNED");
                it->preview = 1; it->pidx = i;
                memcpy(it->pcol, g_carTypes[i].col, sizeof it->pcol);
                it->desc = "EQUIPPED CAR IS DELIVERED WHEN YOU CALL A CAR OR START A DRIVING MISSION.";
                car_stats(it, i);
            }
        else if (tab == 1)
            for (int i = 0; i < NUM_OUTFITS && n < max; i++, n++) {
                ShopItem *it = NEW();
                snprintf(it->name, sizeof it->name, "%s", g_lookNames[i]);
                it->price = g_lookPrice[i];
                it->status = g_prog.outfit == i ? 2 : ((g_prog.outfits >> i) & 1);
                if (it->status == 0) snprintf(it->info, sizeof it->info, "%d COINS", it->price);
                else snprintf(it->info, sizeof it->info, it->status == 2 ? "EQUIPPED" : "OWNED");
                it->preview = 2; it->pidx = i;
                it->desc = "LOOK SHARP AROUND TOWN. OUTFITS ARE PURELY COSMETIC - BUT EVERYONE WILL NOTICE.";
            }
        else
            for (int i = 0; i < NUM_UPGRADES && n < max; i++, n++) {
                ShopItem *it = NEW();
                static const char *ic[4] = {"^^", "$$", "[#]", ">>>"};
                snprintf(it->name, sizeof it->name, "%s", g_upNames[i]);
                it->price = g_upPrice[i];
                it->status = (g_prog.upgrades >> i) & 1;
                if (it->status) snprintf(it->info, sizeof it->info, "OWNED");
                else snprintf(it->info, sizeof it->info, "%d COINS", it->price);
                it->desc = g_upDesc[i];
                it->icon = ic[i];
            }
    } else if (shop == SHOP_ARMORY || shop == SHOP_GUNX) {
        int maxW = shop == SHOP_GUNX ? NUM_WEAPONS : W_ROCKET + 1;
        if (shop == SHOP_GUNX && tab == 1) {
            for (int w = 1; w < maxW && n < max; w++, n++) {
                ShopItem *it = NEW();
                snprintf(it->name, sizeof it->name, "SELL %s", g_weap[w].name);
                if ((g_prog.weapons >> w) & 1) {
                    it->status = 0;
                    it->price = 0;
                    snprintf(it->info, sizeof it->info, "WE PAY %d COINS", weapon_sell_value(w));
                } else { it->status = 4; snprintf(it->info, sizeof it->info, "YOU DON'T OWN ONE"); }
                it->preview = 4; it->pidx = w;
                it->desc = "SELLING GIVES BACK HALF OF WHAT YOU PAID FOR THE GUN AND ITS UPGRADES, PLUS HALF THE VALUE OF YOUR AMMO.";
                weap_stats(it, w);
            }
            return n;
        }
        if (shop == SHOP_GUNX && tab >= 2) tab--; /* the stat tabs come after SELL */
        if (tab == 0)
            for (int w = 1; w < maxW && n < max; w++, n++) {
                ShopItem *it = NEW();
                snprintf(it->name, sizeof it->name, "%s", g_weap[w].name);
                int own = (g_prog.weapons >> w) & 1;
                if (!own) { it->status = 0; it->price = g_weap[w].price; snprintf(it->info, sizeof it->info, "%d COINS", it->price); }
                else { it->status = 0; it->price = g_weap[w].ammoPrice; snprintf(it->info, sizeof it->info, "OWNED  -  AMMO %d  -  +%d FOR %d", g_prog.ammo[w], g_weap[w].ammoPack, it->price); }
                it->preview = 4; it->pidx = w;
                it->desc = g_weap[w].desc;
                weap_stats(it, w);
            }
        else if (tab <= 4)
            for (int w = 1; w < maxW && n < max; w++, n++) {
                ShopItem *it = NEW();
                int k = tab - 1;
                char nm[48];
                snprintf(nm, sizeof nm, "%s %s", g_weap[w].name, WSTAT_NAMES[k]);
                level_item(it, nm, g_prog.wup[w][k], 3, wstat_price(w, k), WSTAT_DESC[k]);
                if (!((g_prog.weapons >> w) & 1)) { it->status = 4; snprintf(it->info, sizeof it->info, "BUY THE %s FIRST", g_weap[w].name); }
                it->preview = 4; it->pidx = w;
                weap_stats(it, w);
            }
        else {
            ShopItem *it = NEW();
            snprintf(it->name, sizeof it->name, "BODY ARMOR");
            it->price = 150;
            if (g_pl.armor >= 99.5f) { it->status = 3; snprintf(it->info, sizeof it->info, "FULLY ARMORED"); }
            else snprintf(it->info, sizeof it->info, "ARMOR %d/100  -  150 COINS", (int)g_pl.armor);
            it->desc = "SOAKS UP 60% OF INCOMING DAMAGE UNTIL IT IS WORN DOWN.";
            it->icon = "[+]";
            n++;
            it = NEW();
            level_item(it, "VITALITY TRAINING", g_prog.vitality, 4, 400 * (g_prog.vitality + 1), "+25 MAXIMUM HEALTH PER LEVEL.");
            it->icon = "<3";
            n++;
            it = NEW();
            snprintf(it->name, sizeof it->name, "FIRST AID KIT");
            it->price = 50;
            if (g_pl.health >= player_max_health() - 0.5f) { it->status = 3; snprintf(it->info, sizeof it->info, "ALREADY AT FULL HEALTH"); }
            else snprintf(it->info, sizeof it->info, "HEALTH %d  -  50 COINS", (int)g_pl.health);
            it->desc = "RESTORES YOUR HEALTH TO FULL ON THE SPOT.";
            it->icon = "+";
            n++;
        }
    } else if (shop == SHOP_HANGAR) {
        if (tab == 0)
            for (int i = 0; i < NUM_AIR_SHOP && n < max; i++, n++) {
                ShopItem *it = NEW();
                int type = AIR_FIRST + i;
                snprintf(it->name, sizeof it->name, "%s", g_carTypes[type].name);
                it->price = g_carTypes[type].price;
                it->status = g_prog.curAir == i ? 2 : ((g_prog.aircraft >> i) & 1);
                if (it->status == 0) snprintf(it->info, sizeof it->info, "%d COINS", it->price);
                else snprintf(it->info, sizeof it->info, it->status == 2 ? "AT YOUR STAND" : "OWNED");
                it->preview = 3; it->pidx = type;
                memcpy(it->pcol, g_prog.airPaint ? g_paints[g_prog.airPaint] : g_carTypes[type].col, sizeof it->pcol);
                it->desc = "YOUR SELECTED AIRCRAFT WAITS AT YOUR PRIVATE STAND ON THE APRON. OWNED AIRCRAFT CAN ALSO BE FLOWN FROM THEIR OWN STANDS.";
                car_stats(it, type);
            }
        else if (tab == 1)
            for (int k = 0; k < 6 && n < max; k++, n++) {
                ShopItem *it = NEW();
                level_item(it, AIR_UP_NAMES[k], g_prog.airUp[k], 3, AIR_UP_BASE[k] * (g_prog.airUp[k] + 1), AIR_UP_DESC[k]);
                static const char *ic[6] = {">>", "/^", "[#]", "==", "->", "[F]"};
                it->icon = ic[k];
            }
        else
            for (int k = 0; k < 9 && n < max; k++, n++) {
                ShopItem *it = NEW();
                snprintf(it->name, sizeof it->name, "%s", g_paintNames[k]);
                it->price = k ? 100 : 0;
                it->status = g_prog.airPaint == k ? 2 : 0;
                snprintf(it->info, sizeof it->info, it->status == 2 ? "APPLIED" : k ? "100 COINS" : "FREE");
                it->preview = 3;
                it->pidx = AIR_FIRST + g_prog.curAir;
                memcpy(it->pcol, k ? g_paints[k] : g_carTypes[it->pidx].col, sizeof it->pcol);
                it->desc = "PAINT JOBS APPLY TO YOUR OWN AIRCRAFT.";
            }
    } else {
        if (tab == 0)
            for (int k = 0; k < 4 && n < max; k++, n++) {
                ShopItem *it = NEW();
                level_item(it, CAR_UP_NAMES[k], g_prog.carUp[k], 3, CAR_UP_BASE[k] * (g_prog.carUp[k] + 1), CAR_UP_DESC[k]);
                static const char *ic[4] = {">>", "(T)", "[#]", "[F]"};
                it->icon = ic[k];
            }
        else
            for (int k = 0; k < 9 && n < max; k++, n++) {
                ShopItem *it = NEW();
                snprintf(it->name, sizeof it->name, "%s", g_paintNames[k]);
                it->price = k ? 80 : 0;
                it->status = g_prog.carPaint == k ? 2 : 0;
                snprintf(it->info, sizeof it->info, it->status == 2 ? "APPLIED" : k ? "80 COINS" : "FREE");
                it->preview = 1;
                it->pidx = g_prog.curCar;
                memcpy(it->pcol, k ? g_paints[k] : g_carTypes[g_prog.curCar].col, sizeof it->pcol);
                it->desc = "PAINT JOBS APPLY TO YOUR OWN CARS - THE ONES YOU CALL.";
            }
    }
#undef NEW
    return n;
}

static int pay(int price) {
    if (g_prog.coins >= price) {
        g_prog.coins -= price;
        audio_play(SND_BUY, 0.7f, 1);
        return 1;
    }
    audio_play(SND_DENY, 0.6f, 1);
    hud_toast("NOT ENOUGH COINS - YOU NEED %d MORE", price - g_prog.coins);
    return 0;
}
static void repaint_cars(void) {
    for (int i = 0; i < MAX_CARS; i++)
        if (g_cars[i].used && g_cars[i].called)
            memcpy(g_cars[i].col, g_prog.carPaint ? g_paints[g_prog.carPaint] : g_carTypes[g_cars[i].type].col, sizeof g_cars[i].col);
}

void shop_buy(int shop, int tab, int i) {
    ShopItem items[16];
    int n = shop_items(shop, tab, items, 16);
    if (i < 0 || i >= n) return;
    ShopItem *it = &items[i];
    if (it->status == 4) { audio_play(SND_DENY, 0.5f, 1); hud_toast("%s", it->info); return; }
    if (it->status == 3) { audio_play(SND_DENY, 0.4f, 1); hud_toast("ALREADY MAXED OUT"); return; }
    if (shop == SHOP_BOLT) {
        unsigned *mask = tab == 0 ? &g_prog.cars : tab == 1 ? &g_prog.outfits : &g_prog.upgrades;
        if ((*mask >> i) & 1) {
            if (tab == 0) { g_prog.curCar = i; hud_toast("%s EQUIPPED - PRESS [%s] TO CALL IT", g_carTypes[i].name, act_key(ACT_CALLCAR)); }
            else if (tab == 1) { g_prog.outfit = i; hud_toast("WEARING: %s", g_lookNames[i]); }
            audio_play(SND_CLICK, 0.4f, 1);
            return;
        }
        if (!pay(it->price)) return;
        *mask |= 1u << i;
        if (tab == 0) g_prog.curCar = i;
        if (tab == 1) g_prog.outfit = i;
        hud_toast("PURCHASED %s!", it->name);
    } else if (shop == SHOP_GUNX && tab == 1) {
        int w = i + 1, v = weapon_sell_value(w);
        add_coins(v);
        g_prog.earned -= v;
        g_prog.weapons &= ~(1u << w);
        g_prog.ammo[w] = 0;
        memset(g_prog.wup[w], 0, sizeof g_prog.wup[w]);
        g_pl.clip[w] = 0;
        if (g_pl.weapon == w) g_pl.weapon = W_FISTS;
        audio_play(SND_COIN, 0.6f, 0.9f);
        hud_toast("SOLD %s FOR %d COINS", g_weap[w].name, v);
    } else if (shop == SHOP_ARMORY || shop == SHOP_GUNX) {
        if (shop == SHOP_GUNX && tab >= 2) tab--;
        if (tab == 0) {
            int w = i + 1;
            if (!pay(it->price)) return;
            if ((g_prog.weapons >> w) & 1) { g_prog.ammo[w] += g_weap[w].ammoPack; hud_toast("+%d %s AMMO", g_weap[w].ammoPack, g_weap[w].name); }
            else { give_weapon(w, g_weap[w].ammoPack); g_pl.weapon = w; hud_toast("PURCHASED %s! SWITCH WEAPONS WITH [%s]", g_weap[w].name, act_key(ACT_NEXTWEAP)); }
        } else if (tab <= 4) {
            int w = i + 1, k = tab - 1;
            if (!pay(it->price)) return;
            g_prog.wup[w][k]++;
            hud_toast("%s UPGRADED TO LEVEL %d", it->name, g_prog.wup[w][k]);
        } else {
            if (!pay(it->price)) return;
            if (i == 0) { g_pl.armor = 100; hud_toast("BODY ARMOR EQUIPPED"); }
            else if (i == 1) { g_prog.vitality++; g_pl.health += 25; hud_toast("MAXIMUM HEALTH IS NOW %d", (int)player_max_health()); }
            else { g_pl.health = player_max_health(); audio_play(SND_HEAL, 0.6f, 1); hud_toast("ALL PATCHED UP!"); }
        }
    } else if (shop == SHOP_HANGAR) {
        if (tab == 0) {
            if (!((g_prog.aircraft >> i) & 1)) {
                if (!pay(it->price)) return;
                g_prog.aircraft |= 1u << i;
                hud_toast("PURCHASED %s!", it->name);
            } else audio_play(SND_CLICK, 0.4f, 1);
            g_prog.curAir = i;
            sim_respawn_stands();
            hud_toast("YOUR %s IS WAITING AT YOUR STAND", it->name);
        } else if (tab == 1) {
            if (!pay(it->price)) return;
            g_prog.airUp[i]++;
            hud_toast("%s UPGRADED TO LEVEL %d", it->name, g_prog.airUp[i]);
        } else {
            if (it->status == 2) return;
            if (!pay(it->price)) return;
            g_prog.airPaint = i;
            sim_respawn_stands();
            hud_toast("AIRCRAFT PAINTED: %s", it->name);
        }
    } else {
        if (tab == 0) {
            if (!pay(it->price)) return;
            g_prog.carUp[i]++;
            hud_toast("%s UPGRADED TO LEVEL %d", it->name, g_prog.carUp[i]);
        } else {
            if (it->status == 2) return;
            if (!pay(it->price)) return;
            g_prog.carPaint = i;
            repaint_cars();
            hud_toast("CAR PAINTED: %s", it->name);
        }
    }
    save_game();
}
