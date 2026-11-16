/* Rendering helpers: primitives, bitmap font, 2D UI, character & car models, PNG writer. */
#include "game.h"

/* ---------------------------------------------------------------- font */
/* 5x7 glyphs for ASCII 32..95, each row 5 bits (bit4 = leftmost). */
static const unsigned char FONT[64][7] = {
    {0, 0, 0, 0, 0, 0, 0},                          /* space */
    {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04},     /* ! */
    {0x0A, 0x0A, 0, 0, 0, 0, 0},                    /* " */
    {0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A},     /* # */
    {0x04, 0x0F, 0x14, 0x0E, 0x05, 0x1E, 0x04},     /* $ */
    {0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03},     /* % */
    {0x0C, 0x12, 0x14, 0x08, 0x15, 0x12, 0x0D},     /* & */
    {0x04, 0x04, 0x08, 0, 0, 0, 0},                 /* ' */
    {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02},     /* ( */
    {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08},     /* ) */
    {0x00, 0x04, 0x15, 0x0E, 0x15, 0x04, 0x00},     /* * */
    {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00},     /* + */
    {0, 0, 0, 0, 0x0C, 0x04, 0x08},                 /* , */
    {0, 0, 0, 0x1F, 0, 0, 0},                       /* - */
    {0, 0, 0, 0, 0, 0x0C, 0x0C},                    /* . */
    {0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10},     /* / */
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},     /* 0 */
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},     /* 1 */
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},     /* 2 */
    {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E},     /* 3 */
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},     /* 4 */
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},     /* 5 */
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E},     /* 6 */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},     /* 7 */
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},     /* 8 */
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},     /* 9 */
    {0, 0x0C, 0x0C, 0, 0x0C, 0x0C, 0},              /* : */
    {0, 0x0C, 0x0C, 0, 0x0C, 0x04, 0x08},           /* ; */
    {0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02},     /* < */
    {0, 0, 0x1F, 0, 0x1F, 0, 0},                    /* = */
    {0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08},     /* > */
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04},     /* ? */
    {0x0E, 0x11, 0x17, 0x15, 0x17, 0x10, 0x0E},     /* @ */
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},     /* A */
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},     /* B */
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},     /* C */
    {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E},     /* D */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},     /* E */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},     /* F */
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F},     /* G */
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},     /* H */
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E},     /* I */
    {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C},     /* J */
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},     /* K */
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},     /* L */
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11},     /* M */
    {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11},     /* N */
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},     /* O */
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},     /* P */
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D},     /* Q */
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},     /* R */
    {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E},     /* S */
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},     /* T */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},     /* U */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},     /* V */
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A},     /* W */
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},     /* X */
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04},     /* Y */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F},     /* Z */
    {0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E},     /* [ */
    {0x10, 0x10, 0x08, 0x04, 0x02, 0x01, 0x01},     /* \ */
    {0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E},     /* ] */
    {0x04, 0x0A, 0x11, 0, 0, 0, 0},                 /* ^ */
    {0, 0, 0, 0, 0, 0, 0x1F},                       /* _ */
};
static const unsigned char *glyph(char c) {
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c < 32 || c > 95) c = '?';
    return FONT[c - 32];
}

/* ------------------------------------------------------------ 3D prims */
void gl_box(float x0, float y0, float z0, float x1, float y1, float z1) {
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(x0, y1, z0); glVertex3f(x0, y1, z1); glVertex3f(x1, y1, z1); glVertex3f(x1, y1, z0);
    glNormal3f(0, -1, 0);
    glVertex3f(x0, y0, z0); glVertex3f(x1, y0, z0); glVertex3f(x1, y0, z1); glVertex3f(x0, y0, z1);
    glNormal3f(1, 0, 0);
    glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1); glVertex3f(x1, y0, z1);
    glNormal3f(-1, 0, 0);
    glVertex3f(x0, y0, z0); glVertex3f(x0, y0, z1); glVertex3f(x0, y1, z1); glVertex3f(x0, y1, z0);
    glNormal3f(0, 0, 1);
    glVertex3f(x0, y0, z1); glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1); glVertex3f(x0, y1, z1);
    glNormal3f(0, 0, -1);
    glVertex3f(x0, y0, z0); glVertex3f(x0, y1, z0); glVertex3f(x1, y1, z0); glVertex3f(x1, y0, z0);
    glEnd();
}
void gl_cube(float cx, float cy, float cz, float hx, float hy, float hz) {
    gl_box(cx - hx, cy - hy, cz - hz, cx + hx, cy + hy, cz + hz);
}
void gl_frustum_y(float cx, float y0, float cz, float r0, float r1, float h, int n) {
    float slope = (r0 - r1) / h;
    glBegin(GL_QUADS);
    for (int i = 0; i < n; i++) {
        float a0 = 2 * PI * i / n, a1 = 2 * PI * (i + 1) / n, am = (a0 + a1) * 0.5f;
        V3 nn = vnorm(v3(cosf(am), slope, sinf(am)));
        glNormal3f(nn.x, nn.y, nn.z);
        glVertex3f(cx + cosf(a0) * r0, y0, cz + sinf(a0) * r0);
        glVertex3f(cx + cosf(a1) * r0, y0, cz + sinf(a1) * r0);
        glVertex3f(cx + cosf(a1) * r1, y0 + h, cz + sinf(a1) * r1);
        glVertex3f(cx + cosf(a0) * r1, y0 + h, cz + sinf(a0) * r1);
    }
    glEnd();
    if (r1 > 0.001f) {
        glBegin(GL_POLYGON);
        glNormal3f(0, 1, 0);
        for (int i = n - 1; i >= 0; i--) { float a = 2 * PI * i / n; glVertex3f(cx + cosf(a) * r1, y0 + h, cz + sinf(a) * r1); }
        glEnd();
    }
    glBegin(GL_POLYGON);
    glNormal3f(0, -1, 0);
    for (int i = 0; i < n; i++) { float a = 2 * PI * i / n; glVertex3f(cx + cosf(a) * r0, y0, cz + sinf(a) * r0); }
    glEnd();
}
void gl_cyl_x(float cx, float cy, float cz, float r, float hw, int n) {
    glBegin(GL_QUADS);
    for (int i = 0; i < n; i++) {
        float a0 = 2 * PI * i / n, a1 = 2 * PI * (i + 1) / n, am = (a0 + a1) * 0.5f;
        glNormal3f(0, cosf(am), sinf(am));
        glVertex3f(cx - hw, cy + cosf(a0) * r, cz + sinf(a0) * r);
        glVertex3f(cx + hw, cy + cosf(a0) * r, cz + sinf(a0) * r);
        glVertex3f(cx + hw, cy + cosf(a1) * r, cz + sinf(a1) * r);
        glVertex3f(cx - hw, cy + cosf(a1) * r, cz + sinf(a1) * r);
    }
    glEnd();
    for (int s = -1; s <= 1; s += 2) {
        glBegin(GL_POLYGON);
        glNormal3f((float)s, 0, 0);
        for (int i = 0; i < n; i++) { float a = 2 * PI * i / n; glVertex3f(cx + hw * s, cy + cosf(a) * r, cz + sinf(a) * r); }
        glEnd();
    }
}
void gl_disc_y(float cx, float y, float cz, float r, int n) {
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0, 1, 0);
    glVertex3f(cx, y, cz);
    for (int i = 0; i <= n; i++) { float a = -2 * PI * i / n; glVertex3f(cx + cosf(a) * r, y, cz + sinf(a) * r); }
    glEnd();
}

