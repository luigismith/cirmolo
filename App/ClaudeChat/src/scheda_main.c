/* Cirmolo - scheda del gioco: collegamento con il kit delle app (spruce/cirmolo-kit).
 * I dati del gioco arrivano da PyUI in /tmp/ia-scheda.json (o nel file indicato da IA_SCHEDA). */
#include <stdlib.h>
#include <string.h>

#include "i18n.h"
#include "platform.h"
#include "scheda_app.h"

#define SAVES "/mnt/SDCARD/Saves/claude"

static void *k_create(float sr, const char *state)
{
    (void)state;
    i18n_init("lang");
    GameInfo g;
    const char *req = getenv("IA_SCHEDA");
    scheda_read_request(req && *req ? req : "/tmp/ia-scheda.json", &g);
    const char *saves = getenv("CIRMOLO_SAVES");
    return scheda_create(sr, saves && *saves ? saves : SAVES, &g);
}
static void k_destroy(void *p) { scheda_destroy(p); i18n_free(); }
static void k_audio(void *p, float *out, int n) { scheda_audio(p, out, n); }
static void k_button(void *p, int b, int pressed) { scheda_button(p, b, pressed); }
static void k_axes(void *p, float lx, float ly, float rx, float ry, float l2, float r2)
{
    (void)lx; (void)rx; (void)l2; (void)r2;
    scheda_axes(p, ly, ry);
}
static void k_update(void *p, float dt) { scheda_update(p, dt); }
static void k_draw(void *p, Canvas *c) { scheda_draw(p, c); }
static int k_quit(void *p) { return scheda_wants_quit(p); }
static int k_needs_draw(void *p) { return scheda_needs_draw(p); }

const CirmoloApp *cirmolo_app(void)
{
    static const CirmoloApp desc = {
        .title = "Scheda del gioco",
        .state_path = SAVES "/scheda.txt",
        .usb_output = 1,
        .create = k_create, .destroy = k_destroy, .audio = k_audio,
        .button = k_button, .axes = k_axes, .update = k_update, .draw = k_draw, .wants_quit = k_quit,
        .needs_draw = k_needs_draw,
    };
    return &desc;
}
