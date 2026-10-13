/* Cirmolo Synth - groovebox: logica dell'app e interfaccia (vedi gb_app.h). */
#include "gb_app.h"
#include "midi.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

/* ------------------------------------------------------------------ palette */
#define C_BG_TOP   RGB(13, 15, 21)
#define C_BG_BOT   RGB(19, 21, 31)
#define C_PANEL    RGB(26, 29, 41)
#define C_PANEL_HI RGB(37, 41, 57)
#define C_CELL     RGB(33, 37, 51)
#define C_LINE     RGB(50, 56, 76)
#define C_TEXT     RGB(238, 236, 244)
#define C_MUTED    RGB(140, 146, 168)
#define C_DIM      RGB(84, 90, 112)
#define C_VIOLET   RGB(150, 116, 242)
#define C_VIOLET_D RGB(80, 58, 156)
#define C_GREEN    RGB(96, 196, 128)
#define C_AMBER    RGB(246, 186, 88)
#define C_RED      RGB(240, 98, 96)
#define C_CYAN     RGB(92, 200, 226)
#define C_PINK     RGB(236, 120, 196)
#define C_LIME     RGB(196, 226, 84)

static const char *NOTE_NAMES[12] = { "Do", "Do#", "Re", "Re#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" };

/* I 12 tasti della pagina Live, nell'ordine delle tracce: croce (ovest, sud, est, nord), Y B A X, dorsali. */
static const int LIVE_BTNS[GB_DRUMS] = { PAD_LEFT, PAD_DOWN, PAD_RIGHT, PAD_UP, PAD_Y, PAD_B, PAD_A, PAD_X, PAD_L2, PAD_L1, PAD_R1, PAD_R2 };
static const char *LIVE_LABELS[GB_DRUMS] = { "sinistra", "giù", "destra", "su", "Y", "B", "A", "X", "L2", "L1", "R1", "R2" };

static const char *COND_NAMES[COND_COUNT] = { "Sempre", "Solo nel fill", "Non nel fill", "1° giro su 2", "2° giro su 2", "1 giro su 4", "Solo il 1° giro" };
static const char *SPEED_NAMES[4] = { "normale", "doppia", "metà", "terzine" };

static uint32_t model_color(int m)
{
    switch (m) {
    case DM_KICK_808: case DM_KICK_909: case DM_KICK_ACOUSTIC: case DM_KICK_FM: case DM_KICK_HARD: case DM_CHIP_KICK: return C_RED;
    case DM_SNARE_808: case DM_SNARE_909: case DM_SNARE_ACOUSTIC: case DM_SNARE_BRUSH: case DM_RIM: case DM_CLAP: case DM_SNAP:
    case DM_CHIP_SNARE: return C_AMBER;
    case DM_HAT_CLOSED: case DM_HAT_OPEN: case DM_HAT_909: case DM_CYMBAL: case DM_RIDE: case DM_SHAKER: case DM_TAMBOURINE:
    case DM_TRIANGLE: case DM_CHIP_HAT: return C_CYAN;
    case DM_TOM: case DM_TOM_ELECTRO: case DM_CONGA: case DM_BONGO: case DM_TABLA: return C_GREEN;
    case DM_COWBELL: case DM_CLAVE: case DM_WOODBLOCK: return C_PINK;
    default: return C_VIOLET;
    }
}

/* ------------------------------------------------------------------ parametri dei suoni e del master */
enum { K_MODEL, K_SEMI, K_UNIT, K_PAN, K_CHOKE, K_WAVE, K_BPM, K_SWING, K_DTIME, K_BIPOLAR, K_FB, K_CLOCK };
enum { T_DRUM, T_ACID, T_FOLD, T_MASTER };
typedef struct { const char *name; int kind; size_t off; } Param;

static const Param DRUM_PARAMS[] = {
    { "Strumento", K_MODEL, offsetof(GbDrum, model) },
    { "Altezza", K_SEMI, offsetof(GbDrum, tune) },
    { "Durata", K_UNIT, offsetof(GbDrum, decay) },
    { "Colore", K_UNIT, offsetof(GbDrum, tone) },
    { "Attacco", K_UNIT, offsetof(GbDrum, snap) },
    { "Spinta", K_UNIT, offsetof(GbDrum, drive) },
    { "Volume", K_UNIT, offsetof(GbDrum, level) },
    { "Pan", K_PAN, offsetof(GbDrum, pan) },
    { "Gruppo che zittisce", K_CHOKE, offsetof(GbDrum, choke) },
};
static const Param ACID_PARAMS[] = {
    { "Onda", K_WAVE, offsetof(GbAcid, wave) },
    { "Altezza", K_SEMI, offsetof(GbAcid, tune) },
    { "Taglio", K_UNIT, offsetof(GbAcid, cutoff) },
    { "Risonanza", K_UNIT, offsetof(GbAcid, resonance) },
    { "Inviluppo", K_UNIT, offsetof(GbAcid, env_mod) },
    { "Durata", K_UNIT, offsetof(GbAcid, decay) },
    { "Accento", K_UNIT, offsetof(GbAcid, accent) },
    { "Spinta", K_UNIT, offsetof(GbAcid, drive) },
    { "Volume", K_UNIT, offsetof(GbAcid, level) },
    { "Pan", K_PAN, offsetof(GbAcid, pan) },
};
static const Param FOLD_PARAMS[] = {
    { "Altezza", K_SEMI, offsetof(GbFold, tune) },
    { "Pieghe", K_UNIT, offsetof(GbFold, fold) },
    { "Simmetria", K_UNIT, offsetof(GbFold, symmetry) },
    { "Rapporto", K_UNIT, offsetof(GbFold, mod_ratio) },
    { "Modulazione", K_UNIT, offsetof(GbFold, mod_amount) },
    { "Durata", K_UNIT, offsetof(GbFold, decay) },
    { "Colore del gate", K_UNIT, offsetof(GbFold, gate_color) },
    { "Rilascio", K_UNIT, offsetof(GbFold, release) },
    { "Volume", K_UNIT, offsetof(GbFold, level) },
    { "Pan", K_PAN, offsetof(GbFold, pan) },
};
static const Param MASTER_PARAMS[] = {
    { "Tempo", K_BPM, offsetof(GbMaster, bpm) },
    { "Swing", K_SWING, offsetof(GbMaster, swing) },
    { "Eco: tempo", K_DTIME, offsetof(GbMaster, delay_time) },
    { "Eco: ritorni", K_FB, offsetof(GbMaster, delay_feedback) },
    { "Eco: colore", K_UNIT, offsetof(GbMaster, delay_tone) },
    { "Riverbero", K_UNIT, offsetof(GbMaster, reverb_size) },
    { "Smorzamento", K_UNIT, offsetof(GbMaster, reverb_damp) },
    { "Compressore", K_UNIT, offsetof(GbMaster, comp) },
    { "Pompa", K_UNIT, offsetof(GbMaster, pump) },
    { "Filtro DJ", K_BIPOLAR, offsetof(GbMaster, filter) },
    { "Risonanza", K_UNIT, offsetof(GbMaster, filter_res) },
    { "Lo-fi", K_UNIT, offsetof(GbMaster, lofi) },
    { "Nastro", K_UNIT, offsetof(GbMaster, tape) },
    { "Volume", K_UNIT, offsetof(GbMaster, volume) },
    { "Clock MIDI in uscita", K_CLOCK, 0 },          /* dell'app, non del motore */
};
#define N_DRUM_P ((int)(sizeof(DRUM_PARAMS) / sizeof(DRUM_PARAMS[0])))
#define N_ACID_P ((int)(sizeof(ACID_PARAMS) / sizeof(ACID_PARAMS[0])))
#define N_FOLD_P ((int)(sizeof(FOLD_PARAMS) / sizeof(FOLD_PARAMS[0])))
#define N_MASTER_P ((int)(sizeof(MASTER_PARAMS) / sizeof(MASTER_PARAMS[0])))

/* Campi della scheda del passo e della traccia (pagina Ritmo, tasto X). */
enum {
    F_NOTE, F_ACCENT, F_SLIDE, F_LEN, F_VEL, F_PROB, F_COND, F_RATCHET, F_MICRO, F_LOCK_TUNE, F_LOCK_DECAY, F_LOCK_TONE,
    F_HEADER, F_T_LENGTH, F_T_SPEED, F_EUC_HITS, F_EUC_ROT, F_EUC_APPLY, F_T_SHIFT, F_T_CLEAR
};
#define MAX_FIELDS 24

enum { MIX_LEVEL, MIX_PAN, MIX_DELAY, MIX_REVERB, MIX_ROWS };
static const char *MIX_ROW_NAMES[MIX_ROWS] = { "Volume", "Pan", "Eco", "Riverbero" };

enum { LIVE_PLAY, LIVE_REC, LIVE_MUTE, LIVE_MODES };
static const char *LIVE_MODE_NAMES[LIVE_MODES] = { "Suona", "Suona e registra", "Zittisci" };

#define REC_CHUNK 4096
#define MAX_CHAIN 64

struct App {
    Gb *g;
    char dir[400], state_path[460];
    int screen, quit, quit_dialog, dirty;
    int down[PAD_COUNT];
    float rep[PAD_COUNT];
    float time, toast_t, save_t;
    char toast[120];
    int modified;
    /* nomi delle tracce di batteria (dal kit o dal modello scelto) */
    char track_name[GB_DRUMS][32];
    int kit;
    /* Ritmo */
    int edit_pat, cx, cy;
    int editor, ed_sel;
    int last_note[2];
    int euc_hits, euc_rot;
    float preview_t;
    int preview_track, preview_note;
    /* Suoni */
    int snd_track, snd_sel;
    /* Mixer */
    int mix_x, mix_row, master_sel;
    float filter_base;
    /* Live */
    int live_mode;
    int step_seen[GB_TRACKS];
    float step_t[GB_TRACKS];
    /* Pattern */
    int pat_focus, pat_sel, groove_sel, clip, clear_armed;
    float groove_scroll;
    int chain[MAX_CHAIN], chain_n;
    /* levette */
    float lx, ly, rx, ry;
    float repeat_div;
    int fill;
    /* colpi */
    uint32_t seen[GB_TRACKS];
    float glow[GB_TRACKS];
    /* registrazione */
    FILE *rec_f;
    uint32_t rec_frames;
    char rec_path[520];
    int16_t rec_buf[REC_CHUNK * 2];
    /* esportazione a pezzi, un po' a ogni fotogramma */
    Gb *exp_g;
    FILE *exp_f;
    uint32_t exp_frames, exp_tail;
    int exp_target;
    char exp_path[520];
    /* MIDI */
    char midi_name[64];
    int knobs_abs, midi_track;
    int clock_out;                /* manda clock MIDI, Start e Stop al dispositivo collegato */
    int mpk_plus;                 /* Akai MPK mini Plus: pad 13-16 per fill e stutter */
    float midi_filter, midi_mod;  /* rotella del pitch (o joystick) e della modulazione */
    int pad_perf[4];
    uint8_t midi_held[2][128];
    float scope[1300];
};

/* ------------------------------------------------------------------ utilita' */
static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static int is_melodic(int t) { return t == GB_ACID || t == GB_FOLD; }

static void toast(App *a, const char *msg)
{
    snprintf(a->toast, sizeof(a->toast), "%s", msg);
    a->toast_t = 2.2f;
    a->dirty = 1;
}

static void changed(App *a) { a->modified = 1; a->dirty = 1; }

static const char *track_label(App *a, int t)
{
    if (t == GB_ACID) return "Acido";
    if (t == GB_FOLD) return "Piega";
    return a->track_name[t];
}

static uint32_t track_color(App *a, int t)
{
    if (t == GB_ACID) return C_LIME;
    if (t == GB_FOLD) return C_VIOLET;
    return model_color(gb_drum(a->g, t)->model);
}

static void note_name(char *out, size_t n, int note)
{
    int m = 48 + note;
    snprintf(out, n, "%s%d", NOTE_NAMES[((m % 12) + 12) % 12], m / 12 - 1);
}

/* Taglia il testo con i puntini perche' stia in maxw pixel. */
static void fit_text(int font, const char *src, int maxw, char *out, size_t n)
{
    snprintf(out, n, "%s", src);
    if (gfx_text_width(font, out) <= maxw) return;
    size_t len = strlen(out);
    while (len > 0) {
        len--;
        while (len > 0 && ((unsigned char)out[len] & 0xC0) == 0x80) len--;
        if (len + 4 > n) continue;
        memcpy(out + len, "…", 4);
        out[len + 3] = 0;
        if (gfx_text_width(font, out) <= maxw) return;
    }
}

static void step_default(GbStep *s, int note)
{
    memset(s, 0, sizeof(*s));
    s->vel = 100;
    s->prob = 100;
    s->cond = COND_ALWAYS;
    s->ratchet = 1;
    s->note = (int8_t)note;
    s->len = 1;
    s->lock_tune = s->lock_decay = s->lock_tone = GB_LOCK_NONE;
}

static int step_is_default(const GbStep *s)
{
    GbStep d;
    step_default(&d, s->note);
    return !memcmp(&d, s, sizeof(d));
}

static void track_clear(GbTrackPattern *t)
{
    for (int i = 0; i < GB_MAX_STEPS; i++) step_default(&t->step[i], 0);
}

static void pattern_clear(GbPattern *p)
{
    for (int t = 0; t < GB_TRACKS; t++) {
        track_clear(&p->track[t]);
        p->track[t].length = 16;
        p->track[t].speed = 0;
    }
    p->name[0] = 0;
}

static int pattern_is_empty(const GbPattern *p)
{
    for (int t = 0; t < GB_TRACKS; t++)
        for (int i = 0; i < p->track[t].length; i++)
            if (p->track[t].step[i].on) return 0;
    return 1;
}

/* Passi visibili nella griglia: la traccia piu' lunga del pattern, a pagine intere da 16. */
static int grid_steps(App *a)
{
    GbPattern *p = gb_pattern(a->g, a->edit_pat);
    int m = 16;
    for (int t = 0; t < GB_TRACKS; t++) if (p->track[t].length > m) m = p->track[t].length;
    return (m + 15) / 16 * 16;
}

static void set_track_names_from_kit(App *a)
{
    for (int t = 0; t < GB_DRUMS; t++) {
        const char *n = gb_kit_track_name(a->kit, t);
        snprintf(a->track_name[t], sizeof(a->track_name[t]), "%s", n ? n : gb_model_name(gb_drum(a->g, t)->model));
    }
}

static void load_kit(App *a, int kit)
{
    int n = gb_kit_count();
    a->kit = ((kit % n) + n) % n;
    gb_load_kit(a->g, a->kit);
    set_track_names_from_kit(a);
    changed(a);
}

