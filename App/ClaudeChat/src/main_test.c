/* Chiedi all'IA - prove senza rete: JSON, stream SSE di Anthropic (risposta normale, rifiuto, ripiego,
 * errori) e compatibili OpenAI (ragionamento, <think>, uso, errori), cronologia mista rimandata identica,
 * fornitori e chiavi, voce (base64, WAV, ricampionamento, testo da leggere, trascrizioni), traduzioni,
 * tastiera a schermo, voce nell'interfaccia, schermate (BMP).
 *
 * Uso: chat-test <cartella dei font> <cartella di uscita>
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chat_app.h"
#include "i18n.h"
#include "llm.h"
#include "providers.h"
#include "voice.h"
#include "gfx.h"
#include "json.h"
#include "platform.h"

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

static int checks, fails;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("ERRORE riga %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void write_bmp(const char *path, const Canvas *c)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    int row = (c->w * 3 + 3) & ~3, size = 54 + row * c->h;
    unsigned char hdr[54] = { 'B', 'M' };
    hdr[2] = size; hdr[3] = size >> 8; hdr[4] = size >> 16; hdr[5] = size >> 24;
    hdr[10] = 54; hdr[14] = 40; hdr[18] = c->w; hdr[19] = c->w >> 8; hdr[22] = c->h; hdr[23] = c->h >> 8;
    hdr[26] = 1; hdr[28] = 24;
    fwrite(hdr, 1, 54, f);
    unsigned char *line = calloc(1, (size_t)row);
    for (int y = c->h - 1; y >= 0; y--) {
        for (int x = 0; x < c->w; x++) {
            uint32_t p = c->px[y * c->w + x];
            line[x * 3] = p & 255; line[x * 3 + 1] = (p >> 8) & 255; line[x * 3 + 2] = (p >> 16) & 255;
        }
        fwrite(line, 1, (size_t)row, f);
    }
    free(line);
    fclose(f);
}

/* ------------------------------------------------------------------ stream registrati */
#define START(model, in) \
    "event: message_start\n" \
    "data: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_01\",\"type\":\"message\",\"role\":\"assistant\",\"model\":\"" model "\"," \
    "\"content\":[],\"stop_reason\":null,\"usage\":{\"input_tokens\":" in ",\"cache_creation_input_tokens\":0,\"cache_read_input_tokens\":0,\"output_tokens\":3}}}\n\n"
#define THINK(i, sig) \
    "event: content_block_start\ndata: {\"type\":\"content_block_start\",\"index\":" i ",\"content_block\":{\"type\":\"thinking\",\"thinking\":\"\",\"signature\":\"\"}}\n\n" \
    "event: content_block_delta\ndata: {\"type\":\"content_block_delta\",\"index\":" i ",\"delta\":{\"type\":\"signature_delta\",\"signature\":\"" sig "\"}}\n\n" \
    "event: content_block_stop\ndata: {\"type\":\"content_block_stop\",\"index\":" i "}\n\n"
#define TEXT_START(i) "event: content_block_start\ndata: {\"type\":\"content_block_start\",\"index\":" i ",\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n\n"
#define TEXT_DELTA(i, t) "event: content_block_delta\ndata: {\"type\":\"content_block_delta\",\"index\":" i ",\"delta\":{\"type\":\"text_delta\",\"text\":\"" t "\"}}\n\n"
#define STOP(i) "event: content_block_stop\ndata: {\"type\":\"content_block_stop\",\"index\":" i "}\n\n"
#define END(reason, out) \
    "event: ping\ndata: {\"type\":\"ping\"}\n\n" \
    "event: message_delta\ndata: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"" reason "\",\"stop_sequence\":null},\"usage\":{\"output_tokens\":" out "}}\n\n" \
    "event: message_stop\ndata: {\"type\":\"message_stop\"}\n\n" \
    "\n@@CIRMOLO_HTTP 200\n"

static const char *NORMAL =
    START("claude-opus-5-5", "420") THINK("0", "EqQBCkYIBhgCKkBz/+firma==")
    TEXT_START("1") TEXT_DELTA("1", "Roma è la capitale d\\u2019Italia.") TEXT_DELTA("1", "\\n\\n**Qualche dato:**\\n- abitanti: circa 2,7 milioni\\n- fiume: il Tevere")
    STOP("1") END("end_turn", "57");

static const char *REFUSAL =
    START("claude-opus-5-5", "88")
    "event: message_delta\ndata: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"refusal\",\"stop_details\":{\"type\":\"refusal\",\"category\":\"cyber\",\"explanation\":null}},\"usage\":{\"output_tokens\":0}}\n\n"
    "event: message_stop\ndata: {\"type\":\"message_stop\"}\n\n\n@@CIRMOLO_HTTP 200\n";

static const char *FALLBACK =
    START("claude-opus-5-5", "300") THINK("0", "SIG-OPUS-55")
    TEXT_START("1") TEXT_DELTA("1", "Prima parte.") STOP("1")
    "event: content_block_start\ndata: {\"type\":\"content_block_start\",\"index\":2,\"content_block\":{\"type\":\"fallback\",\"from\":{\"model\":\"claude-opus-5-5\"},\"to\":{\"model\":\"claude-opus-5\"}}}\n\n"
    STOP("2") THINK("3", "SIG-OPUS-5")
    TEXT_START("4") TEXT_DELTA("4", "Seconda parte.") STOP("4") END("end_turn", "40");

static const char *HTTP401 =
    "{\"type\":\"error\",\"error\":{\"type\":\"authentication_error\",\"message\":\"invalid x-api-key\"},\"request_id\":\"req_011\"}\n"
    "@@CIRMOLO_HTTP 401\n";

static const char *OVERLOADED =
    START("claude-opus-5-5", "100") TEXT_START("0") TEXT_DELTA("0", "Inizio")
    "event: error\ndata: {\"type\":\"error\",\"error\":{\"type\":\"overloaded_error\",\"message\":\"Overloaded\"}}\n\n"
    "\n@@CIRMOLO_HTTP 200\n";


/* ------------------------------------------------------------------ JSON */
static void test_json(void)
{
    const char *src = "{\"a\":\"\\u00e8 \\\"x\\\" \\ud83d\\ude00\",\"n\":-1.5e2,\"l\":[1,true,null,{\"b\":\"c\"}],\"e\":{}}";
    JNode *j = json_parse(src, strlen(src));
    CHECK(j != NULL, "JSON valido non letto");
    CHECK(j && !strcmp(json_str(j, "a"), "\xc3\xa8 \"x\" \xf0\x9f\x98\x80"), "stringa decodificata: %s", j ? json_str(j, "a") : "-");
    CHECK(j && json_num(j, "n", 0) == -150.0, "numero");
    const JNode *l = json_get(j, "l");
    CHECK(l && l->type == J_ARRAY && l->child && l->child->next->type == J_TRUE, "array");
    CHECK(l && !strncmp(src + l->start, "[1,true", 7) && src[l->end - 1] == ']', "posizione nel sorgente");
    json_free(j);
    const char *bad[] = { "{\"a\":}", "[1,2", "{\"a\" 1}", "\"\\x\"", "tru", "{} x" };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        JNode *b = json_parse(bad[i], strlen(bad[i]));
        CHECK(!b, "JSON non valido accettato: %s", bad[i]);
        json_free(b);
    }
    Buf b = { 0 };
    json_escape(&b, "riga\n\"virgolette\" \\ tab\t \x01 è");
    CHECK(!strcmp(b.p, "\"riga\\n\\\"virgolette\\\" \\\\ tab\\t \\u0001 è\""), "escape: %s", b.p);
    JNode *r = json_parse(b.p, b.len);
    CHECK(r && r->type == J_STRING && !strcmp(r->str, "riga\n\"virgolette\" \\ tab\t \x01 è"), "andata e ritorno");
    json_free(r);
    buf_free(&b);
}

