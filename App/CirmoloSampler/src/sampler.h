/* Cirmolo Sampler - motore audio: campioni WAV su 16 pad, voci polifoniche e sequencer a passi.
 *
 * I campioni vengono convertiti al caricamento in float a 48 kHz (mono o stereo), cosi' nel thread
 * audio resta solo la lettura con interpolazione lineare (il pitch e' un passo di lettura diverso).
 * Ogni pad ha volume, intonazione, ritaglio (inizio/fine), modo (colpo singolo, tenuto, loop),
 * inversione e gruppo di esclusione (charleston aperto/chiuso). Il sequencer ha 16 passi per 16 pad,
 * 4 pattern concatenabili, swing, metronomo e registrazione dal vivo quantizzata.
 *
 * Thread: sampler_render() gira nel thread audio. Colpi, rilasci e cambi di campione arrivano
 * dall'interfaccia attraverso una coda senza lock (un produttore, un consumatore); i parametri dei pad
 * e del sequencer sono letti direttamente dalle strutture condivise (valori allineati: letture e
 * scritture atomiche su ARM64 e x86-64). I campioni sostituiti vengono restituiti all'interfaccia,
 * che li libera con sampler_collect(): nel thread audio non si alloca e non si libera mai memoria.
 * sampler_capture_feed() gira nel thread dell'ingresso (microfono USB).
 */
#ifndef CIRMOLO_SAMPLER_H
#define CIRMOLO_SAMPLER_H

#include <stdint.h>

#define SP_PADS 16
#define SP_PREVIEW SP_PADS            /* slot in piu' per l'anteprima della libreria */
#define SP_SLOTS (SP_PADS + 1)
#define SP_STEPS 16
#define SP_PATTERNS 4
#define SP_VOICES 24
#define SP_GROUPS 4
#define SP_SCOPE 2048
#define SP_RATE 48000
#define SP_REC_SECONDS 20
#define SP_REC_FRAMES (SP_RATE * SP_REC_SECONDS)
#define SP_MAX_SAMPLE_SECONDS 30

typedef struct {
    float *data;                      /* 48 kHz, mono o stereo interlacciato */
    int channels, frames;
    int src_rate, src_bits, src_channels;   /* com'era il file */
    char path[512];
    char name[48];                    /* nome del file senza estensione, per lo schermo */
} Sample;

enum { MODE_ONESHOT, MODE_HOLD, MODE_LOOP, MODE_COUNT };

typedef struct {
    float volume;                     /* 0..1.5 */
    float pitch;                      /* semitoni -12..12 */
    float start, end;                 /* ritaglio, frazioni 0..1 */
    int   mode;                       /* MODE_* */
    int   reverse;
    int   group;                      /* 0 = nessuno, 1..SP_GROUPS */
    int   mute, solo;                 /* valgono per il sequencer */
} PadParams;

typedef struct {
    uint8_t vel[SP_PADS][SP_STEPS];   /* 0 = spento, altrimenti velocity 1..127 */
    int length;                       /* passi suonati, 1..16 */
} SeqPattern;

enum { REC_IDLE, REC_WAIT, REC_ON, REC_DONE };

typedef struct Sampler Sampler;

/* ------------------------------------------------------------------ campioni */
/* Legge un WAV (PCM 8/16/24/32 bit o float, mono o stereo, 8-192 kHz) e lo converte a float 48 kHz
   con ricampionamento lineare; NULL se il file manca o non e' un WAV. */
Sample *sample_load_wav(const char *path);
void    sample_free(Sample *s);

/* ------------------------------------------------------------------ motore */
Sampler *sampler_create(float sample_rate);
void     sampler_destroy(Sampler *s);
float    sampler_sample_rate(const Sampler *s);

/* Thread audio: riempie frames campioni stereo interlacciati (L, R). */
void sampler_render(Sampler *s, float *out, int frames);

/* Pad (thread dell'interfaccia). sampler_set_sample prende possesso del campione (NULL svuota lo
   slot); quello vecchio viene liberato alla prossima sampler_collect, dopo che il thread audio l'ha
   lasciato. sampler_sample restituisce quello richiesto dall'interfaccia (anche se la coda non l'ha
   ancora applicato). */
void          sampler_set_sample(Sampler *s, int slot, Sample *smp);
const Sample *sampler_sample(const Sampler *s, int slot);
void          sampler_collect(Sampler *s);
PadParams    *sampler_pad(Sampler *s, int slot);
void          sampler_pad_defaults(PadParams *p);

