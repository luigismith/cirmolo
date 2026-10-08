/* Cirmolo Synth - logica dell'app e interfaccia (vedi app.h). */
#include "app.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ palette */
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
#define C_GREEN_D  RGB(44, 104, 70)
#define C_AMBER    RGB(244, 186, 92)
#define C_RED      RGB(236, 96, 104)

/* ------------------------------------------------------------------ musica */
static const char *NOTE_NAMES[12] = { "Do", "Do#", "Re", "Re#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" };

typedef struct { const char *name; int n; int iv[12]; } Scale;
static const Scale SCALES[] = {
    { "maggiore", 7, { 0, 2, 4, 5, 7, 9, 11 } },
    { "minore", 7, { 0, 2, 3, 5, 7, 8, 10 } },
    { "dorica", 7, { 0, 2, 3, 5, 7, 9, 10 } },
    { "misolidia", 7, { 0, 2, 4, 5, 7, 9, 10 } },
    { "pentatonica maggiore", 5, { 0, 2, 4, 7, 9 } },
    { "pentatonica minore", 5, { 0, 3, 5, 7, 10 } },
    { "blues", 6, { 0, 3, 5, 6, 7, 10 } },
    { "cromatica", 12, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
};
#define SCALE_COUNT ((int)(sizeof(SCALES) / sizeof(SCALES[0])))

/* I tasti che suonano, nell'ordine dei gradi: croce sinistra-giu'-destra-su, poi Y-B-A-X (ovest-sud-est-nord). */
static const int NOTE_BTNS[8] = { BTN_LEFT, BTN_DOWN, BTN_RIGHT, BTN_UP, BTN_Y, BTN_B, BTN_A, BTN_X };

static const char *DRUM_NAMES[DRUM_COUNT] = { "Cassa", "Rullante", "Charleston", "Charl. aperto", "Battimani" };

/* ------------------------------------------------------------------ preset */
typedef struct { const char *name; SynthPatch p; FxParams fx; } Preset;

static const Preset PRESETS[] = {
    { "Pigna", { .osc1_wave = WAVE_SAW, .osc2_wave = WAVE_SAW, .osc2_semi = 0, .osc2_detune = 9, .osc_mix = 0.5f, .pulse_width = 0.5f,
        .noise = 0.02f, .cutoff = 0.52f, .resonance = 0.22f, .env_amount = 0.18f, .key_track = 0.3f,
        .f_attack = 0.6f, .f_decay = 1.2f, .f_sustain = 0.5f, .f_release = 1.4f,
        .a_attack = 0.45f, .a_decay = 1.0f, .a_sustain = 0.85f, .a_release = 1.6f,
        .lfo_rate = 0.25f, .lfo_pitch = 0, .lfo_filter = 0.15f, .mono = 0, .glide = 0, .drive = 0.1f, .volume = 0.8f },
      { .delay_steps = 3, .delay_feedback = 0.35f, .delay_mix = 0.18f, .reverb_mix = 0.35f, .reverb_size = 0.8f, .drums_volume = 0.8f } },
    { "Basso Dolomiti", { .osc1_wave = WAVE_SAW, .osc2_wave = WAVE_SQUARE, .osc2_semi = -12, .osc2_detune = 0, .osc_mix = 0.45f, .pulse_width = 0.5f,
        .noise = 0, .cutoff = 0.26f, .resonance = 0.45f, .env_amount = 0.42f, .key_track = 0.4f,
        .f_attack = 0.003f, .f_decay = 0.28f, .f_sustain = 0.15f, .f_release = 0.15f,
        .a_attack = 0.003f, .a_decay = 0.4f, .a_sustain = 0.7f, .a_release = 0.12f,
        .lfo_rate = 0.5f, .lfo_pitch = 0, .lfo_filter = 0, .mono = 1, .glide = 0.06f, .drive = 0.35f, .volume = 0.85f },
      { .delay_steps = 3, .delay_feedback = 0.2f, .delay_mix = 0, .reverb_mix = 0.08f, .reverb_size = 0.4f, .drums_volume = 0.8f } },
    { "Lead Cirmolo", { .osc1_wave = WAVE_SQUARE, .osc2_wave = WAVE_SAW, .osc2_semi = 0, .osc2_detune = 12, .osc_mix = 0.5f, .pulse_width = 0.35f,
        .noise = 0, .cutoff = 0.55f, .resonance = 0.3f, .env_amount = 0.3f, .key_track = 0.5f,
        .f_attack = 0.01f, .f_decay = 0.4f, .f_sustain = 0.5f, .f_release = 0.3f,
        .a_attack = 0.01f, .a_decay = 0.2f, .a_sustain = 0.9f, .a_release = 0.25f,
        .lfo_rate = 5.0f, .lfo_pitch = 0, .lfo_filter = 0, .mono = 1, .glide = 0.09f, .drive = 0.25f, .volume = 0.75f },
      { .delay_steps = 3, .delay_feedback = 0.4f, .delay_mix = 0.25f, .reverb_mix = 0.2f, .reverb_size = 0.6f, .drums_volume = 0.8f } },
    { "Pizzico", { .osc1_wave = WAVE_SAW, .osc2_wave = WAVE_SQUARE, .osc2_semi = 12, .osc2_detune = 3, .osc_mix = 0.25f, .pulse_width = 0.5f,
        .noise = 0, .cutoff = 0.22f, .resonance = 0.35f, .env_amount = 0.55f, .key_track = 0.6f,
        .f_attack = 0.001f, .f_decay = 0.22f, .f_sustain = 0, .f_release = 0.25f,
        .a_attack = 0.001f, .a_decay = 0.6f, .a_sustain = 0, .a_release = 0.4f,
        .lfo_rate = 0.5f, .lfo_pitch = 0, .lfo_filter = 0, .mono = 0, .glide = 0, .drive = 0.1f, .volume = 0.85f },
      { .delay_steps = 3, .delay_feedback = 0.45f, .delay_mix = 0.3f, .reverb_mix = 0.25f, .reverb_size = 0.7f, .drums_volume = 0.8f } },
    { "Organo", { .osc1_wave = WAVE_SINE, .osc2_wave = WAVE_SQUARE, .osc2_semi = 12, .osc2_detune = 0, .osc_mix = 0.3f, .pulse_width = 0.5f,
        .noise = 0, .cutoff = 0.7f, .resonance = 0.05f, .env_amount = 0, .key_track = 0,
        .f_attack = 0.01f, .f_decay = 0.5f, .f_sustain = 1, .f_release = 0.2f,
        .a_attack = 0.005f, .a_decay = 0.1f, .a_sustain = 1, .a_release = 0.08f,
        .lfo_rate = 6.0f, .lfo_pitch = 0.08f, .lfo_filter = 0, .mono = 0, .glide = 0, .drive = 0.15f, .volume = 0.7f },
      { .delay_steps = 3, .delay_feedback = 0.2f, .delay_mix = 0, .reverb_mix = 0.25f, .reverb_size = 0.6f, .drums_volume = 0.8f } },
    { "Archi", { .osc1_wave = WAVE_SAW, .osc2_wave = WAVE_SAW, .osc2_semi = 0, .osc2_detune = 14, .osc_mix = 0.5f, .pulse_width = 0.5f,
        .noise = 0, .cutoff = 0.6f, .resonance = 0.1f, .env_amount = 0.1f, .key_track = 0.2f,
        .f_attack = 0.8f, .f_decay = 1.5f, .f_sustain = 0.7f, .f_release = 1.5f,
        .a_attack = 0.7f, .a_decay = 1.0f, .a_sustain = 0.9f, .a_release = 2.0f,
        .lfo_rate = 4.5f, .lfo_pitch = 0.05f, .lfo_filter = 0, .mono = 0, .glide = 0, .drive = 0, .volume = 0.75f },
      { .delay_steps = 4, .delay_feedback = 0.2f, .delay_mix = 0, .reverb_mix = 0.45f, .reverb_size = 0.9f, .drums_volume = 0.8f } },
    { "Campana", { .osc1_wave = WAVE_SINE, .osc2_wave = WAVE_TRIANGLE, .osc2_semi = 19, .osc2_detune = 0, .osc_mix = 0.35f, .pulse_width = 0.5f,
        .noise = 0, .cutoff = 0.8f, .resonance = 0, .env_amount = 0, .key_track = 0,
        .f_attack = 0.001f, .f_decay = 1.0f, .f_sustain = 1, .f_release = 1.0f,
        .a_attack = 0.001f, .a_decay = 2.2f, .a_sustain = 0, .a_release = 2.0f,
        .lfo_rate = 0.5f, .lfo_pitch = 0, .lfo_filter = 0, .mono = 0, .glide = 0, .drive = 0, .volume = 0.8f },
      { .delay_steps = 6, .delay_feedback = 0.3f, .delay_mix = 0.2f, .reverb_mix = 0.4f, .reverb_size = 0.85f, .drums_volume = 0.8f } },
    { "Otto bit", { .osc1_wave = WAVE_SQUARE, .osc2_wave = WAVE_SQUARE, .osc2_semi = -12, .osc2_detune = 0, .osc_mix = 0.3f, .pulse_width = 0.25f,
        .noise = 0, .cutoff = 1.0f, .resonance = 0, .env_amount = 0, .key_track = 0,
        .f_attack = 0.001f, .f_decay = 0.5f, .f_sustain = 1, .f_release = 0.1f,
        .a_attack = 0.001f, .a_decay = 0.15f, .a_sustain = 0.6f, .a_release = 0.08f,
        .lfo_rate = 0.5f, .lfo_pitch = 0, .lfo_filter = 0, .mono = 0, .glide = 0, .drive = 0, .volume = 0.6f },
      { .delay_steps = 2, .delay_feedback = 0.25f, .delay_mix = 0.15f, .reverb_mix = 0.05f, .reverb_size = 0.4f, .drums_volume = 0.8f } },
};
#define PRESET_COUNT ((int)(sizeof(PRESETS) / sizeof(PRESETS[0])))

/* ------------------------------------------------------------------ parametri della pagina Suono */
typedef enum { K_HEADER, K_LIN, K_EXP, K_WAVE, K_BOOL, K_PRESET, K_ROOT, K_SCALE, K_TEMPO, K_SWING } Kind;
typedef enum { T_PATCH, T_FX } Target;
typedef enum { F_PCT, F_SEC, F_HZ, F_CENT, F_SEMI, F_BIPCT, F_STEPS, F_MIX, F_CUTOFF } Fmt;
typedef struct { const char *name; Kind kind; Target target; size_t off; float min, max, step; Fmt fmt; } Param;

#define PP(f) T_PATCH, offsetof(SynthPatch, f)
#define PF(f) T_FX, offsetof(FxParams, f)
static const Param PARAMS[] = {
    { "Generale", K_HEADER },
    { "Preset", K_PRESET },
    { "Tonalità", K_ROOT },
    { "Scala", K_SCALE },
    { "Volume", K_LIN, PP(volume), 0, 1, 0.05f, F_PCT },
    { "Oscillatori", K_HEADER },
    { "Onda 1", K_WAVE, PP(osc1_wave) },
    { "Onda 2", K_WAVE, PP(osc2_wave) },
    { "Intervallo onda 2", K_LIN, PP(osc2_semi), -24, 24, 1, F_SEMI },
    { "Desintonia", K_LIN, PP(osc2_detune), -50, 50, 1, F_CENT },
    { "Miscela 1 / 2", K_LIN, PP(osc_mix), 0, 1, 0.05f, F_MIX },
    { "Larghezza impulso", K_LIN, PP(pulse_width), 0.05f, 0.95f, 0.05f, F_PCT },
    { "Rumore", K_LIN, PP(noise), 0, 1, 0.05f, F_PCT },
    { "Filtro", K_HEADER },
    { "Taglio", K_LIN, PP(cutoff), 0, 1, 0.02f, F_CUTOFF },
    { "Risonanza", K_LIN, PP(resonance), 0, 0.97f, 0.03f, F_PCT },
    { "Quantità inviluppo", K_LIN, PP(env_amount), -1, 1, 0.05f, F_BIPCT },
    { "Segue la tastiera", K_LIN, PP(key_track), 0, 1, 0.1f, F_PCT },
    { "Attacco", K_EXP, PP(f_attack), 0.001f, 5, 0, F_SEC },
    { "Decadimento", K_EXP, PP(f_decay), 0.005f, 8, 0, F_SEC },
    { "Sostegno", K_LIN, PP(f_sustain), 0, 1, 0.05f, F_PCT },
    { "Rilascio", K_EXP, PP(f_release), 0.005f, 8, 0, F_SEC },
    { "Ampiezza", K_HEADER },
    { "Attacco", K_EXP, PP(a_attack), 0.001f, 5, 0, F_SEC },
    { "Decadimento", K_EXP, PP(a_decay), 0.005f, 8, 0, F_SEC },
    { "Sostegno", K_LIN, PP(a_sustain), 0, 1, 0.05f, F_PCT },
    { "Rilascio", K_EXP, PP(a_release), 0.005f, 8, 0, F_SEC },
    { "Modulazione", K_HEADER },
    { "Velocità LFO", K_EXP, PP(lfo_rate), 0.05f, 20, 0, F_HZ },
    { "LFO sull'intonazione", K_LIN, PP(lfo_pitch), 0, 1, 0.05f, F_PCT },
    { "LFO sul filtro", K_LIN, PP(lfo_filter), 0, 1, 0.05f, F_PCT },
    { "Esecuzione", K_HEADER },
    { "Monofonico", K_BOOL, PP(mono) },
    { "Glide", K_LIN, PP(glide), 0, 1, 0.02f, F_SEC },
    { "Saturazione", K_LIN, PP(drive), 0, 1, 0.05f, F_PCT },
    { "Effetti", K_HEADER },
    { "Delay", K_LIN, PF(delay_steps), 1, 8, 1, F_STEPS },
    { "Ritorno del delay", K_LIN, PF(delay_feedback), 0, 0.9f, 0.05f, F_PCT },
    { "Livello del delay", K_LIN, PF(delay_mix), 0, 1, 0.05f, F_PCT },
    { "Riverbero", K_LIN, PF(reverb_mix), 0, 1, 0.05f, F_PCT },
    { "Ampiezza del riverbero", K_LIN, PF(reverb_size), 0, 1, 0.05f, F_PCT },
    { "Volume batteria", K_LIN, PF(drums_volume), 0, 1, 0.05f, F_PCT },
    { "Sequenza", K_HEADER },
    { "Tempo", K_TEMPO },
    { "Swing", K_SWING },
};
#define PARAM_COUNT ((int)(sizeof(PARAMS) / sizeof(PARAMS[0])))
static const char *WAVE_NAMES[WAVE_COUNT] = { "Dente di sega", "Quadra", "Triangolo", "Sinusoide" };

/* Campi salvati nel file di stato. */
typedef struct { const char *key; Target target; size_t off; int is_int; } Field;
static const Field FIELDS[] = {
    { "osc1_wave", PP(osc1_wave), 1 }, { "osc2_wave", PP(osc2_wave), 1 }, { "osc2_semi", PP(osc2_semi), 0 },
    { "osc2_detune", PP(osc2_detune), 0 }, { "osc_mix", PP(osc_mix), 0 }, { "pulse_width", PP(pulse_width), 0 },
    { "noise", PP(noise), 0 }, { "cutoff", PP(cutoff), 0 }, { "resonance", PP(resonance), 0 },
    { "env_amount", PP(env_amount), 0 }, { "key_track", PP(key_track), 0 },
    { "f_attack", PP(f_attack), 0 }, { "f_decay", PP(f_decay), 0 }, { "f_sustain", PP(f_sustain), 0 }, { "f_release", PP(f_release), 0 },
    { "a_attack", PP(a_attack), 0 }, { "a_decay", PP(a_decay), 0 }, { "a_sustain", PP(a_sustain), 0 }, { "a_release", PP(a_release), 0 },
    { "lfo_rate", PP(lfo_rate), 0 }, { "lfo_pitch", PP(lfo_pitch), 0 }, { "lfo_filter", PP(lfo_filter), 0 },
    { "mono", PP(mono), 1 }, { "glide", PP(glide), 0 }, { "drive", PP(drive), 0 }, { "volume", PP(volume), 0 },
    { "delay_steps", PF(delay_steps), 0 }, { "delay_feedback", PF(delay_feedback), 0 }, { "delay_mix", PF(delay_mix), 0 },
    { "reverb_mix", PF(reverb_mix), 0 }, { "reverb_size", PF(reverb_size), 0 }, { "drums_volume", PF(drums_volume), 0 },
};
#define FIELD_COUNT ((int)(sizeof(FIELDS) / sizeof(FIELDS[0])))

/* ------------------------------------------------------------------ stato */
struct App {
    Synth *s;
    char path[512];
    int screen, quit_dialog, quit;
    int preset, modified;
    int root, scale, octave;
    int down[BTN_COUNT];
    int btn_notes[8][3], btn_count[8];
    int sustained[128];
    float lx, ly, rx, ry, l2, r2;
    int sel;
    float scroll;
    int audition;
    int cx, cy;
    float rep[BTN_COUNT];
    float time;
    char last_label[48];
    float note_glow;
    uint32_t drum_seen[DRUM_COUNT];
    float drum_glow[DRUM_COUNT];
    float scope[1200];
};

static void *field_ptr(App *a, Target t, size_t off)
{
    return (t == T_PATCH ? (char *)synth_patch(a->s) : (char *)synth_fx(a->s)) + off;
}

static void apply_scale(App *a)
{
    const Scale *sc = &SCALES[a->scale];
    synth_set_scale(a->s, 48 + a->root, sc->iv, sc->n);      /* la riga melodica del sequencer parte dal Do3 */
}

void app_load_preset(App *a, int i)
{
    if (i < 0 || i >= PRESET_COUNT) return;
    a->preset = i;
    a->modified = 0;
    *synth_patch(a->s) = PRESETS[i].p;
    *synth_fx(a->s) = PRESETS[i].fx;
}

int app_preset_count(void) { return PRESET_COUNT; }
const char *app_preset_name(int i) { return (i >= 0 && i < PRESET_COUNT) ? PRESETS[i].name : ""; }

static void default_pattern(App *a)
{
    Pattern *p = synth_pattern(a->s);
    memset(p, 0, sizeof(*p));
    for (int i = 0; i < SY_STEPS; i++) p->note[i] = -1;
    int kick[] = { 0, 4, 8, 10, 12 }, snare[] = { 4, 12 }, ohat[] = { 14 };
    for (size_t i = 0; i < sizeof(kick) / sizeof(int); i++) p->drum[DRUM_KICK][kick[i]] = 1;
    for (size_t i = 0; i < sizeof(snare) / sizeof(int); i++) p->drum[DRUM_SNARE][snare[i]] = 1;
    for (int i = 0; i < SY_STEPS; i += 2) if (i != 14) p->drum[DRUM_HAT][i] = (i % 4 == 0) ? 2 : 1;
    for (size_t i = 0; i < sizeof(ohat) / sizeof(int); i++) p->drum[DRUM_OPENHAT][ohat[i]] = 1;
    int nstep[] = { 0, 3, 6, 8, 11, 14 }, ndeg[] = { 0, 0, 4, 0, 3, 4 };
    for (size_t i = 0; i < sizeof(nstep) / sizeof(int); i++) p->note[nstep[i]] = (int8_t)ndeg[i];
    p->accent[0] = p->accent[8] = 1;
}

/* ------------------------------------------------------------------ salvataggio */
void app_save(App *a)
{
    if (!a->path[0]) return;
    char tmp[600];
    snprintf(tmp, sizeof(tmp), "%s.tmp", a->path);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    fprintf(f, "# Cirmolo Synth: stato salvato automaticamente\nversion=1\npreset=%d\nmodified=%d\nroot=%d\nscale=%d\noctave=%d\ntempo=%g\nswing=%g\n",
            a->preset, a->modified, a->root, a->scale, a->octave, synth_tempo(a->s), synth_swing(a->s));
    for (int i = 0; i < FIELD_COUNT; i++) {
        void *ptr = field_ptr(a, FIELDS[i].target, FIELDS[i].off);
        if (FIELDS[i].is_int) fprintf(f, "%s=%d\n", FIELDS[i].key, *(int *)ptr);
        else fprintf(f, "%s=%g\n", FIELDS[i].key, *(float *)ptr);
    }
    Pattern *p = synth_pattern(a->s);
    for (int d = 0; d < DRUM_COUNT; d++) {
        fprintf(f, "drum%d=", d);
        for (int i = 0; i < SY_STEPS; i++) fputc(".xX"[p->drum[d][i] % 3], f);
        fputc('\n', f);
    }
    fprintf(f, "notes=");
    for (int i = 0; i < SY_STEPS; i++) fprintf(f, "%d%s", p->note[i], i < SY_STEPS - 1 ? "," : "\n");
    fprintf(f, "accents=");
    for (int i = 0; i < SY_STEPS; i++) fputc(p->accent[i] ? 'X' : '.', f);
    fputc('\n', f);
    fclose(f);
    remove(a->path);
    rename(tmp, a->path);
}

static int load_state(App *a)
{
    FILE *f = fopen(a->path, "r");
    if (!f) return 0;
    char line[256];
    Pattern *p = synth_pattern(a->s);
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq || line[0] == '#') continue;
        *eq = 0;
        char *k = line, *v = eq + 1;
        v[strcspn(v, "\r\n")] = 0;
        if (!strcmp(k, "preset")) { int i = atoi(v); if (i >= 0 && i < PRESET_COUNT) a->preset = i; }
        else if (!strcmp(k, "modified")) a->modified = atoi(v);
        else if (!strcmp(k, "root")) a->root = ((atoi(v) % 12) + 12) % 12;
        else if (!strcmp(k, "scale")) { int i = atoi(v); if (i >= 0 && i < SCALE_COUNT) a->scale = i; }
        else if (!strcmp(k, "octave")) { int o = atoi(v); if (o >= 1 && o <= 7) a->octave = o; }
        else if (!strcmp(k, "tempo")) synth_set_tempo(a->s, (float)atof(v));
        else if (!strcmp(k, "swing")) synth_set_swing(a->s, (float)atof(v));
        else if (!strncmp(k, "drum", 4)) {
            int d = atoi(k + 4);
            if (d >= 0 && d < DRUM_COUNT)
                for (int i = 0; i < SY_STEPS && v[i]; i++) p->drum[d][i] = v[i] == 'X' ? 2 : (v[i] == 'x' ? 1 : 0);
        } else if (!strcmp(k, "notes")) {
            char *t = v;
            for (int i = 0; i < SY_STEPS && t; i++) {
                int n = atoi(t);
                p->note[i] = (int8_t)(n < -1 ? -1 : (n > 15 ? 15 : n));
                t = strchr(t, ',');
                if (t) t++;
            }
        } else if (!strcmp(k, "accents")) {
            for (int i = 0; i < SY_STEPS && v[i]; i++) p->accent[i] = v[i] == 'X';
        } else {
            for (int i = 0; i < FIELD_COUNT; i++)
                if (!strcmp(k, FIELDS[i].key)) {
                    void *ptr = field_ptr(a, FIELDS[i].target, FIELDS[i].off);
                    if (FIELDS[i].is_int) *(int *)ptr = atoi(v);
                    else *(float *)ptr = (float)atof(v);
                }
        }
    }
    fclose(f);
    return 1;
}

