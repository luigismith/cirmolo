/* Fioritura per Miyoo Flip - applicazione (vedi fio_app.h).
 *
 * Tempo: tutta la musica (echi del giardino, passi delle lucciole, sogno) si decide qui, nel thread
 * dell'interfaccia, con LOOKAHEAD secondi di anticipo sull'orologio del motore (fio_clock): le note
 * partono con il loro istante esatto, e la luce di ogni fiore si accende quando la nota esce davvero
 * (la coda VIS aspetta l'istante, meno un blocco audio).
 */
#include "fio_app.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#define MKDIR(p) mkdir(p, 0755)
#endif

#include "fio_draw.h"
#include "fio_ear.h"
#include "fio_lp.h"
#include "midi.h"
#include "platform.h"

#define W 640
#define H 480
#define G_TOP 62.0f                 /* corda piu' acuta del giardino */
#define G_BOT 440.0f                /* corda piu' grave */
#define G_LEFT 34.0f
#define G_RIGHT 606.0f
#define LOOKAHEAD 0.12              /* secondi di anticipo con cui si preparano le note */
#define DREAM_IDLE 6.0f             /* secondi senza toccare niente prima che il sogno cominci */
#define MAX_SEEDS 96
#define MAX_FF 64
#define MAX_BLOOMS 110
#define MAX_VIS 768
#define N_POLLEN 46
#define GRID 8
#define CELL 44.0f
#define GAP 4.0f
#define GRID_X 130.0f                /* griglia di 380 px al centro, bordi lontani da scritte e note */
#define GRID_Y 56.0f
#define PI_F 3.14159265f
#define BG_EVERY 4
#define RIBBON 80                   /* punti della scia del canto */
#define MAX_PETALS 120
#define MAX_SPARKS 8
#define N_RECENT 32                 /* note suonate di recente: il giardino non deve sentire se stesso */

#define C_TEXT   RGB(240, 236, 250)
#define C_MUTED  RGB(170, 164, 198)
#define C_DIM    RGB(110, 106, 140)
#define C_GOLD   RGB(242, 196, 92)
#define C_VIOLET RGB(154, 124, 244)
#define C_RED    RGB(236, 96, 104)
#define C_GREEN  RGB(110, 198, 142)
#define C_PANEL  RGB(14, 13, 26)

/* ------------------------------------------------------------------ scale e atmosfere */
enum { SC_PENTA_MAJ, SC_PENTA_MIN, SC_MAJOR, SC_MINOR, SC_DORIAN, SC_LYDIAN, SC_MIXO, SC_INSEN, SC_HIRAJOSHI, SC_WHOLE, SC_COUNT };
typedef struct { const char *name; int n; int steps[8]; } Scale;
static const Scale SCALES[SC_COUNT] = {
    { "Pentatonica maggiore", 5, { 0, 2, 4, 7, 9 } },
    { "Pentatonica minore", 5, { 0, 3, 5, 7, 10 } },
    { "Maggiore", 7, { 0, 2, 4, 5, 7, 9, 11 } },
    { "Minore", 7, { 0, 2, 3, 5, 7, 8, 10 } },
    { "Dorica", 7, { 0, 2, 3, 5, 7, 9, 10 } },
    { "Lidia", 7, { 0, 2, 4, 6, 7, 9, 11 } },
    { "Misolidia", 7, { 0, 2, 4, 5, 7, 9, 10 } },
    { "In-sen", 5, { 0, 1, 5, 7, 10 } },
    { "Hirajoshi", 5, { 0, 2, 3, 7, 8 } },
    { "Esatonale", 6, { 0, 2, 4, 6, 8, 10 } },
};
static const char *NOTE_IT[12] = { "Do", "Do#", "Re", "Mib", "Mi", "Fa", "Fa#", "Sol", "Lab", "La", "Sib", "Si" };

typedef struct {
    const char *name, *desc;
    int timbre, scale, root;
    float bright, length, reverb, shimmer, delay, delay_time, drone;
    uint32_t top, bottom, haze;
    uint32_t col[3];                 /* fiori: dal grave all'acuto */
} Mood;

static const Mood MOODS[] = {
    { "Alba", "campane su una pentatonica, luce calda", TIM_CAMPANA, SC_PENTA_MAJ, 2, 0.45f, 5.5f, 0.70f, 0.35f, 0.22f, 0.48f, 0.25f,
      RGB(14, 12, 36), RGB(84, 40, 66), RGB(255, 140, 100), { RGB(255, 168, 88), RGB(255, 118, 150), RGB(255, 226, 150) } },
    { "Lago", "vetro in modo lidio, acqua ferma", TIM_VETRO, SC_LYDIAN, 5, 0.55f, 4.0f, 0.80f, 0.15f, 0.30f, 0.62f, 0.20f,
      RGB(4, 16, 28), RGB(12, 54, 74), RGB(50, 170, 190), { RGB(60, 200, 230), RGB(120, 240, 210), RGB(210, 250, 255) } },
    { "Bosco", "arpa in modo dorico, tra gli aghi del cirmolo", TIM_ARPA, SC_DORIAN, 9, 0.50f, 4.5f, 0.55f, 0.0f, 0.35f, 0.36f, 0.30f,
      RGB(5, 16, 11), RGB(20, 48, 30), RGB(100, 190, 130), { RGB(110, 198, 142), RGB(190, 230, 110), RGB(242, 196, 92) } },
    { "Notte", "soffi sulla pentatonica minore, cielo viola", TIM_SOFFIO, SC_PENTA_MIN, 1, 0.40f, 5.0f, 0.90f, 0.55f, 0.20f, 0.70f, 0.40f,
      RGB(7, 7, 20), RGB(34, 24, 68), RGB(150, 120, 240), { RGB(154, 124, 244), RGB(196, 176, 250), RGB(130, 170, 255) } },
    { "Pioggia", "gocce in maggiore, eco lunga", TIM_GOCCIA, SC_MAJOR, 7, 0.50f, 3.0f, 0.60f, 0.10f, 0.55f, 0.33f, 0.15f,
      RGB(11, 14, 22), RGB(38, 47, 62), RGB(110, 140, 180), { RGB(150, 190, 230), RGB(200, 220, 245), RGB(110, 150, 210) } },
    { "Neve", "kalimba sulla scala in-sen, silenzio bianco", TIM_KALIMBA, SC_INSEN, 4, 0.60f, 3.5f, 0.75f, 0.45f, 0.30f, 0.52f, 0.20f,
      RGB(14, 18, 32), RGB(60, 70, 98), RGB(190, 200, 235), { RGB(235, 240, 255), RGB(170, 200, 255), RGB(210, 190, 245) } },
};
#define MOOD_COUNT ((int)(sizeof(MOODS) / sizeof(MOODS[0])))

typedef struct { int timbre, scale, root; float bright, length, reverb, shimmer, delay, drone; } MoodSet;

/* ------------------------------------------------------------------ pannello */
enum { R_MOOD, R_TIMBRE, R_SCALE, R_ROOT, R_BRIGHT, R_LENGTH, R_RESPIRO, R_MEMORIA, R_VENTO, R_TEMPO,
       R_REVERB, R_SHIMMER, R_DELAY, R_DRONE, R_SOGNO, R_ASCOLTO, R_VOLUME, R_REC, R_RESET, R_COUNT };
static const char *ROW_NAMES[R_COUNT] = { "Atmosfera", "Strumento", "Scala", "Tonica", "Luce", "Coda", "Respiro",
    "Memoria", "Vento", "Tempo delle lucciole", "Spazio", "Shimmer", "Eco", "Bordone", "Sogno", "Ascolto", "Volume",
    "Registra in WAV", "Ripristina l'atmosfera" };
static const int MEMORIE[] = { 1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 0 };
#define N_MEMORIE ((int)(sizeof(MEMORIE) / sizeof(MEMORIE[0])))

/* ------------------------------------------------------------------ strutture */
typedef struct {
    int    alive, serial, dream;
    double phase;                    /* posizione nel giro, 0..1 */
    long long lap;                   /* giro del prossimo ritorno */
    float  x;                        /* 0..1 da sinistra */
    int    deg;                      /* corda della scala, o -1 per una nota fuori scala */
    float  note;                     /* nota MIDI quando deg < 0 */
    float  vel;
    int    done;                     /* ritorni gia' suonati */
    float  glow, ydraw;
} Seed;

enum { VIS_BLOOM, VIS_WALL };
typedef struct { long long when; int kind; float x, y, s; uint32_t col; int a, b; } Vis;
typedef struct { float x, y, s, age, life, rot; uint32_t col; int petals, small; } Bloom;
typedef struct { float x, y, ph, r, sp; } Pollen;
typedef struct { float x, y, vx, vy, age, life, r, ph; uint32_t col; } Petal;   /* petali portati dal soffio */
typedef struct { float x, y, vx, vy, age, life; } Spark;                       /* lucciola nata da un battito */

struct FioApp {
    Fio *dsp;
    float sr;
    char state_path[512], rec_dir[512];
    MoodSet set[MOOD_COUNT];
    int mood, mode;
    FioGlobal g;
    unsigned rng;
    float t;                         /* secondi dall'avvio */

    /* tempo musicale (campioni) */
    long long now, sched;
    double g_ph0, g_len;             /* giro del giardino: fase = g_ph0 + (t - g_t0) / g_len */
    long long g_t0;

    /* giardino */
    Seed seeds[MAX_SEEDS];
    int serial;
    float cx, cy, cy_draw;           /* cursore (pixel) */
    int cdeg;                        /* corda del cursore */
    int a_last_deg; float a_last_t;  /* A tenuto: scia di note */
    float dream_timer, gx, gy, gtx, gty; int gdeg; float gx_n; int dreaming;
    float line_glow[64];

    /* lucciole */
    Firefly ff[MAX_FF];
    int pr[MAX_FF], pc[MAX_FF], bump[MAX_FF];
    int nff, f_playing, f_steps;
    long long f_next, f_tcur;
    double f_len;
    int cell_r, cell_c;
    float wall[4][GRID];

    /* grafica */
    uint32_t *sky[MOOD_COUNT];
    int sky_from; float sky_t;
    uint32_t *bg; int bg_wait;      /* cielo + nebbie, rifatto ogni BG_EVERY disegni */
    Vis vis[MAX_VIS]; int nvis;
    Bloom blooms[MAX_BLOOMS]; int nblooms;
    Pollen pollen[N_POLLEN];
    float chrome, hot;
    int draw_toggle;

    /* comandi */
    int held[PAD_COUNT], eat[PAD_COUNT]; float held_t[PAD_COUNT], rep_t[PAD_COUNT];
    float lx, ly;
    float idle;
    int help, sel_down, b_used, panel, prow, pscroll, quit_dialog, quit;
    char toast[160]; float toast_t;

    /* MIDI e Launchpad */
    FioLp lp; char midi_dev[96]; int lp_base; float lp_flash[8][8]; float lp_timer;

    /* microfono: l'orecchio del giardino (canto = fiori, soffio = vento, battito = lucciole) */
    FioEar *ear;
    int mic; char mic_name[128];
    float ear_quiet;                 /* secondi dall'ultimo gesto sentito (il sogno aspetta anche questo) */
    float ear_busy;                  /* > 0: si sente qualcosa, si disegna piu' spesso */
    float pen_x, pen_last;           /* la penna del canto scrive da sinistra a destra */
    float rib_x[RIBBON], rib_y[RIBBON], rib_a[RIBBON], rib_t[RIBBON]; int rib_n, rib_w; float rib_push;
    float voice_y, voice_a;
    float gust, front_x, front_wait, sow_t, strum_t; int front_on;
    Petal petals[MAX_PETALS]; int npetals;
    Spark sparks[MAX_SPARKS]; int nsparks;
    double claps[6]; int nclaps;
    float ff_flash;
    float recent_note[N_RECENT], recent_t[N_RECENT]; int recent_w;
    float sres[64];                  /* corde che vibrano per simpatia */

    /* registrazione */
    FILE *rec; char rec_path[600]; long long rec_frames;

    int dirty; float save_t;
};

/* ------------------------------------------------------------------ aiuti */
static inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static inline float clamp01(float x) { return clampf(x, 0.0f, 1.0f); }
static inline int clampi(int x, int lo, int hi) { return x < lo ? lo : (x > hi ? hi : x); }
static inline int wrapi(int x, int n) { return ((x % n) + n) % n; }
static float frand(FioApp *a)
{
    a->rng ^= a->rng << 13; a->rng ^= a->rng >> 17; a->rng ^= a->rng << 5;
    return (float)(a->rng >> 8) / 16777216.0f;
}
static float smooth01(float t) { t = clamp01(t); return t * t * (3.0f - 2.0f * t); }

static void toast(FioApp *a, const char *msg)
{
    snprintf(a->toast, sizeof(a->toast), "%s", msg);
    a->toast_t = 2.6f;
}

static const MoodSet *mset(const FioApp *a) { return &a->set[a->mood]; }
static const Scale *scale_of(const FioApp *a) { return &SCALES[clampi(mset(a)->scale, 0, SC_COUNT - 1)]; }
static int g_ndeg(const FioApp *a) { return scale_of(a)->n * 3 + 1; }
/* tonica piu' grave del giardino: tra La2 (45) e Sol#3 (56) */
static int base_note(const FioApp *a) { return 45 + wrapi(mset(a)->root - 9, 12); }
static int deg_note(const FioApp *a, int d)
{
    const Scale *s = scale_of(a);
    if (d < 0) d = 0;
    return base_note(a) + 12 * (d / s->n) + s->steps[d % s->n];
}
static float deg_y(const FioApp *a, int d) { return G_BOT - (float)d * (G_BOT - G_TOP) / (float)(g_ndeg(a) - 1); }
static int y_deg(const FioApp *a, float y)
{
    int n = g_ndeg(a);
    float f = (G_BOT - y) / (G_BOT - G_TOP) * (float)(n - 1);
    return clampi((int)lroundf(f), 0, n - 1);
}
static float note_y(const FioApp *a, float note)
{
    return clampf(G_BOT - (note - (float)base_note(a)) / 36.0f * (G_BOT - G_TOP), G_TOP - 10.0f, G_BOT + 10.0f);
}
/* corda della scala per una nota MIDI, -1 se non c'e' */
static int note_deg(const FioApp *a, int note)
{
    int n = g_ndeg(a);
    for (int d = 0; d < n; d++) if (deg_note(a, d) == note) return d;
    return -1;
}
static float seed_note(const FioApp *a, const Seed *s) { return s->deg >= 0 ? (float)deg_note(a, clampi(s->deg, 0, g_ndeg(a) - 1)) : s->note; }
static float seed_y(const FioApp *a, const Seed *s) { return s->deg >= 0 ? deg_y(a, clampi(s->deg, 0, g_ndeg(a) - 1)) : note_y(a, s->note); }
static float x_pix(float x) { return G_LEFT + x * (G_RIGHT - G_LEFT); }
static float pan_of(float x) { return (x * 2.0f - 1.0f) * 0.85f; }
static void note_label(char *out, size_t n, int note) { snprintf(out, n, "%s%d", NOTE_IT[wrapi(note, 12)], note / 12 - 1); }

/* colore dei fiori per altezza: 0 grave .. 1 acuto */
static uint32_t palette(const FioApp *a, float h)
{
    const uint32_t *c = MOODS[a->mood].col;
    h = clamp01(h);
    return h < 0.5f ? gfx_mix(c[0], c[1], h * 2.0f) : gfx_mix(c[1], c[2], h * 2.0f - 1.0f);
}
static uint32_t palette_y(const FioApp *a, float y) { return palette(a, (G_BOT - y) / (G_BOT - G_TOP)); }

