/* Front end: game states, menus, HUD, shop, options, key rebinding, map, dialogs. */
#include "game.h"

enum { S_MENU, S_PLAY, S_PAUSE, S_OPTIONS, S_CONTROLS, S_SHOP, S_DIALOG, S_HOWTO, S_MAP, S_CONFIRM, S_TRAVEL, S_FLIGHT };
int g_wpOn;
V3 g_wpPos;
static int travelSel, travelByPlane, flightDest, flightPlane, bmMap = -1;
static float flightT, bmZoom = 1;
static V3 bmCenter;
#define TICKET_PRICE 75
static int state = S_MENU, optReturn = S_MENU, ctlReturn = S_OPTIONS, sel, optSel, ctlRow, ctlCol, ctlWait, ctlWaitFrames;
static int dlgMission, shopTab, shopSel, shopId, confirmSel, howReturn = S_MENU;
static float msgT, menuT, coinShown, fpsAcc, fps = 60;
static int fpsFrames;
static char msgBuf[200];
static char toasts[4][140];
static float toastT[4];
static int lmx = -1, lmy = -1, mouseMoved;
static int scenarioSet = 0;

static float S(void) { return fminf(g_winH / 720.0f, g_winW / 1280.0f); }

void hud_msg(float secs, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msgBuf, sizeof msgBuf, fmt, ap);
    va_end(ap);
    msgT = secs;
}
void hud_toast(const char *fmt, ...) {
    for (int i = 3; i > 0; i--) { memcpy(toasts[i], toasts[i - 1], sizeof toasts[0]); toastT[i] = toastT[i - 1]; }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(toasts[0], sizeof toasts[0], fmt, ap);
    va_end(ap);
    toastT[0] = 3.5f;
}
void ui_open_dialog(int m) { dlgMission = m; state = S_DIALOG; audio_play(SND_CLICK, 0.5f, 1); }
void ui_open_travel(int byPlane) { state = S_TRAVEL; travelByPlane = byPlane; travelSel = (g_curMap + 1) % NUM_MAPS; audio_play(SND_BEEP, 0.5f, 1); audio_engine(0, 0); }
void ui_open_shop(int which) { state = S_SHOP; shopId = which; shopTab = 0; shopSel = 0; audio_play(SND_BUY, 0.4f, 1.3f); }

/* --------------------------------------------------------- input helpers */
static int nav_up(void) { return key_pressed(XK_Up) || key_pressed(XK_w) || g_wheel > 0; }
static int nav_down(void) { return key_pressed(XK_Down) || key_pressed(XK_s) || g_wheel < 0; }
static int nav_left(void) { return key_pressed(XK_Left) || key_pressed(XK_a); }
static int nav_right(void) { return key_pressed(XK_Right) || key_pressed(XK_d); }
static int nav_ok(void) { return key_pressed(XK_Return) || key_pressed(XK_KP_Enter) || key_pressed(XK_space); }
static int nav_back(void) { return key_pressed(XK_Escape); }
static int mouse_in(float x, float y, float w, float h) { return g_mouseX >= x && g_mouseX <= x + w && g_mouseY >= y && g_mouseY <= y + h; }
static void click(void) { audio_play(SND_CLICK, 0.4f, 1); }

static void dim(float a) { ui_rect(0, 0, (float)g_winW, (float)g_winH, 0.02f, 0.03f, 0.08f, a); }
static void panel(float x, float y, float w, float h) {
    float s = S();
    ui_rect(x, y, w, h, 0.06f, 0.08f, 0.16f, 0.88f);
    ui_frame_rect(x, y, w, h, 3 * s, 1, 0.75f, 0.15f, 0.9f);
}

static int menu_list(const char **items, int n, int *selp, float y0, float sc) {
    float s = S();
    float lh = sc * 12;
    int chosen = -1;
    if (nav_up()) { *selp = (*selp + n - 1) % n; click(); }
    if (nav_down()) { *selp = (*selp + 1) % n; click(); }
    for (int i = 0; i < n; i++) {
        float w = fmaxf(ui_text_w(items[i], sc) + 60 * s, 320 * s);
        float x = g_winW * 0.5f - w / 2, y = y0 + i * lh;
        if (mouse_in(x, y - 7 * s, w, sc * 7 + 14 * s)) {
            if (mouseMoved && *selp != i) { *selp = i; }
            if (g_mouseClick) chosen = i;
        }
        int on = i == *selp;
        if (on) ui_rect(x, y - 7 * s, w, sc * 7 + 14 * s, 1, 0.75f, 0.12f, 0.92f);
        else ui_rect(x, y - 7 * s, w, sc * 7 + 14 * s, 0.05f, 0.07f, 0.15f, 0.55f);
        ui_text_c(g_winW * 0.5f, y, sc, items[i], on ? 0.1f : 1, on ? 0.08f : 1, on ? 0.05f : 1, 1);
    }
    if (nav_ok()) chosen = *selp;
    if (chosen >= 0) click();
    return chosen;
}

/* ------------------------------------------------------------ backdrop */
static void menu_backdrop(float dt) {
    menuT += dt;
    sim_ambient(dt);
    float a = menuT * 0.04f;
    g_camPos = v3(cosf(a) * 240, 95, sinf(a) * 240);
    g_camLook = v3(cosf(a + 0.6f) * 40, 0, sinf(a + 0.6f) * 40);
    sim_render();
}
static void title(float s) {
    float cx = g_winW * 0.5f;
    ui_text_c(cx + 4 * s, 64 * s, 11 * s, "MAPLE BAY", 0.6f, 0.15f, 0.05f, 1);
    ui_text_c(cx, 60 * s, 11 * s, "MAPLE BAY", 1, 0.82f, 0.2f, 1);
    ui_text_c(cx, 150 * s, 8 * s, "MAYHEM", 1, 0.35f, 0.2f, 1);
    ui_text_c(cx, 218 * s, 1.8f * s, "DRIVE  -  JUMP  -  SMASH  -  SAVE THE TOWN", 0.9f, 0.95f, 1, 1);
}

static void start_game(int fromSave) {
    sim_new_game(fromSave);
    state = S_PLAY;
    coinShown = (float)g_prog.coins;
    if (fromSave) hud_msg(4, "WELCOME BACK TO MAPLE BAY!");
    else hud_msg(6, "WELCOME TO MAPLE BAY!  TALK TO PEOPLE WITH  !  OVER THEIR HEADS");
    hud_toast("PRESS [%s] FOR THE MAP,  [ESC] TO PAUSE", act_key(ACT_MAP));
}

static void draw_main_menu(float dt) {
    float s = S();
    menu_backdrop(dt);
    ui_begin();
    dim(0.35f);
    title(s);
    int hasSave = save_exists();
    const char *items[5];
    int ids[5], n = 0;
    if (hasSave) { items[n] = "CONTINUE"; ids[n++] = 0; }
    items[n] = "NEW GAME"; ids[n++] = 1;
    items[n] = "OPTIONS"; ids[n++] = 2;
    items[n] = "HOW TO PLAY"; ids[n++] = 3;
    items[n] = "QUIT"; ids[n++] = 4;
    if (sel >= n) sel = 0;
    int c = menu_list(items, n, &sel, 300 * s, 3.2f * s);
    ui_text_c(g_winW * 0.5f, g_winH - 30 * s, 1.4f * s, "F11 FULLSCREEN   -   F12 SCREENSHOT", 0.8f, 0.8f, 0.85f, 0.8f);
    ui_end();
    if (c >= 0) {
        switch (ids[c]) {
        case 0: start_game(1); break;
        case 1:
            if (hasSave) { state = S_CONFIRM; confirmSel = 1; }
            else start_game(0);
            break;
        case 2: optReturn = S_MENU; state = S_OPTIONS; optSel = 0; break;
        case 3: howReturn = S_MENU; state = S_HOWTO; break;
        case 4: g_quit = 1; break;
        }
    }
    if (nav_back()) { /* nothing on main menu */ }
}
static void draw_confirm(float dt) {
    float s = S();
    menu_backdrop(dt);
    ui_begin();
    dim(0.55f);
    panel(g_winW * 0.5f - 380 * s, 200 * s, 760 * s, 300 * s);
    ui_text_c(g_winW * 0.5f, 240 * s, 2.6f * s, "START A NEW GAME?", 1, 0.85f, 0.2f, 1);
    ui_text_c(g_winW * 0.5f, 290 * s, 1.7f * s, "YOUR CURRENT SAVE WILL BE OVERWRITTEN.", 1, 1, 1, 1);
    const char *items[2] = {"YES, START OVER", "NO, GO BACK"};
    int c = menu_list(items, 2, &confirmSel, 350 * s, 2.6f * s);
    ui_end();
    if (c == 0) start_game(0);
    if (c == 1 || nav_back()) state = S_MENU;
}

