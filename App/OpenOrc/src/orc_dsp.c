/* OpenOrc per Miyoo Flip - motore audio (vedi orc_dsp.h). */
#include "orc_dsp.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PI_F       3.14159265358979f
#define QUEUE_SIZE 512
#define SUB        32                  /* campioni per passo di controllo: 0,67 ms a 48 kHz */
#define SINE_N     2048

enum { EV_CHORD_ON, EV_CHORD_OFF, EV_NOTE_ON, EV_NOTE_OFF, EV_ALL_OFF, EV_TEMPO, EV_TRANSPORT, EV_BEAT,
       EV_LOOP_REC, EV_LOOP_PLAY, EV_LOOP_CLEAR, EV_LOOP_CONFIG };

/* a: nota del basso (accordi), nota (melodia) o valore intero; b: dinamica o valore reale. */
typedef struct { int type, n, a, tag; float b; int notes[ORC_MAX_CHORD]; } Event;

typedef struct {
    float t;                 /* battiti dall'inizio del giro (letto anche dall'interfaccia) */
    int kind;                /* per il disegno: 0 accordo, 1 nota, 2 spegnimento */
    double armed_from;       /* non suona prima di questo battito: niente eco di quello appena suonato */
    Event e;
} LoopEv;

typedef struct { OrcSound snd; float bend, mod, bright; } Control;

enum { ST_ATTACK, ST_DECAY, ST_SUSTAIN, ST_RELEASE };

typedef struct {
    int active, stage, note, layer, chord, released, delay;
    float vel, freq, level, ienv, rel_mul;
    float pc, pm, ph2, ic1, ic2;
    uint32_t age;
} Voice;

typedef struct {
    int notes[ORC_MAX_CHORD], n, held, bass, tag, strum_down, step;
    float vel;
    double free_count;       /* campioni al prossimo passo, a trasporto fermo */
    long grid;               /* ultimo passo della griglia, a trasporto in moto */
} Performer;

typedef struct { float *buf; int len, pos; float store; } Comb;
typedef struct { float *buf; int len, pos; } Allpass;
typedef struct { int active; float t, vel, phase, lp, ic1, ic2; } Drum;
enum { DR_KICK = DRUM_KICK, DR_SNARE = DRUM_SNARE, DR_HAT = DRUM_HAT, DR_OPEN = DRUM_OPEN, DR_CLAP = DRUM_CLAP, DR_COUNT = DRUM_PARTS };

static float SINE[SINE_N + 1];

struct Orc {
    float sr, inv_sr;

    Event q[QUEUE_SIZE];
    int q_head, q_tail;

    Control ctl_stage;                  /* dell'interfaccia */
    Control ctl_buf[3];                 /* triplo buffer */
    int ctl_back, ctl_middle, ctl_front;
    Control ctl;                        /* copia del thread audio */

    Voice v[ORC_VOICES];
    uint32_t age_counter, rng;
    Performer pf[ORC_LAYERS];
    int pub_n[ORC_LAYERS], pub_tag[ORC_LAYERS], pub_notes[ORC_LAYERS][ORC_MAX_CHORD];
    int voices_pub, starts, starts_pub;

    int bass_active, bass_released, bass_owner;
    float bass_freq, bass_target, bass_level, bass_ph, bass_lp, bass_lp2;
    float lfo;

    float bpm;
    int transport, beat_on, beat_style, beat_step, beat_step_pub;
    float beat_level;
    double beats;
    Drum dr[DR_COUNT];
    float clap_a1, clap_a2, clap_a3;

    LoopEv loop[ORC_LOOP_MAX];
    int loop_n, loop_state, loop_bars, loop_full;
    float loop_q;
    double loop_origin, loop_len, play_from;
    int rec_chord_open, rec_note_open[128];
    double rec_chord_t, rec_note_t[128];

    float *ch_l, *ch_r; int ch_len, ch_pos; float ch_phase;
    float *dl, *dr_; int dlen, dpos; float dlp_l, dlp_r;
    Comb comb_l[4], comb_r[4];
    Allpass ap_l[2], ap_r[2];

    float mix[SUB];
    float scope[ORC_SCOPE];
    int scope_pos;
};

/* ------------------------------------------------------------------ utilita' */
static inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static inline float clamp01(float x) { return clampf(x, 0.0f, 1.0f); }
static inline float midi_hz(float n) { return 440.0f * exp2f((n - 69.0f) / 12.0f); }
static inline float white(Orc *o)
{
    o->rng ^= o->rng << 13; o->rng ^= o->rng >> 17; o->rng ^= o->rng << 5;
    return (float)(int32_t)o->rng * (1.0f / 2147483648.0f);
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
    float f = x - (float)i;
    i &= SINE_N - 1;
    return SINE[i] + (SINE[i + 1] - SINE[i]) * f;
}

/* Campi letti anche dall'interfaccia: il thread audio li scrive in modo atomico. */
static inline void set_i(int *p, int v) { __atomic_store_n(p, v, __ATOMIC_RELEASE); }
static inline int get_i(const int *p) { return __atomic_load_n(p, __ATOMIC_ACQUIRE); }
static inline void set_d(double *p, double v) { __atomic_store(p, &v, __ATOMIC_RELEASE); }
static inline double get_d(const double *p) { double v; __atomic_load(p, &v, __ATOMIC_ACQUIRE); return v; }
static inline void set_f(float *p, float v) { __atomic_store(p, &v, __ATOMIC_RELAXED); }
static inline float get_f(const float *p) { float v; __atomic_load(p, &v, __ATOMIC_RELAXED); return v; }

/* ------------------------------------------------------------------ voci */
static Voice *voice_alloc(Orc *o)
{
    Voice *best = NULL;
    for (int i = 0; i < ORC_VOICES; i++) if (!o->v[i].active) return &o->v[i];
    for (int i = 0; i < ORC_VOICES; i++)          /* la piu' vecchia tra quelle rilasciate */
        if (o->v[i].released && (!best || o->v[i].age < best->age)) best = &o->v[i];
    if (best) return best;
    best = &o->v[0];
    for (int i = 1; i < ORC_VOICES; i++) if (o->v[i].age < best->age) best = &o->v[i];
    return best;
}

static void voice_start(Orc *o, int layer, int note, float vel, int chord, int delay, float rel_mul)
{
    if (note < 0 || note > 127) return;
    Voice *v = voice_alloc(o);
    memset(v, 0, sizeof(*v));
    v->active = 1;
    v->stage = ST_ATTACK;
    v->note = note;
    v->layer = layer;
    v->chord = chord;
    v->vel = clampf(vel, 0.05f, 1.0f);
    v->freq = midi_hz((float)note);
    v->ienv = 1.0f;
    v->delay = delay;
    v->rel_mul = rel_mul;
    v->pc = (white(o) + 1.0f) * 0.5f;
    v->ph2 = (white(o) + 1.0f) * 0.5f;
    v->age = ++o->age_counter;
    o->starts++;
}

