/* Fioritura per Miyoo Flip - motore audio (vedi fio_dsp.h). */
#include "fio_dsp.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SUB 32                      /* passo di lavoro: le note partono a multipli di 32 campioni */
#define QUEUE 1024                  /* coda interfaccia -> audio */
#define PENDING 512                 /* note in attesa del loro istante */
#define SINE_N 4096
#define KS_MAX 2400                 /* corda piu' grave: 20 Hz a 48 kHz */
#define TAP_FRAMES (48000 * 4)
#define SH_LEN 4096
#define SH_WIN 2048
#define PI_F 3.14159265f

static float SINE[SINE_N + 1];

enum { EV_NOTE, EV_MIX, EV_SILENCE };

typedef struct { long long when; int type; FioNote n; FioMix m; } Event;
typedef struct { long long when; FioNote n; } Pend;
typedef struct { float *buf; int len, pos; float store; } Comb;
typedef struct { float *buf; int len, pos; } Allpass;

typedef struct {
    int   active, timbre, killing;
    float gl, gr;                   /* guadagni stereo, dinamica compresa */
    float t;                        /* secondi dall'inizio */
    float env, dec;                 /* inviluppo che decide la fine della voce e il suo decadimento */
    float att, att_inc;             /* attacco 0..1 */
    float kill;                     /* spegnimento rapido (voce rubata o silenzio) */
    float freq;
    float ph[4], inc[4], amp[4], pdec[4];
    float mph, minc, idx, idec;     /* campana: modulante FM */
    float *ks; int klen, kpos; float kfb, kb, klast, kap, kz;   /* arpa: corda (Karplus-Strong) */
    float y1, y2, a1, a2, ng, breath;                           /* soffio: rumore intonato */
    float hold;                     /* soffio: secondi prima del decadimento */
    float vib;                      /* fase del vibrato */
    float glide, gdec;              /* goccia: intonazione che sale e si ferma */
    float click, cdec, clp;         /* kalimba: colpo del dito */
    unsigned age;
    int wait;                       /* campioni di silenzio prima di partire (istante esatto) */
} Voice;

struct Fio {
    float sr, inv_sr;
    Voice v[FIO_VOICES];
    float *ks_mem;
    Event q[QUEUE];
    int q_head, q_tail;             /* head: lo scrive l'interfaccia; tail: il thread audio */
    Pend pend[PENDING];
    int npend;
    FioMix mix, mix_t;              /* corrente (levigato) e voluto */
    int have_mix;
    float mix_l[SUB], mix_r[SUB];
    /* eco ping-pong */
    float *dl, *dr; int dlen, dpos; float dlp_l, dlp_r, dts;
    /* riverbero */
    Comb cl[8], cr[8];
    Allpass al[4], ar[4];
    float *sh; int sh_pos; float sh_fb;
    /* bordone */
    float dr_ph[6], dr_note, dr_lp[2][2], dr_lfo, dr_amp;
    unsigned rng, age;
    long long clock, cur, last_start;      /* cur: inizio del passo in corso */
    float level, level_pub;
    int voices_pub, starts;
    /* registrazione */
    float *tap; int tap_on, tap_w, tap_r, tap_lost;
};

/* ------------------------------------------------------------------ aiuti */
static inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static inline float clamp01(float x) { return clampf(x, 0.0f, 1.0f); }
static inline float midi_hz(float n) { return 440.0f * exp2f((n - 69.0f) / 12.0f); }
static inline float white(Fio *f)
{
    f->rng ^= f->rng << 13; f->rng ^= f->rng >> 17; f->rng ^= f->rng << 5;
    return (float)(int32_t)f->rng * (1.0f / 2147483648.0f);
}
static inline float softclip(float x)
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}
static inline float polyblep(float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}
/* sin(2 pi giri) da tabella */
static inline float sin_t(float turns)
{
    turns -= floorf(turns);
    float x = turns * SINE_N;
    int i = (int)x;
    float fr = x - (float)i;
    i &= SINE_N - 1;
    return SINE[i] + (SINE[i + 1] - SINE[i]) * fr;
}
static inline float wrap1(float p) { return p >= 1.0f ? p - 1.0f : p; }
/* coefficiente per campione di un decadimento che perde 60 dB in t secondi */
static inline float t60(float t, float sr) { return expf(-6.9078f / (fmaxf(t, 0.005f) * sr)); }

