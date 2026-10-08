/* Cirmolo kit - lettura di un flusso MIDI grezzo (come arriva da /dev/snd/midiC*D*).
 *
 * Gestisce running status, messaggi di sistema comune, sysex (scartati) e messaggi di tempo reale
 * (clock, start, stop: consegnati da soli, anche in mezzo a un altro messaggio).
 */
#ifndef CIRMOLO_MIDI_H
#define CIRMOLO_MIDI_H

typedef struct {
    unsigned char status, data[2];
    int have, need, sysex;
} MidiParser;

void midi_parser_reset(MidiParser *p);

/* Un byte alla volta: se completa un messaggio lo scrive in out e ne restituisce la lunghezza (1..3),
   altrimenti 0. */
int midi_parse_byte(MidiParser *p, unsigned char b, unsigned char out[3]);

/* Aiuti per le app. */
#define MIDI_TYPE(m)    ((m)[0] & 0xF0)
#define MIDI_CHANNEL(m) ((m)[0] & 0x0F)            /* 0..15 */
enum { MIDI_NOTE_OFF = 0x80, MIDI_NOTE_ON = 0x90, MIDI_POLY_AT = 0xA0, MIDI_CC = 0xB0,
       MIDI_PROGRAM = 0xC0, MIDI_CHANNEL_AT = 0xD0, MIDI_PITCH_BEND = 0xE0 };

#endif