/* testo che sfuma nel cielo (gfx_text non ha la trasparenza) */
static uint32_t fade(const FioApp *a, uint32_t col, float alpha) { return gfx_mix(MOODS[a->mood].top, col, clamp01(alpha)); }

/* ------------------------------------------------------------------ motore */
static void push_mix(FioApp *a)
{
    const MoodSet *s = mset(a);
    FioMix m;
    m.reverb = s->reverb;
    m.shimmer = s->shimmer;
    m.delay = s->delay;
    m.delay_time = MOODS[a->mood].delay_time;
    m.drone = s->drone;
    m.drone_note = (float)(base_note(a) - 12);
    m.volume = a->g.volume;
    fio_set_mix(a->dsp, &m);
}

static void sound(FioApp *a, long long when, float note, float vel, float pan)
{
    const MoodSet *s = mset(a);
    FioNote n;
    n.timbre = s->timbre;
    n.note = note;
    n.vel = vel;
    n.pan = pan;
    n.bright = s->bright;
    n.length = s->length;
    fio_play(a->dsp, when, &n);
    a->recent_note[a->recent_w] = note;                 /* quando esce davvero, nel tempo dell'app */
    a->recent_t[a->recent_w] = a->t + (when > a->now ? (float)(when - a->now) / a->sr : 0.0f);
    a->recent_w = (a->recent_w + 1) % N_RECENT;
}

static void vis_push(FioApp *a, long long when, int kind, float x, float y, float s, uint32_t col, int ia, int ib)
{
    if (a->nvis >= MAX_VIS) return;
    Vis *v = &a->vis[a->nvis++];
    v->when = when; v->kind = kind; v->x = x; v->y = y; v->s = s; v->col = col; v->a = ia; v->b = ib;
}

/* ------------------------------------------------------------------ giardino */
static double g_phase_at(const FioApp *a, long long t) { return a->g_ph0 + (double)(t - a->g_t0) / a->g_len; }
static long long g_time_of(const FioApp *a, double ph) { return a->g_t0 + (long long)llround((ph - a->g_ph0) * a->g_len); }
static void g_set_len(FioApp *a, double seconds)
{
    double ph = g_phase_at(a, a->sched);
    a->g_ph0 = ph;
    a->g_t0 = a->sched;
    a->g_len = seconds * a->sr;
}

static int seed_count(const FioApp *a)
{
    int n = 0;
    for (int i = 0; i < MAX_SEEDS; i++) n += a->seeds[i].alive;
    return n;
}

static Seed *seed_new(FioApp *a)
{
    Seed *old = NULL;
    for (int i = 0; i < MAX_SEEDS; i++) {
        if (!a->seeds[i].alive) return &a->seeds[i];
        if (!old || a->seeds[i].serial < old->serial) old = &a->seeds[i];
    }
    return old;                     /* pieno: il piu' vecchio lascia il posto */
}

static void plant(FioApp *a, int deg, float note, float x, float vel, int dream)
{
    Seed *s = seed_new(a);
    memset(s, 0, sizeof(*s));
    s->alive = 1;
    s->serial = ++a->serial;
    s->dream = dream;
    s->deg = deg >= 0 ? clampi(deg, 0, g_ndeg(a) - 1) : -1;
    s->note = note;
    s->x = clamp01(x);
    s->vel = clamp01(vel);
    long long t0 = fio_clock(a->dsp);
    if (t0 < a->now) t0 = a->now;
    double ph = g_phase_at(a, t0);
    s->phase = ph - floor(ph);
    s->lap = (long long)floor(ph) + 1;
    s->glow = 1.0f;
    s->ydraw = seed_y(a, s);
    float n = seed_note(a, s);
    sound(a, t0, n, s->vel, pan_of(s->x));
    vis_push(a, t0, VIS_BLOOM, x_pix(s->x), s->ydraw, s->vel, palette_y(a, s->ydraw), s->deg, s->serial);
    a->dirty = 1;
}

/* Un ritorno del fiore all'istante t: sempre piu' piano, e il vento lo puo' spostare di una corda. */
static void echo(FioApp *a, Seed *s, long long t)
{
    s->done++;
    int M = a->g.memoria;
    float v = M > 0 ? s->vel * powf(1.0f - (float)s->done / (float)(M + 1), 0.85f) : s->vel * 0.82f;
    if (a->g.vento > 0.001f && s->deg >= 0 && frand(a) < a->g.vento * 0.5f) {
        int d = s->deg + (frand(a) < 0.5f ? -1 : 1);
        if (d >= 0 && d < g_ndeg(a)) s->deg = d;
        s->x = clampf(s->x + (frand(a) - 0.5f) * 0.08f, 0.02f, 0.98f);
    }
    float y = seed_y(a, s);
    sound(a, t, seed_note(a, s), v, pan_of(s->x));
    vis_push(a, t, VIS_BLOOM, x_pix(s->x), y, v, palette_y(a, y), s->deg, s->serial);
    if (M > 0 && s->done >= M) s->alive = 0;            /* l'ultima nota suona fino in fondo */
}

static void schedule_garden(FioApp *a, long long A, long long B)
{
    double pA = g_phase_at(a, A), pB = g_phase_at(a, B);
    for (int i = 0; i < MAX_SEEDS; i++) {
        Seed *s = &a->seeds[i];
        if (!s->alive) continue;
        double due = (double)s->lap + s->phase;
        while (s->alive && due < pB) {
            long long t = due >= pA ? g_time_of(a, due) : A;
            echo(a, s, t);
            s->lap++;
            due += 1.0;
        }
    }
}

static void undo_seed(FioApp *a)
{
    Seed *last = NULL;
    for (int i = 0; i < MAX_SEEDS; i++) {
        Seed *s = &a->seeds[i];
        if (s->alive && !s->dream && (!last || s->serial > last->serial)) last = s;
    }
    if (!last)
        for (int i = 0; i < MAX_SEEDS; i++) {
            Seed *s = &a->seeds[i];
            if (s->alive && (!last || s->serial > last->serial)) last = s;
        }
    if (last) { last->alive = 0; toast(a, "Tolto l'ultimo fiore"); a->dirty = 1; }
    else toast(a, "Il giardino e' vuoto");
}

static void clear_garden(FioApp *a)
{
    for (int i = 0; i < MAX_SEEDS; i++) a->seeds[i].alive = 0;
    toast(a, "Giardino svuotato");
    a->dirty = 1;
}

/* Sogno: un cursore fantasma che cammina per le corde (passi piccoli, ogni tanto un salto o una terza
   insieme) e pianta da solo, piu' fitto quando il giardino e' quasi vuoto. */
static void dream_next(FioApp *a)
{
    static const int STEPS[] = { -3, -2, -2, -1, -1, -1, 1, 1, 1, 2, 2, 3 };
    int n = g_ndeg(a);
    int d = a->gdeg + STEPS[(int)(frand(a) * 12.0f) % 12];
    if (d < n / 6) d += 2;
    if (d > n - 1 - n / 8) d -= 2;
    a->gdeg = clampi(d, 0, n - 1);
    a->gx_n = clampf(a->gx_n + (frand(a) - 0.5f) * 0.36f, 0.06f, 0.94f);
    a->gtx = x_pix(a->gx_n);
    a->gty = deg_y(a, a->gdeg);
    int alive = seed_count(a);
    a->dream_timer = alive < 5 ? 0.9f + frand(a) * 1.5f : 1.6f + frand(a) * 2.6f;
}

static void dream_tick(FioApp *a, float dt)
{
    int active = a->g.sogno && a->mode == MODE_GARDEN && a->idle >= DREAM_IDLE && a->ear_quiet >= DREAM_IDLE &&
                 !a->panel && !a->quit_dialog;
    if (active && !a->dreaming) {
        a->gx = a->cx; a->gy = a->cy_draw;
        a->gdeg = a->cdeg;
        a->gx_n = clamp01((a->cx - G_LEFT) / (G_RIGHT - G_LEFT));
        dream_next(a);
        a->dream_timer = 1.2f;
    }
    a->dreaming = active;
    if (!active) return;
    float k = 1.0f - expf(-dt * 4.0f);
    a->gx += (a->gtx - a->gx) * k;
    a->gy += (a->gty - a->gy) * k;
    a->dream_timer -= dt;
    if (a->dream_timer > 0.0f) return;
    if (seed_count(a) < 26) {
        float vel = 0.45f + frand(a) * 0.4f;
        plant(a, a->gdeg, 0.0f, a->gx_n, vel, 1);
        if (frand(a) < 0.15f && a->gdeg + 2 < g_ndeg(a))
            plant(a, a->gdeg + 2, 0.0f, clamp01(a->gx_n + 0.05f), vel * 0.8f, 1);
    }
    dream_next(a);
}

/* ------------------------------------------------------------------ lucciole */
int lucciole_step(Firefly *ff, int n, int *hits, int max)
{
    static const int DR[4] = { 1, 0, -1, 0 }, DC[4] = { 0, 1, 0, -1 };
    int nh = 0;
    for (int i = 0; i < n; i++) {
        int d = ff[i].d & 3, nr = ff[i].r + DR[d], nc = ff[i].c + DC[d];
        if (nr < 0 || nr >= GRID || nc < 0 || nc >= GRID) {
            int code = d * GRID + ((d == 0 || d == 2) ? ff[i].c : ff[i].r);
            int dup = 0;
            for (int k = 0; k < nh; k++) dup |= hits[k] == code;
            if (!dup && nh < max) hits[nh++] = code;
            ff[i].d = (d + 2) & 3;
        } else {
            ff[i].r = nr;
            ff[i].c = nc;
        }
    }
    unsigned char turn[MAX_FF];
    for (int i = 0; i < n && i < MAX_FF; i++) {
        int same = 0;
        for (int k = 0; k < n; k++) same += ff[k].r == ff[i].r && ff[k].c == ff[i].c;
        turn[i] = same >= 2;
    }
    for (int i = 0; i < n && i < MAX_FF; i++) if (turn[i]) ff[i].d = (ff[i].d + 1) & 3;
    return nh;
}

static float ff_note(const FioApp *a, int idx)
{
    const Scale *s = scale_of(a);
    return (float)(base_note(a) + 12 + 12 * (idx / s->n) + s->steps[idx % s->n]);
}
static void cell_xy(int r, int c, float *x, float *y)
{
    *x = GRID_X + (float)c * (CELL + GAP) + CELL * 0.5f;
    *y = GRID_Y + (float)(GRID - 1 - r) * (CELL + GAP) + CELL * 0.5f;
}
/* punto del bordo colpito (lato, indice) */
static void wall_xy(int side, int idx, float *x, float *y)
{
    float cx, cy;
    if (side == 0 || side == 2) { cell_xy(side == 0 ? GRID - 1 : 0, idx, &cx, &cy); cy += side == 0 ? -(CELL * 0.5f + 7.0f) : CELL * 0.5f + 7.0f; }
    else { cell_xy(idx, side == 1 ? GRID - 1 : 0, &cx, &cy); cx += side == 1 ? CELL * 0.5f + 7.0f : -(CELL * 0.5f + 7.0f); }
    *x = cx;
    *y = cy;
}

static void ff_set_tempo(FioApp *a) { a->f_len = a->sr * 60.0 / clampf(a->g.tempo, 30.0f, 300.0f) / 2.0; }

static void ff_dream(FioApp *a)
{
    float p = frand(a);
    if (a->nff && p < 0.5f) {
        int i = (int)(frand(a) * (float)a->nff) % a->nff;
        a->ff[i].d = (a->ff[i].d + 1) & 3;
    } else if (p < 0.8f && a->nff < 10) {
        int i = a->nff++;
        a->ff[i].r = (int)(frand(a) * GRID) % GRID;
        a->ff[i].c = (int)(frand(a) * GRID) % GRID;
        a->ff[i].d = (int)(frand(a) * 4.0f) & 3;
        a->pr[i] = a->ff[i].r; a->pc[i] = a->ff[i].c; a->bump[i] = 0;
    } else if (a->nff > 3) {
        int i = (int)(frand(a) * (float)a->nff) % a->nff;
        a->nff--;
        a->ff[i] = a->ff[a->nff]; a->pr[i] = a->pr[a->nff]; a->pc[i] = a->pc[a->nff]; a->bump[i] = a->bump[a->nff];
    }
}

static void schedule_fireflies(FioApp *a, long long A, long long B)
{
    if (!a->f_playing) return;
    if (a->f_next < A) a->f_next = A;
    while (a->f_next < B) {
        long long t = a->f_next;
        int old_d[MAX_FF];
        for (int i = 0; i < a->nff; i++) { a->pr[i] = a->ff[i].r; a->pc[i] = a->ff[i].c; old_d[i] = a->ff[i].d; }
        int hits[32];
        int nh = lucciole_step(a->ff, a->nff, hits, 32);
        for (int i = 0; i < a->nff; i++)
            a->bump[i] = (a->pr[i] == a->ff[i].r && a->pc[i] == a->ff[i].c && ((old_d[i] + 2) & 3) == a->ff[i].d) ? 1 + old_d[i] : 0;
        a->f_tcur = t;
        float scale = nh > 1 ? 1.0f / sqrtf((float)nh) : 1.0f;
        for (int k = 0; k < nh; k++) {
            int side = hits[k] / GRID, idx = hits[k] % GRID;
            float pan = (side == 0 || side == 2) ? ((float)idx / (GRID - 1) * 2.0f - 1.0f) * 0.75f : (side == 1 ? 0.7f : -0.7f);
            float vel = (0.6f + 0.2f * frand(a)) * (0.55f + 0.45f * scale);
            sound(a, t, ff_note(a, idx), vel, pan);
            float x, y;
            wall_xy(side, idx, &x, &y);
            vis_push(a, t, VIS_WALL, x, y, vel, palette(a, (float)idx / (GRID - 1)), hits[k], 0);
        }
        a->f_steps++;
        if (a->g.sogno && a->mode == MODE_FIREFLIES && a->idle >= DREAM_IDLE && a->ear_quiet >= DREAM_IDLE && a->f_steps % 16 == 0) ff_dream(a);
        a->f_next = t + (long long)a->f_len;
    }
}

static int ff_at(const FioApp *a, int r, int c)
{
    for (int i = 0; i < a->nff; i++) if (a->ff[i].r == r && a->ff[i].c == c) return i;
    return -1;
}
static void ff_remove(FioApp *a, int i)
{
    a->nff--;
    a->ff[i] = a->ff[a->nff]; a->pr[i] = a->pr[a->nff]; a->pc[i] = a->pc[a->nff]; a->bump[i] = a->bump[a->nff];
}
/* A su una casella: nasce una lucciola verso l'alto, poi gira a destra, giu', sinistra, poi vola via. */
static void ff_cycle(FioApp *a, int r, int c)
{
    int i = ff_at(a, r, c);
    if (i < 0) {
        if (a->nff >= MAX_FF) { toast(a, "Troppe lucciole"); return; }
        i = a->nff++;
        a->ff[i].r = r; a->ff[i].c = c; a->ff[i].d = 0;
        a->pr[i] = r; a->pc[i] = c; a->bump[i] = 0;
    } else if (a->ff[i].d == 3) {
        ff_remove(a, i);
    } else {
        a->ff[i].d++;
    }
    a->dirty = 1;
}
static void ff_clear_cell(FioApp *a, int r, int c)
{
    int i;
    while ((i = ff_at(a, r, c)) >= 0) ff_remove(a, i);
    a->dirty = 1;
}
static void ff_play(FioApp *a, int on)
{
    a->f_playing = on;
    if (on) a->f_next = a->sched;
    toast(a, on ? "Lucciole in volo" : "Lucciole ferme");
    a->dirty = 1;
}

