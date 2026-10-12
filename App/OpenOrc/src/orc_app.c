/* OpenOrc per Miyoo Flip - applicazione (vedi orc_app.h).
 *
 * Gli accordi si suonano come sull'Orchid: tenendo un pad di qualita' (Dim, Min, Maj, Sus) ogni tasto
 * suona l'accordo su quella fondamentale e i pad 6, m7, M7 e 9 aggiungono le estensioni. Senza pad i
 * tasti suonano la melodia; con il modo tonalita' attivo suonano gli accordi della scala (i tasti fuori
 * scala prendono il grado sotto). Il voicing sceglie da solo il rivolto piu' vicino al registro della
 * manopola K1, cosi' le voci passano da un accordo all'altro muovendosi poco.
 *
 * MPK mini IV (i valori di fabbrica dei suoi preset utente, cosi' non va configurata; tutti riassegnabili
 * nella pagina MIDI o con un preset in tastiere/):
 *   pad banco A, canale 10: 40-43 Dim Min Maj Sus (fila in alto), 36-39 6 m7 M7 9 (fila in basso);
 *   pad banco B: 44-47 batteria, loop registra, loop suona/ferma, loop annulla (fila in basso),
 *                48-51 modo tonalita', esecuzione successiva, suono precedente, suono successivo;
 *   manopole CC 24-31: voicing, basso, quantita' dell'esecuzione, tono, chorus, delay, riverbero, volume
 *   (da qualunque porta e in modo relativo: la MPK mini IV le manda solo sulla porta 2, 1-63 a destra e
 *   65-127 a sinistra, misurato l'11/10/2026; pad e tasti arrivano sia sulla porta 1 sia sulla 2);
 *   rotelle: pitch bend (+-2 semitoni) e modulazione (vibrato); pedale sustain (CC 64);
 *   program change: suono. Trasporto sulla porta 2 (DAW): Play CC 76, Rec 77, Loop 74, Undo 73.
 * Sulla Flip, nella pagina Suona, croce e A B X Y suonano gli accordi della tonalita' (I IV V vi sulla
 * croce) e i dorsali li cambiano (L1 maggiore/minore, R1 sus4, L2 settima, R2 nona).
 *
 * SELECT toccato cambia pagina. SELECT tenuto apre l'elenco dei suoni e fa da tasto delle funzioni, in
 * tutte le pagine: su/giu' suono, L1/R1 categoria, sinistra/destra tonalita', L2 maggiore/minore, R2 modo
 * tonalita', A registra il loop, B lo suona o lo ferma, Y annulla, X batteria.
 *
 * Tastiere: ogni mappa ricorda il dispositivo per cui e' fatta e si ricarica quando lo si ricollega. Una
 * tastiera mai vista si configura con l'IA (pagina MIDI, prima riga, o la domanda che compare quando la si
 * collega): si muovono i controlli, il modello gratuito migliore tra quelli con una chiave propone la mappa.
 *
 * Suoni: quelli di fabbrica (PRESETS) e quelli dell'utente, un file di testo ciascuno in
 * Saves/openorc/suoni. Il suono corrente si puo' cambiare: un asterisco lo segnala finche' non lo si
 * salva o ripristina. Il loop si salva all'uscita in Saves/openorc/loop.txt e si riprende fermo.
 */
#include "orc_app.h"

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define make_dir(p) _mkdir(p)
#else
#define make_dir(p) mkdir(p, 0755)
#endif

#include "ask.h"
#include "chords.h"
#include "iaconf.h"
#include "midi.h"
#include "net.h"
#include "orc_ia.h"
#include "platform.h"
#include "providers.h"

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
typedef struct { const char *name, *cat; OrcSound s; int spread; } Preset;
/* motore, tono, carattere, attacco, decadimento, sostegno, rilascio, vibrato, basso, tono del basso,
   chorus, delay, riverbero, esecuzione, quantita', volume, stereo. In ordine di categoria: e' l'ordine
   dell'elenco, dei pad Suono precedente/successivo e del program change. */
static const Preset PRESETS[] = {
    { "Lungolago",          "Atmosfere", { ENG_VA,    0.45f, 0.45f, 0.600f, 1.20f, 0.80f, 1.80f, 0.10f, 0.50f, 0.30f, 0.60f, 0.20f, 0.60f, PERF_CHORD,   0.30f, 0.80f, 0.00f }, SPREAD_OPEN },
    { "Nebbia",             "Atmosfere", { ENG_VA,    0.25f, 0.70f, 1.200f, 2.00f, 0.90f, 2.50f, 0.20f, 0.30f, 0.20f, 0.70f, 0.30f, 0.80f, PERF_CHORD,   0.30f, 0.80f, 0.00f }, SPREAD_WIDE },
    { "Psichedelia",        "Atmosfere", { ENG_VA,    0.50f, 0.85f, 0.010f, 0.80f, 0.60f, 0.60f, 0.30f, 0.50f, 0.50f, 0.80f, 0.50f, 0.50f, PERF_SLOP,    0.70f, 0.80f, 0.00f }, SPREAD_WIDE },
    { "Aurora",             "Atmosfere", { ENG_SUPER, 0.40f, 0.45f, 0.900f, 2.00f, 0.85f, 2.20f, 0.08f, 0.35f, 0.25f, 0.35f, 0.20f, 0.75f, PERF_CHORD,   0.30f, 0.75f, 0.90f }, SPREAD_WIDE },
    { "Archi d'inverno",    "Atmosfere", { ENG_SUPER, 0.32f, 0.30f, 0.350f, 1.00f, 0.90f, 1.30f, 0.18f, 0.40f, 0.30f, 0.25f, 0.10f, 0.55f, PERF_CHORD,   0.30f, 0.80f, 0.70f }, SPREAD_OPEN },
    { "Respiro di vetro",   "Atmosfere", { ENG_FM,    0.28f, 0.00f, 0.700f, 2.50f, 0.50f, 2.00f, 0.12f, 0.20f, 0.30f, 0.40f, 0.25f, 0.55f, PERF_SLOP,    0.40f, 0.80f, 0.85f }, SPREAD_WIDE },
    { "Tramonto FM",        "Tastiere",  { ENG_FM,    0.30f, 0.00f, 0.003f, 2.20f, 0.00f, 0.60f, 0.15f, 0.45f, 0.30f, 0.45f, 0.15f, 0.35f, PERF_CHORD,   0.30f, 0.80f, 0.00f }, SPREAD_CLOSE },
    { "Wurli di notte",     "Tastiere",  { ENG_REED,  0.45f, 0.50f, 0.003f, 1.60f, 0.30f, 0.35f, 0.35f, 0.40f, 0.30f, 0.15f, 0.10f, 0.30f, PERF_SLOP,    0.35f, 0.80f, 0.00f }, SPREAD_CLOSE },
    { "Valigia stereo",     "Tastiere",  { ENG_REED,  0.40f, 0.35f, 0.003f, 2.00f, 0.35f, 0.45f, 0.55f, 0.40f, 0.30f, 0.10f, 0.10f, 0.35f, PERF_CHORD,   0.30f, 0.80f, 1.00f }, SPREAD_CLOSE },
    { "Organo della pieve", "Tastiere",  { ENG_ORGAN, 0.40f, 0.50f, 0.006f, 0.50f, 1.00f, 0.08f, 0.35f, 0.50f, 0.30f, 0.60f, 0.00f, 0.35f, PERF_CHORD,   0.30f, 0.75f, 0.00f }, SPREAD_CLOSE },
    { "Organo rotante",     "Tastiere",  { ENG_ORGAN, 0.55f, 0.70f, 0.006f, 0.50f, 1.00f, 0.10f, 0.45f, 0.50f, 0.40f, 0.75f, 0.00f, 0.30f, PERF_CHORD,   0.30f, 0.72f, 0.80f }, SPREAD_CLOSE },
    { "Chitarra di carta",  "Corde",     { ENG_VA,    0.35f, 0.10f, 0.002f, 0.70f, 0.00f, 0.30f, 0.00f, 0.40f, 0.30f, 0.20f, 0.10f, 0.30f, PERF_STRUM,   0.30f, 0.85f, 0.00f }, SPREAD_CLOSE },
    { "Dodici corde",       "Corde",     { ENG_PLUCK, 0.65f, 0.65f, 0.002f, 1.00f, 1.00f, 0.60f, 0.00f, 0.40f, 0.35f, 0.50f, 0.10f, 0.35f, PERF_STRUM,   0.25f, 0.85f, 0.75f }, SPREAD_OPEN },
    { "Koto del lago",      "Corde",     { ENG_PLUCK, 0.75f, 0.55f, 0.002f, 1.00f, 1.00f, 0.60f, 0.10f, 0.00f, 0.30f, 0.00f, 0.30f, 0.50f, PERF_SLOP,    0.50f, 0.85f, 0.60f }, SPREAD_CLOSE },
    { "Arpa celtica",       "Corde",     { ENG_PLUCK, 0.55f, 0.80f, 0.002f, 1.00f, 1.00f, 1.50f, 0.00f, 0.00f, 0.30f, 0.15f, 0.15f, 0.60f, PERF_HARP,    0.30f, 0.85f, 1.00f }, SPREAD_CLOSE },
    { "Arpa di vetro",      "Corde",     { ENG_FM,    0.50f, 0.30f, 0.002f, 1.80f, 0.00f, 1.20f, 0.05f, 0.00f, 0.30f, 0.30f, 0.35f, 0.55f, PERF_HARP,    0.35f, 0.80f, 0.00f }, SPREAD_CLOSE },
    { "Pizzicato",          "Corde",     { ENG_PLUCK, 0.35f, 0.15f, 0.002f, 1.00f, 1.00f, 0.15f, 0.00f, 0.45f, 0.30f, 0.10f, 0.05f, 0.40f, PERF_CHORD,   0.30f, 0.85f, 0.70f }, SPREAD_OPEN },
    { "Cascata",            "Arpeggi",   { ENG_VA,    0.60f, 0.20f, 0.002f, 0.40f, 0.40f, 0.25f, 0.00f, 0.35f, 0.40f, 0.30f, 0.45f, 0.40f, PERF_ARP,     0.65f, 0.80f, 0.00f }, SPREAD_CLOSE },
    { "Pioggia",            "Arpeggi",   { ENG_PLUCK, 0.60f, 0.55f, 0.002f, 1.00f, 1.00f, 0.50f, 0.00f, 0.30f, 0.30f, 0.20f, 0.45f, 0.50f, PERF_ARP,     0.65f, 0.85f, 1.00f }, SPREAD_CLOSE },
    { "Ritmo in levare",    "Arpeggi",   { ENG_VA,    0.55f, 0.40f, 0.002f, 0.30f, 0.50f, 0.12f, 0.00f, 0.60f, 0.50f, 0.30f, 0.25f, 0.25f, PERF_PATTERN, 0.30f, 0.80f, 0.00f }, SPREAD_CLOSE },
    { "Treno notturno",     "Arpeggi",   { ENG_SUPER, 0.45f, 0.35f, 0.002f, 0.25f, 0.30f, 0.12f, 0.00f, 0.60f, 0.50f, 0.20f, 0.30f, 0.25f, PERF_PATTERN, 0.55f, 0.80f, 0.70f }, SPREAD_CLOSE },
    { "Campane lunari",     "Campane",   { ENG_FM,    0.70f, 0.75f, 0.002f, 3.00f, 0.00f, 2.50f, 0.00f, 0.00f, 0.30f, 0.20f, 0.40f, 0.70f, PERF_STRUM,   0.40f, 0.75f, 0.00f }, SPREAD_WIDE },
    { "Carillon",           "Campane",   { ENG_FM,    0.55f, 0.79f, 0.002f, 1.50f, 0.00f, 1.20f, 0.00f, 0.00f, 0.30f, 0.10f, 0.35f, 0.55f, PERF_ARP,     0.30f, 0.75f, 0.90f }, SPREAD_CLOSE },
    { "Ottoni",             "Ottoni",    { ENG_VA,    0.30f, 0.30f, 0.050f, 0.60f, 0.75f, 0.25f, 0.12f, 0.50f, 0.40f, 0.20f, 0.10f, 0.35f, PERF_STRUM,   0.05f, 0.80f, 0.00f }, SPREAD_OPEN },
    { "Fanfara",            "Ottoni",    { ENG_SUPER, 0.38f, 0.22f, 0.060f, 0.50f, 0.80f, 0.25f, 0.12f, 0.50f, 0.40f, 0.15f, 0.10f, 0.35f, PERF_STRUM,   0.05f, 0.78f, 0.50f }, SPREAD_OPEN },
};
#define PRESET_COUNT ((int)(sizeof(PRESETS) / sizeof(PRESETS[0])))
#define USER_CAT "Tuoi"
/* L'ordine delle versioni fino all'11/10/2026: gli stati salvati allora hanno solo il numero del suono. */
static const char *OLD_ORDER[12] = {
    "Lungolago", "Tramonto FM", "Wurli di notte", "Arpa di vetro", "Cascata", "Psichedelia",
    "Organo della pieve", "Ottoni", "Ritmo in levare", "Campane lunari", "Chitarra di carta", "Nebbia",
};

static const char *ENGINE_NAMES[ENG_COUNT] = { "Analogico", "FM", "Piano elettrico", "Organo", "Corda pizzicata", "Ensemble" };
static const char *PERFORM_NAMES[PERF_COUNT] = { "Accordo", "Strum", "Slop", "Arpeggio", "Pattern", "Arpa" };
static const char *BEAT_NAMES[BEAT_COUNT] = { "Disco", "Psichedelico", "Trap", "Bossa nova" };
static const char *SPREAD_NAMES[SPREAD_COUNT] = { "Stretta", "Aperta", "Ampia" };
static const char *KNOB_MODE_NAMES[3] = { "Assolute", "Relative", "Relative (64)" };
static const char *ARP_RATES[5] = { "1/4", "1/8", "1/8 terzina", "1/16", "1/32" };
static const char *ARP_SHORT[5] = { "1/4", "1/8", "1/8T", "1/16", "1/32" };
static const int LOOP_BARS[4] = { 1, 2, 4, 8 };
static const float QUANT_BEATS[4] = { 0.0f, 0.25f, 0.5f, 1.0f };
static const char *QUANT_NAMES[4] = { "No", "1/16", "1/8", "1/4" };

static int ieq(const char *x, const char *y);
static char *trim(char *s);

/* ------------------------------------------------------------------ MIDI */
enum { MAP_NONE, MAP_NOTE, MAP_CC };
typedef struct { int kind, port, ch, num; } MidiMap;          /* port e ch: -1 = qualsiasi */

enum { T_CHORD = 0, T_FUNC = 8, T_KNOB = 16, T_TRANS = 24, T_COUNT = 29 };
enum { F_BEAT, F_LOOP_REC, F_LOOP_PLAY, F_LOOP_CLEAR, F_KEYMODE, F_PERFORM, F_SOUND_PREV, F_SOUND_NEXT };
enum { K_VOICING, K_BASS, K_AMOUNT, K_TONE, K_CHORUS, K_DELAY, K_REVERB, K_VOLUME };
enum { X_PLAY, X_RECORD, X_OVERDUB, X_LOOP, X_UNDO };

