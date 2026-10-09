/* Chiedi all'IA - risposte della Messages API di Anthropic (stream SSE: message_start, content_block_*,
 * message_delta, message_stop, error), con rifiuti (stop_reason "refusal") e ripieghi su un altro modello. */
#include "llm.h"

#include "i18n.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void api_error(Reply *r, const char *type, const char *message)
{
    const char *it = NULL;
    type = type ? type : "";
    if (message && strstr(message, "credit balance")) it = N_("Credito esaurito sull'account Anthropic.");
    else if (!strcmp(type, "authentication_error")) it = N_("Chiave API di Anthropic non valida: controllala nelle impostazioni.");
    else if (!strcmp(type, "permission_error")) it = N_("Questa chiave non può usare il modello scelto.");
    else if (!strcmp(type, "not_found_error")) it = N_("Modello non trovato.");
    else if (!strcmp(type, "rate_limit_error")) it = N_("Troppe richieste: aspetta un momento e riprova.");
    else if (!strcmp(type, "overloaded_error")) it = N_("Claude è sovraccarico in questo momento: riprova tra poco.");
    else if (!strcmp(type, "api_error")) it = N_("Errore sul server di Anthropic: riprova.");
    else if (!strcmp(type, "request_too_large")) it = N_("Conversazione troppo lunga: iniziane una nuova.");
    if (it) reply_set_error(r, it);
    else {
        char msg[300];
        snprintf(msg, sizeof(msg), tr("Richiesta rifiutata: %s"), message && *message ? message : type);
        reply_set_error(r, msg);
    }
}

static void handle(Reply *r, const JNode *ev, const char *src)
{
    const char *type = json_str(ev, "type");
    if (!type) return;
    if (!strcmp(type, "message_start")) {
        const JNode *m = json_get(ev, "message"), *u = json_get(m, "usage");
        const char *model = json_str(m, "model");
        if (model) snprintf(r->model, sizeof(r->model), "%s", model);
        r->in_tokens = (long)json_num(u, "input_tokens", 0);
        r->cache_write = (long)json_num(u, "cache_creation_input_tokens", 0);
        r->cache_read = (long)json_num(u, "cache_read_input_tokens", 0);
        r->out_tokens = (long)json_num(u, "output_tokens", 0);
        if (r->state == RS_IDLE) r->state = RS_WAITING;
    } else if (!strcmp(type, "content_block_start")) {
        const JNode *cb = json_get(ev, "content_block");
        const char *bt = json_str(cb, "type");
        if (!bt) { r->cur = -1; return; }
        Block *b = reply_new_block(r, BLK_OTHER);
        if (!b) return;
        if (!strcmp(bt, "text")) {
            b->type = BLK_TEXT;
            buf_adds(&b->a, json_str(cb, "text") ? json_str(cb, "text") : "");
            r->state = RS_WRITING;
        } else if (!strcmp(bt, "thinking")) {
            b->type = BLK_THINKING;
            buf_adds(&b->a, json_str(cb, "thinking") ? json_str(cb, "thinking") : "");
            buf_adds(&b->b, json_str(cb, "signature") ? json_str(cb, "signature") : "");
            r->state = RS_THINKING;
        } else if (!strcmp(bt, "redacted_thinking")) {
            b->type = BLK_REDACTED;
            buf_adds(&b->a, json_str(cb, "data") ? json_str(cb, "data") : "");
            r->state = RS_THINKING;
        } else if (!strcmp(bt, "tool_use")) {
            b->type = BLK_TOOL;                   /* gli argomenti arrivano a pezzi (input_json_delta) */
            buf_adds(&b->b, json_str(cb, "id") ? json_str(cb, "id") : "");
            snprintf(b->name, sizeof(b->name), "%s", json_str(cb, "name") ? json_str(cb, "name") : "");
        } else if (!strcmp(bt, "fallback")) {
            b->type = BLK_FALLBACK;
            const char *from = json_path_str(cb, "from", "model"), *to = json_path_str(cb, "to", "model");
            buf_adds(&b->a, from ? from : "");
            buf_adds(&b->b, to ? to : "");
            r->fallback = 1;
        } else {
            buf_add(&b->a, src + cb->start, (size_t)(cb->end - cb->start));   /* blocco sconosciuto: si conserva com'e' */
        }
    } else if (!strcmp(type, "content_block_delta")) {
        Block *b = r->cur >= 0 ? &r->blk[r->cur] : NULL;
        const JNode *d = json_get(ev, "delta");
        const char *dt = json_str(d, "type");
        if (!b || !dt) return;
        if (!strcmp(dt, "text_delta") && b->type == BLK_TEXT) { buf_adds(&b->a, json_str(d, "text") ? json_str(d, "text") : ""); r->state = RS_WRITING; }
        else if (!strcmp(dt, "thinking_delta") && b->type == BLK_THINKING) buf_adds(&b->a, json_str(d, "thinking") ? json_str(d, "thinking") : "");
        else if (!strcmp(dt, "signature_delta") && b->type == BLK_THINKING) buf_adds(&b->b, json_str(d, "signature") ? json_str(d, "signature") : "");
        else if (!strcmp(dt, "input_json_delta") && b->type == BLK_TOOL) buf_adds(&b->a, json_str(d, "partial_json") ? json_str(d, "partial_json") : "");
    } else if (!strcmp(type, "content_block_stop")) {
        r->cur = -1;
    } else if (!strcmp(type, "message_delta")) {
        const JNode *d = json_get(ev, "delta"), *u = json_get(ev, "usage");
        const char *sr = json_str(d, "stop_reason");
        if (sr) snprintf(r->stop_reason, sizeof(r->stop_reason), "%s", sr);
        const char *cat = json_path_str(d, "stop_details", "category");
        if (cat) snprintf(r->category, sizeof(r->category), "%s", cat);
        if (u) {
            r->out_tokens = (long)json_num(u, "output_tokens", r->out_tokens);
            r->in_tokens = (long)json_num(u, "input_tokens", r->in_tokens);
            r->cache_write = (long)json_num(u, "cache_creation_input_tokens", r->cache_write);
            r->cache_read = (long)json_num(u, "cache_read_input_tokens", r->cache_read);
        }
    } else if (!strcmp(type, "message_stop")) {
        r->state = !strcmp(r->stop_reason, "refusal") ? RS_REFUSED : RS_DONE;
    } else if (!strcmp(type, "error")) {
        const JNode *e = json_get(ev, "error");
        api_error(r, json_str(e, "type"), json_str(e, "message"));
    }
}