static void voice_release(Voice *v)
{
    if (!v->active || v->released) return;
    v->released = 1;
    if (v->delay > 0) v->active = 0;              /* non ancora partita: non parte piu' */
    else v->stage = ST_RELEASE;
}

static void release_chord_voices(Orc *o, int layer)
{
    for (int i = 0; i < ORC_VOICES; i++)
        if (o->v[i].chord && o->v[i].layer == layer) voice_release(&o->v[i]);
}

static void publish_layer(Orc *o, int L)
{
    Performer *p = &o->pf[L];
    int n = p->held ? p->n : 0;
    for (int i = 0; i < n; i++) set_i(&o->pub_notes[L][i], p->notes[i]);
    set_i(&o->pub_tag[L], p->tag);
    set_i(&o->pub_n[L], n);
}

static void layer_off(Orc *o, int L)
{
    for (int i = 0; i < ORC_VOICES; i++) if (o->v[i].layer == L) voice_release(&o->v[i]);
    o->pf[L].held = 0;
    if (o->bass_owner == L) o->bass_released = 1;
    publish_layer(o, L);
}

static void bass_start(Orc *o, int note, int L)
{
    float f = midi_hz((float)note);
    if (!o->bass_active) { o->bass_freq = f; o->bass_level = 0.0f; }
    o->bass_target = f;
    o->bass_active = 1;
    o->bass_released = 0;
    o->bass_owner = L;
}

/* ------------------------------------------------------------------ esecuzione degli accordi */
static const int ARP_DIV[5] = { 1, 2, 3, 4, 8 };    /* semiminime, crome, terzine, semicrome, biscrome */
static const char *PATTERNS[4] = { "x..x..x...x..x..", "x...x.x...x.x...", "x.x..x.x..x.x..x", "x..x...x..x...x." };

static int perf_rate(const OrcSound *s)
{
    return s->perform == PERF_ARP ? ARP_DIV[(int)(clamp01(s->perform_amount) * 4.99f)] : 4;
}
static double step_samples(const Orc *o, int per_beat) { return o->sr * 60.0 / o->bpm / per_beat; }

/* arpeggio su due ottave, su e giu' */
static void arp_step(Orc *o, int L)
{
    Performer *p = &o->pf[L];
    int total = p->n * 2, period = total > 1 ? 2 * total - 2 : 1;
    int k = p->step % period, pos = k < total ? k : period - k;
    p->step++;
    release_chord_voices(o, L);
    voice_start(o, L, p->notes[pos % p->n] + 12 * (pos / p->n), p->vel * (pos == 0 ? 1.0f : 0.85f), 1, 0, 0.6f);
}

static void pattern_step(Orc *o, int L, int st)
{
    Performer *p = &o->pf[L];
    const char *pat = PATTERNS[(int)(clamp01(o->ctl.snd.perform_amount) * 3.99f)];
    st &= 15;
    if (pat[st] == 'x') {
        release_chord_voices(o, L);
        for (int i = 0; i < p->n; i++) voice_start(o, L, p->notes[i], p->vel * (st % 4 == 0 ? 1.0f : 0.8f), 1, 0, 0.5f);
    } else if (st & 1) {
        release_chord_voices(o, L);               /* stacco: il pattern respira */
    }
}

static void perform_start(Orc *o, int L)
{
    Performer *p = &o->pf[L];
    const OrcSound *s = &o->ctl.snd;
    float amt = clamp01(s->perform_amount);
    int n = p->n, rate = perf_rate(s);
    p->step = 0;
    p->free_count = step_samples(o, rate);
    p->grid = (long)floor(o->beats * rate);
    switch (s->perform) {
    case PERF_STRUM: {
        int gap = (int)((0.008f + amt * 0.09f) * o->sr);
        for (int i = 0; i < n; i++) {
            int k = p->strum_down ? n - 1 - i : i;
            voice_start(o, L, p->notes[k], p->vel * (1.0f - 0.04f * i), 1, gap * i, 1.0f);
        }
        p->strum_down = !p->strum_down;
        break;
    }
    case PERF_SLOP:
        for (int i = 0; i < n; i++) {
            int d = (int)((white(o) + 1.0f) * 0.5f * amt * 0.07f * o->sr);
            voice_start(o, L, p->notes[i], p->vel * (0.82f + 0.09f * (white(o) + 1.0f)), 1, d, 1.0f);
        }
        break;
    case PERF_HARP: {
        int gap = (int)((0.025f + amt * 0.09f) * o->sr), k = 0, octs = n < 4 ? 3 : 2;
        for (int oc = 0; oc < octs; oc++)
            for (int i = 0; i < n; i++, k++) voice_start(o, L, p->notes[i] + 12 * oc, p->vel * (0.95f - 0.02f * k), 1, gap * k, 2.5f);
        break;
    }
    case PERF_ARP:
        if (n) arp_step(o, L);
        break;
    case PERF_PATTERN:                            /* la prima botta subito, poi il pattern */
        for (int i = 0; i < n; i++) voice_start(o, L, p->notes[i], p->vel, 1, 0, 0.5f);
        p->step = 1;
        break;
    default:
        for (int i = 0; i < n; i++) voice_start(o, L, p->notes[i], p->vel, 1, 0, 1.0f);
    }
}

static void perform_tick(Orc *o, int L, int frames)
{
    Performer *p = &o->pf[L];
    const OrcSound *s = &o->ctl.snd;
    if (!p->held || !p->n || (s->perform != PERF_ARP && s->perform != PERF_PATTERN)) return;
    int rate = perf_rate(s);
    if (o->transport) {                           /* a tempo con batteria e looper */
        long g = (long)floor(o->beats * rate);
        if (g == p->grid) return;
        p->grid = g;
        if (s->perform == PERF_ARP) arp_step(o, L);
        else pattern_step(o, L, (int)(g & 15));
    } else {
        p->free_count -= frames;
        if (p->free_count > 0.0) return;
        p->free_count += step_samples(o, rate);
        if (p->free_count <= 0.0) p->free_count = step_samples(o, rate);
        if (s->perform == PERF_ARP) arp_step(o, L);
        else pattern_step(o, L, p->step++);
    }
}

