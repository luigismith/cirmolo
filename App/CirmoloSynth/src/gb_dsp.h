/* Cirmolo Synth - groovebox: motore audio.
 *
 * Tutto e' sintetizzato, senza campioni. Quattordici tracce suonano insieme:
 *   - 12 tracce di batteria (GB_DRUMS), ognuna con uno strumento scelto fra ~30 modelli (casse 808/909/
 *     acustica/FM, rullanti, battimani, charleston, piatti, tom, conga, campanaccio, shaker, chiptune...).
 *     Un "kit" (GB_KIT_COUNT, in gb_kits.c) assegna modello e parametri alle 12 tracce.
 *   - traccia ACIDO: basso monofonico alla 303 (dente di sega o quadra, filtro a 4 poli risonante,
 *     inviluppo, accento, slide);
 *   - traccia PIEGA: synth West Coast a 4 voci (oscillatore complesso con wavefolder e low-pass gate).
 * Sequencer nel thread audio, a passi di sedicesimo, con lunghezza propria per traccia (poliritmi),
 * probabilita', condizioni (fill, 1 su 2...), ripetizioni rapide (ratchet), spostamento fine e
 * parametri bloccati per passo. Mixer con volume, pan, mute, solo, mandate a delay e riverbero.
 * Uscita: compressore con pompa sulla cassa, filtro DJ, lo-fi, saturazione a nastro.
 *
 * Thread: gb_render() gira nel thread audio. Le azioni dall'interfaccia (colpi, note, trasporto, cambio
 * di pattern) passano da una coda senza lock (un produttore, un consumatore). Pattern, suoni e mixer sono
 * strutture condivise che l'interfaccia scrive direttamente: campi int/float/uint8_t allineati, letture e
 * scritture atomiche su ARM64 e x86-64; un passo puo' cambiare a meta' lettura senza danni.
 */
#ifndef CIRMOLO_GB_DSP_H
#define CIRMOLO_GB_DSP_H

#include <stdint.h>

#define GB_RATE 48000
#define GB_DRUMS 12
#define GB_ACID GB_DRUMS            /* indice della traccia del basso acido */
#define GB_FOLD (GB_DRUMS + 1)      /* indice della traccia Piega */
#define GB_TRACKS (GB_DRUMS + 2)
#define GB_MAX_STEPS 64             /* fino a 4 pagine da 16 */
#define GB_PATTERNS 16
#define GB_FOLD_VOICES 4
#define GB_SCOPE 2048
#define GB_REC_FRAMES (GB_RATE * 4) /* buffer circolare della registrazione: 4 secondi */

/* Modelli di batteria. */
enum {
    DM_KICK_808, DM_KICK_909, DM_KICK_ACOUSTIC, DM_KICK_FM, DM_KICK_HARD,    /* HARD: gabber, distorta */
    DM_SNARE_808, DM_SNARE_909, DM_SNARE_ACOUSTIC, DM_SNARE_BRUSH,          /* BRUSH: spazzola */
    DM_RIM, DM_CLAP, DM_SNAP,                                               /* SNAP: schiocco di dita */
    DM_HAT_CLOSED, DM_HAT_OPEN, DM_HAT_909, DM_CYMBAL, DM_RIDE,             /* HAT_909: metallico FM */
    DM_TOM, DM_TOM_ELECTRO,                                                 /* ELECTRO: Simmons */
    DM_CONGA, DM_BONGO, DM_TABLA,                                           /* TABLA: con la caduta d'altezza */
    DM_COWBELL, DM_CLAVE, DM_WOODBLOCK, DM_SHAKER, DM_TAMBOURINE, DM_TRIANGLE,
    DM_CHIP_KICK, DM_CHIP_SNARE, DM_CHIP_HAT,                               /* 8 bit, rumore LFSR */
    DM_ZAP, DM_GLITCH, DM_NOISE_SWEEP,                                      /* laser, bit sbriciolati, soffio */
    DM_COUNT
};

/* Parametri di uno strumento di batteria (tutti 0..1 salvo dove indicato). */
typedef struct {
    int   model;          /* DM_* */
    float tune;           /* semitoni -24..24 rispetto all'altezza tipica del modello */
    float decay;          /* durata: 0 cortissima, 1 lunghissima (per il modello) */
    float tone;           /* colore: filtro, brillantezza, rapporto fra corpo e rumore */
    float snap;           /* attacco: clic, schiocco, caduta d'altezza (il senso dipende dal modello) */
    float drive;          /* saturazione */
    float level;          /* volume 0..1 */
    float pan;            /* -1 sinistra .. 1 destra */
    int   choke;          /* 0 nessuno; tracce con lo stesso gruppo si zittiscono a vicenda (charleston) */
} GbDrum;

