/* OpenOrc per Miyoo Flip - applicazione (vedi orc_app.h).
 *
 * Gli accordi si suonano come sull'Orchid: tenendo un pad di qualita' (Dim, Min, Maj, Sus) ogni tasto
 * suona l'accordo su quella fondamentale e i pad 6, m7, M7 e 9 aggiungono le estensioni. Senza pad i
 * tasti suonano la melodia; con il modo tonalita' attivo suonano gli accordi della scala (i tasti fuori
 * scala prendono il grado sotto). Il voicing sceglie da solo il rivolto piu' vicino al registro della
 * manopola K1, cosi' le voci passano da un accordo all'altro muovendosi poco.
 *
 * MPK mini IV (valori di partenza, tutti riassegnabili nella pagina MIDI):
 *   pad banco A, canale 10: 40-43 Dim Min Maj Sus (fila in alto), 36-39 6 m7 M7 9 (fila in basso);
 *   pad banco B: 44-47 batteria, loop registra, loop suona/ferma, loop cancella (fila in basso),
 *                48-51 modo tonalita', esecuzione successiva, suono precedente, suono successivo;
 *   manopole CC 70-77: voicing, basso, quantita' dell'esecuzione, tono, chorus, delay, riverbero, volume;
 *   rotelle: pitch bend (+-2 semitoni) e modulazione (vibrato); pedale sustain (CC 64);
 *   program change: suono. I tasti di trasporto escono su un'altra porta e si collegano dalla pagina MIDI.
 * Sulla Flip, nella pagina Suona, croce e A B X Y suonano gli accordi della tonalita' (I IV V vi sulla
 * croce) e i dorsali li cambiano (L1 maggiore/minore, R1 sus4, L2 settima, R2 nona).
 */
#include "orc_app.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chords.h"
#include "midi.h"
#include "platform.h"

/* ------------------------------------------------------------------ palette (come le altre app Cirmolo) */
#define C_BG_TOP   RGB(13, 16, 22)
#define C_BG_BOT   RGB(18, 23, 32)
#define C_BAR      RGB(16, 20, 28)
#define C_PANEL    RGB(25, 31, 42)
#define C_PANEL_HI RGB(34, 42, 57)
#define C_LINE     RGB(48, 58, 78)
#define C_TEXT     RGB(236, 236, 244)
#define C_MUTED    RGB(138, 148, 170)
#define C_DIM      RGB(88, 98, 120)
#define C_GOLD     RGB(242, 196, 92)
#define C_GOLD_D   RGB(112, 84, 34)
#define C_VIOLET   RGB(150, 116, 242)
#define C_VIOLET_D RGB(84, 60, 160)
#define C_TEAL     RGB(84, 200, 196)
#define C_GREEN    RGB(92, 190, 126)
#define C_RED      RGB(236, 96, 104)
#define C_KEY_W    RGB(214, 218, 228)
#define C_KEY_B    RGB(34, 38, 48)

static float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static float clamp01(float x) { return clampf(x, 0.0f, 1.0f); }

/* ------------------------------------------------------------------ suoni */
typedef struct { const char *name; OrcSound s; int spread; } Preset;
/* motore, tono, carattere, attacco, decadimento, sostegno, rilascio, vibrato, basso, tono del basso,
   chorus, delay, riverbero, esecuzione, quantita', volume */
static const Preset PRESETS[] = {
    { "Lungolago",          { ENG_VA,    0.45f, 0.45f, 0.600f, 1.2f, 0.80f, 1.80f, 0.10f, 0.50f, 0.30f, 0.60f, 0.20f, 0.60f, PERF_CHORD,   0.30f, 0.80f }, SPREAD_OPEN },
    { "Tramonto FM",        { ENG_FM,    0.30f, 0.00f, 0.003f, 2.2f, 0.00f, 0.60f, 0.15f, 0.45f, 0.30f, 0.45f, 0.15f, 0.35f, PERF_CHORD,   0.30f, 0.80f }, SPREAD_CLOSE },
    { "Wurli di notte",     { ENG_REED,  0.45f, 0.50f, 0.003f, 1.6f, 0.30f, 0.35f, 0.35f, 0.40f, 0.30f, 0.15f, 0.10f, 0.30f, PERF_SLOP,    0.35f, 0.80f }, SPREAD_CLOSE },
    { "Arpa di vetro",      { ENG_FM,    0.50f, 0.30f, 0.002f, 1.8f, 0.00f, 1.20f, 0.05f, 0.00f, 0.30f, 0.30f, 0.35f, 0.55f, PERF_HARP,    0.35f, 0.80f }, SPREAD_CLOSE },
    { "Cascata",            { ENG_VA,    0.60f, 0.20f, 0.002f, 0.4f, 0.40f, 0.25f, 0.00f, 0.35f, 0.40f, 0.30f, 0.45f, 0.40f, PERF_ARP,     0.65f, 0.80f }, SPREAD_CLOSE },
    { "Psichedelia",        { ENG_VA,    0.50f, 0.85f, 0.010f, 0.8f, 0.60f, 0.60f, 0.30f, 0.50f, 0.50f, 0.80f, 0.50f, 0.50f, PERF_SLOP,    0.70f, 0.80f }, SPREAD_WIDE },
    { "Organo della pieve", { ENG_ORGAN, 0.40f, 0.50f, 0.006f, 0.5f, 1.00f, 0.08f, 0.35f, 0.50f, 0.30f, 0.60f, 0.00f, 0.35f, PERF_CHORD,   0.30f, 0.75f }, SPREAD_CLOSE },
    { "Ottoni",             { ENG_VA,    0.30f, 0.30f, 0.050f, 0.6f, 0.75f, 0.25f, 0.12f, 0.50f, 0.40f, 0.20f, 0.10f, 0.35f, PERF_STRUM,   0.05f, 0.80f }, SPREAD_OPEN },
    { "Ritmo in levare",    { ENG_VA,    0.55f, 0.40f, 0.002f, 0.3f, 0.50f, 0.12f, 0.00f, 0.60f, 0.50f, 0.30f, 0.25f, 0.25f, PERF_PATTERN, 0.30f, 0.80f }, SPREAD_CLOSE },
    { "Campane lunari",     { ENG_FM,    0.70f, 0.75f, 0.002f, 3.0f, 0.00f, 2.50f, 0.00f, 0.00f, 0.30f, 0.20f, 0.40f, 0.70f, PERF_STRUM,   0.40f, 0.75f }, SPREAD_WIDE },
    { "Chitarra di carta",  { ENG_VA,    0.35f, 0.10f, 0.002f, 0.7f, 0.00f, 0.30f, 0.00f, 0.40f, 0.30f, 0.20f, 0.10f, 0.30f, PERF_STRUM,   0.30f, 0.85f }, SPREAD_CLOSE },
    { "Nebbia",             { ENG_VA,    0.25f, 0.70f, 1.200f, 2.0f, 0.90f, 2.50f, 0.20f, 0.30f, 0.20f, 0.70f, 0.30f, 0.80f, PERF_CHORD,   0.30f, 0.80f }, SPREAD_WIDE },
};
#define PRESET_COUNT ((int)(sizeof(PRESETS) / sizeof(PRESETS[0])))

static const char *ENGINE_NAMES[ENG_COUNT] = { "Analogico", "FM", "Piano elettrico", "Organo" };
static const char *PERFORM_NAMES[PERF_COUNT] = { "Accordo", "Strum", "Slop", "Arpeggio", "Pattern", "Arpa" };
static const char *BEAT_NAMES[BEAT_COUNT] = { "Disco", "Psichedelico", "Trap", "Bossa nova" };
static const char *SPREAD_NAMES[SPREAD_COUNT] = { "Stretta", "Aperta", "Ampia" };
static const char *KNOB_MODE_NAMES[3] = { "Assolute", "Relative", "Relative (64)" };
static const char *ARP_RATES[5] = { "1/4", "1/8", "1/8 terzina", "1/16", "1/32" };
static const char *ARP_SHORT[5] = { "1/4", "1/8", "1/8T", "1/16", "1/32" };
static const int LOOP_BARS[4] = { 1, 2, 4, 8 };
static const float QUANT_BEATS[4] = { 0.0f, 0.25f, 0.5f, 1.0f };
static const char *QUANT_NAMES[4] = { "No", "1/16", "1/8", "1/4" };

/* ------------------------------------------------------------------ MIDI */
enum { MAP_NONE, MAP_NOTE, MAP_CC };
typedef struct { int kind, port, ch, num; } MidiMap;          /* port e ch: -1 = qualsiasi */

enum { T_CHORD = 0, T_FUNC = 8, T_KNOB = 16, T_TRANS = 24, T_COUNT = 29 };
enum { F_BEAT, F_LOOP_REC, F_LOOP_PLAY, F_LOOP_CLEAR, F_KEYMODE, F_PERFORM, F_SOUND_PREV, F_SOUND_NEXT };
enum { K_VOICING, K_BASS, K_AMOUNT, K_TONE, K_CHORUS, K_DELAY, K_REVERB, K_VOLUME };
enum { X_PLAY, X_RECORD, X_OVERDUB, X_LOOP, X_UNDO };

static const char *TARGET_NAMES[T_COUNT] = {
    "Dim", "Min", "Maj", "Sus", "6", "m7", "M7", "9",
    "Batteria", "Loop: registra", "Loop: suona o ferma", "Loop: cancella",
    "Modo tonalità", "Esecuzione successiva", "Suono precedente", "Suono successivo",
    "K1 Voicing", "K2 Basso", "K3 Esecuzione", "K4 Tono", "K5 Chorus", "K6 Delay", "K7 Riverbero", "K8 Volume",
    "Play/Stop", "Record", "Overdub", "Loop", "Undo",
};
static const char *CHORD_PAD_NAMES[8] = { "Dim", "Min", "Maj", "Sus", "6", "m7", "M7", "9" };

enum { ROW_HEADER, ROW_TARGET, ROW_KNOB_MODE };
typedef struct { int kind, first, last; const char *label; } MidiRow;
#define MIDI_ROW_COUNT 34
static MidiRow MIDI_ROWS[MIDI_ROW_COUNT];

static void midi_rows_init(void)
{
    static const struct { int first, last; const char *label; } groups[4] = {
        { T_CHORD, T_CHORD + 7, "Pad degli accordi (banco A)" },
        { T_FUNC, T_FUNC + 7, "Pad delle funzioni (banco B)" },
        { T_KNOB, T_KNOB + 7, "Manopole" },
        { T_TRANS, T_TRANS + 4, "Tasti di trasporto" },
    };
    int r = 0;
    for (int g = 0; g < 4; g++) {
        MIDI_ROWS[r++] = (MidiRow){ ROW_HEADER, groups[g].first, groups[g].last, groups[g].label };
        for (int t = groups[g].first; t <= groups[g].last; t++) MIDI_ROWS[r++] = (MidiRow){ ROW_TARGET, t, t, TARGET_NAMES[t] };
        if (g == 2) MIDI_ROWS[r++] = (MidiRow){ ROW_KNOB_MODE, 0, 0, "Tipo di manopole" };
    }
}

/* ------------------------------------------------------------------ pulsanti della Flip */
/* Croce: sinistra I, su IV, destra V, giu' vi; Y ii, X iii, B vii, A l'accordo maggiore fuori scala piu'
   usato (VII bemolle in maggiore, V in minore). L'ultimo grado serve all'ascolto della pagina Suono. */
#define FLIP_SLOTS 8
#define AUDITION_SLOT 8
static const int FLIP_PADS[FLIP_SLOTS] = { PAD_LEFT, PAD_UP, PAD_RIGHT, PAD_DOWN, PAD_Y, PAD_X, PAD_B, PAD_A };
static const int FLIP_DEGREE[FLIP_SLOTS + 1] = { 0, 3, 4, 5, 1, 2, 6, -1, 0 };
#define SRC_FLIP(i) (128 + (i))
enum { MOD_SWAP, MOD_SUS, MOD_SEVENTH, MOD_NINTH, MOD_COUNT };     /* L1, R1, L2, R2 */

/* ------------------------------------------------------------------ elenchi delle pagine */
enum { P_SOUND, P_KEY, P_SCALE, P_KEYMODE, P_VOICING, P_SPREAD, P_PERFORM, P_AMOUNT,
       P_ENGINE, P_TONE, P_CHAR, P_ATTACK, P_DECAY, P_SUSTAIN, P_RELEASE, P_VIBRATO,
       P_BASS, P_BASS_TONE, P_CHORUS, P_DELAY, P_REVERB, P_VOLUME, P_COUNT };