/* ----------------------------------------------------------------- HUD */
static void map_xf(V3 w, V3 center, float cx, float cy, float scale, float yaw, float *sx, float *sy) {
    V3 f = fwd_of(yaw), r = right_of(yaw);
    float dx = w.x - center.x, dz = w.z - center.z;
    *sx = cx + (dx * r.x + dz * r.z) * scale;
    *sy = cy - (dx * f.x + dz * f.z) * scale;
}
static struct { V3 center; float cx, cy, scale, yaw, half; int mini; } MP;
static void map_icon(V3 p, int kind) {
    float s = S(), x, y;
    if (p.x > 50000) return; /* not on this island */
    map_xf(p, MP.center, MP.cx, MP.cy, MP.scale, MP.yaw, &x, &y);
    if (MP.mini) {
        float dx = x - MP.cx, dy = y - MP.cy, m = fmaxf(fabsf(dx), fabsf(dy));
        float lim = MP.half - 7 * s;
        if (m > lim) {
            if (kind != 0 && kind != 14) return;
            x = MP.cx + dx * lim / m;
            y = MP.cy + dy * lim / m;
        }
    }
    float r = (MP.mini ? 5.5f : 7.5f) * s;
    switch (kind) {
    case 0:
        ui_circle(x, y, r + 2 * s, 0, 0, 0, 0.8f);
        ui_circle(x, y, r, 1, 0.9f, 0.1f, 1);
        break;
    case 1:
        ui_circle(x, y, r + 2 * s, 0, 0, 0, 0.8f);
        ui_circle(x, y, r, 1, 0.55f, 0.1f, 1);
        ui_text_c(x, y - 3.5f * s, 1.0f * s, "!", 1, 1, 1, 1);
        break;
    case 2: ui_circle(x, y, r * 0.7f, 0.3f, 0.6f, 1, 0.9f); break;
    case 3: ui_circle(x, y, r * 0.8f, 1, 0.2f, 0.2f, 1); break;
    case 4:
        ui_circle(x, y, r + 2 * s, 0, 0, 0, 0.8f);
        ui_circle(x, y, r, 0.2f, 0.85f, 0.3f, 1);
        ui_text_c(x, y - 3.5f * s, 1.0f * s, "$", 1, 1, 1, 1);
        break;
    case 5: ui_circle(x, y, r * 0.8f, 0.3f, 0.7f, 1, 1); break;
    case 6: ui_circle(x, y, r * 0.6f, 0.75f, 0.25f, 1, 1); break;
    case 7: ui_circle(x, y, r * 0.65f, 0.2f, 0.45f, 1, 1); ui_circle(x, y, r * 0.3f, 1, 1, 1, 1); break;
    case 14: {
        float d = r * 1.4f;
        glColor4f(0, 0, 0, 0.85f);
        glBegin(GL_QUADS); glVertex2f(x, y - d - 2 * s); glVertex2f(x + d + 2 * s, y); glVertex2f(x, y + d + 2 * s); glVertex2f(x - d - 2 * s, y); glEnd();
        glColor4f(0.3f, 0.7f, 1, 1);
        glBegin(GL_QUADS); glVertex2f(x, y - d); glVertex2f(x + d, y); glVertex2f(x, y + d); glVertex2f(x - d, y); glEnd();
    } break;
    default:
        if (kind >= 8 && kind <= 16) {
            static const char *L[9] = {"G", "A", "T", "+", "F", "P", "", "W", ">"};
            static const float C[9][3] = {{0.9f, 0.3f, 0.2f}, {0.25f, 0.55f, 1}, {1, 0.55f, 0.1f}, {0.95f, 0.95f, 0.95f}, {0.95f, 0.8f, 0.1f}, {0.2f, 0.3f, 0.8f},
                                          {0, 0, 0}, {0.95f, 0.7f, 0.1f}, {0.3f, 0.75f, 1}};
            int k = kind - 8;
            ui_circle(x, y, r + 2 * s, 0, 0, 0, 0.8f);
            ui_circle(x, y, r, C[k][0], C[k][1], C[k][2], 1);
            ui_text_c(x, y - 3.5f * s, 1.0f * s, L[k], k == 3 ? 0.9f : 1, k == 3 ? 0.1f : 1, k == 3 ? 0.1f : 1, 1);
        }
        break;
    }
}
static void map_player_arrow(float s) {
    float x, y;
    V3 pp = player_pos();
    map_xf(pp, MP.center, MP.cx, MP.cy, MP.scale, MP.yaw, &x, &y);
    float fy = g_pl.car >= 0 ? g_cars[g_pl.car].yaw : g_pl.yaw;
    float a = fy - MP.yaw;
    /* yaw increase turns left on screen */
    float sz = (MP.mini ? 9 : 11) * s;
    float ux = -sinf(a), uy = -cosf(a);
    float rx = -uy, ry = ux;
    glColor4f(0, 0, 0, 0.8f);
    glBegin(GL_TRIANGLES);
    glVertex2f(x + ux * sz * 1.3f, y + uy * sz * 1.3f);
    glVertex2f(x - ux * sz * 0.8f + rx * sz, y - uy * sz * 0.8f + ry * sz);
    glVertex2f(x - ux * sz * 0.8f - rx * sz, y - uy * sz * 0.8f - ry * sz);
    glColor4f(1, 1, 1, 1);
    glVertex2f(x + ux * sz, y + uy * sz);
    glVertex2f(x - ux * sz * 0.6f + rx * sz * 0.7f, y - uy * sz * 0.6f + ry * sz * 0.7f);
    glVertex2f(x - ux * sz * 0.6f - rx * sz * 0.7f, y - uy * sz * 0.6f - ry * sz * 0.7f);
    glEnd();
}
static void draw_map_geometry(void) {
    V3 f = fwd_of(MP.yaw), r = right_of(MP.yaw);
    GLfloat m[16] = {r.x, -f.x, 0, 0, r.z, -f.z, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    glPushMatrix();
    glTranslatef(MP.cx, MP.cy, 0);
    glScalef(MP.scale, MP.scale, 1);
    glMultMatrixf(m);
    glTranslatef(-MP.center.x, -MP.center.z, 0);
    world_draw_map();
    glEnable(GL_BLEND);
    turf_draw_map();
    glPopMatrix();
}
static void draw_markers(void) {
    combat_map_markers(map_icon);
    map_icon(g_spots.shop, 4);
    map_icon(g_spots.armory, 8);
    map_icon(g_spots.hangar, 9);
    map_icon(g_spots.tuner, 10);
    for (int i = 0; i < g_spots.nhosp; i++) map_icon(g_spots.hosp[i], 11);
    map_icon(g_spots.police, 13);
    map_icon(g_spots.gunx, 15);
    map_icon(g_spots.travel, 16);
    for (int i = 0; i < g_spots.ngas; i++) map_icon(g_spots.gas[i], 12);
    for (int i = 0; i < MAX_CARS; i++)
        if (g_cars[i].used && g_cars[i].called && i != g_pl.car && !g_cars[i].wrecked) map_icon(g_cars[i].pos, 5);
    mission_map_markers(map_icon);
    if (g_wpOn) map_icon(g_wpPos, 14);
}
/* GPS line along the roads to the waypoint */
static void draw_route(void) {
    if (!g_wpOn) return;
    float s = S();
    V3 pts[64];
    int n = road_route(player_pos(), g_wpPos, pts, 64);
    for (int pass = 0; pass < 2; pass++) {
        float w = (pass ? 2.2f : 4.0f) * s * (MP.mini ? 1 : 1.2f);
        if (pass) glColor4f(0.35f, 0.75f, 1, 0.95f); else glColor4f(0, 0, 0, 0.6f);
        glBegin(GL_QUADS);
        for (int i = 0; i + 1 < n; i++) {
            float x0, y0, x1, y1;
            map_xf(pts[i], MP.center, MP.cx, MP.cy, MP.scale, MP.yaw, &x0, &y0);
            map_xf(pts[i + 1], MP.center, MP.cx, MP.cy, MP.scale, MP.yaw, &x1, &y1);
            float dx = x1 - x0, dy = y1 - y0, l = sqrtf(dx * dx + dy * dy);
            if (l < 0.01f) continue;
            float nx = -dy / l * w * 0.5f, ny = dx / l * w * 0.5f;
            glVertex2f(x0 + nx - dx / l * w * 0.5f, y0 + ny - dy / l * w * 0.5f); glVertex2f(x1 + nx + dx / l * w * 0.5f, y1 + ny + dy / l * w * 0.5f);
            glVertex2f(x1 - nx + dx / l * w * 0.5f, y1 - ny + dy / l * w * 0.5f); glVertex2f(x0 - nx - dx / l * w * 0.5f, y0 - ny - dy / l * w * 0.5f);
        }
        glEnd();
    }
}
static void draw_minimap(float s) {
    float size = 200 * s, mx = g_winW - size - 16 * s, my = 16 * s;
    ui_rect(mx - 4 * s, my - 4 * s, size + 8 * s, size + 8 * s, 0, 0, 0, 0.65f);
    glEnable(GL_SCISSOR_TEST);
    glScissor((int)mx, (int)(g_winH - (my + size)), (int)size, (int)size);
    MP.center = player_pos();
    MP.cx = mx + size / 2;
    MP.cy = my + size / 2;
    MP.scale = size / 230.0f;
    MP.yaw = g_camYaw;
    MP.half = size / 2;
    MP.mini = 1;
    draw_map_geometry();
    draw_route();
    draw_markers();
    map_player_arrow(s);
    ui_text_c(mx + size / 2, my + size - 14 * s, 1.1f * s, map_name(g_curMap), 1, 1, 1, 0.85f);
    glDisable(GL_SCISSOR_TEST);
    ui_frame_rect(mx - 4 * s, my - 4 * s, size + 8 * s, size + 8 * s, 2 * s, 1, 0.75f, 0.15f, 0.9f);
}
static V3 block_center(int i, int j) {
    float x0, z0, x1, z1;
    block_rect(i, j, &x0, &z0, &x1, &z1);
    return v3((x0 + x1) / 2, 0, (z0 + z1) / 2);
}
static void set_waypoint(V3 p, const char *what) {
    g_wpOn = 1;
    g_wpPos = v3(p.x, 0, p.z);
    audio_play(SND_CHECK, 0.5f, 1.2f);
    if (what) hud_toast("WAYPOINT SET: %s", what);
}
static int nearest_of(const V3 *list, int n, V3 *out) {
    V3 pp = player_pos();
    float bd = 1e18f;
    int found = 0;
    for (int i = 0; i < n; i++) {
        if (list[i].x > 50000) continue;
        float d = dist2xz(pp, list[i]);
        if (d < bd) { bd = d; *out = list[i]; found = 1; }
    }
    return found;
}
static void draw_bigmap(void) {
    float s = S();
    ui_begin();
    dim(0.7f);
    float size = fminf(g_winW * 0.9f, g_winH * 0.86f);
    float mx = g_winW * 0.5f - size / 2, my = g_winH * 0.5f - size / 2 + 10 * s;
    /* view: fit the island and airport, then apply zoom and pan */
    float zmin = -ISLAND, zmax = AIR_Z1 + 10, span = fmaxf(2 * ISLAND, zmax - zmin) * 1.03f;
    V3 home = v3(0, 0, (zmin + zmax) / 2);
    if (bmMap != g_curMap) { bmMap = g_curMap; bmZoom = 1; bmCenter = home; }
    float base = size / span;
    int inMap = mouse_in(mx, my, size, size);
    /* zoom toward the cursor with the wheel */
    if (g_wheel != 0) {
        float nz = clampf(bmZoom * (g_wheel > 0 ? 1.35f : 1 / 1.35f), 1, 8);
        if (inMap) {
            float sc0 = base * bmZoom, sc1 = base * nz;
            float ax = (g_mouseX - (mx + size / 2)), ay = (g_mouseY - (my + size / 2));
            /* big map yaw is PI: screen x = world x, screen y = world z */
            bmCenter.x += ax / sc0 - ax / sc1;
            bmCenter.z += ay / sc0 - ay / sc1;
        }
        bmZoom = nz;
    }
    float pan = 600 / (base * bmZoom) * (1.0f / 60);
    if (key_down(XK_Left) || key_down(XK_a)) bmCenter.x -= pan * size * 0.02f;
    if (key_down(XK_Right) || key_down(XK_d)) bmCenter.x += pan * size * 0.02f;
    if (key_down(XK_Up) || key_down(XK_w)) bmCenter.z -= pan * size * 0.02f;
    if (key_down(XK_Down) || key_down(XK_s)) bmCenter.z += pan * size * 0.02f;
    if (key_pressed(XK_c)) { bmCenter = player_pos(); if (bmZoom < 2.5f) bmZoom = 2.5f; }
    if (key_pressed(XK_r)) { bmCenter = home; bmZoom = 1; }
    if (bmZoom <= 1.001f) bmCenter = home;
    glEnable(GL_SCISSOR_TEST);
    glScissor((int)mx, (int)(g_winH - (my + size)), (int)size, (int)size);
    MP.center = bmCenter;
    MP.cx = mx + size / 2;
    MP.cy = my + size / 2;
    MP.scale = base * bmZoom;
    MP.yaw = PI;
    MP.half = size / 2;
    MP.mini = 0;
    draw_map_geometry();
    struct { const char *n; V3 p; } labels[16];
    int nl = 0;
#define LAB(t, pos) do { labels[nl].n = t; labels[nl].p = pos; nl++; } while (0)
    LAB("AIRPORT", v3(0, 0, AZ(455)));
    if (g_curMap == 0) {
        LAB("HOME", g_spots.home); LAB("SHOP", v3(-35, 0, 48)); LAB("DOCKS", v3(245, 0, -245)); LAB("PIZZA", v3(-175, 0, 175));
        LAB("FIZZ LABS", v3(175, 0, 175)); LAB("STUNT PARK", v3(35, 0, 35)); LAB("CLOCK TOWER", v3(-35, 0, -46));
        LAB("RUST RATS", v3(35, 0, -205)); LAB("NEON VIPERS", v3(-105, 0, -205));
        LAB("HOSPITAL", block_center(10, 5)); LAB("MALL", block_center(1, 10)); LAB("STADIUM", block_center(9, 10));
        LAB("FIRE STATION", block_center(5, 0)); LAB("BUILDING SITE", block_center(11, 11));
    } else if (g_curMap == 1) {
        LAB("CANYON", block_center(0, 0)); LAB("CASINO", block_center(2, 4)); LAB("CLINIC", block_center(5, 1));
        LAB("OASIS", block_center(5, 5)); LAB("LOCK & LOAD", block_center(4, 4));
    } else {
        LAB("SKI JUMP", block_center(0, 0)); LAB("FROZEN LAKE", block_center(5, 0)); LAB("LODGE", block_center(2, 2));
        LAB("ARMORY", block_center(4, 4)); LAB("CLINIC", block_center(1, 4));
    }
#undef LAB
    float ls = clampf(1.1f + bmZoom * 0.15f, 1.1f, 1.8f) * s;
    for (int i = 0; i < nl; i++) {
        float x, y;
        map_xf(labels[i].p, MP.center, MP.cx, MP.cy, MP.scale, MP.yaw, &x, &y);
        ui_text_c(x, y + 8 * s, ls, labels[i].n, 1, 1, 1, 0.95f);
    }
    draw_route();
    draw_markers();
    map_player_arrow(s);
    /* cursor: click to drop a waypoint */
    if (inMap) {
        ui_rect(g_mouseX - 8 * s, g_mouseY - 1 * s, 16 * s, 2 * s, 1, 1, 1, 0.8f);
        ui_rect(g_mouseX - 1 * s, g_mouseY - 8 * s, 2 * s, 16 * s, 1, 1, 1, 0.8f);
        if (g_mouseClick) {
            V3 w = v3(bmCenter.x + (g_mouseX - MP.cx) / MP.scale, 0, bmCenter.z + (g_mouseY - MP.cy) / MP.scale);
            if (g_wpOn && dist2xz(w, g_wpPos) < (12 / MP.scale) * (12 / MP.scale)) { g_wpOn = 0; audio_play(SND_CLICK, 0.5f, 0.8f); }
            else set_waypoint(w, NULL);
        }
    }
    glDisable(GL_SCISSOR_TEST);
    if (key_pressed(MOUSE_BASE + 3) || key_pressed(XK_BackSpace) || key_pressed(XK_Delete)) { if (g_wpOn) audio_play(SND_CLICK, 0.5f, 0.8f); g_wpOn = 0; }
    /* quick waypoints */
    {
        V3 p;
        V3 shops[5] = {g_spots.shop, g_spots.armory, g_spots.gunx, g_spots.tuner, g_spots.hangar};
        V3 car = FAR_AWAY;
        for (int i = 0; i < MAX_CARS; i++) if (g_cars[i].used && g_cars[i].called && i != g_pl.car && !g_cars[i].wrecked) car = g_cars[i].pos;
        if (key_pressed(XK_1) && nearest_of(g_spots.gas, g_spots.ngas, &p)) set_waypoint(p, "NEAREST GAS STATION");
        if (key_pressed(XK_2) && nearest_of(g_spots.hosp, g_spots.nhosp, &p)) set_waypoint(p, "NEAREST HOSPITAL");
        if (key_pressed(XK_3) && nearest_of(shops, 5, &p)) set_waypoint(p, "NEAREST SHOP");
        if (key_pressed(XK_4) && g_spots.travel.x < 50000) set_waypoint(g_spots.travel, "AIRPORT FLIGHTS DESK");
        if (key_pressed(XK_5)) {
            if (mission_target(&p)) set_waypoint(p, "MISSION OBJECTIVE");
            else {
                V3 gv[NUM_MISSIONS];
                int n = 0;
                for (int i = 0; i < NUM_MISSIONS; i++)
                    if (mission_available(i) && !((g_prog.missions >> i) & 1)) gv[n++] = g_spots.giver[i];
                if (nearest_of(gv, n, &p)) set_waypoint(p, "NEXT MISSION");
            }
        }
        if (key_pressed(XK_6) && car.x < 50000) set_waypoint(car, "YOUR CAR");
    }
    ui_frame_rect(mx - 3 * s, my - 3 * s, size + 6 * s, size + 6 * s, 3 * s, 1, 0.75f, 0.15f, 1);
    char buf[160];
    snprintf(buf, sizeof buf, "MAP OF %s", map_name(g_curMap));
    ui_text_c(g_winW * 0.5f, my - 26 * s, 2.4f * s, buf, 1, 0.85f, 0.2f, 1);
    float ly = my + 10 * s, lx = mx - 230 * s;
    if (lx < 10 * s) { lx = 16 * s; ly = my + 10 * s; }
    ui_rect(lx - 6 * s, ly - 6 * s, 210 * s, 330 * s, 0, 0, 0, 0.6f);
    {
        static const char *nm[14] = {"MISSION", "REPLAY MISSION", "SHOP", "OBJECTIVE", "YOUR CAR", "ARMORY", "WEAPON STORE", "HANGAR",
                                     "TUNERS", "HOSPITAL", "GAS", "POLICE", "FLIGHTS", "WAYPOINT"};
        static const float C[14][3] = {{1, 0.55f, 0.1f}, {0.3f, 0.6f, 1}, {0.2f, 0.85f, 0.3f}, {1, 0.9f, 0.1f}, {0.3f, 0.7f, 1}, {0.9f, 0.3f, 0.2f},
                                       {0.95f, 0.7f, 0.1f}, {0.25f, 0.55f, 1}, {1, 0.55f, 0.1f}, {0.95f, 0.95f, 0.95f}, {0.95f, 0.8f, 0.1f},
                                       {0.2f, 0.3f, 0.8f}, {0.3f, 0.75f, 1}, {0.3f, 0.7f, 1}};
        for (int i = 0; i < 14; i++) {
            float yy = ly + i * 21 * s;
            ui_circle(lx + 6 * s, yy + 5 * s, 6 * s, C[i][0], C[i][1], C[i][2], 1);
            ui_text(lx + 20 * s, yy, 1.3f * s, nm[i], 1, 1, 1, 1);
        }
        if (g_curMap == 0) {
            ui_rect(lx, ly + 14 * 21 * s, 12 * s, 10 * s, 0.8f, 0.2f, 0.4f, 1);
            ui_text(lx + 20 * s, ly + 14 * 21 * s, 1.3f * s, "GANG TURF", 1, 1, 1, 1);
        }
    }
    float rx = mx + size + 14 * s;
    if (rx + 200 * s < g_winW) {
        ui_rect(rx - 6 * s, my + 4 * s, 212 * s, 236 * s, 0, 0, 0, 0.6f);
        static const char *qk[] = {"CLICK  SET WAYPOINT", "RIGHT CLICK  CLEAR", "WHEEL  ZOOM", "WASD  PAN", "C  CENTER ON YOU", "R  RESET VIEW",
                                   "1  NEAREST GAS", "2  NEAREST HOSPITAL", "3  NEAREST SHOP", "4  AIRPORT FLIGHTS", "5  MISSION", "6  YOUR CAR"};
        for (int i = 0; i < 12; i++) ui_text(rx, my + (12 + i * 19) * s, 1.2f * s, qk[i], i < 6 ? 0.85f : 0.6f, i < 6 ? 0.85f : 0.85f, i < 6 ? 0.9f : 1, 1);
    }
    if (g_wpOn) snprintf(buf, sizeof buf, "WAYPOINT %dM AWAY   -   [%s] OR [ESC] TO CLOSE", (int)distxz(player_pos(), g_wpPos), act_key(ACT_MAP));
    else snprintf(buf, sizeof buf, "CLICK THE MAP TO SET A WAYPOINT   -   [%s] OR [ESC] TO CLOSE", act_key(ACT_MAP));
    ui_text_c(g_winW * 0.5f, my + size + 10 * s, 1.5f * s, buf, 0.85f, 0.85f, 0.9f, 1);
    ui_end();
}

/* ------------------------------------------------------------- travel */
static const char *islandDesc[NUM_MAPS] = {
    "THE BIG CITY: STORY MISSIONS, STREET GANGS, SHOPS, A STADIUM AND THE CLOCK TOWER.",
    "DESERT RESORT: THE LUCKY SANDS CASINO, DEVIL'S CANYON STUNTS AND THE LOCK & LOAD GUN STORE.",
    "SNOWY MOUNTAIN TOWN: A GIANT SKI JUMP, A FROZEN LAKE, COZY CABINS AND THE BOOM BROS ARMORY."};
static void draw_travel(float dt) {
    float s = S();
    menuT += dt;
    sim_render();
    ui_begin();
    dim(0.55f);
    float pw = 900 * s, ph = 520 * s, px = g_winW * 0.5f - pw / 2, py = g_winH * 0.5f - ph / 2;
    panel(px, py, pw, ph);
    ui_text_c(g_winW * 0.5f, py + 22 * s, 3.2f * s, travelByPlane ? "FLIGHT PLAN" : "MAPLE AIR - BOOK A FLIGHT", 1, 0.85f, 0.2f, 1);
    char buf[200];
    if (travelByPlane) snprintf(buf, sizeof buf, "YOU ARE OVER OPEN SEA. WHERE ARE YOU FLYING?");
    else snprintf(buf, sizeof buf, "TICKETS COST %d COINS.  YOU HAVE %d COINS.", TICKET_PRICE, g_prog.coins);
    ui_text_c(g_winW * 0.5f, py + 66 * s, 1.6f * s, buf, 0.85f, 0.9f, 1, 1);
    int n = NUM_MAPS + 1;
    if (nav_up()) { travelSel = (travelSel + n - 1) % n; click(); }
    if (nav_down()) { travelSel = (travelSel + 1) % n; click(); }
    int choose = -1;
    for (int i = 0; i < n; i++) {
        float y = py + (100 + i * 92) * s, x = px + 30 * s, w = pw - 60 * s, h = 82 * s;
        if (i == NUM_MAPS) h = 44 * s;
        if (mouse_in(x, y, w, h)) { if (mouseMoved) travelSel = i; if (g_mouseClick) choose = i; }
        int on = i == travelSel;
        ui_rect(x, y, w, h, on ? 1 : 0.12f, on ? 0.75f : 0.14f, on ? 0.12f : 0.25f, 0.92f);
        float tc = on ? 0.08f : 1;
        if (i == NUM_MAPS) { ui_text_c(g_winW * 0.5f, y + 14 * s, 2 * s, travelByPlane ? "TURN BACK" : "NOT NOW", tc, tc, tc, 1); continue; }
        ui_text(x + 16 * s, y + 10 * s, 2.4f * s, map_name(i), tc, tc, tc, 1);
        int gz = 0, lo = i == 0 ? 0 : i == 1 ? 10 : 15, hi = i == 0 ? 10 : i == 1 ? 15 : 20;
        for (int k = lo; k < hi; k++) gz += (g_prog.gizmos >> k) & 1;
        snprintf(buf, sizeof buf, "%s   GIZMOS %d/%d", i == g_curMap ? "YOU ARE HERE" : "", gz, hi - lo);
        ui_text_r(x + w - 16 * s, y + 14 * s, 1.4f * s, buf, on ? 0.2f : 0.5f, on ? 0.2f : 1, on ? 0.2f : 0.5f, 1);
        ui_text_wrap(x + 16 * s, y + 44 * s, w - 32 * s, 1.35f * s, islandDesc[i], on ? 0.15f : 0.85f, on ? 0.15f : 0.85f, on ? 0.15f : 0.9f, 1);
    }
    ui_text_c(g_winW * 0.5f, py + ph - 28 * s, 1.4f * s, "UP/DOWN SELECT   ENTER FLY   ESC CANCEL", 0.8f, 0.8f, 0.85f, 1);
    ui_end();
    if (nav_ok()) choose = travelSel;
    if (nav_back()) choose = NUM_MAPS;
    if (choose < 0) return;
    if (choose == NUM_MAPS || choose == g_curMap) {
        if (choose == g_curMap && choose != NUM_MAPS) hud_toast("YOU ARE ALREADY ON %s", map_name(choose));
        Car *pc = player_car();
        if (travelByPlane && pc) {
            /* turn the aircraft back toward the island */
            pc->yaw = atan2f(-pc->pos.x, AZ(200) - pc->pos.z);
            pc->pos = vadd(pc->pos, vmul(fwd_of(pc->yaw), 40));
            pc->vel = vmul(fwd_of(pc->yaw), car_speed(pc));
            g_camYaw = pc->yaw;
        }
        state = S_PLAY;
        return;
    }
    if (!travelByPlane) {
        if (g_prog.coins < TICKET_PRICE) { audio_play(SND_DENY, 0.6f, 1); hud_toast("YOU NEED %d COINS FOR A TICKET", TICKET_PRICE); return; }
        g_prog.coins -= TICKET_PRICE;
        audio_play(SND_BUY, 0.6f, 1);
    }
    flightDest = choose;
    flightPlane = travelByPlane;
    flightT = 0;
    state = S_FLIGHT;
}
static void draw_flight(float dt) {
    float s = S();
    flightT += dt;
    ui_begin();
    float W = (float)g_winW, H = (float)g_winH;
    glBegin(GL_QUADS);
    glColor3f(0.3f, 0.55f, 0.92f); glVertex2f(0, 0); glVertex2f(W, 0);
    glColor3f(0.8f, 0.9f, 1); glVertex2f(W, H * 0.62f); glVertex2f(0, H * 0.62f);
    glColor3f(0.18f, 0.45f, 0.72f); glVertex2f(0, H * 0.62f); glVertex2f(W, H * 0.62f);
    glColor3f(0.1f, 0.3f, 0.55f); glVertex2f(W, H); glVertex2f(0, H);
    glEnd();
    for (int i = 0; i < 6; i++) {
        float x = fmodf(i * 260 + 900 - flightT * 380 * (1 + i * 0.15f), W + 300) - 150;
        float y = H * (0.15f + 0.07f * i);
        ui_rect(x, y, 160 * s, 26 * s, 1, 1, 1, 0.85f);
        ui_rect(x + 40 * s, y - 14 * s, 90 * s, 20 * s, 1, 1, 1, 0.85f);
    }
    float px = W * 0.5f + sinf(flightT * 2) * 20 * s, py = H * 0.42f + sinf(flightT * 3) * 8 * s;
    ui_rect(px - 90 * s, py - 10 * s, 180 * s, 22 * s, 0.95f, 0.95f, 0.92f, 1);
    ui_rect(px - 20 * s, py - 60 * s, 40 * s, 120 * s, 0.9f, 0.9f, 0.88f, 1);
    ui_rect(px + 80 * s, py - 30 * s, 14 * s, 40 * s, 0.85f, 0.2f, 0.15f, 1);
    ui_rect(px - 100 * s, py - 6 * s, 14 * s, 14 * s, 0.3f, 0.3f, 0.3f, 1);
    char buf[120];
    snprintf(buf, sizeof buf, "FLYING TO %s", map_name(flightDest));
    ui_text_c(W * 0.5f, H * 0.7f, 4 * s, buf, 1, 1, 1, 1);
    float f = clampf(flightT / 2.6f, 0, 1);
    ui_rect(W * 0.3f, H * 0.82f, W * 0.4f, 12 * s, 0, 0, 0, 0.5f);
    ui_rect(W * 0.3f, H * 0.82f, W * 0.4f * f, 12 * s, 1, 0.8f, 0.2f, 1);
    ui_end();
    if (flightT > 2.6f) {
        travel_to(flightDest, flightPlane);
        state = S_PLAY;
    }
}

static void hud_bar(float x, float y, float w, float h, float f, float r, float g, float b) {
    ui_rect(x - 2, y - 2, w + 4, h + 4, 0, 0, 0, 0.6f);
    ui_rect(x, y, w * clampf(f, 0, 1), h, r, g, b, 0.95f);
}
static void draw_hud(float dt) {
    float s = S();
    char buf[200];
    ui_begin();
    float target = (float)g_prog.coins;
    if (fabsf(coinShown - target) < 1) coinShown = target;
    else coinShown += (target - coinShown) * clampf(dt * 8, 0, 1) + (target > coinShown ? 0.5f : -0.5f);
    ui_rect(10 * s, 10 * s, 210 * s, 78 * s, 0, 0, 0, 0.4f);
    ui_circle(36 * s, 34 * s, 15 * s, 1, 0.82f, 0.15f, 1);
    ui_circle(36 * s, 34 * s, 9 * s, 0.85f, 0.6f, 0.08f, 1);
    snprintf(buf, sizeof buf, "%d", (int)(coinShown + 0.5f));
    ui_text(60 * s, 24 * s, 3 * s, buf, 1, 0.9f, 0.3f, 1);
    int gz = 0, ms = 0;
    for (int i = 0; i < NUM_GIZMOS; i++) if (g_prog.gizmos & (1u << i)) gz++;
    for (int i = 0; i < NUM_MISSIONS; i++) if (g_prog.missions & (1u << i)) ms++;
    snprintf(buf, sizeof buf, "GIZMOS %d/%d   MISSIONS %d/%d", gz, NUM_GIZMOS, ms, NUM_MISSIONS);
    ui_text(22 * s, 64 * s, 1.3f * s, buf, 0.85f, 0.75f, 1, 1);

    /* health and armor */
    {
        float hx = 22 * s, hy = 96 * s, hw = 186 * s;
        ui_rect(10 * s, 88 * s, 210 * s, g_pl.armor > 0 ? 38 * s : 24 * s, 0, 0, 0, 0.4f);
        float hf = g_pl.health / player_max_health();
        float pulse = hf < 0.25f ? 0.6f + 0.4f * sinf(menuT * 10) : 1;
        hud_bar(hx, hy, hw, 8 * s, hf, 0.95f * pulse, hf > 0.3f ? 0.25f : 0.1f, 0.25f);
        if (g_pl.armor > 0) hud_bar(hx, hy + 14 * s, hw, 6 * s, g_pl.armor / 100, 0.4f, 0.7f, 1);
    }
    draw_minimap(s);
    /* wanted level */
    {
        int st = wanted_stars();
        float x = g_winW - 16 * s, y = 228 * s;
        if (st > 0 || g_wanted > 0) {
            ui_rect(x - 150 * s, y - 6 * s, 150 * s, 30 * s, 0, 0, 0, 0.45f);
            for (int i = 0; i < 5; i++) {
                int on = i < st;
                float fl = on && ((int)(menuT * 4) & 1) ? 0.75f : 1;
                ui_text(x - 140 * s + i * 27 * s, y, 2.6f * s, "*", on ? 1 * fl : 0.3f, on ? 0.85f * fl : 0.3f, on ? 0.2f : 0.3f, on ? 1 : 0.6f);
            }
        }
    }
    if (g_wpOn) {
        snprintf(buf, sizeof buf, "WAYPOINT %dM", (int)distxz(player_pos(), g_wpPos));
        float y = (wanted_stars() || g_wanted > 0 ? 262 : 228) * s;
        ui_rect(g_winW - 166 * s, y - 6 * s, 150 * s, 24 * s, 0, 0, 0, 0.45f);
        ui_text_r(g_winW - 24 * s, y, 1.5f * s, buf, 0.45f, 0.8f, 1, 1);
    }
    mission_hud(s);
    /* gang turf status */
    if (g_pl.car < 0 || 1) {
        int k = turf_at(player_pos());
        if (k >= 0 && !((g_prog.turf >> k) & 1)) {
            snprintf(buf, sizeof buf, "%s TURF  -  TAKE DOWN %d MORE TO CLAIM IT", g_gangNames[g_turfs[k].gang], TURF_KILLS - g_turfProg[k]);
            float y = mission_active() ? 170 * s : 20 * s;
            float w = ui_text_w(buf, 1.6f * s) + 30 * s;
            ui_rect(g_winW * 0.5f - w / 2, y - 6 * s, w, 24 * s, 0, 0, 0, 0.5f);
            ui_text_c(g_winW * 0.5f, y, 1.6f * s, buf, g_turfs[k].gang == GANG_RATS ? 1 : 0.8f, g_turfs[k].gang == GANG_RATS ? 0.4f : 0.45f, g_turfs[k].gang == GANG_RATS ? 0.3f : 1, 1);
        } else if (k >= 0) {
            float y = mission_active() ? 170 * s : 20 * s;
            ui_text_c(g_winW * 0.5f, y, 1.5f * s, "YOUR TURF", 0.4f, 1, 0.45f, 0.9f);
        }
    }

    Car *c = player_car();
    if (c) {
        const CarType *t = &g_carTypes[c->type];
        int air = t->kind != VK_CAR;
        float sp = sqrtf(c->vel.x * c->vel.x + c->vel.z * c->vel.z + (air ? c->vy * c->vy : 0)) * 3.6f;
        float bx = g_winW - 250 * s, by = g_winH - 132 * s;
        ui_rect(bx - 10 * s, by - 10 * s, 250 * s, 132 * s, 0, 0, 0, 0.4f);
        snprintf(buf, sizeof buf, "%d", (int)sp);
        ui_text_r(bx + 150 * s, by, 5 * s, buf, 1, 1, 1, 1);
        ui_text(bx + 158 * s, by + 18 * s, 1.8f * s, "KM/H", 0.8f, 0.8f, 0.85f, 1);
        ui_text(bx, by + 46 * s, 1.3f * s, t->name, 0.9f, 0.85f, 0.6f, 1);
        float hf = c->health / c->maxHealth;
        hud_bar(bx, by + 62 * s, 220 * s, 8 * s, hf, hf > 0.35f ? 0.3f : 1, hf > 0.35f ? 0.9f : 0.3f, 0.3f);
        float ff = c->fuel;
        int low = ff < 0.15f && ((int)(menuT * 4) & 1);
        ui_text(bx, by + 76 * s, 1.1f * s, "FUEL", 0.85f, 0.85f, 0.6f, 1);
        hud_bar(bx + 40 * s, by + 76 * s, 180 * s, 7 * s, ff, low ? 1 : 0.95f, low ? 0.2f : 0.75f, 0.15f);
        if (air) {
            float gy = ground_height(c->pos.x, c->pos.z, c->pos.y + 0.1f, 0.3f);
            int mlv = t->missiles + g_prog.airUp[4];
            snprintf(buf, sizeof buf, "ALT %dM   THR %d%%   %s", (int)(c->pos.y - gy), (int)(c->throttle * 100),
                     mlv ? (c->misCD <= 0 ? "MISSILE READY" : "RELOADING") : "NO MISSILES");
            ui_text(bx, by + 92 * s, 1.2f * s, buf, 0.75f, 0.9f, 1, 1);
            if (t->kind == VK_PLANE && !c->onGround && c->aspd < t->maxSpeed * 0.4f && ((int)(menuT * 4) & 1))
                ui_text_c(g_winW * 0.5f, g_winH * 0.28f, 2.4f * s, "STALL - SPEED UP!", 1, 0.3f, 0.2f, 1);
        } else if (g_prog.upgrades & UP_NITRO) hud_bar(bx, by + 94 * s, 220 * s, 7 * s, g_pl.nitro, 0.3f, 0.6f, 1);
        /* aircraft gun sight */
        if (air && !c->wrecked) {
            V3 aimP = t->kind == VK_PLANE ? vadd(c->pos, vmul(air_fwd3(c), 120)) : vadd(g_camPos, vmul(cam_forward(), 120));
            V3 F = cam_forward(), v = vsub(aimP, g_camPos);
            float z = vdot(v, F);
            if (z > 1) {
                float fh = tanf(g_set.fov * g_fovMul * 0.5f / DEG);
                float sx = g_winW * 0.5f + vdot(v, g_camRight) / z / fh * g_winH * 0.5f;
                float sy = g_winH * 0.5f - vdot(v, g_camUp) / z / fh * g_winH * 0.5f;
                ui_frame_rect(sx - 12 * s, sy - 12 * s, 24 * s, 24 * s, 2 * s, 0.4f, 1, 0.4f, 0.85f);
                ui_rect(sx - 1.5f * s, sy - 1.5f * s, 3 * s, 3 * s, 0.4f, 1, 0.4f, 0.9f);
            }
        }
    } else if (!g_pl.dead) {
        /* weapon panel */
        int w = g_pl.weapon;
        float wx = 16 * s, wy = g_winH - 96 * s;
        ui_rect(wx - 6 * s, wy - 8 * s, 250 * s, 52 * s, 0, 0, 0, 0.45f);
        ui_text(wx, wy, 2 * s, g_weap[w].name, 1, 0.9f, 0.5f, 1);
        if (w == W_FISTS) ui_text(wx, wy + 22 * s, 1.3f * s, "KICK WITH FIRE OR KICK", 0.8f, 0.8f, 0.85f, 1);
        else {
            if (g_pl.reloadT > 0) snprintf(buf, sizeof buf, "RELOADING...");
            else snprintf(buf, sizeof buf, "%d / %d", g_pl.clip[w], g_prog.ammo[w] - g_pl.clip[w]);
            ui_text(wx, wy + 20 * s, 2 * s, buf, g_pl.clip[w] ? 1 : 1, g_pl.clip[w] ? 1 : 0.35f, g_pl.clip[w] ? 1 : 0.3f, 1);
            /* crosshair / scope */
            float cx = g_winW * 0.5f, cy = g_winH * 0.5f;
            int scoped = act_down(ACT_AIM) && weap_zoom(w) > 2.5f;
            if (scoped) {
                float r = g_winH * 0.42f, R = (float)(g_winW + g_winH);
                glColor4f(0, 0, 0, 0.92f);
                glBegin(GL_TRIANGLE_STRIP);
                for (int i = 0; i <= 48; i++) {
                    float an = 2 * PI * i / 48;
                    glVertex2f(cx + cosf(an) * r, cy + sinf(an) * r);
                    glVertex2f(cx + cosf(an) * R, cy + sinf(an) * R);
                }
                glEnd();
                ui_rect(cx - r, cy - 1 * s, 2 * r, 2 * s, 0, 0, 0, 0.9f);
                ui_rect(cx - 1 * s, cy - r, 2 * s, 2 * r, 0, 0, 0, 0.9f);
                ui_circle(cx, cy, 2.5f * s, 1, 0.2f, 0.2f, 1);
            } else {
                float gap = (6 + weap_spread(w) * 180 * (act_down(ACT_AIM) ? 0.5f : 1)) * s;
                ui_rect(cx - gap - 9 * s, cy - 1 * s, 9 * s, 2 * s, 1, 1, 1, 0.9f);
                ui_rect(cx + gap, cy - 1 * s, 9 * s, 2 * s, 1, 1, 1, 0.9f);
                ui_rect(cx - 1 * s, cy - gap - 9 * s, 2 * s, 9 * s, 1, 1, 1, 0.9f);
                ui_rect(cx - 1 * s, cy + gap, 2 * s, 9 * s, 1, 1, 1, 0.9f);
                ui_rect(cx - 1 * s, cy - 1 * s, 2 * s, 2 * s, 1, 0.3f, 0.3f, 0.9f);
            }
        }
    }
    if (g_pl.flash > 0) ui_rect(0, 0, (float)g_winW, (float)g_winH, 0.8f, 0.05f, 0.05f, clampf(g_pl.flash * 0.6f, 0, 0.35f));
    if (g_pl.dead) {
        dim(clampf(g_pl.deadT * 0.4f, 0, 0.6f));
        ui_text_c(g_winW * 0.5f, g_winH * 0.36f, 8 * s, g_pl.busted ? "BUSTED!" : "KNOCKED OUT!", g_pl.busted ? 0.35f : 1, g_pl.busted ? 0.55f : 0.25f, g_pl.busted ? 1 : 0.2f, 1);
    }
    if (g_prompt[0]) {
        float w = ui_text_w(g_prompt, 2 * s) + 40 * s;
        ui_rect(g_winW * 0.5f - w / 2, g_winH - 150 * s, w, 36 * s, 0, 0, 0, 0.6f);
        ui_text_c(g_winW * 0.5f, g_winH - 139 * s, 2 * s, g_prompt, 1, 1, 0.6f, 1);
    }
    if (msgT > 0) {
        msgT -= dt;
        float a = clampf(msgT * 2, 0, 1);
        float sc = 2.6f * s;
        float w = ui_text_w(msgBuf, sc);
        while (w > g_winW * 0.92f && sc > 1) { sc -= 0.2f * s; w = ui_text_w(msgBuf, sc); }
        ui_rect(g_winW * 0.5f - w / 2 - 20 * s, g_winH * 0.5f + 60 * s, w + 40 * s, sc * 7 + 24 * s, 0, 0, 0, 0.55f * a);
        ui_text_c(g_winW * 0.5f, g_winH * 0.5f + 72 * s, sc, msgBuf, 1, 0.9f, 0.3f, a);
    }
    for (int i = 0; i < 4; i++) {
        if (toastT[i] <= 0) continue;
        toastT[i] -= dt;
        float a = clampf(toastT[i] * 2, 0, 1);
        float y = g_winH - 200 * s - i * 28 * s;
        float w = ui_text_w(toasts[i], 1.6f * s);
        ui_rect(16 * s, y - 6 * s, w + 24 * s, 24 * s, 0, 0, 0, 0.5f * a);
        ui_text(28 * s, y, 1.6f * s, toasts[i], 1, 1, 1, a);
    }
    if (g_set.showFps) {
        snprintf(buf, sizeof buf, "%d FPS", (int)(fps + 0.5f));
        ui_text(16 * s, g_winH - 24 * s, 1.5f * s, buf, 0.7f, 1, 0.7f, 1);
    }
    ui_end();
}

/* --------------------------------------------------------------- pause */
static void game_backdrop(void) { sim_render(); }
static void draw_pause(void) {
    float s = S();
    game_backdrop();
    ui_begin();
    dim(0.55f);
    ui_text_c(g_winW * 0.5f, 60 * s, 6 * s, "PAUSED", 1, 0.85f, 0.2f, 1);
    const char *items[8];
    int ids[8], n = 0;
    items[n] = "RESUME"; ids[n++] = 0;
    items[n] = "MAP"; ids[n++] = 1;
    items[n] = "OPTIONS"; ids[n++] = 2;
    items[n] = "HOW TO PLAY"; ids[n++] = 7;
    if (mission_active()) { items[n] = "ABORT MISSION"; ids[n++] = 3; }
    items[n] = "SAVE GAME"; ids[n++] = 4;
    items[n] = "QUIT TO MAIN MENU"; ids[n++] = 5;
    items[n] = "QUIT GAME"; ids[n++] = 6;
    if (sel >= n) sel = 0;
    int c = menu_list(items, n, &sel, 160 * s, 2.8f * s);
    int gz = 0, ms = 0, cars = 0, outs = 0, ups = 0;
    for (int i = 0; i < NUM_GIZMOS; i++) gz += (g_prog.gizmos >> i) & 1;
    for (int i = 0; i < NUM_MISSIONS; i++) ms += (g_prog.missions >> i) & 1;
    for (int i = 1; i < NUM_SHOP_CARS; i++) cars += (g_prog.cars >> i) & 1;
    for (int i = 1; i < NUM_OUTFITS; i++) outs += (g_prog.outfits >> i) & 1;
    for (int i = 0; i < NUM_UPGRADES; i++) ups += (g_prog.upgrades >> i) & 1;
    int pct = (ms * 5 + gz * 2 + cars + outs + ups) * 100 / (NUM_MISSIONS * 5 + NUM_GIZMOS * 2 + (NUM_SHOP_CARS - 1) + (NUM_OUTFITS - 1) + NUM_UPGRADES);
    char buf[160];
    int pt = (int)g_prog.playTime;
    snprintf(buf, sizeof buf, "COMPLETION %d%%   -   COINS %d   -   PLAY TIME %d:%02d:%02d", pct, g_prog.coins, pt / 3600, (pt / 60) % 60, pt % 60);
    ui_text_c(g_winW * 0.5f, g_winH - 40 * s, 1.6f * s, buf, 0.85f, 0.85f, 0.95f, 1);
    ui_end();
    if (nav_back() || act_pressed(ACT_PAUSE)) { state = S_PLAY; return; }
    if (c < 0) return;
    switch (ids[c]) {
    case 0: state = S_PLAY; break;
    case 1: state = S_MAP; break;
    case 2: optReturn = S_PAUSE; state = S_OPTIONS; optSel = 0; break;
    case 3: mission_abort(); hud_msg(3, "MISSION ABORTED"); state = S_PLAY; break;
    case 4:
        if (save_game()) hud_toast("GAME SAVED");
        else hud_toast("COULD NOT SAVE!");
        state = S_PLAY;
        break;
    case 5: save_game(); mission_abort(); state = S_MENU; sel = 0; break;
    case 6: save_game(); g_quit = 1; break;
    case 7: howReturn = S_PAUSE; state = S_HOWTO; break;
    }
}

/* ------------------------------------------------------------- options */
static void draw_options(float dt) {
    float s = S();
    if (optReturn == S_MENU) menu_backdrop(dt);
    else game_backdrop();
    ui_begin();
    dim(0.6f);
    ui_text_c(g_winW * 0.5f, 50 * s, 5 * s, "OPTIONS", 1, 0.85f, 0.2f, 1);
    enum { N = 9 };
    const char *labels[N] = {"CONTROLS & KEY BINDINGS", "MOUSE SENSITIVITY", "INVERT MOUSE Y", "SOUND VOLUME", "MUSIC VOLUME", "FIELD OF VIEW", "FULLSCREEN", "SHOW FPS", "BACK"};
    char vals[N][32];
    memset(vals, 0, sizeof vals);
    snprintf(vals[1], 32, "%.1f", g_set.sens);
    snprintf(vals[2], 32, "%s", g_set.invertY ? "ON" : "OFF");
    snprintf(vals[3], 32, "%d", g_set.volume);
    snprintf(vals[4], 32, "%d", g_set.music);
    snprintf(vals[5], 32, "%d", g_set.fov);
    snprintf(vals[6], 32, "%s", g_set.fullscreen ? "ON" : "OFF");
    snprintf(vals[7], 32, "%s", g_set.showFps ? "ON" : "OFF");
    if (nav_up()) { optSel = (optSel + N - 1) % N; click(); }
    if (nav_down()) { optSel = (optSel + 1) % N; click(); }
    float pw = 760 * s, px = g_winW * 0.5f - pw / 2, y0 = 130 * s, lh = 50 * s;
    panel(px, y0 - 20 * s, pw, N * lh + 30 * s);
    int adj = 0, act = 0;
    for (int i = 0; i < N; i++) {
        float y = y0 + i * lh;
        int hov = mouse_in(px + 10 * s, y - 8 * s, pw - 20 * s, lh - 6 * s);
        if (hov && mouseMoved) optSel = i;
        if (hov && g_mouseClick) {
            optSel = i;
            if (vals[i][0]) adj = (g_mouseX > px + pw * 0.75f) ? 1 : (g_mouseX > px + pw * 0.55f ? -1 : 1);
            else act = 1;
        }
        int on = i == optSel;
        if (on) ui_rect(px + 10 * s, y - 8 * s, pw - 20 * s, lh - 6 * s, 1, 0.75f, 0.12f, 0.9f);
        float tc = on ? 0.08f : 1;
        ui_text(px + 30 * s, y + 4 * s, 2.4f * s, labels[i], tc, tc, tc, 1);
        if (vals[i][0]) {
            char v[96];
            snprintf(v, sizeof v, "<  %.31s  >", vals[i]);
            ui_text_r(px + pw - 30 * s, y + 4 * s, 2.4f * s, v, on ? 0.1f : 1, on ? 0.1f : 0.85f, on ? 0.1f : 0.3f, 1);
        }
    }
    ui_text_c(g_winW * 0.5f, y0 + N * lh + 30 * s, 1.5f * s, "UP/DOWN SELECT   LEFT/RIGHT CHANGE   ENTER CONFIRM   ESC BACK", 0.8f, 0.8f, 0.85f, 1);
    ui_end();
    if (nav_left()) adj = -1;
    if (nav_right()) adj = 1;
    if (nav_ok()) { if (vals[optSel][0]) adj = 1; else act = 1; }
    if (adj) {
        click();
        switch (optSel) {
        case 1: g_set.sens = clampf(roundf((g_set.sens + adj * 0.1f) * 10) / 10, 0.1f, 5.0f); break;
        case 2: g_set.invertY = !g_set.invertY; break;
        case 3: g_set.volume = (int)clampf((float)(g_set.volume + adj), 0, 10); audio_play(SND_COIN, 0.5f, 1); break;
        case 4: g_set.music = (int)clampf((float)(g_set.music + adj), 0, 10); break;
        case 5: g_set.fov = (int)clampf((float)(g_set.fov + adj * 5), 50, 110); break;
        case 6: g_set.fullscreen = !g_set.fullscreen; plat_set_fullscreen(g_set.fullscreen); break;
        case 7: g_set.showFps = !g_set.showFps; break;
        }
    }
    if (act) {
        click();
        if (optSel == 0) { ctlReturn = S_OPTIONS; state = S_CONTROLS; ctlRow = 0; ctlCol = 0; ctlWait = 0; }
        else if (optSel == N - 1) { config_save(); state = optReturn; }
    }
    if (nav_back()) { config_save(); state = optReturn; }
}

/* ------------------------------------------------------------ controls */
static int bind_conflict(int a, int slot) {
    int code = g_set.binds[a][slot];
    if (!code) return 0;
    for (int b = 0; b < ACT_COUNT; b++) {
        if (b == a) continue;
        if (!(g_actCtx[a] & g_actCtx[b])) continue;
        if (g_set.binds[b][0] == code || g_set.binds[b][1] == code) return 1;
    }
    return 0;
}
static void draw_controls(float dt) {
    float s = S();
    if (optReturn == S_MENU) menu_backdrop(dt);
    else game_backdrop();
    ui_begin();
    dim(0.7f);
    ui_text_c(g_winW * 0.5f, 22 * s, 3.6f * s, "CONTROLS & KEY BINDINGS", 1, 0.85f, 0.2f, 1);
    int rows = ACT_COUNT + 2;
    float pw = 980 * s, px = g_winW * 0.5f - pw / 2, y0 = 70 * s;
    float lh = fminf(28 * s, (g_winH - 165 * s) / rows);
    float c1 = px + pw * 0.46f, c2 = px + pw * 0.73f, cw = pw * 0.25f;
    panel(px, y0 - 10 * s, pw, rows * lh + 20 * s);
    ui_text(c1 + 10 * s, y0 - 2 * s, 1.4f * s, "PRIMARY", 0.7f, 0.8f, 1, 1);
    ui_text(c2 + 10 * s, y0 - 2 * s, 1.4f * s, "SECONDARY", 0.7f, 0.8f, 1, 1);
    float ty = y0 + 14 * s;
    int clicked = 0;
    for (int r = 0; r < rows; r++) {
        float y = ty + r * lh;
        int on = r == ctlRow;
        if (r < ACT_COUNT) {
            if (on) ui_rect(px + 8 * s, y - 3 * s, pw - 16 * s, lh - 2 * s, 1, 1, 1, 0.08f);
            static const char *cn[8] = {"", " (ON FOOT)", " (DRIVING)", " (FOOT/CAR)", " (FLYING)", " (FOOT/AIR)", " (VEHICLE)", ""};
            const char *ctx = cn[g_actCtx[r] & 7];
            char lab[320];
            snprintf(lab, sizeof lab, "%s%s", g_actNames[r], ctx);
            ui_text(px + 22 * s, y + 3 * s, 1.6f * s, lab, 1, 1, 1, 1);
            for (int col = 0; col < 2; col++) {
                float cx = col ? c2 : c1;
                int cs = on && col == ctlCol;
                int hov = mouse_in(cx, y - 3 * s, cw, lh - 2 * s);
                if (hov && mouseMoved && !ctlWait) { ctlRow = r; ctlCol = col; }
                if (hov && g_mouseClick && !ctlWait) { ctlRow = r; ctlCol = col; clicked = 1; }
                if (cs) ui_rect(cx, y - 3 * s, cw, lh - 2 * s, 1, 0.75f, 0.12f, ctlWait ? 0.5f + 0.4f * sinf(menuT * 10) : 0.9f);
                else ui_rect(cx, y - 3 * s, cw, lh - 2 * s, 0.15f, 0.18f, 0.3f, 0.8f);
                const char *kn = (cs && ctlWait) ? "PRESS A KEY..." : key_name(g_set.binds[r][col]);
                int conf = bind_conflict(r, col);
                float tr = cs ? 0.1f : (conf ? 1 : 1), tg = cs ? 0.1f : (conf ? 0.5f : 1), tb = cs ? 0.1f : (conf ? 0.2f : 1);
                ui_text(cx + 10 * s, y + 3 * s, 1.6f * s, kn, tr, tg, tb, 1);
            }
        } else {
            const char *lab = r == ACT_COUNT ? "RESET TO DEFAULTS" : "BACK";
            float w = 360 * s, x = g_winW * 0.5f - w / 2;
            int hov = mouse_in(x, y - 3 * s, w, lh - 2 * s);
            if (hov && mouseMoved && !ctlWait) ctlRow = r;
            if (hov && g_mouseClick && !ctlWait) { ctlRow = r; clicked = 1; }
            if (on) ui_rect(x, y - 3 * s, w, lh - 2 * s, 1, 0.75f, 0.12f, 0.9f);
            ui_text_c(g_winW * 0.5f, y + 3 * s, 1.8f * s, lab, on ? 0.1f : 1, on ? 0.1f : 1, on ? 0.1f : 1, 1);
        }
    }
    const char *help = ctlWait ? "PRESS THE NEW KEY OR MOUSE BUTTON   -   ESC TO CANCEL"
                               : "ENTER/CLICK: REBIND   DELETE: CLEAR   ARROWS: MOVE   ESC: BACK   (ORANGE = CONFLICT)  ESC ALWAYS PAUSES";
    ui_text_c(g_winW * 0.5f, ty + rows * lh + 16 * s, 1.35f * s, help, 0.85f, 0.85f, 0.9f, 1);
    ui_end();

    if (ctlWait) {
        if (ctlWaitFrames > 0) { ctlWaitFrames--; return; }
        if (g_lastKey) {
            int k = g_lastKey;
            if (k == XK_Escape) { ctlWait = 0; click(); return; }
            if (k == XK_F11 || k == XK_F12) return;
            g_set.binds[ctlRow][ctlCol] = k;
            if (g_set.binds[ctlRow][1 - ctlCol] == k) g_set.binds[ctlRow][1 - ctlCol] = 0;
            ctlWait = 0;
            audio_play(SND_CHECK, 0.5f, 1);
        }
        return;
    }
    if (key_pressed(XK_Up)) { ctlRow = (ctlRow + rows - 1) % rows; click(); }
    if (key_pressed(XK_Down)) { ctlRow = (ctlRow + 1) % rows; click(); }
    if (g_wheel > 0) ctlRow = (ctlRow + rows - 1) % rows;
    if (g_wheel < 0) ctlRow = (ctlRow + 1) % rows;
    if (key_pressed(XK_Left) || key_pressed(XK_Right)) { ctlCol = 1 - ctlCol; click(); }
    if ((key_pressed(XK_Delete) || key_pressed(XK_BackSpace)) && ctlRow < ACT_COUNT) { g_set.binds[ctlRow][ctlCol] = 0; click(); }
    if (key_pressed(XK_Return) || key_pressed(XK_KP_Enter) || clicked) {
        click();
        if (ctlRow < ACT_COUNT) { ctlWait = 1; ctlWaitFrames = 1; }
        else if (ctlRow == ACT_COUNT) { settings_default_binds(); hud_toast("CONTROLS RESET TO DEFAULTS"); }
        else { config_save(); state = ctlReturn; }
    }
    if (nav_back()) { config_save(); state = ctlReturn; }
}

/* ---------------------------------------------------------------- shop */
static void preview3d(float x, float y, float w, float h, const ShopItem *it) {
    glViewport((int)x, (int)(g_winH - (y + h)), (int)w, (int)h);
    glEnable(GL_SCISSOR_TEST);
    glScissor((int)x, (int)(g_winH - (y + h)), (int)w, (int)h);
    glClear(GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    set_perspective(32, w / h, 0.3f, 100);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    V3 sr = g_camRight, su = g_camUp;
    int k = it->preview;
    if (k == 1) look_at(v3(0, 3.2f, 10.5f), v3(0, 0.8f, 0));
    else if (k == 3) look_at(v3(0, 7.5f, 21), v3(0, 1.4f, 0));
    else if (k == 4) look_at(v3(0, 0.5f, 3.0f), v3(0, 0, 0));
    else look_at(v3(0, 1.7f, 5.2f), v3(0, 1.0f, 0));
    setup_light();
    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_FOG);
    glDisable(GL_BLEND);
    glRotatef(menuT * 40, 0, 1, 0);
    glColor3f(0.3f, 0.3f, 0.35f);
    if (k == 1) {
        gl_frustum_y(0, -0.1f, 0, 3.6f, 3.6f, 0.1f, 24);
        draw_car_body(it->pidx, it->pcol, menuT * 4, 0, 0);
    } else if (k == 3) {
        gl_frustum_y(0, -0.1f, 0, 7, 7, 0.1f, 24);
        draw_aircraft_body(it->pidx, it->pcol, menuT * 20, 0, 1);
    } else if (k == 4) {
        glScalef(2.2f, 2.2f, 2.2f);
        glRotatef(90, 0, 1, 0);
        glTranslatef(0, 0, -0.2f);
        draw_weapon_model(it->pidx);
    } else {
        gl_frustum_y(0, -0.1f, 0, 1.2f, 1.2f, 0.1f, 20);
        draw_character(v3(0, 0, 0), 0, menuT * 6, 0, &g_looks[it->pidx], 0, 0);
    }
    glDisable(GL_LIGHTING);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, g_winW, g_winH);
    g_camRight = sr;
    g_camUp = su;
    ui_begin();
}
static void stat_bar(float x, float y, float w, const char *n, float f, float s) {
    ui_text(x, y, 1.4f * s, n, 0.85f, 0.85f, 0.9f, 1);
    ui_rect(x + 120 * s, y, w - 120 * s, 10 * s, 0.15f, 0.15f, 0.2f, 1);
    ui_rect(x + 120 * s, y, (w - 120 * s) * clampf(f, 0, 1), 10 * s, 1, 0.75f, 0.15f, 1);
}
static void draw_shop(float dt) {
    float s = S();
    menuT += dt;
    game_backdrop();
    ui_begin();
    dim(0.5f);
    float pw = 1040 * s, ph = 620 * s, px = g_winW * 0.5f - pw / 2, py = g_winH * 0.5f - ph / 2;
    panel(px, py, pw, ph);
    ui_text(px + 26 * s, py + 20 * s, 3.4f * s, shop_title(shopId), 1, 0.85f, 0.2f, 1);
    char buf[160];
    snprintf(buf, sizeof buf, "COINS: %d", g_prog.coins);
    ui_text_r(px + pw - 26 * s, py + 26 * s, 2.4f * s, buf, 1, 0.9f, 0.3f, 1);
    const char *tabs[8];
    int nt = shop_tabs(shopId, tabs);
    if (shopTab >= nt) shopTab = 0;
    if (nav_left() || key_pressed(XK_q)) { shopTab = (shopTab + nt - 1) % nt; shopSel = 0; click(); }
    if (nav_right() || key_pressed(XK_e) || key_pressed(XK_Tab)) { shopTab = (shopTab + 1) % nt; shopSel = 0; click(); }
    float tw = fminf(170 * s, (pw - 52 * s) / nt - 8 * s);
    for (int t = 0; t < nt; t++) {
        float tx = px + 26 * s + t * (tw + 8 * s), ty = py + 64 * s;
        int hov = mouse_in(tx, ty, tw, 34 * s);
        if (hov && g_mouseClick) { shopTab = t; shopSel = 0; click(); }
        int on = t == shopTab;
        ui_rect(tx, ty, tw, 34 * s, on ? 1 : 0.15f, on ? 0.75f : 0.18f, on ? 0.12f : 0.3f, 0.95f);
        float sc = fminf(2 * s, (tw - 10 * s) / (ui_text_w(tabs[t], 1) + 1));
        ui_text_c(tx + tw / 2, ty + 17 * s - sc * 3.5f, sc, tabs[t], on ? 0.1f : 1, on ? 0.1f : 1, on ? 0.1f : 1, 1);
    }
    ShopItem items[16];
    int n = shop_items(shopId, shopTab, items, 16);
    if (shopSel >= n) shopSel = n - 1;
    if (shopSel < 0) shopSel = 0;
    if (nav_up()) { shopSel = (shopSel + n - 1) % n; click(); }
    if (nav_down()) { shopSel = (shopSel + 1) % n; click(); }
    float lx = px + 26 * s, ly = py + 116 * s, lw = 460 * s, lh = 52 * s;
    int vis = (int)((ph - 170 * s) / lh);
    int first = shopSel - vis + 1;
    if (first < 0) first = 0;
    int buy = 0;
    for (int i = first; i < n && i < first + vis; i++) {
        float y = ly + (i - first) * lh;
        ShopItem *it = &items[i];
        int hov = mouse_in(lx, y, lw, lh - 6 * s);
        if (hov && mouseMoved) shopSel = i;
        if (hov && g_mouseClick) { if (shopSel == i) buy = 1; shopSel = i; }
        int on = i == shopSel;
        ui_rect(lx, y, lw, lh - 6 * s, on ? 1 : 0.12f, on ? 0.75f : 0.14f, on ? 0.12f : 0.25f, 0.92f);
        float tc = on ? 0.08f : 1;
        ui_text(lx + 14 * s, y + 7 * s, 1.9f * s, it->name, tc, tc, tc, 1);
        int afford = it->status != 0 || g_prog.coins >= it->price;
        float r = 1, g = 0.9f, b = 0.4f;
        if (it->status == 2 || it->status == 1 || it->status == 3) { r = 0.4f; g = 1; b = 0.4f; }
        else if (it->status == 4 || !afford) { r = 1; g = 0.4f; b = 0.4f; }
        if (on) r = g = b = 0.2f;
        ui_text(lx + 14 * s, y + 27 * s, 1.35f * s, it->info, r, g, b, 1);
    }
    if (first > 0) ui_text_c(lx + lw / 2, ly - 12 * s, 1.4f * s, "^", 1, 1, 1, 0.8f);
    if (first + vis < n) ui_text_c(lx + lw / 2, ly + vis * lh - 4 * s, 1.4f * s, "v", 1, 1, 1, 0.8f);
    float rx = px + 520 * s, ry = py + 116 * s, rw = 490 * s;
    ui_rect(rx, ry, rw, 270 * s, 0.03f, 0.04f, 0.1f, 0.9f);
    if (n > 0) {
        ShopItem *it = &items[shopSel];
        if (it->icon && !it->preview) ui_text_c(rx + rw / 2, ry + 105 * s, 6 * s, it->icon, 1, 0.8f, 0.2f, 1);
        else preview3d(rx, ry, rw, 270 * s, it);
        float yy = ry + 290 * s;
        for (int k = 0; k < it->nstats; k++) { stat_bar(rx + 10 * s, yy, rw - 20 * s, it->statName[k], it->stat[k], s); yy += 24 * s; }
        if (it->desc) ui_text_wrap(rx + 10 * s, yy + 6 * s, rw - 20 * s, 1.5f * s, it->desc, 0.85f, 0.85f, 0.9f, 1);
    }
    ui_text_c(g_winW * 0.5f, py + ph - 30 * s, 1.5f * s, "UP/DOWN SELECT   LEFT/RIGHT CATEGORY   ENTER BUY/EQUIP   ESC LEAVE", 0.8f, 0.8f, 0.85f, 1);
    ui_end();
    if (nav_ok()) buy = 1;
    if (buy && n > 0) shop_buy(shopId, shopTab, shopSel);
    if (nav_back()) { state = S_PLAY; save_game(); }
}

