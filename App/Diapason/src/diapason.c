/* Diapason - accordatore, note di riferimento e metronomo per la Miyoo Flip (Cirmolo).
 *
 * Accordatore: algoritmo YIN (de Cheveigne' e Kawahara, 2002) sul segnale del microfono ridotto a meta'
 * frequenza, da 40 a 1500 Hz. La Flip non ha un microfono interno: serve un microfono USB-C o un
 * adattatore USB-C con ingresso microfono (il kit lo apre da solo appena viene collegato).
 * Senza microfono restano la nota di riferimento (per accordare a orecchio) e il metronomo.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diapason.h"
#include "gfx.h"
#include "platform.h"

#define PI_F 3.14159265358979f
#define TWO_PI_F 6.28318530717959f

/* ------------------------------------------------------------------ palette (come Cirmolo Synth) */
#define C_BG_TOP   RGB(13, 16, 22)
#define C_BG_BOT   RGB(18, 23, 32)
#define C_PANEL    RGB(25, 31, 42)
#define C_PANEL_HI RGB(34, 42, 57)
#define C_LINE     RGB(48, 58, 78)
#define C_TEXT     RGB(236, 236, 244)
#define C_MUTED    RGB(138, 148, 170)
#define C_DIM      RGB(88, 98, 120)
#define C_VIOLET   RGB(150, 116, 242)
#define C_VIOLET_D RGB(84, 60, 160)
#define C_GREEN    RGB(92, 190, 126)
#define C_AMBER    RGB(244, 186, 92)
#define C_RED      RGB(236, 96, 104)

static const char *NOTE_NAMES[12] = { "Do", "Do#", "Re", "Re#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" };

typedef struct { const char *name; int n; int notes[7]; } Instrument;
static const Instrument INSTRUMENTS[] = {
    { "Cromatico", 0, { 0 } },
    { "Chitarra", 6, { 40, 45, 50, 55, 59, 64 } },
    { "Chitarra drop D", 6, { 38, 45, 50, 55, 59, 64 } },
    { "Basso", 4, { 28, 33, 38, 43 } },
    { "Ukulele", 4, { 67, 60, 64, 69 } },
    { "Violino", 4, { 55, 62, 69, 76 } },
};
#define INSTRUMENT_COUNT ((int)(sizeof(INSTRUMENTS) / sizeof(INSTRUMENTS[0])))

/* ------------------------------------------------------------------ stato */
#define RING 16384               /* campioni del microfono tenuti (a 48 kHz: 0,34 s) */
#define YIN_W 1024               /* finestra di analisi (a frequenza dimezzata) */
#define YIN_TAU_MAX 640          /* 24000 / 640 = 37,5 Hz */
#define YIN_TAU_MIN 15           /* 24000 / 15 = 1600 Hz */
#define HISTORY 5

enum { PAGE_TUNER, PAGE_METRO, PAGE_COUNT };

struct Diapason {
    float sr;
    char path[512];
    int page, quit_dialog, quit;
    int down[PAD_COUNT];
    float rep[PAD_COUNT];
    float time;

    /* microfono */
    float ring[RING];
    uint32_t ring_w;
    float cap_rate;
    int mic;
    char mic_name[96];

    /* accordatore */
    float a4;
    int instrument, sel;         /* corda (o nota, in modo cromatico) scelta per la nota di riferimento */
    int chroma_note;             /* nota MIDI di riferimento nel modo cromatico */
    float freq, cents, needle, level, conf;
    int note;                    /* nota rilevata (MIDI), -1 se non c'e' segnale */
    float hist[HISTORY];
    int hist_n;
    float hold;                  /* secondi da cui si tiene l'ultima nota rilevata */
    float buf[2 * (YIN_W + YIN_TAU_MAX) + 8], dec[YIN_W + YIN_TAU_MAX + 4], d[YIN_TAU_MAX + 2];

    /* nota di riferimento (thread audio) */
    int ref_on;
    float ref_freq, ref_phase, ref_env, ref_built, ref_amp[49];
    int ref_n;

    /* metronomo (thread audio) */
    int metro_on;
    float bpm;
    int beats, subdiv, accent;
    double tick_count;           /* campioni fino al prossimo colpo */
    int tick_index;              /* colpo corrente dentro la battuta (in suddivisioni) */
    int beat_now;                /* per l'interfaccia */
    uint32_t beat_serial;
    float click_t, click_freq, click_amp, click_phase;
    float taps[4];
    int tap_n;
    float last_tap;
    uint32_t beat_seen;
    float since_beat;            /* secondi dall'ultimo battito (per il pendolo) */
};

/* ------------------------------------------------------------------ utilita' */
static float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static float midi_hz(const Diapason *d, float note) { return d->a4 * exp2f((note - 69.0f) / 12.0f); }