static const char *PARAM_NAMES[P_COUNT] = {
    "Suono", "Tonalità", "Scala", "Modo tonalità", "Voicing", "Apertura", "Esecuzione", "Quantità",
    "Motore", "Tono", "Carattere", "Attacco", "Decadimento", "Sostegno", "Rilascio", "Vibrato",
    "Livello", "Tono", "Chorus", "Delay", "Riverbero", "Volume",
};
#define HDR(i) (-1 - (i))
static const char *SOUND_HEADERS[4] = { "Accordi", "Timbro", "Basso", "Effetti" };
static const int SOUND_ROWS[] = {
    P_SOUND,
    HDR(0), P_KEY, P_SCALE, P_KEYMODE, P_VOICING, P_SPREAD, P_PERFORM, P_AMOUNT,
    HDR(1), P_ENGINE, P_TONE, P_CHAR, P_ATTACK, P_DECAY, P_SUSTAIN, P_RELEASE, P_VIBRATO,
    HDR(2), P_BASS, P_BASS_TONE,
    HDR(3), P_CHORUS, P_DELAY, P_REVERB, P_VOLUME,
};
#define SOUND_ROW_COUNT ((int)(sizeof(SOUND_ROWS) / sizeof(SOUND_ROWS[0])))

enum { R_BEAT, R_STYLE, R_LEVEL, R_TEMPO, R_CLOCK, R_BARS, R_QUANT, R_COUNT };
static const char *RHYTHM_NAMES[R_COUNT] = {
    "Batteria", "Stile", "Livello", "Tempo", "Segui il clock MIDI", "Lunghezza massima del loop", "Quantizzazione del loop",
};

/* ------------------------------------------------------------------ stato */
typedef struct {
    OrcSound snd;
    int sound, key, minor, keymode, spread;
    float voicing;
    float bpm;
    int beat_on, beat_style;
    float beat_level;
    int clock_follow, loop_bars, loop_quant, knob_mode;
    MidiMap map[T_COUNT];
} Settings;

typedef struct { int src; float vel; } Held;      /* src: tasto 0..127 o SRC_FLIP(pulsante) */

struct OrcApp {
    Orc *orc;
    float sr;
    char path[512];
    Settings set;
    int page, quit_dialog, quit;
    int down[PAD_COUNT];
    float rep[PAD_COUNT];
    float time;

    /* esecuzione */
    int pad_held[8];
    unsigned pad_when[8], pad_clock;
    int mods[MOD_COUNT];
    Held stack[24];
    int stack_n;
    unsigned char key_kind[128];                  /* 0 libero, 1 accordo, 2 melodia */
    unsigned char melody_down[128], melody_pedal[128];
    int sustain, chord_pedal;
    int chord_live;
    int notes[ORC_MAX_CHORD], notes_n, bass, root, degree, spelling;
    unsigned mask;
    char name_root[8], name_suffix[16], symbol[24], numeral[24];
    float glow;
    float midi_bend, midi_mod, stick_bend, stick_mod, bright;

    /* MIDI */
    char midi_name[64];
    int learn, learn_first, learn_last;
    int midi_sel, midi_scroll;
    float midi_flash;
    char last_msg[96];
    int clock_ticks;
    float clock_start, clock_seen, clock_bpm;

    /* pagine a elenco */
    int sound_sel, sound_scroll, rhythm_sel, rhythm_scroll;
    char toast[112];
    float toast_t;
    char flash[48];
    float flash_t;
    int dirty;
    float save_t;
    float taps[4];
    int tap_n;
    float last_tap;
    uint32_t drawn_key;
    float drawn_time;
};

static void toast(OrcApp *a, const char *msg)
{
    snprintf(a->toast, sizeof(a->toast), "%s", msg);
    a->toast_t = 2.2f;
}

static void flash(OrcApp *a, const char *label, const char *value)
{
    snprintf(a->flash, sizeof(a->flash), "%s  %s", label, value);
    a->flash_t = 1.2f;
}

static void percent(char *out, size_t n, float v) { snprintf(out, n, "%d%%", (int)lrintf(clamp01(v) * 100.0f)); }

static void seconds(char *out, size_t n, float s)
{
    if (s < 0.1f) snprintf(out, n, "%d ms", (int)lrintf(s * 1000.0f));
    else {
        snprintf(out, n, "%.2f s", s);
        for (char *p = out; *p; p++) if (*p == '.') *p = ',';
    }
}

static void note_label(char *out, size_t n, int note)
{
    snprintf(out, n, "%s%d", note_it(note % 12, SPELL_MIX), note / 12 - 1);
}

/* ------------------------------------------------------------------ motore */
static void sound_changed(OrcApp *a)
{
    orc_set_sound(a->orc, &a->set.snd);
    a->dirty = 1;
}

static void beat_changed(OrcApp *a)
{
    orc_set_beat(a->orc, a->set.beat_on, a->set.beat_style, a->set.beat_level);
    a->dirty = 1;
}

static void tempo_changed(OrcApp *a)
{
    orc_set_tempo(a->orc, a->set.bpm);
    a->dirty = 1;
}

static void loop_config_changed(OrcApp *a)
{
    orc_loop_config(a->orc, LOOP_BARS[a->set.loop_bars], QUANT_BEATS[a->set.loop_quant]);
    a->dirty = 1;
}

static void expression(OrcApp *a)
{
    orc_set_expression(a->orc, a->midi_bend + a->stick_bend, fmaxf(a->midi_mod, a->stick_mod), a->bright);
}

/* ------------------------------------------------------------------ accordi */
static int quality_held(const OrcApp *a)
{
    static const int QUALITY[4] = { Q_DIM, Q_MIN, Q_MAJ, Q_SUS4 };
    if (a->pad_held[0] && a->pad_held[2]) return Q_AUG;          /* Dim + Maj */
    if (a->pad_held[1] && a->pad_held[3]) return Q_SUS2;         /* Min + Sus */
    int q = -1;
    unsigned best = 0;
    for (int i = 0; i < 4; i++)                                  /* vince l'ultimo premuto */
        if (a->pad_held[i] && a->pad_when[i] >= best) { best = a->pad_when[i]; q = QUALITY[i]; }
    return q;
}

static int ext_held(const OrcApp *a)
{
    int e = 0;
    if (a->pad_held[4]) e |= EXT_6;
    if (a->pad_held[5] || a->mods[MOD_SEVENTH]) e |= EXT_7;
    if (a->pad_held[6]) e |= EXT_MAJ7;
    if (a->pad_held[7] || a->mods[MOD_NINTH]) e |= EXT_9;
    return e;
}

static int any_pad_held(const OrcApp *a)
{
    for (int i = 0; i < 8; i++) if (a->pad_held[i]) return 1;
    return 0;
}

/* Accordo per una sorgente (tasto o pulsante della Flip) con i pad e i dorsali tenuti adesso. */
static int build_chord(const OrcApp *a, int src, int *root, int *iv, int *degree, int *spelling)
{
    const Settings *s = &a->set;
    int q = quality_held(a), ext = ext_held(a), n;
    *degree = -1;
    *spelling = SPELL_MIX;
    if (src >= 128) {
        int d = FLIP_DEGREE[src - 128];
        *spelling = key_spelling(s->key, s->minor);
        if (d >= 0 && q < 0) {
            n = chord_diatonic(s->key, s->minor, d, ext, root, iv);
            *degree = d;
        } else {
            *root = d >= 0 ? scale_pc(s->key, s->minor, d) : (s->key + (s->minor ? 7 : 10)) % 12;
            n = chord_intervals(q >= 0 ? q : Q_MAJ, ext, iv);
        }
    } else if (s->keymode && q < 0) {
        int d = scale_degree(s->key, s->minor, src % 12);
        n = chord_diatonic(s->key, s->minor, d, ext, root, iv);
        *degree = d;
        *spelling = key_spelling(s->key, s->minor);
    } else {
        *root = src % 12;
        n = chord_intervals(q >= 0 ? q : Q_MAJ, ext, iv);
        if (s->keymode) *spelling = key_spelling(s->key, s->minor);
    }
    if (a->mods[MOD_SWAP]) n = chord_swap_third(iv, n);
    if (a->mods[MOD_SUS]) n = chord_sus4(iv, n);
    return n;
}

static int chord_tag(int root, unsigned mask, int spelling) { return (int)((unsigned)root | mask << 4 | (unsigned)spelling << 28); }

/* Manda al motore l'accordo della sorgente in cima alla pila. Con retrigger = 0 non riparte se non
   cambia nulla (pad premuti che non cambiano l'accordo, manopola del voicing ferma su un rivolto). */
static void send_chord(OrcApp *a, int retrigger)
{
    if (!a->stack_n) return;
    const Held *h = &a->stack[a->stack_n - 1];
    int root, iv[CHORD_MAX_IV], degree, spelling;
    int n = build_chord(a, h->src, &root, iv, &degree, &spelling);
    int notes[ORC_MAX_CHORD];
    unsigned mask = chord_mask(iv, n);
    /* tra accordi diversi le voci si legano al precedente; lo stesso accordo segue solo il registro (K1) */
    int other = a->notes_n && (root != a->root || mask != a->mask);
    int nn = chord_voicing(root, iv, n, a->set.voicing, a->set.spread, other ? a->notes : NULL, other ? a->notes_n : 0,
                           notes, ORC_MAX_CHORD);
    int bass = chord_bass(root);
    if (!retrigger && a->chord_live && nn == a->notes_n && bass == a->bass && !memcmp(notes, a->notes, sizeof(int) * (size_t)nn))
        return;
    orc_chord_on(a->orc, notes, nn, bass, h->vel, chord_tag(root, mask, spelling));
    memcpy(a->notes, notes, sizeof(int) * (size_t)nn);
    a->notes_n = nn;
    a->bass = bass;
    a->root = root;
    a->mask = mask;
    a->degree = degree;
    a->spelling = spelling;
    snprintf(a->name_root, sizeof(a->name_root), "%s", note_it(root, spelling));
    snprintf(a->name_suffix, sizeof(a->name_suffix), "%s", chord_suffix(mask));
    chord_symbol(a->symbol, sizeof(a->symbol), root, mask, spelling);
    if (degree >= 0) chord_numeral(a->numeral, sizeof(a->numeral), degree, mask);
    else a->numeral[0] = 0;
    a->chord_live = 1;
    a->chord_pedal = 0;
    a->glow = 1.0f;
}

static void chord_release(OrcApp *a)
{
    if (!a->chord_live) return;
    if (a->sustain) { a->chord_pedal = 1; return; }
    orc_chord_off(a->orc);
    a->chord_live = 0;
}

static int stack_find(const OrcApp *a, int src)
{
    for (int i = 0; i < a->stack_n; i++) if (a->stack[i].src == src) return i;
    return -1;
}

static void chord_press(OrcApp *a, int src, float vel)
{
    int i = stack_find(a, src);
    if (i >= 0) { memmove(&a->stack[i], &a->stack[i + 1], sizeof(Held) * (size_t)(a->stack_n - i - 1)); a->stack_n--; }
    if (a->stack_n == (int)(sizeof(a->stack) / sizeof(a->stack[0]))) {
        memmove(&a->stack[0], &a->stack[1], sizeof(Held) * (size_t)(a->stack_n - 1));
        a->stack_n--;
    }
    a->stack[a->stack_n++] = (Held){ src, vel };
    send_chord(a, 1);
}

/* Si torna al tasto tenuto prima (l'ultimo vince), oppure l'accordo si spegne. */
static void chord_unpress(OrcApp *a, int src)
{
    int i = stack_find(a, src);
    if (i < 0) return;
    int top = i == a->stack_n - 1;
    memmove(&a->stack[i], &a->stack[i + 1], sizeof(Held) * (size_t)(a->stack_n - i - 1));
    a->stack_n--;
    if (!a->stack_n) chord_release(a);
    else if (top) send_chord(a, 0);
}

static void modifiers_changed(OrcApp *a)
{
    if (a->stack_n) send_chord(a, 0);
}

static void key_down(OrcApp *a, int note, float vel)
{
    if (a->key_kind[note]) return;
    if (a->set.keymode || any_pad_held(a)) {
        a->key_kind[note] = 1;
        chord_press(a, note, vel);
    } else {
        a->key_kind[note] = 2;
        a->melody_down[note] = 1;
        a->melody_pedal[note] = 0;
        orc_note_on(a->orc, note, vel);
    }
}