/* -------------------------------------------------------------- dialog */
static void draw_dialog(void) {
    float s = S();
    game_backdrop();
    ui_begin();
    dim(0.4f);
    int m = dlgMission;
    float pw = 860 * s, ph = 400 * s, px = g_winW * 0.5f - pw / 2, py = g_winH - ph - 40 * s;
    panel(px, py, pw, ph);
    ui_text(px + 30 * s, py + 24 * s, 2.2f * s, mission_giver(m), 0.6f, 0.85f, 1, 1);
    int done = (g_prog.missions >> m) & 1;
    char buf[200];
    snprintf(buf, sizeof buf, "MISSION %d: %s%s", m + 1, mission_name(m), done ? "  (REPLAY)" : "");
    ui_text(px + 30 * s, py + 56 * s, 3 * s, buf, 1, 0.85f, 0.2f, 1);
    ui_text_wrap(px + 30 * s, py + 110 * s, pw - 60 * s, 1.9f * s, mission_brief(m), 1, 1, 1, 1);
    snprintf(buf, sizeof buf, "REWARD: %d COINS     TIME LIMIT: %d SECONDS%s", mission_reward(m), (int)mission_time(m),
             mission_is_needcar(m) ? "     A CAR WILL BE PROVIDED" : "");
    ui_text(px + 30 * s, py + ph - 90 * s, 1.6f * s, buf, 0.6f, 1, 0.6f, 1);
    snprintf(buf, sizeof buf, "[ENTER] OR [%s] ACCEPT          [ESC] NOT NOW", act_key(ACT_INTERACT));
    ui_text_c(g_winW * 0.5f, py + ph - 48 * s, 2 * s, buf, 1, 1, 0.6f, 1);
    ui_end();
    if (key_pressed(XK_Return) || key_pressed(XK_KP_Enter) || act_pressed(ACT_INTERACT) || g_mouseClick) {
        state = S_PLAY;
        mission_start(m);
    } else if (nav_back()) state = S_PLAY;
}