/* ------------------------------------------------------------------ atmosfere e modi */
static void set_mood(FioApp *a, int m)
{
    m = wrapi(m, MOOD_COUNT);
    if (m == a->mood) return;
    a->sky_from = a->mood;
    a->sky_t = 0.0f;
    a->mood = m;
    a->cdeg = clampi(a->cdeg, 0, g_ndeg(a) - 1);
    a->cy = deg_y(a, a->cdeg);
    a->lp_base = clampi(a->lp_base, 0, g_ndeg(a) - 8);
    push_mix(a);
    char t[160];
    snprintf(t, sizeof(t), "%s: %s", MOODS[m].name, MOODS[m].desc);
    toast(a, t);
    a->dirty = 1;
}

static void set_mode(FioApp *a, int mode)
{
    a->mode = mode ? MODE_FIREFLIES : MODE_GARDEN;
    a->nblooms = 0;
    toast(a, a->mode == MODE_GARDEN ? "Giardino" : (a->nff ? "Lucciole" : "Lucciole: A fa nascere una lucciola"));
    a->dirty = 1;
}

static void mood_reset(FioApp *a, int m)
{
    const Mood *d = &MOODS[m];
    MoodSet *s = &a->set[m];
    s->timbre = d->timbre; s->scale = d->scale; s->root = d->root;
    s->bright = d->bright; s->length = d->length; s->reverb = d->reverb; s->shimmer = d->shimmer;
    s->delay = d->delay; s->drone = d->drone;
}

/* ------------------------------------------------------------------ registrazione */
static void put_u32(unsigned char *p, uint32_t v) { p[0] = v & 255; p[1] = (v >> 8) & 255; p[2] = (v >> 16) & 255; p[3] = (v >> 24) & 255; }
static void wav_header(FILE *f, int sr, long long frames)
{
    unsigned char h[44];
    uint32_t data = (uint32_t)(frames * 4);
    memcpy(h, "RIFF", 4); put_u32(h + 4, 36 + data); memcpy(h + 8, "WAVEfmt ", 8);
    put_u32(h + 16, 16); h[20] = 1; h[21] = 0; h[22] = 2; h[23] = 0;
    put_u32(h + 24, (uint32_t)sr); put_u32(h + 28, (uint32_t)sr * 4); h[32] = 4; h[33] = 0; h[34] = 16; h[35] = 0;
    memcpy(h + 36, "data", 4); put_u32(h + 40, data);
    fseek(f, 0, SEEK_SET);
    fwrite(h, 1, 44, f);
}

static int make_dirs(const char *path)
{
    char tmp[600];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char c = *p;
            *p = 0;
            MKDIR(tmp);
            *p = c;
        }
    }
    MKDIR(tmp);
    struct stat st;
    return stat(tmp, &st) == 0 ? 0 : -1;
}

static void rec_pump(FioApp *a)
{
    if (!a->rec) return;
    static float buf[2 * 4096];
    static int16_t pcm[2 * 4096];
    int n;
    while ((n = fio_tap_read(a->dsp, buf, 4096)) > 0) {
        for (int i = 0; i < 2 * n; i++) {
            float v = clampf(buf[i], -1.0f, 1.0f) * 32767.0f;
            pcm[i] = (int16_t)lrintf(v);
        }
        fwrite(pcm, sizeof(int16_t), (size_t)(2 * n), a->rec);
        a->rec_frames += n;
    }
}

static void rec_start(FioApp *a)
{
    if (a->rec) return;
    if (make_dirs(a->rec_dir)) { toast(a, "Non riesco a creare la cartella delle registrazioni"); return; }
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char name[80];
    if (tm) strftime(name, sizeof(name), "fioritura-%Y-%m-%d-%H%M%S.wav", tm);
    else snprintf(name, sizeof(name), "fioritura-%ld.wav", (long)now);
    snprintf(a->rec_path, sizeof(a->rec_path), "%s/%s", a->rec_dir, name);
    a->rec = fopen(a->rec_path, "wb");
    if (!a->rec) { toast(a, "Non riesco a scrivere la registrazione"); return; }
    wav_header(a->rec, (int)a->sr, 0);
    a->rec_frames = 0;
    fio_tap(a->dsp, 1);
    toast(a, "Registrazione in corso");
}

static void rec_stop(FioApp *a)
{
    if (!a->rec) return;
    rec_pump(a);
    fio_tap(a->dsp, 0);
    wav_header(a->rec, (int)a->sr, a->rec_frames);
    fclose(a->rec);
    a->rec = NULL;
    const char *base = strrchr(a->rec_path, '/');
    char t[200];
    snprintf(t, sizeof(t), "Salvata: %s", base ? base + 1 : a->rec_path);
    toast(a, t);
}

/* ------------------------------------------------------------------ stato */
int fioapp_save(FioApp *a)
{
    if (!a->state_path[0]) return -1;
    char dir[512], tmp[600];
    snprintf(dir, sizeof(dir), "%s", a->state_path);
    char *sl = strrchr(dir, '/');
    if (sl) { *sl = 0; make_dirs(dir); }
    snprintf(tmp, sizeof(tmp), "%s.tmp", a->state_path);
    FILE *f = fopen(tmp, "w");
    if (!f) return -1;
    fprintf(f, "# Fioritura: stato (si riscrive da solo)\n");
    fprintf(f, "modo=%d\natmosfera=%d\nrespiro=%g\nmemoria=%d\nvento=%g\ntempo=%g\nsogno=%d\nvolume=%g\nascolto=%g\n",
            a->mode, a->mood, a->g.respiro, a->g.memoria, a->g.vento, a->g.tempo, a->g.sogno, a->g.volume, a->g.ascolto);
    fprintf(f, "cursore=%g,%d\ncella=%d,%d\nlucciole_in_volo=%d\n", (a->cx - G_LEFT) / (G_RIGHT - G_LEFT), a->cdeg, a->cell_r, a->cell_c, a->f_playing);
    for (int m = 0; m < MOOD_COUNT; m++) {
        const MoodSet *s = &a->set[m];
        fprintf(f, "a%d=%d,%d,%d,%g,%g,%g,%g,%g,%g\n", m, s->timbre, s->scale, s->root, s->bright, s->length, s->reverb, s->shimmer, s->delay, s->drone);
    }
    for (int i = 0; i < MAX_SEEDS; i++) {
        const Seed *s = &a->seeds[i];
        if (s->alive) fprintf(f, "fiore=%d,%g,%g,%g,%.6f,%d,%d\n", s->deg, s->note, s->x, s->vel, s->phase, s->done, s->dream);
    }
    for (int i = 0; i < a->nff; i++) fprintf(f, "lucciola=%d,%d,%d\n", a->ff[i].r, a->ff[i].c, a->ff[i].d);
    int bad = ferror(f);
    if (fclose(f) || bad) return -1;
    remove(a->state_path);
    if (rename(tmp, a->state_path)) return -1;
    a->dirty = 0;
    return 0;
}

static void load_state(FioApp *a)
{
    FILE *f = fopen(a->state_path, "r");
    if (!f) return;
    char line[400];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (line[0] == '#' || !eq) continue;
        *eq = 0;
        const char *k = line, *v = eq + 1;
        if (!strcmp(k, "modo")) a->mode = atoi(v) ? MODE_FIREFLIES : MODE_GARDEN;
        else if (!strcmp(k, "atmosfera")) a->mood = clampi(atoi(v), 0, MOOD_COUNT - 1);
        else if (!strcmp(k, "respiro")) a->g.respiro = clampf((float)atof(v), 4.0f, 40.0f);
        else if (!strcmp(k, "memoria")) a->g.memoria = clampi(atoi(v), 0, 16);
        else if (!strcmp(k, "vento")) a->g.vento = clamp01((float)atof(v));
        else if (!strcmp(k, "tempo")) a->g.tempo = clampf((float)atof(v), 40.0f, 240.0f);
        else if (!strcmp(k, "sogno")) a->g.sogno = atoi(v) != 0;
        else if (!strcmp(k, "volume")) a->g.volume = clamp01((float)atof(v));
        else if (!strcmp(k, "ascolto")) a->g.ascolto = clamp01((float)atof(v));
        else if (!strcmp(k, "cursore")) { float x = 0.5f; int d = 0; if (sscanf(v, "%f,%d", &x, &d) == 2) { a->cx = x_pix(clamp01(x)); a->cdeg = d; } }
        else if (!strcmp(k, "cella")) { int r, c; if (sscanf(v, "%d,%d", &r, &c) == 2) { a->cell_r = clampi(r, 0, GRID - 1); a->cell_c = clampi(c, 0, GRID - 1); } }
        else if (!strcmp(k, "lucciole_in_volo")) a->f_playing = atoi(v) != 0;
        else if (k[0] == 'a' && k[1] >= '0' && k[1] <= '9') {
            int m = atoi(k + 1);
            MoodSet s;
            if (m >= 0 && m < MOOD_COUNT &&
                sscanf(v, "%d,%d,%d,%f,%f,%f,%f,%f,%f", &s.timbre, &s.scale, &s.root, &s.bright, &s.length, &s.reverb, &s.shimmer, &s.delay, &s.drone) == 9) {
                s.timbre = clampi(s.timbre, 0, TIM_COUNT - 1);
                s.scale = clampi(s.scale, 0, SC_COUNT - 1);
                s.root = wrapi(s.root, 12);
                s.bright = clamp01(s.bright); s.length = clampf(s.length, 0.5f, 12.0f);
                s.reverb = clamp01(s.reverb); s.shimmer = clamp01(s.shimmer); s.delay = clamp01(s.delay); s.drone = clamp01(s.drone);
                a->set[m] = s;
            }
        } else if (!strcmp(k, "fiore")) {
            Seed s;
            memset(&s, 0, sizeof(s));
            double ph;
            if (sscanf(v, "%d,%f,%f,%f,%lf,%d,%d", &s.deg, &s.note, &s.x, &s.vel, &ph, &s.done, &s.dream) == 7) {
                Seed *d = seed_new(a);
                *d = s;
                d->alive = 1;
                d->serial = ++a->serial;
                d->phase = ph - floor(ph);
                d->lap = 0;
                d->x = clamp01(d->x);
                d->vel = clamp01(d->vel);
                d->ydraw = -1.0f;
            }
        } else if (!strcmp(k, "lucciola")) {
            int r, c, d;
            if (sscanf(v, "%d,%d,%d", &r, &c, &d) == 3 && a->nff < MAX_FF) {
                int i = a->nff++;
                a->ff[i].r = clampi(r, 0, GRID - 1); a->ff[i].c = clampi(c, 0, GRID - 1); a->ff[i].d = d & 3;
                a->pr[i] = a->ff[i].r; a->pc[i] = a->ff[i].c;
            }
        }
    }
    fclose(f);
}

/* ------------------------------------------------------------------ creazione */
FioApp *fioapp_create(float sample_rate, const char *state_path)
{
    FioApp *a = calloc(1, sizeof(FioApp));
    if (!a) return NULL;
    a->sr = sample_rate > 8000.0f ? sample_rate : 48000.0f;
    a->dsp = fio_create(a->sr);
    if (!a->dsp) { free(a); return NULL; }
    a->rng = 0x2545F491u ^ (unsigned)time(NULL);
    for (int m = 0; m < MOOD_COUNT; m++) mood_reset(a, m);
    a->g.respiro = 14.0f;
    a->g.memoria = 6;
    a->g.vento = 0.12f;
    a->g.tempo = 104.0f;
    a->g.sogno = 1;
    a->g.volume = 0.8f;
    a->g.ascolto = 0.5f;
    a->ear = fear_create();
    a->ear_quiet = 1000.0f;
    a->pen_x = 0.1f;
    a->pen_last = -100.0f;
    a->cx = x_pix(0.5f);
    a->cdeg = 7;
    a->cell_r = 3; a->cell_c = 3;
    a->prow = 0;
    a->panel = 0;
    snprintf(a->state_path, sizeof(a->state_path), "%s", state_path ? state_path : "");
    char dir[512];
    snprintf(dir, sizeof(dir), "%s", a->state_path);
    char *sl = strrchr(dir, '/');
    if (sl) *sl = 0; else snprintf(dir, sizeof(dir), ".");
    snprintf(a->rec_dir, sizeof(a->rec_dir), "%s/registrazioni", dir);
    if (a->state_path[0]) load_state(a);
    a->cdeg = clampi(a->cdeg, 0, g_ndeg(a) - 1);
    a->cy = a->cy_draw = deg_y(a, a->cdeg);
    for (int i = 0; i < MAX_SEEDS; i++) if (a->seeds[i].alive) a->seeds[i].ydraw = seed_y(a, &a->seeds[i]);
    a->g_len = a->g.respiro * a->sr;
    a->lp_base = clampi(scale_of(a)->n, 0, g_ndeg(a) - 8);
    ff_set_tempo(a);
    for (int m = 0; m < MOOD_COUNT; m++) {
        a->sky[m] = malloc(sizeof(uint32_t) * W * H);
        if (a->sky[m]) fx_sky(a->sky[m], W, H, MOODS[m].top, MOODS[m].bottom, MOODS[m].haze);
    }
    a->sky_from = a->mood;
    a->sky_t = 1.0f;
    a->bg = malloc(sizeof(uint32_t) * W * H);
    for (int i = 0; i < N_POLLEN; i++) {
        Pollen *p = &a->pollen[i];
        p->x = frand(a) * W; p->y = 40.0f + frand(a) * (H - 40.0f);
        p->ph = frand(a); p->r = 1.2f + frand(a) * 1.8f; p->sp = 4.0f + frand(a) * 9.0f;
    }
    a->chrome = 1.0f;
    push_mix(a);
    if (!seed_count(a) && !a->nff) toast(a, "A pianta un fiore  ·  tieni SELECT per l'aiuto");
    return a;
}

void fioapp_destroy(FioApp *a)
{
    if (!a) return;
    rec_stop(a);
    fioapp_save(a);
    flp_disconnect(&a->lp);                 /* il Launchpad torna in Live mode */
    for (int m = 0; m < MOOD_COUNT; m++) free(a->sky[m]);
    free(a->bg);
    fio_destroy(a->dsp);
    fear_destroy(a->ear);
    free(a);
}

void fioapp_audio(FioApp *a, float *out, int frames) { fio_render(a->dsp, out, frames); }