/* ------------------------------------------------------------------ batteria */
static const char *BEATS[BEAT_COUNT][DR_COUNT] = {
    /* cassa              rullante            chiuso              aperto              battimani */
    { "x...x...x...x...", "....x.......x...", "xx.xxx.xxx.xxx.x", "..x...x...x...x.", "....x.......x..." },   /* disco */
    { "x......x..x.....", "....x.......x...", "x.x.x.x.x.x.x.x.", "..............x.", "................" },   /* psichedelico */
    { "x......x..x.....", "........x.......", "x.x.x.x.x.xxx.x.", "................", "........x......." },   /* trap */
    { "x..xx...x..xx...", "x..x..x...x..x..", "x.x.x.x.x.x.x.x.", "................", "................" },   /* bossa */
};
static const float BEAT_SWING[BEAT_COUNT] = { 0.0f, 0.3f, 0.0f, 0.12f };   /* ritardo dei sedicesimi pari */

static void drum_hit(Orc *o, int k, float vel)
{
    Drum *d = &o->dr[k];
    memset(d, 0, sizeof(*d));
    d->active = 1;
    d->vel = vel;
    if (k == DR_HAT) o->dr[DR_OPEN].active = 0;   /* il chiuso ferma l'aperto */
}

static float drum_sample(Orc *o, int k)
{
    Drum *d = &o->dr[k];
    if (!d->active) return 0.0f;
    float t = d->t, out = 0.0f;
    d->t += o->inv_sr;
    int trap = o->beat_style == BEAT_TRAP;
    switch (k) {
    case DR_KICK: {
        float f = trap ? 42.0f + 90.0f * expf(-t / 0.06f) : 48.0f + 120.0f * expf(-t / 0.028f);
        d->phase += f * o->inv_sr;
        if (d->phase >= 1.0f) d->phase -= 1.0f;
        out = sin_t(d->phase) * expf(-t / (trap ? 0.9f : 0.3f)) * 1.1f;
        if (trap) out = softclip(out * 1.6f);
        if (t > (trap ? 3.0f : 1.6f)) d->active = 0;
        break;
    }
    case DR_SNARE: {
        d->phase += 185.0f * o->inv_sr;
        if (d->phase >= 1.0f) d->phase -= 1.0f;
        float n = white(o);
        d->lp += 0.25f * (n - d->lp);
        if (o->beat_style == BEAT_BOSSA)          /* colpo di cerchio */
            out = sin_t(d->phase * 9.0f) * expf(-t / 0.012f) * 0.6f + (n - d->lp) * expf(-t / 0.01f) * 0.4f;
        else
            out = sin_t(d->phase) * expf(-t / 0.06f) * 0.5f + (n - d->lp) * expf(-t / 0.15f) * 0.7f;
        if (t > 0.8f) d->active = 0;
        break;
    }
    case DR_HAT:
    case DR_OPEN: {
        float n = white(o);
        d->lp += 0.6f * (n - d->lp);
        out = (n - d->lp) * (k == DR_HAT ? expf(-t / 0.03f) * 0.45f : expf(-t / 0.28f) * 0.4f);
        if (t > (k == DR_HAT ? 0.3f : 1.5f)) d->active = 0;
        break;
    }
    case DR_CLAP: {
        float x = white(o);
        float v3 = x - d->ic2, v1 = o->clap_a1 * d->ic1 + o->clap_a2 * v3, v2 = d->ic2 + o->clap_a2 * d->ic1 + o->clap_a3 * v3;
        d->ic1 = 2.0f * v1 - d->ic1;
        d->ic2 = 2.0f * v2 - d->ic2;
        float e = 0.0f;
        for (int i = 0; i < 3; i++) { float tb = t - i * 0.011f; if (tb >= 0.0f && tb < 0.011f) e = fmaxf(e, expf(-tb / 0.004f)); }
        if (t > 0.022f) e = fmaxf(e, expf(-(t - 0.022f) / 0.13f) * 0.8f);
        out = v1 * e * 1.5f;
        if (t > 0.9f) d->active = 0;
        break;
    }
    }
    return out * d->vel;
}

static void drum_tick(Orc *o)
{
    double q = o->beats * 4.0;
    int step = (int)floor(q);
    if ((step & 1) && q - step < BEAT_SWING[o->beat_style]) step--;
    step &= 15;
    if (step == o->beat_step) return;
    o->beat_step = step;
    set_i(&o->beat_step_pub, o->beat_on ? step : -1);
    if (o->beat_on) {
        float lvl = clamp01(o->beat_level);
        for (int k = 0; k < DR_COUNT; k++)
            if (BEATS[o->beat_style][k][step] == 'x') drum_hit(o, k, lvl * (step % 4 == 0 ? 1.0f : 0.75f));
    } else if ((o->loop_state == LOOP_ARMED || o->loop_state == LOOP_RECORDING) && step % 4 == 0) {
        drum_hit(o, DR_HAT, step == 0 ? 0.9f : 0.45f);          /* metronomo mentre si registra */
    }
}

/* ------------------------------------------------------------------ looper */
static double loop_wrap(const Orc *o, double t)
{
    t = fmod(t, o->loop_len);
    return t < 0.0 ? t + o->loop_len : t;
}
static double loop_quant(const Orc *o, double t) { return o->loop_q > 0.0f ? floor(t / o->loop_q + 0.5) * o->loop_q : t; }
static double loop_gap(const Orc *o) { return o->loop_q > 0.0f ? o->loop_q : 0.0625; }

static void loop_add(Orc *o, double t, const Event *e, double armed_from)
{
    if (o->loop_n >= ORC_LOOP_MAX) { set_i(&o->loop_full, 1); return; }
    LoopEv *le = &o->loop[o->loop_n];
    le->e = *e;
    le->armed_from = armed_from;
    set_f(&le->t, (float)loop_wrap(o, t));
    set_i(&le->kind, e->type == EV_CHORD_ON ? 0 : (e->type == EV_NOTE_ON ? 1 : 2));
    set_i(&o->loop_n, o->loop_n + 1);
}

static void loop_forget_open(Orc *o)
{
    o->rec_chord_open = 0;
    memset(o->rec_note_open, 0, sizeof(o->rec_note_open));
}

/* Gli eventi dal vivo finiscono nel giro mentre si registra o si sovraincide. */
static void loop_capture(Orc *o, const Event *e)
{
    int st = o->loop_state;
    double b = o->beats, t;
    if (st == LOOP_ARMED) {
        if (b < o->loop_origin - 0.25) return;    /* un attimo prima della battuta conta come l'inizio */
        t = 0.0;
    } else if (st == LOOP_RECORDING || st == LOOP_OVERDUB) {
        t = loop_quant(o, b - o->loop_origin);
    } else {
        return;
    }
    double gap = loop_gap(o);
    switch (e->type) {
    case EV_CHORD_ON:
        o->rec_chord_open = 1;
        o->rec_chord_t = t;
        break;
    case EV_CHORD_OFF:
        if (!o->rec_chord_open) return;           /* accordo partito prima della registrazione */
        if (t < o->rec_chord_t + gap) t = o->rec_chord_t + gap;
        o->rec_chord_open = 0;
        break;
    case EV_NOTE_ON:
        o->rec_note_open[e->a & 127] = 1;
        o->rec_note_t[e->a & 127] = t;
        break;
    case EV_NOTE_OFF: {
        int k = e->a & 127;
        if (!o->rec_note_open[k]) return;
        if (t < o->rec_note_t[k] + gap) t = o->rec_note_t[k] + gap;
        o->rec_note_open[k] = 0;
        break;
    }
    default:
        return;
    }
    loop_add(o, t, e, b + 0.5);
}

