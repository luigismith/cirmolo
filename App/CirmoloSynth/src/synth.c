/* Cirmolo Synth - motore audio (vedi synth.h). */
#include "synth.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PI_F      3.14159265358979f
#define TWO_PI_F  6.28318530717959f
#define QUEUE_SIZE 512
#define MAX_DELAY_SECONDS 2.5f
#define CUTOFF_MIN_OCT 4.321928f          /* log2(20) */
#define CUTOFF_RANGE_OCT 9.813781f        /* log2(18000) - log2(20) */

enum { EV_NOTE_ON, EV_NOTE_OFF, EV_ALL_OFF, EV_DRUM };
typedef struct { int type, a; float b; } Event;

enum { ENV_IDLE, ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE };
typedef struct { int stage; float level; } Env;

typedef struct {
    int active, note, released;
    float vel, freq, target;
    float ph1, ph2;
    Env aenv, fenv;
    float ic1, ic2;
    uint32_t age;
} Voice;

typedef struct { float *buf; int len, pos; float store; } Comb;
typedef struct { float *buf; int len, pos; } Allpass;

typedef struct {
    int active;
    float t, vel, phase, lp, ic1, ic2;
} Drum;

struct Synth {
    float sr, inv_sr;
    SynthPatch patch;
    FxParams fx;
    Pattern patterns[SY_PATTERNS];
    int selected, playing_pat;
    unsigned chain;

    Event q[QUEUE_SIZE];
    int q_head, q_tail;

    Voice v[SY_VOICES];
    uint32_t age_counter;
    int held[16], held_count;

    float bend, cut_oct, res_add, vibrato;
    float lfo_phase, vib_phase;
    uint32_t rng;

    Drum drums[DRUM_COUNT];
    uint32_t drum_flash[DRUM_COUNT];

    float bpm, swing;
    int playing, step;
    double countdown;
    int seq_note;
    double seq_off;
    int root, scale[12], scale_len;

    float *dl, *dr;
    int dlen, dpos;
    float dlp_l, dlp_r;
    Comb comb_l[4], comb_r[4];
    Allpass ap_l[2], ap_r[2];

    float scope[SY_SCOPE];
    int scope_pos;
    float peak;

    /* arpeggiatore: note tenute in ordine crescente */
    int arp_held[16], arp_n, arp_idx, arp_note, arp_was_on;
    float arp_vel;
    double arp_count, arp_off;

    /* registrazione */
    int16_t *rec;
    uint32_t rec_w, rec_r, rec_drop;
    int rec_on;
};

/* ------------------------------------------------------------------ utilita' */

static inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

