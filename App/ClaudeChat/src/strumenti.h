/* Chiedi all'IA - strumenti che il modello puo' usare per agire sulla console (comandi a voce o scritti):
 * stato della console, volume, luminosita', ricerca e avvio dei giochi, diario delle partite.
 * Le descrizioni per il modello sono in inglese (le capiscono tutti); le risposte all'utente restano nella
 * sua lingua. Volume e luminosita' passano da ia-azioni.sh, che usa le funzioni di spruce. */
#ifndef CLAUDECHAT_STRUMENTI_H
#define CLAUDECHAT_STRUMENTI_H

#include <stddef.h>

#include "collezione.h"

/* Elenco degli strumenti in JSON, nel formato di Anthropic o in quello compatibile OpenAI. */
const char *strumenti_anthropic(void);
const char *strumenti_openai(void);

typedef struct {
    const char *saves;                 /* Saves/claude (diario) */
    const char *app_dir;               /* cartella con ia-azioni.sh ("" = quella corrente) */
    const char *root;                  /* la SD, per la collezione */
    const char *play_cmd;              /* dove scrivere il comando del gioco da avviare */
    Collection *coll;
    int *coll_loaded;
    int launched;                      /* 1 se avvia_gioco ha preparato un gioco */
    char note[160];                    /* cosa e' successo, per un avviso breve sullo schermo */
} ToolCtx;

/* Esegue lo strumento con gli argomenti JSON; restituisce il risultato da dare al modello (malloc). */
char *strumento_esegui(ToolCtx *ctx, const char *name, const char *args_json);

#endif