static void key_up(OrcApp *a, int note)
{
    if (a->key_kind[note] == 1) chord_unpress(a, note);
    else if (a->key_kind[note] == 2) {
        a->melody_down[note] = 0;
        if (a->sustain) a->melody_pedal[note] = 1;
        else orc_note_off(a->orc, note);
    }
    a->key_kind[note] = 0;
}

static void pedal(OrcApp *a, int down)
{
    if (down == a->sustain) return;
    a->sustain = down;
    if (down) return;
    if (a->chord_pedal && !a->stack_n) { orc_chord_off(a->orc); a->chord_live = 0; }
    a->chord_pedal = 0;
    for (int n = 0; n < 128; n++)
        if (a->melody_pedal[n]) { a->melody_pedal[n] = 0; if (!a->melody_down[n]) orc_note_off(a->orc, n); }
}

static void panic(OrcApp *a)
{
    orc_all_off(a->orc);
    a->stack_n = 0;
    a->chord_live = 0;
    a->chord_pedal = 0;
    memset(a->key_kind, 0, sizeof(a->key_kind));
    memset(a->melody_down, 0, sizeof(a->melody_down));
    memset(a->melody_pedal, 0, sizeof(a->melody_pedal));
}

static void chord_pad(OrcApp *a, int i, int pressed)
{
    if (a->pad_held[i] == pressed) return;
    a->pad_held[i] = pressed;
    if (pressed) a->pad_when[i] = ++a->pad_clock;
    modifiers_changed(a);
}

/* ------------------------------------------------------------------ funzioni */
static void load_preset(OrcApp *a, int i)
{
    i = ((i % PRESET_COUNT) + PRESET_COUNT) % PRESET_COUNT;
    float vol = a->set.snd.volume;                /* il volume resta quello scelto */
    a->set.sound = i;
    a->set.snd = PRESETS[i].s;
    a->set.snd.volume = vol;
    a->set.spread = PRESETS[i].spread;
    sound_changed(a);
    modifiers_changed(a);
    toast(a, PRESETS[i].name);
}

static void transport_set(OrcApp *a, int on)
{
    orc_transport(a->orc, on);
}

static void transport_toggle(OrcApp *a)
{
    int on = !orc_transport_on(a->orc);
    transport_set(a, on);
    if (on) toast(a, a->set.beat_on ? "Batteria e loop in moto" : "Tempo in moto (batteria spenta)");
    else toast(a, "Fermo");
}

static void beat_toggle(OrcApp *a)
{
    a->set.beat_on = !a->set.beat_on;
    beat_changed(a);
    char t[64];
    if (a->set.beat_on) {
        if (!orc_transport_on(a->orc)) transport_set(a, 1);
        snprintf(t, sizeof(t), "Batteria: %s", BEAT_NAMES[a->set.beat_style]);
    } else {
        OrcLoopInfo li;
        orc_loop_info(a->orc, &li);
        if (li.state == LOOP_EMPTY || li.state == LOOP_STOPPED) transport_set(a, 0);
        snprintf(t, sizeof(t), "Batteria spenta");
    }
    toast(a, t);
}

static void loop_record(OrcApp *a)
{
    OrcLoopInfo li;
    orc_loop_info(a->orc, &li);
    orc_loop_record(a->orc);
    switch (li.state) {
    case LOOP_EMPTY: toast(a, orc_transport_on(a->orc) ? "Loop: registra dalla prossima battuta" : "Loop: registra"); break;
    case LOOP_ARMED: toast(a, "Loop annullato"); break;
    case LOOP_RECORDING: toast(a, "Loop chiuso: suona"); break;
    case LOOP_PLAYING: case LOOP_STOPPED: toast(a, "Loop: sovraincisione"); break;
    case LOOP_OVERDUB: toast(a, "Loop: suona"); break;
    }
}

static void loop_play(OrcApp *a)
{
    OrcLoopInfo li;
    orc_loop_info(a->orc, &li);
    orc_loop_play(a->orc);
    switch (li.state) {
    case LOOP_EMPTY: toast(a, "Il loop è vuoto: prima registra"); break;
    case LOOP_PLAYING: case LOOP_OVERDUB: toast(a, "Loop fermo"); break;
    default: toast(a, "Loop: suona"); break;
    }
}

static void loop_clear(OrcApp *a)
{
    orc_loop_clear(a->orc);
    toast(a, "Loop cancellato");
}

static void keymode_toggle(OrcApp *a)
{
    a->set.keymode = !a->set.keymode;
    a->dirty = 1;
    char t[64], k[32];
    key_name(k, sizeof(k), a->set.key, a->set.minor);
    if (a->set.keymode) snprintf(t, sizeof(t), "Modo tonalità: %s", k);
    else snprintf(t, sizeof(t), "Modo tonalità spento");
    toast(a, t);
}

static void perform_next(OrcApp *a)
{
    a->set.snd.perform = (a->set.snd.perform + 1) % PERF_COUNT;
    sound_changed(a);
    char t[64];
    snprintf(t, sizeof(t), "Esecuzione: %s", PERFORM_NAMES[a->set.snd.perform]);
    toast(a, t);
}

static void function(OrcApp *a, int f)
{
    switch (f) {
    case F_BEAT: beat_toggle(a); break;
    case F_LOOP_REC: loop_record(a); break;
    case F_LOOP_PLAY: loop_play(a); break;
    case F_LOOP_CLEAR: loop_clear(a); break;
    case F_KEYMODE: keymode_toggle(a); break;
    case F_PERFORM: perform_next(a); break;
    case F_SOUND_PREV: load_preset(a, a->set.sound - 1); break;
    case F_SOUND_NEXT: load_preset(a, a->set.sound + 1); break;
    }
}

static float *knob_ref(OrcApp *a, int k)
{
    switch (k) {
    case K_VOICING: return &a->set.voicing;
    case K_BASS: return &a->set.snd.bass_level;
    case K_AMOUNT: return &a->set.snd.perform_amount;
    case K_TONE: return &a->set.snd.tone;
    case K_CHORUS: return &a->set.snd.chorus;
    case K_DELAY: return &a->set.snd.delay;
    case K_REVERB: return &a->set.snd.reverb;
    default: return &a->set.snd.volume;
    }
}

static void amount_text(const OrcApp *a, char *out, size_t n);

static void knob(OrcApp *a, int k, int v)
{
    float *ref = knob_ref(a, k), x;
    if (a->set.knob_mode == 0) x = v / 127.0f;
    else {
        int d = a->set.knob_mode == 1 ? (v < 64 ? v : v - 128) : v - 64;
        x = clamp01(*ref + d / 100.0f);
    }
    *ref = x;
    char t[32];
    if (k == K_VOICING) { a->dirty = 1; modifiers_changed(a); percent(t, sizeof(t), x); }
    else {
        sound_changed(a);
        if (k == K_AMOUNT) amount_text(a, t, sizeof(t));
        else percent(t, sizeof(t), x);
    }
    flash(a, TARGET_NAMES[T_KNOB + k] + 3, t);
}

static void control(OrcApp *a, int t, int pressed)
{
    if (t < T_FUNC) { chord_pad(a, t - T_CHORD, pressed); return; }
    if (!pressed) return;
    if (t < T_KNOB) { function(a, t - T_FUNC); return; }
    switch (t - T_TRANS) {
    case X_PLAY: transport_toggle(a); break;
    case X_RECORD: case X_OVERDUB: loop_record(a); break;
    case X_LOOP: loop_play(a); break;
    case X_UNDO: loop_clear(a); break;
    }
}

/* ------------------------------------------------------------------ MIDI */
static void default_map(MidiMap *m)
{
    static const int chord_notes[8] = { 40, 41, 42, 43, 36, 37, 38, 39 };
    for (int i = 0; i < T_COUNT; i++) m[i] = (MidiMap){ MAP_NONE, -1, -1, 0 };
    for (int i = 0; i < 8; i++) {
        m[T_CHORD + i] = (MidiMap){ MAP_NOTE, 0, 9, chord_notes[i] };
        m[T_FUNC + i] = (MidiMap){ MAP_NOTE, 0, 9, 44 + i };
        m[T_KNOB + i] = (MidiMap){ MAP_CC, 0, -1, 70 + i };
    }
}

static void map_text(const MidiMap *m, char *out, size_t n)
{
    char ch[24] = "", port[24] = "";
    if (m->ch >= 0) snprintf(ch, sizeof(ch), " · can. %d", m->ch + 1);
    if (m->port > 0) snprintf(port, sizeof(port), " · porta %d", m->port + 1);
    if (m->kind == MAP_NOTE) snprintf(out, n, "Nota %d%s%s", m->num, ch, port);
    else if (m->kind == MAP_CC) snprintf(out, n, "CC %d%s%s", m->num, ch, port);
    else snprintf(out, n, "-");
}

static void describe(OrcApp *a, int port, int type, int ch, int d1, int d2)
{
    char p[24] = "", nn[16];
    if (port > 0) snprintf(p, sizeof(p), " · porta %d", port + 1);
    switch (type) {
    case MIDI_NOTE_ON:
        note_label(nn, sizeof(nn), d1);
        if (d2 > 0) snprintf(a->last_msg, sizeof(a->last_msg), "Nota %d (%s) · dinamica %d · canale %d%s", d1, nn, d2, ch + 1, p);
        else snprintf(a->last_msg, sizeof(a->last_msg), "Nota %d (%s) rilasciata · canale %d%s", d1, nn, ch + 1, p);
        break;
    case MIDI_NOTE_OFF:
        note_label(nn, sizeof(nn), d1);
        snprintf(a->last_msg, sizeof(a->last_msg), "Nota %d (%s) rilasciata · canale %d%s", d1, nn, ch + 1, p);
        break;
    case MIDI_CC:
        snprintf(a->last_msg, sizeof(a->last_msg), "CC %d = %d · canale %d%s", d1, d2, ch + 1, p);
        break;
    case MIDI_PITCH_BEND:
        snprintf(a->last_msg, sizeof(a->last_msg), "Pitch bend %+d · canale %d%s", ((d2 << 7) | d1) - 8192, ch + 1, p);
        break;
    case MIDI_PROGRAM:
        snprintf(a->last_msg, sizeof(a->last_msg), "Program change %d · canale %d%s", d1, ch + 1, p);
        break;
    case MIDI_CHANNEL_AT:
    case MIDI_POLY_AT:
        snprintf(a->last_msg, sizeof(a->last_msg), "Aftertouch · canale %d%s", ch + 1, p);
        break;
    }
}

static int map_same(const MidiMap *x, const MidiMap *y)
{
    return x->kind == y->kind && x->num == y->num && x->kind != MAP_NONE &&
           (x->port < 0 || y->port < 0 || x->port == y->port) && (x->ch < 0 || y->ch < 0 || x->ch == y->ch);
}

static void learn_message(OrcApp *a, int port, int type, int ch, int d1, int d2)
{
    int t = a->learn, knob_target = t >= T_KNOB && t < T_TRANS;
    if (knob_target && type != MIDI_CC) return;              /* le manopole vogliono un CC */
    if (!knob_target && type == MIDI_CC && d2 < 64) return;  /* pad in modo CC: conta la pressione */
    MidiMap m = { type == MIDI_CC ? MAP_CC : MAP_NOTE, port, knob_target ? -1 : ch, d1 };
    for (int i = a->learn_first; i < t; i++)                 /* in sequenza: lo stesso controllo vale una volta */
        if (map_same(&a->set.map[i], &m)) return;
    for (int i = 0; i < T_COUNT; i++)
        if (i != t && map_same(&a->set.map[i], &m)) a->set.map[i].kind = MAP_NONE;     /* niente doppioni */
    a->set.map[t] = m;
    a->dirty = 1;
    char v[48], msg[96];
    map_text(&m, v, sizeof(v));
    if (t < a->learn_last) {
        a->learn = t + 1;
        snprintf(msg, sizeof(msg), "%s: %s. Ora %s", TARGET_NAMES[t], v, TARGET_NAMES[t + 1]);
    } else {
        a->learn = -1;
        snprintf(msg, sizeof(msg), "%s: %s", TARGET_NAMES[t], v);
    }
    toast(a, msg);
}

