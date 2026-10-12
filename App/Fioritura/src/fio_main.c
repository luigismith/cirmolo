/* Fioritura per Miyoo Flip - collegamento con il kit delle app Cirmolo (spruce/cirmolo-kit).
 * Tutte le porte MIDI: il Launchpad Mini MK3 parla sulla seconda («LPMiniMK3 MIDI»). */
#include "fio_app.h"
#include "platform.h"

static void *k_create(float sr, const char *state) { return fioapp_create(sr, state); }
static void k_destroy(void *p) { fioapp_destroy(p); }
static void k_audio(void *p, float *out, int n) { fioapp_audio(p, out, n); }
static void k_midi_port(void *p, int port, const unsigned char *m, int len) { fioapp_midi(p, port, m, len); }
static void k_midi_status(void *p, const char *dev) { fioapp_midi_status(p, dev); }
static void k_button(void *p, int b, int pressed) { fioapp_button(p, b, pressed); }
static void k_axes(void *p, float lx, float ly, float rx, float ry, float l2, float r2) { fioapp_axes(p, lx, ly, rx, ry, l2, r2); }
static void k_update(void *p, float dt) { fioapp_update(p, dt); }
static void k_draw(void *p, Canvas *c) { fioapp_draw(p, c); }
static int k_quit(void *p) { return fioapp_wants_quit(p); }
static int k_needs_draw(void *p) { return fioapp_needs_draw(p); }

const CirmoloApp *cirmolo_app(void)
{
    static const CirmoloApp desc = {
        .title = "Fioritura",
        .state_path = "/mnt/SDCARD/Saves/fioritura/stato.txt",
        .create = k_create, .destroy = k_destroy, .audio = k_audio,
        .midi_status = k_midi_status, .midi_port = k_midi_port,
        .button = k_button, .axes = k_axes, .update = k_update, .draw = k_draw, .wants_quit = k_quit,
        .needs_draw = k_needs_draw,
    };
    return &desc;
}