/* All'inizio della registrazione, quello che si sta gia' tenendo entra nel giro da subito. */
static void loop_begin(Orc *o)
{
    Performer *p = &o->pf[ORC_LIVE];
    if (p->held && !o->rec_chord_open) {
        Event e = { EV_CHORD_ON, p->n, p->bass, p->tag, p->vel, { 0 } };
        memcpy(e.notes, p->notes, sizeof(int) * (size_t)p->n);
        o->rec_chord_open = 1;
        o->rec_chord_t = 0.0;
        loop_add(o, 0.0, &e, 0.0);
    }
    for (int i = 0; i < ORC_VOICES; i++) {
        Voice *v = &o->v[i];
        if (!v->active || v->chord || v->released || v->layer != ORC_LIVE || o->rec_note_open[v->note]) continue;
        Event e = { EV_NOTE_ON, 0, v->note, 0, v->vel, { 0 } };
        o->rec_note_open[v->note] = 1;
        o->rec_note_t[v->note] = 0.0;
        loop_add(o, 0.0, &e, 0.0);
    }
}

/* Chiude accordo e note rimasti aperti al tempo t (non avvolto). Con skip_late, quelli partiti dopo t
   restano aperti: sono stati suonati in anticipo per l'inizio del giro successivo. */
static void loop_close(Orc *o, double t, int skip_late)
{
    double gap = loop_gap(o);
    if (o->rec_chord_open && !(skip_late && t <= o->rec_chord_t)) {
        Event e = { EV_CHORD_OFF, 0, 0, 0, 0.0f, { 0 } };
        loop_add(o, fmax(t, o->rec_chord_t + gap), &e, 0.0);
    }
    for (int k = 0; k < 128; k++) {
        if (!o->rec_note_open[k] || (skip_late && t <= o->rec_note_t[k])) continue;
        Event e = { EV_NOTE_OFF, 0, k, 0, 0.0f, { 0 } };
        loop_add(o, fmax(t, o->rec_note_t[k] + gap), &e, 0.0);
    }
    loop_forget_open(o);
}

/* Fine della registrazione: a richiesta la lunghezza si arrotonda alla battuta piu' vicina. */
static void loop_finish(Orc *o, double b, int round_len)
{
    if (round_len) {
        double bars = floor((b - o->loop_origin) / 4.0 + 0.5);
        if (bars < 1.0) bars = 1.0;
        if (bars > o->loop_bars) bars = o->loop_bars;
        double len = bars * 4.0;
        if (len < o->loop_len) {
            set_d(&o->loop_len, len);
            for (int i = 0; i < o->loop_n; i++)
                if (o->loop[i].t >= len) set_f(&o->loop[i].t, (float)fmod(o->loop[i].t, len));
        }
    }
    loop_close(o, o->loop_len - 1.0 / 32.0, 1);
    o->play_from = b;
    set_i(&o->loop_state, LOOP_PLAYING);
}

static void loop_reset(Orc *o)
{
    set_i(&o->loop_n, 0);
    set_i(&o->loop_full, 0);
    set_d(&o->loop_len, o->loop_bars * 4.0);
    loop_forget_open(o);
}

static void transport_set(Orc *o, int on);

static void loop_rec(Orc *o)
{
    double b = o->beats;
    switch (o->loop_state) {
    case LOOP_EMPTY:
        loop_reset(o);
        if (!o->transport) {                      /* a trasporto fermo si parte subito */
            transport_set(o, 1);
            set_d(&o->loop_origin, 0.0);
            set_i(&o->loop_state, LOOP_RECORDING);
            loop_begin(o);
        } else {
            double bar = floor(b / 4.0) * 4.0;
            if (b - bar < 0.5) {                  /* appena dopo la battuta: si parte da li' */
                set_d(&o->loop_origin, bar);
                set_i(&o->loop_state, LOOP_RECORDING);
                loop_begin(o);
            } else {                              /* altrimenti si aspetta la prossima */
                set_d(&o->loop_origin, bar + 4.0);
                set_i(&o->loop_state, LOOP_ARMED);
            }
        }
        break;
    case LOOP_ARMED:
        set_i(&o->loop_state, LOOP_EMPTY);
        break;
    case LOOP_RECORDING:
        loop_finish(o, b, 1);
        break;
    case LOOP_PLAYING:
        loop_forget_open(o);
        set_i(&o->loop_state, LOOP_OVERDUB);
        break;
    case LOOP_OVERDUB:
        loop_close(o, loop_quant(o, b - o->loop_origin), 0);
        set_i(&o->loop_state, LOOP_PLAYING);
        break;
    case LOOP_STOPPED:
        if (!o->transport) transport_set(o, 1);
        o->play_from = o->beats;
        loop_forget_open(o);
        set_i(&o->loop_state, LOOP_OVERDUB);
        break;
    }
}

static void loop_play(Orc *o)
{
    switch (o->loop_state) {
    case LOOP_PLAYING:
        set_i(&o->loop_state, LOOP_STOPPED);
        layer_off(o, ORC_LOOP);
        break;
    case LOOP_OVERDUB:
        loop_close(o, loop_quant(o, o->beats - o->loop_origin), 0);
        set_i(&o->loop_state, LOOP_STOPPED);
        layer_off(o, ORC_LOOP);
        break;
    case LOOP_STOPPED:
        if (!o->transport) transport_set(o, 1);   /* riparte dall'inizio del giro */
        o->play_from = o->beats;
        set_i(&o->loop_state, LOOP_PLAYING);
        break;
    case LOOP_RECORDING:
        loop_finish(o, o->beats, 1);
        break;
    case LOOP_ARMED:
        set_i(&o->loop_state, LOOP_EMPTY);
        break;
    default:
        break;
    }
}

static void loop_clear(Orc *o)
{
    loop_reset(o);
    set_i(&o->loop_state, LOOP_EMPTY);
    layer_off(o, ORC_LOOP);
}

static void apply(Orc *o, const Event *e, int L);