/* ------------------------------------------------------------------ creazione */
App *app_create(Synth *s, const char *state_path)
{
    App *a = calloc(1, sizeof(App));
    if (!a) return NULL;
    a->s = s;
    if (state_path) snprintf(a->path, sizeof(a->path), "%s", state_path);
    a->octave = 4;
    a->sel = 1;
    a->cy = 0;
    app_load_preset(a, 0);
    default_pattern(a);
    synth_set_tempo(s, 110.0f);
    synth_set_swing(s, 0.1f);
    if (a->path[0]) {
        /* il preset prima, poi i valori salvati sopra */
        FILE *f = fopen(a->path, "r");
        if (f) {
            char line[64];
            while (fgets(line, sizeof(line), f))
                if (!strncmp(line, "preset=", 7)) { app_load_preset(a, atoi(line + 7)); break; }
            fclose(f);
        }
        load_state(a);
    }
    apply_scale(a);
    snprintf(a->last_label, sizeof(a->last_label), "%s", "");
    return a;
}

void app_destroy(App *a)
{
    if (!a) return;
    synth_all_notes_off(a->s);
    app_save(a);
    free(a);
}

int app_wants_quit(const App *a) { return a->quit; }
int app_screen(const App *a) { return a->screen; }

/* ------------------------------------------------------------------ note */
static int play_midi(App *a, int degree)
{
    const Scale *sc = &SCALES[a->scale];
    int oct = degree / sc->n, idx = degree % sc->n;
    int m = 12 * (a->octave + 1) + a->root + 12 * oct + sc->iv[idx];
    return m < 0 ? 0 : (m > 127 ? 127 : m);
}

