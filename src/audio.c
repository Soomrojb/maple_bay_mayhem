/* Audio: ALSA is loaded at runtime with dlopen, so the game still runs (silently)
 * when no sound system is present. All sounds and music are synthesized. */
#include "game.h"
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>

#define RATE 22050
#define MAXV 24

typedef struct { float *d; int n; } Sample;
typedef struct { int id, active; float pos, step, vol; } Voice;

static Sample smp[SND_COUNT];
static Voice voices[MAXV];
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_t th;
static volatile int running = 0;
static void *pcm = NULL, *lib = NULL;
static volatile float engRpm = 0, engVol = 0;
static volatile int engKind = 0;
static float *music;
static int musicLen, musicPos;

static int (*p_open)(void **, const char *, int, int);
static int (*p_set)(void *, int, int, unsigned, unsigned, int, unsigned);
static long (*p_writei)(void *, const void *, unsigned long);
static int (*p_recover)(void *, int, int);
static int (*p_close)(void *);
static int (*p_drop)(void *);

static unsigned nseed = 22222;
static float noise(void) { nseed = nseed * 1103515245u + 12345u; return ((nseed >> 9) & 0xFFFF) / 32768.0f - 1.0f; }

static Sample mk(float secs) { Sample s; s.n = (int)(secs * RATE); s.d = calloc((size_t)s.n, sizeof(float)); return s; }
static float sq(float ph) { return fmodf(ph, 1.0f) < 0.5f ? 1.0f : -1.0f; }
static float tri(float ph) { float f = fmodf(ph, 1.0f); return f < 0.5f ? f * 4 - 1 : 3 - f * 4; }

/* add a tone: freq sweeps f0->f1, wave 0 sine 1 square 2 triangle */
static void tone(Sample *s, float start, float dur, float f0, float f1, int wave, float vol) {
    int a = (int)(start * RATE), n = (int)(dur * RATE);
    float ph = 0;
    for (int i = 0; i < n && a + i < s->n; i++) {
        float t = (float)i / n;
        float f = f0 + (f1 - f0) * t;
        ph += f / RATE;
        float v = wave == 0 ? sinf(ph * 2 * PI) : wave == 1 ? sq(ph) * 0.6f : tri(ph);
        float env = fminf(1.0f, i / (RATE * 0.004f)) * (1.0f - t);
        s->d[a + i] += v * env * vol;
    }
}
static void noisy(Sample *s, float start, float dur, float vol, float lp, float decay) {
    int a = (int)(start * RATE), n = (int)(dur * RATE);
    float y = 0;
    for (int i = 0; i < n && a + i < s->n; i++) {
        float t = (float)i / n;
        y += (noise() - y) * lp;
        s->d[a + i] += y * vol * powf(1.0f - t, decay);
    }
}