static void realtime(OrcApp *a, int st)
{
    switch (st) {
    case 0xF8:                                    /* clock: 24 per semiminima */
        if (a->clock_seen > 0.5f || !a->clock_ticks) { a->clock_ticks = 0; a->clock_start = a->time; }
        a->clock_seen = 0.0f;
        if (++a->clock_ticks >= 97) {
            float span = a->time - a->clock_start;
            if (span > 0.2f) {
                float bpm = 60.0f * (float)(a->clock_ticks - 1) / 24.0f / span;
                a->clock_bpm = a->clock_bpm > 0.0f ? 0.6f * a->clock_bpm + 0.4f * bpm : bpm;
                if (a->set.clock_follow && fabsf(roundf(a->clock_bpm) - a->set.bpm) >= 1.0f) {
                    a->set.bpm = clampf(roundf(a->clock_bpm), 30.0f, 300.0f);
                    tempo_changed(a);
                }
            }
            a->clock_ticks = 1;
            a->clock_start = a->time;
        }
        break;
    case 0xFA:
    case 0xFB:
        transport_set(a, 1);
        break;
    case 0xFC:
        transport_set(a, 0);
        break;
    }
}

void orcapp_midi(OrcApp *a, int port, const unsigned char *m, int len)
{
    if (len < 1) return;
    int st = m[0];
    if (st >= 0xF8) { realtime(a, st); return; }
    if (st >= 0xF0) return;
    a->midi_flash = 1.0f;
    int type = st & 0xF0, ch = st & 0x0F, d1 = len > 1 ? m[1] : 0, d2 = len > 2 ? m[2] : 0;
    describe(a, port, type, ch, d1, d2);
    int on = type == MIDI_NOTE_ON && d2 > 0, off = type == MIDI_NOTE_OFF || (type == MIDI_NOTE_ON && d2 == 0);
    if (a->learn >= 0 && (on || type == MIDI_CC)) { learn_message(a, port, type, ch, d1, d2); return; }
    if (on || off || type == MIDI_CC) {
        int kind = type == MIDI_CC ? MAP_CC : MAP_NOTE;
        for (int t = 0; t < T_COUNT; t++) {
            const MidiMap *mp = &a->set.map[t];
            if (mp->kind != kind || mp->num != d1 || (mp->port >= 0 && mp->port != port) || (mp->ch >= 0 && mp->ch != ch)) continue;
            if (t >= T_KNOB && t < T_TRANS) { if (kind == MAP_CC) knob(a, t - T_KNOB, d2); }
            else control(a, t, kind == MAP_CC ? d2 >= 64 : on);
            return;
        }
    }
    switch (type) {
    case MIDI_NOTE_ON:
        if (d2 > 0) key_down(a, d1, 0.15f + 0.85f * d2 / 127.0f);
        else key_up(a, d1);
        break;
    case MIDI_NOTE_OFF:
        key_up(a, d1);
        break;
    case MIDI_CC:
        if (d1 == 1) { a->midi_mod = d2 / 127.0f; expression(a); }
        else if (d1 == 64) pedal(a, d2 >= 64);
        else if (d1 == 120 || d1 == 123) panic(a);
        break;
    case MIDI_PITCH_BEND:
        a->midi_bend = (((d2 << 7) | d1) - 8192) / 8192.0f * 2.0f;
        expression(a);
        break;
    case MIDI_PROGRAM:
        load_preset(a, d1 % PRESET_COUNT);
        break;
    }
}

void orcapp_midi_status(OrcApp *a, const char *device)
{
    int was = a->midi_name[0] != 0;
    snprintf(a->midi_name, sizeof(a->midi_name), "%s", device ? device : "");
    if (device) {
        char t[112];
        snprintf(t, sizeof(t), "Tastiera collegata: %s", device);
        toast(a, t);
    } else if (was) {
        panic(a);
        memset(a->pad_held, 0, sizeof(a->pad_held));
        toast(a, "Tastiera MIDI scollegata");
    }
}

/* ------------------------------------------------------------------ salvataggio */
static void settings_default(Settings *s)
{
    memset(s, 0, sizeof(*s));
    s->sound = 0;
    s->snd = PRESETS[0].s;
    s->spread = PRESETS[0].spread;
    s->key = 0;
    s->voicing = 0.5f;
    s->bpm = 96.0f;
    s->beat_style = BEAT_DISCO;
    s->beat_level = 0.7f;
    s->clock_follow = 1;
    s->loop_bars = 2;
    s->loop_quant = 2;
    default_map(s->map);
}

static void save(OrcApp *a)
{
    if (!a->path[0]) return;
    char tmp[600];
    snprintf(tmp, sizeof(tmp), "%s.tmp", a->path);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    const Settings *s = &a->set;
    const OrcSound *d = &s->snd;
    fprintf(f, "# OpenOrc\nsound=%d\nengine=%d\ntone=%g\ncharacter=%g\nattack=%g\ndecay=%g\nsustain=%g\nrelease=%g\n"
               "vibrato=%g\nbass=%g\nbass_tone=%g\nchorus=%g\ndelay=%g\nreverb=%g\nperform=%d\namount=%g\nvolume=%g\n",
            s->sound, d->engine, d->tone, d->character, d->attack, d->decay, d->sustain, d->release, d->vibrato,
            d->bass_level, d->bass_tone, d->chorus, d->delay, d->reverb, d->perform, d->perform_amount, d->volume);
    fprintf(f, "key=%d\nminor=%d\nkeymode=%d\nvoicing=%g\nspread=%d\nbpm=%g\nbeat=%d\nbeat_style=%d\nbeat_level=%g\n"
               "clock=%d\nloop_bars=%d\nquant=%d\nknob_mode=%d\n",
            s->key, s->minor, s->keymode, s->voicing, s->spread, s->bpm, s->beat_on, s->beat_style, s->beat_level,
            s->clock_follow, s->loop_bars, s->loop_quant, s->knob_mode);
    for (int t = 0; t < T_COUNT; t++)
        fprintf(f, "map%d=%d,%d,%d,%d\n", t, s->map[t].kind, s->map[t].port, s->map[t].ch, s->map[t].num);
    fclose(f);
#ifdef _WIN32
    remove(a->path);
#endif
    rename(tmp, a->path);
    a->dirty = 0;
}

static void load(OrcApp *a)
{
    FILE *f = fopen(a->path, "r");
    if (!f) return;
    Settings *s = &a->set;
    OrcSound *d = &s->snd;
    char line[160];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq || line[0] == '#') continue;
        *eq = 0;
        const char *k = line, *val = eq + 1;
        float v = (float)atof(val);
        int t, kind, port, ch, num;
        if (sscanf(k, "map%d", &t) == 1 && t >= 0 && t < T_COUNT) {
            if (sscanf(val, "%d,%d,%d,%d", &kind, &port, &ch, &num) == 4 && kind >= MAP_NONE && kind <= MAP_CC)
                s->map[t] = (MidiMap){ kind, port < -1 || port > 7 ? -1 : port, ch < -1 || ch > 15 ? -1 : ch, num & 127 };
        }
        else if (!strcmp(k, "sound")) s->sound = (int)clampf(v, 0, PRESET_COUNT - 1);
        else if (!strcmp(k, "engine")) d->engine = (int)clampf(v, 0, ENG_COUNT - 1);
        else if (!strcmp(k, "tone")) d->tone = clamp01(v);
        else if (!strcmp(k, "character")) d->character = clamp01(v);
        else if (!strcmp(k, "attack")) d->attack = clampf(v, 0.002f, 8.0f);
        else if (!strcmp(k, "decay")) d->decay = clampf(v, 0.01f, 8.0f);
        else if (!strcmp(k, "sustain")) d->sustain = clamp01(v);
        else if (!strcmp(k, "release")) d->release = clampf(v, 0.01f, 8.0f);
        else if (!strcmp(k, "vibrato")) d->vibrato = clamp01(v);
        else if (!strcmp(k, "bass")) d->bass_level = clamp01(v);
        else if (!strcmp(k, "bass_tone")) d->bass_tone = clamp01(v);
        else if (!strcmp(k, "chorus")) d->chorus = clamp01(v);
        else if (!strcmp(k, "delay")) d->delay = clamp01(v);
        else if (!strcmp(k, "reverb")) d->reverb = clamp01(v);
        else if (!strcmp(k, "perform")) d->perform = (int)clampf(v, 0, PERF_COUNT - 1);
        else if (!strcmp(k, "amount")) d->perform_amount = clamp01(v);
        else if (!strcmp(k, "volume")) d->volume = clamp01(v);
        else if (!strcmp(k, "key")) s->key = (int)clampf(v, 0, 11);
        else if (!strcmp(k, "minor")) s->minor = v != 0;
        else if (!strcmp(k, "keymode")) s->keymode = v != 0;
        else if (!strcmp(k, "voicing")) s->voicing = clamp01(v);
        else if (!strcmp(k, "spread")) s->spread = (int)clampf(v, 0, SPREAD_COUNT - 1);
        else if (!strcmp(k, "bpm")) s->bpm = clampf(v, 30, 300);
        else if (!strcmp(k, "beat")) s->beat_on = v != 0;
        else if (!strcmp(k, "beat_style")) s->beat_style = (int)clampf(v, 0, BEAT_COUNT - 1);
        else if (!strcmp(k, "beat_level")) s->beat_level = clamp01(v);
        else if (!strcmp(k, "clock")) s->clock_follow = v != 0;
        else if (!strcmp(k, "loop_bars")) s->loop_bars = (int)clampf(v, 0, 3);
        else if (!strcmp(k, "quant")) s->loop_quant = (int)clampf(v, 0, 3);
        else if (!strcmp(k, "knob_mode")) s->knob_mode = (int)clampf(v, 0, 2);
    }
    fclose(f);
}

/* ------------------------------------------------------------------ API */
OrcApp *orcapp_create(float sample_rate, const char *state_path)
{
    OrcApp *a = calloc(1, sizeof(OrcApp));
    if (!a) return NULL;
    a->orc = orc_create(sample_rate);
    if (!a->orc) { free(a); return NULL; }
    if (!MIDI_ROWS[0].label) midi_rows_init();
    a->sr = sample_rate;
    if (state_path) snprintf(a->path, sizeof(a->path), "%s", state_path);
    settings_default(&a->set);
    load(a);
    a->set.beat_on = 0;                           /* si parte sempre in silenzio */
    a->learn = -1;
    a->degree = -1;
    a->sound_sel = 0;
    a->midi_sel = 1;
    orc_set_sound(a->orc, &a->set.snd);
    orc_set_tempo(a->orc, a->set.bpm);
    orc_set_beat(a->orc, 0, a->set.beat_style, a->set.beat_level);
    orc_loop_config(a->orc, LOOP_BARS[a->set.loop_bars], QUANT_BEATS[a->set.loop_quant]);
    a->dirty = 0;
    return a;
}

void orcapp_destroy(OrcApp *a)
{
    if (!a) return;
    save(a);
    orc_destroy(a->orc);
    free(a);
}

void orcapp_audio(OrcApp *a, float *out, int frames) { orc_render(a->orc, out, frames); }
int orcapp_wants_quit(const OrcApp *a) { return a->quit; }
Orc *orcapp_engine(OrcApp *a) { return a->orc; }
void orcapp_set_page(OrcApp *a, int page) { a->page = ((page % PAGE_COUNT) + PAGE_COUNT) % PAGE_COUNT; }
void orcapp_save(OrcApp *a) { save(a); }
int orcapp_chord_live(const OrcApp *a) { return a->chord_live; }
const OrcSound *orcapp_sound(const OrcApp *a) { return &a->set.snd; }
int orcapp_keymode(const OrcApp *a) { return a->set.keymode; }

const char *orcapp_chord_name(const OrcApp *a)
{
    static char name[32];
    snprintf(name, sizeof(name), "%s%s", a->name_root, a->name_suffix);
    return name;
}

int orcapp_chord_notes(const OrcApp *a, int *notes, int *bass)
{
    memcpy(notes, a->notes, sizeof(int) * (size_t)a->notes_n);
    if (bass) *bass = a->bass;
    return a->notes_n;
}

void orcapp_select(OrcApp *a, int row)
{
    if (a->page == PAGE_SOUND) a->sound_sel = row;
    else if (a->page == PAGE_RHYTHM) a->rhythm_sel = row;
    else if (a->page == PAGE_MIDI) a->midi_sel = row;
}