float text3d_width(const char *s, float px) { return (float)strlen(s) * 6 * px - px; }
void text3d(const char *s, V3 o, V3 right, V3 up, float px) {
    V3 n = vnorm(vcross(right, up));
    glNormal3f(n.x, n.y, n.z);
    glBegin(GL_QUADS);
    for (int c = 0; s[c]; c++) {
        const unsigned char *g = glyph(s[c]);
        for (int row = 0; row < 7; row++)
            for (int col = 0; col < 5; col++) {
                if (!(g[row] & (0x10 >> col))) continue;
                float x0 = (c * 6 + col) * px, x1 = x0 + px;
                float y1 = (7 - row) * px, y0 = y1 - px;
                V3 a = vadd(o, vadd(vmul(right, x0), vmul(up, y0)));
                V3 b = vadd(o, vadd(vmul(right, x1), vmul(up, y0)));
                V3 cc = vadd(o, vadd(vmul(right, x1), vmul(up, y1)));
                V3 d = vadd(o, vadd(vmul(right, x0), vmul(up, y1)));
                glVertex3f(a.x, a.y, a.z); glVertex3f(b.x, b.y, b.z); glVertex3f(cc.x, cc.y, cc.z); glVertex3f(d.x, d.y, d.z);
            }
    }
    glEnd();
}

void set_perspective(float fovDeg, float aspect, float n, float f) {
    float h = n * tanf(fovDeg * 0.5f / DEG);
    glFrustum(-h * aspect, h * aspect, -h, h, n, f);
}
void look_at(V3 eye, V3 at) {
    V3 f = vnorm(vsub(at, eye));
    V3 s = vnorm(vcross(f, v3(0, 1, 0)));
    V3 u = vcross(s, f);
    GLfloat m[16] = {s.x, u.x, -f.x, 0, s.y, u.y, -f.y, 0, s.z, u.z, -f.z, 0, 0, 0, 0, 1};
    glMultMatrixf(m);
    glTranslatef(-eye.x, -eye.y, -eye.z);
    g_camRight = s;
    g_camUp = u;
}
void setup_light(void) {
    GLfloat pos[4] = {0.45f, 0.8f, 0.35f, 0.0f};
    GLfloat amb[4] = {0.48f, 0.48f, 0.52f, 1};
    GLfloat dif[4] = {0.68f, 0.66f, 0.6f, 1};
    GLfloat zero[4] = {0, 0, 0, 1};
    glLightfv(GL_LIGHT0, GL_POSITION, pos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, zero);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
    glLightfv(GL_LIGHT0, GL_SPECULAR, zero);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_NORMALIZE);
}

/* ------------------------------------------------------------------ 2D */
void ui_begin(void) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, g_winW, g_winH, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
void ui_end(void) { glDisable(GL_BLEND); }
void ui_rect(float x, float y, float w, float h, float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
}
void ui_frame_rect(float x, float y, float w, float h, float t, float r, float g, float b, float a) {
    ui_rect(x, y, w, t, r, g, b, a);
    ui_rect(x, y + h - t, w, t, r, g, b, a);
    ui_rect(x, y + t, t, h - 2 * t, r, g, b, a);
    ui_rect(x + w - t, y + t, t, h - 2 * t, r, g, b, a);
}
void ui_circle(float cx, float cy, float rad, float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= 20; i++) { float an = 2 * PI * i / 20; glVertex2f(cx + cosf(an) * rad, cy + sinf(an) * rad); }
    glEnd();
}
float ui_text_w(const char *s, float sc) { int n = (int)strlen(s); return n ? (n * 6 - 1) * sc : 0; }
static void text_raw(float x, float y, float sc, const char *s) {
    glBegin(GL_QUADS);
    for (int c = 0; s[c]; c++) {
        const unsigned char *g = glyph(s[c]);
        for (int row = 0; row < 7; row++) {
            int col = 0;
            while (col < 5) {
                if (!(g[row] & (0x10 >> col))) { col++; continue; }
                int st = col;
                while (col < 5 && (g[row] & (0x10 >> col))) col++;
                float x0 = x + (c * 6 + st) * sc, x1 = x + (c * 6 + col) * sc;
                float y0 = y + row * sc, y1 = y0 + sc;
                glVertex2f(x0, y0); glVertex2f(x1, y0); glVertex2f(x1, y1); glVertex2f(x0, y1);
            }
        }
    }
    glEnd();
}
void ui_text(float x, float y, float sc, const char *s, float r, float g, float b, float a) {
    float o = sc * 0.6f;
    if (o < 1) o = 1;
    glColor4f(0, 0, 0, a * 0.65f);
    text_raw(x + o, y + o, sc, s);
    glColor4f(r, g, b, a);
    text_raw(x, y, sc, s);
}
void ui_text_c(float cx, float y, float sc, const char *s, float r, float g, float b, float a) { ui_text(cx - ui_text_w(s, sc) * 0.5f, y, sc, s, r, g, b, a); }
void ui_text_r(float rx, float y, float sc, const char *s, float r, float g, float b, float a) { ui_text(rx - ui_text_w(s, sc), y, sc, s, r, g, b, a); }
float ui_text_wrap(float x, float y, float maxw, float sc, const char *s, float r, float g, float b, float a) {
    char line[256];
    int ll = 0;
    float yy = y;
    const char *p = s;
    while (*p) {
        if (*p == '\n') { line[ll] = 0; ui_text(x, yy, sc, line, r, g, b, a); yy += 10 * sc; ll = 0; p++; continue; }
        const char *we = p;
        while (*we && *we != ' ' && *we != '\n') we++;
        int wl = (int)(we - p);
        int need = ll + (ll ? 1 : 0) + wl;
        if (ll && (need * 6 - 1) * sc > maxw) { line[ll] = 0; ui_text(x, yy, sc, line, r, g, b, a); yy += 10 * sc; ll = 0; }
        if (ll && ll < 250) line[ll++] = ' ';
        for (int i = 0; i < wl && ll < 250; i++) line[ll++] = p[i];
        p = we;
        while (*p == ' ') p++;
    }
    if (ll) { line[ll] = 0; ui_text(x, yy, sc, line, r, g, b, a); yy += 10 * sc; }
    return yy - y;
}