static void gen_samples(void) {
    Sample s;
    s = mk(0.3f); tone(&s, 0, 0.07f, 988, 988, 1, 0.35f); tone(&s, 0.06f, 0.22f, 1319, 1319, 1, 0.35f); smp[SND_COIN] = s;
    s = mk(0.2f); tone(&s, 0, 0.18f, 260, 640, 1, 0.3f); smp[SND_JUMP] = s;
    s = mk(0.5f); noisy(&s, 0, 0.45f, 1.0f, 0.35f, 2.5f); tone(&s, 0, 0.25f, 120, 50, 0, 0.6f); smp[SND_CRASH] = s;
    s = mk(0.35f); noisy(&s, 0, 0.3f, 0.9f, 0.6f, 3); tone(&s, 0, 0.15f, 180, 70, 0, 0.5f); smp[SND_BREAK] = s;
    s = mk(1.0f);
    tone(&s, 0.00f, 0.14f, 523, 523, 1, 0.3f); tone(&s, 0.12f, 0.14f, 659, 659, 1, 0.3f);
    tone(&s, 0.24f, 0.14f, 784, 784, 1, 0.3f); tone(&s, 0.36f, 0.6f, 1047, 1047, 1, 0.32f);
    tone(&s, 0.36f, 0.6f, 523, 523, 2, 0.3f); smp[SND_SUCCESS] = s;
    s = mk(0.9f); tone(&s, 0, 0.25f, 392, 380, 1, 0.3f); tone(&s, 0.25f, 0.25f, 311, 300, 1, 0.3f); tone(&s, 0.5f, 0.4f, 247, 200, 1, 0.3f); smp[SND_FAIL] = s;
    s = mk(0.05f); tone(&s, 0, 0.04f, 1400, 1200, 1, 0.25f); smp[SND_CLICK] = s;
    s = mk(0.45f); tone(&s, 0, 0.42f, 392, 392, 1, 0.25f); tone(&s, 0, 0.42f, 494, 494, 1, 0.25f); smp[SND_HORN] = s;
    s = mk(0.45f); tone(&s, 0, 0.08f, 1800, 1800, 1, 0.25f); tone(&s, 0.08f, 0.3f, 2400, 2400, 0, 0.35f); noisy(&s, 0, 0.05f, 0.3f, 0.9f, 1); smp[SND_BUY] = s;
    s = mk(0.3f); tone(&s, 0, 0.1f, 880, 880, 0, 0.4f); tone(&s, 0.08f, 0.2f, 1320, 1320, 0, 0.4f); smp[SND_CHECK] = s;
    s = mk(0.15f); noisy(&s, 0, 0.1f, 0.6f, 0.5f, 2); tone(&s, 0, 0.1f, 160, 80, 0, 0.5f); smp[SND_KICK] = s;
    s = mk(1.4f); noisy(&s, 0, 1.3f, 1.2f, 0.12f, 1.6f); tone(&s, 0, 0.6f, 90, 30, 0, 0.8f); smp[SND_EXPLODE] = s;
    s = mk(0.2f); tone(&s, 0, 0.16f, 660, 660, 1, 0.3f); smp[SND_BEEP] = s;
    s = mk(0.5f); tone(&s, 0, 0.45f, 1320, 1320, 1, 0.3f); smp[SND_GO] = s;
    s = mk(0.15f); tone(&s, 0, 0.12f, 140, 90, 0, 0.6f); noisy(&s, 0, 0.05f, 0.3f, 0.3f, 2); smp[SND_DOOR] = s;
    s = mk(0.9f);
    for (int i = 0; i < 6; i++) tone(&s, i * 0.08f, 0.3f, 784 * powf(1.26f, (float)i * 0.5f), 784 * powf(1.26f, (float)i * 0.5f), 0, 0.25f);
    smp[SND_GIZMO] = s;
    s = mk(0.25f); tone(&s, 0, 0.22f, 160, 150, 1, 0.3f); smp[SND_DENY] = s;
    s = mk(0.22f); noisy(&s, 0, 0.2f, 1.0f, 0.7f, 4); tone(&s, 0, 0.06f, 900, 200, 1, 0.3f); smp[SND_SHOT] = s;
    s = mk(0.45f); noisy(&s, 0, 0.42f, 1.3f, 0.4f, 3); tone(&s, 0, 0.12f, 220, 60, 0, 0.7f); smp[SND_SHOTGUN] = s;
    s = mk(0.6f); noisy(&s, 0, 0.55f, 1.1f, 0.85f, 5); tone(&s, 0, 0.3f, 1400, 300, 0, 0.25f); smp[SND_RIFLE] = s;
    s = mk(0.09f); noisy(&s, 0, 0.08f, 0.9f, 0.6f, 3); tone(&s, 0, 0.05f, 300, 120, 1, 0.3f); smp[SND_MG] = s;
    s = mk(0.9f); noisy(&s, 0, 0.85f, 0.7f, 0.08f, 1.2f); tone(&s, 0, 0.7f, 300, 900, 2, 0.15f); smp[SND_ROCKET] = s;
    s = mk(0.25f); tone(&s, 0, 0.2f, 330, 180, 1, 0.3f); noisy(&s, 0, 0.1f, 0.4f, 0.3f, 2); smp[SND_HURT] = s;
    s = mk(0.4f); tone(&s, 0, 0.04f, 2200, 2000, 1, 0.2f); tone(&s, 0.18f, 0.05f, 1500, 1300, 1, 0.25f); noisy(&s, 0.18f, 0.05f, 0.4f, 0.8f, 2); smp[SND_RELOAD] = s;
    s = mk(0.06f); tone(&s, 0, 0.04f, 2600, 2600, 1, 0.15f); smp[SND_EMPTY] = s;
    s = mk(0.6f); for (int i = 0; i < 4; i++) tone(&s, i * 0.1f, 0.2f, 523 * powf(1.19f, (float)i), 523 * powf(1.19f, (float)i), 0, 0.3f); smp[SND_HEAL] = s;
    s = mk(0.2f); noisy(&s, 0, 0.18f, 0.3f, 0.15f, 0.5f); smp[SND_FUEL] = s;
    s = mk(1.2f); tone(&s, 0, 0.6f, 700, 1100, 2, 0.3f); tone(&s, 0.6f, 0.6f, 1100, 700, 2, 0.3f); smp[SND_SIREN] = s;
    s = mk(1.0f); tone(&s, 0, 0.2f, 660, 660, 1, 0.3f); tone(&s, 0.25f, 0.2f, 660, 660, 1, 0.3f); tone(&s, 0.5f, 0.45f, 440, 400, 1, 0.3f); smp[SND_BUSTED] = s;
}

