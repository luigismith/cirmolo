/* Cirmolo kit - strato comune delle app native di Cirmolo per la Miyoo Flip.
 *
 * Ogni app definisce cirmolo_app() con le sue funzioni; platform.c fa il resto: carica SDL2 a runtime
 * (quella di PyUI), apre la finestra come PyUI, l'audio in uscita e, se richiesto, un ingresso
 * (microfono USB), legge i tasti della Flip da evdev e disegna il framebuffer 640x480 dell'app.
 */
#ifndef CIRMOLO_PLATFORM_H
#define CIRMOLO_PLATFORM_H

#include "gfx.h"

/* Tasti. Prefisso PAD_: i nomi BTN_* sono gia' usati da linux/input.h. */
enum {
    PAD_UP, PAD_DOWN, PAD_LEFT, PAD_RIGHT,
    PAD_A, PAD_B, PAD_X, PAD_Y,
    PAD_L1, PAD_R1, PAD_L2, PAD_R2,
    PAD_SELECT, PAD_START, PAD_MENU,
    PAD_L3, PAD_R3,                       /* pressione delle levette */
    PAD_COUNT
};

typedef struct {
    const char *title;                    /* titolo della finestra e del log */
    const char *state_path;               /* file di stato sulla console (la cartella viene creata) */
    int wants_capture;                    /* 1 = apre anche un ingresso audio, se c'e' */
    int usb_output;                       /* 1 = suona su una scheda audio USB (cuffie), se c'e' all'avvio */

    void *(*create)(float sample_rate, const char *state_path);
    void (*destroy)(void *app);
    /* Thread audio: riempie frames campioni stereo interlacciati. */
    void (*audio)(void *app, float *out, int frames);
    /* Thread dell'ingresso: frames campioni mono. Facoltativa. */
    void (*capture)(void *app, const float *in, int frames);
    /* Ingresso aperto (nome del dispositivo e frequenza) o chiuso (NULL). Facoltativa. */
    void (*capture_status)(void *app, const char *device, float sample_rate);

    /* MIDI da una tastiera USB: un messaggio completo (1-3 byte, vedi midi.h) sul thread dell'interfaccia.
       Facoltativa. Il kit cerca da solo i dispositivi /dev/snd/midiC*D*, anche collegati dopo l'avvio.
       Le porte sono aperte anche in scrittura: l'app manda messaggi al dispositivo (LED di un controller)
       con cirmolo_midi_send() di midi.h, dal thread dell'interfaccia. */
    void (*midi)(void *app, const unsigned char *msg, int len);
    /* Tastiera MIDI collegata (nome) o scollegata (NULL). Facoltativa. */
    void (*midi_status)(void *app, const char *device);

    void (*button)(void *app, int pad, int pressed);
    /* Levette -1..1 (su = negativo), grilletti 0..1. */
    void (*axes)(void *app, float lx, float ly, float rx, float ry, float l2, float r2);
    void (*update)(void *app, float dt);
    void (*draw)(void *app, Canvas *c);
    int  (*wants_quit)(void *app);

    /* Come midi, ma da tutte le porte del dispositivo, con il loro numero (0 = la prima). La MPK mini IV,
       per esempio, manda tasti, pad, rotelle e manopole sulla porta 0 e i tasti di trasporto sulle altre.
       Se c'e', il kit chiama questa al posto di midi; senza, le app ricevono solo la porta 0. Facoltativa. */
    void (*midi_port)(void *app, int port, const unsigned char *msg, int len);

    /* 0 se dall'ultimo disegno sullo schermo non e' cambiato niente: il kit salta disegno e presentazione
       di quel fotogramma (meno CPU e batteria). Facoltativa: senza, si disegna sempre. */
    int  (*needs_draw)(void *app);

    /* Testo da una tastiera (USB sulla console, quella del PC in prova): utf8 con special = TEXT_CHARS,
       oppure un tasto di modifica (utf8 vuoto). Facoltativa: con questa il kit attiva l'inserimento di
       testo di SDL e i tasti della tastiera non fanno piu' da tasti della Flip. */
    void (*text_input)(void *app, const char *utf8, int special);
} CirmoloApp;

enum { TEXT_CHARS, TEXT_BACKSPACE, TEXT_ENTER, TEXT_LEFT, TEXT_RIGHT, TEXT_UP, TEXT_DOWN, TEXT_DELETE };

/* Immagine (PNG, JPG...) letta con la SDL_image di PyUI e ridotta per stare in maxw x maxh: pixel ARGB
   (malloc, li libera l'app) e dimensioni. -1 se non c'e' l'immagine o la libreria. */
int cirmolo_load_image(const char *path, int maxw, int maxh, uint32_t **px, int *w, int *h);

/* Definita da ogni app. */
const CirmoloApp *cirmolo_app(void);

#endif