static void apply_chain(App *a) { gb_set_chain(a->g, a->chain, a->chain_n); }

/* Il filtro DJ: quello del mixer piu' la levetta sinistra. */
static void apply_filter(App *a)
{
    gb_master(a->g)->filter = clampf(a->filter_base + a->lx + a->midi_filter, -1.0f, 1.0f);
}

/* Stutter e fill: la levetta destra, la rotella della modulazione e i pad 13-16 della MPK mini Plus. */
static void apply_perf(App *a)
{
    float div = 0.0f;
    float up = fmaxf(a->ry < 0 ? -a->ry : 0.0f, a->midi_mod);
    if (up > 0.25f) div = up < 0.55f ? 4.0f : up < 0.8f ? 2.0f : up < 0.95f ? 1.0f : 0.5f;
    if (a->pad_perf[1]) div = 2.0f;
    if (a->pad_perf[2]) div = 1.0f;
    if (a->pad_perf[3]) div = 0.5f;
    if (div != a->repeat_div) { a->repeat_div = div; gb_set_repeat(a->g, div); }
    int fill = a->ry > 0.5f || a->pad_perf[0];
    if (fill != a->fill) { a->fill = fill; gb_set_fill(a->g, fill); }
    a->dirty = 1;
}

/* Clock MIDI a 24 ppq: dal thread audio, un byte di tempo reale (si puo' infilare fra gli altri). */
static void clock_tick(void *user, int frame_offset)
{
    App *a = user;
    static const unsigned char tick = 0xF8;
    if (a->clock_out && cirmolo_midi_ports() > 0) cirmolo_midi_send(0, &tick, 1);
    (void)frame_offset;
}

/* Suona o ferma; con echo manda anche Start / Stop al dispositivo MIDI. */
static void transport(App *a, int on, int echo)
{
    if (on && echo && a->clock_out && cirmolo_midi_ports() > 0) { unsigned char m = 0xFA; cirmolo_midi_send(0, &m, 1); }
    gb_play(a->g, on);
    if (!on && echo && a->clock_out && cirmolo_midi_ports() > 0) { unsigned char m = 0xFC; cirmolo_midi_send(0, &m, 1); }
    a->dirty = 1;
}

/* ------------------------------------------------------------------ valori di partenza */
static void default_sounds(App *a)
{
    GbAcid *ac = gb_acid(a->g);
    *ac = (GbAcid){ .wave = 0, .tune = 0, .cutoff = 0.32f, .resonance = 0.72f, .env_mod = 0.6f, .decay = 0.4f, .accent = 0.6f,
                    .drive = 0.3f, .level = 0.7f, .pan = 0.0f };
    GbFold *fo = gb_fold(a->g);
    *fo = (GbFold){ .tune = 0, .fold = 0.4f, .symmetry = 0.5f, .mod_ratio = 0.3f, .mod_amount = 0.2f, .decay = 0.45f,
                    .gate_color = 0.7f, .release = 0.3f, .level = 0.6f, .pan = 0.0f };
    for (int t = 0; t < GB_TRACKS; t++) *gb_mix(a->g, t) = (GbMix){ .level = 0.8f };
    gb_mix(a->g, GB_ACID)->delay_send = 0.18f;
    gb_mix(a->g, GB_FOLD)->delay_send = 0.25f;
    gb_mix(a->g, GB_FOLD)->reverb_send = 0.35f;
    *gb_master(a->g) = (GbMaster){ .bpm = 120, .swing = 0, .delay_time = 3, .delay_feedback = 0.35f, .delay_tone = 0.5f,
                                    .reverb_size = 0.55f, .reverb_damp = 0.5f, .comp = 0.3f, .pump = 0.0f, .filter = 0.0f,
                                    .filter_res = 0.3f, .lofi = 0.0f, .tape = 0.1f, .volume = 0.8f };
    a->filter_base = 0.0f;
}

/* Primo avvio: il primo ritmo pronto con il suo kit, un giro di acido e qualche nota della Piega. */
static void default_song(App *a)
{
    for (int p = 0; p < GB_PATTERNS; p++) pattern_clear(gb_pattern(a->g, p));
    GbPattern *p0 = gb_pattern(a->g, 0);
    if (gb_groove_count() > 0) {
        gb_groove_load(0, p0);
        load_kit(a, gb_groove_kit(0));
        gb_master(a->g)->bpm = gb_groove_bpm(0);
        gb_master(a->g)->swing = gb_groove_swing(0);
        snprintf(p0->name, sizeof(p0->name), "%s", gb_groove_name(0));
    } else {
        load_kit(a, 0);
    }
    static const int acid[16] = { 0, -1, 0, 12, -1, 0, 3, -1, 0, -1, 10, 0, -1, 7, 12, -1 };
    static const int acc[16] = { 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0 };
    static const int sld[16] = { 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0 };
    for (int i = 0; i < 16; i++) {
        GbStep *s = &p0->track[GB_ACID].step[i];
        step_default(s, acid[i] < 0 ? 0 : acid[i] - 12);
        s->on = acid[i] >= 0;
        s->accent = (uint8_t)acc[i];
        s->slide = (uint8_t)sld[i];
    }
    static const int fold[4] = { 12, 15, 19, 22 };
    for (int i = 0; i < 4; i++) {
        GbStep *s = &p0->track[GB_FOLD].step[i * 4 + 2];
        step_default(s, fold[i]);
        s->on = 1;
        s->len = 2;
        s->vel = 90;
    }
    a->last_note[0] = -12;
    a->last_note[1] = 12;
}

/* ------------------------------------------------------------------ salvataggio */
static void write_state(App *a, FILE *f)
{
    Gb *g = a->g;
    fprintf(f, "version=gb1\nkit=%d\nscreen=%d\nedit_pat=%d\ncursor=%d,%d\nnotes=%d,%d\neuclid=%d,%d\nlive=%d\nfilter=%.4f\nclock_out=%d\n",
            a->kit, a->screen, a->edit_pat, a->cx, a->cy, a->last_note[0], a->last_note[1], a->euc_hits, a->euc_rot, a->live_mode,
            a->filter_base, a->clock_out);
    for (int t = 0; t < GB_DRUMS; t++) {
        GbDrum *d = gb_drum(g, t);
        fprintf(f, "d%d=%d,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%d\n", t, d->model, d->tune, d->decay, d->tone, d->snap, d->drive,
                d->level, d->pan, d->choke);
        fprintf(f, "dn%d=%s\n", t, a->track_name[t]);
    }
    GbAcid *ac = gb_acid(g);
    fprintf(f, "acid=%d,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n", ac->wave, ac->tune, ac->cutoff, ac->resonance, ac->env_mod,
            ac->decay, ac->accent, ac->drive, ac->level, ac->pan);
    GbFold *fo = gb_fold(g);
    fprintf(f, "fold=%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n", fo->tune, fo->fold, fo->symmetry, fo->mod_ratio,
            fo->mod_amount, fo->decay, fo->gate_color, fo->release, fo->level, fo->pan);
    for (int t = 0; t < GB_TRACKS; t++) {
        GbMix *m = gb_mix(g, t);
        fprintf(f, "mix%d=%.4f,%.4f,%.4f,%d,%d\n", t, m->level, m->delay_send, m->reverb_send, m->mute, m->solo);
    }
    GbMaster *ms = gb_master(g);
    fprintf(f, "master=%.2f,%.4f,%.2f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n", ms->bpm, ms->swing, ms->delay_time,
            ms->delay_feedback, ms->delay_tone, ms->reverb_size, ms->reverb_damp, ms->comp, ms->pump, ms->filter_res, ms->lofi,
            ms->tape, ms->volume);
    fprintf(f, "chain=");
    for (int i = 0; i < a->chain_n; i++) fprintf(f, "%s%d", i ? "," : "", a->chain[i]);
    fprintf(f, "\n");
    for (int p = 0; p < GB_PATTERNS; p++) {
        GbPattern *pt = gb_pattern(g, p);
        if (pt->name[0]) fprintf(f, "pname%d=%s\n", p, pt->name);
        for (int t = 0; t < GB_TRACKS; t++) {
            GbTrackPattern *tr = &pt->track[t];
            if (tr->length != 16 || tr->speed) fprintf(f, "len%d.%d=%d,%d\n", p, t, tr->length, tr->speed);
            for (int i = 0; i < GB_MAX_STEPS; i++) {
                GbStep *s = &tr->step[i];
                if (step_is_default(s) && s->note == 0) continue;
                fprintf(f, "st%d.%d.%d=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", p, t, i, s->on, s->vel, s->prob, s->cond, s->ratchet,
                        s->micro, s->note, s->slide, s->accent, s->len, s->lock_tune, s->lock_decay, s->lock_tone);
            }
        }
    }
}

void app_save(App *a)
{
    MKDIR(a->dir);
    char tmp[480];
    snprintf(tmp, sizeof(tmp), "%s.tmp", a->state_path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return;
    write_state(a, f);
    int ok = !ferror(f);
    if (fclose(f) || !ok) { remove(tmp); return; }
    remove(a->state_path);
    rename(tmp, a->state_path);
    a->modified = 0;
}

static void read_line(App *a, char *k, char *v, int *version_ok)
{
    Gb *g = a->g;
    int t, p, i;
    if (!strcmp(k, "version")) { *version_ok = !strcmp(v, "gb1"); return; }
    if (!*version_ok) return;
    if (!strcmp(k, "kit")) a->kit = clampi(atoi(v), 0, gb_kit_count() - 1);
    else if (!strcmp(k, "screen")) a->screen = clampi(atoi(v), 0, SCR_COUNT - 1);
    else if (!strcmp(k, "edit_pat")) a->edit_pat = clampi(atoi(v), 0, GB_PATTERNS - 1);
    else if (!strcmp(k, "cursor")) { sscanf(v, "%d,%d", &a->cx, &a->cy); a->cx = clampi(a->cx, 0, GB_MAX_STEPS - 1); a->cy = clampi(a->cy, 0, GB_TRACKS - 1); }
    else if (!strcmp(k, "notes")) sscanf(v, "%d,%d", &a->last_note[0], &a->last_note[1]);
    else if (!strcmp(k, "euclid")) sscanf(v, "%d,%d", &a->euc_hits, &a->euc_rot);
    else if (!strcmp(k, "live")) a->live_mode = clampi(atoi(v), 0, LIVE_MODES - 1);
    else if (!strcmp(k, "clock_out")) a->clock_out = atoi(v) != 0;
    else if (!strcmp(k, "filter")) a->filter_base = clampf((float)atof(v), -1.0f, 1.0f);
    else if (sscanf(k, "dn%d", &t) == 1 && t >= 0 && t < GB_DRUMS) snprintf(a->track_name[t], sizeof(a->track_name[t]), "%s", v);
    else if (sscanf(k, "d%d", &t) == 1 && t >= 0 && t < GB_DRUMS && k[1] >= '0' && k[1] <= '9') {
        GbDrum *d = gb_drum(g, t);
        sscanf(v, "%d,%f,%f,%f,%f,%f,%f,%f,%d", &d->model, &d->tune, &d->decay, &d->tone, &d->snap, &d->drive, &d->level, &d->pan, &d->choke);
        d->model = clampi(d->model, 0, DM_COUNT - 1);
    } else if (!strcmp(k, "acid")) {
        GbAcid *ac = gb_acid(g);
        sscanf(v, "%d,%f,%f,%f,%f,%f,%f,%f,%f,%f", &ac->wave, &ac->tune, &ac->cutoff, &ac->resonance, &ac->env_mod, &ac->decay,
               &ac->accent, &ac->drive, &ac->level, &ac->pan);
    } else if (!strcmp(k, "fold")) {
        GbFold *fo = gb_fold(g);
        sscanf(v, "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f", &fo->tune, &fo->fold, &fo->symmetry, &fo->mod_ratio, &fo->mod_amount, &fo->decay,
               &fo->gate_color, &fo->release, &fo->level, &fo->pan);
    } else if (sscanf(k, "mix%d", &t) == 1 && t >= 0 && t < GB_TRACKS) {
        GbMix *m = gb_mix(g, t);
        int mute = 0, solo = 0;
        sscanf(v, "%f,%f,%f,%d,%d", &m->level, &m->delay_send, &m->reverb_send, &mute, &solo);
        m->mute = (uint8_t)(mute != 0);
        m->solo = (uint8_t)(solo != 0);
    } else if (!strcmp(k, "master")) {
        GbMaster *ms = gb_master(g);
        sscanf(v, "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f", &ms->bpm, &ms->swing, &ms->delay_time, &ms->delay_feedback, &ms->delay_tone,
               &ms->reverb_size, &ms->reverb_damp, &ms->comp, &ms->pump, &ms->filter_res, &ms->lofi, &ms->tape, &ms->volume);
        ms->bpm = clampf(ms->bpm, 40, 240);
    } else if (!strcmp(k, "chain")) {
        a->chain_n = 0;
        for (char *s = v; *s && a->chain_n < MAX_CHAIN;) {
            a->chain[a->chain_n++] = clampi(atoi(s), 0, GB_PATTERNS - 1);
            char *c = strchr(s, ',');
            if (!c) break;
            s = c + 1;
        }
    } else if (sscanf(k, "pname%d", &p) == 1 && p >= 0 && p < GB_PATTERNS) {
        snprintf(gb_pattern(g, p)->name, sizeof(gb_pattern(g, p)->name), "%s", v);
    } else if (sscanf(k, "len%d.%d", &p, &t) == 2 && p >= 0 && p < GB_PATTERNS && t >= 0 && t < GB_TRACKS) {
        int len = 16, sp = 0;
        sscanf(v, "%d,%d", &len, &sp);
        gb_pattern(g, p)->track[t].length = (uint8_t)clampi(len, 1, GB_MAX_STEPS);
        gb_pattern(g, p)->track[t].speed = (uint8_t)clampi(sp, 0, 3);
    } else if (sscanf(k, "st%d.%d.%d", &p, &t, &i) == 3 && p >= 0 && p < GB_PATTERNS && t >= 0 && t < GB_TRACKS && i >= 0 && i < GB_MAX_STEPS) {
        int f[13] = { 0, 100, 100, 0, 1, 0, 0, 0, 0, 1, GB_LOCK_NONE, GB_LOCK_NONE, GB_LOCK_NONE };
        sscanf(v, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &f[0], &f[1], &f[2], &f[3], &f[4], &f[5], &f[6], &f[7], &f[8], &f[9], &f[10],
               &f[11], &f[12]);
        GbStep *s = &gb_pattern(g, p)->track[t].step[i];
        s->on = (uint8_t)(f[0] != 0);
        s->vel = (uint8_t)clampi(f[1], 1, 127);
        s->prob = (uint8_t)clampi(f[2], 1, 100);
        s->cond = (uint8_t)clampi(f[3], 0, COND_COUNT - 1);
        s->ratchet = (uint8_t)clampi(f[4], 1, 4);
        s->micro = (int8_t)clampi(f[5], -50, 50);
        s->note = (int8_t)clampi(f[6], -24, 48);
        s->slide = (uint8_t)(f[7] != 0);
        s->accent = (uint8_t)(f[8] != 0);
        s->len = (uint8_t)clampi(f[9], 1, 16);
        s->lock_tune = (int8_t)(f[10] == GB_LOCK_NONE ? GB_LOCK_NONE : clampi(f[10], -24, 24));
        s->lock_decay = (int8_t)(f[11] == GB_LOCK_NONE ? GB_LOCK_NONE : clampi(f[11], 0, 100));
        s->lock_tone = (int8_t)(f[12] == GB_LOCK_NONE ? GB_LOCK_NONE : clampi(f[12], 0, 100));
    }
}

static int load_state(App *a)
{
    FILE *f = fopen(a->state_path, "rb");
    if (!f) return 0;
    char line[512];
    int version_ok = 0, any = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        read_line(a, line, eq + 1, &version_ok);
        any |= version_ok;
    }
    fclose(f);
    return any;
}

