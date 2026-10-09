/* Cirmolo - "Cosa gioco?": collegamento con il kit delle app (spruce/cirmolo-kit). */
#include <stdlib.h>

#include "consigli_app.h"
#include "i18n.h"
#include "platform.h"

static void *k_create(float sr, const char *state)
{
    (void)sr; (void)state;
    i18n_init("lang");
    const char *saves = getenv("CIRMOLO_SAVES");
    return consigli_create("/mnt/SDCARD", saves && *saves ? saves : "/mnt/SDCARD/Saves/claude", "/tmp/ia-gioca.sh");
}
static void k_destroy(void *p) { consigli_destroy(p); i18n_free(); }
static void k_audio(void *p, float *out, int n) { (void)p; for (int i = 0; i < 2 * n; i++) out[i] = 0.0f; }
static void k_button(void *p, int b, int pressed) { consigli_button(p, b, pressed); }
static void k_update(void *p, float dt) { consigli_update(p, dt); }
static void k_draw(void *p, Canvas *c) { consigli_draw(p, c); }
static int k_quit(void *p) { return consigli_wants_quit(p); }
static int k_needs_draw(void *p) { return consigli_needs_draw(p); }

const CirmoloApp *cirmolo_app(void)
{
    static const CirmoloApp desc = {
        .title = "Cosa gioco?",
        .state_path = "/mnt/SDCARD/Saves/claude/consigli.txt",
        .create = k_create, .destroy = k_destroy, .audio = k_audio,
        .button = k_button, .update = k_update, .draw = k_draw, .wants_quit = k_quit,
        .needs_draw = k_needs_draw,
    };
    return &desc;
}