/* ------------------------------------------------------------------ Launchpad */
static void lp_event(FioApp *a, const FlpEvent *e)
{
    a->idle = 0.0f;
    a->hot = 0.5f;
    if (!e->on) return;
    if (e->kind == FLP_EV_PAD) {
        if (a->mode == MODE_GARDEN) {
            int d = a->lp_base + e->row;
            if (d < g_ndeg(a)) {
                a->cdeg = d; a->cy = deg_y(a, d); a->cx = x_pix(((float)e->col + 0.5f) / 8.0f);
                plant(a, d, 0.0f, ((float)e->col + 0.5f) / 8.0f, 0.8f, 0);
            }
        } else {
            a->cell_r = e->row; a->cell_c = e->col;
            if (a->lp.shift) ff_clear_cell(a, e->row, e->col);
            else ff_cycle(a, e->row, e->col);
        }
    } else if (e->kind == FLP_EV_TOP) {
        switch (e->index) {
        case FLP_LEFT: set_mood(a, a->mood - 1); break;
        case FLP_RIGHT: set_mood(a, a->mood + 1); break;
        case FLP_UP:
        case FLP_DOWN: {
            int dir = e->index == FLP_UP ? 1 : -1;
            if (a->mode == MODE_GARDEN) a->lp_base = clampi(a->lp_base + dir * scale_of(a)->n, 0, g_ndeg(a) - 8);
            else { a->g.tempo = clampf(a->g.tempo + 4.0f * (float)dir, 40.0f, 240.0f); ff_set_tempo(a); }
            break;
        }
        case FLP_SESSION: set_mode(a, !a->mode); break;
        case FLP_DRUMS: a->g.sogno = !a->g.sogno; toast(a, a->g.sogno ? "Sogno acceso" : "Sogno spento"); break;
        case FLP_KEYS: ff_play(a, !a->f_playing); break;
        default: break;
        }
    } else if (e->kind == FLP_EV_SIDE) {
        if (e->index < MOOD_COUNT) set_mood(a, e->index);
        else if (e->index == 6) {
            if (!a->lp.shift) toast(a, "Tieni User per svuotare");
            else if (a->mode == MODE_GARDEN) clear_garden(a);
            else { a->nff = 0; toast(a, "Lucciole liberate"); }
        } else if (e->index == 7) {
            if (a->rec) rec_stop(a); else rec_start(a);
        }
    }
}

static void lp_render(FioApp *a)
{
    FioLp *lp = &a->lp;
    if (!lp->connected) return;
    const Mood *md = &MOODS[a->mood];
    flp_clear(lp);
    float lev[8][8];
    uint32_t col[8][8];
    memset(lev, 0, sizeof(lev));
    memset(col, 0, sizeof(col));
    if (a->mode == MODE_GARDEN) {
        const Scale *s = scale_of(a);
        for (int r = 0; r < 8; r++) {
            int d = a->lp_base + r;
            if (d >= g_ndeg(a)) continue;
            int tonic = d % s->n == 0;
            for (int c = 0; c < 8; c++) { col[r][c] = tonic ? md->col[0] : md->col[1]; lev[r][c] = tonic ? 0.10f : 0.035f; }
        }
        for (int i = 0; i < MAX_SEEDS; i++) {
            const Seed *s = &a->seeds[i];
            if (!s->alive || s->deg < a->lp_base || s->deg >= a->lp_base + 8) continue;
            int r = s->deg - a->lp_base, c = clampi((int)(s->x * 8.0f), 0, 7);
            float l = 0.28f + 0.35f * s->glow;
            if (l > lev[r][c]) { lev[r][c] = l; col[r][c] = palette(a, (float)r / 7.0f); }
        }
    } else {
        static const int DIRC[4] = { 2, 1, 0, 1 };
        lev[a->cell_r][a->cell_c] = 0.1f;
        col[a->cell_r][a->cell_c] = RGB(255, 255, 255);
        for (int i = 0; i < a->nff; i++) {
            int r = a->ff[i].r, c = a->ff[i].c;
            lev[r][c] = 1.0f;
            col[r][c] = md->col[DIRC[a->ff[i].d & 3]];
        }
    }
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++) {
            float f = a->lp_flash[r][c];
            if (f > 0.02f && f > lev[r][c] * 0.7f) { col[r][c] = gfx_mix(md->col[2], RGB(255, 255, 255), 0.4f); lev[r][c] = fmaxf(lev[r][c], f); }
            flp_set(lp, flp_pad(r, c), col[r][c], lev[r][c]);
        }
    flp_set(lp, flp_top(FLP_LEFT), RGB(255, 255, 255), 0.25f);
    flp_set(lp, flp_top(FLP_RIGHT), RGB(255, 255, 255), 0.25f);
    flp_set(lp, flp_top(FLP_UP), RGB(255, 255, 255), 0.2f);
    flp_set(lp, flp_top(FLP_DOWN), RGB(255, 255, 255), 0.2f);
    flp_set(lp, flp_top(FLP_SESSION), a->mode == MODE_GARDEN ? md->col[1] : C_GOLD, 0.8f);
    flp_set(lp, flp_top(FLP_DRUMS), C_VIOLET, a->g.sogno ? 1.0f : 0.12f);
    flp_set(lp, flp_top(FLP_KEYS), C_GOLD, a->f_playing ? 1.0f : 0.12f);
    flp_set(lp, flp_top(FLP_USER), RGB(255, 255, 255), lp->shift ? 1.0f : 0.1f);
    for (int m = 0; m < MOOD_COUNT && m < 6; m++) flp_set(lp, flp_side(m), MOODS[m].col[0], m == a->mood ? 1.0f : 0.15f);
    flp_set(lp, flp_side(6), C_RED, lp->shift ? 0.9f : 0.12f);
    flp_set(lp, flp_side(7), C_RED, a->rec ? (0.55f + 0.45f * sinf(a->t * 6.0f)) : 0.12f);
    flp_set(lp, 99, md->col[0], 0.8f);
    flp_flush(lp);
}

/* ------------------------------------------------------------------ MIDI */
void fioapp_midi(FioApp *a, int port, const unsigned char *m, int len)
{
    FlpEvent e;
    if (a->lp.connected && flp_decode(&a->lp, port, m, len, &e)) { lp_event(a, &e); return; }
    if (len >= 3 && MIDI_TYPE(m) == MIDI_NOTE_ON && m[2] > 0) {
        int deg = note_deg(a, m[1]);
        float x = 0.12f + frand(a) * 0.76f;
        plant(a, deg, (float)m[1], x, 0.15f + 0.85f * (float)m[2] / 127.0f, 0);
        a->idle = 0.0f;
        a->hot = 0.5f;
    }
}

void fioapp_midi_status(FioApp *a, const char *device)
{
    if (!device) {
        if (a->lp.connected) { a->lp.connected = 0; a->lp.port = -1; }
        if (a->midi_dev[0]) toast(a, "MIDI scollegato");
        a->midi_dev[0] = 0;
        return;
    }
    snprintf(a->midi_dev, sizeof(a->midi_dev), "%s", device);
    char t[160];
    if (flp_match(device)) {
        flp_connect(&a->lp);
        toast(a, "Launchpad collegato: la griglia e' il giardino");
    } else {
        snprintf(t, sizeof(t), "Tastiera MIDI: %s (ogni nota e' un fiore)", device);
        toast(a, t);
    }
}

/* ------------------------------------------------------------------ microfono: l'orecchio del giardino */
static void bloom_add(FioApp *a, float x, float y, float s, uint32_t col, int petals, int small);

void fioapp_capture(FioApp *a, const float *in, int frames) { if (a->ear) fear_feed(a->ear, in, frames); }

void fioapp_capture_status(FioApp *a, const char *device, float sample_rate)
{
    if (!a->ear) return;
    int was = a->mic;
    a->mic = device != NULL && sample_rate > 4000.0f;
    fear_set_rate(a->ear, a->mic ? sample_rate : 0.0f);
    a->gust = 0.0f; a->voice_a = 0.0f; a->nclaps = 0; a->front_on = 0;
    memset(a->sres, 0, sizeof(a->sres));
    if (a->mic) {
        snprintf(a->mic_name, sizeof(a->mic_name), "%s", device);
        toast(a, a->g.ascolto > 0.001f ? "Il giardino ti ascolta: canta, soffia, batti le mani"
                                       : "Microfono collegato, ma l'ascolto è spento (START)");
    } else {
        a->mic_name[0] = 0;
        if (was) toast(a, "Microfono scollegato");
    }
}

/* nella gamma del giardino, per ottave */
static float fold_note(const FioApp *a, float m)
{
    float lo = (float)base_note(a) - 0.5f, hi = (float)base_note(a) + 36.5f;
    while (m < lo) m += 12.0f;
    while (m > hi) m -= 12.0f;
    return m;
}
static int nearest_deg(const FioApp *a, float m)
{
    int n = g_ndeg(a), best = 0;
    float bd = 1e9f;
    for (int d = 0; d < n; d++) {
        float dd = fabsf((float)deg_note(a, d) - m);
        if (dd < bd) { bd = dd; best = d; }
    }
    return best;
}

/* Il microfono sente l'altoparlante (o non si sa ancora): una nota cominciata subito dopo che il giardino
   ha suonato la stessa nota (anche un'ottava sopra o sotto) e' il giardino stesso, non chi canta. onset e'
   l'inizio della nota cantata, nel tempo dell'app. In cuffia non si scarta niente. */
static int heard_self(const FioApp *a, float note, float onset)
{
    if (fear_hears_output(a->ear) < 0) return 0;
    for (int i = 0; i < N_RECENT; i++) {
        float d = onset - a->recent_t[i];
        if (a->recent_t[i] <= 0.0f || d < -0.15f || d > 0.45f) continue;
        if (wrapi((int)lroundf(a->recent_note[i] - note), 12) == 0) return 1;
    }
    return 0;
}

/* Canto: la nota tenuta diventa un fiore sulla corda piu' vicina, dove scrive la penna. t: inizio della
   nota in secondi di microfono. */
static void ear_sing(FioApp *a, float m, float strength, double t)
{
    int d = nearest_deg(a, fold_note(a, m));
    a->ear_quiet = 0.0f;
    a->ear_busy = 1.0f;
    float onset = a->t - (float)(fear_time(a->ear) - t) - 0.025f;     /* il microfono arriva con un blocco di ritardo */
    if (heard_self(a, (float)deg_note(a, d), onset)) return;
    plant(a, d, 0.0f, a->pen_x, 0.3f + 0.6f * strength, 0);
    if (a->mode == MODE_GARDEN) bloom_add(a, x_pix(a->pen_x), deg_y(a, d), 0.8f, RGB(255, 255, 255), 0, 1);
}

static void petal_add(FioApp *a, float x, float y, uint32_t col)
{
    Petal *p = a->npetals < MAX_PETALS ? &a->petals[a->npetals++] : &a->petals[(int)(frand(a) * MAX_PETALS) % MAX_PETALS];
    p->x = x; p->y = y;
    p->vx = 30.0f + 90.0f * frand(a);
    p->vy = (frand(a) - 0.65f) * 60.0f;
    p->age = 0.0f;
    p->life = 1.3f + 1.4f * frand(a);
    p->r = 2.5f + 3.0f * frand(a);
    p->ph = frand(a) * 2.0f * PI_F;
    p->col = col;
}

/* Il fronte del vento passa da sinistra a destra e pizzica i fiori che incontra, come un'arpa eolica:
   ogni fiore suona piano, perde petali e a volte il vento lo sposta di una corda. */
static void strum(FioApp *a, float x0, float x1)
{
    for (int i = 0; i < MAX_SEEDS; i++) {
        Seed *s = &a->seeds[i];
        if (!s->alive) continue;
        float x = x_pix(s->x);
        if (x <= x0 || x > x1) continue;
        float y = seed_y(a, s);
        uint32_t col = palette_y(a, y);
        s->glow = fmaxf(s->glow, 0.8f);
        int np = 3 + (int)(5.0f * a->gust);
        for (int k = 0; k < np; k++) petal_add(a, x + (frand(a) - 0.5f) * 10.0f, y + (frand(a) - 0.5f) * 10.0f, col);
        if (a->t - a->strum_t >= 0.03f) {            /* al piu' una nota ogni 30 ms: un giardino fitto non satura */
            a->strum_t = a->t;
            float v = s->vel * (0.22f + 0.4f * a->gust);
            sound(a, 0, seed_note(a, s), v, pan_of(s->x));
            if (a->mode == MODE_GARDEN) bloom_add(a, x, y, v, col, 0, 1);
        }
        if (s->deg >= 0 && frand(a) < 0.3f * a->gust) {
            int d = s->deg + (frand(a) < 0.5f ? -1 : 1);
            if (d >= 0 && d < g_ndeg(a)) { s->deg = d; a->dirty = 1; }
        }
    }
}

static void spark_add(FioApp *a)
{
    if (a->nsparks >= MAX_SPARKS) return;
    Spark *s = &a->sparks[a->nsparks++];
    int right = frand(a) < 0.5f;
    float ang = (frand(a) - 0.5f) * 0.7f, sp = 250.0f + 120.0f * frand(a);
    s->x = right ? (float)W + 10.0f : -10.0f;
    s->y = G_TOP + 40.0f + frand(a) * (G_BOT - G_TOP - 80.0f);
    s->vx = (right ? -1.0f : 1.0f) * sp * cosf(ang);
    s->vy = sp * sinf(ang);
    s->age = 0.0f;
    s->life = 2.6f;
}

/* Battito: un battito solo fa nascere una lucciola (e le lucciole partono); battiti regolari danno il
   tempo alle lucciole, un battito un quarto (un passo e' un ottavo), con un passo sul battito. */
static void ear_clap(FioApp *a, double t)
{
    a->ear_quiet = 0.0f;
    a->ear_busy = 1.0f;
    a->ff_flash = 1.0f;
    for (int i = 0; i < MAX_SEEDS; i++) if (a->seeds[i].alive) a->seeds[i].glow = fmaxf(a->seeds[i].glow, 0.45f);
    if (a->nclaps && t - a->claps[a->nclaps - 1] > 2.0) a->nclaps = 0;
    if (a->nclaps == 6) { memmove(a->claps, a->claps + 1, sizeof(double) * 5); a->nclaps = 5; }
    a->claps[a->nclaps++] = t;
    if (a->nclaps == 1) {
        if (a->nff >= MAX_FF) return;
        int r = 0, c = 0;
        for (int tries = 0; tries < 24; tries++) {
            r = (int)(frand(a) * GRID) % GRID;
            c = (int)(frand(a) * GRID) % GRID;
            if (ff_at(a, r, c) < 0) break;
        }
        int i = a->nff++;
        a->ff[i].r = r; a->ff[i].c = c; a->ff[i].d = (int)(frand(a) * 4.0f) & 3;
        a->pr[i] = r; a->pc[i] = c; a->bump[i] = 0;
        float x, y;
        cell_xy(r, c, &x, &y);
        if (a->mode == MODE_FIREFLIES) bloom_add(a, x, y, 1.0f, MOODS[a->mood].col[2], 0, 1);
        else spark_add(a);
        if (!a->f_playing) { a->f_playing = 1; a->f_next = a->sched; }
        toast(a, "Una lucciola dalle tue mani");
        a->dirty = 1;
        return;
    }
    if (a->nclaps < 4) return;
    int n = a->nclaps;
    double mean = (a->claps[n - 1] - a->claps[0]) / (double)(n - 1);
    if (mean < 0.25 || mean > 1.5) return;
    for (int i = 1; i < n; i++) if (fabs(a->claps[i] - a->claps[i - 1] - mean) > 0.15 * mean) return;
    a->g.tempo = clampf(roundf((float)(60.0 / mean)), 40.0f, 240.0f);
    ff_set_tempo(a);
    /* fase: il battito e' arrivato qualche centesimo dopo (microfono) e le note escono dopo (uscita) */
    double age = fear_time(a->ear) - t + 0.07;
    long long clap_s = a->now - (long long)(age * a->sr), step = (long long)a->f_len;
    if (step > 0) {
        long long k = (a->sched - clap_s + step - 1) / step;
        if (k < 0) k = 0;
        a->f_next = clap_s + k * step;
    }
    a->f_playing = 1;
    char msg[80];
    snprintf(msg, sizeof(msg), "Le lucciole seguono le tue mani: %d BPM", (int)lroundf(a->g.tempo));
    toast(a, msg);
    a->dirty = 1;
}

