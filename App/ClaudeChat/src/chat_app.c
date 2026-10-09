/* Chiedi all'IA per Miyoo Flip - interfaccia (vedi chat_app.h).
 *
 * Due modi nella pagina della chat: si scrive con la tastiera a schermo (croce per scegliere il tasto,
 * A per premerlo, B cancella, Y spazio, X maiuscole, L2 simboli, START invia) oppure si legge
 * (croce per scorrere, A per tornare a scrivere, B ferma la risposta o la lettura). SELECT passa tra chat,
 * lettura e impostazioni (due schede, Chat e Voce, con L1/R1). Una tastiera USB scrive direttamente.
 * R2 tenuto premuto registra dal microfono (USB o cuffie Bluetooth): al rilascio la voce diventa testo e
 * parte la domanda; le risposte si possono far leggere ad alta voce. R2 premuto e lasciato subito apre
 * le domande veloci.
 *
 * File sulla SD, in Saves/claude: chiavi/<fornitore>.txt (le chiavi API), conversazione.json,
 * impostazioni.txt, fornitori.json (facoltativo), modelli/ (elenchi scaricati), archivio/ (le
 * conversazioni chiuse, in testo semplice).
 * Tutti i testi passano da tr(): l'italiano e' la lingua di partenza, le traduzioni stanno in lang/.
 */
#include "chat_app.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "i18n.h"
#include "platform.h"
#include "providers.h"
#include "voice.h"

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <unistd.h>
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
        { { N_("Maiusc"), 3, KS_SHIFT }, { N_("àè?!"), 3, KS_LAYER }, { N_("spazio"), 8, KS_SPACE }, { N_("Cancella"), 3, KS_BACK }, { N_("Invia"), 3, KS_SEND } },
    },
    {
        { { "à", 2 }, { "è", 2 }, { "é", 2 }, { "ì", 2 }, { "ò", 2 }, { "ù", 2 }, { "!", 2 }, { "\"", 2 }, { ":", 2 }, { ";", 2 } },
        { { "@", 2 }, { "#", 2 }, { "€", 2 }, { "%", 2 }, { "&", 2 }, { "*", 2 }, { "(", 2 }, { ")", 2 }, { "-", 2 }, { "_", 2 } },
        { { "+", 2 }, { "=", 2 }, { "/", 2 }, { "\\", 2 }, { "<", 2 }, { ">", 2 }, { "[", 2 }, { "]", 2 }, { "{", 2 }, { "}", 2 } },
        { { "~", 2 }, { "^", 2 }, { "|", 2 }, { "°", 2 }, { "«", 2 }, { "»", 2 }, { "…", 2 }, { "$", 2 }, { "£", 2 }, { "ç", 2 } },
        { { N_("Maiusc"), 3, KS_SHIFT }, { N_("abc"), 3, KS_LAYER }, { N_("spazio"), 8, KS_SPACE }, { N_("Cancella"), 3, KS_BACK }, { N_("Invia"), 3, KS_SEND } },
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
    N_("Continua"), N_("Spiegamelo in modo più semplice"), N_("Fammi un esempio"), N_("Riassumi in tre punti"), N_("Traducilo in inglese"), N_("Grazie!"),
    N_("Rileggi l'ultima risposta ad alta voce"),
};
#define QUICK_COUNT ((int)(sizeof(QUICK) / sizeof(QUICK[0])))
#define QUICK_SPEAK (QUICK_COUNT - 1)                 /* non si invia: legge l'ultima risposta */

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
enum { V_IDLE, V_HOLD, V_RECORDING, V_TRANSCRIBING };
enum { MIC_USB, MIC_BT };
enum { TAB_CHAT, TAB_VOICE };
static const char *const EFFORTS[3] = { "low", "medium", "high" };

typedef struct { char model[96]; char voice[32]; } ProvPrefs;

struct ChatApp {
    float sr;
    char dir[400], settings_path[440];
    Registry reg;
    ProvPrefs *prefs;                             /* modello e voce scelti per ogni fornitore */
    int prov, model;                              /* fornitore e modello in uso */
    char key[256];                                /* chiave del fornitore in uso */
    int effort, concise;

    Conversation conv;
    Reply reply;
    Transfer xfer;
    int busy, offline, reply_prov, reply_model, retried;

    Transfer mxfer;                               /* elenco dei modelli dal fornitore */
    Buf mbody;
    int mfetch;

    int page, tab, typing, quit_dialog, quit, quick_open, quick_sel, confirm_new, entering_key, key_target;
    int picker_open, picker_sel, picker_top;
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

    /* voce */
    int speak, autosend, mic, stt_prov, tts_prov;
    Recorder rec;
    Stt stt;
    Tts tts;
    Player pl;
    int vstate;
    float hold_t;
    volatile float cap_rate;
    char cap_name[96];
    char *speak_text;
    size_t speak_pos, speak_src_len;
    int speak_final, speak_first, speaking;

    /* suoni: tasto, risposta pronta, inizio e fine registrazione */
    int clicks, chimes, beeps, clicks_seen, chimes_seen, beeps_seen, beep_up;
    float click_t, chime_t, beep_t;
    uint32_t rng;
};

static void input_insert(ChatApp *a, const char *t);

static void toast(ChatApp *a, const char *msg)
{
    snprintf(a->toast, sizeof(a->toast), "%s", msg);
    a->toast_t = 2.8f;
}

static void toastf(ChatApp *a, const char *fmt, const char *arg)
{
    char t[200];
    snprintf(t, sizeof(t), fmt, arg);
    toast(a, t);
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
static void beep(ChatApp *a, int up) { a->beep_up = up; __atomic_add_fetch(&a->beeps, 1, __ATOMIC_RELEASE); }

static Provider *prov_at(ChatApp *a, int i) { return i >= 0 && i < a->reg.n ? &a->reg.p[i] : NULL; }
static Provider *cur_prov(ChatApp *a) { return &a->reg.p[a->prov]; }
static Model *cur_model(ChatApp *a)
{
    Provider *p = cur_prov(a);
    return a->model >= 0 && a->model < p->nmodels ? &p->models[a->model] : NULL;
}
static const char *cur_model_id(ChatApp *a) { Model *m = cur_model(a); return m ? m->id : ""; }

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
    Buf b = { 0 };
    buf_printf(&b, "# Chiedi all'IA\nprovider=%s\neffort=%d\nconcise=%d\nspeak=%d\nautosend=%d\nmic=%d\nstt=%s\ntts=%s\n",
               cur_prov(a)->id, a->effort, a->concise, a->speak, a->autosend, a->mic,
               a->stt_prov >= 0 ? a->reg.p[a->stt_prov].id : "none", a->tts_prov >= 0 ? a->reg.p[a->tts_prov].id : "none");
    for (int i = 0; i < a->reg.n; i++) {
        if (a->prefs[i].model[0]) buf_printf(&b, "model.%s=%s\n", a->reg.p[i].id, a->prefs[i].model);
        if (a->prefs[i].voice[0]) buf_printf(&b, "voice.%s=%s\n", a->reg.p[i].id, a->prefs[i].voice);
    }
    write_file(a->settings_path, b.p, b.len);
    buf_free(&b);
}

static void load_key(ChatApp *a) { provider_key(cur_prov(a), a->dir, a->key, sizeof(a->key)); }

static int has_key(ChatApp *a, int prov)
{
    Provider *p = prov_at(a, prov);
    if (!p) return 0;
    if (!p->needs_key) return 1;
    char k[256];
    provider_key(p, a->dir, k, sizeof(k));
    int ok = k[0] != 0;
    memset(k, 0, sizeof(k));
    return ok;
}

/* Sceglie il fornitore e il suo ultimo modello. */
static void set_provider(ChatApp *a, int i)
{
    if (i < 0 || i >= a->reg.n) return;
    a->prov = i;
    Provider *p = cur_prov(a);
    int m = a->prefs[i].model[0] ? provider_find_model(p, a->prefs[i].model) : -1;
    a->model = m >= 0 ? m : 0;
    if (p->nmodels) snprintf(a->prefs[i].model, sizeof(a->prefs[i].model), "%s", p->models[a->model].id);
    load_key(a);
}

static void set_model(ChatApp *a, int m)
{
    Provider *p = cur_prov(a);
    if (m < 0 || m >= p->nmodels) return;
    a->model = m;
    snprintf(a->prefs[a->prov].model, sizeof(a->prefs[a->prov].model), "%s", p->models[m].id);
}

static const char *voice_of(ChatApp *a, int prov, char *out, size_t n)
{
    Provider *p = prov_at(a, prov);
    out[0] = 0;
    if (!p) return out;
    if (a->prefs[prov].voice[0]) snprintf(out, n, "%s", a->prefs[prov].voice);
    else { snprintf(out, n, "%s", p->voices); out[strcspn(out, ",")] = 0; }
    return out;
}

/* Primo fornitore con trascrizione (o sintesi) e una chiave: Groq e Gemini prima, sono gratuiti. */
static int pick_voice_provider(ChatApp *a, int tts)
{
    static const char *ORDER[] = { "groq", "gemini", "openai" };
    for (size_t k = 0; k < sizeof(ORDER) / sizeof(ORDER[0]); k++) {
        int i = registry_find(&a->reg, ORDER[k]);
        if (i >= 0 && (tts ? a->reg.p[i].tts : a->reg.p[i].stt) && has_key(a, i)) return i;
    }
    for (int i = 0; i < a->reg.n; i++) if ((tts ? a->reg.p[i].tts : a->reg.p[i].stt) && has_key(a, i)) return i;
    return -1;
}

