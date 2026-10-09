/* Cirmolo Sampler - Launchpad Mini MK3: protocollo, LED e decodifica dei tasti (vedi launchpad.h, con
 * i riferimenti alle pagine del manuale). Le azioni sul sampler stanno in app.c. */
#include "launchpad.h"

#include <string.h>

#include "midi.h"

/* Intestazione SysEx del dispositivo (R. p. 6) e comandi (R. p. 23). */
static const unsigned char SX_HEAD[] = { 0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D };
#define SX_CMD_LED  0x03          /* LED lighting (R. p. 14) */
#define SX_CMD_MODE 0x0E          /* Programmer (1) / Live (0) (R. p. 7-8) */

#define SX_MAX (6 + 1 + 81 * 4 + 1)   /* intestazione, comando, 81 spec da al massimo 4 byte, F7 */

/* Indici validi della superficie (R. p. 10): 64 pad, 8 tasti in alto (91-98), 8 a destra (19..89), logo 99. */
static int lp_valid(int i)
{
    int r = i / 10, c = i % 10;
    if (r >= 1 && r <= 8 && c >= 1 && c <= 8) return 1;
    if (i >= 91 && i <= 99) return 1;
    return c == 9 && i >= 19 && i <= 89;
}

int lp_led_pad(int pad) { return 11 + (pad / 8) * 10 + pad % 8; }                  /* righe 1-2 */
int lp_led_step(int step) { return 31 + (step / 8) * 10 + step % 8; }              /* righe 3-4 */
int lp_led_overview(int row, int col) { return (5 + row) * 10 + 1 + col; }         /* righe 5-8 */
int lp_led_top(int i) { return 91 + i; }
int lp_led_scene(int i) { return 89 - 10 * i; }
int lp_overview_first(int sel) { return sel / 4 * 4; }

/* Tinta chiara / scura di un colore pieno della palette (R. p. 11: quartetti 4k..4k+3). */
static int light(int c) { return c >= 5 ? c - 1 : c; }
static int dim(int c) { return c >= 5 ? c + 1 : (c > 1 ? c - 1 : c); }

int lp_pad_colour(const Sample *smp, const PadParams *pp)
{
    static const int groups[SP_GROUPS] = { LPC_RED, LPC_ORANGE, LPC_YELLOW, LPC_MAGENTA };
    static const int folders[] = { LPC_GREEN, LPC_AZURE, LPC_BLUE, LPC_VIOLET, LPC_TEAL, LPC_LIME, LPC_PINK, LPC_SKY };
    if (!smp) return LPC_OFF;
    if (pp && pp->group >= 1 && pp->group <= SP_GROUPS) return groups[pp->group - 1];
    /* cartella del file: hash FNV del percorso senza il nome */
    const char *end = smp->path, *p;
    for (p = smp->path; *p; p++) if (*p == '/' || *p == '\\') end = p;
    unsigned h = 2166136261u;
    for (p = smp->path; p < end; p++) { h ^= (unsigned char)*p; h *= 16777619u; }
    return folders[h % (sizeof(folders) / sizeof(folders[0]))];
}

int lp_match(const char *device)
{
    static const char want[] = "launchpadminimk";
    if (!device) return 0;
    char flat[96];
    size_t n = 0;
    for (const char *p = device; *p && n < sizeof(flat) - 1; p++) {
        if (*p == ' ' || *p == '_' || *p == '-') continue;
        flat[n++] = (char)(*p >= 'A' && *p <= 'Z' ? *p + 32 : *p);
    }
    flat[n] = 0;
    return strstr(flat, want) != NULL;
}

/* Manda alla porta del Launchpad o, finche' non e' nota, a tutte quelle del dispositivo (2 per il
   Launchpad: DAW e MIDI; senza kit, nelle prove, si assumono 2). 0 se almeno una scrittura e' riuscita. */
static int lp_send(Launchpad *lp, const unsigned char *msg, int len)
{
    if (lp->port >= 0) return cirmolo_midi_send(lp->port, msg, len);
    int n = cirmolo_midi_ports(), ok = -1;
    if (n < 1) n = 2;
    for (int p = 0; p < n; p++) if (cirmolo_midi_send(p, msg, len) == 0) ok = 0;
    return ok;
}

static void lp_send_mode(Launchpad *lp, int programmer)
{
    unsigned char m[9];
    memcpy(m, SX_HEAD, 6);
    m[6] = SX_CMD_MODE;
    m[7] = programmer ? 1 : 0;
    m[8] = 0xF7;
    lp_send(lp, m, 9);
}

