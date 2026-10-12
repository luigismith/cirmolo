/* Fioritura - Novation Launchpad Mini MK3 come superficie: griglia 8x8 con LED RGB, 8 tasti in alto,
 * 8 a destra. Protocollo dal «Launchpad Mini [MK3] Programmer's reference manual» (vedi anche
 * App/CirmoloSampler/src/launchpad.h, che ne riporta le pagine):
 * - modalita' Programmer: F0 00 20 29 02 0D 0E 01 F7 (uscita: ... 0E 00 F7, sempre all'uscita dell'app);
 * - pad: Note On canale 1, nota = 10 x riga + colonna, riga 1 in basso (11..88); Note Off = velocity 0;
 * - tasti in alto CC 91-98 (su, giu', sinistra, destra, Session, Drums, Keys, User), a destra CC 89..19
 *   dall'alto in basso, logo 99 (solo LED);
 * - LED: SysEx F0 00 20 29 02 0D 03 <spec>... F7 con spec RGB = 03, indice, R, G, B (0-127).
 * I LED si aggiornano con un solo SysEx e solo per quel che cambia.
 */
#ifndef FIORITURA_LP_H
#define FIORITURA_LP_H

#include <stdint.h>

#define FLP_LEDS 100

enum { FLP_EV_PAD, FLP_EV_TOP, FLP_EV_SIDE };
enum { FLP_UP, FLP_DOWN, FLP_LEFT, FLP_RIGHT, FLP_SESSION, FLP_DRUMS, FLP_KEYS, FLP_USER };

typedef struct {
    int kind;                 /* FLP_EV_* */
    int row, col;             /* pad: riga 0-7 dal basso, colonna 0-7 */
    int index;                /* tasto in alto 0-7 (FLP_*) o a destra 0-7 dall'alto */
    int on;                   /* 1 premuto, 0 rilasciato */
} FlpEvent;

typedef struct {
    int connected;
    int port;                 /* porta da cui arrivano i pad; -1 finche' non si sa (si manda a tutte) */
    int shift;                /* User tenuto */
    unsigned char want[FLP_LEDS][3], dev[FLP_LEDS][3];
    unsigned char known[FLP_LEDS];
    int frames_sent;
} FioLp;

int  flp_match(const char *device);        /* il nome del dispositivo e' un Launchpad Mini MK3? */
void flp_connect(FioLp *lp);
void flp_disconnect(FioLp *lp);
int  flp_decode(FioLp *lp, int port, const unsigned char *msg, int len, FlpEvent *ev);

int  flp_pad(int row, int col);            /* indice LED di un pad (riga 0 in basso) */
int  flp_top(int i);
int  flp_side(int i);                      /* 0 = in alto */
void flp_clear(FioLp *lp);                 /* stato voluto: tutto spento */
/* colore 0xRRGGBB moltiplicato per la luminosita' 0..1 */
void flp_set(FioLp *lp, int idx, uint32_t rgb, float bright);
void flp_flush(FioLp *lp);

#endif
