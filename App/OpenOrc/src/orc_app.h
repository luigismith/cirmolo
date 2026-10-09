/* OpenOrc per Miyoo Flip - applicazione: accordi, MPK mini IV, pagine e stato. */
#ifndef OPENORC_APP_H
#define OPENORC_APP_H

#include <stddef.h>

#include "gfx.h"
#include "orc_dsp.h"

enum { PAGE_PLAY, PAGE_SOUND, PAGE_RHYTHM, PAGE_MIDI, PAGE_COUNT };

typedef struct OrcApp OrcApp;

OrcApp *orcapp_create(float sample_rate, const char *state_path);
void orcapp_destroy(OrcApp *a);
void orcapp_audio(OrcApp *a, float *out, int frames);
void orcapp_midi(OrcApp *a, int port, const unsigned char *msg, int len);
void orcapp_midi_status(OrcApp *a, const char *device);
void orcapp_button(OrcApp *a, int pad, int pressed);
void orcapp_axes(OrcApp *a, float lx, float ly, float rx, float ry, float l2, float r2);
void orcapp_update(OrcApp *a, float dt);
void orcapp_draw(OrcApp *a, Canvas *c);
int  orcapp_needs_draw(OrcApp *a);                    /* 0 se lo schermo resterebbe uguale */
int  orcapp_wants_quit(const OrcApp *a);

/* Preset della tastiera MIDI (file di testo, vedi tastiere/ nell'app): 0 se riuscito. */
int  orcapp_load_keyboard(OrcApp *a, const char *path, char *name, size_t name_n);
int  orcapp_save_keyboard(OrcApp *a, const char *path, const char *name);

/* Per le prove. */
Orc *orcapp_engine(OrcApp *a);
void orcapp_set_page(OrcApp *a, int page);
void orcapp_select(OrcApp *a, int row);               /* riga scelta nella pagina a elenco corrente */
const char *orcapp_chord_name(const OrcApp *a);       /* ultimo accordo suonato dal vivo */
int  orcapp_chord_notes(const OrcApp *a, int *notes, int *bass);
int  orcapp_chord_live(const OrcApp *a);
const OrcSound *orcapp_sound(const OrcApp *a);
int  orcapp_keymode(const OrcApp *a);
void orcapp_save(OrcApp *a);

#endif