/* ------------------------------------------------------------- how to */
static void draw_howto(float dt) {
    float s = S();
    if (howReturn == S_MENU) menu_backdrop(dt);
    else game_backdrop();
    ui_begin();
    dim(0.75f);
    float pw = 1080 * s, px = g_winW * 0.5f - pw / 2, py = 30 * s;
    panel(px, py, pw, g_winH - 60 * s);
    ui_text_c(g_winW * 0.5f, py + 20 * s, 4 * s, "HOW TO PLAY", 1, 0.85f, 0.2f, 1);
    char t[3600];
    snprintf(t, sizeof t,
             "EXPLORE MAPLE BAY ON FOOT, BY CAR OR BY AIR. HELP THE LOCALS WITH MISSIONS, FIGHT THE STREET GANGS AND SPEND YOUR COINS IN THE SHOPS.\n\n"
             "ON FOOT:  MOVE [%s %s %s %s]  JUMP [%s]  SPRINT [%s]  KICK [%s]  FIRE [%s]  AIM [%s]  RELOAD [%s]  WEAPONS [%s]/[%s]\n"
             "DRIVING:  ACCELERATE [%s]  BRAKE [%s]  STEER [%s %s]  HANDBRAKE [%s]  HORN [%s]  RESET [%s]\n"
             "FLYING:  THROTTLE [%s]/[%s]  TURN [%s %s]  CLIMB/PULL UP [%s]  DIVE/DESCEND [%s]  GUNS [%s]  MISSILES [%s]\n"
             "ANYWHERE: ENTER/EXIT & TALK [%s]  CALL YOUR CAR [%s]  MAP [%s]  CAMERA: MOUSE OR [%s %s %s %s]\n\n"
             "- PEOPLE WITH A YELLOW ! HAVE MISSIONS. NEW STORY LINES START AT THE AIRPORT AND AT THE ARMORY.\n"
             "- PLANES NEED SPEED: FULL THROTTLE DOWN THE RUNWAY, THEN PULL UP. HELICOPTERS CLIMB STRAIGHT UP. BAIL OUT IN THE AIR AND PRESS JUMP FOR A PARACHUTE.\n"
             "- VEHICLES BURN FUEL. STOP AT A GAS STATION (F ON THE MAP) TO REFUEL FOR A FEW COINS.\n"
             "- THE RUST RATS AND NEON VIPERS FIGHT OVER THEIR TURF. TAKE DOWN 8 MEMBERS IN A TURF BLOCK TO CLAIM IT - CLAIMED TURF PAYS YOU EVERY MINUTE.\n"
             "- GUNFIRE BRINGS THE POLICE. HURT PEOPLE OR PROPERTY IN FRONT OF THEM AND YOU BECOME WANTED. RUN FAR AWAY OR LAY LOW TO LOSE THEM - IF THEY CATCH YOU, YOU ARE BUSTED!\n"
             "- IF YOUR HEALTH RUNS OUT YOU WAKE UP AT MAPLE MEDICAL (A SMALL BILL APPLIES). THE CLINIC CAN ALSO PATCH YOU UP.\n"
             "- THREE ISLANDS: BOOK A FLIGHT AT THE AIRPORT FLIGHTS DESK, OR FLY YOUR OWN PLANE OUT OVER THE OPEN SEA TO PICK A DESTINATION.\n"
             "- MAP: CLICK TO DROP A WAYPOINT AND FOLLOW THE BLUE GPS LINE. KEYS 1-6 SET QUICK WAYPOINTS (GAS, HOSPITAL, SHOP, FLIGHTS, MISSION, CAR).\n"
             "- SHOPS: BOLT & BARGAIN ($), BOOM BROS ARMORY (G), LOCK & LOAD WEAPON STORE (W - BUY, SELL, UPGRADE), SKYLINE HANGAR (A), TURBO TUNERS (T).",
             act_key(ACT_FORWARD), act_key(ACT_LEFT), act_key(ACT_BACK), act_key(ACT_RIGHT), act_key(ACT_JUMP), act_key(ACT_SPRINT), act_key(ACT_KICK),
             act_key(ACT_FIRE), act_key(ACT_AIM), act_key(ACT_RELOAD), act_key(ACT_NEXTWEAP), act_key(ACT_PREVWEAP),
             act_key(ACT_FORWARD), act_key(ACT_BACK), act_key(ACT_LEFT), act_key(ACT_RIGHT), act_key(ACT_HANDBRAKE), act_key(ACT_HORN), act_key(ACT_RESETCAR),
             act_key(ACT_FORWARD), act_key(ACT_BACK), act_key(ACT_LEFT), act_key(ACT_RIGHT), act_key(ACT_ASCEND), act_key(ACT_DESCEND), act_key(ACT_FIRE), act_key(ACT_MISSILE),
             act_key(ACT_INTERACT), act_key(ACT_CALLCAR), act_key(ACT_MAP), act_key(ACT_CAM_LEFT), act_key(ACT_CAM_RIGHT), act_key(ACT_CAM_UP), act_key(ACT_CAM_DOWN));
    ui_text_wrap(px + 30 * s, py + 76 * s, pw - 60 * s, 1.45f * s, t, 1, 1, 1, 1);
    ui_text_c(g_winW * 0.5f, g_winH - 60 * s, 1.6f * s, "PRESS ESC OR ENTER TO GO BACK", 0.8f, 0.8f, 0.85f, 1);
    ui_end();
    if (nav_back() || nav_ok() || g_mouseClick) state = howReturn;
}