static void load_settings(ChatApp *a)
{
    char *s = read_file(a->settings_path, NULL);
    int prov = 0, stt_set = 0, tts_set = 0;
    a->stt_prov = a->tts_prov = -1;
    if (s) {
        for (char *line = strtok(s, "\n"); line; line = strtok(NULL, "\n")) {
            line[strcspn(line, "\r")] = 0;
            char *eq = strchr(line, '=');
            if (!eq || line[0] == '#') continue;
            *eq = 0;
            const char *k = line, *v = eq + 1;
            int iv = atoi(v);
            if (!strcmp(k, "provider")) { int i = registry_find(&a->reg, v); if (i >= 0) prov = i; }
            else if (!strcmp(k, "model") && isdigit((unsigned char)v[0])) {   /* impostazioni della 0.2: solo Claude */
                static const char *OLD[] = { "claude-opus-5-5", "claude-sonnet-5-5", "claude-haiku-5-5" };
                if (iv >= 0 && iv < 3) snprintf(a->prefs[0].model, sizeof(a->prefs[0].model), "%s", OLD[iv]);
            } else if (!strncmp(k, "model.", 6) || !strncmp(k, "voice.", 6)) {
                int i = registry_find(&a->reg, k + 6);
                if (i < 0) continue;
                if (k[0] == 'm') snprintf(a->prefs[i].model, sizeof(a->prefs[i].model), "%s", v);
                else snprintf(a->prefs[i].voice, sizeof(a->prefs[i].voice), "%s", v);
            }
            else if (!strcmp(k, "effort") && iv >= 0 && iv < 3) a->effort = iv;
            else if (!strcmp(k, "concise")) a->concise = iv != 0;
            else if (!strcmp(k, "speak")) a->speak = iv != 0;
            else if (!strcmp(k, "autosend")) a->autosend = iv != 0;
            else if (!strcmp(k, "mic")) a->mic = iv == MIC_BT ? MIC_BT : MIC_USB;
            else if (!strcmp(k, "stt")) { stt_set = 1; a->stt_prov = registry_find(&a->reg, v); }
            else if (!strcmp(k, "tts")) { tts_set = 1; a->tts_prov = registry_find(&a->reg, v); }
        }
        free(s);
    }
    if (a->stt_prov >= 0 && !a->reg.p[a->stt_prov].stt) a->stt_prov = -1;
    if (a->tts_prov >= 0 && !a->reg.p[a->tts_prov].tts) a->tts_prov = -1;
    if (!stt_set) a->stt_prov = pick_voice_provider(a, 0);
    if (!tts_set) a->tts_prov = pick_voice_provider(a, 1);
    set_provider(a, prov);
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
    buf_adds(&b, "Stai parlando con una persona attraverso «Chiedi all'IA», un'app di Cirmolo per la console portatile "
                 "Miyoo Flip: lo schermo è piccolo (640x480) e si scrive con una tastiera a schermo o si parla al "
                 "microfono, quindi i messaggi che ricevi possono essere brevi o avere qualche errore di battitura o "
                 "di trascrizione.");
    if (tm) buf_printf(&b, " Oggi è %s %d %s %d.", DAYS[tm->tm_wday], tm->tm_mday, MONTHS[tm->tm_mon], tm->tm_year + 1900);
    const char *lang = i18n_language();
    if (!strcmp(lang, "Italian")) buf_adds(&b, " Rispondi nella lingua di chi scrive, di solito l'italiano.");
    else buf_printf(&b, " Rispondi nella lingua di chi scrive (l'interfaccia è in: %s).", lang);
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

/* Archivio in testo semplice, leggibile dal PC: Saves/claude/archivio/AAAA-MM-GG_HHMMSS.txt */
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
        const char *who = m->role == ROLE_USER ? tr("Tu") : (m->model ? model_display_name(m->model) : tr("Assistente"));
        buf_printf(&b, "%s%s\n%s\n", who, m->excluded ? tr(" (non inviato)") : "", m->text ? m->text : "");
        if (m->note) buf_printf(&b, "[%s]\n", m->note);
        buf_adds(&b, "\n");
    }
    buf_printf(&b, tr("Token: %ld in ingresso, %ld in uscita. Costo stimato: %.4f $\n"),
               a->conv.in_tokens + a->conv.cache_read + a->conv.cache_write, a->conv.out_tokens, a->conv.cost);
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
        m->note = dupstr(tr("Senza risposta: l'app è stata chiusa."));
    }
}

/* ------------------------------------------------------------------ lettura ad alta voce */
static int voice_ready(ChatApp *a, int tts)
{
    int p = tts ? a->tts_prov : a->stt_prov;
    return p >= 0 && has_key(a, p);
}

static void speech_stop(ChatApp *a)
{
    tts_cancel(&a->tts);
    player_clear(&a->pl);
    free(a->speak_text);
    a->speak_text = NULL;
    a->speak_pos = a->speak_src_len = 0;
    a->speak_final = 0;
}

static void speech_begin(ChatApp *a)
{
    speech_stop(a);
    a->speak_first = 1;
    a->speaking = a->speak && voice_ready(a, 1);
}

/* Testo da leggere: durante la risposta solo le righe complete (la pulizia e' stabile sui prefissi). */
static void speech_feed(ChatApp *a, const char *text, int final)
{
    if (!a->speaking) return;
    size_t n = strlen(text);
    if (!final && n == a->speak_src_len) return;
    a->speak_src_len = n;
    char *src = dupstr(text);
    if (!final) {
        char *nl = strrchr(src, '\n');
        if (!nl) { free(src); return; }
        nl[1] = 0;
    }
    char *clean = tts_clean(src);
    free(src);
    free(a->speak_text);
    a->speak_text = clean;
    a->speak_final = final;
}

static void speech_pump(ChatApp *a, float dt)
{
    (void)dt;
    char err[200];
    if (a->tts.active && tts_poll(&a->tts, &a->pl, err, sizeof(err)) && err[0]) {
        toast(a, err);
        free(a->speak_text);
        a->speak_text = NULL;
        a->speaking = 0;
    }
    if (!a->speaking || !a->speak_text || a->tts.active || a->offline) return;   /* nelle prove si guarda solo il testo */
    if (player_queued(&a->pl) > a->pl.src_rate * 8) return;      /* abbastanza audio in coda */
    char *chunk = tts_next_chunk(a->speak_text, &a->speak_pos, a->speak_first ? 220 : 700, a->speak_final);
    if (!chunk) {
        if (a->speak_final) { free(a->speak_text); a->speak_text = NULL; }
        return;
    }
    a->speak_first = 0;
    Provider *p = &a->reg.p[a->tts_prov];
    char key[256], voice[32], msg[200];
    provider_key(p, a->dir, key, sizeof(key));
    voice_of(a, a->tts_prov, voice, sizeof(voice));
    if (tts_start(&a->tts, p, key, p->tts_model, voice, chunk, lang_code(i18n_language()), &a->pl, msg, sizeof(msg))) {
        toast(a, msg);
        a->speaking = 0;
    }
    memset(key, 0, sizeof(key));
    free(chunk);
}

static int speech_busy(ChatApp *a) { return a->tts.active || player_queued(&a->pl) > 0 || (a->speaking && a->speak_text); }

static void speak_last(ChatApp *a)
{
    for (int i = a->conv.n - 1; i >= 0; i--) {
        const ChatMsg *m = &a->conv.msg[i];
        if (m->role != ROLE_ASSISTANT || m->excluded) continue;
        if (!voice_ready(a, 1)) { toast(a, tr("Per la lettura scegli la sintesi vocale e la sua chiave (SELECT, scheda Voce).")); return; }
        int was = a->speak;
        a->speak = 1;
        speech_begin(a);
        a->speak = was;
        speech_feed(a, m->text, 1);
        return;
    }
    toast(a, tr("Nessuna risposta da leggere."));
}

/* ------------------------------------------------------------------ invio e risposte */
static void cost_add(ChatApp *a, const Reply *r)
{
    Provider *p = prov_at(a, a->reply_prov);
    const Model *m = p && a->reply_model >= 0 && a->reply_model < p->nmodels ? &p->models[a->reply_model] : NULL;
    int k = p ? provider_find_model(p, r->model) : -1;
    if (k >= 0) m = &p->models[k];
    a->conv.in_tokens += r->in_tokens;
    a->conv.out_tokens += r->out_tokens;
    a->conv.cache_read += r->cache_read;
    a->conv.cache_write += r->cache_write;
    a->conv.cost += model_cost(m, r->in_tokens, r->out_tokens, r->cache_read, r->cache_write);
}

static int start_request(ChatApp *a, char *msg, size_t msgn)
{
    Provider *p = cur_prov(a);
    Model *m = cur_model(a);
    if (!m) { snprintf(msg, msgn, "%s", tr("Nessun modello: aggiorna l'elenco nelle impostazioni.")); return -1; }
    Buf h = { 0 };
    provider_auth_headers(p, a->key, &h);
    buf_adds(&h, "Content-Type: application/json\n");
    char url[220], *body;
    if (p->proto == PROTO_ANTHROPIC) {
        body = anthropic_request(&a->conv, m->id, (m->flags & MF_EFFORT) ? EFFORTS[a->effort] : NULL, (m->flags & MF_FALLBACK) != 0);
        if (m->flags & MF_FALLBACK) buf_adds(&h, "anthropic-beta: " ANTHROPIC_FALLBACK_BETA "\n");
        snprintf(url, sizeof(url), "%s/messages", p->base);
    } else {
        body = openai_request(&a->conv, m->id, 0, p->stream_options);
        snprintf(url, sizeof(url), "%s/chat/completions", p->base);
    }
    Request rq = { url, h.p, body, 0, NULL, NULL, 900 };
    int rc = net_start(&a->xfer, &rq, msg, msgn);
    if (h.p) memset(h.p, 0, h.len);               /* la chiave resta solo nel file temporaneo, in RAM */
    buf_free(&h);
    free(body);
    return rc;
}

