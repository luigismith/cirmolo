/* Cirmolo Sampler - motore audio (vedi sampler.h). */
#include "sampler.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI_F 3.14159265f
#define Q_SIZE 512                    /* potenza di due */
#define RETIRED 64                    /* campioni sostituiti in attesa di essere liberati */
#define MAX_PER_SLOT 4                /* colpi sovrapposti dello stesso pad */

enum { EV_ON, EV_OFF, EV_ALL_OFF, EV_SET_SAMPLE, EV_SUSTAIN };
enum { ST_ATTACK, ST_SUSTAIN, ST_RELEASE };

typedef struct { uint8_t type; int8_t slot; int key; float vel, semis; Sample *smp; } Event;

typedef struct {
    int active, slot, key, group;
    const Sample *smp;
    double pos, inc;                  /* posizione in frame e passo di lettura (senza bend) */
    float gain;
    int s0, s1;                       /* ritaglio: [s0, s1) */
    int reverse, loop, hold, pending; /* pending: rilascio in attesa del pedale */
    int stage;
    float env, env_inc;
    uint32_t born;
} Voice;

struct Sampler {
    float sr;
    int borrowed;                     /* 1 = i campioni sono di un altro motore (esportazione) */
    /* coda: scrive l'interfaccia (q_head), legge il thread audio (q_tail) */
    Event q[Q_SIZE];
    int q_head, q_tail;
    /* campioni: slot_sample e' del thread audio, ui_sample l'ombra per l'interfaccia */
    Sample *slot_sample[SP_SLOTS];
    Sample *ui_sample[SP_SLOTS];
    Sample *retired[RETIRED];
    int ret_head, ret_tail;
    PadParams pads[SP_SLOTS];
    /* parametri condivisi */
    float volume, bend, cutoff;
    int sustain;
    /* voci */
    Voice voices[SP_VOICES];
    uint32_t born;
    int fade;                         /* frame delle dissolvenze ai bordi */
    /* sequencer */
    SeqPattern patterns[SP_PATTERNS];
    int selected, playing_pat;
    unsigned chain;
    float bpm, swing;
    int metronome, playing, restart, record;
    int step;
    double countdown, step_len;
    int skip[SP_PADS];                /* passo+1 da saltare dopo una registrazione quantizzata in avanti */
    int stop_after_bars, bars_done;   /* per l'esportazione */
    /* metronomo */
    float click_amp, click_phase, click_freq;
    /* filtro sull'uscita */
    float lp1[2], lp2[2];
    /* ingresso */
    float cap_rate, cap_level, cap_prev;
    double cap_phase;
    float *rec_buf;
    int rec_n, rec_state;
    float rec_threshold;
    /* per l'interfaccia */
    float scope[SP_SCOPE];
    int scope_pos;
    float peak;
    int voices_n, slot_voices[SP_SLOTS];
    uint32_t flash[SP_SLOTS];
    float slot_pos[SP_SLOTS];
};

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static void push(Sampler *s, Event e)
{
    int h = s->q_head, n = (h + 1) & (Q_SIZE - 1);
    if (n == __atomic_load_n(&s->q_tail, __ATOMIC_ACQUIRE)) {   /* coda piena: si perde l'evento */
        if (e.type == EV_SET_SAMPLE && e.smp) sample_free(e.smp);
        return;
    }
    s->q[h] = e;
    __atomic_store_n(&s->q_head, n, __ATOMIC_RELEASE);
}

/* ------------------------------------------------------------------ WAV */
static uint32_t rd32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t rd16(const unsigned char *p) { return (uint16_t)(p[0] | p[1] << 8); }

static void sample_set_name(Sample *s, const char *path)
{
    snprintf(s->path, sizeof(s->path), "%s", path);
    const char *base = path;
    for (const char *p = path; *p; p++) if (*p == '/' || *p == '\\') base = p + 1;
    snprintf(s->name, sizeof(s->name), "%s", base);
    char *dot = strrchr(s->name, '.');
    if (dot && dot != s->name) *dot = 0;
}