/* ------------------------------------------------------------------ creazione */
App *app_create(Gb *g, const char *state_path)
{
    App *a = calloc(1, sizeof(App));
    if (!a) return NULL;
    a->g = g;
    snprintf(a->state_path, sizeof(a->state_path), "%s", state_path ? state_path : "stato.txt");
    snprintf(a->dir, sizeof(a->dir), "%s", a->state_path);
    char *sl = strrchr(a->dir, '/');
    if (!sl) sl = strrchr(a->dir, '\\');
    if (sl) *sl = 0; else snprintf(a->dir, sizeof(a->dir), ".");
    a->euc_hits = 5;
    a->midi_track = GB_ACID;
    a->preview_track = -1;
    a->clip = -1;
    a->clock_out = 1;
    for (int t = 0; t < GB_TRACKS; t++) a->step_seen[t] = -1;
    default_sounds(a);
    for (int p = 0; p < GB_PATTERNS; p++) pattern_clear(gb_pattern(g, p));
    load_kit(a, 0);
    if (!load_state(a)) {                       /* niente stato (o quello del synth di prima): si parte da capo */
        default_sounds(a);
        default_song(a);
    }
    a->filter_base = gb_master(g)->filter = a->filter_base;
    gb_select_pattern(g, a->edit_pat);
    apply_chain(a);
    gb_set_clock_hook(g, clock_tick, a);
    a->snd_track = a->cy;
    a->modified = 0;
    a->dirty = 1;
    return a;
}

/* ------------------------------------------------------------------ registrazione ed esportazione */
static void wav_header(FILE *f, uint32_t frames, int rate)
{
    uint32_t data = frames * 4;
    unsigned char h[44] = { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 2, 0 };
    uint32_t v[] = { 36 + data, (uint32_t)rate, (uint32_t)rate * 4 };
    memcpy(h + 4, &v[0], 4);
    memcpy(h + 24, &v[1], 4);
    memcpy(h + 28, &v[2], 4);
    h[32] = 4; h[34] = 16;
    memcpy(h + 36, "data", 4);
    memcpy(h + 40, &data, 4);
    fseek(f, 0, SEEK_SET);
    fwrite(h, 1, 44, f);
}

static void out_name(App *a, const char *prefix, char *out, size_t n)
{
    char dir[460];
    snprintf(dir, sizeof(dir), "%s/registrazioni", a->dir);
    MKDIR(a->dir);
    MKDIR(dir);
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char ts[32] = "0";
    if (tm) strftime(ts, sizeof(ts), "%Y-%m-%d-%H%M%S", tm);
    snprintf(out, n, "%s/%s-%s.wav", dir, prefix, ts);
}

static void rec_drain(App *a)
{
    if (!a->rec_f) return;
    int n;
    while ((n = gb_rec_read(a->g, a->rec_buf, REC_CHUNK)) > 0) {
        fwrite(a->rec_buf, 4, (size_t)n, a->rec_f);
        a->rec_frames += (uint32_t)n;
    }
}

static void rec_stop(App *a)
{
    if (!a->rec_f) return;
    gb_rec_stop(a->g);
    rec_drain(a);
    wav_header(a->rec_f, a->rec_frames, (int)gb_sample_rate(a->g));
    fclose(a->rec_f);
    a->rec_f = NULL;
    char t[160];
    const char *b = strrchr(a->rec_path, '/');
    snprintf(t, sizeof(t), "Salvata: %s", b ? b + 1 : a->rec_path);
    toast(a, t);
}

static void rec_start(App *a)
{
    out_name(a, "registrazione", a->rec_path, sizeof(a->rec_path));
    a->rec_f = fopen(a->rec_path, "wb");
    if (!a->rec_f) { toast(a, "Non riesco a creare il file della registrazione"); return; }
    wav_header(a->rec_f, 0, (int)gb_sample_rate(a->g));
    a->rec_frames = 0;
    gb_rec_start(a->g);
    toast(a, "Registrazione dell'uscita: R3 per fermare");
}

/* Copia suoni, mixer, master e pattern in un altro motore (per l'esportazione). */
static void copy_engine(App *a, Gb *dst)
{
    for (int t = 0; t < GB_DRUMS; t++) *gb_drum(dst, t) = *gb_drum(a->g, t);
    *gb_acid(dst) = *gb_acid(a->g);
    *gb_fold(dst) = *gb_fold(a->g);
    for (int t = 0; t < GB_TRACKS; t++) *gb_mix(dst, t) = *gb_mix(a->g, t);
    *gb_master(dst) = *gb_master(a->g);
    gb_master(dst)->filter = a->filter_base;
    for (int p = 0; p < GB_PATTERNS; p++) *gb_pattern(dst, p) = *gb_pattern(a->g, p);
}

static void export_finish(App *a, int ok)
{
    if (a->exp_f) {
        wav_header(a->exp_f, a->exp_frames, GB_RATE);
        fclose(a->exp_f);
        a->exp_f = NULL;
    }
    if (a->exp_g) { gb_destroy(a->exp_g); a->exp_g = NULL; }
    if (ok) {
        char t[200];
        const char *b = strrchr(a->exp_path, '/');
        snprintf(t, sizeof(t), "Esportata: %s (%u s)", b ? b + 1 : a->exp_path, a->exp_frames / GB_RATE);
        toast(a, t);
    } else {
        remove(a->exp_path);
        toast(a, "Esportazione non riuscita");
    }
}

/* Esporta in WAV la catena (o il pattern che si modifica) per loops giri, piu' due secondi di coda. */
static int export_begin(App *a, int loops)
{
    if (a->exp_g) return -1;
    a->exp_g = gb_create(GB_RATE);
    if (!a->exp_g) return -1;
    copy_engine(a, a->exp_g);
    if (a->chain_n) gb_set_chain(a->exp_g, a->chain, a->chain_n);
    else gb_select_pattern(a->exp_g, a->edit_pat);
    a->exp_target = loops * (a->chain_n ? a->chain_n : 1);
    gb_play(a->exp_g, 1);
    out_name(a, a->chain_n ? "catena" : "pattern", a->exp_path, sizeof(a->exp_path));
    a->exp_f = fopen(a->exp_path, "wb");
    if (!a->exp_f) { gb_destroy(a->exp_g); a->exp_g = NULL; toast(a, "Non riesco a creare il file"); return -1; }
    wav_header(a->exp_f, 0, GB_RATE);
    a->exp_frames = a->exp_tail = 0;
    return 0;
}

/* Un pezzo di esportazione: max_frames campioni; 1 quando ha finito. */
static int export_step(App *a, int max_frames)
{
    static float buf[1024 * 2];
    static int16_t pcm[1024 * 2];
    int done = 0;
    while (max_frames > 0 && !done) {
        int n = max_frames > 1024 ? 1024 : max_frames;
        gb_render_offline(a->exp_g, buf, n);
        for (int i = 0; i < n * 2; i++) pcm[i] = (int16_t)lrintf(clampf(buf[i], -1.0f, 1.0f) * 32767.0f);
        fwrite(pcm, 4, (size_t)n, a->exp_f);
        a->exp_frames += (uint32_t)n;
        max_frames -= n;
        if (gb_playing(a->exp_g) && gb_bar_count(a->exp_g) >= a->exp_target) gb_play(a->exp_g, 0);
        if (!gb_playing(a->exp_g)) { a->exp_tail += (uint32_t)n; if (a->exp_tail >= GB_RATE * 2) done = 1; }
        if (a->exp_frames > GB_RATE * 600u) done = 1;         /* al massimo 10 minuti */
    }
    return done;
}

int app_export_wav(App *a, const char *path, int loops)
{
    if (export_begin(a, loops)) return -1;
    if (path) {
        fclose(a->exp_f);
        remove(a->exp_path);
        snprintf(a->exp_path, sizeof(a->exp_path), "%s", path);
        a->exp_f = fopen(path, "wb");
        if (!a->exp_f) { gb_destroy(a->exp_g); a->exp_g = NULL; return -1; }
        wav_header(a->exp_f, 0, GB_RATE);
    }
    while (!export_step(a, GB_RATE)) {}
    export_finish(a, 1);
    return 0;
}

void app_destroy(App *a)
{
    if (!a) return;
    gb_set_clock_hook(a->g, NULL, NULL);
    if (gb_playing(a->g)) transport(a, 0, 1);
    if (a->rec_f) rec_stop(a);
    if (a->exp_g) export_finish(a, 0);
    app_save(a);
    free(a);
}

int app_wants_quit(const App *a) { return a->quit; }
int app_screen(const App *a) { return a->screen; }
int app_edit_pattern(const App *a) { return a->edit_pat; }

void app_set_screen(App *a, int screen)
{
    a->screen = ((screen % SCR_COUNT) + SCR_COUNT) % SCR_COUNT;
    a->editor = 0;
    a->clear_armed = 0;
    a->dirty = 1;
}

void app_set_cursor(App *a, int step, int track)
{
    a->cx = clampi(step, 0, GB_MAX_STEPS - 1);
    a->cy = clampi(track, 0, GB_TRACKS - 1);
    a->snd_track = a->cy;
    a->dirty = 1;
}

void app_open_editor(App *a, int open) { a->editor = open; a->ed_sel = 0; a->dirty = 1; }

void app_set_mixer(App *a, int strip, int row)
{
    a->mix_x = clampi(strip, 0, GB_TRACKS);
    if (a->mix_x == GB_TRACKS) a->master_sel = clampi(row, 0, N_MASTER_P - 1);
    else a->mix_row = clampi(row, 0, MIX_ROWS - 1);
    a->dirty = 1;
}

/* ------------------------------------------------------------------ anteprime */
static void preview_stop(App *a)
{
    if (a->preview_track >= 0) gb_note_off(a->g, a->preview_track, a->preview_note);
    a->preview_track = -1;
}

static void preview_note(App *a, int track, int note)
{
    preview_stop(a);
    a->preview_track = track;
    a->preview_note = 48 + note;
    a->preview_t = 0.25f;
    gb_note_on(a->g, track, 48 + note, 0.8f);
}

static void audition(App *a, int t, int note)
{
    if (is_melodic(t)) preview_note(a, t, note);
    else gb_hit(a->g, t, 0.8f);
}

/* ------------------------------------------------------------------ pagina Ritmo */
static GbStep *cur_step(App *a) { return &gb_pattern(a->g, a->edit_pat)->track[a->cy].step[a->cx]; }
static GbTrackPattern *cur_track(App *a) { return &gb_pattern(a->g, a->edit_pat)->track[a->cy]; }

static void select_pattern(App *a, int p)
{
    a->edit_pat = ((p % GB_PATTERNS) + GB_PATTERNS) % GB_PATTERNS;
    if (!a->chain_n) gb_select_pattern(a->g, a->edit_pat);
    if (a->cx >= grid_steps(a)) a->cx = grid_steps(a) - 1;
    a->dirty = 1;
}

static void toggle_step(App *a)
{
    GbStep *s = cur_step(a);
    int mel = is_melodic(a->cy);
    if (!s->on) {
        int keep_note = s->note;
        if (step_is_default(s) || !mel) {
            int note = mel ? a->last_note[a->cy - GB_ACID] : 0;
            GbStep old = *s;
            step_default(s, note);
            if (!mel) { s->vel = old.vel ? old.vel : 100; s->ratchet = old.ratchet ? old.ratchet : 1; }
        } else {
            s->note = (int8_t)keep_note;
        }
        s->on = 1;
        if (!gb_playing(a->g)) audition(a, a->cy, s->note);
    } else {
        s->on = 0;
    }
    if (a->cx >= cur_track(a)->length) {          /* passo oltre la fine della traccia: la allunga */
        cur_track(a)->length = (uint8_t)clampi((a->cx / 16 + 1) * 16, 1, GB_MAX_STEPS);
    }
    changed(a);
}

/* B tenuto + croce: velocita' e ripetizioni (batterie), nota e ottava (melodiche). */
static void quick_edit(App *a, int b)
{
    GbStep *s = cur_step(a);
    if (!s->on) toggle_step(a);
    if (is_melodic(a->cy)) {
        int d = b == PAD_UP ? 1 : b == PAD_DOWN ? -1 : b == PAD_RIGHT ? 12 : -12;
        s->note = (int8_t)clampi(s->note + d, -24, 48);
        a->last_note[a->cy - GB_ACID] = s->note;
        preview_note(a, a->cy, s->note);
    } else if (b == PAD_UP || b == PAD_DOWN) {
        s->vel = (uint8_t)clampi(s->vel + (b == PAD_UP ? 16 : -16), 7, 127);
        if (s->vel > 119) s->vel = 127;
        gb_hit(a->g, a->cy, s->vel / 127.0f);
    } else {
        s->ratchet = (uint8_t)clampi(s->ratchet + (b == PAD_RIGHT ? 1 : -1), 1, 4);
    }
    changed(a);
}

static int editor_fields(App *a, int *f)
{
    int n = 0, t = a->cy;
    if (is_melodic(t)) {
        f[n++] = F_NOTE;
        f[n++] = F_ACCENT;
        f[n++] = t == GB_ACID ? F_SLIDE : F_LEN;
    }
    f[n++] = F_VEL;
    f[n++] = F_PROB;
    f[n++] = F_COND;
    f[n++] = F_RATCHET;
    f[n++] = F_MICRO;
    if (!is_melodic(t)) f[n++] = F_LOCK_TUNE;
    f[n++] = F_LOCK_DECAY;
    f[n++] = F_LOCK_TONE;
    f[n++] = F_HEADER;
    f[n++] = F_T_LENGTH;
    f[n++] = F_T_SPEED;
    f[n++] = F_EUC_HITS;
    f[n++] = F_EUC_ROT;
    f[n++] = F_EUC_APPLY;
    f[n++] = F_T_SHIFT;
    f[n++] = F_T_CLEAR;
    return n;
}