/* ------------------------------------------------------------------ stream */
static void feed_chunks(Reply *r, const char *s, unsigned seed)
{
    size_t n = strlen(s), i = 0;
    while (i < n) {
        seed = seed * 1103515245u + 12345u;
        size_t k = 1 + (seed >> 16) % 37;
        if (k > n - i) k = n - i;
        reply_feed(r, s + i, k);
        i += k;
    }
}

static void test_streams(void)
{
    for (unsigned seed = 1; seed <= 20; seed++) {   /* a pezzi di lunghezza qualsiasi */
        Reply r;
        reply_init(&r, PROTO_ANTHROPIC, "Claude");
        feed_chunks(&r, NORMAL, seed);
        reply_finish(&r, 0, "");
        char *text = reply_text(&r), *json = reply_message_json(&r, NULL);
        CHECK(r.state == RS_DONE, "stato %d", r.state);
        CHECK(!strcmp(text, "Roma è la capitale d\xe2\x80\x99Italia.\n\n**Qualche dato:**\n- abitanti: circa 2,7 milioni\n- fiume: il Tevere"), "testo: %s", text);
        CHECK(json && strstr(json, "{\"type\":\"thinking\",\"thinking\":\"\",\"signature\":\"EqQBCkYIBhgCKkBz/+firma==\"}") != NULL, "pensiero rimandato: %s", json ? json : "-");
        CHECK(json && strstr(json, "{\"type\":\"thinking\"") < strstr(json, "{\"type\":\"text\""), "ordine dei blocchi");
        JNode *j = json ? json_parse(json, strlen(json)) : NULL;
        CHECK(j && !strcmp(json_str(j, "role"), "assistant"), "messaggio JSON valido");
        json_free(j);
        CHECK(r.in_tokens == 420 && r.out_tokens == 57 && !strcmp(r.model, "claude-opus-5-5"), "uso: %ld %ld %s", r.in_tokens, r.out_tokens, r.model);
        free(text);
        free(json);
        reply_free(&r);
    }

    Reply r;
    reply_init(&r, PROTO_ANTHROPIC, "Claude");
    reply_feed(&r, REFUSAL, strlen(REFUSAL));
    reply_finish(&r, 0, "");
    CHECK(r.state == RS_REFUSED && !strcmp(r.category, "cyber"), "rifiuto: stato %d categoria %s", r.state, r.category);
    CHECK(reply_message_json(&r, NULL) == NULL, "rifiuto: niente da rimandare");
    reply_free(&r);

    reply_init(&r, PROTO_ANTHROPIC, "Claude");
    reply_feed(&r, FALLBACK, strlen(FALLBACK));
    reply_finish(&r, 0, "");
    char *text = reply_text(&r), *json = reply_message_json(&r, NULL);
    CHECK(r.state == RS_DONE && r.fallback, "ripiego: stato %d", r.state);
    CHECK(!strcmp(text, "Prima parte.\n\nSeconda parte."), "ripiego, testo: %s", text);
    CHECK(json && !strstr(json, "SIG-OPUS-55") && strstr(json, "SIG-OPUS-5\"") && !strstr(json, "fallback"), "ripiego, cronologia: %s", json ? json : "-");
    CHECK(json && strstr(json, "Prima parte.") && strstr(json, "Seconda parte."), "ripiego, testi conservati");
    free(text);
    free(json);
    reply_free(&r);

    reply_init(&r, PROTO_ANTHROPIC, "Claude");
    reply_feed(&r, HTTP401, strlen(HTTP401));
    reply_finish(&r, 0, "");
    CHECK(r.state == RS_ERROR && r.http_status == 401 && strstr(r.error, "Chiave API di Anthropic non valida"), "401: %d %s", r.state, r.error);
    reply_free(&r);

    reply_init(&r, PROTO_ANTHROPIC, "Claude");
    reply_feed(&r, OVERLOADED, strlen(OVERLOADED));
    reply_finish(&r, 0, "");
    CHECK(r.state == RS_ERROR && strstr(r.error, "sovraccarico"), "errore a meta': %s", r.error);
    reply_free(&r);

    reply_init(&r, PROTO_ANTHROPIC, "Claude");
    reply_finish(&r, 6, "Could not resolve host: api.anthropic.com");
    CHECK(r.state == RS_ERROR && strstr(r.error, "Wi-Fi"), "senza rete: %s", r.error);
    reply_free(&r);

    reply_init(&r, PROTO_ANTHROPIC, "Claude");
    const char *cut = START("claude-opus-5-5", "10") TEXT_START("0") TEXT_DELTA("0", "Meta'");
    reply_feed(&r, cut, strlen(cut));
    reply_finish(&r, 18, "transfer closed");
    CHECK(r.state == RS_ERROR && strstr(r.error, "interrotta"), "stream troncato: %s", r.error);
    reply_free(&r);
}

/* ------------------------------------------------------------------ cronologia e richiesta */

/* ------------------------------------------------------------------ stream compatibili OpenAI */
#define OA(delta) "data: {\"id\":\"c1\",\"object\":\"chat.completion.chunk\",\"model\":\"deepseek-flash\",\"choices\":[{\"index\":0,\"delta\":" delta ",\"finish_reason\":null}]}\n\n"
static const char *OA_NORMAL =
    ": keep-alive\n\n"
    OA("{\"role\":\"assistant\",\"content\":\"\"}")
    OA("{\"reasoning_content\":\"L'utente chiede la capitale.\"}")
    OA("{\"content\":\"Roma è \"}")
    OA("{\"content\":\"la capitale.\\nPunto due.\"}")
    "data: {\"id\":\"c1\",\"model\":\"deepseek-flash\",\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"stop\"}]}\n\n"
    "data: {\"id\":\"c1\",\"model\":\"deepseek-flash\",\"choices\":[],\"usage\":{\"prompt_tokens\":120,\"completion_tokens\":30,\"prompt_tokens_details\":{\"cached_tokens\":20}}}\n\n"
    "data: [DONE]\n\n\n@@CIRMOLO_HTTP 200\n";
static const char *OA_THINK =
    OA("{\"content\":\"<think>\\nragiono\"}")
    OA("{\"content\":\" ancora</think>\\n\\nEcco la risposta.\"}")
    "data: [DONE]\n\n\n@@CIRMOLO_HTTP 200\n";
