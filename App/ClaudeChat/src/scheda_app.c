/* Cirmolo - scheda del gioco (vedi scheda_app.h). */
#include "scheda_app.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "ask.h"
#include "i18n.h"
#include "iaconf.h"
#include "json.h"
#include "llm.h"
#include "mdtext.h"
#include "platform.h"
#include "voice.h"

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#define MKDIR(p) mkdir(p, 0755)
#endif

#define C_BG_TOP   RGB(13, 16, 22)
#define C_BG_BOT   RGB(18, 23, 32)
#define C_BAR      RGB(16, 20, 28)
#define C_PANEL    RGB(25, 31, 42)
#define C_LINE     RGB(48, 58, 78)
#define C_TEXT     RGB(236, 236, 244)
#define C_MUTED    RGB(138, 148, 170)
#define C_DIM      RGB(88, 98, 120)
#define C_GOLD     RGB(242, 196, 92)
#define C_VIOLET   RGB(150, 116, 242)
#define C_RED      RGB(236, 96, 104)

#define TOP 56
#define BOTTOM 444
#define IMG_W 200
#define IMG_H 280

struct Scheda {
    float sr, time;
    char saves[400], card_path[700];
    GameInfo g;
    Registry reg;
    char *text;                                   /* scheda (Markdown) */
    char error[300], model_name[96];
    Wrap wrap;
    int wrap_w;
    size_t wrapped_len;
    float scroll;
    int busy, offline, quit, down[PAD_COUNT];
    float rep[PAD_COUNT];
    Transfer xfer;
    Reply reply;
    uint32_t *img;
    int img_w, img_h;
    /* lettura ad alta voce */
    Player pl;
    Tts tts;
    char *speak_text;
    size_t speak_pos;
    int speak_first;
    uint32_t drawn_key;
    float drawn_time;
};

/* ------------------------------------------------------------------ dati del gioco e prompt */
int scheda_read_request(const char *path, GameInfo *g)
{
    memset(g, 0, sizeof(*g));
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    Buf b = { 0 };
    char tmp[2048];
    size_t n;
    while ((n = fread(tmp, 1, sizeof(tmp), f)) > 0) buf_add(&b, tmp, n);
    fclose(f);
    JNode *j = json_parse(b.p ? b.p : "", b.len);
    const char *v;
    if ((v = json_str(j, "rom"))) snprintf(g->rom, sizeof(g->rom), "%s", v);
    if ((v = json_str(j, "system"))) snprintf(g->system, sizeof(g->system), "%s", v);
    if ((v = json_str(j, "system_name"))) snprintf(g->system_name, sizeof(g->system_name), "%s", v);
    if ((v = json_str(j, "name"))) snprintf(g->name, sizeof(g->name), "%s", v);
    if ((v = json_str(j, "image"))) snprintf(g->image, sizeof(g->image), "%s", v);
    json_free(j);
    buf_free(&b);
    if (!g->name[0] && g->rom[0]) {               /* senza nome: quello del file */
        const char *base = strrchr(g->rom, '/');
        snprintf(g->name, sizeof(g->name), "%s", base ? base + 1 : g->rom);
        char *dot = strrchr(g->name, '.');
        if (dot) *dot = 0;
    }
    if (!g->system_name[0]) snprintf(g->system_name, sizeof(g->system_name), "%s", g->system);
    return g->rom[0] && g->system[0] ? 0 : -1;
}

static void safe_name(char *s)
{
    for (; *s; s++) if (strchr("/\\:*?\"<>|", *s)) *s = '_';
}

void scheda_card_path(const char *saves_dir, const GameInfo *g, char *out, size_t n)
{
    char sys[64], rom[300];
    snprintf(sys, sizeof(sys), "%s", g->system);
    const char *base = strrchr(g->rom, '/');
    snprintf(rom, sizeof(rom), "%s", base ? base + 1 : g->rom);
    char *dot = strrchr(rom, '.');
    if (dot && dot != rom) *dot = 0;
    safe_name(sys);
    safe_name(rom);
    snprintf(out, n, "%s/schede/%s/%s.md", saves_dir, sys, rom);
}

char *scheda_system_prompt(const char *language)
{
    Buf b = { 0 };
    buf_printf(&b,
        "You write short game cards for a retro handheld console with a small screen (640x480). Write in %s, "
        "with a warm, clear and concrete tone. Use exactly these Markdown sections, with the headings translated "
        "into %s: '## In short' (one line: year, developer or publisher, genre, number of players), "
        "'## What it is about' (two or three sentences, no spoilers), '## How to play' (goal and typical "
        "controls, two to four sentences), '## Tips to get started' (three short bullet points), '## Trivia' "
        "(two short bullet points). No tables, no emoji, no other sections, about 200 words in all. "
        "The player owns the game; the cover picture, if attached, helps you recognise the exact release. "
        "Never invent facts: if you are not sure about a date, a name or a detail, leave it out or say it is "
        "uncertain; if you do not recognise the game, say so in the first section and write only what the title, "
        "the system and the cover suggest.", language, language);
    return buf_steal(&b);
}

