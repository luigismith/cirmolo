/* Chiedi all'IA per Miyoo Flip - collegamento con il kit delle app Cirmolo (spruce/cirmolo-kit). */
#include "chat_app.h"
#include "i18n.h"
#include "platform.h"

static void *k_create(float sr, const char *state) { i18n_init("lang"); return chat_create(sr, state); }
static void k_destroy(void *p) { chat_destroy(p); i18n_free(); }
static void k_audio(void *p, float *out, int n) { chat_audio(p, out, n); }
static void k_capture(void *p, const float *in, int n) { chat_capture(p, in, n); }
static void k_capture_status(void *p, const char *dev, float rate) { chat_capture_status(p, dev, rate); }
static void k_button(void *p, int b, int pressed) { chat_button(p, b, pressed); }
static void k_axes(void *p, float lx, float ly, float rx, float ry, float l2, float r2) { chat_axes(p, lx, ly, rx, ry, l2, r2); }
static void k_update(void *p, float dt) { chat_update(p, dt); }
static void k_draw(void *p, Canvas *c) { chat_draw(p, c); }
static int k_quit(void *p) { return chat_wants_quit(p); }
static int k_needs_draw(void *p) { return chat_needs_draw(p); }
static void k_text(void *p, const char *utf8, int special) { chat_text(p, utf8, special); }

const CirmoloApp *cirmolo_app(void)
{
    static const CirmoloApp desc = {
        .title = "Chiedi all'IA",
        .state_path = "/mnt/SDCARD/Saves/claude/impostazioni.txt",
        .wants_capture = 1, .usb_output = 1,
        .create = k_create, .destroy = k_destroy, .audio = k_audio,
        .capture = k_capture, .capture_status = k_capture_status,
        .button = k_button, .axes = k_axes, .update = k_update, .draw = k_draw, .wants_quit = k_quit,
        .needs_draw = k_needs_draw, .text_input = k_text,
    };
    return &desc;
}
