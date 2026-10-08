/* Chiedi a Claude per Miyoo Flip - interfaccia (vedi chat_app.h).
 *
 * Due modi nella pagina della chat: si scrive con la tastiera a schermo (croce per scegliere il tasto,
 * A per premerlo, B cancella, Y spazio, X maiuscole, L2 simboli, START invia) oppure si legge
 * (croce per scorrere, A per tornare a scrivere, B ferma la risposta). SELECT passa tra chat, lettura e
 * impostazioni. Una tastiera USB collegata alla console scrive direttamente.
 *
 * File sulla SD, in Saves/claude: chiave.txt (la chiave API, la mette l'utente), conversazione.json,
 * impostazioni.txt, archivio/ (le conversazioni chiuse, in testo semplice).
 */
#include "chat_app.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "platform.h"

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#define MKDIR(p) mkdir(p, 0755)
#endif

/* ------------------------------------------------------------------ palette (come le altre app Cirmolo) */
#define C_BG_TOP   RGB(13, 16, 22)
#define C_BG_BOT   RGB(18, 23, 32)
#define C_BAR      RGB(16, 20, 28)
#define C_PANEL    RGB(25, 31, 42)
#define C_PANEL_HI RGB(34, 42, 57)
#define C_LINE     RGB(48, 58, 78)
#define C_TEXT     RGB(236, 236, 244)
#define C_MUTED    RGB(138, 148, 170)
#define C_DIM      RGB(88, 98, 120)
#define C_GOLD     RGB(242, 196, 92)
#define C_GOLD_D   RGB(112, 84, 34)
#define C_VIOLET   RGB(150, 116, 242)
#define C_VIOLET_D RGB(70, 52, 140)
#define C_GREEN    RGB(92, 190, 126)
#define C_RED      RGB(236, 96, 104)
#define C_CODE     RGB(20, 24, 33)

/* ------------------------------------------------------------------ geometria */
#define HEADER_H   47
#define CHAT_Y0    50
#define TYPE_CHAT_Y1 214
#define INPUT_Y    218
#define INPUT_H    50
#define HINT_Y     287
#define KB_Y       294
#define KB_ROW     37
#define READ_CHAT_Y1 444
#define BUBBLE_PAD 9
#define MAX_INPUT  4000

/* ------------------------------------------------------------------ tastiera a schermo */
enum { KS_CHAR, KS_SHIFT, KS_LAYER, KS_SPACE, KS_BACK, KS_SEND };
typedef struct { const char *t; int w, sp; } KeyDef;           /* w in mezze unita': 20 per riga */
#define KROWS 5
#define KCOLS 10
static const KeyDef KB[2][KROWS][KCOLS] = {
    {
        { { "1", 2 }, { "2", 2 }, { "3", 2 }, { "4", 2 }, { "5", 2 }, { "6", 2 }, { "7", 2 }, { "8", 2 }, { "9", 2 }, { "0", 2 } },
        { { "q", 2 }, { "w", 2 }, { "e", 2 }, { "r", 2 }, { "t", 2 }, { "y", 2 }, { "u", 2 }, { "i", 2 }, { "o", 2 }, { "p", 2 } },
        { { "a", 2 }, { "s", 2 }, { "d", 2 }, { "f", 2 }, { "g", 2 }, { "h", 2 }, { "j", 2 }, { "k", 2 }, { "l", 2 }, { "'", 2 } },
        { { "z", 2 }, { "x", 2 }, { "c", 2 }, { "v", 2 }, { "b", 2 }, { "n", 2 }, { "m", 2 }, { ",", 2 }, { ".", 2 }, { "?", 2 } },
        { { "Maiusc", 3, KS_SHIFT }, { "àè?!", 3, KS_LAYER }, { "spazio", 8, KS_SPACE }, { "Cancella", 3, KS_BACK }, { "Invia", 3, KS_SEND } },
    },
    {
        { { "à", 2 }, { "è", 2 }, { "é", 2 }, { "ì", 2 }, { "ò", 2 }, { "ù", 2 }, { "!", 2 }, { "\"", 2 }, { ":", 2 }, { ";", 2 } },
        { { "@", 2 }, { "#", 2 }, { "€", 2 }, { "%", 2 }, { "&", 2 }, { "*", 2 }, { "(", 2 }, { ")", 2 }, { "-", 2 }, { "_", 2 } },
        { { "+", 2 }, { "=", 2 }, { "/", 2 }, { "\\", 2 }, { "<", 2 }, { ">", 2 }, { "[", 2 }, { "]", 2 }, { "{", 2 }, { "}", 2 } },
        { { "~", 2 }, { "^", 2 }, { "|", 2 }, { "°", 2 }, { "«", 2 }, { "»", 2 }, { "…", 2 }, { "$", 2 }, { "£", 2 }, { "ç", 2 } },
        { { "Maiusc", 3, KS_SHIFT }, { "abc", 3, KS_LAYER }, { "spazio", 8, KS_SPACE }, { "Cancella", 3, KS_BACK }, { "Invia", 3, KS_SEND } },
    },
};

static int row_len(int layer, int r)
{
    int n = 0;
    while (n < KCOLS && KB[layer][r][n].t) n++;
    return n;
}

static float key_x(int layer, int r, int c, float *w)
{
    const float unit = 636.0f / 20.0f;
    float x = 2.0f;
    for (int i = 0; i < c; i++) x += KB[layer][r][i].w * unit;
    if (w) *w = KB[layer][r][c].w * unit;
    return x;
}

/* maiuscole delle lettere accentate della tastiera */
static const char *upper_of(const char *t, char *buf)
{
    static const char *pairs[][2] = { { "à", "À" }, { "è", "È" }, { "é", "É" }, { "ì", "Ì" }, { "ò", "Ò" }, { "ù", "Ù" }, { "ç", "Ç" } };
    for (size_t i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++) if (!strcmp(t, pairs[i][0])) return pairs[i][1];
    if (t[0] && !t[1] && islower((unsigned char)t[0])) { buf[0] = (char)toupper((unsigned char)t[0]); buf[1] = 0; return buf; }
    return t;
}

static const char *QUICK[] = {
    "Continua", "Spiegamelo in modo più semplice", "Fammi un esempio", "Riassumi in tre punti", "Traducilo in inglese", "Grazie!",
};
#define QUICK_COUNT ((int)(sizeof(QUICK) / sizeof(QUICK[0])))

/* ------------------------------------------------------------------ impaginazione dei messaggi */
enum { ST_NORMAL, ST_HEAD, ST_BULLET, ST_QUOTE, ST_CODE, ST_RULE, ST_BLANK };
typedef struct { int start, len, style, indent, w; } WLine;
typedef struct {
    Buf disp;                     /* testo da mostrare, senza i segni del Markdown */
    WLine *line;
    int n, cap;
    int width, maxw, height;      /* larghezza a cui e' stato spezzato, riga piu' larga, altezza */
    size_t src_len;
    const char *src;
} Wrap;

static int style_font(int st) { return st == ST_HEAD ? FONT_BOLD : (st == ST_CODE ? FONT_SMALL : FONT_BODY); }
static int style_height(int st) { return st == ST_HEAD ? 27 : (st == ST_CODE ? 20 : (st == ST_BLANK ? 9 : (st == ST_RULE ? 12 : 23))); }

static int width_n(int font, const char *s, int n)
{
    char tmp[512];
    if (n > (int)sizeof(tmp) - 1) {
        n = (int)sizeof(tmp) - 1;
        while (n > 0 && ((unsigned char)s[n] & 0xC0) == 0x80) n--;
    }
    memcpy(tmp, s, (size_t)n);
    tmp[n] = 0;
    return gfx_text_width(font, tmp);
}

static void add_line(Wrap *w, int start, int len, int style, int indent, int width)
{
    if (w->n == w->cap) {
        w->cap = w->cap ? w->cap * 2 : 32;
        WLine *l = realloc(w->line, sizeof(WLine) * (size_t)w->cap);
        if (!l) abort();
        w->line = l;
    }
    w->line[w->n++] = (WLine){ start, len, style, indent, width };
    if (width + indent > w->maxw) w->maxw = width + indent;
    w->height += style_height(style);
}