static const char *OA_401 = "{\"error\":{\"message\":\"Incorrect API key provided\",\"type\":\"invalid_request_error\",\"code\":\"invalid_api_key\"}}\n\n@@CIRMOLO_HTTP 401\n";
static const char *OA_GEMINI_429 = "[{\"error\":{\"code\":429,\"message\":\"Resource has been exhausted\",\"status\":\"RESOURCE_EXHAUSTED\"}}]\n\n@@CIRMOLO_HTTP 429\n";
static const char *OA_MIDERR = OA("{\"content\":\"Ini\"}") "data: {\"error\":{\"message\":\"Internal server error\",\"code\":500}}\n\n\n@@CIRMOLO_HTTP 200\n";
static const char *OA_FILTER = OA("{\"content\":\"\"}") "data: {\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"content_filter\"}]}\n\ndata: [DONE]\n\n\n@@CIRMOLO_HTTP 200\n";

static void test_openai(void)
{
    for (unsigned seed = 1; seed <= 10; seed++) {
        Reply r;
        reply_init(&r, PROTO_OPENAI, "DeepSeek");
        feed_chunks(&r, OA_NORMAL, seed);
        reply_finish(&r, 0, "");
        char *t = reply_text(&r);
        int proto = -1;
        char *j = reply_message_json(&r, &proto);
        CHECK(r.state == RS_DONE, "OpenAI: stato %d", r.state);
        CHECK(!strcmp(t, "Roma è la capitale.\nPunto due."), "OpenAI: testo '%s'", t);
        CHECK(proto == PROTO_PLAIN && j && !strcmp(j, "{\"role\":\"assistant\",\"content\":\"Roma è la capitale.\\nPunto due.\"}"), "OpenAI: cronologia %s", j ? j : "-");
        CHECK(r.in_tokens == 100 && r.cache_read == 20 && r.out_tokens == 30 && !strcmp(r.model, "deepseek-flash"), "OpenAI: uso %ld %ld %ld", r.in_tokens, r.cache_read, r.out_tokens);
        free(t);
        free(j);
        reply_free(&r);
    }
    Reply r;
    reply_init(&r, PROTO_OPENAI, "MiniMax");
    const char *half = OA("{\"content\":\"<think>\\nragiono\"}");
    reply_feed(&r, half, strlen(half));
    char *t = reply_text(&r);
    CHECK(r.state == RS_THINKING && !t[0], "<think> aperto: stato %d testo '%s'", r.state, t);
    free(t);
    reply_free(&r);
    reply_feed(&r, OA_THINK, strlen(OA_THINK));
    reply_finish(&r, 0, "");
    t = reply_text(&r);
    CHECK(r.state == RS_DONE && !strcmp(t, "Ecco la risposta."), "<think> tolto: '%s'", t);
    free(t);
    reply_free(&r);

    reply_init(&r, PROTO_OPENAI, "OpenAI");
    reply_feed(&r, OA_401, strlen(OA_401));
    reply_finish(&r, 0, "");
    CHECK(r.state == RS_ERROR && strstr(r.error, "Chiave API di OpenAI non valida"), "OpenAI 401: %s", r.error);
    reply_free(&r);
    reply_init(&r, PROTO_OPENAI, "Google Gemini");
    reply_feed(&r, OA_GEMINI_429, strlen(OA_GEMINI_429));
    reply_finish(&r, 0, "");
    CHECK(r.state == RS_ERROR && strstr(r.error, "Google Gemini"), "Gemini 429: %s", r.error);
    reply_free(&r);
    reply_init(&r, PROTO_OPENAI, "Groq");
    reply_feed(&r, OA_MIDERR, strlen(OA_MIDERR));
    reply_finish(&r, 0, "");
    CHECK(r.state == RS_ERROR && strstr(r.error, "Groq"), "errore a meta': %s", r.error);
    reply_free(&r);
    reply_init(&r, PROTO_OPENAI, "Qwen");
    reply_feed(&r, OA_FILTER, strlen(OA_FILTER));
    reply_finish(&r, 0, "");
    CHECK(r.state == RS_REFUSED, "filtro dei contenuti: %d", r.state);
    reply_free(&r);
    reply_init(&r, PROTO_OPENAI, "Groq");
    reply_finish(&r, 6, "Could not resolve host");
    CHECK(r.state == RS_ERROR && strstr(r.error, "Wi-Fi"), "OpenAI senza rete: %s", r.error);
    reply_free(&r);
}

/* ------------------------------------------------------------------ conversazione mista */
static int count_msgs(const char *body)
{
    JNode *j = json_parse(body, strlen(body));
    const JNode *m = json_get(j, "messages");
    int n = 0;
    for (const JNode *x = m ? m->child : NULL; x; x = x->next) n++;
    json_free(j);
    return n;
}

