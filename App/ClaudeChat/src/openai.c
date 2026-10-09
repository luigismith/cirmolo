/* Chiedi all'IA - risposte "compatibili OpenAI" (Chat Completions in streaming).
 *
 * Ogni riga "data:" e' un pezzo: choices[0].delta.content (testo), delta.reasoning_content o
 * delta.reasoning (ragionamento: DeepSeek, Qwen, Kimi, GLM, OpenRouter, Groq), choices[0].finish_reason,
 * e in fondo usage (con stream_options.include_usage). "data: [DONE]" chiude. Gli errori arrivano come
 * {"error":{...}} (anche a meta' stream) o, su Gemini, come [{"error":{...}}].
 */
#include "llm.h"

#include "i18n.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Block *current(Reply *r, int type)
{
    if (r->cur >= 0 && r->blk[r->cur].type == type) return &r->blk[r->cur];
    return reply_new_block(r, type);
}

static void api_error(Reply *r, int status, const JNode *e)
{
    const char *msg = json_str(e, "message");
    const char *code = json_str(e, "code"), *type = json_str(e, "type"), *st = json_str(e, "status");
    char low[200] = "";
    snprintf(low, sizeof(low), "%s %s %s %s", msg ? msg : "", code ? code : "", type ? type : "", st ? st : "");
    for (char *p = low; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    char out[300];
    const char *who = r->who[0] ? r->who : tr("il fornitore");
    if (status == 401 || strstr(low, "invalid api key") || strstr(low, "invalid_api_key") || strstr(low, "api key not valid") || strstr(low, "unauthorized"))
        snprintf(out, sizeof(out), tr("Chiave API di %s non valida: controllala nelle impostazioni."), who);
    else if (status == 402 || strstr(low, "insufficient") || strstr(low, "balance") || strstr(low, "quota") || strstr(low, "credit"))
        snprintf(out, sizeof(out), tr("Credito o quota esauriti su %s.%s"), who, status == 429 ? tr(" Riprova più tardi o cambia modello.") : "");
    else if (status == 429 || strstr(low, "rate limit") || strstr(low, "resource_exhausted"))
        snprintf(out, sizeof(out), tr("Limite di richieste raggiunto su %s: aspetta un po' o cambia modello."), who);
    else if (status == 404 || strstr(low, "model_not_found") || strstr(low, "does not exist") || strstr(low, "not found"))
        snprintf(out, sizeof(out), tr("Modello non trovato su %s: aggiorna l'elenco dei modelli."), who);
    else if (status == 403 || strstr(low, "permission") || strstr(low, "region") || strstr(low, "location"))
        snprintf(out, sizeof(out), tr("Accesso negato da %s (chiave, modello o paese non abilitati)."), who);
    else if (status == 413 || strstr(low, "context length") || strstr(low, "too long") || strstr(low, "maximum context"))
        snprintf(out, sizeof(out), "%s", tr("Conversazione troppo lunga per questo modello: iniziane una nuova."));
    else if (status >= 500)
        snprintf(out, sizeof(out), tr("Errore sul server di %s (HTTP %d): riprova."), who, status);
    else if (msg && *msg)
        snprintf(out, sizeof(out), tr("Richiesta rifiutata da %s: %s"), who, msg);
    else
        snprintf(out, sizeof(out), tr("Risposta inattesa da %s (HTTP %d)."), who, status);
    reply_set_error(r, out);
}

void openai_line(Reply *r, const char *data, size_t n)
{
    if (n >= 6 && !strncmp(data, "[DONE]", 6)) {
        if (r->state != RS_ERROR && r->state != RS_REFUSED) r->state = RS_DONE;
        return;
    }
    JNode *ev = json_parse(data, n);
    if (!ev) return;
    const JNode *err = json_get(ev, "error");
    if (err) { api_error(r, r->http_status, err); json_free(ev); return; }
    const char *model = json_str(ev, "model");
    if (model && !r->model[0]) snprintf(r->model, sizeof(r->model), "%s", model);
    if (r->state == RS_IDLE) r->state = RS_WAITING;
    const JNode *ch = json_get(ev, "choices");
    const JNode *c0 = ch && ch->type == J_ARRAY ? ch->child : NULL;
    if (c0) {
        const JNode *d = json_get(c0, "delta");
        const char *think = json_str(d, "reasoning_content");
        if (!think) think = json_str(d, "reasoning");
        if (think && *think) {
            Block *b = current(r, BLK_THINKING);
            if (b) buf_adds(&b->a, think);
            r->state = RS_THINKING;
        }
        const char *text = json_str(d, "content");
        if (text && *text) {
            Block *b = current(r, BLK_TEXT);
            if (b) buf_adds(&b->a, text);
            int open = b && strstr(b->a.p, "<think>") && !strstr(b->a.p, "</think>");
            r->state = open ? RS_THINKING : RS_WRITING;
        }
        const char *fr = json_str(c0, "finish_reason");
        if (fr) {
            snprintf(r->stop_reason, sizeof(r->stop_reason), "%s", fr);
            if (!strcmp(fr, "content_filter")) { r->state = RS_REFUSED; snprintf(r->category, sizeof(r->category), "%s", tr("filtro")); }
        }
    }
    const JNode *u = json_get(ev, "usage");
    if (u && u->type == J_OBJECT) {
        long cached = (long)json_num(json_get(u, "prompt_tokens_details"), "cached_tokens", 0);
        if (!cached) cached = (long)json_num(u, "prompt_cache_hit_tokens", 0);   /* DeepSeek */
        r->in_tokens = (long)json_num(u, "prompt_tokens", r->in_tokens) - cached;
        r->cache_read = cached;
        r->out_tokens = (long)json_num(u, "completion_tokens", r->out_tokens);
    }
    json_free(ev);
}

void openai_http_error(Reply *r)
{
    JNode *j = json_parse(r->body.p ? r->body.p : "", r->body.len);
    const JNode *e = json_get(j, "error");
    if (!e && j && j->type == J_ARRAY && j->child) e = json_get(j->child, "error");    /* Gemini */
    if (!e && j && json_str(j, "detail")) {                                            /* altri server */
        char msg[260];
        snprintf(msg, sizeof(msg), "%s", json_str(j, "detail"));
        Buf b = { 0 };
        buf_adds(&b, "{\"message\":");
        json_escape(&b, msg);
        buf_adds(&b, "}");
        JNode *d = json_parse(b.p, b.len);
        api_error(r, r->http_status, d);
        json_free(d);
        buf_free(&b);
    } else if (e && e->type == J_STRING) {
        Buf b = { 0 };
        buf_adds(&b, "{\"message\":");
        json_escape(&b, e->str);
        buf_adds(&b, "}");
        JNode *d = json_parse(b.p, b.len);
        api_error(r, r->http_status, d);
        json_free(d);
        buf_free(&b);
    } else {
        api_error(r, r->http_status, e);
    }
    json_free(j);
}