static void note_label(char *out, size_t n, int midi)
{
    snprintf(out, n, "%s%d", NOTE_NAMES[midi % 12], midi / 12 - 1);
}

static void release_all(App *a)
{
    synth_all_notes_off(a->s);
    memset(a->btn_count, 0, sizeof(a->btn_count));
    memset(a->sustained, 0, sizeof(a->sustained));
    if (a->audition) a->audition = 0;
}

static void press_note(App *a, int idx)
{
    int chord = a->down[BTN_R2];
    int degs[3] = { idx, idx + 2, idx + 4 };
    int n = chord ? 3 : 1;
    a->btn_count[idx] = n;
    for (int i = 0; i < n; i++) {
        int m = play_midi(a, degs[i]);
        a->btn_notes[idx][i] = m;
        a->sustained[m] = 0;
        synth_note_on(a->s, m, 0.85f);
    }
    char name[16];
    note_label(name, sizeof(name), a->btn_notes[idx][0]);
    if (chord) {
        int r = a->btn_notes[idx][0], t3 = a->btn_notes[idx][1] - r, t5 = a->btn_notes[idx][2] - r;
        const char *q = (t3 == 4 && t5 == 7) ? "maggiore" : (t3 == 3 && t5 == 7) ? "minore" :
                        (t3 == 3 && t5 == 6) ? "diminuito" : (t3 == 4 && t5 == 8) ? "aumentato" : "";
        snprintf(a->last_label, sizeof(a->last_label), "%s %s", NOTE_NAMES[r % 12], q);
    } else {
        snprintf(a->last_label, sizeof(a->last_label), "%s", name);
    }
    a->note_glow = 1.0f;
}

