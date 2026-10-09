/* Cirmolo Piano - motore General MIDI su TinySoundFont (spruce/cirmolo-kit/third_party/tsf.h).
 *
 * Carica un SoundFont (.sf2) e suona i suoi preset su 16 canali MIDI, con il canale 10 (indice 9)
 * riservato alla batteria come vuole il General MIDI.
 *
 * Thread: piano_render() gira nel thread audio ed e' l'unico a toccare tsf dopo il caricamento. Note,
 * controlli e cambi di strumento arrivano dal thread dell'interfaccia attraverso una coda senza lock
 * (un produttore, un consumatore); le voci sono preallocate (tsf_set_max_voices) e i 16 canali creati
 * al caricamento, cosi' nel thread audio non si alloca mai memoria.
 */
#ifndef CIRMOLO_PIANO_H
#define CIRMOLO_PIANO_H

#include <stdint.h>

#define PIANO_CHANNELS 16
#define PIANO_DRUM_CHANNEL 9
#define PIANO_VOICES 64
#define PIANO_SCOPE 2048

typedef struct Piano Piano;

Piano *piano_create(float sample_rate);
/* Carica il SoundFont: 0 se va tutto bene, -1 se il file manca o non e' valido. Va chiamata prima
   che parta l'audio (o comunque mentre piano_render non gira). */
int    piano_load(Piano *p, const char *sf2_path);
int    piano_loaded(const Piano *p);
void   piano_destroy(Piano *p);
float  piano_sample_rate(const Piano *p);

/* Thread audio: riempie frames campioni stereo interlacciati (L, R). */
void piano_render(Piano *p, float *out, int frames);

/* Thread dell'interfaccia. Canali 0..15, note 0..127, velocity 0..1. */
void piano_note_on(Piano *p, int ch, int key, float vel);
void piano_note_off(Piano *p, int ch, int key);
/* Control change come arriva dal MIDI (CC7 volume, CC64 sustain, CC0/32 banco, CC120/121/123...);
   CC1 (modulazione) diventa un vibrato, perche' tsf non ha i modulatori. */
void piano_control(Piano *p, int ch, int cc, int value);
void piano_pitch_bend(Piano *p, int ch, int value14);         /* 0..16383, 8192 = al centro */
/* Program change MIDI: usa il banco scelto con CC0/32 sul canale; sul canale 9 cerca una batteria. */
void piano_program(Piano *p, int ch, int program);
/* Strumento per indice di preset del SoundFont (0..piano_preset_count-1). */
void piano_set_preset(Piano *p, int ch, int preset_index);
void piano_all_notes_off(Piano *p);                           /* tutti i canali, con il rilascio */
void piano_all_sounds_off(Piano *p);                          /* tutti i canali, subito */

/* Volume generale 0..1 (letto dal thread audio). */
void  piano_set_volume(Piano *p, float v);
float piano_volume(const Piano *p);

/* Preset del SoundFont (fissi dopo il caricamento, leggibili da qualunque thread). */
int         piano_preset_count(const Piano *p);
const char *piano_preset_name(const Piano *p, int index);
int         piano_preset_bank(const Piano *p, int index);
int         piano_preset_program(const Piano *p, int index);
int         piano_find_preset(const Piano *p, int bank, int program);   /* -1 se non c'e' */
/* Preset attualmente assegnato a un canale (quello chiesto dall'interfaccia, anche se la coda non
   l'ha ancora applicato). */
int         piano_channel_preset(const Piano *p, int ch);

/* Per l'interfaccia: voci in uso, ultimi campioni (mono) e picco recente. */
int   piano_active_voices(const Piano *p);
void  piano_scope(const Piano *p, float *dst, int n);
float piano_peak(const Piano *p);

#endif