/* ---------------------------------------------------------------- frame */
static const char *scenario;

/* ------------------------------------------------------- test harness */
/* Scenarios named test_* drive the game with injected input and print PASS/FAIL. */
void plat_inject(int code, int down);
static int tframe, tdone;
static float tv[8];
static void tresult(const char *name, int ok, const char *fmt, ...) {
    char b[200];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    if (!tdone) fprintf(stderr, "TEST %-12s %s  %s\n", name, ok ? "PASS" : "FAIL", b);
    tdone = 1;
    g_quit = 1;
}
static void tap(int code) { plat_inject(code, 1); }
/* steer the camera until the crosshair is on a world point; returns 1 when lined up */
static int aim_at(V3 t) {
    V3 f = cam_forward();
    V3 d = vnorm(vsub(t, g_camPos));
    float ye = wrap_pi(atan2f(d.x, d.z) - atan2f(f.x, f.z));
    float pe = asinf(clampf(d.y, -1, 1)) - asinf(clampf(f.y, -1, 1));
    g_camYaw += ye * 0.7f;
    g_camPitch -= pe * 0.7f;
    return fabsf(ye) < 0.012f && fabsf(pe) < 0.012f;
}
static void untap(int code) { plat_inject(code, 0); }
static void test_tick(void) {
    if (!scenario || strncmp(scenario, "test_", 5)) return;
    const char *t = scenario + 5;
    int f = tframe++;
    if (t[0] == 'm' && f % 20 == 2) untap(XK_f);
    if (!strcmp(t, "drive")) {
        Car *c = player_car();
        if (f == 1) { tv[0] = c ? c->pos.x : 0; tap(XK_w); }
        if (f == 150) { tv[1] = c->pos.x; tv[2] = c->yaw; tap(XK_d); }
        if (f == 200) { untap(XK_d); untap(XK_w); tv[3] = c->yaw; }
        if (f == 210) tresult("drive", c && tv[1] - tv[0] > 20 && wrap_pi(tv[3] - tv[2]) < -0.2f, "moved %.1fm along +X, yaw %.2f -> %.2f (right turn = decreasing)", tv[1] - tv[0], tv[2], tv[3]);
    } else if (!strcmp(t, "jump")) {
        if (f == 5) tap(XK_space);
        if (f == 6) untap(XK_space);
        if (f == 20) tv[0] = g_pl.pos.y;
        if (f == 90) tresult("jump", tv[0] > 1.0f && g_pl.onGround && g_pl.pos.y < 0.05f, "height after 14 frames %.2f, landed=%d", tv[0], g_pl.onGround);
    } else if (!strcmp(t, "enter")) {
        if (f == 2) { g_pl.pos = vadd(g_spots.homeCar, v3(0, 0, -2.6f)); }
        if (f == 4) tap(XK_e);
        if (f == 5) untap(XK_e);
        if (f == 10) tv[0] = g_pl.car;
        if (f == 12) tap(XK_e);
        if (f == 13) untap(XK_e);
        if (f == 20) tresult("enter/exit", tv[0] >= 0 && g_pl.car < 0, "car idx while inside %d, after exit %d", (int)tv[0], g_pl.car);
    } else if (!strcmp(t, "kick")) {
        int ci = -1;
        for (int i = 0; i < g_ncrates; i++) if (g_crates[i].alive && g_crates[i].pos.y < 0.01f) { ci = i; break; }
        if (f == 2) { tv[0] = (float)ci; V3 cp = g_crates[ci].pos; g_pl.pos = v3(cp.x - 1.5f, 0, cp.z); g_pl.yaw = PI / 2; g_pl.vel = v3(0, 0, 0); }
        if (f == 4) tap(XK_f);
        if (f == 5) untap(XK_f);
        if (f == 8) tv[1] = g_prog.coins;
        if (f == 60) tresult("kick crate", !g_crates[(int)tv[0]].alive, "crate %d broken=%d coins %d -> %d", (int)tv[0], !g_crates[(int)tv[0]].alive, (int)tv[1], g_prog.coins);
    } else if (!strcmp(t, "ramp")) {
        Car *c = player_car();
        if (f == 1) { c->pos = v3(8 + 4 - 12, 0, 8 + 10); c->yaw = PI / 2; c->vel = v3(30, 0, 0); tap(XK_w); }
        if (c && c->pos.y > tv[0]) tv[0] = c->pos.y;
        if (f == 150) { untap(XK_w); tresult("ramp jump", tv[0] > 4.6f && c->onGround, "peak height %.2f, gizmos=%u, landed=%d", tv[0], g_prog.gizmos, c->onGround); }
    } else if (!strcmp(t, "shop")) {
        if (f == 3) tap(XK_Down);
        if (f == 4) untap(XK_Down);
        if (f == 6) tap(XK_Return);
        if (f == 7) untap(XK_Return);
        if (f == 10) tresult("shop buy", (g_prog.cars & 2) && g_prog.coins == 4600 && g_prog.curCar == 1, "cars mask %u coins %d curCar %d", g_prog.cars, g_prog.coins, g_prog.curCar);
    } else if (!strcmp(t, "rebind")) {
        if (f == 3) tap(XK_Return);
        if (f == 4) untap(XK_Return);
        if (f == 8) tap(XK_x);
        if (f == 9) untap(XK_x);
        if (f == 12) tresult("rebind", g_set.binds[ACT_FORWARD][0] == XK_x, "forward primary = %s", key_name(g_set.binds[ACT_FORWARD][0]));
    } else if (!strcmp(t, "climb")) {
        /* hop from each floating platform to the next using real input */
        int k = f / 60, ph = f % 60;
        if (k > 19) return;
        V3 tc = v3(g_spots.towerTop.x, 0, g_spots.towerTop.z);
        V3 cur = vadd(tc, v3(cosf(k * 0.55f) * 8.5f, 1.4f * (k + 1), sinf(k * 0.55f) * 8.5f));
        V3 nxt = k < 18 ? vadd(tc, v3(cosf((k + 1) * 0.55f) * 8.5f, 1.4f * (k + 2), sinf((k + 1) * 0.55f) * 8.5f)) : g_spots.towerTop;
        V3 d = vnorm(v3(nxt.x - cur.x, 0, nxt.z - cur.z));
        if (k == 19) { tresult("tower climb", (int)tv[0] == 19, "%d of 19 jumps landed", (int)tv[0]); return; }
        if (ph == 0) { g_pl.pos = vadd(cur, vmul(d, k == 18 ? 1.2f : 0.0f)); g_pl.vel = v3(0, 0, 0); g_pl.onGround = 1; g_camYaw = atan2f(d.x, d.z); }
        if (ph == 2) { tap(XK_w); if (k == 18) tap(XK_Shift_L); tap(XK_space); }
        if (ph == 3) untap(XK_space);
        if (ph == 30) { untap(XK_w); untap(XK_Shift_L); }
        if (ph == 58) {
            int ok = g_pl.onGround && fabsf(g_pl.pos.y - nxt.y) < 0.15f;
            if (ok) tv[0] += 1;
            else fprintf(stderr, "  climb step %d failed: y=%.2f want %.2f\n", k, g_pl.pos.y, nxt.y);
        }
    } else if (!strcmp(t, "fly")) {
        Car *c = player_car();
        if (!c) { tresult("fly", 0, "not in a plane"); return; }
        if (f == 2) tap(XK_w);
        if (c->aspd > tv[1]) tv[1] = c->aspd;
        if (f > 2 && c->aspd > g_carTypes[c->type].maxSpeed * 0.5f && !tv[2]) { tv[2] = f; tap(XK_space); }
        if (tv[2] && f == (int)tv[2] + 90) untap(XK_space);
        if (c->pos.y > tv[0]) tv[0] = c->pos.y;
        if (f == 900) { untap(XK_w); tresult("plane flight", tv[0] > 15 && !c->wrecked && c->pos.y > 8, "top speed %.1f, rotate at frame %d, peak alt %.1f, alt now %.1f, wrecked %d", tv[1], (int)tv[2], tv[0], c->pos.y, c->wrecked); }
    } else if (!strcmp(t, "heli")) {
        Car *c = player_car();
        if (!c) { tresult("heli", 0, "not in a helicopter"); return; }
        if (f == 2) { tv[0] = c->pos.y; tap(XK_space); }
        if (f == 200) untap(XK_space);
        if (f == 280) tv[1] = c->pos.y;
        if (f == 400) { tv[2] = c->pos.y; tap(XK_Control_L); }
        if (f == 900) { untap(XK_Control_L); tresult("helicopter", tv[1] - tv[0] > 8 && fabsf(tv[2] - tv[1]) < 3 && c->onGround && !c->wrecked,
                                                     "start %.1f, after climb %.1f, after hover %.1f, landed %d wrecked %d", tv[0], tv[1], tv[2], c->onGround, c->wrecked); }
    } else if (!strcmp(t, "shoot")) {
        int found;
        V3 tp = thug_nearest(g_pl.pos, GANG_RATS, &found);
        if (f == 2) { g_godMode = 1; give_weapon(W_PISTOL, 60); g_pl.weapon = W_PISTOL; V3 sp = v3(road_c(5 + BO), 0, road_c(1 + BO) + 25); spawn_thug(GANG_RATS, vadd(sp, v3(12, 0, 0)), 3); g_pl.pos = sp; }
        if (f < 10) return;
        int kills = 0;
        for (int k = 0; k < NUM_TURF; k++) kills += g_turfProg[k];
        if (kills > 0) { tresult("shoot gang", 1, "gang member down after %d shots (turf progress %d)", (int)tv[0], kills); return; }
        if (!found) return;
        int ok = aim_at(vadd(tp, v3(0, 1.2f, 0)));
        if (ok && f % 10 == 0) { tap(XK_v); tv[0] += 1; }
        if (f % 10 == 5) untap(XK_v);
        if (f > 1100) tresult("shoot gang", 0, "target still standing after %d shots", (int)tv[0]);
    } else if (!strcmp(t, "wanted")) {
        if (f == 2) { int ci = spawn_car(CAR_POLICE, vadd(g_pl.pos, v3(18, 0, 0)), 0, AI_POLICE); g_cars[ci].crew = 0; give_weapon(W_PISTOL, 40); g_pl.weapon = W_PISTOL; }
        if (f >= 10 && f < 40 && f % 6 == 0) tap(XK_v);
        if (f % 6 == 3) untap(XK_v);
        if (f == 60) { tv[0] = wanted_stars(); g_godMode = 1; g_pl.pos = v3(0, 0, AZ(450)); }
        if (f > 60 && g_pl.dead) { tresult("wanted", 0, "busted while hiding"); return; }
        if (f > 60 && wanted_stars() == 0 && !tv[1]) tv[1] = f;
        if (f == 1500) tresult("wanted", tv[0] >= 1 && tv[1] > 0, "stars after shooting near police %d, cleared at frame %d", (int)tv[0], (int)tv[1]);
    } else if (!strcmp(t, "chase")) {
        if (f == 2) { wanted_crime(3.0f, g_pl.pos); tv[0] = 1e9f; g_pl.pos = v3(road_c(3 + BO) + LANE, 0, road_c(5 + BO)); }
        if (f > 2 && wanted_stars() < 3) wanted_crime(3.0f, g_pl.pos);
        for (int i = 0; i < MAX_CARS; i++)
            if (g_cars[i].used && g_cars[i].ai == AI_POLICE) { float d = distxz(g_cars[i].pos, g_pl.pos); if (d < tv[0]) tv[0] = d; }
        if (g_pl.dead) tv[1] = 1;
        if (f == 2400) tresult("police chase", tv[0] < 30, "closest police car %.1fm, player dealt with %d", tv[0], (int)tv[1]);
    } else if (!strcmp(t, "copsgang")) {
        /* police respond to a gang member without the player being involved */
        V3 base = v3(road_c(2 + BO), 0, road_c(6 + BO) + 20);
        if (f == 2) { g_godMode = 1; g_pl.pos = vadd(base, v3(0, 0, 45)); spawn_thug(GANG_RATS, vadd(base, v3(0, 0, -6)), -1); spawn_thug(GANG_POLICE, vadd(base, v3(0, 0, 10)), -1); }
        int found;
        thug_nearest(base, GANG_RATS, &found);
        if (f > 5 && !found) { tresult("police vs gang", 1, "police took down the gang member at frame %d", f); return; }
        if (f == 1200) tresult("police vs gang", 0, "gang member still standing");
    } else if (!strcmp(t, "travel")) {
        /* book a flight at the desk, then fly home in a plane over the open sea */
        if (f == 2) { g_pl.pos = g_spots.travel; tv[0] = g_prog.coins; }
        if (f == 4) tap(XK_e);
        if (f == 5) untap(XK_e);
        if (f == 8) { tv[1] = state == S_TRAVEL; tap(XK_Return); }
        if (f == 9) untap(XK_Return);
        if (f == 300) {
            tv[2] = g_curMap;
            tv[3] = distxz(g_pl.pos, g_spots.travel) < 8;
            for (int i = 0; i < MAX_CARS; i++) if (g_cars[i].used && g_cars[i].stand == 0) {
                Car *c = &g_cars[i];
                put_player_in_car(i);
                c->pos = v3(0, 80, -CITY_HALF - 300); c->yaw = PI; c->onGround = 0; c->aspd = 40; c->throttle = 1;
                break;
            }
        }
        if (f > 300 && f < 900 && state == S_TRAVEL && !tv[4]) { tv[4] = f; travelSel = 0; tap(XK_Return); }
        if (tv[4] && f == (int)tv[4] + 1) untap(XK_Return);
        if (f == 900) {
            Car *c = player_car();
            tresult("travel", tv[1] && tv[2] == 1 && tv[3] && g_prog.coins == (int)tv[0] - TICKET_PRICE && g_curMap == 0 && c && is_aircraft(c) && !c->onGround,
                    "menu %d, airline to map %d at desk %d, coins %d -> %d, flew back to map %d in the air %d", (int)tv[1], (int)tv[2], (int)tv[3], (int)tv[0], g_prog.coins, g_curMap, c && !c->onGround);
        }
    } else if (!strcmp(t, "gunx")) {
        if (f == 2) ui_open_shop(SHOP_GUNX);
        if (f == 3) tv[0] = g_prog.coins;
        if (f == 4) { shopSel = W_ASSAULT - 1; tap(XK_Return); }
        if (f == 5) untap(XK_Return);
        if (f == 8) { tv[1] = (g_prog.weapons >> W_ASSAULT) & 1; tv[2] = g_prog.coins; tap(XK_Right); }
        if (f == 9) untap(XK_Right);
        if (f == 11) { shopSel = W_ASSAULT - 1; tap(XK_Return); }
        if (f == 12) untap(XK_Return);
        if (f == 16) tresult("weapon store", tv[1] && !((g_prog.weapons >> W_ASSAULT) & 1) && tv[2] == tv[0] - 2200 && g_prog.coins > tv[2],
                             "bought %d (coins %d -> %d), sold it back for %d", (int)tv[1], (int)tv[0], (int)tv[2], g_prog.coins - (int)tv[2]);
    } else if (!strcmp(t, "grenade")) {
        int ci = -1;
        for (int i = 0; i < MAX_CARS; i++) if (g_cars[i].used && g_cars[i].ai == AI_PARKED && !is_aircraft(&g_cars[i]) && !g_cars[i].called) { ci = i; break; }
        if (ci < 0) { tresult("grenade", 0, "no parked car"); return; }
        Car *c = &g_cars[ci];
        if (f == 2) { g_godMode = 1; give_weapon(W_GRENADE, 20); g_pl.weapon = W_GRENADE; tv[0] = ci; tv[1] = c->health;
                      g_pl.pos = vsub(c->pos, vmul(fwd_of(c->yaw), 15)); g_pl.pos.y = 0; }
        c = &g_cars[(int)tv[0]];
        int ok = aim_at(vadd(c->pos, v3(0, 0.5f, 0)));
        if (ok && f % 30 == 0) tap(XK_v);
        if (f % 30 == 5) untap(XK_v);
        if (getenv("MB_DEBUG") && f % 30 == 0) fprintf(stderr, "car %.1f %.1f pl %.1f %.1f ok %d\n", c->pos.x, c->pos.z, g_pl.pos.x, g_pl.pos.z, ok);
        if (f == 400) tresult("grenade", c->health < tv[1], "car health %.0f -> %.0f", tv[1], c->health);
    } else if (!strcmp(t, "waypoint")) {
        if (f == 2) state = S_MAP;
        if (f == 4) tap(XK_1);
        if (f == 5) untap(XK_1);
        if (f == 8) {
            V3 r[64];
            tv[0] = g_wpOn;
            tv[1] = road_route(g_pl.pos, g_wpPos, r, 64);
            state = S_PLAY;
            g_pl.pos = g_wpPos;
        }
        if (f == 20) tresult("waypoint", tv[0] && tv[1] > 3 && !g_wpOn, "set %d, gps points %d, cleared on arrival %d", (int)tv[0], (int)tv[1], !g_wpOn);
    } else if (!strcmp(t, "islands")) {
        /* load both new islands and walk around a bit */
        if (f == 2) travel_to(1, 0);
        if (f == 60) { tv[0] = g_nboxes; tv[1] = g_curMap; travel_to(2, 0); }
        if (f == 120) { tv[2] = g_nboxes; tv[3] = g_curMap; travel_to(0, 0); }
        if (f == 180) tresult("islands", tv[1] == 1 && tv[3] == 2 && g_curMap == 0 && tv[0] > 200 && tv[2] > 200,
                              "sunset sands %d boxes, frostpeak %d boxes, maple bay %d boxes", (int)tv[0], (int)tv[2], g_nboxes);
    } else if (!strcmp(t, "busted")) {
        if (f == 2) { tv[0] = g_prog.coins; wanted_crime(1.5f, g_pl.pos); spawn_thug(GANG_POLICE, vadd(g_pl.pos, v3(4, 0, 0)), -1); }
        if (g_pl.busted) tv[1] = 1;
        if (f == 600) tresult("busted", tv[1] && !g_pl.dead && wanted_stars() == 0 && distxz(g_pl.pos, g_spots.police) < 3 && g_prog.coins < tv[0],
                              "busted %d, respawned at station %d, coins %d -> %d", (int)tv[1], distxz(g_pl.pos, g_spots.police) < 3, (int)tv[0], g_prog.coins);
    } else if (!strcmp(t, "fuel")) {
        Car *c = player_car();
        if (!c) { tresult("fuel", 0, "no car"); return; }
        if (f == 2) { c->fuel = 0.004f; tap(XK_w); }
        if (f == 300) { tv[0] = c->fuel; tv[1] = car_speed(c); untap(XK_w); }
        if (f == 320) { c->pos = g_spots.gas[1]; c->vel = v3(0, 0, 0); tv[2] = g_prog.coins; }
        if (f == 700) tresult("fuel", tv[0] == 0 && tv[1] < 3 && c->fuel > 0.99f && g_prog.coins < tv[2],
                              "fuel after driving %.3f speed %.1f, after gas station %.2f, coins %d -> %d", tv[0], tv[1], c->fuel, (int)tv[2], g_prog.coins);
    } else if (!strcmp(t, "armory")) {
        if (f == 3) tap(XK_Return);
        if (f == 4) untap(XK_Return);
        if (f == 6) { tap(XK_Right); }
        if (f == 7) untap(XK_Right);
        if (f == 9) tap(XK_Return);
        if (f == 10) untap(XK_Return);
        if (f == 14) tresult("armory", (g_prog.weapons & 2) && g_prog.wup[W_PISTOL][WU_POWER] == 1 && g_prog.coins == 5000 - 300 - 250,
                             "weapons %u, pistol power %d, coins %d", g_prog.weapons, g_prog.wup[W_PISTOL][WU_POWER], g_prog.coins);
    } else if (!strcmp(t, "hangar")) {
        if (f == 3) { tap(XK_Down); }
        if (f == 4) untap(XK_Down);
        if (f == 5) tap(XK_Down);
        if (f == 6) untap(XK_Down);
        if (f == 8) tap(XK_Return);
        if (f == 9) untap(XK_Return);
        if (f == 12) {
            int at = 0;
            for (int i = 0; i < MAX_CARS; i++) if (g_cars[i].used && g_cars[i].stand == NUM_STANDS - 1 && g_cars[i].type == AIR_FIRST + 2) at = 1;
            tresult("hangar", (g_prog.aircraft & 4) && g_prog.curAir == 2 && g_prog.coins == 3500 && at, "aircraft %u cur %d coins %d delivered %d", g_prog.aircraft, g_prog.curAir, g_prog.coins, at);
        }
    } else if (t[0] == 'm' && t[1] >= '0' && t[1] <= '9') {
        int m = atoi(t + 1);
        if (f == 1) { if (m >= 14) { travel_to(m - 13, 0); g_prog.coins = 5000; } tv[0] = g_prog.coins; mission_start(m); }
        if (f > 200 && !mission_active() && !tdone) {
            tresult(scenario, (g_prog.missions >> m) & 1, "mission %d done=%d coins %d -> %d frame %d [%s]", m, (g_prog.missions >> m) & 1, (int)tv[0], g_prog.coins, f, msgBuf);
            return;
        }
        if (f < 200) return;
        V3 p;
        if (m >= 9 && m <= 13 && m != 11) {
            /* shooting missions: really aim and pull the trigger */
            g_godMode = 1;
            if (f == 200) {
                int w = m == 9 ? W_PISTOL : W_RIFLE;
                give_weapon(w, 200);
                g_pl.weapon = w;
            }
            if (!mission_target(&p)) return;
            if (m == 10) p = vadd(p, v3(0, 1.2f, 0));
            float want = m == 9 ? 12 : m == 10 ? 10 : 45;
            V3 d = v3(g_pl.pos.x - p.x, 0, g_pl.pos.z - p.z);
            float hd = vlen(d);
            int blocked = ray_world(vadd(g_pl.pos, v3(0, 1.6f, 0)), p) < 0.97f;
            if (f % 60 == 0 && (hd > want * 1.6f || hd < want * 0.5f || blocked)) {
                for (int k = 0; k < 16; k++) {
                    float a = (m == 9) ? PI + (k - 8) * 0.05f : k * PI / 8 + f * 0.01f;
                    float r = m == 9 ? want + k * 0.6f : want;
                    V3 c = vadd(v3(p.x, 0, p.z), v3(sinf(a) * r, 0, cosf(a) * r));
                    c.y = ground_height(c.x, c.z, 0, 0.5f);
                    V3 q = c;
                    collide_circle(&q, 0.4f, 0.45f, 1.8f, NULL);
                    if (dist2xz(q, c) > 0.01f || ray_world(vadd(c, v3(0, 1.6f, 0)), p) < 0.97f) continue;
                    g_pl.pos = c;
                    break;
                }
                g_pl.vel = v3(0, 0, 0);
            }
            int ok = aim_at(p);
            if (getenv("MB_DEBUG") && f % 120 == 0) fprintf(stderr, "m%d f%d alive %d pl %.0f %.0f %.0f tgt %.0f %.0f %.0f ok %d pitch %.2f ammo %d clip %d\n", m, f, targets_alive(), g_pl.pos.x, g_pl.pos.y, g_pl.pos.z, p.x, p.y, p.z, ok, g_camPitch, g_prog.ammo[g_pl.weapon], g_pl.clip[g_pl.weapon]);
            if (ok && f % 12 == 0) tap(XK_v);
            if (f % 12 == 6) untap(XK_v);
            return;
        }
        if (f % 20) return;
        if (!mission_target(&p)) return;
        Car *c = player_car();
        if (m == 3) {
            if (c) g_pl.car = -1;
            V3 d = vnorm(v3(1, 0, 0));
            g_pl.pos = vsub(p, vmul(d, 1.4f));
            g_pl.pos.y = 0;
            g_pl.yaw = PI / 2;
            g_pl.vel = v3(0, 0, 0);
            tap(XK_f);
            return;
        }
        if (m == 4 || m == 7) {
            V3 d = vnorm(v3(p.x - c->pos.x, 0, p.z - c->pos.z));
            c->pos = vsub(p, vmul(d, 5.5f));
            c->pos.y = p.y;
            c->yaw = atan2f(d.x, d.z);
            c->vel = vmul(d, 22 + 5.0f * (f % 3));
            return;
        }
        if (m == 5) { g_pl.pos = p; g_pl.vel = v3(0, 0, 0); return; }
        if (c) { c->pos = p; c->vel = v3(0, 0, 0); if (m == 8) { c->onGround = 0; c->aspd = 40; c->pitch = 0; } }
        else { g_pl.pos = p; g_pl.pos.y = fmaxf(0, p.y - 1.2f); g_pl.vel = v3(0, 0, 0); }
    }
    if (tframe % 20 == 1) untap(XK_f);
}

