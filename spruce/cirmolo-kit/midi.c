/* Cirmolo kit - parser MIDI (vedi midi.h). */
#include "midi.h"

#include <string.h>

void midi_parser_reset(MidiParser *p) { memset(p, 0, sizeof(*p)); }

static int data_bytes(unsigned char status)
{
    switch (status & 0xF0) {
    case 0x80: case 0x90: case 0xA0: case 0xB0: case 0xE0: return 2;
    case 0xC0: case 0xD0: return 1;
    }
    switch (status) {
    case 0xF1: case 0xF3: return 1;   /* quarter frame, song select */
    case 0xF2: return 2;              /* song position */
    default: return 0;                /* F6 tune request, F4/F5 non definiti */
    }
}

int midi_parse_byte(MidiParser *p, unsigned char b, unsigned char out[3])
{
    if (b >= 0xF8) {                  /* tempo reale: non interrompe il messaggio in corso */
        out[0] = b;
        return b == 0xF9 || b == 0xFD ? 0 : 1;
    }
    if (b == 0xF0) { p->sysex = 1; p->status = 0; return 0; }
    if (b == 0xF7) { p->sysex = 0; return 0; }
    if (b & 0x80) {                   /* nuovo stato (chiude anche un sysex rimasto aperto) */
        p->sysex = 0;
        p->have = 0;
        p->need = data_bytes(b);
        if (p->need == 0) {
            p->status = 0;
            out[0] = b;
            return 1;
        }
        p->status = b;
        return 0;
    }
    if (p->sysex || !p->status) return 0;
    p->data[p->have++] = b;
    if (p->have < p->need) return 0;
    out[0] = p->status;
    out[1] = p->data[0];
    out[2] = p->need > 1 ? p->data[1] : 0;
    int len = 1 + p->need;
    p->have = 0;
    if (p->status >= 0xF0) p->status = 0;    /* i messaggi di sistema comune non hanno running status */
    return len;
}