/* Spezza il paragrafo [start, end) del testo da mostrare in righe larghe al massimo avail. */
static void wrap_para(Wrap *w, int start, int end, int style, int first_indent, int indent, int avail)
{
    const char *s = w->disp.p;
    int font = style_font(style);
    if (start >= end) { add_line(w, start, 0, style, first_indent, 0); return; }
    int ls = start, lw = 0, pos = start, cur_indent = first_indent;
    while (pos < end) {
        int ws = pos, we = pos;
        while (we < end && s[we] == ' ') we++;
        while (we < end && s[we] != ' ') we++;
        int ww = width_n(font, s + ws, we - ws);
        if (lw + ww <= avail - cur_indent) { lw += ww; pos = we; continue; }
        if (ws > ls) {                               /* la parola va a capo */
            add_line(w, ls, ws - ls, style, cur_indent, lw);
            cur_indent = indent;
            while (ws < end && s[ws] == ' ') ws++;
            ls = pos = ws;
            lw = 0;
            continue;
        }
        /* parola piu' lunga della riga: si spezza per caratteri */
        int cut = ws, cw = 0;
        while (cut < we) {
            int nx = cut + 1;
            while (nx < we && ((unsigned char)s[nx] & 0xC0) == 0x80) nx++;
            int chw = width_n(font, s + cut, nx - cut);
            if (cw + chw > avail - cur_indent && cut > ws) break;
            cw += chw;
            cut = nx;
        }
        add_line(w, ls, cut - ls, style, cur_indent, cw);
        cur_indent = indent;
        ls = pos = cut;
        lw = 0;
    }
    if (pos > ls || ls == start) add_line(w, ls, pos - ls, style, cur_indent, lw);
}

/* Copia una riga di testo togliendo i segni del Markdown in linea (grassetto, codice). */
static void add_inline(Buf *d, const char *p, int len)
{
    for (int i = 0; i < len; i++) {
        if ((p[i] == '*' || p[i] == '_') && i + 1 < len && p[i + 1] == p[i]) { i++; continue; }
        if (p[i] == '`') continue;
        buf_add(d, p + i, 1);
    }
}

static void wrap_text(Wrap *w, const char *src, int avail)
{
    buf_clear(&w->disp);
    buf_add(&w->disp, "", 0);
    w->n = w->maxw = w->height = 0;
    w->width = avail;
    w->src = src;
    w->src_len = strlen(src);
    int in_code = 0;
    const char *p = src;
    while (*p) {
        const char *e = strchr(p, '\n');
        int len = e ? (int)(e - p) : (int)strlen(p);
        const char *q = p;
        int qlen = len;
        while (qlen > 0 && (q[qlen - 1] == ' ' || q[qlen - 1] == '\r')) qlen--;
        int lead = 0;
        while (lead < qlen && q[lead] == ' ') lead++;
        int start = (int)w->disp.len;
        if (qlen - lead >= 3 && !strncmp(q + lead, "```", 3)) {
            in_code = !in_code;
        } else if (in_code) {
            buf_add(&w->disp, q, (size_t)qlen);
            wrap_para(w, start, (int)w->disp.len, ST_CODE, 8, 8, avail);
        } else if (qlen == lead) {
            if (w->n && w->line[w->n - 1].style != ST_BLANK) add_line(w, start, 0, ST_BLANK, 0, 0);
        } else if (qlen - lead >= 3 && (!strncmp(q + lead, "---", 3) || !strncmp(q + lead, "***", 3)) && strspn(q + lead, "-*_ ") >= (size_t)(qlen - lead)) {
            add_line(w, start, 0, ST_RULE, 0, 0);
        } else if (q[lead] == '#') {
            int h = lead;
            while (h < qlen && q[h] == '#') h++;
            while (h < qlen && q[h] == ' ') h++;
            add_inline(&w->disp, q + h, qlen - h);
            wrap_para(w, start, (int)w->disp.len, ST_HEAD, 0, 0, avail);
        } else if (qlen - lead >= 2 && (q[lead] == '-' || q[lead] == '*' || q[lead] == '+') && q[lead + 1] == ' ') {
            int ind = lead * 6;
            buf_adds(&w->disp, "\xe2\x80\xa2 ");
            add_inline(&w->disp, q + lead + 2, qlen - lead - 2);
            wrap_para(w, start, (int)w->disp.len, ST_BULLET, ind, ind + 16, avail);
        } else if (q[lead] == '>') {
            int h = lead + 1;
            while (h < qlen && q[h] == ' ') h++;
            add_inline(&w->disp, q + h, qlen - h);
            wrap_para(w, start, (int)w->disp.len, ST_QUOTE, 12, 12, avail);
        } else {
            int num = lead;
            while (num < qlen && isdigit((unsigned char)q[num])) num++;
            int numbered = num > lead && num + 1 < qlen && (q[num] == '.' || q[num] == ')') && q[num + 1] == ' ';
            add_inline(&w->disp, q + lead, qlen - lead);
            wrap_para(w, start, (int)w->disp.len, ST_NORMAL, lead * 6, numbered ? lead * 6 + 20 : lead * 6, avail);
        }
        p = e ? e + 1 : p + len;
    }
    while (w->n && w->line[w->n - 1].style == ST_BLANK) { w->height -= style_height(ST_BLANK); w->n--; }
}

static void wrap_free(Wrap *w) { buf_free(&w->disp); free(w->line); memset(w, 0, sizeof(*w)); }

/* Testo semplice (il messaggio in scrittura): gli offset restano quelli del testo di partenza. */
static void wrap_plain(Wrap *w, const char *src, int avail)
{
    buf_clear(&w->disp);
    buf_adds(&w->disp, src);
    w->n = w->maxw = w->height = 0;
    w->width = avail;
    w->src = src;
    w->src_len = strlen(src);
    int start = 0, len = (int)w->disp.len;
    for (int i = 0; i <= len; i++)
        if (i == len || w->disp.p[i] == '\n') { wrap_para(w, start, i, ST_NORMAL, 0, 0, avail); start = i + 1; }
}

/* ------------------------------------------------------------------ stato */
struct ChatApp {
    float sr;
    char dir[400], settings_path[440];
    Conversation conv;
    Reply reply;
    Transfer xfer;
    int busy, offline;
    char key[256];
    int model, effort, concise;

    int page, typing, quit_dialog, quit, quick_open, quick_sel, confirm_new, entering_key;
    int set_sel;
    int down[PAD_COUNT];
    float rep[PAD_COUNT];
    float time;

    Buf input;
    int cursor, layer, shift, kr, kc;

    float scroll;
    int follow;
    Wrap *wraps;
    int nwraps;
    Wrap live;
    char *live_text;

    char toast[200];
    float toast_t;
    uint32_t drawn_key;
    float drawn_time;

    /* suoni: tasto e risposta pronta */
    int clicks, chimes, clicks_seen, chimes_seen;
    float click_t, chime_t;
    uint32_t rng;
};

static void toast(ChatApp *a, const char *msg)
{
    snprintf(a->toast, sizeof(a->toast), "%s", msg);
    a->toast_t = 2.6f;
}

static char *dupstr(const char *s)
{
    size_t n = strlen(s);
    char *p = malloc(n + 1);
    if (!p) abort();
    memcpy(p, s, n + 1);
    return p;
}

static void click(ChatApp *a) { __atomic_add_fetch(&a->clicks, 1, __ATOMIC_RELEASE); }

/* ------------------------------------------------------------------ file */
static char *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    Buf b = { 0 };
    char tmp[4096];
    size_t n;
    while ((n = fread(tmp, 1, sizeof(tmp), f)) > 0) buf_add(&b, tmp, n);
    fclose(f);
    if (len) *len = b.len;
    if (!b.p) buf_add(&b, "", 0);
    return buf_steal(&b);
}

static int write_file(const char *path, const char *data, size_t len)
{
    char tmp[520];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return -1;
    int ok = fwrite(data, 1, len, f) == len;
    ok &= fclose(f) == 0;
    if (!ok) { remove(tmp); return -1; }
#ifdef _WIN32
    remove(path);
#endif
    return rename(tmp, path);
}

static void path_in(const ChatApp *a, char *out, size_t n, const char *name) { snprintf(out, n, "%s/%s", a->dir, name); }

static void save_settings(ChatApp *a)
{
    char t[200];
    int n = snprintf(t, sizeof(t), "# Chiedi a Claude\nmodel=%d\neffort=%d\nconcise=%d\n", a->model, a->effort, a->concise);
    write_file(a->settings_path, t, (size_t)n);
}