char *scheda_user_prompt(const GameInfo *g)
{
    Buf b = { 0 };
    const char *base = strrchr(g->rom, '/');
    buf_printf(&b, "Game: %s\nSystem: %s\nFile: %s\nWrite the game card.", g->name, g->system_name, base ? base + 1 : g->rom);
    return buf_steal(&b);
}

/* ------------------------------------------------------------------ file */
static char *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    Buf b = { 0 };
    char tmp[8192];
    size_t n;
    while ((n = fread(tmp, 1, sizeof(tmp), f)) > 0) buf_add(&b, tmp, n);
    fclose(f);
    buf_add(&b, "", 0);
    if (len) *len = b.len;
    return buf_steal(&b);
}

static void mkdirs_for(const char *file)
{
    char d[700];
    snprintf(d, sizeof(d), "%s", file);
    for (char *p = d + 1; *p; p++) {
        if (*p != '/' && *p != '\\') continue;
        char c = *p;
        *p = 0;
        MKDIR(d);
        *p = c;
    }
}

static void save_card(Scheda *s)
{
    if (!s->text || !s->text[0]) return;
    mkdirs_for(s->card_path);
    FILE *f = fopen(s->card_path, "wb");
    if (!f) return;
    fputs(s->text, f);
    fclose(f);
}

/* ------------------------------------------------------------------ scrittura della scheda */
static void set_text(Scheda *s, const char *t)
{
    free(s->text);
    s->text = t ? strdup(t) : NULL;
    s->wrapped_len = (size_t)-1;
}

static void start_generation(Scheda *s)
{
    if (s->busy) return;
    s->error[0] = 0;
    IaSettings st;
    ia_settings_read(s->saves, &st);
    const Provider *p;
    const Model *m;
    char key[256];
    if (ia_console_model(&s->reg, &st, s->saves, &p, &m, key, sizeof(key), s->error, sizeof(s->error))) return;
    snprintf(s->model_name, sizeof(s->model_name), "%s", m->name);
    /* la copertina aiuta a riconoscere l'edizione giusta (se il modello legge le immagini) */
    char *img_b64 = NULL;
    const char *mime = "image/png";
    size_t il = 0;
    char *img = s->g.image[0] ? read_file(s->g.image, &il) : NULL;
    if (img && il > 16 && il < 4u * 1024 * 1024) {
        if ((unsigned char)img[0] == 0xFF && (unsigned char)img[1] == 0xD8) mime = "image/jpeg";
        else if (!memcmp(img, "RIFF", 4)) mime = "image/webp";
        img_b64 = b64_encode((unsigned char *)img, il);
    }
    free(img);
    char *sys = scheda_system_prompt(i18n_language()), *user = scheda_user_prompt(&s->g);
    AskSpec q = { sys, user, img_b64, mime, 1500, "low" };
    char url[260];
    char *body = ask_request(p, m, &q, url, sizeof(url));
    Buf h = { 0 };
    provider_auth_headers(p, key, &h);
    buf_adds(&h, "Content-Type: application/json\n");
    memset(key, 0, sizeof(key));
    reply_init(&s->reply, p->proto == PROTO_ANTHROPIC ? PROTO_ANTHROPIC : PROTO_OPENAI, p->name);
    s->reply.state = RS_WAITING;
    set_text(s, NULL);
    s->scroll = 0;
    s->busy = 1;
    if (!s->offline) {
        Request rq = { url, h.p, body, 0, NULL, NULL, 180 };
        char msg[200];
        if (net_start(&s->xfer, &rq, msg, sizeof(msg))) { snprintf(s->error, sizeof(s->error), "%s", msg); s->busy = 0; }
    }
    if (h.p) memset(h.p, 0, h.len);
    buf_free(&h);
    free(body);
    free(sys);
    free(user);
    free(img_b64);
}

