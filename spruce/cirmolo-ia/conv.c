/* Chiedi all'IA - conversazione, richieste e parte comune delle risposte (vedi llm.h). */
#include "llm.h"

#include "i18n.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_str(const char *s)
{
    size_t n = strlen(s ? s : "");
    char *p = malloc(n + 1);
    if (!p) abort();
    memcpy(p, s ? s : "", n + 1);
    return p;
}

/* ------------------------------------------------------------------ conversazione */
void conv_init(Conversation *c, const char *system)
{
    memset(c, 0, sizeof(*c));
    c->system = dup_str(system);
}

void conv_free(Conversation *c)
{
    for (int i = 0; i < c->n; i++) { free(c->msg[i].json); free(c->msg[i].text); free(c->msg[i].note); free(c->msg[i].model); }
    free(c->msg);
    free(c->system);
    memset(c, 0, sizeof(*c));
}

ChatMsg *conv_add(Conversation *c, int role, int proto, char *json, char *text)
{
    if (c->n == c->cap) {
        c->cap = c->cap ? c->cap * 2 : 16;
        ChatMsg *m = realloc(c->msg, sizeof(ChatMsg) * (size_t)c->cap);
        if (!m) abort();
        c->msg = m;
    }
    ChatMsg *m = &c->msg[c->n++];
    memset(m, 0, sizeof(*m));
    m->role = role;
    m->proto = proto;
    m->json = json;
    m->text = text;
    return m;
}

static char *plain_json(int role, const char *text)
{
    Buf b = { 0 };
    buf_adds(&b, role == ROLE_USER ? "{\"role\":\"user\",\"content\":" : "{\"role\":\"assistant\",\"content\":");
    json_escape(&b, text ? text : "");
    buf_adds(&b, "}");
    return buf_steal(&b);
}

void conv_add_user(Conversation *c, const char *text)
{
    conv_add(c, ROLE_USER, PROTO_PLAIN, plain_json(ROLE_USER, text), dup_str(text));
}

char *anthropic_request(const Conversation *c, const char *model, const char *effort, int fallbacks)
{
    Buf b = { 0 };
    /* thinking non c'e': sui modelli Claude recenti il pensiero adattivo e' gia' attivo; l'impegno lo regola */
    buf_adds(&b, "{\"model\":");
    json_escape(&b, model);
    buf_adds(&b, ",\"max_tokens\":64000,\"stream\":true,\"cache_control\":{\"type\":\"ephemeral\"},");
    if (effort) buf_printf(&b, "\"output_config\":{\"effort\":\"%s\"},", effort);
    if (fallbacks) buf_adds(&b, "\"fallbacks\":\"default\",");
    buf_adds(&b, "\"system\":");
    json_escape(&b, c->system);
    buf_adds(&b, ",\"messages\":[");
    int first = 1;
    for (int i = 0; i < c->n; i++) {
        const ChatMsg *m = &c->msg[i];
        if (m->excluded) continue;
        if (!first) buf_add(&b, ",", 1);
        first = 0;
        if (m->proto == PROTO_ANTHROPIC || m->role == ROLE_USER) buf_adds(&b, m->json);
        else { char *p = plain_json(m->role, m->text); buf_adds(&b, p); free(p); }   /* risposta di un altro fornitore */
    }
    buf_adds(&b, "]}");
    return buf_steal(&b);
}

char *openai_request(const Conversation *c, const char *model, int max_tokens, int stream_options)
{
    Buf b = { 0 };
    buf_adds(&b, "{\"model\":");
    json_escape(&b, model);
    buf_adds(&b, ",\"stream\":true,");
    if (stream_options) buf_adds(&b, "\"stream_options\":{\"include_usage\":true},");
    if (max_tokens > 0) buf_printf(&b, "\"max_tokens\":%d,", max_tokens);
    buf_adds(&b, "\"messages\":[{\"role\":\"system\",\"content\":");
    json_escape(&b, c->system);
    buf_adds(&b, "}");
    for (int i = 0; i < c->n; i++) {
        const ChatMsg *m = &c->msg[i];
        if (m->excluded) continue;
        char *p = plain_json(m->role, m->text);
        buf_adds(&b, ",");
        buf_adds(&b, p);
        free(p);
    }
    buf_adds(&b, "]}");
    return buf_steal(&b);
}

