/* Cirmolo - scheda del gioco: descrizione, come si gioca, consigli e curiosita' scritti dal modello della
 * console, con la copertina. Si apre dal menu del gioco in PyUI ("Scheda del gioco (IA)"), che scrive i
 * dati del gioco in /tmp/ia-scheda.json: {"rom","system","system_name","name","image"}.
 * La scheda si salva in Saves/claude/schede/<sistema>/<rom senza estensione>.md e la volta dopo si legge
 * da li'. Y la riscrive, X la legge ad alta voce, B torna ai giochi. */
#ifndef CLAUDECHAT_SCHEDA_H
#define CLAUDECHAT_SCHEDA_H

#include <stddef.h>

#include "gfx.h"

typedef struct Scheda Scheda;

typedef struct {
    char rom[512], system[64], system_name[96], name[200], image[512];
} GameInfo;

/* Legge i dati del gioco dal JSON di PyUI; 0 se va bene. */
int scheda_read_request(const char *path, GameInfo *g);
/* Percorso della scheda salvata per il gioco. */
void scheda_card_path(const char *saves_dir, const GameInfo *g, char *out, size_t n);
/* Istruzioni e domanda per il modello (malloc), nella lingua indicata (nome inglese come in PyUI). */
char *scheda_system_prompt(const char *language);
char *scheda_user_prompt(const GameInfo *g);

Scheda *scheda_create(float sample_rate, const char *saves_dir, const GameInfo *g);
void scheda_destroy(Scheda *s);
void scheda_audio(Scheda *s, float *out, int frames);
void scheda_button(Scheda *s, int pad, int pressed);
void scheda_axes(Scheda *s, float ly, float ry);
void scheda_update(Scheda *s, float dt);
void scheda_draw(Scheda *s, Canvas *c);
int  scheda_needs_draw(Scheda *s);
int  scheda_wants_quit(const Scheda *s);

/* prove senza rete */
void scheda_set_offline(Scheda *s, int offline);
void scheda_feed(Scheda *s, const char *sse, int finish);
int  scheda_busy(const Scheda *s);
const char *scheda_text(const Scheda *s);
const char *scheda_error(const Scheda *s);

#endif