static const char *field_name(App *a, int f)
{
    switch (f) {
    case F_NOTE: return "Nota";
    case F_ACCENT: return "Accento";
    case F_SLIDE: return "Slide verso il passo dopo";
    case F_LEN: return "Durata (passi)";
    case F_VEL: return "Dinamica";
    case F_PROB: return "Probabilità";
    case F_COND: return "Condizione";
    case F_RATCHET: return "Ripetizioni";
    case F_MICRO: return "Spostamento";
    case F_LOCK_TUNE: return "Altezza solo qui";
    case F_LOCK_DECAY: return "Durata solo qui";
    case F_LOCK_TONE: return a->cy == GB_ACID ? "Taglio solo qui" : a->cy == GB_FOLD ? "Pieghe solo qui" : "Colore solo qui";
    case F_HEADER: return "Traccia";
    case F_T_LENGTH: return "Lunghezza";
    case F_T_SPEED: return "Velocità";
    case F_EUC_HITS: return "Euclideo: colpi";
    case F_EUC_ROT: return "Euclideo: rotazione";
    case F_EUC_APPLY: return "Applica l'euclideo (A)";
    case F_T_SHIFT: return "Sposta tutto (sin./des.)";
    case F_T_CLEAR: return "Svuota la traccia (A)";
    default: return "";
    }
}

static void field_value(App *a, int f, char *out, size_t n)
{
    GbStep *s = cur_step(a);
    GbTrackPattern *tr = cur_track(a);
    out[0] = 0;
    switch (f) {
    case F_NOTE: note_name(out, n, s->note); break;
    case F_ACCENT: snprintf(out, n, "%s", s->accent ? "sì" : "no"); break;
    case F_SLIDE: snprintf(out, n, "%s", s->slide ? "sì" : "no"); break;
    case F_LEN: snprintf(out, n, "%d", s->len); break;
    case F_VEL: snprintf(out, n, "%d", s->vel); break;
    case F_PROB: snprintf(out, n, "%d %%", s->prob); break;
    case F_COND: snprintf(out, n, "%s", COND_NAMES[s->cond % COND_COUNT]); break;
    case F_RATCHET: snprintf(out, n, "%d", s->ratchet); break;
    case F_MICRO: snprintf(out, n, "%+d %%", s->micro); break;
    case F_LOCK_TUNE: if (s->lock_tune == GB_LOCK_NONE) snprintf(out, n, "come la traccia"); else snprintf(out, n, "%+d semitoni", s->lock_tune); break;
    case F_LOCK_DECAY: if (s->lock_decay == GB_LOCK_NONE) snprintf(out, n, "come la traccia"); else snprintf(out, n, "%d %%", s->lock_decay); break;
    case F_LOCK_TONE: if (s->lock_tone == GB_LOCK_NONE) snprintf(out, n, "come la traccia"); else snprintf(out, n, "%d %%", s->lock_tone); break;
    case F_T_LENGTH: snprintf(out, n, "%d passi", tr->length); break;
    case F_T_SPEED: snprintf(out, n, "%s", SPEED_NAMES[tr->speed & 3]); break;
    case F_EUC_HITS: snprintf(out, n, "%d su %d", a->euc_hits, tr->length); break;
    case F_EUC_ROT: snprintf(out, n, "%d", a->euc_rot); break;
    default: break;
    }
}

static int lock_step(int v, int d, int lo, int hi, int step)
{
    if (v == GB_LOCK_NONE) return d > 0 ? lo + (hi - lo) / 2 : GB_LOCK_NONE;
    v += d * step;
    if (v < lo) return GB_LOCK_NONE;
    return v > hi ? hi : v;
}

static void field_adjust(App *a, int f, int d, int coarse)
{
    GbStep *s = cur_step(a);
    GbTrackPattern *tr = cur_track(a);
    int is_step = f < F_HEADER;
    if (is_step && !s->on && f != F_MICRO) toggle_step(a);
    switch (f) {
    case F_NOTE: s->note = (int8_t)clampi(s->note + d * (coarse ? 12 : 1), -24, 48); a->last_note[a->cy - GB_ACID] = s->note; preview_note(a, a->cy, s->note); break;
    case F_ACCENT: s->accent = !s->accent; break;
    case F_SLIDE: s->slide = !s->slide; break;
    case F_LEN: s->len = (uint8_t)clampi(s->len + d, 1, 16); break;
    case F_VEL: s->vel = (uint8_t)clampi(s->vel + d * (coarse ? 16 : 4), 1, 127); break;
    case F_PROB: s->prob = (uint8_t)clampi(s->prob + d * (coarse ? 25 : 5), 1, 100); if (s->prob == 6 && d > 0) s->prob = 5; break;
    case F_COND: s->cond = (uint8_t)((s->cond + d + COND_COUNT) % COND_COUNT); break;
    case F_RATCHET: s->ratchet = (uint8_t)clampi(s->ratchet + d, 1, 4); break;
    case F_MICRO: s->micro = (int8_t)clampi(s->micro + d * (coarse ? 25 : 5), -50, 50); break;
    case F_LOCK_TUNE: s->lock_tune = (int8_t)lock_step(s->lock_tune, d, -24, 24, coarse ? 12 : 1); break;
    case F_LOCK_DECAY: s->lock_decay = (int8_t)lock_step(s->lock_decay, d, 0, 100, coarse ? 25 : 5); break;
    case F_LOCK_TONE: s->lock_tone = (int8_t)lock_step(s->lock_tone, d, 0, 100, coarse ? 25 : 5); break;
    case F_T_LENGTH: tr->length = (uint8_t)clampi(tr->length + d * (coarse ? 16 : 1), 1, GB_MAX_STEPS); a->euc_hits = clampi(a->euc_hits, 0, tr->length); break;
    case F_T_SPEED: tr->speed = (uint8_t)((tr->speed + d + 4) % 4); break;
    case F_EUC_HITS: a->euc_hits = clampi(a->euc_hits + d, 0, tr->length); break;
    case F_EUC_ROT: a->euc_rot = clampi(a->euc_rot + d, 0, tr->length - 1); break;
    case F_T_SHIFT: {
        int L = tr->length;
        GbStep tmp[GB_MAX_STEPS];
        for (int i = 0; i < L; i++) tmp[(i + d + L) % L] = tr->step[i];
        memcpy(tr->step, tmp, sizeof(GbStep) * (size_t)L);
        break;
    }
    default: return;
    }
    if (f == F_VEL && !is_melodic(a->cy)) gb_hit(a->g, a->cy, s->vel / 127.0f);
    changed(a);
}

static void field_action(App *a, int f)
{
    GbTrackPattern *tr = cur_track(a);
    if (f == F_EUC_APPLY) {
        int note = is_melodic(a->cy) ? a->last_note[a->cy - GB_ACID] : 0;
        gb_euclid(tr, tr->length, a->euc_hits, a->euc_rot, 100);
        for (int i = 0; i < tr->length; i++) {
            GbStep *s = &tr->step[i];
            int on = s->on, vel = s->vel ? s->vel : 100;
            step_default(s, note);
            s->on = (uint8_t)on;
            s->vel = (uint8_t)vel;
        }
        char t[96];
        snprintf(t, sizeof(t), "Euclideo: %d colpi su %d", a->euc_hits, tr->length);
        toast(a, t);
        changed(a);
    } else if (f == F_T_CLEAR) {
        track_clear(tr);
        toast(a, "Traccia svuotata");
        changed(a);
    } else if (f == F_ACCENT || f == F_SLIDE) {
        field_adjust(a, f, 1, 0);
    }
}

static void editor_button(App *a, int b)
{
    int f[MAX_FIELDS], n = editor_fields(a, f);
    a->ed_sel = clampi(a->ed_sel, 0, n - 1);
    if (b == PAD_B || b == PAD_X) { a->editor = 0; return; }
    if (b == PAD_UP || b == PAD_DOWN) {
        int d = b == PAD_UP ? -1 : 1;
        do a->ed_sel = (a->ed_sel + d + n) % n; while (f[a->ed_sel] == F_HEADER);
        return;
    }
    int coarse = a->down[PAD_L1] || a->down[PAD_R1];
    if (b == PAD_LEFT || b == PAD_RIGHT) field_adjust(a, f[a->ed_sel], b == PAD_RIGHT ? 1 : -1, coarse);
    else if (b == PAD_L1 || b == PAD_R1) field_adjust(a, f[a->ed_sel], b == PAD_R1 ? 1 : -1, 1);
    else if (b == PAD_A) field_action(a, f[a->ed_sel]);
    else if (b == PAD_Y) audition(a, a->cy, cur_step(a)->note);
}

static void ritmo_button(App *a, int b)
{
    if (a->editor) { editor_button(a, b); return; }
    int steps = grid_steps(a);
    if (a->down[PAD_B] && (b == PAD_UP || b == PAD_DOWN || b == PAD_LEFT || b == PAD_RIGHT)) { quick_edit(a, b); return; }
    switch (b) {
    case PAD_UP: a->cy = (a->cy + GB_TRACKS - 1) % GB_TRACKS; break;
    case PAD_DOWN: a->cy = (a->cy + 1) % GB_TRACKS; break;
    case PAD_LEFT: a->cx = (a->cx + steps - 1) % steps; break;
    case PAD_RIGHT: a->cx = (a->cx + 1) % steps; break;
    case PAD_L1: a->cx = (a->cx + steps - 16) % steps; break;
    case PAD_R1: a->cx = (a->cx + 16) % steps; break;
    case PAD_L2: select_pattern(a, a->edit_pat - 1); break;
    case PAD_R2: select_pattern(a, a->edit_pat + 1); break;
    case PAD_A: toggle_step(a); break;
    case PAD_Y: audition(a, a->cy, cur_step(a)->on ? cur_step(a)->note : a->last_note[clampi(a->cy - GB_ACID, 0, 1)]); break;
    case PAD_X: a->editor = 1; a->ed_sel = 0; break;
    default: break;
    }
    a->snd_track = a->cy;
    if (is_melodic(a->cy)) a->midi_track = a->cy;
}

/* ------------------------------------------------------------------ pagina Suoni */
static const Param *track_params(int t, int *n, int *target)
{
    if (t == GB_ACID) { *n = N_ACID_P; *target = T_ACID; return ACID_PARAMS; }
    if (t == GB_FOLD) { *n = N_FOLD_P; *target = T_FOLD; return FOLD_PARAMS; }
    *n = N_DRUM_P;
    *target = T_DRUM;
    return DRUM_PARAMS;
}

static void *target_ptr(App *a, int target, int t)
{
    switch (target) {
    case T_ACID: return gb_acid(a->g);
    case T_FOLD: return gb_fold(a->g);
    case T_MASTER: return gb_master(a->g);
    default: return gb_drum(a->g, t);
    }
}

static void param_text(App *a, const Param *p, void *base, char *out, size_t n)
{
    float *fv = (float *)((char *)base + p->off);
    int *iv = (int *)((char *)base + p->off);
    switch (p->kind) {
    case K_MODEL: snprintf(out, n, "%s", gb_model_name(*iv)); break;
    case K_WAVE: snprintf(out, n, "%s", *iv ? "Quadra" : "Dente di sega"); break;
    case K_CHOKE: if (*iv) snprintf(out, n, "%d", *iv); else snprintf(out, n, "nessuno"); break;
    case K_SEMI: snprintf(out, n, "%+d", (int)lrintf(*fv)); break;
    case K_PAN: {
        int v = (int)lrintf(*fv * 100);
        if (v == 0) snprintf(out, n, "centro"); else snprintf(out, n, "%s %d", v < 0 ? "sin." : "des.", abs(v));
        break;
    }
    case K_BPM: snprintf(out, n, "%d BPM", (int)lrintf(*fv)); break;
    case K_SWING: snprintf(out, n, "%d %%", (int)lrintf(*fv * 100)); break;
    case K_DTIME: snprintf(out, n, "%d/16", (int)lrintf(*fv)); break;
    case K_BIPOLAR: {
        int v = (int)lrintf(*fv * 100);
        if (v == 0) snprintf(out, n, "aperto"); else snprintf(out, n, "%s %d", v < 0 ? "bassi" : "alti", abs(v));
        break;
    }
    case K_FB: snprintf(out, n, "%d %%", (int)lrintf(*fv / 0.9f * 100)); break;
    default: snprintf(out, n, "%d %%", (int)lrintf(*fv * 100)); break;
    }
    (void)a;
}

/* Valore 0..1 del parametro (per le barre e le manopole MIDI); -1 per quelli a scelta. */
static float param_fraction(const Param *p, void *base)
{
    float v = *(float *)((char *)base + p->off);
    switch (p->kind) {
    case K_SEMI: return (v + 24) / 48;
    case K_PAN: case K_BIPOLAR: return (v + 1) / 2;
    case K_BPM: return (v - 40) / 200;
    case K_SWING: return v / 0.75f;
    case K_DTIME: return (v - 1) / 7;
    case K_FB: return v / 0.9f;
    case K_UNIT: return v;
    default: return -1;
    }
}

static void param_set_fraction(const Param *p, void *base, float t)
{
    float *fv = (float *)((char *)base + p->off);
    t = clampf(t, 0, 1);
    switch (p->kind) {
    case K_SEMI: *fv = roundf(t * 48 - 24); break;
    case K_PAN: case K_BIPOLAR: *fv = t * 2 - 1; break;
    case K_BPM: *fv = roundf(40 + t * 200); break;
    case K_SWING: *fv = t * 0.75f; break;
    case K_DTIME: *fv = roundf(1 + t * 7); break;
    case K_FB: *fv = t * 0.9f; break;
    case K_UNIT: *fv = t; break;
    default: break;
    }
}

static void param_adjust(App *a, const Param *p, void *base, int d, int coarse)
{
    float *fv = (float *)((char *)base + p->off);
    int *iv = (int *)((char *)base + p->off);
    switch (p->kind) {
    case K_MODEL: *iv = (*iv + d + DM_COUNT) % DM_COUNT; break;
    case K_WAVE: *iv = !*iv; break;
    case K_CHOKE: *iv = clampi(*iv + d, 0, 4); break;
    case K_SEMI: *fv = clampf(roundf(*fv) + d * (coarse ? 12 : 1), -24, 24); break;
    case K_PAN: case K_BIPOLAR: *fv = clampf(roundf((*fv + d * (coarse ? 0.25f : 0.05f)) * 100) / 100, -1, 1); break;
    case K_BPM: *fv = clampf(roundf(*fv) + d * (coarse ? 10 : 1), 40, 240); break;
    case K_SWING: *fv = clampf(roundf((*fv + d * (coarse ? 0.1f : 0.01f)) * 100) / 100, 0, 0.75f); break;
    case K_DTIME: *fv = clampf(roundf(*fv) + d, 1, 8); break;
    case K_FB: *fv = clampf(*fv + d * (coarse ? 0.09f : 0.018f), 0, 0.9f); break;
    default: *fv = clampf(roundf((*fv + d * (coarse ? 0.1f : 0.02f)) * 100) / 100, 0, 1); break;
    }
    changed(a);
}