static const char *TARGET_NAMES[T_COUNT] = {
    "Dim", "Min", "Maj", "Sus", "6", "m7", "M7", "9",
    "Batteria", "Loop: registra", "Loop: suona o ferma", "Loop: annulla",
    "Modo tonalità", "Esecuzione successiva", "Suono precedente", "Suono successivo",
    "K1 Voicing", "K2 Basso", "K3 Esecuzione", "K4 Tono", "K5 Chorus", "K6 Delay", "K7 Riverbero", "K8 Volume",
    "Play/Stop", "Record", "Overdub", "Loop", "Undo",
};
static const char *CHORD_PAD_NAMES[8] = { "Dim", "Min", "Maj", "Sus", "6", "m7", "M7", "9" };
/* A cosa serve ogni funzione, per il modello che configura una tastiera nuova. */
static const char *TARGET_DESCR[T_COUNT] = {
    "chord pad 1 (top row): diminished quality, held while playing a key", "chord pad 2 (top row): minor quality",
    "chord pad 3 (top row): major quality", "chord pad 4 (top row): sus4 quality",
    "chord pad 5 (bottom row): adds the 6th", "chord pad 6 (bottom row): adds the minor 7th",
    "chord pad 7 (bottom row): adds the major 7th", "chord pad 8 (bottom row): adds the 9th",
    "function pad: drums on/off", "function pad: loop record/overdub", "function pad: loop play/stop",
    "function pad: loop undo", "function pad: key mode on/off", "function pad: next play mode",
    "function pad: previous sound", "function pad: next sound",
    "knob K1: chord register (voicing)", "knob K2: bass level", "knob K3: play mode amount", "knob K4: tone",
    "knob K5: chorus", "knob K6: delay", "knob K7: reverb", "knob K8: volume",
    "transport button: play/stop", "transport button: record", "transport button: overdub",
    "transport button: loop on/off", "transport button: undo",
};

enum { ROW_HEADER, ROW_TARGET, ROW_KNOB_MODE, ROW_AI };
typedef struct { int kind, first, last; const char *label; } MidiRow;
#define MIDI_ROW_COUNT 35
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
    MIDI_ROWS[r++] = (MidiRow){ ROW_AI, 0, 0, "Configura la tastiera con l'IA" };
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
enum { P_SOUND, P_SAVE_NEW, P_OVERWRITE, P_RESTORE, P_RANDOM, P_RENAME, P_DELETE,
       P_KEY, P_SCALE, P_KEYMODE, P_VOICING, P_SPREAD, P_PERFORM, P_AMOUNT, P_FOLLOW,
       P_ENGINE, P_TONE, P_CHAR, P_ATTACK, P_DECAY, P_SUSTAIN, P_RELEASE, P_VIBRATO,
       P_BASS, P_BASS_TONE, P_STEREO, P_CHORUS, P_DELAY, P_REVERB, P_VOLUME, P_COUNT };
static const char *PARAM_NAMES[P_COUNT] = {
    "Suono", "Salva come nuovo suono", "Aggiorna il tuo suono", "Ripristina", "Variazione casuale", "Rinomina", "Elimina",
    "Tonalità", "Scala", "Modo tonalità", "Voicing", "Apertura", "Esecuzione", "Quantità", "Cambia con il suono",
    "Motore", "Tono", "Carattere", "Attacco", "Decadimento", "Sostegno", "Rilascio", "Vibrato",
    "Livello", "Tono", "Ampiezza stereo", "Chorus", "Delay", "Riverbero", "Volume",
};
#define HDR(i) (-1 - (i))
static const char *const CHORD_HEADERS[3] = { "Tonalità", "Voicing", "Esecuzione" };
static const int CHORD_ROWS[] = {
    HDR(0), P_KEY, P_SCALE, P_KEYMODE,
    HDR(1), P_VOICING, P_SPREAD,
    HDR(2), P_PERFORM, P_AMOUNT, P_FOLLOW,
};
static const char *const SOUND_HEADERS[4] = { "Suono", "Timbro", "Basso", "Effetti" };
static const int SOUND_ROWS[] = {
    HDR(0), P_SOUND, P_SAVE_NEW, P_OVERWRITE, P_RESTORE, P_RANDOM, P_RENAME, P_DELETE,
    HDR(1), P_ENGINE, P_TONE, P_CHAR, P_ATTACK, P_DECAY, P_SUSTAIN, P_RELEASE, P_VIBRATO,
    HDR(2), P_BASS, P_BASS_TONE,
    HDR(3), P_STEREO, P_CHORUS, P_DELAY, P_REVERB, P_VOLUME,
};
#define CHORD_ROW_COUNT ((int)(sizeof(CHORD_ROWS) / sizeof(CHORD_ROWS[0])))
#define SOUND_ROW_COUNT ((int)(sizeof(SOUND_ROWS) / sizeof(SOUND_ROWS[0])))
static int is_action(int p) { return p >= P_SAVE_NEW && p <= P_DELETE; }

enum { R_BEAT, R_STYLE, R_LEVEL, R_TEMPO, R_CLOCK, R_BARS, R_QUANT, R_COUNT };
static const char *RHYTHM_NAMES[R_COUNT] = {
    "Batteria", "Stile", "Livello", "Tempo", "Segui il clock MIDI", "Lunghezza massima del loop", "Quantizzazione del loop",
};

/* ------------------------------------------------------------------ stato */
typedef struct {
    OrcSound snd;
    int sound, key, minor, keymode, spread;
    int follow;                                   /* 1: esecuzione, quantita' e apertura cambiano con il suono */
    char sound_name[40];
    float voicing;
    float bpm;
    int beat_on, beat_style;
    float beat_level;
    int clock_follow, loop_bars, loop_quant, knob_mode;
    MidiMap map[T_COUNT];
    char map_device[64];                          /* la tastiera per cui e' fatta la mappa */
} Settings;

#define USER_MAX 64
typedef struct { char name[40]; char path[600]; OrcSound s; int spread; } UserSound;

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
    float knob_t[8];                              /* manopola appena girata: si accende nella pagina Suona */

    /* pagine a elenco */
    int sound_sel, sound_scroll, chords_sel, chords_scroll, rhythm_sel, rhythm_scroll;
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
    float scope[1200];
    int kbd_next;                                 /* prossimo preset della tastiera (Y nella pagina MIDI) */

    /* suoni */
    UserSound user[USER_MAX];
    int user_n;
    OrcSound base;                                /* il suono com'e' salvato: per l'asterisco e Ripristina */
    int base_spread, base_ok;
    uint32_t rng;
    int del_armed;                                /* Elimina premuto una volta */
    float del_t;
    int editing;                                  /* Rinomina: si scrive con una tastiera USB */
    char edit[40];

    /* SELECT tenuto: elenco dei suoni e funzioni; peek: l'elenco in vista per un attimo */
    int sel_down, sel_used, overlay;
    float sel_time, peek_t;

    /* configurazione della tastiera con l'IA */
    int ai;                                       /* AI_OFF, AI_ASK, AI_LISTEN, AI_WAIT, AI_SHOW, AI_ERROR */
    IaScan scan;
    IaMap ai_map[T_COUNT];
    int ai_knob_mode, ai_sel, ai_scroll, ai_count, ai_try;
    char ai_dev[64], ai_long[128], ai_name[64], ai_note[400], ai_err[300], ai_model[96], ai_last[96], ai_skip[64];
    char ai_dir[512];                             /* Saves/claude: fornitori e chiavi di Chiedi all'IA */
    char *ai_fake;                                /* risposta finta (prove) */
    float ai_time;
    Registry reg;
    int reg_loaded;
    Transfer xfer;
    Reply reply;
};
enum { AI_OFF, AI_ASK, AI_LISTEN, AI_WAIT, AI_SHOW, AI_ERROR };
static void ai_device_connected(OrcApp *a, const char *device);

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

/* ------------------------------------------------------------------ suoni: elenco */
static int sound_count(const OrcApp *a) { return PRESET_COUNT + a->user_n; }
static int sound_is_user(const OrcApp *a, int i) { return i >= PRESET_COUNT && i < sound_count(a); }

static const char *sound_name(const OrcApp *a, int i)
{
    if (i >= 0 && i < PRESET_COUNT) return PRESETS[i].name;
    if (sound_is_user(a, i)) return a->user[i - PRESET_COUNT].name;
    return "";
}

static const char *sound_cat(const OrcApp *a, int i) { return i >= 0 && i < PRESET_COUNT ? PRESETS[i].cat : USER_CAT; }

static void sound_values(const OrcApp *a, int i, OrcSound *s, int *spread)
{
    if (sound_is_user(a, i)) { *s = a->user[i - PRESET_COUNT].s; *spread = a->user[i - PRESET_COUNT].spread; return; }
    if (i < 0 || i >= PRESET_COUNT) i = 0;
    *s = PRESETS[i].s;
    *spread = PRESETS[i].spread;
}

static int sound_find(const OrcApp *a, const char *name)
{
    for (int i = 0; i < sound_count(a); i++) if (ieq(sound_name(a, i), name)) return i;
    return -1;
}

static int feq(float x, float y) { return fabsf(x - y) < 1e-4f; }

/* Cambiato rispetto a com'e' salvato? Il volume non conta: resta quello scelto da un suono all'altro. */
static int sound_modified(const OrcApp *a)
{
    if (!a->base_ok) return 1;
    const OrcSound *s = &a->set.snd, *b = &a->base;
    if (s->engine != b->engine || !feq(s->tone, b->tone) || !feq(s->character, b->character) || !feq(s->attack, b->attack) ||
        !feq(s->decay, b->decay) || !feq(s->sustain, b->sustain) || !feq(s->release, b->release) ||
        !feq(s->vibrato, b->vibrato) || !feq(s->bass_level, b->bass_level) || !feq(s->bass_tone, b->bass_tone) ||
        !feq(s->chorus, b->chorus) || !feq(s->delay, b->delay) || !feq(s->reverb, b->reverb) || !feq(s->stereo, b->stereo))
        return 1;
    return a->set.follow && (s->perform != b->perform || !feq(s->perform_amount, b->perform_amount) || a->set.spread != a->base_spread);
}

/* Nome del suono corrente, con l'asterisco se e' stato cambiato. */
static void sound_label(const OrcApp *a, char *out, size_t n)
{
    snprintf(out, n, "%.39s%s", a->set.sound_name, sound_modified(a) ? "*" : "");
}

static void load_sound(OrcApp *a, int i)
{
    int n = sound_count(a), spread;
    i = ((i % n) + n) % n;
    OrcSound s, cur = a->set.snd;
    sound_values(a, i, &s, &spread);
    a->base = s;
    a->base_spread = spread;
    a->base_ok = 1;
    s.volume = cur.volume;                        /* il volume resta quello scelto */
    if (!a->set.follow) { s.perform = cur.perform; s.perform_amount = cur.perform_amount; spread = a->set.spread; }
    a->set.snd = s;
    a->set.spread = spread;
    a->set.sound = i;
    snprintf(a->set.sound_name, sizeof(a->set.sound_name), "%s", sound_name(a, i));
    a->del_armed = 0;
    sound_changed(a);
    modifiers_changed(a);
}

/* Suono precedente o successivo; peek = l'elenco si fa vedere per un attimo (MPK, program change). */
static void sound_step(OrcApp *a, int dir, int peek)
{
    load_sound(a, a->set.sound + dir);
    if (peek) a->peek_t = 1.6f;
}

/* Primo suono della categoria successiva, o di quella corrente (precedente se ci si e' gia'). */
static void sound_category_step(OrcApp *a, int dir)
{
    int n = sound_count(a), i = a->set.sound, j = i;
    const char *cat = sound_cat(a, i);
    if (dir > 0) {
        do { j = (j + 1) % n; } while (j != i && !strcmp(sound_cat(a, j), cat));
    } else {
        while (j > 0 && !strcmp(sound_cat(a, j - 1), cat)) j--;
        if (j == i) {
            j = (i - 1 + n) % n;
            const char *pc = sound_cat(a, j);
            while (j > 0 && !strcmp(sound_cat(a, j - 1), pc)) j--;
        }
    }
    load_sound(a, j);
}

/* ------------------------------------------------------------------ funzioni */
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

/* Annulla: la sovraincisione in corso, poi le precedenti, infine tutto il giro. */
static void loop_undo(OrcApp *a)
{
    OrcLoopInfo li;
    orc_loop_info(a->orc, &li);
    orc_loop_undo(a->orc);
    if (li.state == LOOP_OVERDUB) toast(a, "Sovraincisione annullata");
    else if ((li.state == LOOP_PLAYING || li.state == LOOP_STOPPED) && li.layers > 0) toast(a, "Tolta l'ultima sovraincisione");
    else if (li.state == LOOP_EMPTY) toast(a, "Il loop è già vuoto");
    else toast(a, "Loop cancellato");
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
    case F_LOOP_CLEAR: loop_undo(a); break;
    case F_KEYMODE: keymode_toggle(a); break;
    case F_PERFORM: perform_next(a); break;
    case F_SOUND_PREV: sound_step(a, -1, 1); break;
    case F_SOUND_NEXT: sound_step(a, 1, 1); break;
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
    a->knob_t[k] = 1.2f;
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
    case X_UNDO: loop_undo(a); break;
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
        m[T_KNOB + i] = (MidiMap){ MAP_CC, -1, -1, 24 + i };   /* da qualunque porta: la MPK le manda sulla 2 */
    }
    /* trasporto: CC sulla porta DAW, come li leggono gli script della community (Overdub non noto) */
    m[T_TRANS + X_PLAY] = (MidiMap){ MAP_CC, 1, -1, 76 };
    m[T_TRANS + X_RECORD] = (MidiMap){ MAP_CC, 1, -1, 77 };
    m[T_TRANS + X_LOOP] = (MidiMap){ MAP_CC, 1, -1, 74 };
    m[T_TRANS + X_UNDO] = (MidiMap){ MAP_CC, 1, -1, 73 };
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
    if (a->ai == AI_LISTEN) {                     /* configurazione con l'IA: si ascolta e basta */
        ia_scan_add(&a->scan, port, type, ch, d1, d2);
        snprintf(a->ai_last, sizeof(a->ai_last), "%s", a->last_msg);
        return;
    }
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
        load_sound(a, d1 % sound_count(a));
        a->peek_t = 1.6f;
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
        ai_device_connected(a, device);
    } else if (was) {
        if (a->ai == AI_LISTEN || a->ai == AI_ASK) a->ai = AI_OFF;
        panic(a);
        memset(a->pad_held, 0, sizeof(a->pad_held));
        toast(a, "Tastiera MIDI scollegata");
    }
}

/* ------------------------------------------------------------------ preset della tastiera */
/* File di testo leggibili, una riga per controllo con i nomi della pagina MIDI:
 *   nome = Akai MPK mini IV
 *   manopole = Relative
 *   Maj = nota 42 canale 10 porta 1
 *   K1 Voicing = cc 24
 *   Play/Stop = -
 * Si cercano in Saves/openorc/tastiere (quelli dell'utente, che vincono a parita' di nome) e nella
 * cartella tastiere dell'app; Y nella pagina MIDI li carica uno dopo l'altro. */
#define KBD_MAX 32
typedef struct { char name[64]; char path[600]; } KbdFile;

static int lower(int ch) { return ch >= 'A' && ch <= 'Z' ? ch + 32 : ch; }

static int ieq(const char *x, const char *y)
{
    while (*x && lower((unsigned char)*x) == lower((unsigned char)*y)) { x++; y++; }
    return !*x && !*y;
}

static int icmp(const void *p, const void *q)
{
    const char *x = ((const KbdFile *)p)->name, *y = ((const KbdFile *)q)->name;
    while (*x && lower((unsigned char)*x) == lower((unsigned char)*y)) { x++; y++; }
    return lower((unsigned char)*x) - lower((unsigned char)*y);
}

static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
    return s;
}

