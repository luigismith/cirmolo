/* Cirmolo Sampler - Novation Launchpad Mini [MK3] come superficie di controllo: griglia 8x8 di pad con
 * LED RGB, 8 tasti in alto, 8 a destra, USB MIDI.
 *
 * Riferimenti (R.) al «Launchpad Mini [MK3] Programmer's reference manual» di Novation/Focusrite, PDF
 * ufficiale "Launchpad Mini - Programmers Reference Manual.pdf" (23 pagine, MD5 fdc9fa9063a23b59a2d771eebeecde19):
 * - p. 6: due interfacce USB MIDI, «LPMiniMK3 DAW» (la prima) e «LPMiniMK3 MIDI» (la seconda): la
 *   seconda porta i messaggi della modalita' Programmer e riceve i comandi dei LED; la prima serve ai DAW
 *   (Session mode). I Note Off escono come Note On con velocity 0. Device Inquiry: F0 7E 7F 06 01 F7, la
 *   risposta e' F0 7E 00 06 02 00 20 29 13 01 00 00 <4 byte di versione> F7 (13h = Launchpad Mini MK3).
 * - p. 6-7: ogni SysEx comincia con F0 00 20 29 02 0D, poi il byte del comando.
 * - p. 7-8: F0 00 20 29 02 0D 0E <1|0> F7 entra (1) ed esce (0) dalla modalita' Programmer; in Live il
 *   dispositivo torna al layout Session (in DAW mode) o al Custom 2 «Keys». In Programmer e' disabilitato
 *   anche il menu Setup (Session tenuto): per questo si deve sempre tornare in Live all'uscita.
 * - p. 10: in Programmer i pad mandano Note On sul canale 1 con nota = 10 x riga + colonna, riga 1 in
 *   basso (11-18 la prima riga, 81-88 l'ultima); i tasti in alto mandano CC 91-98 (▲ ▼ ◀ ▶ Session Drums
 *   Keys User), quelli a destra CC 89, 79, 69, 59, 49, 39, 29, 19 dall'alto in basso; il logo e' il CC 99
 *   (solo LED). Tutti accettano Note o CC per accendersi.
 * - p. 11: palette di 128 colori: 0 spento, 1 grigio scuro, 2 grigio, 3 bianco, 5 rosso, 9 arancio,
 *   13 giallo, 21 verde, 33 turchese, 37 azzurro, 45 blu, 49 viola, 53 magenta, 57 rosa; in ogni quartetto
 *   4k..4k+3 il primo e' la tinta chiara, poi pieno, scuro, piu' scuro.
 * - p. 12: colori via Note/CC con velocity = indice della palette: canale 1 fisso, canale 2 lampeggio (fra
 *   il colore fisso e quello inviato, un periodo per battito del clock MIDI o a 120 BPM), canale 3
 *   pulsazione (due battiti). p. 13: esempi (90 0B 05 = pad in basso a sinistra rosso).
 * - p. 14: SysEx «LED lighting» F0 00 20 29 02 0D 03 <spec>... F7, fino a 81 spec in un messaggio (tutta la
 *   superficie); spec = tipo, indice, dati: 0 fisso (1 byte di palette), 1 lampeggio (colore B, colore A),
 *   2 pulsazione (1 byte), 3 RGB (3 byte 0-127). Gli indici sono quelli della modalita' Programmer.
 * - p. 22: luminosita' (comando 08) e feedback interno/esterno dei LED (0A).
 * Il manuale non parla di velocity dei pad: il Mini MK3 non e' sensibile alla forza (lo e' il Launchpad X)
 * e manda un valore fisso; qui 127 vale come colpo normale (100), un altro valore viene usato com'e'.
 *
 * Disposizione scelta (riga 1 in basso):
 * - righe 1-2: i 16 pad del sampler (A1-A8, B1-B8): spento se vuoto, colore del gruppo di esclusione o
 *   della cartella del campione, bianco mentre suona, pulsante quello scelto;
 * - righe 3-4: i 16 passi del pad scelto nel pattern scelto (riga 3 passi 1-8, riga 4 passi 9-16):
 *   acceso = colore del pad (chiaro se accentato, scuro se piano), spento = grigio scuro, oltre la
 *   lunghezza = spento, passo in moto = bianco;
 * - righe 5-8: vista d'insieme di 4 pad, 2 passi per colonna: i 4 pad del quartetto che contiene il pad
 *   scelto (A1-A4, A5-A8, B1-B4 o B5-B8), riga 5 il primo. Colonna piena = entrambi i passi, scura = uno
 *   solo, grigio scuro sui quarti (colonne 1 e 5), bianco sul passo in moto. Premere = i due passi on/off
 *   (se uno e' acceso si spengono, altrimenti si accende il primo); con User tenuto sceglie quel pad.
 * - tasti in alto: ▲ banco A, ▼ banco B (per i tasti della Flip), ◀ ▶ pattern precedente/successivo
 *   (con User: lunghezza del pattern -1/+1), Session play/stop, Drums registrazione dal vivo, Keys
 *   metronomo, User tenuto = «shift»: un pad delle righe 1-2 viene scelto senza suonare, un passo viene
 *   cancellato invece che commutato;
 * - tasti a destra: i primi 4 scelgono il pattern A-D (acceso quello scelto, verde quello in moto,
 *   lampeggia chi e' in catena; con User: entra/esce dalla catena), poi muto del pad scelto, solo del pad
 *   scelto, svuota il pattern (solo con User tenuto), tap tempo.
 * I LED si aggiornano con un solo SysEx per fotogramma e solo per quel che cambia.
 */