void ui_init(const char *autoshot) { scenario = autoshot; }
static void apply_scenario(void) {
    scenarioSet = 1;
    if (!scenario) return;
    if (!strcmp(scenario, "menu")) return;
    if (!strcmp(scenario, "options")) { state = S_OPTIONS; return; }
    if (!strcmp(scenario, "controls")) { state = S_CONTROLS; return; }
    if (!strcmp(scenario, "test_rebind")) { state = S_CONTROLS; return; }
    if (!strcmp(scenario, "howto")) { state = S_HOWTO; return; }
    start_game(0);
    g_prog.coins = 5000;
    msgT = 0;
    toastT[0] = 0;
    if (!strcmp(scenario, "shop") || !strcmp(scenario, "test_shop")) state = S_SHOP;
    else if (!strcmp(scenario, "test_rebind")) state = S_CONTROLS;
    else if (!strcmp(scenario, "test_drive") || !strcmp(scenario, "test_ramp")) {
        for (int i = 0; i < MAX_CARS; i++) if (g_cars[i].used && g_cars[i].called) { put_player_in_car(i); break; }
    }
    else if (!strcmp(scenario, "test_fly") || !strcmp(scenario, "flying")) {
        for (int i = 0; i < MAX_CARS; i++) if (g_cars[i].used && g_cars[i].stand == 0) {
            Car *c = &g_cars[i];
            c->pos = g_spots.runway; c->yaw = g_spots.runwayYaw;
            put_player_in_car(i);
            if (!strcmp(scenario, "flying")) { c->pos = v3(-60, 70, AZ(300)); c->yaw = PI; c->onGround = 0; c->aspd = 40; c->throttle = 0.8f; g_camYaw = PI; }
            break;
        }
    } else if (!strcmp(scenario, "test_heli") || !strcmp(scenario, "heli")) {
        for (int i = 0; i < MAX_CARS; i++) if (g_cars[i].used && g_cars[i].type == AIR_FIRST + 2) {
            put_player_in_car(i);
            if (!strcmp(scenario, "heli")) { Car *c = &g_cars[i]; c->pos = v3(30, 45, 120); c->onGround = 0; c->throttle = 1; c->yaw = PI; g_camYaw = PI; g_camPitch = 0.35f; }
            break;
        }
    } else if (!strcmp(scenario, "test_fuel")) {
        for (int i = 0; i < MAX_CARS; i++) if (g_cars[i].used && g_cars[i].called) { put_player_in_car(i); break; }
    } else if (!strcmp(scenario, "test_armory") || !strcmp(scenario, "armory")) ui_open_shop(SHOP_ARMORY);
    else if (!strcmp(scenario, "test_hangar") || !strcmp(scenario, "hangar")) ui_open_shop(SHOP_HANGAR);
    else if (!strcmp(scenario, "tuner")) ui_open_shop(SHOP_TUNER);
    else if (!strcmp(scenario, "gunx")) ui_open_shop(SHOP_GUNX);
    else if (!strcmp(scenario, "travelmenu")) { g_pl.pos = g_spots.travel; ui_open_travel(0); }
    else if (!strcmp(scenario, "desert") || !strcmp(scenario, "snow") || !strcmp(scenario, "desertmap") || !strcmp(scenario, "snowmap")) {
        travel_to(scenario[0] == 'd' ? 1 : 2, 0);
        g_pl.pos = scenario[0] == 'd' ? v3(-105, 0, -130) : v3(-145, 0, -150);
        g_camYaw = scenario[0] == 'd' ? -2.3f : -2.5f;
        g_camPitch = 0.35f;
        if (strstr(scenario, "map")) state = S_MAP;
    } else if (!strcmp(scenario, "city")) {
        g_pl.pos = v3(380, 0, 0);
        g_camYaw = -1.9f;
        g_camPitch = 0.2f;
    } else if (!strcmp(scenario, "mapwp")) {
        g_wpOn = 1; g_wpPos = v3(330, 0, 300); state = S_MAP;
    }
    else if (!strcmp(scenario, "airport")) { g_pl.pos = v3(-150, 0, AZ(395)); g_camYaw = 0.6f; g_camPitch = 0.3f; }
    else if (!strcmp(scenario, "gunfight")) {
        give_weapon(W_SMG, 120); g_pl.weapon = W_SMG; g_godMode = 1;
        g_pl.pos = v3(0, 0, road_c(1 + BO) - 20); g_camYaw = 0; g_camPitch = 0.2f;
    } else if (!strcmp(scenario, "police")) {
        for (int i = 0; i < MAX_CARS; i++) if (g_cars[i].used && g_cars[i].called) { put_player_in_car(i); break; }
        wanted_crime(3.0f, player_pos()); g_godMode = 1;
    }
    else if (!strcmp(scenario, "map")) state = S_MAP;
    else if (!strcmp(scenario, "dialog")) ui_open_dialog(0);
    else if (!strcmp(scenario, "car") || !strcmp(scenario, "stunt") || !strcmp(scenario, "race") || !strcmp(scenario, "chase") || !strcmp(scenario, "checkpoint")) {
        for (int i = 0; i < MAX_CARS; i++) if (g_cars[i].used && g_cars[i].called) { put_player_in_car(i); break; }
        if (!strcmp(scenario, "stunt") && g_pl.car >= 0) {
            Car *c = &g_cars[g_pl.car];
            c->pos = v3(8 + 4 - 18, 0, 8 + 10);
            c->yaw = PI / 2;
            c->vel = v3(30, 0, 0);
            g_camYaw = PI / 2;
        }
        if (!strcmp(scenario, "race")) mission_start(6);
        if (!strcmp(scenario, "chase")) mission_start(7);
        if (!strcmp(scenario, "checkpoint")) mission_start(0);
    } else if (!strcmp(scenario, "tower")) {
        g_pl.pos = v3(-35, 0, -55);
        g_pl.yaw = 0;
        g_camYaw = 0.3f;
        g_camPitch = 0.45f;
    } else if (!strcmp(scenario, "downtown")) {
        g_pl.pos = v3(4, 0, -20);
        g_camYaw = 0.8f;
        g_camPitch = 0.15f;
    }
}

