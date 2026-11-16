/* Platform layer: X11 window, GLX context, input, config & save files. */
#include "game.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <GL/glx.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#define KEY_ARR (0x10000 + 32)
static Display *dpy;
static Window win;
static GLXContext glc;
static Atom wmDelete;
static Cursor blankCursor;
static int grabbed = 0, wantGrab = 0, focused = 1, offscreen = 0;
static unsigned char kdown[KEY_ARR], kpress[KEY_ARR];

int g_winW = 1280, g_winH = 720, g_quit = 0;
float g_mouseDX, g_mouseDY, g_wheel;
int g_mouseX, g_mouseY, g_mouseClick, g_lastKey;
char g_baseDir[1024] = ".";
Settings g_set;
Progress g_prog;
int g_autoshotFrames = 0;

const char *g_actNames[ACT_COUNT] = {
    "FORWARD / ACCELERATE / THROTTLE", "BACK / BRAKE", "LEFT / STEER LEFT", "RIGHT / STEER RIGHT",
    "JUMP / PARACHUTE", "SPRINT / NITRO", "INTERACT / ENTER VEHICLE", "KICK",
    "FIRE WEAPON", "AIM / SCOPE", "RELOAD", "NEXT WEAPON", "PREVIOUS WEAPON",
    "HANDBRAKE", "HORN", "CALL CAR", "RESET VEHICLE", "CLIMB / PULL UP", "DESCEND / DIVE", "FIRE MISSILE",
    "CAMERA LEFT", "CAMERA RIGHT", "CAMERA UP", "CAMERA DOWN", "TOGGLE MAP", "PAUSE"};
const char *g_actIds[ACT_COUNT] = {
    "forward", "back", "left", "right", "jump", "sprint", "interact", "kick",
    "fire", "aim", "reload", "next_weapon", "prev_weapon",
    "handbrake", "horn", "callcar", "resetcar", "ascend", "descend", "missile",
    "cam_left", "cam_right", "cam_up", "cam_down", "map", "pause"};
#define ALL (CTX_FOOT | CTX_CAR | CTX_AIR)
const int g_actCtx[ACT_COUNT] = {
    ALL, ALL, ALL, ALL, CTX_FOOT, CTX_FOOT | CTX_CAR, ALL, CTX_FOOT,
    CTX_FOOT | CTX_AIR, CTX_FOOT, CTX_FOOT, CTX_FOOT, CTX_FOOT,
    CTX_CAR, CTX_CAR, CTX_FOOT, CTX_CAR | CTX_AIR, CTX_AIR, CTX_AIR, CTX_AIR,
    ALL, ALL, ALL, ALL, ALL, ALL};

void base_path(char *out, size_t n, const char *name) { snprintf(out, n, "%s/%s", g_baseDir, name); }

/* ------------------------------------------------------------ input API */
static int kidx(int code) {
    if (code == 0) return -1;
    if (code >= MOUSE_BASE) return 0x10000 + ((code - MOUSE_BASE) & 31);
    return code & 0xFFFF;
}
int key_down(int code) { int i = kidx(code); return i >= 0 ? kdown[i] : 0; }
int key_pressed(int code) { int i = kidx(code); return i >= 0 ? kpress[i] : 0; }
int act_down(int a) {
    for (int s = 0; s < 2; s++) if (g_set.binds[a][s] && key_down(g_set.binds[a][s])) return 1;
    return 0;
}
int act_pressed(int a) {
    for (int s = 0; s < 2; s++) if (g_set.binds[a][s] && key_pressed(g_set.binds[a][s])) return 1;
    return 0;
}

