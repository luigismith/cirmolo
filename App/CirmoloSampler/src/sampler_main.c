/* Cirmolo Sampler - collegamento con il kit delle app native (spruce/cirmolo-kit/platform.c).
 *
 * Chiede al kit anche l'ingresso audio (microfono USB) e tutte le porte MIDI del dispositivo collegato
 * (la MPK mini IV manda i tasti di trasporto su una porta diversa da pad e tasti).
 */
#include <stdlib.h>

#include "app.h"
#include "platform.h"
#include "sampler.h"

typedef struct { Sampler *sampler; App *app; } SamplerApp;

static void *create(float sample_rate, const char *state_path)
{
    SamplerApp *sa = calloc(1, sizeof(SamplerApp));
    if (!sa) return NULL;
    sa->sampler = sampler_create(sample_rate);
    if (!sa->sampler) { free(sa); return NULL; }
    sa->app = app_create(sa->sampler, state_path);
    if (!sa->app) { sampler_destroy(sa->sampler); free(sa); return NULL; }
    return sa;
}

static void destroy(void *p)
{
    SamplerApp *sa = p;
    app_destroy(sa->app);
    sampler_destroy(sa->sampler);
    free(sa);
}

static void audio(void *p, float *out, int frames) { sampler_render(((SamplerApp *)p)->sampler, out, frames); }
static void capture(void *p, const float *in, int frames) { app_capture(((SamplerApp *)p)->app, in, frames); }
static void capture_status(void *p, const char *dev, float rate) { app_capture_status(((SamplerApp *)p)->app, dev, rate); }
static void button(void *p, int pad, int pressed) { app_button(((SamplerApp *)p)->app, pad, pressed); }
static void midi_port(void *p, int port, const unsigned char *m, int len) { (void)port; app_midi(((SamplerApp *)p)->app, m, len); }
static void midi_status(void *p, const char *dev) { app_midi_status(((SamplerApp *)p)->app, dev); }
static void axes(void *p, float lx, float ly, float rx, float ry, float l2, float r2) { app_axes(((SamplerApp *)p)->app, lx, ly, rx, ry, l2, r2); }
static void update(void *p, float dt) { app_update(((SamplerApp *)p)->app, dt); }
static void draw(void *p, Canvas *c) { app_draw(((SamplerApp *)p)->app, c); }
static int needs_draw(void *p) { return app_needs_draw(((SamplerApp *)p)->app); }
static int wants_quit(void *p) { return app_wants_quit(((SamplerApp *)p)->app); }

const CirmoloApp *cirmolo_app(void)
{
    static const CirmoloApp desc = {
        .title = "Cirmolo Sampler",
        .state_path = "/mnt/SDCARD/Saves/sampler/stato.txt",
        .wants_capture = 1,
        .create = create, .destroy = destroy, .audio = audio, .capture = capture, .capture_status = capture_status,
        .midi_port = midi_port, .midi_status = midi_status,
        .button = button, .axes = axes, .update = update, .draw = draw, .wants_quit = wants_quit,
        .needs_draw = needs_draw,
    };
    return &desc;
}
