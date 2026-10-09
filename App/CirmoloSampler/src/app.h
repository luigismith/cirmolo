/* Cirmolo Sampler - logica dell'app e interfaccia, indipendenti dalla piattaforma. */
#ifndef CIRMOLO_SAMPLER_APP_H
#define CIRMOLO_SAMPLER_APP_H

#include "gfx.h"
#include "platform.h"   /* tasti PAD_* (spruce/cirmolo-kit) */
#include "sampler.h"

enum { PAGE_PADS, PAGE_EDIT, PAGE_SEQ, PAGE_LIB, PAGE_REC, PAGE_OPTIONS, PAGE_COUNT };

typedef struct App App;

/* state_path: file di stato; le registrazioni vanno in <cartella dello stato>/campioni e le
   esportazioni in <cartella dello stato>/esportazioni. */
App *app_create(Sampler *s, const char *state_path);
void app_destroy(App *a);                 /* salva lo stato */
void app_save(App *a);

void app_button(App *a, int button, int pressed);
/* Levette -1..1 (su = negativo), grilletti 0..1: sinistra orizzontale = pitch bend, destra verticale
   = filtro passa-basso; nella pagina Modifica la destra verticale ritocca l'intonazione di un decimo. */
void app_axes(App *a, float lx, float ly, float rx, float ry, float l2, float r2);
/* Tastiera o pad MIDI USB (spruce/cirmolo-kit/midi.h): un messaggio completo; nome del dispositivo o NULL. */
void app_midi(App *a, const unsigned char *msg, int len);
void app_midi_status(App *a, const char *device);
/* Microfono USB: campioni mono dal thread dell'ingresso; nome e frequenza, o NULL se scollegato. */
void app_capture(App *a, const float *in, int frames);
void app_capture_status(App *a, const char *device, float sample_rate);
void app_update(App *a, float dt);
void app_draw(App *a, Canvas *c);
int  app_needs_draw(App *a);
int  app_wants_quit(const App *a);

/* Per le prove. */
void app_set_page(App *a, int page);
int  app_page(const App *a);
int  app_selected_pad(const App *a);
void app_select_pad(App *a, int pad);
int  app_bank(const App *a);
/* Assegna un WAV a un pad (lo carica); 0 se va bene. */
int  app_assign_wav(App *a, int pad, const char *path);
/* Cartelle della libreria (0 = registrazioni, 1 = libreria di serie), al posto di quelle della console. */
void app_set_library(App *a, const char *recordings_dir, const char *stock_dir);
const char *app_last_recording(const App *a);     /* percorso dell'ultima registrazione salvata ("" se nessuna) */
const char *app_last_export(const App *a);
int  app_export_pattern(App *a);                  /* 0 se il file e' stato scritto */
int  app_metronome(const App *a);
int  app_clock_follow(const App *a);

#endif