const char *key_name(int code) {
    static char buf[64][40];
    static int bi;
    char *b = buf[bi++ & 63];
    if (!code) return "---";
    if (code >= MOUSE_BASE) {
        int m = code - MOUSE_BASE;
        switch (m) {
        case 1: return "MOUSE LEFT";
        case 2: return "MOUSE MIDDLE";
        case 3: return "MOUSE RIGHT";
        case 4: return "WHEEL UP";
        case 5: return "WHEEL DOWN";
        }
        snprintf(b, 40, "MOUSE %d", m);
        return b;
    }
    switch (code) {
    case XK_space: return "SPACE";
    case XK_Shift_L: return "LEFT SHIFT";
    case XK_Shift_R: return "RIGHT SHIFT";
    case XK_Control_L: return "LEFT CTRL";
    case XK_Control_R: return "RIGHT CTRL";
    case XK_Alt_L: return "LEFT ALT";
    case XK_Alt_R: return "RIGHT ALT";
    case XK_Return: return "ENTER";
    case XK_KP_Enter: return "NUM ENTER";
    case XK_Escape: return "ESC";
    case XK_Tab: return "TAB";
    case XK_BackSpace: return "BACKSPACE";
    case XK_Up: return "UP";
    case XK_Down: return "DOWN";
    case XK_Left: return "LEFT";
    case XK_Right: return "RIGHT";
    case XK_Caps_Lock: return "CAPS LOCK";
    }
    const char *s = XKeysymToString((KeySym)code);
    if (!s) return "?";
    int i = 0;
    for (; s[i] && i < 39; i++) {
        char c = s[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c == '_') c = ' ';
        b[i] = c;
    }
    b[i] = 0;
    return b;
}
const char *act_key(int a) {
    if (g_set.binds[a][0]) return key_name(g_set.binds[a][0]);
    if (g_set.binds[a][1]) return key_name(g_set.binds[a][1]);
    return "UNBOUND";
}

/* ------------------------------------------------------------- settings */
void settings_default_binds(void) {
    static const int d[ACT_COUNT][2] = {
        {XK_w, XK_Up}, {XK_s, XK_Down}, {XK_a, XK_Left}, {XK_d, XK_Right},
        {XK_space, 0}, {XK_Shift_L, 0}, {XK_e, 0}, {XK_f, 0},
        {MOUSE_BASE + 1, XK_v}, {MOUSE_BASE + 3, XK_x}, {XK_r, 0}, {XK_q, MOUSE_BASE + 4}, {XK_z, MOUSE_BASE + 5},
        {XK_space, XK_Control_L}, {XK_h, MOUSE_BASE + 3}, {XK_c, 0}, {XK_r, 0},
        {XK_space, 0}, {XK_Control_L, 0}, {XK_g, MOUSE_BASE + 3},
        {XK_j, 0}, {XK_l, 0}, {XK_i, 0}, {XK_k, 0}, {XK_m, XK_Tab}, {XK_p, 0}};
    memcpy(g_set.binds, d, sizeof d);
}
void settings_defaults(void) {
    g_set.sens = 1.0f;
    g_set.invertY = 0;
    g_set.volume = 8;
    g_set.music = 5;
    g_set.fov = 75;
    g_set.fullscreen = 0;
    g_set.showFps = 0;
    settings_default_binds();
}
static void code_str(int code, char *out, size_t n) {
    if (!code) { snprintf(out, n, "none"); return; }
    if (code >= MOUSE_BASE) { snprintf(out, n, "Mouse%d", code - MOUSE_BASE); return; }
    const char *s = XKeysymToString((KeySym)code);
    snprintf(out, n, "%s", s ? s : "none");
}
static int str_code(const char *s) {
    if (!strcmp(s, "none") || !*s) return 0;
    if (!strncmp(s, "Mouse", 5)) return MOUSE_BASE + atoi(s + 5);
    KeySym k = XStringToKeysym(s);
    return k == NoSymbol ? 0 : (int)k;
}
static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ' || e[-1] == '\t')) *--e = 0;
    return s;
}
void config_save(void) {
    char p[1200];
    base_path(p, sizeof p, "config.ini");
    FILE *f = fopen(p, "w");
    if (!f) return;
    fprintf(f, "# Maple Bay Mayhem settings (also editable in-game: Options)\n");
    fprintf(f, "sensitivity=%.2f\ninvert_y=%d\nvolume=%d\nmusic=%d\nfov=%d\nfullscreen=%d\nshow_fps=%d\n",
            g_set.sens, g_set.invertY, g_set.volume, g_set.music, g_set.fov, g_set.fullscreen, g_set.showFps);
    fprintf(f, "# key bindings: bind.<action>=<primary>,<secondary>  (X11 keysym names, MouseN, or none)\n");
    for (int a = 0; a < ACT_COUNT; a++) {
        char k0[64], k1[64];
        code_str(g_set.binds[a][0], k0, sizeof k0);
        code_str(g_set.binds[a][1], k1, sizeof k1);
        fprintf(f, "bind.%s=%s,%s\n", g_actIds[a], k0, k1);
    }
    fclose(f);
}
void config_load(void) {
    settings_defaults();
    char p[1200], line[256];
    base_path(p, sizeof p, "config.ini");
    FILE *f = fopen(p, "r");
    if (!f) return;
    int sawFire = 0;
    while (fgets(line, sizeof line, f)) {
        char *l = trim(line);
        if (!strncmp(l, "bind.fire=", 10)) sawFire = 1;
        if (*l == '#' || !*l) continue;
        char *eq = strchr(l, '=');
        if (!eq) continue;
        *eq = 0;
        char *k = trim(l), *v = trim(eq + 1);
        if (!strcmp(k, "sensitivity")) g_set.sens = clampf((float)atof(v), 0.1f, 5.0f);
        else if (!strcmp(k, "invert_y")) g_set.invertY = atoi(v) != 0;
        else if (!strcmp(k, "volume")) g_set.volume = (int)clampf((float)atoi(v), 0, 10);
        else if (!strcmp(k, "music")) g_set.music = (int)clampf((float)atoi(v), 0, 10);
        else if (!strcmp(k, "fov")) g_set.fov = (int)clampf((float)atoi(v), 50, 110);
        else if (!strcmp(k, "fullscreen")) g_set.fullscreen = atoi(v) != 0;
        else if (!strcmp(k, "show_fps")) g_set.showFps = atoi(v) != 0;
        else if (!strncmp(k, "bind.", 5)) {
            for (int a = 0; a < ACT_COUNT; a++) {
                if (strcmp(k + 5, g_actIds[a])) continue;
                char *c = strchr(v, ',');
                if (c) { *c = 0; g_set.binds[a][1] = str_code(trim(c + 1)); }
                else g_set.binds[a][1] = 0;
                g_set.binds[a][0] = str_code(trim(v));
            }
        }
    }
    fclose(f);
    /* config from version 1: the left mouse button used to kick, now it fires weapons */
    if (!sawFire)
        for (int k = 0; k < 2; k++)
            if (g_set.binds[ACT_KICK][k] == MOUSE_BASE + 1) g_set.binds[ACT_KICK][k] = 0;
}