static void finish_generation(Scheda *s)
{
    s->busy = 0;
    char *t = reply_text(&s->reply);
    if (s->reply.state == RS_DONE && t[0]) {
        time_t now = time(NULL);
        char stamp[32] = "";
        struct tm *tm = localtime(&now);
        if (tm) strftime(stamp, sizeof(stamp), "%d/%m/%Y", tm);
        Buf b = { 0 };
        buf_adds(&b, t);
        buf_printf(&b, "\n\n---\n");
        buf_printf(&b, tr("Scheda scritta da %s il %s: può contenere errori."), s->model_name, stamp);
        buf_adds(&b, "\n");
        free(t);
        t = buf_steal(&b);
        set_text(s, t);
        save_card(s);
    } else {
        snprintf(s->error, sizeof(s->error), "%s", s->reply.state == RS_REFUSED ? tr("Il modello non ha risposto a questa richiesta.")
                                                  : (s->reply.error[0] ? tr(s->reply.error) : tr("La risposta si è interrotta.")));
        set_text(s, t[0] ? t : NULL);
    }
    free(t);
    reply_free(&s->reply);
}

static void reply_sink(void *ud, const char *d, size_t n) { reply_feed(ud, d, n); }

/* ------------------------------------------------------------------ lettura ad alta voce */
static void speech_stop(Scheda *s)
{
    tts_cancel(&s->tts);
    player_clear(&s->pl);
    free(s->speak_text);
    s->speak_text = NULL;
}

static int speaking(Scheda *s) { return s->tts.active || player_queued(&s->pl) > 0 || s->speak_text; }

static void speech_pump(Scheda *s)
{
    char err[200];
    if (s->tts.active && tts_poll(&s->tts, &s->pl, err, sizeof(err)) && err[0]) {
        snprintf(s->error, sizeof(s->error), "%s", err);
        free(s->speak_text);
        s->speak_text = NULL;
    }
    if (!s->speak_text || s->tts.active || s->offline) return;
    if (player_queued(&s->pl) > s->pl.src_rate * 8) return;
    char *chunk = tts_next_chunk(s->speak_text, &s->speak_pos, s->speak_first ? 220 : 700, 1);
    if (!chunk) { free(s->speak_text); s->speak_text = NULL; return; }
    s->speak_first = 0;
    IaSettings st;
    ia_settings_read(s->saves, &st);
    char key[256], voice[40], msg[200];
    const Provider *tp = ia_tts(&s->reg, &st, s->saves, key, sizeof(key), voice, sizeof(voice));
    if (!tp || tts_start(&s->tts, tp, key, tp->tts_model, voice, chunk, "", &s->pl, msg, sizeof(msg))) {
        snprintf(s->error, sizeof(s->error), "%s", tp ? msg : tr("Per la lettura scegli la sintesi vocale e la sua chiave (Chiedi all'IA, scheda Voce)."));
        free(s->speak_text);
        s->speak_text = NULL;
    }
    memset(key, 0, sizeof(key));
    free(chunk);
}

static void toggle_speech(Scheda *s)
{
    if (speaking(s)) { speech_stop(s); return; }
    if (!s->text || s->busy) return;
    s->speak_text = tts_clean(s->text);
    s->speak_pos = 0;
    s->speak_first = 1;
}

/* ------------------------------------------------------------------ API */
Scheda *scheda_create(float sample_rate, const char *saves_dir, const GameInfo *g)
{
    Scheda *s = calloc(1, sizeof(Scheda));
    if (!s) return NULL;
    s->sr = sample_rate;
    snprintf(s->saves, sizeof(s->saves), "%s", saves_dir);
    s->g = *g;
    s->xfer.pid = -1;
    s->tts.xfer.pid = -1;
    s->wrapped_len = (size_t)-1;
    registry_load(&s->reg, s->saves);
    player_init(&s->pl);
    reply_init(&s->reply, PROTO_ANTHROPIC, "");
    scheda_card_path(s->saves, g, s->card_path, sizeof(s->card_path));
    if (g->image[0]) cirmolo_load_image(g->image, IMG_W, IMG_H, &s->img, &s->img_w, &s->img_h);
    if (!g->rom[0]) snprintf(s->error, sizeof(s->error), "%s", tr("Apri la scheda dal menu di un gioco (tasto del menu sul gioco)."));
    char *saved = read_file(s->card_path, NULL);
    if (saved && saved[0]) set_text(s, saved);
    free(saved);
    return s;
}

void scheda_destroy(Scheda *s)
{
    if (!s) return;
    if (s->busy && !s->offline) net_cancel(&s->xfer);
    speech_stop(s);
    player_free(&s->pl);
    reply_free(&s->reply);
    wrap_free(&s->wrap);
    registry_free(&s->reg);
    free(s->text);
    free(s->img);
    free(s);
}

void scheda_audio(Scheda *s, float *out, int frames)
{
    memset(out, 0, sizeof(float) * 2 * (size_t)frames);
    player_mix(&s->pl, out, frames, s->sr, 0.9f);
}