static void note_name(char *out, size_t n, int midi)
{
    snprintf(out, n, "%s%d", NOTE_NAMES[((midi % 12) + 12) % 12], midi / 12 - 1);
}

static int ref_note(const Diapason *d)
{
    const Instrument *in = &INSTRUMENTS[d->instrument];
    return in->n ? in->notes[d->sel % in->n] : d->chroma_note;
}

static const char *tempo_name(float bpm)
{
    if (bpm < 40) return "Grave";
    if (bpm < 60) return "Largo";
    if (bpm < 66) return "Larghetto";
    if (bpm < 76) return "Adagio";
    if (bpm < 108) return "Andante";
    if (bpm < 120) return "Moderato";
    if (bpm < 168) return "Allegro";
    if (bpm < 200) return "Presto";
    return "Prestissimo";
}

/* ------------------------------------------------------------------ salvataggio */
static void save(Diapason *d)
{
    if (!d->path[0]) return;
    FILE *f = fopen(d->path, "w");
    if (!f) return;
    fprintf(f, "# Diapason\na4=%g\ninstrument=%d\nsel=%d\nchroma=%d\nbpm=%g\nbeats=%d\nsubdiv=%d\naccent=%d\n",
            d->a4, d->instrument, d->sel, d->chroma_note, d->bpm, d->beats, d->subdiv, d->accent);
    fclose(f);
}

static void load(Diapason *d)
{
    FILE *f = fopen(d->path, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        float v = (float)atof(eq + 1);
        if (!strcmp(line, "a4")) d->a4 = clampf(v, 400, 480);
        else if (!strcmp(line, "instrument")) d->instrument = (int)clampf(v, 0, INSTRUMENT_COUNT - 1);
        else if (!strcmp(line, "sel")) d->sel = (int)clampf(v, 0, 6);
        else if (!strcmp(line, "chroma")) d->chroma_note = (int)clampf(v, 24, 96);
        else if (!strcmp(line, "bpm")) d->bpm = clampf(v, 20, 300);
        else if (!strcmp(line, "beats")) d->beats = (int)clampf(v, 1, 12);
        else if (!strcmp(line, "subdiv")) d->subdiv = (int)clampf(v, 1, 4);
        else if (!strcmp(line, "accent")) d->accent = v != 0;
    }
    fclose(f);
}

/* ------------------------------------------------------------------ API */
Diapason *diapason_create(float sample_rate, const char *state_path)
{
    Diapason *d = calloc(1, sizeof(Diapason));
    if (!d) return NULL;
    d->sr = sample_rate;
    if (state_path) snprintf(d->path, sizeof(d->path), "%s", state_path);
    d->a4 = 440.0f;
    d->instrument = 1;
    d->chroma_note = 69;
    d->bpm = 90.0f;
    d->beats = 4;
    d->subdiv = 1;
    d->accent = 1;
    d->note = -1;
    d->cap_rate = 48000.0f;
    load(d);
    return d;
}

void diapason_destroy(Diapason *d)
{
    if (!d) return;
    save(d);
    free(d);
}

void diapason_capture(Diapason *d, const float *in, int frames)
{
    uint32_t w = d->ring_w;
    for (int i = 0; i < frames; i++) d->ring[(w + (uint32_t)i) % RING] = in[i];
    __atomic_store_n(&d->ring_w, w + (uint32_t)frames, __ATOMIC_RELEASE);
}

void diapason_capture_status(Diapason *d, const char *device, float rate)
{
    d->mic = device != NULL;
    if (device) { snprintf(d->mic_name, sizeof(d->mic_name), "%s", device); d->cap_rate = rate; }
    else d->mic_name[0] = 0;
    d->note = -1;
    d->hist_n = 0;
}

