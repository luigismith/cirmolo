/* OpenOrc per Miyoo Flip - motore audio.
 *
 * Clone dell'Orchid (Telepathic Instruments) pensato per la Akai MPK mini IV: accordi eseguiti in
 * 6 modi (accordo, strum, slop, arpeggio, pattern, arpa), 6 motori di suono (analogico, FM, piano
 * elettrico, organo, corda pizzicata, ensemble, coro) da 16 voci in stereo, basso monofonico, batteria con
 * 4 stili, looper a strati con annulla, chorus, delay, riverbero con shimmer, nastro e pompa sul battito.
 *
 * Stereo: ogni voce ha una posizione (le note dell'accordo dalla piu' grave a sinistra alla piu' acuta a
 * destra, gli arpeggi che si spostano da un lato all'altro); OrcSound.stereo dice quanto si allargano.
 * A 0 tutto e' al centro, come le versioni mono. L'ensemble apre anche i suoi oscillatori e il piano
 * elettrico fa il tremolo da un lato all'altro.
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

enum { ENG_VA, ENG_FM, ENG_REED, ENG_ORGAN, ENG_PLUCK, ENG_SUPER, ENG_VOICE, ENG_COUNT };
enum { PERF_CHORD, PERF_STRUM, PERF_SLOP, PERF_ARP, PERF_PATTERN, PERF_HARP, PERF_COUNT };
enum { BEAT_DISCO, BEAT_PSYCH, BEAT_TRAP, BEAT_BOSSA, BEAT_COUNT };
enum { DRUM_KICK, DRUM_SNARE, DRUM_HAT, DRUM_OPEN, DRUM_CLAP, DRUM_PARTS };
enum { ORC_LIVE, ORC_LOOP, ORC_LAYERS };
enum { LOOP_EMPTY, LOOP_ARMED, LOOP_RECORDING, LOOP_PLAYING, LOOP_OVERDUB, LOOP_STOPPED };

typedef struct {
    int   engine;              /* ENG_* */
    float tone;                /* 0..1: brillantezza (taglio del filtro, indice FM, armoniche) */
    float character;           /* 0..1: VA desintonia e forma, FM rapporto, reed spinta, organo registri,
                                  corda durata, ensemble desintonia, coro vocale (U O A E I) */
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
    float stereo;              /* 0..1: ampiezza stereo (0 = tutto al centro) */
    float resonance;           /* 0..1: risonanza del filtro (analogico ed ensemble), con piu' inviluppo */
    float shimmer;             /* 0..1: il riverbero rimanda un'ottava sopra (serve il riverbero) */
    float tape;                /* 0..1: nastro: wow, flutter, saturazione, fruscio */
    float pump;                /* 0..1: il suono respira sul battito (a trasporto in moto) */
} OrcSound;

typedef struct {
    int    state;              /* LOOP_* */
    int    events, full;       /* eventi registrati; 1 se non ce ne stanno altri */
    int    layers;             /* sovraincisioni sopra il primo giro (quelle che annulla puo' togliere) */
    double length;             /* battiti */
    double position;           /* battiti dall'inizio del giro; negativo mentre attende la battuta */
} OrcLoopInfo;

/* Un evento del loop, per salvarlo e riprenderlo. type: 0 accordo, 1 fine dell'accordo, 2 nota, 3 fine
   della nota; t in battiti dall'inizio del giro; pass 0 = primo giro, poi le sovraincisioni. */
typedef struct {
    float t;
    int   type, pass, n, a, tag;   /* a: basso dell'accordo o nota */
    float b;                       /* dinamica */
    int   notes[ORC_MAX_CHORD];
} OrcLoopEvent;

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
/* Annulla: in sovraincisione toglie quella in corso; altrimenti l'ultima sovraincisione; con il solo primo
   giro (o mentre registra) cancella tutto. */
void orc_loop_undo(Orc *o);
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
/* Salvataggio del loop: solo con l'audio fermo (prima che parta o dopo che si e' chiuso). L'import lascia
   il loop fermo, pronto a suonare. */
int    orc_loop_export(const Orc *o, OrcLoopEvent *ev, int max, double *length, int *layers);
void   orc_loop_import(Orc *o, const OrcLoopEvent *ev, int n, double length, int layers);
int    orc_active_voices(const Orc *o);
int    orc_voice_starts(const Orc *o);                /* voci partite dalla creazione (per le prove) */

#endif
