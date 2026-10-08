/* Chiedi a Claude - client della Messages API (vedi claude.h). */
#include "claude.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const ModelInfo CLAUDE_MODELS[MODEL_COUNT] = {
    { "claude-opus-5-5", "Claude Opus 5.5", 1, 4.0, 20.0, 5.0, 0.20 },
    { "claude-sonnet-5-5", "Claude Sonnet 5.5", 1, 2.0, 10.0, 2.5, 0.20 },
    { "claude-haiku-5-5", "Claude Haiku 5.5", 0, 0.10, 0.50, 0.125, 0.01 },
};
const char *const CLAUDE_EFFORTS[EFFORT_COUNT] = { "low", "medium", "high" };

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
    for (int i = 0; i < c->n; i++) { free(c->msg[i].json); free(c->msg[i].text); free(c->msg[i].note); }
    free(c->msg);
    free(c->system);
    memset(c, 0, sizeof(*c));
}

ChatMsg *conv_add(Conversation *c, int role, char *json, char *text)
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
    m->json = json;
    m->text = text;
    return m;
}

void conv_add_user(Conversation *c, const char *text)
{
    Buf b = { 0 };
    buf_adds(&b, "{\"role\":\"user\",\"content\":");
    json_escape(&b, text);
    buf_adds(&b, "}");
    conv_add(c, ROLE_USER, buf_steal(&b), dup_str(text));
}

const char *conv_beta(int model)
{
    return CLAUDE_MODELS[model].fallbacks ? "server-side-fallback-2026-07-01" : NULL;
}

