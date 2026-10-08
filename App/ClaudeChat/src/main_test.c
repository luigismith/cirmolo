/* Chiedi a Claude - prove senza rete: JSON, lettura degli stream SSE (risposta normale, rifiuto,
 * ripiego su un altro modello, errori), cronologia rimandata identica, salvataggio, tastiera a schermo,
 * schermate (BMP).
 *
 * Uso: chat-test <cartella dei font> <cartella di uscita>
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chat_app.h"
#include "claude.h"
#include "gfx.h"
#include "json.h"
#include "platform.h"

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
        reply_init(&r);
        feed_chunks(&r, NORMAL, seed);
        reply_finish(&r, 0, "");
        char *text = reply_text(&r), *json = reply_message_json(&r);
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
    reply_init(&r);
    reply_feed(&r, REFUSAL, strlen(REFUSAL));
    reply_finish(&r, 0, "");
    CHECK(r.state == RS_REFUSED && !strcmp(r.category, "cyber"), "rifiuto: stato %d categoria %s", r.state, r.category);
    CHECK(reply_message_json(&r) == NULL, "rifiuto: niente da rimandare");
    reply_free(&r);

    reply_init(&r);
    reply_feed(&r, FALLBACK, strlen(FALLBACK));
    reply_finish(&r, 0, "");
    char *text = reply_text(&r), *json = reply_message_json(&r);
    CHECK(r.state == RS_DONE && r.fallback, "ripiego: stato %d", r.state);
    CHECK(!strcmp(text, "Prima parte.\n\nSeconda parte."), "ripiego, testo: %s", text);
    CHECK(json && !strstr(json, "SIG-OPUS-55") && strstr(json, "SIG-OPUS-5\"") && !strstr(json, "fallback"), "ripiego, cronologia: %s", json ? json : "-");
    CHECK(json && strstr(json, "Prima parte.") && strstr(json, "Seconda parte."), "ripiego, testi conservati");
    free(text);
    free(json);
    reply_free(&r);

    reply_init(&r);
    reply_feed(&r, HTTP401, strlen(HTTP401));
    reply_finish(&r, 0, "");
    CHECK(r.state == RS_ERROR && r.http_status == 401 && strstr(r.error, "Chiave API non valida"), "401: %d %s", r.state, r.error);
    reply_free(&r);

    reply_init(&r);
    reply_feed(&r, OVERLOADED, strlen(OVERLOADED));
    reply_finish(&r, 0, "");
    CHECK(r.state == RS_ERROR && strstr(r.error, "sovraccarico"), "errore a meta': %s", r.error);
    reply_free(&r);

    reply_init(&r);
    reply_finish(&r, 6, "Could not resolve host: api.anthropic.com");
    CHECK(r.state == RS_ERROR && strstr(r.error, "Wi-Fi"), "senza rete: %s", r.error);
    reply_free(&r);

    reply_init(&r);
    const char *cut = START("claude-opus-5-5", "10") TEXT_START("0") TEXT_DELTA("0", "Meta'");
    reply_feed(&r, cut, strlen(cut));
    reply_finish(&r, 18, "transfer closed");
    CHECK(r.state == RS_ERROR && strstr(r.error, "interrotta"), "stream troncato: %s", r.error);
    reply_free(&r);
}

/* ------------------------------------------------------------------ cronologia e richiesta */
static void test_conversation(void)
{
    Conversation c;
    conv_init(&c, "Sistema \"di prova\"");
    conv_add_user(&c, "Qual è la capitale d'Italia?");
    Reply r;
    reply_init(&r);
    reply_feed(&r, NORMAL, strlen(NORMAL));
    reply_finish(&r, 0, "");
    char *json = reply_message_json(&r);
    conv_add(&c, ROLE_ASSISTANT, json, reply_text(&r));
    reply_free(&r);
    conv_add_user(&c, "Domanda fallita");
    c.msg[c.n - 1].excluded = 1;
    conv_add_user(&c, "E quella della Francia?");

    for (int m = 0; m < MODEL_COUNT; m++) {
        char *body = conv_request(&c, m, EFFORT_MEDIUM);
        JNode *j = json_parse(body, strlen(body));
        CHECK(j != NULL, "corpo della richiesta non valido");
        CHECK(j && !strcmp(json_str(j, "model"), CLAUDE_MODELS[m].id), "modello");
        CHECK(j && json_get(j, "stream") && json_get(j, "stream")->type == J_TRUE, "stream");
        CHECK(j && !strcmp(json_path_str(j, "output_config", "effort"), "medium"), "impegno");
        CHECK(j && !json_get(j, "thinking"), "thinking omesso (adattivo)");
        CHECK(j && json_num(j, "max_tokens", 0) == 64000, "max_tokens");
        CHECK(j && !strcmp(json_path_str(j, "cache_control", "type"), "ephemeral"), "cache automatica");
        const char *fb = j ? json_str(j, "fallbacks") : NULL;
        CHECK(CLAUDE_MODELS[m].fallbacks ? (fb && !strcmp(fb, "default")) : !json_get(j, "fallbacks"), "ripiego per %s", CLAUDE_MODELS[m].id);
        CHECK((conv_beta(m) != NULL) == CLAUDE_MODELS[m].fallbacks, "intestazione beta");
        const JNode *msgs = json_get(j, "messages");
        int n = 0;
        for (const JNode *x = msgs ? msgs->child : NULL; x; x = x->next) n++;
        CHECK(n == 3, "messaggi inviati %d (quello fallito escluso)", n);
        CHECK(strstr(body, json) != NULL, "messaggio di Claude rimandato identico");
        json_free(j);
        free(body);
    }
    /* salvataggio e ripresa: stessi byte per i messaggi */
    char *saved = conv_save(&c);
    Conversation d;
    conv_init(&d, "");
    CHECK(conv_load(&d, saved, strlen(saved)) == 0, "conversazione non riletta");
    CHECK(d.n == c.n && !strcmp(d.system, c.system), "messaggi riletti %d su %d", d.n, c.n);
    for (int i = 0; i < c.n && i < d.n; i++) {
        CHECK(!strcmp(c.msg[i].json, d.msg[i].json), "JSON del messaggio %d cambiato", i);
        CHECK(!strcmp(c.msg[i].text, d.msg[i].text) && c.msg[i].excluded == d.msg[i].excluded, "testo o esclusione del messaggio %d", i);
    }
    char *b1 = conv_request(&c, MODEL_OPUS, EFFORT_HIGH), *b2 = conv_request(&d, MODEL_OPUS, EFFORT_HIGH);
    CHECK(!strcmp(b1, b2), "la richiesta dopo il salvataggio e' identica");
    free(b1);
    free(b2);
    free(saved);
    conv_free(&d);
    conv_free(&c);
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
        int tr = -1, tc = -1;
        for (int r = 0; r < 4 && tr < 0; r++) { const char *p = strchr(L0[r], lower); if (p) { tr = r; tc = (int)(p - L0[r]); } }
        if (tr < 0) continue;
        int layer, r, c;
        chat_osk_pos(a, &layer, &r, &c);
        if (layer) tap(a, PAD_L2);
        for (; r < tr; r++) tap(a, PAD_DOWN);
        for (; r > tr; r--) tap(a, PAD_UP);
        chat_osk_pos(a, &layer, &r, &c);
        for (; c < tc; c++) tap(a, PAD_RIGHT);
        for (; c > tc; c--) tap(a, PAD_LEFT);
        tap(a, PAD_A);
    }
}

