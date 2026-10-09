/* Cirmolo IA - diario delle partite: una riga JSON per partita in Saves/claude/diario/diario.jsonl con gioco,
 * sistema, inizio, durata, ultima schermata (la miniatura del salvataggio automatico di RetroArch, copiata in
 * diario/img/) e, se e' attivo, un riassunto del modello ("dove eri rimasto"). Lo scrive ia-diario alla fine
 * di ogni partita; lo leggono la scheda del gioco e il promemoria all'avvio. */
#ifndef CIRMOLO_IA_DIARIO_H
#define CIRMOLO_IA_DIARIO_H

#include <stddef.h>

typedef struct {
    char rom[512], system[64], name[200], img[300], summary[700];
    long start, secs;
} DiaryEntry;

int  diary_append(const char *saves_dir, const DiaryEntry *e);
/* Ultima partita del gioco (0 se c'e'), con il tempo totale e il numero di partite. */
int  diary_last(const char *saves_dir, const char *rom, DiaryEntry *out, long *total_secs, int *sessions);
/* Nome leggibile dal file: senza cartella ed estensione. */
void diary_name_from_rom(const char *rom, char *out, size_t n);
/* "25 min", "1 h 20 min" */
void diary_duration(long secs, char *out, size_t n);
/* "09/10, 21:30" */
void diary_date(long epoch, char *out, size_t n);
/* Partite recenti per i consigli (malloc), una riga per gioco dalla piu' recente, al massimo max:
   "SFC | Chrono Trigger (USA) | 3 partite, 4 h 10 min, ultima il 09/10 | dove eri rimasto". NULL se vuoto. */
char *diary_digest(const char *saves_dir, int max);
/* Istruzioni per il riassunto dell'ultima schermata (malloc), nella lingua indicata. */
char *diary_summary_prompt(const char *language, const DiaryEntry *e);

#endif