static inline void set_i(int *p, int v) { __atomic_store_n(p, v, __ATOMIC_RELEASE); }
static inline int get_i(const int *p) { return __atomic_load_n(p, __ATOMIC_ACQUIRE); }
static inline void set_ll(long long *p, long long v) { __atomic_store_n(p, v, __ATOMIC_RELEASE); }
static inline long long get_ll(const long long *p) { return __atomic_load_n(p, __ATOMIC_ACQUIRE); }
static inline void set_f(float *p, float v) { __atomic_store(p, &v, __ATOMIC_RELAXED); }
static inline float get_f(const float *p) { float v; __atomic_load(p, &v, __ATOMIC_RELAXED); return v; }

static void comb_init(Comb *c, int len) { c->buf = calloc((size_t)len, sizeof(float)); c->len = len; }
static void allpass_init(Allpass *a, int len) { a->buf = calloc((size_t)len, sizeof(float)); a->len = len; }
static inline float comb_run(Comb *c, float in, float fb, float damp)
{
    float o = c->buf[c->pos];
    c->store = o * (1.0f - damp) + c->store * damp;
    c->buf[c->pos] = in + c->store * fb;
    if (++c->pos >= c->len) c->pos = 0;
    return o;
}
static inline float allpass_run(Allpass *a, float in)
{
    float b = a->buf[a->pos];
    a->buf[a->pos] = in + b * 0.5f;
    if (++a->pos >= a->len) a->pos = 0;
    return b - in;
}
static inline float read_frac(const float *buf, int len, int pos, float delay)
{
    float rp = (float)pos - delay;
    while (rp < 0) rp += (float)len;
    int i0 = (int)rp;
    float fr = rp - (float)i0;
    if (i0 >= len) i0 -= len;
    int i1 = i0 + 1 >= len ? 0 : i0 + 1;
    return buf[i0] + (buf[i1] - buf[i0]) * fr;
}

static const char *TIMBRE_NAMES[TIM_COUNT] = { "Campana", "Vetro", "Arpa", "Soffio", "Goccia", "Kalimba" };
const char *fio_timbre_name(int t) { return t >= 0 && t < TIM_COUNT ? TIMBRE_NAMES[t] : "?"; }

/* Guadagno di ogni strumento: tarato perche' alla stessa dinamica suonino forti uguale (vedi le prove). */
static const float TIMBRE_GAIN[TIM_COUNT] = { 0.62f, 0.62f, 0.95f, 0.5f, 0.7f, 0.66f };

/* ------------------------------------------------------------------ voci */
static Voice *voice_alloc(Fio *f)
{
    for (int i = 0; i < FIO_VOICES; i++) if (!f->v[i].active) return &f->v[i];
    /* tutte occupate: la piu' debole (di solito una coda che sta finendo) */
    Voice *best = &f->v[0];
    float lo = best->env * best->kill;
    for (int i = 1; i < FIO_VOICES; i++) {
        float e = f->v[i].env * f->v[i].kill;
        if (e < lo) { lo = e; best = &f->v[i]; }
    }
    return best;
}