static void load_settings(ChatApp *a)
{
    char *s = read_file(a->settings_path, NULL);
    if (!s) return;
    for (char *line = strtok(s, "\n"); line; line = strtok(NULL, "\n")) {
        int v;
        if (sscanf(line, "model=%d", &v) == 1 && v >= 0 && v < MODEL_COUNT) a->model = v;
        else if (sscanf(line, "effort=%d", &v) == 1 && v >= 0 && v < EFFORT_COUNT) a->effort = v;
        else if (sscanf(line, "concise=%d", &v) == 1) a->concise = v != 0;
    }
    free(s);
}

/* La chiave: solo lettere, cifre, '-' e '_' (niente a capo o spazi nell'intestazione HTTP). */
static int clean_key(const char *in, char *out, size_t n)
{
    size_t k = 0;
    for (const char *p = in; *p; p++) {
        if (isspace((unsigned char)*p)) continue;
        if (!isalnum((unsigned char)*p) && *p != '-' && *p != '_') return -1;
        if (k + 1 >= n) return -1;
        out[k++] = *p;
    }
    out[k] = 0;
    return k >= 20 ? 0 : -1;
}

static void load_key(ChatApp *a)
{
    char path[440];
    path_in(a, path, sizeof(path), "chiave.txt");
    char *s = read_file(path, NULL);
    a->key[0] = 0;
    if (s && clean_key(s, a->key, sizeof(a->key))) a->key[0] = 0;
    free(s);
}

static void save_conversation(ChatApp *a)
{
    char path[440];
    path_in(a, path, sizeof(path), "conversazione.json");
    char *s = conv_save(&a->conv);
    write_file(path, s, strlen(s));
    free(s);
}

static char *system_prompt(int concise)
{
    static const char *DAYS[7] = { "domenica", "lunedì", "martedì", "mercoledì", "giovedì", "venerdì", "sabato" };
    static const char *MONTHS[12] = { "gennaio", "febbraio", "marzo", "aprile", "maggio", "giugno", "luglio",
                                      "agosto", "settembre", "ottobre", "novembre", "dicembre" };
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    Buf b = { 0 };
    buf_adds(&b, "Stai parlando con una persona attraverso «Chiedi a Claude», un'app per la console portatile "
                 "Miyoo Flip: lo schermo è piccolo (640x480) e si scrive con una tastiera a schermo, quindi i "
                 "messaggi che ricevi possono essere brevi o avere qualche errore di battitura.");
    if (tm) buf_printf(&b, " Oggi è %s %d %s %d.", DAYS[tm->tm_wday], tm->tm_mday, MONTHS[tm->tm_mon], tm->tm_year + 1900);
    buf_adds(&b, " Rispondi nella lingua di chi scrive, di solito l'italiano.");
    if (concise) buf_adds(&b, " Preferisci risposte brevi e dirette, con paragrafi corti; usa elenchi semplici solo quando aiutano.");
    buf_adds(&b, " Evita tabelle ed emoji: lo schermo mostra il Markdown solo in parte (titoli, elenchi, grassetto e codice).");
    return buf_steal(&b);
}

static void new_conversation(ChatApp *a)
{
    char *sys = system_prompt(a->concise);
    conv_free(&a->conv);
    conv_init(&a->conv, sys);
    free(sys);
    for (int i = 0; i < a->nwraps; i++) wrap_free(&a->wraps[i]);
    free(a->wraps);
    a->wraps = NULL;
    a->nwraps = 0;
    a->scroll = 0;
    a->follow = 1;
}

/* Archivio in testo semplice, leggibile dal PC: Saves/claude/archivio/AAAA-MM-GG_HHMM.txt */
static void archive_conversation(ChatApp *a)
{
    if (!a->conv.n) return;
    char dir[440], path[480];
    path_in(a, dir, sizeof(dir), "archivio");
    MKDIR(dir);
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char stamp[32] = "conversazione";
    if (tm) strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H%M%S", tm);
    snprintf(path, sizeof(path), "%s/%s.txt", dir, stamp);
    Buf b = { 0 };
    for (int i = 0; i < a->conv.n; i++) {
        const ChatMsg *m = &a->conv.msg[i];
        buf_printf(&b, "%s%s\n%s\n", m->role == ROLE_USER ? "Tu" : "Claude", m->excluded ? " (non inviato)" : "", m->text ? m->text : "");
        if (m->note) buf_printf(&b, "[%s]\n", m->note);
        buf_adds(&b, "\n");
    }
    buf_printf(&b, "Token: %ld in ingresso, %ld in uscita. Costo stimato: %.4f $\n", a->conv.in_tokens + a->conv.cache_read + a->conv.cache_write, a->conv.out_tokens, a->conv.cost);
    write_file(path, b.p ? b.p : "", b.len);
    buf_free(&b);
}

static void load_conversation(ChatApp *a)
{
    char path[440];
    path_in(a, path, sizeof(path), "conversazione.json");
    size_t len = 0;
    char *s = read_file(path, &len);
    if (!s || conv_load(&a->conv, s, len)) { free(s); new_conversation(a); return; }
    free(s);
    /* una domanda rimasta senza risposta (app chiusa durante l'attesa) non si rimanda */
    if (a->conv.n && a->conv.msg[a->conv.n - 1].role == ROLE_USER && !a->conv.msg[a->conv.n - 1].excluded) {
        ChatMsg *m = &a->conv.msg[a->conv.n - 1];
        m->excluded = 1;
        free(m->note);
        m->note = dupstr("Senza risposta: l'app è stata chiusa.");
    }
}

/* ------------------------------------------------------------------ invio e risposte */
static void cost_add(ChatApp *a, const Reply *r)
{
    const ModelInfo *m = &CLAUDE_MODELS[a->model];
    for (int i = 0; i < MODEL_COUNT; i++) if (!strcmp(r->model, CLAUDE_MODELS[i].id)) m = &CLAUDE_MODELS[i];
    a->conv.in_tokens += r->in_tokens;
    a->conv.out_tokens += r->out_tokens;
    a->conv.cache_read += r->cache_read;
    a->conv.cache_write += r->cache_write;
    a->conv.cost += (r->in_tokens * m->in + r->out_tokens * m->out + r->cache_write * m->cache_write + r->cache_read * m->cache_read) / 1e6;
}

static void finish_reply(ChatApp *a)
{
    Reply *r = &a->reply;
    a->busy = 0;
    ChatMsg *user = NULL;
    for (int i = a->conv.n - 1; i >= 0; i--) if (a->conv.msg[i].role == ROLE_USER) { user = &a->conv.msg[i]; break; }
    char *json = r->state == RS_DONE ? reply_message_json(r) : NULL;
    char *text = reply_text(r);
    if (r->model[0]) cost_add(a, r);
    if (json) {
        ChatMsg *m = conv_add(&a->conv, ROLE_ASSISTANT, json, text);
        if (r->fallback || (r->model[0] && strcmp(r->model, CLAUDE_MODELS[a->model].id))) {
            char note[120];
            snprintf(note, sizeof(note), "Ha risposto %s.", claude_model_name(r->model));
            m->note = dupstr(note);
        }
        __atomic_add_fetch(&a->chimes, 1, __ATOMIC_RELEASE);
    } else {
        /* scambio non riuscito: resta da leggere ma non torna all'API */
        char note[360];
        switch (r->state) {
        case RS_REFUSED:
            snprintf(note, sizeof(note), "Claude non ha risposto a questa richiesta%s%s%s.", r->category[0] ? " (categoria: " : "", r->category, r->category[0] ? ")" : "");
            break;
        case RS_CANCELLED: snprintf(note, sizeof(note), "Risposta interrotta."); break;
        case RS_DONE: snprintf(note, sizeof(note), "Nessuna risposta."); break;
        default: snprintf(note, sizeof(note), "%s", r->error[0] ? r->error : "Errore sconosciuto.");
        }
        if (user) user->excluded = 1;
        if (text[0]) {
            ChatMsg *m = conv_add(&a->conv, ROLE_ASSISTANT, dupstr("{\"role\":\"assistant\",\"content\":[]}"), text);
            m->excluded = 1;
            m->note = dupstr(note);
        } else {
            free(text);
            if (user) { free(user->note); user->note = dupstr(note); }
        }
        if (r->state != RS_CANCELLED) toast(a, note);
    }
    free(a->live_text);
    a->live_text = NULL;
    wrap_free(&a->live);
    reply_free(&a->reply);
    save_conversation(a);
}