static int note_held_by_button(App *a, int midi)
{
    for (int b = 0; b < 8; b++)
        for (int i = 0; i < a->btn_count[b]; i++)
            if (a->btn_notes[b][i] == midi) return 1;
    return 0;
}

static void release_note(App *a, int idx)
{
    int n = a->btn_count[idx];
    a->btn_count[idx] = 0;
    for (int i = 0; i < n; i++) {
        int m = a->btn_notes[idx][i];
        if (note_held_by_button(a, m)) continue;
        if (a->down[BTN_L2]) a->sustained[m] = 1;
        else synth_note_off(a->s, m);
    }
}

static void release_sustain(App *a)
{
    for (int m = 0; m < 128; m++)
        if (a->sustained[m]) {
            a->sustained[m] = 0;
            if (!note_held_by_button(a, m)) synth_note_off(a->s, m);
        }
}

/* ------------------------------------------------------------------ parametri */
static void param_text(App *a, const Param *pr, char *out, size_t n)
{
    switch (pr->kind) {
    case K_PRESET: snprintf(out, n, "%s%s", PRESETS[a->preset].name, a->modified ? " *" : ""); return;
    case K_ROOT: snprintf(out, n, "%s", NOTE_NAMES[a->root]); return;
    case K_SCALE: snprintf(out, n, "%s", SCALES[a->scale].name); return;
    case K_TEMPO: snprintf(out, n, "%d BPM", (int)lrintf(synth_tempo(a->s))); return;
    case K_SWING: snprintf(out, n, "%d %%", (int)lrintf(synth_swing(a->s) * 100)); return;
    case K_WAVE: { int w = *(int *)field_ptr(a, pr->target, pr->off); snprintf(out, n, "%s", WAVE_NAMES[(w % WAVE_COUNT + WAVE_COUNT) % WAVE_COUNT]); return; }
    case K_BOOL: snprintf(out, n, "%s", *(int *)field_ptr(a, pr->target, pr->off) ? "Sì" : "No"); return;
    default: break;
    }
    float v = *(float *)field_ptr(a, pr->target, pr->off);
    switch (pr->fmt) {
    case F_PCT: snprintf(out, n, "%d %%", (int)lrintf(v * 100)); break;
    case F_BIPCT: snprintf(out, n, lrintf(v * 100) ? "%+d %%" : "%d %%", (int)lrintf(v * 100)); break;
    case F_SEC:
        if (v < 0.0005f) snprintf(out, n, "spento");
        else if (v < 1.0f) snprintf(out, n, "%d ms", (int)lrintf(v * 1000));
        else snprintf(out, n, "%.1f s", v);
        break;
    case F_HZ: snprintf(out, n, v < 10 ? "%.2f Hz" : "%.1f Hz", v); break;
    case F_CENT: snprintf(out, n, lrintf(v) ? "%+d cent" : "%d cent", (int)lrintf(v)); break;
    case F_SEMI: snprintf(out, n, lrintf(v) ? "%+d semitoni" : "%d semitoni", (int)lrintf(v)); break;
    case F_STEPS: snprintf(out, n, "%d/16", (int)lrintf(v)); break;
    case F_MIX: snprintf(out, n, "%d / %d", (int)lrintf((1 - v) * 100), (int)lrintf(v * 100)); break;
    case F_CUTOFF: {
        float hz = 20.0f * exp2f(v * 9.813781f);
        if (hz >= 1000) snprintf(out, n, "%.1f kHz", hz / 1000);
        else snprintf(out, n, "%d Hz", (int)lrintf(hz));
        break;
    }
    }
    for (char *c = out; *c; c++) if (*c == '.') *c = ',';      /* virgola decimale */
}