/* ----------------------------------------------------------- character */
static void limb(float px, float py, float pz, float ang, float w, float len, float d, const float *col, const float *endc) {
    glPushMatrix();
    glTranslatef(px, py, pz);
    glRotatef(ang * DEG, 1, 0, 0);
    glColor3fv(col);
    gl_box(-w / 2, -len + (endc ? 0.16f : 0), -d / 2, w / 2, 0, d / 2);
    if (endc) {
        glColor3fv(endc);
        gl_box(-w / 2 - 0.01f, -len, -d / 2 - 0.01f, w / 2 + 0.01f, -len + 0.16f, d / 2 + (w > 0.2f ? 0.08f : 0.01f));
    }
    glPopMatrix();
}
void draw_weapon_model(int w) {
    switch (w) {
    case W_PISTOL:
        glColor3f(0.2f, 0.2f, 0.22f); gl_box(-0.045f, -0.02f, -0.05f, 0.045f, 0.09f, 0.26f);
        glColor3f(0.35f, 0.25f, 0.15f); gl_box(-0.04f, -0.16f, -0.05f, 0.04f, 0.0f, 0.04f);
        break;
    case W_SMG:
        glColor3f(0.15f, 0.15f, 0.17f); gl_box(-0.05f, -0.03f, -0.15f, 0.05f, 0.1f, 0.4f);
        gl_box(-0.03f, -0.25f, 0.05f, 0.03f, -0.03f, 0.12f);
        break;
    case W_SHOTGUN:
        glColor3f(0.45f, 0.28f, 0.14f); gl_box(-0.05f, -0.05f, -0.35f, 0.05f, 0.06f, 0.1f);
        glColor3f(0.25f, 0.25f, 0.27f); gl_box(-0.04f, 0.0f, 0.1f, 0.04f, 0.07f, 0.7f);
        break;
    case W_RIFLE:
        glColor3f(0.3f, 0.36f, 0.22f); gl_box(-0.045f, -0.06f, -0.4f, 0.045f, 0.06f, 0.3f);
        glColor3f(0.15f, 0.15f, 0.16f); gl_box(-0.025f, -0.01f, 0.3f, 0.025f, 0.04f, 0.95f);
        gl_box(-0.04f, 0.07f, -0.15f, 0.04f, 0.15f, 0.2f);
        break;
    case W_ASSAULT:
        glColor3f(0.18f, 0.18f, 0.2f); gl_box(-0.05f, -0.05f, -0.35f, 0.05f, 0.08f, 0.45f);
        glColor3f(0.4f, 0.3f, 0.18f); gl_box(-0.045f, -0.08f, -0.5f, 0.045f, 0.05f, -0.3f);
        glColor3f(0.12f, 0.12f, 0.13f); gl_box(-0.03f, -0.28f, 0.0f, 0.03f, -0.05f, 0.09f);
        gl_box(-0.02f, 0.0f, 0.45f, 0.02f, 0.04f, 0.75f);
        gl_box(-0.03f, 0.08f, 0.0f, 0.03f, 0.14f, 0.15f);
        break;
    case W_GRENADE:
        glColor3f(0.35f, 0.3f, 0.2f); gl_box(-0.05f, -0.06f, -0.3f, 0.05f, 0.06f, 0.1f);
        glColor3f(0.2f, 0.22f, 0.2f);
        glPushMatrix(); glRotatef(90, 0, 1, 0); gl_cyl_x(-0.25f, 0.05f, 0, 0.12f, 0.2f, 10); glPopMatrix();
        glColor3f(0.15f, 0.15f, 0.15f); gl_box(-0.03f, 0.02f, 0.4f, 0.03f, 0.08f, 0.6f);
        break;
    case W_ROCKET:
        glColor3f(0.3f, 0.4f, 0.25f);
        glPushMatrix(); glRotatef(90, 0, 1, 0); gl_cyl_x(-0.15f, 0.1f, 0, 0.11f, 0.65f, 10); glPopMatrix();
        glColor3f(0.9f, 0.3f, 0.1f);
        glPushMatrix(); glRotatef(90, 0, 1, 0); gl_cyl_x(-0.82f, 0.1f, 0, 0.07f, 0.04f, 8); glPopMatrix();
        break;
    }
}
void draw_character(V3 p, float yaw, float anim, float move, const Look *L, float kick, int air) {
    draw_character_ex(p, yaw, anim, move, L, kick, air, 0, 0);
}
void draw_character_ex(V3 p, float yaw, float anim, float move, const Look *L, float kick, int air, int weapon, float aimPitch) {
    glPushMatrix();
    glTranslatef(p.x, p.y, p.z);
    glRotatef(yaw * DEG, 0, 1, 0);
    float sw = sinf(anim) * 0.85f * move;
    float bob = fabsf(sinf(anim)) * 0.07f * move;
    if (air) sw = 0.55f;
    glTranslatef(0, bob, 0);
    float legR = -sw;
    if (kick > 0) legR = -1.6f * sinf(clampf(kick / 0.35f, 0, 1) * PI);
    limb(-0.14f, 0.86f, 0, sw, 0.23f, 0.86f, 0.25f, L->pants, L->shoes);
    limb(0.14f, 0.86f, 0, legR, 0.23f, 0.86f, 0.25f, L->pants, L->shoes);
    glColor3fv(L->shirt);
    gl_box(-0.31f, 0.82f, -0.18f, 0.31f, 1.46f, 0.18f);
    float armA = air ? -2.4f : -sw * 0.9f;
    float armB = air ? -2.4f : sw * 0.9f;
    if (weapon > 0) {
        float aa = -PI / 2 - aimPitch;
        glPushMatrix();
        glTranslatef(-0.40f, 1.43f, 0);
        glRotatef(aa * DEG, 1, 0, 0);
        glColor3fv(L->shirt); gl_box(-0.085f, -0.48f, -0.09f, 0.085f, 0, 0.09f);
        glColor3fv(L->skin); gl_box(-0.09f, -0.64f, -0.09f, 0.09f, -0.48f, 0.09f);
        glTranslatef(0, -0.6f, 0);
        glRotatef(90, 1, 0, 0);
        draw_weapon_model(weapon);
        glPopMatrix();
        if (weapon >= W_SMG) limb(0.40f, 1.43f, 0, aa + 0.15f, 0.17f, 0.58f, 0.18f, L->shirt, L->skin);
        else limb(0.40f, 1.43f, 0, armB, 0.17f, 0.64f, 0.18f, L->shirt, L->skin);
    } else {
        limb(-0.40f, 1.43f, 0, armA, 0.17f, 0.64f, 0.18f, L->shirt, L->skin);
        limb(0.40f, 1.43f, 0, armB, 0.17f, 0.64f, 0.18f, L->shirt, L->skin);
    }
    glColor3fv(L->skin);
    gl_box(-0.21f, 1.47f, -0.21f, 0.21f, 1.89f, 0.21f);
    glColor3f(0.05f, 0.05f, 0.08f);
    gl_box(-0.13f, 1.66f, 0.205f, -0.05f, 1.75f, 0.225f);
    gl_box(0.05f, 1.66f, 0.205f, 0.13f, 1.75f, 0.225f);
    glColor3f(0.55f, 0.2f, 0.2f);
    gl_box(-0.08f, 1.54f, 0.205f, 0.08f, 1.57f, 0.22f);
    glColor3fv(L->hat);
    switch (L->hat_style) {
    case 0: gl_box(-0.23f, 1.86f, -0.23f, 0.23f, 2.0f, 0.23f); gl_box(-0.2f, 1.86f, 0.2f, 0.2f, 1.9f, 0.45f); break;
    case 1: gl_box(-0.42f, 1.86f, -0.42f, 0.42f, 1.9f, 0.42f); gl_box(-0.22f, 1.9f, -0.22f, 0.22f, 2.05f, 0.22f); break;
    case 2: gl_box(-0.32f, 1.87f, -0.32f, 0.32f, 1.9f, 0.32f); gl_box(-0.2f, 1.9f, -0.2f, 0.2f, 2.12f, 0.2f); break;
    case 3: gl_box(-0.26f, 1.44f, -0.26f, 0.26f, 2.0f, 0.24f);
        glColor4f(0.3f, 0.7f, 1.0f, 1); gl_box(-0.18f, 1.6f, 0.23f, 0.18f, 1.8f, 0.27f); break;
    case 5: gl_box(-0.23f, 1.86f, -0.23f, 0.23f, 2.25f, 0.23f); gl_box(-0.28f, 2.15f, -0.28f, 0.28f, 2.32f, 0.28f); break;
    case 6: gl_box(-0.24f, 1.86f, -0.24f, 0.24f, 2.02f, 0.24f); gl_box(-0.2f, 1.86f, 0.2f, 0.2f, 1.9f, 0.4f);
        glColor3f(1, 0.85f, 0.2f); gl_box(-0.06f, 1.92f, 0.24f, 0.06f, 1.99f, 0.26f); break;
    case 7: gl_box(-0.2f, 1.88f, -0.2f, 0.2f, 1.98f, 0.2f);
        for (int i = -1; i <= 1; i++) gl_box(i * 0.15f - 0.04f, 1.98f, -0.04f, i * 0.15f + 0.04f, 2.1f, 0.04f);
        break;
    default: break;
    }
    glPopMatrix();
}