static void test_conversation(void)
{
    Conversation c;
    conv_init(&c, "Sistema \"di prova\"");
    conv_add_user(&c, "Qual è la capitale d'Italia?");
    Reply r;
    reply_init(&r, PROTO_ANTHROPIC, "Claude");
    reply_feed(&r, NORMAL, strlen(NORMAL));
    reply_finish(&r, 0, "");
    int proto = -1;
    char *json = reply_message_json(&r, &proto);
    CHECK(proto == PROTO_ANTHROPIC, "formato Claude");
    conv_add(&c, ROLE_ASSISTANT, proto, json, reply_text(&r));
    reply_free(&r);
    conv_add_user(&c, "Domanda fallita");
    c.msg[c.n - 1].excluded = 1;
    conv_add_user(&c, "E quella della Francia?");
    reply_init(&r, PROTO_OPENAI, "DeepSeek");
    reply_feed(&r, OA_NORMAL, strlen(OA_NORMAL));
    reply_finish(&r, 0, "");
    char *oj = reply_message_json(&r, &proto);
    conv_add(&c, ROLE_ASSISTANT, proto, oj, reply_text(&r));
    reply_free(&r);
    conv_add_user(&c, "Grazie");

    const char *ids[] = { "claude-opus-5-5", "claude-sonnet-5-5", "claude-haiku-5-5" };
    for (int m = 0; m < 3; m++) {
        char *body = anthropic_request(&c, ids[m], "medium", m != 2);
        JNode *j = json_parse(body, strlen(body));
        CHECK(j != NULL, "corpo della richiesta non valido");
        CHECK(j && !strcmp(json_str(j, "model"), ids[m]), "modello");
        CHECK(j && json_get(j, "stream") && json_get(j, "stream")->type == J_TRUE, "stream");
        CHECK(j && !strcmp(json_path_str(j, "output_config", "effort"), "medium"), "impegno");
        CHECK(j && !json_get(j, "thinking"), "thinking omesso (adattivo)");
        CHECK(j && json_num(j, "max_tokens", 0) == 64000, "max_tokens");
        CHECK(j && !strcmp(json_path_str(j, "cache_control", "type"), "ephemeral"), "cache automatica");
        const char *fb = j ? json_str(j, "fallbacks") : NULL;
        CHECK(m != 2 ? (fb && !strcmp(fb, "default")) : !json_get(j, "fallbacks"), "ripiego per %s", ids[m]);
        CHECK(count_msgs(body) == 5, "messaggi inviati %d (quello fallito escluso)", count_msgs(body));
        CHECK(strstr(body, json) != NULL, "messaggio di Claude rimandato identico");
        CHECK(strstr(body, "{\"role\":\"assistant\",\"content\":\"Roma è la capitale.\\nPunto due.\"}") != NULL, "risposta di DeepSeek come testo");
        json_free(j);
        free(body);
    }
    char *ob = openai_request(&c, "glm-4.7-flash", 0, 1);
    JNode *j = json_parse(ob, strlen(ob));
    const JNode *msgs = json_get(j, "messages");
    CHECK(j && count_msgs(ob) == 6 && !strcmp(json_str(msgs->child, "role"), "system"), "OpenAI: system e 5 messaggi (%d)", count_msgs(ob));
    CHECK(j && !strstr(ob, "thinking") && !strstr(ob, "signature") && strstr(ob, "\"include_usage\":true"), "OpenAI: solo testo, con l'uso");
    CHECK(j && !json_get(j, "max_tokens"), "OpenAI: max_tokens omesso");
    json_free(j);
    free(ob);
    ob = openai_request(&c, "x", 512, 0);
    CHECK(!strstr(ob, "stream_options") && strstr(ob, "\"max_tokens\":512"), "OpenAI senza stream_options");
    free(ob);

    /* salvataggio e ripresa: stessi byte e stesso formato */
    char *saved = conv_save(&c);
    Conversation d;
    conv_init(&d, "");
    CHECK(conv_load(&d, saved, strlen(saved)) == 0, "conversazione non riletta");
    CHECK(d.n == c.n && !strcmp(d.system, c.system), "messaggi riletti %d su %d", d.n, c.n);
    for (int i = 0; i < c.n && i < d.n; i++) {
        CHECK(!strcmp(c.msg[i].json, d.msg[i].json) && c.msg[i].proto == d.msg[i].proto, "JSON o formato del messaggio %d", i);
        CHECK(!strcmp(c.msg[i].text, d.msg[i].text) && c.msg[i].excluded == d.msg[i].excluded, "testo o esclusione del messaggio %d", i);
    }
    char *b1 = anthropic_request(&c, ids[0], "high", 1), *b2 = anthropic_request(&d, ids[0], "high", 1);
    CHECK(!strcmp(b1, b2), "la richiesta dopo il salvataggio e' identica");
    free(b1);
    free(b2);
    free(saved);
    /* salvataggio della 0.2, senza "proto": i messaggi dell'assistente sono di Claude */
    const char *old = "{\"system\":\"s\",\"messages\":[{\"role\":\"user\",\"content\":\"ciao\"},{\"role\":\"assistant\",\"content\":[{\"type\":\"text\",\"text\":\"ehi\"}]}],"
                      "\"meta\":[{\"excluded\":0,\"text\":\"ciao\"},{\"excluded\":0,\"text\":\"ehi\"}]}";
    CHECK(conv_load(&d, old, strlen(old)) == 0 && d.n == 2 && d.msg[1].proto == PROTO_ANTHROPIC && d.msg[0].proto == PROTO_PLAIN, "salvataggio 0.2");
    conv_free(&d);
    conv_free(&c);
}

/* ------------------------------------------------------------------ fornitori e chiavi */
static void write_text(const char *path, const char *s)
{
    FILE *f = fopen(path, "wb");
    if (f) { fputs(s, f); fclose(f); }
}

