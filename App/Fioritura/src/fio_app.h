/* Fioritura per Miyoo Flip - applicazione: un giardino generativo di suoni e luci.
 *
 * Due modi, che suonano insieme:
 * - Giardino (alla maniera di Bloom di Brian Eno e Peter Chilvers): ogni nota piantata fiorisce sullo
 *   schermo e torna a ogni giro del «respiro», sempre piu' piano, finche' la «memoria» non la lascia
 *   andare. L'altezza e' la riga (le corde della scala), il punto da sinistra a destra e' la posizione
 *   nello stereo. Il «sogno» pianta da solo quando non si tocca niente; il «vento» sposta i fiori.
 * - Lucciole (alla maniera di Otomata di Batuhan Bozkurt): in una griglia 8x8 ogni lucciola vola dritta;
 *   quando tocca un bordo suona la nota di quella riga o colonna e torna indietro; due lucciole che si
 *   incontrano girano a destra. Pochi semi bastano per giri che cambiano piano.
 * Sei atmosfere (strumento, scala, tonica, colori, spazio), un bordone, registrazione in WAV, tastiera
 * MIDI (pianta note esatte) e Launchpad Mini MK3 (griglia e LED a colori).
 */
#ifndef FIORITURA_APP_H
#define FIORITURA_APP_H

#include "fio_dsp.h"
#include "gfx.h"

enum { MODE_GARDEN, MODE_FIREFLIES };

typedef struct {
    float respiro;            /* secondi di un giro del giardino (4-40) */
    int   memoria;            /* ripetizioni di ogni fiore (1-16); 0 = per sempre */
    float vento;              /* 0..1: probabilita' che un fiore si sposti di una corda a ogni giro */
    float tempo;              /* battiti al minuto delle lucciole (un passo = un ottavo) */
    int   sogno;              /* 1: quando non si tocca niente il giardino pianta da solo */
    float volume;             /* 0..1 */
} FioGlobal;

/* Lucciola: riga (0 in basso) e colonna 0..7, direzione 0 su, 1 destra, 2 giu', 3 sinistra. */
typedef struct { int r, c, d; } Firefly;

/* Un passo delle lucciole (regole sopra). hits riceve i colpi sui bordi, senza doppioni, come
   lato * 8 + indice (lato 0 in alto, 1 a destra, 2 in basso, 3 a sinistra; indice = colonna per i lati
   in alto e in basso, riga per gli altri). Restituisce quanti colpi. */
int lucciole_step(Firefly *ff, int n, int *hits, int max);

typedef struct FioApp FioApp;

FioApp *fioapp_create(float sample_rate, const char *state_path);
void fioapp_destroy(FioApp *a);
void fioapp_audio(FioApp *a, float *out, int frames);
void fioapp_midi(FioApp *a, int port, const unsigned char *msg, int len);
void fioapp_midi_status(FioApp *a, const char *device);
void fioapp_button(FioApp *a, int pad, int pressed);
void fioapp_axes(FioApp *a, float lx, float ly, float rx, float ry, float l2, float r2);
void fioapp_update(FioApp *a, float dt);
void fioapp_draw(FioApp *a, Canvas *c);
int  fioapp_needs_draw(FioApp *a);
int  fioapp_wants_quit(const FioApp *a);
int  fioapp_save(FioApp *a);                          /* 0 se lo stato e' stato scritto */

/* Per le prove. */
Fio  *fioapp_engine(FioApp *a);
FioGlobal *fioapp_global(FioApp *a);
void fioapp_global_changed(FioApp *a);                /* dopo aver cambiato fioapp_global() */
int  fioapp_mode(const FioApp *a);
void fioapp_set_mode(FioApp *a, int mode);
int  fioapp_mood(const FioApp *a);
void fioapp_set_mood(FioApp *a, int mood);
int  fioapp_mood_count(void);
const char *fioapp_mood_name(int mood);
int  fioapp_mood_timbre(const FioApp *a);
void fioapp_set_cursor(FioApp *a, float x, float y);  /* giardino: 0..1 da sinistra e dal basso */
void fioapp_set_cell(FioApp *a, int row, int col);    /* lucciole */
int  fioapp_seed_count(const FioApp *a);
int  fioapp_seed_echoes(const FioApp *a, int i);      /* ripetizioni gia' fatte dall'i-esimo fiore vivo */
int  fioapp_firefly_count(const FioApp *a);
const Firefly *fioapp_fireflies(const FioApp *a);
int  fioapp_playing(const FioApp *a);                 /* lucciole in volo */
int  fioapp_panel(const FioApp *a);                   /* riga scelta del pannello, -1 se chiuso */
int  fioapp_help(const FioApp *a);
int  fioapp_recording(const FioApp *a);
const char *fioapp_record_path(const FioApp *a);
const char *fioapp_toast(const FioApp *a);
int  fioapp_dreaming(const FioApp *a);
int  fioapp_lp_connected(const FioApp *a);
void fioapp_set_record_dir(FioApp *a, const char *dir);

#endif