static void scroll_by(Scheda *s, float dy)
{
    s->scroll += dy;
    if (s->scroll < 0) s->scroll = 0;
}

static void press(Scheda *s, int b)
{
    switch (b) {
    case PAD_UP: scroll_by(s, -46); break;
    case PAD_DOWN: scroll_by(s, 46); break;
    case PAD_LEFT: scroll_by(s, -300); break;
    case PAD_RIGHT: scroll_by(s, 300); break;
    case PAD_Y: speech_stop(s); start_generation(s); break;
    case PAD_X: toggle_speech(s); break;
    case PAD_B: case PAD_MENU: case PAD_SELECT: s->quit = 1; break;
    }
}

void scheda_button(Scheda *s, int b, int pressed)
{
    if (b < 0 || b >= PAD_COUNT) return;
    if (pressed && !s->down[b]) { s->down[b] = 1; s->rep[b] = 0; press(s, b); }
    else if (!pressed) s->down[b] = 0;
}

void scheda_axes(Scheda *s, float ly, float ry)
{
    float v = fabsf(ry) > 0.25f ? ry : (fabsf(ly) > 0.25f ? ly : 0.0f);
    if (v != 0.0f) scroll_by(s, v * 9.0f);
}

void scheda_update(Scheda *s, float dt)
{
    s->time += dt;
    for (int b = PAD_UP; b <= PAD_DOWN; b++) {
        if (!s->down[b]) continue;
        s->rep[b] += dt;
        while (s->rep[b] > 0.38f) { s->rep[b] -= 0.06f; press(s, b); }
    }
    if (s->busy && !s->offline) {
        int code = 0;
        char err[200];
        if (net_poll(&s->xfer, reply_sink, &s->reply, &code, err, sizeof(err))) {
            reply_finish(&s->reply, code, err);
            finish_generation(s);
        }
    }
    speech_pump(s);
    /* prima apertura: la scheda non c'e' ancora, si scrive subito */
    if (!s->text && !s->busy && !s->error[0] && s->time > 0.2f) start_generation(s);
}

int scheda_wants_quit(const Scheda *s) { return s->quit; }

/* ------------------------------------------------------------------ disegno */
static uint32_t fnv(uint32_t h, const void *p, size_t n)
{
    const unsigned char *b = p;
    while (n--) { h ^= *b++; h *= 16777619u; }
    return h;
}

int scheda_needs_draw(Scheda *s)
{
    int v[] = { (int)s->scroll, s->busy, s->busy ? (int)(s->time * 3) : 0, speaking(s), (int)strlen(s->error),
                s->text ? (int)strlen(s->text) : -1, (int)(s->busy ? s->reply.nblk : 0) };
    uint32_t h = fnv(2166136261u, v, sizeof(v));
    if (s->busy) for (int i = 0; i < s->reply.nblk; i++) h = fnv(h, &s->reply.blk[i].a.len, sizeof(size_t));
    if (h == s->drawn_key && s->time - s->drawn_time < 1.0f) return 0;
    s->drawn_key = h;
    s->drawn_time = s->time;
    return 1;
}

static void draw_line(Canvas *c, const Wrap *w, const WLine *l, int x, int y, uint32_t col)
{
    char tmp[1024];
    int n = l->len < (int)sizeof(tmp) - 1 ? l->len : (int)sizeof(tmp) - 1;
    memcpy(tmp, w->disp.p + l->start, (size_t)n);
    tmp[n] = 0;
    gfx_text(c, style_font(l->style), x + l->indent, y, tmp, col);
}

static void blit(Canvas *c, const uint32_t *px, int w, int h, int x0, int y0)
{
    for (int y = 0; y < h; y++) {
        int cy = y0 + y;
        if (cy < 0 || cy >= c->h) continue;
        for (int x = 0; x < w; x++) {
            int cx = x0 + x;
            if (cx < 0 || cx >= c->w) continue;
            uint32_t p = px[y * w + x], a = p >> 24;
            if (a == 255) c->px[cy * c->w + cx] = p | 0xFF000000u;
            else if (a) c->px[cy * c->w + cx] = gfx_mix(c->px[cy * c->w + cx], p | 0xFF000000u, a / 255.0f);
        }
    }
}