static void suoni_button(App *a, int b)
{
    int n, target;
    const Param *ps = track_params(a->snd_track, &n, &target);
    void *base = target_ptr(a, target, a->snd_track);
    a->snd_sel = clampi(a->snd_sel, 0, n - 1);
    int coarse = a->down[PAD_B];
    switch (b) {
    case PAD_UP: a->snd_sel = (a->snd_sel + n - 1) % n; break;
    case PAD_DOWN: a->snd_sel = (a->snd_sel + 1) % n; break;
    case PAD_LEFT: case PAD_RIGHT: {
        const Param *p = &ps[a->snd_sel];
        param_adjust(a, p, base, b == PAD_RIGHT ? 1 : -1, coarse);
        if (p->kind == K_MODEL) {               /* nuovo strumento: prende il nome del modello */
            snprintf(a->track_name[a->snd_track], sizeof(a->track_name[a->snd_track]), "%s", gb_model_name(gb_drum(a->g, a->snd_track)->model));
            gb_hit(a->g, a->snd_track, 0.8f);
        }
        break;
    }
    case PAD_L1: a->snd_track = (a->snd_track + GB_TRACKS - 1) % GB_TRACKS; break;
    case PAD_R1: a->snd_track = (a->snd_track + 1) % GB_TRACKS; break;
    case PAD_L2: case PAD_R2: {
        load_kit(a, a->kit + (b == PAD_R2 ? 1 : -1));
        char t[96];
        snprintf(t, sizeof(t), "Kit %s", gb_kit_name(a->kit));
        toast(a, t);
        break;
    }
    case PAD_A: audition(a, a->snd_track, a->last_note[clampi(a->snd_track - GB_ACID, 0, 1)]); break;
    case PAD_Y: {                                /* variazione: piccoli spostamenti casuali dei valori */
        for (int i = 0; i < n; i++) {
            if (ps[i].kind != K_UNIT || !strcmp(ps[i].name, "Volume")) continue;
            float *fv = (float *)((char *)base + ps[i].off);
            *fv = clampf(*fv + ((rand() % 2001) / 1000.0f - 1.0f) * 0.15f, 0, 1);
        }
        changed(a);
        audition(a, a->snd_track, a->last_note[clampi(a->snd_track - GB_ACID, 0, 1)]);
        toast(a, "Variazione");
        break;
    }
    default: break;
    }
    a->cy = a->snd_track;
    if (is_melodic(a->snd_track)) a->midi_track = a->snd_track;
    int n2, t2;
    track_params(a->snd_track, &n2, &t2);
    a->snd_sel = clampi(a->snd_sel, 0, n2 - 1);
}

/* ------------------------------------------------------------------ pagina Mixer */
static float *pan_ptr(App *a, int t)
{
    if (t == GB_ACID) return &gb_acid(a->g)->pan;
    if (t == GB_FOLD) return &gb_fold(a->g)->pan;
    return &gb_drum(a->g, t)->pan;
}

static void mixer_button(App *a, int b)
{
    int master = a->mix_x == GB_TRACKS;
    switch (b) {
    case PAD_LEFT: a->mix_x = (a->mix_x + GB_TRACKS) % (GB_TRACKS + 1); break;
    case PAD_RIGHT: a->mix_x = (a->mix_x + 1) % (GB_TRACKS + 1); break;
    case PAD_L1: if (master) a->master_sel = (a->master_sel + N_MASTER_P - 1) % N_MASTER_P; else a->mix_row = (a->mix_row + MIX_ROWS - 1) % MIX_ROWS; break;
    case PAD_R1: if (master) a->master_sel = (a->master_sel + 1) % N_MASTER_P; else a->mix_row = (a->mix_row + 1) % MIX_ROWS; break;
    case PAD_UP: case PAD_DOWN: {
        int d = b == PAD_UP ? 1 : -1, coarse = a->down[PAD_B];
        if (master) {
            const Param *p = &MASTER_PARAMS[a->master_sel];
            if (p->kind == K_BIPOLAR) {
                a->filter_base = clampf(roundf((a->filter_base + d * (coarse ? 0.25f : 0.05f)) * 100) / 100, -1, 1);
                apply_filter(a);
                changed(a);
            } else if (p->kind == K_CLOCK) {
                a->clock_out = !a->clock_out;
                toast(a, a->clock_out ? "Clock MIDI in uscita: acceso" : "Clock MIDI in uscita: spento");
                changed(a);
            } else {
                param_adjust(a, p, gb_master(a->g), d, coarse);
            }
        } else {
            GbMix *m = gb_mix(a->g, a->mix_x);
            float step = coarse ? 0.1f : 0.02f;
            if (a->mix_row == MIX_LEVEL) m->level = clampf(roundf((m->level + d * step) * 100) / 100, 0, 1);
            else if (a->mix_row == MIX_PAN) { float *p = pan_ptr(a, a->mix_x); *p = clampf(roundf((*p + d * (coarse ? 0.25f : 0.05f)) * 100) / 100, -1, 1); }
            else if (a->mix_row == MIX_DELAY) m->delay_send = clampf(roundf((m->delay_send + d * step) * 100) / 100, 0, 1);
            else m->reverb_send = clampf(roundf((m->reverb_send + d * step) * 100) / 100, 0, 1);
            changed(a);
        }
        break;
    }
    case PAD_A: if (!master) { GbMix *m = gb_mix(a->g, a->mix_x); m->mute = !m->mute; changed(a); } break;
    case PAD_Y: if (!master) { GbMix *m = gb_mix(a->g, a->mix_x); m->solo = !m->solo; changed(a); } break;
    case PAD_X: if (!master) audition(a, a->mix_x, a->last_note[clampi(a->mix_x - GB_ACID, 0, 1)]); break;
    default: break;
    }
    if (a->mix_x < GB_TRACKS) { a->snd_track = a->cy = a->mix_x; }
}

/* ------------------------------------------------------------------ pagina Live */
static float step_seconds(App *a) { return 60.0f / fmaxf(40.0f, gb_master(a->g)->bpm) / 4.0f; }

/* Registra un colpo nel pattern che suona, sul passo piu' vicino. */
static void live_record(App *a, int t)
{
    int st = gb_track_step(a->g, t);
    if (st < 0) return;
    int pat = gb_playing_pattern(a->g);
    GbTrackPattern *tr = &gb_pattern(a->g, pat)->track[t];
    float phase = (a->time - a->step_t[t]) / step_seconds(a);
    if (phase > 0.5f) st = (st + 1) % tr->length;
    GbStep *s = &tr->step[st];
    if (!s->on) { step_default(s, 0); s->on = 1; }
    changed(a);
}

static void live_button(App *a, int b)
{
    if (b == PAD_L3) {
        a->live_mode = (a->live_mode + 1) % LIVE_MODES;
        toast(a, LIVE_MODE_NAMES[a->live_mode]);
        return;
    }
    for (int t = 0; t < GB_DRUMS; t++) {
        if (LIVE_BTNS[t] != b) continue;
        if (a->live_mode == LIVE_MUTE) {
            GbMix *m = gb_mix(a->g, t);
            m->mute = !m->mute;
            changed(a);
        } else {
            gb_hit(a->g, t, 0.85f);
            if (a->live_mode == LIVE_REC && gb_playing(a->g)) live_record(a, t);
        }
        return;
    }
}

/* ------------------------------------------------------------------ pagina Pattern */
static void pattern_button(App *a, int b)
{
    if (b != PAD_B) a->clear_armed = 0;
    int nG = gb_groove_count();
    if (a->pat_focus == 1) {                          /* lista dei ritmi pronti */
        switch (b) {
        case PAD_UP: a->groove_sel = (a->groove_sel + nG - 1) % nG; break;
        case PAD_DOWN: a->groove_sel = (a->groove_sel + 1) % nG; break;
        case PAD_L1: a->groove_sel = clampi(a->groove_sel - 10, 0, nG - 1); break;
        case PAD_R1: a->groove_sel = clampi(a->groove_sel + 10, 0, nG - 1); break;
        case PAD_LEFT: a->pat_focus = 0; break;
        case PAD_A: {
            GbPattern *p = gb_pattern(a->g, a->edit_pat);
            gb_groove_load(a->groove_sel, p);
            snprintf(p->name, sizeof(p->name), "%s", gb_groove_name(a->groove_sel));
            load_kit(a, gb_groove_kit(a->groove_sel));
            gb_master(a->g)->bpm = gb_groove_bpm(a->groove_sel);
            gb_master(a->g)->swing = gb_groove_swing(a->groove_sel);
            char t[120];
            snprintf(t, sizeof(t), "%s nel pattern %d, kit %s", gb_groove_name(a->groove_sel), a->edit_pat + 1, gb_kit_name(a->kit));
            toast(a, t);
            changed(a);
            break;
        }
        default: break;
        }
        return;
    }
    int col = a->pat_sel % 4, row = a->pat_sel / 4;
    switch (b) {
    case PAD_LEFT: if (col > 0) a->pat_sel--; break;
    case PAD_RIGHT: if (col < 3) a->pat_sel++; else if (nG > 0) a->pat_focus = 1; break;
    case PAD_UP: a->pat_sel = ((row + 3) % 4) * 4 + col; break;
    case PAD_DOWN: a->pat_sel = ((row + 1) % 4) * 4 + col; break;
    case PAD_A:
        if (a->chain_n) { a->chain_n = 0; apply_chain(a); toast(a, "Catena spenta"); }
        select_pattern(a, a->pat_sel);
        break;
    case PAD_X:
        if (a->chain_n < MAX_CHAIN) { a->chain[a->chain_n++] = a->pat_sel; apply_chain(a); changed(a); }
        break;
    case PAD_L1:
        if (a->chain_n) { a->chain_n--; apply_chain(a); changed(a); if (!a->chain_n) gb_select_pattern(a->g, a->edit_pat); }
        break;
    case PAD_Y:
        if (a->clip < 0 || a->clip == a->pat_sel) { a->clip = a->pat_sel; toast(a, "Copiato: Y su un altro pattern per incollare"); }
        else {
            *gb_pattern(a->g, a->pat_sel) = *gb_pattern(a->g, a->clip);
            char t[80];
            snprintf(t, sizeof(t), "Pattern %d incollato nel %d", a->clip + 1, a->pat_sel + 1);
            toast(a, t);
            a->clip = -1;
            changed(a);
        }
        break;
    case PAD_B:
        if (!a->clear_armed) { a->clear_armed = 1; toast(a, "B di nuovo per svuotare il pattern"); }
        else { pattern_clear(gb_pattern(a->g, a->pat_sel)); a->clear_armed = 0; toast(a, "Pattern svuotato"); changed(a); }
        break;
    case PAD_R1:
        if (export_begin(a, a->chain_n ? 1 : 4) == 0) toast(a, a->chain_n ? "Esporto la catena..." : "Esporto il pattern (4 giri)...");
        break;
    default: break;
    }
}

/* ------------------------------------------------------------------ tasti */
static int repeats(App *a, int b)
{
    if (b != PAD_UP && b != PAD_DOWN && b != PAD_LEFT && b != PAD_RIGHT && b != PAD_L1 && b != PAD_R1) return 0;
    if (a->screen == SCR_LIVE) return 0;
    if (a->screen == SCR_RITMO && !a->editor && (b == PAD_L1 || b == PAD_R1)) return 0;
    if (a->screen == SCR_SUONI && (b == PAD_L1 || b == PAD_R1)) return 0;
    if (a->screen == SCR_PATTERN && a->pat_focus == 0 && (b == PAD_L1 || b == PAD_R1)) return 0;
    return 1;
}

static void handle_press(App *a, int b)
{
    a->dirty = 1;
    if (a->quit_dialog) {
        if (b == PAD_A || b == PAD_DOWN) { a->quit = 1; preview_stop(a); gb_all_notes_off(a->g); }
        else if (b == PAD_B || b == PAD_UP || b == PAD_MENU) a->quit_dialog = 0;
        return;
    }
    if (b == PAD_MENU) { a->quit_dialog = 1; preview_stop(a); return; }
    if (b == PAD_SELECT) { app_set_screen(a, a->screen + 1); return; }
    if (b == PAD_START) { transport(a, !gb_playing(a->g), 1); return; }
    if (b == PAD_R3) { if (a->rec_f) rec_stop(a); else rec_start(a); return; }
    switch (a->screen) {
    case SCR_RITMO: ritmo_button(a, b); break;
    case SCR_SUONI: suoni_button(a, b); break;
    case SCR_MIXER: mixer_button(a, b); break;
    case SCR_LIVE: live_button(a, b); break;
    default: pattern_button(a, b); break;
    }
    if (b == PAD_L3 && a->screen != SCR_LIVE) { gb_all_notes_off(a->g); toast(a, "Tutte le note spente"); }
}

void app_button(App *a, int b, int pressed)
{
    if (b < 0 || b >= PAD_COUNT) return;
    if (pressed) {
        if (a->down[b]) app_button(a, b, 0);
        a->down[b] = 1;
        a->rep[b] = 0.0f;
        handle_press(a, b);
    } else {
        if (!a->down[b]) return;
        a->down[b] = 0;
        a->dirty = 1;
    }
}

static float deadzone(float v)
{
    if (fabsf(v) < 0.15f) return 0.0f;
    return (v - (v > 0 ? 0.15f : -0.15f)) / 0.85f;
}

/* Levette: sinistra filtro DJ; destra in su stutter (piu' in alto = piu' fitto), in giu' fill. */
void app_axes(App *a, float lx, float ly, float rx, float ry, float l2, float r2)
{
    float olx = a->lx, ory = a->ry;
    a->lx = deadzone(lx);
    a->ly = deadzone(ly);
    a->rx = deadzone(rx);
    a->ry = deadzone(ry);
    if (a->lx != olx) { apply_filter(a); a->dirty = 1; }
    if (a->ry != ory) apply_perf(a);
    (void)l2;
    (void)r2;
}