static void trim(char *s)
{
    size_t n = strlen(s), i = 0;
    while (i < n && isspace((unsigned char)s[i])) i++;
    while (n > i && isspace((unsigned char)s[n - 1])) n--;
    memmove(s, s + i, n - i);
    s[n - i] = 0;
}

static int send_text(ChatApp *a, const char *text_in)
{
    if (a->busy) { toast(a, "Aspetta la fine della risposta (B la ferma)."); return -1; }
    char *text = dupstr(text_in);
    trim(text);
    if (!text[0]) { free(text); return -1; }
    if (!a->key[0] && !a->offline) {
        toast(a, "Manca la chiave API: guarda nelle impostazioni (SELECT).");
        free(text);
        return -1;
    }
    conv_add_user(&a->conv, text);
    free(text);
    reply_free(&a->reply);
    a->reply.state = RS_WAITING;
    a->busy = 1;
    a->follow = 1;
    a->typing = 0;
    if (!a->offline) {
        char *body = conv_request(&a->conv, a->model, a->effort);
        char msg[200];
        if (transfer_start(&a->xfer, a->key, conv_beta(a->model), body, msg, sizeof(msg))) {
            a->reply.state = RS_ERROR;
            snprintf(a->reply.error, sizeof(a->reply.error), "%s", msg);
            finish_reply(a);
        }
        free(body);
    }
    save_conversation(a);
    return 0;
}

static void send_input(ChatApp *a)
{
    if (!a->input.len) { toast(a, "Scrivi qualcosa prima di inviare."); return; }
    if (send_text(a, a->input.p) == 0) { buf_clear(&a->input); a->cursor = 0; }
}

static void cancel_reply(ChatApp *a)
{
    if (!a->busy) return;
    if (!a->offline) transfer_cancel(&a->xfer);
    a->reply.state = RS_CANCELLED;
    finish_reply(a);
    toast(a, "Risposta interrotta.");
}

static void retry_last(ChatApp *a)
{
    if (a->busy) return;
    for (int i = a->conv.n - 1; i >= 0; i--) {
        ChatMsg *m = &a->conv.msg[i];
        if (m->role != ROLE_USER) continue;
        if (!m->excluded) { toast(a, "L'ultima domanda ha già una risposta."); return; }
        char *t = dupstr(m->text);
        send_text(a, t);
        free(t);
        return;
    }
    toast(a, "Niente da riprovare.");
}

/* ------------------------------------------------------------------ scrittura */
static void input_insert(ChatApp *a, const char *t)
{
    size_t n = strlen(t);
    if (a->input.len + n > MAX_INPUT) { toast(a, "Messaggio troppo lungo."); return; }
    buf_add(&a->input, "", 0);
    buf_add(&a->input, t, n);                     /* spazio in fondo, poi si sposta */
    memmove(a->input.p + a->cursor + n, a->input.p + a->cursor, a->input.len - n - (size_t)a->cursor);
    memcpy(a->input.p + a->cursor, t, n);
    a->cursor += (int)n;
}

static int u8_prev(const char *s, int pos)
{
    if (pos <= 0) return 0;
    pos--;
    while (pos > 0 && ((unsigned char)s[pos] & 0xC0) == 0x80) pos--;
    return pos;
}

static int u8_next(const char *s, int len, int pos)
{
    if (pos >= len) return len;
    pos++;
    while (pos < len && ((unsigned char)s[pos] & 0xC0) == 0x80) pos++;
    return pos;
}

static void input_backspace(ChatApp *a)
{
    if (a->cursor <= 0) return;
    int p = u8_prev(a->input.p, a->cursor);
    memmove(a->input.p + p, a->input.p + a->cursor, a->input.len - (size_t)a->cursor);
    a->input.len -= (size_t)(a->cursor - p);
    a->input.p[a->input.len] = 0;
    a->cursor = p;
}

static void input_delete(ChatApp *a)
{
    if (a->cursor >= (int)a->input.len) return;
    int nx = u8_next(a->input.p, (int)a->input.len, a->cursor);
    memmove(a->input.p + a->cursor, a->input.p + nx, a->input.len - (size_t)nx);
    a->input.len -= (size_t)(nx - a->cursor);
    a->input.p[a->input.len] = 0;
}

/* maiuscola automatica all'inizio e dopo la fine di una frase */
static void auto_shift(ChatApp *a)
{
    if (a->shift == 2 || a->entering_key) return;
    const char *s = a->input.p;
    int c = a->cursor;
    if (c == 0) { a->shift = 1; return; }
    if (c >= 2 && s[c - 1] == ' ' && (s[c - 2] == '.' || s[c - 2] == '!' || s[c - 2] == '?')) a->shift = 1;
}

static void press_key(ChatApp *a)
{
    const KeyDef *k = &KB[a->layer][a->kr][a->kc];
    char tmp[4];
    switch (k->sp) {
    case KS_SHIFT: a->shift = (a->shift + 1) % 3; break;
    case KS_LAYER: a->layer = !a->layer; break;
    case KS_SPACE: input_insert(a, " "); auto_shift(a); break;
    case KS_BACK: input_backspace(a); break;
    case KS_SEND: return;                         /* gestito da chi chiama */
    default:
        input_insert(a, a->shift ? upper_of(k->t, tmp) : k->t);
        if (a->shift == 1) a->shift = 0;
        auto_shift(a);
    }
    click(a);
}

static void finish_key_entry(ChatApp *a, int save)
{
    if (save) {
        char clean[256];
        if (clean_key(a->input.p ? a->input.p : "", clean, sizeof(clean))) { toast(a, "Chiave non valida: contiene caratteri strani o è troppo corta."); return; }
        char path[440];
        path_in(a, path, sizeof(path), "chiave.txt");
        if (write_file(path, clean, strlen(clean))) { toast(a, "Impossibile salvare la chiave sulla SD."); return; }
        snprintf(a->key, sizeof(a->key), "%s", clean);
        toast(a, "Chiave salvata in Saves/claude/chiave.txt.");
    }
    memset(a->input.p ? a->input.p : (char *)"", 0, a->input.len);   /* la chiave non resta in memoria */
    buf_clear(&a->input);
    a->cursor = 0;
    a->entering_key = 0;
    a->typing = 0;
    a->page = PAGE_SETTINGS;
}

static void move_key(ChatApp *a, int dr, int dc)
{
    if (dc) {
        int n = row_len(a->layer, a->kr);
        a->kc = (a->kc + dc + n) % n;
        return;
    }
    float w, cx = key_x(a->layer, a->kr, a->kc, &w) + w / 2;
    a->kr = (a->kr + dr + KROWS) % KROWS;
    int best = 0;
    float bd = 1e9f;
    for (int c = 0; c < row_len(a->layer, a->kr); c++) {
        float kw, kx = key_x(a->layer, a->kr, c, &kw) + kw / 2;
        if (fabsf(kx - cx) < bd) { bd = fabsf(kx - cx); best = c; }
    }
    a->kc = best;
}

/* ------------------------------------------------------------------ tasti */
enum { S_MODEL, S_EFFORT, S_STYLE, S_NEW, S_KEY, S_USAGE, S_COUNT };

static void settings_change(ChatApp *a, int dir)
{
    switch (a->set_sel) {
    case S_MODEL: a->model = (a->model + dir + MODEL_COUNT) % MODEL_COUNT; break;
    case S_EFFORT: a->effort = (a->effort + dir + EFFORT_COUNT) % EFFORT_COUNT; break;
    case S_STYLE: a->concise = !a->concise; toast(a, "Vale dalla prossima conversazione."); break;
    default: return;
    }
    save_settings(a);
}

static void scroll_by(ChatApp *a, float dy)
{
    a->scroll += dy;
    if (a->scroll < 0) a->scroll = 0;
    a->follow = 0;                                /* chat_draw lo riattiva se si torna in fondo */
}

static int repeats(const ChatApp *a, int b)
{
    if (a->quit_dialog || a->confirm_new) return 0;
    if (b == PAD_UP || b == PAD_DOWN || b == PAD_LEFT || b == PAD_RIGHT) return 1;
    if (a->page == PAGE_CHAT && a->typing) return b == PAD_B || b == PAD_L1 || b == PAD_R1 ||
        (b == PAD_A && KB[a->layer][a->kr][a->kc].sp == KS_BACK);
    return 0;
}

