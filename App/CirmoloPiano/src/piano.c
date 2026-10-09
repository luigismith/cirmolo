/* Cirmolo Piano - motore General MIDI su TinySoundFont (vedi piano.h). */
#include "piano.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* tsf.h calcola gli offset dei campi sottraendo puntatori nulli: clang lo segnala con -Wextra, ma e'
   codice di terzi che non tocchiamo. */
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnull-pointer-subtraction"
#endif
#define TSF_IMPLEMENTATION
#define TSF_STATIC
#include "third_party/tsf.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif

/* ------------------------------------------------------------------ coda degli eventi */
enum { EV_NOTE_ON, EV_NOTE_OFF, EV_CC, EV_PRESET, EV_ALL_OFF, EV_SOUNDS_OFF };

typedef struct { uint8_t type, ch; int a, b; float f; } Event;

#define Q_SIZE 512                      /* potenza di due */
#define CHUNK 64                        /* campioni fra un aggiornamento del vibrato e l'altro */

struct Piano {
    tsf *f;
    float sr;
    int loaded;
    /* coda: scrive l'interfaccia (q_head), legge il thread audio (q_tail) */
    Event q[Q_SIZE];
    int q_head, q_tail;
    /* parametri condivisi: scritti dall'interfaccia, letti dal thread audio (allineati: atomici) */
    float volume;
    float mod[PIANO_CHANNELS];          /* modulazione 0..1 -> vibrato */
    int bend[PIANO_CHANNELS];           /* pitch bend 0..16383 */
    /* ombra dei canali per l'interfaccia */
    int ch_preset[PIANO_CHANNELS];
    int ch_bank[PIANO_CHANNELS];        /* come in tsf: bit 15 = scelto con CC0 (MSB) */
    /* stato del thread audio */
    float vib_phase;
    float scope[PIANO_SCOPE];
    int scope_pos;
    float peak;
    int voices;
};

static void push(Piano *p, Event e)
{
    int h = p->q_head, n = (h + 1) & (Q_SIZE - 1);
    if (n == __atomic_load_n(&p->q_tail, __ATOMIC_ACQUIRE)) return;   /* coda piena: si perde l'evento */
    p->q[h] = e;
    __atomic_store_n(&p->q_head, n, __ATOMIC_RELEASE);
}

/* ------------------------------------------------------------------ creazione */
Piano *piano_create(float sample_rate)
{
    Piano *p = calloc(1, sizeof(Piano));
    if (!p) return NULL;
    p->sr = sample_rate > 8000.0f ? sample_rate : 48000.0f;
    p->volume = 0.8f;
    for (int c = 0; c < PIANO_CHANNELS; c++) p->bend[c] = 8192;
    return p;
}

int piano_load(Piano *p, const char *sf2_path)
{
    if (!p || !sf2_path) return -1;
    tsf *f = tsf_load_filename(sf2_path);
    if (!f) return -1;
    if (p->f) tsf_close(p->f);
    p->f = f;
    tsf_set_output(f, TSF_STEREO_INTERLEAVED, (int)p->sr, 0.0f);
    tsf_set_max_voices(f, PIANO_VOICES);
    /* tutti i canali esistono da subito: il thread audio non deve allocare */
    for (int c = 0; c < PIANO_CHANNELS; c++) {
        tsf_channel_set_bank_preset(f, c, 0, 0);
        p->ch_preset[c] = tsf_channel_get_preset_index(f, c);
        p->ch_bank[c] = 0;
    }
    int drums = tsf_get_presetindex(f, 128, 0);
    if (drums >= 0) {
        tsf_channel_set_presetindex(f, PIANO_DRUM_CHANNEL, drums);
        p->ch_preset[PIANO_DRUM_CHANNEL] = drums;
    }
    p->loaded = 1;
    return 0;
}

int piano_loaded(const Piano *p) { return p && p->loaded; }

void piano_destroy(Piano *p)
{
    if (!p) return;
    if (p->f) tsf_close(p->f);
    free(p);
}

float piano_sample_rate(const Piano *p) { return p->sr; }

/* ------------------------------------------------------------------ interfaccia -> coda */
static int chan_ok(int ch) { return ch >= 0 && ch < PIANO_CHANNELS; }