/* a cheerful little chiptune loop, rendered once */
static void gen_music(void) {
    const float bpm = 128;
    const float beat = 60.0f / bpm;
    const int bars = 8;
    musicLen = (int)(bars * 4 * beat * RATE);
    music = calloc((size_t)musicLen, sizeof(float));
    /* chord roots (MIDI) per bar: C G Am F C G F G */
    const int roots[8] = {48, 43, 45, 41, 48, 43, 41, 43};
    const int minor[8] = {0, 0, 1, 0, 0, 0, 0, 0};
    const int scale[5] = {0, 2, 4, 7, 9};
    unsigned rs = 99;
    int eighth = (int)(beat * 0.5f * RATE);
    for (int bar = 0; bar < bars; bar++) {
        int r = roots[bar];
        for (int e = 0; e < 8; e++) {
            int start = (int)((bar * 4 + e * 0.5f) * beat * RATE);
            /* bass: root/fifth on eighths, triangle */
            int bn = (e % 4 == 2) ? r + 7 : r;
            float bf = 440.0f * powf(2.0f, (bn - 69) / 12.0f);
            float ph = 0;
            for (int i = 0; i < eighth && start + i < musicLen; i++) {
                ph += bf / RATE;
                float env = 1.0f - (float)i / eighth * 0.7f;
                music[start + i] += tri(ph) * 0.22f * env;
            }
            /* lead melody */
            rs = rs * 1664525u + 1013904223u;
            int play = ((rs >> 16) % 10) < 7;
            if (bar % 4 == 3 && e >= 6) play = 0;
            if (play) {
                int deg = (rs >> 20) % 5;
                int oct = ((rs >> 24) % 3 == 0) ? 12 : 0;
                int note = r + 24 + scale[deg] + oct - (minor[bar] && deg == 2 ? 1 : 0);
                float f = 440.0f * powf(2.0f, (note - 69) / 12.0f);
                int len = ((rs >> 26) & 1) ? eighth * 2 : eighth;
                ph = 0;
                for (int i = 0; i < len && start + i < musicLen; i++) {
                    ph += f / RATE;
                    float env = fminf(1.0f, i / 200.0f) * expf(-3.0f * i / (float)len);
                    music[start + i] += sq(ph) * 0.07f * env + sinf(ph * 2 * PI) * 0.06f * env;
                }
            }
            /* drums */
            int kick = (e == 0 || e == 4);
            int snare = (e == 2 || e == 6);
            float kp = 0;
            for (int i = 0; i < eighth && start + i < musicLen; i++) {
                float t = (float)i / RATE;
                float v = 0;
                if (kick && t < 0.15f) { kp += (120 * expf(-t * 25) + 45) / RATE; v += sinf(kp * 2 * PI) * 0.45f * (1 - t / 0.15f); }
                if (snare && t < 0.12f) v += noise() * 0.16f * (1 - t / 0.12f);
                if (t < 0.03f) v += noise() * 0.05f * (1 - t / 0.03f);
                music[start + i] += v;
            }
        }
    }
}