/* Thread audio: nota di riferimento e metronomo. */
void diapason_audio(Diapason *d, float *out, int frames)
{
    const float sr = d->sr, inv = 1.0f / sr;
    const float target = d->ref_on ? 1.0f : 0.0f;
    const float env_step = 1.0f / (0.02f * sr);
    for (int i = 0; i < frames; i++) {
        float s = 0.0f;
        /* nota di riferimento: sinusoide con un po' di seconda e terza armonica, attacco e stacco morbidi */
        if (d->ref_env < target) d->ref_env = fminf(target, d->ref_env + env_step);
        else if (d->ref_env > target) d->ref_env = fmaxf(target, d->ref_env - env_step);
        if (d->ref_env > 0.0f) {
            /* L'altoparlante della Flip non emette quasi nulla sotto i 250-300 Hz: una sinusoide a 82 Hz
               (Mi della chitarra) non si sentirebbe. La nota e' una somma di armoniche col peso spostato
               dove l'altoparlante suona (250-2500 Hz): l'orecchio ricostruisce comunque l'altezza della
               fondamentale. Ampiezze calcolate quando cambia la nota; armoniche con la ricorrenza
               sin(kx) = 2 cos(x) sin((k-1)x) - sin((k-2)x). */
            float f = d->ref_freq;
            if (f != d->ref_built) {
                d->ref_built = f;
                int n = (int)(6000.0f / f);
                if (n > 48) n = 48;
                if (n < 1) n = 1;
                float sum2 = 0.0f;
                for (int k = 1; k <= n; k++) {
                    float fk = f * k;
                    float w = (fk < 250.0f ? 0.3f : 1.0f) / (1.0f + (fk / 2500.0f) * (fk / 2500.0f)) / sqrtf((float)k);
                    d->ref_amp[k] = w;
                    sum2 += w * w;
                }
                float norm = 0.24f / sqrtf(0.5f * sum2);      /* valore efficace costante per ogni nota */
                for (int k = 1; k <= n; k++) d->ref_amp[k] *= norm;
                d->ref_n = n;
            }
            d->ref_phase += f * inv;
            if (d->ref_phase >= 1.0f) d->ref_phase -= 1.0f;
            float x = TWO_PI_F * d->ref_phase;
            /* fasi a quarti di giro con andamento quadratico (come le fasi di Schroeder): stesso suono,
               ma le armoniche non si sommano tutte nello stesso istante, quindi niente picchi che saturano */
            float cx = cosf(x), c2 = 2.0f * cx;
            float sp = 0.0f, sc = sinf(x), cp = 1.0f, cc = cx, acc = 0.0f;
            for (int k = 1; k <= d->ref_n; k++) {
                float v;
                switch ((k * (k - 1) / 2) & 3) {
                case 0: v = sc; break;
                case 1: v = cc; break;
                case 2: v = -sc; break;
                default: v = -cc;
                }
                acc += d->ref_amp[k] * v;
                float sn = c2 * sc - sp, cn = c2 * cc - cp;
                sp = sc; sc = sn;
                cp = cc; cc = cn;
            }
            s += acc * d->ref_env;
        }
        /* metronomo: colpi sintetizzati (accento piu' acuto, suddivisioni piu' piane) */
        if (d->metro_on) {
            d->tick_count -= 1.0;
            if (d->tick_count <= 0.0) {
                int per_bar = d->beats * d->subdiv;
                int idx = d->tick_index % per_bar;
                int on_beat = idx % d->subdiv == 0;
                int first = idx == 0;
                d->click_t = 0.0f;
                d->click_phase = 0.0f;
                d->click_freq = first && d->accent ? 2100.0f : (on_beat ? 1500.0f : 1100.0f);
                d->click_amp = first && d->accent ? 0.9f : (on_beat ? 0.65f : 0.35f);
                if (on_beat) { d->beat_now = idx / d->subdiv; __atomic_add_fetch(&d->beat_serial, 1, __ATOMIC_RELEASE); }
                d->tick_index = (idx + 1) % per_bar;
                d->tick_count += sr * 60.0 / d->bpm / d->subdiv;
            }
        }
        if (d->click_amp > 0.0f) {
            d->click_phase += d->click_freq * inv;
            if (d->click_phase >= 1.0f) d->click_phase -= 1.0f;
            float e = expf(-d->click_t / 0.012f);
            s += sinf(TWO_PI_F * d->click_phase) * e * d->click_amp;
            d->click_t += inv;
            if (d->click_t > 0.08f) d->click_amp = 0.0f;
        }
        s = s > 1.0f ? 1.0f : (s < -1.0f ? -1.0f : s);
        out[2 * i] = s;
        out[2 * i + 1] = s;
    }
}

/* YIN su x (n = YIN_W + YIN_TAU_MAX campioni) a frequenza sr: frequenza fondamentale o 0. */
static float yin(Diapason *d, const float *x, float sr, float *confidence)
{
    float *df = d->d;
    df[0] = 1.0f;
    float running = 0.0f;
    int found = -1;
    for (int tau = 1; tau <= YIN_TAU_MAX; tau++) {
        float sum = 0.0f;
        for (int j = 0; j < YIN_W; j++) {
            float diff = x[j] - x[j + tau];
            sum += diff * diff;
        }
        running += sum;
        df[tau] = running > 0.0f ? sum * tau / running : 1.0f;
        if (found < 0 && tau >= YIN_TAU_MIN && tau > 2 && df[tau - 1] < 0.12f && df[tau] > df[tau - 1]) found = tau - 1;
    }
    if (found < 0) {
        /* niente sotto la soglia: il minimo globale, se e' abbastanza netto */
        float best = 1.0f;
        for (int tau = YIN_TAU_MIN; tau < YIN_TAU_MAX; tau++) if (df[tau] < best) { best = df[tau]; found = tau; }
        if (best > 0.3f) { *confidence = 0.0f; return 0.0f; }
    }
    *confidence = 1.0f - df[found];
    float t = (float)found;
    if (found > 1 && found < YIN_TAU_MAX) {          /* interpolazione parabolica */
        float a = df[found - 1], b = df[found], c = df[found + 1];
        float den = a - 2.0f * b + c;
        if (fabsf(den) > 1e-9f) t += 0.5f * (a - c) / den;
    }
    return sr / t;
}