static void voice_start(Fio *f, const FioNote *n, int wait)
{
    Voice *v = voice_alloc(f);
    float *ks = v->ks;                      /* la linea della corda e' fissa per voce */
    memset(v, 0, sizeof(*v));
    v->ks = ks;
    const float sr = f->sr, inv = f->inv_sr;
    int tim = n->timbre >= 0 && n->timbre < TIM_COUNT ? n->timbre : TIM_CAMPANA;
    float f0 = clampf(midi_hz(n->note), 20.0f, 6000.0f);
    float vel = clamp01(n->vel), b = clamp01(n->bright), L = clampf(n->length, 0.2f, 20.0f);
    float amp = (0.15f + 0.85f * vel * vel) * TIMBRE_GAIN[tim];
    float pa = (clampf(n->pan, -1.0f, 1.0f) + 1.0f) * PI_F * 0.25f;
    v->active = 1;
    v->timbre = tim;
    v->gl = cosf(pa) * amp;
    v->gr = sinf(pa) * amp;
    v->freq = f0;
    v->env = 1.0f;
    v->kill = 1.0f;
    v->age = ++f->age;
    v->wait = wait > 0 ? wait : 0;
    v->att_inc = 1.0f / (0.002f * sr);
    for (int k = 0; k < 4; k++) v->ph[k] = 0.0f;

    switch (tim) {
    case TIM_CAMPANA:
        /* FM con rapporto 3,5 (parziali inarmoniche da campana), una seconda portante appena calante che
           batte con la prima e il «hum» un'ottava sotto, che resta piu' a lungo */
        v->inc[0] = f0 * inv;
        v->inc[1] = (f0 * 1.0021f + 0.3f) * inv;
        v->inc[2] = f0 * 0.5f * inv;
        v->amp[2] = 0.18f;
        v->pdec[2] = t60(L * 1.3f, sr);
        v->minc = f0 * 3.5f * inv;
        v->idx = (0.5f + 2.4f * b) / (2.0f * PI_F);
        v->idec = expf(-1.0f / (L * 0.16f * sr));
        v->dec = t60(L, sr);
        break;
    case TIM_VETRO: {
        /* barretta libera (glockenspiel, vetro): parziali 1 - 2,756 - 5,404 - 8,933, le alte durano meno */
        static const float R[4] = { 1.0f, 2.756f, 5.404f, 8.933f };
        const float A[4] = { 1.0f, 0.28f + 0.2f * b, 0.06f + 0.16f * b, 0.08f * b };
        static const float T[4] = { 1.0f, 0.42f, 0.2f, 0.1f };
        for (int k = 0; k < 4; k++) {
            v->inc[k] = f0 * R[k] * inv;
            v->amp[k] = f0 * R[k] < sr * 0.45f ? A[k] : 0.0f;
            v->pdec[k] = t60(L * T[k], sr);
        }
        v->att_inc = 1.0f / (0.004f * sr);
        v->dec = v->pdec[0];
        break;
    }
    case TIM_ARPA: {
        /* corda pizzicata: un colpo di rumore filtrato in una linea lunga un periodo, che si ripete perdendo
           le alte. Ritardo totale = linea + mezzo campione della media + allpass frazionario. */
        float P = sr / f0;
        int len = (int)(P - 0.6f);
        if (len < 2) len = 2;
        if (len > KS_MAX) len = KS_MAX;
        float d = P - 0.5f - (float)len;
        d = clampf(d, 0.1f, 1.1f);
        v->kap = (1.0f - d) / (1.0f + d);
        v->klen = len;
        float lp = 0.0f, coef = 0.2f + 0.75f * b, mean = 0.0f, pw = 0.0f;
        for (int i = 0; i < len; i++) { lp += coef * (white(f) - lp); v->ks[i] = lp; mean += lp; }
        mean /= (float)len;
        for (int i = 0; i < len; i++) { v->ks[i] -= mean; pw += v->ks[i] * v->ks[i]; }
        float g = pw > 0.0f ? 0.6f / sqrtf(pw / (float)len) : 0.0f;
        for (int i = 0; i < len; i++) v->ks[i] *= g;
        v->kfb = fminf(expf(-6.9078f / (L * f0)), 0.99985f);
        v->kb = 0.1f + 0.25f * b;
        v->dec = t60(L, sr);
        v->att_inc = 1.0f / (0.001f * sr);
        break;
    }
    case TIM_SOFFIO: {
        /* soffio: seno con un po' di seconda e terza armonica, rumore intonato da un risonatore sulla nota,
           attacco lento, vibrato che arriva dopo */
        v->inc[0] = f0 * inv; v->amp[0] = 1.0f;
        v->inc[1] = 2.0f * f0 * inv; v->amp[1] = 0.16f;
        v->inc[2] = 3.0f * f0 * inv; v->amp[2] = 0.05f * b;
        float bw = fmaxf(f0 / 9.0f, 25.0f), r = expf(-PI_F * bw * inv);
        v->a1 = 2.0f * r * cosf(2.0f * PI_F * f0 * inv);
        v->a2 = -r * r;
        /* rumore bianco uniforme (varianza 1/3) in un risonatore stretto: varianza in uscita circa
           g^2 / (3 * 4 (1 - r) sin^2 theta); g la porta a RMS 0,5 */
        v->ng = 0.5f * sqrtf(12.0f * (1.0f - r)) * fabsf(sinf(2.0f * PI_F * f0 * inv));
        v->breath = 0.22f + 0.4f * b;
        v->att_inc = 1.0f / (0.38f * sr);
        v->hold = L * 0.22f;
        v->dec = t60(L * 0.8f, sr);
        break;
    }
    case TIM_GOCCIA:
        /* goccia: un seno che sale di colpo fino alla nota (la bolla che si chiude) e svanisce presto */
        v->inc[0] = f0 * inv; v->amp[0] = 1.0f;
        v->inc[1] = 2.02f * f0 * inv; v->amp[1] = 0.1f + 0.1f * b;
        v->pdec[1] = t60(0.08f, sr);
        v->glide = -0.42f;
        v->gdec = expf(-1.0f / (0.016f * sr));
        v->dec = t60(clampf(0.16f + L * 0.07f, 0.16f, 0.7f), sr);
        v->att_inc = 1.0f / (0.0005f * sr);
        break;
    case TIM_KALIMBA: {
        static const float R[3] = { 1.0f, 6.27f, 11.9f };
        const float A[3] = { 1.0f, 0.12f + 0.24f * b, 0.07f * b };
        static const float T[3] = { 0.6f, 0.06f, 0.03f };
        for (int k = 0; k < 3; k++) {
            v->inc[k] = f0 * R[k] * inv;
            v->amp[k] = f0 * R[k] < sr * 0.45f ? A[k] : 0.0f;
            v->pdec[k] = t60(L * T[k], sr);
        }
        v->click = 0.25f + 0.2f * b;
        v->cdec = expf(-1.0f / (0.003f * sr));
        v->dec = v->pdec[0];
        v->att_inc = 1.0f / (0.0008f * sr);
        break;
    }
    }
    f->starts++;
    f->last_start = f->cur + v->wait;
}