void lp_connect(Launchpad *lp)
{
    memset(lp, 0, sizeof(*lp));
    lp->connected = 1;
    lp->port = -1;
    memset(lp->dev_type, 0xFF, sizeof(lp->dev_type));
    memset(lp->dev_col, 0xFF, sizeof(lp->dev_col));
    memset(lp->dev_col2, 0xFF, sizeof(lp->dev_col2));
    lp_send_mode(lp, 1);
}

void lp_disconnect(Launchpad *lp)
{
    if (!lp->connected) return;
    lp_send_mode(lp, 0);
    lp->connected = 0;
    lp->shift = 0;
    lp->port = -1;
}

int lp_decode(Launchpad *lp, int port, const unsigned char *m, int len, LpEvent *ev)
{
    if (!lp->connected || len < 3 || MIDI_CHANNEL(m) != 0) return 0;
    int type = MIDI_TYPE(m);
    memset(ev, 0, sizeof(*ev));
    if (type == MIDI_NOTE_ON || type == MIDI_NOTE_OFF) {
        int n = m[1], row = n / 10, col = n % 10;
        if (row < 1 || row > 8 || col < 1 || col > 8) return 0;
        ev->on = type == MIDI_NOTE_ON && m[2] > 0;
        ev->vel = m[2] == 127 ? 100 / 127.0f : m[2] / 127.0f;
        if (row <= 2) { ev->kind = LP_EV_PAD; ev->index = (row - 1) * 8 + col - 1; }
        else if (row <= 4) { ev->kind = LP_EV_STEP; ev->index = (row - 3) * 8 + col - 1; }
        else { ev->kind = LP_EV_OVERVIEW; ev->index = row - 5; ev->col = col - 1; }
    } else if (type == MIDI_CC) {
        int cc = m[1];
        ev->on = m[2] > 0;
        if (cc >= 91 && cc <= 98) { ev->kind = LP_EV_TOP; ev->index = cc - 91; }
        else if (cc % 10 == 9 && cc >= 19 && cc <= 89) { ev->kind = LP_EV_SCENE; ev->index = (89 - cc) / 10; }
        else return 0;
        if (ev->kind == LP_EV_TOP && ev->index == LP_TOP_USER) lp->shift = ev->on;
    } else return 0;
    if (lp->port < 0) lp->port = port;        /* da qui in poi i LED vanno solo su questa porta */
    return 1;
}

static void set(Launchpad *lp, int idx, int type, int col, int col2)
{
    lp->type[idx] = (unsigned char)type;
    lp->col[idx] = (unsigned char)col;
    lp->col2[idx] = (unsigned char)col2;
}