char *conv_save(const Conversation *c)
{
    Buf b = { 0 };
    buf_adds(&b, "{\"system\":");
    json_escape(&b, c->system);
    buf_adds(&b, ",\n\"messages\":[");
    for (int i = 0; i < c->n; i++) {
        buf_adds(&b, i ? ",\n" : "\n");
        buf_adds(&b, c->msg[i].json);
    }
    buf_adds(&b, "],\n\"meta\":[");
    for (int i = 0; i < c->n; i++) {
        const ChatMsg *m = &c->msg[i];
        buf_adds(&b, i ? ",\n" : "\n");
        buf_printf(&b, "{\"excluded\":%d,\"proto\":%d,\"text\":", m->excluded, m->proto);
        json_escape(&b, m->text);
        if (m->note) { buf_adds(&b, ",\"note\":"); json_escape(&b, m->note); }
        if (m->model) { buf_adds(&b, ",\"model\":"); json_escape(&b, m->model); }
        buf_adds(&b, "}");
    }
    buf_printf(&b, "],\n\"usage\":{\"in\":%ld,\"out\":%ld,\"cache_read\":%ld,\"cache_write\":%ld,\"cost\":%.6f}}\n",
               c->in_tokens, c->out_tokens, c->cache_read, c->cache_write, c->cost);
    return buf_steal(&b);
}

int conv_load(Conversation *c, const char *json, size_t len)
{
    JNode *root = json_parse(json, len);
    if (!root) return -1;
    const char *system = json_str(root, "system");
    const JNode *msgs = json_get(root, "messages"), *meta = json_get(root, "meta");
    if (!system || !msgs || msgs->type != J_ARRAY) { json_free(root); return -1; }
    conv_free(c);
    conv_init(c, system);
    const JNode *mt = meta && meta->type == J_ARRAY ? meta->child : NULL;
    for (const JNode *m = msgs->child; m; m = m->next) {
        const char *role = json_str(m, "role");
        if (!role) continue;
        Buf raw = { 0 };
        buf_add(&raw, json + m->start, (size_t)(m->end - m->start));
        const char *text = mt ? json_str(mt, "text") : NULL;
        int r = !strcmp(role, "assistant") ? ROLE_ASSISTANT : ROLE_USER;
        /* senza "proto" (salvataggi della 0.2) i messaggi dell'assistente sono di Claude */
        int proto = (int)json_num(mt, "proto", r == ROLE_ASSISTANT ? PROTO_ANTHROPIC : PROTO_PLAIN);
        ChatMsg *cm = conv_add(c, r, proto, buf_steal(&raw), dup_str(text ? text : ""));
        if (!text && cm->role == ROLE_USER) { free(cm->text); cm->text = dup_str(json_str(m, "content")); }
        if (mt) {
            cm->excluded = (int)json_num(mt, "excluded", 0) != 0;
            const char *note = json_str(mt, "note"), *model = json_str(mt, "model");
            if (note) cm->note = dup_str(note);
            if (model) cm->model = dup_str(model);
            mt = mt->next;
        }
    }
    const JNode *u = json_get(root, "usage");
    c->in_tokens = (long)json_num(u, "in", 0);
    c->out_tokens = (long)json_num(u, "out", 0);
    c->cache_read = (long)json_num(u, "cache_read", 0);
    c->cache_write = (long)json_num(u, "cache_write", 0);
    c->cost = json_num(u, "cost", 0);
    json_free(root);
    return 0;
}

/* ------------------------------------------------------------------ risposta: parte comune */
void reply_init(Reply *r, int proto, const char *who)
{
    memset(r, 0, sizeof(*r));
    r->cur = -1;
    r->proto = proto;
    snprintf(r->who, sizeof(r->who), "%s", who ? who : "");
}

void reply_free(Reply *r)
{
    int proto = r->proto;
    char who[sizeof(r->who)];
    memcpy(who, r->who, sizeof(who));
    for (int i = 0; i < REPLY_BLOCKS; i++) { buf_free(&r->blk[i].a); buf_free(&r->blk[i].b); }
    buf_free(&r->line);
    buf_free(&r->body);
    reply_init(r, proto, who);
}

void reply_reset(Reply *r) { reply_free(r); }

void reply_set_error(Reply *r, const char *msg)
{
    snprintf(r->error, sizeof(r->error), "%s", msg);
    r->state = RS_ERROR;
}