/* ---------------------------------------------------------------- cars */
static void wheel(float x, float y, float z, float r, float rot, float steer) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(steer * DEG, 0, 1, 0);
    glRotatef(rot * DEG, 1, 0, 0);
    glColor3f(0.12f, 0.12f, 0.13f);
    gl_cyl_x(0, 0, 0, r, 0.16f, 10);
    glColor3f(0.75f, 0.75f, 0.78f);
    gl_box(-0.18f, -r * 0.45f, -0.06f, 0.18f, r * 0.45f, 0.06f);
    gl_box(-0.18f, -0.06f, -r * 0.45f, 0.18f, 0.06f, r * 0.45f);
    glPopMatrix();
}
void draw_car_body(int type, const float *col, float wr, float steer, int wrecked) {
    const CarType *t = &g_carTypes[type];
    float L = t->len, W = t->wid, hw = W / 2, hl = L / 2;
    float c[3] = {col[0], col[1], col[2]};
    float k = wrecked ? 0.22f : 1.0f;
    float body[3] = {c[0] * k, c[1] * k, c[2] * k};
    float glass[3] = {0.22f * k + 0.05f, 0.3f * k + 0.05f, 0.42f * k + 0.05f};
    float dark[3] = {0.15f, 0.15f, 0.16f};
    float rad = 0.38f;
    float wz = L * 0.32f;
    int st = t->style;
    if (st == CS_TRUCK || st == CS_ARMOR) rad = 0.48f;
    if (st == CS_HOTROD) rad = 0.42f;
    glColor3fv(body);
    switch (st) {
    case CS_SEDAN:
    case CS_TAXI:
    case CS_POLICE:
        gl_box(-hw, 0.32f, -hl, hw, 0.95f, hl);
        if (st == CS_POLICE) {
            glColor3f(0.1f * k + 0.05f, 0.12f * k + 0.05f, 0.25f * k + 0.05f);
            gl_box(-hw - 0.01f, 0.32f, -hl * 0.45f, hw + 0.01f, 0.8f, hl * 0.45f);
            glColor3fv(body);
        }
        glColor3fv(glass);
        gl_box(-hw + 0.12f, 0.95f, -L * 0.3f, hw - 0.12f, 1.45f, L * 0.13f);
        glColor3fv(body);
        gl_box(-hw + 0.1f, 1.45f, -L * 0.31f, hw - 0.1f, 1.55f, L * 0.11f);
        if (st == CS_TAXI) {
            glColor3f(0.1f, 0.1f, 0.1f); gl_box(-0.35f, 1.55f, -0.35f, 0.35f, 1.8f, 0.05f);
            glColor3f(1, 1, 0.6f); gl_box(-0.3f, 1.58f, -0.37f, 0.3f, 1.77f, 0.07f);
            glColor3f(0.1f, 0.1f, 0.1f);
            for (int i = 0; i < 6; i++) gl_box(hw + 0.001f, 0.6f, -hl + 0.5f + i * 0.6f, hw + 0.02f, 0.75f, -hl + 0.8f + i * 0.6f);
        }
        if (st == CS_POLICE) {
            int ph = ((int)(g_gameTime * 6)) & 1;
            float on = wrecked ? 0.2f : 1.0f;
            glColor3f(0.2f, 0.2f, 0.22f); gl_box(-0.7f, 1.55f, -0.25f, 0.7f, 1.62f, 0.15f);
            glColor3f(ph ? on : 0.35f, 0.08f, 0.08f); gl_box(-0.65f, 1.62f, -0.2f, -0.05f, 1.78f, 0.1f);
            glColor3f(0.08f, 0.15f, !ph ? on : 0.35f); gl_box(0.05f, 1.62f, -0.2f, 0.65f, 1.78f, 0.1f);
        }
        break;
    case CS_TRUCK:
    case CS_ARMOR:
        gl_box(-hw, 0.45f, -hl, hw, 1.15f, hl);
        if (st == CS_TRUCK) {
            glColor3fv(glass); gl_box(-hw + 0.1f, 1.15f, L * 0.02f, hw - 0.1f, 1.8f, L * 0.3f);
            glColor3fv(body); gl_box(-hw + 0.08f, 1.8f, L * 0.0f, hw - 0.08f, 1.9f, L * 0.31f);
            glColor3f(body[0] * 0.8f, body[1] * 0.8f, body[2] * 0.8f);
            gl_box(-hw, 1.15f, -hl, -hw + 0.15f, 1.55f, -L * 0.02f);
            gl_box(hw - 0.15f, 1.15f, -hl, hw, 1.55f, -L * 0.02f);
            gl_box(-hw, 1.15f, -hl, hw, 1.55f, -hl + 0.15f);
        } else {
            gl_box(-hw + 0.05f, 1.15f, -hl + 0.2f, hw - 0.05f, 2.1f, L * 0.3f);
            glColor3fv(glass); gl_box(-hw + 0.2f, 1.55f, L * 0.3f, hw - 0.2f, 1.9f, L * 0.31f);
            glColor3f(0.95f * k, 0.75f * k, 0.15f * k);
            gl_box(-hw - 0.01f, 0.85f, -hl, hw + 0.01f, 0.95f, hl);
            gl_box(-hw + 0.3f, 2.1f, -0.4f, hw - 0.3f, 2.2f, 0.4f);
        }
        break;
    case CS_VAN:
    case CS_ICECREAM:
        gl_box(-hw, 0.35f, -hl, hw, 2.05f, hl * 0.8f);
        gl_box(-hw, 0.35f, hl * 0.8f, hw, 1.1f, hl);
        glColor3fv(glass);
        gl_box(-hw + 0.1f, 1.2f, hl * 0.8f - 0.01f, hw - 0.1f, 1.85f, hl * 0.8f + 0.02f);
        gl_box(-hw - 0.01f, 1.25f, hl * 0.3f, hw + 0.01f, 1.8f, hl * 0.75f);
        if (st == CS_ICECREAM) {
            glColor3f(1, 1, 1); gl_box(-hw - 0.01f, 0.4f, -hl + 0.2f, hw + 0.01f, 0.7f, hl * 0.78f);
            glColor3f(0.85f, 0.6f, 0.3f); gl_frustum_y(0, 2.05f, -0.4f, 0.05f, 0.45f, 0.9f, 8);
            glColor3f(1.0f, 0.6f, 0.75f); gl_frustum_y(0, 2.95f, -0.4f, 0.5f, 0.15f, 0.55f, 8);
        }
        break;
    case CS_SPORT:
        gl_box(-hw, 0.28f, -hl, hw, 0.78f, hl);
        glColor3fv(glass);
        gl_box(-hw + 0.18f, 0.78f, -L * 0.22f, hw - 0.18f, 1.18f, L * 0.1f);
        glColor3fv(body);
        gl_box(-hw + 0.15f, 1.15f, -L * 0.2f, hw - 0.15f, 1.22f, L * 0.05f);
        glColor3f(0.1f, 0.1f, 0.1f);
        gl_box(-hw + 0.2f, 0.78f, -hl + 0.1f, -hw + 0.3f, 1.15f, -hl + 0.25f);
        gl_box(hw - 0.3f, 0.78f, -hl + 0.1f, hw - 0.2f, 1.15f, -hl + 0.25f);
        glColor3fv(body);
        gl_box(-hw, 1.15f, -hl, hw, 1.24f, -hl + 0.45f);
        glColor3f(1, 1, 1);
        gl_box(-0.25f, 0.781f, -hl, -0.1f, 0.79f, hl);
        gl_box(0.1f, 0.781f, -hl, 0.25f, 0.79f, hl);
        break;
    case CS_HOTROD:
        gl_box(-hw * 0.8f, 0.4f, -hl, hw * 0.8f, 0.95f, hl * 0.4f);
        gl_box(-hw * 0.5f, 0.4f, hl * 0.4f, hw * 0.5f, 0.85f, hl);
        glColor3f(0.75f, 0.75f, 0.8f);
        gl_box(-0.35f, 0.85f, hl * 0.45f, 0.35f, 1.25f, hl * 0.9f);
        glColor3f(0.9f, 0.85f, 0.7f);
        gl_box(-hw * 0.8f, 0.95f, -hl * 0.6f, hw * 0.8f, 1.45f, -hl * 0.1f);
        glColor3f(1, 0.85f * k, 0.1f * k);
        gl_box(-hw * 0.81f, 0.6f, -hl * 0.2f, hw * 0.81f, 0.75f, hl * 0.4f);
        glColor3f(0.6f, 0.6f, 0.65f);
        gl_box(hw * 0.8f, 0.4f, -hl * 0.1f, hw * 0.8f + 0.12f, 0.52f, hl * 0.5f);
        gl_box(-hw * 0.8f - 0.12f, 0.4f, -hl * 0.1f, -hw * 0.8f, 0.52f, hl * 0.5f);
        break;
    case CS_ROCKET:
        gl_box(-hw * 0.7f, 0.3f, -hl, hw * 0.7f, 0.85f, hl);
        gl_box(-hw, 0.3f, -hl * 0.6f, hw, 0.6f, hl * 0.5f);
        glColor3fv(glass);
        gl_box(-0.35f, 0.85f, -L * 0.05f, 0.35f, 1.2f, L * 0.2f);
        glColor3fv(body);
        gl_box(-0.06f, 0.85f, -hl, 0.06f, 1.75f, -hl + 0.9f);
        gl_box(-hw - 0.3f, 0.45f, -hl, -hw, 0.55f, -hl + 0.8f);
        gl_box(hw, 0.45f, -hl, hw + 0.3f, 0.55f, -hl + 0.8f);
        glColor3f(0.35f, 0.35f, 0.4f);
        gl_cyl_x(0, 0.58f, -hl - 0.1f, 0.25f, 0.25f, 8);
        break;
    }
    /* bumpers and lights */
    if (st != CS_HOTROD && st != CS_ROCKET) {
        glColor3fv(dark);
        float by = (st == CS_TRUCK || st == CS_ARMOR) ? 0.45f : 0.3f;
        gl_box(-hw - 0.02f, by, hl - 0.05f, hw + 0.02f, by + 0.2f, hl + 0.1f);
        gl_box(-hw - 0.02f, by, -hl - 0.1f, hw + 0.02f, by + 0.2f, -hl + 0.05f);
    }
    float ly = (st == CS_TRUCK || st == CS_ARMOR) ? 0.85f : (st == CS_SPORT ? 0.55f : 0.68f);
    glColor3f(1.0f * k, 1.0f * k, 0.75f * k);
    gl_box(-hw + 0.15f, ly, hl - 0.02f, -hw + 0.5f, ly + 0.16f, hl + 0.03f);
    gl_box(hw - 0.5f, ly, hl - 0.02f, hw - 0.15f, ly + 0.16f, hl + 0.03f);
    glColor3f(0.9f * k, 0.1f, 0.1f);
    gl_box(-hw + 0.15f, ly, -hl - 0.03f, -hw + 0.45f, ly + 0.14f, -hl + 0.02f);
    gl_box(hw - 0.45f, ly, -hl - 0.03f, hw - 0.15f, ly + 0.14f, -hl + 0.02f);
    float wx = hw - 0.08f;
    if (st == CS_HOTROD) {
        wheel(-wx, 0.5f, -wz, 0.5f, wr, 0);
        wheel(wx, 0.5f, -wz, 0.5f, wr, 0);
        wheel(-wx * 0.75f, 0.33f, wz, 0.33f, wr, steer);
        wheel(wx * 0.75f, 0.33f, wz, 0.33f, wr, steer);
    } else {
        wheel(-wx, rad, -wz, rad, wr, 0);
        wheel(wx, rad, -wz, rad, wr, 0);
        wheel(-wx, rad, wz, rad, wr, steer);
        wheel(wx, rad, wz, rad, wr, steer);
    }
}
void draw_car(const Car *c) {
    glPushMatrix();
    glTranslatef(c->pos.x, c->pos.y, c->pos.z);
    glRotatef(c->yaw * DEG, 0, 1, 0);
    glRotatef(-c->pitch * DEG, 1, 0, 0);
    glRotatef(c->roll * DEG, 0, 0, 1);
    if (g_carTypes[c->type].kind != VK_CAR) draw_aircraft_body(c->type, c->col, c->rotor, c->wrecked, c->onGround ? 1.0f : 0.0f);
    else draw_car_body(c->type, c->col, c->wheelRot, c->steerVis, c->wrecked);
    glPopMatrix();
}