static float param_fraction(App *a, const Param *pr)
{
    if (pr->kind == K_LIN || pr->kind == K_EXP) {
        float v = *(float *)field_ptr(a, pr->target, pr->off);
        if (pr->kind == K_EXP) return logf(fmaxf(v, pr->min) / pr->min) / logf(pr->max / pr->min);
        return (v - pr->min) / (pr->max - pr->min);
    }
    if (pr->kind == K_TEMPO) return (synth_tempo(a->s) - 40) / 200.0f;
    if (pr->kind == K_SWING) return synth_swing(a->s) / 0.6f;
    return -1.0f;
}

static void param_adjust(App *a, const Param *pr, int dir, int coarse)
{
    switch (pr->kind) {
    case K_HEADER: return;
    case K_PRESET: app_load_preset(a, (a->preset + dir + PRESET_COUNT) % PRESET_COUNT); return;
    case K_ROOT: a->root = (a->root + dir + 12) % 12; apply_scale(a); return;
    case K_SCALE: a->scale = (a->scale + dir + SCALE_COUNT) % SCALE_COUNT; apply_scale(a); return;
    case K_TEMPO: synth_set_tempo(a->s, synth_tempo(a->s) + dir * (coarse ? 10.0f : 1.0f)); return;
    case K_SWING: synth_set_swing(a->s, synth_swing(a->s) + dir * 0.05f); return;
    case K_WAVE: { int *w = field_ptr(a, pr->target, pr->off); *w = (*w + dir + WAVE_COUNT) % WAVE_COUNT; a->modified = 1; return; }
    case K_BOOL: { int *b = field_ptr(a, pr->target, pr->off); *b = !*b; a->modified = 1; synth_all_notes_off(a->s); return; }
    case K_EXP: {
        float *v = field_ptr(a, pr->target, pr->off);
        float f = coarse ? 1.7f : 1.15f;
        float nv = dir > 0 ? fmaxf(*v, pr->min) * f : *v / f;
        *v = nv < pr->min ? pr->min : (nv > pr->max ? pr->max : nv);
        a->modified = 1;
        return;
    }
    default: {
        float *v = field_ptr(a, pr->target, pr->off);
        float nv = *v + dir * pr->step * (coarse ? 5.0f : 1.0f);
        nv = roundf(nv / pr->step) * pr->step;
        *v = nv < pr->min ? pr->min : (nv > pr->max ? pr->max : nv);
        a->modified = 1;
    }
    }
}

static void move_selection(App *a, int dir)
{
    int i = a->sel;
    do { i += dir; } while (i >= 0 && i < PARAM_COUNT && PARAMS[i].kind == K_HEADER);
    if (i >= 0 && i < PARAM_COUNT) a->sel = i;
}

/* ------------------------------------------------------------------ input */
void app_set_screen(App *a, int screen)
{
    if (screen == a->screen) return;
    release_all(a);
    a->screen = (screen % SCREEN_COUNT + SCREEN_COUNT) % SCREEN_COUNT;
}

static void seq_button(App *a, int b)
{
    Pattern *p = synth_pattern(a->s);
    int x = a->cx, row = a->cy;
    switch (b) {
    case BTN_LEFT: a->cx = (a->cx + SY_STEPS - 1) % SY_STEPS; break;
    case BTN_RIGHT: a->cx = (a->cx + 1) % SY_STEPS; break;
    case BTN_UP: a->cy = (a->cy + DRUM_COUNT) % (DRUM_COUNT + 1); break;
    case BTN_DOWN: a->cy = (a->cy + 1) % (DRUM_COUNT + 1); break;
    case BTN_A:
        if (row < DRUM_COUNT) {
            p->drum[row][x] = p->drum[row][x] ? 0 : 1;
            if (p->drum[row][x] && !synth_playing(a->s)) synth_drum_hit(a->s, row, 0.72f);
        } else {
            if (p->note[x] < 0) {
                int prev = 0;
                for (int i = 1; i <= SY_STEPS; i++) { int j = (x - i + SY_STEPS) % SY_STEPS; if (p->note[j] >= 0) { prev = p->note[j]; break; } }
                p->note[x] = (int8_t)prev;
                if (!synth_playing(a->s)) { int m = synth_degree_to_midi(a->s, prev); synth_note_on(a->s, m, 0.8f); synth_note_off(a->s, m); }
            } else {
                p->note[x] = -1;
            }
        }
        break;
    case BTN_B:
        if (row < DRUM_COUNT) p->drum[row][x] = p->drum[row][x] == 2 ? 1 : 2;
        else p->accent[x] = !p->accent[x];
        break;
    case BTN_X:
    case BTN_Y:
        if (row == DRUM_COUNT) {
            int d = p->note[x] < 0 ? 0 : p->note[x] + (b == BTN_X ? 1 : -1);
            d = d < 0 ? 0 : (d > 15 ? 15 : d);
            p->note[x] = (int8_t)d;
            if (!synth_playing(a->s)) { int m = synth_degree_to_midi(a->s, d); synth_note_on(a->s, m, 0.8f); synth_note_off(a->s, m); }
        }
        break;
    case BTN_L1: synth_set_tempo(a->s, synth_tempo(a->s) - 1); break;
    case BTN_R1: synth_set_tempo(a->s, synth_tempo(a->s) + 1); break;
    case BTN_L2: synth_set_swing(a->s, synth_swing(a->s) - 0.05f); break;
    case BTN_R2: synth_set_swing(a->s, synth_swing(a->s) + 0.05f); break;
    }
}

