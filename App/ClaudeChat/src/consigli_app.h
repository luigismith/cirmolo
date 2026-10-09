/* Cirmolo - "Cosa gioco?": tre domande (tempo, voglia, gioco nuovo o da riprendere), poi il modello della
 * console sceglie 4 giochi tra quelli della SD, tenendo conto del diario delle partite. A avvia il gioco
 * scelto: l'app scrive il comando in /tmp/ia-gioca.sh e lo lancia il suo launch.sh dopo l'uscita
 * (principal.sh cancella /tmp/cmd_to_run.sh quando l'app si chiude). */
#ifndef CLAUDECHAT_CONSIGLI_H
#define CLAUDECHAT_CONSIGLI_H

#include "collezione.h"
#include "gfx.h"

typedef struct Consigli Consigli;

/* Istruzioni (catalogo e diario compresi) e domanda per il modello (malloc). */
char *consigli_system_prompt(const char *language, const char *catalog, const char *digest);
char *consigli_user_prompt(int time, int mood, int kind, const char *exclude);
/* Giochi scelti dal JSON del modello: indici nella collezione e motivi; restituisce quanti (al massimo max). */
int consigli_parse(const Collection *c, const char *reply, int *games, char (*why)[300], int max);

/* root: la SD (/mnt/SDCARD); saves: Saves/claude; play_cmd: dove scrivere il comando del gioco scelto. */
Consigli *consigli_create(const char *root, const char *saves, const char *play_cmd);
void consigli_destroy(Consigli *s);
void consigli_button(Consigli *s, int pad, int pressed);
void consigli_update(Consigli *s, float dt);
void consigli_draw(Consigli *s, Canvas *c);
int  consigli_needs_draw(Consigli *s);
int  consigli_wants_quit(const Consigli *s);

/* prove senza rete */
void consigli_set_offline(Consigli *s, int offline);
void consigli_feed(Consigli *s, const char *sse, int finish);
int  consigli_busy(const Consigli *s);
int  consigli_count(const Consigli *s);
int  consigli_page(const Consigli *s);           /* 0 domande, 1 risultati */
const char *consigli_error(const Consigli *s);
const Collection *consigli_collection(const Consigli *s);

#endif