static void finish_reply(ChatApp *a)
{
    Reply *r = &a->reply;
    Provider *p = prov_at(a, a->reply_prov);
    /* alcuni server non accettano stream_options: si riprova una volta senza */
    if (r->state == RS_ERROR && r->proto == PROTO_OPENAI && r->http_status == 400 && !a->retried && p && p->stream_options &&
        r->body.p && strstr(r->body.p, "stream_options")) {
        p->stream_options = 0;
        a->retried = 1;
        reply_reset(r);
        r->state = RS_WAITING;
        char msg[200];
        if (!start_request(a, msg, sizeof(msg))) return;
        reply_set_error(r, msg);
    }
    a->busy = 0;
    ChatMsg *user = NULL;
    for (int i = a->conv.n - 1; i >= 0; i--) if (a->conv.msg[i].role == ROLE_USER) { user = &a->conv.msg[i]; break; }
    int proto = PROTO_PLAIN;
    char *json = r->state == RS_DONE ? reply_message_json(r, &proto) : NULL;
    char *text = reply_text(r);
    if (r->model[0] || r->in_tokens || r->out_tokens) cost_add(a, r);
    const char *asked = p && a->reply_model >= 0 && a->reply_model < p->nmodels ? p->models[a->reply_model].id : "";
    if (json) {
        ChatMsg *m = conv_add(&a->conv, ROLE_ASSISTANT, proto, json, text);
        m->model = dupstr(r->model[0] ? r->model : asked);
        if (r->fallback || (r->proto == PROTO_ANTHROPIC && r->model[0] && strcmp(r->model, asked))) {
            char note[160];
            snprintf(note, sizeof(note), tr("Ha risposto %s."), model_display_name(r->model));
            m->note = dupstr(note);
        }
        if (!strcmp(r->stop_reason, "length") || !strcmp(r->stop_reason, "max_tokens")) {
            free(m->note);
            m->note = dupstr(tr("Risposta tagliata: era troppo lunga."));
        }
        __atomic_add_fetch(&a->chimes, 1, __ATOMIC_RELEASE);
        speech_feed(a, text, 1);
    } else {
        /* scambio non riuscito: resta da leggere ma non torna al modello */
        char note[360];
        switch (r->state) {
        case RS_REFUSED:
            if (r->category[0]) snprintf(note, sizeof(note), tr("Il modello non ha risposto a questa richiesta (categoria: %s)."), r->category);
            else snprintf(note, sizeof(note), "%s", tr("Il modello non ha risposto a questa richiesta."));
            break;
        case RS_CANCELLED: snprintf(note, sizeof(note), "%s", tr("Risposta interrotta.")); break;
        case RS_DONE: snprintf(note, sizeof(note), "%s", tr("Nessuna risposta.")); break;
        default: snprintf(note, sizeof(note), "%s", r->error[0] ? tr(r->error) : tr("Errore sconosciuto."));
        }
        if (user) user->excluded = 1;
        if (text[0]) {
            ChatMsg *m = conv_add(&a->conv, ROLE_ASSISTANT, PROTO_PLAIN, dupstr("{\"role\":\"assistant\",\"content\":[]}"), text);
            m->excluded = 1;
            m->note = dupstr(note);
        } else {
            free(text);
            if (user) { free(user->note); user->note = dupstr(note); }
        }
        if (r->state != RS_CANCELLED) toast(a, note);
        speech_stop(a);
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
    if (a->busy) { toast(a, tr("Aspetta la fine della risposta (B la ferma).")); return -1; }
    char *text = dupstr(text_in);
    trim(text);
    if (!text[0]) { free(text); return -1; }
    Provider *p = cur_prov(a);
    if (p->needs_key && !a->key[0] && !a->offline) {
        toastf(a, tr("Manca la chiave API di %s: guarda nelle impostazioni (SELECT)."), p->name);
        free(text);
        return -1;
    }
    conv_add_user(&a->conv, text);
    free(text);
    reply_init(&a->reply, p->proto == PROTO_ANTHROPIC ? PROTO_ANTHROPIC : PROTO_OPENAI, p->name);
    a->reply.state = RS_WAITING;
    a->reply_prov = a->prov;
    a->reply_model = a->model;
    a->retried = 0;
    a->busy = 1;
    a->follow = 1;
    a->typing = 0;
    speech_begin(a);
    if (!a->offline) {
        char msg[200];
        if (start_request(a, msg, sizeof(msg))) {
            a->reply.state = RS_ERROR;
            snprintf(a->reply.error, sizeof(a->reply.error), "%s", msg);
            finish_reply(a);
        }
    }
    save_conversation(a);
    return 0;
}

static void send_input(ChatApp *a)
{
    if (!a->input.len) { toast(a, tr("Scrivi qualcosa prima di inviare.")); return; }
    if (send_text(a, a->input.p) == 0) { buf_clear(&a->input); a->cursor = 0; }
}

static void cancel_reply(ChatApp *a)
{
    if (!a->busy) return;
    if (!a->offline) net_cancel(&a->xfer);
    a->reply.state = RS_CANCELLED;
    finish_reply(a);
    toast(a, tr("Risposta interrotta."));
}

static void retry_last(ChatApp *a)
{
    if (a->busy) return;
    for (int i = a->conv.n - 1; i >= 0; i--) {
        ChatMsg *m = &a->conv.msg[i];
        if (m->role != ROLE_USER) continue;
        if (!m->excluded) { toast(a, tr("L'ultima domanda ha già una risposta.")); return; }
        char *t = dupstr(m->text);
        send_text(a, t);
        free(t);
        return;
    }
    toast(a, tr("Niente da riprovare."));
}

/* ------------------------------------------------------------------ elenco dei modelli dal fornitore */
static void fetch_models(ChatApp *a)
{
    if (a->mfetch >= 0) { toast(a, tr("Sto già scaricando l'elenco dei modelli.")); return; }
    Provider *p = cur_prov(a);
    if (p->needs_key && !a->key[0]) { toastf(a, tr("Prima metti la chiave di %s."), p->name); return; }
    Buf h = { 0 };
    provider_auth_headers(p, a->key, &h);
    char url[220], msg[200];
    snprintf(url, sizeof(url), "%s/models%s", p->base, p->proto == PROTO_ANTHROPIC ? "?limit=1000" : "");
    Request rq = { url, h.p, NULL, 0, "GET", NULL, 60 };
    buf_free(&a->mbody);
    if (net_start(&a->mxfer, &rq, msg, sizeof(msg))) toast(a, msg);
    else { a->mfetch = a->prov; toast(a, tr("Scarico l'elenco dei modelli...")); }
    if (h.p) memset(h.p, 0, h.len);
    buf_free(&h);
}

static void mbody_sink(void *ud, const char *d, size_t n)
{
    Buf *b = ud;
    if (b->len < 4u * 1024 * 1024) buf_add(b, d, n);
}

static void fetch_poll(ChatApp *a)
{
    if (a->mfetch < 0) return;
    int code = 0;
    char err[200];
    if (!net_poll(&a->mxfer, mbody_sink, &a->mbody, &code, err, sizeof(err))) return;
    Provider *p = &a->reg.p[a->mfetch];
    a->mfetch = -1;
    int http = 0;
    char *mark = a->mbody.p ? strstr(a->mbody.p, "\n@@CIRMOLO_HTTP ") : NULL;
    if (mark) { http = net_http_line(mark + 1); *mark = 0; a->mbody.len = (size_t)(mark - a->mbody.p); }
    if (code) { const char *m = net_curl_message(code); toast(a, m ? tr(m) : err); }
    else if (http != 200) { char t[160]; snprintf(t, sizeof(t), tr("%s non ha dato l'elenco (HTTP %d)."), p->name, http); toast(a, t); }
    else {
        char **ids = NULL;
        int n = parse_model_list(a->mbody.p ? a->mbody.p : "", a->mbody.len, &ids);
        if (n <= 0) toast(a, tr("Elenco dei modelli vuoto o non riconosciuto."));
        else {
            char keep[96];
            snprintf(keep, sizeof(keep), "%s", p == cur_prov(a) ? cur_model_id(a) : "");
            provider_set_fetched(p, a->dir, (const char *const *)ids, n);
            if (p == cur_prov(a)) { int k = provider_find_model(p, keep); a->model = k >= 0 ? k : 0; }
            char t[120];
            snprintf(t, sizeof(t), tr("%d modelli scaricati."), n);
            toast(a, t);
        }
        for (int i = 0; i < n; i++) free(ids[i]);
        free(ids);
    }
    buf_free(&a->mbody);
}

/* ------------------------------------------------------------------ microfono */
static void bt_script(ChatApp *a, const char *what)
{
#ifndef _WIN32
    if (a->offline) return;
    pid_t pid = fork();
    if (pid == 0) {
        for (int fd = 3; fd < 1024; fd++) close(fd);
        execlp("sh", "sh", "bt-microfono.sh", what, (char *)NULL);
        _exit(127);
    }
#else
    (void)a; (void)what;
#endif
}

static void start_recording(ChatApp *a)
{
    a->vstate = V_IDLE;
    if (a->busy) { toast(a, tr("Aspetta la fine della risposta (B la ferma).")); return; }
    if (a->stt_prov < 0) { toast(a, tr("Per parlare scegli la trascrizione (SELECT, scheda Voce).")); return; }
    if (!has_key(a, a->stt_prov)) { toastf(a, tr("Manca la chiave di %s per la trascrizione."), a->reg.p[a->stt_prov].name); return; }
    speech_stop(a);
    a->speaking = 0;
    char msg[200];
    if (a->mic == MIC_BT) {
        if (rec_start_bt(&a->rec, msg, sizeof(msg))) { toast(a, msg); return; }
    } else {
        if (a->cap_rate <= 0) { toast(a, tr("Nessun microfono: collega un microfono o delle cuffie USB.")); return; }
        rec_start_usb(&a->rec, a->cap_rate);
    }
    a->vstate = V_RECORDING;
    beep(a, 1);
}

static void stop_recording(ChatApp *a, int send)
{
    if (a->vstate != V_RECORDING) return;
    rec_stop(&a->rec);
    a->vstate = V_IDLE;
    beep(a, 0);
    if (!send) { toast(a, tr("Registrazione annullata.")); return; }
    if (rec_seconds(&a->rec) < 0.5f) {
        toast(a, a->mic == MIC_BT && a->rec.len == 0 ? tr("Nessun suono dalle cuffie Bluetooth: sono collegate?")
                                                     : tr("Troppo breve: tieni premuto R2 mentre parli."));
        return;
    }
    size_t wl = 0;
    char *wav = wav_encode(a->rec.pcm, a->rec.len, a->rec.rate, &wl);
    Provider *p = &a->reg.p[a->stt_prov];
    char key[256], msg[200];
    provider_key(p, a->dir, key, sizeof(key));
    if (a->offline) { free(wav); a->vstate = V_TRANSCRIBING; return; }
    if (stt_start(&a->stt, p, key, p->stt_model, wav, wl, lang_code(i18n_language()), msg, sizeof(msg))) toast(a, msg);
    else a->vstate = V_TRANSCRIBING;
    memset(key, 0, sizeof(key));
    free(wav);
}

static void got_transcript(ChatApp *a, const char *text)
{
    if (!text[0]) { toast(a, tr("Non ho capito: riprova parlando più vicino al microfono.")); return; }
    if (a->autosend && !a->entering_key) {
        if (send_text(a, text) == 0) return;
    }
    a->typing = 1;
    if (a->input.len && a->input.p[a->input.len - 1] != ' ') input_insert(a, " ");
    input_insert(a, text);
}

static void voice_update(ChatApp *a, float dt)
{
    if (a->vstate == V_HOLD) {
        a->hold_t += dt;
        if (!a->down[PAD_R2]) a->vstate = V_IDLE;
        else if (a->hold_t > 0.28f) start_recording(a);
    }
    if (a->vstate == V_RECORDING) {
        rec_poll(&a->rec);
        if (a->mic == MIC_BT && a->rec.bt_pid < 0) { a->vstate = V_IDLE; toast(a, tr("Microfono Bluetooth non disponibile: attivalo nelle impostazioni e ricollega le cuffie.")); }
        else if (rec_seconds(&a->rec) >= VOICE_MAX_SEC - 0.05f) stop_recording(a, 1);
    }
    if (a->vstate == V_TRANSCRIBING && !a->offline) {
        char out[MAX_INPUT], err[300];
        if (stt_poll(&a->stt, &a->reg.p[a->stt_prov], out, sizeof(out), err, sizeof(err))) {
            a->vstate = V_IDLE;
            if (err[0]) toast(a, err);
            else got_transcript(a, out);
        }
    }
}

/* ------------------------------------------------------------------ scrittura */
static void input_insert(ChatApp *a, const char *t)
{
    size_t n = strlen(t);
    if (a->input.len + n > MAX_INPUT) { toast(a, tr("Messaggio troppo lungo.")); return; }
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
        Provider *p = &a->reg.p[a->key_target];
        if (clean_key(a->input.p ? a->input.p : "", clean, sizeof(clean))) { toast(a, tr("Chiave non valida: contiene caratteri strani o è troppo corta.")); return; }
        if (provider_save_key(p, a->dir, clean)) { toast(a, tr("Impossibile salvare la chiave sulla SD.")); return; }
        if (a->key_target == a->prov) snprintf(a->key, sizeof(a->key), "%s", clean);
        memset(clean, 0, sizeof(clean));
        char t[200];
        snprintf(t, sizeof(t), tr("Chiave salvata in Saves/claude/chiavi/%s.txt."), p->id);
        toast(a, t);
        if (a->stt_prov < 0 && p->stt) a->stt_prov = a->key_target;      /* la voce si configura da sola */
        if (a->tts_prov < 0 && p->tts) a->tts_prov = a->key_target;
        save_settings(a);
    }
    memset(a->input.p ? a->input.p : (char *)"", 0, a->input.len);   /* la chiave non resta in memoria */
    buf_clear(&a->input);
    a->cursor = 0;
    a->entering_key = 0;
    a->typing = 0;
    a->page = PAGE_SETTINGS;
}