/* ------------------------------------------------------------------ pagina Suono: parametri */
static void amount_text(const OrcApp *a, char *out, size_t n)
{
    float v = clamp01(a->set.snd.perform_amount);
    switch (a->set.snd.perform) {
    case PERF_ARP: snprintf(out, n, "%s", ARP_RATES[(int)(v * 4.99f)]); break;
    case PERF_PATTERN: snprintf(out, n, "Ritmo %d", (int)(v * 3.99f) + 1); break;
    case PERF_STRUM: snprintf(out, n, "%d ms", (int)lrintf((0.008f + v * 0.09f) * 1000.0f)); break;
    case PERF_HARP: snprintf(out, n, "%d ms", (int)lrintf((0.025f + v * 0.09f) * 1000.0f)); break;
    default: percent(out, n, v); break;
    }
}

static float param_bar(const OrcApp *a, int p)
{
    const OrcSound *s = &a->set.snd;
    switch (p) {
    case P_VOICING: return a->set.voicing;
    case P_AMOUNT: return s->perform_amount;
    case P_TONE: return s->tone;
    case P_CHAR: return s->character;
    case P_SUSTAIN: return s->sustain;
    case P_VIBRATO: return s->vibrato;
    case P_BASS: return s->bass_level;
    case P_BASS_TONE: return s->bass_tone;
    case P_CHORUS: return s->chorus;
    case P_DELAY: return s->delay;
    case P_REVERB: return s->reverb;
    case P_VOLUME: return s->volume;
    case P_ATTACK: return clamp01(log2f(s->attack / 0.002f) / 12.0f);
    case P_DECAY: return clamp01(log2f(s->decay / 0.01f) / 9.7f);
    case P_RELEASE: return clamp01(log2f(s->release / 0.01f) / 9.7f);
    }
    return -1.0f;
}

static const char *param_label(const OrcApp *a, int p)
{
    if (p == P_VIBRATO && a->set.snd.engine == ENG_REED) return "Tremolo";
    if (p == P_CHAR && a->set.snd.engine == ENG_FM) return "Rapporto";
    if (p == P_CHAR && a->set.snd.engine == ENG_ORGAN) return "Registri";
    return PARAM_NAMES[p];
}

static void param_text(const OrcApp *a, int p, char *out, size_t n)
{
    const OrcSound *s = &a->set.snd;
    switch (p) {
    case P_SOUND: snprintf(out, n, "%s", PRESETS[a->set.sound].name); break;
    case P_KEY: snprintf(out, n, "%s", note_it(a->set.key, key_spelling(a->set.key, a->set.minor))); break;
    case P_SCALE: snprintf(out, n, "%s", a->set.minor ? "Minore" : "Maggiore"); break;
    case P_KEYMODE: snprintf(out, n, "%s", a->set.keymode ? "Attivo" : "Spento"); break;
    case P_SPREAD: snprintf(out, n, "%s", SPREAD_NAMES[a->set.spread]); break;
    case P_PERFORM: snprintf(out, n, "%s", PERFORM_NAMES[s->perform]); break;
    case P_AMOUNT: amount_text(a, out, n); break;
    case P_ENGINE: snprintf(out, n, "%s", ENGINE_NAMES[s->engine]); break;
    case P_CHAR:
        if (s->engine == ENG_FM) {
            static const char *R[7] = { "1", "2", "3", "4", "1/2", "3,5", "7" };
            snprintf(out, n, "%s", R[(int)(clamp01(s->character) * 6.99f)]);
        } else percent(out, n, s->character);
        break;
    case P_ATTACK: seconds(out, n, s->attack); break;
    case P_DECAY: seconds(out, n, s->decay); break;
    case P_RELEASE: seconds(out, n, s->release); break;
    default: percent(out, n, param_bar(a, p)); break;
    }
}

static void param_change(OrcApp *a, int p, int dir, int big)
{
    OrcSound *s = &a->set.snd;
    float step = big ? 0.2f : 0.05f, mul = big ? 2.0f : 1.25f;
    switch (p) {
    case P_SOUND: load_preset(a, a->set.sound + dir); return;
    case P_KEY:
        a->set.key = (a->set.key + dir * (big ? 7 : 1) + 120) % 12;     /* con L1/R1 per quinte */
        a->dirty = 1;
        modifiers_changed(a);
        return;
    case P_SCALE: a->set.minor = !a->set.minor; a->dirty = 1; modifiers_changed(a); return;
    case P_KEYMODE: a->set.keymode = !a->set.keymode; a->dirty = 1; return;
    case P_VOICING: a->set.voicing = clamp01(a->set.voicing + dir * step); a->dirty = 1; modifiers_changed(a); return;
    case P_SPREAD: a->set.spread = (a->set.spread + dir + SPREAD_COUNT) % SPREAD_COUNT; a->dirty = 1; modifiers_changed(a); return;
    case P_PERFORM: s->perform = (s->perform + dir + PERF_COUNT) % PERF_COUNT; break;
    case P_ENGINE: s->engine = (s->engine + dir + ENG_COUNT) % ENG_COUNT; break;
    case P_AMOUNT:
        if (s->perform == PERF_ARP) s->perform_amount = clamp01((floorf(s->perform_amount * 4.99f) + dir) / 4.99f + 0.01f);
        else if (s->perform == PERF_PATTERN) s->perform_amount = clamp01((floorf(s->perform_amount * 3.99f) + dir) / 3.99f + 0.01f);
        else s->perform_amount = clamp01(s->perform_amount + dir * step);
        break;
    case P_CHAR:
        if (s->engine == ENG_FM) s->character = clamp01((floorf(s->character * 6.99f) + dir) / 6.99f + 0.01f);
        else s->character = clamp01(s->character + dir * step);
        break;
    case P_TONE: s->tone = clamp01(s->tone + dir * step); break;
    case P_ATTACK: s->attack = clampf(dir > 0 ? s->attack * mul : s->attack / mul, 0.002f, 8.0f); break;
    case P_DECAY: s->decay = clampf(dir > 0 ? s->decay * mul : s->decay / mul, 0.01f, 8.0f); break;
    case P_RELEASE: s->release = clampf(dir > 0 ? s->release * mul : s->release / mul, 0.01f, 8.0f); break;
    case P_SUSTAIN: s->sustain = clamp01(s->sustain + dir * step); break;
    case P_VIBRATO: s->vibrato = clamp01(s->vibrato + dir * step); break;
    case P_BASS: s->bass_level = clamp01(s->bass_level + dir * step); break;
    case P_BASS_TONE: s->bass_tone = clamp01(s->bass_tone + dir * step); break;
    case P_CHORUS: s->chorus = clamp01(s->chorus + dir * step); break;
    case P_DELAY: s->delay = clamp01(s->delay + dir * step); break;
    case P_REVERB: s->reverb = clamp01(s->reverb + dir * step); break;
    case P_VOLUME: s->volume = clamp01(s->volume + dir * step); break;
    }
    sound_changed(a);
}

/* ------------------------------------------------------------------ pagina Ritmo: parametri */
static void rhythm_text(const OrcApp *a, int r, char *out, size_t n)
{
    const Settings *s = &a->set;
    switch (r) {
    case R_BEAT: snprintf(out, n, "%s", s->beat_on ? "Sì" : "No"); break;
    case R_STYLE: snprintf(out, n, "%s", BEAT_NAMES[s->beat_style]); break;
    case R_LEVEL: percent(out, n, s->beat_level); break;
    case R_TEMPO: snprintf(out, n, "%d BPM", (int)lrintf(s->bpm)); break;
    case R_CLOCK: snprintf(out, n, "%s", s->clock_follow ? "Sì" : "No"); break;
    case R_BARS: snprintf(out, n, "%d %s", LOOP_BARS[s->loop_bars], LOOP_BARS[s->loop_bars] == 1 ? "battuta" : "battute"); break;
    case R_QUANT: snprintf(out, n, "%s", QUANT_NAMES[s->loop_quant]); break;
    }
}

static void rhythm_change(OrcApp *a, int r, int dir, int big)
{
    Settings *s = &a->set;
    switch (r) {
    case R_BEAT: beat_toggle(a); return;
    case R_STYLE: s->beat_style = (s->beat_style + dir + BEAT_COUNT) % BEAT_COUNT; beat_changed(a); return;
    case R_LEVEL: s->beat_level = clamp01(s->beat_level + dir * (big ? 0.2f : 0.05f)); beat_changed(a); return;
    case R_TEMPO:
        s->bpm = clampf(roundf(s->bpm) + dir * (big ? 10.0f : 1.0f), 30.0f, 300.0f);
        tempo_changed(a);
        if (s->clock_follow && a->clock_seen < 0.5f && a->clock_ticks) toast(a, "Il tempo arriva dal clock MIDI");
        return;
    case R_CLOCK: s->clock_follow = !s->clock_follow; a->dirty = 1; return;
    case R_BARS: s->loop_bars = (s->loop_bars + dir + 4) % 4; loop_config_changed(a); return;
    case R_QUANT: s->loop_quant = (s->loop_quant + dir + 4) % 4; loop_config_changed(a); return;
    }
}

static void tap_tempo(OrcApp *a)
{
    float now = a->time;
    if (a->tap_n && now - a->last_tap > 2.0f) a->tap_n = 0;
    a->last_tap = now;
    if (a->tap_n < 4) a->taps[a->tap_n++] = now;
    else { memmove(a->taps, a->taps + 1, sizeof(float) * 3); a->taps[3] = now; }
    if (a->tap_n >= 2) {
        float span = (a->taps[a->tap_n - 1] - a->taps[0]) / (float)(a->tap_n - 1);
        if (span > 0.15f) {
            a->set.bpm = clampf(roundf(60.0f / span), 30.0f, 300.0f);
            tempo_changed(a);
        }
    }
    char t[48];
    snprintf(t, sizeof(t), "Tap: %d BPM", (int)lrintf(a->set.bpm));
    toast(a, t);
}

/* ------------------------------------------------------------------ tasti della Flip */
static int sound_row_param(int row) { return row >= 0 && row < SOUND_ROW_COUNT ? SOUND_ROWS[row] : -1; }

static void move_sound_sel(OrcApp *a, int dir)
{
    int r = a->sound_sel;
    do { r = (r + dir + SOUND_ROW_COUNT) % SOUND_ROW_COUNT; } while (SOUND_ROWS[r] < 0);
    a->sound_sel = r;
}

static void move_midi_sel(OrcApp *a, int dir)
{
    a->midi_sel = (a->midi_sel + dir + MIDI_ROW_COUNT) % MIDI_ROW_COUNT;
}

static void midi_jump(OrcApp *a, int dir)
{
    int r = a->midi_sel;
    for (int i = 0; i < MIDI_ROW_COUNT; i++) {
        r = (r + dir + MIDI_ROW_COUNT) % MIDI_ROW_COUNT;
        if (MIDI_ROWS[r].kind == ROW_HEADER) break;
    }
    a->midi_sel = r;
}

static void learn_start(OrcApp *a, int first, int last)
{
    a->learn = first;
    a->learn_first = first;
    a->learn_last = last;
    char t[96];
    if (first == last) snprintf(t, sizeof(t), "%s: muovi o premi il controllo", TARGET_NAMES[first]);
    else snprintf(t, sizeof(t), "Uno alla volta, a partire da %s", TARGET_NAMES[first]);
    toast(a, t);
}

static void release_flip(OrcApp *a)
{
    for (int i = 0; i <= AUDITION_SLOT; i++) chord_unpress(a, SRC_FLIP(i));
    if (a->mods[MOD_SWAP] || a->mods[MOD_SUS] || a->mods[MOD_SEVENTH] || a->mods[MOD_NINTH]) {
        memset(a->mods, 0, sizeof(a->mods));
        modifiers_changed(a);
    }
}

static int repeats(const OrcApp *a, int b)
{
    if (a->page == PAGE_PLAY) return 0;
    if (b == PAD_UP || b == PAD_DOWN || b == PAD_LEFT || b == PAD_RIGHT) return 1;
    return a->page != PAGE_MIDI && (b == PAD_L1 || b == PAD_R1);
}