static void analyze(Diapason *d, float dt)
{
    d->hold += dt;
    if (!d->mic) return;
    int n = YIN_W + YIN_TAU_MAX;
    int raw = 2 * n;
    uint32_t w = __atomic_load_n(&d->ring_w, __ATOMIC_ACQUIRE);
    if (w < (uint32_t)raw) return;
    for (int i = 0; i < raw; i++) d->buf[i] = d->ring[(w - (uint32_t)raw + (uint32_t)i) % RING];
    /* meta' frequenza: media di coppie (basta come filtro per il campo 40-1500 Hz) */
    float mean = 0.0f;
    for (int i = 0; i < n; i++) { d->dec[i] = 0.5f * (d->buf[2 * i] + d->buf[2 * i + 1]); mean += d->dec[i]; }
    mean /= n;
    float rms = 0.0f;
    for (int i = 0; i < n; i++) { d->dec[i] -= mean; rms += d->dec[i] * d->dec[i]; }
    rms = sqrtf(rms / n);
    d->level = d->level * 0.7f + rms * 0.3f;
    if (rms < 0.004f) {                              /* silenzio */
        if (d->hold > 1.2f) { d->note = -1; d->hist_n = 0; }
        return;
    }
    float conf = 0.0f;
    float f = yin(d, d->dec, d->cap_rate * 0.5f, &conf);
    if (f < 35.0f || f > 1700.0f || conf < 0.6f) return;
    {   /* affinamento a piena frequenza: differenza attorno al periodo stimato, minimo per interpolazione */
        float tf = d->cap_rate / f;
        int t0 = (int)tf;
        if (t0 >= 3 && t0 + 3 + YIN_W < raw) {
            float v[7];
            for (int k = -3; k <= 3; k++) {
                float sum = 0.0f;
                for (int j = 0; j < YIN_W; j++) { float diff = d->buf[j] - d->buf[j + t0 + k]; sum += diff * diff; }
                v[k + 3] = sum;
            }
            int m = 3;
            for (int k = 1; k < 6; k++) if (v[k] < v[m]) m = k;
            if (m > 0 && m < 6) {
                float den = v[m - 1] - 2.0f * v[m] + v[m + 1];
                float t = (float)(t0 + m - 3) + (fabsf(den) > 1e-12f ? 0.5f * (v[m - 1] - v[m + 1]) / den : 0.0f);
                if (t > 2.0f && fabsf(t - tf) < 3.0f) f = d->cap_rate / t;
            }
        }
    }
    /* mediana delle ultime stime: toglie i salti d'ottava isolati */
    if (d->hist_n < HISTORY) d->hist[d->hist_n++] = f;
    else { memmove(d->hist, d->hist + 1, sizeof(float) * (HISTORY - 1)); d->hist[HISTORY - 1] = f; }
    float s[HISTORY];
    memcpy(s, d->hist, sizeof(float) * (size_t)d->hist_n);
    for (int i = 1; i < d->hist_n; i++) for (int j = i; j > 0 && s[j - 1] > s[j]; j--) { float t = s[j]; s[j] = s[j - 1]; s[j - 1] = t; }
    float med = s[d->hist_n / 2];
    d->freq = med;
    d->conf = conf;
    float note_f = 69.0f + 12.0f * log2f(med / d->a4);
    const Instrument *in = &INSTRUMENTS[d->instrument];
    int target = (int)lrintf(note_f);
    if (in->n) {                                     /* con uno strumento: la corda piu' vicina */
        float best = 1e9f;
        for (int i = 0; i < in->n; i++) {
            float dist = fabsf(note_f - in->notes[i]);
            if (dist < best) { best = dist; target = in->notes[i]; }
        }
    }
    d->note = target;
    d->cents = clampf((note_f - target) * 100.0f, -99.0f, 99.0f);
    d->hold = 0.0f;
}

/* ------------------------------------------------------------------ input */
static void set_ref(Diapason *d) { d->ref_freq = midi_hz(d, (float)ref_note(d)); }

static void tap_tempo(Diapason *d)
{
    float now = d->time;
    if (d->tap_n > 0 && now - d->last_tap > 2.0f) d->tap_n = 0;
    if (d->tap_n > 0) {
        float interval = now - d->last_tap;
        if (d->tap_n > 4) d->tap_n = 4;
        memmove(d->taps + 1, d->taps, sizeof(float) * 3);
        d->taps[0] = interval;
        int n = d->tap_n < 4 ? d->tap_n : 4;
        float sum = 0.0f;
        for (int i = 0; i < n; i++) sum += d->taps[i];
        d->bpm = clampf(roundf(60.0f / (sum / n)), 20, 300);
    }
    d->tap_n++;
    d->last_tap = now;
}

