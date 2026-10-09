/* Cirmolo Piano - logica dell'app e interfaccia, indipendenti dalla piattaforma. */
#ifndef CIRMOLO_PIANO_APP_H
#define CIRMOLO_PIANO_APP_H

#include "gfx.h"
#include "piano.h"
#include "platform.h"   /* tasti PAD_* (spruce/cirmolo-kit) */

enum { PAGE_PLAY, PAGE_INSTR, PAGE_OPTIONS, PAGE_COUNT };

typedef struct App App;

/* sf2_path serve solo per il messaggio a schermo se il SoundFont non si e' caricato. */
App *app_create(Piano *p, const char *state_path, const char *sf2_path);
void app_destroy(App *a);                 /* salva lo stato */
void app_save(App *a);

void app_button(App *a, int button, int pressed);
/* Levette -1..1 (su = negativo), grilletti 0..1: levetta sinistra = pitch bend, destra in su = vibrato. */
void app_axes(App *a, float lx, float ly, float rx, float ry, float l2, float r2);
/* Tastiera MIDI USB (spruce/cirmolo-kit/midi.h): un messaggio completo; nome del dispositivo o NULL. */
void app_midi(App *a, const unsigned char *msg, int len);
void app_midi_status(App *a, const char *device);
void app_update(App *a, float dt);
void app_draw(App *a, Canvas *c);
int  app_needs_draw(App *a);
int  app_wants_quit(const App *a);

/* Per le prove. */
void  app_set_page(App *a, int page);
int   app_page(const App *a);
int   app_instrument(const App *a);        /* indice del preset corrente (canale 0) */
void  app_set_instrument(App *a, int preset_index);
float app_volume(const App *a);
int   app_octave(const App *a);
int   app_key_lit(const App *a, int key);  /* tasto acceso sulla tastiera disegnata */

#endif
