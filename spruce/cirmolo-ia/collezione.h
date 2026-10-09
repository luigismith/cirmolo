/* Cirmolo IA - la collezione di giochi sulla SD, per i consigli ("Cosa gioco?").
 *
 * Sistemi da Emu/<SISTEMA>/config.json (label, launch, extlist), giochi da Roms/<SISTEMA> (e una cartella
 * sotto, come ARCADE/CPS1+2), nomi da miyoogamelist.xml o gamelist.xml quando ci sono, altrimenti dal file.
 * Le edizioni dello stesso gioco (Europe, USA, Japan...) diventano un titolo solo: si tiene il file in
 * italiano o europeo se c'e'. Il catalogo per il modello e' compatto: una riga per sistema con i titoli
 * separati da " | ". */
#ifndef CIRMOLO_IA_COLLEZIONE_H
#define CIRMOLO_IA_COLLEZIONE_H

#include <stddef.h>

typedef struct {
    char id[32], label[48], launch[300];       /* id = cartella in Emu/ e Roms/; launch = percorso completo */
} GameSystemInfo;

typedef struct {
    int sys;                                  /* indice in Collection.sys */
    char title[160];                          /* titolo pulito (senza regione e tag) */
    char rom[400];
    int pref;                                 /* preferenza dell'edizione (piu' alto = meglio) */
} CollGame;

typedef struct {
    GameSystemInfo *sys;
    int nsys;
    CollGame *g;
    int n, cap;
} Collection;

/* Legge la collezione da root (di solito /mnt/SDCARD). */
void collection_scan(Collection *c, const char *root);
void collection_free(Collection *c);
/* Titolo pulito da un nome: via estensione, "(USA)", "[!]" e simili. */
void collection_clean_title(const char *name, char *out, size_t n);
/* Catalogo per il modello (malloc): "## SFC (SNES)\nTitolo | Titolo | ...\n" */
char *collection_catalog(const Collection *c);
/* Gioco per sistema e titolo come li ha scritti il modello (anche approssimati); -1 se non c'e'. */
int collection_find(const Collection *c, const char *system, const char *title);
/* Ricerca per parole (tutte devono esserci nel titolo, anche a pezzi), facoltativamente in un sistema:
   indici dei giochi in out, al massimo max; restituisce quanti. */
int collection_search(const Collection *c, const char *text, const char *system, int *out, int max);
/* Comando per avviare il gioco come fa PyUI (malloc). */
char *collection_launch_cmd(const Collection *c, int game);

#endif