/* ---------------------------------------------------------- save files */
void progress_reset(void) {
    memset(&g_prog, 0, sizeof g_prog);
    g_prog.coins = 50;
    g_prog.cars = 1;
    g_prog.outfits = 1;
    g_prog.aircraft = 1;
    g_prog.weapons = 1;
}
int save_exists(void) {
    char p[1200];
    base_path(p, sizeof p, "savegame.dat");
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
}
int save_game(void) {
    if (g_autoshotFrames > 0) return 1; /* never touch the real save during automated test runs */
    char p[1200];
    base_path(p, sizeof p, "savegame.dat");
    FILE *f = fopen(p, "w");
    if (!f) return 0;
    fprintf(f, "coins=%d\nmissions=%u\ncars=%u\ncurcar=%d\noutfits=%u\noutfit=%d\nupgrades=%u\ngizmos=%u\nearned=%d\nplaytime=%.1f\n",
            g_prog.coins, g_prog.missions, g_prog.cars, g_prog.curCar, g_prog.outfits, g_prog.outfit,
            g_prog.upgrades, g_prog.gizmos, g_prog.earned, g_prog.playTime);
    fprintf(f, "map=%d\n", g_prog.map);
    fprintf(f, "aircraft=%u\ncurair=%d\nweapons=%u\nturf=%u\ncarpaint=%d\nairpaint=%d\nvitality=%d\nbodyarmor=%d\n",
            g_prog.aircraft, g_prog.curAir, g_prog.weapons, g_prog.turf, g_prog.carPaint, g_prog.airPaint, g_prog.vitality, g_prog.bodyArmor);
    fprintf(f, "ammo=");
    for (int i = 0; i < 8; i++) fprintf(f, "%d%s", g_prog.ammo[i], i < 7 ? "," : "\n");
    fprintf(f, "wup=");
    for (int i = 0; i < 8; i++) for (int k = 0; k < 4; k++) fprintf(f, "%d%s", g_prog.wup[i][k], (i == 7 && k == 3) ? "\n" : ",");
    fprintf(f, "carup=%d,%d,%d,%d\nairup=%d,%d,%d,%d,%d,%d\n", g_prog.carUp[0], g_prog.carUp[1], g_prog.carUp[2], g_prog.carUp[3],
            g_prog.airUp[0], g_prog.airUp[1], g_prog.airUp[2], g_prog.airUp[3], g_prog.airUp[4], g_prog.airUp[5]);
    fclose(f);
    return 1;
}
int load_game(void) {
    progress_reset();
    char p[1200], line[256];
    base_path(p, sizeof p, "savegame.dat");
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char *k = trim(line), *v = trim(eq + 1);
        unsigned u = (unsigned)strtoul(v, NULL, 10);
        if (!strcmp(k, "coins")) g_prog.coins = atoi(v);
        else if (!strcmp(k, "missions")) g_prog.missions = u;
        else if (!strcmp(k, "cars")) g_prog.cars = u | 1;
        else if (!strcmp(k, "curcar")) g_prog.curCar = atoi(v);
        else if (!strcmp(k, "outfits")) g_prog.outfits = u | 1;
        else if (!strcmp(k, "outfit")) g_prog.outfit = atoi(v);
        else if (!strcmp(k, "upgrades")) g_prog.upgrades = u;
        else if (!strcmp(k, "gizmos")) g_prog.gizmos = u;
        else if (!strcmp(k, "earned")) g_prog.earned = atoi(v);
        else if (!strcmp(k, "playtime")) g_prog.playTime = (float)atof(v);
        else if (!strcmp(k, "aircraft")) g_prog.aircraft = u | 1;
        else if (!strcmp(k, "map")) g_prog.map = (int)clampf((float)atoi(v), 0, NUM_MAPS - 1);
        else if (!strcmp(k, "curair")) g_prog.curAir = atoi(v);
        else if (!strcmp(k, "weapons")) g_prog.weapons = u | 1;
        else if (!strcmp(k, "turf")) g_prog.turf = u;
        else if (!strcmp(k, "carpaint")) g_prog.carPaint = atoi(v) % 9;
        else if (!strcmp(k, "airpaint")) g_prog.airPaint = atoi(v) % 9;
        else if (!strcmp(k, "vitality")) g_prog.vitality = (int)clampf((float)atoi(v), 0, 4);
        else if (!strcmp(k, "bodyarmor")) g_prog.bodyArmor = (int)clampf((float)atoi(v), 0, 100);
        else if (!strcmp(k, "ammo") || !strcmp(k, "wup") || !strcmp(k, "carup") || !strcmp(k, "airup")) {
            int vals[32], n = 0;
            char *q = v;
            while (*q && n < 32) { vals[n++] = (int)strtol(q, &q, 10); if (*q == ',') q++; else break; }
            if (!strcmp(k, "ammo")) for (int i = 0; i < n && i < 8; i++) g_prog.ammo[i] = vals[i] < 0 ? 0 : vals[i];
            else if (!strcmp(k, "wup")) for (int i = 0; i < n; i++) g_prog.wup[i / 4][i % 4] = (unsigned char)clampf((float)vals[i], 0, 3);
            else if (!strcmp(k, "carup")) for (int i = 0; i < n && i < 4; i++) g_prog.carUp[i] = (int)clampf((float)vals[i], 0, 3);
            else for (int i = 0; i < n && i < 6; i++) g_prog.airUp[i] = (int)clampf((float)vals[i], 0, 3);
        }
    }
    fclose(f);
    if (g_prog.curCar < 0 || g_prog.curCar >= NUM_SHOP_CARS || !(g_prog.cars & (1u << g_prog.curCar))) g_prog.curCar = 0;
    if (g_prog.outfit < 0 || g_prog.outfit >= NUM_OUTFITS || !(g_prog.outfits & (1u << g_prog.outfit))) g_prog.outfit = 0;
    if (g_prog.curAir < 0 || g_prog.curAir >= NUM_AIR_SHOP || !(g_prog.aircraft & (1u << g_prog.curAir))) g_prog.curAir = 0;
    return 1;
}

