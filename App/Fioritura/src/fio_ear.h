/* Fioritura - l'orecchio del giardino: ascolta un microfono USB e riconosce tre gesti.
 *
 * - Canto: una nota tenuta (voce, fischio, flauto, una corda). YIN (de Cheveigne' e Kawahara, 2002)
 *   circa ogni 21 ms, da 70 a 2000 Hz; la nota conta quando resta ferma per un decimo di secondo.
 * - Soffio: un suono forte e senza altezza che dura (si soffia sul microfono, oppure «ssss»).
 * - Battito: un attacco secco che si spegne subito (mani, schiocco di dita, un colpo sul tavolo).
 * Misura anche quanto suono c'e' attorno a ogni corda del giardino: le corde vibrano per simpatia.
 *
 * Il giardino non deve sentire se stesso: oltre al rumore di fondo, l'orecchio controlla se dopo ogni
 * attacco del giardino il microfono si alza (altoparlante vicino); se si', impara di quanto e ascolta solo
 * quello che lo supera. In cuffia se ne accorge dopo pochi attacchi e non ne tiene conto.
 *
 * Thread: fear_feed() dal thread del microfono (anello senza lock, un produttore e un consumatore);
 * tutto il resto dal thread dell'interfaccia.
 */
#ifndef FIORITURA_EAR_H
#define FIORITURA_EAR_H

#define FEAR_STRINGS 64

enum { EAR_NOTE = 1, EAR_CLAP, EAR_BLOW_START, EAR_BLOW_END };

typedef struct {
    int    kind;          /* EAR_* */
    float  note;          /* EAR_NOTE: nota MIDI (frazionaria) */
    float  strength;      /* 0..1 */
    double t;             /* quando e' cominciato, in secondi di microfono analizzati (vedi fear_time) */
} EarEvent;

typedef struct FioEar FioEar;

FioEar *fear_create(void);
void  fear_destroy(FioEar *e);
void  fear_feed(FioEar *e, const float *in, int n);          /* thread del microfono: campioni mono */

void  fear_set_rate(FioEar *e, float rate);                  /* frequenza del microfono; 0 = nessuno */
void  fear_set_sensitivity(FioEar *e, float s);              /* 0..1 */
void  fear_set_strings(FioEar *e, const float *notes, int n);/* corde del giardino (note MIDI) */

/* Analizza il suono arrivato dall'ultima chiamata. out_level: livello recente dell'uscita del giardino
   (fio_level). Mette in ev fino a max eventi e restituisce quanti. */
int   fear_poll(FioEar *e, float out_level, EarEvent *ev, int max);

/* Stato dopo fear_poll. */
double fear_time(const FioEar *e);                           /* secondi di microfono analizzati */
int   fear_voiced(const FioEar *e);                          /* 1 se in questo momento sente una nota */
float fear_pitch(const FioEar *e);                           /* la nota che sente (MIDI, frazionaria) */
float fear_level(const FioEar *e);                           /* 0..1: quanto il suono supera la soglia */
float fear_blow(const FioEar *e);                            /* 0..1: forza del soffio, 0 se non soffia */
float fear_string(const FioEar *e, int i);                   /* 0..1: vibrazione per simpatia della corda i */
/* Il microfono sente l'altoparlante? 1 si' (provato su piu' attacchi del giardino), -1 no (cuffie, o
   altoparlante lontano), 0 non si sa ancora. */
int   fear_hears_output(const FioEar *e);
float fear_threshold_db(const FioEar *e);                    /* soglia attuale (prove e registro) */
float fear_floor_db(const FioEar *e);                        /* rumore di fondo (registro) */
float fear_leak_db(const FioEar *e);                         /* quanto dell'uscita rientra, -80 se non si sa (registro) */

#endif
