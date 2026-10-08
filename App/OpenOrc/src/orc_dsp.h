/* OpenOrc per Miyoo Flip - motore audio.
 *
 * Clone dell'Orchid (Telepathic Instruments) pensato per la Akai MPK mini IV: accordi eseguiti in
 * 6 modi (accordo, strum, slop, arpeggio, pattern, arpa), 4 motori di suono (analogico, FM, piano
 * elettrico, organo) da 16 voci, basso monofonico, batteria con 4 stili, looper, chorus, delay e
 * riverbero.
 *
 * Thread: orc_render() gira nel thread audio; tutto il resto arriva dall'interfaccia con una coda senza
 * lock (un produttore, un consumatore) o con un triplo buffer (suono ed espressione). Strum, arpeggi,
 * pattern, batteria e looper sono calcolati nel thread audio, a passi di 32 campioni (0,67 ms).
 *
 * Due strati suonano insieme: ORC_LIVE (quello che si suona) e ORC_LOOP (quello che il looper
 * ripete). Ognuno ha il suo accordo; il basso segue l'ultimo accordo partito.
 */
#ifndef OPENORC_DSP_H
#define OPENORC_DSP_H

#define ORC_VOICES 16
#define ORC_MAX_CHORD 10
#define ORC_SCOPE 2048
#define ORC_LOOP_MAX 1024

enum { ENG_VA, ENG_FM, ENG_REED, ENG_ORGAN, ENG_COUNT };
enum { PERF_CHORD, PERF_STRUM, PERF_SLOP, PERF_ARP, PERF_PATTERN, PERF_HARP, PERF_COUNT };
enum { BEAT_DISCO, BEAT_PSYCH, BEAT_TRAP, BEAT_BOSSA, BEAT_COUNT };
enum { DRUM_KICK, DRUM_SNARE, DRUM_HAT, DRUM_OPEN, DRUM_CLAP, DRUM_PARTS };
enum { ORC_LIVE, ORC_LOOP, ORC_LAYERS };
enum { LOOP_EMPTY, LOOP_ARMED, LOOP_RECORDING, LOOP_PLAYING, LOOP_OVERDUB, LOOP_STOPPED };

typedef struct {
    int   engine;              /* ENG_* */
    float tone;                /* 0..1: brillantezza (taglio del filtro, indice FM, armoniche) */
    float character;           /* 0..1: VA desintonia e forma, FM rapporto, reed spinta, organo registri */
    float attack, decay;       /* secondi */
    float sustain;             /* 0..1 */
    float release;             /* secondi */
    float vibrato;             /* 0..1: vibrato (tremolo nel piano elettrico) */
    float bass_level;          /* 0..1, 0 = niente basso */
    float bass_tone;           /* 0..1 */
    float chorus, delay, reverb;
    int   perform;             /* PERF_* */
    float perform_amount;      /* 0..1: tempo dello strum, velocita' dell'arpeggio, pattern, slop */
    float volume;              /* 0..1 */
} OrcSound;

typedef struct {
    int    state;              /* LOOP_* */
    int    events, full;       /* eventi registrati; 1 se non ce ne stanno altri */
    double length;             /* battiti */
    double position;           /* battiti dall'inizio del giro; negativo mentre attende la battuta */
} OrcLoopInfo;

typedef struct Orc Orc;

Orc  *orc_create(float sample_rate);
void  orc_destroy(Orc *o);
void  orc_render(Orc *o, float *out, int frames);     /* stereo interlacciato */

/* Dal thread dell'interfaccia. */
void orc_set_sound(Orc *o, const OrcSound *s);
void orc_set_expression(Orc *o, float bend_semitones, float mod, float bright);
/* tag: valore libero dell'interfaccia (nome dell'accordo); torna da orc_layer_notes, anche dal loop */
void orc_chord_on(Orc *o, const int *notes, int count, int bass_note, float velocity, int tag);
void orc_chord_off(Orc *o);
void orc_note_on(Orc *o, int note, float velocity);   /* melodia, sopra gli accordi */
void orc_note_off(Orc *o, int note);
void orc_all_off(Orc *o);

void orc_set_tempo(Orc *o, float bpm);
void orc_transport(Orc *o, int on);                  /* orologio di batteria, looper, arpeggi */
void orc_set_beat(Orc *o, int on, int style, float level);

/* Looper (si comporta come i looper a pedale):
   registra: vuoto -> registra (dalla battuta, o subito a trasporto fermo); in registrazione -> chiude il
   giro (arrotondato alle battute) e suona; suona <-> sovraincisione; fermo -> sovraincisione.
   suona: suona <-> fermo; in registrazione chiude il giro. */
void orc_loop_record(Orc *o);
void orc_loop_play(Orc *o);
void orc_loop_clear(Orc *o);
void orc_loop_config(Orc *o, int max_bars, float quantize_beats);

/* Stato, per il disegno: letture atomiche, sicure dal thread dell'interfaccia. */
int    orc_transport_on(const Orc *o);
double orc_beats(const Orc *o);                       /* battiti da quando e' partito il trasporto */
int    orc_beat_step(const Orc *o);                   /* sedicesimo della batteria, -1 se tace */
const char *orc_beat_pattern(int style, int part);    /* 16 caratteri, 'x' = colpo (DRUM_*) */
void   orc_loop_info(const Orc *o, OrcLoopInfo *info);
int    orc_loop_marks(const Orc *o, float *beats, int *kinds, int max);   /* kinds: 0 accordo, 1 nota */
int    orc_layer_notes(const Orc *o, int layer, int *notes, int *tag);    /* accordo tenuto dallo strato */
void   orc_scope(const Orc *o, float *dst, int n);
int    orc_active_voices(const Orc *o);
int    orc_voice_starts(const Orc *o);                /* voci partite dalla creazione (per le prove) */

#endif