/* ------------------------------------------------------------- platform */
double plat_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void do_grab(void) {
    XWarpPointer(dpy, None, win, 0, 0, 0, 0, g_winW / 2, g_winH / 2);
    int r = XGrabPointer(dpy, win, True, ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                         GrabModeAsync, GrabModeAsync, win, blankCursor, CurrentTime);
    if (r == GrabSuccess) {
        XDefineCursor(dpy, win, blankCursor);
        grabbed = 1;
    }
}
static void do_ungrab(void) {
    XUngrabPointer(dpy, CurrentTime);
    XUndefineCursor(dpy, win);
    grabbed = 0;
}
void plat_grab_mouse(int on) {
    if (offscreen) return;
    wantGrab = on;
    if (on && !grabbed && focused) do_grab();
    if (!on && grabbed) do_ungrab();
}

void plat_set_fullscreen(int on) {
    if (offscreen) return;
    XEvent e;
    memset(&e, 0, sizeof e);
    e.type = ClientMessage;
    e.xclient.window = win;
    e.xclient.message_type = XInternAtom(dpy, "_NET_WM_STATE", False);
    e.xclient.format = 32;
    e.xclient.data.l[0] = on ? 1 : 0;
    e.xclient.data.l[1] = (long)XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    e.xclient.data.l[2] = 0;
    e.xclient.data.l[3] = 1;
    XSendEvent(dpy, DefaultRootWindow(dpy), False, SubstructureRedirectMask | SubstructureNotifyMask, &e);
    XFlush(dpy);
}

