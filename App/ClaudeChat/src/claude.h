/* Chiedi a Claude - client della Messages API di Anthropic (HTTP grezzo, il C non ha un SDK ufficiale).
 *
 * - Conversation: la cronologia come va rimandata all'API. Ogni messaggio conserva il suo JSON esatto,
 *   pensieri (thinking) compresi, e non viene mai modificato: la storia cresce solo in coda, come chiede
 *   il "preserved thinking" di Claude Opus 5.5. Uno scambio fallito o interrotto si esclude per intero.
 * - Reply: la risposta in arrivo, letta dallo stream SSE (message_start, content_block_*, message_delta,
 *   message_stop, error), con rifiuti (stop_reason "refusal") e ripieghi su un altro modello.
 * - Transfer: la richiesta HTTPS, fatta da curl in un processo figlio (sulla console; sul PC no).
 */
#ifndef CLAUDECHAT_CLAUDE_H
#define CLAUDECHAT_CLAUDE_H

#include "json.h"

enum { MODEL_OPUS, MODEL_SONNET, MODEL_HAIKU, MODEL_COUNT };
enum { EFFORT_LOW, EFFORT_MEDIUM, EFFORT_HIGH, EFFORT_COUNT };

typedef struct {
    const char *id, *name;
    int fallbacks;                     /* ripiego lato server sui rifiuti ("default") */
    double in, out, cache_write, cache_read;   /* dollari per milione di token (listino del 06/10/2026) */
} ModelInfo;
extern const ModelInfo CLAUDE_MODELS[MODEL_COUNT];
extern const char *const CLAUDE_EFFORTS[EFFORT_COUNT];

enum { ROLE_USER, ROLE_ASSISTANT };
typedef struct {
    int role;
    char *json;                        /* {"role":...,"content":...} da rimandare identico */
    char *text;                        /* testo da mostrare */
    int excluded;                      /* scambio fallito o interrotto: non si rimanda */
    char *note;                        /* nota sotto il messaggio (errori, modello di ripiego) o NULL */
} ChatMsg;

typedef struct {
    char *system;                      /* fissato all'inizio della conversazione */
    ChatMsg *msg;
    int n, cap;
    long in_tokens, out_tokens, cache_read, cache_write;
    double cost;                       /* stima in dollari */
} Conversation;

void conv_init(Conversation *c, const char *system);
void conv_free(Conversation *c);
ChatMsg *conv_add(Conversation *c, int role, char *json, char *text);   /* prende json e text (malloc) */
void conv_add_user(Conversation *c, const char *text);
/* Corpo JSON della richiesta con tutta la cronologia valida (stream, cache automatica, ripieghi). */
char *conv_request(const Conversation *c, int model, int effort);
const char *conv_beta(int model);                                       /* intestazione anthropic-beta o NULL */
/* Salvataggio: {"system":...,"messages":[...],"excluded":[...]} con i JSON dei messaggi ricopiati. */
char *conv_save(const Conversation *c);
int conv_load(Conversation *c, const char *json, size_t len);           /* 0 se va bene */

enum { RS_IDLE, RS_WAITING, RS_THINKING, RS_WRITING, RS_DONE, RS_ERROR, RS_REFUSED, RS_CANCELLED };
enum { BLK_TEXT, BLK_THINKING, BLK_REDACTED, BLK_FALLBACK, BLK_OTHER };
#define REPLY_BLOCKS 32

typedef struct {
    int type;
    Buf a;                             /* testo, pensiero, dati, JSON grezzo, modello di partenza */
    Buf b;                             /* firma del pensiero, modello di arrivo */
} Block;

typedef struct {
    int state;
    Block blk[REPLY_BLOCKS];
    int nblk, cur;                     /* cur: blocco in arrivo (-1 nessuno) */
    char model[64];                    /* modello che risponde (message_start) */
    char stop_reason[32], category[32];
    char error[300];
    long in_tokens, out_tokens, cache_read, cache_write;
    int http_status, fallback;
    Buf line, body;                    /* riga SSE in costruzione; corpo non SSE (errori HTTP) */
} Reply;

void reply_init(Reply *r);
void reply_free(Reply *r);
void reply_feed(Reply *r, const char *data, size_t len);
/* Fine del trasferimento: exit code di curl (0 se tutto bene) e il suo messaggio d'errore. */
void reply_finish(Reply *r, int curl_exit, const char *curl_err);
const char *claude_model_name(const char *id);     /* "Claude Opus 5.5" per "claude-opus-5-5" */
char *reply_text(const Reply *r);                  /* testo da mostrare (malloc) */
char *reply_message_json(const Reply *r);          /* messaggio per la cronologia, NULL se non c'e' testo */
const char *reply_status(const Reply *r);          /* "sta pensando", "scrive"... */

typedef struct {
    int pid, out, err;
    char hdr[64], body[64];
    Buf errbuf;
} Transfer;

/* Avvia la richiesta: 0 se curl e' partito, altrimenti -1 con il motivo in msg. */
int transfer_start(Transfer *t, const char *key, const char *beta, const char *body, char *msg, size_t msgn);
/* Legge quello che e' arrivato e lo passa alla risposta; 1 quando il trasferimento e' finito. */
int transfer_poll(Transfer *t, Reply *r);
void transfer_cancel(Transfer *t);

#endif
