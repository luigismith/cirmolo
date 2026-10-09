/* Cirmolo Piano - collegamento con il kit delle app native (spruce/cirmolo-kit/platform.c).
 *
 * Il SoundFont si cerca in sf2/GeneralUser-GS.sf2 rispetto alla cartella dell'app (launch.sh entra li');
 * CIRMOLO_SF2=<file> ne usa un altro.
 */
#include <stdio.h>
#include <stdlib.h>

#include "app.h"
#include "piano.h"
#include "platform.h"

#define SF2_DEFAULT "sf2/GeneralUser-GS.sf2"

typedef struct { Piano *piano; App *app; } PianoApp;

static void *create(float sample_rate, const char *state_path)
{
    PianoApp *pa = calloc(1, sizeof(PianoApp));
    if (!pa) return NULL;
    const char *sf2 = getenv("CIRMOLO_SF2");
    if (!sf2 || !*sf2) sf2 = SF2_DEFAULT;
    pa->piano = piano_create(sample_rate);
    if (!pa->piano) { free(pa); return NULL; }
    if (piano_load(pa->piano, sf2)) fprintf(stderr, "SoundFont non caricato: %s\n", sf2);
    else fprintf(stderr, "SoundFont: %s, %d preset\n", sf2, piano_preset_count(pa->piano));
    pa->app = app_create(pa->piano, state_path, sf2);
    if (!pa->app) { piano_destroy(pa->piano); free(pa); return NULL; }
    return pa;
}

static void destroy(void *p)
{
    PianoApp *pa = p;
    app_destroy(pa->app);
    piano_destroy(pa->piano);
    free(pa);
}

static void audio(void *p, float *out, int frames) { piano_render(((PianoApp *)p)->piano, out, frames); }
static void button(void *p, int pad, int pressed) { app_button(((PianoApp *)p)->app, pad, pressed); }
static void midi(void *p, const unsigned char *m, int len) { app_midi(((PianoApp *)p)->app, m, len); }
static void midi_status(void *p, const char *dev) { app_midi_status(((PianoApp *)p)->app, dev); }
static void axes(void *p, float lx, float ly, float rx, float ry, float l2, float r2) { app_axes(((PianoApp *)p)->app, lx, ly, rx, ry, l2, r2); }
static void update(void *p, float dt) { app_update(((PianoApp *)p)->app, dt); }
static void draw(void *p, Canvas *c) { app_draw(((PianoApp *)p)->app, c); }
static int needs_draw(void *p) { return app_needs_draw(((PianoApp *)p)->app); }
static int wants_quit(void *p) { return app_wants_quit(((PianoApp *)p)->app); }

const CirmoloApp *cirmolo_app(void)
{
    static const CirmoloApp desc = {
        .title = "Cirmolo Piano",
        .state_path = "/mnt/SDCARD/Saves/piano/stato.txt",
        .create = create, .destroy = destroy, .audio = audio, .midi = midi, .midi_status = midi_status,
        .button = button, .axes = axes, .update = update, .draw = draw, .wants_quit = wants_quit,
        .needs_draw = needs_draw,
    };
    return &desc;
}
