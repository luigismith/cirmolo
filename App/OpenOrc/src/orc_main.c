/* OpenOrc per Miyoo Flip - collegamento con il kit delle app Cirmolo (spruce/cirmolo-kit). */
#include "orc_app.h"
#include "platform.h"

static void *k_create(float sr, const char *state) { return orcapp_create(sr, state); }
static void k_destroy(void *p) { orcapp_destroy(p); }
static void k_audio(void *p, float *out, int n) { orcapp_audio(p, out, n); }
static void k_midi_port(void *p, int port, const unsigned char *m, int len) { orcapp_midi(p, port, m, len); }
static void k_midi_status(void *p, const char *dev) { orcapp_midi_status(p, dev); }
static void k_button(void *p, int b, int pressed) { orcapp_button(p, b, pressed); }
static void k_axes(void *p, float lx, float ly, float rx, float ry, float l2, float r2) { orcapp_axes(p, lx, ly, rx, ry, l2, r2); }
static void k_update(void *p, float dt) { orcapp_update(p, dt); }
static void k_draw(void *p, Canvas *c) { orcapp_draw(p, c); }
static int k_quit(void *p) { return orcapp_wants_quit(p); }
static int k_needs_draw(void *p) { return orcapp_needs_draw(p); }

const CirmoloApp *cirmolo_app(void)
{
    static const CirmoloApp desc = {
        .title = "OpenOrc",
        .state_path = "/mnt/SDCARD/Saves/openorc/stato.txt",
        .create = k_create, .destroy = k_destroy, .audio = k_audio,
        .midi_status = k_midi_status, .midi_port = k_midi_port,
        .button = k_button, .axes = k_axes, .update = k_update, .draw = k_draw, .wants_quit = k_quit,
        .needs_draw = k_needs_draw,
    };
    return &desc;
}