char *conv_request(const Conversation *c, int model, int effort)
{
    const ModelInfo *m = &CLAUDE_MODELS[model];
    Buf b = { 0 };
    /* thinking non c'e': su questi modelli il pensiero adattivo e' gia' attivo; l'impegno lo regola */
    buf_printf(&b, "{\"model\":\"%s\",\"max_tokens\":64000,\"stream\":true,\"cache_control\":{\"type\":\"ephemeral\"},"
                   "\"output_config\":{\"effort\":\"%s\"},", m->id, CLAUDE_EFFORTS[effort]);
    if (m->fallbacks) buf_adds(&b, "\"fallbacks\":\"default\",");
    buf_adds(&b, "\"system\":");
    json_escape(&b, c->system);
    buf_adds(&b, ",\"messages\":[");
    int first = 1;
    for (int i = 0; i < c->n; i++) {
        if (c->msg[i].excluded) continue;
        if (!first) buf_add(&b, ",", 1);
        buf_adds(&b, c->msg[i].json);
        first = 0;
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
        buf_adds(&b, i ? ",\n" : "\n");
        buf_printf(&b, "{\"excluded\":%d,\"text\":", c->msg[i].excluded);
        json_escape(&b, c->msg[i].text);
        if (c->msg[i].note) { buf_adds(&b, ",\"note\":"); json_escape(&b, c->msg[i].note); }
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
        ChatMsg *cm = conv_add(c, !strcmp(role, "assistant") ? ROLE_ASSISTANT : ROLE_USER, buf_steal(&raw), dup_str(text ? text : ""));
        if (!text && cm->role == ROLE_USER) { free(cm->text); cm->text = dup_str(json_str(m, "content")); }
        if (mt) {
            cm->excluded = (int)json_num(mt, "excluded", 0) != 0;
            const char *note = json_str(mt, "note");
            if (note) cm->note = dup_str(note);
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

/* ------------------------------------------------------------------ risposta in arrivo */
void reply_init(Reply *r)
{
    memset(r, 0, sizeof(*r));
    r->cur = -1;
}

void reply_free(Reply *r)
{
    for (int i = 0; i < REPLY_BLOCKS; i++) { buf_free(&r->blk[i].a); buf_free(&r->blk[i].b); }
    buf_free(&r->line);
    buf_free(&r->body);
    reply_init(r);
}

static void set_error(Reply *r, const char *msg)
{
    snprintf(r->error, sizeof(r->error), "%s", msg);
    r->state = RS_ERROR;
}

static void api_error(Reply *r, const char *type, const char *message)
{
    const char *it = NULL;
    type = type ? type : "";
    if (message && strstr(message, "credit balance")) it = "Credito esaurito sull'account Anthropic.";
    else if (!strcmp(type, "authentication_error")) it = "Chiave API non valida: controlla Saves/claude/chiave.txt.";
    else if (!strcmp(type, "permission_error")) it = "Questa chiave non può usare il modello scelto.";
    else if (!strcmp(type, "not_found_error")) it = "Modello non trovato.";
    else if (!strcmp(type, "rate_limit_error")) it = "Troppe richieste: aspetta un momento e riprova.";
    else if (!strcmp(type, "overloaded_error")) it = "Claude è sovraccarico in questo momento: riprova tra poco.";
    else if (!strcmp(type, "api_error")) it = "Errore sul server di Anthropic: riprova.";
    else if (!strcmp(type, "request_too_large")) it = "Conversazione troppo lunga: iniziane una nuova.";
    if (it) set_error(r, it);
    else {
        char msg[300];
        snprintf(msg, sizeof(msg), "Richiesta rifiutata: %s", message && *message ? message : type);
        set_error(r, msg);
    }
}

static Block *block_at(Reply *r, int api_index)
{
    (void)api_index;
    return r->cur >= 0 ? &r->blk[r->cur] : NULL;
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
        if (!bt || r->nblk >= REPLY_BLOCKS) { r->cur = -1; return; }
        Block *b = &r->blk[r->nblk];
        buf_clear(&b->a);
        buf_clear(&b->b);
        buf_add(&b->a, "", 0);
        buf_add(&b->b, "", 0);
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
        } else if (!strcmp(bt, "fallback")) {
            b->type = BLK_FALLBACK;
            const char *from = json_path_str(cb, "from", "model"), *to = json_path_str(cb, "to", "model");
            buf_adds(&b->a, from ? from : "");
            buf_adds(&b->b, to ? to : "");
            r->fallback = 1;
        } else {
            b->type = BLK_OTHER;                  /* blocco sconosciuto: si conserva il JSON com'e' */
            buf_add(&b->a, src + cb->start, (size_t)(cb->end - cb->start));
        }
        r->cur = r->nblk++;
    } else if (!strcmp(type, "content_block_delta")) {
        Block *b = block_at(r, (int)json_num(ev, "index", -1));
        const JNode *d = json_get(ev, "delta");
        const char *dt = json_str(d, "type");
        if (!b || !dt) return;
        if (!strcmp(dt, "text_delta") && b->type == BLK_TEXT) { buf_adds(&b->a, json_str(d, "text") ? json_str(d, "text") : ""); r->state = RS_WRITING; }
        else if (!strcmp(dt, "thinking_delta") && b->type == BLK_THINKING) buf_adds(&b->a, json_str(d, "thinking") ? json_str(d, "thinking") : "");
        else if (!strcmp(dt, "signature_delta") && b->type == BLK_THINKING) buf_adds(&b->b, json_str(d, "signature") ? json_str(d, "signature") : "");
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

static void line(Reply *r, char *s, size_t n)
{
    if (n && s[n - 1] == '\r') s[--n] = 0;
    if (!strncmp(s, "@@CIRMOLO_HTTP ", 15)) { r->http_status = atoi(s + 15); return; }
    if (!strncmp(s, "data:", 5)) {
        const char *j = s + 5;
        while (*j == ' ') j++;
        JNode *ev = json_parse(j, n - (size_t)(j - s));
        if (ev) { handle(r, ev, j); json_free(ev); }
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
        JNode *j = json_parse(r->body.p ? r->body.p : "", r->body.len);
        const JNode *e = json_get(j, "error");
        if (e) api_error(r, json_str(e, "type"), json_str(e, "message"));
        else {
            char msg[80];
            snprintf(msg, sizeof(msg), "Risposta inattesa del server (HTTP %d).", r->http_status);
            set_error(r, msg);
        }
        json_free(j);
        return;
    }
    if (curl_exit) {
        const char *m;
        switch (curl_exit) {
        case 6: m = "Nessuna connessione: attiva il Wi-Fi."; break;
        case 7: m = "Il server di Anthropic non risponde."; break;
        case 28: m = "Tempo scaduto: la connessione è lenta o assente."; break;
        case 35: case 60: case 77: m = "Connessione sicura non riuscita: controlla data e ora della console."; break;
        case 52: case 56: case 18: m = "Connessione interrotta."; break;
        case 127: m = "curl non trovato."; break;
        default: m = NULL;
        }
        char msg[300];
        if (m) snprintf(msg, sizeof(msg), "%s", m);
        else snprintf(msg, sizeof(msg), "Errore di rete (curl %d)%s%s", curl_exit, curl_err && *curl_err ? ": " : ".", curl_err ? curl_err : "");
        if (r->state != RS_DONE && r->state != RS_REFUSED) set_error(r, msg);
        return;
    }
    if (r->state != RS_DONE && r->state != RS_REFUSED) set_error(r, "La risposta si è interrotta.");
}

static const char *short_model(const char *id)
{
    static const char *OTHERS[][2] = { { "claude-opus-5", "Claude Opus 5" }, { "claude-opus-4-8", "Claude Opus 4.8" },
                                       { "claude-sonnet-5", "Claude Sonnet 5" }, { "claude-fable-5-1", "Claude Fable 5.1" } };
    for (int i = 0; i < MODEL_COUNT; i++) if (!strcmp(id, CLAUDE_MODELS[i].id)) return CLAUDE_MODELS[i].name;
    for (size_t i = 0; i < sizeof(OTHERS) / sizeof(OTHERS[0]); i++) if (!strcmp(id, OTHERS[i][0])) return OTHERS[i][1];
    return id;
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
    return buf_steal(&b);
}

char *reply_message_json(const Reply *r)
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
        default:
            buf_add(&b, k->a.p, k->a.len);
        }
    }
    buf_adds(&b, "]}");
    if (!any_text) { buf_free(&b); return NULL; }
    return buf_steal(&b);
}