static void press(ChatApp *a, int b)
{
    if (a->quit_dialog) {
        if (b == PAD_A || b == PAD_DOWN) { a->quit = 1; }
        else if (b == PAD_B || b == PAD_UP || b == PAD_MENU) a->quit_dialog = 0;
        return;
    }
    if (a->confirm_new) {
        if (b == PAD_A) {
            if (a->busy) cancel_reply(a);
            archive_conversation(a);
            new_conversation(a);
            save_conversation(a);
            toast(a, "Conversazione archiviata in Saves/claude/archivio.");
            a->page = PAGE_CHAT;
            a->typing = 1;
        }
        if (b == PAD_A || b == PAD_B) a->confirm_new = 0;
        return;
    }
    if (a->quick_open) {
        switch (b) {
        case PAD_UP: a->quick_sel = (a->quick_sel + QUICK_COUNT - 1) % QUICK_COUNT; break;
        case PAD_DOWN: a->quick_sel = (a->quick_sel + 1) % QUICK_COUNT; break;
        case PAD_A: a->quick_open = 0; send_text(a, QUICK[a->quick_sel]); break;
        case PAD_B: case PAD_X: case PAD_R2: a->quick_open = 0; break;
        }
        return;
    }
    if (b == PAD_MENU) { a->quit_dialog = 1; return; }

    if (a->page == PAGE_SETTINGS) {
        switch (b) {
        case PAD_UP: a->set_sel = (a->set_sel + S_COUNT - 1) % S_COUNT; break;
        case PAD_DOWN: a->set_sel = (a->set_sel + 1) % S_COUNT; break;
        case PAD_LEFT: settings_change(a, -1); break;
        case PAD_RIGHT: settings_change(a, 1); break;
        case PAD_A:
            if (a->set_sel == S_NEW) a->confirm_new = 1;
            else if (a->set_sel == S_KEY) {
                a->entering_key = 1;
                a->page = PAGE_CHAT;
                a->typing = 1;
                buf_clear(&a->input);
                a->cursor = 0;
                a->layer = 0;
                a->shift = 0;
            } else settings_change(a, 1);
            break;
        case PAD_SELECT: case PAD_B: a->page = PAGE_CHAT; a->typing = 0; break;
        }
        return;
    }

    if (a->typing) {
        switch (b) {
        case PAD_UP: move_key(a, -1, 0); break;
        case PAD_DOWN: move_key(a, 1, 0); break;
        case PAD_LEFT: move_key(a, 0, -1); break;
        case PAD_RIGHT: move_key(a, 0, 1); break;
        case PAD_A:
            if (KB[a->layer][a->kr][a->kc].sp == KS_SEND) {
                if (a->entering_key) finish_key_entry(a, 1);
                else send_input(a);
            } else press_key(a);
            break;
        case PAD_B: input_backspace(a); click(a); break;
        case PAD_Y: input_insert(a, " "); auto_shift(a); click(a); break;
        case PAD_X: a->shift = (a->shift + 1) % 3; break;
        case PAD_L1: a->cursor = u8_prev(a->input.p ? a->input.p : "", a->cursor); break;
        case PAD_R1: a->cursor = u8_next(a->input.p ? a->input.p : "", (int)a->input.len, a->cursor); break;
        case PAD_L2: a->layer = !a->layer; break;
        case PAD_R2: if (!a->entering_key) { a->quick_open = 1; a->quick_sel = 0; } break;
        case PAD_START:
            if (a->entering_key) finish_key_entry(a, 1);
            else send_input(a);
            break;
        case PAD_SELECT:
            if (a->entering_key) finish_key_entry(a, 0);
            else a->typing = 0;
            break;
        }
        return;
    }

    switch (b) {                                  /* lettura */
    case PAD_UP: scroll_by(a, -46); break;
    case PAD_DOWN: scroll_by(a, 46); break;
    case PAD_LEFT: scroll_by(a, -300); break;
    case PAD_RIGHT: scroll_by(a, 300); break;
    case PAD_A: case PAD_START: a->typing = 1; auto_shift(a); break;
    case PAD_B: if (a->busy) cancel_reply(a); break;
    case PAD_X: case PAD_R2: a->quick_open = 1; a->quick_sel = 0; break;
    case PAD_Y: retry_last(a); break;
    case PAD_SELECT: a->page = PAGE_SETTINGS; break;
    case PAD_L1: a->scroll = 0; a->follow = 0; break;
    case PAD_R1: a->follow = 1; break;
    }
}

void chat_button(ChatApp *a, int b, int pressed)
{
    if (b < 0 || b >= PAD_COUNT) return;
    if (pressed) {
        if (a->down[b]) return;
        a->down[b] = 1;
        a->rep[b] = 0.0f;
        press(a, b);
    } else {
        a->down[b] = 0;
    }
}

void chat_axes(ChatApp *a, float lx, float ly, float rx, float ry, float l2, float r2)
{
    (void)lx; (void)rx; (void)l2; (void)r2;
    float v = fabsf(ry) > 0.25f ? ry : (fabsf(ly) > 0.25f ? ly : 0.0f);
    if (v != 0.0f && a->page == PAGE_CHAT) scroll_by(a, v * 9.0f);   /* le levette scorrono la chat */
}

void chat_text(ChatApp *a, const char *utf8, int special)
{
    if (a->quit_dialog || a->confirm_new || a->quick_open) return;
    if (a->page != PAGE_CHAT) return;
    a->typing = 1;
    switch (special) {
    case TEXT_CHARS:
        for (const char *p = utf8; *p; p++) if ((unsigned char)*p < 0x20) return;
        input_insert(a, utf8);
        if (a->shift == 1) a->shift = 0;
        break;
    case TEXT_BACKSPACE: input_backspace(a); break;
    case TEXT_DELETE: input_delete(a); break;
    case TEXT_LEFT: a->cursor = u8_prev(a->input.p ? a->input.p : "", a->cursor); break;
    case TEXT_RIGHT: a->cursor = u8_next(a->input.p ? a->input.p : "", (int)a->input.len, a->cursor); break;
    case TEXT_UP: scroll_by(a, -46); break;
    case TEXT_DOWN: scroll_by(a, 46); break;
    case TEXT_ENTER:
        if (a->entering_key) finish_key_entry(a, 1);
        else send_input(a);
        break;
    }
}

/* ------------------------------------------------------------------ API */
ChatApp *chat_create(float sample_rate, const char *state_path)
{
    ChatApp *a = calloc(1, sizeof(ChatApp));
    if (!a) return NULL;
    a->sr = sample_rate;
    a->rng = 0x9E3779B9u;
    snprintf(a->settings_path, sizeof(a->settings_path), "%s", state_path ? state_path : "impostazioni.txt");
    snprintf(a->dir, sizeof(a->dir), "%s", a->settings_path);
    char *slash = strrchr(a->dir, '/');
    if (!slash) slash = strrchr(a->dir, '\\');
    if (slash) *slash = 0;
    else snprintf(a->dir, sizeof(a->dir), ".");
    MKDIR(a->dir);
    a->model = MODEL_OPUS;
    a->effort = EFFORT_MEDIUM;
    a->concise = 1;
    a->follow = 1;
    a->xfer.pid = -1;
    reply_init(&a->reply);
    load_settings(a);
    load_key(a);
    load_conversation(a);
    a->typing = 1;
    a->kr = 1;
    auto_shift(a);
    return a;
}

void chat_destroy(ChatApp *a)
{
    if (!a) return;
    if (a->busy) cancel_reply(a);
    save_conversation(a);
    for (int i = 0; i < a->nwraps; i++) wrap_free(&a->wraps[i]);
    free(a->wraps);
    wrap_free(&a->live);
    free(a->live_text);
    conv_free(&a->conv);
    reply_free(&a->reply);
    if (a->input.p) memset(a->input.p, 0, a->input.len);
    buf_free(&a->input);
    memset(a->key, 0, sizeof(a->key));
    free(a);
}

