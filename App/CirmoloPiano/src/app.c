/* Cirmolo Piano - logica dell'app e interfaccia (vedi app.h).
 *
 * Tre pagine: Suona (strumento, oscilloscopio, tastiera disegnata, tastierina dei tasti della Flip),
 * Strumenti (elenco scorrevole dei preset del SoundFont, per famiglie General MIDI) e Opzioni
 * (volume, ottava, tonalita', scala, forza dei tasti). Una tastiera MIDI USB suona direttamente sul
 * motore; i tasti della Flip suonano otto gradi di una scala sul canale 1.
 */
#include "app.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "midi.h"

/* ------------------------------------------------------------------ palette */
#define C_BG_TOP   RGB(13, 16, 22)
#define C_BG_BOT   RGB(18, 23, 32)
#define C_BAR      RGB(16, 20, 28)
#define C_PANEL    RGB(25, 31, 42)
#define C_PANEL_HI RGB(34, 42, 57)
#define C_LINE     RGB(48, 58, 78)
#define C_TEXT     RGB(236, 236, 244)
#define C_MUTED    RGB(138, 148, 170)
#define C_DIM      RGB(88, 98, 120)
#define C_VIOLET   RGB(150, 116, 242)
#define C_VIOLET_L RGB(196, 176, 250)
#define C_VIOLET_D RGB(84, 60, 160)
#define C_GREEN    RGB(92, 190, 126)
#define C_AMBER    RGB(244, 186, 92)
#define C_RED      RGB(236, 96, 104)
#define C_KEY_W    RGB(214, 218, 228)
#define C_KEY_W_D  RGB(168, 172, 184)
#define C_KEY_B    RGB(30, 34, 44)

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
static const int NOTE_BTNS[8] = { PAD_LEFT, PAD_DOWN, PAD_RIGHT, PAD_UP, PAD_Y, PAD_B, PAD_A, PAD_X };
static const char *NOTE_BTN_LABELS[8] = { "sinistra", "giù", "destra", "su", "Y", "B", "A", "X" };

/* Le 16 famiglie del General MIDI (8 programmi ciascuna). */
static const char *GM_FAMILIES[16] = {
    "Pianoforti", "Percussioni cromatiche", "Organi", "Chitarre", "Bassi", "Archi", "Insiemi", "Ottoni",
    "Ance", "Flauti", "Synth lead", "Synth pad", "Effetti synth", "Etnici", "Percussioni", "Effetti sonori",
};

#define MAIN_CH 0                        /* canale dei tasti della Flip e dello strumento corrente */
#define KEY_LO 36                        /* tastiera disegnata: Do2..Do7 */
#define KEY_HI 96
#define KEY_X0 14
#define KEY_W 17

/* ------------------------------------------------------------------ opzioni */
enum { OPT_VOLUME, OPT_OCTAVE, OPT_ROOT, OPT_SCALE, OPT_VELOCITY, OPT_COUNT };
static const char *OPT_NAMES[OPT_COUNT] = { "Volume", "Ottava della tastierina", "Tonalità", "Scala", "Forza dei tasti" };

/* ------------------------------------------------------------------ stato */
typedef struct { int header; int preset; const char *label; } Row;   /* riga della pagina Strumenti */

struct App {
    Piano *p;
    char path[512];
    char sf2[512];
    int page, quit_dialog, quit;
    /* strumento corrente e sua posizione nell'ordine per banco/programma */
    int preset;
    int *order, count;                   /* preset in ordine (banco 0, altri banchi, batterie) */
    int *pos_of;                         /* posizione di ogni preset in order[] */
    Row *rows;                           /* righe della pagina Strumenti, con le intestazioni */
    int row_count;
    int *row_of;                         /* riga di ogni preset */
    int sel, scroll;                     /* pagina Strumenti */
    int opt_sel;                         /* pagina Opzioni */
    /* tastierina della Flip */
    float volume, velocity;
    int octave, root, scale;
    int down[PAD_COUNT];
    int btn_note[8];                     /* nota suonata da ogni tasto, -1 se non suona */
    int chord[4], chord_n;               /* accordo di prova (START) e nota di prova (A nelle liste) */
    float chord_t;
    float rep[PAD_COUNT];
    float lx, ry;
    int last_bend, last_mod;
    /* tasti accesi sulla tastiera disegnata: quante sorgenti tengono ogni nota */
    uint8_t lit[128], lit_drum[128];
    /* tastiera MIDI */
    char midi_name[64];
    float midi_flash;
    /* interfaccia */
    char last_label[48];
    float note_glow;
    char toast[96];
    float toast_t;
    float time;
    float scope[1200];
    uint32_t drawn_key;
    float drawn_time;
};

static void toast(App *a, const char *msg)
{
    snprintf(a->toast, sizeof(a->toast), "%s", msg);
    a->toast_t = 2.5f;
}

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* ------------------------------------------------------------------ elenco dei preset */
/* Ordine per banco e programma (le batterie, banco 128, in fondo). */
static int preset_after(const Piano *p, int i, int j)
{
    int bi = piano_preset_bank(p, i), bj = piano_preset_bank(p, j);
    if (bi != bj) return bi > bj;
    int pi = piano_preset_program(p, i), pj = piano_preset_program(p, j);
    if (pi != pj) return pi > pj;
    return i > j;
}