static void metro_toggle(Diapason *d)
{
    if (!d->metro_on) { d->tick_index = 0; d->tick_count = 0.0; }
    d->metro_on = !d->metro_on;
}

static int repeats(int page, int b)
{
    if (page == PAGE_METRO) return b == PAD_UP || b == PAD_DOWN || b == PAD_L1 || b == PAD_R1;
    return b == PAD_UP || b == PAD_DOWN || b == PAD_LEFT || b == PAD_RIGHT;
}

static void press(Diapason *d, int b)
{
    if (d->quit_dialog) {
        if (b == PAD_A || b == PAD_DOWN) { d->quit = 1; d->ref_on = 0; d->metro_on = 0; }
        else if (b == PAD_B || b == PAD_UP || b == PAD_MENU) d->quit_dialog = 0;
        return;
    }
    if (b == PAD_MENU) { d->quit_dialog = 1; return; }
    if (b == PAD_SELECT) { d->page = (d->page + 1) % PAGE_COUNT; return; }
    if (b == PAD_START) { metro_toggle(d); return; }
    if (d->page == PAGE_TUNER) {
        const Instrument *in = &INSTRUMENTS[d->instrument];
        switch (b) {
        case PAD_L1: d->instrument = (d->instrument + INSTRUMENT_COUNT - 1) % INSTRUMENT_COUNT; d->sel = 0; break;
        case PAD_R1: d->instrument = (d->instrument + 1) % INSTRUMENT_COUNT; d->sel = 0; break;
        case PAD_LEFT:
            if (in->n) d->sel = (d->sel + in->n - 1) % in->n;
            else d->chroma_note = d->chroma_note > 24 ? d->chroma_note - 1 : 24;
            break;
        case PAD_RIGHT:
            if (in->n) d->sel = (d->sel + 1) % in->n;
            else d->chroma_note = d->chroma_note < 96 ? d->chroma_note + 1 : 96;
            break;
        case PAD_UP: d->a4 = clampf(d->a4 + 1.0f, 400, 480); break;
        case PAD_DOWN: d->a4 = clampf(d->a4 - 1.0f, 400, 480); break;
        case PAD_A: d->ref_on = !d->ref_on; break;
        case PAD_Y: d->a4 = 440.0f; break;
        }
        set_ref(d);
    } else {
        switch (b) {
        case PAD_A: metro_toggle(d); break;
        case PAD_UP: d->bpm = clampf(d->bpm + 1, 20, 300); break;
        case PAD_DOWN: d->bpm = clampf(d->bpm - 1, 20, 300); break;
        case PAD_R1: d->bpm = clampf(d->bpm + 10, 20, 300); break;
        case PAD_L1: d->bpm = clampf(d->bpm - 10, 20, 300); break;
        case PAD_LEFT: d->beats = d->beats > 1 ? d->beats - 1 : 1; d->tick_index = 0; break;
        case PAD_RIGHT: d->beats = d->beats < 12 ? d->beats + 1 : 12; d->tick_index = 0; break;
        case PAD_X: d->subdiv = d->subdiv % 4 + 1; d->tick_index = 0; break;
        case PAD_Y: tap_tempo(d); break;
        case PAD_B: d->accent = !d->accent; break;
        }
    }
}

void diapason_button(Diapason *d, int b, int pressed)
{
    if (b < 0 || b >= PAD_COUNT) return;
    if (pressed) {
        d->down[b] = 1;
        d->rep[b] = 0.0f;
        press(d, b);
    } else {
        d->down[b] = 0;
    }
}

void diapason_update(Diapason *d, float dt)
{
    d->time += dt;
    for (int b = 0; b < PAD_COUNT; b++) {
        if (!d->down[b] || d->quit_dialog || !repeats(d->page, b)) continue;
        d->rep[b] += dt;
        while (d->rep[b] > 0.38f) { d->rep[b] -= 0.07f; press(d, b); }
    }
    analyze(d, dt);
    uint32_t serial = __atomic_load_n(&d->beat_serial, __ATOMIC_ACQUIRE);
    if (serial != d->beat_seen) { d->beat_seen = serial; d->since_beat = 0.0f; }
    else d->since_beat += dt;
    /* l'ago segue i centesimi con un po' d'inerzia */
    float target = d->note >= 0 ? d->cents : 0.0f;
    d->needle += (target - d->needle) * fminf(1.0f, dt * 10.0f);
    set_ref(d);
}

int diapason_wants_quit(const Diapason *d) { return d->quit; }