/* Una voce per n campioni, sommata in mix_l/mix_r. */
static void voice_render(Fio *f, Voice *v, int n)
{
    const float inv = f->inv_sr;
    float *ml = f->mix_l, *mr = f->mix_r;
    const float kdec = expf(-1.0f / (0.012f * f->sr));
    int i0 = 0;
    if (v->wait > 0) { i0 = v->wait < n ? v->wait : n; v->wait -= i0; }
    for (int i = i0; i < n; i++) {
        float x = 0.0f;
        switch (v->timbre) {
        case TIM_CAMPANA: {
            float m = sin_t(v->mph) * v->idx;
            v->mph = wrap1(v->mph + v->minc);
            v->idx *= v->idec;
            x = (sin_t(v->ph[0] + m) + 0.55f * sin_t(v->ph[1] + m * 0.6f)) * 0.66f + sin_t(v->ph[2]) * v->amp[2];
            v->ph[0] = wrap1(v->ph[0] + v->inc[0]);
            v->ph[1] = wrap1(v->ph[1] + v->inc[1]);
            v->ph[2] = wrap1(v->ph[2] + v->inc[2]);
            v->amp[2] *= v->pdec[2];
            x *= v->env;
            break;
        }
        case TIM_VETRO:
        case TIM_KALIMBA:
            for (int k = 0; k < 4; k++) {
                if (v->amp[k] <= 0.0f) continue;
                x += sin_t(v->ph[k]) * v->amp[k];
                v->ph[k] = wrap1(v->ph[k] + v->inc[k]);
                v->amp[k] *= v->pdec[k];
            }
            if (v->click > 1e-4f) {
                v->clp += 0.35f * (white(f) - v->clp);
                x += v->clp * v->click;
                v->click *= v->cdec;
            }
            break;
        case TIM_ARPA: {
            float s = v->ks[v->kpos];
            float avg = 0.5f * (s + v->klast);
            v->klast = s;
            float lp = avg + v->kb * (s - avg);
            float ap = v->kap * lp + v->kz;
            v->kz = lp - v->kap * ap;
            v->ks[v->kpos] = ap * v->kfb;
            if (++v->kpos >= v->klen) v->kpos = 0;
            x = s;
            break;
        }
        case TIM_SOFFIO: {
            float depth = 0.0026f * fminf(1.0f, fmaxf(0.0f, v->t - 0.5f));
            float vm = 1.0f + depth * sin_t(v->vib);
            v->vib = wrap1(v->vib + 4.7f * inv);
            for (int k = 0; k < 3; k++) {
                x += sin_t(v->ph[k]) * v->amp[k];
                v->ph[k] = wrap1(v->ph[k] + v->inc[k] * vm);
            }
            float y = v->ng * white(f) + v->a1 * v->y1 + v->a2 * v->y2;
            v->y2 = v->y1;
            v->y1 = y;
            x = x * 0.8f + y * v->breath;
            x *= v->env;
            break;
        }
        case TIM_GOCCIA: {
            float g = 1.0f + v->glide;
            v->glide *= v->gdec;
            x = sin_t(v->ph[0]) + sin_t(v->ph[1]) * v->amp[1];
            v->ph[0] = wrap1(v->ph[0] + v->inc[0] * g);
            v->ph[1] = wrap1(v->ph[1] + v->inc[1] * g);
            v->amp[1] *= v->pdec[1];
            x *= v->env;
            break;
        }
        }
        /* attacco (curva morbida), decadimento, spegnimento */
        if (v->att < 1.0f) {
            v->att += v->att_inc;
            if (v->att > 1.0f) v->att = 1.0f;
            float a = v->att;
            x *= a * a * (3.0f - 2.0f * a);
        }
        if (v->timbre != TIM_SOFFIO || v->t > v->hold) v->env *= v->dec;
        if (v->killing) v->kill *= kdec;
        x *= v->kill;
        ml[i] += x * v->gl;
        mr[i] += x * v->gr;
        v->t += inv;
    }
    if ((v->att >= 1.0f && v->env < 1e-4f) || v->kill < 1e-3f) v->active = 0;
}