/* Eventi del giro che cadono nel passo [b0, b1): prima gli spegnimenti, poi le accensioni. */
static void loop_tick(Orc *o, double b0, double b1)
{
    int st = o->loop_state;
    if (st == LOOP_ARMED && b1 > o->loop_origin) {
        st = LOOP_RECORDING;
        set_i(&o->loop_state, st);
        loop_begin(o);
    }
    if (st == LOOP_RECORDING && b1 > o->loop_origin + o->loop_len) {
        loop_finish(o, o->loop_origin + o->loop_len, 0);
        st = LOOP_PLAYING;
    }
    if ((st != LOOP_PLAYING && st != LOOP_OVERDUB) || !o->loop_n) return;
    double start = b0 > o->play_from ? b0 : o->play_from;
    if (start >= b1) return;
    double len = o->loop_len, base = o->loop_origin + floor((start - o->loop_origin) / len) * len;
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < o->loop_n; i++) {
            LoopEv *le = &o->loop[i];
            int off = le->e.type == EV_CHORD_OFF || le->e.type == EV_NOTE_OFF;
            if (off != (pass == 0)) continue;
            double c = base + le->t;
            if (c < start) c += len;
            if (c < b1 && c >= le->armed_from) apply(o, &le->e, ORC_LOOP);
        }
}

/* ------------------------------------------------------------------ trasporto ed eventi */
static void transport_set(Orc *o, int on)
{
    if (on && !o->transport) {
        set_d(&o->beats, 0.0);
        o->beat_step = -1;
        for (int L = 0; L < ORC_LAYERS; L++) o->pf[L].grid = -1;
        int st = o->loop_state;
        if (st == LOOP_PLAYING || st == LOOP_OVERDUB || st == LOOP_STOPPED) { set_d(&o->loop_origin, 0.0); o->play_from = 0.0; }
        set_i(&o->transport, 1);
    } else if (!on && o->transport) {
        int st = o->loop_state;
        if (st == LOOP_RECORDING) loop_finish(o, o->beats, 1);
        else if (st == LOOP_ARMED) set_i(&o->loop_state, LOOP_EMPTY);
        else if (st == LOOP_OVERDUB) { loop_close(o, loop_quant(o, o->beats - o->loop_origin), 0); set_i(&o->loop_state, LOOP_PLAYING); }
        layer_off(o, ORC_LOOP);
        set_i(&o->transport, 0);
        set_i(&o->beat_step_pub, -1);
    }
}

static void apply(Orc *o, const Event *e, int L)
{
    Performer *p = &o->pf[L];
    switch (e->type) {
    case EV_CHORD_ON:
        release_chord_voices(o, L);
        p->n = e->n < 0 ? 0 : (e->n > ORC_MAX_CHORD ? ORC_MAX_CHORD : e->n);
        memcpy(p->notes, e->notes, sizeof(int) * (size_t)p->n);
        p->vel = e->b;
        p->bass = e->a;
        p->tag = e->tag;
        p->held = 1;
        perform_start(o, L);
        if (o->ctl.snd.bass_level > 0.005f && e->a >= 0) bass_start(o, e->a, L);
        publish_layer(o, L);
        break;
    case EV_CHORD_OFF:
        if (!p->held) break;
        p->held = 0;
        release_chord_voices(o, L);
        if (o->bass_owner == L) o->bass_released = 1;
        publish_layer(o, L);
        break;
    case EV_NOTE_ON:
        voice_start(o, L, e->a, e->b, 0, 0, 1.0f);
        break;
    case EV_NOTE_OFF:
        for (int i = 0; i < ORC_VOICES; i++) {
            Voice *v = &o->v[i];
            if (v->active && !v->chord && v->layer == L && v->note == e->a) voice_release(v);
        }
        break;
    }
    if (L == ORC_LIVE) loop_capture(o, e);
}

static void handle(Orc *o, const Event *e)
{
    switch (e->type) {
    case EV_CHORD_ON:
    case EV_CHORD_OFF:
    case EV_NOTE_ON:
    case EV_NOTE_OFF:
        apply(o, e, ORC_LIVE);
        break;
    case EV_ALL_OFF:
        if (o->loop_state == LOOP_RECORDING || o->loop_state == LOOP_OVERDUB)
            loop_close(o, loop_quant(o, o->beats - o->loop_origin), 0);
        for (int L = 0; L < ORC_LAYERS; L++) layer_off(o, L);
        o->bass_released = 1;
        break;
    case EV_TEMPO:
        o->bpm = clampf(e->b, 30.0f, 300.0f);
        break;
    case EV_TRANSPORT:
        transport_set(o, e->a);
        break;
    case EV_BEAT:
        o->beat_on = e->a & 1;
        o->beat_style = (e->a >> 1) % BEAT_COUNT;
        o->beat_level = e->b;
        set_i(&o->beat_step_pub, (o->transport && o->beat_on) ? o->beat_step : -1);
        break;
    case EV_LOOP_REC:
        loop_rec(o);
        break;
    case EV_LOOP_PLAY:
        loop_play(o);
        break;
    case EV_LOOP_CLEAR:
        loop_clear(o);
        break;
    case EV_LOOP_CONFIG:
        o->loop_bars = e->a < 1 ? 1 : (e->a > 16 ? 16 : e->a);
        o->loop_q = e->b < 0.0f ? 0.0f : e->b;
        if (o->loop_state == LOOP_EMPTY) set_d(&o->loop_len, o->loop_bars * 4.0);
        break;
    }
}

/* ------------------------------------------------------------------ effetti */
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