static int parse_map(const char *v, MidiMap *out)
{
    MidiMap m = { MAP_NONE, -1, -1, 0 };
    char w[16];
    int num, used = 0;
    if (!*v || !strcmp(v, "-")) { *out = m; return 1; }
    if (sscanf(v, "%15s %d%n", w, &num, &used) != 2 || num < 0 || num > 127) return 0;
    if (ieq(w, "nota") || ieq(w, "note")) m.kind = MAP_NOTE;
    else if (ieq(w, "cc")) m.kind = MAP_CC;
    else return 0;
    m.num = num;
    v += used;
    while (sscanf(v, "%15s %d%n", w, &num, &used) == 2) {
        if ((ieq(w, "canale") || ieq(w, "channel")) && num >= 1 && num <= 16) m.ch = num - 1;
        else if ((ieq(w, "porta") || ieq(w, "port")) && num >= 1 && num <= 8) m.port = num - 1;
        else return 0;
        v += used;
    }
    *out = m;
    return 1;
}

static int user_kbd_dir(const OrcApp *a, char *out, size_t n)
{
    const char *slash = strrchr(a->path, '/');
    if (!slash) return 0;
    snprintf(out, n, "%.*s/tastiere", (int)(slash - a->path), a->path);
    return 1;
}

static int kbd_list(const OrcApp *a, KbdFile *out)
{
    char dirs[2][512];
    int nd = 0, n = 0;
    if (user_kbd_dir(a, dirs[nd], sizeof(dirs[nd]))) nd++;
    snprintf(dirs[nd++], sizeof(dirs[0]), "tastiere");
    for (int d = 0; d < nd; d++) {
        DIR *dp = opendir(dirs[d]);
        if (!dp) continue;
        struct dirent *e;
        while ((e = readdir(dp)) && n < KBD_MAX) {
            size_t l = strlen(e->d_name);
            if (l < 5 || l >= sizeof(out[0].name) || !ieq(e->d_name + l - 4, ".txt")) continue;
            int dup = 0;
            for (int i = 0; i < n; i++) dup |= ieq(out[i].name, e->d_name);
            if (dup) continue;
            snprintf(out[n].name, sizeof(out[n].name), "%s", e->d_name);
            snprintf(out[n].path, sizeof(out[n].path), "%s/%s", dirs[d], e->d_name);
            n++;
        }
        closedir(dp);
    }
    qsort(out, (size_t)n, sizeof(KbdFile), icmp);
    return n;
}

void orcapp_set_knob_mode(OrcApp *a, int mode)
{
    a->set.knob_mode = mode < 0 ? 0 : mode > 2 ? 2 : mode;
    a->dirty = 1;
}

int orcapp_load_keyboard(OrcApp *a, const char *path, char *name, size_t name_n)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    MidiMap m[T_COUNT];
    for (int t = 0; t < T_COUNT; t++) m[t] = (MidiMap){ MAP_NONE, -1, -1, 0 };
    int mode = a->set.knob_mode, found = 0;
    char line[256], nm[64] = "", dev[64] = "";
    while (fgets(line, sizeof(line), f)) {
        char *s = line;
        if (!strncmp(s, "\xEF\xBB\xBF", 3)) s += 3;           /* file salvati con il Blocco note */
        s = trim(s);
        char *eq = strchr(s, '=');
        if (!*s || *s == '#' || !eq) continue;
        *eq = 0;
        char *k = trim(s), *v = trim(eq + 1);
        if (ieq(k, "nome")) snprintf(nm, sizeof(nm), "%s", v);
        else if (ieq(k, "dispositivo")) snprintf(dev, sizeof(dev), "%.63s", v);
        else if (ieq(k, "manopole")) { for (int i = 0; i < 3; i++) if (ieq(v, KNOB_MODE_NAMES[i])) mode = i; }
        else {
            int t = 0;                            /* prima il nome esatto: m7 e M7 sono diversi */
            while (t < T_COUNT && strcmp(k, TARGET_NAMES[t])) t++;
            if (t == T_COUNT) for (t = 0; t < T_COUNT && !ieq(k, TARGET_NAMES[t]); t++) {}
            if (t == T_COUNT && ieq(k, "Loop: cancella")) t = T_FUNC + F_LOOP_CLEAR;     /* nome fino all'11/10/2026 */
            if (t < T_COUNT) found += parse_map(v, &m[t]);
        }
    }
    fclose(f);
    if (!found) return -1;
    memcpy(a->set.map, m, sizeof(m));
    a->set.knob_mode = mode;
    snprintf(a->set.map_device, sizeof(a->set.map_device), "%s", dev[0] ? dev : a->midi_name);
    a->learn = -1;
    a->dirty = 1;
    if (name) {
        if (!nm[0]) {
            const char *b = strrchr(path, '/');
            snprintf(nm, sizeof(nm), "%s", b ? b + 1 : path);
            char *dot = strrchr(nm, '.');
            if (dot) *dot = 0;
        }
        snprintf(name, name_n, "%s", nm);
    }
    return 0;
}

int orcapp_save_keyboard(OrcApp *a, const char *path, const char *name)
{
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "# OpenOrc - preset della tastiera MIDI (Y nella pagina MIDI lo carica)\n"
               "# Ogni riga: controllo = nota N | cc N, poi canale 1-16 e porta 1-8 se servono; - = nessuno\n"
               "nome = %s\n", name);
    if (a->set.map_device[0]) fprintf(f, "dispositivo = %s\n", a->set.map_device);
    fprintf(f, "manopole = %s\n", KNOB_MODE_NAMES[a->set.knob_mode]);
    for (int t = 0; t < T_COUNT; t++) {
        const MidiMap *m = &a->set.map[t];
        fprintf(f, "%s = ", TARGET_NAMES[t]);
        if (m->kind == MAP_NONE) { fprintf(f, "-\n"); continue; }
        fprintf(f, "%s %d", m->kind == MAP_NOTE ? "nota" : "cc", m->num);
        if (m->ch >= 0) fprintf(f, " canale %d", m->ch + 1);
        if (m->port >= 0) fprintf(f, " porta %d", m->port + 1);
        fprintf(f, "\n");
    }
    return fclose(f) ? -1 : 0;
}

static void kbd_next(OrcApp *a)
{
    static KbdFile files[KBD_MAX];
    int n = kbd_list(a, files);
    char name[64], t[112];
    a->learn = -1;
    for (int tries = 0; tries < n; tries++) {
        const KbdFile *k = &files[a->kbd_next++ % n];
        if (orcapp_load_keyboard(a, k->path, name, sizeof(name))) continue;
        if (n > 1) snprintf(t, sizeof(t), "Tastiera: %s (Y: la prossima)", name);
        else snprintf(t, sizeof(t), "Tastiera: %s", name);
        toast(a, t);
        return;
    }
    default_map(a->set.map);
    snprintf(a->set.map_device, sizeof(a->set.map_device), "MPK mini IV");
    a->dirty = 1;
    toast(a, "Pad e manopole come nella MPK mini IV");
}

static void kbd_save(OrcApp *a)
{
    char dir[512], path[600];
    if (!user_kbd_dir(a, dir, sizeof(dir))) return;
    make_dir(dir);
    snprintf(path, sizeof(path), "%s/mia-tastiera.txt", dir);
    if (orcapp_save_keyboard(a, path, "La mia tastiera")) toast(a, "Non riesco a salvare il preset");
    else toast(a, "Salvato in Saves/openorc/tastiere/mia-tastiera.txt");
}

/* ------------------------------------------------------------------ salvataggio */
static void settings_default(Settings *s)
{
    memset(s, 0, sizeof(*s));
    s->sound = 0;
    s->snd = PRESETS[0].s;
    s->spread = PRESETS[0].spread;
    snprintf(s->sound_name, sizeof(s->sound_name), "%s", PRESETS[0].name);
    s->follow = 1;
    s->key = 0;
    s->voicing = 0.5f;
    s->bpm = 96.0f;
    s->beat_style = BEAT_DISCO;
    s->beat_level = 0.7f;
    s->clock_follow = 1;
    s->loop_bars = 2;
    s->loop_quant = 2;
    s->knob_mode = 1;                             /* le manopole della MPK mini IV sono relative di fabbrica */
    default_map(s->map);
    snprintf(s->map_device, sizeof(s->map_device), "MPK mini IV");
}

/* I parametri di un suono come righe chiave=valore: li usano il file di stato e i suoni dell'utente. */
static void sound_write(FILE *f, const OrcSound *d, int spread)
{
    fprintf(f, "engine=%d\ntone=%g\ncharacter=%g\nattack=%g\ndecay=%g\nsustain=%g\nrelease=%g\nvibrato=%g\nbass=%g\n"
               "bass_tone=%g\nchorus=%g\ndelay=%g\nreverb=%g\nperform=%d\namount=%g\nvolume=%g\nstereo=%g\nspread=%d\n",
            d->engine, d->tone, d->character, d->attack, d->decay, d->sustain, d->release, d->vibrato, d->bass_level,
            d->bass_tone, d->chorus, d->delay, d->reverb, d->perform, d->perform_amount, d->volume, d->stereo, spread);
}

static int sound_read(OrcSound *d, int *spread, const char *k, float v)
{
    if (!strcmp(k, "engine")) d->engine = (int)clampf(v, 0, ENG_COUNT - 1);
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
    else if (!strcmp(k, "stereo")) d->stereo = clamp01(v);
    else if (!strcmp(k, "spread")) *spread = (int)clampf(v, 0, SPREAD_COUNT - 1);
    else return 0;
    return 1;
}

static void save(OrcApp *a)
{
    if (!a->path[0]) return;
    char tmp[600];
    snprintf(tmp, sizeof(tmp), "%s.tmp", a->path);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    const Settings *s = &a->set;
    fprintf(f, "# OpenOrc\nsound=%d\nsound_name=%s\n", s->sound, s->sound_name);
    sound_write(f, &s->snd, s->spread);
    fprintf(f, "key=%d\nminor=%d\nkeymode=%d\nvoicing=%g\nfollow=%d\nbpm=%g\nbeat=%d\nbeat_style=%d\nbeat_level=%g\n"
               "clock=%d\nloop_bars=%d\nquant=%d\nknob_mode=%d\n",
            s->key, s->minor, s->keymode, s->voicing, s->follow, s->bpm, s->beat_on, s->beat_style, s->beat_level,
            s->clock_follow, s->loop_bars, s->loop_quant, s->knob_mode);
    for (int t = 0; t < T_COUNT; t++)
        fprintf(f, "map%d=%d,%d,%d,%d\n", t, s->map[t].kind, s->map[t].port, s->map[t].ch, s->map[t].num);
    fprintf(f, "map_device=%s\n", s->map_device);
    fclose(f);
#ifdef _WIN32
    remove(a->path);
#endif
    rename(tmp, a->path);
    a->dirty = 0;
}

/* Restituisce 1 se il file aveva il nome del suono (dal 12/10/2026); prima c'era solo il numero. */
static int load(OrcApp *a)
{
    FILE *f = fopen(a->path, "r");
    if (!f) return 0;
    Settings *s = &a->set;
    OrcSound *d = &s->snd;
    char line[160];
    int named = 0;
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
        else if (!strcmp(k, "sound")) s->sound = (int)v;
        else if (!strcmp(k, "sound_name")) {
            char nm[64];
            snprintf(nm, sizeof(nm), "%s", val);
            snprintf(s->sound_name, sizeof(s->sound_name), "%s", trim(nm));
            named = s->sound_name[0] != 0;
        }
        else if (!strcmp(k, "map_device")) {
            char nm[96];
            snprintf(nm, sizeof(nm), "%s", val);
            snprintf(s->map_device, sizeof(s->map_device), "%.63s", trim(nm));
        }
        else if (sound_read(d, &s->spread, k, v)) {}
        else if (!strcmp(k, "key")) s->key = (int)clampf(v, 0, 11);
        else if (!strcmp(k, "minor")) s->minor = v != 0;
        else if (!strcmp(k, "keymode")) s->keymode = v != 0;
        else if (!strcmp(k, "voicing")) s->voicing = clamp01(v);
        else if (!strcmp(k, "follow")) s->follow = v != 0;
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
    return named;
}

/* ------------------------------------------------------------------ suoni dell'utente */
/* Un file di testo per suono in Saves/openorc/suoni: «nome=...» e poi i parametri come nel file di
 * stato. Si possono copiare, condividere e ritoccare con un editor di testo. */
static int user_dir(const OrcApp *a, char *out, size_t n)
{
    const char *slash = strrchr(a->path, '/');
    if (!slash) return 0;
    snprintf(out, n, "%.*s/suoni", (int)(slash - a->path), a->path);
    return 1;
}

static void name_from_file(const char *path, char *out, size_t n)
{
    const char *b = strrchr(path, '/');
    snprintf(out, n, "%.39s", b ? b + 1 : path);
    char *dot = strrchr(out, '.');
    if (dot) *dot = 0;
}

static int user_read(const char *path, UserSound *u)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    u->s = PRESETS[0].s;
    u->spread = PRESETS[0].spread;
    u->name[0] = 0;
    char line[256];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        char *s = line;
        if (!strncmp(s, "\xEF\xBB\xBF", 3)) s += 3;           /* file salvati con il Blocco note */
        s = trim(s);
        char *eq = strchr(s, '=');
        if (!*s || *s == '#' || !eq) continue;
        *eq = 0;
        char *k = trim(s), *v = trim(eq + 1);
        if (ieq(k, "nome") || ieq(k, "name")) snprintf(u->name, sizeof(u->name), "%.39s", v);
        else found += sound_read(&u->s, &u->spread, k, (float)atof(v));
    }
    fclose(f);
    if (!found) return -1;
    if (!u->name[0]) name_from_file(path, u->name, sizeof(u->name));
    snprintf(u->path, sizeof(u->path), "%s", path);
    return 0;
}

static int user_cmp(const void *p, const void *q)
{
    const char *x = ((const UserSound *)p)->name, *y = ((const UserSound *)q)->name;
    while (*x && lower((unsigned char)*x) == lower((unsigned char)*y)) { x++; y++; }
    return lower((unsigned char)*x) - lower((unsigned char)*y);
}

static void users_scan(OrcApp *a)
{
    char dir[512];
    a->user_n = 0;
    if (!user_dir(a, dir, sizeof(dir))) return;
    DIR *dp = opendir(dir);
    if (!dp) return;
    struct dirent *e;
    while ((e = readdir(dp)) && a->user_n < USER_MAX) {
        size_t l = strlen(e->d_name);
        if (l < 5 || !ieq(e->d_name + l - 4, ".txt")) continue;
        char path[600];
        snprintf(path, sizeof(path), "%.511s/%.80s", dir, e->d_name);
        UserSound *u = &a->user[a->user_n];
        if (user_read(path, u)) continue;
        if (sound_find(a, u->name) >= 0) {        /* nome doppio (un file copiato a mano): vale il nome del file */
            name_from_file(path, u->name, sizeof(u->name));
            if (sound_find(a, u->name) >= 0) continue;
        }
        a->user_n++;
    }
    closedir(dp);
    qsort(a->user, (size_t)a->user_n, sizeof(UserSound), user_cmp);
}

/* Nome di file dal nome del suono: «Nebbia 2» -> nebbia-2 */
static void slug(const char *name, char *out, size_t n)
{
    size_t j = 0;
    int dash = 0;
    for (const unsigned char *p = (const unsigned char *)name; *p && j + 2 < n; p++) {
        int c = *p;
        if (c >= 'A' && c <= 'Z') c += 32;
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) { out[j++] = (char)c; dash = 0; }
        else if (!dash && j) { out[j++] = '-'; dash = 1; }
    }
    while (j && out[j - 1] == '-') j--;
    out[j] = 0;
    if (!j) snprintf(out, n, "suono");
}