Sample *sample_load_wav(const char *path)
{
    if (!path || !*path) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    unsigned char hdr[12];
    if (fread(hdr, 1, 12, f) != 12 || memcmp(hdr, "RIFF", 4) || memcmp(hdr + 8, "WAVE", 4)) { fclose(f); return NULL; }
    int fmt_tag = 0, channels = 0, rate = 0, bits = 0, have_fmt = 0;
    unsigned char *data = NULL;
    uint32_t data_len = 0;
    unsigned char ch[8];
    while (fread(ch, 1, 8, f) == 8) {
        uint32_t len = rd32(ch + 4);
        if (!memcmp(ch, "fmt ", 4) && len >= 16 && len < 4096) {
            unsigned char fm[4096];
            if (fread(fm, 1, len, f) != len) break;
            fmt_tag = rd16(fm);
            channels = rd16(fm + 2);
            rate = (int)rd32(fm + 4);
            bits = rd16(fm + 14);
            if (fmt_tag == 0xFFFE && len >= 40) fmt_tag = rd16(fm + 24);   /* WAVE_FORMAT_EXTENSIBLE: il sottoformato */
            have_fmt = 1;
        } else if (!memcmp(ch, "data", 4)) {
            if (!have_fmt) break;
            uint32_t cap = (uint32_t)SP_MAX_SAMPLE_SECONDS * (uint32_t)rate * (uint32_t)channels * (uint32_t)(bits / 8);
            if (len > cap) len = cap;             /* oltre i 30 secondi si taglia */
            data = malloc(len ? len : 1);
            if (!data) break;
            data_len = (uint32_t)fread(data, 1, len, f);
            break;
        } else {
            if (fseek(f, (long)(len + (len & 1)), SEEK_CUR)) break;
        }
        if (len & 1) fseek(f, 1, SEEK_CUR);
    }
    fclose(f);
    int ok = data && (fmt_tag == 1 || fmt_tag == 3) && channels >= 1 && rate >= 8000 && rate <= 192000 &&
             ((fmt_tag == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) || (fmt_tag == 3 && bits == 32));
    if (!ok) { free(data); return NULL; }
    int bps = bits / 8, frame_bytes = bps * channels;
    int src_frames = (int)(data_len / (uint32_t)frame_bytes);
    int out_ch = channels >= 2 ? 2 : 1;
    if (src_frames < 1) { free(data); return NULL; }
    /* decodifica in float alla frequenza del file */
    float *src = malloc(sizeof(float) * (size_t)src_frames * (size_t)out_ch);
    if (!src) { free(data); return NULL; }
    for (int i = 0; i < src_frames; i++) {
        for (int c = 0; c < out_ch; c++) {
            const unsigned char *p = data + (size_t)i * (size_t)frame_bytes + (size_t)c * (size_t)bps;
            float v;
            if (fmt_tag == 3) { float fv; memcpy(&fv, p, 4); v = fv; }
            else if (bits == 8) v = ((int)p[0] - 128) / 128.0f;
            else if (bits == 16) v = (int16_t)rd16(p) / 32768.0f;
            else if (bits == 24) v = (int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24) / 2147483648.0f;
            else v = (int32_t)rd32(p) / 2147483648.0f;
            src[(size_t)i * (size_t)out_ch + (size_t)c] = v != v ? 0.0f : clampf(v, -1.0f, 1.0f);
        }
    }
    free(data);
    Sample *s = calloc(1, sizeof(Sample));
    if (!s) { free(src); return NULL; }
    s->channels = out_ch;
    s->src_rate = rate;
    s->src_bits = bits;
    s->src_channels = channels;
    sample_set_name(s, path);
    if (rate == SP_RATE) {
        s->data = src;
        s->frames = src_frames;
        return s;
    }
    /* ricampionamento lineare a 48 kHz */
    double ratio = (double)rate / SP_RATE;
    int out_frames = (int)((double)src_frames / ratio);
    if (out_frames < 1) out_frames = 1;
    float *dst = malloc(sizeof(float) * (size_t)out_frames * (size_t)out_ch);
    if (!dst) { free(src); free(s); return NULL; }
    for (int i = 0; i < out_frames; i++) {
        double p = i * ratio;
        int i0 = (int)p;
        float fr = (float)(p - i0);
        int i1 = i0 + 1 < src_frames ? i0 + 1 : i0;
        for (int c = 0; c < out_ch; c++) {
            float a = src[(size_t)i0 * (size_t)out_ch + (size_t)c], b = src[(size_t)i1 * (size_t)out_ch + (size_t)c];
            dst[(size_t)i * (size_t)out_ch + (size_t)c] = a + (b - a) * fr;
        }
    }
    free(src);
    s->data = dst;
    s->frames = out_frames;
    return s;
}

void sample_free(Sample *s)
{
    if (!s) return;
    free(s->data);
    free(s);
}

/* ------------------------------------------------------------------ creazione */
void sampler_pad_defaults(PadParams *p)
{
    memset(p, 0, sizeof(*p));
    p->volume = 1.0f;
    p->end = 1.0f;
}

void sampler_pattern_clear(SeqPattern *p)
{
    memset(p, 0, sizeof(*p));
    p->length = SP_STEPS;
}