void anthropic_line(Reply *r, const char *data, size_t n)
{
    JNode *ev = json_parse(data, n);
    if (ev) { handle(r, ev, data); json_free(ev); }
}

void anthropic_http_error(Reply *r)
{
    JNode *j = json_parse(r->body.p ? r->body.p : "", r->body.len);
    const JNode *e = json_get(j, "error");
    if (e) api_error(r, json_str(e, "type"), json_str(e, "message"));
    else {
        char msg[80];
        snprintf(msg, sizeof(msg), tr("Risposta inattesa del server (HTTP %d)."), r->http_status);
        reply_set_error(r, msg);
    }
    json_free(j);
}

char *anthropic_message_json(const Reply *r)
{
    int last_fb = -1, any_text = 0;
    for (int i = 0; i < r->nblk; i++) if (r->blk[i].type == BLK_FALLBACK) last_fb = i;
    Buf b = { 0 };
    buf_adds(&b, "{\"role\":\"assistant\",\"content\":[");
    int first = 1;
    for (int i = 0; i < r->nblk; i++) {
        const Block *k = &r->blk[i];
        /* prima dell'ultimo ripiego si rimandano solo i testi: i pensieri del modello che ha rifiutato no */
        if (i < last_fb && k->type != BLK_TEXT) continue;
        if (k->type == BLK_FALLBACK) continue;
        if (k->type == BLK_TEXT && !k->a.len) continue;          /* l'API non accetta testi vuoti */
        if (k->type == BLK_THINKING && !k->b.len) continue;      /* pensiero senza firma: incompleto */
        if (!first) buf_add(&b, ",", 1);
        first = 0;
        switch (k->type) {
        case BLK_TEXT:
            buf_adds(&b, "{\"type\":\"text\",\"text\":");
            json_escape_n(&b, k->a.p, k->a.len);
            buf_adds(&b, "}");
            any_text = 1;
            break;
        case BLK_THINKING:
            buf_adds(&b, "{\"type\":\"thinking\",\"thinking\":");
            json_escape_n(&b, k->a.p ? k->a.p : "", k->a.len);
            buf_adds(&b, ",\"signature\":");
            json_escape_n(&b, k->b.p, k->b.len);
            buf_adds(&b, "}");
            break;
        case BLK_REDACTED:
            buf_adds(&b, "{\"type\":\"redacted_thinking\",\"data\":");
            json_escape_n(&b, k->a.p ? k->a.p : "", k->a.len);
            buf_adds(&b, "}");
            break;
        case BLK_TOOL:
            buf_adds(&b, "{\"type\":\"tool_use\",\"id\":");
            json_escape_n(&b, k->b.p ? k->b.p : "", k->b.len);
            buf_adds(&b, ",\"name\":");
            json_escape(&b, k->name);
            buf_adds(&b, ",\"input\":");
            buf_adds(&b, k->a.len ? k->a.p : "{}");
            buf_adds(&b, "}");
            any_text = 1;                         /* un messaggio con soli strumenti va rimandato lo stesso */
            break;
        default:
            buf_add(&b, k->a.p, k->a.len);
        }
    }
    buf_adds(&b, "]}");
    if (!any_text) { buf_free(&b); return NULL; }
    return buf_steal(&b);
}