void scheda_draw(Scheda *s, Canvas *c)
{
    gfx_vgradient(c, 0, 0, c->w, c->h, C_BG_TOP, C_BG_BOT);
    /* testo: la scheda salvata, quella in arrivo o niente */
    const char *src = s->text;
    char *live = NULL;
    if (s->busy) { live = reply_text(&s->reply); src = live; }
    int x0 = s->img ? 14 + IMG_W + 18 : 18, avail = 622 - x0;
    if (src && (strlen(src) != s->wrapped_len || s->wrap_w != avail)) {
        wrap_text(&s->wrap, src, avail);
        s->wrapped_len = strlen(src);
        s->wrap_w = avail;
    }
    int total = src ? s->wrap.height + 20 : 0, view = BOTTOM - TOP;
    float max_scroll = total > view ? (float)(total - view) : 0.0f;
    if (s->busy) s->scroll = max_scroll;          /* mentre scrive, si segue il testo */
    if (s->scroll > max_scroll) s->scroll = max_scroll;
    int y = TOP + 10 - (int)s->scroll;
    if (src) {
        for (int i = 0; i < s->wrap.n; i++) {
            const WLine *l = &s->wrap.line[i];
            int h = style_height(l->style);
            if (y + h > TOP - 4 && y < BOTTOM + 20 && l->style != ST_BLANK) {
                if (l->style == ST_RULE) gfx_rect(c, x0, y + h / 2, avail, 1, C_LINE);
                else {
                    uint32_t col = l->style == ST_HEAD ? C_GOLD : (l->style == ST_QUOTE ? C_MUTED : C_TEXT);
                    if (s->text && !s->busy && i >= s->wrap.n - 2 && strstr(s->text, "\n---\n")) col = C_DIM;   /* firma */
                    draw_line(c, &s->wrap, l, x0, y + h - 6, col);
                }
            }
            y += h;
        }
    }
    free(live);
    if (s->busy && (!src || !src[0])) {
        char t[120];
        snprintf(t, sizeof(t), "%s%.*s", tr("Scrivo la scheda"), (int)(s->time * 3) % 4, "...");
        gfx_text(c, FONT_BODY, x0, TOP + 34, t, C_MUTED);
    }
    if (s->error[0]) {
        gfx_round_rect(c, x0 - 6, BOTTOM - 46, avail + 12, 40, 10, C_PANEL, 1.0f);
        gfx_text(c, FONT_SMALL, x0 + 4, BOTTOM - 21, s->error, C_RED);
    }
    if (max_scroll > 0) {
        float h = (float)view * view / total, sy = TOP + (view - h) * s->scroll / max_scroll;
        gfx_round_rect(c, 630, sy, 4, h, 2, C_LINE, 1.0f);
    }
    /* copertina */
    if (s->img) {
        gfx_round_rect(c, 10, TOP + 6, IMG_W + 8, s->img_h + 8, 8, C_PANEL, 1.0f);
        blit(c, s->img, s->img_w, s->img_h, 14 + (IMG_W - s->img_w) / 2, TOP + 10);
    }
    /* intestazione */
    gfx_rect(c, 0, 0, c->w, TOP - 6, C_BAR);
    gfx_rect(c, 0, TOP - 6, c->w, 1, C_LINE);
    char title[220];
    snprintf(title, sizeof(title), "%s", s->g.name);
    while (gfx_text_width(FONT_TITLE, title) > 600 && strlen(title) > 4) { title[strlen(title) - 4] = 0; strcat(title, "..."); }
    gfx_text(c, FONT_TITLE, 16, 30, title, C_TEXT);
    char sub[200];
    snprintf(sub, sizeof(sub), "%s · %s", s->g.system_name, tr("Scheda del gioco"));
    gfx_text(c, FONT_SMALL, 16, 46, sub, C_MUTED);
    if (speaking(s)) gfx_text_right(c, FONT_SMALL, 626, 46, tr("legge..."), C_VIOLET);
    /* tasti */
    gfx_rect(c, 0, 446, c->w, 34, C_BAR);
    gfx_rect(c, 0, 446, c->w, 1, C_LINE);
    gfx_text_center(c, FONT_SMALL, 320, 468,
                    s->busy ? tr("Scrivo la scheda... · B torna ai giochi")
                            : tr("su/giù scorre · X leggi ad alta voce · Y riscrivi · B torna ai giochi"), C_MUTED);
}

/* prove */
void scheda_set_offline(Scheda *s, int offline) { s->offline = offline; }
int scheda_busy(const Scheda *s) { return s->busy; }
const char *scheda_text(const Scheda *s) { return s->text ? s->text : ""; }
const char *scheda_error(const Scheda *s) { return s->error; }
void scheda_feed(Scheda *s, const char *sse, int finish)
{
    if (!s->busy) return;
    reply_feed(&s->reply, sse, strlen(sse));
    if (finish) { reply_finish(&s->reply, 0, ""); finish_generation(s); }
}