Sampler *sampler_create(float sample_rate)
{
    Sampler *s = calloc(1, sizeof(Sampler));
    if (!s) return NULL;
    s->sr = sample_rate > 8000.0f ? sample_rate : SP_RATE;
    s->volume = 0.8f;
    s->cutoff = 1.0f;
    s->bpm = 120.0f;
    s->fade = (int)(s->sr * 0.002f);
    for (int i = 0; i < SP_SLOTS; i++) { sampler_pad_defaults(&s->pads[i]); s->slot_pos[i] = -1.0f; }
    for (int p = 0; p < SP_PATTERNS; p++) sampler_pattern_clear(&s->patterns[p]);
    s->playing_pat = -1;
    s->step = -1;
    s->rec_buf = calloc(SP_REC_FRAMES, sizeof(float));
    if (!s->rec_buf) { free(s); return NULL; }
    return s;
}

void sampler_destroy(Sampler *s)
{
    if (!s) return;
    if (!s->borrowed) {
        /* campioni ancora in coda, negli slot e fra quelli sostituiti: ognuno sta in un posto solo */
        for (int t = s->q_tail; t != s->q_head; t = (t + 1) & (Q_SIZE - 1))
            if (s->q[t].type == EV_SET_SAMPLE) sample_free(s->q[t].smp);
        for (int i = 0; i < SP_SLOTS; i++) sample_free(s->slot_sample[i]);
        for (int t = s->ret_tail; t != s->ret_head; t = (t + 1) % RETIRED) sample_free(s->retired[t]);
    }
    free(s->rec_buf);
    free(s);
}

float sampler_sample_rate(const Sampler *s) { return s->sr; }

/* ------------------------------------------------------------------ interfaccia -> coda */
static int slot_ok(int slot) { return slot >= 0 && slot < SP_SLOTS; }

void sampler_set_sample(Sampler *s, int slot, Sample *smp)
{
    if (!slot_ok(slot)) { sample_free(smp); return; }
    s->ui_sample[slot] = smp;
    push(s, (Event){ EV_SET_SAMPLE, (int8_t)slot, 0, 0.0f, 0.0f, smp });
}

const Sample *sampler_sample(const Sampler *s, int slot) { return slot_ok(slot) ? s->ui_sample[slot] : NULL; }

void sampler_collect(Sampler *s)
{
    int t = s->ret_tail;
    while (t != __atomic_load_n(&s->ret_head, __ATOMIC_ACQUIRE)) {
        sample_free(s->retired[t]);
        t = (t + 1) % RETIRED;
        __atomic_store_n(&s->ret_tail, t, __ATOMIC_RELEASE);
    }
}

PadParams *sampler_pad(Sampler *s, int slot) { return &s->pads[slot_ok(slot) ? slot : 0]; }

void sampler_trigger(Sampler *s, int slot, float vel, float semitones, int key)
{
    if (!slot_ok(slot)) return;
    if (vel <= 0.0f) { sampler_release(s, slot, key); return; }
    push(s, (Event){ EV_ON, (int8_t)slot, key, vel > 1.0f ? 1.0f : vel, semitones, NULL });
}

void sampler_release(Sampler *s, int slot, int key)
{
    if (!slot_ok(slot)) return;
    push(s, (Event){ EV_OFF, (int8_t)slot, key, 0.0f, 0.0f, NULL });
}

void sampler_release_all(Sampler *s) { push(s, (Event){ EV_ALL_OFF, 0, 0, 0.0f, 0.0f, NULL }); }
void sampler_set_sustain(Sampler *s, int on) { push(s, (Event){ EV_SUSTAIN, 0, on != 0, 0.0f, 0.0f, NULL }); }
void sampler_set_bend(Sampler *s, float semitones) { s->bend = clampf(semitones, -24.0f, 24.0f); }
void sampler_set_cutoff(Sampler *s, float v) { s->cutoff = clampf(v, 0.0f, 1.0f); }
void  sampler_set_volume(Sampler *s, float v) { s->volume = clampf(v, 0.0f, 1.0f); }
float sampler_volume(const Sampler *s) { return s->volume; }

/* ------------------------------------------------------------------ sequencer: parametri */
SeqPattern *sampler_pattern(Sampler *s) { return &s->patterns[s->selected]; }
SeqPattern *sampler_pattern_at(Sampler *s, int i) { return &s->patterns[(i % SP_PATTERNS + SP_PATTERNS) % SP_PATTERNS]; }
void sampler_select_pattern(Sampler *s, int i) { s->selected = (i % SP_PATTERNS + SP_PATTERNS) % SP_PATTERNS; }
int sampler_selected_pattern(const Sampler *s) { return s->selected; }
void sampler_set_chain(Sampler *s, unsigned mask) { s->chain = mask & ((1u << SP_PATTERNS) - 1); }
unsigned sampler_chain(const Sampler *s) { return s->chain; }
int sampler_playing_pattern(const Sampler *s) { return s->playing ? s->playing_pat : -1; }
void sampler_set_tempo(Sampler *s, float bpm) { s->bpm = clampf(bpm, 40.0f, 240.0f); }
float sampler_tempo(const Sampler *s) { return s->bpm; }
void sampler_set_swing(Sampler *s, float swing) { s->swing = clampf(swing, 0.0f, 0.6f); }
float sampler_swing(const Sampler *s) { return s->swing; }
void sampler_set_metronome(Sampler *s, int on) { s->metronome = on != 0; }
int sampler_metronome(const Sampler *s) { return s->metronome; }