static void sound_button(App *a, int b)
{
    switch (b) {
    case BTN_UP: move_selection(a, -1); break;
    case BTN_DOWN: move_selection(a, 1); break;
    case BTN_LEFT: param_adjust(a, &PARAMS[a->sel], -1, a->down[BTN_L1] || a->down[BTN_R1]); break;
    case BTN_RIGHT: param_adjust(a, &PARAMS[a->sel], 1, a->down[BTN_L1] || a->down[BTN_R1]); break;
    case BTN_A: if (!a->audition) { a->audition = play_midi(a, 0); synth_note_on(a->s, a->audition, 0.85f); } break;
    case BTN_B: app_set_screen(a, SCREEN_PLAY); break;
    }
}

static int repeats(int screen, int b)
{
    if (screen == SCREEN_SOUND) return b == BTN_UP || b == BTN_DOWN || b == BTN_LEFT || b == BTN_RIGHT;
    if (screen == SCREEN_SEQ) return b == BTN_UP || b == BTN_DOWN || b == BTN_LEFT || b == BTN_RIGHT || b == BTN_L1 || b == BTN_R1;
    return 0;
}

static void handle_press(App *a, int b)
{
    if (a->quit_dialog) {
        if (b == BTN_A) { a->quit = 1; release_all(a); }
        else if (b == BTN_B || b == BTN_MENU) a->quit_dialog = 0;
        return;
    }
    if (b == BTN_MENU) { a->quit_dialog = 1; release_all(a); return; }
    if (b == BTN_SELECT) { app_set_screen(a, a->screen + 1); return; }
    if (b == BTN_START) { synth_play(a->s, !synth_playing(a->s)); return; }
    if (a->screen == SCREEN_PLAY) {
        for (int i = 0; i < 8; i++) if (NOTE_BTNS[i] == b) { press_note(a, i); return; }
        if (b == BTN_L1 && a->octave > 1) a->octave--;
        if (b == BTN_R1 && a->octave < 7) a->octave++;
    } else if (a->screen == SCREEN_SOUND) {
        sound_button(a, b);
    } else {
        seq_button(a, b);
    }
}

void app_button(App *a, int b, int pressed)
{
    if (b < 0 || b >= BTN_COUNT) return;
    if (pressed) {
        if (a->down[b]) return;
        a->down[b] = 1;
        a->rep[b] = 0.0f;
        handle_press(a, b);
    } else {
        if (!a->down[b]) return;
        a->down[b] = 0;
        if (a->screen == SCREEN_PLAY && !a->quit_dialog) {
            for (int i = 0; i < 8; i++) if (NOTE_BTNS[i] == b && a->btn_count[i]) release_note(a, i);
            if (b == BTN_L2) release_sustain(a);
        }
        if (a->screen == SCREEN_SOUND && b == BTN_A && a->audition) { synth_note_off(a->s, a->audition); a->audition = 0; }
    }
}

static float deadzone(float v)
{
    if (fabsf(v) < 0.15f) return 0.0f;
    return (v - (v > 0 ? 0.15f : -0.15f)) / 0.85f;
}

void app_axes(App *a, float lx, float ly, float rx, float ry, float l2, float r2)
{
    a->lx = deadzone(lx);
    a->ly = deadzone(ly);
    a->rx = deadzone(rx);
    a->ry = deadzone(ry);
    a->l2 = l2;
    a->r2 = r2;
    synth_set_performance(a->s, a->lx * 2.0f, -a->ly * 3.0f, a->rx * 0.3f, a->ry < 0 ? -a->ry : 0.0f);
}

void app_update(App *a, float dt)
{
    a->time += dt;
    a->note_glow = fmaxf(0.0f, a->note_glow - dt * 1.5f);
    for (int d = 0; d < DRUM_COUNT; d++) {
        uint32_t f = synth_drum_flash(a->s, d);
        if (f != a->drum_seen[d]) { a->drum_seen[d] = f; a->drum_glow[d] = 1.0f; }
        a->drum_glow[d] = fmaxf(0.0f, a->drum_glow[d] - dt * 4.0f);
    }
    if (a->quit_dialog) return;
    for (int b = 0; b < BTN_COUNT; b++) {
        if (!a->down[b] || !repeats(a->screen, b)) continue;
        a->rep[b] += dt;
        while (a->rep[b] > 0.38f) {
            a->rep[b] -= 0.06f;
            handle_press(a, b);
        }
    }
    /* scorrimento morbido della lista dei parametri */
    float target = (float)a->sel - 5.0f;
    if (target < 0) target = 0;
    if (target > PARAM_COUNT - 12) target = (float)(PARAM_COUNT - 12);
    a->scroll += (target - a->scroll) * fminf(1.0f, dt * 12.0f);
}

/* ------------------------------------------------------------------ disegno */
static void draw_cone(Canvas *c, float x, float y, float s)
{
    for (int i = -2; i <= 2; i++)
        gfx_line(c, x, y + 8 * s, x + i * 4.5f * s, y + (1 + abs(i) * 1.5f) * s, 1.4f * s, C_GREEN, 0.9f);
    gfx_round_rect(c, x - 5 * s, y - 7 * s, 10 * s, 15 * s, 5 * s, C_VIOLET, 1.0f);
    gfx_line(c, x - 4 * s, y - 2 * s, x + 4 * s, y + 3 * s, 1.0f, C_VIOLET_D, 0.9f);
    gfx_line(c, x + 4 * s, y - 2 * s, x - 4 * s, y + 3 * s, 1.0f, C_VIOLET_D, 0.9f);
}

static void draw_header(App *a, Canvas *c)
{
    gfx_rect(c, 0, 0, c->w, 46, RGB(16, 20, 28));
    gfx_rect(c, 0, 46, c->w, 1, C_LINE);
    draw_cone(c, 26, 22, 1.3f);
    gfx_text(c, FONT_TITLE, 48, 31, "Cirmolo Synth", C_TEXT);
    static const char *tabs[SCREEN_COUNT] = { "Suona", "Suono", "Sequenza" };
    int x = 236;
    for (int i = 0; i < SCREEN_COUNT; i++) {
        int w = gfx_text_width(FONT_BOLD, tabs[i]) + 24;
        if (i == a->screen) gfx_round_rect(c, x, 10, w, 27, 13.5f, C_VIOLET_D, 1.0f);
        gfx_text_center(c, FONT_BOLD, x + w / 2, 29, tabs[i], i == a->screen ? C_TEXT : C_MUTED);
        x += w + 6;
    }
    char t[32];
    snprintf(t, sizeof(t), "%d BPM", (int)lrintf(synth_tempo(a->s)));
    int playing = synth_playing(a->s);
    gfx_text_right(c, FONT_BOLD, 594, 29, t, playing ? C_TEXT : C_MUTED);
    if (playing) {
        int st = synth_current_step(a->s);
        float pulse = (st >= 0 && st % 4 == 0) ? 1.0f : 0.55f;
        gfx_circle(c, 614, 23, 7, C_GREEN, pulse);
    } else {
        gfx_rect(c, 608, 17, 12, 12, C_DIM);
    }
}