/* ------------------------------------------------------------------ MIDI */
void app_midi_status(App *a, const char *device)
{
    snprintf(a->midi_name, sizeof(a->midi_name), "%s", device ? device : "");
    a->knobs_abs = 0;
    a->mpk_plus = device && (strstr(device, "MPK mini Plus") || strstr(device, "MPK Mini Plus") || strstr(device, "MPKminiPlus"));
    memset(a->pad_perf, 0, sizeof(a->pad_perf));
    a->midi_filter = a->midi_mod = 0.0f;
    apply_filter(a);
    apply_perf(a);
    if (!device) {
        for (int k = 0; k < 2; k++)
            for (int n = 0; n < 128; n++)
                if (a->midi_held[k][n]) gb_note_off(a->g, GB_ACID + k, n);
        memset(a->midi_held, 0, sizeof(a->midi_held));
    } else {
        char t[96];
        snprintf(t, sizeof(t), "MIDI collegato: %s", device);
        toast(a, t);
    }
    a->dirty = 1;
}

/* Note del canale 10: 36-47 vanno in ordine sulle 12 tracce (i pad dei controller), le altre come nel
   General MIDI. */
static int drum_for_note(int n)
{
    if (n >= 36 && n <= 47) return n - 36;
    switch (n) {
    case 35: return 0;
    case 48: case 50: return 7;
    case 49: case 51: case 52: case 53: case 55: case 57: case 59: return 8;
    case 54: case 58: case 69: case 70: return 10;
    case 56: return 9;
    case 60: case 61: case 62: case 63: case 64: case 65: case 66: case 67: case 68: return 10;
    default: return 11;
    }
}

/* Le otto manopole regolano i primi otto valori continui della traccia scelta in Suoni. */
static void knob_set(App *a, int k, float t, int relative)
{
    int n, target;
    const Param *ps = track_params(a->snd_track, &n, &target);
    void *base = target_ptr(a, target, a->snd_track);
    int idx = -1;
    for (int i = 0, c = 0; i < n; i++) {
        if (param_fraction(&ps[i], base) < 0) continue;
        if (c++ == k) { idx = i; break; }
    }
    if (idx < 0) {                                    /* batterie: l'ottava manopola e' la mandata al riverbero */
        if (k != 7 || is_melodic(a->snd_track)) return;
        GbMix *m = gb_mix(a->g, a->snd_track);
        m->reverb_send = clampf(relative ? m->reverb_send + t : t, 0, 1);
        changed(a);
        return;
    }
    float v = relative ? param_fraction(&ps[idx], base) + t : t;
    param_set_fraction(&ps[idx], base, v);
    a->snd_sel = idx;
    changed(a);
}

static void midi_knob(App *a, int cc, int v)
{
    if (cc >= 70 && cc <= 77) { knob_set(a, cc - 70, v / 127.0f, 0); return; }
    if (cc < 24 || cc > 31) return;
    int k = cc - 24;
    if (!a->knobs_abs && v >= 16 && v <= 111) a->knobs_abs = 1;
    if (a->knobs_abs) knob_set(a, k, v / 127.0f, 0);
    else if (v != 0 && v != 64) knob_set(a, k, (v < 64 ? v : v - 128) / 127.0f, 1);
}

static void app_midi(App *a, const unsigned char *m, int len)
{
    if (len < 1) return;
    if (m[0] == 0xFA || m[0] == 0xFB) { transport(a, 1, 0); return; }
    if (m[0] == 0xFC) { transport(a, 0, 0); return; }
    int type = m[0] & 0xF0, ch = m[0] & 0x0F;
    int d1 = len > 1 ? m[1] & 0x7F : 0, d2 = len > 2 ? m[2] & 0x7F : 0;
    int k = a->midi_track - GB_ACID;
    if (type == 0x90 && d2 > 0) {
        if (ch == 9) {
            if (a->mpk_plus && d1 >= 48 && d1 <= 51) { a->pad_perf[d1 - 48] = 1; apply_perf(a); return; }
            int t = drum_for_note(d1);
            gb_hit(a->g, t, 0.3f + 0.7f * d2 / 127.0f);
            if (a->live_mode == LIVE_REC && gb_playing(a->g)) live_record(a, t);
            return;
        }
        a->midi_held[k][d1] = 1;
        gb_note_on(a->g, a->midi_track, d1, 0.3f + 0.7f * d2 / 127.0f);
    } else if (type == 0x80 || type == 0x90) {
        if (ch == 9) {
            if (a->mpk_plus && d1 >= 48 && d1 <= 51) { a->pad_perf[d1 - 48] = 0; apply_perf(a); }
            return;
        }
        for (int j = 0; j < 2; j++)
            if (a->midi_held[j][d1]) { a->midi_held[j][d1] = 0; gb_note_off(a->g, GB_ACID + j, d1); }
    } else if (type == 0xB0) {
        if (d1 == 123 || d1 == 120) { gb_all_notes_off(a->g); memset(a->midi_held, 0, sizeof(a->midi_held)); }
        else if (d1 == 1) { a->midi_mod = d2 / 127.0f; apply_perf(a); }
        else midi_knob(a, d1, d2);
    } else if (type == 0xE0) {                       /* rotella del pitch o joystick: filtro DJ */
        a->midi_filter = (((d2 << 7) | d1) - 8192) / 8192.0f;
        apply_filter(a);
        a->dirty = 1;
    } else if (type == 0xC0) {
        load_kit(a, d1 % gb_kit_count());
    }
}

void app_midi_port(App *a, int port, const unsigned char *m, int len)
{
    if (port == 0 || (len >= 1 && m[0] >= 0xF8)) { app_midi(a, m, len); return; }
    if (len == 3 && (m[0] & 0xF0) == 0xB0) midi_knob(a, m[1] & 0x7F, m[2] & 0x7F);
}

/* ------------------------------------------------------------------ aggiornamento */
void app_update(App *a, float dt)
{
    a->time += dt;
    if (a->toast_t > 0) { a->toast_t = fmaxf(0.0f, a->toast_t - dt); a->dirty = 1; }
    rec_drain(a);
    if (a->exp_g) {
        if (export_step(a, GB_RATE / 2)) export_finish(a, 1);
        a->dirty = 1;
    }
    if (a->preview_track >= 0 && (a->preview_t -= dt) <= 0) preview_stop(a);
    for (int t = 0; t < GB_TRACKS; t++) {
        uint32_t f = gb_track_flash(a->g, t);
        if (f != a->seen[t]) { a->seen[t] = f; a->glow[t] = 1.0f; }
        if (a->glow[t] > 0) { a->glow[t] = fmaxf(0.0f, a->glow[t] - dt * 5.0f); a->dirty = 1; }
        int st = gb_track_step(a->g, t);
        if (st != a->step_seen[t]) { a->step_seen[t] = st; a->step_t[t] = a->time; }
    }
    if (a->modified && (a->save_t += dt) > 30.0f) { a->save_t = 0; app_save(a); }
    if (a->quit_dialog) return;
    for (int b = 0; b < PAD_COUNT; b++) {
        if (!a->down[b] || !repeats(a, b)) continue;
        a->rep[b] += dt;
        while (a->rep[b] > 0.36f) {
            a->rep[b] -= 0.06f;
            handle_press(a, b);
        }
    }
    float target = (float)a->groove_sel - 6.0f;
    target = clampf(target, 0, fmaxf(0, (float)gb_groove_count() - 14));
    if (fabsf(target - a->groove_scroll) > 0.01f) { a->groove_scroll += (target - a->groove_scroll) * fminf(1.0f, dt * 12.0f); a->dirty = 1; }
}

int app_needs_draw(App *a)
{
    int d = a->dirty || gb_playing(a->g) || a->rec_f || a->exp_g || a->preview_track >= 0;
    a->dirty = 0;
    return d;
}

/* ------------------------------------------------------------------ disegno: parti comuni */
/* Sulla Flip il disegno deve stare in pochi millisecondi (misurati il 13/10/2026 a 1,1 GHz: 4 rettangoli
   semitrasparenti grandi 14,6 ms, 600 linee con antialiasing 24 ms, 224 rettangoli arrotondati 7 ms):
   pannelli con l'antialiasing solo negli angoli, caselle smussate fatte di rettangoli pieni, sfondo
   calcolato una volta, oscilloscopio a strisce verticali. */
static void panel(Canvas *c, float x, float y, float w, float h, float r, uint32_t col)
{
    if (h < 4 * r + 2 || w < 2 * r + 2) { gfx_round_rect(c, x, y, w, h, r, col, 1.0f); return; }
    gfx_round_rect(c, x, y, w, 2 * r, r, col, 1.0f);
    gfx_round_rect(c, x, y + h - 2 * r, w, 2 * r, r, col, 1.0f);
    gfx_rect(c, (int)x, (int)(y + r), (int)w, (int)(h - 2 * r), col);
}

static void bevel(Canvas *c, int x, int y, int w, int h, uint32_t col)
{
    gfx_rect(c, x + 2, y, w - 4, h, col);
    gfx_rect(c, x + 1, y + 1, w - 2, h - 2, col);
    gfx_rect(c, x, y + 2, w, h - 4, col);
}

static void draw_background(Canvas *c)
{
    static uint32_t *bg;
    static int bw, bh;
    if (!bg || bw != c->w || bh != c->h) {
        free(bg);
        bg = malloc(sizeof(uint32_t) * (size_t)c->w * (size_t)c->h);
        if (!bg) { gfx_vgradient(c, 0, 0, c->w, c->h, C_BG_TOP, C_BG_BOT); return; }
        bw = c->w;
        bh = c->h;
        Canvas t = { bg, bw, bh };
        gfx_vgradient(&t, 0, 0, bw, bh, C_BG_TOP, C_BG_BOT);
    }
    memcpy(c->px, bg, sizeof(uint32_t) * (size_t)bw * (size_t)bh);
}

static void draw_logo(Canvas *c, float x, float y, float s)
{
    for (int i = -2; i <= 2; i++)
        gfx_line(c, x, y + 8 * s, x + i * 4.5f * s, y + (1 + abs(i) * 1.5f) * s, 1.4f * s, C_GREEN, 0.9f);
    panel(c, x - 5 * s, y - 7 * s, 10 * s, 15 * s, 5 * s, C_VIOLET);
    gfx_line(c, x - 4 * s, y - 2 * s, x + 4 * s, y + 3 * s, 1.0f, C_VIOLET_D, 0.9f);
    gfx_line(c, x + 4 * s, y - 2 * s, x - 4 * s, y + 3 * s, 1.0f, C_VIOLET_D, 0.9f);
}

static void draw_header(App *a, Canvas *c)
{
    gfx_rect(c, 0, 0, c->w, 44, RGB(15, 17, 25));
    gfx_rect(c, 0, 44, c->w, 1, C_LINE);
    draw_logo(c, 22, 21, 1.2f);
    static const char *tabs[SCR_COUNT] = { "Ritmo", "Suoni", "Mixer", "Live", "Pattern" };
    int x = 42;
    for (int i = 0; i < SCR_COUNT; i++) {
        int w = gfx_text_width(FONT_BOLD, tabs[i]) + 22;
        if (i == a->screen) panel(c, x, 9, w, 27, 13.5f, C_VIOLET_D);
        gfx_text_center(c, FONT_BOLD, x + w / 2, 28, tabs[i], i == a->screen ? C_TEXT : C_MUTED);
        x += w + 2;
    }
    char t[48];
    int pat = gb_playing_pattern(a->g);
    snprintf(t, sizeof(t), "P%d  %d", pat + 1, (int)lrintf(gb_master(a->g)->bpm));
    int playing = gb_playing(a->g);
    gfx_text_right(c, FONT_BOLD, 604, 28, t, playing ? C_TEXT : C_MUTED);
    if (playing) {
        int st = gb_track_step(a->g, 0);
        gfx_circle(c, 620, 22, 7, C_GREEN, (st >= 0 && st % 4 == 0) ? 1.0f : 0.5f);
    } else {
        gfx_rect(c, 614, 16, 12, 12, C_DIM);
    }
}

static void draw_hints(Canvas *c, const char *text)
{
    gfx_rect(c, 0, 448, c->w, 32, RGB(15, 17, 25));
    gfx_rect(c, 0, 448, c->w, 1, C_LINE);
    gfx_text_center(c, FONT_SMALL, c->w / 2, 469, text, C_MUTED);
}

static void draw_scope(App *a, Canvas *c, int x, int y, int w, int h, uint32_t color)
{
    panel(c, x, y, w, h, 10, RGB(11, 13, 19));
    gfx_rect(c, x + 8, y + h / 2, w - 16, 1, C_LINE);
    int n = w - 16;
    if (n > 600) n = 600;
    gb_scope(a->g, a->scope, n * 2);
    int start = 0;
    for (int i = 1; i < n; i++)
        if (a->scope[i - 1] <= 0.0f && a->scope[i] > 0.0f) { start = i; break; }
    float peak = 0.05f;
    for (int i = 0; i < n; i++) peak = fmaxf(peak, fabsf(a->scope[start + i]));
    float gain = (h * 0.42f) / fmaxf(peak, 0.25f);
    uint32_t glow = gfx_mix(RGB(11, 13, 19), color, 0.3f);
    int py = 0;
    for (int i = 0; i < n; i++) {
        int yy = (int)lrintf(y + h / 2.0f - a->scope[start + i] * gain);
        yy = clampi(yy, y + 2, y + h - 3);
        if (i) {
            int top = py < yy ? py : yy, bot = py < yy ? yy : py;
            gfx_rect(c, x + 8 + i, top - 2, 1, bot - top + 5, glow);
            gfx_rect(c, x + 8 + i, top, 1, bot - top + 2, color);
        }
        py = yy;
    }
}

static void draw_bar(Canvas *c, float x, float y, float w, float h, float v, uint32_t col, int bipolar)
{
    panel(c, x, y, w, h, h / 2, C_PANEL_HI);
    if (bipolar) {
        float mid = x + w / 2, end = mid + clampf(v, -1, 1) * w / 2;
        panel(c, fminf(mid, end), y, fabsf(end - mid) + 2, h, h / 2, col);
        gfx_rect(c, (int)mid, (int)y - 2, 1, (int)h + 4, C_DIM);
    } else {
        panel(c, x, y, fmaxf(h, clampf(v, 0, 1) * w), h, h / 2, v > 0 ? col : C_PANEL_HI);
    }
}

/* ------------------------------------------------------------------ disegno: Ritmo */
#define GRID_X 124
#define GRID_Y 76
#define CELL_W 31
#define ROW_H 26