const char *reply_status(const Reply *r)
{
    switch (r->state) {
    case RS_WAITING: return "In attesa di Claude";
    case RS_THINKING: return "Claude sta pensando";
    case RS_WRITING: return "Claude scrive";
    case RS_DONE: return r->fallback ? "Risposta da un modello di riserva" : "Fatto";
    case RS_REFUSED: return "Claude non ha risposto";
    case RS_CANCELLED: return "Interrotta";
    case RS_ERROR: return "Errore";
    }
    return "";
}

const char *claude_model_name(const char *id) { return short_model(id); }

/* ------------------------------------------------------------------ trasferimento con curl */
#ifndef _WIN32
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define API_URL "https://api.anthropic.com/v1/messages"

static int write_file(char *tmpl, const char *data, size_t n)
{
    int fd = mkstemp(tmpl);
    if (fd < 0) return -1;
    fchmod(fd, 0600);
    size_t done = 0;
    while (done < n) {
        ssize_t w = write(fd, data + done, n - done);
        if (w <= 0) { close(fd); unlink(tmpl); return -1; }
        done += (size_t)w;
    }
    close(fd);
    return 0;
}

static void cleanup(Transfer *t)
{
    if (t->out >= 0) close(t->out);
    if (t->err >= 0) close(t->err);
    t->out = t->err = -1;
    if (t->hdr[0]) unlink(t->hdr);
    if (t->body[0]) unlink(t->body);
    t->hdr[0] = t->body[0] = 0;
    buf_free(&t->errbuf);
}