/* qsort_r non e' portabile: ordinamento per inserzione, i preset sono poche centinaia */
static void sort_presets(const Piano *p, int *v, int n)
{
    for (int i = 1; i < n; i++) {
        int k = v[i], j = i - 1;
        while (j >= 0 && preset_after(p, v[j], k)) { v[j + 1] = v[j]; j--; }
        v[j + 1] = k;
    }
}

static void build_rows(App *a)
{
    int n = piano_preset_count(a->p);
    a->count = n;
    a->order = calloc((size_t)(n > 0 ? n : 1), sizeof(int));
    a->pos_of = calloc((size_t)(n > 0 ? n : 1), sizeof(int));
    a->row_of = calloc((size_t)(n > 0 ? n : 1), sizeof(int));
    a->rows = calloc((size_t)n + 64, sizeof(Row));
    if (!a->order || !a->pos_of || !a->row_of || !a->rows) { a->count = 0; a->row_count = 0; return; }
    for (int i = 0; i < n; i++) a->order[i] = i;
    sort_presets(a->p, a->order, n);
    int r = 0, last_group = -1;
    static char bank_labels[48][32];
    int bank_label_n = 0;
    for (int k = 0; k < n; k++) {
        int i = a->order[k], bank = piano_preset_bank(a->p, i), prog = piano_preset_program(a->p, i);
        a->pos_of[i] = k;
        int group = bank == 0 ? prog / 8 : (bank >= 128 ? 1000 : 100 + bank);
        if (group != last_group && r < n + 63) {
            const char *label;
            if (bank == 0) label = GM_FAMILIES[prog / 8];
            else if (bank >= 128) label = "Batterie";
            else {
                if (bank_label_n < 48) {
                    snprintf(bank_labels[bank_label_n], sizeof(bank_labels[0]), "Variazioni GS · banco %d", bank);
                    label = bank_labels[bank_label_n++];
                } else label = "Variazioni GS";
            }
            a->rows[r++] = (Row){ 1, -1, label };
            last_group = group;
        }
        a->row_of[i] = r;
        a->rows[r++] = (Row){ 0, i, piano_preset_name(a->p, i) };
    }
    a->row_count = r;
}

static int gm_family(const App *a, int preset)
{
    if (piano_preset_bank(a->p, preset) >= 128) return -1;
    return piano_preset_program(a->p, preset) / 8;
}

/* ------------------------------------------------------------------ strumento */
void app_set_instrument(App *a, int idx)
{
    if (idx < 0 || idx >= a->count) return;
    a->preset = idx;
    for (int c = 0; c < PIANO_CHANNELS; c++)
        if (c != PIANO_DRUM_CHANNEL) piano_set_preset(a->p, c, idx);
    if (a->row_of) a->sel = a->row_of[idx];
}

int app_instrument(const App *a) { return a->preset; }

static void step_instrument(App *a, int delta)
{
    if (a->count <= 0) return;
    int pos = (a->pos_of[a->preset] + delta) % a->count;
    if (pos < 0) pos += a->count;
    app_set_instrument(a, a->order[pos]);
}

/* ------------------------------------------------------------------ salvataggio */
void app_save(App *a)
{
    if (!a->path[0]) return;
    char tmp[600];
    snprintf(tmp, sizeof(tmp), "%s.tmp", a->path);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    fprintf(f, "# Cirmolo Piano: stato salvato automaticamente\nversion=1\nbank=%d\nprogram=%d\nvolume=%g\noctave=%d\nroot=%d\nscale=%d\nvelocity=%g\n",
            piano_preset_bank(a->p, a->preset), piano_preset_program(a->p, a->preset), a->volume, a->octave, a->root, a->scale, a->velocity);
    fclose(f);
    remove(a->path);
    rename(tmp, a->path);
}

static void load_state(App *a)
{
    FILE *f = fopen(a->path, "r");
    if (!f) return;
    char line[256];
    int bank = 0, program = 0;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq || line[0] == '#') continue;
        *eq = 0;
        const char *k = line;
        char *v = eq + 1;
        v[strcspn(v, "\r\n")] = 0;
        if (!strcmp(k, "bank")) bank = atoi(v);
        else if (!strcmp(k, "program")) program = atoi(v);
        else if (!strcmp(k, "volume")) a->volume = clampf((float)atof(v), 0.0f, 1.0f);
        else if (!strcmp(k, "octave")) { int o = atoi(v); if (o >= 1 && o <= 7) a->octave = o; }
        else if (!strcmp(k, "root")) a->root = ((atoi(v) % 12) + 12) % 12;
        else if (!strcmp(k, "scale")) { int s = atoi(v); if (s >= 0 && s < SCALE_COUNT) a->scale = s; }
        else if (!strcmp(k, "velocity")) a->velocity = clampf((float)atof(v), 0.3f, 1.0f);
    }
    fclose(f);
    int idx = piano_find_preset(a->p, bank, program);
    if (idx >= 0) app_set_instrument(a, idx);
}