static void ear_tick(FioApp *a, float dt)
{
    a->ear_quiet += dt;
    if (a->ear_busy > 0.0f) a->ear_busy -= dt;
    a->ff_flash = fmaxf(0.0f, a->ff_flash - dt * 2.5f);
    int on = 0;
    if (a->ear && a->mic) {
        on = a->g.ascolto > 0.001f;
        float notes[64];
        int n = g_ndeg(a) < 64 ? g_ndeg(a) : 64;
        for (int d = 0; d < n; d++) notes[d] = (float)deg_note(a, d);
        fear_set_strings(a->ear, notes, n);
        fear_set_sensitivity(a->ear, a->g.ascolto);
        EarEvent ev[16];
        int ne = fear_poll(a->ear, fio_level(a->dsp), ev, 16);
        for (int i = 0; on && !a->quit_dialog && i < ne; i++) {
            if (ev[i].kind == EAR_NOTE) ear_sing(a, ev[i].note, ev[i].strength, ev[i].t);
            else if (ev[i].kind == EAR_CLAP) ear_clap(a, ev[i].t);
            else if (ev[i].kind == EAR_BLOW_START) {
                a->ear_quiet = 0.0f;
                if (!a->front_on) { a->front_on = 1; a->front_x = G_LEFT - 30.0f; }
            }
        }
        for (int d = 0; d < 64; d++) a->sres[d] = on && d < n ? fear_string(a->ear, d) : 0.0f;
    }

    /* canto: la penna avanza mentre si sente una nota, tutto il giardino in poco piu' di mezzo respiro */
    if (on && fear_voiced(a->ear)) {
        if (a->t - a->pen_last > 2.5f) a->pen_x = 0.06f + 0.08f * frand(a);   /* frase nuova: da sinistra */
        a->pen_last = a->t;
        a->pen_x += dt / clampf(a->g.respiro * 0.6f, 5.0f, 24.0f);
        if (a->pen_x > 0.94f) a->pen_x = 0.06f;
        float m = fold_note(a, fear_pitch(a->ear)), y = note_y(a, m);
        a->voice_y = a->voice_a > 0.01f ? a->voice_y + (y - a->voice_y) * (1.0f - expf(-dt * 25.0f)) : y;
        a->voice_a = fminf(1.0f, a->voice_a + dt * 8.0f);
        int d = nearest_deg(a, m);
        a->line_glow[d] = fmaxf(a->line_glow[d], 0.25f + 0.35f * fear_level(a->ear));
        a->rib_push -= dt;
        if (a->rib_push <= 0.0f) {
            a->rib_x[a->rib_w] = x_pix(a->pen_x);
            a->rib_y[a->rib_w] = a->voice_y;
            a->rib_a[a->rib_w] = fear_level(a->ear);
            a->rib_t[a->rib_w] = a->t;
            a->rib_w = (a->rib_w + 1) % RIBBON;
            if (a->rib_n < RIBBON) a->rib_n++;
            a->rib_push = 1.0f / 45.0f;
        }
        a->ear_quiet = 0.0f;
        a->ear_busy = 0.5f;
    } else {
        a->voice_a = fmaxf(0.0f, a->voice_a - dt * 4.0f);
    }

    /* soffio: il vento cresce in fretta e cala piano; il fronte riparte finche' si soffia */
    float want = on ? fear_blow(a->ear) : 0.0f;
    a->gust += (want - a->gust) * (1.0f - expf(-dt * (want > a->gust ? 6.0f : 1.5f)));
    if (want > 0.0f) { a->ear_quiet = 0.0f; a->ear_busy = 0.5f; }
    if (a->front_on) {
        float prev = a->front_x;
        a->front_x += (G_RIGHT - G_LEFT) / (1.6f - 0.8f * a->gust) * dt;
        strum(a, prev, a->front_x);
        if (a->front_x > G_RIGHT + 30.0f) { a->front_on = 0; a->front_wait = 0.3f; }
    } else if (want > 0.0f && (a->front_wait -= dt) <= 0.0f) {
        a->front_on = 1;
        a->front_x = G_LEFT - 30.0f;
    }
    /* su un giardino quasi vuoto il vento semina */
    if (want > 0.12f && seed_count(a) < 4) {
        a->sow_t -= dt;
        if (a->sow_t <= 0.0f) {
            int n = g_ndeg(a);
            float xn = a->front_on ? (a->front_x - G_LEFT) / (G_RIGHT - G_LEFT) : frand(a);
            plant(a, n / 4 + (int)(frand(a) * (float)(n / 2)), 0.0f, clampf(xn, 0.05f, 0.95f), 0.4f + 0.3f * a->gust, 1);
            a->sow_t = 0.45f;
        }
    }
    /* petali: si staccano dai fiori finche' c'e' vento, poi volano via */
    if (a->gust > 0.08f && a->mode == MODE_GARDEN) {
        float want_n = a->gust * 90.0f * dt;
        int alive = seed_count(a);
        while (want_n > 0.0f && (want_n >= 1.0f || frand(a) < want_n)) {
            want_n -= 1.0f;
            if (alive) {
                int k = (int)(frand(a) * (float)alive), i = 0;
                for (; i < MAX_SEEDS; i++) if (a->seeds[i].alive && k-- == 0) break;
                if (i < MAX_SEEDS) { const Seed *s = &a->seeds[i]; petal_add(a, x_pix(s->x), s->ydraw, palette_y(a, s->ydraw)); }
            } else {
                float y = G_TOP + frand(a) * (G_BOT - G_TOP);
                petal_add(a, -8.0f, y, palette_y(a, y));
            }
        }
    }
    float wind = 50.0f + 280.0f * a->gust;
    for (int i = 0; i < a->npetals;) {
        Petal *p = &a->petals[i];
        p->age += dt;
        if (p->age >= p->life || p->x > W + 12.0f) { *p = a->petals[--a->npetals]; continue; }
        p->vx += (wind - p->vx) * (1.0f - expf(-dt * 2.5f));
        p->vy += (-10.0f - p->vy) * (1.0f - expf(-dt * 1.5f));
        p->x += p->vx * dt;
        p->y += p->vy * dt + sinf(p->ph + p->age * 5.0f) * 24.0f * dt;
        i++;
    }
    if (a->npetals) a->ear_busy = fmaxf(a->ear_busy, 0.1f);
    for (int i = 0; i < a->nsparks;) {
        Spark *s = &a->sparks[i];
        s->age += dt;
        if (s->age >= s->life) { *s = a->sparks[--a->nsparks]; continue; }
        s->x += s->vx * dt;
        s->y += s->vy * dt + sinf(s->age * 7.0f) * 40.0f * dt;
        s->vx *= expf(-dt * 0.5f);
        i++;
    }
    if (a->nsparks) a->ear_busy = fmaxf(a->ear_busy, 0.1f);
}

/* ------------------------------------------------------------------ pannello */
static void row_value(const FioApp *a, int row, char *out, size_t n)
{
    const MoodSet *s = mset(a);
    out[0] = 0;
    switch (row) {
    case R_MOOD: snprintf(out, n, "%s", MOODS[a->mood].name); break;
    case R_TIMBRE: snprintf(out, n, "%s", fio_timbre_name(s->timbre)); break;
    case R_SCALE: snprintf(out, n, "%s", scale_of(a)->name); break;
    case R_ROOT: snprintf(out, n, "%s", NOTE_IT[wrapi(s->root, 12)]); break;
    case R_BRIGHT: snprintf(out, n, "%d%%", (int)lroundf(s->bright * 100.0f)); break;
    case R_LENGTH: snprintf(out, n, "%.1f s", s->length); break;
    case R_RESPIRO: snprintf(out, n, "%d s", (int)lroundf(a->g.respiro)); break;
    case R_MEMORIA: if (a->g.memoria > 0) snprintf(out, n, "%d %s", a->g.memoria, a->g.memoria == 1 ? "ritorno" : "ritorni"); else snprintf(out, n, "per sempre"); break;
    case R_VENTO: snprintf(out, n, "%d%%", (int)lroundf(a->g.vento * 100.0f)); break;
    case R_TEMPO: snprintf(out, n, "%d BPM", (int)lroundf(a->g.tempo)); break;
    case R_REVERB: snprintf(out, n, "%d%%", (int)lroundf(s->reverb * 100.0f)); break;
    case R_SHIMMER: snprintf(out, n, "%d%%", (int)lroundf(s->shimmer * 100.0f)); break;
    case R_DELAY: snprintf(out, n, "%d%%", (int)lroundf(s->delay * 100.0f)); break;
    case R_DRONE: if (s->drone > 0.004f) snprintf(out, n, "%d%%", (int)lroundf(s->drone * 100.0f)); else snprintf(out, n, "spento"); break;
    case R_SOGNO: snprintf(out, n, "%s", a->g.sogno ? "acceso" : "spento"); break;
    case R_ASCOLTO: if (a->g.ascolto > 0.001f) snprintf(out, n, "%d%%", (int)lroundf(a->g.ascolto * 100.0f)); else snprintf(out, n, "spento"); break;
    case R_VOLUME: snprintf(out, n, "%d%%", (int)lroundf(a->g.volume * 100.0f)); break;
    case R_REC:
        if (a->rec) { int sec = (int)(a->rec_frames / (long long)a->sr); snprintf(out, n, "%02d:%02d", sec / 60, sec % 60); }
        else snprintf(out, n, "A");
        break;
    case R_RESET: snprintf(out, n, "A"); break;
    }
}

static void preview(FioApp *a)
{
    int d = clampi(a->cdeg, 0, g_ndeg(a) - 1);
    sound(a, 0, (float)deg_note(a, d), 0.7f, 0.0f);
}

static float stepf(float v, float st, int dir, float lo, float hi) { return clampf(roundf((v + st * (float)dir) / st) * st, lo, hi); }

static void panel_change(FioApp *a, int row, int dir)
{
    MoodSet *s = &a->set[a->mood];
    switch (row) {
    case R_MOOD: set_mood(a, a->mood + dir); return;
    case R_TIMBRE: s->timbre = wrapi(s->timbre + dir, TIM_COUNT); preview(a); break;
    case R_SCALE:
        s->scale = wrapi(s->scale + dir, SC_COUNT);
        a->cdeg = clampi(a->cdeg, 0, g_ndeg(a) - 1);
        a->cy = deg_y(a, a->cdeg);
        a->lp_base = clampi(a->lp_base, 0, g_ndeg(a) - 8);
        preview(a);
        break;
    case R_ROOT: s->root = wrapi(s->root + dir, 12); push_mix(a); preview(a); break;
    case R_BRIGHT: s->bright = stepf(s->bright, 0.05f, dir, 0.0f, 1.0f); preview(a); break;
    case R_LENGTH: s->length = stepf(s->length, 0.5f, dir, 0.5f, 12.0f); preview(a); break;
    case R_RESPIRO: a->g.respiro = stepf(a->g.respiro, 1.0f, dir, 4.0f, 40.0f); g_set_len(a, a->g.respiro); break;
    case R_MEMORIA: {
        int i = 0;
        while (i < N_MEMORIE && MEMORIE[i] != a->g.memoria) i++;
        if (i >= N_MEMORIE) i = 5;
        a->g.memoria = MEMORIE[clampi(i + dir, 0, N_MEMORIE - 1)];
        break;
    }
    case R_VENTO: a->g.vento = stepf(a->g.vento, 0.05f, dir, 0.0f, 1.0f); break;
    case R_TEMPO: a->g.tempo = stepf(a->g.tempo, 2.0f, dir, 40.0f, 240.0f); ff_set_tempo(a); break;
    case R_REVERB: s->reverb = stepf(s->reverb, 0.05f, dir, 0.0f, 1.0f); push_mix(a); break;
    case R_SHIMMER: s->shimmer = stepf(s->shimmer, 0.05f, dir, 0.0f, 1.0f); push_mix(a); break;
    case R_DELAY: s->delay = stepf(s->delay, 0.05f, dir, 0.0f, 1.0f); push_mix(a); break;
    case R_DRONE: s->drone = stepf(s->drone, 0.05f, dir, 0.0f, 1.0f); push_mix(a); break;
    case R_SOGNO: a->g.sogno = !a->g.sogno; break;
    case R_ASCOLTO: a->g.ascolto = stepf(a->g.ascolto, 0.1f, dir, 0.0f, 1.0f); break;
    case R_VOLUME: a->g.volume = stepf(a->g.volume, 0.05f, dir, 0.0f, 1.0f); push_mix(a); break;
    default: return;
    }
    a->dirty = 1;
}

static void panel_button(FioApp *a, int b)
{
    switch (b) {
    case PAD_UP: a->prow = wrapi(a->prow - 1, R_COUNT); break;
    case PAD_DOWN: a->prow = wrapi(a->prow + 1, R_COUNT); break;
    case PAD_LEFT: panel_change(a, a->prow, -1); break;
    case PAD_RIGHT: panel_change(a, a->prow, 1); break;
    case PAD_A:
        if (a->prow == R_REC) { if (a->rec) rec_stop(a); else rec_start(a); }
        else if (a->prow == R_RESET) { mood_reset(a, a->mood); push_mix(a); a->cdeg = clampi(a->cdeg, 0, g_ndeg(a) - 1); a->cy = deg_y(a, a->cdeg); toast(a, "Atmosfera ripristinata"); a->dirty = 1; }
        else panel_change(a, a->prow, 1);
        break;
    case PAD_B:
    case PAD_START:
        a->panel = 0;
        if (a->dirty) fioapp_save(a);
        break;
    case PAD_L1: set_mood(a, a->mood - 1); break;
    case PAD_R1: set_mood(a, a->mood + 1); break;
    default: break;
    }
}

/* ------------------------------------------------------------------ comandi */
static void memoria_step(FioApp *a, int dir)
{
    panel_change(a, R_MEMORIA, dir);
    char t[80], v[40];
    row_value(a, R_MEMORIA, v, sizeof(v));
    snprintf(t, sizeof(t), "Memoria: %s", v);
    toast(a, t);
}
static void tempo_step(FioApp *a, int dir)
{
    a->g.tempo = clampf(a->g.tempo + 4.0f * (float)dir, 40.0f, 240.0f);
    ff_set_tempo(a);
    char t[60];
    snprintf(t, sizeof(t), "Tempo: %d BPM", (int)lroundf(a->g.tempo));
    toast(a, t);
    a->dirty = 1;
}

static void garden_button(FioApp *a, int b)
{
    switch (b) {
    case PAD_A:
        plant(a, a->cdeg, 0.0f, clamp01((a->cx - G_LEFT) / (G_RIGHT - G_LEFT)), 0.8f, 0);
        a->a_last_deg = a->cdeg;
        a->a_last_t = a->t;
        break;
    case PAD_UP: a->cdeg = clampi(a->cdeg + 1, 0, g_ndeg(a) - 1); a->cy = deg_y(a, a->cdeg); break;
    case PAD_DOWN: a->cdeg = clampi(a->cdeg - 1, 0, g_ndeg(a) - 1); a->cy = deg_y(a, a->cdeg); break;
    case PAD_X: case PAD_R1: set_mood(a, a->mood + 1); break;
    case PAD_L1: set_mood(a, a->mood - 1); break;
    case PAD_Y: a->g.sogno = !a->g.sogno; toast(a, a->g.sogno ? "Sogno acceso: se non tocchi niente, il giardino continua da solo" : "Sogno spento"); a->dirty = 1; break;
    case PAD_L2: memoria_step(a, -1); break;
    case PAD_R2: memoria_step(a, 1); break;
    default: break;
    }
}

static void ff_move(FioApp *a, int b)
{
    if (b == PAD_UP) a->cell_r = clampi(a->cell_r + 1, 0, GRID - 1);
    if (b == PAD_DOWN) a->cell_r = clampi(a->cell_r - 1, 0, GRID - 1);
    if (b == PAD_LEFT) a->cell_c = clampi(a->cell_c - 1, 0, GRID - 1);
    if (b == PAD_RIGHT) a->cell_c = clampi(a->cell_c + 1, 0, GRID - 1);
}