static void *audio_thread(void *arg) {
    (void)arg;
    enum { N = 512 };
    float mix[N];
    short out[N];
    float ph1 = 0, ph2 = 0, ny = 0;
    float rpmS = 0, volS = 0;
    while (running) {
        memset(mix, 0, sizeof mix);
        float master = g_set.volume / 10.0f;
        float mus = g_set.music / 10.0f * 0.55f;
        pthread_mutex_lock(&mtx);
        for (int v = 0; v < MAXV; v++) {
            Voice *vo = &voices[v];
            if (!vo->active) continue;
            Sample *s = &smp[vo->id];
            for (int i = 0; i < N; i++) {
                int ix = (int)vo->pos;
                if (ix >= s->n - 1) { vo->active = 0; break; }
                float fr = vo->pos - ix;
                mix[i] += (s->d[ix] * (1 - fr) + s->d[ix + 1] * fr) * vo->vol;
                vo->pos += vo->step;
            }
        }
        pthread_mutex_unlock(&mtx);
        float tr = engRpm, tv = engVol;
        for (int i = 0; i < N; i++) {
            rpmS += (tr - rpmS) * 0.002f;
            volS += (tv - volS) * 0.002f;
            float f = 38 + rpmS * 110;
            ph1 += f / RATE;
            ph2 += f * 0.5f / RATE;
            ny += (noise() - ny) * 0.2f;
            float saw = fmodf(ph1, 1.0f) * 2 - 1;
            float e = saw * 0.5f + sq(ph2) * 0.3f + ny * 0.25f;
            if (engKind == 2) { /* rotor: chopping noise */
                float chop = 0.5f + 0.5f * sinf(ph2 * 0.18f * 2 * PI);
                e = ny * 1.3f * chop * chop + saw * 0.15f;
            } else if (engKind == 1) e = saw * 0.35f + ny * 0.6f;
            mix[i] += e * volS * 0.18f;
            if (music && mus > 0) mix[i] += music[musicPos] * mus;
            if (music) musicPos = (musicPos + 1) % musicLen;
        }
        for (int i = 0; i < N; i++) {
            float x = mix[i] * master;
            x = x / (1.0f + fabsf(x) * 0.6f);
            out[i] = (short)(clampf(x, -1, 1) * 32000);
        }
        long r = p_writei(pcm, out, N);
        if (r < 0) {
            if (p_recover(pcm, (int)r, 1) < 0) usleep(10000);
        }
    }
    return NULL;
}

void audio_init(void) {
    lib = dlopen("libasound.so.2", RTLD_NOW);
    if (!lib) { fprintf(stderr, "[audio] libasound not found - running silent\n"); return; }
    p_open = dlsym(lib, "snd_pcm_open");
    p_set = dlsym(lib, "snd_pcm_set_params");
    p_writei = dlsym(lib, "snd_pcm_writei");
    p_recover = dlsym(lib, "snd_pcm_recover");
    p_close = dlsym(lib, "snd_pcm_close");
    p_drop = dlsym(lib, "snd_pcm_drop");
    if (!p_open || !p_set || !p_writei || !p_recover || !p_close) { fprintf(stderr, "[audio] missing ALSA symbols\n"); return; }
    if (p_open(&pcm, "default", 0, 0) < 0) { fprintf(stderr, "[audio] cannot open audio device\n"); pcm = NULL; return; }
    /* format 2 = S16_LE, access 3 = RW_INTERLEAVED */
    if (p_set(pcm, 2, 3, 1, RATE, 1, 70000) < 0) { fprintf(stderr, "[audio] cannot configure device\n"); p_close(pcm); pcm = NULL; return; }
    gen_samples();
    gen_music();
    running = 1;
    if (pthread_create(&th, NULL, audio_thread, NULL) != 0) { running = 0; p_close(pcm); pcm = NULL; }
}

void audio_shutdown(void) {
    if (running) {
        running = 0;
        pthread_join(th, NULL);
    }
    if (pcm) {
        if (p_drop) p_drop(pcm);
        p_close(pcm);
        pcm = NULL;
    }
}

void audio_play(int id, float vol, float pitch) {
    if (!running || id < 0 || id >= SND_COUNT) return;
    pthread_mutex_lock(&mtx);
    int best = -1;
    for (int v = 0; v < MAXV; v++) if (!voices[v].active) { best = v; break; }
    if (best < 0) best = rand() % MAXV;
    voices[best].id = id;
    voices[best].pos = 0;
    voices[best].step = pitch;
    voices[best].vol = vol;
    voices[best].active = 1;
    pthread_mutex_unlock(&mtx);
}

void audio_engine_kind(int kind) { engKind = kind; }
void audio_engine(float rpm01, float vol) {
    engRpm = clampf(rpm01, 0, 1.5f);
    engVol = clampf(vol, 0, 1);
}