static void test_ui(const char *outdir)
{
    char state[512];
    snprintf(state, sizeof(state), "%s/claude/impostazioni.txt", outdir);
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/claude", outdir);
    char conv_path[600];
    snprintf(conv_path, sizeof(conv_path), "%s/conversazione.json", dir);
    remove(conv_path);
    char key_path[600];
    snprintf(key_path, sizeof(key_path), "%s/chiave.txt", dir);
    remove(key_path);

    ChatApp *a = chat_create(48000.0f, state);
    uint32_t *px = malloc(sizeof(uint32_t) * 640 * 480);
    Canvas cv = { px, 640, 480 };
    char path[600];
#define SHOT(name) do { chat_update(a, 0.016f); chat_draw(a, &cv); snprintf(path, sizeof(path), "%s/%s.bmp", outdir, name); write_bmp(path, &cv); } while (0)
    SHOT("chat-1-senza-chiave");

    osk_type(a, "ciao claude");
    CHECK(!strcmp(chat_input(a), "Ciao claude"), "tastiera a schermo: %s", chat_input(a));
    tap(a, PAD_B);
    tap(a, PAD_L1);
    tap(a, PAD_L1);
    chat_text(a, "X", TEXT_CHARS);
    CHECK(!strcmp(chat_input(a), "Ciao claXud"), "cursore e cancella: %s", chat_input(a));
    for (int i = 0; i < 20; i++) chat_text(a, "", TEXT_BACKSPACE);
    chat_text(a, "", TEXT_RIGHT);
    CHECK(!strcmp(chat_input(a), "ud"), "cancella fino all'inizio: %s", chat_input(a));
    chat_text(a, "", TEXT_LEFT);
    chat_text(a, "", TEXT_DELETE);
    chat_text(a, "", TEXT_DELETE);
    CHECK(!strcmp(chat_input(a), ""), "cancella in avanti: %s", chat_input(a));
    tap(a, PAD_L2);
    tap(a, PAD_UP);  /* dalla riga 1 alla 0 dei simboli: lettere accentate */
    int layer, r, c;
    chat_osk_pos(a, &layer, &r, &c);
    tap(a, PAD_A);
    CHECK(layer == 1 && r == 0 && strlen(chat_input(a)) == 2, "simboli: livello %d riga %d testo '%s'", layer, r, chat_input(a));
    tap(a, PAD_B);
    tap(a, PAD_L2);

    /* senza chiave non parte niente */
    chat_text(a, "Qual è la capitale d'Italia?", TEXT_CHARS);
    tap(a, PAD_START);
    CHECK(!chat_busy(a) && strstr(chat_toast(a), "chiave"), "senza chiave: %s", chat_toast(a));
    chat_set_key(a, "sk-ant-prova-0000000000000000000000");
    chat_set_offline(a, 1);
    tap(a, PAD_START);
    CHECK(chat_busy(a) && chat_conversation(a)->n == 1 && !strcmp(chat_input(a), ""), "invio");
    chat_feed_reply(a, START("claude-opus-5-5", "420") THINK("0", "SIG1") TEXT_START("1") TEXT_DELTA("1", "Roma è la capitale"), 0);
    SHOT("chat-2-arriva");
    chat_feed_reply(a, TEXT_DELTA("1", " d\\u2019Italia.\\n\\n## Qualche dato\\n- **abitanti:** circa 2,7 milioni\\n- fiume: il Tevere\\n\\n```\\nprint(\\\"Roma\\\")\\n```") STOP("1") END("end_turn", "57"), 1);
    Conversation *cv2 = chat_conversation(a);
    CHECK(!chat_busy(a) && cv2->n == 2 && cv2->msg[1].role == ROLE_ASSISTANT && !cv2->msg[1].excluded, "risposta in cronologia");
    CHECK(cv2->out_tokens == 57 && cv2->cost > 0.0, "consumo: %ld token, %f $", cv2->out_tokens, cv2->cost);
    SHOT("chat-3-lettura");

    /* rifiuto: resta da leggere ma non torna all'API */
    chat_set_view(a, PAGE_CHAT, 1);
    chat_text(a, "Domanda delicata", TEXT_CHARS);
    chat_text(a, "", TEXT_ENTER);
    chat_feed_reply(a, REFUSAL, 1);
    CHECK(cv2->n == 3 && cv2->msg[2].excluded && cv2->msg[2].note && strstr(cv2->msg[2].note, "cyber"), "rifiuto escluso con nota");
    /* errore 401: idem; Y riprova con un nuovo messaggio */
    chat_text(a, "Ancora una", TEXT_CHARS);
    chat_text(a, "", TEXT_ENTER);
    chat_feed_reply(a, HTTP401, 1);
    CHECK(cv2->n == 4 && cv2->msg[3].excluded && strstr(cv2->msg[3].note, "Chiave"), "errore 401 con nota");
    chat_set_view(a, PAGE_CHAT, 0);
    tap(a, PAD_Y);
    CHECK(chat_busy(a) && cv2->n == 5 && !strcmp(cv2->msg[4].text, "Ancora una"), "riprova");
    chat_feed_reply(a, START("claude-opus-5-5", "500") TEXT_START("0") TEXT_DELTA("0", "Eccomi.") STOP("0") END("end_turn", "5"), 1);
    char *body = conv_request(cv2, MODEL_OPUS, EFFORT_MEDIUM);
    JNode *j = json_parse(body, strlen(body));
    const JNode *msgs = json_get(j, "messages");
    int n = 0;
    for (const JNode *x = msgs ? msgs->child : NULL; x; x = x->next) n++;
    CHECK(n == 4, "nella richiesta solo gli scambi riusciti: %d", n);
    json_free(j);
    free(body);

    /* domanda veloce durante una risposta: rifiutata con un messaggio */
    tap(a, PAD_X);
    tap(a, PAD_A);
    CHECK(chat_busy(a), "domanda veloce inviata");
    tap(a, PAD_B);
    CHECK(!chat_busy(a) && cv2->msg[cv2->n - 1].excluded, "B ferma la risposta");
    SHOT("chat-4-errori");

    chat_set_view(a, PAGE_CHAT, 1);
    SHOT("chat-5-scrivi");
    chat_set_view(a, PAGE_SETTINGS, 0);
    SHOT("chat-6-impostazioni");
    tap(a, PAD_DOWN); tap(a, PAD_DOWN); tap(a, PAD_DOWN); tap(a, PAD_DOWN);
    tap(a, PAD_A);                                /* Chiave API: inserimento */
    chat_text(a, "sk-ant-api03-abcdefghijklmnopqrstuvwxyz", TEXT_CHARS);
    SHOT("chat-7-chiave");
    tap(a, PAD_START);
    FILE *f = fopen(key_path, "rb");
    char kb[128] = "";
    if (f) { size_t got = fread(kb, 1, sizeof(kb) - 1, f); kb[got] = 0; fclose(f); }
    CHECK(!strcmp(kb, "sk-ant-api03-abcdefghijklmnopqrstuvwxyz"), "chiave salvata: %s", kb);
    chat_set_view(a, PAGE_CHAT, 0);
    tap(a, PAD_X);
    SHOT("chat-8-veloci");
    tap(a, PAD_B);
    tap(a, PAD_MENU);
    SHOT("chat-9-uscita");
    tap(a, PAD_B);

    /* la conversazione si ritrova alla riapertura */
    int before = cv2->n;
    chat_destroy(a);
    a = chat_create(48000.0f, state);
    CHECK(chat_conversation(a)->n == before, "conversazione ripresa: %d su %d", chat_conversation(a)->n, before);
    chat_destroy(a);
    free(px);
}

int main(int argc, char **argv)
{
    const char *fonts = argc > 1 ? argv[1] : ".", *outdir = argc > 2 ? argv[2] : ".";
    char f1[512], f2[512];
    snprintf(f1, sizeof(f1), "%s/BeVietnamPro-Regular.ttf", fonts);
    snprintf(f2, sizeof(f2), "%s/BeVietnamPro-SemiBold.ttf", fonts);
    if (gfx_load_fonts(f1, f2)) return 1;
    test_json();
    printf("JSON: %d controlli, %d errori\n", checks, fails);
    test_streams();
    printf("stream: %d controlli, %d errori\n", checks, fails);
    test_conversation();
    printf("cronologia: %d controlli, %d errori\n", checks, fails);
    test_ui(outdir);
    printf("interfaccia: %d controlli, %d errori\n", checks, fails);
    printf("TOTALE: %d controlli, %d errori\n", checks, fails);
    gfx_free_fonts();
    return fails ? 1 : 0;
}