void sampler_play(Sampler *s, int on)
{
    if (on) { s->restart = 1; __atomic_store_n(&s->playing, 1, __ATOMIC_RELEASE); }
    else __atomic_store_n(&s->playing, 0, __ATOMIC_RELEASE);
}

void sampler_resume(Sampler *s) { __atomic_store_n(&s->playing, 1, __ATOMIC_RELEASE); }
int sampler_playing(const Sampler *s) { return s->playing; }
int sampler_current_step(const Sampler *s) { return s->playing ? __atomic_load_n(&s->step, __ATOMIC_ACQUIRE) : -1; }
void sampler_set_record(Sampler *s, int on) { s->record = on != 0; }
int sampler_record(const Sampler *s) { return s->record; }

/* ------------------------------------------------------------------ ingresso */
void sampler_capture_set_rate(Sampler *s, float rate) { s->cap_rate = rate > 0.0f ? rate : 0.0f; if (rate <= 0.0f) s->cap_level = 0.0f; }
float sampler_capture_rate(const Sampler *s) { return s->cap_rate; }
float sampler_capture_level(const Sampler *s) { return s->cap_level; }

static void rec_push(Sampler *s, float v)
{
    int n = s->rec_n;
    if (n >= SP_REC_FRAMES) { __atomic_store_n(&s->rec_state, REC_DONE, __ATOMIC_RELEASE); return; }
    s->rec_buf[n] = v;
    __atomic_store_n(&s->rec_n, n + 1, __ATOMIC_RELEASE);
}

void sampler_capture_feed(Sampler *s, const float *in, int frames)
{
    if (frames <= 0) return;
    float pk = 0.0f;
    for (int i = 0; i < frames; i++) { float a = fabsf(in[i]); if (a > pk) pk = a; }
    if (pk != pk) pk = 0.0f;
    s->cap_level = fmaxf(pk, s->cap_level * 0.7f);
    int st = __atomic_load_n(&s->rec_state, __ATOMIC_ACQUIRE);
    if (st == REC_WAIT && pk >= s->rec_threshold) { st = REC_ON; __atomic_store_n(&s->rec_state, REC_ON, __ATOMIC_RELEASE); }
    if (st != REC_ON) return;
    float rate = s->cap_rate > 0.0f ? s->cap_rate : SP_RATE;
    if (fabsf(rate - SP_RATE) < 1.0f) {
        for (int i = 0; i < frames && s->rec_state == REC_ON; i++) rec_push(s, in[i]);
        return;
    }
    double step = rate / SP_RATE;             /* ricampionamento lineare verso i 48 kHz */
    for (int i = 0; i < frames && s->rec_state == REC_ON; i++) {
        float x0 = s->cap_prev, x1 = in[i];
        while (s->cap_phase < 1.0) {
            rec_push(s, x0 + (x1 - x0) * (float)s->cap_phase);
            s->cap_phase += step;
        }
        s->cap_phase -= 1.0;
        s->cap_prev = x1;
    }
}

void sampler_rec_start(Sampler *s, float threshold)
{
    s->rec_n = 0;
    s->cap_phase = 0.0;
    s->cap_prev = 0.0f;
    s->rec_threshold = threshold;
    __atomic_store_n(&s->rec_state, threshold > 0.0f ? REC_WAIT : REC_ON, __ATOMIC_RELEASE);
}

void sampler_rec_stop(Sampler *s)
{
    int st = __atomic_load_n(&s->rec_state, __ATOMIC_ACQUIRE);
    if (st == REC_ON || st == REC_WAIT) __atomic_store_n(&s->rec_state, REC_DONE, __ATOMIC_RELEASE);
}

int sampler_rec_state(const Sampler *s) { return __atomic_load_n(&s->rec_state, __ATOMIC_ACQUIRE); }
int sampler_rec_frames(const Sampler *s) { return __atomic_load_n(&s->rec_n, __ATOMIC_ACQUIRE); }