/* Per i test. */
float diapason_freq(const Diapason *d) { return d->note >= 0 ? d->freq : 0.0f; }
int diapason_note(const Diapason *d) { return d->note; }
float diapason_cents(const Diapason *d) { return d->cents; }
void diapason_set_page(Diapason *d, int page) { d->page = page % PAGE_COUNT; }
void diapason_set_instrument(Diapason *d, int i) { d->instrument = i % INSTRUMENT_COUNT; }
uint32_t diapason_beat_serial(const Diapason *d) { return __atomic_load_n(&d->beat_serial, __ATOMIC_ACQUIRE); }

/* ------------------------------------------------------------------ disegno */
static void draw_header(Diapason *d, Canvas *c)
{
    gfx_rect(c, 0, 0, c->w, 46, RGB(16, 20, 28));
    gfx_rect(c, 0, 46, c->w, 1, C_LINE);
    /* diapason stilizzato */
    gfx_line(c, 18, 10, 18, 26, 3.0f, C_VIOLET, 1.0f);
    gfx_line(c, 30, 10, 30, 26, 3.0f, C_VIOLET, 1.0f);
    gfx_line(c, 18, 26, 24, 31, 3.0f, C_VIOLET, 1.0f);
    gfx_line(c, 30, 26, 24, 31, 3.0f, C_VIOLET, 1.0f);
    gfx_line(c, 24, 31, 24, 39, 3.0f, C_VIOLET, 1.0f);
    gfx_text(c, FONT_TITLE, 42, 31, "Diapason", C_TEXT);
    static const char *tabs[PAGE_COUNT] = { "Accordatore", "Metronomo" };
    int x = 300;
    for (int i = 0; i < PAGE_COUNT; i++) {
        int w = gfx_text_width(FONT_BOLD, tabs[i]) + 24;
        if (i == d->page) gfx_round_rect(c, x, 10, w, 27, 13.5f, C_VIOLET_D, 1.0f);
        gfx_text_center(c, FONT_BOLD, x + w / 2, 29, tabs[i], i == d->page ? C_TEXT : C_MUTED);
        x += w + 6;
    }
    if (d->metro_on) gfx_circle(c, 618, 23, 7, C_GREEN, 0.4f + 0.6f * expf(-d->since_beat * 8.0f));
}

static void draw_hints(Canvas *c, const char *text)
{
    gfx_rect(c, 0, 446, c->w, 34, RGB(16, 20, 28));
    gfx_rect(c, 0, 446, c->w, 1, C_LINE);
    gfx_text_center(c, FONT_SMALL, c->w / 2, 468, text, C_MUTED);
}

