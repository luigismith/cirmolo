/* Cirmolo Synth - groovebox: logica dell'app e interfaccia (640x480, tasti della Flip, MIDI).
 *
 * Cinque pagine, SELECT passa alla successiva:
 *   Ritmo    griglia di 14 tracce (12 batterie, Acido, Piega) per 16 passi, pagine fino a 64 passi;
 *            X apre la scheda del passo e della traccia (probabilita', condizioni, ripetizioni, euclideo...)
 *   Suoni    kit (26), strumento e parametri di ogni traccia
 *   Mixer    volume, pan, mandate, mute e solo delle tracce; tempo, swing ed effetti del master
 *   Live     i 12 tasti suonano le 12 batterie (anche registrando nel pattern) o le zittiscono
 *   Pattern  16 pattern, catena, ritmi pronti, esportazione in WAV
 * Su tutte le pagine: levetta sinistra filtro DJ, levetta destra su stutter e giu' fill, START suona o
 * ferma, R3 registra l'uscita, MENU esce.
 */
#ifndef CIRMOLO_GB_APP_H
#define CIRMOLO_GB_APP_H

#include "gb_dsp.h"
#include "gfx.h"
#include "platform.h"

enum { SCR_RITMO, SCR_SUONI, SCR_MIXER, SCR_LIVE, SCR_PATTERN, SCR_COUNT };

typedef struct App App;

App *app_create(Gb *g, const char *state_path);
void app_destroy(App *a);                 /* salva lo stato */
void app_save(App *a);
void app_button(App *a, int pad, int pressed);
void app_axes(App *a, float lx, float ly, float rx, float ry, float l2, float r2);
void app_midi_port(App *a, int port, const unsigned char *m, int len);
void app_midi_status(App *a, const char *device);
void app_update(App *a, float dt);
void app_draw(App *a, Canvas *c);
int  app_needs_draw(App *a);
int  app_wants_quit(const App *a);

/* Per le prove. */
int  app_screen(const App *a);
void app_set_screen(App *a, int screen);
int  app_edit_pattern(const App *a);
void app_set_cursor(App *a, int step, int track);
void app_open_editor(App *a, int open);
void app_set_mixer(App *a, int strip, int row);   /* strip GB_TRACKS = master, row = sua voce */
int  app_export_wav(App *a, const char *path, int loops);   /* 0 se riuscita */

#endif