static void begin_key_entry(ChatApp *a, int prov)
{
    if (prov < 0) return;
    a->key_target = prov;
    a->entering_key = 1;
    a->page = PAGE_CHAT;
    a->typing = 1;
    buf_clear(&a->input);
    a->cursor = 0;
    a->layer = 0;
    a->shift = 0;
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

/* ------------------------------------------------------------------ impostazioni */
enum { S_PROVIDER, S_MODEL, S_EFFORT, S_STYLE, S_KEY, S_NEW, S_USAGE, S_COUNT };
enum { W_SPEAK, W_TTS, W_VOICE, W_STT, W_AUTOSEND, W_MIC, W_STATUS, W_COUNT };

static int tab_count(const ChatApp *a) { return a->tab == TAB_CHAT ? S_COUNT : W_COUNT; }

/* Prossimo fornitore con trascrizione o sintesi (-1 = spenta). */
static int next_voice_prov(ChatApp *a, int cur, int dir, int tts)
{
    int n = a->reg.n + 1, i = cur + 1;           /* 0 = spenta, 1.. = fornitori */
    for (int k = 0; k < n; k++) {
        i = (i + dir + n) % n;
        if (i == 0) return -1;
        const Provider *p = &a->reg.p[i - 1];
        if (tts ? p->tts : p->stt) return i - 1;
    }
    return -1;
}

static void cycle_voice(ChatApp *a, int dir)
{
    Provider *p = prov_at(a, a->tts_prov);
    if (!p || !p->voices[0]) return;
    char cur[32], list[200];
    voice_of(a, a->tts_prov, cur, sizeof(cur));
    snprintf(list, sizeof(list), "%s", p->voices);
    char *v[40];
    int n = 0, at = 0;
    for (char *t = strtok(list, ","); t && n < 40; t = strtok(NULL, ",")) { if (!strcmp(t, cur)) at = n; v[n++] = t; }
    if (!n) return;
    at = (at + dir + n) % n;
    snprintf(a->prefs[a->tts_prov].voice, sizeof(a->prefs[a->tts_prov].voice), "%s", v[at]);
}

static void settings_change(ChatApp *a, int dir)
{
    if (a->tab == TAB_CHAT) {
        switch (a->set_sel) {
        case S_PROVIDER: {                       /* i fornitori solo per la voce non si scelgono qui */
            int i = a->prov;
            for (int k = 0; k < a->reg.n; k++) { i = (i + dir + a->reg.n) % a->reg.n; if (a->reg.p[i].nmodels) break; }
            set_provider(a, i);
            break;
        }
        case S_MODEL: { int n = cur_prov(a)->nmodels; if (n) set_model(a, (a->model + dir + n) % n); break; }
        case S_EFFORT:
            if (cur_prov(a)->proto != PROTO_ANTHROPIC) { toast(a, tr("L'impegno si sceglie solo per i modelli Claude.")); return; }
            a->effort = (a->effort + dir + 3) % 3;
            break;
        case S_STYLE: a->concise = !a->concise; toast(a, tr("Vale dalla prossima conversazione.")); break;
        default: return;
        }
    } else {
        switch (a->set_sel) {
        case W_SPEAK:
            a->speak = !a->speak;
            if (!a->speak) { speech_stop(a); a->speaking = 0; }
            else if (!voice_ready(a, 1)) toast(a, tr("Scegli anche la sintesi vocale e mettine la chiave."));
            break;
        case W_TTS: a->tts_prov = next_voice_prov(a, a->tts_prov, dir, 1); speech_stop(a); break;
        case W_VOICE: cycle_voice(a, dir); break;
        case W_STT: a->stt_prov = next_voice_prov(a, a->stt_prov, dir, 0); break;
        case W_AUTOSEND: a->autosend = !a->autosend; break;
        case W_MIC:
            a->mic = a->mic == MIC_USB ? MIC_BT : MIC_USB;
            bt_script(a, a->mic == MIC_BT ? "on" : "off");
            if (a->mic == MIC_BT) toast(a, tr("Microfono Bluetooth: se non funziona, spegni e riaccendi le cuffie."));
            break;
        default: return;
        }
    }
    save_settings(a);
}

static void settings_activate(ChatApp *a)
{
    if (a->tab == TAB_CHAT) {
        switch (a->set_sel) {
        case S_MODEL: a->picker_open = 1; a->picker_sel = a->model; a->picker_top = a->model - 4; break;
        case S_KEY: begin_key_entry(a, a->prov); break;
        case S_NEW: a->confirm_new = 1; break;
        case S_USAGE: break;
        default: settings_change(a, 1);
        }
    } else {
        int prov = a->set_sel == W_TTS ? a->tts_prov : (a->set_sel == W_STT ? a->stt_prov : -2);
        if (prov >= 0 && a->reg.p[prov].needs_key) begin_key_entry(a, prov);
        else if (a->set_sel != W_STATUS) settings_change(a, 1);
    }
}

/* ------------------------------------------------------------------ tasti */
static void scroll_by(ChatApp *a, float dy)
{
    a->scroll += dy;
    if (a->scroll < 0) a->scroll = 0;
    a->follow = 0;                                /* chat_draw lo riattiva se si torna in fondo */
}

static int repeats(const ChatApp *a, int b)
{
    if (a->quit_dialog || a->confirm_new || a->vstate == V_RECORDING) return 0;
    if (b == PAD_UP || b == PAD_DOWN || b == PAD_LEFT || b == PAD_RIGHT) return 1;
    if (a->page == PAGE_CHAT && a->typing && !a->picker_open) return b == PAD_B || b == PAD_L1 || b == PAD_R1 ||
        (b == PAD_A && KB[a->layer][a->kr][a->kc].sp == KS_BACK);
    return 0;
}

static void picker_press(ChatApp *a, int b)
{
    int n = cur_prov(a)->nmodels;
    switch (b) {
    case PAD_UP: if (n) a->picker_sel = (a->picker_sel + n - 1) % n; break;
    case PAD_DOWN: if (n) a->picker_sel = (a->picker_sel + 1) % n; break;
    case PAD_LEFT: a->picker_sel = a->picker_sel > 8 ? a->picker_sel - 8 : 0; break;
    case PAD_RIGHT: a->picker_sel = a->picker_sel + 8 < n ? a->picker_sel + 8 : (n ? n - 1 : 0); break;
    case PAD_A: if (n) { set_model(a, a->picker_sel); save_settings(a); } a->picker_open = 0; break;
    case PAD_X: fetch_models(a); break;
    case PAD_B: case PAD_SELECT: a->picker_open = 0; break;
    }
}

static void press(ChatApp *a, int b)
{
    if (a->quit_dialog) {
        if (b == PAD_A || b == PAD_DOWN) { a->quit = 1; }
        else if (b == PAD_B || b == PAD_UP || b == PAD_MENU) a->quit_dialog = 0;
        return;
    }
    if (a->vstate == V_RECORDING) {               /* mentre si parla: B annulla, il resto aspetta */
        if (b == PAD_B) stop_recording(a, 0);
        return;
    }
    if (a->confirm_new) {
        if (b == PAD_A) {
            if (a->busy) cancel_reply(a);
            speech_stop(a);
            archive_conversation(a);
            new_conversation(a);
            save_conversation(a);
            toast(a, tr("Conversazione archiviata in Saves/claude/archivio."));
            a->page = PAGE_CHAT;
            a->typing = 1;
        }
        if (b == PAD_A || b == PAD_B) a->confirm_new = 0;
        return;
    }
    if (a->picker_open) { picker_press(a, b); return; }
    if (a->quick_open) {
        switch (b) {
        case PAD_UP: a->quick_sel = (a->quick_sel + QUICK_COUNT - 1) % QUICK_COUNT; break;
        case PAD_DOWN: a->quick_sel = (a->quick_sel + 1) % QUICK_COUNT; break;
        case PAD_A:
            a->quick_open = 0;
            if (a->quick_sel == QUICK_SPEAK) speak_last(a);
            else send_text(a, tr(QUICK[a->quick_sel]));
            break;
        case PAD_B: case PAD_X: case PAD_R2: a->quick_open = 0; break;
        }
        return;
    }
    if (b == PAD_MENU) { a->quit_dialog = 1; return; }
    if (b == PAD_R2 && a->page == PAGE_CHAT && !a->entering_key) { a->vstate = V_HOLD; a->hold_t = 0; return; }

    if (a->page == PAGE_SETTINGS) {
        int n = tab_count(a);
        switch (b) {
        case PAD_UP: a->set_sel = (a->set_sel + n - 1) % n; break;
        case PAD_DOWN: a->set_sel = (a->set_sel + 1) % n; break;
        case PAD_LEFT: settings_change(a, -1); break;
        case PAD_RIGHT: settings_change(a, 1); break;
        case PAD_L1: case PAD_R1: a->tab = !a->tab; a->set_sel = 0; break;
        case PAD_A: settings_activate(a); break;
        case PAD_X: if (a->tab == TAB_CHAT && a->set_sel == S_MODEL) fetch_models(a); break;
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
    case PAD_B:
        if (a->busy) cancel_reply(a);
        else if (speech_busy(a)) { speech_stop(a); a->speaking = 0; toast(a, tr("Lettura fermata.")); }
        break;
    case PAD_X: a->quick_open = 1; a->quick_sel = 0; break;
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
        if (b == PAD_R2) {
            if (a->vstate == V_HOLD) {            /* premuto e lasciato subito: domande veloci */
                a->vstate = V_IDLE;
                if (!a->quit_dialog && !a->confirm_new && !a->picker_open) { a->quick_open = 1; a->quick_sel = 0; }
            } else if (a->vstate == V_RECORDING) stop_recording(a, 1);
        }
    }
}

void chat_axes(ChatApp *a, float lx, float ly, float rx, float ry, float l2, float r2)
{
    (void)lx; (void)rx; (void)l2; (void)r2;
    float v = fabsf(ry) > 0.25f ? ry : (fabsf(ly) > 0.25f ? ly : 0.0f);
    if (v != 0.0f && a->page == PAGE_CHAT && !a->picker_open) scroll_by(a, v * 9.0f);   /* le levette scorrono la chat */
}

void chat_text(ChatApp *a, const char *utf8, int special)
{
    if (a->quit_dialog || a->confirm_new || a->quick_open || a->picker_open || a->vstate == V_RECORDING) return;
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
    registry_load(&a->reg, a->dir);
    a->prefs = calloc((size_t)a->reg.n, sizeof(ProvPrefs));
    if (!a->prefs) abort();
    a->effort = 1;
    a->concise = 1;
    a->speak = 1;
    a->autosend = 1;
    a->follow = 1;
    a->mfetch = -1;
    a->xfer.pid = a->mxfer.pid = -1;
    rec_init(&a->rec);
    player_init(&a->pl);
    a->stt.xfer.pid = a->tts.xfer.pid = -1;
    load_settings(a);
    reply_init(&a->reply, PROTO_ANTHROPIC, "");
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
    if (a->mfetch >= 0) net_cancel(&a->mxfer);
    stop_recording(a, 0);
    stt_cancel(&a->stt);
    speech_stop(a);
    save_conversation(a);
    for (int i = 0; i < a->nwraps; i++) wrap_free(&a->wraps[i]);
    free(a->wraps);
    wrap_free(&a->live);
    free(a->live_text);
    conv_free(&a->conv);
    reply_free(&a->reply);
    if (a->input.p) memset(a->input.p, 0, a->input.len);
    buf_free(&a->input);
    buf_free(&a->mbody);
    buf_free(&a->tts.pending);
    memset(a->key, 0, sizeof(a->key));
    rec_free(&a->rec);
    player_free(&a->pl);
    registry_free(&a->reg);
    free(a->prefs);
    free(a);
}

void chat_capture(ChatApp *a, const float *in, int frames) { rec_capture(&a->rec, in, frames); }

void chat_capture_status(ChatApp *a, const char *device, float rate)
{
    snprintf(a->cap_name, sizeof(a->cap_name), "%s", device ? device : "");
    a->cap_rate = device ? rate : 0.0f;
}

/* Suoni discreti (tasti, risposta pronta, registrazione) e la voce che legge. */
void chat_audio(ChatApp *a, float *out, int frames)
{
    int clicks = __atomic_load_n(&a->clicks, __ATOMIC_ACQUIRE), chimes = __atomic_load_n(&a->chimes, __ATOMIC_ACQUIRE);
    int beeps = __atomic_load_n(&a->beeps, __ATOMIC_ACQUIRE);
    if (clicks != a->clicks_seen) { a->clicks_seen = clicks; a->click_t = 0.0f; }
    if (chimes != a->chimes_seen) { a->chimes_seen = chimes; a->chime_t = 0.0f; }
    if (beeps != a->beeps_seen) { a->beeps_seen = beeps; a->beep_t = 0.0f; }
    const float inv = 1.0f / a->sr;
    int quiet_chime = a->speaking;                /* se poi legge, il segnale di fine si sente appena */
    for (int i = 0; i < frames; i++) {
        float s = 0.0f;
        if (a->click_t < 0.03f) {
            a->rng ^= a->rng << 13; a->rng ^= a->rng >> 17; a->rng ^= a->rng << 5;
            s += (float)(int32_t)a->rng * (1.0f / 2147483648.0f) * expf(-a->click_t / 0.004f) * 0.08f;
            a->click_t += inv;
        }
        if (a->chime_t < 0.6f) {
            float t = a->chime_t, f = t < 0.12f ? 659.25f : 880.0f, tt = t < 0.12f ? t : t - 0.12f;
            s += sinf(6.2831853f * f * tt) * expf(-tt / 0.12f) * (quiet_chime ? 0.04f : 0.12f);
            a->chime_t += inv;
        }
        if (a->beep_t < 0.16f) {                  /* due note in su all'inizio, in giu' alla fine */
            float t = a->beep_t, lo = 587.33f, hi = 880.0f;
            float f = (t < 0.07f) == (a->beep_up != 0) ? lo : hi;
            s += sinf(6.2831853f * f * t) * 0.10f * (1.0f - t / 0.16f);
            a->beep_t += inv;
        }
        out[2 * i] = out[2 * i + 1] = s;
    }
    player_mix(&a->pl, out, frames, a->sr, 0.9f);
}

static void reply_sink(void *ud, const char *d, size_t n) { reply_feed(ud, d, n); }

void chat_update(ChatApp *a, float dt)
{
    a->time += dt;
    for (int b = 0; b < PAD_COUNT; b++) {
        if (!a->down[b] || !repeats(a, b)) continue;
        a->rep[b] += dt;
        while (a->rep[b] > 0.38f) { a->rep[b] -= 0.06f; press(a, b); }
    }
    if (a->busy && !a->offline) {
        int code = 0;
        char err[300];
        if (net_poll(&a->xfer, reply_sink, &a->reply, &code, err, sizeof(err))) {
            reply_finish(&a->reply, code, err);
            finish_reply(a);
        }
    }
    if (a->busy && a->speaking) {
        char *t = reply_text(&a->reply);
        speech_feed(a, t, 0);
        free(t);
    }
    fetch_poll(a);
    voice_update(a, dt);
    speech_pump(a, dt);
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
int chat_select_provider(ChatApp *a, const char *id) { int i = registry_find(&a->reg, id); if (i >= 0) set_provider(a, i); return i; }
const char *chat_model_id(ChatApp *a) { return cur_model_id(a); }
int chat_voice_state(const ChatApp *a) { return a->vstate; }
void chat_set_voice(ChatApp *a, const char *stt, const char *tts)
{
    a->stt_prov = stt ? registry_find(&a->reg, stt) : -1;
    a->tts_prov = tts ? registry_find(&a->reg, tts) : -1;
}
void chat_feed_transcript(ChatApp *a, const char *text) { if (a->vstate == V_TRANSCRIBING) { a->vstate = V_IDLE; got_transcript(a, text); } }
char *chat_speech_text(ChatApp *a) { return a->speak_text ? dupstr(a->speak_text) : NULL; }
void chat_open_settings(ChatApp *a, int tab, int sel) { a->page = PAGE_SETTINGS; a->tab = tab; a->set_sel = sel; }
void chat_open_picker(ChatApp *a) { a->picker_open = 1; a->picker_sel = a->model; a->picker_top = 0; }

void chat_feed_reply(ChatApp *a, const char *sse, int finish)
{
    if (!a->busy) return;
    reply_feed(&a->reply, sse, strlen(sse));
    if (a->speaking) { char *t = reply_text(&a->reply); speech_feed(a, t, 0); free(t); }
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
    int v[] = { a->page, a->tab, a->typing, a->quit_dialog, a->quick_open, a->quick_sel, a->confirm_new, a->entering_key, a->set_sel,
                a->cursor, a->layer, a->shift, a->kr, a->kc, a->busy, a->reply.state, a->conv.n, a->prov, a->model, a->effort,
                a->concise, a->speak, a->autosend, a->mic, a->stt_prov, a->tts_prov, a->picker_open, a->picker_sel, a->mfetch,
                (int)a->scroll, a->follow, a->toast_t > 0.0f, a->key[0] != 0, a->vstate, a->cap_rate > 0, speech_busy(a),
                a->busy || a->vstate >= V_RECORDING ? (int)(a->time * 8.0f) : 0,
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

/* Nome corto del modello per l'intestazione. */
static const char *short_name(const Model *m)
{
    if (!m) return "";
    const char *n = m->name;
    if (!strncmp(n, "Claude ", 7)) n += 7;
    const char *slash = strrchr(n, '/');          /* "vendor/modello" degli elenchi scaricati */
    return slash && slash[1] ? slash + 1 : n;
}

static void draw_header(ChatApp *a, Canvas *c)
{
    gfx_rect(c, 0, 0, c->w, HEADER_H - 1, C_BAR);
    gfx_rect(c, 0, HEADER_H - 1, c->w, 1, C_LINE);
    draw_logo(c, 14, 10);
    gfx_text(c, FONT_TITLE, 50, 32, tr("Chiedi all'IA"), C_TEXT);
    char model[64];
    snprintf(model, sizeof(model), "%s", short_name(cur_model(a)));
    while (gfx_text_width(FONT_BOLD, model) > 300 && strlen(model) > 4) { model[strlen(model) - 4] = 0; strcat(model, "..."); }
    int ok = !cur_prov(a)->needs_key || a->key[0];
    uint32_t dot = !ok ? C_RED : (a->busy ? C_GOLD : C_GREEN);
    int w = gfx_text_width(FONT_BOLD, model);
    gfx_text_right(c, FONT_BOLD, 626, 30, model, C_MUTED);
    gfx_circle(c, 626 - w - 14, 24, 5.0f, dot, a->busy ? 0.5f + 0.5f * sinf(a->time * 6.0f) : 1.0f);
    if (speech_busy(a)) {                         /* altoparlante: sta leggendo */
        float x = 626 - w - 44, y = 24;
        gfx_rect(c, (int)x, (int)y - 4, 5, 8, C_VIOLET);
        gfx_line(c, x + 5, y - 4, x + 11, y - 9, 2.0f, C_VIOLET, 1.0f);
        gfx_line(c, x + 11, y - 9, x + 11, y + 9, 2.0f, C_VIOLET, 1.0f);
        gfx_line(c, x + 11, y + 9, x + 5, y + 4, 2.0f, C_VIOLET, 1.0f);
    }
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
    Provider *p = cur_prov(a);
    char t[200];
    gfx_round_rect(c, 14, y0 + 6, 612, 150, 14, C_PANEL, 1.0f);
    gfx_text(c, FONT_BOLD, 30, y0 + 36, tr("Ciao! Scrivi una domanda e premi START."), C_TEXT);
    if (!p->needs_key || a->key[0]) {
        gfx_text(c, FONT_SMALL, 30, y0 + 64, tr("La tastiera: croce per scegliere, A per scrivere, B cancella, Y spazio."), C_MUTED);
        gfx_text(c, FONT_SMALL, 30, y0 + 86, tr("Tieni premuto R2 per parlare (serve un microfono USB o delle cuffie)."), C_MUTED);
        gfx_text(c, FONT_SMALL, 30, y0 + 108, tr("SELECT: lettura e impostazioni (fornitore, modello, voce)."), C_MUTED);
        snprintf(t, sizeof(t), tr("%s, %s. Serve il Wi-Fi."), p->name, cur_model(a) ? cur_model(a)->name : "-");
        gfx_text(c, FONT_SMALL, 30, y0 + 130, t, C_DIM);
    } else {
        snprintf(t, sizeof(t), tr("Manca la chiave API di %s. Due modi per metterla:"), p->name);
        gfx_text(c, FONT_SMALL, 30, y0 + 64, t, C_GOLD);
        snprintf(t, sizeof(t), tr("1. dal PC, nel file Saves/claude/chiavi/%s.txt della SD;"), p->id);
        gfx_text(c, FONT_SMALL, 30, y0 + 86, t, C_MUTED);
        gfx_text(c, FONT_SMALL, 30, y0 + 108, tr("2. qui: SELECT due volte, Chiave API, A, poi scrivila e START."), C_MUTED);
        snprintf(t, sizeof(t), tr("La crei su %s. Gratis: GLM (Z.ai), Groq, OpenRouter, Mistral."), p->site);
        gfx_text(c, FONT_SMALL, 30, y0 + 130, t, C_DIM);
    }
}

static void draw_chat(ChatApp *a, Canvas *c, int y0, int y1)
{
    const int avail_user = 440, avail_ai = 560;
    int total = 8;
    for (int i = 0; i < a->conv.n; i++) {
        const ChatMsg *m = &a->conv.msg[i];
        total += msg_height(wrap_for(a, i, m->role == ROLE_USER ? avail_user : avail_ai), m->note);
    }
    char *live = NULL;
    if (a->busy) {
        live = reply_text(&a->reply);
        if (!a->live_text || strcmp(live, a->live_text)) {
            free(a->live_text);
            a->live_text = live;
            wrap_text(&a->live, a->live_text, avail_ai);
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
        Wrap *w = wrap_for(a, i, m->role == ROLE_USER ? avail_user : avail_ai);
        int h = msg_height(w, m->note);
        if (y + h >= y0 - 10 && y <= y1 + 10)
            draw_bubble(c, w, m->role == ROLE_USER, y, m->excluded, m->note, m->excluded && m->note && strcmp(m->note, tr("Risposta interrotta.")));
        y += h;
    }
    if (a->busy) {
        char status[80];
        int dots = (int)(a->time * 3.0f) % 4;
        snprintf(status, sizeof(status), "%s%.*s", tr(reply_status(&a->reply)), dots, "...");
        if (a->live.n) draw_bubble(c, &a->live, 0, y, 0, status, 0);
        else {
            gfx_round_rect(c, 14, y, 260, 2 * BUBBLE_PAD + 22, 12, C_PANEL, 1.0f);
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
        int n = (int)a->input.len;
        for (int i = 0; i < n; i++) buf_adds(&shown, i >= n - 4 ? (char[]){ a->input.p[i], 0 } : "\xe2\x80\xa2");
        cur = (int)shown.len;
    } else {
        buf_add(&shown, a->input.p ? a->input.p : "", a->input.len);
    }
    if (!shown.len && !a->entering_key)
        gfx_text(c, FONT_BODY, 22, INPUT_Y + 31, a->busy ? tr("Sto rispondendo...") : tr("Scrivi qui il messaggio, o tieni premuto R2 e parla"), C_DIM);
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
    if (a->entering_key) {
        char t[160];
        snprintf(t, sizeof(t), tr("chiave di %s: START salva, SELECT annulla"), a->reg.p[a->key_target].name);
        gfx_text_right(c, FONT_SMALL, 620, INPUT_Y + 44, t, C_GOLD);
    }
}

static void draw_keyboard(ChatApp *a, Canvas *c)
{
    gfx_rect(c, 0, HINT_Y - 18, c->w, c->h - HINT_Y + 18, C_BAR);
    gfx_text_center(c, FONT_SMALL, 320, HINT_Y, tr("A tasto · B cancella · Y spazio · X maiuscole · L2 simboli · START invia · R2 tieni: parla"), C_DIM);
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
            const char *label = d->sp ? tr(d->t) : (a->shift ? upper_of(d->t, tmp) : d->t);
            int font = d->sp ? FONT_SMALL : FONT_BOLD;
            if (d->sp == KS_SHIFT && a->shift == 2) label = tr("BLOCCO");
            if (d->sp == KS_SEND && a->entering_key) label = tr("Salva");
            gfx_text_center(c, font, (int)(x + w / 2), (int)(y + (d->sp ? 22 : 23)), label, sel ? C_BAR : (d->sp ? C_MUTED : C_TEXT));
        }
}

static void draw_hints(Canvas *c, const char *text)
{
    gfx_rect(c, 0, 446, c->w, 34, C_BAR);
    gfx_rect(c, 0, 446, c->w, 1, C_LINE);
    gfx_text_center(c, FONT_SMALL, c->w / 2, 468, text, C_MUTED);
}

static void price_tag(const Model *m, char *out, size_t n)
{
    out[0] = 0;
    if (!m) return;
    if (m->flags & MF_FREE) snprintf(out, n, "%s", tr("gratis"));
    else if (m->in >= 0 && m->out >= 0) {
        snprintf(out, n, "%.2f / %.2f $", m->in, m->out);
        for (char *p = out; *p; p++) if (*p == '.') *p = ',';
    }
}

static void draw_row(Canvas *c, int y, int sel, const char *label, const char *val, uint32_t col)
{
    if (sel) gfx_round_rect(c, 14, y, 612, 34, 10, C_PANEL_HI, 1.0f);
    gfx_text(c, FONT_BODY, 32, y + 23, label, sel ? C_TEXT : C_MUTED);
    char v[96];
    snprintf(v, sizeof(v), "%s", val);
    while (gfx_text_width(FONT_BOLD, v) > 330 && strlen(v) > 4) { v[strlen(v) - 4] = 0; strcat(v, "..."); }
    gfx_text_right(c, FONT_BOLD, 610, y + 23, v, sel ? C_GOLD : col);
}

static void draw_settings(ChatApp *a, Canvas *c)
{
    /* schede */
    const char *tabs[2] = { tr("Chat"), tr("Voce") };
    for (int i = 0; i < 2; i++) {
        int x = 14 + i * 130;
        gfx_round_rect(c, x, 54, 122, 30, 15, i == a->tab ? C_GOLD_D : C_PANEL, 1.0f);
        gfx_text_center(c, FONT_BOLD, x + 61, 75, tabs[i], i == a->tab ? C_TEXT : C_MUTED);
    }
    gfx_text_right(c, FONT_SMALL, 626, 75, tr("L1/R1 cambia scheda"), C_DIM);
    char vals[8][96], help[2][160];
    const char *labels[8];
    uint32_t cols[8];
    int n = tab_count(a);
    for (int i = 0; i < 8; i++) { vals[i][0] = 0; cols[i] = C_TEXT; }
    help[0][0] = help[1][0] = 0;
    Provider *p = cur_prov(a);
    if (a->tab == TAB_CHAT) {
        static const char *L[S_COUNT] = { N_("Fornitore"), N_("Modello"), N_("Impegno"), N_("Stile delle risposte"), N_("Chiave API"), N_("Nuova conversazione"), N_("Consumo") };
        static const char *EFF[3] = { N_("Basso"), N_("Medio"), N_("Alto") };
        for (int i = 0; i < S_COUNT; i++) labels[i] = tr(L[i]);
        snprintf(vals[S_PROVIDER], 96, "%s", p->name);
        Model *m = cur_model(a);
        char tag[40];
        price_tag(m, tag, sizeof(tag));
        snprintf(vals[S_MODEL], 96, "%s%s%s%s", m ? m->name : "-", tag[0] ? " (" : "", tag, tag[0] ? ")" : "");
        snprintf(vals[S_EFFORT], 96, "%s", p->proto == PROTO_ANTHROPIC ? tr(EFF[a->effort]) : "-");
        snprintf(vals[S_STYLE], 96, "%s", a->concise ? tr("Brevi") : tr("Normali"));
        if (!p->needs_key) snprintf(vals[S_KEY], 96, "%s", tr("non serve"));
        else if (a->key[0]) snprintf(vals[S_KEY], 96, tr("presente (...%s)"), a->key + strlen(a->key) - 4);
        else { snprintf(vals[S_KEY], 96, "%s", tr("mancante")); cols[S_KEY] = C_RED; }
        snprintf(vals[S_NEW], 96, "%s", tr("A: archivia e ricomincia"));
        long tok = a->conv.in_tokens + a->conv.cache_read + a->conv.cache_write + a->conv.out_tokens;
        snprintf(vals[S_USAGE], 96, tr("%ld token, circa %.3f $"), tok, a->conv.cost);
        for (char *q = vals[S_USAGE]; *q; q++) if (*q == '.') *q = ',';
        switch (a->set_sel) {
        case S_PROVIDER:
            snprintf(help[0], 160, "%s", tr(p->note));
            snprintf(help[1], 160, tr("Chiave in Saves/claude/chiavi/%s.txt, si crea su %s."), p->id, p->site);
            break;
        case S_MODEL:
            snprintf(help[0], 160, "%s", tr("A apre l'elenco; X lo aggiorna dal fornitore (servono Wi-Fi e chiave)."));
            snprintf(help[1], 160, "%s", tr("Il prezzo è in dollari per milione di token (ingresso / uscita)."));
            break;
        case S_EFFORT:
            snprintf(help[0], 160, "%s", tr("Quanto Claude ragiona prima di rispondere: più alto vuol dire"));
            snprintf(help[1], 160, "%s", tr("risposte più accurate, ma più lente e costose. Solo per Claude."));
            break;
        case S_STYLE:
            snprintf(help[0], 160, "%s", tr("Brevi: risposte corte e dirette, adatte allo schermo piccolo."));
            snprintf(help[1], 160, "%s", tr("Vale dalla prossima conversazione."));
            break;
        case S_KEY:
            snprintf(help[0], 160, "%s", tr("A: scrivila con la tastiera, oppure mettila dal PC nel file sulla SD."));
            snprintf(help[1], 160, "%s", tr("Meglio una chiave con un limite di spesa."));
            break;
        case S_NEW:
            snprintf(help[0], 160, "%s", tr("Salva la chat in Saves/claude/archivio come testo"));
            snprintf(help[1], 160, "%s", tr("e ne inizia una nuova (anche per risparmiare token)."));
            break;
        default:
            snprintf(help[0], 160, "%s", tr("Stima con i prezzi di listino, se il fornitore li pubblica."));
            snprintf(help[1], 160, "%s", tr("I ragionamenti contano come token in uscita anche se non si vedono."));
        }
    } else {
        static const char *L[W_COUNT] = { N_("Leggi le risposte"), N_("Sintesi vocale"), N_("Voce"), N_("Trascrizione"), N_("Invio automatico"), N_("Microfono"), N_("Stato") };
        for (int i = 0; i < W_COUNT; i++) labels[i] = tr(L[i]);
        snprintf(vals[W_SPEAK], 96, "%s", a->speak ? tr("Sì") : tr("No"));
        Provider *t = prov_at(a, a->tts_prov), *s = prov_at(a, a->stt_prov);
        if (t) snprintf(vals[W_TTS], 96, "%s%s", t->name, has_key(a, a->tts_prov) ? "" : tr(" (manca la chiave)"));
        else snprintf(vals[W_TTS], 96, "%s", tr("spenta"));
        if (t && !has_key(a, a->tts_prov)) cols[W_TTS] = C_RED;
        char v[32];
        snprintf(vals[W_VOICE], 96, "%s", t ? voice_of(a, a->tts_prov, v, sizeof(v)) : "-");
        if (s) snprintf(vals[W_STT], 96, "%s%s", s->name, has_key(a, a->stt_prov) ? "" : tr(" (manca la chiave)"));
        else snprintf(vals[W_STT], 96, "%s", tr("spenta"));
        if (s && !has_key(a, a->stt_prov)) cols[W_STT] = C_RED;
        snprintf(vals[W_AUTOSEND], 96, "%s", a->autosend ? tr("Sì") : tr("No"));
        snprintf(vals[W_MIC], 96, "%s", a->mic == MIC_BT ? tr("Cuffie Bluetooth") : tr("USB"));
        if (a->mic == MIC_BT) snprintf(vals[W_STATUS], 96, "%s", tr("Bluetooth (HFP, 8 kHz)"));
        else if (a->cap_rate > 0) snprintf(vals[W_STATUS], 96, "%s", a->cap_name);
        else { snprintf(vals[W_STATUS], 96, "%s", tr("nessun microfono USB")); cols[W_STATUS] = C_DIM; }
        switch (a->set_sel) {
        case W_SPEAK:
            snprintf(help[0], 160, "%s", tr("Legge le risposte mentre arrivano; B le ferma."));
            snprintf(help[1], 160, "%s", tr("Nelle domande veloci (X) c'è anche «Rileggi l'ultima risposta»."));
            break;
        case W_TTS:
            snprintf(help[0], 160, "%s", tr("Chi trasforma il testo in voce. A: scrivi la sua chiave."));
            snprintf(help[1], 160, "%s", tr("OpenAI: voci naturali, a pagamento. Gemini: con limiti gratuiti."));
            break;
        case W_VOICE:
            snprintf(help[0], 160, "%s", tr("La voce che legge le risposte (sinistra/destra per cambiarla)."));
            break;
        case W_STT:
            snprintf(help[0], 160, "%s", tr("Chi trasforma la voce in testo. A: scrivi la sua chiave."));
            snprintf(help[1], 160, "%s", tr("Groq (Whisper) ha un piano gratuito; vanno bene anche OpenAI e Gemini."));
            break;
        case W_AUTOSEND:
            snprintf(help[0], 160, "%s", tr("Sì: la domanda parte appena lasci R2."));
            snprintf(help[1], 160, "%s", tr("No: il testo va nella casella, da correggere e inviare con START."));
            break;
        case W_MIC:
            snprintf(help[0], 160, "%s", tr("USB: microfono o cuffie USB-C, anche collegati ad app aperta."));
            snprintf(help[1], 160, "%s", tr("Bluetooth (prova): le cuffie passano alla modalità telefono, 8 kHz."));
            break;
        default:
            snprintf(help[0], 160, "%s", tr("Nella chat tieni premuto R2 e parla; lascialo per inviare."));
            snprintf(help[1], 160, "%s", tr("B mentre parli annulla. Al massimo un minuto per volta."));
        }
    }
    for (int i = 0; i < n; i++) draw_row(c, 94 + i * 37, i == a->set_sel, labels[i], vals[i], cols[i]);
    gfx_round_rect(c, 14, 358, 612, 82, 14, C_PANEL, 1.0f);
    gfx_text(c, FONT_SMALL, 30, 388, help[0], C_MUTED);
    gfx_text(c, FONT_SMALL, 30, 414, help[1], C_MUTED);
    draw_hints(c, tr("Su/giù sceglie · sinistra/destra cambia · A attiva · SELECT o B torna alla chat"));
}

static void draw_picker(ChatApp *a, Canvas *c)
{
    Provider *p = cur_prov(a);
    gfx_rect_alpha(c, 0, 0, c->w, c->h, RGB(0, 0, 0), 0.6f);
    gfx_round_rect(c, 40, 40, 560, 400, 16, C_PANEL_HI, 1.0f);
    gfx_round_frame(c, 40, 40, 560, 400, 16, 2, C_GOLD, 1.0f);
    char t[120];
    snprintf(t, sizeof(t), tr("Modelli di %s"), p->name);
    gfx_text(c, FONT_BOLD, 60, 70, t, C_GOLD);
    if (a->mfetch >= 0) gfx_text_right(c, FONT_SMALL, 580, 70, tr("scarico..."), C_MUTED);
    const int rows = 10, rh = 30;
    if (a->picker_sel < a->picker_top) a->picker_top = a->picker_sel;
    if (a->picker_sel >= a->picker_top + rows) a->picker_top = a->picker_sel - rows + 1;
    if (a->picker_top > p->nmodels - rows) a->picker_top = p->nmodels - rows;
    if (a->picker_top < 0) a->picker_top = 0;
    for (int i = 0; i < rows && a->picker_top + i < p->nmodels; i++) {
        int k = a->picker_top + i, y = 84 + i * rh;
        const Model *m = &p->models[k];
        if (k == a->picker_sel) gfx_round_rect(c, 52, y, 536, rh - 3, 8, C_GOLD_D, 1.0f);
        char name[100], tag[40];
        snprintf(name, sizeof(name), "%s%s", k == a->model ? "\xe2\x80\xa2 " : "", m->name);
        while (gfx_text_width(FONT_BODY, name) > 380 && strlen(name) > 4) { name[strlen(name) - 4] = 0; strcat(name, "..."); }
        gfx_text(c, FONT_BODY, 64, y + 20, name, k == a->picker_sel ? C_TEXT : C_MUTED);
        price_tag(m, tag, sizeof(tag));
        gfx_text_right(c, FONT_SMALL, 578, y + 19, tag, k == a->picker_sel ? C_TEXT : (m->flags & MF_FREE ? C_GREEN : C_DIM));
    }
    if (!p->nmodels) gfx_text_center(c, FONT_BODY, 320, 200, tr("Nessun modello: premi X per scaricare l'elenco."), C_MUTED);
    gfx_text_center(c, FONT_SMALL, 320, 426, tr("A sceglie · X aggiorna dal fornitore · B chiude"), C_GOLD);
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
    gfx_round_rect(c, 110, 240 - h / 2, 420, h, 16, C_PANEL_HI, 1.0f);
    gfx_round_frame(c, 110, 240 - h / 2, 420, h, 16, 2, C_GOLD, 1.0f);
    gfx_text_center(c, FONT_BOLD, 320, 240 - h / 2 + 30, tr("Domande veloci"), C_GOLD);
    for (int i = 0; i < QUICK_COUNT; i++) {
        int y = 240 - h / 2 + 44 + i * 34;
        if (i == a->quick_sel) gfx_round_rect(c, 122, y, 396, 30, 9, C_GOLD_D, 1.0f);
        gfx_text(c, FONT_BODY, 138, y + 21, tr(QUICK[i]), i == a->quick_sel ? C_TEXT : (i == QUICK_SPEAK ? C_VIOLET : C_MUTED));
    }
}

static void draw_voice(ChatApp *a, Canvas *c)
{
    gfx_rect_alpha(c, 0, 0, c->w, c->h, RGB(0, 0, 0), 0.5f);
    gfx_round_rect(c, 130, 150, 380, 170, 20, C_PANEL_HI, 1.0f);
    gfx_round_frame(c, 130, 150, 380, 170, 20, 2, a->vstate == V_RECORDING ? C_RED : C_VIOLET, 1.0f);
    /* microfono */
    float cx = 320, cy = 200;
    float pulse = a->vstate == V_RECORDING ? 6.0f + 18.0f * fminf(1.0f, a->rec.level * 3.0f) : 4.0f + 3.0f * sinf(a->time * 5.0f);
    gfx_circle(c, cx, cy, 20 + pulse, a->vstate == V_RECORDING ? C_RED : C_VIOLET, 0.25f);
    gfx_round_rect(c, cx - 8, cy - 18, 16, 28, 8, C_TEXT, 1.0f);
    gfx_line(c, cx - 13, cy + 2, cx - 13, cy + 6, 2.0f, C_TEXT, 1.0f);
    gfx_line(c, cx + 13, cy + 2, cx + 13, cy + 6, 2.0f, C_TEXT, 1.0f);
    gfx_line(c, cx - 13, cy + 6, cx, cy + 16, 2.0f, C_TEXT, 1.0f);
    gfx_line(c, cx + 13, cy + 6, cx, cy + 16, 2.0f, C_TEXT, 1.0f);
    gfx_line(c, cx, cy + 16, cx, cy + 22, 2.0f, C_TEXT, 1.0f);
    if (a->vstate == V_RECORDING) {
        char t[64];
        snprintf(t, sizeof(t), tr("Ti ascolto... %.1f s"), rec_seconds(&a->rec));
        for (char *q = t; *q; q++) if (*q == '.' && q[1] >= '0' && q[1] <= '9') *q = ',';
        gfx_text_center(c, FONT_BOLD, 320, 262, t, C_TEXT);
        gfx_text_center(c, FONT_SMALL, 320, 296, tr("Lascia R2 per inviare · B annulla"), C_MUTED);
    } else {
        char t[64];
        snprintf(t, sizeof(t), "%s%.*s", tr("Trascrivo"), (int)(a->time * 3.0f) % 4, "...");
        gfx_text_center(c, FONT_BOLD, 320, 262, t, C_TEXT);
        gfx_text_center(c, FONT_SMALL, 320, 296, a->stt_prov >= 0 ? a->reg.p[a->stt_prov].name : "", C_MUTED);
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
        draw_hints(c, a->busy ? tr("B ferma la risposta · su/giù scorre · A scrivi · X domande veloci")
                   : speech_busy(a) ? tr("B ferma la lettura · su/giù scorre · A scrivi · R2 tieni: parla")
                                    : tr("A scrivi · R2 tieni: parla · su/giù scorre · X veloci · Y riprova · SELECT impostazioni"));
    }
    if (a->picker_open) draw_picker(a, c);
    if (a->quick_open) draw_quick(a, c);
    if (a->vstate == V_RECORDING || a->vstate == V_TRANSCRIBING) draw_voice(a, c);
    if (a->toast_t > 0.0f && a->toast[0]) {
        float al = fminf(1.0f, a->toast_t * 3.0f);
        int w = gfx_text_width(FONT_BOLD, a->toast) + 36;
        if (w > 624) w = 624;
        int ty = a->page == PAGE_CHAT && a->typing ? 168 : 398;
        gfx_round_rect(c, 320 - w / 2.0f, ty, w, 36, 18, C_BAR, 0.94f * al);
        gfx_round_frame(c, 320 - w / 2.0f, ty, w, 36, 18, 1.5f, C_GOLD, al);
        gfx_text_center(c, FONT_BOLD, 320, ty + 24, a->toast, gfx_mix(C_BAR, C_TEXT, al));
    }
    if (a->confirm_new) draw_dialog(c, tr("Nuova conversazione?"), tr("Questa finisce nell'archivio sulla SD."), tr("A: sì   ·   B: no"));
    if (a->quit_dialog) draw_dialog(c, tr("Uscire da Chiedi all'IA?"), tr("La conversazione resta salvata."), tr("A o giù: esci   ·   B o su: resta"));
}
