/* Cirmolo Synth - motore audio.
 *
 * Tutto il suono e' calcolato qui, senza campioni: 8 voci polifoniche (o una monofonica con glide),
 * due oscillatori con anti-aliasing PolyBLEP, filtro passa-basso risonante (SVF), due inviluppi ADSR,
 * LFO, batteria sintetizzata, sequencer a 16 passi, delay ping-pong e riverbero.
 *
 * Thread: synth_render() gira nel thread audio. Note e colpi arrivano dal thread dell'interfaccia
 * attraverso una coda senza lock (un produttore, un consumatore); i parametri sono letti
 * direttamente dalle strutture condivise (float allineati: letture/scritture atomiche su ARM64 e x86-64).
 */
#ifndef CIRMOLO_SYNTH_H
#define CIRMOLO_SYNTH_H

#include <stdint.h>

#define SY_VOICES 8
#define SY_STEPS 16
#define SY_PATTERNS 4
#define SY_REC_FRAMES (48000 * 4)   /* buffer della registrazione: 4 secondi */
#define SY_SCOPE 2048

enum { WAVE_SAW, WAVE_SQUARE, WAVE_TRIANGLE, WAVE_SINE, WAVE_COUNT };
enum { ARP_OFF, ARP_UP, ARP_DOWN, ARP_UPDOWN, ARP_RANDOM, ARP_COUNT };
enum { DRUM_KICK, DRUM_SNARE, DRUM_HAT, DRUM_OPENHAT, DRUM_CLAP, DRUM_COUNT };

typedef struct {
    int   osc1_wave, osc2_wave;   /* WAVE_* */
    float osc2_semi;              /* -24..24 semitoni */
    float osc2_detune;            /* -50..50 centesimi */
    float osc_mix;                /* 0 = solo osc 1, 1 = solo osc 2 */
    float pulse_width;            /* 0.05..0.95, per l'onda quadra */
    float noise;                  /* 0..1 */
    float cutoff;                 /* 0..1 -> 20 Hz..18 kHz (esponenziale) */
    float resonance;              /* 0..1 */
    float env_amount;             /* -1..1 -> fino a 6 ottave */
    float key_track;              /* 0..1 */
    float f_attack, f_decay, f_sustain, f_release;   /* secondi, livello 0..1 */
    float a_attack, a_decay, a_sustain, a_release;
    float lfo_rate;               /* Hz */
    float lfo_pitch;              /* 0..1 -> fino a 1 semitono */
    float lfo_filter;             /* 0..1 -> fino a 2 ottave */
    int   mono;                   /* 1 = monofonico con glide e legato */
    float glide;                  /* secondi */
    float drive;                  /* 0..1, saturazione */
    float volume;                 /* 0..1 */
    int   arp_mode;               /* ARP_*: con l'arpeggiatore le note tenute vengono suonate a turno */
    float arp_rate;               /* sedicesimi per nota: 0.5, 1, 2, 4 */
    int   arp_octaves;            /* 1..3 */
} SynthPatch;

typedef struct {
    float delay_steps;            /* 1..8 sedicesimi (a tempo) */
    float delay_feedback;         /* 0..0.9 */
    float delay_mix;              /* 0..1 */
    float reverb_mix;             /* 0..1 */
    float reverb_size;            /* 0..1 */
    float drums_volume;           /* 0..1 */
} FxParams;

typedef struct {
    uint8_t drum[DRUM_COUNT][SY_STEPS];   /* 0 spento, 1 acceso, 2 accento */
    int8_t  note[SY_STEPS];               /* -1 pausa, altrimenti grado della scala 0..15 */
    uint8_t accent[SY_STEPS];
} Pattern;

typedef struct Synth Synth;

Synth *synth_create(float sample_rate);
void   synth_destroy(Synth *s);
float  synth_sample_rate(const Synth *s);

/* Thread audio: riempie frames campioni stereo interlacciati (L, R). */
void synth_render(Synth *s, float *out, int frames);

/* Thread dell'interfaccia. */
void synth_note_on(Synth *s, int midi_note, float velocity);
void synth_note_off(Synth *s, int midi_note);
void synth_all_notes_off(Synth *s);
void synth_drum_hit(Synth *s, int drum, float velocity);

SynthPatch *synth_patch(Synth *s);
FxParams   *synth_fx(Synth *s);
Pattern    *synth_pattern(Synth *s);                 /* il pattern scelto (quello che si modifica) */
Pattern    *synth_pattern_at(Synth *s, int index);
/* Quattro pattern: si suona quello scelto oppure, se la catena non e' vuota, i pattern della catena
   in ordine (bit 0 = A ... bit 3 = D). I cambi avvengono a fine battuta. */
void synth_select_pattern(Synth *s, int index);
int  synth_selected_pattern(const Synth *s);
void synth_set_chain(Synth *s, unsigned mask);
unsigned synth_chain(const Synth *s);
int  synth_playing_pattern(const Synth *s);

/* Registrazione: il thread audio scrive in un buffer circolare, l'interfaccia lo svuota su file. */
void synth_rec_start(Synth *s);
void synth_rec_stop(Synth *s);
int  synth_rec_active(const Synth *s);
int  synth_rec_read(Synth *s, int16_t *dst, int max_frames);   /* campioni stereo interlacciati */
uint32_t synth_rec_dropped(const Synth *s);

/* Controlli dal vivo: bend in semitoni, filtro in ottave, risonanza aggiunta, vibrato 0..1. */
void synth_set_performance(Synth *s, float bend, float cutoff_oct, float res_add, float vibrato);

/* Sequencer. La scala serve a tradurre i gradi del pattern in note MIDI. */
void  synth_set_tempo(Synth *s, float bpm);
float synth_tempo(const Synth *s);
void  synth_set_swing(Synth *s, float swing);       /* 0..0.6 */
float synth_swing(const Synth *s);
void  synth_set_scale(Synth *s, int root_midi, const int *intervals, int count);
int   synth_degree_to_midi(const Synth *s, int degree);
void  synth_play(Synth *s, int on);
int   synth_playing(const Synth *s);
int   synth_current_step(const Synth *s);           /* -1 se fermo */

/* Per l'interfaccia: ultimi campioni (mono) e livello di picco recente. */
void  synth_scope(const Synth *s, float *dst, int n);
float synth_peak(Synth *s);
int   synth_active_voices(const Synth *s);
uint32_t synth_drum_flash(const Synth *s, int drum);  /* contatore che cresce a ogni colpo */

#endif