typedef struct {
    int   wave;           /* 0 dente di sega, 1 quadra */
    float tune;           /* semitoni -24..24 */
    float cutoff;         /* 0..1 */
    float resonance;      /* 0..1 */
    float env_mod;        /* 0..1 */
    float decay;          /* 0..1 */
    float accent;         /* 0..1: quanto pesa l'accento (filtro e volume) */
    float drive;          /* 0..1 */
    float level, pan;
} GbAcid;

typedef struct {
    float tune;           /* semitoni -24..24 */
    float fold;           /* 0..1: quante pieghe (timbro) */
    float symmetry;       /* 0..1: 0.5 simmetrico */
    float mod_ratio;      /* 0..1 -> rapporto del modulatore 0.5..8 (scatti armonici sotto 0.75) */
    float mod_amount;     /* 0..1 */
    float decay;          /* 0..1: durata del low-pass gate */
    float gate_color;     /* 0..1: 0 solo volume, 1 anche filtro (piu' "bongo") */
    float release;        /* 0..1: quanto resta dopo la fine del passo */
    float level, pan;
} GbFold;

/* Passo del sequencer. Per le batterie conta vel, prob, cond, ratchet, micro e i blocchi; per le
   tracce melodiche anche note, slide e len. */
enum { COND_ALWAYS, COND_FILL, COND_NOT_FILL, COND_1_2, COND_2_2, COND_1_4, COND_FIRST, COND_COUNT };
#define GB_LOCK_NONE (-128)
typedef struct {
    uint8_t on;           /* 0 spento, 1 acceso */
    uint8_t vel;          /* 1..127 */
    uint8_t prob;         /* 1..100 (%) */
    uint8_t cond;         /* COND_* (FILL: solo mentre si tiene il fill; 1_2: al primo giro di due...) */
    uint8_t ratchet;      /* 1..4 colpi nel passo */
    int8_t  micro;        /* -50..50: spostamento in % di passo */
    int8_t  note;         /* melodiche: semitoni da Do3 (MIDI 48), -24..48 */
    uint8_t slide;        /* melodiche: legato con portamento verso il passo dopo (acido) */
    uint8_t accent;       /* melodiche: accento */
    uint8_t len;          /* melodiche: durata in passi 1..16 (Piega; l'acido usa slide) */
    int8_t  lock_tune;    /* GB_LOCK_NONE oppure valore bloccato per questo passo: semitoni -24..24 */
    int8_t  lock_decay;   /* GB_LOCK_NONE oppure 0..100 */
    int8_t  lock_tone;    /* GB_LOCK_NONE oppure 0..100 (batterie: tone; acido: cutoff; Piega: fold) */
} GbStep;

typedef struct {
    GbStep  step[GB_MAX_STEPS];
    uint8_t length;       /* 1..64 */
    uint8_t speed;        /* 0 = 1x, 1 = 2x (trentaduesimi), 2 = 1/2x (ottavi), 3 = 3/4x (terzine) */
} GbTrackPattern;

typedef struct {
    GbTrackPattern track[GB_TRACKS];
    char name[24];
} GbPattern;

typedef struct {
    float level;          /* 0..1 volume della traccia (oltre a quello dello strumento) */
    float delay_send, reverb_send;
    uint8_t mute, solo;
} GbMix;

typedef struct {
    float bpm;            /* 40..240 */
    float swing;          /* 0..0.75 sui sedicesimi pari: ritardo in sedicesimi (0.33 = terzina) */
    float delay_time;     /* in sedicesimi 1..8 (ping-pong a tempo) */
    float delay_feedback; /* 0..0.9 */
    float delay_tone;     /* 0..1 scuro..chiaro */
    float reverb_size;    /* 0..1 */
    float reverb_damp;    /* 0..1 */
    float comp;           /* 0..1 compressione del bus */
    float pump;           /* 0..1 il bus "respira" sulla cassa (traccia 0) */
    float filter;         /* -1..1: <0 passa-basso, >0 passa-alto, 0 aperto (filtro DJ) */
    float filter_res;     /* 0..1 */
    float lofi;           /* 0..1 riduzione di bit e frequenza */
    float tape;           /* 0..1 saturazione e un po' di wow */
    float volume;         /* 0..1 */
} GbMaster;

typedef struct Gb Gb;

Gb   *gb_create(float sample_rate);
void  gb_destroy(Gb *g);
float gb_sample_rate(const Gb *g);

/* Thread audio: frames campioni stereo interlacciati (L, R). */
void  gb_render(Gb *g, float *out, int frames);

