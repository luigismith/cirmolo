/* Diapason - accordatore, note di riferimento e metronomo (vedi diapason.c). */
#ifndef DIAPASON_H
#define DIAPASON_H

#include <stdint.h>

#include "gfx.h"

typedef struct Diapason Diapason;

Diapason *diapason_create(float sample_rate, const char *state_path);
void diapason_destroy(Diapason *d);                       /* salva le impostazioni */
void diapason_audio(Diapason *d, float *out, int frames); /* thread audio: stereo interlacciato */
void diapason_capture(Diapason *d, const float *in, int frames);
void diapason_capture_status(Diapason *d, const char *device, float sample_rate);
void diapason_button(Diapason *d, int pad, int pressed);
void diapason_update(Diapason *d, float dt);
void diapason_draw(Diapason *d, Canvas *c);
int  diapason_wants_quit(const Diapason *d);

/* Per i test. */
float diapason_freq(const Diapason *d);
int   diapason_note(const Diapason *d);
float diapason_cents(const Diapason *d);
void  diapason_set_page(Diapason *d, int page);
void  diapason_set_instrument(Diapason *d, int index);
uint32_t diapason_beat_serial(const Diapason *d);

#endif