/* Colpo: velocity 0..1, semitoni in piu' rispetto all'intonazione del pad (tasti cromatici), key -1 per
   il pad stesso oppure la nota MIDI (una voce per nota). Un pad in modo loop si avvia e si ferma a colpi
   alterni; in modo tenuto suona finche' non arriva il rilascio. */
void sampler_trigger(Sampler *s, int slot, float vel, float semitones, int key);
void sampler_release(Sampler *s, int slot, int key);     /* key -1: le voci del pad; -2: tutte le voci dello slot */
void sampler_release_all(Sampler *s);
void sampler_set_sustain(Sampler *s, int on);            /* CC64: i rilasci dei pad tenuti aspettano */
void sampler_set_bend(Sampler *s, float semitones);      /* pitch bend su tutte le voci */
void sampler_set_cutoff(Sampler *s, float v);            /* filtro passa-basso sull'uscita, 0..1 (1 = aperto) */
void  sampler_set_volume(Sampler *s, float v);
float sampler_volume(const Sampler *s);

/* ------------------------------------------------------------------ sequencer */
SeqPattern *sampler_pattern(Sampler *s);                 /* il pattern scelto (quello che si modifica) */
SeqPattern *sampler_pattern_at(Sampler *s, int index);
void        sampler_pattern_clear(SeqPattern *p);
/* Si suona il pattern scelto oppure, se la catena non e' vuota, i pattern della catena in ordine
   (bit 0 = A ... bit 3 = D). I cambi avvengono a fine battuta. */
void     sampler_select_pattern(Sampler *s, int index);
int      sampler_selected_pattern(const Sampler *s);
void     sampler_set_chain(Sampler *s, unsigned mask);
unsigned sampler_chain(const Sampler *s);
int      sampler_playing_pattern(const Sampler *s);      /* -1 se fermo */
void  sampler_set_tempo(Sampler *s, float bpm);          /* 40..240 */
float sampler_tempo(const Sampler *s);
void  sampler_set_swing(Sampler *s, float swing);        /* 0..0.6 */
float sampler_swing(const Sampler *s);
void  sampler_set_metronome(Sampler *s, int on);
int   sampler_metronome(const Sampler *s);
void  sampler_play(Sampler *s, int on);                  /* on = 1 riparte dal primo passo */
void  sampler_resume(Sampler *s);                        /* riprende da dove si era fermato (MIDI continue) */
int   sampler_playing(const Sampler *s);
int   sampler_current_step(const Sampler *s);            /* -1 se fermo */
/* Registrazione dal vivo: mentre il pattern gira, i colpi dei pad finiscono nel passo piu' vicino. */
void  sampler_set_record(Sampler *s, int on);
int   sampler_record(const Sampler *s);

/* ------------------------------------------------------------------ ingresso (microfono) */
void  sampler_capture_set_rate(Sampler *s, float rate);  /* 0 = nessun microfono */
float sampler_capture_rate(const Sampler *s);
void  sampler_capture_feed(Sampler *s, const float *in, int frames);   /* thread dell'ingresso, mono */
float sampler_capture_level(const Sampler *s);           /* picco recente 0..1 */
/* Registrazione: con soglia > 0 aspetta che il livello la superi (REC_WAIT), poi registra (REC_ON)
   fino a sampler_rec_stop o ai 20 secondi (REC_DONE). */
void  sampler_rec_start(Sampler *s, float threshold);
void  sampler_rec_stop(Sampler *s);
int   sampler_rec_state(const Sampler *s);
int   sampler_rec_frames(const Sampler *s);
/* Con REC_DONE: il buffer registrato (mono, 48 kHz), valido fino alla prossima rec_start; torna a REC_IDLE. */
const float *sampler_rec_take(Sampler *s, int *frames);

/* ------------------------------------------------------------------ per l'interfaccia */
void     sampler_scope(const Sampler *s, float *dst, int n);   /* ultimi campioni (mono) */
float    sampler_peak(const Sampler *s);
int      sampler_active_voices(const Sampler *s);
int      sampler_slot_voices(const Sampler *s, int slot);      /* voci che suonano in uno slot */
uint32_t sampler_slot_flash(const Sampler *s, int slot);       /* contatore che cresce a ogni colpo */
float    sampler_slot_position(const Sampler *s, int slot);    /* 0..1 nel campione dell'ultima voce, -1 se tace */

/* Esportazione (thread dell'interfaccia): rende il pattern scelto, o la catena, una volta sola piu'
   la coda, con un motore a parte che usa gli stessi campioni. Stereo interlacciato, da liberare con
   free(); NULL se non c'e' niente da suonare. */
float *sampler_render_pattern(Sampler *s, int *frames);

#endif
