/* Chiedi all'IA - conversazione e risposte dei modelli, con due protocolli:
 *
 * - PROTO_ANTHROPIC: Messages API di Anthropic (HTTP grezzo, il C non ha un SDK ufficiale). I messaggi di
 *   Claude conservano il loro JSON esatto, pensieri (thinking) compresi, e non vengono mai modificati:
 *   la storia cresce solo in coda, come chiede il "preserved thinking" dei modelli Claude recenti.
 * - PROTO_OPENAI: Chat Completions "compatibile OpenAI", lo parlano quasi tutti gli altri (OpenAI, Gemini,
 *   DeepSeek, Qwen, Kimi, GLM, MiniMax, Groq, OpenRouter, Ollama...). Si rimanda solo il testo.
 *
 * Nella stessa conversazione si puo' cambiare modello o fornitore: ai fornitori OpenAI va il testo di
 * tutti i messaggi; ad Anthropic i messaggi di Claude identici e quelli degli altri come testo.
 * Uno scambio fallito o interrotto si esclude per intero (resta da leggere, non si rimanda).
 */
#ifndef CLAUDECHAT_LLM_H
#define CLAUDECHAT_LLM_H

#include "json.h"
#include "net.h"

enum { PROTO_PLAIN, PROTO_ANTHROPIC, PROTO_OPENAI };

enum { ROLE_USER, ROLE_ASSISTANT };
typedef struct {
    int role;
    int proto;                         /* formato di json: PROTO_ANTHROPIC (messaggio di Claude) o PROTO_PLAIN */
    char *json;                        /* {"role":...,"content":...} da rimandare identico */
    char *text;                        /* testo da mostrare */
    int excluded;                      /* scambio fallito o interrotto: non si rimanda */
    char *note;                        /* nota sotto il messaggio (errori, chi ha risposto) o NULL */
    char *model;                       /* modello che ha risposto (assistente) o NULL */
    int tool;                          /* risultati degli strumenti: si rimandano ma non si mostrano */
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
ChatMsg *conv_add(Conversation *c, int role, int proto, char *json, char *text);   /* prende json e text (malloc) */
void conv_add_user(Conversation *c, const char *text);
/* Salvataggio: {"system":...,"messages":[...],"meta":[...],"usage":{...}} con i JSON dei messaggi ricopiati. */
char *conv_save(const Conversation *c);
int conv_load(Conversation *c, const char *json, size_t len);           /* 0 se va bene */

/* Richiesta alla Messages API: stream, cache automatica; effort NULL = predefinito del modello;
   fallbacks = ripiego lato server sui rifiuti ("default", serve l'intestazione beta);
   tools = array JSON degli strumenti nel formato di Anthropic, o NULL. */
char *anthropic_request(const Conversation *c, const char *model, const char *effort, int fallbacks, const char *tools);
#define ANTHROPIC_FALLBACK_BETA "server-side-fallback-2026-07-01"
/* Richiesta Chat Completions in streaming (system come primo messaggio, solo testo); max_tokens 0 = non
   indicato (i modelli recenti di OpenAI vogliono max_completion_tokens); stream_options per avere l'uso. */
char *openai_request(const Conversation *c, const char *model, int max_tokens, int stream_options, const char *tools);
/* Risultati degli strumenti chiamati nell'ultima risposta, come li vuole il protocollo: Anthropic un messaggio
   "user" con i tool_result, OpenAI un messaggio "tool" per chiamata. */
void conv_add_tool_results(Conversation *c, int proto, int n, const char *const *ids, const char *const *results);

enum { RS_IDLE, RS_WAITING, RS_THINKING, RS_WRITING, RS_DONE, RS_ERROR, RS_REFUSED, RS_CANCELLED };
enum { BLK_TEXT, BLK_THINKING, BLK_REDACTED, BLK_FALLBACK, BLK_OTHER, BLK_TOOL };
#define REPLY_BLOCKS 32

typedef struct {
    int type;
    Buf a;                             /* testo, pensiero, dati, JSON grezzo, modello di partenza, argomenti */
    Buf b;                             /* firma del pensiero, modello di arrivo, id della chiamata */
    char name[64];                     /* strumento chiamato (BLK_TOOL) */
} Block;

typedef struct {
    int proto;                         /* PROTO_ANTHROPIC o PROTO_OPENAI */
    char who[48];                      /* nome del fornitore, per i messaggi d'errore */
    int state;
    Block blk[REPLY_BLOCKS];
    int nblk, cur;                     /* cur: blocco in arrivo (-1 nessuno) */
    char model[96];                    /* modello che risponde */
    char stop_reason[32], category[32];
    char error[300];
    long in_tokens, out_tokens, cache_read, cache_write;
    int http_status, fallback;
    Buf line, body;                    /* riga SSE in costruzione; corpo non SSE (errori HTTP) */
    int tool_blk[16];                  /* OpenAI: blocco di ogni tool_calls[].index (+1, 0 = nessuno) */
} Reply;

void reply_init(Reply *r, int proto, const char *who);
void reply_reset(Reply *r);            /* svuota, tenendo protocollo e fornitore */
void reply_free(Reply *r);
void reply_feed(Reply *r, const char *data, size_t len);
/* Fine del trasferimento: exit code di curl (0 se tutto bene) e il suo messaggio d'errore. */
void reply_finish(Reply *r, int curl_exit, const char *curl_err);
char *reply_text(const Reply *r);                  /* testo da mostrare (malloc) */
/* Messaggio per la cronologia (NULL se non c'e' testo) e il suo formato. */
char *reply_message_json(const Reply *r, int *proto);
const char *reply_status(const Reply *r);          /* "sta pensando", "scrive"... */
/* Strumenti chiamati nella risposta (blocchi BLK_TOOL: name, b = id, a = argomenti JSON). */
int reply_tool_count(const Reply *r);
const Block *reply_tool(const Reply *r, int i);

/* interni, uno per protocollo */
void anthropic_line(Reply *r, const char *data, size_t n);
void anthropic_http_error(Reply *r);
char *anthropic_message_json(const Reply *r);
void openai_line(Reply *r, const char *data, size_t n);
void openai_http_error(Reply *r);
void reply_set_error(Reply *r, const char *msg);
Block *reply_new_block(Reply *r, int type);

/* Nome leggibile di un modello noto ("Claude Opus 5.5" per "claude-opus-5-5"), altrimenti l'id. */
const char *model_display_name(const char *id);

#endif