static void test_providers(const char *outdir)
{
    char dir[512], path[600];
    snprintf(dir, sizeof(dir), "%s/fornitori", outdir);
    MKDIR(dir);
    static const char *const STALE[] = { "chiave.txt", "chiavi/anthropic.txt", "chiavi/zai.txt", NULL };
    for (int i = 0; STALE[i]; i++) { snprintf(path, sizeof(path), "%s/%s", dir, STALE[i]); remove(path); }
    snprintf(path, sizeof(path), "%s/fornitori.json", dir);
    write_text(path, "{\"fornitori\":[{\"id\":\"ollama\",\"nome\":\"Ollama di casa\",\"url\":\"http://192.168.1.10:11434/v1/\","
                     "\"modelli\":[\"qwen3:8b\",{\"id\":\"gemma4\",\"nome\":\"Gemma 4\",\"gratis\":true}]},"
                     "{\"id\":\"qwen\",\"url\":\"https://ws123.ap-southeast-1.maas.aliyuncs.com/compatible-mode/v1\"},"
                     "{\"id\":\"../male\",\"url\":\"x\"}]}");
    snprintf(path, sizeof(path), "%s/modelli", dir);
    MKDIR(path);
    snprintf(path, sizeof(path), "%s/modelli/openrouter.txt", dir);
    write_text(path, "qwen/qwen3.8-max:free\nopenrouter/free\n");
    Registry reg;
    registry_load(&reg, dir);
    CHECK(reg.n >= 13 && !strcmp(reg.p[0].id, "anthropic") && reg.p[0].proto == PROTO_ANTHROPIC, "Claude per primo (%d fornitori)", reg.n);
    int ol = registry_find(&reg, "ollama"), qw = registry_find(&reg, "qwen"), orr = registry_find(&reg, "openrouter");
    CHECK(registry_find(&reg, "../male") < 0, "id con barre rifiutato");
    CHECK(ol >= 0 && !reg.p[ol].needs_key && !strcmp(reg.p[ol].base, "http://192.168.1.10:11434/v1") && reg.p[ol].nmodels == 2, "fornitore aggiunto");
    CHECK(ol >= 0 && (reg.p[ol].models[1].flags & MF_FREE) && !strcmp(reg.p[ol].models[1].name, "Gemma 4"), "modello gratuito aggiunto");
    CHECK(qw >= 0 && strstr(reg.p[qw].base, "ws123") && reg.p[qw].needs_key && reg.p[qw].nmodels == 3, "fornitore cambiato solo nell'url");
    CHECK(orr >= 0 && provider_find_model(&reg.p[orr], "qwen/qwen3.8-max:free") >= 0 &&
          reg.p[orr].nmodels == reg.p[orr].npreset + 1, "modelli scaricati, senza doppioni (%d)", reg.p[orr].nmodels);
    for (int i = 0; i < reg.n; i++) CHECK(reg.p[i].nmodels > 0 && reg.p[i].base[0], "fornitore %s senza modelli o url", reg.p[i].id);

    char key[256];
    provider_key(&reg.p[0], dir, key, sizeof(key));
    CHECK(!key[0], "nessuna chiave");
    snprintf(path, sizeof(path), "%s/chiave.txt", dir);
    write_text(path, " sk-ant-api03-vecchia-0000000000\n");
    provider_key(&reg.p[0], dir, key, sizeof(key));
    CHECK(!strcmp(key, "sk-ant-api03-vecchia-0000000000"), "chiave della 0.2: %s", key);
    int zai = registry_find(&reg, "zai");
    CHECK(provider_save_key(&reg.p[zai], dir, "abcdef0123456789.XyZ") == 0, "chiave salvata");
    provider_key(&reg.p[zai], dir, key, sizeof(key));
    CHECK(!strcmp(key, "abcdef0123456789.XyZ"), "chiave di Z.ai con il punto: %s", key);
    char out[64];
    CHECK(clean_key("sk ant\n", out, sizeof(out)) != 0 && clean_key("sk-ant-api03-abc$def0000000", out, sizeof(out)) != 0, "chiavi non valide");
    Buf h = { 0 };
    provider_auth_headers(&reg.p[0], "K1", &h);
    CHECK(!strcmp(h.p, "x-api-key: K1\nanthropic-version: 2023-06-01\n"), "intestazioni Anthropic: %s", h.p);
    buf_clear(&h);
    provider_auth_headers(&reg.p[orr], "K2", &h);
    CHECK(strstr(h.p, "Authorization: Bearer K2\n") == h.p && strstr(h.p, "X-Title: Cirmolo\n"), "intestazioni OpenRouter: %s", h.p);
    buf_clear(&h);
    provider_auth_headers(&reg.p[ol], "", &h);
    CHECK(!h.p || !h.len, "senza chiave nessuna intestazione");
    buf_free(&h);

    const char *list = "{\"object\":\"list\",\"data\":[{\"id\":\"gpt-5-nano\"},{\"id\":\"whisper-1\"},{\"id\":\"text-embedding-3-small\"},"
                       "{\"id\":\"gpt-4o-mini-tts\"},{\"id\":\"dall-e-3\"},{\"id\":\"gpt-4.1\"}]}";
    char **ids = NULL;
    int n = parse_model_list(list, strlen(list), &ids);
    CHECK(n == 2 && !strcmp(ids[0], "gpt-4.1") && !strcmp(ids[1], "gpt-5-nano"), "elenco filtrato e ordinato: %d", n);
    for (int i = 0; i < n; i++) free(ids[i]);
    free(ids);
    const char *gl = "{\"object\":\"list\",\"data\":[{\"id\":\"models/gemini-3.8-flash\"},{\"id\":\"models/text-embedding-004\"}]}";
    n = parse_model_list(gl, strlen(gl), &ids);
    CHECK(n == 1 && !strcmp(ids[0], "gemini-3.8-flash"), "Gemini: prefisso models/ tolto");
    for (int i = 0; i < n; i++) free(ids[i]);
    free(ids);
    CHECK(parse_model_list("<html>", 6, &ids) == -1, "elenco non valido");
    const char *fetched[] = { "zz-nuovo", "glm-4.7-flash" };
    int before = reg.p[zai].npreset;
    provider_set_fetched(&reg.p[zai], dir, fetched, 2);
    CHECK(reg.p[zai].nmodels == before + 1 && provider_find_model(&reg.p[zai], "zz-nuovo") == before, "modelli scaricati aggiunti in coda");
    provider_set_fetched(&reg.p[zai], dir, fetched, 1);
    CHECK(reg.p[zai].nmodels == before + 1, "nuovo elenco sostituisce il vecchio");
    CHECK(!strcmp(model_display_name("claude-opus-5-5"), "Claude Opus 5.5") && !strcmp(model_display_name("boh"), "boh"), "nomi dei modelli");
    Model m = { "x", "x", 4.0, 20.0, 0.2, 0 };
    CHECK(fabs(model_cost(&m, 1000000, 100000, 0, 0) - 6.0) < 1e-9, "costo");
    Model unk = { "y", "y", -1, -1, -1, 0 };
    CHECK(model_cost(&unk, 1000, 1000, 0, 0) == 0.0, "costo sconosciuto");
    registry_free(&reg);
}

/* ------------------------------------------------------------------ voce */
static void test_voice(void)
{
    const char *s = "Ciao, è un test! \x01\xff";
    char *e = b64_encode((const unsigned char *)s, strlen(s));
    size_t n = 0;
    unsigned char *d = b64_decode(e, strlen(e), &n);
    CHECK(n == strlen(s) && !memcmp(d, s, n), "base64 andata e ritorno");
    free(e);
    free(d);
    e = b64_encode((const unsigned char *)"Ma", 2);
    CHECK(!strcmp(e, "TWE="), "base64 con =: %s", e);
    free(e);

    int16_t pcm[4] = { 0, 1000, -1000, 32767 };
    size_t wl = 0;
    char *wav = wav_encode(pcm, 4, 16000, &wl);
    uint32_t rate;
    memcpy(&rate, wav + 24, 4);
    CHECK(wl == 52 && !memcmp(wav, "RIFF", 4) && !memcmp(wav + 36, "data", 4) && rate == 16000 && !memcmp(wav + 44, pcm, 8), "WAV");
    free(wav);

    Recorder r;
    rec_init(&r);
    rec_start_usb(&r, 48000.0f);
    float buf[480];
    for (int b = 0; b < 100; b++) {             /* un secondo di la a 48 kHz, a blocchi */
        for (int i = 0; i < 480; i++) buf[i] = 0.5f * sinf(6.2831853f * 440.0f * (b * 480 + i) / 48000.0f);
        rec_capture(&r, buf, 480);
    }
    CHECK(abs(r.len - 16000) <= 1 && fabsf(rec_seconds(&r) - 1.0f) < 0.01f, "ricampionamento a 16 kHz: %d", r.len);
    int peak = 0;
    for (int i = 0; i < r.len; i++) if (abs(r.pcm[i]) > peak) peak = abs(r.pcm[i]);
    CHECK(peak > 14000 && peak < 17000 && r.level > 0.45f, "ampiezza conservata: %d", peak);
    rec_stop(&r);
    int len = r.len;
    rec_capture(&r, buf, 480);
    CHECK(r.len == len, "fermo: niente campioni");
    rec_free(&r);

    Player pl;
    player_init(&pl);
    int16_t one[2400];
    for (int i = 0; i < 2400; i++) one[i] = 16384;
    pl.src_rate = 24000;
    player_push(&pl, one, 2400);
    float out[2 * 4000];
    memset(out, 0, sizeof(out));
    player_mix(&pl, out, 4000, 48000.0f, 1.0f);
    CHECK(fabsf(out[0] - 0.5f) < 0.01f && fabsf(out[2 * 3999 + 1] - 0.5f) < 0.01f && player_queued(&pl) == 400, "voce a 24 kHz su uscita a 48 kHz: %f, coda %d", out[0], player_queued(&pl));
    player_push(&pl, one, 100);
    player_clear(&pl);
    memset(out, 0, sizeof(out));
    player_mix(&pl, out, 10, 48000.0f, 1.0f);
    player_mix(&pl, out, 10, 48000.0f, 1.0f);
    CHECK(player_queued(&pl) == 0 && out[0] == 0.0f, "lettura fermata: coda %d", player_queued(&pl));
    player_free(&pl);

    char *c = tts_clean("# Titolo\nUn **punto** in `codice` e [un link](https://x.it).\n- primo\n- secondo\n\n```\nprint(1)\n```\nVedi https://esempio.it/pagina ora.\n---\n| a | b |");
    CHECK(c && strstr(c, "Titolo.\n") == c && strstr(c, "Un punto in codice e un link.") && strstr(c, "primo.\nsecondo.") &&
          strstr(c, "(segue del codice") && !strstr(c, "print") && strstr(c, "Vedi (collegamento) ora.") && !strstr(c, "---") && !strstr(c, "**"),
          "testo da leggere: [%s]", c ? c : "-");
    free(c);

    const char *txt = "Prima frase. Seconda frase, un po' più lunga! Terza: 2.7 milioni";
    size_t pos = 0;
    char *k1 = tts_next_chunk(txt, &pos, 20, 0);
    CHECK(k1 && !strcmp(k1, "Prima frase."), "pezzo 1: %s", k1 ? k1 : "-");
    char *k2 = tts_next_chunk(txt, &pos, 200, 0);
    CHECK(k2 && !strcmp(k2, "Seconda frase, un po' più lunga! Terza:"), "pezzo 2: %s", k2 ? k2 : "-");
    char *k3 = tts_next_chunk(txt, &pos, 200, 0);
    CHECK(!k3, "frase non finita: si aspetta");
    char *k4 = tts_next_chunk(txt, &pos, 200, 1);
    CHECK(k4 && !strcmp(k4, "2.7 milioni"), "ultima frase: %s", k4 ? k4 : "-");
    CHECK(!tts_next_chunk(txt, &pos, 200, 1), "finito");
    free(k1); free(k2); free(k4);
    const char *longs = "parola parola parola parola parola parola parola parola";
    pos = 0;
    char *k5 = tts_next_chunk(longs, &pos, 20, 0);
    CHECK(k5 && strlen(k5) <= 20 && k5[strlen(k5) - 1] != ' ', "frase lunga tagliata a uno spazio: '%s'", k5 ? k5 : "-");
    free(k5);

    const char *oa = "{\"text\":\"  Che ore sono?  \"}";
    char *t = stt_parse(oa, strlen(oa), 0);
    CHECK(t && !strcmp(t, "Che ore sono?"), "trascrizione OpenAI: %s", t ? t : "-");
    free(t);
    const char *gm = "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"\\\"Accendi la luce\\\"\\n\"}],\"role\":\"model\"}}]}";
    t = stt_parse(gm, strlen(gm), 1);
    CHECK(t && !strcmp(t, "Accendi la luce"), "trascrizione Gemini: %s", t ? t : "-");
    free(t);
    const char *empty = "{\"candidates\":[{\"finishReason\":\"STOP\"}]}";
    t = stt_parse(empty, strlen(empty), 1);
    CHECK(t && !t[0], "Gemini senza parole");
    free(t);
    CHECK(stt_parse("{}", 2, 0) == NULL, "trascrizione non riconosciuta");
    CHECK(!strcmp(lang_code("Italian"), "it") && !strcmp(lang_code("Chinese (S)"), "zh") && !strcmp(lang_code("Klingon"), ""), "codici delle lingue");
}