/* --- dall'interfaccia: strutture condivise --- */
GbDrum   *gb_drum(Gb *g, int track);              /* 0..GB_DRUMS-1 */
GbAcid   *gb_acid(Gb *g);
GbFold   *gb_fold(Gb *g);
GbMix    *gb_mix(Gb *g, int track);                /* 0..GB_TRACKS-1 */
GbMaster *gb_master(Gb *g);
GbPattern *gb_pattern(Gb *g, int index);           /* 0..GB_PATTERNS-1 */

/* Kit: copia modello e parametri del kit nelle 12 tracce (non tocca pattern e mixer). */
int         gb_kit_count(void);
const char *gb_kit_name(int kit);
const char *gb_kit_track_name(int kit, int track);  /* nome dello strumento nel kit: "Cassa", "Conga alta"... */
void        gb_load_kit(Gb *g, int kit);
const char *gb_model_name(int model);               /* nome italiano del modello: "Cassa 808"... */

/* --- dall'interfaccia: azioni (coda verso il thread audio) --- */
void gb_hit(Gb *g, int track, float velocity);      /* suona subito una batteria (anteprima, pad, MIDI) */
void gb_note_on(Gb *g, int track, int midi_note, float velocity);   /* GB_ACID o GB_FOLD, dal vivo */
void gb_note_off(Gb *g, int track, int midi_note);
void gb_all_notes_off(Gb *g);

void gb_play(Gb *g, int on);                        /* parte dall'inizio del pattern */
int  gb_playing(const Gb *g);
/* Pattern che suona; il cambio avviene alla fine del giro del pattern in corso (o subito se fermo).
   Con una catena non vuota si suonano in ordine i pattern della catena, a ripetizione. */
void gb_select_pattern(Gb *g, int index);
int  gb_playing_pattern(const Gb *g);               /* da fermo: quello che partira' */
void gb_set_chain(Gb *g, const int *patterns, int count);   /* count 0 = niente catena; massimo 64 */
int  gb_chain_position(const Gb *g);                 /* -1 senza catena */
void gb_set_fill(Gb *g, int on);                     /* tenuto: valgono i passi COND_FILL */
/* Stutter dal vivo: ripete l'ultimo frammento lungo div sedicesimi (0 spento; 4 = 1/4, 2 = 1/8, 1 = 1/16,
   0.5 = 1/32) finche' non si toglie. */
void gb_set_repeat(Gb *g, float div);

/* Posizione per l'interfaccia: passo corrente della traccia (-1 se fermo) e contatore dei colpi. */
int      gb_track_step(const Gb *g, int track);
uint32_t gb_track_flash(const Gb *g, int track);     /* cresce a ogni colpo suonato */
float    gb_track_level(Gb *g, int track);           /* livello recente 0..1 (per i led del mixer) */
int      gb_bar_count(const Gb *g);                  /* giri del pattern completati da quando suona */
int      gb_active_voices(const Gb *g, int track);   /* voci che suonano: batteria 0..2, ACID 0/1, FOLD 0..4 */

/* Registrazione dell'uscita: il thread audio scrive in un buffer circolare, l'interfaccia lo svuota. */
void     gb_rec_start(Gb *g);
void     gb_rec_stop(Gb *g);
int      gb_rec_active(const Gb *g);
int      gb_rec_read(Gb *g, int16_t *dst, int max_frames);
uint32_t gb_rec_dropped(const Gb *g);

/* Uscita a tempo per la sincronizzazione esterna: chiamata nel thread audio a ogni ventiquattresimo di
   battito (clock MIDI, 24 ppq), con lo scarto in campioni dall'inizio del blocco. Facoltativa. */
void gb_set_clock_hook(Gb *g, void (*hook)(void *user, int frame_offset), void *user);

/* Per l'interfaccia. */
void  gb_scope(const Gb *g, float *dst, int n);
float gb_peak(Gb *g);
float gb_cpu_load(const Gb *g);                      /* frazione del tempo del blocco usata dal render */

/* Generatore euclideo: riempie i primi length passi della traccia con hits colpi distribuiti, ruotati
   (rotate > 0 sposta a destra); mette anche la lunghezza della traccia a length. */
void gb_euclid(GbTrackPattern *t, int length, int hits, int rotate, int velocity);

/* Ritmi pronti (gb_kits.c): un pattern di batteria completo per stile, con nome e kit consigliato. */
int         gb_groove_count(void);
const char *gb_groove_name(int i);
int         gb_groove_kit(int i);
float       gb_groove_bpm(int i);
float       gb_groove_swing(int i);
void        gb_groove_load(int i, GbPattern *dst);   /* tocca solo le 12 tracce di batteria */

/* Rendering offline (prove ed esportazione): suona n frame senza il thread audio. */
void gb_render_offline(Gb *g, float *out, int frames);

#endif