static void ff_button(FioApp *a, int b)
{
    switch (b) {
    case PAD_A: ff_cycle(a, a->cell_r, a->cell_c); break;
    case PAD_UP: case PAD_DOWN: case PAD_LEFT: case PAD_RIGHT: ff_move(a, b); break;
    case PAD_X: case PAD_R1: set_mood(a, a->mood + 1); break;
    case PAD_L1: set_mood(a, a->mood - 1); break;
    case PAD_Y: ff_play(a, !a->f_playing); break;
    case PAD_L2: tempo_step(a, -1); break;
    case PAD_R2: tempo_step(a, 1); break;
    default: break;
    }
}

void fioapp_button(FioApp *a, int b, int pressed)
{
    if (b < 0 || b >= PAD_COUNT) return;
    int was = a->held[b];
    a->held[b] = pressed;
    if (pressed && !was) { a->held_t[b] = 0.0f; a->rep_t[b] = 0.0f; }
    a->idle = 0.0f;
    a->hot = 0.5f;
    if (pressed && was) return;                          /* ripetizioni del sistema: le fa update */
    if (!pressed && a->eat[b]) { a->eat[b] = 0; return; }
    if (pressed && (a->quit_dialog || a->panel || a->help) && b != PAD_SELECT) a->eat[b] = 1;

    if (a->quit_dialog) {
        if (!pressed) return;
        if (b == PAD_A || b == PAD_DOWN) a->quit = 1;
        else if (b == PAD_B || b == PAD_UP || b == PAD_MENU) a->quit_dialog = 0;
        return;
    }
    if (b == PAD_MENU) { if (pressed) { a->quit_dialog = 1; a->help = 0; } return; }
    if (b == PAD_SELECT) {
        if (pressed) a->sel_down = 1;
        else {
            if (a->sel_down && !a->help && !a->panel) set_mode(a, !a->mode);
            a->help = 0;
            a->sel_down = 0;
        }
        return;
    }
    if (a->help) return;
    if (a->panel) { if (pressed) panel_button(a, b); return; }
    if (b == PAD_START) { if (pressed) { a->panel = 1; a->pscroll = clampi(a->prow - 5, 0, R_COUNT); } return; }
    if (b == PAD_B) {
        if (pressed) a->b_used = 0;
        else if (!a->b_used) {
            if (a->mode == MODE_GARDEN) undo_seed(a);
            else ff_clear_cell(a, a->cell_r, a->cell_c);
        }
        return;
    }
    if (!pressed) return;
    if (a->mode == MODE_GARDEN) garden_button(a, b);
    else ff_button(a, b);
}

void fioapp_axes(FioApp *a, float lx, float ly, float rx, float ry, float l2, float r2)
{
    (void)rx; (void)ry; (void)l2; (void)r2;
    a->lx = fabsf(lx) > 0.18f ? lx : 0.0f;
    a->ly = fabsf(ly) > 0.18f ? ly : 0.0f;
    if (a->lx != 0.0f || a->ly != 0.0f) { a->idle = 0.0f; a->hot = 0.5f; }
}

/* ripetizione di un tasto tenuto: 1 a ogni scatto dopo il primo */
static int repeat(FioApp *a, int b, float first, float every)
{
    if (!a->held[b] || a->held_t[b] < first) return 0;
    if (a->rep_t[b] > 0.0f) return 0;
    a->rep_t[b] = every;
    return 1;
}

static void input_tick(FioApp *a, float dt)
{
    for (int b = 0; b < PAD_COUNT; b++)
        if (a->held[b]) { a->held_t[b] += dt; if (a->rep_t[b] > 0.0f) a->rep_t[b] -= dt; }
    if (a->quit_dialog) return;
    if (a->held[PAD_SELECT] && a->sel_down && a->held_t[PAD_SELECT] >= 0.35f && !a->panel) a->help = 1;
    if (a->help) return;
    if (a->panel) {
        if (repeat(a, PAD_UP, 0.32f, 0.08f)) panel_button(a, PAD_UP);
        if (repeat(a, PAD_DOWN, 0.32f, 0.08f)) panel_button(a, PAD_DOWN);
        if (repeat(a, PAD_LEFT, 0.32f, 0.07f)) panel_button(a, PAD_LEFT);
        if (repeat(a, PAD_RIGHT, 0.32f, 0.07f)) panel_button(a, PAD_RIGHT);
        return;
    }
    /* B tenuto: svuota */
    if (a->held[PAD_B] && !a->b_used && a->held_t[PAD_B] >= 0.8f) {
        a->b_used = 1;
        if (a->mode == MODE_GARDEN) clear_garden(a);
        else { a->nff = 0; toast(a, "Lucciole liberate"); a->dirty = 1; }
    }
    if (a->mode == MODE_GARDEN) {
        if (repeat(a, PAD_UP, 0.3f, 0.11f)) garden_button(a, PAD_UP);
        if (repeat(a, PAD_DOWN, 0.3f, 0.11f)) garden_button(a, PAD_DOWN);
        if (repeat(a, PAD_L2, 0.4f, 0.15f)) memoria_step(a, -1);
        if (repeat(a, PAD_R2, 0.4f, 0.15f)) memoria_step(a, 1);
        float mx = (float)(a->held[PAD_RIGHT] - a->held[PAD_LEFT]);
        float ht = fmaxf(a->held_t[PAD_LEFT] * a->held[PAD_LEFT], a->held_t[PAD_RIGHT] * a->held[PAD_RIGHT]);
        a->cx += mx * 210.0f * (1.0f + 1.4f * clamp01(ht / 0.7f)) * dt;
        if (a->lx != 0.0f || a->ly != 0.0f) {
            a->cx += a->lx * 340.0f * dt;
            a->cy = clampf(a->cy + a->ly * 300.0f * dt, G_TOP, G_BOT);
            a->cdeg = y_deg(a, a->cy);
        }
        a->cx = clampf(a->cx, G_LEFT, G_RIGHT);
        /* A tenuto: ogni corda nuova sotto il cursore e' un fiore (glissando) */
        if (a->held[PAD_A] && a->cdeg != a->a_last_deg && a->t - a->a_last_t > 0.045f) {
            plant(a, a->cdeg, 0.0f, clamp01((a->cx - G_LEFT) / (G_RIGHT - G_LEFT)), 0.72f, 0);
            a->a_last_deg = a->cdeg;
            a->a_last_t = a->t;
        }
    } else {
        int dirs[4] = { PAD_UP, PAD_DOWN, PAD_LEFT, PAD_RIGHT };
        for (int k = 0; k < 4; k++) if (repeat(a, dirs[k], 0.28f, 0.08f)) ff_move(a, dirs[k]);
        if (repeat(a, PAD_L2, 0.4f, 0.1f)) tempo_step(a, -1);
        if (repeat(a, PAD_R2, 0.4f, 0.1f)) tempo_step(a, 1);
        /* levetta: una casella per scatto */
        static float st = 0.0f;
        st -= dt;
        if ((fabsf(a->lx) > 0.6f || fabsf(a->ly) > 0.6f) && st <= 0.0f) {
            if (fabsf(a->lx) > fabsf(a->ly)) ff_move(a, a->lx > 0 ? PAD_RIGHT : PAD_LEFT);
            else ff_move(a, a->ly > 0 ? PAD_DOWN : PAD_UP);
            st = 0.14f;
        }
    }
}

/* ------------------------------------------------------------------ grafica: eventi e animazioni */
static void bloom_add(FioApp *a, float x, float y, float s, uint32_t col, int petals, int small)
{
    Bloom *b;
    if (a->nblooms < MAX_BLOOMS) b = &a->blooms[a->nblooms++];
    else {
        b = &a->blooms[0];
        for (int i = 1; i < a->nblooms; i++) if (a->blooms[i].age / a->blooms[i].life > b->age / b->life) b = &a->blooms[i];
    }
    b->x = x; b->y = y; b->s = clamp01(s); b->col = col; b->petals = petals; b->small = small;
    b->age = 0.0f;
    b->life = small ? 0.9f : 2.2f + 1.2f * b->s;
    b->rot = frand(a) * 2.0f * PI_F;
}

static void vis_spawn(FioApp *a, const Vis *v)
{
    if (v->kind == VIS_BLOOM) {
        if (a->mode == MODE_GARDEN) bloom_add(a, v->x, v->y, v->s, v->col, 5 + (v->b & 1), 0);
        if (v->a >= 0 && v->a < 64) a->line_glow[v->a] = fmaxf(a->line_glow[v->a], 0.4f + 0.6f * v->s);
        for (int i = 0; i < MAX_SEEDS; i++) if (a->seeds[i].alive && a->seeds[i].serial == v->b) a->seeds[i].glow = 1.0f;
        if (a->mode == MODE_GARDEN && v->a >= a->lp_base && v->a < a->lp_base + 8) {
            int c = clampi((int)((v->x - G_LEFT) / (G_RIGHT - G_LEFT) * 8.0f), 0, 7);
            a->lp_flash[v->a - a->lp_base][c] = 1.0f;
        }
    } else {
        int side = v->a / GRID, idx = v->a % GRID;
        a->wall[side][idx] = 1.0f;
        if (a->mode == MODE_FIREFLIES) {
            bloom_add(a, v->x, v->y, v->s, v->col, 0, 1);
            int r = side == 0 ? 7 : side == 2 ? 0 : idx, c = side == 1 ? 7 : side == 3 ? 0 : idx;
            a->lp_flash[r][c] = 1.0f;
        }
    }
}

void fioapp_update(FioApp *a, float dt)
{
    a->t += dt;
    a->now = fio_clock(a->dsp);
    if (a->sched < a->now) a->sched = a->now;
    long long horizon = a->now + (long long)(LOOKAHEAD * a->sr);
    if (horizon > a->sched) {
        schedule_garden(a, a->sched, horizon);
        schedule_fireflies(a, a->sched, horizon);
        a->sched = horizon;
    }
    /* luci all'istante in cui le note escono (meno un blocco audio) */
    long long vis_now = a->now - 1024;
    for (int i = 0; i < a->nvis;) {
        if (a->vis[i].when <= vis_now) { vis_spawn(a, &a->vis[i]); a->vis[i] = a->vis[--a->nvis]; }
        else i++;
    }

    a->idle += dt;
    if (a->hot > 0.0f) a->hot -= dt;
    input_tick(a, dt);
    ear_tick(a, dt);
    dream_tick(a, dt);

    /* animazioni */
    float k = 1.0f - expf(-dt * 16.0f);
    a->cy_draw += (a->cy - a->cy_draw) * k;
    for (int i = 0; i < a->nblooms;) {
        Bloom *b = &a->blooms[i];
        b->age += dt;
        if (b->age >= b->life) { *b = a->blooms[--a->nblooms]; continue; }
        i++;
    }
    float dk = expf(-dt * 2.2f);
    for (int d = 0; d < 64; d++) a->line_glow[d] *= dk;
    for (int s = 0; s < 4; s++) for (int i = 0; i < GRID; i++) a->wall[s][i] *= expf(-dt * 5.0f);
    for (int r = 0; r < 8; r++) for (int c = 0; c < 8; c++) a->lp_flash[r][c] *= expf(-dt * 4.0f);
    for (int i = 0; i < MAX_SEEDS; i++) {
        Seed *s = &a->seeds[i];
        if (!s->alive) continue;
        s->glow *= expf(-dt * 1.6f);
        float ty = seed_y(a, s);
        if (s->ydraw < 0.0f) s->ydraw = ty;
        s->ydraw += (ty - s->ydraw) * (1.0f - expf(-dt * 3.0f));
    }
    for (int i = 0; i < N_POLLEN; i++) {
        Pollen *p = &a->pollen[i];
        p->y -= p->sp * dt;
        p->ph += dt * (0.15f + p->r * 0.05f);
        p->x += sinf(p->ph * 2.0f * PI_F) * 6.0f * dt + a->gust * 170.0f * dt;
        if (p->x > W + 4.0f) p->x -= W + 8.0f;
        if (p->y < 30.0f) { p->y = H + 4.0f; p->x = frand(a) * W; }
    }
    if (a->sky_t < 1.0f) a->sky_t = fminf(1.0f, a->sky_t + dt / 1.4f);
    float want = (a->idle < 5.0f || a->panel || a->quit_dialog || a->help) ? 1.0f : 0.0f;
    a->chrome += (want - a->chrome) * (1.0f - expf(-dt * (want > a->chrome ? 10.0f : 1.2f)));
    if (a->toast_t > 0.0f) a->toast_t -= dt;

    rec_pump(a);
    a->lp_timer -= dt;
    if (a->lp_timer <= 0.0f) { lp_render(a); a->lp_timer = 1.0f / 30.0f; }
    a->save_t += dt;
    if (a->dirty && a->save_t > 20.0f && !a->panel) { fioapp_save(a); a->save_t = 0.0f; }
}

int fioapp_needs_draw(FioApp *a)
{
    if (a->hot > 0.0f) return 1;            /* mentre si tocca: tutti i fotogrammi */
    /* poi 30 al secondo; quando la cornice e' sparita e nessuno tocca niente, 20 bastano per luci cosi'
       lente (e la batteria dura di piu') */
    int every = a->chrome < 0.05f && !a->panel && !a->help && !a->quit_dialog && a->ear_busy <= 0.0f ? 3 : 2;
    if (++a->draw_toggle >= every) { a->draw_toggle = 0; return 1; }
    return 0;
}

int fioapp_wants_quit(const FioApp *a) { return a->quit; }

/* ------------------------------------------------------------------ disegno */
static void draw_nebula(FioApp *a, Canvas *c);

static void draw_sky(FioApp *a, Canvas *c)
{
    const uint32_t *cur = a->sky[a->mood], *from = a->sky[a->sky_from];
    if (!cur || !a->bg || c->w != W || c->h != H) {
        gfx_vgradient(c, 0, 0, c->w, c->h, MOODS[a->mood].top, MOODS[a->mood].bottom);
        return;
    }
    /* il cielo e le nebbie cambiano piano: si ricompongono un fotogramma su BG_EVERY */
    if (--a->bg_wait <= 0) {
        Canvas bg = { a->bg, W, H };
        if (a->sky_t < 1.0f && from && from != cur) fx_blend(a->bg, from, cur, W * H, smooth01(a->sky_t));
        else memcpy(a->bg, cur, sizeof(uint32_t) * W * H);
        draw_nebula(a, &bg);
        a->bg_wait = BG_EVERY;
    }
    memcpy(c->px, a->bg, sizeof(uint32_t) * W * H);
}

static void draw_pollen(FioApp *a, Canvas *c)
{
    uint32_t col = MOODS[a->mood].col[2];
    for (int i = 0; i < N_POLLEN; i++) {
        const Pollen *p = &a->pollen[i];
        float tw = 0.5f + 0.5f * sinf((p->ph * 3.0f + (float)i) * 2.0f * PI_F);
        fx_glow(c, p->x, p->y, p->r * 2.2f, col, 0.06f + 0.12f * tw);
    }
}

