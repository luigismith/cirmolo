/* Fioritura - Launchpad Mini MK3 (vedi fio_lp.h). */
#include "fio_lp.h"

#include <string.h>

#include "midi.h"

static const unsigned char SX_HEAD[] = { 0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D };
#define SX_CMD_LED  0x03
#define SX_CMD_MODE 0x0E
#define SX_MAX (6 + 1 + 81 * 5 + 1)

static int valid(int i)
{
    int r = i / 10, c = i % 10;
    if (r >= 1 && r <= 8 && c >= 1 && c <= 8) return 1;
    if (i >= 91 && i <= 99) return 1;
    return c == 9 && i >= 19 && i <= 89;
}

int flp_pad(int row, int col) { return 11 + row * 10 + col; }
int flp_top(int i) { return 91 + i; }
int flp_side(int i) { return 89 - 10 * i; }

int flp_match(const char *device)
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

/* Alla porta del Launchpad o, finche' non e' nota, a tutte (2 per il Launchpad: DAW e MIDI). */
static int send(FioLp *lp, const unsigned char *msg, int len)
{
    if (lp->port >= 0) return cirmolo_midi_send(lp->port, msg, len);
    int n = cirmolo_midi_ports(), ok = -1;
    if (n < 1) n = 2;
    for (int p = 0; p < n; p++) if (cirmolo_midi_send(p, msg, len) == 0) ok = 0;
    return ok;
}

static void send_mode(FioLp *lp, int programmer)
{
    unsigned char m[9];
    memcpy(m, SX_HEAD, 6);
    m[6] = SX_CMD_MODE;
    m[7] = programmer ? 1 : 0;
    m[8] = 0xF7;
    send(lp, m, 9);
}

void flp_connect(FioLp *lp)
{
    memset(lp, 0, sizeof(*lp));
    lp->connected = 1;
    lp->port = -1;
    send_mode(lp, 1);
}

void flp_disconnect(FioLp *lp)
{
    if (!lp->connected) return;
    flp_clear(lp);
    flp_flush(lp);                            /* si spegne tutto prima di tornare in Live */
    send_mode(lp, 0);
    lp->connected = 0;
    lp->shift = 0;
    lp->port = -1;
}

int flp_decode(FioLp *lp, int port, const unsigned char *m, int len, FlpEvent *ev)
{
    if (!lp->connected || len < 3 || MIDI_CHANNEL(m) != 0) return 0;
    int type = MIDI_TYPE(m);
    memset(ev, 0, sizeof(*ev));
    if (type == MIDI_NOTE_ON || type == MIDI_NOTE_OFF) {
        int n = m[1], row = n / 10, col = n % 10;
        if (row < 1 || row > 8 || col < 1 || col > 8) return 0;
        ev->kind = FLP_EV_PAD;
        ev->row = row - 1;
        ev->col = col - 1;
        ev->on = type == MIDI_NOTE_ON && m[2] > 0;
    } else if (type == MIDI_CC) {
        int cc = m[1];
        ev->on = m[2] > 0;
        if (cc >= 91 && cc <= 98) { ev->kind = FLP_EV_TOP; ev->index = cc - 91; }
        else if (cc % 10 == 9 && cc >= 19 && cc <= 89) { ev->kind = FLP_EV_SIDE; ev->index = (89 - cc) / 10; }
        else return 0;
        if (ev->kind == FLP_EV_TOP && ev->index == FLP_USER) lp->shift = ev->on;
    } else {
        return 0;
    }
    if (lp->port < 0) lp->port = port;
    return 1;
}

void flp_clear(FioLp *lp) { memset(lp->want, 0, sizeof(lp->want)); }

void flp_set(FioLp *lp, int idx, uint32_t rgb, float bright)
{
    if (idx < 0 || idx >= FLP_LEDS) return;
    if (bright < 0.0f) bright = 0.0f;
    if (bright > 1.0f) bright = 1.0f;
    int c[3] = { (int)((rgb >> 16) & 255), (int)((rgb >> 8) & 255), (int)(rgb & 255) };
    for (int k = 0; k < 3; k++) {
        /* 0-255 -> 0-127, a gradini di 4 per non mandare un SysEx a ogni minima dissolvenza */
        int v = (int)((float)c[k] * bright * (127.0f / 255.0f) + 0.5f);
        if (v > 0 && v < 4) v = 4;
        v &= ~3;
        if (v > 127) v = 124;
        lp->want[idx][k] = (unsigned char)v;
    }
}

void flp_flush(FioLp *lp)
{
    if (!lp->connected) return;
    unsigned char buf[SX_MAX], changed[FLP_LEDS];
    int n = 0, specs = 0;
    memcpy(buf, SX_HEAD, 6);
    n = 6;
    buf[n++] = SX_CMD_LED;
    for (int i = 0; i < FLP_LEDS; i++) {
        if (!valid(i)) continue;
        if (lp->known[i] && !memcmp(lp->want[i], lp->dev[i], 3)) continue;
        buf[n++] = 3;
        buf[n++] = (unsigned char)i;
        buf[n++] = lp->want[i][0];
        buf[n++] = lp->want[i][1];
        buf[n++] = lp->want[i][2];
        changed[specs++] = (unsigned char)i;
    }
    if (!specs) return;
    buf[n++] = 0xF7;
    if (send(lp, buf, n)) return;            /* non scritto: si riprova al prossimo giro */
    for (int k = 0; k < specs; k++) {
        int i = changed[k];
        memcpy(lp->dev[i], lp->want[i], 3);
        lp->known[i] = 1;
    }
    lp->frames_sent++;
}