void lp_render(Launchpad *lp, Sampler *s, int sel, int bank, const float *glow)
{
    if (!lp->connected) return;
    memset(lp->type, 0, sizeof(lp->type));
    memset(lp->col, 0, sizeof(lp->col));
    memset(lp->col2, 0, sizeof(lp->col2));
    int colour[SP_PADS];
    for (int p = 0; p < SP_PADS; p++) colour[p] = lp_pad_colour(sampler_sample(s, p), sampler_pad(s, p));

    /* righe 1-2: i pad */
    for (int p = 0; p < SP_PADS; p++) {
        int idx = lp_led_pad(p), c = colour[p];
        const PadParams *pp = sampler_pad(s, p);
        int sounding = sampler_slot_voices(s, p) > 0 || (glow && glow[p] > 0.5f);
        if (sounding) set(lp, idx, LP_STATIC, LPC_WHITE, 0);
        else if (p == sel) set(lp, idx, LP_PULSE, c ? c : LPC_GREY, 0);
        else set(lp, idx, LP_STATIC, pp->mute ? dim(c) : c, 0);
    }

    /* righe 3-4: i passi del pad scelto nel pattern scelto */
    int pat_i = sampler_selected_pattern(s), playing = sampler_playing_pattern(s);
    const SeqPattern *pat = sampler_pattern_at(s, pat_i);
    int step = playing == pat_i ? sampler_current_step(s) : -1;
    int sc = colour[sel] ? colour[sel] : LPC_GREY;
    for (int i = 0; i < SP_STEPS; i++) {
        int v = pat->vel[sel][i], c;
        if (i >= pat->length) c = LPC_OFF;
        else if (!v) c = LPC_GREY_DARK;
        else c = v >= 127 ? light(sc) : (v >= 100 ? sc : dim(sc));
        if (i == step) c = LPC_WHITE;
        set(lp, lp_led_step(i), LP_STATIC, c, 0);
    }

    /* righe 5-8: vista d'insieme del quartetto del pad scelto, 2 passi per colonna */
    int first = lp_overview_first(sel);
    for (int r = 0; r < 4; r++) {
        int p = first + r, pc = colour[p] ? colour[p] : LPC_GREY;
        for (int col = 0; col < 8; col++) {
            int i0 = 2 * col, v0 = pat->vel[p][i0], v1 = pat->vel[p][i0 + 1], c;
            if (i0 >= pat->length) c = LPC_OFF;
            else if (v0 && v1) c = pc;
            else if (v0 || v1) c = dim(pc);
            else c = col % 4 == 0 ? LPC_GREY_DARK : LPC_OFF;
            if (step >= 0 && step / 2 == col) c = LPC_WHITE;
            set(lp, lp_led_overview(r, col), LP_STATIC, c, 0);
        }
    }

    /* tasti in alto */
    set(lp, lp_led_top(LP_TOP_UP), LP_STATIC, bank == 0 ? LPC_WHITE : LPC_GREY_DARK, 0);
    set(lp, lp_led_top(LP_TOP_DOWN), LP_STATIC, bank == 1 ? LPC_WHITE : LPC_GREY_DARK, 0);
    set(lp, lp_led_top(LP_TOP_LEFT), LP_STATIC, LPC_GREY, 0);
    set(lp, lp_led_top(LP_TOP_RIGHT), LP_STATIC, LPC_GREY, 0);
    set(lp, lp_led_top(LP_TOP_SESSION), LP_STATIC, sampler_playing(s) ? LPC_GREEN : dim(dim(LPC_GREEN)), 0);
    set(lp, lp_led_top(LP_TOP_DRUMS), LP_STATIC, sampler_record(s) ? LPC_RED : dim(dim(LPC_RED)), 0);
    set(lp, lp_led_top(LP_TOP_KEYS), LP_STATIC, sampler_metronome(s) ? LPC_YELLOW : dim(dim(LPC_YELLOW)), 0);
    set(lp, lp_led_top(LP_TOP_USER), LP_STATIC, lp->shift ? LPC_WHITE : LPC_GREY_DARK, 0);

    /* tasti a destra */
    unsigned chain = sampler_chain(s);
    for (int i = 0; i < SP_PATTERNS; i++) {
        int base = i == pat_i ? LPC_VIOLET : dim(dim(LPC_VIOLET));
        if (sampler_playing(s) && playing == i) base = LPC_GREEN;
        if ((chain >> i) & 1) set(lp, lp_led_scene(i), LP_FLASH, light(LPC_VIOLET), base);   /* B lampeggia su A (R. p. 12, 14) */
        else set(lp, lp_led_scene(i), LP_STATIC, base, 0);
    }
    const PadParams *sp = sampler_pad(s, sel);
    set(lp, lp_led_scene(LP_SCENE_MUTE), LP_STATIC, sp->mute ? LPC_RED : dim(dim(LPC_RED)), 0);
    set(lp, lp_led_scene(LP_SCENE_SOLO), LP_STATIC, sp->solo ? LPC_YELLOW : dim(dim(LPC_YELLOW)), 0);
    set(lp, lp_led_scene(LP_SCENE_CLEAR), LP_STATIC, lp->shift ? LPC_ORANGE : dim(dim(LPC_ORANGE)), 0);
    set(lp, lp_led_scene(LP_SCENE_TAP), LP_STATIC, LPC_AZURE, 0);
    set(lp, 99, LP_STATIC, LPC_VIOLET, 0);                                                  /* logo */
}

void lp_flush(Launchpad *lp)
{
    if (!lp->connected) return;
    unsigned char buf[SX_MAX], changed[LP_LEDS];
    int n = 0, specs = 0;
    memcpy(buf, SX_HEAD, 6);
    n = 6;
    buf[n++] = SX_CMD_LED;
    for (int i = 0; i < LP_LEDS; i++) {
        if (!lp_valid(i)) continue;
        if (lp->type[i] == lp->dev_type[i] && lp->col[i] == lp->dev_col[i] && lp->col2[i] == lp->dev_col2[i]) continue;
        buf[n++] = lp->type[i];
        buf[n++] = (unsigned char)i;
        buf[n++] = lp->col[i];
        if (lp->type[i] == LP_FLASH) buf[n++] = lp->col2[i];
        changed[specs++] = (unsigned char)i;
    }
    if (!specs) return;
    buf[n++] = 0xF7;
    if (lp_send(lp, buf, n)) return;          /* non scritto: si riprova al prossimo fotogramma */
    for (int k = 0; k < specs; k++) {
        int i = changed[k];
        lp->dev_type[i] = lp->type[i];
        lp->dev_col[i] = lp->col[i];
        lp->dev_col2[i] = lp->col2[i];
    }
    lp->frames_sent++;
}