/* Scrive un suono; old_path: il file che sostituisce (aggiorna, rinomina), che non conta come occupato. */
static int user_write(OrcApp *a, const char *name, const OrcSound *snd, int spread, const char *old_path)
{
    char dir[512], sl[48], path[600], tmp[620];
    if (!user_dir(a, dir, sizeof(dir))) return -1;
    make_dir(dir);
    slug(name, sl, sizeof(sl));
    snprintf(path, sizeof(path), "%s/%s.txt", dir, sl);
    for (int k = 2; k < 1000; k++) {              /* mai sopra il file di un altro suono */
        if (old_path && !strcmp(path, old_path)) break;
        FILE *t = fopen(path, "r");
        if (!t) break;
        fclose(t);
        snprintf(path, sizeof(path), "%s/%s-%d.txt", dir, sl, k);
    }
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return -1;
    fprintf(f, "# OpenOrc - un suono (Saves/openorc/suoni): si puo' copiare e ritoccare\nnome=%s\n", name);
    sound_write(f, snd, spread);
    if (fclose(f)) return -1;
#ifdef _WIN32
    remove(path);
#endif
    if (rename(tmp, path)) return -1;
    if (old_path && old_path[0] && strcmp(old_path, path)) remove(old_path);
    return 0;
}

/* Dopo un salvataggio: si rilegge la cartella e il suono corrente diventa quello salvato. */
static void sound_adopt(OrcApp *a, const char *name)
{
    users_scan(a);
    int i = sound_find(a, name);
    a->set.sound = i >= 0 ? i : 0;
    snprintf(a->set.sound_name, sizeof(a->set.sound_name), "%s", name);
    a->base = a->set.snd;
    a->base_spread = a->set.spread;
    a->base_ok = 1;
    a->del_armed = 0;
    a->dirty = 1;
}

/* Il primo nome libero: «Nebbia» e «Nebbia 2» diventano «Nebbia 2», «Nebbia 3»... */
static void name_next(const OrcApp *a, char *out, size_t n)
{
    char base[40];
    snprintf(base, sizeof(base), "%s", a->set.sound_name[0] ? a->set.sound_name : "Suono");
    size_t l = strlen(base), e = l;
    while (e > 0 && base[e - 1] >= '0' && base[e - 1] <= '9') e--;
    if (e < l && e > 1 && base[e - 1] == ' ') base[e - 1] = 0;
    for (int k = 2; k < 1000; k++) {
        snprintf(out, n, "%.30s %d", base, k);
        if (sound_find(a, out) < 0) return;
    }
}

static void sound_save_new(OrcApp *a)
{
    char name[40], t[112];
    if (!a->path[0]) return;
    if (a->user_n >= USER_MAX) { toast(a, "Troppi suoni tuoi: eliminane uno"); return; }
    name_next(a, name, sizeof(name));
    if (user_write(a, name, &a->set.snd, a->set.spread, NULL)) { toast(a, "Non riesco a salvare il suono"); return; }
    sound_adopt(a, name);
    snprintf(t, sizeof(t), "Salvato come «%s»", name);
    toast(a, t);
}

static void sound_overwrite(OrcApp *a)
{
    char name[40], old[600], t[112];
    if (!sound_is_user(a, a->set.sound)) { toast(a, "Si aggiornano solo i tuoi: usa «Salva come nuovo suono»"); return; }
    const UserSound *u = &a->user[a->set.sound - PRESET_COUNT];
    snprintf(name, sizeof(name), "%s", u->name);
    snprintf(old, sizeof(old), "%s", u->path);
    if (user_write(a, name, &a->set.snd, a->set.spread, old)) { toast(a, "Non riesco a salvare il suono"); return; }
    sound_adopt(a, name);
    snprintf(t, sizeof(t), "«%s» aggiornato", name);
    toast(a, t);
}

static void sound_restore(OrcApp *a)
{
    char t[112];
    if (!a->base_ok) { toast(a, "Non c'è un suono salvato a cui tornare"); return; }
    if (!sound_modified(a)) { toast(a, "Nessuna modifica da togliere"); return; }
    OrcSound s = a->base, cur = a->set.snd;
    s.volume = cur.volume;
    if (!a->set.follow) { s.perform = cur.perform; s.perform_amount = cur.perform_amount; }
    else a->set.spread = a->base_spread;
    a->set.snd = s;
    sound_changed(a);
    modifiers_changed(a);
    snprintf(t, sizeof(t), "Tornato a «%s»", a->set.sound_name);
    toast(a, t);
}

static float rnd(OrcApp *a)                      /* -1..1 */
{
    a->rng ^= a->rng << 13; a->rng ^= a->rng >> 17; a->rng ^= a->rng << 5;
    return (float)(int32_t)a->rng / 2147483648.0f;
}

/* Variazione casuale: piccoli spostamenti di timbro, inviluppo ed effetti; motore ed esecuzione restano. */
static void sound_variation(OrcApp *a)
{
    OrcSound *s = &a->set.snd;
    s->tone = clamp01(s->tone + 0.15f * rnd(a));
    if (s->engine == ENG_FM) {
        if (rnd(a) > -0.2f) s->character = clamp01((floorf(s->character * 6.99f) + (rnd(a) > 0.0f ? 1.0f : -1.0f)) / 6.99f + 0.01f);
    } else {
        s->character = clamp01(s->character + 0.2f * rnd(a));
    }
    s->attack = clampf(s->attack * exp2f(rnd(a)), 0.002f, 8.0f);
    s->decay = clampf(s->decay * exp2f(0.8f * rnd(a)), 0.01f, 8.0f);
    s->release = clampf(s->release * exp2f(0.8f * rnd(a)), 0.01f, 8.0f);
    if (s->engine != ENG_PLUCK) s->sustain = clamp01(s->sustain + 0.15f * rnd(a));
    s->vibrato = clamp01(s->vibrato + 0.1f * rnd(a));
    s->bass_tone = clamp01(s->bass_tone + 0.15f * rnd(a));
    s->chorus = clamp01(s->chorus + 0.15f * rnd(a));
    s->delay = clamp01(s->delay + 0.12f * rnd(a));
    s->reverb = clamp01(s->reverb + 0.15f * rnd(a));
    s->stereo = clamp01(s->stereo + 0.25f * rnd(a));
    sound_changed(a);
    toast(a, "Variazione: A per un'altra, Ripristina per tornare");
}

static void sound_delete(OrcApp *a)
{
    char name[40], t[112];
    if (!sound_is_user(a, a->set.sound)) { toast(a, "Si possono eliminare solo i tuoi suoni"); return; }
    if (!a->del_armed) { a->del_armed = 1; a->del_t = 3.0f; toast(a, "Premi ancora A per eliminarlo"); return; }
    const UserSound *u = &a->user[a->set.sound - PRESET_COUNT];
    snprintf(name, sizeof(name), "%s", u->name);
    a->del_armed = 0;
    if (remove(u->path)) { toast(a, "Non riesco a eliminare il file"); return; }
    int i = a->set.sound;
    users_scan(a);
    load_sound(a, i < sound_count(a) ? i : sound_count(a) - 1);
    snprintf(t, sizeof(t), "«%s» eliminato", name);
    toast(a, t);
}

static void rename_start(OrcApp *a)
{
    if (!sound_is_user(a, a->set.sound)) { toast(a, "Si possono rinominare solo i tuoi suoni"); return; }
    a->editing = 1;
    snprintf(a->edit, sizeof(a->edit), "%s", a->user[a->set.sound - PRESET_COUNT].name);
}

static void rename_finish(OrcApp *a, int ok)
{
    char name[40], old[600], t[112];
    a->editing = 0;
    if (!ok) { toast(a, "Nome lasciato com'era"); return; }
    if (!sound_is_user(a, a->set.sound)) return;
    snprintf(old, sizeof(old), "%s", a->edit);
    snprintf(name, sizeof(name), "%s", trim(old));
    const UserSound *u = &a->user[a->set.sound - PRESET_COUNT];
    if (!name[0]) { toast(a, "Il nome è vuoto: lasciato com'era"); return; }
    if (!strcmp(name, u->name)) return;
    int other = sound_find(a, name);
    if (other >= 0 && other != a->set.sound) { toast(a, "C'è già un suono con questo nome"); return; }
    OrcSound s = u->s;
    int spread = u->spread;
    snprintf(old, sizeof(old), "%s", u->path);
    if (user_write(a, name, &s, spread, old)) { toast(a, "Non riesco a rinominare il suono"); return; }
    users_scan(a);
    int i = sound_find(a, name);
    a->set.sound = i >= 0 ? i : 0;
    snprintf(a->set.sound_name, sizeof(a->set.sound_name), "%s", name);
    a->dirty = 1;
    snprintf(t, sizeof(t), "Ora si chiama «%s»", name);
    toast(a, t);
}

/* Dopo il file di stato: il suono si ritrova per nome (prima c'era solo il numero nell'ordine vecchio).
   Se non c'e' piu' (un suono tuo eliminato a mano) restano i valori, con l'asterisco. */
static void resolve_sound(OrcApp *a, int named)
{
    int i = -1;
    if (named) i = sound_find(a, a->set.sound_name);
    else if (a->set.sound >= 0 && a->set.sound < 12) i = sound_find(a, OLD_ORDER[a->set.sound]);
    if (i >= 0) {
        a->set.sound = i;
        snprintf(a->set.sound_name, sizeof(a->set.sound_name), "%s", sound_name(a, i));
        sound_values(a, i, &a->base, &a->base_spread);
        a->base_ok = 1;
    } else {
        a->set.sound = 0;
        a->base_ok = 0;
    }
}

/* ------------------------------------------------------------------ loop salvato */
static int loop_file(const OrcApp *a, char *out, size_t n)
{
    const char *slash = strrchr(a->path, '/');
    if (!slash) return 0;
    snprintf(out, n, "%.*s/loop.txt", (int)(slash - a->path), a->path);
    return 1;
}

static OrcLoopEvent loop_buf[ORC_LOOP_MAX];

/* Solo ad audio fermo: all'uscita (il kit chiude l'audio prima di destroy). */
static void loop_save(OrcApp *a)
{
    char path[600], tmp[620];
    if (!loop_file(a, path, sizeof(path))) return;
    double len;
    int layers, n = orc_loop_export(a->orc, loop_buf, ORC_LOOP_MAX, &len, &layers);
    if (!n) { remove(path); return; }
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    fprintf(f, "# OpenOrc - il loop dell'ultima sessione: si riprende all'avvio, fermo\n"
               "# ev = battito tipo strato basso/nota tag dinamica note...\nlength=%g\nlayers=%d\n", len, layers);
    for (int i = 0; i < n; i++) {
        const OrcLoopEvent *e = &loop_buf[i];
        fprintf(f, "ev=%g %d %d %d %d %g %d", e->t, e->type, e->pass, e->a, e->tag, e->b, e->n);
        for (int k = 0; k < e->n; k++) fprintf(f, " %d", e->notes[k]);
        fputc('\n', f);
    }
    if (fclose(f)) return;
#ifdef _WIN32
    remove(path);
#endif
    rename(tmp, path);
}

/* Prima che parta l'audio. */
static void loop_load(OrcApp *a)
{
    char path[600], line[256];
    if (!loop_file(a, path, sizeof(path))) return;
    FILE *f = fopen(path, "r");
    if (!f) return;
    double len = 0.0;
    int layers = 0, n = 0;
    while (fgets(line, sizeof(line), f) && n < ORC_LOOP_MAX) {
        if (!strncmp(line, "length=", 7)) len = atof(line + 7);
        else if (!strncmp(line, "layers=", 7)) layers = atoi(line + 7);
        else if (!strncmp(line, "ev=", 3)) {
            OrcLoopEvent *e = &loop_buf[n];
            memset(e, 0, sizeof(*e));
            int used = 0;
            if (sscanf(line + 3, "%f %d %d %d %d %f %d%n", &e->t, &e->type, &e->pass, &e->a, &e->tag, &e->b, &e->n, &used) != 7) continue;
            if (e->n < 0 || e->n > ORC_MAX_CHORD) continue;
            const char *p = line + 3 + used;
            int ok = 1;
            for (int k = 0; k < e->n && ok; k++) ok = sscanf(p, "%d%n", &e->notes[k], &used) == 1, p += used;
            if (ok) n++;
        }
    }
    fclose(f);
    if (n) orc_loop_import(a->orc, loop_buf, n, len, layers);
}

/* ------------------------------------------------------------------ tastiere: ricaricate e configurate con l'IA */
/* Un preset con «dispositivo = <nome>» uguale alla tastiera collegata: si carica. */
static int kbd_for_device(OrcApp *a, const char *device)
{
    static KbdFile files[KBD_MAX];
    int n = kbd_list(a, files);
    for (int i = 0; i < n; i++) {
        FILE *f = fopen(files[i].path, "r");
        if (!f) continue;
        char line[256], dev[96] = "";
        while (fgets(line, sizeof(line), f)) {
            char *s = line;
            if (!strncmp(s, "\xEF\xBB\xBF", 3)) s += 3;
            s = trim(s);
            char *eq = strchr(s, '=');
            if (!eq || *s == '#') continue;
            *eq = 0;
            if (ieq(trim(s), "dispositivo")) { snprintf(dev, sizeof(dev), "%s", trim(eq + 1)); break; }
        }
        fclose(f);
        char name[64];
        if (dev[0] && ia_same_device(dev, device) && !orcapp_load_keyboard(a, files[i].path, name, sizeof(name))) {
            char t[112];
            snprintf(t, sizeof(t), "Tastiera %s: la sua mappa è pronta", name);
            toast(a, t);
            return 1;
        }
    }
    return 0;
}

/* Una tastiera appena collegata: la sua mappa se c'e', altrimenti la proposta di configurarla con l'IA. */
static void ai_device_connected(OrcApp *a, const char *device)
{
    if (!device || !*device || ia_same_device(device, a->set.map_device)) return;
    if (kbd_for_device(a, device)) return;
    if (a->ai == AI_OFF && !ia_same_device(device, a->ai_skip)) {
        snprintf(a->ai_dev, sizeof(a->ai_dev), "%s", device);
        ia_device_longname(device, a->ai_long, sizeof(a->ai_long));
        a->ai = AI_ASK;
    }
}

static void ai_listen(OrcApp *a)
{
    if (!a->midi_name[0]) { toast(a, "Collega prima una tastiera MIDI"); a->ai = AI_OFF; return; }
    panic(a);
    memset(a->pad_held, 0, sizeof(a->pad_held));
    snprintf(a->ai_dev, sizeof(a->ai_dev), "%s", a->midi_name);
    ia_device_longname(a->ai_dev, a->ai_long, sizeof(a->ai_long));
    ia_scan_reset(&a->scan);
    a->ai_last[0] = 0;
    a->ai_err[0] = 0;
    a->learn = -1;
    a->ai = AI_LISTEN;
}

/* Il modello: il migliore gratuito tra quelli con una chiave salvata in Chiedi all'IA; altrimenti quello
   scelto per le funzioni IA della console. Groq con gpt-oss 120B: gratis senza carta, veloce, bravo a
   rispondere in JSON. */
