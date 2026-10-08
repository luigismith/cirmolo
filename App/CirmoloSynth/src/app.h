/* Cirmolo Synth - logica dell'app e interfaccia, indipendenti dalla piattaforma. */
#ifndef CIRMOLO_APP_H
#define CIRMOLO_APP_H

#include "gfx.h"
#include "synth.h"

enum {
    PAD_UP, PAD_DOWN, PAD_LEFT, PAD_RIGHT,
    PAD_A, PAD_B, PAD_X, PAD_Y,
    PAD_L1, PAD_R1, PAD_L2, PAD_R2,
    PAD_SELECT, PAD_START, PAD_MENU,
    PAD_COUNT
};

enum { SCREEN_PLAY, SCREEN_SOUND, SCREEN_SEQ, SCREEN_COUNT };

typedef struct App App;

App *app_create(Synth *synth, const char *state_path);
void app_destroy(App *a);                 /* salva lo stato */
void app_save(App *a);

void app_button(App *a, int button, int pressed);
/* Levette -1..1 (su = negativo), grilletti 0..1. */
void app_axes(App *a, float lx, float ly, float rx, float ry, float l2, float r2);
void app_update(App *a, float dt);
void app_draw(App *a, Canvas *c);
int  app_wants_quit(const App *a);

/* Per i test. */
void app_set_screen(App *a, int screen);
int  app_screen(const App *a);
int  app_preset_count(void);
void app_load_preset(App *a, int index);
const char *app_preset_name(int index);

#endif