Block *reply_new_block(Reply *r, int type)
{
    if (r->nblk >= REPLY_BLOCKS) { r->cur = -1; return NULL; }
    Block *b = &r->blk[r->nblk];
    buf_clear(&b->a);
    buf_clear(&b->b);
    buf_add(&b->a, "", 0);
    buf_add(&b->b, "", 0);
    b->type = type;
    r->cur = r->nblk++;
    return b;
}

static void line(Reply *r, char *s, size_t n)
{
    if (n && s[n - 1] == '\r') s[--n] = 0;
    int st = net_http_line(s);
    if (st) { r->http_status = st; return; }
    if (!strncmp(s, "data:", 5)) {
        const char *j = s + 5;
        while (*j == ' ') j++;
        size_t jn = n - (size_t)(j - s);
        if (r->proto == PROTO_OPENAI) openai_line(r, j, jn);
        else anthropic_line(r, j, jn);
        return;
    }
    if (!strncmp(s, "event:", 6) || s[0] == ':' || !n) return;
    if (r->body.len < 16384) { buf_add(&r->body, s, n); buf_add(&r->body, "\n", 1); }   /* corpo di un errore HTTP */
}

void reply_feed(Reply *r, const char *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (data[i] != '\n') continue;
        buf_add(&r->line, data, i);
        line(r, r->line.p ? r->line.p : (char *)"", r->line.len);
        buf_clear(&r->line);
        data += i + 1;
        len -= i + 1;
        i = (size_t)-1;
    }
    if (len) buf_add(&r->line, data, len);
}

void reply_finish(Reply *r, int curl_exit, const char *curl_err)
{
    if (r->line.len) { buf_add(&r->line, "", 0); line(r, r->line.p, r->line.len); buf_clear(&r->line); }
    if (r->state == RS_CANCELLED || r->state == RS_ERROR) return;
    if (r->http_status && r->http_status != 200) {
        if (r->proto == PROTO_OPENAI) openai_http_error(r);
        else anthropic_http_error(r);
        return;
    }
    if (curl_exit) {
        const char *m = net_curl_message(curl_exit);
        char msg[300];
        if (m) snprintf(msg, sizeof(msg), "%s", m);
        else snprintf(msg, sizeof(msg), tr("Errore di rete (curl %d)%s%s"), curl_exit, curl_err && *curl_err ? ": " : ".", curl_err ? curl_err : "");
        if (r->state != RS_DONE && r->state != RS_REFUSED) reply_set_error(r, msg);
        return;
    }
    if (r->state != RS_DONE && r->state != RS_REFUSED) reply_set_error(r, N_("La risposta si è interrotta."));
}

char *reply_text(const Reply *r)
{
    Buf b = { 0 };
    buf_add(&b, "", 0);
    for (int i = 0; i < r->nblk; i++) {
        const Block *k = &r->blk[i];
        if (k->type == BLK_TEXT && k->a.len) buf_add(&b, k->a.p, k->a.len);
        else if (k->type == BLK_FALLBACK && b.len) buf_adds(&b, "\n\n");
    }
    /* alcuni modelli (DeepSeek, Qwen, MiniMax) mettono il ragionamento tra <think> e </think> nel testo */
    char *t = buf_steal(&b);
    char *open = strstr(t, "<think>");
    if (open) {
        char *close = strstr(open, "</think>");
        if (close) memmove(open, close + 8, strlen(close + 8) + 1);
        else *open = 0;                           /* sta ancora ragionando */
        char *s = open;
        while (*s == '\n' || *s == ' ') s++;
        if (s != open) memmove(open, s, strlen(s) + 1);
    }
    return t;
}

char *reply_message_json(const Reply *r, int *proto)
{
    if (r->proto == PROTO_ANTHROPIC) {
        if (proto) *proto = PROTO_ANTHROPIC;
        return anthropic_message_json(r);
    }
    if (proto) *proto = PROTO_PLAIN;
    char *t = reply_text(r);
    if (!t[0]) { free(t); return NULL; }
    char *j = plain_json(ROLE_ASSISTANT, t);
    free(t);
    return j;
}

const char *reply_status(const Reply *r)
{
    switch (r->state) {
    case RS_WAITING: return N_("In attesa della risposta");
    case RS_THINKING: return N_("Sta pensando");
    case RS_WRITING: return N_("Scrive");
    case RS_DONE: return r->fallback ? "Risposta da un modello di riserva" : "Fatto";
    case RS_REFUSED: return N_("Nessuna risposta");
    case RS_CANCELLED: return N_("Interrotta");
    case RS_ERROR: return N_("Errore");
    }
    return N_("");
}