static void draw_bloom(Canvas *c, const Bloom *b)
{
    float t = b->age / b->life, u = 1.0f - t;
    float e = 1.0f - u * u * u;                         /* si apre in fretta e rallenta */
    if (b->small) {
        fx_ring(c, b->x, b->y, 4.0f + 20.0f * e, 1.6f, b->col, u * u * (0.4f + 0.4f * b->s));
        fx_glow(c, b->x, b->y, 8.0f + 8.0f * b->s, b->col, u * u * 0.7f);
        return;
    }
    float r = 6.0f + (24.0f + 58.0f * b->s) * e;
    fx_ring(c, b->x, b->y, r, 1.0f + 1.8f * u, b->col, powf(u, 1.6f) * (0.30f + 0.45f * b->s));
    if (t < 0.6f) fx_ring(c, b->x, b->y, r * 0.55f, 1.0f, b->col, (0.6f - t) * 0.5f * b->s);
    fx_glow(c, b->x, b->y, (12.0f + 20.0f * b->s) * (1.0f - 0.45f * t), b->col, u * u * (0.45f + 0.4f * b->s));
    fx_glow(c, b->x, b->y, 3.0f + 4.0f * b->s, gfx_mix(b->col, RGB(255, 255, 255), 0.6f), u * u * u);
    if (b->petals > 0) {
        /* petali a goccia: tre luci in fila dal centro verso fuori, che si aprono e ruotano piano */
        float open = 0.35f + 0.65f * e, pr = (14.0f + 30.0f * b->s) * open;
        uint32_t pc = gfx_mix(b->col, RGB(255, 255, 255), 0.18f);
        for (int k = 0; k < b->petals; k++) {
            float ang = b->rot + 2.0f * PI_F * (float)k / (float)b->petals + t * 0.5f;
            float ca = cosf(ang), sa = sinf(ang);
            static const float POS[3] = { 0.38f, 0.66f, 0.9f }, RAD[3] = { 6.5f, 5.0f, 3.2f }, AL[3] = { 0.30f, 0.38f, 0.30f };
            for (int j = 0; j < 3; j++) {
                float d = pr * POS[j];
                fx_glow(c, b->x + ca * d, b->y + sa * d, (RAD[j] * (0.6f + 0.8f * b->s)) * (0.5f + 0.5f * u), pc, u * u * AL[j] * (0.6f + 0.6f * b->s));
            }
        }
    }
    if (t < 0.22f) {
        /* lampo a stella nei primi istanti */
        float k = 1.0f - t / 0.22f, L = (10.0f + 22.0f * b->s) * (0.6f + 0.4f * k);
        uint32_t w = gfx_mix(b->col, RGB(255, 255, 255), 0.7f);
        gfx_line(c, b->x - L, b->y, b->x + L, b->y, 1.2f, w, 0.55f * k * k);
        gfx_line(c, b->x, b->y - L * 0.8f, b->x, b->y + L * 0.8f, 1.2f, w, 0.45f * k * k);
    }
}

/* nebbie: tre grandi aloni dei colori dell'atmosfera che vagano piano dietro a tutto */
static void draw_nebula(FioApp *a, Canvas *c)
{
    const Mood *md = &MOODS[a->mood];
    static const float SP[3][4] = { { 0.016f, 0.012f, 0.0f, 1.3f }, { 0.010f, 0.015f, 2.1f, 0.4f }, { 0.013f, 0.009f, 4.0f, 2.6f } };
    for (int i = 0; i < 3; i++) {
        float x = W * (0.5f + 0.36f * sinf(a->t * SP[i][0] * 2.0f * PI_F + SP[i][2]));
        float y = H * (0.52f + 0.26f * sinf(a->t * SP[i][1] * 2.0f * PI_F + SP[i][3]));
        fx_glow_soft(c, x, y, 150.0f + 30.0f * (float)i, md->col[i], 0.05f);
    }
}

/* quello che il microfono porta nel giardino: petali, la scia del canto, lucciole nate dalle mani */
static void draw_ear(FioApp *a, Canvas *c)
{
    for (int i = 0; i < a->npetals; i++) {
        const Petal *p = &a->petals[i];
        float u = 1.0f - p->age / p->life, fin = fminf(1.0f, p->age * 6.0f) * u;
        fx_glow_soft(c, p->x, p->y, p->r * 2.6f, p->col, 0.3f * fin);
        fx_glow(c, p->x, p->y, p->r, gfx_mix(p->col, RGB(255, 255, 255), 0.45f), 0.75f * fin);
    }
    uint32_t vc = gfx_mix(MOODS[a->mood].col[2], RGB(255, 255, 255), 0.3f);
    for (int k = 0; k < a->rib_n; k++) {
        int i = (a->rib_w - 1 - k + RIBBON) % RIBBON;
        float age = a->t - a->rib_t[i];
        if (age > 2.6f) break;
        float u = 1.0f - age / 2.6f;
        fx_glow(c, a->rib_x[i], a->rib_y[i], 3.5f + 6.0f * a->rib_a[i], vc, (0.12f + 0.3f * a->rib_a[i]) * u * u);
        if (k + 1 < a->rib_n) {
            int j = (i - 1 + RIBBON) % RIBBON;
            if (a->rib_t[i] - a->rib_t[j] < 0.12f && fabsf(a->rib_x[i] - a->rib_x[j]) < 60.0f)
                gfx_line(c, a->rib_x[j], a->rib_y[j], a->rib_x[i], a->rib_y[i], 2.0f, vc, 0.8f * u);
        }
    }
    if (a->voice_a > 0.01f) {                     /* la punta della penna, all'altezza esatta della voce */
        float x = x_pix(a->pen_x);
        fx_glow(c, x, a->voice_y, 16.0f, vc, 0.3f * a->voice_a);
        fx_glow(c, x, a->voice_y, 4.0f, RGB(255, 255, 255), 0.85f * a->voice_a);
    }
    uint32_t fc = MOODS[a->mood].col[2];
    for (int i = 0; i < a->nsparks; i++) {
        const Spark *s = &a->sparks[i];
        float u = 1.0f - s->age / s->life, len = sqrtf(s->vx * s->vx + s->vy * s->vy) + 1.0f;
        for (int k = 1; k <= 4; k++)
            fx_glow(c, s->x - s->vx / len * 11.0f * (float)k, s->y - s->vy / len * 11.0f * (float)k, 13.0f - 2.0f * (float)k, fc, 0.32f * u / (float)k);
        fx_glow(c, s->x, s->y, 36.0f, fc, 0.22f * u);
        fx_glow(c, s->x, s->y, 18.0f, fc, 0.4f * u);
        fx_glow(c, s->x, s->y, 7.0f, gfx_mix(fc, RGB(255, 255, 255), 0.6f), u);
    }
}

static void draw_garden(FioApp *a, Canvas *c)
{
    const Scale *s = scale_of(a);
    int n = g_ndeg(a);
    /* le corde: con il microfono vibrano per simpatia con i suoni della stanza, e il soffio le fa ondeggiare */
    for (int d = 0; d < n; d++) {
        float y = deg_y(a, d);
        int tonic = d % s->n == 0;
        float res = d < 64 ? a->sres[d] : 0.0f;
        float al = (tonic ? 0.085f : 0.04f) + 0.30f * (d < 64 ? a->line_glow[d] : 0.0f) + 0.5f * res;
        if (d == a->cdeg) al += 0.08f;
        float as = 7.0f * res, aw = 5.0f * a->gust;
        if (as + aw < 0.3f) fx_hline(c, y, (int)G_LEFT, (int)G_RIGHT, palette_y(a, y), al);
        else fx_wave(c, y, (int)G_LEFT, (int)G_RIGHT, palette_y(a, y), al, as, 1 + d % 3, a->t * (23.0f + 1.7f * (float)d),
                     aw, a->t * 9.0f - (float)d * 0.45f);
    }
    draw_pollen(a, c);
    /* il fronte del vento */
    if (a->front_on) {
        uint32_t wc = MOODS[a->mood].col[2];
        for (int k = 0; k < 6; k++) {
            float y = G_TOP + (G_BOT - G_TOP) * ((float)k + 0.5f) / 6.0f;
            fx_glow_soft(c, a->front_x + sinf(a->t * 3.0f + (float)k * 1.3f) * 8.0f, y, 50.0f, wc, 0.05f + 0.1f * a->gust);
        }
    }
    /* il giro: una linea in alto con il punto che scorre e i fiori come tacche */
    double ph = g_phase_at(a, a->now);
    float p = (float)(ph - floor(ph));
    fx_hline(c, 44.5f, (int)G_LEFT, (int)G_RIGHT, MOODS[a->mood].col[1], 0.06f);
    for (int i = 0; i < MAX_SEEDS; i++) {
        const Seed *sd = &a->seeds[i];
        if (!sd->alive) continue;
        fx_glow(c, x_pix((float)sd->phase), 44.5f, 2.2f + 2.4f * sd->glow, palette_y(a, sd->ydraw), 0.14f + 0.6f * sd->glow);
    }
    fx_glow(c, x_pix(p), 44.5f, 5.0f, MOODS[a->mood].col[2], 0.55f);
    /* i semi */
    for (int i = 0; i < MAX_SEEDS; i++) {
        const Seed *sd = &a->seeds[i];
        if (!sd->alive) continue;
        int M = a->g.memoria;
        float life = M > 0 ? 1.0f - (float)sd->done / (float)(M + 1) : 0.8f;
        float x = x_pix(sd->x), y = sd->ydraw;
        uint32_t col = palette_y(a, y);
        float breath = 0.85f + 0.15f * sinf(a->t * 1.7f + (float)sd->serial);
        fx_glow(c, x, y, 11.0f + 12.0f * sd->glow, col, (0.06f + 0.10f * life) * breath + 0.25f * sd->glow);
        fx_glow(c, x, y, 5.0f + 6.0f * sd->glow, col, (0.12f + 0.35f * life) * breath + 0.4f * sd->glow);
        fx_glow(c, x, y, 2.2f, gfx_mix(col, RGB(255, 255, 255), 0.5f), 0.35f + 0.5f * life);
    }
    for (int i = 0; i < a->nblooms; i++) draw_bloom(c, &a->blooms[i]);
    draw_ear(a, c);
    /* il cursore del sogno */
    if (a->dreaming) {
        fx_ring(c, a->gx, a->gy, 11.0f, 1.4f, MOODS[a->mood].col[1], 0.35f);
        fx_glow(c, a->gx, a->gy, 9.0f, MOODS[a->mood].col[1], 0.12f);
    }
    /* il cursore */
    float on = a->dreaming ? 0.25f : 0.35f + 0.65f * a->chrome;
    uint32_t cc = MOODS[a->mood].col[2];
    float cy = a->cy_draw;
    fx_ring(c, a->cx, cy, 13.0f, 2.0f, cc, 0.8f * on);
    fx_glow(c, a->cx, cy, 16.0f, cc, 0.12f * on);
    fx_glow(c, a->cx, cy, 2.5f, cc, 0.8f * on);
    if (a->chrome > 0.02f && !a->dreaming) {
        char nn[16];
        note_label(nn, sizeof(nn), deg_note(a, a->cdeg));
        float lx = a->cx + 20.0f > G_RIGHT - 40.0f ? a->cx - 20.0f - (float)gfx_text_width(FONT_SMALL, nn) : a->cx + 20.0f;
        gfx_text(c, FONT_SMALL, (int)lx, (int)(cy + 5.0f), nn, fade(a, C_TEXT, 0.85f * a->chrome));
    }
}

static void draw_fireflies(FioApp *a, Canvas *c)
{
    const Mood *md = &MOODS[a->mood];
    static const int DIRC[4] = { 2, 1, 0, 1 };
    /* caselle */
    for (int r = 0; r < GRID; r++)
        for (int col = 0; col < GRID; col++) {
            float x, y;
            cell_xy(r, col, &x, &y);
            gfx_round_rect(c, x - CELL * 0.5f, y - CELL * 0.5f, CELL, CELL, 9.0f, md->col[1], 0.075f);
        }
    /* bordi: si accendono dove le lucciole li toccano */
    for (int side = 0; side < 4; side++)
        for (int i = 0; i < GRID; i++) {
            float x, y;
            wall_xy(side, i, &x, &y);
            float g = a->wall[side][i];
            uint32_t wc = palette(a, (float)i / (GRID - 1));
            if (side == 0 || side == 2) gfx_round_rect(c, x - CELL * 0.42f, y - 1.5f, CELL * 0.84f, 3.0f, 1.5f, wc, 0.18f + 0.7f * g);
            else gfx_round_rect(c, x - 1.5f, y - CELL * 0.42f, 3.0f, CELL * 0.84f, 1.5f, wc, 0.18f + 0.7f * g);
            if (g > 0.03f) fx_glow(c, x, y, 11.0f + 8.0f * g, wc, 0.55f * g);
        }
    draw_pollen(a, c);
    /* note delle righe (e delle colonne, uguali da sinistra a destra) */
    if (a->chrome > 0.02f) {
        for (int i = 0; i < GRID; i++) {
            float x, y;
            char nn[16];
            note_label(nn, sizeof(nn), (int)ff_note(a, i));
            cell_xy(i, 0, &x, &y);
            gfx_text_right(c, FONT_SMALL, (int)(GRID_X - 30.0f), (int)(y + 5.0f), nn, fade(a, C_MUTED, 0.75f * a->chrome));
        }
    }
    /* cursore */
    {
        float x, y;
        cell_xy(a->cell_r, a->cell_c, &x, &y);
        float pulse = 0.6f + 0.4f * sinf(a->t * 4.0f);
        gfx_round_frame(c, x - CELL * 0.5f - 2.0f, y - CELL * 0.5f - 2.0f, CELL + 4.0f, CELL + 4.0f, 11.0f, 2.0f, md->col[2], (0.45f + 0.45f * a->chrome) * pulse);
    }
    for (int i = 0; i < a->nblooms; i++) draw_bloom(c, &a->blooms[i]);
    /* lucciole: un salto rapido verso la casella nuova quando il passo suona */
    long long vis_now = a->now - 1024;
    float D = (float)fmin(0.09 * a->sr, 0.6 * a->f_len);
    float f = a->f_playing ? clamp01((float)(vis_now - (a->f_tcur - (long long)D)) / D) : 1.0f;
    float e = smooth01(f);
    static const float DX[4] = { 0, 1, 0, -1 }, DY[4] = { -1, 0, 1, 0 };
    for (int i = 0; i < a->nff; i++) {
        float x0, y0, x1, y1;
        cell_xy(a->pr[i], a->pc[i], &x0, &y0);
        cell_xy(a->ff[i].r, a->ff[i].c, &x1, &y1);
        float x = x0 + (x1 - x0) * e, y = y0 + (y1 - y0) * e;
        if (a->bump[i] && a->f_playing) {
            int bd = a->bump[i] - 1;
            float k = sinf(f * PI_F) * 9.0f;
            x += DX[bd] * k;
            y += DY[bd] * k;
        }
        int d = a->ff[i].d & 3;
        uint32_t col = md->col[DIRC[d]];
        if (e > 0.0f && e < 1.0f) {
            for (int k = 1; k <= 3; k++) {
                float tx = x - (x1 - x0) * 0.12f * (float)k, ty = y - (y1 - y0) * 0.12f * (float)k;
                fx_glow(c, tx, ty, 10.0f - 2.0f * (float)k, col, 0.22f / (float)k);
            }
        }
        float tw = 0.85f + 0.15f * sinf(a->t * 9.0f + (float)i * 1.7f);
        fx_glow(c, x, y, 30.0f + 18.0f * a->ff_flash, col, 0.14f * tw + 0.3f * a->ff_flash);
        fx_glow(c, x, y, 17.0f, col, 0.36f * tw);
        fx_glow(c, x, y, 6.0f, gfx_mix(col, RGB(255, 255, 255), 0.55f), 0.9f);
        fx_glow(c, x + DX[d] * 12.0f, y + DY[d] * 12.0f, 4.0f, gfx_mix(col, RGB(255, 255, 255), 0.4f), 0.8f);
    }
}