/* ------------------------------------------------------------ aircraft */
static void tri3(float ax, float ay, float az, float bx, float by, float bz, float cx, float cy, float cz) {
    glBegin(GL_TRIANGLES);
    glNormal3f(0, 1, 0);
    glVertex3f(ax, ay, az); glVertex3f(bx, by, bz); glVertex3f(cx, cy, cz);
    glEnd();
}
static void prop(float z, float y, float rot, float r) {
    glPushMatrix();
    glTranslatef(0, y, z);
    glRotatef(rot * DEG, 0, 0, 1);
    glColor3f(0.15f, 0.15f, 0.15f);
    gl_box(-r, -0.09f, -0.03f, r, 0.09f, 0.03f);
    gl_box(-0.09f, -r, -0.03f, 0.09f, r, 0.03f);
    glColor3f(0.85f, 0.85f, 0.85f);
    gl_box(-0.15f, -0.15f, -0.05f, 0.15f, 0.15f, 0.2f);
    glPopMatrix();
}
static void rotor_top(float y, float rot, float r, int blades) {
    glPushMatrix();
    glTranslatef(0, y, 0);
    glRotatef(rot * DEG, 0, 1, 0);
    glColor3f(0.12f, 0.12f, 0.13f);
    for (int i = 0; i < blades; i++) {
        glPushMatrix();
        glRotatef(i * 360.0f / blades, 0, 1, 0);
        gl_box(-0.16f, -0.04f, 0, 0.16f, 0.04f, r);
        glPopMatrix();
    }
    gl_box(-0.2f, -0.12f, -0.2f, 0.2f, 0.08f, 0.2f);
    glPopMatrix();
}
void draw_aircraft_body(int type, const float *col, float rot, int wrecked, float gear) {
    const CarType *t = &g_carTypes[type];
    float k = wrecked ? 0.22f : 1.0f;
    float body[3] = {col[0] * k, col[1] * k, col[2] * k};
    float acc[3] = {(1 - col[0] * 0.6f) * k, (1 - col[1] * 0.6f) * k, (1 - col[2] * 0.3f) * k};
    float glass[3] = {0.25f * k + 0.05f, 0.35f * k + 0.05f, 0.5f * k + 0.05f};
    switch (t->style) {
    case CS_PLANE:
    case CS_BIPLANE: {
        int bi = t->style == CS_BIPLANE;
        glColor3fv(body);
        gl_box(-0.6f, 0.9f, -3.6f, 0.6f, 1.9f, 3.0f);
        gl_box(-0.45f, 1.0f, 3.0f, 0.45f, 1.8f, 3.8f);
        glColor3fv(glass);
        if (bi) gl_box(-0.4f, 1.9f, 0.0f, 0.4f, 2.15f, 0.4f);
        else gl_box(-0.5f, 1.9f, 0.4f, 0.5f, 2.45f, 1.9f);
        glColor3fv(body);
        if (bi) {
            gl_box(-4.6f, 1.0f, 0.6f, 4.6f, 1.15f, 2.1f);
            gl_box(-4.8f, 2.6f, 0.9f, 4.8f, 2.75f, 2.4f);
            glColor3f(0.3f * k, 0.25f * k, 0.2f * k);
            for (int sd = -1; sd <= 1; sd += 2) {
                gl_box(sd * 3.6f - 0.05f, 1.15f, 1.2f, sd * 3.6f + 0.05f, 2.6f, 1.3f);
                gl_box(sd * 1.0f - 0.05f, 1.8f, 1.4f, sd * 1.0f + 0.05f, 2.6f, 1.5f);
            }
            glColor3fv(acc);
            gl_box(-4.85f, 2.58f, 0.88f, -4.0f, 2.77f, 2.42f);
            gl_box(4.0f, 2.58f, 0.88f, 4.85f, 2.77f, 2.42f);
        } else {
            gl_box(-5.0f, 1.55f, 0.3f, 5.0f, 1.72f, 2.0f);
            glColor3fv(acc);
            gl_box(-5.05f, 1.53f, 0.28f, -4.1f, 1.74f, 2.02f);
            gl_box(4.1f, 1.53f, 0.28f, 5.05f, 1.74f, 2.02f);
            gl_box(-0.61f, 1.3f, -3.0f, 0.61f, 1.45f, 2.8f);
        }
        glColor3fv(body);
        gl_box(-0.08f, 1.9f, -3.6f, 0.08f, 3.3f, -2.7f);
        gl_box(-1.9f, 1.8f, -3.6f, 1.9f, 1.95f, -2.8f);
        glColor3fv(acc);
        gl_box(-0.09f, 2.8f, -3.62f, 0.09f, 3.32f, -3.0f);
        prop(3.85f, 1.4f, rot, 1.35f);
        if (gear > 0.5f || wrecked) {
            glColor3f(0.3f, 0.3f, 0.32f);
            gl_box(-1.05f, 0.35f, 1.15f, -0.95f, 0.95f, 1.25f);
            gl_box(0.95f, 0.35f, 1.15f, 1.05f, 0.95f, 1.25f);
            glColor3f(0.1f, 0.1f, 0.1f);
            gl_cyl_x(-1.0f, 0.35f, 1.2f, 0.35f, 0.12f, 10);
            gl_cyl_x(1.0f, 0.35f, 1.2f, 0.35f, 0.12f, 10);
            gl_cyl_x(0, 0.2f, -3.2f, 0.2f, 0.08f, 8);
            glColor3f(0.3f, 0.3f, 0.32f);
            gl_box(-0.05f, 0.2f, -3.25f, 0.05f, 0.95f, -3.15f);
        }
    } break;
    case CS_JET: {
        glColor3fv(body);
        gl_box(-0.7f, 0.9f, -4.6f, 0.7f, 1.9f, 3.2f);
        gl_box(-0.45f, 1.05f, 3.2f, 0.45f, 1.7f, 4.6f);
        gl_box(-0.25f, 1.15f, 4.6f, 0.25f, 1.5f, 5.3f);
        glColor3fv(glass);
        gl_box(-0.42f, 1.9f, 1.4f, 0.42f, 2.4f, 3.2f);
        glColor3fv(body);
        tri3(-0.7f, 1.4f, 1.5f, -5.2f, 1.4f, -3.0f, -0.7f, 1.4f, -3.6f);
        tri3(0.7f, 1.4f, 1.5f, 5.2f, 1.4f, -3.0f, 0.7f, 1.4f, -3.6f);
        tri3(-0.7f, 1.38f, 1.5f, -5.2f, 1.38f, -3.0f, -0.7f, 1.38f, -3.6f);
        tri3(0.7f, 1.38f, 1.5f, 5.2f, 1.38f, -3.0f, 0.7f, 1.38f, -3.6f);
        glColor3fv(acc);
        gl_box(-0.6f, 1.9f, -4.6f, -0.45f, 3.4f, -3.2f);
        gl_box(0.45f, 1.9f, -4.6f, 0.6f, 3.4f, -3.2f);
        gl_box(-2.2f, 1.35f, -4.6f, 2.2f, 1.45f, -3.8f);
        glColor3f(0.85f * k, 0.85f * k, 0.88f * k);
        gl_box(-3.3f, 1.05f, -1.5f, -3.1f, 1.3f, 0.6f);
        gl_box(3.1f, 1.05f, -1.5f, 3.3f, 1.3f, 0.6f);
        glColor3f(1.0f * k, 0.55f * k, 0.15f * k);
        gl_box(-0.5f, 1.05f, -4.75f, 0.5f, 1.75f, -4.6f);
        if (gear > 0.5f || wrecked) {
            glColor3f(0.1f, 0.1f, 0.1f);
            gl_cyl_x(-1.1f, 0.3f, -1.0f, 0.3f, 0.12f, 10);
            gl_cyl_x(1.1f, 0.3f, -1.0f, 0.3f, 0.12f, 10);
            gl_cyl_x(0, 0.3f, 3.2f, 0.3f, 0.1f, 10);
            glColor3f(0.3f, 0.3f, 0.32f);
            gl_box(-1.15f, 0.3f, -1.05f, -1.05f, 0.95f, -0.95f);
            gl_box(1.05f, 0.3f, -1.05f, 1.15f, 0.95f, -0.95f);
            gl_box(-0.05f, 0.3f, 3.15f, 0.05f, 0.95f, 3.25f);
        }
    } break;
    case CS_HELI:
    case CS_GUNSHIP: {
        int gs = t->style == CS_GUNSHIP;
        float sc = gs ? 1.2f : 1.0f;
        glPushMatrix();
        glScalef(sc, sc, sc);
        glColor3fv(body);
        gl_box(-1.1f, 0.55f, -1.2f, 1.1f, 2.3f, 1.7f);
        glColor3fv(glass);
        gl_box(-1.0f, 0.95f, 1.7f, 1.0f, 2.2f, 2.4f);
        glColor3fv(body);
        gl_box(-0.28f, 1.45f, -5.2f, 0.28f, 1.95f, -1.2f);
        gl_box(-0.06f, 1.5f, -5.4f, 0.06f, 2.9f, -4.7f);
        glColor3fv(acc);
        gl_box(-1.11f, 1.2f, -1.21f, 1.11f, 1.4f, 1.71f);
        glColor3f(0.25f, 0.25f, 0.27f);
        for (int sd = -1; sd <= 1; sd += 2) {
            gl_box(sd * 0.95f - 0.08f, 0.0f, -1.3f, sd * 0.95f + 0.08f, 0.1f, 2.0f);
            gl_box(sd * 0.95f - 0.05f, 0.1f, -0.6f, sd * 0.95f + 0.05f, 0.6f, -0.5f);
            gl_box(sd * 0.95f - 0.05f, 0.1f, 1.0f, sd * 0.95f + 0.05f, 0.6f, 1.1f);
        }
        if (gs) {
            glColor3fv(body);
            gl_box(-2.4f, 1.1f, -0.2f, 2.4f, 1.25f, 0.8f);
            glColor3f(0.2f, 0.22f, 0.2f);
            for (int sd = -1; sd <= 1; sd += 2) {
                glPushMatrix(); glTranslatef(sd * 2.2f, 0.95f, 0.3f); glRotatef(90, 0, 1, 0);
                gl_cyl_x(0, 0, 0, 0.22f, 0.8f, 8); glPopMatrix();
            }
            gl_box(-0.08f, 0.45f, 2.0f, 0.08f, 0.6f, 2.9f);
        }
        glColor3f(0.3f, 0.3f, 0.32f);
        gl_box(-0.12f, 2.3f, -0.12f, 0.12f, 2.7f, 0.12f);
        rotor_top(2.72f, rot, gs ? 6.0f : 5.2f, gs ? 4 : 2);
        glPushMatrix();
        glTranslatef(0.12f, 2.4f, -5.1f);
        glRotatef(rot * 3 * DEG, 1, 0, 0);
        glColor3f(0.12f, 0.12f, 0.13f);
        gl_box(-0.03f, -0.7f, -0.08f, 0.03f, 0.7f, 0.08f);
        glPopMatrix();
        glPopMatrix();
    } break;
    }
}