static void draw_cell(App *a, Canvas *c, int x, int y, const GbStep *s, int t, int inside, int playhead)
{
    uint32_t col = track_color(a, t);
    int w = CELL_W - 3, h = ROW_H - 4;
    if (!inside) {
        bevel(c, x + 1, y + 2, w, h, gfx_mix(RGB(16, 18, 26), C_PANEL, 0.45f));
        return;
    }
    uint32_t fill = C_CELL;
    if (s->on) {
        float k = 0.35f + 0.65f * s->vel / 127.0f;
        fill = gfx_mix(C_CELL, col, k);
        if (is_melodic(t) && s->accent) fill = gfx_mix(fill, C_TEXT, 0.25f);
    }
    if (playhead) fill = gfx_mix(fill, C_TEXT, s->on ? 0.5f : 0.15f);
    bevel(c, x + 1, y + 2, w, h, fill);
    if (!s->on) return;
    uint32_t ink = RGB(16, 18, 26);
    if (is_melodic(t)) {
        int m = 48 + s->note;
        gfx_text_center(c, FONT_SMALL, x + 1 + w / 2, y + 17, NOTE_NAMES[((m % 12) + 12) % 12], ink);
        if (t == GB_ACID && s->slide) gfx_rect(c, x + w - 3, y + 11, 6, 2, ink);
        if (t == GB_FOLD && s->len > 1) gfx_rect(c, x + w - 2, y + 9, 5, 8, gfx_mix(fill, ink, 0.5f));
    } else if (s->ratchet > 1) {
        for (int r = 0; r < s->ratchet; r++) gfx_rect(c, x + 6 + r * 5, y + 15, 3, 3, ink);
    }
    if (s->prob < 100) {                              /* probabilita': angolo tagliato */
        gfx_rect(c, x + 2, y + 3, 5, 5, C_BG_TOP);
    }
    if (s->cond != COND_ALWAYS) gfx_circle(c, x + w - 4, y + 7, 2.5f, s->cond == COND_FILL ? C_AMBER : ink, 1.0f);
    if (s->lock_tune != GB_LOCK_NONE || s->lock_decay != GB_LOCK_NONE || s->lock_tone != GB_LOCK_NONE)
        gfx_rect(c, x + 5, y + h - 1, w - 8, 2, ink);
    if (s->micro) gfx_rect(c, x + 1 + w / 2 + s->micro * w / 120, y + 3, 2, 4, ink);
}

static void draw_editor(App *a, Canvas *c)
{
    int f[MAX_FIELDS], n = editor_fields(a, f);
    int x = 300, y = 52, w = 330, rh = 21;
    /* le tracce melodiche hanno 20 campi: oltre 17 righe la lista scorre e tiene in vista quello scelto */
    int vis = n < 17 ? n : 17, first = 0;
    if (n > vis) first = clampi(a->ed_sel - vis / 2, 0, n - vis);
    int h = 38 + vis * rh;
    panel(c, x, y, w, h, 14, C_PANEL_HI);
    gfx_round_frame(c, x, y, w, h, 14, 1.5f, track_color(a, a->cy), 1.0f);
    char t[96];
    snprintf(t, sizeof(t), "Passo %d · %s", a->cx + 1, track_label(a, a->cy));
    gfx_text(c, FONT_BOLD, x + 14, y + 25, t, C_TEXT);
    for (int k = 0; k < 5; k++) {                     /* triangolini: c'e' altro sopra / sotto */
        if (first > 0) gfx_rect(c, x + w - 40 - k, y + 12 + k, 1 + 2 * k, 1, C_MUTED);
        if (first + vis < n) gfx_rect(c, x + w - 24 - k, y + 20 - k, 1 + 2 * k, 1, C_MUTED);
    }
    for (int i = first; i < first + vis; i++) {
        int yy = y + 34 + (i - first) * rh;
        if (f[i] == F_HEADER) {
            gfx_rect(c, x + 14, yy + 10, w - 28, 1, C_LINE);
            gfx_rect(c, x + 14, yy + 3, gfx_text_width(FONT_SMALL, "Traccia") + 10, 14, C_PANEL_HI);
            gfx_text(c, FONT_SMALL, x + 16, yy + 15, "Traccia", C_VIOLET);
            continue;
        }
        int sel = i == a->ed_sel;
        if (sel) panel(c, x + 8, yy, w - 16, rh - 1, 8, C_VIOLET_D);
        gfx_text(c, FONT_SMALL, x + 16, yy + 15, field_name(a, f[i]), sel ? C_TEXT : C_MUTED);
        char v[48];
        field_value(a, f[i], v, sizeof(v));
        gfx_text_right(c, FONT_SMALL, x + w - 16, yy + 15, v, sel ? C_TEXT : RGB(205, 208, 222));
    }
}

static void draw_ritmo(App *a, Canvas *c)
{
    GbPattern *p = gb_pattern(a->g, a->edit_pat);
    int page = a->cx / 16, steps = grid_steps(a);
    int same = gb_playing(a->g) && gb_playing_pattern(a->g) == a->edit_pat;
    char t[160];
    snprintf(t, sizeof(t), "Pattern %d%s%s · pagina %d/%d", a->edit_pat + 1, p->name[0] ? " · " : "", p->name, page + 1, steps / 16);
    char fit[160];
    fit_text(FONT_SMALL, t, 380, fit, sizeof(fit));
    gfx_text(c, FONT_SMALL, 14, 66, fit, C_MUTED);
    if (gb_playing(a->g) && !same) gfx_text(c, FONT_SMALL, 400, 66, "(suona un altro)", C_AMBER);
    snprintf(t, sizeof(t), "Kit %s", gb_kit_name(a->kit));
    gfx_text_right(c, FONT_SMALL, 626, 66, t, C_MUTED);
    for (int i = 0; i < 16; i += 4)
        gfx_rect(c, GRID_X + i * CELL_W - 1, GRID_Y - 2, CELL_W * 4 - 1, ROW_H * GB_TRACKS + 3, gfx_mix(RGB(16, 18, 26), C_PANEL, 0.6f));
    for (int t2 = 0; t2 < GB_TRACKS; t2++) {
        int y = GRID_Y + t2 * ROW_H;
        GbTrackPattern *tr = &p->track[t2];
        GbMix *mx = gb_mix(a->g, t2);
        uint32_t col = track_color(a, t2);
        if (t2 == GB_ACID) gfx_rect(c, 10, y - 1, 616, 1, C_LINE);
        char nm[40];
        fit_text(FONT_SMALL, track_label(a, t2), 96, nm, sizeof(nm));
        uint32_t nc = gfx_mix(t2 == a->cy ? C_TEXT : C_MUTED, col, a->glow[t2] * 0.9f);
        if (mx->mute) nc = C_DIM;
        gfx_circle(c, 14, y + 13, 4, mx->mute ? C_DIM : col, 0.4f + 0.6f * a->glow[t2]);
        gfx_text_right(c, FONT_SMALL, GRID_X - 6, y + 18, nm, nc);
        int pst = same ? gb_track_step(a->g, t2) : -1;
        for (int i = 0; i < 16; i++) {
            int si = page * 16 + i;
            draw_cell(a, c, GRID_X + i * CELL_W, y, &tr->step[si], t2, si < tr->length, si == pst);
            if (t2 == a->cy && si == a->cx) gfx_round_frame(c, GRID_X + i * CELL_W - 1, y + 1, CELL_W - 1, ROW_H - 2, 7, 2.2f, C_AMBER, 1.0f);
        }
        if (tr->length != 16 || tr->speed) {           /* lunghezza o velocita' propria: a destra */
            snprintf(t, sizeof(t), "%d%s", tr->length, tr->speed == 1 ? "»" : tr->speed == 2 ? "«" : tr->speed == 3 ? "³" : "");
            gfx_text(c, FONT_SMALL, GRID_X + 16 * CELL_W + 1, y + 18, t, C_DIM);
        }
    }
    if (a->editor) {
        draw_editor(a, c);
        draw_hints(c, "su/giù scegli · sin./des. cambia (L1/R1 di più) · A esegue · Y ascolta · B chiude");
    } else if (a->down[PAD_B]) {
        draw_hints(c, is_melodic(a->cy) ? "B + su/giù: nota · B + sin./des.: ottava" : "B + su/giù: dinamica · B + sin./des.: ripetizioni");
    } else {
        draw_hints(c, "A passo · B+croce valore · X scheda · Y ascolta · L1/R1 pagina · L2/R2 pattern · START");
    }
}

/* ------------------------------------------------------------------ disegno: Suoni */
static void draw_suoni(App *a, Canvas *c)
{
    panel(c, 12, 54, 212, 388, 14, C_PANEL);
    char t[120], fit[120];
    gfx_text(c, FONT_SMALL, 26, 78, "Kit (L2/R2)", C_MUTED);
    snprintf(t, sizeof(t), "%d/%d", a->kit + 1, gb_kit_count());
    gfx_text_right(c, FONT_SMALL, 210, 78, t, C_DIM);
    fit_text(FONT_TITLE, gb_kit_name(a->kit), 184, fit, sizeof(fit));
    gfx_text(c, FONT_TITLE, 26, 108, fit, C_TEXT);
    for (int t2 = 0; t2 < GB_TRACKS; t2++) {
        int y = 122 + t2 * 22;
        int sel = t2 == a->snd_track;
        uint32_t col = track_color(a, t2);
        if (sel) panel(c, 18, y, 200, 21, 8, C_VIOLET_D);
        gfx_circle(c, 30, y + 10.5f, 4.5f, col, 0.45f + 0.55f * a->glow[t2]);
        fit_text(FONT_SMALL, track_label(a, t2), 168, fit, sizeof(fit));
        gfx_text(c, FONT_SMALL, 42, y + 16, fit, sel ? C_TEXT : gfx_mix(C_MUTED, col, a->glow[t2]));
    }
    int n, target;
    const Param *ps = track_params(a->snd_track, &n, &target);
    void *base = target_ptr(a, target, a->snd_track);
    uint32_t col = track_color(a, a->snd_track);
    panel(c, 234, 54, 394, 388, 14, C_PANEL);
    fit_text(FONT_TITLE, track_label(a, a->snd_track), 250, fit, sizeof(fit));
    gfx_text(c, FONT_TITLE, 250, 86, fit, C_TEXT);
    const char *sub = target == T_ACID ? "Basso acido, monofonico" : target == T_FOLD ? "West Coast, 4 voci" : gb_model_name(gb_drum(a->g, a->snd_track)->model);
    gfx_text_right(c, FONT_SMALL, 614, 84, sub, col);
    gfx_circle(c, 600, 112, 9, col, 0.25f + 0.75f * a->glow[a->snd_track]);
    for (int i = 0; i < n; i++) {
        int y = 100 + i * 27;
        int sel = i == a->snd_sel;
        if (sel) panel(c, 242, y, 340, 26, 9, C_VIOLET_D);
        gfx_text(c, FONT_BODY, 254, y + 19, ps[i].name, sel ? C_TEXT : RGB(205, 208, 222));
        char v[64];
        param_text(a, &ps[i], base, v, sizeof(v));
        fit_text(FONT_BOLD, v, 170, fit, sizeof(fit));
        gfx_text_right(c, FONT_BOLD, 572, y + 19, fit, sel ? C_TEXT : C_MUTED);
        float fr = param_fraction(&ps[i], base);
        if (fr >= 0 && sel) gfx_rect(c, 254, y + 22, (int)(306 * clampf(fr, 0, 1)), 2, C_AMBER);
    }
    draw_scope(a, c, 242, 376, 378, 58, col);
    draw_hints(c, "su/giù scegli · sin./des. cambia (B tenuto: di più) · L1/R1 traccia · A ascolta · Y variazione");
}

/* ------------------------------------------------------------------ disegno: Mixer */
static void draw_mixer(App *a, Canvas *c)
{
    int sx = 10, sw = 33;
    char t[96];
    int any_solo = 0;
    for (int i = 0; i < GB_TRACKS; i++) any_solo |= gb_mix(a->g, i)->solo;
    for (int i = 0; i < GB_TRACKS; i++) {
        int x = sx + i * sw;
        GbMix *m = gb_mix(a->g, i);
        uint32_t col = track_color(a, i);
        int sel = i == a->mix_x;
        panel(c, x, 82, sw - 3, 356, 9, sel ? C_PANEL_HI : C_PANEL);
        if (sel) gfx_round_frame(c, x, 82, sw - 3, 356, 9, 1.5f, C_AMBER, 1.0f);
        snprintf(t, sizeof(t), "%s", i == GB_ACID ? "Ac" : i == GB_FOLD ? "Pi" : "");
        if (i < GB_DRUMS) snprintf(t, sizeof(t), "%d", i + 1);
        gfx_text_center(c, FONT_SMALL, x + (sw - 3) / 2, 100, t, sel ? C_TEXT : C_MUTED);
        gfx_circle(c, x + (sw - 3) / 2.0f, 112, 4, col, 0.4f + 0.6f * a->glow[i]);
        /* fader e livello */
        float fy = 124, fh = 180;
        panel(c, x + 6, fy, 6, fh, 3, C_CELL);
        float lv = clampf(gb_track_level(a->g, i), 0, 1);
        int audible = !m->mute && (!any_solo || m->solo);
        if (lv > 0.001f) panel(c, x + 6, fy + fh * (1 - lv), 6, fh * lv, 3, audible ? C_GREEN : C_DIM);
        float ky = fy + fh * (1 - m->level);
        panel(c, x + 15, ky - 6, 12, 12, 4, (sel && a->mix_row == MIX_LEVEL) ? C_AMBER : C_TEXT);
        gfx_rect(c, x + 17, (int)ky, 8, 1, C_PANEL);
        /* pan e mandate */
        float pan = *pan_ptr(a, i);
        int bx = x + 3, bw2 = sw - 9;
        draw_bar(c, bx, 318, bw2, 5, pan, (sel && a->mix_row == MIX_PAN) ? C_AMBER : C_CYAN, 1);
        draw_bar(c, bx, 334, bw2, 5, m->delay_send, (sel && a->mix_row == MIX_DELAY) ? C_AMBER : C_VIOLET, 0);
        draw_bar(c, bx, 350, bw2, 5, m->reverb_send, (sel && a->mix_row == MIX_REVERB) ? C_AMBER : C_PINK, 0);
        /* muto e solo */
        panel(c, x + 3, 366, sw - 9, 28, 7, m->mute ? C_RED : C_CELL);
        gfx_text_center(c, FONT_SMALL, x + (sw - 3) / 2, 385, "M", m->mute ? C_TEXT : C_DIM);
        panel(c, x + 3, 400, sw - 9, 28, 7, m->solo ? C_AMBER : C_CELL);
        gfx_text_center(c, FONT_SMALL, x + (sw - 3) / 2, 419, "S", m->solo ? RGB(20, 20, 20) : C_DIM);
    }
    /* riga di stato in alto */
    if (a->mix_x < GB_TRACKS) {
        GbMix *m = gb_mix(a->g, a->mix_x);
        float v = a->mix_row == MIX_LEVEL ? m->level : a->mix_row == MIX_DELAY ? m->delay_send : a->mix_row == MIX_REVERB ? m->reverb_send : 0;
        char val[32];
        if (a->mix_row == MIX_PAN) {
            int pv = (int)lrintf(*pan_ptr(a, a->mix_x) * 100);
            if (pv) snprintf(val, sizeof(val), "%s %d", pv < 0 ? "sin." : "des.", abs(pv)); else snprintf(val, sizeof(val), "centro");
        } else {
            snprintf(val, sizeof(val), "%d %%", (int)lrintf(v * 100));
        }
        snprintf(t, sizeof(t), "%s · %s %s", track_label(a, a->mix_x), MIX_ROW_NAMES[a->mix_row], val);
    } else {
        snprintf(t, sizeof(t), "Master · %s", MASTER_PARAMS[a->master_sel].name);
    }
    gfx_text(c, FONT_BOLD, 14, 72, t, C_TEXT);
    /* master */
    int mx = 476, mw = 152;
    int sel = a->mix_x == GB_TRACKS;
    panel(c, mx, 52, mw, 386, 12, sel ? C_PANEL_HI : C_PANEL);
    if (sel) gfx_round_frame(c, mx, 52, mw, 386, 12, 1.5f, C_AMBER, 1.0f);
    for (int i = 0; i < N_MASTER_P; i++) {
        int y = 57 + i * 25;
        const Param *p = &MASTER_PARAMS[i];
        int on = sel && i == a->master_sel;
        if (on) panel(c, mx + 4, y, mw - 8, 24, 8, C_VIOLET_D);
        char nm[40];
        fit_text(FONT_SMALL, p->kind == K_CLOCK ? "Clock MIDI" : p->name, 92, nm, sizeof(nm));
        gfx_text(c, FONT_SMALL, mx + 10, y + 17, nm, on ? C_TEXT : C_MUTED);
        char v[40];
        if (p->kind == K_CLOCK) {
            snprintf(v, sizeof(v), "%s", a->clock_out ? "sì" : "no");
        } else if (p->kind == K_BIPOLAR) {
            int fv = (int)lrintf(a->filter_base * 100);
            if (fv) snprintf(v, sizeof(v), "%s %d", fv < 0 ? "bassi" : "alti", abs(fv)); else snprintf(v, sizeof(v), "aperto");
        } else {
            param_text(a, p, gb_master(a->g), v, sizeof(v));
        }
        gfx_text_right(c, FONT_SMALL, mx + mw - 10, y + 17, v, on ? C_TEXT : RGB(205, 208, 222));
    }
    if (sel) draw_hints(c, "L1/R1 scegli · su/giù cambia (B tenuto: di più) · sin./des. traccia");
    else draw_hints(c, "sin./des. traccia · L1/R1 volume, pan, eco, riverbero · su/giù cambia · A muto · Y solo · X ascolta");
}