static void draw_chrome(FioApp *a, Canvas *c)
{
    if (a->mic && a->g.ascolto > 0.001f) {          /* l'orecchio: un punto che si accende con quello che sente */
        float lv = fear_level(a->ear);
        uint32_t ec = MOODS[a->mood].col[2];
        fx_glow(c, (float)W - 21.0f, 18.0f, 4.0f + 8.0f * lv, ec, 0.18f + 0.55f * lv);
        fx_glow(c, (float)W - 21.0f, 18.0f, 1.8f, gfx_mix(ec, RGB(255, 255, 255), 0.5f), 0.35f + 0.5f * lv);
    }
    float al = a->chrome;
    if (al < 0.02f && !a->rec) return;
    char t[200], v[64];
    if (al >= 0.02f) {
        gfx_rect_alpha(c, 0, 0, W, 34, RGB(0, 0, 0), 0.28f * al);
        snprintf(t, sizeof(t), "%s  ·  %s", a->mode == MODE_GARDEN ? "Giardino" : "Lucciole", MOODS[a->mood].name);
        gfx_text(c, FONT_BOLD, 16, 24, t, fade(a, C_TEXT, al));
        int n = seed_count(a);
        if (a->mode == MODE_GARDEN) {
            row_value(a, R_MEMORIA, v, sizeof(v));
            snprintf(t, sizeof(t), "%d %s  ·  memoria %s  ·  respiro %d s%s", n, n == 1 ? "fiore" : "fiori", v,
                     (int)lroundf(a->g.respiro), a->f_playing && a->nff ? "  ·  lucciole in volo" : "");
        } else {
            snprintf(t, sizeof(t), "%d %s  ·  %d BPM  ·  %s%s", a->nff, a->nff == 1 ? "lucciola" : "lucciole", (int)lroundf(a->g.tempo),
                     a->f_playing ? "in volo" : "ferme", n ? "  ·  il giardino suona" : "");
        }
        int x = 16 + gfx_text_width(FONT_BOLD, "Lucciole  ·  Pioggia") + 18;
        gfx_text(c, FONT_SMALL, x, 23, t, fade(a, C_MUTED, al));
        int rx = W - 14;
        if (a->mic) rx -= 16 + gfx_text_right(c, FONT_SMALL, rx - 16, 23, a->g.ascolto > 0.001f ? "ascolta" : "non ascolta",
                                              fade(a, C_GOLD, al * (a->g.ascolto > 0.001f ? 1.0f : 0.6f))) + 14;
        if (a->midi_dev[0]) {
            snprintf(v, sizeof(v), "%s", a->lp.connected ? "Launchpad" : "MIDI");
            rx -= gfx_text_right(c, FONT_SMALL, rx, 23, v, fade(a, C_GREEN, al)) + 14;
        }
        if (a->g.sogno) rx -= gfx_text_right(c, FONT_SMALL, rx, 23, a->dreaming ? "sogna..." : "sogno", fade(a, C_VIOLET, al * (a->dreaming ? 1.0f : 0.8f))) + 14;
    }
    if (a->rec) {
        int sec = (int)(a->rec_frames / (long long)a->sr);
        snprintf(v, sizeof(v), "REC %02d:%02d", sec / 60, sec % 60);
        float pulse = 0.6f + 0.4f * sinf(a->t * 5.0f);
        int w = gfx_text_width(FONT_SMALL, v);
        fx_glow(c, (float)(W - 24 - w), 50.0f, 6.0f, C_RED, pulse);
        gfx_text(c, FONT_SMALL, W - 14 - w, 55, v, C_RED);
    }
    if (al >= 0.02f && !a->panel) {
        const char *hint = a->mode == MODE_GARDEN
            ? "A pianta   B togli   X atmosfera   Y sogno   L2 R2 memoria   SELECT lucciole   START pannello"
            : "A lucciola   B togli   X atmosfera   Y vola   L2 R2 tempo   SELECT giardino   START pannello";
        gfx_text_center(c, FONT_SMALL, W / 2, H - 9, hint, fade(a, C_MUTED, 0.85f * al));
    }
}

static void draw_panel(FioApp *a, Canvas *c)
{
    const float x0 = 318.0f, y0 = 40.0f, w = 310.0f, h = 420.0f, rh = 27.0f;
    gfx_round_rect(c, x0, y0, w, h, 16.0f, C_PANEL, 0.86f);
    gfx_round_frame(c, x0, y0, w, h, 16.0f, 1.5f, MOODS[a->mood].col[1], 0.6f);
    char t[100], v[64];
    snprintf(t, sizeof(t), "Pannello  ·  %s", MOODS[a->mood].name);
    gfx_text(c, FONT_BOLD, (int)x0 + 18, (int)y0 + 30, t, C_TEXT);
    int visible = (int)((h - 64.0f) / rh);
    if (a->prow < a->pscroll) a->pscroll = a->prow;
    if (a->prow >= a->pscroll + visible) a->pscroll = a->prow - visible + 1;
    for (int i = 0; i < visible && a->pscroll + i < R_COUNT; i++) {
        int row = a->pscroll + i;
        float y = y0 + 46.0f + (float)i * rh;
        int sel = row == a->prow;
        if (sel) gfx_round_rect(c, x0 + 8.0f, y, w - 16.0f, rh - 3.0f, 8.0f, MOODS[a->mood].col[1], 0.22f);
        const char *name = row == R_REC && a->rec ? "Ferma la registrazione" : ROW_NAMES[row];
        gfx_text(c, FONT_BODY, (int)x0 + 20, (int)(y + 18.0f), name, sel ? C_TEXT : C_MUTED);
        row_value(a, row, v, sizeof(v));
        if (row == R_REC || row == R_RESET) {
            if (sel && !(row == R_REC && a->rec)) snprintf(v, sizeof(v), "premi A");
            else if (!sel && !(row == R_REC && a->rec)) v[0] = 0;
        }
        uint32_t vc = row == R_REC && a->rec ? C_RED : (sel ? C_GOLD : C_TEXT);
        if (sel && row != R_REC && row != R_RESET) {
            snprintf(t, sizeof(t), "<  %s  >", v);
            gfx_text_right(c, FONT_BOLD, (int)(x0 + w - 18.0f), (int)(y + 18.0f), t, vc);
        } else if (v[0]) {
            gfx_text_right(c, FONT_BODY, (int)(x0 + w - 18.0f), (int)(y + 18.0f), v, vc);
        }
    }
    if (a->pscroll > 0) gfx_text_center(c, FONT_SMALL, (int)(x0 + w * 0.5f), (int)y0 + 42, "...", C_DIM);
    if (a->pscroll + visible < R_COUNT) gfx_text_center(c, FONT_SMALL, (int)(x0 + w * 0.5f), (int)(y0 + h - 8.0f), "...", C_DIM);
    const char *hint = "su giù sceglie   ·   sinistra destra cambia   ·   L1 R1 atmosfera   ·   B chiude";
    if (a->prow == R_ASCOLTO)
        hint = a->mic ? "Canta: fiorisce   ·   soffia: c'è vento   ·   batti le mani: nasce una lucciola"
                      : "Collega un microfono USB-C: poi canta, soffia o batti le mani";
    gfx_text_center(c, FONT_SMALL, W / 2, H - 9, hint, a->prow == R_ASCOLTO ? C_GOLD : C_MUTED);
}

static void draw_help(FioApp *a, Canvas *c)
{
    gfx_rect_alpha(c, 0, 0, W, H, RGB(0, 0, 0), 0.55f);
    gfx_round_rect(c, 30, 34, 580, 412, 18, C_PANEL, 0.94f);
    gfx_round_frame(c, 30, 34, 580, 412, 18, 1.5f, MOODS[a->mood].col[1], 0.7f);
    gfx_text(c, FONT_TITLE, 52, 72, a->mode == MODE_GARDEN ? "Il giardino" : "Le lucciole", C_TEXT);
    static const char *G[][2] = {
        { "Croce su giù", "cambia corda (la nota)" }, { "Croce, levetta", "sposta il cursore (sinistra e destra = stereo)" },
        { "A", "pianta un fiore; tenuto, ogni corda nuova suona" }, { "B / B tenuto", "toglie l'ultimo fiore / svuota il giardino" },
        { "X, L1 R1", "atmosfera successiva / precedente" }, { "Y", "sogno: da fermo il giardino pianta da solo" },
        { "L2 R2", "memoria: quante volte torna ogni fiore" }, { "SELECT", "passa alle lucciole (tenuto: questo aiuto)" },
        { "START", "pannello: strumento, scala, spazio, registra" }, { "MENU", "esci" },
    };
    static const char *F[][2] = {
        { "Croce, levetta", "sceglie la casella" }, { "A", "nasce una lucciola, poi la gira; al quinto colpo vola via" },
        { "B / B tenuto", "toglie le lucciole della casella / tutte" }, { "Y", "fa volare o ferma le lucciole" },
        { "X, L1 R1", "atmosfera successiva / precedente" }, { "L2 R2", "tempo" },
        { "Le regole", "volano dritte; sul bordo suonano e tornano indietro" }, { "", "due nella stessa casella girano a destra" },
        { "SELECT", "torna al giardino (tenuto: questo aiuto)" }, { "START  ·  MENU", "pannello  ·  esci" },
    };
    const char *(*rows)[2] = a->mode == MODE_GARDEN ? G : F;
    for (int i = 0; i < 10; i++) {
        int y = 106 + i * 28;
        gfx_text(c, FONT_BOLD, 52, y, rows[i][0], C_GOLD);
        gfx_text(c, FONT_BODY, 200, y, rows[i][1], C_TEXT);
    }
    const char *lp = a->mode == MODE_GARDEN
        ? "Launchpad: la griglia sono 8 corde (su giù le sposta d'ottava), le colonne lo stereo"
        : "Launchpad: la griglia e' quella delle lucciole; User tenuto + pad le toglie";
    int ly = a->mic ? 384 : 400;
    gfx_text(c, FONT_SMALL, 52, ly, lp, C_MUTED);
    gfx_text(c, FONT_SMALL, 52, ly + 20, "Lati del Launchpad: le 6 atmosfere, svuota (con User), registra", C_MUTED);
    if (a->mic) gfx_text(c, FONT_SMALL, 52, 428, "Microfono: canta e fiorisce, soffia e c'è vento, batti le mani e nasce una lucciola", C_GOLD);
}

static void draw_quit(Canvas *c)
{
    gfx_rect_alpha(c, 0, 0, c->w, c->h, RGB(0, 0, 0), 0.6f);
    gfx_round_rect(c, 150, 170, 340, 140, 18, C_PANEL, 0.96f);
    gfx_round_frame(c, 150, 170, 340, 140, 18, 2, C_VIOLET, 1.0f);
    gfx_text_center(c, FONT_TITLE, 320, 222, "Uscire da Fioritura?", C_TEXT);
    gfx_text_center(c, FONT_BODY, 320, 256, "Il giardino e le lucciole restano salvati.", C_MUTED);
    gfx_text_center(c, FONT_BOLD, 320, 290, "A o giù: esci   ·   B o su: resta", C_GOLD);
}

void fioapp_draw(FioApp *a, Canvas *c)
{
    draw_sky(a, c);
    if (a->mode == MODE_GARDEN) draw_garden(a, c);
    else draw_fireflies(a, c);
    draw_chrome(a, c);
    if (a->toast_t > 0.0f && a->toast[0] && !a->help) {
        float al = fminf(1.0f, a->toast_t * 3.0f);
        int w = gfx_text_width(FONT_BOLD, a->toast) + 36;
        if (w > 620) w = 620;
        float y = a->mode == MODE_GARDEN ? 398.0f : 404.0f;
        gfx_round_rect(c, (float)(W - w) * 0.5f, y, (float)w, 34.0f, 17.0f, C_PANEL, 0.82f * al);
        gfx_text_center(c, FONT_BOLD, W / 2, (int)y + 23, a->toast, gfx_mix(C_PANEL, C_TEXT, al));
    }
    if (a->panel) draw_panel(a, c);
    if (a->help) draw_help(a, c);
    if (a->quit_dialog) draw_quit(c);
}

/* ------------------------------------------------------------------ per le prove */
Fio *fioapp_engine(FioApp *a) { return a->dsp; }
FioGlobal *fioapp_global(FioApp *a) { return &a->g; }
void fioapp_global_changed(FioApp *a)
{
    if (fabs(a->g_len - a->g.respiro * a->sr) > 1.0) g_set_len(a, a->g.respiro);
    ff_set_tempo(a);
    push_mix(a);
}
int fioapp_mode(const FioApp *a) { return a->mode; }
void fioapp_set_mode(FioApp *a, int mode) { set_mode(a, mode); }
int fioapp_mood(const FioApp *a) { return a->mood; }
void fioapp_set_mood(FioApp *a, int mood) { set_mood(a, mood); }
int fioapp_mood_count(void) { return MOOD_COUNT; }
const char *fioapp_mood_name(int m) { return m >= 0 && m < MOOD_COUNT ? MOODS[m].name : "?"; }
int fioapp_mood_timbre(const FioApp *a) { return mset(a)->timbre; }
void fioapp_set_cursor(FioApp *a, float x, float y)
{
    a->cx = x_pix(clamp01(x));
    a->cy = clampf(G_BOT - clamp01(y) * (G_BOT - G_TOP), G_TOP, G_BOT);
    a->cdeg = y_deg(a, a->cy);
    a->cy = deg_y(a, a->cdeg);
}
void fioapp_set_cell(FioApp *a, int row, int col) { a->cell_r = clampi(row, 0, GRID - 1); a->cell_c = clampi(col, 0, GRID - 1); }
int fioapp_seed_count(const FioApp *a) { return seed_count(a); }
int fioapp_seed_echoes(const FioApp *a, int i)
{
    for (int k = 0; k < MAX_SEEDS; k++) if (a->seeds[k].alive && i-- == 0) return a->seeds[k].done;
    return -1;
}
float fioapp_seed_pitch(const FioApp *a, int i)
{
    for (int k = 0; k < MAX_SEEDS; k++) if (a->seeds[k].alive && i-- == 0) return seed_note(a, &a->seeds[k]);
    return -1.0f;
}
int fioapp_firefly_count(const FioApp *a) { return a->nff; }
const Firefly *fioapp_fireflies(const FioApp *a) { return a->ff; }
int fioapp_playing(const FioApp *a) { return a->f_playing; }
int fioapp_panel(const FioApp *a) { return a->panel ? a->prow : -1; }
int fioapp_help(const FioApp *a) { return a->help; }
int fioapp_recording(const FioApp *a) { return a->rec != NULL; }
const char *fioapp_record_path(const FioApp *a) { return a->rec_path; }
const char *fioapp_toast(const FioApp *a) { return a->toast_t > 0.0f ? a->toast : ""; }
int fioapp_dreaming(const FioApp *a) { return a->dreaming; }
int fioapp_lp_connected(const FioApp *a) { return a->lp.connected; }
void fioapp_set_record_dir(FioApp *a, const char *dir) { snprintf(a->rec_dir, sizeof(a->rec_dir), "%s", dir); }
int fioapp_mic(const FioApp *a) { return a->mic; }
float fioapp_gust(const FioApp *a) { return a->gust; }
int fioapp_petals(const FioApp *a) { return a->npetals; }
float fioapp_ear_strings(const FioApp *a, int deg) { return deg >= 0 && deg < 64 ? a->sres[deg] : 0.0f; }
