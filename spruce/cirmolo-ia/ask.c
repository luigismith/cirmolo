/* Cirmolo IA - una domanda sola, con un'immagine facoltativa (vedi ask.h). */
#include "ask.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "i18n.h"

char *ask_request(const Provider *p, const Model *m, const AskSpec *q, char *url, size_t urln)
{
    const char *mime = q->image_mime ? q->image_mime : "image/png";
    int max = q->max_tokens > 0 ? q->max_tokens : 4096;
    Buf b = { 0 };
    buf_adds(&b, "{\"model\":");
    json_escape(&b, m->id);
    if (p->proto == PROTO_ANTHROPIC) {
        snprintf(url, urln, "%s/messages", p->base);
        buf_printf(&b, ",\"max_tokens\":%d,\"stream\":true,", max);
        if (q->effort && (m->flags & MF_EFFORT)) buf_printf(&b, "\"output_config\":{\"effort\":\"%s\"},", q->effort);
        if (q->cache) buf_adds(&b, "\"cache_control\":{\"type\":\"ephemeral\"},");
        if (q->system) { buf_adds(&b, "\"system\":"); json_escape(&b, q->system); buf_adds(&b, ","); }
        buf_adds(&b, "\"messages\":[{\"role\":\"user\",\"content\":[");
        if (q->image_b64) {
            buf_adds(&b, "{\"type\":\"image\",\"source\":{\"type\":\"base64\",\"media_type\":");
            json_escape(&b, mime);
            buf_adds(&b, ",\"data\":\"");
            buf_adds(&b, q->image_b64);
            buf_adds(&b, "\"}},");
        }
        buf_adds(&b, "{\"type\":\"text\",\"text\":");
        json_escape(&b, q->prompt);
        buf_adds(&b, "}]}]}");
    } else {
        snprintf(url, urln, "%s/chat/completions", p->base);
        buf_adds(&b, ",\"stream\":true,");
        if (p->stream_options) buf_adds(&b, "\"stream_options\":{\"include_usage\":true},");
        buf_adds(&b, "\"messages\":[");
        if (q->system) { buf_adds(&b, "{\"role\":\"system\",\"content\":"); json_escape(&b, q->system); buf_adds(&b, "},"); }
        buf_adds(&b, "{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":");
        json_escape(&b, q->prompt);
        buf_adds(&b, "}");
        if (q->image_b64) {
            buf_adds(&b, ",{\"type\":\"image_url\",\"image_url\":{\"url\":\"data:");
            buf_adds(&b, mime);
            buf_adds(&b, ";base64,");
            buf_adds(&b, q->image_b64);
            buf_adds(&b, "\"}}");
        }
        buf_adds(&b, "]}]}");
    }
    return buf_steal(&b);
}

static void reply_sink(void *ud, const char *d, size_t n) { reply_feed(ud, d, n); }

#ifndef _WIN32
#include <unistd.h>
#define NAP() usleep(20000)
#else
#define NAP() ((void)0)
#endif

char *ask_blocking(const Provider *p, const Model *m, const char *key, const AskSpec *q, int timeout,
                   char *err, size_t errn)
{
    char url[260];
    char *body = ask_request(p, m, q, url, sizeof(url));
    Buf h = { 0 };
    provider_auth_headers(p, key, &h);
    buf_adds(&h, "Content-Type: application/json\n");
    Request rq = { url, h.p, body, 0, NULL, NULL, timeout > 0 ? timeout : 120 };
    Transfer t;
    Reply r;
    reply_init(&r, p->proto == PROTO_ANTHROPIC ? PROTO_ANTHROPIC : PROTO_OPENAI, p->name);
    char *out = NULL;
    if (net_start(&t, &rq, err, errn) == 0) {
        int code = 0;
        char cerr[200];
        while (!net_poll(&t, reply_sink, &r, &code, cerr, sizeof(cerr))) NAP();
        reply_finish(&r, code, cerr);
        if (r.state == RS_DONE) out = reply_text(&r);
        else if (r.state == RS_REFUSED) snprintf(err, errn, "%s", tr("Il modello non ha risposto a questa richiesta."));
        else snprintf(err, errn, "%s", r.error[0] ? tr(r.error) : tr("Errore sconosciuto."));
    }
    if (h.p) memset(h.p, 0, h.len);
    buf_free(&h);
    free(body);
    reply_free(&r);
    return out;
}