int transfer_start(Transfer *t, const char *key, const char *beta, const char *body, char *msg, size_t msgn)
{
    memset(t, 0, sizeof(*t));
    t->pid = -1;
    t->out = t->err = -1;
    Buf h = { 0 };
    buf_printf(&h, "Content-Type: application/json\nx-api-key: %s\nanthropic-version: 2023-06-01\n", key);
    if (beta) buf_printf(&h, "anthropic-beta: %s\n", beta);
    snprintf(t->hdr, sizeof(t->hdr), "/tmp/claude-h-XXXXXX");
    snprintf(t->body, sizeof(t->body), "/tmp/claude-b-XXXXXX");
    int bad = write_file(t->hdr, h.p, h.len);
    buf_free(&h);                                 /* la chiave resta solo nel file temporaneo, in RAM */
    if (bad || write_file(t->body, body, strlen(body))) {
        snprintf(msg, msgn, "Impossibile preparare la richiesta in /tmp.");
        cleanup(t);
        return -1;
    }
    const char *curl = access("/mnt/SDCARD/spruce/bin64/curl", X_OK) == 0 ? "/mnt/SDCARD/spruce/bin64/curl" : "curl";
    const char *ca = access("/mnt/SDCARD/spruce/etc/ca-certificates.crt", R_OK) == 0 ? "/mnt/SDCARD/spruce/etc/ca-certificates.crt" : NULL;
    char hdrarg[80], bodyarg[80];
    snprintf(hdrarg, sizeof(hdrarg), "@%s", t->hdr);
    snprintf(bodyarg, sizeof(bodyarg), "@%s", t->body);
    const char *argv[24];
    int k = 0;
    argv[k++] = curl;
    argv[k++] = "-sS";
    argv[k++] = "-N";
    argv[k++] = "--connect-timeout"; argv[k++] = "20";
    argv[k++] = "--max-time"; argv[k++] = "900";
    argv[k++] = "-X"; argv[k++] = "POST";
    argv[k++] = "-H"; argv[k++] = hdrarg;
    argv[k++] = "--data-binary"; argv[k++] = bodyarg;
    argv[k++] = "-w"; argv[k++] = "\n@@CIRMOLO_HTTP %{http_code}\n";
    if (ca) { argv[k++] = "--cacert"; argv[k++] = ca; }
    argv[k++] = API_URL;
    argv[k] = NULL;
    int po[2], pe[2];
    if (pipe(po)) { snprintf(msg, msgn, "pipe: %s", strerror(errno)); cleanup(t); return -1; }
    if (pipe(pe)) { close(po[0]); close(po[1]); snprintf(msg, msgn, "pipe: %s", strerror(errno)); cleanup(t); return -1; }
    pid_t pid = fork();
    if (pid < 0) {
        close(po[0]); close(po[1]); close(pe[0]); close(pe[1]);
        snprintf(msg, msgn, "fork: %s", strerror(errno));
        cleanup(t);
        return -1;
    }
    if (pid == 0) {
        dup2(po[1], 1);
        dup2(pe[1], 2);
        for (int fd = 3; fd < 1024; fd++) close(fd);   /* niente audio, video o SD aperti nel figlio */
        execvp(curl, (char *const *)argv);
        _exit(127);
    }
    close(po[1]);
    close(pe[1]);
    fcntl(po[0], F_SETFL, fcntl(po[0], F_GETFL) | O_NONBLOCK);
    fcntl(pe[0], F_SETFL, fcntl(pe[0], F_GETFL) | O_NONBLOCK);
    t->pid = pid;
    t->out = po[0];
    t->err = pe[0];
    return 0;
}

int transfer_poll(Transfer *t, Reply *r)
{
    if (t->pid < 0) return 1;
    char buf[8192];
    int eof = 0;
    for (;;) {
        ssize_t n = read(t->out, buf, sizeof(buf));
        if (n > 0) { reply_feed(r, buf, (size_t)n); continue; }
        if (n == 0) eof = 1;
        else if (errno == EINTR) continue;
        break;
    }
    for (;;) {
        ssize_t n = read(t->err, buf, sizeof(buf));
        if (n > 0) { if (t->errbuf.len < 1024) buf_add(&t->errbuf, buf, (size_t)n); continue; }
        if (n < 0 && errno == EINTR) continue;
        break;
    }
    if (!eof) return 0;
    int status = 0;
    waitpid(t->pid, &status, 0);
    t->pid = -1;
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    char err[300] = "";
    if (t->errbuf.len) {
        snprintf(err, sizeof(err), "%s", t->errbuf.p);
        err[strcspn(err, "\r\n")] = 0;
        if (!strncmp(err, "curl: ", 6)) memmove(err, err + 6, strlen(err + 6) + 1);
    }
    reply_finish(r, code, err);
    cleanup(t);
    return 1;
}

void transfer_cancel(Transfer *t)
{
    if (t->pid > 0) {
        kill(t->pid, SIGTERM);
        int status;
        for (int i = 0; i < 50 && waitpid(t->pid, &status, WNOHANG) == 0; i++) usleep(10000);
        if (waitpid(t->pid, &status, WNOHANG) == 0) { kill(t->pid, SIGKILL); waitpid(t->pid, &status, 0); }
    }
    t->pid = -1;
    cleanup(t);
}

#else  /* sul PC il client non si collega: le prove usano risposte registrate */

int transfer_start(Transfer *t, const char *key, const char *beta, const char *body, char *msg, size_t msgn)
{
    (void)key; (void)beta; (void)body;
    memset(t, 0, sizeof(*t));
    t->pid = -1;
    snprintf(msg, msgn, "Sul PC questa app non si collega a Claude.");
    return -1;
}
int transfer_poll(Transfer *t, Reply *r) { (void)t; (void)r; return 1; }
void transfer_cancel(Transfer *t) { t->pid = -1; }

#endif