const float *sampler_rec_take(Sampler *s, int *frames)
{
    if (__atomic_load_n(&s->rec_state, __ATOMIC_ACQUIRE) != REC_DONE) { *frames = 0; return NULL; }
    *frames = __atomic_load_n(&s->rec_n, __ATOMIC_ACQUIRE);
    __atomic_store_n(&s->rec_state, REC_IDLE, __ATOMIC_RELEASE);
    return s->rec_buf;
}

/* ------------------------------------------------------------------ per l'interfaccia */
void sampler_scope(const Sampler *s, float *dst, int n)
{
    int pos = __atomic_load_n(&s->scope_pos, __ATOMIC_ACQUIRE);
    if (n > SP_SCOPE) n = SP_SCOPE;
    for (int i = 0; i < n; i++) dst[i] = s->scope[(pos - n + i + SP_SCOPE) % SP_SCOPE];
}

float sampler_peak(const Sampler *s) { return s->peak; }
int sampler_active_voices(const Sampler *s) { return __atomic_load_n(&s->voices_n, __ATOMIC_ACQUIRE); }
int sampler_slot_voices(const Sampler *s, int slot) { return slot_ok(slot) ? __atomic_load_n(&s->slot_voices[slot], __ATOMIC_ACQUIRE) : 0; }
uint32_t sampler_slot_flash(const Sampler *s, int slot) { return slot_ok(slot) ? __atomic_load_n(&s->flash[slot], __ATOMIC_ACQUIRE) : 0; }
float sampler_slot_position(const Sampler *s, int slot) { return slot_ok(slot) ? s->slot_pos[slot] : -1.0f; }

/* ------------------------------------------------------------------ thread audio: voci */
static void voice_release(Sampler *s, Voice *v, float seconds)
{
    if (v->stage == ST_RELEASE) return;
    v->stage = ST_RELEASE;
    v->env_inc = 1.0f / fmaxf(1.0f, seconds * s->sr);
}

static Voice *voice_alloc(Sampler *s)
{
    Voice *best = NULL;
    for (int i = 0; i < SP_VOICES; i++) {
        Voice *v = &s->voices[i];
        if (!v->active) return v;
        if (!best || v->born < best->born) best = v;    /* la piu' vecchia viene rubata */
    }
    return best;
}

static void trigger_internal(Sampler *s, int slot, float vel, float semis, int key, int from_seq)
{
    const Sample *smp = s->slot_sample[slot];
    if (!smp || smp->frames < 2) return;
    const PadParams p = s->pads[slot];
    int loop = p.mode == MODE_LOOP;
    if (loop && key < 0 && !from_seq) {                  /* loop a colpi alterni: il secondo lo ferma */
        int found = 0;
        for (int i = 0; i < SP_VOICES; i++) {
            Voice *v = &s->voices[i];
            if (v->active && v->slot == slot && v->key < 0 && v->loop && v->stage != ST_RELEASE) { voice_release(s, v, 0.01f); found = 1; }
        }
        if (found) return;
    }
    int same = 0;
    Voice *oldest = NULL;
    for (int i = 0; i < SP_VOICES; i++) {
        Voice *v = &s->voices[i];
        if (!v->active || v->stage == ST_RELEASE) continue;
        if (p.group && v->group == p.group) { voice_release(s, v, 0.005f); continue; }      /* gruppo di esclusione */
        if (v->slot != slot) continue;
        if (key >= 0 && v->key == key) { voice_release(s, v, 0.005f); continue; }           /* stessa nota: una voce sola */
        if (loop && v->loop && v->key < 0 && from_seq) { voice_release(s, v, 0.005f); continue; }   /* il sequencer riavvia il loop */
        same++;
        if (!oldest || v->born < oldest->born) oldest = v;
    }
    if (same >= MAX_PER_SLOT && oldest) voice_release(s, oldest, 0.005f);
    Voice *v = voice_alloc(s);
    int s0 = (int)(clampf(p.start, 0.0f, 1.0f) * smp->frames), s1 = (int)(clampf(p.end, 0.0f, 1.0f) * smp->frames);
    if (s1 > smp->frames) s1 = smp->frames;
    if (s0 > smp->frames - 2) s0 = smp->frames - 2;
    if (s0 < 0) s0 = 0;
    if (s1 < s0 + 2) s1 = s0 + 2;
    memset(v, 0, sizeof(*v));
    v->active = 1;
    v->slot = slot;
    v->key = key;
    v->group = p.group;
    v->smp = smp;
    v->s0 = s0;
    v->s1 = s1;
    v->reverse = p.reverse != 0;
    v->loop = loop;
    v->hold = p.mode == MODE_HOLD || (loop && key >= 0);
    v->pos = v->reverse ? s1 - 1 : s0;
    v->inc = exp2((p.pitch + semis) / 12.0);
    v->gain = vel * clampf(p.volume, 0.0f, 1.5f);
    v->stage = ST_ATTACK;
    v->env_inc = 1.0f / fmaxf(1.0f, 0.002f * s->sr);
    v->born = ++s->born;
    s->flash[slot]++;
    /* registrazione dal vivo: il colpo va nel passo piu' vicino */
    if (s->record && s->playing && !from_seq && key < 0 && slot < SP_PADS && s->step >= 0 && s->playing_pat >= 0) {
        SeqPattern *pat = &s->patterns[s->playing_pat];
        int len = pat->length < 1 ? 1 : (pat->length > SP_STEPS ? SP_STEPS : pat->length);
        double phase = s->step_len > 0.0 ? 1.0 - s->countdown / s->step_len : 0.0;
        int target = s->step;
        if (phase >= 0.5) { target = (s->step + 1) % len; s->skip[slot] = target + 1; }
        int iv = (int)(vel * 127.0f + 0.5f);
        pat->vel[slot][target] = (uint8_t)(iv < 1 ? 1 : (iv > 127 ? 127 : iv));
    }
}