/* Suoni discreti: un ticchettio sui tasti e due note quando la risposta e' pronta. */
void chat_audio(ChatApp *a, float *out, int frames)
{
    int clicks = __atomic_load_n(&a->clicks, __ATOMIC_ACQUIRE), chimes = __atomic_load_n(&a->chimes, __ATOMIC_ACQUIRE);
    if (clicks != a->clicks_seen) { a->clicks_seen = clicks; a->click_t = 0.0f; }
    if (chimes != a->chimes_seen) { a->chimes_seen = chimes; a->chime_t = 0.0f; }
    const float inv = 1.0f / a->sr;
    for (int i = 0; i < frames; i++) {
        float s = 0.0f;
        if (a->click_t < 0.03f) {
            a->rng ^= a->rng << 13; a->rng ^= a->rng >> 17; a->rng ^= a->rng << 5;
            s += (float)(int32_t)a->rng * (1.0f / 2147483648.0f) * expf(-a->click_t / 0.004f) * 0.08f;
            a->click_t += inv;
        }
        if (a->chime_t < 0.6f) {
            float t = a->chime_t, f = t < 0.12f ? 659.25f : 880.0f, tt = t < 0.12f ? t : t - 0.12f;
            s += sinf(6.2831853f * f * tt) * expf(-tt / 0.12f) * 0.12f;
            a->chime_t += inv;
        }
        out[2 * i] = out[2 * i + 1] = s;
    }
}

void chat_update(ChatApp *a, float dt)
{
    a->time += dt;
    for (int b = 0; b < PAD_COUNT; b++) {
        if (!a->down[b] || !repeats(a, b)) continue;
        a->rep[b] += dt;
        while (a->rep[b] > 0.38f) { a->rep[b] -= 0.06f; press(a, b); }
    }
    if (a->busy && !a->offline && transfer_poll(&a->xfer, &a->reply)) finish_reply(a);
    a->toast_t = fmaxf(0.0f, a->toast_t - dt);
}

int chat_wants_quit(const ChatApp *a) { return a->quit; }

/* prove */
void chat_set_offline(ChatApp *a, int offline) { a->offline = offline; }
Conversation *chat_conversation(ChatApp *a) { return &a->conv; }
const char *chat_input(const ChatApp *a) { return a->input.p ? a->input.p : ""; }
int chat_busy(const ChatApp *a) { return a->busy; }
const char *chat_toast(const ChatApp *a) { return a->toast_t > 0.0f ? a->toast : ""; }
void chat_set_view(ChatApp *a, int page, int typing) { a->page = page; a->typing = typing; }
void chat_set_key(ChatApp *a, const char *key) { snprintf(a->key, sizeof(a->key), "%s", key ? key : ""); }
void chat_osk_pos(const ChatApp *a, int *layer, int *row, int *col) { *layer = a->layer; *row = a->kr; *col = a->kc; }

void chat_feed_reply(ChatApp *a, const char *sse, int finish)
{
    if (!a->busy) return;
    reply_feed(&a->reply, sse, strlen(sse));
    if (finish) { reply_finish(&a->reply, 0, ""); finish_reply(a); }
}

/* ------------------------------------------------------------------ disegno */
static uint32_t fnv(uint32_t h, const void *p, size_t n)
{
    const unsigned char *b = p;
    while (n--) { h ^= *b++; h *= 16777619u; }
    return h;
}

int chat_needs_draw(ChatApp *a)
{
    int v[] = { a->page, a->typing, a->quit_dialog, a->quick_open, a->quick_sel, a->confirm_new, a->entering_key, a->set_sel,
                a->cursor, a->layer, a->shift, a->kr, a->kc, a->busy, a->reply.state, a->conv.n, a->model, a->effort, a->concise,
                (int)a->scroll, a->follow, a->toast_t > 0.0f, a->key[0] != 0,
                a->busy ? (int)(a->time * 3.0f) : 0,
                a->typing ? (int)(a->time * 2.0f) : 0 };
    uint32_t h = fnv(2166136261u, v, sizeof(v));
    if (a->input.len) h = fnv(h, a->input.p, a->input.len);
    if (a->busy) for (int i = 0; i < a->reply.nblk; i++) h = fnv(h, &a->reply.blk[i].a.len, sizeof(size_t));
    if (a->toast_t > 0.0f) return 1;
    if (h == a->drawn_key && a->time - a->drawn_time < 1.0f) return 0;
    a->drawn_key = h;
    a->drawn_time = a->time;
    return 1;
}

static void draw_logo(Canvas *c, float x, float y)
{
    gfx_round_rect(c, x, y, 26, 19, 7, C_GOLD, 1.0f);
    gfx_line(c, x + 6, y + 17, x + 3, y + 25, 3.0f, C_GOLD, 1.0f);
    for (int i = 0; i < 3; i++) gfx_circle(c, x + 7 + i * 6, y + 9.5f, 2.0f, C_BAR, 1.0f);
}

static void draw_header(ChatApp *a, Canvas *c)
{
    gfx_rect(c, 0, 0, c->w, HEADER_H - 1, C_BAR);
    gfx_rect(c, 0, HEADER_H - 1, c->w, 1, C_LINE);
    draw_logo(c, 14, 10);
    gfx_text(c, FONT_TITLE, 50, 32, "Chiedi a Claude", C_TEXT);
    const char *model = CLAUDE_MODELS[a->model].name + 7;           /* senza "Claude " */
    uint32_t dot = !a->key[0] ? C_RED : (a->busy ? C_GOLD : C_GREEN);
    int w = gfx_text_width(FONT_BOLD, model);
    gfx_text_right(c, FONT_BOLD, 626, 30, model, C_MUTED);
    gfx_circle(c, 626 - w - 14, 24, 5.0f, dot, a->busy ? 0.5f + 0.5f * sinf(a->time * 6.0f) : 1.0f);
}

static Wrap *wrap_for(ChatApp *a, int i, int avail)
{
    if (a->nwraps < a->conv.n) {
        Wrap *w = realloc(a->wraps, sizeof(Wrap) * (size_t)a->conv.n);
        if (!w) abort();
        memset(w + a->nwraps, 0, sizeof(Wrap) * (size_t)(a->conv.n - a->nwraps));
        a->wraps = w;
        a->nwraps = a->conv.n;
    }
    Wrap *w = &a->wraps[i];
    const char *t = a->conv.msg[i].text ? a->conv.msg[i].text : "";
    if (w->src != t || w->src_len != strlen(t) || w->width != avail) wrap_text(w, t, avail);
    return w;
}

static void draw_text_line(Canvas *c, const Wrap *w, const WLine *l, int x, int y, uint32_t col)
{
    char tmp[1024];
    int n = l->len < (int)sizeof(tmp) - 1 ? l->len : (int)sizeof(tmp) - 1;
    memcpy(tmp, w->disp.p + l->start, (size_t)n);
    tmp[n] = 0;
    gfx_text(c, style_font(l->style), x + l->indent, y, tmp, col);
}

/* Altezza di un messaggio (bolla e nota). */
static int msg_height(const Wrap *w, const char *note) { return w->height + 2 * BUBBLE_PAD + (note ? 22 : 0) + 10; }

static void draw_bubble(Canvas *c, const Wrap *w, int user, int y, int dim, const char *note, int note_red)
{
    int bw = w->maxw + 2 * BUBBLE_PAD + 4, bh = w->height + 2 * BUBBLE_PAD;
    if (bw < 40) bw = 40;
    int x = user ? 626 - bw : 14;
    uint32_t fill = user ? (dim ? RGB(48, 40, 82) : C_VIOLET_D) : C_PANEL;
    gfx_round_rect(c, x, y, bw, bh, 12, fill, 1.0f);
    int ly = y + BUBBLE_PAD;
    for (int i = 0; i < w->n; i++) {
        const WLine *l = &w->line[i];
        int h = style_height(l->style);
        if (ly + h >= CHAT_Y0 - 30 && ly <= READ_CHAT_Y1 + 30) {
            if (l->style == ST_CODE) gfx_rect(c, x + 6, ly, bw - 12, h, C_CODE);
            if (l->style == ST_QUOTE) gfx_rect(c, x + BUBBLE_PAD + 2, ly + 2, 3, h - 4, C_LINE);
            if (l->style == ST_RULE) gfx_rect(c, x + BUBBLE_PAD, ly + h / 2, bw - 2 * BUBBLE_PAD, 1, C_LINE);
            else if (l->style != ST_BLANK) {
                uint32_t col = dim ? C_DIM : (l->style == ST_HEAD ? C_GOLD : (l->style == ST_CODE ? RGB(180, 220, 190) : C_TEXT));
                draw_text_line(c, w, l, x + BUBBLE_PAD + 2, ly + h - 6, col);
            }
        }
        ly += h;
    }
    if (note) {
        int nx = user ? 626 : 18;
        if (user) gfx_text_right(c, FONT_SMALL, nx, y + bh + 17, note, note_red ? C_RED : C_DIM);
        else gfx_text(c, FONT_SMALL, nx, y + bh + 17, note, note_red ? C_RED : C_DIM);
    }
}