/* ------------------------------------------------------------------ traduzioni */
static void test_i18n(const char *outdir)
{
    char dir[512], path[600];
    snprintf(dir, sizeof(dir), "%s/lang", outdir);
    MKDIR(dir);
    snprintf(path, sizeof(path), "%s/English.json", dir);
    write_text(path, "\xEF\xBB\xBF{\n  \"Ciao\": \"Hello\",\n  \"Riga\\ndue\": \"Line\\ntwo \\u00e8\",\n  \"Vuoto\": \"\",\n  \"Ha risposto %s.\": \"%s replied.\"\n}\n");
    CHECK(!strcmp(i18n_set(dir, "Italian"), "Italian") && !strcmp(tr("Ciao"), "Ciao"), "italiano: nessuna traduzione");
    CHECK(!strcmp(i18n_set(dir, "English"), "English") && !strcmp(tr("Ciao"), "Hello"), "inglese");
    CHECK(!strcmp(tr("Riga\ndue"), "Line\ntwo \xc3\xa8"), "escape: %s", tr("Riga\ndue"));
    CHECK(!strcmp(tr("Vuoto"), "Vuoto") && !strcmp(tr("Mai visto"), "Mai visto"), "senza traduzione resta in italiano");
    CHECK(!strcmp(i18n_set(dir, "Klingon"), "English") && !strcmp(tr("Ciao"), "Hello"), "lingua senza file: inglese");
    char t[64];
    snprintf(t, sizeof(t), tr("Ha risposto %s."), "Qwen");
    CHECK(!strcmp(t, "Qwen replied."), "segnaposto: %s", t);
    i18n_set(dir, NULL);
}

/* ------------------------------------------------------------------ interfaccia */
static void tap(ChatApp *a, int b) { chat_button(a, b, 1); chat_update(a, 0.016f); chat_button(a, b, 0); chat_update(a, 0.016f); }

/* scrive con la tastiera a schermo, muovendosi con la croce come farebbe una persona */
static void osk_type(ChatApp *a, const char *s)
{
    static const char *L0[5] = { "1234567890", "qwertyuiop", "asdfghjkl'", "zxcvbnm,.?", "" };
    while (*s) {
        char ch = *s++;
        int lower = ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch;
        if (ch == ' ') { tap(a, PAD_Y); continue; }
        int tr_ = -1, tc = -1;
        for (int r = 0; r < 4 && tr_ < 0; r++) { const char *p = strchr(L0[r], lower); if (p) { tr_ = r; tc = (int)(p - L0[r]); } }
        if (tr_ < 0) continue;
        int layer, r, c;
        chat_osk_pos(a, &layer, &r, &c);
        if (layer) tap(a, PAD_L2);
        for (; r < tr_; r++) tap(a, PAD_DOWN);
        for (; r > tr_; r--) tap(a, PAD_UP);
        chat_osk_pos(a, &layer, &r, &c);
        for (; c < tc; c++) tap(a, PAD_RIGHT);
        for (; c > tc; c--) tap(a, PAD_LEFT);
        tap(a, PAD_A);
    }
}

static void rm_tree_files(const char *dir, const char *const *names)
{
    char p[600];
    for (int i = 0; names[i]; i++) { snprintf(p, sizeof(p), "%s/%s", dir, names[i]); remove(p); }
}