void blob_shadow(float x, float y, float z, float rx, float rz, float yaw, float a) {
    glPushMatrix();
    glTranslatef(x, y + 0.05f, z);
    glRotatef(yaw * DEG, 0, 1, 0);
    glScalef(rx, 1, rz);
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(0, 0, 0, a);
    glVertex3f(0, 0, 0);
    glColor4f(0, 0, 0, 0);
    for (int i = 0; i <= 16; i++) { float an = 2 * PI * i / 16; glVertex3f(cosf(an), 0, sinf(an)); }
    glEnd();
    glPopMatrix();
}

/* ------------------------------------------------------------------ PNG */
static unsigned crc_tab[256];
static void crc_init(void) {
    for (unsigned n = 0; n < 256; n++) {
        unsigned c = n;
        for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_tab[n] = c;
    }
}
static unsigned crc_upd(unsigned c, const unsigned char *b, size_t n) {
    for (size_t i = 0; i < n; i++) c = crc_tab[(c ^ b[i]) & 255] ^ (c >> 8);
    return c;
}
static void be32(unsigned char *p, unsigned v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static void chunk(FILE *f, const char *type, const unsigned char *d, unsigned len) {
    unsigned char h[8];
    be32(h, len);
    memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f);
    if (len) fwrite(d, 1, len, f);
    unsigned c = crc_upd(0xFFFFFFFFu, (const unsigned char *)type, 4);
    c = crc_upd(c, d, len) ^ 0xFFFFFFFFu;
    unsigned char cb[4];
    be32(cb, c);
    fwrite(cb, 1, 4, f);
}
void save_png(const char *path, int w, int h, const unsigned char *rgb) {
    crc_init();
    FILE *f = fopen(path, "wb");
    if (!f) return;
    static const unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    fwrite(sig, 1, 8, f);
    unsigned char ihdr[13];
    be32(ihdr, (unsigned)w);
    be32(ihdr + 4, (unsigned)h);
    ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);
    size_t rowb = (size_t)w * 3 + 1, raw = rowb * h;
    unsigned char *img = malloc(raw);
    for (int y = 0; y < h; y++) {
        img[y * rowb] = 0;
        memcpy(img + y * rowb + 1, rgb + (size_t)(h - 1 - y) * w * 3, (size_t)w * 3);
    }
    size_t nblk = (raw + 65534) / 65535;
    size_t zl = 2 + raw + nblk * 5 + 4;
    unsigned char *z = malloc(zl), *q = z;
    *q++ = 0x78; *q++ = 0x01;
    unsigned a = 1, b = 0;
    for (size_t i = 0; i < raw; i++) { a = (a + img[i]) % 65521; b = (b + a) % 65521; }
    for (size_t off = 0; off < raw; off += 65535) {
        unsigned len = (unsigned)(raw - off > 65535 ? 65535 : raw - off);
        *q++ = (off + len >= raw) ? 1 : 0;
        *q++ = len & 255; *q++ = len >> 8;
        *q++ = (~len) & 255; *q++ = ((~len) >> 8) & 255;
        memcpy(q, img + off, len);
        q += len;
    }
    be32(q, (b << 16) | a);
    q += 4;
    chunk(f, "IDAT", z, (unsigned)(q - z));
    chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(img);
    free(z);
}