static void release_internal(Sampler *s, int slot, int key)
{
    for (int i = 0; i < SP_VOICES; i++) {
        Voice *v = &s->voices[i];
        if (!v->active || v->slot != slot || v->stage == ST_RELEASE) continue;
        if (key == -2) { voice_release(s, v, 0.01f); continue; }
        if (key != v->key || !v->hold) continue;
        if (s->sustain) v->pending = 1;
        else voice_release(s, v, 0.01f);
    }
}

static void apply(Sampler *s, const Event *e)
{
    switch (e->type) {
    case EV_ON: trigger_internal(s, e->slot, e->vel, e->semis, e->key, 0); break;
    case EV_OFF: release_internal(s, e->slot, e->key); break;
    case EV_ALL_OFF:
        for (int i = 0; i < SP_VOICES; i++) if (s->voices[i].active) voice_release(s, &s->voices[i], 0.01f);
        break;
    case EV_SET_SAMPLE: {
        for (int i = 0; i < SP_VOICES; i++) if (s->voices[i].active && s->voices[i].slot == e->slot) s->voices[i].active = 0;
        Sample *old = s->slot_sample[e->slot];
        s->slot_sample[e->slot] = e->smp;
        if (old) {
            int h = s->ret_head, n = (h + 1) % RETIRED;
            if (n != __atomic_load_n(&s->ret_tail, __ATOMIC_ACQUIRE)) { s->retired[h] = old; __atomic_store_n(&s->ret_head, n, __ATOMIC_RELEASE); }
            /* se la fila e' piena il campione resta in memoria: non si libera mai nel thread audio */
        }
        break;
    }
    case EV_SUSTAIN:
        s->sustain = e->key;
        if (!s->sustain)
            for (int i = 0; i < SP_VOICES; i++) if (s->voices[i].active && s->voices[i].pending) { s->voices[i].pending = 0; voice_release(s, &s->voices[i], 0.01f); }
        break;
    }
}

static void drain(Sampler *s)
{
    int t = s->q_tail;
    while (t != __atomic_load_n(&s->q_head, __ATOMIC_ACQUIRE)) {
        apply(s, &s->q[t]);
        t = (t + 1) & (Q_SIZE - 1);
        __atomic_store_n(&s->q_tail, t, __ATOMIC_RELEASE);
    }
}

/* Somma la voce nel buffer stereo; restituisce 0 quando la voce e' finita. */
static int render_voice(Sampler *s, Voice *v, float *out, int frames, double bend_mul)
{
    const Sample *smp = v->smp;
    const float *d = smp->data;
    const int ch = smp->channels, s0 = v->s0, s1 = v->s1;
    const double inc = v->inc * bend_mul;
    const float fade = (float)s->fade;
    double pos = v->pos;
    for (int i = 0; i < frames; i++) {
        if (v->stage == ST_ATTACK) {
            v->env += v->env_inc;
            if (v->env >= 1.0f) { v->env = 1.0f; v->stage = ST_SUSTAIN; }
        } else if (v->stage == ST_RELEASE) {
            v->env -= v->env_inc;
            if (v->env <= 0.0f) { v->active = 0; return 0; }
        }
        if (!v->reverse) {
            if (pos >= s1 - 1) {
                if (!v->loop) { v->active = 0; return 0; }
                pos -= (s1 - 1 - s0);
                if (pos < s0 || pos >= s1 - 1) pos = s0;
            }
        } else {
            if (pos <= s0) {
                if (!v->loop) { v->active = 0; return 0; }
                pos += (s1 - 1 - s0);
                if (pos <= s0 || pos > s1 - 1) pos = s1 - 1;
            }
        }
        int i0 = (int)pos;
        float fr = (float)(pos - i0);
        int i1 = i0 + 1 < s1 ? i0 + 1 : i0;
        float edge = 1.0f;
        if (!v->loop) {
            float dist = v->reverse ? (float)(pos - s0) : (float)(s1 - 1 - pos);
            if (dist < fade) edge = dist / fade;
        }
        float g = v->gain * v->env * edge;
        if (ch == 1) {
            float a = d[i0], b = d[i1];
            float x = (a + (b - a) * fr) * g;
            out[2 * i] += x;
            out[2 * i + 1] += x;
        } else {
            float l = d[2 * i0] + (d[2 * i1] - d[2 * i0]) * fr, r = d[2 * i0 + 1] + (d[2 * i1 + 1] - d[2 * i0 + 1]) * fr;
            out[2 * i] += l * g;
            out[2 * i + 1] += r * g;
        }
        pos += v->reverse ? -inc : inc;
    }
    v->pos = pos;
    return 1;
}