static void set_vsync(void) {
    const char *ext = glXQueryExtensionsString(dpy, DefaultScreen(dpy));
    if (ext && strstr(ext, "GLX_EXT_swap_control")) {
        typedef void (*F)(Display *, GLXDrawable, int);
        F f = (F)glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalEXT");
        if (f) { f(dpy, win, 1); return; }
    }
    if (ext && strstr(ext, "GLX_MESA_swap_control")) {
        typedef int (*F)(unsigned);
        F f = (F)glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalMESA");
        if (f) { f(1); return; }
    }
    if (ext && strstr(ext, "GLX_SGI_swap_control")) {
        typedef int (*F)(int);
        F f = (F)glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalSGI");
        if (f) f(1);
    }
}

/* Offscreen mode (used by --shot): the window is never mapped, rendering goes to an FBO. */
typedef void (*PFNGENFB)(GLsizei, GLuint *);
typedef void (*PFNBINDFB)(GLenum, GLuint);
typedef void (*PFNRBSTORE)(GLenum, GLenum, GLsizei, GLsizei);
typedef void (*PFNFBRB)(GLenum, GLenum, GLenum, GLuint);
typedef GLenum (*PFNCHECKFB)(GLenum);
static int setup_fbo(void) {
    PFNGENFB genFB = (PFNGENFB)glXGetProcAddressARB((const GLubyte *)"glGenFramebuffers");
    PFNBINDFB bindFB = (PFNBINDFB)glXGetProcAddressARB((const GLubyte *)"glBindFramebuffer");
    PFNGENFB genRB = (PFNGENFB)glXGetProcAddressARB((const GLubyte *)"glGenRenderbuffers");
    PFNBINDFB bindRB = (PFNBINDFB)glXGetProcAddressARB((const GLubyte *)"glBindRenderbuffer");
    PFNRBSTORE rbStore = (PFNRBSTORE)glXGetProcAddressARB((const GLubyte *)"glRenderbufferStorage");
    PFNFBRB fbRB = (PFNFBRB)glXGetProcAddressARB((const GLubyte *)"glFramebufferRenderbuffer");
    PFNCHECKFB check = (PFNCHECKFB)glXGetProcAddressARB((const GLubyte *)"glCheckFramebufferStatus");
    if (!genFB || !bindFB || !genRB || !bindRB || !rbStore || !fbRB || !check) return 0;
    GLuint fb, rb[2];
    genFB(1, &fb);
    bindFB(0x8D40, fb);
    genRB(2, rb);
    bindRB(0x8D41, rb[0]);
    rbStore(0x8D41, 0x8058, g_winW, g_winH);
    fbRB(0x8D40, 0x8CE0, 0x8D41, rb[0]);
    bindRB(0x8D41, rb[1]);
    rbStore(0x8D41, 0x81A6, g_winW, g_winH);
    fbRB(0x8D40, 0x8D00, 0x8D41, rb[1]);
    if (check(0x8D40) != 0x8CD5) return 0;
    glDrawBuffer(0x8CE0);
    glReadBuffer(0x8CE0);
    offscreen = 1;
    return 1;
}
void plat_inject(int code, int down) {
    int i = kidx(code);
    if (i < 0) return;
    if (down && !kdown[i]) { kpress[i] = 1; g_lastKey = code; }
    kdown[i] = (unsigned char)down;
}