/* ------------------------------------------------------------------ disegno: Live */
static void draw_pad(App *a, Canvas *c, float cx, float cy, float w, float h, int t, const char *key)
{
    GbMix *m = gb_mix(a->g, t);
    uint32_t col = track_color(a, t);
    float g = a->glow[t];
    int pressed = a->down[LIVE_BTNS[t]];
    uint32_t fill = m->mute ? RGB(30, 30, 38) : gfx_mix(gfx_mix(C_PANEL_HI, col, 0.25f), col, fmaxf(g, pressed ? 0.6f : 0.0f));
    if (g > 0.05f && !m->mute) gfx_round_rect(c, cx - w / 2 - 5, cy - h / 2 - 5, w + 10, h + 10, 16, col, 0.25f * g);
    panel(c, cx - w / 2, cy - h / 2, w, h, 12, fill);
    char nm[40];
    fit_text(FONT_SMALL, track_label(a, t), (int)w - 10, nm, sizeof(nm));
    gfx_text_center(c, FONT_SMALL, (int)cx, (int)cy + 2, nm, m->mute ? C_DIM : C_TEXT);
    gfx_text_center(c, FONT_SMALL, (int)cx, (int)(cy + h / 2 - 6), m->mute ? "muto" : key, m->mute ? C_RED : gfx_mix(C_MUTED, C_TEXT, g));
}

static void draw_live(App *a, Canvas *c)
{
    char t[120];
    uint32_t mc = a->live_mode == LIVE_REC ? C_RED : a->live_mode == LIVE_MUTE ? C_AMBER : C_GREEN;
    int w = gfx_text_width(FONT_BOLD, LIVE_MODE_NAMES[a->live_mode]) + 28;
    gfx_round_rect(c, 14, 54, w, 28, 14, mc, 0.25f);
    gfx_round_frame(c, 14, 54, w, 28, 14, 1.5f, mc, 1.0f);
    gfx_text_center(c, FONT_BOLD, 14 + w / 2, 74, LIVE_MODE_NAMES[a->live_mode], C_TEXT);
    snprintf(t, sizeof(t), "Pattern %d · giro %d · kit %s", gb_playing_pattern(a->g) + 1, gb_bar_count(a->g) + 1, gb_kit_name(a->kit));
    gfx_text_right(c, FONT_SMALL, 626, 73, t, C_MUTED);
    /* dorsali in alto: L2 L1 R1 R2 */
    static const int top[4] = { 8, 9, 10, 11 };
    for (int i = 0; i < 4; i++) draw_pad(a, c, 88 + i * 155, 118, 140, 54, top[i], LIVE_LABELS[top[i]]);
    /* croce a sinistra, tasti a destra (ovest, sud, est, nord) */
    static const float off[4][2] = { { -1, 0 }, { 0, 1 }, { 1, 0 }, { 0, -1 } };
    for (int i = 0; i < 4; i++) {
        draw_pad(a, c, 166 + off[i][0] * 92, 274 + off[i][1] * 66, 104, 58, i, LIVE_LABELS[i]);
        draw_pad(a, c, 474 + off[i][0] * 92, 274 + off[i][1] * 66, 104, 58, 4 + i, LIVE_LABELS[4 + i]);
    }
    /* levette */
    gfx_text(c, FONT_SMALL, 14, 382, "Filtro DJ", C_MUTED);
    draw_bar(c, 90, 372, 140, 8, gb_master(a->g)->filter, C_CYAN, 1);
    gfx_text(c, FONT_SMALL, 250, 382, "Stutter", C_MUTED);
    const char *rp = a->repeat_div == 0 ? "spento" : a->repeat_div >= 4 ? "1/4" : a->repeat_div >= 2 ? "1/8" : a->repeat_div >= 1 ? "1/16" : "1/32";
    gfx_text(c, FONT_BOLD, 312, 382, rp, a->repeat_div > 0 ? C_AMBER : C_DIM);
    gfx_text(c, FONT_SMALL, 400, 382, "Fill", C_MUTED);
    gfx_text(c, FONT_BOLD, 436, 382, a->fill ? "sì" : "no", a->fill ? C_AMBER : C_DIM);
    draw_scope(a, c, 12, 394, 616, 48, C_GREEN);
    draw_hints(c, "i 12 tasti suonano · L3 modo (suona, registra, zittisci) · levette: filtro, stutter, fill");
}

/* ------------------------------------------------------------------ disegno: Pattern */
static void draw_pattern(App *a, Canvas *c)
{
    char t[120], fit[120];
    int playing = gb_playing_pattern(a->g);
    for (int i = 0; i < GB_PATTERNS; i++) {
        int col = i % 4, row = i / 4;
        float x = 12 + col * 92, y = 54 + row * 66;
        GbPattern *p = gb_pattern(a->g, i);
        int sel = a->pat_focus == 0 && i == a->pat_sel, edit = i == a->edit_pat;
        panel(c, x, y, 86, 60, 10, edit ? C_VIOLET_D : C_PANEL);
        if (sel) gfx_round_frame(c, x - 1, y - 1, 88, 62, 11, 2.2f, C_AMBER, 1.0f);
        snprintf(t, sizeof(t), "%d", i + 1);
        gfx_text(c, FONT_BOLD, (int)x + 8, (int)y + 21, t, C_TEXT);
        if (i == playing && gb_playing(a->g)) gfx_circle(c, x + 74, y + 12, 5, C_GREEN, 1.0f);
        if (a->clip == i) gfx_text_right(c, FONT_SMALL, (int)x + 80, (int)y + 20, "copia", C_AMBER);
        fit_text(FONT_SMALL, p->name[0] ? p->name : (pattern_is_empty(p) ? "vuoto" : ""), 74, fit, sizeof(fit));
        gfx_text(c, FONT_SMALL, (int)x + 8, (int)y + 38, fit, p->name[0] ? C_MUTED : C_DIM);
        /* densita': un trattino per traccia */
        for (int t2 = 0; t2 < GB_TRACKS; t2++) {
            int hits = 0;
            for (int s = 0; s < p->track[t2].length; s++) hits += p->track[t2].step[s].on;
            if (!hits) continue;
            float h = fminf(10.0f, 2.0f + hits * 0.6f);
            gfx_rect(c, (int)x + 8 + t2 * 5, (int)(y + 54 - h), 3, (int)h, track_color(a, t2));
        }
    }
    /* catena */
    panel(c, 12, 322, 362, 120, 12, C_PANEL);
    snprintf(t, sizeof(t), "Catena (%d)", a->chain_n);
    gfx_text(c, FONT_SMALL, 24, 344, t, a->chain_n ? C_AMBER : C_MUTED);
    if (!a->chain_n) gfx_text(c, FONT_SMALL, 24, 370, "spenta: X aggiunge il pattern scelto", C_DIM);
    int pos = gb_chain_position(a->g);
    int first = a->chain_n > 24 ? a->chain_n - 24 : 0;
    for (int i = first; i < a->chain_n; i++) {
        int k = i - first, x = 24 + (k % 12) * 29, y = 354 + (k / 12) * 34;
        int cur = gb_playing(a->g) && i == pos;
        panel(c, x, y, 26, 28, 7, cur ? C_GREEN : C_PANEL_HI);
        snprintf(t, sizeof(t), "%d", a->chain[i] + 1);
        gfx_text_center(c, FONT_SMALL, x + 13, y + 19, t, cur ? RGB(16, 18, 26) : C_TEXT);
    }
    /* ritmi pronti */
    int lx = 384, lw = 244;
    panel(c, lx, 54, lw, 388, 12, a->pat_focus == 1 ? C_PANEL_HI : C_PANEL);
    snprintf(t, sizeof(t), "Ritmi pronti (%d)", gb_groove_count());
    gfx_text(c, FONT_BOLD, lx + 12, 78, t, C_TEXT);
    int nG = gb_groove_count();
    float sc = a->groove_scroll;
    for (int i = (int)sc; i < nG && i < (int)sc + 15; i++) {
        float y = 88 + (i - sc) * 23;
        if (y < 84 || y > 418) continue;
        int on = a->pat_focus == 1 && i == a->groove_sel;
        if (on) panel(c, lx + 6, y, lw - 12, 22, 8, C_VIOLET_D);
        fit_text(FONT_SMALL, gb_groove_name(i), 160, fit, sizeof(fit));
        gfx_text(c, FONT_SMALL, lx + 14, (int)y + 16, fit, on ? C_TEXT : RGB(205, 208, 222));
        snprintf(t, sizeof(t), "%d", (int)lrintf(gb_groove_bpm(i)));
        gfx_text_right(c, FONT_SMALL, lx + lw - 14, (int)y + 16, t, C_DIM);
    }
    if (a->exp_g) {
        snprintf(t, sizeof(t), "Esportazione... %u s", a->exp_frames / GB_RATE);
        draw_hints(c, t);
    } else if (a->pat_focus == 1) {
        draw_hints(c, "su/giù scegli · A carica nel pattern che modifichi (con kit e tempo) · sinistra torna");
    } else {
        draw_hints(c, "A modifica · X in catena · L1 toglie dalla catena · Y copia/incolla · B svuota · R1 WAV");
    }
}

static void draw_quit(Canvas *c)
{
    gfx_rect_alpha(c, 0, 0, c->w, c->h, RGB(0, 0, 0), 0.6f);
    panel(c, 150, 170, 340, 140, 18, C_PANEL_HI);
    gfx_round_frame(c, 150, 170, 340, 140, 18, 2, C_VIOLET, 1.0f);
    gfx_text_center(c, FONT_TITLE, 320, 222, "Uscire dal synth?", C_TEXT);
    gfx_text_center(c, FONT_BODY, 320, 256, "Suoni e pattern restano salvati.", C_MUTED);
    gfx_text_center(c, FONT_BOLD, 320, 290, "A o giù: esci   ·   B o su: resta", C_AMBER);
}

#ifdef GB_PROFILE
double gb_prof[3];
double gb_prof_now(void);
#define PROF(i, x) do { double t_ = gb_prof_now(); x; gb_prof[i] += gb_prof_now() - t_; } while (0)
#else
#define PROF(i, x) x
#endif

void app_draw(App *a, Canvas *c)
{
    PROF(0, draw_background(c));
    PROF(1, draw_header(a, c));
    switch (a->screen) {
    case SCR_RITMO: PROF(2, draw_ritmo(a, c)); break;
    case SCR_SUONI: PROF(2, draw_suoni(a, c)); break;
    case SCR_MIXER: PROF(2, draw_mixer(a, c)); break;
    case SCR_LIVE: PROF(2, draw_live(a, c)); break;
    default: PROF(2, draw_pattern(a, c)); break;
    }
    if (a->toast_t > 0.0f && a->toast[0]) {
        float al = fminf(1.0f, a->toast_t * 3.0f);
        char fit[140];
        fit_text(FONT_BOLD, a->toast, 580, fit, sizeof(fit));
        int w = gfx_text_width(FONT_BOLD, fit) + 36;
        gfx_round_rect(c, (c->w - w) / 2.0f, 404, w, 34, 17, C_PANEL_HI, 0.96f * al);
        gfx_round_frame(c, (c->w - w) / 2.0f, 404, w, 34, 17, 1.5f, C_VIOLET, al);
        gfx_text_center(c, FONT_BOLD, c->w / 2, 427, fit, gfx_mix(C_BG_BOT, C_TEXT, al));
    }
    if (a->rec_f) {
        unsigned secs = a->rec_frames / (unsigned)gb_sample_rate(a->g);
        char t[64];
        snprintf(t, sizeof(t), "REC %u:%02u   ·   R3 per fermare", secs / 60, secs % 60);
        gfx_rect(c, 0, 449, c->w, 31, RGB(42, 16, 22));
        int w = gfx_text_width(FONT_BOLD, t);
        gfx_circle(c, (c->w - w) / 2.0f - 14, 464, 6, C_RED, 0.55f + 0.45f * sinf(a->time * 6.0f));
        gfx_text_center(c, FONT_BOLD, c->w / 2 + 4, 470, t, C_TEXT);
    }
    if (a->quit_dialog) draw_quit(c);
}