/* ------------------------------------------------------------------ thread audio: sequencer */
static void click(Sampler *s, int accent)
{
    s->click_amp = accent ? 0.45f : 0.3f;
    s->click_freq = accent ? 1760.0f : 1175.0f;
    s->click_phase = 0.0f;
}

static int pattern_length(const SeqPattern *p)
{
    int l = p->length;
    return l < 1 ? 1 : (l > SP_STEPS ? SP_STEPS : l);
}

static void seq_tick(Sampler *s)
{
    if (!__atomic_load_n(&s->playing, __ATOMIC_ACQUIRE)) return;
    if (s->restart) {
        s->restart = 0;
        s->step = -1;
        s->countdown = 0.0;
        s->playing_pat = -1;
        s->bars_done = 0;
        memset(s->skip, 0, sizeof(s->skip));
    }
    s->countdown -= 1.0;
    if (s->countdown > 0.0) return;
    int cur_pat = s->playing_pat >= 0 ? s->playing_pat : s->selected;
    int next = s->step + 1;
    if (next >= pattern_length(&s->patterns[cur_pat]) || next < 0 || s->playing_pat < 0) {
        /* nuova battuta: quale pattern suona */
        if (s->stop_after_bars && s->bars_done >= s->stop_after_bars) { s->playing = 0; return; }
        s->bars_done++;
        next = 0;
        if (s->chain) {
            int from = s->playing_pat;
            for (int i = 1; i <= SP_PATTERNS; i++) {
                int c = from < 0 ? i - 1 : (from + i) % SP_PATTERNS;
                if (s->chain & (1u << c)) { s->playing_pat = c; break; }
            }
        } else {
            s->playing_pat = s->selected;
        }
    }
    __atomic_store_n(&s->step, next, __ATOMIC_RELEASE);
    const SeqPattern *pat = &s->patterns[s->playing_pat];
    double base = s->sr * 60.0 / s->bpm / 4.0;
    double len = (next % 2 == 0) ? base * (1.0 + s->swing) : base * (1.0 - s->swing);
    s->countdown += len;
    s->step_len = len;
    if (s->metronome && next % 4 == 0) click(s, next == 0);
    int any_solo = 0;
    for (int p = 0; p < SP_PADS; p++) if (s->pads[p].solo) any_solo = 1;
    for (int p = 0; p < SP_PADS; p++) {
        int vel = pat->vel[p][next];
        if (s->skip[p] == next + 1) { s->skip[p] = 0; continue; }
        if (!vel) continue;
        if (s->pads[p].mute || (any_solo && !s->pads[p].solo)) continue;
        trigger_internal(s, p, vel / 127.0f, 0.0f, -1, 1);
    }
}

static inline float soft_clip(float x)
{
    if (x > 0.9f) { float d = x - 0.9f; return 0.9f + d / (1.0f + d * 10.0f); }
    if (x < -0.9f) { float d = -0.9f - x; return -0.9f - d / (1.0f + d * 10.0f); }
    return x;
}