/* ------------------------------------------------------------------ creazione */
App *app_create(Piano *p, const char *state_path, const char *sf2_path)
{
    App *a = calloc(1, sizeof(App));
    if (!a) return NULL;
    a->p = p;
    if (state_path) snprintf(a->path, sizeof(a->path), "%s", state_path);
    if (sf2_path) snprintf(a->sf2, sizeof(a->sf2), "%s", sf2_path);
    a->volume = 0.8f;
    a->velocity = 0.85f;
    a->octave = 4;
    a->last_bend = 8192;
    for (int i = 0; i < 8; i++) a->btn_note[i] = -1;
    build_rows(a);
    if (a->count > 0) {
        int idx = piano_find_preset(p, 0, 0);
        app_set_instrument(a, idx >= 0 ? idx : a->order[0]);
    }
    if (a->path[0]) load_state(a);
    piano_set_volume(p, a->volume);
    a->sel = a->count > 0 ? a->row_of[a->preset] : 0;
    return a;
}

void app_destroy(App *a)
{
    if (!a) return;
    piano_all_notes_off(a->p);
    app_save(a);
    free(a->order);
    free(a->pos_of);
    free(a->row_of);
    free(a->rows);
    free(a);
}

int app_wants_quit(const App *a) { return a->quit; }
int app_page(const App *a) { return a->page; }
float app_volume(const App *a) { return a->volume; }
int app_octave(const App *a) { return a->octave; }
int app_key_lit(const App *a, int key) { return key >= 0 && key < 128 && (a->lit[key] || a->lit_drum[key]); }

/* ------------------------------------------------------------------ note */
static void note_label(char *out, size_t n, int midi) { snprintf(out, n, "%s%d", NOTE_NAMES[midi % 12], midi / 12 - 1); }

static int degree_to_midi(const App *a, int degree)
{
    const Scale *sc = &SCALES[a->scale];
    int oct = degree / sc->n, idx = degree % sc->n;
    int m = 12 * (a->octave + 1) + a->root + 12 * oct + sc->iv[idx];
    return m < 0 ? 0 : (m > 127 ? 127 : m);
}

static void lit_on(App *a, int ch, int key)
{
    if (key < 0 || key > 127) return;
    uint8_t *v = ch == PIANO_DRUM_CHANNEL ? a->lit_drum : a->lit;
    if (v[key] < 255) v[key]++;
}

static void lit_off(App *a, int ch, int key)
{
    if (key < 0 || key > 127) return;
    uint8_t *v = ch == PIANO_DRUM_CHANNEL ? a->lit_drum : a->lit;
    if (v[key]) v[key]--;
}

static void sound_note(App *a, int ch, int key, float vel)
{
    piano_note_on(a->p, ch, key, vel);
    lit_on(a, ch, key);
    note_label(a->last_label, sizeof(a->last_label), key);
    a->note_glow = 1.0f;
}

static void chord_release(App *a)
{
    for (int i = 0; i < a->chord_n; i++) { piano_note_off(a->p, MAIN_CH, a->chord[i]); lit_off(a, MAIN_CH, a->chord[i]); }
    a->chord_n = 0;
    a->chord_t = 0.0f;
}

/* Accordo di prova: primo, terzo, quinto grado e ottava della scala, tenuti un secondo e mezzo. */
static void chord_play(App *a)
{
    chord_release(a);
    static const int deg[4] = { 0, 2, 4 };
    for (int i = 0; i < 3; i++) a->chord[i] = degree_to_midi(a, deg[i]);
    a->chord[3] = degree_to_midi(a, SCALES[a->scale].n);
    a->chord_n = 4;
    for (int i = 0; i < 4; i++) sound_note(a, MAIN_CH, a->chord[i], a->velocity);
    snprintf(a->last_label, sizeof(a->last_label), "%s %s", NOTE_NAMES[a->root], SCALES[a->scale].name);
    a->chord_t = 1.5f;
}

/* Nota di prova (A nelle pagine Strumenti e Opzioni): il primo grado, per quattro decimi di secondo. */
static void audition(App *a)
{
    chord_release(a);
    a->chord[0] = degree_to_midi(a, 0);
    a->chord_n = 1;
    sound_note(a, MAIN_CH, a->chord[0], a->velocity);
    a->chord_t = 0.4f;
}

static void press_note(App *a, int idx)
{
    int m = degree_to_midi(a, idx);
    if (a->btn_note[idx] >= 0) { piano_note_off(a->p, MAIN_CH, a->btn_note[idx]); lit_off(a, MAIN_CH, a->btn_note[idx]); }
    a->btn_note[idx] = m;
    sound_note(a, MAIN_CH, m, a->velocity);
}

static void release_note(App *a, int idx)
{
    if (a->btn_note[idx] < 0) return;
    piano_note_off(a->p, MAIN_CH, a->btn_note[idx]);
    lit_off(a, MAIN_CH, a->btn_note[idx]);
    a->btn_note[idx] = -1;
}

static void release_all(App *a)
{
    for (int i = 0; i < 8; i++) release_note(a, i);
    chord_release(a);
    piano_all_notes_off(a->p);
    memset(a->lit, 0, sizeof(a->lit));
    memset(a->lit_drum, 0, sizeof(a->lit_drum));
}