static int ai_pick(OrcApp *a, int from, const Provider **pp, Model *mm, char *key, size_t keyn)
{
    static const struct { const char *prov, *model; } FREE[] = {
        { "groq", "openai/gpt-oss-120b" },
        { "groq", "qwen/qwen3.8-27b" },                /* limiti al minuto separati da gpt-oss */
        { "groq", "openai/gpt-oss-20b" },
        { "openrouter", "nvidia/nemotron-3-ultra-550b-a55b:free" },
        { "zai", "glm-4.7-flash" },
        { "mistral", "mistral-medium-latest" },
    };
    if (!a->reg_loaded) { registry_load(&a->reg, a->ai_dir); a->reg_loaded = 1; }
    for (size_t i = (size_t)from; i < sizeof(FREE) / sizeof(FREE[0]); i++) {
        int pi = registry_find(&a->reg, FREE[i].prov);
        if (pi < 0) continue;
        const Provider *p = &a->reg.p[pi];
        provider_key(p, a->ai_dir, key, keyn);
        if (!key[0]) continue;
        int mi = provider_find_model(p, FREE[i].model);
        if (mi >= 0) *mm = p->models[mi];
        else { memset(mm, 0, sizeof(*mm)); snprintf(mm->id, sizeof(mm->id), "%s", FREE[i].model); snprintf(mm->name, sizeof(mm->name), "%s", FREE[i].model); }
        *pp = p;
        a->ai_try = (int)i;
        return 0;
    }
    if (from > 0) return -1;
    IaSettings st;
    ia_settings_read(a->ai_dir, &st);
    const Model *m = NULL;
    char err[200];
    if (!ia_console_model(&a->reg, &st, a->ai_dir, pp, &m, key, keyn, err, sizeof(err)) && m) { *mm = *m; a->ai_try = 99; return 0; }
    return -1;
}

static void ai_finish(OrcApp *a, const char *text)
{
    int km = -1;
    a->ai_count = ia_parse_reply(text, &a->scan, TARGET_NAMES, T_COUNT, a->ai_map, &km, a->ai_name, sizeof(a->ai_name),
                                 a->ai_note, sizeof(a->ai_note), a->ai_err, sizeof(a->ai_err));
    if (a->ai_count < 0) { a->ai = AI_ERROR; return; }
    int guess = ia_guess_knob_mode(&a->scan, &a->ai_map[T_KNOB], 8);
    a->ai_knob_mode = guess >= 0 ? guess : (km >= 0 ? km : a->set.knob_mode);
    if (!a->ai_name[0]) snprintf(a->ai_name, sizeof(a->ai_name), "%s", a->ai_long[0] ? a->ai_long : a->ai_dev);
    a->ai_sel = 1;
    a->ai_scroll = 0;
    a->ai = AI_SHOW;
}

static void reply_sink(void *ud, const char *d, size_t n) { reply_feed(ud, d, n); }

static void ai_request(OrcApp *a, int from);

static void ai_send(OrcApp *a)
{
    int notes, knobs, buttons, bends;
    ia_scan_counts(&a->scan, &notes, &knobs, &buttons, &bends);
    if (!notes && !knobs && !buttons) { toast(a, "Non è arrivato niente: muovi manopole e pad, poi A"); return; }
    a->ai_err[0] = 0;
    a->ai_time = 0.0f;
    if (a->ai_fake) {                             /* prove: la risposta e' gia' qui */
        snprintf(a->ai_model, sizeof(a->ai_model), "risposta di prova");
        ai_finish(a, a->ai_fake);
        return;
    }
    ai_request(a, 0);
}

/* Domanda al modello candidato numero from (o al primo con una chiave dopo di lui). */
static void ai_request(OrcApp *a, int from)
{
    const Provider *p = NULL;
    Model m;
    char key[256];
    if (ai_pick(a, from, &p, &m, key, sizeof(key))) {
        if (from > 0)
            snprintf(a->ai_err, sizeof(a->ai_err), "I modelli gratuiti hanno raggiunto il limite di richieste al minuto: riprova tra un minuto.");
        else
            snprintf(a->ai_err, sizeof(a->ai_err), "Serve una chiave gratuita. Consigliata: Groq, gratis senza carta, su console.groq.com. "
                     "Salvala in Chiedi all'IA, scheda Fornitori, oppure in Saves/claude/chiavi/groq.txt.");
        a->ai = AI_ERROR;
        return;
    }
    snprintf(a->ai_model, sizeof(a->ai_model), "%s · %s", p->name, m.name[0] ? m.name : m.id);
    char *sys = ia_system_prompt();
    char *user = ia_user_prompt(&a->scan, a->ai_dev, a->ai_long, TARGET_NAMES, TARGET_DESCR, T_COUNT);
    AskSpec q = { sys, user, NULL, NULL, 2500, "low", 0 };
    char url[260];
    char *body = ask_request(p, &m, &q, url, sizeof(url));
    Buf h = { 0 };
    provider_auth_headers(p, key, &h);
    buf_adds(&h, "Content-Type: application/json\n");
    memset(key, 0, sizeof(key));
    reply_init(&a->reply, p->proto == PROTO_ANTHROPIC ? PROTO_ANTHROPIC : PROTO_OPENAI, p->name);
    a->reply.state = RS_WAITING;
    Request rq = { url, h.p, body, 0, NULL, NULL, 120 };
    char msg[200];
    if (net_start(&a->xfer, &rq, msg, sizeof(msg))) {
        snprintf(a->ai_err, sizeof(a->ai_err), "%s", msg);
        reply_free(&a->reply);
        a->ai = AI_ERROR;
    } else {
        a->ai = AI_WAIT;
    }
    if (h.p) memset(h.p, 0, h.len);
    buf_free(&h);
    free(body);
    free(sys);
    free(user);
}

static void ai_poll(OrcApp *a)
{
    if (a->ai != AI_WAIT || !net_active(&a->xfer)) return;
    int code = 0;
    char err[200];
    if (!net_poll(&a->xfer, reply_sink, &a->reply, &code, err, sizeof(err))) return;
    reply_finish(&a->reply, code, err);
    if (a->reply.state != RS_DONE && a->reply.http_status == 429 && a->ai_try < 90) {   /* limite: il prossimo modello */
        reply_free(&a->reply);
        ai_request(a, a->ai_try + 1);
        return;
    }
    char *t = reply_text(&a->reply);
    if (a->reply.state == RS_DONE) ai_finish(a, t);
    else {
        snprintf(a->ai_err, sizeof(a->ai_err), "%s", a->reply.error[0] ? a->reply.error : "La risposta dell'IA si è interrotta.");
        a->ai = AI_ERROR;
    }
    free(t);
    reply_free(&a->reply);
}

static void ai_cancel(OrcApp *a)
{
    if (a->ai == AI_WAIT && net_active(&a->xfer)) { net_cancel(&a->xfer); reply_free(&a->reply); }
    a->ai = AI_OFF;
}

/* La proposta accettata: diventa la mappa e un preset della tastiera, che si ricarica da solo. */
static void ai_apply(OrcApp *a)
{
    for (int t = 0; t < T_COUNT; t++) {
        const IaMap *m = &a->ai_map[t];
        a->set.map[t] = m->kind == IA_NONE ? (MidiMap){ MAP_NONE, -1, -1, 0 }
                                           : (MidiMap){ m->kind == IA_NOTE ? MAP_NOTE : MAP_CC, m->port, m->ch, m->num };
    }
    a->set.knob_mode = a->ai_knob_mode;
    snprintf(a->set.map_device, sizeof(a->set.map_device), "%s", a->ai_dev);
    a->dirty = 1;
    a->ai = AI_OFF;
    char dir[512], sl[48], path[600], t[112];
    if (user_kbd_dir(a, dir, sizeof(dir))) {
        make_dir(dir);
        slug(a->ai_name, sl, sizeof(sl));
        snprintf(path, sizeof(path), "%s/%s.txt", dir, sl);
        if (!orcapp_save_keyboard(a, path, a->ai_name)) {
            snprintf(t, sizeof(t), "%s configurata e salvata", a->ai_name);
            toast(a, t);
            return;
        }
    }
    snprintf(t, sizeof(t), "%s configurata", a->ai_name);
    toast(a, t);
}

static void ai_move(OrcApp *a, int dir)
{
    /* righe della proposta: 4 intestazioni e le funzioni, come nella pagina MIDI (senza la riga dell'IA) */
    int r = a->ai_sel;
    do { r = (r + dir + (MIDI_ROW_COUNT - 1)) % (MIDI_ROW_COUNT - 1); } while (MIDI_ROWS[r + 1].kind != ROW_TARGET);
    a->ai_sel = r;
}

static void ai_press(OrcApp *a, int b)
{
    switch (a->ai) {
    case AI_ASK:
        if (b == PAD_A) ai_listen(a);
        else if (b == PAD_B || b == PAD_MENU) { snprintf(a->ai_skip, sizeof(a->ai_skip), "%s", a->ai_dev); a->ai = AI_OFF; }
        break;
    case AI_LISTEN:
        if (b == PAD_A) ai_send(a);
        else if (b == PAD_B || b == PAD_MENU) { a->ai = AI_OFF; toast(a, "Configurazione annullata"); }
        else if (b == PAD_X) { ia_scan_reset(&a->scan); a->ai_last[0] = 0; }
        break;
    case AI_WAIT:
        if (b == PAD_B || b == PAD_MENU) { ai_cancel(a); toast(a, "Configurazione annullata"); }
        break;
    case AI_SHOW:
        if (b == PAD_A) ai_apply(a);
        else if (b == PAD_B || b == PAD_MENU) { a->ai = AI_OFF; toast(a, "Proposta scartata: la mappa resta com'era"); }
        else if (b == PAD_X) ai_send(a);
        else if (b == PAD_UP) ai_move(a, -1);
        else if (b == PAD_DOWN) ai_move(a, 1);
        break;
    case AI_ERROR:
        if (b == PAD_A) { if (a->scan.n) ai_send(a); else ai_listen(a); }
        else if (b == PAD_B || b == PAD_MENU) a->ai = AI_OFF;
        else if (b == PAD_X) ai_listen(a);
        break;
    }
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
    {   /* Saves/claude accanto a Saves/openorc: fornitori e chiavi di Chiedi all'IA */
        const char *slash = strrchr(a->path, '/');
        const char *up = NULL;
        for (const char *q = a->path; slash && q < slash; q++) if (*q == '/') up = q;
        if (up) snprintf(a->ai_dir, sizeof(a->ai_dir), "%.*s/claude", (int)(up - a->path), a->path);
    }
    settings_default(&a->set);
    int named = load(a);
    users_scan(a);
    resolve_sound(a, named);
    /* stato salvato dalle versioni con le manopole sui CC 70-77: si passa ai valori di fabbrica della MPK */
    int old = 1;
    for (int i = 0; i < 8; i++) {
        const MidiMap *k = &a->set.map[T_KNOB + i];
        old &= k->kind == MAP_CC && k->port == 0 && k->ch < 0 && k->num == 70 + i;
    }
    for (int i = T_TRANS; i < T_COUNT; i++) old &= a->set.map[i].kind == MAP_NONE;
    if (old) {
        MidiMap d[T_COUNT];
        default_map(d);
        memcpy(&a->set.map[T_KNOB], &d[T_KNOB], sizeof(MidiMap) * (T_COUNT - T_KNOB));
    }
    /* stato salvato dalle versioni che aspettavano le manopole sulla porta 1 in modo assoluto: la MPK
       mini IV le manda solo sulla porta 2, in modo relativo (misurato l'11/10/2026) */
    int p1 = 1;
    for (int i = 0; i < 8; i++) {
        const MidiMap *k = &a->set.map[T_KNOB + i];
        p1 &= k->kind == MAP_CC && k->port == 0 && k->ch < 0 && k->num == 24 + i;
    }
    if (p1) {
        for (int i = 0; i < 8; i++) a->set.map[T_KNOB + i].port = -1;
        if (a->set.knob_mode == 0) a->set.knob_mode = 1;
        a->dirty = 1;
    }
    a->set.beat_on = 0;                           /* si parte sempre in silenzio */
    a->learn = -1;
    a->degree = -1;
    a->sound_sel = 1;
    a->chords_sel = 1;
    a->midi_sel = 0;
    a->rng = 0x9E3779B9u;
    orc_set_sound(a->orc, &a->set.snd);
    orc_set_tempo(a->orc, a->set.bpm);
    orc_set_beat(a->orc, 0, a->set.beat_style, a->set.beat_level);
    orc_loop_config(a->orc, LOOP_BARS[a->set.loop_bars], QUANT_BEATS[a->set.loop_quant]);
    loop_load(a);
    a->dirty = 0;
    return a;
}

void orcapp_destroy(OrcApp *a)
{
    if (!a) return;
    save(a);
    loop_save(a);
    if (a->ai == AI_WAIT) ai_cancel(a);
    if (a->reg_loaded) registry_free(&a->reg);
    free(a->ai_fake);
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

int orcapp_page(const OrcApp *a) { return a->page; }
int orcapp_sound_count(const OrcApp *a) { return sound_count(a); }
const char *orcapp_sound_name(const OrcApp *a, int i) { return sound_name(a, i); }
int orcapp_sound_find(const OrcApp *a, const char *name) { return sound_find(a, name); }
int orcapp_current_sound(const OrcApp *a) { return a->set.sound; }
const char *orcapp_current_name(const OrcApp *a) { return a->set.sound_name; }
int orcapp_sound_modified(const OrcApp *a) { return sound_modified(a); }
int orcapp_overlay(const OrcApp *a) { return a->overlay ? 1 : (a->peek_t > 0.0f ? 2 : 0); }
int orcapp_editing(const OrcApp *a) { return a->editing; }
const char *orcapp_toast(const OrcApp *a) { return a->toast_t > 0.0f ? a->toast : ""; }
void orcapp_set_sound(OrcApp *a, const OrcSound *s) { a->set.snd = *s; sound_changed(a); }
void orcapp_set_follow(OrcApp *a, int on) { a->set.follow = on != 0; a->dirty = 1; }
int orcapp_ai_state(const OrcApp *a) { return a->ai; }
void orcapp_ai_start(OrcApp *a) { ai_listen(a); }
void orcapp_set_ai_dir(OrcApp *a, const char *dir) { snprintf(a->ai_dir, sizeof(a->ai_dir), "%s", dir ? dir : ""); }
void orcapp_ai_fake(OrcApp *a, const char *reply) { free(a->ai_fake); a->ai_fake = reply ? strdup(reply) : NULL; }
const char *orcapp_ai_error(const OrcApp *a) { return a->ai_err; }
const char *orcapp_map_device(const OrcApp *a) { return a->set.map_device; }

void orcapp_select(OrcApp *a, int row)
{
    if (a->page == PAGE_SOUND) a->sound_sel = row;
    else if (a->page == PAGE_CHORDS) a->chords_sel = row;
    else if (a->page == PAGE_RHYTHM) a->rhythm_sel = row;
    else if (a->page == PAGE_MIDI) a->midi_sel = row;
}

/* ------------------------------------------------------------------ pagine Accordi e Suono: parametri */
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
    case P_STEREO: return s->stereo;
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
    int e = a->set.snd.engine;
    if (p == P_VIBRATO && e == ENG_REED) return "Tremolo";
    if (p == P_CHAR && e == ENG_FM) return "Rapporto";
    if (p == P_CHAR && e == ENG_ORGAN) return "Registri";
    if (p == P_CHAR && e == ENG_PLUCK) return "Durata";
    if (p == P_CHAR && e == ENG_SUPER) return "Desintonia";
    if (p == P_OVERWRITE && sound_is_user(a, a->set.sound)) return "Aggiorna";
    return PARAM_NAMES[p];
}

/* Le azioni che hanno senso adesso (le altre si vedono spente). */
static int action_ok(const OrcApp *a, int p)
{
    switch (p) {
    case P_OVERWRITE: case P_RENAME: case P_DELETE: return sound_is_user(a, a->set.sound);
    case P_RESTORE: return a->base_ok && sound_modified(a);
    case P_SAVE_NEW: return a->path[0] != 0;
    }
    return 1;
}