static inline float white(Synth *s)
{
    s->rng ^= s->rng << 13;
    s->rng ^= s->rng >> 17;
    s->rng ^= s->rng << 5;
    return (float)(int32_t)s->rng * (1.0f / 2147483648.0f);
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

static inline float oscillator(int wave, float ph, float dt, float pw)
{
    switch (wave) {
    case WAVE_SAW:
        return 2.0f * ph - 1.0f - polyblep(ph, dt);
    case WAVE_SQUARE: {
        float v = ph < pw ? 1.0f : -1.0f;
        float t2 = ph - pw;
        if (t2 < 0.0f) t2 += 1.0f;
        v += polyblep(ph, dt) - polyblep(t2, dt);
        return v - (2.0f * pw - 1.0f);              /* niente componente continua */
    }
    case WAVE_TRIANGLE:
        return 4.0f * fabsf(ph - 0.5f) - 1.0f;
    default:
        return sinf(TWO_PI_F * ph);
    }
}

static float env_coef(float seconds, float sr)
{
    if (seconds < 0.001f) seconds = 0.001f;
    return expf(-4.0f / (seconds * sr));           /* ~98% della strada nel tempo indicato */
}

static inline float env_step(Env *e, float attack_inc, float dcoef, float sustain, float rcoef)
{
    switch (e->stage) {
    case ENV_ATTACK:
        e->level += attack_inc;
        if (e->level >= 1.0f) { e->level = 1.0f; e->stage = ENV_DECAY; }
        break;
    case ENV_DECAY:
        e->level = sustain + (e->level - sustain) * dcoef;
        if (fabsf(e->level - sustain) < 1e-4f) { e->level = sustain; e->stage = ENV_SUSTAIN; }
        break;
    case ENV_SUSTAIN:
        e->level = sustain;
        break;
    case ENV_RELEASE:
        e->level *= rcoef;
        if (e->level < 1e-4f) { e->level = 0.0f; e->stage = ENV_IDLE; }
        break;
    default:
        e->level = 0.0f;
    }
    return e->level;
}

static inline float midi_hz(int note) { return 440.0f * exp2f((float)(note - 69) / 12.0f); }

/* ------------------------------------------------------------------ voci */

static void voice_start(Synth *s, Voice *v, int note, float vel, int keep_freq)
{
    float f = midi_hz(note);
    if (!v->active) {
        v->ph1 = (white(s) + 1.0f) * 0.5f;
        v->ph2 = (white(s) + 1.0f) * 0.5f;
        v->ic1 = v->ic2 = 0.0f;
        v->aenv.level = v->fenv.level = 0.0f;
    }
    v->freq = (keep_freq && v->active) ? v->freq : f;
    v->target = f;
    v->active = 1;
    v->released = 0;
    v->note = note;
    v->vel = vel;
    v->age = ++s->age_counter;
    v->aenv.stage = ENV_ATTACK;                    /* riparte dal livello attuale: niente click */
    v->fenv.stage = ENV_ATTACK;
}

static void voice_release(Voice *v)
{
    v->released = 1;
    if (v->aenv.stage != ENV_IDLE) v->aenv.stage = ENV_RELEASE;
    if (v->fenv.stage != ENV_IDLE) v->fenv.stage = ENV_RELEASE;
}

static void held_remove(Synth *s, int note)
{
    int j = 0;
    for (int i = 0; i < s->held_count; i++)
        if (s->held[i] != note) s->held[j++] = s->held[i];
    s->held_count = j;
}

static void note_on_internal(Synth *s, int note, float vel)
{
    if (note < 0 || note > 127) return;
    if (s->patch.mono) {
        held_remove(s, note);
        if (s->held_count == 16) memmove(s->held, s->held + 1, 15 * sizeof(int)), s->held_count = 15;
        s->held[s->held_count++] = note;
        Voice *v = &s->v[0];
        for (int i = 1; i < SY_VOICES; i++) if (s->v[i].active) voice_release(&s->v[i]);
        if (v->active && !v->released && s->held_count > 1) {   /* legato: scivola, non riparte */
            v->target = midi_hz(note);
            v->note = note;
            v->vel = vel;
        } else {
            voice_start(s, v, note, vel, 1);
        }
        return;
    }
    Voice *pick = NULL;
    for (int i = 0; i < SY_VOICES && !pick; i++)
        if (s->v[i].active && s->v[i].note == note) pick = &s->v[i];
    for (int i = 0; i < SY_VOICES && !pick; i++)
        if (!s->v[i].active) pick = &s->v[i];
    if (!pick) {                                   /* ruba: prima la piu' vecchia rilasciata */
        for (int i = 0; i < SY_VOICES; i++)
            if (s->v[i].released && (!pick || s->v[i].age < pick->age)) pick = &s->v[i];
    }
    if (!pick) {
        pick = &s->v[0];
        for (int i = 1; i < SY_VOICES; i++) if (s->v[i].age < pick->age) pick = &s->v[i];
    }
    voice_start(s, pick, note, vel, 0);
}

static void note_off_internal(Synth *s, int note)
{
    if (s->patch.mono) {
        held_remove(s, note);
        Voice *v = &s->v[0];
        if (v->active && !v->released && v->note == note) {
            if (s->held_count > 0) {
                v->note = s->held[s->held_count - 1];
                v->target = midi_hz(v->note);
            } else {
                voice_release(v);
            }
        }
        return;
    }
    for (int i = 0; i < SY_VOICES; i++)
        if (s->v[i].active && !s->v[i].released && s->v[i].note == note) voice_release(&s->v[i]);
}

static void all_off_internal(Synth *s)
{
    s->held_count = 0;
    for (int i = 0; i < SY_VOICES; i++) if (s->v[i].active) voice_release(&s->v[i]);
}

/* ------------------------------------------------------------------ arpeggiatore */

static void arp_add(Synth *s, int note, float vel)
{
    for (int i = 0; i < s->arp_n; i++) if (s->arp_held[i] == note) return;
    if (s->arp_n == 16) return;
    int i = s->arp_n++;
    while (i > 0 && s->arp_held[i - 1] > note) { s->arp_held[i] = s->arp_held[i - 1]; i--; }
    s->arp_held[i] = note;
    s->arp_vel = vel;
    if (s->arp_n == 1) { s->arp_idx = 0; s->arp_count = 0.0; }      /* la prima nota parte subito */
}

static void arp_remove(Synth *s, int note)
{
    int j = 0;
    for (int i = 0; i < s->arp_n; i++) if (s->arp_held[i] != note) s->arp_held[j++] = s->arp_held[i];
    s->arp_n = j;
}

static void arp_tick(Synth *s, double step_len)
{
    if (s->arp_note >= 0) {
        s->arp_off -= 1.0;
        if (s->arp_off <= 0.0) { note_off_internal(s, s->arp_note); s->arp_note = -1; }
    }
    int on = s->patch.arp_mode > ARP_OFF && s->patch.arp_mode < ARP_COUNT;
    if (!on && s->arp_was_on) {                     /* spento con note tenute: suonano normalmente */
        for (int i = 0; i < s->arp_n; i++) note_on_internal(s, s->arp_held[i], s->arp_vel);
        s->arp_n = 0;
    }
    s->arp_was_on = on;
    if (!on || s->arp_n == 0) return;
    s->arp_count -= 1.0;
    if (s->arp_count > 0.0) return;
    float rate = s->patch.arp_rate;
    if (rate < 0.5f) rate = 0.5f;
    if (rate > 4.0f) rate = 4.0f;
    double interval = step_len * rate;
    s->arp_count += interval;
    int oct = s->patch.arp_octaves < 1 ? 1 : (s->patch.arp_octaves > 3 ? 3 : s->patch.arp_octaves);
    int len = s->arp_n * oct, pos;
    switch (s->patch.arp_mode) {
    case ARP_DOWN: pos = len - 1 - s->arp_idx % len; break;
    case ARP_UPDOWN: {
        int period = len > 1 ? 2 * len - 2 : 1, k = s->arp_idx % period;
        pos = k < len ? k : period - k;
        break;
    }
    case ARP_RANDOM: pos = (int)((white(s) + 1.0f) * 0.5f * len) % len; break;
    default: pos = s->arp_idx % len;
    }
    s->arp_idx++;
    int note = s->arp_held[pos % s->arp_n] + 12 * (pos / s->arp_n);
    if (s->arp_note >= 0) note_off_internal(s, s->arp_note);
    note_on_internal(s, note, s->arp_vel);
    s->arp_note = note;
    s->arp_off = interval * 0.6;
}

/* ------------------------------------------------------------------ batteria */

static void drum_trigger(Synth *s, int kind, float vel)
{
    if (kind < 0 || kind >= DRUM_COUNT) return;
    Drum *d = &s->drums[kind];
    d->active = 1;
    d->t = 0.0f;
    d->vel = vel;
    d->phase = 0.0f;
    d->lp = d->ic1 = d->ic2 = 0.0f;
    if (kind == DRUM_HAT) s->drums[DRUM_OPENHAT].active = 0;     /* il charleston chiuso strozza quello aperto */
    s->drum_flash[kind]++;
}

static float drum_sample(Synth *s, int kind)
{
    Drum *d = &s->drums[kind];
    if (!d->active) return 0.0f;
    float t = d->t, out = 0.0f;
    d->t += s->inv_sr;
    switch (kind) {
    case DRUM_KICK: {
        float f = 46.0f + 125.0f * expf(-t / 0.028f);
        d->phase += f * s->inv_sr;
        if (d->phase >= 1.0f) d->phase -= 1.0f;
        float amp = expf(-t / 0.33f);
        out = sinf(TWO_PI_F * d->phase) * amp * 1.15f;
        if (t < 0.003f) out += white(s) * 0.25f * (1.0f - t / 0.003f);
        if (t > 1.8f) d->active = 0;
        break;
    }
    case DRUM_SNARE: {
        d->phase += 190.0f * s->inv_sr;
        if (d->phase >= 1.0f) d->phase -= 1.0f;
        float n = white(s);
        d->lp += 0.25f * (n - d->lp);
        out = sinf(TWO_PI_F * d->phase) * expf(-t / 0.07f) * 0.55f + (n - d->lp) * expf(-t / 0.15f) * 0.75f;
        if (t > 0.8f) d->active = 0;
        break;
    }
    case DRUM_HAT:
    case DRUM_OPENHAT: {
        float n = white(s);
        d->lp += 0.6f * (n - d->lp);
        float hp = n - d->lp;
        out = hp * (kind == DRUM_HAT ? expf(-t / 0.035f) * 0.5f : expf(-t / 0.3f) * 0.42f);
        if (t > (kind == DRUM_HAT ? 0.3f : 1.6f)) d->active = 0;
        break;
    }
    case DRUM_CLAP: {
        float g = tanf(PI_F * 1150.0f * s->inv_sr), k = 0.8f;
        float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        float x = white(s);
        float v3 = x - d->ic2, v1 = a1 * d->ic1 + a2 * v3, v2 = d->ic2 + a2 * d->ic1 + a3 * v3;
        d->ic1 = 2.0f * v1 - d->ic1;
        d->ic2 = 2.0f * v2 - d->ic2;
        float e = 0.0f;
        for (int i = 0; i < 3; i++) {
            float tb = t - i * 0.011f;
            if (tb >= 0.0f && tb < 0.011f) { float b = expf(-tb / 0.004f); if (b > e) e = b; }
        }
        if (t > 0.022f) { float tail = expf(-(t - 0.022f) / 0.13f) * 0.8f; if (tail > e) e = tail; }
        out = v1 * e * 1.6f;
        if (t > 0.9f) d->active = 0;
        break;
    }
    }
    return out * d->vel;
}

/* ------------------------------------------------------------------ effetti */

static void comb_init(Comb *c, int len)
{
    c->buf = calloc((size_t)len, sizeof(float));
    c->len = len;
    c->pos = 0;
    c->store = 0.0f;
}

static void allpass_init(Allpass *a, int len)
{
    a->buf = calloc((size_t)len, sizeof(float));
    a->len = len;
    a->pos = 0;
}

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

/* ------------------------------------------------------------------ API */

Synth *synth_create(float sample_rate)
{
    Synth *s = calloc(1, sizeof(Synth));
    if (!s) return NULL;
    s->sr = sample_rate;
    s->inv_sr = 1.0f / sample_rate;
    s->rng = 0x9E3779B9u;
    s->bpm = 110.0f;
    s->step = -1;
    s->seq_note = -1;
    s->root = 48;
    static const int major[7] = { 0, 2, 4, 5, 7, 9, 11 };
    memcpy(s->scale, major, sizeof(major));
    s->scale_len = 7;
    for (int p = 0; p < SY_PATTERNS; p++)
        for (int i = 0; i < SY_STEPS; i++) s->patterns[p].note[i] = -1;
    s->playing_pat = -1;
    s->arp_note = -1;
    s->rec = calloc((size_t)SY_REC_FRAMES * 2, sizeof(int16_t));

    s->dlen = (int)(sample_rate * MAX_DELAY_SECONDS);
    s->dl = calloc((size_t)s->dlen, sizeof(float));
    s->dr = calloc((size_t)s->dlen, sizeof(float));
    static const int combs[4] = { 1116, 1188, 1277, 1356 }, aps[2] = { 556, 441 };
    float k = sample_rate / 44100.0f;
    for (int i = 0; i < 4; i++) {
        comb_init(&s->comb_l[i], (int)(combs[i] * k));
        comb_init(&s->comb_r[i], (int)((combs[i] + 23) * k));
    }
    for (int i = 0; i < 2; i++) {
        allpass_init(&s->ap_l[i], (int)(aps[i] * k));
        allpass_init(&s->ap_r[i], (int)((aps[i] + 23) * k));
    }
    return s;
}

void synth_destroy(Synth *s)
{
    if (!s) return;
    free(s->dl);
    free(s->dr);
    free(s->rec);
    for (int i = 0; i < 4; i++) { free(s->comb_l[i].buf); free(s->comb_r[i].buf); }
    for (int i = 0; i < 2; i++) { free(s->ap_l[i].buf); free(s->ap_r[i].buf); }
    free(s);
}

float synth_sample_rate(const Synth *s) { return s->sr; }

static void push(Synth *s, int type, int a, float b)
{
    int h = s->q_head, n = (h + 1) % QUEUE_SIZE;
    if (n == __atomic_load_n(&s->q_tail, __ATOMIC_ACQUIRE)) return;   /* coda piena: si perde l'evento */
    s->q[h].type = type;
    s->q[h].a = a;
    s->q[h].b = b;
    __atomic_store_n(&s->q_head, n, __ATOMIC_RELEASE);
}

void synth_note_on(Synth *s, int note, float vel) { push(s, EV_NOTE_ON, note, vel); }
void synth_note_off(Synth *s, int note) { push(s, EV_NOTE_OFF, note, 0.0f); }
void synth_all_notes_off(Synth *s) { push(s, EV_ALL_OFF, 0, 0.0f); }
void synth_drum_hit(Synth *s, int drum, float vel) { push(s, EV_DRUM, drum, vel); }

SynthPatch *synth_patch(Synth *s) { return &s->patch; }
FxParams *synth_fx(Synth *s) { return &s->fx; }
Pattern *synth_pattern(Synth *s) { return &s->patterns[s->selected]; }
Pattern *synth_pattern_at(Synth *s, int i) { return &s->patterns[(i % SY_PATTERNS + SY_PATTERNS) % SY_PATTERNS]; }
void synth_select_pattern(Synth *s, int i) { s->selected = (i % SY_PATTERNS + SY_PATTERNS) % SY_PATTERNS; }
int synth_selected_pattern(const Synth *s) { return s->selected; }
void synth_set_chain(Synth *s, unsigned mask) { s->chain = mask & ((1u << SY_PATTERNS) - 1); }
unsigned synth_chain(const Synth *s) { return s->chain; }
int synth_playing_pattern(const Synth *s) { return s->playing ? s->playing_pat : -1; }

void synth_rec_start(Synth *s)
{
    __atomic_store_n(&s->rec_r, __atomic_load_n(&s->rec_w, __ATOMIC_ACQUIRE), __ATOMIC_RELEASE);
    s->rec_drop = 0;
    __atomic_store_n(&s->rec_on, 1, __ATOMIC_RELEASE);
}

void synth_rec_stop(Synth *s) { __atomic_store_n(&s->rec_on, 0, __ATOMIC_RELEASE); }
int synth_rec_active(const Synth *s) { return __atomic_load_n(&s->rec_on, __ATOMIC_ACQUIRE); }
uint32_t synth_rec_dropped(const Synth *s) { return s->rec_drop; }

int synth_rec_read(Synth *s, int16_t *dst, int max_frames)
{
    uint32_t w = __atomic_load_n(&s->rec_w, __ATOMIC_ACQUIRE), r = s->rec_r;
    int n = (int)(w - r);
    if (n > max_frames) n = max_frames;
    for (int i = 0; i < n; i++) {
        uint32_t k = (r + (uint32_t)i) % SY_REC_FRAMES;
        dst[2 * i] = s->rec[2 * k];
        dst[2 * i + 1] = s->rec[2 * k + 1];
    }
    __atomic_store_n(&s->rec_r, r + (uint32_t)n, __ATOMIC_RELEASE);
    return n;
}

void synth_set_performance(Synth *s, float bend, float cutoff_oct, float res_add, float vibrato)
{
    s->bend = bend;
    s->cut_oct = cutoff_oct;
    s->res_add = res_add;
    s->vibrato = vibrato;
}

void synth_set_tempo(Synth *s, float bpm) { s->bpm = clampf(bpm, 40.0f, 240.0f); }
float synth_tempo(const Synth *s) { return s->bpm; }
void synth_set_swing(Synth *s, float swing) { s->swing = clampf(swing, 0.0f, 0.6f); }
float synth_swing(const Synth *s) { return s->swing; }

void synth_set_scale(Synth *s, int root, const int *intervals, int count)
{
    if (count < 1) return;
    if (count > 12) count = 12;
    for (int i = 0; i < count; i++) s->scale[i] = intervals[i];
    s->scale_len = count;
    s->root = root;
}

int synth_degree_to_midi(const Synth *s, int degree)
{
    int n = s->scale_len;
    int oct = degree >= 0 ? degree / n : -((-degree + n - 1) / n);
    int idx = degree - oct * n;
    return s->root + 12 * oct + s->scale[idx];
}

void synth_play(Synth *s, int on)
{
    if (on && !s->playing) {
        s->step = -1;
        s->playing_pat = -1;
        s->countdown = 0.0;
        __atomic_store_n(&s->playing, 1, __ATOMIC_RELEASE);
    } else if (!on && s->playing) {
        __atomic_store_n(&s->playing, 0, __ATOMIC_RELEASE);
    }
}

int synth_playing(const Synth *s) { return __atomic_load_n(&s->playing, __ATOMIC_ACQUIRE); }
int synth_current_step(const Synth *s) { return s->playing ? s->step : -1; }

void synth_scope(const Synth *s, float *dst, int n)
{
    if (n > SY_SCOPE) n = SY_SCOPE;
    int p = __atomic_load_n(&s->scope_pos, __ATOMIC_ACQUIRE);
    for (int i = 0; i < n; i++) dst[i] = s->scope[(p - n + i + SY_SCOPE) % SY_SCOPE];
}

float synth_peak(Synth *s) { return s->peak; }

int synth_active_voices(const Synth *s)
{
    int n = 0;
    for (int i = 0; i < SY_VOICES; i++) n += s->v[i].active;
    return n;
}

uint32_t synth_drum_flash(const Synth *s, int drum)
{
    return (drum >= 0 && drum < DRUM_COUNT) ? s->drum_flash[drum] : 0;
}

/* ------------------------------------------------------------------ sequencer */

static void seq_tick(Synth *s)
{
    if (s->seq_note >= 0) {
        s->seq_off -= 1.0;
        if (s->seq_off <= 0.0) { note_off_internal(s, s->seq_note); s->seq_note = -1; }
    }
    if (!s->playing) {
        if (s->step != -1) {
            s->step = -1;
            if (s->seq_note >= 0) { note_off_internal(s, s->seq_note); s->seq_note = -1; }
        }
        return;
    }
    s->countdown -= 1.0;
    if (s->countdown > 0.0) return;
    s->step = (s->step + 1) % SY_STEPS;
    if (s->step == 0) {                               /* nuova battuta: quale pattern suona */
        if (s->chain) {
            int from = s->playing_pat;
            for (int i = 1; i <= SY_PATTERNS; i++) {
                int c = from < 0 ? i - 1 : (from + i) % SY_PATTERNS;
                if (s->chain & (1u << c)) { s->playing_pat = c; break; }
            }
        } else {
            s->playing_pat = s->selected;
        }
    }
    if (s->playing_pat < 0) s->playing_pat = s->selected;
    const Pattern *pat = &s->patterns[s->playing_pat];
    double base = s->sr * 60.0 / s->bpm / 4.0;
    double len = (s->step % 2 == 0) ? base * (1.0 + s->swing) : base * (1.0 - s->swing);
    s->countdown += len;
    for (int d = 0; d < DRUM_COUNT; d++) {
        int v = pat->drum[d][s->step];
        if (v) drum_trigger(s, d, v == 2 ? 1.0f : 0.72f);
    }
    int deg = pat->note[s->step];
    if (deg >= 0) {
        if (s->seq_note >= 0) note_off_internal(s, s->seq_note);
        s->seq_note = synth_degree_to_midi(s, deg);
        note_on_internal(s, s->seq_note, pat->accent[s->step] ? 1.0f : 0.75f);
        s->seq_off = base * 0.8;
    }
}

/* ------------------------------------------------------------------ rendering */

void synth_render(Synth *s, float *out, int frames)
{
    /* eventi dall'interfaccia */
    int t = s->q_tail;
    while (t != __atomic_load_n(&s->q_head, __ATOMIC_ACQUIRE)) {
        Event e = s->q[t];
        switch (e.type) {
        case EV_NOTE_ON:
            if (s->patch.arp_mode > ARP_OFF) arp_add(s, e.a, e.b);
            else note_on_internal(s, e.a, e.b);
            break;
        case EV_NOTE_OFF:
            arp_remove(s, e.a);
            note_off_internal(s, e.a);
            break;
        case EV_ALL_OFF:
            s->arp_n = 0;
            if (s->arp_note >= 0) { note_off_internal(s, s->arp_note); s->arp_note = -1; }
            all_off_internal(s);
            break;
        case EV_DRUM: drum_trigger(s, e.a, e.b); break;
        }
        t = (t + 1) % QUEUE_SIZE;
        __atomic_store_n(&s->q_tail, t, __ATOMIC_RELEASE);
    }

    const SynthPatch p = s->patch;                 /* copia: valori stabili per tutto il blocco */
    const FxParams fx = s->fx;
    const float sr = s->sr, inv_sr = s->inv_sr;
    const float a_inc = 1.0f / (fmaxf(p.a_attack, 0.001f) * sr);
    const float a_dc = env_coef(p.a_decay, sr), a_rc = env_coef(p.a_release, sr);
    const float f_inc = 1.0f / (fmaxf(p.f_attack, 0.001f) * sr);
    const float f_dc = env_coef(p.f_decay, sr), f_rc = env_coef(p.f_release, sr);
    const float a_sus = clampf(p.a_sustain, 0.0f, 1.0f), f_sus = clampf(p.f_sustain, 0.0f, 1.0f);
    const float ratio2 = exp2f((p.osc2_semi + p.osc2_detune / 100.0f) / 12.0f);
    const float pw = clampf(p.pulse_width, 0.05f, 0.95f);
    const float mix = clampf(p.osc_mix, 0.0f, 1.0f);
    const float res = clampf(p.resonance + s->res_add, 0.0f, 0.97f);
    const float k = 2.0f - 2.0f * res;
    const float base_oct = CUTOFF_MIN_OCT + clampf(p.cutoff, 0.0f, 1.0f) * CUTOFF_RANGE_OCT + s->cut_oct;
    const float glide_coef = (p.mono && p.glide > 0.001f) ? 1.0f - expf(-4.0f / (p.glide * sr)) : 1.0f;
    const float drive = 1.0f + clampf(p.drive, 0.0f, 1.0f) * 4.0f;
    const float max_fc = 0.42f * sr;
    const double step_len = sr * 60.0 / s->bpm / 4.0;
    int dtime = (int)(clampf(fx.delay_steps, 1.0f, 8.0f) * step_len);
    if (dtime > s->dlen - 1) dtime = s->dlen - 1;
    const float fb = clampf(fx.delay_feedback, 0.0f, 0.9f);
    const float rfb = 0.7f + clampf(fx.reverb_size, 0.0f, 1.0f) * 0.26f;
    float block_peak = 0.0f;

    const int rec_on = __atomic_load_n(&s->rec_on, __ATOMIC_ACQUIRE);
    uint32_t rec_w = s->rec_w;

    for (int i = 0; i < frames; i++) {
        seq_tick(s);
        arp_tick(s, step_len);

        s->lfo_phase += p.lfo_rate * inv_sr;
        if (s->lfo_phase >= 1.0f) s->lfo_phase -= 1.0f;
        s->vib_phase += 5.5f * inv_sr;
        if (s->vib_phase >= 1.0f) s->vib_phase -= 1.0f;
        float lfo = sinf(TWO_PI_F * s->lfo_phase);
        float vib = sinf(TWO_PI_F * s->vib_phase);
        float pitch_mul = exp2f((s->bend + lfo * p.lfo_pitch + vib * s->vibrato * 0.5f) / 12.0f);
        float lfo_oct = lfo * p.lfo_filter * 2.0f;

        float sum = 0.0f;
        for (int vi = 0; vi < SY_VOICES; vi++) {
            Voice *v = &s->v[vi];
            if (!v->active) continue;
            v->freq += (v->target - v->freq) * glide_coef;
            float dt1 = v->freq * pitch_mul * inv_sr;
            if (dt1 > 0.45f) dt1 = 0.45f;
            float dt2 = dt1 * ratio2;
            if (dt2 > 0.45f) dt2 = 0.45f;
            v->ph1 += dt1; if (v->ph1 >= 1.0f) v->ph1 -= 1.0f;
            v->ph2 += dt2; if (v->ph2 >= 1.0f) v->ph2 -= 1.0f;
            float x = oscillator(p.osc1_wave, v->ph1, dt1, pw) * (1.0f - mix) + oscillator(p.osc2_wave, v->ph2, dt2, pw) * mix;
            if (p.noise > 0.0f) x += p.noise * white(s);
            float ae = env_step(&v->aenv, a_inc, a_dc, a_sus, a_rc);
            float fe = env_step(&v->fenv, f_inc, f_dc, f_sus, f_rc);
            if (v->aenv.stage == ENV_IDLE) { v->active = 0; continue; }
            float oct = base_oct + p.env_amount * 6.0f * fe + p.key_track * (float)(v->note - 60) / 12.0f + lfo_oct;
            float fc = exp2f(oct);
            if (fc < 16.0f) fc = 16.0f;
            if (fc > max_fc) fc = max_fc;
            float g = tanf(PI_F * fc * inv_sr);
            float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
            float v3 = x - v->ic2;
            float v1 = a1 * v->ic1 + a2 * v3;
            float v2 = v->ic2 + a2 * v->ic1 + a3 * v3;
            v->ic1 = 2.0f * v1 - v->ic1;
            v->ic2 = 2.0f * v2 - v->ic2;
            sum += v2 * ae * v->vel;
        }
        float synth_out = softclip(sum * 0.32f * drive) * clampf(p.volume, 0.0f, 1.0f) * (drive > 1.0f ? 1.0f / sqrtf(drive) + 0.25f : 1.0f);

        float drums = 0.0f;
        for (int d = 0; d < DRUM_COUNT; d++) drums += drum_sample(s, d);
        drums *= clampf(fx.drums_volume, 0.0f, 1.0f) * 0.75f;

        /* delay ping-pong a tempo, con smorzamento nel ritorno */
        int rp = s->dpos - dtime;
        if (rp < 0) rp += s->dlen;
        float rl = s->dl[rp], rr = s->dr[rp];
        s->dlp_l += 0.35f * (rl - s->dlp_l);
        s->dlp_r += 0.35f * (rr - s->dlp_r);
        s->dl[s->dpos] = synth_out + s->dlp_r * fb;
        s->dr[s->dpos] = s->dlp_l * fb;
        if (++s->dpos >= s->dlen) s->dpos = 0;
        float out_l = synth_out + drums + rl * fx.delay_mix;
        float out_r = synth_out + drums + rr * fx.delay_mix;

        /* riverbero (Freeverb ridotto: 4 comb + 2 allpass per canale) */
        if (fx.reverb_mix > 0.001f) {
            float in = (synth_out + drums * 0.25f) * 0.03f;
            float wl = 0.0f, wr = 0.0f;
            for (int c = 0; c < 4; c++) {
                wl += comb_run(&s->comb_l[c], in, rfb, 0.25f);
                wr += comb_run(&s->comb_r[c], in, rfb, 0.25f);
            }
            for (int a = 0; a < 2; a++) { wl = allpass_run(&s->ap_l[a], wl); wr = allpass_run(&s->ap_r[a], wr); }
            out_l += wl * fx.reverb_mix * 3.0f;
            out_r += wr * fx.reverb_mix * 3.0f;
        }

        out_l = softclip(out_l);
        out_r = softclip(out_r);
        out[2 * i] = out_l;
        out[2 * i + 1] = out_r;
        if (rec_on) {
            if (rec_w - __atomic_load_n(&s->rec_r, __ATOMIC_ACQUIRE) < SY_REC_FRAMES) {
                uint32_t k = rec_w % SY_REC_FRAMES;
                s->rec[2 * k] = (int16_t)lrintf(out_l * 32767.0f);
                s->rec[2 * k + 1] = (int16_t)lrintf(out_r * 32767.0f);
                rec_w++;
            } else {
                s->rec_drop++;
            }
        }
        float m = 0.5f * (out_l + out_r);
        s->scope[s->scope_pos] = m;
        __atomic_store_n(&s->scope_pos, (s->scope_pos + 1) % SY_SCOPE, __ATOMIC_RELEASE);
        if (fabsf(m) > block_peak) block_peak = fabsf(m);
    }
    __atomic_store_n(&s->rec_w, rec_w, __ATOMIC_RELEASE);
    s->peak = fmaxf(s->peak * 0.85f, block_peak);
}