/* ------------------------------------------------------------------ opzioni */
static void opt_text(const App *a, int i, char *out, size_t n)
{
    switch (i) {
    case OPT_VOLUME: snprintf(out, n, "%d %%", (int)lrintf(a->volume * 100)); break;
    case OPT_OCTAVE: snprintf(out, n, "%d  (%s%d)", a->octave, NOTE_NAMES[a->root], a->octave); break;
    case OPT_ROOT: snprintf(out, n, "%s", NOTE_NAMES[a->root]); break;
    case OPT_SCALE: snprintf(out, n, "%s", SCALES[a->scale].name); break;
    case OPT_VELOCITY: snprintf(out, n, "%d %%", (int)lrintf(a->velocity * 100)); break;
    default: out[0] = 0;
    }
}

static float opt_fraction(const App *a, int i)
{
    if (i == OPT_VOLUME) return a->volume;
    if (i == OPT_VELOCITY) return (a->velocity - 0.3f) / 0.7f;
    return -1.0f;
}

static void opt_adjust(App *a, int i, int dir)
{
    switch (i) {
    case OPT_VOLUME: a->volume = clampf(roundf((a->volume + dir * 0.05f) * 20.0f) / 20.0f, 0.0f, 1.0f); piano_set_volume(a->p, a->volume); break;
    case OPT_OCTAVE: a->octave = a->octave + dir < 1 ? 1 : (a->octave + dir > 7 ? 7 : a->octave + dir); break;
    case OPT_ROOT: a->root = (a->root + dir + 12) % 12; break;
    case OPT_SCALE: a->scale = (a->scale + dir + SCALE_COUNT) % SCALE_COUNT; break;
    case OPT_VELOCITY: a->velocity = clampf(roundf((a->velocity + dir * 0.05f) * 20.0f) / 20.0f, 0.3f, 1.0f); break;
    }
}

/* ------------------------------------------------------------------ input */
void app_set_page(App *a, int page)
{
    page = (page % PAGE_COUNT + PAGE_COUNT) % PAGE_COUNT;
    if (page == a->page) return;
    for (int i = 0; i < 8; i++) release_note(a, i);
    a->page = page;
    if (page == PAGE_INSTR && a->count > 0) a->sel = a->row_of[a->preset];
}

static void move_sel(App *a, int dir, int steps)
{
    if (a->row_count <= 0) return;
    int s = a->sel;
    for (int k = 0; k < steps; k++) {
        int t = s;
        do { t += dir; } while (t >= 0 && t < a->row_count && a->rows[t].header);
        if (t < 0 || t >= a->row_count) break;
        s = t;
    }
    a->sel = s;
}

static int repeats(int page, int b)
{
    if (page == PAGE_PLAY) return 0;
    return b == PAD_UP || b == PAD_DOWN || b == PAD_LEFT || b == PAD_RIGHT;
}

static void handle_press(App *a, int b)
{
    if (a->quit_dialog) {
        if (b == PAD_A || b == PAD_DOWN) { a->quit = 1; release_all(a); }
        else if (b == PAD_B || b == PAD_UP || b == PAD_MENU) a->quit_dialog = 0;
        return;
    }
    switch (b) {
    case PAD_MENU: a->quit_dialog = 1; release_all(a); return;
    case PAD_SELECT: app_set_page(a, a->page + 1); return;
    case PAD_L1: step_instrument(a, -1); return;
    case PAD_R1: step_instrument(a, 1); return;
    case PAD_L2: step_instrument(a, -8); return;
    case PAD_R2: step_instrument(a, 8); return;
    case PAD_START: chord_play(a); return;
    case PAD_L3: if (a->octave > 1) a->octave--; return;
    case PAD_R3: if (a->octave < 7) a->octave++; return;
    }
    if (a->page == PAGE_PLAY) {
        for (int i = 0; i < 8; i++) if (NOTE_BTNS[i] == b) { press_note(a, i); return; }
    } else if (a->page == PAGE_INSTR) {
        switch (b) {
        case PAD_UP: move_sel(a, -1, 1); break;
        case PAD_DOWN: move_sel(a, 1, 1); break;
        case PAD_LEFT: move_sel(a, -1, 8); break;
        case PAD_RIGHT: move_sel(a, 1, 8); break;
        case PAD_A:
            if (a->sel >= 0 && a->sel < a->row_count && !a->rows[a->sel].header) {
                app_set_instrument(a, a->rows[a->sel].preset);
                audition(a);
            }
            break;
        case PAD_B: app_set_page(a, PAGE_PLAY); break;
        }
    } else {
        switch (b) {
        case PAD_UP: a->opt_sel = (a->opt_sel + OPT_COUNT - 1) % OPT_COUNT; break;
        case PAD_DOWN: a->opt_sel = (a->opt_sel + 1) % OPT_COUNT; break;
        case PAD_LEFT: opt_adjust(a, a->opt_sel, -1); break;
        case PAD_RIGHT: opt_adjust(a, a->opt_sel, 1); break;
        case PAD_A: audition(a); break;
        case PAD_B: app_set_page(a, PAGE_PLAY); break;
        }
    }
}

