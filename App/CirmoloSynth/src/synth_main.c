/* Cirmolo Synth - collegamento con il kit delle app native (spruce/cirmolo-kit/platform.c). */
#include <stdlib.h>

#include "app.h"
#include "platform.h"
#include "synth.h"

typedef struct { Synth *synth; App *app; } SynthApp;

static void *create(float sample_rate, const char *state_path)
{
    SynthApp *sa = calloc(1, sizeof(SynthApp));
    if (!sa) return NULL;
    sa->synth = synth_create(sample_rate);
    sa->app = app_create(sa->synth, state_path);
    return sa;
}

static void destroy(void *p)
{
    SynthApp *sa = p;
    app_destroy(sa->app);
    synth_destroy(sa->synth);
    free(sa);
}

static void audio(void *p, float *out, int frames) { synth_render(((SynthApp *)p)->synth, out, frames); }
static void button(void *p, int pad, int pressed) { app_button(((SynthApp *)p)->app, pad, pressed); }
static void midi(void *p, const unsigned char *m, int len) { app_midi(((SynthApp *)p)->app, m, len); }
static void midi_status(void *p, const char *dev) { app_midi_status(((SynthApp *)p)->app, dev); }
static void axes(void *p, float lx, float ly, float rx, float ry, float l2, float r2) { app_axes(((SynthApp *)p)->app, lx, ly, rx, ry, l2, r2); }
static void update(void *p, float dt) { app_update(((SynthApp *)p)->app, dt); }
static void draw(void *p, Canvas *c) { app_draw(((SynthApp *)p)->app, c); }
static int wants_quit(void *p) { return app_wants_quit(((SynthApp *)p)->app); }

const CirmoloApp *cirmolo_app(void)
{
    static const CirmoloApp desc = {
        .title = "Cirmolo Synth",
        .state_path = "/mnt/SDCARD/Saves/cirmolo-synth/stato.txt",
        .create = create, .destroy = destroy, .audio = audio, .midi = midi, .midi_status = midi_status,
        .button = button, .axes = axes, .update = update, .draw = draw, .wants_quit = wants_quit,
    };
    return &desc;
}