void sampler_render(Sampler *s, float *out, int frames)
{
    drain(s);
    memset(out, 0, sizeof(float) * 2 * (size_t)frames);
    /* sequencer e metronomo, campione per campione (il tick e' leggero) */
    for (int i = 0; i < frames; i++) {
        seq_tick(s);
        if (s->click_amp > 0.0005f) {
            float v = sinf(s->click_phase) * s->click_amp;
            s->click_phase += 2.0f * PI_F * s->click_freq / s->sr;
            if (s->click_phase > 2.0f * PI_F) s->click_phase -= 2.0f * PI_F;
            s->click_amp *= 0.9968f;
            out[2 * i] += v;
            out[2 * i + 1] += v;
        } else s->click_amp = 0.0f;
    }
    /* voci */
    const double bend_mul = exp2(s->bend / 12.0);
    int counts[SP_SLOTS] = { 0 };
    int n_active = 0;
    for (int i = 0; i < SP_VOICES; i++) {
        Voice *v = &s->voices[i];
        if (!v->active) continue;
        if (render_voice(s, v, out, frames, bend_mul)) {
            n_active++;
            counts[v->slot]++;
            s->slot_pos[v->slot] = (float)((v->pos - v->s0) / (double)(v->s1 - v->s0));
        }
    }
    for (int k = 0; k < SP_SLOTS; k++) {
        if (!counts[k]) s->slot_pos[k] = -1.0f;
        __atomic_store_n(&s->slot_voices[k], counts[k], __ATOMIC_RELEASE);
    }
    /* volume, filtro passa-basso (due poli), soft clip, oscilloscopio */
    const float vol = s->volume, cut = s->cutoff;
    const int filter = cut < 0.995f;
    const float fc = 60.0f * powf(300.0f, cut);                  /* 60 Hz .. 18 kHz */
    const float g = 1.0f - expf(-2.0f * PI_F * fc / s->sr);
    float block_peak = 0.0f;
    int pos = s->scope_pos;
    for (int i = 0; i < frames; i++) {
        float l = out[2 * i] * vol, r = out[2 * i + 1] * vol;
        if (filter) {
            s->lp1[0] += g * (l - s->lp1[0]); s->lp2[0] += g * (s->lp1[0] - s->lp2[0]); l = s->lp2[0];
            s->lp1[1] += g * (r - s->lp1[1]); s->lp2[1] += g * (s->lp1[1] - s->lp2[1]); r = s->lp2[1];
        } else {
            s->lp1[0] = s->lp2[0] = l;
            s->lp1[1] = s->lp2[1] = r;
        }
        l = soft_clip(l);
        r = soft_clip(r);
        out[2 * i] = l;
        out[2 * i + 1] = r;
        float m = 0.5f * (l + r);
        s->scope[pos] = m;
        pos = (pos + 1) % SP_SCOPE;
        if (fabsf(m) > block_peak) block_peak = fabsf(m);
    }
    __atomic_store_n(&s->scope_pos, pos, __ATOMIC_RELEASE);
    s->peak = fmaxf(s->peak * 0.85f, block_peak);
    __atomic_store_n(&s->voices_n, n_active, __ATOMIC_RELEASE);
}

/* ------------------------------------------------------------------ esportazione */
float *sampler_render_pattern(Sampler *src, int *frames_out)
{
    *frames_out = 0;
    Sampler *c = sampler_create(src->sr);
    if (!c) return NULL;
    c->borrowed = 1;
    for (int i = 0; i < SP_SLOTS; i++) { c->slot_sample[i] = src->ui_sample[i]; c->pads[i] = src->pads[i]; }
    memcpy(c->patterns, src->patterns, sizeof(c->patterns));
    c->selected = src->selected;
    c->chain = src->chain;
    c->bpm = src->bpm;
    c->swing = src->swing;
    c->volume = src->volume;
    int bars = 0;
    for (int p = 0; p < SP_PATTERNS; p++) if (src->chain & (1u << p)) bars++;
    if (!bars) bars = 1;
    c->stop_after_bars = bars;
    /* c'e' qualcosa da suonare? */
    int any = 0;
    for (int p = 0; p < SP_PATTERNS; p++) {
        if (src->chain && !(src->chain & (1u << p))) continue;
        if (!src->chain && p != src->selected) continue;
        for (int k = 0; k < SP_PADS && !any; k++)
            for (int i = 0; i < pattern_length(&c->patterns[p]); i++)
                if (c->patterns[p].vel[k][i] && c->slot_sample[k]) { any = 1; break; }
    }
    if (!any) { sampler_destroy(c); return NULL; }
    double step = c->sr * 60.0 / c->bpm / 4.0;
    int cap = (int)(step * SP_STEPS * (1.0 + c->swing) * bars) + (int)(c->sr * 2.5f) + 4096;
    float *buf = malloc(sizeof(float) * 2 * (size_t)cap);
    if (!buf) { sampler_destroy(c); return NULL; }
    sampler_play(c, 1);
    int n = 0, tail = 0, tail_max = (int)(c->sr * 2.0f);
    while (n + 1024 <= cap) {
        sampler_render(c, buf + 2 * n, 1024);
        n += 1024;
        if (!c->playing) {
            tail += 1024;
            if (c->voices_n == 0 || tail >= tail_max) break;
        }
    }
    sampler_destroy(c);
    *frames_out = n;
    return buf;
}