void app_button(App *a, int b, int pressed)
{
    if (b < 0 || b >= PAD_COUNT) return;
    if (pressed) {
        if (a->down[b]) app_button(a, b, 0);   /* mai un tasto bloccato come premuto: prima lo rilascia */
        a->down[b] = 1;
        a->rep[b] = 0.0f;
        handle_press(a, b);
    } else {
        if (!a->down[b]) return;
        a->down[b] = 0;
        if (a->page == PAGE_PLAY && !a->quit_dialog)
            for (int i = 0; i < 8; i++) if (NOTE_BTNS[i] == b) release_note(a, i);
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
    a->ry = deadzone(ry);
    int bend = 8192 + (int)lrintf(a->lx * 8191.0f);
    int mod = a->ry < 0 ? (int)lrintf(-a->ry * 127.0f) : 0;
    if (bend != a->last_bend) { a->last_bend = bend; piano_pitch_bend(a->p, MAIN_CH, bend); }
    if (mod != a->last_mod) { a->last_mod = mod; piano_control(a->p, MAIN_CH, 1, mod); }
}

/* ------------------------------------------------------------------ MIDI */
void app_midi_status(App *a, const char *device)
{
    int was = a->midi_name[0] != 0;
    snprintf(a->midi_name, sizeof(a->midi_name), "%s", device ? device : "");
    if (!device) {
        if (was) { piano_all_notes_off(a->p); memset(a->lit, 0, sizeof(a->lit)); memset(a->lit_drum, 0, sizeof(a->lit_drum)); toast(a, "Tastiera MIDI scollegata"); }
    } else {
        char t[96];
        snprintf(t, sizeof(t), "Tastiera MIDI collegata: %s", device);
        toast(a, t);
    }
}

void app_midi(App *a, const unsigned char *m, int len)
{
    if (len < 1) return;
    int type = MIDI_TYPE(m), ch = MIDI_CHANNEL(m);
    int d1 = len > 1 ? m[1] : 0, d2 = len > 2 ? m[2] : 0;
    a->midi_flash = 1.0f;
    switch (type) {
    case MIDI_NOTE_ON:
    case MIDI_NOTE_OFF:
        if (type == MIDI_NOTE_ON && d2 > 0) sound_note(a, ch, d1, d2 / 127.0f);
        else { piano_note_off(a->p, ch, d1); lit_off(a, ch, d1); }   /* note on con velocity 0 = note off */
        break;
    case MIDI_CC:
        piano_control(a->p, ch, d1, d2);
        if (d1 == 120 || d1 == 123) memset(ch == PIANO_DRUM_CHANNEL ? a->lit_drum : a->lit, 0, 128);
        break;
    case MIDI_PITCH_BEND:
        piano_pitch_bend(a->p, ch, (d2 << 7) | d1);
        break;
    case MIDI_PROGRAM: {
        piano_program(a->p, ch, d1);
        if (ch == MAIN_CH) {
            a->preset = piano_channel_preset(a->p, MAIN_CH);
            if (a->count > 0) a->sel = a->row_of[a->preset];
            char t[96];
            snprintf(t, sizeof(t), "Programma %d: %s", d1 + 1, piano_preset_name(a->p, a->preset));
            toast(a, t);
        }
        break;
    }
    default: break;
    }
}

/* ------------------------------------------------------------------ aggiornamento */
void app_update(App *a, float dt)
{
    a->time += dt;
    a->note_glow = fmaxf(0.0f, a->note_glow - dt * 1.5f);
    a->toast_t = fmaxf(0.0f, a->toast_t - dt);
    a->midi_flash = fmaxf(0.0f, a->midi_flash - dt * 4.0f);
    if (a->chord_t > 0.0f) {
        a->chord_t -= dt;
        if (a->chord_t <= 0.0f) chord_release(a);
    }
    if (a->quit_dialog) return;
    for (int b = 0; b < PAD_COUNT; b++) {
        if (!a->down[b] || !repeats(a->page, b)) continue;
        a->rep[b] += dt;
        while (a->rep[b] > 0.38f) {
            a->rep[b] -= 0.06f;
            handle_press(a, b);
        }
    }
}

static uint32_t fnv(uint32_t h, const void *p, size_t n)
{
    const unsigned char *b = p;
    while (n--) { h ^= *b++; h *= 16777619u; }
    return h;
}

/* Si ridisegna quando qualcosa si muove (suono, luci, messaggi) o quando cambia lo stato mostrato;
   comunque una volta al secondo. */
int app_needs_draw(App *a)
{
    if (a->note_glow > 0.0f || a->toast_t > 0.0f || a->midi_flash > 0.0f || a->quit_dialog || a->chord_t > 0.0f) return 1;
    if (piano_active_voices(a->p) > 0 || piano_peak(a->p) > 0.002f) return 1;
    int v[] = { a->page, a->preset, a->sel, a->opt_sel, a->octave, a->root, a->scale };
    uint32_t h = fnv(2166136261u, v, sizeof(v));
    h = fnv(h, &a->volume, sizeof(a->volume));
    h = fnv(h, &a->velocity, sizeof(a->velocity));
    h = fnv(h, a->lit, sizeof(a->lit));
    h = fnv(h, a->lit_drum, sizeof(a->lit_drum));
    h = fnv(h, a->down, sizeof(a->down));
    h = fnv(h, a->midi_name, strlen(a->midi_name));
    h = fnv(h, a->last_label, strlen(a->last_label));
    if (h == a->drawn_key && a->time - a->drawn_time < 1.0f) return 0;
    a->drawn_key = h;
    a->drawn_time = a->time;
    return 1;
}

/* ------------------------------------------------------------------ disegno */
/* Logo: tre tasti di pianoforte, con un tasto nero in mezzo. */
static void draw_logo(Canvas *c, float x, float y, float s)
{
    gfx_round_rect(c, x - 11 * s, y - 9 * s, 22 * s, 18 * s, 3 * s, C_KEY_W, 1.0f);
    gfx_rect(c, (int)(x - 4 * s), (int)(y - 9 * s), 1, (int)(18 * s), C_KEY_W_D);
    gfx_rect(c, (int)(x + 4 * s), (int)(y - 9 * s), 1, (int)(18 * s), C_KEY_W_D);
    gfx_round_rect(c, x - 7 * s, y - 9 * s, 5 * s, 10 * s, 1.5f * s, C_VIOLET_D, 1.0f);
    gfx_round_rect(c, x + 2 * s, y - 9 * s, 5 * s, 10 * s, 1.5f * s, C_VIOLET_D, 1.0f);
}

static void draw_header(App *a, Canvas *c)
{
    gfx_rect(c, 0, 0, c->w, 46, C_BAR);
    gfx_rect(c, 0, 46, c->w, 1, C_LINE);
    draw_logo(c, 26, 23, 1.2f);
    gfx_text(c, FONT_TITLE, 48, 31, "Cirmolo Piano", C_TEXT);
    static const char *tabs[PAGE_COUNT] = { "Suona", "Strumenti", "Opzioni" };
    int x = 232;
    for (int i = 0; i < PAGE_COUNT; i++) {
        int w = gfx_text_width(FONT_BOLD, tabs[i]) + 24;
        if (i == a->page) gfx_round_rect(c, x, 10, w, 27, 13.5f, C_VIOLET_D, 1.0f);
        gfx_text_center(c, FONT_BOLD, x + w / 2, 29, tabs[i], i == a->page ? C_TEXT : C_MUTED);
        x += w + 6;
    }
    int connected = a->midi_name[0] != 0;
    gfx_circle(c, 572, 23, 5.0f, connected ? gfx_mix(C_GREEN, C_TEXT, a->midi_flash) : C_DIM, 1.0f);
    gfx_text(c, FONT_SMALL, 582, 28, "MIDI", connected ? C_TEXT : C_DIM);
}

static void draw_hints(Canvas *c, const char *text)
{
    gfx_rect(c, 0, 446, c->w, 34, C_BAR);
    gfx_rect(c, 0, 446, c->w, 1, C_LINE);
    gfx_text_center(c, FONT_SMALL, c->w / 2, 468, text, C_MUTED);
}

static void draw_scope(App *a, Canvas *c, int x, int y, int w, int h, uint32_t color)
{
    gfx_round_rect(c, x, y, w, h, 10, RGB(10, 13, 18), 1.0f);
    gfx_line(c, x + 8, y + h / 2.0f, x + w - 8, y + h / 2.0f, 1.0f, C_LINE, 0.8f);
    int n = w - 16;
    if (n > 600) n = 600;
    piano_scope(a->p, a->scope, n * 2);
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

static void draw_meter(Canvas *c, int x, int y, int w, const char *label, float v)
{
    gfx_text(c, FONT_SMALL, x, y, label, C_MUTED);
    float bx = x + 62, bw = w - 62;
    gfx_round_rect(c, bx, y - 9, bw, 8, 4, C_PANEL_HI, 1.0f);
    gfx_round_rect(c, bx, y - 9, fmaxf(8, v * bw), 8, 4, v > 0 ? C_GREEN : C_PANEL_HI, 1.0f);
}

/* Posizione orizzontale dei tasti bianchi (0..6 nell'ottava) e dei neri (fra due bianchi). */
static const int WHITE_POS[12] = { 0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6 };
static const int IS_BLACK[12] = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0 };

static int key_x(int key)
{
    int oct = (key - KEY_LO) / 12, n = key % 12;
    int wx = KEY_X0 + (oct * 7 + WHITE_POS[n]) * KEY_W;
    return IS_BLACK[n] ? wx + KEY_W - 5 : wx;
}

static void draw_keyboard(App *a, Canvas *c, int y, int h)
{
    int bh = h * 0.6f;
    int pad_keys[8];
    for (int i = 0; i < 8; i++) pad_keys[i] = degree_to_midi(a, i);
    for (int pass = 0; pass < 2; pass++) {
        for (int k = KEY_LO; k <= KEY_HI; k++) {
            int n = k % 12, black = IS_BLACK[n];
            if (black != pass) continue;
            int lit = a->lit[k] > 0, drum = a->lit_drum[k] > 0;
            int x = key_x(k);
            if (!black) {
                uint32_t col = drum ? C_AMBER : (lit ? C_VIOLET : C_KEY_W);
                gfx_round_rect(c, x + 1, y, KEY_W - 2, h, 3, col, 1.0f);
                if (!lit && !drum) gfx_rect(c, x + 1, y + h - 4, KEY_W - 2, 3, C_KEY_W_D);
                if (n == 0) {
                    char t[8];
                    snprintf(t, sizeof(t), "%d", k / 12 - 1);
                    gfx_text_center(c, FONT_SMALL, x + KEY_W / 2, y + h - 7, t, lit ? C_TEXT : C_DIM);
                }
            } else if (k < KEY_HI) {
                uint32_t col = drum ? C_AMBER : (lit ? C_VIOLET_L : C_KEY_B);
                gfx_round_rect(c, x, y, 10, bh, 2, col, 1.0f);
                if (!lit && !drum) gfx_rect(c, x + 2, y + 2, 2, bh - 8, RGB(60, 66, 80));
            }
        }
    }
    /* i tasti raggiunti dalla tastierina della Flip: un puntino verde */
    for (int i = 0; i < 8; i++) {
        int k = pad_keys[i];
        if (k < KEY_LO || k > KEY_HI) continue;
        int black = IS_BLACK[k % 12];
        gfx_circle(c, key_x(k) + (black ? 5.0f : KEY_W / 2.0f), black ? y + bh - 8 : y + h - 20, 2.5f, C_GREEN, 0.95f);
    }
}

static void draw_play(App *a, Canvas *c)
{
    /* pannello dello strumento */
    gfx_round_rect(c, 16, 58, 396, 150, 14, C_PANEL, 1.0f);
    gfx_text(c, FONT_SMALL, 32, 80, "Strumento", C_MUTED);
    char t[128];
    if (a->count > 0) {
        const char *name = piano_preset_name(a->p, a->preset);
        gfx_text(c, gfx_text_width(FONT_BIG, name) > 364 ? FONT_TITLE : FONT_BIG, 32, 122, name, C_TEXT);
        int bank = piano_preset_bank(a->p, a->preset), prog = piano_preset_program(a->p, a->preset), fam = gm_family(a, a->preset);
        if (bank >= 128) snprintf(t, sizeof(t), "Batteria · banco %d · programma %d", bank, prog + 1);
        else if (bank == 0) snprintf(t, sizeof(t), "Programma %d · %s", prog + 1, GM_FAMILIES[fam]);
        else snprintf(t, sizeof(t), "Banco %d · programma %d · variazione di %s", bank, prog + 1, GM_FAMILIES[fam]);
        gfx_text(c, FONT_SMALL, 32, 148, t, C_MUTED);
    } else {
        gfx_text(c, FONT_TITLE, 32, 122, "SoundFont non caricato", C_RED);
        snprintf(t, sizeof(t), "manca %s", a->sf2[0] ? a->sf2 : "il file .sf2");
        gfx_text(c, FONT_SMALL, 32, 148, t, C_MUTED);
    }
    int voices = piano_active_voices(a->p);
    if (a->midi_name[0]) snprintf(t, sizeof(t), "%d/%d voci  ·  %s", voices, PIANO_VOICES, a->midi_name);
    else snprintf(t, sizeof(t), "%d/%d voci  ·  nessuna tastiera MIDI", voices, PIANO_VOICES);
    gfx_text(c, FONT_SMALL, 32, 172, t, a->midi_name[0] ? C_GREEN : C_DIM);
    draw_meter(c, 32, 198, 180, "Volume", a->volume);
    snprintf(t, sizeof(t), "Ottava %d  ·  %s %s", a->octave, NOTE_NAMES[a->root], SCALES[a->scale].name);
    gfx_text_right(c, FONT_SMALL, 396, 198, t, C_MUTED);

    /* oscilloscopio con l'ultima nota */
    draw_scope(a, c, 424, 58, 200, 150, C_VIOLET);
    const char *lbl = a->last_label[0] ? a->last_label : "-";
    gfx_text(c, FONT_TITLE, 436, 84, lbl, gfx_mix(C_TEXT, C_VIOLET, a->note_glow * 0.8f));

    draw_keyboard(a, c, 220, 100);

    /* tastierina: otto tasti della Flip, con la nota che suonano */
    for (int i = 0; i < 8; i++) {
        int x = 16 + i * 76, on = a->btn_note[i] >= 0;
        gfx_round_rect(c, x, 334, 72, 58, 10, on ? C_VIOLET : C_PANEL, 1.0f);
        char name[16];
        note_label(name, sizeof(name), degree_to_midi(a, i));
        gfx_text_center(c, FONT_BOLD, x + 36, 359, name, C_TEXT);
        gfx_text_center(c, FONT_SMALL, x + 36, 380, NOTE_BTN_LABELS[i], on ? C_TEXT : C_MUTED);
    }
    draw_hints(c, "L1/R1 strumento · L2/R2 famiglia · L3/R3 ottava · START accordo · levette bend e vibrato · SELECT pagina");
}

static void draw_list_rows(App *a, Canvas *c)
{
    int y0 = 54, y1 = 440, rh = 26, vis = (y1 - y0) / rh, n = a->row_count, sel = a->sel;
    if (n <= 0) {
        gfx_text_center(c, FONT_BODY, c->w / 2, 240, "Nessuno strumento: SoundFont non caricato", C_MUTED);
        return;
    }
    if (sel - 1 >= 0 && a->rows[sel - 1].header && sel - 1 < a->scroll) a->scroll = sel - 1;
    if (sel < a->scroll) a->scroll = sel;
    if (sel >= a->scroll + vis) a->scroll = sel - vis + 1;
    if (a->scroll > n - vis) a->scroll = n - vis;
    if (a->scroll < 0) a->scroll = 0;
    for (int i = a->scroll; i < n && i < a->scroll + vis; i++) {
        int y = y0 + (i - a->scroll) * rh, base = y + rh / 2 + 6;
        const Row *r = &a->rows[i];
        if (r->header) {
            int w = gfx_text_width(FONT_BOLD, r->label);
            gfx_text(c, FONT_BOLD, 28, base, r->label, C_VIOLET);
            if (604 > 48 + w) gfx_rect(c, 38 + w, y + rh / 2, 604 - 38 - w, 1, C_LINE);
            continue;
        }
        int current = r->preset == a->preset;
        if (i == sel) gfx_round_rect(c, 16, y + 1, 608, rh - 2, 9, C_PANEL_HI, 1.0f);
        if (current) gfx_circle(c, 30, y + rh / 2.0f, 4, C_GREEN, 1.0f);
        gfx_text(c, FONT_BODY, 42, base, r->label, i == sel || current ? C_TEXT : C_MUTED);
        char v[24];
        int bank = piano_preset_bank(a->p, r->preset), prog = piano_preset_program(a->p, r->preset);
        if (bank == 0) snprintf(v, sizeof(v), "%03d", prog + 1);
        else snprintf(v, sizeof(v), "%d:%03d", bank, prog + 1);
        gfx_text_right(c, FONT_BOLD, 604, base, v, current ? C_GREEN : (i == sel ? C_AMBER : C_DIM));
    }
    if (n > vis) {                                /* barra di scorrimento */
        float h = (float)(y1 - y0) * vis / n, y = y0 + (float)(y1 - y0 - h) * a->scroll / (n - vis);
        gfx_round_rect(c, 627, y, 4, h, 2, C_LINE, 1.0f);
    }
}

static void draw_instruments(App *a, Canvas *c)
{
    draw_list_rows(a, c);
    draw_hints(c, "su/giù scegli · sinistra/destra salta di 8 · A usa lo strumento · B torna a Suona");
}

static void draw_options(App *a, Canvas *c)
{
    gfx_round_rect(c, 16, 58, 384, 190, 14, C_PANEL, 1.0f);
    int row_h = 34, top = 66;
    for (int i = 0; i < OPT_COUNT; i++) {
        int y = top + i * row_h, selected = i == a->opt_sel;
        if (selected) gfx_round_rect(c, 22, y + 1, 372, row_h - 2, 9, C_VIOLET_D, 1.0f);
        gfx_text(c, FONT_BODY, 34, y + 23, OPT_NAMES[i], selected ? C_TEXT : RGB(205, 208, 220));
        char v[48];
        opt_text(a, i, v, sizeof(v));
        gfx_text_right(c, FONT_BOLD, 384, y + 23, v, selected ? C_TEXT : C_MUTED);
        float fr = opt_fraction(a, i);
        if (fr >= 0.0f && selected) gfx_rect(c, 34, y + row_h - 5, (int)(350 * clampf(fr, 0.0f, 1.0f)), 2, C_AMBER);
    }
    draw_scope(a, c, 412, 58, 212, 190, C_GREEN);
    /* la tastierina con la scala scelta */
    gfx_round_rect(c, 16, 258, 608, 60, 12, C_PANEL, 1.0f);
    gfx_text(c, FONT_SMALL, 32, 280, "Tastierina della Flip", C_MUTED);
    char t[160] = "";
    for (int i = 0; i < 8; i++) {
        char name[16];
        note_label(name, sizeof(name), degree_to_midi(a, i));
        size_t l = strlen(t);
        snprintf(t + l, sizeof(t) - l, "%s%s", i ? "  ·  " : "", name);
    }
    gfx_text(c, FONT_BOLD, 32, 306, t, C_TEXT);
    draw_keyboard(a, c, 330, 100);
    draw_hints(c, "su/giù scegli · sinistra/destra cambia · A prova una nota · B torna a Suona");
}

static void draw_quit(Canvas *c)
{
    gfx_rect_alpha(c, 0, 0, c->w, c->h, RGB(0, 0, 0), 0.6f);
    gfx_round_rect(c, 150, 170, 340, 140, 18, C_PANEL_HI, 1.0f);
    gfx_round_frame(c, 150, 170, 340, 140, 18, 2, C_VIOLET, 1.0f);
    gfx_text_center(c, FONT_TITLE, 320, 222, "Uscire dal piano?", C_TEXT);
    gfx_text_center(c, FONT_BODY, 320, 256, "Strumento e volume restano salvati.", C_MUTED);
    gfx_text_center(c, FONT_BOLD, 320, 290, "A o giù: esci   ·   B o su: resta", C_AMBER);
}

void app_draw(App *a, Canvas *c)
{
    gfx_vgradient(c, 0, 0, c->w, c->h, C_BG_TOP, C_BG_BOT);
    draw_header(a, c);
    if (a->page == PAGE_PLAY) draw_play(a, c);
    else if (a->page == PAGE_INSTR) draw_instruments(a, c);
    else draw_options(a, c);
    if (a->toast_t > 0.0f && a->toast[0]) {
        float al = fminf(1.0f, a->toast_t * 3.0f);
        int w = gfx_text_width(FONT_BOLD, a->toast) + 36;
        gfx_round_rect(c, (c->w - w) / 2.0f, 400, w, 34, 17, C_PANEL_HI, 0.95f * al);
        gfx_round_frame(c, (c->w - w) / 2.0f, 400, w, 34, 17, 1.5f, C_VIOLET, al);
        gfx_text_center(c, FONT_BOLD, c->w / 2, 423, a->toast, gfx_mix(C_BG_BOT, C_TEXT, al));
    }
    if (a->quit_dialog) draw_quit(c);
}