static void draw_welcome(ChatApp *a, Canvas *c, int y0)
{
    gfx_round_rect(c, 14, y0 + 6, 612, 150, 14, C_PANEL, 1.0f);
    gfx_text(c, FONT_BOLD, 30, y0 + 36, "Ciao! Scrivi una domanda e premi START.", C_TEXT);
    if (a->key[0]) {
        gfx_text(c, FONT_SMALL, 30, y0 + 64, "La tastiera: croce per scegliere, A per scrivere, B cancella, Y spazio.", C_MUTED);
        gfx_text(c, FONT_SMALL, 30, y0 + 86, "R2 apre le domande veloci, SELECT passa alla lettura e alle impostazioni.", C_MUTED);
        gfx_text(c, FONT_SMALL, 30, y0 + 108, "Una tastiera USB collegata prima di aprire l'app scrive direttamente.", C_MUTED);
        char t[120];
        snprintf(t, sizeof(t), "Modello: %s, impegno %s. Serve il Wi-Fi.", CLAUDE_MODELS[a->model].name,
                 a->effort == EFFORT_LOW ? "basso" : (a->effort == EFFORT_MEDIUM ? "medio" : "alto"));
        gfx_text(c, FONT_SMALL, 30, y0 + 130, t, C_DIM);
    } else {
        gfx_text(c, FONT_SMALL, 30, y0 + 64, "Manca la chiave API di Anthropic. Due modi per metterla:", C_GOLD);
        gfx_text(c, FONT_SMALL, 30, y0 + 86, "1. dal PC, nel file Saves/claude/chiave.txt della scheda SD;", C_MUTED);
        gfx_text(c, FONT_SMALL, 30, y0 + 108, "2. qui: SELECT due volte, Chiave API, A, poi scrivila e START.", C_MUTED);
        gfx_text(c, FONT_SMALL, 30, y0 + 130, "La crei su console.anthropic.com; meglio una chiave con un limite di spesa.", C_DIM);
    }
}

static void draw_chat(ChatApp *a, Canvas *c, int y0, int y1)
{
    const int avail_user = 440, avail_claude = 560;
    int total = 8;
    for (int i = 0; i < a->conv.n; i++) {
        const ChatMsg *m = &a->conv.msg[i];
        total += msg_height(wrap_for(a, i, m->role == ROLE_USER ? avail_user : avail_claude), m->note);
    }
    char *live = NULL;
    if (a->busy) {
        live = reply_text(&a->reply);
        if (!a->live_text || strcmp(live, a->live_text)) {
            free(a->live_text);
            a->live_text = live;
            wrap_text(&a->live, a->live_text, avail_claude);
        } else {
            free(live);
        }
        total += msg_height(&a->live, "x");
    }
    int view = y1 - y0, max_scroll = total > view ? total - view : 0;
    if (a->follow || a->scroll > max_scroll) a->scroll = (float)max_scroll;
    if (a->scroll >= max_scroll - 1) a->follow = 1;
    if (!a->conv.n && !a->busy) { draw_welcome(a, c, y0); return; }
    int y = y0 + 8 - (int)a->scroll;
    for (int i = 0; i < a->conv.n; i++) {
        const ChatMsg *m = &a->conv.msg[i];
        Wrap *w = wrap_for(a, i, m->role == ROLE_USER ? avail_user : avail_claude);
        int h = msg_height(w, m->note);
        if (y + h >= y0 - 10 && y <= y1 + 10)
            draw_bubble(c, w, m->role == ROLE_USER, y, m->excluded, m->note, m->excluded && m->note && strncmp(m->note, "Risposta interrotta", 19));
        y += h;
    }
    if (a->busy) {
        char status[80];
        int dots = (int)(a->time * 3.0f) % 4;
        snprintf(status, sizeof(status), "%s%.*s", reply_status(&a->reply), dots, "...");
        if (a->live.n) draw_bubble(c, &a->live, 0, y, 0, status, 0);
        else {
            gfx_round_rect(c, 14, y, 200, 2 * BUBBLE_PAD + 22, 12, C_PANEL, 1.0f);
            gfx_text(c, FONT_BODY, 14 + BUBBLE_PAD + 4, y + BUBBLE_PAD + 17, status, C_MUTED);
        }
    }
    if (max_scroll > 0) {                         /* barra di scorrimento */
        float h = (float)view * view / total, sy = y0 + (float)(view - h) * a->scroll / max_scroll;
        gfx_round_rect(c, 632, sy, 4, h, 2, C_LINE, 1.0f);
    }
}

static void draw_input(ChatApp *a, Canvas *c)
{
    gfx_round_rect(c, 8, INPUT_Y, 624, INPUT_H, 12, C_PANEL_HI, 1.0f);
    gfx_round_frame(c, 8, INPUT_Y, 624, INPUT_H, 12, 1.5f, a->entering_key ? C_GOLD : C_LINE, 1.0f);
    /* il testo (o la chiave, mascherata) su due righe: si vede la parte con il cursore */
    Buf shown = { 0 };
    int cur = a->cursor;
    if (a->entering_key) {
        int n = 0;
        for (size_t i = 0; i < a->input.len; i++) n++;
        for (int i = 0; i < n; i++) buf_adds(&shown, i >= n - 4 ? (char[]){ a->input.p[i], 0 } : "\xe2\x80\xa2");
        cur = (int)shown.len;
    } else {
        buf_add(&shown, a->input.p ? a->input.p : "", a->input.len);
    }
    if (!shown.len && !a->entering_key) {
        gfx_text(c, FONT_BODY, 22, INPUT_Y + 31, a->busy ? "Claude sta rispondendo..." : "Scrivi qui il messaggio", C_DIM);
    }
    Wrap w = { 0 };
    wrap_plain(&w, shown.p ? shown.p : "", 590);
    int cl = 0;
    for (int i = 0; i < w.n; i++) if (w.line[i].start <= cur) cl = i;
    int first = cl > 0 ? cl - 1 : 0;
    for (int i = first; i < w.n && i < first + 2; i++) {
        const WLine *l = &w.line[i];
        int y = INPUT_Y + 21 + (i - first) * 22;
        draw_text_line(c, &w, l, 18, y, a->entering_key ? C_GOLD : C_TEXT);
        if (i == cl && (int)(a->time * 2.0f) % 2 == 0) {
            int off = cur - l->start;
            if (off > l->len) off = l->len;
            int cx = 18 + l->indent + width_n(style_font(l->style), w.disp.p + l->start, off < 0 ? 0 : off);
            gfx_rect(c, cx, y - 15, 2, 19, C_GOLD);
        }
    }
    if (!w.n && (int)(a->time * 2.0f) % 2 == 0) gfx_rect(c, 18, INPUT_Y + 6, 2, 19, C_GOLD);
    wrap_free(&w);
    buf_free(&shown);
    if (a->entering_key) gfx_text_right(c, FONT_SMALL, 620, INPUT_Y + 44, "chiave API: START salva, SELECT annulla", C_GOLD);
}

static void draw_keyboard(ChatApp *a, Canvas *c)
{
    gfx_rect(c, 0, HINT_Y - 18, c->w, c->h - HINT_Y + 18, C_BAR);
    gfx_text_center(c, FONT_SMALL, 320, HINT_Y, "A tasto · B cancella · Y spazio · X maiuscole · L2 simboli · L1/R1 cursore · START invia · SELECT leggi", C_DIM);
    for (int r = 0; r < KROWS; r++)
        for (int k = 0; k < row_len(a->layer, r); k++) {
            const KeyDef *d = &KB[a->layer][r][k];
            float w, x = key_x(a->layer, r, k, &w), y = KB_Y + r * KB_ROW;
            int sel = r == a->kr && k == a->kc;
            uint32_t fill = sel ? C_GOLD : (d->sp ? C_PANEL_HI : C_PANEL);
            if (d->sp == KS_SHIFT && a->shift) fill = sel ? C_GOLD : (a->shift == 2 ? C_VIOLET : C_VIOLET_D);
            if (d->sp == KS_SEND && !sel) fill = C_GOLD_D;
            gfx_round_rect(c, x + 2, y, w - 4, KB_ROW - 4, 7, fill, 1.0f);
            char tmp[4];
            const char *label = d->sp ? d->t : (a->shift ? upper_of(d->t, tmp) : d->t);
            int font = d->sp ? FONT_SMALL : FONT_BOLD;
            if (d->sp == KS_SHIFT && a->shift == 2) label = "BLOCCO";
            if (d->sp == KS_SEND && a->entering_key) label = "Salva";
            gfx_text_center(c, font, (int)(x + w / 2), (int)(y + (d->sp ? 22 : 23)), label, sel ? C_BAR : (d->sp ? C_MUTED : C_TEXT));
        }
}