static void draw_hints(Canvas *c, const char *text)
{
    gfx_rect(c, 0, 446, c->w, 34, RGB(16, 20, 28));
    gfx_rect(c, 0, 446, c->w, 1, C_LINE);
    gfx_text_center(c, FONT_SMALL, c->w / 2, 468, text, C_MUTED);
}

static void draw_scope(App *a, Canvas *c, int x, int y, int w, int h, uint32_t color)
{
    gfx_round_rect(c, x, y, w, h, 10, RGB(10, 13, 18), 1.0f);
    gfx_line(c, x + 8, y + h / 2.0f, x + w - 8, y + h / 2.0f, 1.0f, C_LINE, 0.8f);
    int n = w - 16;
    if (n > 600) n = 600;
    synth_scope(a->s, a->scope, n * 2);
    int start = 0;
    for (int i = 1; i < n; i++)
        if (a->scope[i - 1] <= 0.0f && a->scope[i] > 0.0f) { start = i; break; }
    float peak = 0.05f;
    for (int i = 0; i < n; i++) peak = fmaxf(peak, fabsf(a->scope[start + i]));
    float gain = (h * 0.42f) / fmaxf(peak, 0.25f);
    float px = 0, py = 0;
    for (int i = 0; i < n; i++) {
        float xx = x + 8 + i, yy = y + h / 2.0f - a->scope[start + i] * gain;
        if (i) {
            gfx_line(c, px, py, xx, yy, 4.0f, color, 0.18f);
            gfx_line(c, px, py, xx, yy, 1.6f, color, 1.0f);
        }
        px = xx;
        py = yy;
    }
}

static void draw_button_cluster(App *a, Canvas *c, float cx, float cy, const int *btns, int first, const char **labels)
{
    static const float off[4][2] = { { -1, 0 }, { 0, 1 }, { 1, 0 }, { 0, -1 } };     /* ovest, sud, est, nord */
    gfx_circle(c, cx, cy, 80, C_PANEL, 1.0f);
    for (int i = 0; i < 4; i++) {
        int idx = first + i;
        float x = cx + off[i][0] * 50, y = cy + off[i][1] * 50;
        int on = a->btn_count[idx] > 0;
        char name[16];
        note_label(name, sizeof(name), play_midi(a, idx));
        if (on) gfx_circle(c, x, y, 31, C_VIOLET, 0.35f);
        gfx_circle(c, x, y, 26, on ? C_VIOLET : C_PANEL_HI, 1.0f);
        gfx_text_center(c, FONT_BOLD, (int)x, (int)y + 2, name, C_TEXT);
        gfx_text_center(c, FONT_SMALL, (int)x, (int)y + 17, labels[i], on ? C_TEXT : C_MUTED);
        (void)btns;
    }
}

static void draw_meter(Canvas *c, int x, int y, int w, const char *label, float v, int bipolar)
{
    gfx_text(c, FONT_SMALL, x, y, label, C_MUTED);
    float bx = x + 82, bw = w - 82;
    gfx_round_rect(c, bx, y - 9, bw, 8, 4, C_PANEL_HI, 1.0f);
    if (bipolar) {
        float mid = bx + bw / 2, end = mid + v * bw / 2;
        gfx_round_rect(c, fminf(mid, end), y - 9, fabsf(end - mid) + 2, 8, 4, C_GREEN, 1.0f);
        gfx_rect(c, (int)mid, y - 11, 1, 12, C_DIM);
    } else {
        gfx_round_rect(c, bx, y - 9, fmaxf(8, v * bw), 8, 4, v > 0 ? C_GREEN : C_PANEL_HI, 1.0f);
    }
}

static void draw_play(App *a, Canvas *c)
{
    gfx_round_rect(c, 16, 60, 236, 226, 16, C_PANEL, 1.0f);
    gfx_text(c, FONT_SMALL, 32, 86, "Nota", C_MUTED);
    const char *lbl = a->last_label[0] ? a->last_label : "-";
    uint32_t col = gfx_mix(C_TEXT, C_VIOLET, a->note_glow * 0.8f);
    gfx_text(c, a->down[BTN_R2] || gfx_text_width(FONT_BIG, lbl) > 200 ? FONT_TITLE : FONT_BIG, 32, 140, lbl, col);
    char t[96];
    snprintf(t, sizeof(t), "%s%s", PRESETS[a->preset].name, a->modified ? " *" : "");
    gfx_text(c, FONT_SMALL, 32, 176, "Preset", C_MUTED);
    gfx_text(c, FONT_BOLD, 100, 176, t, C_TEXT);
    snprintf(t, sizeof(t), "%s %s", NOTE_NAMES[a->root], SCALES[a->scale].name);
    gfx_text(c, FONT_SMALL, 32, 202, "Scala", C_MUTED);
    gfx_text(c, FONT_BOLD, 100, 202, t, C_TEXT);
    snprintf(t, sizeof(t), "%d", a->octave);
    gfx_text(c, FONT_SMALL, 32, 228, "Ottava", C_MUTED);
    gfx_text(c, FONT_BOLD, 100, 228, t, C_TEXT);
    const char *mode = a->down[BTN_R2] ? "Accordi" : (a->down[BTN_L2] ? "Tenuto" : "Note");
    gfx_text(c, FONT_SMALL, 32, 254, "Modo", C_MUTED);
    gfx_text(c, FONT_BOLD, 100, 254, mode, a->down[BTN_R2] || a->down[BTN_L2] ? C_AMBER : C_TEXT);
    snprintf(t, sizeof(t), "%d/8 voci", synth_active_voices(a->s));
    gfx_text(c, FONT_SMALL, 32, 276, t, C_DIM);

    static const char *dpad[4] = { "sinistra", "giù", "destra", "su" };
    static const char *face[4] = { "Y", "B", "A", "X" };
    draw_button_cluster(a, c, 350, 173, NULL, 0, dpad);
    draw_button_cluster(a, c, 540, 173, NULL, 4, face);

    draw_scope(a, c, 16, 298, 608, 96, C_VIOLET);
    draw_meter(c, 24, 420, 190, "Intonazione", a->lx, 1);
    draw_meter(c, 228, 420, 190, "Filtro", -a->ly, 1);
    draw_meter(c, 432, 420, 190, "Vibrato", a->ry < 0 ? -a->ry : 0, 0);
    draw_hints(c, "L1/R1 ottava · L2 tenuto · R2 accordi · levette: espressione · START sequenza · SELECT pagina");
}

static void draw_envelope(Canvas *c, int x, int y, int w, int h, float at, float de, float su, float re, uint32_t col, const char *title, int active)
{
    gfx_round_rect(c, x, y, w, h, 10, active ? C_PANEL_HI : C_PANEL, 1.0f);
    gfx_text(c, FONT_SMALL, x + 10, y + 18, title, active ? C_TEXT : C_MUTED);
    float total = at + de + 0.6f + re, gx = x + 10, gw = w - 20, gy = y + h - 10, gh = h - 34;
    float x1 = gx + gw * at / total, x2 = x1 + gw * de / total, x3 = x2 + gw * 0.6f / total, x4 = gx + gw;
    float ys = gy - gh * su;
    gfx_line(c, gx, gy, x1, gy - gh, 2.0f, col, 1.0f);
    gfx_line(c, x1, gy - gh, x2, ys, 2.0f, col, 1.0f);
    gfx_line(c, x2, ys, x3, ys, 2.0f, col, 1.0f);
    gfx_line(c, x3, ys, x4, gy, 2.0f, col, 1.0f);
}