static void press(OrcApp *a, int b)
{
    if (a->quit_dialog) {
        if (b == PAD_A || b == PAD_DOWN) { a->quit = 1; panic(a); }
        else if (b == PAD_B || b == PAD_UP || b == PAD_MENU) a->quit_dialog = 0;
        return;
    }
    if (b == PAD_MENU) { release_flip(a); a->learn = -1; a->quit_dialog = 1; return; }
    if (b == PAD_SELECT) {
        release_flip(a);
        a->learn = -1;
        a->page = (a->page + 1) % PAGE_COUNT;
        return;
    }
    if (b == PAD_START) { transport_toggle(a); return; }
    if (b == PAD_L3) { panic(a); toast(a, "Tutte le note spente"); return; }

    switch (a->page) {
    case PAGE_PLAY:
        for (int i = 0; i < FLIP_SLOTS; i++)
            if (FLIP_PADS[i] == b) { chord_press(a, SRC_FLIP(i), 0.8f); return; }
        switch (b) {
        case PAD_L1: a->mods[MOD_SWAP] = 1; break;
        case PAD_R1: a->mods[MOD_SUS] = 1; break;
        case PAD_L2: a->mods[MOD_SEVENTH] = 1; break;
        case PAD_R2: a->mods[MOD_NINTH] = 1; break;
        case PAD_R3: perform_next(a); return;
        default: return;
        }
        modifiers_changed(a);
        break;
    case PAGE_SOUND: {
        int p = sound_row_param(a->sound_sel);
        switch (b) {
        case PAD_UP: move_sound_sel(a, -1); break;
        case PAD_DOWN: move_sound_sel(a, 1); break;
        case PAD_LEFT: param_change(a, p, -1, 0); break;
        case PAD_RIGHT: param_change(a, p, 1, 0); break;
        case PAD_L1: param_change(a, p, -1, 1); break;
        case PAD_R1: param_change(a, p, 1, 1); break;
        case PAD_A: chord_press(a, SRC_FLIP(AUDITION_SLOT), 0.8f); break;
        }
        break;
    }
    case PAGE_RHYTHM:
        switch (b) {
        case PAD_UP: a->rhythm_sel = (a->rhythm_sel + R_COUNT - 1) % R_COUNT; break;
        case PAD_DOWN: a->rhythm_sel = (a->rhythm_sel + 1) % R_COUNT; break;
        case PAD_LEFT: rhythm_change(a, a->rhythm_sel, -1, 0); break;
        case PAD_RIGHT: rhythm_change(a, a->rhythm_sel, 1, 0); break;
        case PAD_L1: rhythm_change(a, R_TEMPO, -1, 1); break;
        case PAD_R1: rhythm_change(a, R_TEMPO, 1, 1); break;
        case PAD_A: loop_record(a); break;
        case PAD_B: loop_play(a); break;
        case PAD_Y: loop_clear(a); break;
        case PAD_X: tap_tempo(a); break;
        }
        break;
    case PAGE_MIDI: {
        const MidiRow *r = &MIDI_ROWS[a->midi_sel];
        switch (b) {
        case PAD_UP: move_midi_sel(a, -1); break;
        case PAD_DOWN: move_midi_sel(a, 1); break;
        case PAD_L1: midi_jump(a, -1); break;
        case PAD_R1: midi_jump(a, 1); break;
        case PAD_LEFT:
        case PAD_RIGHT:
            if (r->kind == ROW_KNOB_MODE) { a->set.knob_mode = (a->set.knob_mode + (b == PAD_RIGHT ? 1 : 2)) % 3; a->dirty = 1; }
            break;
        case PAD_A:
            if (r->kind == ROW_KNOB_MODE) { a->set.knob_mode = (a->set.knob_mode + 1) % 3; a->dirty = 1; }
            else learn_start(a, r->first, r->last);
            break;
        case PAD_B:
            if (a->learn >= 0) { a->learn = -1; toast(a, "Apprendimento annullato"); }
            break;
        case PAD_X:
            if (r->kind == ROW_TARGET) { a->set.map[r->first].kind = MAP_NONE; a->dirty = 1; toast(a, "Assegnazione tolta"); }
            break;
        case PAD_Y:
            default_map(a->set.map);
            a->learn = -1;
            a->dirty = 1;
            toast(a, "Pad e manopole come nella MPK mini IV");
            break;
        }
        break;
    }
    }
}

static void release(OrcApp *a, int b)
{
    for (int i = 0; i < FLIP_SLOTS; i++)
        if (FLIP_PADS[i] == b) chord_unpress(a, SRC_FLIP(i));
    if (b == PAD_A) chord_unpress(a, SRC_FLIP(AUDITION_SLOT));
    int m = b == PAD_L1 ? MOD_SWAP : b == PAD_R1 ? MOD_SUS : b == PAD_L2 ? MOD_SEVENTH : b == PAD_R2 ? MOD_NINTH : -1;
    if (m >= 0 && a->mods[m]) { a->mods[m] = 0; modifiers_changed(a); }
}

void orcapp_button(OrcApp *a, int b, int pressed)
{
    if (b < 0 || b >= PAD_COUNT) return;
    if (pressed) {
        if (a->down[b]) return;
        a->down[b] = 1;
        a->rep[b] = 0.0f;
        press(a, b);
    } else {
        if (!a->down[b]) return;
        a->down[b] = 0;
        release(a, b);
    }
}

void orcapp_axes(OrcApp *a, float lx, float ly, float rx, float ry, float l2, float r2)
{
    (void)rx; (void)l2; (void)r2;
    float bend = fabsf(lx) > 0.2f ? (lx - (lx > 0 ? 0.2f : -0.2f)) / 0.8f * 2.0f : 0.0f;
    float mod = ly < -0.2f ? (-ly - 0.2f) / 0.8f : 0.0f;
    float bright = fabsf(ry) > 0.2f ? -(ry - (ry > 0 ? 0.2f : -0.2f)) / 0.8f : 0.0f;
    if (fabsf(bend - a->stick_bend) > 0.01f || fabsf(mod - a->stick_mod) > 0.01f || fabsf(bright - a->bright) > 0.01f) {
        a->stick_bend = bend;
        a->stick_mod = mod;
        a->bright = bright;
        expression(a);
    }
}

void orcapp_update(OrcApp *a, float dt)
{
    a->time += dt;
    for (int b = 0; b < PAD_COUNT; b++) {
        if (!a->down[b] || a->quit_dialog || !repeats(a, b)) continue;
        a->rep[b] += dt;
        while (a->rep[b] > 0.38f) { a->rep[b] -= 0.07f; press(a, b); }
    }
    a->toast_t = fmaxf(0.0f, a->toast_t - dt);
    a->flash_t = fmaxf(0.0f, a->flash_t - dt);
    a->glow = fmaxf(0.0f, a->glow - dt * 1.6f);
    a->midi_flash = fmaxf(0.0f, a->midi_flash - dt * 4.0f);
    a->clock_seen += dt;
    if (a->clock_seen > 1.0f) { a->clock_ticks = 0; a->clock_bpm = 0.0f; }
    if (a->dirty) {
        a->save_t += dt;
        if (a->save_t > 8.0f) { save(a); a->save_t = 0.0f; }
    } else {
        a->save_t = 0.0f;
    }
}

/* ------------------------------------------------------------------ disegno */
static uint32_t fnv(uint32_t h, const void *p, size_t n)
{
    const unsigned char *b = p;
    while (n--) { h ^= *b++; h *= 16777619u; }
    return h;
}

/* Si ridisegna quando qualcosa si muove (luce dell'accordo, messaggi, trasporto, apprendimento) o quando
   cambia lo stato mostrato; comunque una volta al secondo. */
int orcapp_needs_draw(OrcApp *a)
{
    if (a->glow > 0.0f || a->toast_t > 0.0f || a->flash_t > 0.0f || a->midi_flash > 0.0f || a->learn >= 0 ||
        a->quit_dialog || orc_transport_on(a->orc))
        return 1;
    OrcLoopInfo li;
    orc_loop_info(a->orc, &li);
    int ln[ORC_MAX_CHORD], tag = 0, lnn = orc_layer_notes(a->orc, ORC_LOOP, ln, &tag);
    int v[] = { a->page, a->sound_sel, a->rhythm_sel, a->midi_sel, a->chord_live, a->notes_n, a->bass, a->stack_n,
                li.state, li.events, li.full, lnn, tag, a->clock_ticks > 0 && a->clock_seen < 0.5f };
    uint32_t h = fnv(2166136261u, v, sizeof(v));
    h = fnv(h, &a->set, sizeof(a->set));
    h = fnv(h, a->notes, sizeof(a->notes));
    h = fnv(h, ln, sizeof(int) * (size_t)lnn);
    h = fnv(h, a->name_root, sizeof(a->name_root));
    h = fnv(h, a->name_suffix, sizeof(a->name_suffix));
    h = fnv(h, a->numeral, sizeof(a->numeral));
    h = fnv(h, a->pad_held, sizeof(a->pad_held));
    h = fnv(h, a->mods, sizeof(a->mods));
    for (int i = 0; i < a->stack_n; i++) h = fnv(h, &a->stack[i].src, sizeof(int));
    h = fnv(h, a->melody_down, sizeof(a->melody_down));
    h = fnv(h, a->melody_pedal, sizeof(a->melody_pedal));
    h = fnv(h, a->midi_name, strlen(a->midi_name));
    h = fnv(h, a->last_msg, strlen(a->last_msg));
    if (h == a->drawn_key && a->time - a->drawn_time < 1.0f) return 0;
    a->drawn_key = h;
    a->drawn_time = a->time;
    return 1;
}

static void draw_logo(Canvas *c, float x, float y, float s)
{
    /* un'orchidea: tre petali in alto, due in basso, il labello al centro */
    for (int i = 0; i < 3; i++) {
        float ang = (-90.0f + (i - 1) * 62.0f) * 3.14159265f / 180.0f;
        gfx_circle(c, x + cosf(ang) * 7.0f * s, y + sinf(ang) * 7.0f * s, 5.2f * s, C_GOLD, 1.0f);
    }
    for (int i = 0; i < 2; i++) {
        float ang = (55.0f + i * 70.0f) * 3.14159265f / 180.0f;
        gfx_circle(c, x + cosf(ang) * 7.5f * s, y + sinf(ang) * 7.5f * s, 4.4f * s, C_GOLD, 0.85f);
    }
    gfx_circle(c, x, y + 1.0f * s, 3.6f * s, C_VIOLET, 1.0f);
}

static void draw_header(OrcApp *a, Canvas *c)
{
    gfx_rect(c, 0, 0, c->w, 46, C_BAR);
    gfx_rect(c, 0, 46, c->w, 1, C_LINE);
    draw_logo(c, 24, 23, 1.0f);
    gfx_text(c, FONT_TITLE, 44, 31, "OpenOrc", C_TEXT);
    static const char *tabs[PAGE_COUNT] = { "Suona", "Suono", "Ritmo", "MIDI" };
    int x = 186;
    for (int i = 0; i < PAGE_COUNT; i++) {
        int w = gfx_text_width(FONT_BOLD, tabs[i]) + 22;
        if (i == a->page) gfx_round_rect(c, x, 10, w, 27, 13.5f, C_GOLD_D, 1.0f);
        gfx_text_center(c, FONT_BOLD, x + w / 2, 29, tabs[i], i == a->page ? C_TEXT : C_MUTED);
        x += w + 4;
    }
    /* tastiera MIDI e trasporto */
    int connected = a->midi_name[0] != 0;
    gfx_circle(c, 518, 23, 5.0f, connected ? gfx_mix(C_GREEN, C_TEXT, a->midi_flash) : C_DIM, 1.0f);
    gfx_text(c, FONT_SMALL, 528, 28, "MIDI", connected ? C_TEXT : C_DIM);
    int on = orc_transport_on(a->orc);
    double beats = orc_beats(a->orc);
    float pulse = on ? expf(-(float)(beats - floor(beats)) * 5.0f) : 0.0f;
    if (on) {
        gfx_circle(c, 606, 23, 11.0f, gfx_mix(C_GOLD_D, C_GOLD, pulse), 1.0f);
        gfx_line(c, 603, 17, 603, 29, 2.0f, C_BAR, 1.0f);
        gfx_line(c, 603, 17, 611, 23, 2.0f, C_BAR, 1.0f);
        gfx_line(c, 603, 29, 611, 23, 2.0f, C_BAR, 1.0f);
    } else {
        gfx_circle(c, 606, 23, 11.0f, C_PANEL_HI, 1.0f);
        gfx_rect(c, 602, 19, 8, 8, C_DIM);
    }
}

static void draw_hints(Canvas *c, const char *text)
{
    gfx_rect(c, 0, 446, c->w, 34, C_BAR);
    gfx_rect(c, 0, 446, c->w, 1, C_LINE);
    gfx_text_center(c, FONT_SMALL, c->w / 2, 468, text, C_MUTED);
}