static void test_ui(const char *outdir, const char *langdir)
{
    char state[512], dir[512], path[600];
    snprintf(dir, sizeof(dir), "%s/claude", outdir);
    snprintf(state, sizeof(state), "%s/impostazioni.txt", dir);
    MKDIR(dir);
    static const char *const OLD[] = { "conversazione.json", "chiave.txt", "impostazioni.txt", "chiavi/anthropic.txt", "chiavi/groq.txt",
                                       "chiavi/openai.txt", "chiavi/zai.txt", "fornitori.json", NULL };
    rm_tree_files(dir, OLD);

    i18n_set(langdir, "Italian");
    ChatApp *a = chat_create(48000.0f, state);
    uint32_t *px = malloc(sizeof(uint32_t) * 640 * 480);
    Canvas cv = { px, 640, 480 };
#define SHOT(name) do { chat_update(a, 0.016f); chat_draw(a, &cv); snprintf(path, sizeof(path), "%s/%s.bmp", outdir, name); write_bmp(path, &cv); } while (0)
    CHECK(!strcmp(chat_model_id(a), "claude-opus-5-5"), "predefinito: Claude Opus 5.5 (%s)", chat_model_id(a));
    SHOT("chat-01-senza-chiave");

    osk_type(a, "ciao claude");
    CHECK(!strcmp(chat_input(a), "Ciao claude"), "tastiera a schermo: %s", chat_input(a));
    tap(a, PAD_B);
    tap(a, PAD_L1);
    tap(a, PAD_L1);
    chat_text(a, "X", TEXT_CHARS);
    CHECK(!strcmp(chat_input(a), "Ciao claXud"), "cursore e cancella: %s", chat_input(a));
    for (int i = 0; i < 20; i++) chat_text(a, "", TEXT_BACKSPACE);
    chat_text(a, "", TEXT_RIGHT);
    chat_text(a, "", TEXT_LEFT);
    chat_text(a, "", TEXT_DELETE);
    chat_text(a, "", TEXT_DELETE);
    CHECK(!strcmp(chat_input(a), ""), "cancella in avanti: %s", chat_input(a));

    /* senza chiave non parte niente */
    chat_text(a, "Qual è la capitale d'Italia?", TEXT_CHARS);
    tap(a, PAD_START);
    CHECK(!chat_busy(a) && strstr(chat_toast(a), "chiave"), "senza chiave: %s", chat_toast(a));
    chat_set_key(a, "sk-ant-prova-0000000000000000000000");
    chat_set_offline(a, 1);
    tap(a, PAD_START);
    CHECK(chat_busy(a) && chat_conversation(a)->n == 1 && !strcmp(chat_input(a), ""), "invio");
    chat_feed_reply(a, START("claude-opus-5-5", "420") THINK("0", "SIG1") TEXT_START("1") TEXT_DELTA("1", "Roma è la capitale"), 0);
    SHOT("chat-02-arriva");
    chat_feed_reply(a, TEXT_DELTA("1", " d\\u2019Italia.\\n\\n## Qualche dato\\n- **abitanti:** circa 2,7 milioni\\n- fiume: il Tevere\\n\\n```\\nprint(\\\"Roma\\\")\\n```") STOP("1") END("end_turn", "57"), 1);
    Conversation *cv2 = chat_conversation(a);
    CHECK(!chat_busy(a) && cv2->n == 2 && cv2->msg[1].role == ROLE_ASSISTANT && !cv2->msg[1].excluded && cv2->msg[1].proto == PROTO_ANTHROPIC, "risposta in cronologia");
    CHECK(cv2->out_tokens == 57 && cv2->cost > 0.0 && cv2->msg[1].model && !strcmp(cv2->msg[1].model, "claude-opus-5-5"), "consumo e modello");
    SHOT("chat-03-lettura");

    /* errore 401: resta da leggere ma non torna al modello; Y riprova */
    chat_set_view(a, PAGE_CHAT, 1);
    chat_text(a, "Ancora una", TEXT_CHARS);
    chat_text(a, "", TEXT_ENTER);
    chat_feed_reply(a, HTTP401, 1);
    CHECK(cv2->n == 3 && cv2->msg[2].excluded && strstr(cv2->msg[2].note, "Chiave"), "errore 401 con nota");
    chat_set_view(a, PAGE_CHAT, 0);
    tap(a, PAD_Y);
    CHECK(chat_busy(a) && cv2->n == 4 && !strcmp(cv2->msg[3].text, "Ancora una"), "riprova");
    chat_feed_reply(a, START("claude-opus-5-5", "500") TEXT_START("0") TEXT_DELTA("0", "Eccomi.") STOP("0") END("end_turn", "5"), 1);

    /* cambio di fornitore a meta' conversazione: DeepSeek con la sua chiave */
    CHECK(chat_select_provider(a, "deepseek") >= 0 && !strcmp(chat_model_id(a), "deepseek-flash"), "DeepSeek: %s", chat_model_id(a));
    chat_set_key(a, "sk-deepseek-0000000000000000");
    chat_set_view(a, PAGE_CHAT, 1);
    chat_text(a, "E la Francia?", TEXT_CHARS);
    chat_text(a, "", TEXT_ENTER);
    CHECK(chat_busy(a), "domanda a DeepSeek");
    chat_feed_reply(a, OA_NORMAL, 1);
    CHECK(!chat_busy(a) && cv2->n == 7 && cv2->msg[6].proto == PROTO_PLAIN && !cv2->msg[6].excluded && !strcmp(cv2->msg[6].model, "deepseek-flash"), "risposta di DeepSeek");
    SHOT("chat-04-deepseek");

    /* domanda veloce durante una risposta: B la ferma */
    chat_set_view(a, PAGE_CHAT, 0);
    tap(a, PAD_X);
    tap(a, PAD_A);
    CHECK(chat_busy(a), "domanda veloce inviata");
    tap(a, PAD_B);
    CHECK(!chat_busy(a) && cv2->msg[cv2->n - 1].excluded, "B ferma la risposta");

    /* impostazioni: scheda Chat, scelta del fornitore e del modello */
    chat_select_provider(a, "anthropic");
    chat_open_settings(a, 0, 0);
    SHOT("chat-05-impostazioni");
    tap(a, PAD_RIGHT);
    CHECK(!strcmp(chat_model_id(a), "gpt-5-nano"), "destra: fornitore successivo (%s)", chat_model_id(a));
    tap(a, PAD_LEFT);
    tap(a, PAD_DOWN);
    tap(a, PAD_A);                                /* elenco dei modelli */
    tap(a, PAD_DOWN);
    SHOT("chat-06-modelli");
    tap(a, PAD_A);
    CHECK(!strcmp(chat_model_id(a), "claude-sonnet-5-5"), "modello scelto dall'elenco: %s", chat_model_id(a));
    tap(a, PAD_DOWN); tap(a, PAD_DOWN); tap(a, PAD_DOWN);
    tap(a, PAD_A);                                /* Chiave API: inserimento */
    chat_text(a, "sk-ant-api03-abcdefghijklmnopqrstuvwxyz", TEXT_CHARS);
    SHOT("chat-07-chiave");
    tap(a, PAD_START);
    snprintf(path, sizeof(path), "%s/chiavi/anthropic.txt", dir);
    FILE *f = fopen(path, "rb");
    char kb[128] = "";
    if (f) { size_t got = fread(kb, 1, sizeof(kb) - 1, f); kb[got] = 0; fclose(f); }
    CHECK(!strcmp(kb, "sk-ant-api03-abcdefghijklmnopqrstuvwxyz"), "chiave salvata: %s", kb);

    /* scheda Voce: senza chiavi niente voce; con la chiave di Groq la trascrizione si sceglie da sola */
    tap(a, PAD_R1);
    SHOT("chat-08-voce-senza-chiavi");
    chat_set_voice(a, "groq", NULL);
    tap(a, PAD_DOWN); tap(a, PAD_DOWN); tap(a, PAD_DOWN);
    tap(a, PAD_A);                                /* Trascrizione: A apre la chiave di Groq */
    chat_text(a, "gsk_0123456789abcdefghij", TEXT_CHARS);
    tap(a, PAD_START);
    snprintf(path, sizeof(path), "%s/chiavi/groq.txt", dir);
    f = fopen(path, "rb");
    CHECK(f != NULL, "chiave di Groq salvata");
    if (f) fclose(f);
    snprintf(path, sizeof(path), "%s/chiavi/openai.txt", dir);
    write_text(path, "sk-proj-0123456789abcdefghij");
    chat_set_voice(a, "groq", "openai");
    chat_capture_status(a, "USB Audio Device, USB Audio", 48000.0f);
    chat_open_settings(a, 1, 0);
    SHOT("chat-09-voce");

    /* R2 tenuto: registra; lasciato: trascrive e invia; la risposta si legge a frasi */
    chat_set_view(a, PAGE_CHAT, 0);
    chat_button(a, PAD_R2, 1);
    for (int i = 0; i < 25; i++) chat_update(a, 0.016f);
    CHECK(chat_voice_state(a) == 2, "R2 tenuto: registra (%d)", chat_voice_state(a));
    float buf[960];
    for (int b = 0; b < 50; b++) {
        for (int i = 0; i < 960; i++) buf[i] = 0.3f * sinf(6.2831853f * 220.0f * (b * 960 + i) / 48000.0f);
        chat_capture(a, buf, 960);
    }
    SHOT("chat-10-ascolto");
    chat_button(a, PAD_R2, 0);
    chat_update(a, 0.016f);
    CHECK(chat_voice_state(a) == 3, "R2 lasciato: trascrive (%d)", chat_voice_state(a));
    SHOT("chat-11-trascrivo");
    chat_feed_transcript(a, "Che tempo fa a Roma?");
    CHECK(chat_busy(a) && !strcmp(cv2->msg[cv2->n - 1].text, "Che tempo fa a Roma?"), "domanda a voce inviata");
    chat_feed_reply(a, START("claude-sonnet-5-5", "50") TEXT_START("0") TEXT_DELTA("0", "## Meteo\\nOggi c'è **sole**.\\nDomani pi"), 0);
    char *sp = chat_speech_text(a);
    CHECK(sp && !strcmp(sp, "Meteo.\nOggi c'è sole.\n"), "lettura delle righe complete: [%s]", sp ? sp : "-");
    free(sp);
    chat_feed_reply(a, TEXT_DELTA("0", "oggia.") STOP("0") END("end_turn", "9"), 1);
    sp = chat_speech_text(a);
    CHECK(sp && strstr(sp, "Domani pioggia.") != NULL, "lettura completa: [%s]", sp ? sp : "-");
    free(sp);
    SHOT("chat-12-legge");
    tap(a, PAD_B);
    CHECK(chat_speech_text(a) == NULL, "B ferma la lettura");

    /* R2 premuto e lasciato subito: domande veloci; troppo breve non parte */
    tap(a, PAD_R2);
    SHOT("chat-13-veloci");
    tap(a, PAD_B);
    chat_button(a, PAD_R2, 1);
    for (int i = 0; i < 25; i++) chat_update(a, 0.016f);
    chat_button(a, PAD_R2, 0);
    chat_update(a, 0.016f);
    CHECK(chat_voice_state(a) == 0 && strstr(chat_toast(a), "breve"), "registrazione troppo breve: %s", chat_toast(a));
    /* B durante la registrazione annulla */
    chat_button(a, PAD_R2, 1);
    for (int i = 0; i < 25; i++) chat_update(a, 0.016f);
    chat_capture(a, buf, 960);
    tap(a, PAD_B);
    chat_button(a, PAD_R2, 0);
    chat_update(a, 0.016f);
    CHECK(chat_voice_state(a) == 0 && strstr(chat_toast(a), "annullata"), "registrazione annullata: %s", chat_toast(a));
    /* senza microfono */
    chat_capture_status(a, NULL, 0.0f);
    chat_button(a, PAD_R2, 1);
    for (int i = 0; i < 25; i++) chat_update(a, 0.016f);
    chat_button(a, PAD_R2, 0);
    CHECK(chat_voice_state(a) == 0 && strstr(chat_toast(a), "microfono"), "senza microfono: %s", chat_toast(a));

    tap(a, PAD_MENU);
    SHOT("chat-14-uscita");
    tap(a, PAD_B);

    /* la conversazione e le scelte si ritrovano alla riapertura */
    int before = cv2->n;
    chat_destroy(a);
    a = chat_create(48000.0f, state);
    CHECK(chat_conversation(a)->n == before, "conversazione ripresa: %d su %d", chat_conversation(a)->n, before);
    CHECK(!strcmp(chat_model_id(a), "claude-sonnet-5-5"), "modello ripreso: %s", chat_model_id(a));

    /* in inglese */
    i18n_set(langdir, "English");
    chat_set_view(a, PAGE_CHAT, 0);
    SHOT("chat-15-inglese");
    chat_open_settings(a, 1, 1);
    SHOT("chat-16-inglese-voce");
    i18n_set(langdir, "Italian");
    chat_destroy(a);

    /* impostazioni della 0.2 (model=1 era Sonnet) */
    rm_tree_files(dir, OLD);
    write_text(state, "# Chiedi a Claude\nmodel=1\neffort=2\nconcise=0\n");
    a = chat_create(48000.0f, state);
    CHECK(!strcmp(chat_model_id(a), "claude-sonnet-5-5"), "impostazioni 0.2: %s", chat_model_id(a));
    chat_destroy(a);
    free(px);
}