static void draw_hints(Canvas *c, const char *text)
{
    gfx_rect(c, 0, 446, c->w, 34, C_BAR);
    gfx_rect(c, 0, 446, c->w, 1, C_LINE);
    gfx_text_center(c, FONT_SMALL, c->w / 2, 468, text, C_MUTED);
}

static void draw_settings(ChatApp *a, Canvas *c)
{
    static const char *LABELS[S_COUNT] = { "Modello", "Impegno", "Stile delle risposte", "Nuova conversazione", "Chiave API", "Consumo" };
    static const char *EFFORT_NAMES[EFFORT_COUNT] = { "Basso", "Medio", "Alto" };
    char vals[S_COUNT][96];
    snprintf(vals[S_MODEL], sizeof(vals[0]), "%s", CLAUDE_MODELS[a->model].name);
    snprintf(vals[S_EFFORT], sizeof(vals[0]), "%s", EFFORT_NAMES[a->effort]);
    snprintf(vals[S_STYLE], sizeof(vals[0]), "%s", a->concise ? "Brevi" : "Normali");
    snprintf(vals[S_NEW], sizeof(vals[0]), "A: archivia e ricomincia");
    if (a->key[0]) snprintf(vals[S_KEY], sizeof(vals[0]), "presente (...%s)", a->key + strlen(a->key) - 4);
    else snprintf(vals[S_KEY], sizeof(vals[0]), "mancante");
    long tok = a->conv.in_tokens + a->conv.cache_read + a->conv.cache_write + a->conv.out_tokens;
    snprintf(vals[S_USAGE], sizeof(vals[0]), "%ld token, circa %.3f $", tok, a->conv.cost);
    for (char *p = vals[S_USAGE]; *p; p++) if (*p == '.') *p = ',';
    for (int i = 0; i < S_COUNT; i++) {
        int y = 60 + i * 42;
        if (i == a->set_sel) gfx_round_rect(c, 14, y, 612, 36, 10, C_PANEL_HI, 1.0f);
        gfx_text(c, FONT_BODY, 32, y + 24, LABELS[i], i == a->set_sel ? C_TEXT : C_MUTED);
        uint32_t col = i == a->set_sel ? C_GOLD : C_TEXT;
        if (i == S_KEY && !a->key[0]) col = C_RED;
        gfx_text_right(c, FONT_BOLD, 610, y + 24, vals[i], col);
    }
    static const char *HELP[S_COUNT][2] = {
        { "Opus 5.5 è il più capace; Sonnet 5.5 costa la metà;", "Haiku 5.5 è il più veloce ed economico." },
        { "Quanto Claude ragiona prima di rispondere: più alto vuol dire", "risposte più accurate, ma più lente e costose." },
        { "Brevi: risposte corte e dirette, adatte allo schermo piccolo.", "Vale dalla prossima conversazione." },
        { "Salva la chat in Saves/claude/archivio come testo", "e ne inizia una nuova (anche per risparmiare token)." },
        { "Il file Saves/claude/chiave.txt sulla SD (console.anthropic.com).", "A: scrivila con la tastiera. Meglio una chiave con limite di spesa." },
        { "Stima con i prezzi di listino: i ragionamenti di Claude", "contano come token in uscita anche se non si vedono." },
    };
    gfx_round_rect(c, 14, 324, 612, 110, 14, C_PANEL, 1.0f);
    gfx_text(c, FONT_SMALL, 30, 356, HELP[a->set_sel][0], C_MUTED);
    gfx_text(c, FONT_SMALL, 30, 380, HELP[a->set_sel][1], C_MUTED);
    gfx_text(c, FONT_SMALL, 30, 416, "Le richieste usano il ripiego automatico di Anthropic se un modello rifiuta.", C_DIM);
    draw_hints(c, "Su/giù sceglie · sinistra/destra cambia · A attiva · SELECT o B torna alla chat");
}

static void draw_dialog(Canvas *c, const char *title, const char *body, const char *keys)
{
    gfx_rect_alpha(c, 0, 0, c->w, c->h, RGB(0, 0, 0), 0.6f);
    gfx_round_rect(c, 110, 170, 420, 140, 18, C_PANEL_HI, 1.0f);
    gfx_round_frame(c, 110, 170, 420, 140, 18, 2, C_GOLD, 1.0f);
    gfx_text_center(c, FONT_TITLE, 320, 222, title, C_TEXT);
    gfx_text_center(c, FONT_BODY, 320, 256, body, C_MUTED);
    gfx_text_center(c, FONT_BOLD, 320, 290, keys, C_GOLD);
}

static void draw_quick(ChatApp *a, Canvas *c)
{
    gfx_rect_alpha(c, 0, 0, c->w, c->h, RGB(0, 0, 0), 0.55f);
    int h = 60 + QUICK_COUNT * 34;
    gfx_round_rect(c, 120, 240 - h / 2, 400, h, 16, C_PANEL_HI, 1.0f);
    gfx_round_frame(c, 120, 240 - h / 2, 400, h, 16, 2, C_GOLD, 1.0f);
    gfx_text_center(c, FONT_BOLD, 320, 240 - h / 2 + 30, "Domande veloci", C_GOLD);
    for (int i = 0; i < QUICK_COUNT; i++) {
        int y = 240 - h / 2 + 44 + i * 34;
        if (i == a->quick_sel) gfx_round_rect(c, 132, y, 376, 30, 9, C_GOLD_D, 1.0f);
        gfx_text(c, FONT_BODY, 148, y + 21, QUICK[i], i == a->quick_sel ? C_TEXT : C_MUTED);
    }
}

void chat_draw(ChatApp *a, Canvas *c)
{
    gfx_vgradient(c, 0, 0, c->w, c->h, C_BG_TOP, C_BG_BOT);
    if (a->page == PAGE_SETTINGS) {
        draw_header(a, c);
        draw_settings(a, c);
    } else if (a->typing) {
        draw_chat(a, c, CHAT_Y0, TYPE_CHAT_Y1);
        gfx_vgradient(c, 0, TYPE_CHAT_Y1, c->w, INPUT_Y - TYPE_CHAT_Y1, C_BG_BOT, C_BG_BOT);
        draw_header(a, c);
        draw_input(a, c);
        draw_keyboard(a, c);
    } else {
        draw_chat(a, c, CHAT_Y0, READ_CHAT_Y1);
        draw_header(a, c);
        draw_hints(c, a->busy ? "B ferma la risposta · su/giù scorre · A scrivi · X domande veloci"
                              : "A scrivi · su/giù scorre (sinistra/destra a pagine) · X domande veloci · Y riprova · SELECT impostazioni");
    }
    if (a->quick_open) draw_quick(a, c);
    if (a->toast_t > 0.0f && a->toast[0]) {
        float al = fminf(1.0f, a->toast_t * 3.0f);
        int w = gfx_text_width(FONT_BOLD, a->toast) + 36;
        if (w > 624) w = 624;
        int ty = a->page == PAGE_CHAT && a->typing ? 168 : 398;
        gfx_round_rect(c, 320 - w / 2.0f, ty, w, 36, 18, C_BAR, 0.94f * al);
        gfx_round_frame(c, 320 - w / 2.0f, ty, w, 36, 18, 1.5f, C_GOLD, al);
        gfx_text_center(c, FONT_BOLD, 320, ty + 24, a->toast, gfx_mix(C_BAR, C_TEXT, al));
    }
    if (a->confirm_new) draw_dialog(c, "Nuova conversazione?", "Questa finisce nell'archivio sulla SD.", "A: sì   ·   B: no");
    if (a->quit_dialog) draw_dialog(c, "Uscire da Chiedi a Claude?", "La conversazione resta salvata.", "A o giù: esci   ·   B o su: resta");
}