void ui_frame(float dt) {
    if (!scenarioSet) apply_scenario();
    test_tick();
    mouseMoved = (g_mouseX != lmx || g_mouseY != lmy);
    lmx = g_mouseX;
    lmy = g_mouseY;
    fpsAcc += dt;
    fpsFrames++;
    if (fpsAcc > 0.5f) { fps = fpsFrames / fpsAcc; fpsAcc = 0; fpsFrames = 0; }
    menuT += 0;
    glClearColor(0.5f, 0.7f, 0.95f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    int st = state;
    switch (st) {
    case S_MENU: draw_main_menu(dt); break;
    case S_CONFIRM: draw_confirm(dt); break;
    case S_PLAY:
        if (key_pressed(XK_Escape) || act_pressed(ACT_PAUSE)) { state = S_PAUSE; sel = 0; audio_engine(0, 0); }
        else if (act_pressed(ACT_MAP)) { state = S_MAP; audio_engine(0, 0); }
        else sim_update(dt);
        sim_render();
        draw_hud(dt);
        break;
    case S_MAP:
        sim_render();
        draw_bigmap();
        if (nav_back() || act_pressed(ACT_MAP)) state = S_PLAY;
        break;
    case S_PAUSE: draw_pause(); break;
    case S_OPTIONS: draw_options(dt); break;
    case S_CONTROLS: menuT += dt; draw_controls(dt); break;
    case S_SHOP: draw_shop(dt); break;
    case S_DIALOG: draw_dialog(); break;
    case S_HOWTO: draw_howto(dt); break;
    case S_TRAVEL: draw_travel(dt); break;
    case S_FLIGHT: audio_engine(0, 0); draw_flight(dt); break;
    }
    plat_grab_mouse(state == S_PLAY);
}
