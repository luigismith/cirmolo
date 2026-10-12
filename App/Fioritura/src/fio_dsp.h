/* Fioritura per Miyoo Flip - motore audio.
 *
 * Sei strumenti a una nota (campana FM, vetro, arpa a corda pizzicata, soffio, goccia, kalimba) da 32 voci
 * in stereo, piu' un bordone (tonica e quinta, tre seghe desintonizzate dietro un filtro che respira),
 * eco ping-pong, riverbero lungo con shimmer (la coda rimanda un'ottava sopra) e un limitatore morbido.
 *
 * Tempo: le note arrivano con l'istante (in campioni assoluti) in cui devono partire. L'interfaccia le
 * prepara con un piccolo anticipo (vedi fio_app.c) e il thread audio le fa partire al momento giusto,
 * a passi di 32 campioni (0,67 ms): cosi' gli echi del giardino e i passi delle lucciole restano a tempo
 * anche se l'interfaccia disegna a scatti.
 *
 * Thread: fio_render() gira nel thread audio; il resto arriva dall'interfaccia con una coda senza lock
 * (un produttore, un consumatore). Le letture di stato (orologio, livello, voci) sono atomiche.
 */
#ifndef FIORITURA_DSP_H
#define FIORITURA_DSP_H

#define FIO_VOICES 32

enum { TIM_CAMPANA, TIM_VETRO, TIM_ARPA, TIM_SOFFIO, TIM_GOCCIA, TIM_KALIMBA, TIM_COUNT };

typedef struct {
    float reverb;              /* 0..1: quanto spazio (mandata e lunghezza della coda) */
    float shimmer;             /* 0..1: la coda del riverbero risale di un'ottava */
    float delay;               /* 0..1: eco ping-pong */
    float delay_time;          /* secondi, 0,1..1,2 */
    float drone;               /* 0..1: livello del bordone (0 = spento) */
    float drone_note;          /* nota MIDI della tonica del bordone */
    float volume;              /* 0..1 */
} FioMix;

typedef struct {
    int   timbre;              /* TIM_* */
    float note;                /* nota MIDI (anche frazionaria) */
    float vel;                 /* 0..1 */
    float pan;                 /* -1 sinistra .. 1 destra */
    float bright;              /* 0..1: brillantezza */
    float length;              /* secondi: durata caratteristica (coda della campana, della corda...) */
} FioNote;

typedef struct Fio Fio;

Fio  *fio_create(float sample_rate);
void  fio_destroy(Fio *f);
void  fio_render(Fio *f, float *out, int frames);      /* stereo interlacciato */

/* Dal thread dell'interfaccia. */
void  fio_set_mix(Fio *f, const FioMix *m);
/* when: campione assoluto (vedi fio_clock) in cui la nota deve partire; 0 o un istante passato = subito.
   0 se accodata, -1 se la coda e' piena. */
int   fio_play(Fio *f, long long when, const FioNote *n);
void  fio_silence(Fio *f);                             /* spegne in fretta le voci e svuota le attese */

/* Stato: letture atomiche, sicure dal thread dell'interfaccia. */
long long fio_clock(const Fio *f);                     /* campioni prodotti finora */
float fio_level(const Fio *f);                         /* livello recente dell'uscita, 0..1 */
int   fio_active_voices(const Fio *f);
int   fio_voice_starts(const Fio *f);                  /* voci partite dalla creazione (prove) */
long long fio_last_start(const Fio *f);                /* istante (campioni) dell'ultima voce partita (prove) */

/* Registrazione: con il rubinetto aperto il thread audio copia l'uscita in un anello di 4 secondi;
   l'interfaccia lo svuota con fio_tap_read (frames stereo interlacciati, restituisce quanti ne ha letti). */
void  fio_tap(Fio *f, int on);
int   fio_tap_read(Fio *f, float *dst, int max_frames);
int   fio_tap_lost(const Fio *f);                      /* frames persi perche' l'anello era pieno */

const char *fio_timbre_name(int timbre);

#endif
