/* Prova del parser MIDI del kit (midi.c) sul PC. Uso: test_midi (codice di uscita 0 = tutto bene). */
#include <stdio.h>
#include <string.h>

#include "midi.h"

static int fail;

static void expect(const char *what, const unsigned char *in, int n, const char *want)
{
    MidiParser p;
    midi_parser_reset(&p);
    char got[512] = "";
    unsigned char msg[3];
    for (int i = 0; i < n; i++) {
        int len = midi_parse_byte(&p, in[i], msg);
        for (int k = 0; k < len; k++) {
            char b[8];
            snprintf(b, sizeof(b), "%02X%s", msg[k], k == len - 1 ? " | " : " ");
            strcat(got, b);
        }
    }
    int ok = strcmp(got, want) == 0;
    printf("  %s %s\n", ok ? "OK  " : "FAIL", what);
    if (!ok) { printf("       atteso: %s\n       ottenuto: %s\n", want, got); fail = 1; }
}

int main(void)
{
    const unsigned char a[] = { 0x90, 60, 100, 0x80, 60, 0 };
    expect("note on e off", a, sizeof(a), "90 3C 64 | 80 3C 00 | ");
    const unsigned char b[] = { 0x90, 60, 100, 64, 90, 67, 80 };
    expect("running status (tre note di un accordo)", b, sizeof(b), "90 3C 64 | 90 40 5A | 90 43 50 | ");
    const unsigned char c[] = { 0x99, 36, 120, 0x89, 36, 0 };
    expect("pad sul canale 10", c, sizeof(c), "99 24 78 | 89 24 00 | ");
    const unsigned char d[] = { 0xB0, 70, 10, 71, 127 };
    expect("manopole (CC) con running status", d, sizeof(d), "B0 46 0A | B0 47 7F | ");
    const unsigned char e[] = { 0xE0, 0x00, 0x40, 0xE0, 0x7F, 0x7F };
    expect("pitch bend centro e massimo", e, sizeof(e), "E0 00 40 | E0 7F 7F | ");
    const unsigned char f[] = { 0xC0, 5, 0xD0, 99 };
    expect("program change e aftertouch (un byte di dati)", f, sizeof(f), "C0 05 | D0 63 | ");
    const unsigned char g[] = { 0x90, 60, 0xF8, 100 };
    expect("clock in mezzo a una nota", g, sizeof(g), "F8 | 90 3C 64 | ");
    const unsigned char h[] = { 0xF0, 0x47, 0x7F, 0x49, 0x60, 0xF7, 0x90, 62, 90 };
    expect("sysex scartato", h, sizeof(h), "90 3E 5A | ");
    const unsigned char i2[] = { 0xF0, 1, 2, 0x90, 64, 100 };
    expect("sysex interrotto da un nuovo stato", i2, sizeof(i2), "90 40 64 | ");
    const unsigned char j[] = { 0xF2, 1, 2, 3, 4 };
    expect("song position senza running status", j, sizeof(j), "F2 01 02 | ");
    const unsigned char k[] = { 10, 20, 0x90, 60, 100 };
    expect("dati senza stato all'inizio ignorati", k, sizeof(k), "90 3C 64 | ");
    const unsigned char l[] = { 0xFA, 0xFC, 0xFE };
    expect("start, stop, active sensing", l, sizeof(l), "FA | FC | FE | ");
    printf(fail ? "ESITO: errori\n" : "ESITO: tutto bene\n");
    return fail;
}