/* ------------------------------------------------------------------ voci: un passo di n campioni */
static int render_voices(Orc *o, int n, float pitch)
{
    const OrcSound *s = &o->ctl.snd;
    const float sr = o->sr, inv = o->inv_sr;
    const int eng = s->engine >= 0 && s->engine < ENG_COUNT ? s->engine : ENG_VA;
    const float att_inc = 1.0f / (fmaxf(s->attack, 0.002f) * sr);
    const float sus = clamp01(s->sustain);
    const float dcoef = expf(-4.0f / (fmaxf(s->decay, 0.01f) * sr));
    const float rel = fmaxf(s->release, 0.01f);
    const float tone = clamp01(s->tone + o->ctl.bright * 0.5f);
    const float chr = clamp01(s->character);
    /* analogico: nella prima meta' del carattere cresce la desintonia, nella seconda arriva l'onda quadra */
    const float detune = exp2f(fminf(chr, 0.5f) * 2.0f * 22.0f / 1200.0f);
    const float sq = clampf((chr - 0.5f) * 2.0f, 0.0f, 1.0f);
    /* FM e piano elettrico */
    static const float RATIOS[7] = { 1.0f, 2.0f, 3.0f, 4.0f, 0.5f, 3.5f, 7.0f };
    const float ratio = eng == ENG_FM ? RATIOS[(int)(chr * 6.99f)] : 1.0f;
    const float icoef = expf(-1.0f / ((eng == ENG_FM ? 0.6f : 0.35f) * sr));
    const float ifloor = eng == ENG_FM ? 0.12f : 0.25f;
    const float drive = 1.0f + chr * 3.0f, bias = 0.12f * (drive - 1.0f), bias_out = softclip(bias);
    /* organo: registri */
    const float w2 = 0.55f + 0.25f * chr, w3 = 0.15f + 0.45f * chr, w4 = 0.5f * chr, w6 = 0.35f * chr * chr, w8 = 0.4f * tone;
    const float wnorm = 1.0f / (1.0f + w2 + w3 + w4 + w6 + w8);
    float *mix = o->mix;
    memset(mix, 0, sizeof(float) * (size_t)n);
    int count = 0;
    for (int vi = 0; vi < ORC_VOICES; vi++) {
        Voice *v = &o->v[vi];
        if (!v->active) continue;
        if (v->delay > 0) { v->delay -= n; if (v->delay > 0) continue; v->delay = 0; }
        count++;
        const float rc = expf(-4.0f / (rel * v->rel_mul * sr));
        float dt = v->freq * pitch * inv;
        if (dt > 0.45f) dt = 0.45f;
        const float dt2 = dt * detune;
        float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
        if (eng == ENG_VA) {
            float env = (v->stage == ST_ATTACK || v->stage == ST_DECAY) ? v->level : 0.0f;
            float oct = 6.6f + tone * 6.8f + env * 0.9f + (float)(v->note - 60) / 24.0f + v->vel * 0.5f;
            float fc = fminf(exp2f(oct), 0.42f * sr);
            float g = tanf(PI_F * fc * inv), k = 1.6f;
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }
        const float idx_base = eng == ENG_FM ? (0.4f + tone * 6.0f) * (0.6f + 0.4f * v->vel)
                                             : (0.3f + tone * 2.5f) * (0.4f + 0.6f * v->vel);
        const float amp = v->vel;
        for (int i = 0; i < n; i++) {
            switch (v->stage) {
            case ST_ATTACK:
                v->level += att_inc;
                if (v->level >= 1.0f) { v->level = 1.0f; v->stage = ST_DECAY; }
                break;
            case ST_DECAY:
                v->level = sus + (v->level - sus) * dcoef;
                if (v->level - sus < 1e-4f) { v->level = sus; v->stage = ST_SUSTAIN; }
                break;
            case ST_SUSTAIN:
                break;
            default:
                v->level *= rc;
                break;
            }
            if ((v->stage == ST_SUSTAIN && sus < 1e-4f) || (v->stage == ST_RELEASE && v->level < 1e-4f)) { v->active = 0; break; }
            float x;
            if (eng == ENG_VA) {
                v->pc += dt; if (v->pc >= 1.0f) v->pc -= 1.0f;
                v->ph2 += dt2; if (v->ph2 >= 1.0f) v->ph2 -= 1.0f;
                float saw1 = 2.0f * v->pc - 1.0f - polyblep(v->pc, dt);
                float saw2 = 2.0f * v->ph2 - 1.0f - polyblep(v->ph2, dt2);
                float raw = 0.5f * (saw1 + saw2);
                if (sq > 0.0f) {
                    float half = v->pc + 0.5f;
                    if (half >= 1.0f) half -= 1.0f;
                    float sqr = (v->pc < 0.5f ? 1.0f : -1.0f) + polyblep(v->pc, dt) - polyblep(half, dt);
                    raw = (1.0f - sq) * raw + sq * 0.5f * (sqr + saw2);
                }
                float v3 = raw - v->ic2, v1 = a1 * v->ic1 + a2 * v3, v2 = v->ic2 + a2 * v->ic1 + a3 * v3;
                v->ic1 = 2.0f * v1 - v->ic1;
                v->ic2 = 2.0f * v2 - v->ic2;
                x = v2;
            } else if (eng == ENG_ORGAN) {
                v->pc += dt; if (v->pc >= 1.0f) v->pc -= 1.0f;
                float p = v->pc;
                x = (sin_t(p) + w2 * sin_t(2.0f * p) + w3 * sin_t(3.0f * p) + w4 * sin_t(4.0f * p)
                     + w6 * sin_t(6.0f * p) + w8 * sin_t(8.0f * p)) * wnorm * 1.6f;
            } else {                              /* FM a due operatori; piano elettrico con saturazione */
                v->pc += dt; if (v->pc >= 1.0f) v->pc -= 1.0f;
                v->pm += dt * ratio; if (v->pm >= 1.0f) v->pm -= floorf(v->pm);
                v->ienv = ifloor + (v->ienv - ifloor) * icoef;
                x = sin_t(v->pc + idx_base * v->ienv * 0.15915494f * sin_t(v->pm));
                if (eng == ENG_REED) x = softclip(x * drive + bias) - bias_out;
            }
            mix[i] += x * v->level * amp;
        }
    }
    return count;
}

/* ------------------------------------------------------------------ API */
Orc *orc_create(float sr)
{
    if (SINE[SINE_N / 4] == 0.0f)
        for (int i = 0; i <= SINE_N; i++) SINE[i] = sinf(6.28318530717959f * (float)i / SINE_N);
    Orc *o = calloc(1, sizeof(Orc));
    if (!o) return NULL;
    o->sr = sr;
    o->inv_sr = 1.0f / sr;
    o->rng = 0x2545F491u;
    o->bpm = 100.0f;
    o->beat_step = -1;
    o->beat_step_pub = -1;
    o->beat_level = 0.8f;
    o->bass_owner = -1;
    o->loop_bars = 4;
    o->loop_q = 0.5f;
    o->loop_len = 16.0;

    Control c = { { ENG_VA, 0.5f, 0.4f, 0.01f, 0.8f, 0.7f, 0.5f, 0.1f, 0.5f, 0.3f, 0.3f, 0.2f, 0.3f, PERF_CHORD, 0.3f, 0.8f },
                  0.0f, 0.0f, 0.0f };
    o->ctl_stage = o->ctl = c;
    for (int i = 0; i < 3; i++) o->ctl_buf[i] = c;
    o->ctl_back = 0;
    o->ctl_middle = 1;
    o->ctl_front = 2;

    float g = tanf(PI_F * 1150.0f / sr), k = 0.8f;
    o->clap_a1 = 1.0f / (1.0f + g * (g + k));
    o->clap_a2 = g * o->clap_a1;
    o->clap_a3 = g * o->clap_a2;

    o->ch_len = (int)(sr * 0.05f);
    o->ch_l = calloc((size_t)o->ch_len, sizeof(float));
    o->ch_r = calloc((size_t)o->ch_len, sizeof(float));
    o->dlen = (int)(sr * 2.0f);
    o->dl = calloc((size_t)o->dlen, sizeof(float));
    o->dr_ = calloc((size_t)o->dlen, sizeof(float));
    static const int combs[4] = { 1116, 1188, 1277, 1356 }, aps[2] = { 556, 441 };
    float r = sr / 44100.0f;
    for (int i = 0; i < 4; i++) { comb_init(&o->comb_l[i], (int)(combs[i] * r)); comb_init(&o->comb_r[i], (int)((combs[i] + 23) * r)); }
    for (int i = 0; i < 2; i++) { allpass_init(&o->ap_l[i], (int)(aps[i] * r)); allpass_init(&o->ap_r[i], (int)((aps[i] + 23) * r)); }
    int ok = o->ch_l && o->ch_r && o->dl && o->dr_;
    for (int i = 0; i < 4; i++) ok = ok && o->comb_l[i].buf && o->comb_r[i].buf;
    for (int i = 0; i < 2; i++) ok = ok && o->ap_l[i].buf && o->ap_r[i].buf;
    if (!ok) { orc_destroy(o); return NULL; }
    return o;
}