static void loop_state_text(const OrcLoopInfo *li, char *out, size_t n)
{
    int bars = (int)lrint(li->length / 4.0), bar = (int)(li->position / 4.0) + 1;
    switch (li->state) {
    case LOOP_EMPTY: snprintf(out, n, "vuoto"); break;
    case LOOP_ARMED: snprintf(out, n, "attende la battuta"); break;
    case LOOP_RECORDING: snprintf(out, n, "registra %d/%d", bar, bars); break;
    case LOOP_PLAYING: snprintf(out, n, "suona %d/%d", bar, bars); break;
    case LOOP_OVERDUB: snprintf(out, n, "sovraincide %d/%d", bar, bars); break;
    default: snprintf(out, n, "fermo"); break;
    }
}

/* Tastiera da Do2 a Do6 con le note accese: accordo, basso, melodia e accordo del loop. */
static void draw_keyboard(OrcApp *a, Canvas *c, int y, int h)
{
    enum { LO = 36, HI = 84 };
    unsigned char lit[128] = { 0 };
    int ln[ORC_MAX_CHORD], ltag = 0, lnn = orc_layer_notes(a->orc, ORC_LOOP, ln, &ltag);
    for (int i = 0; i < lnn; i++) if (ln[i] >= 0 && ln[i] < 128) lit[ln[i]] = 4;
    if (a->chord_live) {
        for (int i = 0; i < a->notes_n; i++) lit[a->notes[i]] = 1;
        if (a->set.snd.bass_level > 0.005f) lit[a->bass] = 2;
    }
    for (int n = 0; n < 128; n++) if (a->melody_down[n] || a->melody_pedal[n]) lit[n] = 3;
    static const int white_of[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
    const int kw = 20, whites = 29, x0 = (c->w - whites * kw) / 2;
    uint32_t col_w[5] = { C_KEY_W, C_GOLD, C_TEAL, C_VIOLET, gfx_mix(C_KEY_W, C_GOLD, 0.45f) };
    uint32_t col_b[5] = { C_KEY_B, gfx_mix(C_GOLD, C_KEY_B, 0.2f), gfx_mix(C_TEAL, C_KEY_B, 0.2f), gfx_mix(C_VIOLET, C_KEY_B, 0.2f), gfx_mix(C_GOLD, C_KEY_B, 0.6f) };
    for (int n = LO; n <= HI; n++) {
        int w = white_of[n % 12];
        if (w < 0) continue;
        int idx = (n / 12 - LO / 12) * 7 + w, x = x0 + idx * kw;
        gfx_rect(c, x + 1, y, kw - 2, h, col_w[lit[n]]);
        if (n % 12 == 0 && n > LO) {
            char t[8];
            note_label(t, sizeof(t), n);
            gfx_text_center(c, FONT_SMALL, x + kw / 2, y + h - 7, t, lit[n] ? C_BAR : C_DIM);
        }
    }
    for (int n = LO; n <= HI; n++) {
        if (white_of[n % 12] >= 0) continue;
        int idx = (n / 12 - LO / 12) * 7 + white_of[(n - 1) % 12], x = x0 + (idx + 1) * kw - 6;
        gfx_rect(c, x, y, 12, h * 62 / 100, col_b[lit[n]]);
    }
}

static void draw_play(OrcApp *a, Canvas *c)
{
    char t[96];
    /* accordo */
    gfx_round_rect(c, 16, 56, 384, 142, 14, C_PANEL, 1.0f);
    if (a->name_root[0]) {
        uint32_t col = a->chord_live ? gfx_mix(C_TEXT, C_GOLD, 0.35f + 0.65f * a->glow) : C_DIM;
        int wr = gfx_text_width(FONT_HUGE, a->name_root), ws = gfx_text_width(FONT_BIG, a->name_suffix);
        int x = 208 - (wr + ws + 4) / 2;
        gfx_text(c, FONT_HUGE, x, 128, a->name_root, col);
        gfx_text(c, FONT_BIG, x + wr + 4, 128, a->name_suffix, col);
        char notes[64] = "";
        for (int i = 0; i < a->notes_n && i < 6; i++) {
            int pc = a->notes[i] % 12, dup = 0;
            for (int j = 0; j < i; j++) dup |= a->notes[j] % 12 == pc;
            if (dup) continue;
            size_t l = strlen(notes);
            snprintf(notes + l, sizeof(notes) - l, "%s%s", l ? " " : "", note_it(pc, a->spelling));
        }
        snprintf(t, sizeof(t), "%s   ·   %s", a->symbol, notes);
        gfx_text_center(c, FONT_BODY, 208, 160, t, a->chord_live ? C_TEXT : C_DIM);
        if (a->numeral[0]) {
            char k[32];
            key_name(k, sizeof(k), a->set.key, a->set.minor);
            snprintf(t, sizeof(t), "%s in %s", a->numeral, k);
        } else {
            char b[16];
            note_label(b, sizeof(b), a->bass);
            snprintf(t, sizeof(t), "basso %s", b);
        }
        gfx_text_center(c, FONT_SMALL, 208, 184, t, C_MUTED);
    } else {
        gfx_text_center(c, FONT_TITLE, 208, 106, "Suona un accordo", C_TEXT);
        gfx_text_center(c, FONT_SMALL, 208, 140, "MPK: tieni un pad (Maj, Min...) e premi un tasto", C_MUTED);
        char k[32];
        key_name(k, sizeof(k), a->set.key, a->set.minor);
        snprintf(t, sizeof(t), "Flip: croce e A B X Y, accordi di %s", k);
        gfx_text_center(c, FONT_SMALL, 208, 162, t, C_MUTED);
    }

    /* stato */
    gfx_round_rect(c, 410, 56, 214, 142, 14, C_PANEL, 1.0f);
    const char *labels[5] = { "Suono", "Esecuzione", "Tonalità", "Ritmo", "Loop" };
    char vals[5][48];
    snprintf(vals[0], sizeof(vals[0]), "%s", PRESETS[a->set.sound].name);
    if (a->set.snd.perform == PERF_ARP) snprintf(vals[1], sizeof(vals[1]), "Arpeggio %s", ARP_SHORT[(int)(clamp01(a->set.snd.perform_amount) * 4.99f)]);
    else snprintf(vals[1], sizeof(vals[1]), "%s", PERFORM_NAMES[a->set.snd.perform]);
    key_name(vals[2], sizeof(vals[2]), a->set.key, a->set.minor);
    snprintf(vals[3], sizeof(vals[3]), "%d · %s", (int)lrintf(a->set.bpm), a->set.beat_on ? BEAT_NAMES[a->set.beat_style] : "senza batteria");
    OrcLoopInfo li;
    orc_loop_info(a->orc, &li);
    loop_state_text(&li, vals[4], sizeof(vals[4]));
    for (int i = 0; i < 5; i++) {
        int y = 80 + i * 26;
        gfx_text(c, FONT_SMALL, 422, y, labels[i], C_MUTED);
        uint32_t col = C_TEXT;
        if (i == 2) col = a->set.keymode ? C_GOLD : C_MUTED;
        if (i == 4) col = li.state == LOOP_RECORDING ? C_RED : (li.state == LOOP_EMPTY ? C_DIM : C_TEXT);
        int font = gfx_text_width(FONT_BOLD, vals[i]) > 112 ? FONT_SMALL : FONT_BOLD;
        gfx_text_right(c, font, 612, y, vals[i], col);
    }

    /* tastiera */
    draw_keyboard(a, c, 208, 84);

    /* pad della MPK */
    for (int i = 0; i < 8; i++) {
        int col = i % 4, row = i / 4;
        float x = 16 + col * 74, y = 304 + row * 60;
        int held = a->pad_held[i];
        uint32_t fill = held ? (row == 0 ? C_GOLD : C_VIOLET) : C_PANEL_HI;
        gfx_round_rect(c, x, y, 66, 52, 10, fill, 1.0f);
        gfx_text_center(c, FONT_BOLD, (int)(x + 33), (int)(y + 32), CHORD_PAD_NAMES[i], held ? C_BAR : (row == 0 ? C_TEXT : C_MUTED));
    }
    gfx_text_center(c, FONT_SMALL, 160, 438, a->midi_name[0] ? a->midi_name : "MPK non collegata", a->midi_name[0] ? C_MUTED : C_DIM);

    /* croce e tasti della Flip */
    static const float OFF[4][2] = { { -44, 0 }, { 0, -30 }, { 44, 0 }, { 0, 30 } };
    static const int SLOT_AT[2][4] = { { 0, 1, 2, 3 }, { 4, 5, 7, 6 } };    /* sinistra, su, destra, giu' */
    static const char *LETTERS[4] = { "Y", "X", "A", "B" };
    static const float LETTER_AT[4][2] = { { -12, 5 }, { 0, -5 }, { 12, 5 }, { 0, 15 } };
    for (int d = 0; d < 2; d++) {
        float px = d ? 476 : 320, cx = px + 74, cy = 354;
        gfx_round_rect(c, px, 300, 148, 108, 14, C_PANEL, 1.0f);
        if (d == 0) {
            gfx_round_rect(c, cx - 4, cy - 11, 8, 22, 2, C_LINE, 1.0f);
            gfx_round_rect(c, cx - 11, cy - 4, 22, 8, 2, C_LINE, 1.0f);
        } else {
            for (int k = 0; k < 4; k++)
                gfx_text_center(c, FONT_SMALL, (int)(cx + LETTER_AT[k][0]), (int)(cy + LETTER_AT[k][1]), LETTERS[k], C_DIM);
        }
        for (int k = 0; k < 4; k++) {
            int slot = SLOT_AT[d][k];
            int root, iv[CHORD_MAX_IV], deg, sp;
            int n = build_chord(a, SRC_FLIP(slot), &root, iv, &deg, &sp);
            char name[24];
            chord_name(name, sizeof(name), root, chord_mask(iv, n), sp);
            int held = stack_find(a, SRC_FLIP(slot)) >= 0;
            float x = cx + OFF[k][0] - 27, y = cy + OFF[k][1] - 13;
            gfx_round_rect(c, x, y, 54, 26, 8, held ? C_GOLD : C_PANEL_HI, 1.0f);
            int font = gfx_text_width(FONT_BOLD, name) > 48 ? FONT_SMALL : FONT_BOLD;
            gfx_text_center(c, font, (int)(cx + OFF[k][0]), (int)(cy + OFF[k][1] + 6), name, held ? C_BAR : C_TEXT);
        }
    }
    static const char *MODS[MOD_COUNT] = { "L1 m/M", "R1 sus4", "L2 7", "R2 9" };
    for (int i = 0; i < MOD_COUNT; i++) {
        float x = 320 + i * 77;
        gfx_round_rect(c, x, 415, 73, 23, 11.5f, a->mods[i] ? C_GOLD : C_PANEL, 1.0f);
        gfx_text_center(c, FONT_SMALL, (int)(x + 36), 432, MODS[i], a->mods[i] ? C_BAR : C_MUTED);
    }

    if (a->flash_t > 0.0f) {
        float al = fminf(1.0f, a->flash_t * 4.0f);
        int w = gfx_text_width(FONT_BOLD, a->flash) + 32;
        gfx_round_rect(c, 320 - w / 2.0f, 236, w, 30, 15, C_BAR, 0.92f * al);
        gfx_round_frame(c, 320 - w / 2.0f, 236, w, 30, 15, 1.5f, C_GOLD, al);
        gfx_text_center(c, FONT_BOLD, 320, 257, a->flash, gfx_mix(C_BAR, C_TEXT, al));
    }
    draw_hints(c, "Croce e A B X Y: accordi · L1 L2 R1 R2: varianti · R3 esecuzione · START ritmo");
}

typedef struct { int header; const char *label; char value[48]; float bar; uint32_t color; } ListRow;

static void draw_list(Canvas *c, const ListRow *rows, int n, int sel, int *scroll, int y0, int y1, int rh)
{
    int vis = (y1 - y0) / rh;
    if (sel - 1 >= 0 && rows[sel - 1].header && sel - 1 < *scroll) *scroll = sel - 1;
    if (sel < *scroll) *scroll = sel;
    if (sel >= *scroll + vis) *scroll = sel - vis + 1;
    if (*scroll > n - vis) *scroll = n - vis;
    if (*scroll < 0) *scroll = 0;
    for (int i = *scroll; i < n && i < *scroll + vis; i++) {
        int y = y0 + (i - *scroll) * rh, base = y + rh / 2 + 6;
        const ListRow *r = &rows[i];
        if (r->header) {
            int w = gfx_text_width(FONT_BOLD, r->label), right = 604;
            gfx_text(c, FONT_BOLD, 28, base, r->label, i == sel ? C_GOLD : gfx_mix(C_GOLD, C_MUTED, 0.3f));
            if (r->value[0]) {
                gfx_text_right(c, FONT_SMALL, 604, base, r->value, i == sel ? C_GOLD : C_DIM);
                right -= gfx_text_width(FONT_SMALL, r->value) + 12;
            }
            if (right > 48 + w) gfx_rect(c, 38 + w, y + rh / 2, right - 38 - w, 1, C_LINE);
            if (i == sel) gfx_round_frame(c, 16, y + 1, 608, rh - 2, 9, 1.5f, C_GOLD, 1.0f);
            continue;
        }
        if (i == sel) gfx_round_rect(c, 16, y + 1, 608, rh - 2, 9, C_PANEL_HI, 1.0f);
        gfx_text(c, FONT_BODY, 36, base, r->label, i == sel ? C_TEXT : C_MUTED);
        if (r->bar >= 0.0f) {
            gfx_round_rect(c, 352, y + rh / 2 - 3, 120, 6, 3, C_LINE, 1.0f);
            gfx_round_rect(c, 352, y + rh / 2 - 3, 6 + 114 * clamp01(r->bar), 6, 3, i == sel ? C_GOLD : C_MUTED, 1.0f);
        }
        gfx_text_right(c, FONT_BOLD, 604, base, r->value, r->color ? r->color : (i == sel ? C_GOLD : C_TEXT));
    }
    if (n > vis) {                                /* barra di scorrimento */
        float h = (float)(y1 - y0) * vis / n, y = y0 + (float)(y1 - y0 - h) * *scroll / (n - vis);
        gfx_round_rect(c, 627, y, 4, h, 2, C_LINE, 1.0f);
    }
}

static void draw_sound(OrcApp *a, Canvas *c)
{
    ListRow rows[SOUND_ROW_COUNT];
    for (int i = 0; i < SOUND_ROW_COUNT; i++) {
        int p = SOUND_ROWS[i];
        ListRow *r = &rows[i];
        memset(r, 0, sizeof(*r));
        if (p < 0) { r->header = 1; r->label = SOUND_HEADERS[-1 - p]; continue; }
        r->label = param_label(a, p);
        param_text(a, p, r->value, sizeof(r->value));
        r->bar = param_bar(a, p);
        if (p == P_KEYMODE && !a->set.keymode) r->color = C_MUTED;
    }
    draw_list(c, rows, SOUND_ROW_COUNT, a->sound_sel, &a->sound_scroll, 54, 442, 28);
    draw_hints(c, "Su/giù sceglie · sinistra/destra cambia (L1/R1 a passi grandi) · A tenuto ascolta");
}

static void draw_rhythm(OrcApp *a, Canvas *c)
{
    char t[96];
    int on = orc_transport_on(a->orc);
    /* tempo */
    gfx_round_rect(c, 16, 56, 222, 96, 14, C_PANEL, 1.0f);
    snprintf(t, sizeof(t), "%d", (int)lrintf(a->set.bpm));
    gfx_text(c, FONT_HUGE, 30, 126, t, C_TEXT);
    int w = gfx_text_width(FONT_HUGE, t);
    gfx_text(c, FONT_BOLD, 38 + w, 96, "BPM", C_MUTED);
    gfx_text(c, FONT_SMALL, 38 + w, 118, on ? "in moto" : "fermo", on ? C_GOLD : C_DIM);
    if (a->clock_ticks && a->clock_seen < 0.5f) gfx_text(c, FONT_SMALL, 38 + w, 138, a->set.clock_follow ? "clock MIDI" : "clock ignorato", C_GREEN);

    /* batteria */
    gfx_round_rect(c, 248, 56, 376, 96, 14, C_PANEL, 1.0f);
    snprintf(t, sizeof(t), "%s", BEAT_NAMES[a->set.beat_style]);
    gfx_text(c, FONT_BOLD, 262, 80, t, a->set.beat_on ? C_TEXT : C_MUTED);
    gfx_text_right(c, FONT_SMALL, 612, 80, a->set.beat_on ? "accesa" : "spenta", a->set.beat_on ? C_GOLD : C_DIM);
    static const char *PARTS[3] = { "Cassa", "Rullante", "Piatti" };
    int step = orc_beat_step(a->orc);
    for (int r = 0; r < 3; r++) {
        int y = 90 + r * 19;
        gfx_text(c, FONT_SMALL, 262, y + 13, PARTS[r], C_MUTED);
        const char *p1 = orc_beat_pattern(a->set.beat_style, r == 0 ? DRUM_KICK : (r == 1 ? DRUM_SNARE : DRUM_HAT));
        const char *p2 = orc_beat_pattern(a->set.beat_style, r == 0 ? DRUM_KICK : (r == 1 ? DRUM_CLAP : DRUM_OPEN));
        for (int s = 0; s < 16; s++) {
            float x = 334 + s * 17.5f + (s / 4) * 2.0f;
            int hit = p1[s] == 'x' || p2[s] == 'x';
            uint32_t col = hit ? (a->set.beat_on ? C_GOLD : C_MUTED) : C_PANEL_HI;
            if (s == step) col = hit ? C_TEXT : C_LINE;
            gfx_round_rect(c, x, y, 14, 15, 3, col, 1.0f);
        }
    }

    /* loop */
    OrcLoopInfo li;
    orc_loop_info(a->orc, &li);
    gfx_round_rect(c, 16, 160, 608, 92, 14, C_PANEL, 1.0f);
    char st[48];
    loop_state_text(&li, st, sizeof(st));
    snprintf(t, sizeof(t), "Loop: %s", st);
    uint32_t sc = li.state == LOOP_RECORDING ? C_RED : (li.state == LOOP_ARMED ? C_GOLD : (li.state == LOOP_EMPTY ? C_MUTED : C_TEXT));
    gfx_text(c, FONT_BOLD, 30, 186, t, sc);
    if (li.full) gfx_text_right(c, FONT_SMALL, 610, 186, "pieno: niente altre note", C_RED);
    else {
        snprintf(t, sizeof(t), "%d eventi", li.events);
        gfx_text_right(c, FONT_SMALL, 610, 186, li.state == LOOP_EMPTY ? "A registra, poi suona sulla MPK o sulla Flip" : t, C_DIM);
    }
    float tx = 30, tw = 580, ty = 200, th = 36;
    gfx_round_rect(c, tx, ty, tw, th, 6, C_BAR, 1.0f);
    if (li.length > 0.0) {
        int bars = (int)lrint(li.length / 4.0);
        for (int b = 0; b <= (int)lrint(li.length); b++) {
            float x = tx + tw * (float)(b / li.length);
            if (b % 4 == 0) gfx_rect(c, (int)x, (int)ty, 1, (int)th, C_LINE);
            else gfx_rect(c, (int)x, (int)(ty + th - 6), 1, 6, C_LINE);
        }
        (void)bars;
        float mb[ORC_LOOP_MAX];
        int mk[ORC_LOOP_MAX];
        int n = orc_loop_marks(a->orc, mb, mk, ORC_LOOP_MAX);
        for (int i = 0; i < n; i++) {
            if (mk[i] == 2) continue;
            float x = tx + tw * (float)(mb[i] / li.length);
            if (mk[i] == 0) gfx_rect(c, (int)x, (int)ty + 3, 3, (int)th - 6, C_GOLD);
            else gfx_circle(c, x, ty + th / 2, 3.0f, C_VIOLET, 1.0f);
        }
        if (li.state == LOOP_RECORDING || li.state == LOOP_PLAYING || li.state == LOOP_OVERDUB) {
            float x = tx + tw * (float)(li.position / li.length);
            gfx_rect(c, (int)x, (int)ty - 3, 2, (int)th + 6, li.state == LOOP_PLAYING ? C_TEXT : C_RED);
        }
    }

    /* impostazioni */
    ListRow rows[R_COUNT];
    for (int i = 0; i < R_COUNT; i++) {
        memset(&rows[i], 0, sizeof(rows[i]));
        rows[i].label = RHYTHM_NAMES[i];
        rhythm_text(a, i, rows[i].value, sizeof(rows[i].value));
        rows[i].bar = i == R_LEVEL ? a->set.beat_level : -1.0f;
    }
    draw_list(c, rows, R_COUNT, a->rhythm_sel, &a->rhythm_scroll, 258, 440, 26);
    draw_hints(c, "A registra · B suona/ferma · Y cancella il loop · X tap tempo · L1/R1 tempo ±10");
}

static void draw_midi(OrcApp *a, Canvas *c)
{
    ListRow rows[MIDI_ROW_COUNT];
    int blink = (int)(a->time * 3.0f) % 2;
    for (int i = 0; i < MIDI_ROW_COUNT; i++) {
        const MidiRow *m = &MIDI_ROWS[i];
        ListRow *r = &rows[i];
        memset(r, 0, sizeof(*r));
        r->label = m->label;
        r->bar = -1.0f;
        if (m->kind == ROW_HEADER) {
            r->header = 1;
            snprintf(r->value, sizeof(r->value), "A: impara tutti in fila");
        } else if (m->kind == ROW_KNOB_MODE) {
            snprintf(r->value, sizeof(r->value), "%s", KNOB_MODE_NAMES[a->set.knob_mode]);
        } else if (a->learn == m->first) {
            snprintf(r->value, sizeof(r->value), "in ascolto...");
            r->color = blink ? C_GOLD : C_TEXT;
        } else {
            map_text(&a->set.map[m->first], r->value, sizeof(r->value));
            if (a->set.map[m->first].kind == MAP_NONE) r->color = C_DIM;
        }
    }
    draw_list(c, rows, MIDI_ROW_COUNT, a->midi_sel, &a->midi_scroll, 54, 408, 26);
    gfx_rect(c, 16, 412, 608, 1, C_LINE);
    char t[140];
    snprintf(t, sizeof(t), "Ultimo messaggio: %s", a->last_msg[0] ? a->last_msg : "nessuno");
    gfx_text(c, FONT_SMALL, 24, 434, t, a->midi_flash > 0.05f ? C_TEXT : C_MUTED);
    draw_hints(c, "A impara · B annulla · X toglie · Y valori della MPK mini IV · L1/R1 gruppo");
}

static void draw_quit(Canvas *c)
{
    gfx_rect_alpha(c, 0, 0, c->w, c->h, RGB(0, 0, 0), 0.6f);
    gfx_round_rect(c, 140, 170, 360, 140, 18, C_PANEL_HI, 1.0f);
    gfx_round_frame(c, 140, 170, 360, 140, 18, 2, C_GOLD, 1.0f);
    gfx_text_center(c, FONT_TITLE, 320, 222, "Uscire da OpenOrc?", C_TEXT);
    gfx_text_center(c, FONT_BODY, 320, 256, "Suoni e impostazioni restano salvati.", C_MUTED);
    gfx_text_center(c, FONT_BOLD, 320, 290, "A o giù: esci   ·   B o su: resta", C_GOLD);
}

void orcapp_draw(OrcApp *a, Canvas *c)
{
    gfx_vgradient(c, 0, 0, c->w, c->h, C_BG_TOP, C_BG_BOT);
    draw_header(a, c);
    switch (a->page) {
    case PAGE_PLAY: draw_play(a, c); break;
    case PAGE_SOUND: draw_sound(a, c); break;
    case PAGE_RHYTHM: draw_rhythm(a, c); break;
    default: draw_midi(a, c); break;
    }
    if (a->toast_t > 0.0f && a->toast[0]) {
        float al = fminf(1.0f, a->toast_t * 3.0f);
        int w = gfx_text_width(FONT_BOLD, a->toast) + 36;
        if (w > 620) w = 620;
        gfx_round_rect(c, 320 - w / 2.0f, 398, w, 36, 18, C_BAR, 0.94f * al);
        gfx_round_frame(c, 320 - w / 2.0f, 398, w, 36, 18, 1.5f, C_GOLD, al);
        gfx_text_center(c, FONT_BOLD, 320, 422, a->toast, gfx_mix(C_BAR, C_TEXT, al));
    }
    if (a->quit_dialog) draw_quit(c);
}