/* ------------------------------------------------------------------ coda */
static void start_or_wait(Fio *f, long long when, const FioNote *n)
{
    if (when <= f->clock || f->npend >= PENDING) { voice_start(f, n, 0); return; }
    f->pend[f->npend].when = when;
    f->pend[f->npend].n = *n;
    f->npend++;
}

static void drain(Fio *f)
{
    int t = f->q_tail, h = get_i(&f->q_head);
    while (t != h) {
        const Event *e = &f->q[t];
        switch (e->type) {
        case EV_NOTE: start_or_wait(f, e->when, &e->n); break;
        case EV_MIX:
            f->mix_t = e->m;
            if (!f->have_mix) { f->mix = e->m; f->dr_note = e->m.drone_note; f->have_mix = 1; }
            break;
        case EV_SILENCE:
            f->npend = 0;
            for (int i = 0; i < FIO_VOICES; i++) if (f->v[i].active) f->v[i].killing = 1;
            break;
        }
        t = (t + 1) % QUEUE;
    }
    set_i(&f->q_tail, t);
}

static int push(Fio *f, const Event *e)
{
    int h = __atomic_load_n(&f->q_head, __ATOMIC_RELAXED), nx = (h + 1) % QUEUE;
    if (nx == get_i(&f->q_tail)) return -1;
    f->q[h] = *e;
    set_i(&f->q_head, nx);
    return 0;
}

