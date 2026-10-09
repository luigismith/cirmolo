/* Chiedi all'IA - fornitori e modelli.
 *
 * L'elenco di base e' qui (verificato sulle pagine ufficiali il 09/10/2026); l'utente puo' aggiungerne o
 * cambiarne con Saves/claude/fornitori.json (stesso formato di fornitori-esempio.json nella cartella
 * dell'app), per esempio un server Ollama sul PC di casa. I modelli si possono riscaricare dal fornitore
 * (GET /models): l'elenco scaricato finisce in Saves/claude/modelli/<id>.txt.
 * Le chiavi stanno in Saves/claude/chiavi/<id>.txt (quella di Anthropic anche in chiave.txt, come nella 0.2).
 */
#ifndef CLAUDECHAT_PROVIDERS_H
#define CLAUDECHAT_PROVIDERS_H

#include "llm.h"

enum { STT_NONE, STT_OPENAI, STT_GEMINI };            /* trascrizione: multipart compatibile OpenAI o Gemini */
enum { TTS_NONE, TTS_OPENAI, TTS_GEMINI };            /* sintesi: /audio/speech in PCM o Gemini TTS */

#define MF_FREE   1                                   /* gratuito (con i limiti del fornitore) */
#define MF_EFFORT 2                                   /* accetta output_config.effort (Claude) */
#define MF_FALLBACK 4                                 /* ripiego lato server sui rifiuti (Claude) */

typedef struct {
    char id[96], name[64];
    double in, out, cache_read;                       /* dollari per milione di token; < 0 sconosciuto */
    int flags;
} Model;

typedef struct {
    char id[24], name[40];
    int proto;                                        /* PROTO_ANTHROPIC o PROTO_OPENAI */
    char base[160];                                   /* fino a /v1 compreso (senza / finale) */
    char site[64];                                    /* dove si crea la chiave */
    char note[120];                                   /* una riga di aiuto (gratis, limiti...) */
    char headers[200];                                /* intestazioni in piu' ("Nome: valore\n") */
    int needs_key, stream_options;
    int no_tools;                                     /* il modello non accetta strumenti (si scopre al primo errore) */
    int stt, tts;
    char stt_model[64], tts_model[64], voices[200];   /* voci separate da virgole, la prima e' la predefinita */
    Model *models;
    int nmodels, npreset;
} Provider;

typedef struct {
    Provider *p;
    int n;
} Registry;

void registry_load(Registry *r, const char *dir);     /* elenco di base + fornitori.json + modelli scaricati */
void registry_free(Registry *r);
int registry_find(const Registry *r, const char *id); /* indice o -1 */
int provider_find_model(const Provider *p, const char *id);
/* Sostituisce i modelli scaricati (uno per riga) e li salva in dir/modelli/<id>.txt. */
void provider_set_fetched(Provider *p, const char *dir, const char *const *ids, int n);

/* Chiave del fornitore (stringa vuota se manca); 0 se salvata. */
void provider_key(const Provider *p, const char *dir, char *out, size_t n);
int provider_save_key(const Provider *p, const char *dir, const char *key);
int clean_key(const char *in, char *out, size_t n);   /* 0 se e' una chiave plausibile */

/* Intestazioni HTTP di autenticazione (piu' quelle del fornitore). */
void provider_auth_headers(const Provider *p, const char *key, Buf *h);
/* Elenco dei modelli scaricato: estrae gli id adatti alla chat da {"data":[{"id":...}]} (anche Gemini). */
int parse_model_list(const char *json, size_t len, char ***ids);

const char *model_display_name(const char *id);
double model_cost(const Model *m, long in, long out, long cache_read, long cache_write);

#endif