#ifndef CIRMOLO_LAUNCHPAD_H
#define CIRMOLO_LAUNCHPAD_H

#include "sampler.h"

#define LP_LEDS 100                          /* indici 0..99: note dei pad 11-88, CC dei tasti 19-99 (R. p. 10) */

enum { LP_STATIC = 0, LP_FLASH = 1, LP_PULSE = 2 };          /* tipi del SysEx LED lighting (R. p. 14) */

/* Palette (R. p. 11). */
enum { LPC_OFF = 0, LPC_GREY_DARK = 1, LPC_GREY = 2, LPC_WHITE = 3, LPC_RED = 5, LPC_ORANGE = 9, LPC_YELLOW = 13,
       LPC_LIME = 17, LPC_GREEN = 21, LPC_TEAL = 33, LPC_AZURE = 37, LPC_SKY = 41, LPC_BLUE = 45, LPC_VIOLET = 49,
       LPC_MAGENTA = 53, LPC_PINK = 57 };

/* Eventi decodificati dai messaggi del Launchpad. */
enum { LP_EV_NONE, LP_EV_PAD, LP_EV_STEP, LP_EV_OVERVIEW, LP_EV_TOP, LP_EV_SCENE };
enum { LP_TOP_UP, LP_TOP_DOWN, LP_TOP_LEFT, LP_TOP_RIGHT, LP_TOP_SESSION, LP_TOP_DRUMS, LP_TOP_KEYS, LP_TOP_USER };
enum { LP_SCENE_PAT_A, LP_SCENE_PAT_B, LP_SCENE_PAT_C, LP_SCENE_PAT_D, LP_SCENE_MUTE, LP_SCENE_SOLO, LP_SCENE_CLEAR, LP_SCENE_TAP };

typedef struct {
    int kind;                 /* LP_EV_* */
    int index;                /* pad 0-15, passo 0-15, riga 0-3 della vista d'insieme, tasto in alto o scena 0-7 */
    int col;                  /* colonna 0-7 (vista d'insieme) */
    int on;                   /* 1 premuto, 0 rilasciato */
    float vel;                /* forza del colpo 0..1 (vedi sopra) */
} LpEvent;

typedef struct {
    int connected;            /* riconosciuto e in modalita' Programmer */
    int port;                 /* porta da cui arrivano i pad; -1 finche' non si sa (si manda a tutte) */
    int shift;                /* User tenuto */
    unsigned char type[LP_LEDS], col[LP_LEDS], col2[LP_LEDS];                /* stato voluto dei LED */
    unsigned char dev_type[LP_LEDS], dev_col[LP_LEDS], dev_col2[LP_LEDS];    /* stato sul dispositivo (0xFF = ignoto) */
    int frames_sent;          /* SysEx LED inviati (diagnosi e prove) */
} Launchpad;

/* 1 se il nome del dispositivo e' un Launchpad Mini MK3. Il kit legge il nome da /proc/asound/cardN/id,
   che e' il nome USB senza spazi e tagliato a 15 caratteri («LaunchpadMiniMK»): si confronta senza
   spazi e senza distinguere le maiuscole; il Mini MK2 si presenta come «Launchpad Mini» e non corrisponde. */
int  lp_match(const char *device);

/* Entra in modalita' Programmer (su tutte le porte, finche' non si sa quale ascolta) e prepara il
   ridisegno completo; lp_disconnect torna in Live mode (anche all'uscita dell'app). */
void lp_connect(Launchpad *lp);
void lp_disconnect(Launchpad *lp);

/* Traduce un messaggio MIDI in un evento del Launchpad: 1 se lo era (ev riempito), 0 se non e' suo.
   Aggiorna da solo lo stato di User (shift) e la porta. */
int  lp_decode(Launchpad *lp, int port, const unsigned char *msg, int len, LpEvent *ev);

/* Ricostruisce lo stato voluto dei LED dal sampler (glow: luci dei pad dell'interfaccia, 16 valori 0..1). */
void lp_render(Launchpad *lp, Sampler *s, int sel, int bank, const float *glow);
/* Manda le differenze in un solo SysEx (niente se non cambia nulla): una volta per fotogramma. */
void lp_flush(Launchpad *lp);

/* Primo dei 4 pad mostrati nelle righe 5-8 per il pad scelto. */
int  lp_overview_first(int sel);
/* Indice LED di un pad, di un passo, di una cella della vista d'insieme, di un tasto in alto o a destra. */
int  lp_led_pad(int pad);
int  lp_led_step(int step);
int  lp_led_overview(int row, int col);
int  lp_led_top(int i);
int  lp_led_scene(int i);
/* Colore di palette di un pad caricato: il gruppo di esclusione o, senza gruppo, la cartella del file. */
int  lp_pad_colour(const Sample *smp, const PadParams *pp);

#endif