static void draw_tuner(Diapason *d, Canvas *c)
{
    const Instrument *in = &INSTRUMENTS[d->instrument];
    float cx = 320, cy = 268, r = 196;
    int active = d->mic && d->note >= 0;
    float ac = fabsf(d->needle);
    uint32_t col = !active ? C_DIM : (ac <= 5 ? C_GREEN : (ac <= 15 ? C_AMBER : C_RED));

    /* scala da -50 a +50 centesimi su un arco di 140 gradi */
    for (int k = -50; k <= 50; k += 5) {
        float ang = (float)k / 50.0f * 70.0f * PI_F / 180.0f;
        float len = k % 25 == 0 ? 18.0f : (k % 10 == 0 ? 12.0f : 7.0f);
        float sx = cx + sinf(ang) * r, sy = cy - cosf(ang) * r;
        float ex = cx + sinf(ang) * (r - len), ey = cy - cosf(ang) * (r - len);
        uint32_t tc = abs(k) <= 5 ? C_GREEN : C_DIM;
        gfx_line(c, sx, sy, ex, ey, k % 25 == 0 ? 2.5f : 1.6f, tc, 1.0f);
    }
    gfx_text_center(c, FONT_SMALL, (int)(cx - sinf(70 * PI_F / 180) * (r + 4)) + 6, (int)(cy - cosf(70 * PI_F / 180) * (r + 4)) - 4, "-50", C_DIM);
    gfx_text_center(c, FONT_SMALL, (int)(cx + sinf(70 * PI_F / 180) * (r + 4)) - 6, (int)(cy - cosf(70 * PI_F / 180) * (r + 4)) - 4, "+50", C_DIM);
    gfx_text_center(c, FONT_SMALL, (int)cx, (int)(cy - r - 8), "0", C_GREEN);
    /* ago */
    float ang = clampf(d->needle, -50, 50) / 50.0f * 70.0f * PI_F / 180.0f;
    /* ago corto nella fascia esterna: il centro resta libero per il nome della nota */
    float r0 = r - 92, r1 = r - 24;
    gfx_line(c, cx + sinf(ang) * r0, cy - cosf(ang) * r0, cx + sinf(ang) * r1, cy - cosf(ang) * r1, 5.0f, col, active ? 1.0f : 0.45f);
    gfx_circle(c, cx + sinf(ang) * r1, cy - cosf(ang) * r1, 5.5f, col, active ? 1.0f : 0.45f);

    /* nota */
    char t[64];
    if (active) {
        note_name(t, sizeof(t), d->note);
        gfx_text_center(c, FONT_BIG, (int)cx, 196, t, col == C_GREEN ? C_GREEN : C_TEXT);
        snprintf(t, sizeof(t), "%+d cent   ·   %.1f Hz", (int)lrintf(d->cents), d->freq);
        for (char *p = t; *p; p++) if (*p == '.') *p = ',';
        gfx_text_center(c, FONT_BOLD, (int)cx, 230, t, C_MUTED);
    } else if (d->mic) {
        gfx_text_center(c, FONT_TITLE, (int)cx, 196, "Suona una nota", C_MUTED);
    } else {
        gfx_text_center(c, FONT_TITLE, (int)cx, 186, "Nessun microfono", C_MUTED);
        gfx_text_center(c, FONT_SMALL, (int)cx, 212, "La Flip non ne ha uno: collega un microfono USB-C.", C_DIM);
        gfx_text_center(c, FONT_SMALL, (int)cx, 232, "Intanto A suona la nota di riferimento.", C_DIM);
    }

    /* strumento e corde */
    gfx_round_rect(c, 16, 290, 608, 146, 14, C_PANEL, 1.0f);
    snprintf(t, sizeof(t), "%s", in->name);
    gfx_text(c, FONT_BOLD, 32, 318, t, C_TEXT);
    char a4[40];
    snprintf(a4, sizeof(a4), "La = %d Hz", (int)lrintf(d->a4));
    gfx_text_right(c, FONT_BOLD, 608, 318, a4, d->a4 == 440.0f ? C_MUTED : C_AMBER);
    if (in->n) {
        float pw = 84, gap = (576 - in->n * pw) / (in->n > 1 ? in->n - 1 : 1);
        if (gap > 16) gap = 16;
        float total = in->n * pw + (in->n - 1) * gap, x0 = 320 - total / 2;
        for (int i = 0; i < in->n; i++) {
            float x = x0 + i * (pw + gap);
            int detected = active && d->note == in->notes[i];
            uint32_t fill = detected ? (col == C_GREEN ? C_GREEN : C_VIOLET_D) : C_PANEL_HI;
            gfx_round_rect(c, x, 334, pw, 54, 12, fill, 1.0f);
            if (i == d->sel) gfx_round_frame(c, x - 2, 332, pw + 4, 58, 13, 2.5f, d->ref_on ? C_AMBER : C_MUTED, 1.0f);
            note_name(t, sizeof(t), in->notes[i]);
            gfx_text_center(c, FONT_BOLD, (int)(x + pw / 2), 360, t, C_TEXT);
            snprintf(t, sizeof(t), "corda %d", in->n - i);
            gfx_text_center(c, FONT_SMALL, (int)(x + pw / 2), 380, t, detected ? C_TEXT : C_MUTED);
        }
    } else {
        note_name(t, sizeof(t), d->chroma_note);
        gfx_round_rect(c, 260, 334, 120, 54, 12, C_PANEL_HI, 1.0f);
        gfx_round_frame(c, 258, 332, 124, 58, 13, 2.5f, d->ref_on ? C_AMBER : C_MUTED, 1.0f);
        gfx_text_center(c, FONT_BOLD, 320, 360, t, C_TEXT);
        gfx_text_center(c, FONT_SMALL, 320, 380, "riferimento", C_MUTED);
    }
    snprintf(t, sizeof(t), "%s%s", d->ref_on ? "Nota di riferimento accesa" : "Microfono: ",
             d->ref_on ? "" : (d->mic ? d->mic_name : "nessuno"));
    gfx_text_center(c, FONT_SMALL, 320, 420, t, d->ref_on ? C_AMBER : C_DIM);
    draw_hints(c, "L1/R1 strumento · sinistra/destra corda · A nota di riferimento · su/giù La ±1 Hz · Y La 440");
}

