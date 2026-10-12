/* OpenOrc - configurazione di una tastiera MIDI con l'intelligenza artificiale.
 *
 * Tre passi. Ascolto: l'utente muove tutto quello che ha la tastiera e qui si annota cosa manda ogni
 * controllo (nota o CC, porta, canale, valori). Domanda: al modello vanno il nome della tastiera e i
 * controlli visti, con l'elenco delle funzioni di OpenOrc; il modello usa anche quello che sa di quel
 * modello di tastiera per rispettarne l'ordine fisico. Risposta: un JSON con l'assegnazione, che qui si
 * controlla (solo controlli visti davvero, ognuno una volta sola) prima di proporla all'utente.
 *
 * Il modello predefinito e' il migliore gratuito tra quelli con una chiave salvata (vedi ia_pick_model in
 * orc_app.c): Groq con gpt-oss 120B, gratis senza carta e velocissimo.
 */
#ifndef OPENORC_IA_H
#define OPENORC_IA_H

#include <stddef.h>

enum { IA_NONE, IA_NOTE, IA_CC };                    /* stessi valori di MAP_NONE, MAP_NOTE, MAP_CC */
typedef struct { int kind, port, ch, num; } IaMap;  /* port e ch da 0; -1 = qualsiasi */

#define IA_CTL_MAX 192
typedef struct {
    int kind, port, ch, num;                          /* port e ch da 0 */
    int count, vmin, vmax, order;
    int rel_small, rel_mid, zero_full;                /* valori 1-15/113-127, 56-72, solo 0 o 127 */
    unsigned char vals[16];                           /* valori diversi visti (bit per gruppi di 8) */
} IaCtl;

typedef struct {
    IaCtl c[IA_CTL_MAX];
    int n, order, total;
    int bend[8 * 16];                                 /* pitch bend visto per porta e canale */
    int aftertouch, program;
} IaScan;

void ia_scan_reset(IaScan *s);
/* Un messaggio MIDI durante l'ascolto (type: 0x90, 0x80, 0xB0, 0xE0...). */
void ia_scan_add(IaScan *s, int port, int type, int ch, int d1, int d2);
/* Conti per lo schermo: note diverse, controlli continui, pulsanti CC. */
void ia_scan_counts(const IaScan *s, int *notes, int *knobs, int *buttons, int *bends);
/* Una riga leggibile per ogni gruppo di controlli (per lo schermo e per il modello); restituisce le righe
   scritte in out (malloc), una per riga. */
char *ia_scan_describe(const IaScan *s, int for_model);

/* Nome lungo della scheda ALSA ("AKAI professional LLC MPK mini IV") dato l'id corto ("MPKminiIV"). */
void ia_device_longname(const char *id, char *out, size_t n);

/* Istruzioni e domanda per il modello (malloc). targets: nomi esatti delle funzioni, descr: a cosa servono. */
char *ia_system_prompt(void);
char *ia_user_prompt(const IaScan *s, const char *device, const char *longname, const char *const *targets,
                     const char *const *descr, int ntargets);

/* Legge la risposta: assegnazioni in map (ntargets voci), tipo di manopole (0 assolute, 1 relative,
   2 relative 64; -1 se il modello non lo dice), nome amichevole e nota in italiano. Ogni assegnazione deve
   essere un controllo visto nell'ascolto; quelle che non lo sono si scartano. Restituisce quante funzioni
   sono state assegnate, -1 se la risposta non e' leggibile (motivo in err). */
int ia_parse_reply(const char *text, const IaScan *s, const char *const *targets, int ntargets, IaMap *map,
                   int *knob_mode, char *name, size_t namen, char *note, size_t noten, char *err, size_t errn);

/* Tipo di manopole dedotto dai valori dei CC assegnati alle manopole (-1 se non si capisce). */
int ia_guess_knob_mode(const IaScan *s, const IaMap *knobs, int n);

/* Confronto di nomi di dispositivi senza spazi, maiuscole e segni: "MPK mini IV" = "MPKminiIV". */
int ia_same_device(const char *a, const char *b);

#endif