static int plat_init(int off) {
    dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "Cannot open X display. Is DISPLAY set?\n"); return 0; }
    int attr[] = {GLX_RGBA, GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 24, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8, None};
    XVisualInfo *vi = glXChooseVisual(dpy, DefaultScreen(dpy), attr);
    if (!vi) { attr[3] = 16; vi = glXChooseVisual(dpy, DefaultScreen(dpy), attr); }
    if (!vi) { fprintf(stderr, "No suitable OpenGL visual\n"); return 0; }
    Window root = RootWindow(dpy, vi->screen);
    XSetWindowAttributes swa;
    memset(&swa, 0, sizeof swa);
    swa.colormap = XCreateColormap(dpy, root, vi->visual, AllocNone);
    swa.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                     StructureNotifyMask | FocusChangeMask | ExposureMask;
    win = XCreateWindow(dpy, root, 0, 0, g_winW, g_winH, 0, vi->depth, InputOutput, vi->visual, CWColormap | CWEventMask, &swa);
    XStoreName(dpy, win, "Maple Bay Mayhem");
    XClassHint ch;
    ch.res_name = "maplebay";
    ch.res_class = "MapleBayMayhem";
    XSetClassHint(dpy, win, &ch);
    wmDelete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wmDelete, 1);
    if (g_set.fullscreen) {
        Atom st = XInternAtom(dpy, "_NET_WM_STATE", False), fs = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
        XChangeProperty(dpy, win, st, XA_ATOM, 32, PropModeReplace, (unsigned char *)&fs, 1);
    }
    if (!off) XMapWindow(dpy, win);
    glc = glXCreateContext(dpy, vi, NULL, True);
    if (!glc) { fprintf(stderr, "Cannot create OpenGL context\n"); return 0; }
    glXMakeCurrent(dpy, win, glc);
    if (off && !setup_fbo()) { fprintf(stderr, "Offscreen rendering unavailable\n"); return 0; }
    Bool sup;
    XkbSetDetectableAutoRepeat(dpy, True, &sup);
    static char data[1] = {0};
    Pixmap pm = XCreateBitmapFromData(dpy, win, data, 1, 1);
    XColor c;
    memset(&c, 0, sizeof c);
    blankCursor = XCreatePixmapCursor(dpy, pm, pm, &c, &c, 0, 0);
    XFreePixmap(dpy, pm);
    set_vsync();
    XFree(vi);
    return 1;
}

static void plat_poll(void) {
    memset(kpress, 0, sizeof kpress);
    g_mouseDX = g_mouseDY = 0;
    g_mouseClick = 0;
    g_lastKey = 0;
    g_wheel = 0;
    int moved = 0;
    if (offscreen) return;
    while (XPending(dpy)) {
        XEvent ev;
        XNextEvent(dpy, &ev);
        switch (ev.type) {
        case KeyPress: {
            KeySym ks = XLookupKeysym(&ev.xkey, 0);
            int i = kidx((int)ks);
            if (getenv("MB_DEBUG")) fprintf(stderr, "keypress keycode=%u sym=%lx (%s)\n", ev.xkey.keycode, (unsigned long)ks, XKeysymToString(ks) ? XKeysymToString(ks) : "?");
            if (i >= 0) {
                if (!kdown[i]) { kpress[i] = 1; g_lastKey = (int)ks; }
                kdown[i] = 1;
            }
        } break;
        case KeyRelease: {
            KeySym ks = XLookupKeysym(&ev.xkey, 0);
            int i = kidx((int)ks);
            if (i >= 0) kdown[i] = 0;
        } break;
        case ButtonPress: {
            int b = (int)ev.xbutton.button;
            if (getenv("MB_DEBUG")) fprintf(stderr, "button %d\n", b);
            int i = 0x10000 + (b & 31);
            if (!kdown[i]) kpress[i] = 1;
            kdown[i] = 1;
            g_lastKey = MOUSE_BASE + b;
            if (b == 1) g_mouseClick = 1;
            if (b == 4) g_wheel += 1;
            if (b == 5) g_wheel -= 1;
            if (!grabbed) { g_mouseX = ev.xbutton.x; g_mouseY = ev.xbutton.y; }
        } break;
        case ButtonRelease: kdown[0x10000 + ((int)ev.xbutton.button & 31)] = 0; break;
        case MotionNotify:
            if (grabbed) {
                int cx = g_winW / 2, cy = g_winH / 2;
                if (ev.xmotion.x == cx && ev.xmotion.y == cy) break;
                g_mouseDX += ev.xmotion.x - cx;
                g_mouseDY += ev.xmotion.y - cy;
                moved = 1;
            } else {
                g_mouseX = ev.xmotion.x;
                g_mouseY = ev.xmotion.y;
            }
            break;
        case ConfigureNotify:
            g_winW = ev.xconfigure.width;
            g_winH = ev.xconfigure.height;
            break;
        case ClientMessage:
            if ((Atom)ev.xclient.data.l[0] == wmDelete) g_quit = 1;
            break;
        case FocusOut:
            focused = 0;
            memset(kdown, 0, sizeof kdown);
            if (grabbed) do_ungrab();
            break;
        case FocusIn:
            focused = 1;
            if (wantGrab && !grabbed) do_grab();
            break;
        }
    }
    g_mouseDX = clampf(g_mouseDX, -300, 300);
    g_mouseDY = clampf(g_mouseDY, -300, 300);
    if (grabbed && moved) {
        XWarpPointer(dpy, None, win, 0, 0, 0, 0, g_winW / 2, g_winH / 2);
        XFlush(dpy);
    }
    if (wantGrab && !grabbed && focused) do_grab();
}