void piano_note_on(Piano *p, int ch, int key, float vel)
{
    if (!p->loaded || !chan_ok(ch) || key < 0 || key > 127) return;
    if (vel <= 0.0f) { piano_note_off(p, ch, key); return; }
    push(p, (Event){ EV_NOTE_ON, (uint8_t)ch, key, 0, vel > 1.0f ? 1.0f : vel });
}

void piano_note_off(Piano *p, int ch, int key)
{
    if (!p->loaded || !chan_ok(ch) || key < 0 || key > 127) return;
    push(p, (Event){ EV_NOTE_OFF, (uint8_t)ch, key, 0, 0.0f });
}

void piano_control(Piano *p, int ch, int cc, int value)
{
    if (!p->loaded || !chan_ok(ch) || cc < 0 || cc > 127) return;
    if (value < 0) value = 0;
    if (value > 127) value = 127;
    if (cc == 1) { p->mod[ch] = value / 127.0f; return; }           /* modulazione: vibrato nostro */
    if (cc == 0) p->ch_bank[ch] = 0x8000 | value;                    /* ombra del banco, come tsf */
    else if (cc == 32) p->ch_bank[ch] = ((p->ch_bank[ch] & 0x8000) ? ((p->ch_bank[ch] & 0x7F) << 7) : 0) | value;
    else if (cc == 121) { p->ch_bank[ch] = 0; p->mod[ch] = 0.0f; p->bend[ch] = 8192; }
    push(p, (Event){ EV_CC, (uint8_t)ch, cc, value, 0.0f });
}

void piano_pitch_bend(Piano *p, int ch, int value14)
{
    if (!chan_ok(ch)) return;
    p->bend[ch] = value14 < 0 ? 0 : (value14 > 16383 ? 16383 : value14);
}

void piano_set_preset(Piano *p, int ch, int index)
{
    if (!p->loaded || !chan_ok(ch) || index < 0 || index >= p->f->presetNum) return;
    p->ch_preset[ch] = index;
    push(p, (Event){ EV_PRESET, (uint8_t)ch, index, 0, 0.0f });
}

/* Stessa ricerca di tsf_channel_set_presetnumber, fatta qui per sapere subito quale preset risulta. */
void piano_program(Piano *p, int ch, int program)
{
    if (!p->loaded || !chan_ok(ch)) return;
    program &= 127;
    int bank = p->ch_bank[ch] & 0x7FFF, idx = -1;
    if (ch == PIANO_DRUM_CHANNEL) {
        idx = tsf_get_presetindex(p->f, 128 | bank, program);
        if (idx < 0) idx = tsf_get_presetindex(p->f, 128, program);
        if (idx < 0) idx = tsf_get_presetindex(p->f, 128, 0);
    }
    if (idx < 0) idx = tsf_get_presetindex(p->f, bank, program);
    if (idx < 0) idx = tsf_get_presetindex(p->f, 0, program);
    if (idx >= 0) piano_set_preset(p, ch, idx);
}

void piano_all_notes_off(Piano *p)
{
    if (!p->loaded) return;
    push(p, (Event){ EV_ALL_OFF, 0, 0, 0, 0.0f });
}

void piano_all_sounds_off(Piano *p)
{
    if (!p->loaded) return;
    push(p, (Event){ EV_SOUNDS_OFF, 0, 0, 0, 0.0f });
}