void orc_destroy(Orc *o)
{
    if (!o) return;
    free(o->ch_l); free(o->ch_r); free(o->dl); free(o->dr_);
    for (int i = 0; i < 4; i++) { free(o->comb_l[i].buf); free(o->comb_r[i].buf); }
    for (int i = 0; i < 2; i++) { free(o->ap_l[i].buf); free(o->ap_r[i].buf); }
    free(o);
}

static void push(Orc *o, const Event *e)
{
    int h = o->q_head, n = (h + 1) % QUEUE_SIZE;
    if (n == __atomic_load_n(&o->q_tail, __ATOMIC_ACQUIRE)) return;   /* piena: l'evento si perde */
    o->q[h] = *e;
    __atomic_store_n(&o->q_head, n, __ATOMIC_RELEASE);
}

static void push_simple(Orc *o, int type, int a, float b)
{
    Event e = { type, 0, a, 0, b, { 0 } };
    push(o, &e);
}

static void ctl_publish(Orc *o)
{
    o->ctl_buf[o->ctl_back] = o->ctl_stage;
    o->ctl_back = __atomic_exchange_n(&o->ctl_middle, o->ctl_back | 4, __ATOMIC_ACQ_REL) & 3;
}

void orc_set_sound(Orc *o, const OrcSound *s) { o->ctl_stage.snd = *s; ctl_publish(o); }
void orc_set_expression(Orc *o, float bend, float mod, float bright)
{
    o->ctl_stage.bend = bend;
    o->ctl_stage.mod = mod;
    o->ctl_stage.bright = bright;
    ctl_publish(o);
}

void orc_chord_on(Orc *o, const int *notes, int count, int bass_note, float vel, int tag)
{
    Event e = { EV_CHORD_ON, count > ORC_MAX_CHORD ? ORC_MAX_CHORD : (count < 0 ? 0 : count), bass_note, tag, vel, { 0 } };
    for (int i = 0; i < e.n; i++) e.notes[i] = notes[i];
    push(o, &e);
}
void orc_chord_off(Orc *o) { push_simple(o, EV_CHORD_OFF, 0, 0.0f); }
void orc_note_on(Orc *o, int note, float vel) { push_simple(o, EV_NOTE_ON, note, vel); }
void orc_note_off(Orc *o, int note) { push_simple(o, EV_NOTE_OFF, note, 0.0f); }
void orc_all_off(Orc *o) { push_simple(o, EV_ALL_OFF, 0, 0.0f); }
void orc_set_tempo(Orc *o, float bpm) { push_simple(o, EV_TEMPO, 0, bpm); }
void orc_transport(Orc *o, int on) { push_simple(o, EV_TRANSPORT, on ? 1 : 0, 0.0f); }
void orc_set_beat(Orc *o, int on, int style, float level)
{
    if (style < 0) style = 0;
    push_simple(o, EV_BEAT, (on ? 1 : 0) | (style % BEAT_COUNT) << 1, level);
}
void orc_loop_record(Orc *o) { push_simple(o, EV_LOOP_REC, 0, 0.0f); }
void orc_loop_play(Orc *o) { push_simple(o, EV_LOOP_PLAY, 0, 0.0f); }
void orc_loop_clear(Orc *o) { push_simple(o, EV_LOOP_CLEAR, 0, 0.0f); }
void orc_loop_config(Orc *o, int max_bars, float quantize) { push_simple(o, EV_LOOP_CONFIG, max_bars, quantize); }

int orc_transport_on(const Orc *o) { return get_i(&o->transport); }
double orc_beats(const Orc *o) { return get_i(&o->transport) ? get_d(&o->beats) : 0.0; }
int orc_beat_step(const Orc *o) { return get_i(&o->beat_step_pub); }
const char *orc_beat_pattern(int style, int part)
{
    if (style < 0 || style >= BEAT_COUNT || part < 0 || part >= DR_COUNT) return "................";
    return BEATS[style][part];
}
int orc_active_voices(const Orc *o) { return get_i(&o->voices_pub); }
int orc_voice_starts(const Orc *o) { return get_i(&o->starts_pub); }

void orc_loop_info(const Orc *o, OrcLoopInfo *info)
{
    info->state = get_i(&o->loop_state);
    info->events = get_i(&o->loop_n);
    info->full = get_i(&o->loop_full);
    info->length = get_d(&o->loop_len);
    info->position = 0.0;
    if (!get_i(&o->transport) || info->length <= 0.0) return;
    double rel = get_d(&o->beats) - get_d(&o->loop_origin);
    if (info->state == LOOP_ARMED) info->position = rel;
    else if (info->state != LOOP_EMPTY) {
        double p = fmod(rel, info->length);
        info->position = p < 0.0 ? p + info->length : p;
    }
}

int orc_loop_marks(const Orc *o, float *beats, int *kinds, int max)
{
    int n = get_i(&o->loop_n);
    if (n > max) n = max;
    for (int i = 0; i < n; i++) {
        beats[i] = get_f(&o->loop[i].t);
        kinds[i] = get_i(&o->loop[i].kind);
    }
    return n;
}

int orc_layer_notes(const Orc *o, int layer, int *notes, int *tag)
{
    if (layer < 0 || layer >= ORC_LAYERS) return 0;
    int n = get_i(&o->pub_n[layer]);
    for (int i = 0; i < n; i++) notes[i] = get_i(&o->pub_notes[layer][i]);
    if (tag) *tag = get_i(&o->pub_tag[layer]);
    return n;
}

void orc_scope(const Orc *o, float *dst, int n)
{
    if (n > ORC_SCOPE) n = ORC_SCOPE;
    int p = get_i(&o->scope_pos);
    for (int i = 0; i < n; i++) dst[i] = get_f(&o->scope[(p - n + i + ORC_SCOPE) % ORC_SCOPE]);
}