static void param_text(const OrcApp *a, int p, char *out, size_t n)
{
    const OrcSound *s = &a->set.snd;
    char nm[48];
    switch (p) {
    case P_SOUND: sound_label(a, out, n); break;
    case P_SAVE_NEW: name_next(a, nm, sizeof(nm)); snprintf(out, n, "«%s»", nm); break;
    case P_OVERWRITE: snprintf(out, n, "%s", sound_is_user(a, a->set.sound) ? "salva le modifiche" : "solo per i tuoi suoni"); break;
    case P_RESTORE:
        if (action_ok(a, p)) snprintf(out, n, "torna a «%.30s»", a->set.sound_name);
        else snprintf(out, n, "nessuna modifica");
        break;
    case P_RANDOM: snprintf(out, n, "premi A"); break;
    case P_RENAME: snprintf(out, n, "%s", sound_is_user(a, a->set.sound) ? "con una tastiera USB" : "solo per i tuoi suoni"); break;
    case P_DELETE:
        if (!sound_is_user(a, a->set.sound)) snprintf(out, n, "solo per i tuoi suoni");
        else snprintf(out, n, "%s", a->del_armed ? "premi ancora A" : "A due volte");
        break;
    case P_KEY: snprintf(out, n, "%s", note_it(a->set.key, key_spelling(a->set.key, a->set.minor))); break;
    case P_SCALE: snprintf(out, n, "%s", a->set.minor ? "Minore" : "Maggiore"); break;
    case P_KEYMODE: snprintf(out, n, "%s", a->set.keymode ? "Attivo" : "Spento"); break;
    case P_SPREAD: snprintf(out, n, "%s", SPREAD_NAMES[a->set.spread]); break;
    case P_PERFORM: snprintf(out, n, "%s", PERFORM_NAMES[s->perform]); break;
    case P_AMOUNT: amount_text(a, out, n); break;
    case P_FOLLOW: snprintf(out, n, "%s", a->set.follow ? "Sì" : "No, restano"); break;
    case P_ENGINE: snprintf(out, n, "%s", ENGINE_NAMES[s->engine]); break;
    case P_CHAR:
        if (s->engine == ENG_FM) {
            static const char *R[7] = { "1", "2", "3", "4", "1/2", "3,5", "7" };
            snprintf(out, n, "%s", R[(int)(clamp01(s->character) * 6.99f)]);
        } else if (s->engine == ENG_PLUCK) {
            seconds(out, n, 0.25f * exp2f(clamp01(s->character) * 5.0f));
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
    if (p != P_DELETE) a->del_armed = 0;
    switch (p) {
    case P_SOUND:                                 /* L1/R1: di categoria in categoria */
        if (big) sound_category_step(a, dir);
        else load_sound(a, a->set.sound + dir);
        return;
    case P_SAVE_NEW: case P_OVERWRITE: case P_RESTORE: case P_RANDOM: case P_RENAME: case P_DELETE:
        return;                                   /* azioni: si fanno con A */
    case P_KEY:
        a->set.key = (a->set.key + dir * (big ? 7 : 1) + 120) % 12;     /* con L1/R1 per quinte */
        a->dirty = 1;
        modifiers_changed(a);
        return;
    case P_SCALE: a->set.minor = !a->set.minor; a->dirty = 1; modifiers_changed(a); return;
    case P_KEYMODE: a->set.keymode = !a->set.keymode; a->dirty = 1; return;
    case P_VOICING: a->set.voicing = clamp01(a->set.voicing + dir * step); a->dirty = 1; modifiers_changed(a); return;
    case P_SPREAD: a->set.spread = (a->set.spread + dir + SPREAD_COUNT) % SPREAD_COUNT; a->dirty = 1; modifiers_changed(a); return;
    case P_FOLLOW: a->set.follow = !a->set.follow; a->dirty = 1; return;
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
    case P_STEREO: s->stereo = clamp01(s->stereo + dir * step); break;
    case P_CHORUS: s->chorus = clamp01(s->chorus + dir * step); break;
    case P_DELAY: s->delay = clamp01(s->delay + dir * step); break;
    case P_REVERB: s->reverb = clamp01(s->reverb + dir * step); break;
    case P_VOLUME: s->volume = clamp01(s->volume + dir * step); break;
    }
    sound_changed(a);
}

static void param_action(OrcApp *a, int p)
{
    if (p != P_DELETE) a->del_armed = 0;
    switch (p) {
    case P_SAVE_NEW: sound_save_new(a); break;
    case P_OVERWRITE: sound_overwrite(a); break;
    case P_RESTORE: sound_restore(a); break;
    case P_RANDOM: sound_variation(a); break;
    case P_RENAME: rename_start(a); break;
    case P_DELETE: sound_delete(a); break;
    }
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
static const int *page_rows(int page, int *n, const char *const **headers)
{
    if (page == PAGE_CHORDS) { *n = CHORD_ROW_COUNT; if (headers) *headers = CHORD_HEADERS; return CHORD_ROWS; }
    *n = SOUND_ROW_COUNT;
    if (headers) *headers = SOUND_HEADERS;
    return SOUND_ROWS;
}

static int *page_sel(OrcApp *a) { return a->page == PAGE_CHORDS ? &a->chords_sel : &a->sound_sel; }

static void move_list_sel(OrcApp *a, int dir)
{
    int n, *sel = page_sel(a);
    const int *rows = page_rows(a->page, &n, NULL);
    int r = *sel;
    do { r = (r + dir + n) % n; } while (rows[r] < 0);
    *sel = r;
    a->del_armed = 0;
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
    int dpad = b == PAD_UP || b == PAD_DOWN || b == PAD_LEFT || b == PAD_RIGHT;
    if (a->ai == AI_SHOW) return b == PAD_UP || b == PAD_DOWN;
    if (a->ai != AI_OFF) return 0;
    if (a->overlay) return dpad;
    if (a->page == PAGE_PLAY) return 0;
    if (dpad) return 1;
    return a->page != PAGE_MIDI && (b == PAD_L1 || b == PAD_R1);
}

/* SELECT tenuto: l'elenco dei suoni si apre e i tasti fanno da funzioni. */
static void overlay_open(OrcApp *a)
{
    if (a->overlay) return;
    a->overlay = 1;
    a->peek_t = 0.0f;
    release_flip(a);
    a->learn = -1;
}

static void overlay_press(OrcApp *a, int b)
{
    switch (b) {
    case PAD_UP: sound_step(a, -1, 0); break;
    case PAD_DOWN: sound_step(a, 1, 0); break;
    case PAD_L1: sound_category_step(a, -1); break;
    case PAD_R1: sound_category_step(a, 1); break;
    case PAD_LEFT:
    case PAD_RIGHT:
        a->set.key = (a->set.key + (b == PAD_RIGHT ? 1 : 11)) % 12;
        a->dirty = 1;
        modifiers_changed(a);
        break;
    case PAD_L2: a->set.minor = !a->set.minor; a->dirty = 1; modifiers_changed(a); break;
    case PAD_R2: keymode_toggle(a); break;
    case PAD_A: loop_record(a); break;
    case PAD_B: loop_play(a); break;
    case PAD_Y: loop_undo(a); break;
    case PAD_X: beat_toggle(a); break;
    case PAD_START: transport_toggle(a); break;
    case PAD_L3: panic(a); toast(a, "Tutte le note spente"); break;
    }
}

static void press(OrcApp *a, int b)
{
    if (a->quit_dialog) {
        if (b == PAD_A || b == PAD_DOWN) { a->quit = 1; panic(a); }
        else if (b == PAD_B || b == PAD_UP || b == PAD_MENU) a->quit_dialog = 0;
        return;
    }
    if (a->editing) {
        if (b == PAD_A) rename_finish(a, 1);
        else if (b == PAD_B || b == PAD_MENU) rename_finish(a, 0);
        return;
    }
    if (a->ai != AI_OFF) { ai_press(a, b); return; }
    if (b == PAD_SELECT) { a->sel_down = 1; a->sel_used = 0; a->sel_time = 0.0f; return; }
    if (b == PAD_MENU) { release_flip(a); a->learn = -1; a->overlay = 0; a->sel_down = 0; a->quit_dialog = 1; return; }
    if (a->sel_down) { overlay_open(a); a->sel_used = 1; overlay_press(a, b); return; }
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
    case PAGE_CHORDS:
    case PAGE_SOUND: {
        int n;
        const int *rows = page_rows(a->page, &n, NULL);
        int p = rows[*page_sel(a)];
        switch (b) {
        case PAD_UP: move_list_sel(a, -1); break;
        case PAD_DOWN: move_list_sel(a, 1); break;
        case PAD_LEFT: param_change(a, p, -1, 0); break;
        case PAD_RIGHT: param_change(a, p, 1, 0); break;
        case PAD_L1: param_change(a, p, -1, 1); break;
        case PAD_R1: param_change(a, p, 1, 1); break;
        case PAD_A:
            if (is_action(p)) param_action(a, p);
            else chord_press(a, SRC_FLIP(AUDITION_SLOT), 0.8f);
            break;
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
        case PAD_Y: loop_undo(a); break;
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
            else if (r->kind == ROW_AI) ai_listen(a);
            else learn_start(a, r->first, r->last);
            break;
        case PAD_B:
            if (a->learn >= 0) { a->learn = -1; toast(a, "Apprendimento annullato"); }
            break;
        case PAD_X:
            if (r->kind == ROW_TARGET) { a->set.map[r->first].kind = MAP_NONE; a->dirty = 1; toast(a, "Assegnazione tolta"); }
            break;
        case PAD_Y: kbd_next(a); break;
        case PAD_R2: kbd_save(a); break;
        }
        break;
    }
    }
}

static void release(OrcApp *a, int b)
{
    if (b == PAD_SELECT) {
        int tap = a->sel_down && !a->sel_used && !a->overlay;
        a->sel_down = 0;
        a->overlay = 0;
        if (tap && !a->quit_dialog && !a->editing) {      /* toccato: pagina successiva */
            release_flip(a);
            a->learn = -1;
            a->del_armed = 0;
            a->page = (a->page + 1) % PAGE_COUNT;
        }
        return;
    }
    for (int i = 0; i < FLIP_SLOTS; i++)
        if (FLIP_PADS[i] == b) chord_unpress(a, SRC_FLIP(i));
    if (b == PAD_A) chord_unpress(a, SRC_FLIP(AUDITION_SLOT));
    int m = b == PAD_L1 ? MOD_SWAP : b == PAD_R1 ? MOD_SUS : b == PAD_L2 ? MOD_SEVENTH : b == PAD_R2 ? MOD_NINTH : -1;
    if (m >= 0 && a->mods[m]) { a->mods[m] = 0; modifiers_changed(a); }
}

/* Testo da una tastiera USB: il nome nella Rinomina; altrimenti frecce e Invio fanno da croce e da A. */
void orcapp_text(OrcApp *a, const char *utf8, int special)
{
    if (!a->editing) {
        int b = special == TEXT_UP ? PAD_UP : special == TEXT_DOWN ? PAD_DOWN : special == TEXT_LEFT ? PAD_LEFT :
                special == TEXT_RIGHT ? PAD_RIGHT : special == TEXT_ENTER ? PAD_A : -1;
        if (b >= 0) { orcapp_button(a, b, 1); orcapp_button(a, b, 0); }
        return;
    }
    size_t l = strlen(a->edit);
    switch (special) {
    case TEXT_CHARS: {
        size_t add = strlen(utf8);
        int ok = add > 0;
        for (size_t i = 0; i < add; i++) ok &= (unsigned char)utf8[i] >= 32;
        if (ok && l + add < 32) memcpy(a->edit + l, utf8, add + 1);
        break;
    }
    case TEXT_BACKSPACE:
        while (l > 0) { l--; if (((unsigned char)a->edit[l] & 0xC0) != 0x80) break; }
        a->edit[l] = 0;
        break;
    case TEXT_ENTER:
        rename_finish(a, 1);
        break;
    }
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
    if (a->sel_down && !a->overlay) {
        a->sel_time += dt;
        if (a->sel_time >= 0.3f) overlay_open(a);
    }
    a->peek_t = fmaxf(0.0f, a->peek_t - dt);
    a->ai_time += dt;
    ai_poll(a);
    for (int k = 0; k < 8; k++) a->knob_t[k] = fmaxf(0.0f, a->knob_t[k] - dt);
    if (a->del_armed) { a->del_t -= dt; if (a->del_t <= 0.0f) a->del_armed = 0; }
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

static float scope_peak(OrcApp *a, int n);

/* Si ridisegna quando qualcosa si muove (luce dell'accordo, messaggi, trasporto, apprendimento) o quando
   cambia lo stato mostrato; comunque una volta al secondo. */
int orcapp_needs_draw(OrcApp *a)
{
    int knobs = 0;
    for (int k = 0; k < 8; k++) knobs |= a->knob_t[k] > 0.0f;
    if (a->glow > 0.0f || a->toast_t > 0.0f || a->flash_t > 0.0f || a->midi_flash > 0.0f || a->learn >= 0 || a->ai != AI_OFF ||
        a->quit_dialog || orc_transport_on(a->orc) || a->peek_t > 0.0f || a->editing || knobs)
        return 1;
    if (a->page == PAGE_PLAY && scope_peak(a, 256) > 0.002f) return 1;      /* l'oscilloscopio si muove */
    OrcLoopInfo li;
    orc_loop_info(a->orc, &li);
    int ln[ORC_MAX_CHORD], tag = 0, lnn = orc_layer_notes(a->orc, ORC_LOOP, ln, &tag);
    int v[] = { a->page, a->sound_sel, a->chords_sel, a->rhythm_sel, a->midi_sel, a->chord_live, a->notes_n, a->bass, a->stack_n,
                li.state, li.events, li.full, li.layers, lnn, tag, a->clock_ticks > 0 && a->clock_seen < 0.5f,
                a->overlay, a->user_n, a->del_armed, a->base_ok };
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
    static const char *tabs[PAGE_COUNT] = { "Suona", "Accordi", "Suono", "Ritmo", "MIDI" };
    int x = 44 + gfx_text_width(FONT_TITLE, "OpenOrc") + 14, padw = 20, total = 0;
    for (int i = 0; i < PAGE_COUNT; i++) total += gfx_text_width(FONT_BOLD, tabs[i]) + 4;
    while (padw > 8 && x + total + padw * PAGE_COUNT > 504) padw -= 2;      /* le schede stanno prima della spia MIDI */
    for (int i = 0; i < PAGE_COUNT; i++) {
        int w = gfx_text_width(FONT_BOLD, tabs[i]) + padw;
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
    char more[16] = "";
    if (li->layers > 0) snprintf(more, sizeof(more), " +%d", li->layers);    /* sovraincisioni */
    switch (li->state) {
    case LOOP_EMPTY: snprintf(out, n, "vuoto"); break;
    case LOOP_ARMED: snprintf(out, n, "attende la battuta"); break;
    case LOOP_RECORDING: snprintf(out, n, "registra %d/%d", bar, bars); break;
    case LOOP_PLAYING: snprintf(out, n, "suona %d/%d%s", bar, bars, more); break;
    case LOOP_OVERDUB: snprintf(out, n, "sovraincide %d/%d%s", bar, bars, more); break;
    default: snprintf(out, n, "fermo%s", more); break;
    }
}

static int melody_on(const OrcApp *a)
{
    for (int n = 0; n < 128; n++) if (a->melody_down[n] || a->melody_pedal[n]) return 1;
    return 0;
}

/* Picco delle ultime n uscite del motore: dice se c'e' ancora suono da mostrare. */
static float scope_peak(OrcApp *a, int n)
{
    orc_scope(a->orc, a->scope, n);
    float peak = 0.0f;
    for (int i = 0; i < n; i++) peak = fmaxf(peak, fabsf(a->scope[i]));
    return peak;
}

/* Oscilloscopio dell'uscita, agganciato al passaggio per lo zero in salita. Il colore dice chi suona:
   oro l'accordo dal vivo, viola la melodia, verde acqua il loop. In alto le note dell'accordo con
   l'ottava, cioe' il voicing scelto da K1. */
/* Oscilloscopio come fondale della fascia dell'accordo, agganciato al passaggio per lo zero in salita:
   un tratto tenue dietro al nome, oro per l'accordo dal vivo, viola per la melodia, verde acqua per il
   loop. Restituisce il picco, per scrivere «silenzio» quando non suona nulla. */
static float draw_scope_band(OrcApp *a, Canvas *c, int x, int y, int w, int h)
{
    OrcLoopInfo li;
    orc_loop_info(a->orc, &li);
    uint32_t color = a->chord_live ? C_GOLD : melody_on(a) ? C_VIOLET :
                     (li.state == LOOP_PLAYING || li.state == LOOP_OVERDUB) ? C_TEAL : C_DIM;
    int n = w - 24;
    if (n > 600) n = 600;
    orc_scope(a->orc, a->scope, n * 2);
    int start = 0;
    for (int i = 1; i < n; i++)
        if (a->scope[i - 1] <= 0.0f && a->scope[i] > 0.0f) { start = i; break; }
    float peak = 0.0f;
    for (int i = 0; i < n; i++) peak = fmaxf(peak, fabsf(a->scope[start + i]));
    float mid = y + h * 0.5f, gain = (h * 0.36f) / fmaxf(peak, 0.25f), px = 0, py = 0;
    gfx_line(c, x + 12, mid, x + w - 12, mid, 1.0f, C_LINE, 0.35f);
    for (int i = 0; i < n; i++) {
        float xx = x + 12 + i, yy = mid - a->scope[start + i] * gain;
        if (i) {
            gfx_line(c, px, py, xx, yy, 5.0f, color, 0.06f);
            gfx_line(c, px, py, xx, yy, 1.8f, color, 0.24f);
        }
        px = xx;
        py = yy;
    }
    return peak;
}

/* La riga di stato sotto l'intestazione: suono, esecuzione, tonalita', tempo e loop in una sola riga. */
static void draw_status_line(OrcApp *a, Canvas *c, int y)
{
    char vals[5][48];
    uint32_t cols[5] = { C_TEXT, C_TEXT, C_TEXT, C_TEXT, C_TEXT };
    sound_label(a, vals[0], sizeof(vals[0]));
    if (a->set.snd.perform == PERF_ARP) snprintf(vals[1], sizeof(vals[1]), "Arpeggio %s", ARP_SHORT[(int)(clamp01(a->set.snd.perform_amount) * 4.99f)]);
    else snprintf(vals[1], sizeof(vals[1]), "%s", PERFORM_NAMES[a->set.snd.perform]);
    key_name(vals[2], sizeof(vals[2]), a->set.key, a->set.minor);
    cols[2] = a->set.keymode ? C_GOLD : C_MUTED;
    if (a->set.beat_on) snprintf(vals[3], sizeof(vals[3]), "%d BPM · %s", (int)lrintf(a->set.bpm), BEAT_NAMES[a->set.beat_style]);
    else snprintf(vals[3], sizeof(vals[3]), "%d BPM", (int)lrintf(a->set.bpm));
    OrcLoopInfo li;
    orc_loop_info(a->orc, &li);
    char st[48];
    loop_state_text(&li, st, sizeof(st));
    snprintf(vals[4], sizeof(vals[4]), "loop %s", st);
    cols[4] = li.state == LOOP_RECORDING ? C_RED : (li.state == LOOP_EMPTY ? C_DIM : C_TEXT);
    int total = 0;
    for (int i = 0; i < 5; i++) total += gfx_text_width(FONT_SMALL, vals[i]) + (i < 4 ? 26 : 0);
    int x = 320 - total / 2;
    for (int i = 0; i < 5; i++) {
        x += gfx_text(c, FONT_SMALL, x, y, vals[i], cols[i]);
        if (i < 4) { gfx_text(c, FONT_SMALL, x + 9, y, "·", C_DIM); x += 26; }
    }
}

/* Un tasto della Flip con il nome dell'accordo che suonerebbe adesso (con i dorsali tenuti). */
static void draw_flip_chip(OrcApp *a, Canvas *c, int slot, float cx, float cy, float w, float h, int font)
{
    int root, iv[CHORD_MAX_IV], deg, sp;
    int n = build_chord(a, SRC_FLIP(slot), &root, iv, &deg, &sp);
    char name[24];
    chord_name(name, sizeof(name), root, chord_mask(iv, n), sp);
    int held = stack_find(a, SRC_FLIP(slot)) >= 0;
    gfx_round_rect(c, cx - w / 2, cy - h / 2, w, h, 9, held ? C_GOLD : C_PANEL_HI, 1.0f);
    if (gfx_text_width(font, name) > w - 10) font = FONT_SMALL;
    gfx_text_center(c, font, (int)cx, (int)(cy + (font == FONT_SMALL ? 5 : 6)), name, held ? C_BAR : C_TEXT);
}

/* Croce (d = 0) o A B X Y (d = 1), disposti come sulla console; al centro il simbolo del gruppo. */
static void draw_flip_cluster(OrcApp *a, Canvas *c, int d, float cx, float cy, float w, float h, float dx, float dy, int font)
{
    static const int SLOT_AT[2][4] = { { 0, 1, 2, 3 }, { 4, 5, 7, 6 } };    /* sinistra, su, destra, giu' */
    const float off[4][2] = { { -dx, 0 }, { 0, -dy }, { dx, 0 }, { 0, dy } };
    if (d == 0) {
        gfx_round_rect(c, cx - 4, cy - 12, 8, 24, 2, C_LINE, 1.0f);
        gfx_round_rect(c, cx - 12, cy - 4, 24, 8, 2, C_LINE, 1.0f);
    } else {
        static const char *L[4] = { "Y", "X", "A", "B" };
        static const float at[4][2] = { { -13, 5 }, { 0, -6 }, { 13, 5 }, { 0, 16 } };
        for (int k = 0; k < 4; k++) gfx_text_center(c, FONT_SMALL, (int)(cx + at[k][0]), (int)(cy + at[k][1]), L[k], C_DIM);
    }
    for (int k = 0; k < 4; k++) draw_flip_chip(a, c, SLOT_AT[d][k], cx + off[k][0], cy + off[k][1], w, h, font);
}

static void draw_mod_chip(OrcApp *a, Canvas *c, int m, const char *label, float x, float y, float w)
{
    gfx_round_rect(c, x, y, w, 24, 12, a->mods[m] ? C_GOLD : C_PANEL, 1.0f);
    gfx_text_center(c, FONT_SMALL, (int)(x + w / 2), (int)(y + 17), label, a->mods[m] ? C_BAR : C_MUTED);
}

/* Le manopole della MPK in piccolo, sotto i pad: quella appena girata si accende. */
static void draw_knob_strip(OrcApp *a, Canvas *c, int x, int y)
{
    static const char *KN[8] = { "Voi", "Bas", "Ese", "Ton", "Cho", "Del", "Riv", "Vol" };
    for (int k = 0; k < 8; k++) {
        float cx = x + k * 36 + 16, v = clamp01(*knob_ref(a, k));
        int hot = a->knob_t[k] > 0.0f;
        gfx_text_center(c, FONT_SMALL, (int)cx, y + 12, KN[k], hot ? C_GOLD : C_DIM);
        gfx_round_rect(c, cx - 14, y + 19, 28, 4, 2, C_LINE, 1.0f);
        gfx_round_rect(c, cx - 14, y + 19, 4 + 24 * v, 4, 2, hot ? C_GOLD : C_MUTED, 1.0f);
    }
}

/* Pagina Suona: una riga di stato, la fascia dell'accordo con l'oscilloscopio come fondale, e sotto i
   comandi: i pad della MPK solo quando la tastiera c'e', i tasti della Flip disposti come sulla console. */
static void draw_play(OrcApp *a, Canvas *c)
{
    char t[96];
    draw_status_line(a, c, 68);

    /* fascia dell'accordo */
    const int bx = 16, by = 80, bw = 608, bh = 184;
    gfx_round_rect(c, bx, by, bw, bh, 16, RGB(10, 13, 18), 1.0f);
    float peak = draw_scope_band(a, c, bx, by, bw, bh);
    if (a->name_root[0]) {
        uint32_t col = a->chord_live ? gfx_mix(C_TEXT, C_GOLD, 0.35f + 0.65f * a->glow) : C_MUTED;
        int wr = gfx_text_width(FONT_HUGE, a->name_root), ws = gfx_text_width(FONT_BIG, a->name_suffix);
        int x = 320 - (wr + ws + 4) / 2;
        gfx_text(c, FONT_HUGE, x, by + 86, a->name_root, col);
        gfx_text(c, FONT_BIG, x + wr + 4, by + 86, a->name_suffix, col);
        char notes[64] = "";
        for (int i = 0; i < a->notes_n && i < 6; i++) {
            int pc = a->notes[i] % 12, dup = 0;
            for (int j = 0; j < i; j++) dup |= a->notes[j] % 12 == pc;
            if (dup) continue;
            size_t l = strlen(notes);
            snprintf(notes + l, sizeof(notes) - l, "%s%s", l ? " " : "", note_it(pc, a->spelling));
        }
        snprintf(t, sizeof(t), "%s   ·   %s", a->symbol, notes);
        gfx_text_center(c, FONT_BODY, 320, by + 118, t, a->chord_live ? C_TEXT : C_DIM);
        if (a->numeral[0]) {
            char k[32];
            key_name(k, sizeof(k), a->set.key, a->set.minor);
            snprintf(t, sizeof(t), "%s in %s", a->numeral, k);
        } else {
            char b[16];
            note_label(b, sizeof(b), a->bass);
            snprintf(t, sizeof(t), "basso %s", b);
        }
        gfx_text_center(c, FONT_SMALL, 320, by + 142, t, C_MUTED);
    } else {
        gfx_text_center(c, FONT_TITLE, 320, by + 86, "Suona un accordo", C_TEXT);
        char k[32];
        key_name(k, sizeof(k), a->set.key, a->set.minor);
        if (a->midi_name[0]) snprintf(t, sizeof(t), "Tieni un pad (Maj, Min...) e premi un tasto, oppure croce e A B X Y: accordi di %s", k);
        else snprintf(t, sizeof(t), "Croce e A B X Y suonano gli accordi di %s", k);
        gfx_text_center(c, FONT_SMALL, 320, by + 118, t, C_MUTED);
    }
    /* le note con l'ottava: e' il voicing scelto da K1 */
    if (a->chord_live && a->notes_n) {
        char nn[16];
        t[0] = 0;
        for (int i = 0; i < a->notes_n && i < 7; i++) {
            note_label(nn, sizeof(nn), a->notes[i]);
            size_t l = strlen(t);
            snprintf(t + l, sizeof(t) - l, "%s%s", l ? " " : "", nn);
        }
        if (a->set.snd.bass_level > 0.005f) {
            note_label(nn, sizeof(nn), a->bass);
            size_t l = strlen(t);
            snprintf(t + l, sizeof(t) - l, "  ·  basso %s", nn);
        }
        gfx_text(c, FONT_SMALL, bx + 16, by + bh - 12, t, gfx_mix(C_MUTED, C_GOLD, 0.5f));
    }
    if (peak < 0.002f) gfx_text_right(c, FONT_SMALL, bx + bw - 16, by + bh - 12, "silenzio", C_DIM);

    /* comandi */
    static const char *MODS[MOD_COUNT] = { "L1 m/M", "R1 sus4", "L2 7", "R2 9" };
    if (a->midi_name[0]) {
        /* pad della MPK a sinistra, tasti della Flip piu' piccoli a destra */
        for (int i = 0; i < 8; i++) {
            int col = i % 4, row = i / 4;
            float x = 16 + col * 72, y = 290 + row * 58;
            int held = a->pad_held[i];
            gfx_round_rect(c, x, y, 64, 50, 10, held ? C_GOLD : C_PANEL_HI, 1.0f);
            gfx_text_center(c, FONT_BOLD, (int)(x + 32), (int)(y + 31), CHORD_PAD_NAMES[i], held ? C_BAR : (row == 0 ? C_TEXT : C_MUTED));
        }
        draw_knob_strip(a, c, 16, 404);
        draw_mod_chip(a, c, MOD_SEVENTH, MODS[2], 318, 276, 70);
        draw_mod_chip(a, c, MOD_SWAP, MODS[0], 392, 276, 70);
        draw_mod_chip(a, c, MOD_SUS, MODS[1], 480, 276, 70);
        draw_mod_chip(a, c, MOD_NINTH, MODS[3], 554, 276, 70);
        draw_flip_cluster(a, c, 0, 394, 372, 62, 26, 46, 33, FONT_SMALL);
        draw_flip_cluster(a, c, 1, 548, 372, 62, 26, 46, 33, FONT_SMALL);
    } else {
        /* solo la Flip: i tasti grandi, nella posizione reale della console, i dorsali agli angoli */
        draw_mod_chip(a, c, MOD_SEVENTH, MODS[2], 16, 276, 80);
        draw_mod_chip(a, c, MOD_SWAP, MODS[0], 102, 276, 80);
        draw_mod_chip(a, c, MOD_SUS, MODS[1], 458, 276, 80);
        draw_mod_chip(a, c, MOD_NINTH, MODS[3], 544, 276, 80);
        draw_flip_cluster(a, c, 0, 164, 368, 88, 32, 66, 44, FONT_BOLD);
        draw_flip_cluster(a, c, 1, 476, 368, 88, 32, 66, 44, FONT_BOLD);
    }

    if (a->flash_t > 0.0f) {
        float al = fminf(1.0f, a->flash_t * 4.0f);
        int w = gfx_text_width(FONT_BOLD, a->flash) + 32;
        gfx_round_rect(c, 320 - w / 2.0f, 236, w, 30, 15, C_BAR, 0.92f * al);
        gfx_round_frame(c, 320 - w / 2.0f, 236, w, 30, 15, 1.5f, C_GOLD, al);
        gfx_text_center(c, FONT_BOLD, 320, 257, a->flash, gfx_mix(C_BAR, C_TEXT, al));
    }
    draw_hints(c, "Croce e ABXY: accordi · dorsali: varianti · START ritmo · SELECT tenuto: suoni e loop");
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

/* Pagine Accordi e Suono: elenchi di parametri a gruppi. */
static void draw_params(OrcApp *a, Canvas *c)
{
    int n, *sel = page_sel(a);
    const char *const *hdr;
    const int *rows = page_rows(a->page, &n, &hdr);
    ListRow lr[SOUND_ROW_COUNT > CHORD_ROW_COUNT ? SOUND_ROW_COUNT : CHORD_ROW_COUNT];
    for (int i = 0; i < n; i++) {
        int p = rows[i];
        ListRow *r = &lr[i];
        memset(r, 0, sizeof(*r));
        if (p < 0) { r->header = 1; r->label = hdr[-1 - p]; continue; }
        r->label = param_label(a, p);
        param_text(a, p, r->value, sizeof(r->value));
        r->bar = param_bar(a, p);
        if (p == P_KEYMODE && !a->set.keymode) r->color = C_MUTED;
        if (p == P_FOLLOW && !a->set.follow) r->color = C_MUTED;
        if (is_action(p) && !action_ok(a, p)) r->color = C_DIM;
        if (p == P_DELETE && a->del_armed) r->color = C_RED;
    }
    int *scroll = a->page == PAGE_CHORDS ? &a->chords_scroll : &a->sound_scroll;
    draw_list(c, lr, n, *sel, scroll, 54, 442, 28);
    if (a->page == PAGE_CHORDS) draw_hints(c, "Su/giù sceglie · sinistra/destra cambia (L1/R1 a passi grandi) · A tenuto ascolta");
    else if (is_action(rows[*sel])) draw_hints(c, "A esegue · su/giù sceglie · SELECT tenuto: elenco dei suoni");
    else if (rows[*sel] == P_SOUND) draw_hints(c, "Sinistra/destra: suono · L1/R1: categoria · A tenuto ascolta");
    else draw_hints(c, "Su/giù sceglie · sinistra/destra cambia (L1/R1 a passi grandi) · A tenuto ascolta");
}

/* Elenco dei suoni con SELECT tenuto: il suono corrente in mezzo, i comandi in basso. */
static void draw_overlay(OrcApp *a, Canvas *c)
{
    gfx_rect_alpha(c, 0, 0, c->w, c->h, RGB(0, 0, 0), 0.55f);
    const int x = 40, y = 54, w = 560, h = 386;
    gfx_round_rect(c, x, y, w, h, 16, C_PANEL, 1.0f);
    gfx_round_frame(c, x, y, w, h, 16, 1.5f, C_GOLD, 1.0f);
    char k[32], t[128], nm[48];
    gfx_text(c, FONT_BOLD, x + 22, y + 30, "Suoni", C_TEXT);
    key_name(k, sizeof(k), a->set.key, a->set.minor);
    snprintf(t, sizeof(t), "%s%s", k, a->set.keymode ? " · modo tonalità" : "");
    gfx_text_right(c, FONT_BOLD, x + w - 22, y + 30, t, a->set.keymode ? C_GOLD : C_TEXT);
    int n = sound_count(a), vis = 10, cur = a->set.sound, top = cur - vis / 2;
    if (top > n - vis) top = n - vis;
    if (top < 0) top = 0;
    for (int r = 0; r < vis && top + r < n; r++) {
        int i = top + r, yy = y + 44 + r * 26, is_cur = i == cur;
        if (is_cur) gfx_round_rect(c, x + 12, yy, w - 24, 25, 8, C_GOLD_D, 1.0f);
        if (is_cur) sound_label(a, nm, sizeof(nm));
        else snprintf(nm, sizeof(nm), "%s", sound_name(a, i));
        gfx_text(c, is_cur ? FONT_BOLD : FONT_BODY, x + 30, yy + 18, nm, is_cur ? C_TEXT : C_MUTED);
        if (r == 0 || i == 0 || strcmp(sound_cat(a, i), sound_cat(a, i - 1)))
            gfx_text_right(c, FONT_SMALL, x + w - 30, yy + 17, sound_cat(a, i), is_cur ? C_GOLD : C_DIM);
    }
    if (n > vis) {
        float hh = 260.0f * vis / n, yy = y + 44 + (260.0f - hh) * top / (n - vis);
        gfx_round_rect(c, x + w - 9, yy, 3, hh, 1.5f, C_LINE, 1.0f);
    }
    int hy = y + h - 54;
    gfx_rect(c, x + 18, hy, w - 36, 1, C_LINE);
    gfx_text_center(c, FONT_SMALL, 320, hy + 21, "Su/giù suono · L1/R1 categoria · sinistra/destra tonalità · L2 magg./min. · R2 modo tonalità", C_MUTED);
    OrcLoopInfo li;
    orc_loop_info(a->orc, &li);
    char st[48];
    loop_state_text(&li, st, sizeof(st));
    snprintf(t, sizeof(t), "A registra · B suona/ferma · Y annulla · X batteria   ·   loop %s", st);
    gfx_text_center(c, FONT_SMALL, 320, hy + 43, t, li.state == LOOP_RECORDING ? C_RED : C_MUTED);
}

/* Il suono appena scelto dalla MPK: il precedente, lui e il successivo, per un attimo. */
static void draw_peek(OrcApp *a, Canvas *c)
{
    float al = fminf(1.0f, a->peek_t * 4.0f);
    const int w = 420, x = 320 - w / 2, y = 56, h = 86;
    int n = sound_count(a), cur = a->set.sound;
    char nm[48];
    gfx_round_rect(c, x, y, w, h, 14, C_BAR, al);
    gfx_round_frame(c, x, y, w, h, 14, 1.5f, C_GOLD, al);
    gfx_text_center(c, FONT_SMALL, 320, y + 21, sound_name(a, (cur - 1 + n) % n), gfx_mix(C_BAR, C_DIM, al));
    sound_label(a, nm, sizeof(nm));
    gfx_text_center(c, FONT_BOLD, 320, y + 48, nm, gfx_mix(C_BAR, C_TEXT, al));
    gfx_text(c, FONT_SMALL, x + 16, y + 47, sound_cat(a, cur), gfx_mix(C_BAR, C_GOLD, al));
    gfx_text_center(c, FONT_SMALL, 320, y + 75, sound_name(a, (cur + 1) % n), gfx_mix(C_BAR, C_DIM, al));
}

/* Rinomina: il nome si scrive con una tastiera USB. */
static void draw_edit(OrcApp *a, Canvas *c)
{
    gfx_rect_alpha(c, 0, 0, c->w, c->h, RGB(0, 0, 0), 0.6f);
    gfx_round_rect(c, 80, 158, 480, 156, 18, C_PANEL_HI, 1.0f);
    gfx_round_frame(c, 80, 158, 480, 156, 18, 2, C_GOLD, 1.0f);
    gfx_text_center(c, FONT_BOLD, 320, 192, "Nome del suono", C_TEXT);
    gfx_round_rect(c, 108, 208, 424, 42, 10, C_BAR, 1.0f);
    char t[48];
    snprintf(t, sizeof(t), "%s%s", a->edit, (int)(a->time * 2.0f) % 2 ? "|" : " ");
    gfx_text(c, FONT_BODY, 122, 236, t, C_GOLD);
    gfx_text_center(c, FONT_SMALL, 320, 282, "Scrivi con una tastiera USB · Invio o A conferma · B annulla", C_MUTED);
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
    draw_hints(c, "A registra · B suona/ferma · Y annulla · X tap tempo · L1/R1 tempo ±10");
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
        } else if (m->kind == ROW_AI) {
            if (!a->midi_name[0]) snprintf(r->value, sizeof(r->value), "collega una tastiera");
            else snprintf(r->value, sizeof(r->value), "%.40s", a->midi_name);
            r->color = a->midi_name[0] ? C_VIOLET : C_DIM;
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
    draw_hints(c, "A impara · B annulla · X toglie · Y preset della tastiera · R2 salva · L1/R1 gruppo");
}

/* Testo a capo nella larghezza w: restituisce l'altezza usata. */
static int draw_wrapped(Canvas *c, int font, int x, int y, int w, int lh, const char *text, uint32_t color, int max_lines)
{
    char line[256], cand[256];
    int lines = 0;
    const char *p = text ? text : "";
    while (*p && lines < max_lines) {
        size_t len = 0;
        line[0] = 0;
        while (*p && *p != '\n') {
            const char *e = p;
            while (*e && *e != ' ' && *e != '\n') e++;
            int n = snprintf(cand, sizeof(cand), "%s%s%.*s", line, len ? " " : "", (int)(e - p), p);
            if (n < 0 || n >= (int)sizeof(cand)) { if (!len) p = e; break; }      /* parola lunghissima */
            if (len && gfx_text_width(font, cand) > w) break;
            memcpy(line, cand, (size_t)n + 1);
            len = (size_t)n;
            p = e;
            while (*p == ' ') p++;
        }
        if (*p == '\n') p++;
        if (len) gfx_text(c, font, x, y + lines * lh, line, color);
        lines++;
    }
    return lines * lh;
}

/* Configurazione con l'IA: domanda, ascolto, attesa, proposta, errore. */
static void draw_ai(OrcApp *a, Canvas *c)
{
    char t[200];
    if (a->ai == AI_ASK) {
        gfx_rect_alpha(c, 0, 0, c->w, c->h, RGB(0, 0, 0), 0.6f);
        gfx_round_rect(c, 70, 120, 500, 240, 18, C_PANEL_HI, 1.0f);
        gfx_round_frame(c, 70, 120, 500, 240, 18, 2, C_VIOLET, 1.0f);
        gfx_text_center(c, FONT_SMALL, 320, 152, "Tastiera nuova", C_VIOLET);
        gfx_text_center(c, FONT_TITLE, 320, 186, a->ai_long[0] ? a->ai_long : a->ai_dev, C_TEXT);
        draw_wrapped(c, FONT_BODY, 100, 224, 440, 24, "Vuoi che l'IA la configuri? Muovi manopole, pad e pulsanti: lei sceglie dove metterli, poi decidi tu se tenerla.", C_MUTED, 4);
        gfx_text_center(c, FONT_BOLD, 320, 338, "A sì, configurala   ·   B no", C_GOLD);
        return;
    }
    gfx_vgradient(c, 0, 47, c->w, 399, C_BG_TOP, C_BG_BOT);
    if (a->ai == AI_LISTEN) {
        gfx_text(c, FONT_SMALL, 24, 74, "Configura con l'IA · 1 di 2", C_VIOLET);
        snprintf(t, sizeof(t), "Ascolto la %s", a->ai_long[0] ? a->ai_long : a->ai_dev);
        gfx_text(c, FONT_TITLE, 24, 106, t, C_TEXT);
        draw_wrapped(c, FONT_BODY, 24, 134, 592, 22, "Muovi tutte le manopole, premi tutti i pad (anche negli altri banchi), qualche tasto, le rotelle e i pulsanti di trasporto. Poi premi A.", C_MUTED, 3);
        int notes, knobs, buttons, bends;
        ia_scan_counts(&a->scan, &notes, &knobs, &buttons, &bends);
        const char *lab[4] = { "note", "manopole", "pulsanti", "pitch bend" };
        int val[4] = { notes, knobs, buttons, bends };
        for (int i = 0; i < 4; i++) {
            int x = 24 + i * 150;
            gfx_round_rect(c, x, 198, 140, 56, 12, C_PANEL, 1.0f);
            snprintf(t, sizeof(t), "%d", val[i]);
            gfx_text(c, FONT_TITLE, x + 14, 232, t, val[i] ? C_GOLD : C_DIM);
            gfx_text(c, FONT_SMALL, x + 14, 248, lab[i], C_MUTED);
        }
        char *d = ia_scan_describe(&a->scan, 0);
        draw_wrapped(c, FONT_SMALL, 28, 284, 584, 20, d, C_TEXT, 6);
        free(d);
        snprintf(t, sizeof(t), "Ultimo: %s", a->ai_last[0] ? a->ai_last : "niente ancora");
        gfx_text(c, FONT_SMALL, 28, 428, t, C_DIM);
        draw_hints(c, "A fatto, chiedi all'IA · X ricomincia · B annulla");
        return;
    }
    if (a->ai == AI_WAIT) {
        gfx_text(c, FONT_SMALL, 24, 74, "Configura con l'IA · 2 di 2", C_VIOLET);
        int dots = (int)(a->ai_time * 2.5f) % 4;
        snprintf(t, sizeof(t), "L'IA studia la tua tastiera%.*s", dots, "...");
        gfx_text_center(c, FONT_TITLE, 320, 230, t, C_TEXT);
        gfx_text_center(c, FONT_BODY, 320, 262, a->ai_long[0] ? a->ai_long : a->ai_dev, C_MUTED);
        gfx_text_center(c, FONT_SMALL, 320, 292, a->ai_model, C_DIM);
        for (int i = 0; i < 5; i++) {
            float ph = a->ai_time * 3.0f - i * 0.5f, al = 0.25f + 0.75f * (0.5f + 0.5f * sinf(ph));
            gfx_circle(c, 280 + i * 20, 330, 5.0f, C_VIOLET, al);
        }
        draw_hints(c, "B annulla");
        return;
    }
    if (a->ai == AI_ERROR) {
        gfx_text(c, FONT_SMALL, 24, 74, "Configura con l'IA", C_VIOLET);
        gfx_text(c, FONT_TITLE, 24, 110, "Non ci sono riuscito", C_TEXT);
        draw_wrapped(c, FONT_BODY, 24, 150, 592, 24, a->ai_err, C_RED, 8);
        draw_hints(c, "A riprova · X riascolta la tastiera · B chiudi");
        return;
    }
    /* proposta */
    gfx_text(c, FONT_SMALL, 24, 72, "Proposta dell'IA", C_VIOLET);
    snprintf(t, sizeof(t), "%s · %d funzioni", a->ai_name, a->ai_count);
    gfx_text(c, FONT_BOLD, 24, 96, t, C_TEXT);
    int h = draw_wrapped(c, FONT_SMALL, 24, 120, 592, 18, a->ai_note[0] ? a->ai_note : "Controlla le assegnazioni qui sotto.", C_MUTED, 3);
    ListRow rows[MIDI_ROW_COUNT - 1];
    for (int i = 0; i < MIDI_ROW_COUNT - 1; i++) {
        const MidiRow *m = &MIDI_ROWS[i + 1];
        ListRow *r = &rows[i];
        memset(r, 0, sizeof(*r));
        r->label = m->label;
        r->bar = -1.0f;
        if (m->kind == ROW_HEADER) { r->header = 1; continue; }
        if (m->kind == ROW_KNOB_MODE) { snprintf(r->value, sizeof(r->value), "%s", KNOB_MODE_NAMES[a->ai_knob_mode]); continue; }
        const IaMap *im = &a->ai_map[m->first];
        MidiMap mm = im->kind == IA_NONE ? (MidiMap){ MAP_NONE, -1, -1, 0 }
                                         : (MidiMap){ im->kind == IA_NOTE ? MAP_NOTE : MAP_CC, im->port, im->ch, im->num };
        map_text(&mm, r->value, sizeof(r->value));
        if (mm.kind == MAP_NONE) r->color = C_DIM;
    }
    draw_list(c, rows, MIDI_ROW_COUNT - 1, a->ai_sel, &a->ai_scroll, 126 + h, 442, 24);
    draw_hints(c, "A applica e salva · X chiedi un'altra proposta · B scarta · su/giù scorre");
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
    case PAGE_CHORDS:
    case PAGE_SOUND: draw_params(a, c); break;
    case PAGE_RHYTHM: draw_rhythm(a, c); break;
    default: draw_midi(a, c); break;
    }
    if (a->overlay) draw_overlay(a, c);
    else if (a->peek_t > 0.0f) draw_peek(a, c);
    if (a->toast_t > 0.0f && a->toast[0]) {
        float al = fminf(1.0f, a->toast_t * 3.0f);
        int w = gfx_text_width(FONT_BOLD, a->toast) + 36;
        if (w > 620) w = 620;
        gfx_round_rect(c, 320 - w / 2.0f, 398, w, 36, 18, C_BAR, 0.94f * al);
        gfx_round_frame(c, 320 - w / 2.0f, 398, w, 36, 18, 1.5f, C_GOLD, al);
        gfx_text_center(c, FONT_BOLD, 320, 422, a->toast, gfx_mix(C_BAR, C_TEXT, al));
    }
    if (a->ai != AI_OFF) draw_ai(a, c);
    if (a->editing) draw_edit(a, c);
    if (a->quit_dialog) draw_quit(c);
}