void  piano_set_volume(Piano *p, float v) { p->volume = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
float piano_volume(const Piano *p) { return p->volume; }

/* ------------------------------------------------------------------ preset */
int piano_preset_count(const Piano *p) { return p->loaded ? p->f->presetNum : 0; }

const char *piano_preset_name(const Piano *p, int i)
{
    if (!p->loaded || i < 0 || i >= p->f->presetNum) return "";
    return p->f->presets[i].presetName;
}

int piano_preset_bank(const Piano *p, int i) { return (p->loaded && i >= 0 && i < p->f->presetNum) ? p->f->presets[i].bank : 0; }
int piano_preset_program(const Piano *p, int i) { return (p->loaded && i >= 0 && i < p->f->presetNum) ? p->f->presets[i].preset : 0; }
int piano_find_preset(const Piano *p, int bank, int program) { return p->loaded ? tsf_get_presetindex(p->f, bank, program) : -1; }
int piano_channel_preset(const Piano *p, int ch) { return chan_ok(ch) ? p->ch_preset[ch] : 0; }

/* ------------------------------------------------------------------ per l'interfaccia */
int piano_active_voices(const Piano *p) { return __atomic_load_n(&p->voices, __ATOMIC_ACQUIRE); }

void piano_scope(const Piano *p, float *dst, int n)
{
    int pos = __atomic_load_n(&p->scope_pos, __ATOMIC_ACQUIRE);
    if (n > PIANO_SCOPE) n = PIANO_SCOPE;
    for (int i = 0; i < n; i++) dst[i] = p->scope[(pos - n + i + PIANO_SCOPE) % PIANO_SCOPE];
}

float piano_peak(const Piano *p) { return p->peak; }

/* ------------------------------------------------------------------ thread audio */
static void apply(Piano *p, const Event *e)
{
    tsf *f = p->f;
    switch (e->type) {
    case EV_NOTE_ON: tsf_channel_note_on(f, e->ch, e->a, e->f); break;
    case EV_NOTE_OFF: tsf_channel_note_off(f, e->ch, e->a); break;
    case EV_CC: tsf_channel_midi_control(f, e->ch, e->a, e->b); break;
    case EV_PRESET: tsf_channel_set_presetindex(f, e->ch, e->a); break;
    case EV_ALL_OFF: for (int c = 0; c < PIANO_CHANNELS; c++) tsf_channel_note_off_all(f, c); break;
    case EV_SOUNDS_OFF: for (int c = 0; c < PIANO_CHANNELS; c++) tsf_channel_sounds_off_all(f, c); break;
    }
}

static void drain(Piano *p)
{
    int t = p->q_tail;
    while (t != __atomic_load_n(&p->q_head, __ATOMIC_ACQUIRE)) {
        apply(p, &p->q[t]);
        t = (t + 1) & (Q_SIZE - 1);
        __atomic_store_n(&p->q_tail, t, __ATOMIC_RELEASE);
    }
}

/* Vibrato della rotella di modulazione: un LFO a 5,5 Hz sul pitch bend, fino a mezzo semitono
   (l'intervallo del bend e' 2 semitoni, 4096 passi per semitono). */
static void vibrato(Piano *p, int frames)
{
    float lfo = sinf(p->vib_phase);
    p->vib_phase += 2.0f * 3.14159265f * 5.5f * frames / p->sr;
    if (p->vib_phase > 2.0f * 3.14159265f) p->vib_phase -= 2.0f * 3.14159265f;
    for (int c = 0; c < PIANO_CHANNELS; c++) {
        float m = p->mod[c];
        int w = p->bend[c] + (m > 0.001f ? (int)(lfo * m * 2048.0f) : 0);
        tsf_channel_set_pitchwheel(p->f, c, w < 0 ? 0 : (w > 16383 ? 16383 : w));   /* non fa niente se uguale */
    }
}

static inline float soft_clip(float x)
{
    if (x > 0.9f) { float d = x - 0.9f; return 0.9f + d / (1.0f + d * 10.0f); }
    if (x < -0.9f) { float d = -0.9f - x; return -0.9f - d / (1.0f + d * 10.0f); }
    return x;
}

void piano_render(Piano *p, float *out, int frames)
{
    if (!p->loaded) { memset(out, 0, sizeof(float) * 2 * (size_t)frames); return; }
    drain(p);
    tsf_set_volume(p->f, p->volume);
    float block_peak = 0.0f;
    int pos = p->scope_pos;
    for (int done = 0; done < frames; done += CHUNK) {
        int n = frames - done < CHUNK ? frames - done : CHUNK;
        vibrato(p, n);
        float *o = out + 2 * done;
        tsf_render_float(p->f, o, n, 0);
        for (int i = 0; i < n; i++) {
            float l = soft_clip(o[2 * i]), r = soft_clip(o[2 * i + 1]), m = 0.5f * (l + r);
            o[2 * i] = l;
            o[2 * i + 1] = r;
            p->scope[pos] = m;
            pos = (pos + 1) % PIANO_SCOPE;
            if (fabsf(m) > block_peak) block_peak = fabsf(m);
        }
    }
    __atomic_store_n(&p->scope_pos, pos, __ATOMIC_RELEASE);
    p->peak = fmaxf(p->peak * 0.85f, block_peak);
    __atomic_store_n(&p->voices, tsf_active_voice_count(p->f), __ATOMIC_RELEASE);
}