/* ------------------------------------------------------------------ creazione */
Fio *fio_create(float sample_rate)
{
    if (SINE[SINE_N / 4] == 0.0f)
        for (int i = 0; i <= SINE_N; i++) SINE[i] = sinf(2.0f * PI_F * (float)i / SINE_N);
    Fio *f = calloc(1, sizeof(Fio));
    if (!f) return NULL;
    f->sr = sample_rate > 8000.0f ? sample_rate : 48000.0f;
    f->inv_sr = 1.0f / f->sr;
    f->rng = 0x9E3779B9u;
    f->ks_mem = calloc((size_t)FIO_VOICES * KS_MAX, sizeof(float));
    f->dlen = (int)(f->sr * 1.3f);
    f->dl = calloc((size_t)f->dlen, sizeof(float));
    f->dr = calloc((size_t)f->dlen, sizeof(float));
    f->sh = calloc(SH_LEN, sizeof(float));
    f->tap = calloc((size_t)TAP_FRAMES * 2, sizeof(float));
    float r = f->sr / 44100.0f;
    static const int combs[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 }, aps[4] = { 556, 441, 341, 225 };
    for (int i = 0; i < 8; i++) { comb_init(&f->cl[i], (int)(combs[i] * r)); comb_init(&f->cr[i], (int)((combs[i] + 23) * r)); }
    for (int i = 0; i < 4; i++) { allpass_init(&f->al[i], (int)(aps[i] * r)); allpass_init(&f->ar[i], (int)((aps[i] + 23) * r)); }
    int ok = f->ks_mem && f->dl && f->dr && f->sh && f->tap;
    for (int i = 0; i < 8; i++) ok = ok && f->cl[i].buf && f->cr[i].buf;
    for (int i = 0; i < 4; i++) ok = ok && f->al[i].buf && f->ar[i].buf;
    for (int i = 0; i < FIO_VOICES; i++) f->v[i].ks = f->ks_mem ? f->ks_mem + (size_t)i * KS_MAX : NULL;
    f->mix.volume = f->mix_t.volume = 0.8f;
    f->mix.delay_time = f->mix_t.delay_time = 0.5f;
    f->mix.drone_note = f->mix_t.drone_note = f->dr_note = 38.0f;
    f->dts = 0.5f * f->sr;
    if (!ok) { fio_destroy(f); return NULL; }
    return f;
}

void fio_destroy(Fio *f)
{
    if (!f) return;
    free(f->ks_mem); free(f->dl); free(f->dr); free(f->sh); free(f->tap);
    for (int i = 0; i < 8; i++) { free(f->cl[i].buf); free(f->cr[i].buf); }
    for (int i = 0; i < 4; i++) { free(f->al[i].buf); free(f->ar[i].buf); }
    free(f);
}

/* ------------------------------------------------------------------ interfaccia */
void fio_set_mix(Fio *f, const FioMix *m)
{
    Event e;
    memset(&e, 0, sizeof(e));
    e.type = EV_MIX;
    e.m = *m;
    push(f, &e);
}

int fio_play(Fio *f, long long when, const FioNote *n)
{
    Event e;
    memset(&e, 0, sizeof(e));
    e.type = EV_NOTE;
    e.when = when;
    e.n = *n;
    return push(f, &e);
}

void fio_silence(Fio *f)
{
    Event e;
    memset(&e, 0, sizeof(e));
    e.type = EV_SILENCE;
    push(f, &e);
}

long long fio_clock(const Fio *f) { return get_ll(&f->clock); }
float fio_level(const Fio *f) { return get_f(&f->level_pub); }
int fio_active_voices(const Fio *f) { return get_i(&f->voices_pub); }
int fio_voice_starts(const Fio *f) { return get_i(&f->starts); }
long long fio_last_start(const Fio *f) { return get_ll(&f->last_start); }

void fio_tap(Fio *f, int on)
{
    if (on) { set_i(&f->tap_r, get_i(&f->tap_w)); set_i(&f->tap_lost, 0); }
    set_i(&f->tap_on, on ? 1 : 0);
}

int fio_tap_read(Fio *f, float *dst, int max_frames)
{
    int r = f->tap_r, w = get_i(&f->tap_w), n = 0;
    while (r != w && n < max_frames) {
        dst[2 * n] = f->tap[2 * r];
        dst[2 * n + 1] = f->tap[2 * r + 1];
        r = (r + 1) % TAP_FRAMES;
        n++;
    }
    set_i(&f->tap_r, r);
    return n;
}

