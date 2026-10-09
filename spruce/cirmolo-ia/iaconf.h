/* Cirmolo IA - impostazioni delle funzioni IA della console (traduttore, schede dei giochi...), lette da
 * Saves/claude/impostazioni.txt come le scrive Chiedi all'IA: ia.provider e ia.model (il modello della
 * console; se mancano, quelli della chat), traduzione.lingua, traduzione.voce, tts e voice.<fornitore>. */
#ifndef CIRMOLO_IA_IACONF_H
#define CIRMOLO_IA_IACONF_H

#include "providers.h"

typedef struct {
    char provider[32], model[96];      /* modello della console (o della chat) */
    char lang[16];                     /* lingua della traduzione ("" = dell'interfaccia) */
    char tts[32], voice[32];           /* sintesi vocale e voce scelte */
    int speak;                         /* traduzione letta ad alta voce */
    int diary_ai;                      /* diario: riassunto del modello a fine partita */
    int remind;                        /* promemoria "dove eri rimasto" all'avvio del gioco */
} IaSettings;

void ia_settings_read(const char *saves_dir, IaSettings *s);

/* Fornitore, modello e chiave della console; 0 se va bene, altrimenti -1 con il motivo (tradotto) in err. */
int ia_console_model(const Registry *reg, const IaSettings *s, const char *saves_dir,
                     const Provider **p, const Model **m, char *key, size_t keyn, char *err, size_t errn);

/* Sintesi vocale scelta: fornitore e voce (NULL se non c'e' o manca la chiave). */
const Provider *ia_tts(const Registry *reg, const IaSettings *s, const char *saves_dir, char *key, size_t keyn,
                       char *voice, size_t voicen);

#endif