static void draw_sound(App *a, Canvas *c)
{
    gfx_round_rect(c, 16, 58, 384, 382, 14, C_PANEL, 1.0f);
    int row_h = 30, top = 66, visible = 12;
    float sc = a->scroll;
    int first = (int)floorf(sc);
    for (int i = first; i < first + visible + 1 && i < PARAM_COUNT; i++) {
        float y = top + (i - sc) * row_h;
        if (y < top - 4 || y > top + (visible - 1) * row_h + 2) continue;
        const Param *pr = &PARAMS[i];
        if (pr->kind == K_HEADER) {
            gfx_text(c, FONT_SMALL, 30, (int)y + 21, pr->name, C_VIOLET);
            gfx_rect(c, 30 + gfx_text_width(FONT_SMALL, pr->name) + 8, (int)y + 16, 340 - gfx_text_width(FONT_SMALL, pr->name), 1, C_LINE);
            continue;
        }
        int selected = i == a->sel;
        if (selected) gfx_round_rect(c, 22, y + 1, 372, row_h - 2, 9, C_VIOLET_D, 1.0f);
        gfx_text(c, FONT_BODY, 34, (int)y + 21, pr->name, selected ? C_TEXT : RGB(205, 208, 220));
        char v[48];
        param_text(a, pr, v, sizeof(v));
        gfx_text_right(c, FONT_BOLD, 384, (int)y + 21, v, selected ? C_TEXT : C_MUTED);
        float fr = param_fraction(a, pr);
        if (fr >= 0.0f && selected) gfx_rect(c, 34, (int)y + row_h - 5, (int)(350 * fminf(1.0f, fmaxf(0.0f, fr))), 2, C_AMBER);
    }
    /* pannello destro: oscilloscopio e inviluppi */
    draw_scope(a, c, 412, 58, 212, 120, C_GREEN);
    SynthPatch *p = synth_patch(a->s);
    int sel = a->sel;
    int in_filter = sel >= 14 && sel <= 21, in_amp = sel >= 23 && sel <= 26;
    draw_envelope(c, 412, 188, 212, 118, p->a_attack, p->a_decay, p->a_sustain, p->a_release, C_VIOLET, "Inviluppo ampiezza", in_amp);
    draw_envelope(c, 412, 316, 212, 124, p->f_attack, p->f_decay, p->f_sustain, p->f_release, C_GREEN, "Inviluppo filtro", in_filter);
    draw_hints(c, "su/giù scegli · sinistra/destra cambia (con L1 o R1: di più) · A prova · B torna a Suona");
}

static void draw_seq(App *a, Canvas *c)
{
    Pattern *p = synth_pattern(a->s);
    int gx = 136, gy = 64, cw = 30, ch = 42;
    int step = synth_current_step(a->s);
    for (int i = 0; i < SY_STEPS; i++) {
        int x = gx + i * cw;
        if (i % 4 == 0) gfx_rect_alpha(c, x, gy - 4, cw * 4 - 2, ch * (DRUM_COUNT + 1) + 6, C_PANEL, 0.55f);
        if (i == step) gfx_round_rect(c, x + 4, gy - 11, cw - 8, 5, 2.5f, C_AMBER, 1.0f);
    }
    for (int r = 0; r <= DRUM_COUNT; r++) {
        int y = gy + r * ch;
        const char *name = r < DRUM_COUNT ? DRUM_NAMES[r] : "Melodia";
        float glow = r < DRUM_COUNT ? a->drum_glow[r] : 0.0f;
        uint32_t col = r < DRUM_COUNT ? C_GREEN : C_VIOLET;
        gfx_text_right(c, FONT_BODY, gx - 12, y + 26, name, gfx_mix(r == a->cy ? C_TEXT : C_MUTED, col, glow));
        for (int i = 0; i < SY_STEPS; i++) {
            int x = gx + i * cw;
            int on, acc;
            if (r < DRUM_COUNT) { on = p->drum[r][i] > 0; acc = p->drum[r][i] == 2; }
            else { on = p->note[i] >= 0; acc = p->accent[i]; }
            uint32_t fill = on ? (acc ? col : gfx_mix(col, C_PANEL, 0.35f)) : C_PANEL_HI;
            if (i == step) fill = gfx_mix(fill, C_TEXT, on ? 0.45f : 0.12f);
            gfx_round_rect(c, x + 2, y + 3, cw - 4, ch - 6, 7, fill, 1.0f);
            if (r == DRUM_COUNT && on) {
                int m = synth_degree_to_midi(a->s, p->note[i]);
                gfx_text_center(c, FONT_SMALL, x + cw / 2, y + 22, NOTE_NAMES[m % 12], C_TEXT);
                char o[4];
                snprintf(o, sizeof(o), "%d", m / 12 - 1);
                gfx_text_center(c, FONT_SMALL, x + cw / 2, y + 35, o, RGB(220, 210, 255));
            } else if (acc && on) {
                gfx_circle(c, x + cw / 2.0f, y + ch / 2.0f, 4, C_TEXT, 0.9f);
            }
            if (r == a->cy && i == a->cx) gfx_round_frame(c, x, y + 1, cw, ch - 2, 8, 2.5f, C_AMBER, 1.0f);
        }
    }
    char t[128];
    snprintf(t, sizeof(t), "Tempo %d BPM   ·   Swing %d %%   ·   %s", (int)lrintf(synth_tempo(a->s)), (int)lrintf(synth_swing(a->s) * 100),
             synth_playing(a->s) ? "in riproduzione" : "fermo");
    gfx_text_center(c, FONT_BOLD, c->w / 2, 340, t, C_TEXT);
    draw_scope(a, c, 16, 352, 608, 84, C_GREEN);
    draw_hints(c, "A attiva · B accento · X/Y nota su/giù · L1/R1 tempo · L2/R2 swing · START play/stop");
}

static void draw_quit(Canvas *c)
{
    gfx_rect_alpha(c, 0, 0, c->w, c->h, RGB(0, 0, 0), 0.6f);
    gfx_round_rect(c, 150, 170, 340, 140, 18, C_PANEL_HI, 1.0f);
    gfx_round_frame(c, 150, 170, 340, 140, 18, 2, C_VIOLET, 1.0f);
    gfx_text_center(c, FONT_TITLE, 320, 222, "Uscire dal synth?", C_TEXT);
    gfx_text_center(c, FONT_BODY, 320, 256, "Suoni e sequenza restano salvati.", C_MUTED);
    gfx_text_center(c, FONT_BOLD, 320, 290, "A esci   ·   B resta", C_AMBER);
}

void app_draw(App *a, Canvas *c)
{
    gfx_vgradient(c, 0, 0, c->w, c->h, C_BG_TOP, C_BG_BOT);
    draw_header(a, c);
    if (a->screen == SCREEN_PLAY) draw_play(a, c);
    else if (a->screen == SCREEN_SOUND) draw_sound(a, c);
    else draw_seq(a, c);
    if (a->quit_dialog) draw_quit(c);
}