int fio_tap_lost(const Fio *f) { return get_i(&f->tap_lost); }

/* ------------------------------------------------------------------ resa */
void fio_render(Fio *f, float *out, int frames)
{
    f->cur = f->clock;
    drain(f);
    const float sr = f->sr, inv = f->inv_sr;
    const float ksm = 1.0f - expf(-(float)SUB / (0.06f * sr));       /* levigatura dei parametri */
    const float glide = 1.0f - expf(-1.0f / (0.9f * sr));            /* bordone: scivola sulla nuova tonica */
    const float famp = 1.0f - expf(-1.0f / (0.6f * sr));
    int tap_on = get_i(&f->tap_on);

    for (int off = 0; off < frames; off += SUB) {
        int n = frames - off < SUB ? frames - off : SUB;
        long long now = f->clock + off;
        f->cur = now;

        /* le note che arrivano al loro istante in questo passo */
        for (int k = 0; k < f->npend;) {
            if (f->pend[k].when < now + n) {
                voice_start(f, &f->pend[k].n, (int)(f->pend[k].when - now));
                f->pend[k] = f->pend[--f->npend];
            } else {
                k++;
            }
        }

        FioMix *m = &f->mix;
        const FioMix *t = &f->mix_t;
        m->reverb += (t->reverb - m->reverb) * ksm;
        m->shimmer += (t->shimmer - m->shimmer) * ksm;
        m->delay += (t->delay - m->delay) * ksm;
        m->delay_time += (clampf(t->delay_time, 0.1f, 1.2f) - m->delay_time) * ksm * 0.25f;
        m->drone = t->drone;
        m->volume += (t->volume - m->volume) * ksm;
        float reverb = clamp01(m->reverb), shimmer = clamp01(m->shimmer), delay = clamp01(m->delay);
        float room = 0.84f + 0.13f * reverb;
        float wet = reverb * 2.4f;
        /* shimmer: il giro (comb -> ottava sopra -> comb) resta sotto 0,7 qualunque sia la stanza */
        float sh_k = shimmer * 0.75f * (1.0f - room) / 8.0f;
        float dts_t = clampf(m->delay_time, 0.1f, 1.2f) * sr;
        float vol = clamp01(m->volume);

        memset(f->mix_l, 0, sizeof(float) * (size_t)n);
        memset(f->mix_r, 0, sizeof(float) * (size_t)n);
        int voices = 0;
        for (int i = 0; i < FIO_VOICES; i++)
            if (f->v[i].active) { voice_render(f, &f->v[i], n); voices++; }

        for (int i = 0; i < n; i++) {
            float vl = f->mix_l[i], vr = f->mix_r[i];

            /* bordone: tonica e quinta, tre seghe per nota, filtro che si apre e si chiude piano */
            float drl = 0.0f, drr = 0.0f;
            float dtarget = clamp01(m->drone) * 0.14f;
            f->dr_amp += (dtarget - f->dr_amp) * famp;
            if (f->dr_amp > 1e-5f) {
                f->dr_note += (t->drone_note - f->dr_note) * glide;
                float fr = midi_hz(f->dr_note), ff = midi_hz(f->dr_note + 7.0f);
                static const float DET[3] = { 0.9965f, 1.0035f, 1.0f };
                float s[6];
                for (int k = 0; k < 6; k++) {
                    float hz = (k < 3 ? fr : ff) * DET[k % 3], dt = hz * inv;
                    float p = f->dr_ph[k];
                    s[k] = 2.0f * p - 1.0f - polyblep(p, dt);
                    f->dr_ph[k] = wrap1(p + dt);
                }
                f->dr_lfo = wrap1(f->dr_lfo + 0.055f * inv);
                float breath = 0.5f + 0.5f * sin_t(f->dr_lfo);
                float fc = fr * (1.4f + 3.2f * breath);
                float a = 1.0f - expf(-2.0f * PI_F * fc * inv);
                float in[2] = { s[0] + 0.7f * s[3] + 0.5f * s[2], s[1] + 0.7f * s[4] + 0.5f * s[5] };
                for (int c = 0; c < 2; c++) {
                    f->dr_lp[c][0] += a * (in[c] - f->dr_lp[c][0]);
                    f->dr_lp[c][1] += a * (f->dr_lp[c][0] - f->dr_lp[c][1]);
                }
                drl = f->dr_lp[0][1] * f->dr_amp;
                drr = f->dr_lp[1][1] * f->dr_amp;
            }

            /* eco ping-pong, piu' scura a ogni giro */
            f->dts += (dts_t - f->dts) * 0.0005f;
            float rl = read_frac(f->dl, f->dlen, f->dpos, f->dts), rr = read_frac(f->dr, f->dlen, f->dpos, f->dts);
            f->dlp_l += 0.32f * (rl - f->dlp_l);
            f->dlp_r += 0.32f * (rr - f->dlp_r);
            f->dl[f->dpos] = 0.5f * (vl + vr) + f->dlp_r * 0.46f;
            f->dr[f->dpos] = f->dlp_l * 0.46f;
            if (++f->dpos >= f->dlen) f->dpos = 0;
            float el = rl * delay * 0.85f, er = rr * delay * 0.85f;

            float dry_l = vl + drl, dry_r = vr + drr;
            float out_l = dry_l + el, out_r = dry_r + er;

            /* riverbero (Freeverb: 8 comb e 4 allpass per lato) con shimmer */
            float in = (dry_l + dry_r + el + er + (drl + drr) * 0.5f) * 0.5f * 0.015f + f->sh_fb;
            float wl = 0.0f, wr = 0.0f;
            for (int c = 0; c < 8; c++) { wl += comb_run(&f->cl[c], in, room, 0.26f); wr += comb_run(&f->cr[c], in, room, 0.26f); }
            for (int a = 0; a < 4; a++) { wl = allpass_run(&f->al[a], wl); wr = allpass_run(&f->ar[a], wr); }
            out_l += wl * wet;
            out_r += wr * wet;
            /* shimmer: la coda letta al doppio della velocita' (un'ottava sopra) con due testine in
               dissolvenza, rimandata nel riverbero */
            f->sh[f->sh_pos] = 0.5f * (wl + wr);
            if (shimmer > 0.001f) {
                float sum = 0.0f;
                for (int h = 0; h < 2; h++) {
                    int age = (f->sh_pos + h * SH_WIN / 2) % SH_WIN;
                    float d = (float)(SH_WIN - age);
                    float w = sin_t(0.5f * (float)age / (float)SH_WIN);
                    sum += w * w * read_frac(f->sh, SH_LEN, f->sh_pos, d);
                }
                f->sh_fb = softclip(sum * sh_k * 8.0f) / 8.0f;
                out_l += sum * shimmer * wet * 0.9f;
                out_r += sum * shimmer * wet * 0.9f;
            } else {
                f->sh_fb = 0.0f;
            }
            if (++f->sh_pos >= SH_LEN) f->sh_pos = 0;

            out_l = softclip(out_l * vol);
            out_r = softclip(out_r * vol);
            out[2 * (off + i)] = out_l;
            out[2 * (off + i) + 1] = out_r;
            float pk = 0.5f * (fabsf(out_l) + fabsf(out_r));
            f->level += (pk - f->level) * (pk > f->level ? 0.01f : 0.0002f);

            if (tap_on) {
                int w = f->tap_w, nx = (w + 1) % TAP_FRAMES;
                if (nx == get_i(&f->tap_r)) {
                    f->tap_lost++;
                } else {
                    f->tap[2 * w] = out_l;
                    f->tap[2 * w + 1] = out_r;
                    set_i(&f->tap_w, nx);
                }
            }
        }
        set_i(&f->voices_pub, voices);
    }
    set_f(&f->level_pub, f->level);
    set_ll(&f->clock, f->clock + frames);
    set_i(&f->starts, f->starts);
    set_i(&f->tap_lost, f->tap_lost);
}