static void draw_metro(Diapason *d, Canvas *c)
{
    char t[64];
    snprintf(t, sizeof(t), "%d", (int)lrintf(d->bpm));
    gfx_text_center(c, FONT_BIG, 160, 150, t, C_TEXT);
    gfx_text_center(c, FONT_BOLD, 160, 182, "BPM", C_MUTED);
    gfx_text_center(c, FONT_TITLE, 160, 222, tempo_name(d->bpm), C_VIOLET);

    /* pendolo */
    float beat_len = 60.0f / d->bpm, since = d->since_beat;
    float phase = d->metro_on ? fminf(1.0f, since / beat_len) : 0.5f;
    int dir = (int)(d->beat_seen % 2);
    float swing = d->metro_on ? cosf(PI_F * phase) * (dir ? 1.0f : -1.0f) : 0.0f;
    float px = 470, py = 70, len = 170;
    float ang = swing * 0.45f;
    gfx_line(c, px, py, px + sinf(ang) * len, py + cosf(ang) * len, 4.0f, C_MUTED, 1.0f);
    float flash = d->metro_on ? expf(-since * 6.0f) : 0.0f;
    gfx_circle(c, px + sinf(ang) * len, py + cosf(ang) * len, 16, gfx_mix(C_VIOLET, C_TEXT, flash), 1.0f);
    gfx_circle(c, px, py, 6, C_DIM, 1.0f);

    /* tempi della battuta */
    int n = d->beats;
    float bw = n <= 8 ? 56 : 44, gap = 10, total = n * bw + (n - 1) * gap, x0 = 320 - total / 2;
    for (int i = 0; i < n; i++) {
        int now = d->metro_on && d->beat_now == i;
        uint32_t fill = now ? (i == 0 && d->accent ? C_AMBER : C_GREEN) : (i == 0 && d->accent ? RGB(80, 66, 40) : C_PANEL_HI);
        gfx_round_rect(c, x0 + i * (bw + gap), 290, bw, 50, 12, fill, 1.0f);
        snprintf(t, sizeof(t), "%d", i + 1);
        gfx_text_center(c, FONT_BOLD, (int)(x0 + i * (bw + gap) + bw / 2), 322, t, now ? RGB(20, 20, 24) : C_MUTED);
    }
    static const char *subs[5] = { "", "semiminime", "crome", "terzine", "semicrome" };
    snprintf(t, sizeof(t), "%d/4   ·   %s   ·   accento %s", d->beats, subs[d->subdiv], d->accent ? "sì" : "no");
    gfx_text_center(c, FONT_BOLD, 320, 378, t, C_TEXT);
    gfx_text_center(c, FONT_SMALL, 320, 406, d->metro_on ? "in corso" : "fermo: premi A", d->metro_on ? C_GREEN : C_DIM);
    draw_hints(c, "A avvia/ferma · su/giù ±1 · L1/R1 ±10 · sinistra/destra tempi · X suddivisione · Y tap · B accento");
}

static void draw_quit(Canvas *c)
{
    gfx_rect_alpha(c, 0, 0, c->w, c->h, RGB(0, 0, 0), 0.6f);
    gfx_round_rect(c, 150, 170, 340, 140, 18, C_PANEL_HI, 1.0f);
    gfx_round_frame(c, 150, 170, 340, 140, 18, 2, C_VIOLET, 1.0f);
    gfx_text_center(c, FONT_TITLE, 320, 222, "Uscire da Diapason?", C_TEXT);
    gfx_text_center(c, FONT_BODY, 320, 256, "Le impostazioni restano salvate.", C_MUTED);
    gfx_text_center(c, FONT_BOLD, 320, 290, "A o giù: esci   ·   B o su: resta", C_AMBER);
}

void diapason_draw(Diapason *d, Canvas *c)
{
    gfx_vgradient(c, 0, 0, c->w, c->h, C_BG_TOP, C_BG_BOT);
    draw_header(d, c);
    if (d->page == PAGE_TUNER) draw_tuner(d, c);
    else draw_metro(d, c);
    if (d->quit_dialog) draw_quit(c);
}

/* ------------------------------------------------------------------ collegamento con il kit */
static void *k_create(float sr, const char *state) { return diapason_create(sr, state); }
static void k_destroy(void *p) { diapason_destroy(p); }
static void k_audio(void *p, float *out, int n) { diapason_audio(p, out, n); }
static void k_capture(void *p, const float *in, int n) { diapason_capture(p, in, n); }
static void k_status(void *p, const char *dev, float rate) { diapason_capture_status(p, dev, rate); }
static void k_button(void *p, int b, int pressed) { diapason_button(p, b, pressed); }
static void k_axes(void *p, float lx, float ly, float rx, float ry, float l2, float r2) { (void)p; (void)lx; (void)ly; (void)rx; (void)ry; (void)l2; (void)r2; }
static void k_update(void *p, float dt) { diapason_update(p, dt); }
static void k_draw(void *p, Canvas *c) { diapason_draw(p, c); }
static int k_quit(void *p) { return diapason_wants_quit(p); }

const CirmoloApp *cirmolo_app(void)
{
    static const CirmoloApp desc = {
        .title = "Diapason",
        .state_path = "/mnt/SDCARD/Saves/diapason/stato.txt",
        .wants_capture = 1,
        .create = k_create, .destroy = k_destroy, .audio = k_audio, .capture = k_capture, .capture_status = k_status,
        .button = k_button, .axes = k_axes, .update = k_update, .draw = k_draw, .wants_quit = k_quit,
    };
    return &desc;
}