void orc_render(Orc *o, float *out, int frames)
{
#if defined(__aarch64__)
    {   /* denormali a zero: le code di riverbero e delay non rallentano il processore */
        uint64_t fpcr;
        __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
        if (!(fpcr & (1ull << 24))) __asm__ volatile("msr fpcr, %0" : : "r"(fpcr | (1ull << 24)));
    }
#endif
    if (__atomic_load_n(&o->ctl_middle, __ATOMIC_ACQUIRE) & 4) {
        o->ctl_front = __atomic_exchange_n(&o->ctl_middle, o->ctl_front, __ATOMIC_ACQ_REL) & 3;
        o->ctl = o->ctl_buf[o->ctl_front];
    }
    int t = o->q_tail;
    while (t != __atomic_load_n(&o->q_head, __ATOMIC_ACQUIRE)) {
        handle(o, &o->q[t]);
        t = (t + 1) % QUEUE_SIZE;
        __atomic_store_n(&o->q_tail, t, __ATOMIC_RELEASE);
    }

    const OrcSound *s = &o->ctl.snd;
    const float sr = o->sr, inv = o->inv_sr;
    const int eng = s->engine;
    const float vib = (eng == ENG_REED ? 0.0f : clamp01(s->vibrato) * 0.25f) + clamp01(o->ctl.mod) * 0.45f;
    const float trem_depth = eng == ENG_REED ? clamp01(s->vibrato) * 0.45f : 0.0f;
    const float chorus = clamp01(s->chorus), delay = clamp01(s->delay), reverb = clamp01(s->reverb);
    const float vol = clamp01(s->volume);
    const float bass_lvl = clamp01(s->bass_level) * 0.8f, bass_cut = 0.04f + clamp01(s->bass_tone) * 0.12f;
    const int dtime = (int)fmin(sr * 60.0 / o->bpm * 0.75, (double)(o->dlen - 1));     /* croma puntata */
    int voices = 0, sp = o->scope_pos;

    for (int off = 0; off < frames; off += SUB) {
        int n = frames - off < SUB ? frames - off : SUB;
        double bps = (double)o->bpm / (60.0 * sr);
        if (o->transport) {
            drum_tick(o);
            loop_tick(o, o->beats, o->beats + n * bps);
        }
        for (int L = 0; L < ORC_LAYERS; L++) perform_tick(o, L, n);

        float lfo = sin_t(o->lfo);
        o->lfo += 5.3f * n * inv;
        if (o->lfo >= 1.0f) o->lfo -= 1.0f;
        float pitch = exp2f((o->ctl.bend + lfo * vib) / 12.0f);
        float trem = 1.0f - trem_depth * (0.5f + 0.5f * lfo);
        voices = render_voices(o, n, pitch);

        for (int i = 0; i < n; i++) {
            float vo = softclip(o->mix[i] * 0.25f) * trem;

            float bass = 0.0f;
            if (o->bass_active) {
                o->bass_freq += (o->bass_target - o->bass_freq) * 0.0015f;
                if (o->bass_released) {
                    o->bass_level *= 0.9993f;
                    if (o->bass_level < 1e-4f) { o->bass_active = 0; o->bass_level = 0.0f; }
                } else {
                    o->bass_level += (1.0f - o->bass_level) * 0.004f;
                }
                float bdt = o->bass_freq * inv;
                o->bass_ph += bdt;
                if (o->bass_ph >= 1.0f) o->bass_ph -= 1.0f;
                float raw = 0.65f * sin_t(o->bass_ph) + 0.35f * (2.0f * o->bass_ph - 1.0f - polyblep(o->bass_ph, bdt));
                o->bass_lp += bass_cut * (raw - o->bass_lp);
                o->bass_lp2 += bass_cut * (o->bass_lp - o->bass_lp2);
                bass = o->bass_lp2 * o->bass_level * bass_lvl;
            }

            float drums = 0.0f;
            for (int k = 0; k < DR_COUNT; k++) drums += drum_sample(o, k);
            drums *= 0.8f;

            /* chorus stereo sulle voci */
            float cl = vo, cr = vo;
            if (chorus > 0.001f) {
                o->ch_l[o->ch_pos] = vo;
                o->ch_r[o->ch_pos] = vo;
                o->ch_phase += 0.33f * inv;
                if (o->ch_phase >= 1.0f) o->ch_phase -= 1.0f;
                float dl_ = (0.012f + 0.003f * sin_t(o->ch_phase)) * sr, dr2 = (0.012f + 0.003f * sin_t(o->ch_phase + 0.25f)) * sr;
                float wl = read_frac(o->ch_l, o->ch_len, o->ch_pos, dl_), wr = read_frac(o->ch_r, o->ch_len, o->ch_pos, dr2);
                if (++o->ch_pos >= o->ch_len) o->ch_pos = 0;
                cl = vo * (1.0f - 0.35f * chorus) + wl * 0.7f * chorus;
                cr = vo * (1.0f - 0.35f * chorus) + wr * 0.7f * chorus;
            }
            /* delay ping-pong a croma puntata */
            int rp = o->dpos - dtime;
            if (rp < 0) rp += o->dlen;
            float rl = o->dl[rp], rr = o->dr_[rp];
            o->dlp_l += 0.35f * (rl - o->dlp_l);
            o->dlp_r += 0.35f * (rr - o->dlp_r);
            o->dl[o->dpos] = 0.5f * (cl + cr) + o->dlp_r * 0.38f;
            o->dr_[o->dpos] = o->dlp_l * 0.38f;
            if (++o->dpos >= o->dlen) o->dpos = 0;
            float out_l = cl + bass + drums + rl * delay;
            float out_r = cr + bass + drums + rr * delay;
            if (reverb > 0.001f) {
                float in = (0.5f * (cl + cr) + drums * 0.2f) * 0.03f, wl = 0.0f, wr = 0.0f;
                for (int c = 0; c < 4; c++) { wl += comb_run(&o->comb_l[c], in, 0.86f, 0.25f); wr += comb_run(&o->comb_r[c], in, 0.86f, 0.25f); }
                for (int a = 0; a < 2; a++) { wl = allpass_run(&o->ap_l[a], wl); wr = allpass_run(&o->ap_r[a], wr); }
                out_l += wl * reverb * 3.0f;
                out_r += wr * reverb * 3.0f;
            }
            out_l = softclip(out_l * vol);
            out_r = softclip(out_r * vol);
            out[2 * (off + i)] = out_l;
            out[2 * (off + i) + 1] = out_r;
            set_f(&o->scope[sp], 0.5f * (out_l + out_r));
            sp = (sp + 1) % ORC_SCOPE;
        }
        set_i(&o->scope_pos, sp);
        if (o->transport) set_d(&o->beats, o->beats + n * bps);
    }
    set_i(&o->voices_pub, voices);
    set_i(&o->starts_pub, o->starts);
}