int main(int argc, char **argv)
{
    const char *fonts = argc > 1 ? argv[1] : ".", *outdir = argc > 2 ? argv[2] : ".", *langdir = argc > 3 ? argv[3] : "lang";
    char f1[512], f2[512];
    snprintf(f1, sizeof(f1), "%s/BeVietnamPro-Regular.ttf", fonts);
    snprintf(f2, sizeof(f2), "%s/BeVietnamPro-SemiBold.ttf", fonts);
    if (gfx_load_fonts(f1, f2)) return 1;
    test_json();
    printf("JSON: %d controlli, %d errori\n", checks, fails);
    test_streams();
    printf("stream Anthropic: %d controlli, %d errori\n", checks, fails);
    test_openai();
    printf("stream OpenAI: %d controlli, %d errori\n", checks, fails);
    test_conversation();
    printf("cronologia: %d controlli, %d errori\n", checks, fails);
    test_providers(outdir);
    printf("fornitori: %d controlli, %d errori\n", checks, fails);
    test_voice();
    printf("voce: %d controlli, %d errori\n", checks, fails);
    test_i18n(outdir);
    printf("traduzioni: %d controlli, %d errori\n", checks, fails);
    test_ui(outdir, langdir);
    printf("interfaccia: %d controlli, %d errori\n", checks, fails);
    printf("TOTALE: %d controlli, %d errori\n", checks, fails);
    gfx_free_fonts();
    return fails ? 1 : 0;
}