void take_screenshot(const char *path) {
    int w = g_winW, h = g_winH;
    unsigned char *px = malloc((size_t)w * h * 3);
    if (!px) return;
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glFinish();
    glReadBuffer(offscreen ? 0x8CE0 : GL_BACK);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px);
    if (getenv("MB_DEBUG")) fprintf(stderr, "shot: renderer=%s err=%x px0=%d,%d,%d mid=%d\n", (const char *)glGetString(GL_RENDERER), glGetError(), px[0], px[1], px[2], px[(h/2*w + w/2)*3]);
    save_png(path, w, h, px);
    free(px);
}

int main(int argc, char **argv) {
    char exe[1024];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n > 0) {
        exe[n] = 0;
        char *s = strrchr(exe, '/');
        if (s) { *s = 0; snprintf(g_baseDir, sizeof g_baseDir, "%s", exe); }
    }
    srand((unsigned)time(NULL));
    config_load();
    const char *shotFile = NULL, *scenario = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--windowed")) g_set.fullscreen = 0;
        else if (!strcmp(argv[i], "--fullscreen")) g_set.fullscreen = 1;
        else if (!strcmp(argv[i], "--shot") && i + 3 < argc) {
            g_autoshotFrames = atoi(argv[i + 1]);
            shotFile = argv[i + 2];
            scenario = argv[i + 3];
            i += 3;
        } else if (!strcmp(argv[i], "--help")) {
            printf("Maple Bay Mayhem\n  --windowed      start in a window\n  --fullscreen    start fullscreen\n  F11 toggles fullscreen, F12 saves a screenshot.\n");
            return 0;
        }
    }
    if (shotFile) srand(7);
    if (!plat_init(shotFile != NULL && !getenv("MB_VISIBLE"))) return 1;
    world_build(0);
    sim_init();
    mission_init();
    if (!shotFile) audio_init();
    ui_init(scenario);

    double last = plat_time();
    int frames = 0;
    while (!g_quit) {
        plat_poll();
        double now = plat_time();
        float dt = (float)(now - last);
        last = now;
        if (dt > 0.05f) dt = 0.05f;
        if (dt < 0.0001f) dt = 0.0001f;
        if (shotFile) dt = 1.0f / 60.0f;
        if (key_pressed(XK_F11)) {
            g_set.fullscreen = !g_set.fullscreen;
            plat_set_fullscreen(g_set.fullscreen);
        }
        glViewport(0, 0, g_winW, g_winH);
        ui_frame(dt);
        if (key_pressed(XK_F12)) {
            char dir[1200], p[1300];
            base_path(dir, sizeof dir, "screenshots");
            mkdir(dir, 0755);
            snprintf(p, sizeof p, "%s/shot_%ld.png", dir, (long)time(NULL));
            take_screenshot(p);
            hud_toast("SCREENSHOT SAVED");
        }
        frames++;
        if (shotFile && frames >= g_autoshotFrames) {
            take_screenshot(shotFile);
            g_quit = 1;
        }
        if (offscreen) glFinish();
        else glXSwapBuffers(dpy, win);
    }
    if (!shotFile) config_save();
    audio_shutdown();
    if (grabbed) do_ungrab();
    glXMakeCurrent(dpy, None, NULL);
    glXDestroyContext(dpy, glc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
