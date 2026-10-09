/* Cirmolo - "Cosa gioco?" (vedi consigli_app.h). */
#include "consigli_app.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ask.h"
#include "diario.h"
#include "i18n.h"
#include "iaconf.h"
#include "json.h"
#include "llm.h"
#include "mdtext.h"
#include "platform.h"

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
#define C_RED      RGB(236, 96, 104)

#define MAX_PICKS 5
#define THUMB 84

static const char *const TIMES[] = { N_("Pochi minuti"), N_("Mezz'ora"), N_("Un'ora o più") };
static const char *const TIMES_EN[] = { "a few minutes", "about half an hour", "an hour or more" };
static const char *const MOODS[] = { N_("Qualunque cosa"), N_("Azione veloce"), N_("Una bella storia"), N_("Qualcosa di rilassante"),
                                     N_("Una sfida"), N_("Da giocare in due") };
static const char *const MOODS_EN[] = { "anything good", "quick action", "a good story", "something relaxing", "a real challenge",
                                        "something to play with a friend (two players)" };
static const char *const KINDS[] = { N_("Indifferente"), N_("Un gioco nuovo"), N_("Da riprendere") };
#define NTIME 3
#define NMOOD 6
#define NKIND 3

struct Consigli {
    char root[300], saves[400], play_cmd[300];
    float time;
    int scanned, page, row, opt[3], sel, busy, offline, quit;
    Collection coll;
    Registry reg;
    Transfer xfer;
    Reply reply;
    int picks[MAX_PICKS], npicks;
    char why[MAX_PICKS][300];
    char error[300];
    Buf exclude;                                 /* titoli gia' proposti, per "altri consigli" */
    uint32_t *thumb[MAX_PICKS];
    int tw[MAX_PICKS], th[MAX_PICKS], thumb_tried[MAX_PICKS];
    int down[PAD_COUNT];
    float rep[PAD_COUNT];
    uint32_t drawn_key;
    float drawn_time;
};

/* ------------------------------------------------------------------ prompt e risposta */
char *consigli_system_prompt(const char *language, const char *catalog, const char *digest)
{
    Buf b = { 0 };
    buf_printf(&b,
        "You recommend games to play right now from the player's own collection on a retro handheld console. "
        "Recommend only games that are in the COLLECTION below, writing the system id and the title exactly as "
        "listed. Prefer well-regarded games that fit the request; vary systems when it makes sense; avoid clones, "
        "hacks, prototypes and obscure titles unless they fit especially well. Write each reason in %s: one short, "
        "concrete sentence (why it fits now, what to expect), no spoilers. Reply only with JSON, no other text: "
        "{\"picks\":[{\"system\":\"<id>\",\"title\":\"<title>\",\"why\":\"<sentence>\"}]}\n\n"
        "COLLECTION (one line per system: ## id (name), then titles separated by |)\n", language);
    buf_adds(&b, catalog);
    buf_adds(&b, "\nRECENT PLAY (from the play diary: system | game | sessions, time, last session | where they left off)\n");
    buf_adds(&b, digest && *digest ? digest : "none yet\n");
    return buf_steal(&b);
}

char *consigli_user_prompt(int time, int mood, int kind, const char *exclude)
{
    Buf b = { 0 };
    buf_printf(&b, "Time I have: %s. What I feel like: %s. ", TIMES_EN[time], MOODS_EN[mood]);
    if (kind == 1) buf_adds(&b, "Prefer games I have not played yet (not in the play diary). ");
    if (kind == 2) buf_adds(&b, "Pick only games from the play diary that are worth continuing, and say where I left off. ");
    if (exclude && *exclude) buf_printf(&b, "Do not suggest these again: %s. ", exclude);
    buf_adds(&b, "Suggest 4 games.");
    return buf_steal(&b);
}

int consigli_parse(const Collection *c, const char *reply, int *games, char (*why)[300], int max)
{
    const char *a = strchr(reply, '{'), *z = strrchr(reply, '}');
    if (!a || !z || z < a) return 0;
    JNode *j = json_parse(a, (size_t)(z - a + 1));
    const JNode *picks = json_get(j, "picks");
    int n = 0;
    for (const JNode *p = picks && picks->type == J_ARRAY ? picks->child : NULL; p && n < max; p = p->next) {
        const char *sys = json_str(p, "system"), *title = json_str(p, "title"), *w = json_str(p, "why");
        if (!title) continue;
        int g = collection_find(c, sys, title);
        if (g < 0) continue;
        int dup = 0;
        for (int k = 0; k < n; k++) if (games[k] == g) dup = 1;
        if (dup) continue;
        games[n] = g;
        snprintf(why[n], 300, "%s", w ? w : "");
        n++;
    }
    json_free(j);
    return n;
}

/* ------------------------------------------------------------------ richiesta */
static void reply_sink(void *ud, const char *d, size_t n) { reply_feed(ud, d, n); }

static void clear_picks(Consigli *s)
{
    for (int i = 0; i < MAX_PICKS; i++) { free(s->thumb[i]); s->thumb[i] = NULL; s->thumb_tried[i] = 0; }
    s->npicks = 0;
    s->sel = 0;
}

static void ask(Consigli *s)
{
    if (s->busy) return;
    s->error[0] = 0;
    clear_picks(s);
    s->page = 1;
    if (!s->coll.n) { snprintf(s->error, sizeof(s->error), "%s", tr("Nessun gioco trovato sulla SD.")); return; }
    const char *fake = getenv("CIRMOLO_RISPOSTA_FINTA");      /* prove sulla console senza rete */
    if (fake && *fake) {
        s->npicks = consigli_parse(&s->coll, fake, s->picks, s->why, MAX_PICKS);
        if (!s->npicks) snprintf(s->error, sizeof(s->error), "%s", tr("Il modello non ha scelto giochi della tua collezione: riprova."));
        return;
    }
    IaSettings st;
    ia_settings_read(s->saves, &st);
    const Provider *p;
    const Model *m;
    char key[256];
    if (ia_console_model(&s->reg, &st, s->saves, &p, &m, key, sizeof(key), s->error, sizeof(s->error))) return;
    char *catalog = collection_catalog(&s->coll), *digest = diary_digest(s->saves, 30);
    char *sys = consigli_system_prompt(i18n_language(), catalog, digest);
    char *user = consigli_user_prompt(s->opt[0], s->opt[1], s->opt[2], s->exclude.p);
    AskSpec q = { sys, user, NULL, NULL, 1200, "low", 1 };
    char url[260];
    char *body = ask_request(p, m, &q, url, sizeof(url));
    Buf h = { 0 };
    provider_auth_headers(p, key, &h);
    buf_adds(&h, "Content-Type: application/json\n");
    memset(key, 0, sizeof(key));
    reply_init(&s->reply, p->proto == PROTO_ANTHROPIC ? PROTO_ANTHROPIC : PROTO_OPENAI, p->name);
    s->reply.state = RS_WAITING;
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
    free(catalog);
    free(digest);
}

static void finish(Consigli *s)
{
    s->busy = 0;
    char *t = reply_text(&s->reply);
    if (s->reply.state == RS_DONE) {
        s->npicks = consigli_parse(&s->coll, t, s->picks, s->why, MAX_PICKS);
        if (!s->npicks) snprintf(s->error, sizeof(s->error), "%s", tr("Il modello non ha scelto giochi della tua collezione: riprova."));
        for (int i = 0; i < s->npicks; i++) {          /* per "altri consigli": questi no */
            if (s->exclude.len) buf_adds(&s->exclude, "; ");
            buf_adds(&s->exclude, s->coll.g[s->picks[i]].title);
        }
    } else {
        snprintf(s->error, sizeof(s->error), "%s", s->reply.state == RS_REFUSED ? tr("Il modello non ha risposto a questa richiesta.")
                                                  : (s->reply.error[0] ? tr(s->reply.error) : tr("La risposta si è interrotta.")));
    }
    free(t);
    reply_free(&s->reply);
}

static void play(Consigli *s)
{
    if (s->sel < 0 || s->sel >= s->npicks) return;
    char *cmd = collection_launch_cmd(&s->coll, s->picks[s->sel]);
    FILE *f = fopen(s->play_cmd, "wb");
    if (f) { fputs(cmd, f); fclose(f); s->quit = 1; }
    else snprintf(s->error, sizeof(s->error), "%s", tr("Impossibile preparare l'avvio del gioco."));
    free(cmd);
}

/* ------------------------------------------------------------------ API */
Consigli *consigli_create(const char *root, const char *saves, const char *play_cmd)
{
    Consigli *s = calloc(1, sizeof(Consigli));
    if (!s) return NULL;
    snprintf(s->root, sizeof(s->root), "%s", root);
    snprintf(s->saves, sizeof(s->saves), "%s", saves);
    snprintf(s->play_cmd, sizeof(s->play_cmd), "%s", play_cmd);
    s->xfer.pid = -1;
    registry_load(&s->reg, s->saves);
    reply_init(&s->reply, PROTO_ANTHROPIC, "");
    remove(s->play_cmd);
    return s;
}

void consigli_destroy(Consigli *s)
{
    if (!s) return;
    if (s->busy && !s->offline) net_cancel(&s->xfer);
    clear_picks(s);
    reply_free(&s->reply);
    collection_free(&s->coll);
    registry_free(&s->reg);
    buf_free(&s->exclude);
    free(s);
}

static void press(Consigli *s, int b)
{
    if (s->page == 0) {
        static const int N[3] = { NTIME, NMOOD, NKIND };
        switch (b) {
        case PAD_UP: s->row = (s->row + 2) % 3; break;
        case PAD_DOWN: s->row = (s->row + 1) % 3; break;
        case PAD_LEFT: s->opt[s->row] = (s->opt[s->row] + N[s->row] - 1) % N[s->row]; break;
        case PAD_RIGHT: s->opt[s->row] = (s->opt[s->row] + 1) % N[s->row]; break;
        case PAD_A: case PAD_START: if (s->scanned) { buf_clear(&s->exclude); ask(s); } break;
        case PAD_B: case PAD_MENU: s->quit = 1; break;
        }
        return;
    }
    switch (b) {
    case PAD_UP: if (s->npicks) s->sel = (s->sel + s->npicks - 1) % s->npicks; break;
    case PAD_DOWN: if (s->npicks) s->sel = (s->sel + 1) % s->npicks; break;
    case PAD_A: case PAD_START: if (!s->busy) play(s); break;
    case PAD_X: if (!s->busy) ask(s); break;
    case PAD_B:
        if (s->busy && !s->offline) net_cancel(&s->xfer);
        if (s->busy) { s->busy = 0; reply_free(&s->reply); }
        s->page = 0;
        s->error[0] = 0;
        break;
    case PAD_MENU: s->quit = 1; break;
    }
}

void consigli_button(Consigli *s, int b, int pressed)
{
    if (b < 0 || b >= PAD_COUNT) return;
    if (pressed && !s->down[b]) { s->down[b] = 1; s->rep[b] = 0; press(s, b); }
    else if (!pressed) s->down[b] = 0;
}

void consigli_update(Consigli *s, float dt)
{
    s->time += dt;
    if (!s->scanned && s->time > 0.05f) {          /* dopo il primo disegno: "leggo la collezione" */
        clock_t t0 = clock();
        time_t w0 = time(NULL);
        collection_scan(&s->coll, s->root);
        s->scanned = 1;
        fprintf(stderr, "collezione: %d giochi in %d sistemi, %.0f ms di CPU, circa %ld s\n", s->coll.n, s->coll.nsys,
                (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC, (long)(time(NULL) - w0));
    }
    if (s->busy && !s->offline) {
        int code = 0;
        char err[200];
        if (net_poll(&s->xfer, reply_sink, &s->reply, &code, err, sizeof(err))) {
            reply_finish(&s->reply, code, err);
            finish(s);
        }
    }
}

int consigli_wants_quit(const Consigli *s) { return s->quit; }

/* ------------------------------------------------------------------ disegno */
static uint32_t fnv(uint32_t h, const void *p, size_t n)
{
    const unsigned char *b = p;
    while (n--) { h ^= *b++; h *= 16777619u; }
    return h;
}

int consigli_needs_draw(Consigli *s)
{
    int v[] = { s->scanned, s->page, s->row, s->opt[0], s->opt[1], s->opt[2], s->sel, s->busy, s->npicks,
                (int)strlen(s->error), s->busy ? (int)(s->time * 3) : 0 };
    uint32_t h = fnv(2166136261u, v, sizeof(v));
    if (h == s->drawn_key && s->time - s->drawn_time < 1.0f) return 0;
    s->drawn_key = h;
    s->drawn_time = s->time;
    return 1;
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
            if (a == 255) c->px[cy * c->w + cx] = p;
            else if (a) c->px[cy * c->w + cx] = gfx_mix(c->px[cy * c->w + cx], p | 0xFF000000u, a / 255.0f);
        }
    }
}

/* Copertina dal Imgs/ del sistema, se c'e' (come la cerca PyUI: stesso nome del file, .png). */
static void load_thumb(Consigli *s, int i)
{
    if (s->thumb_tried[i]) return;
    s->thumb_tried[i] = 1;
    const CollGame *g = &s->coll.g[s->picks[i]];
    char base[400], path[800];
    const char *slash = strrchr(g->rom, '/');
    snprintf(base, sizeof(base), "%s", slash ? slash + 1 : g->rom);
    char *dot = strrchr(base, '.');
    if (dot) *dot = 0;
    snprintf(path, sizeof(path), "%.*s/Imgs/%s.png", slash ? (int)(slash - g->rom) : 0, g->rom, base);
    cirmolo_load_image(path, THUMB, THUMB, &s->thumb[i], &s->tw[i], &s->th[i]);
}

static void header(Canvas *c, const char *title, const char *sub)
{
    gfx_rect(c, 0, 0, c->w, 50, C_BAR);
    gfx_rect(c, 0, 50, c->w, 1, C_LINE);
    gfx_text(c, FONT_TITLE, 16, 30, title, C_TEXT);
    if (sub) gfx_text_right(c, FONT_SMALL, 626, 30, sub, C_MUTED);
}

static void hints(Canvas *c, const char *t)
{
    gfx_rect(c, 0, 446, c->w, 34, C_BAR);
    gfx_rect(c, 0, 446, c->w, 1, C_LINE);
    gfx_text_center(c, FONT_SMALL, 320, 468, t, C_MUTED);
}

void consigli_draw(Consigli *s, Canvas *c)
{
    gfx_vgradient(c, 0, 0, c->w, c->h, C_BG_TOP, C_BG_BOT);
    char sub[80] = "";
    if (s->scanned) snprintf(sub, sizeof(sub), tr("%d giochi sulla SD"), s->coll.n);
    header(c, tr("Cosa gioco?"), s->scanned ? sub : tr("leggo la collezione..."));
    if (s->page == 0) {
        static const char *const Q[3] = { N_("Quanto tempo hai?"), N_("Che voglia hai?"), N_("Nuovo o da riprendere?") };
        const char *const *opts[3] = { TIMES, MOODS, KINDS };
        for (int r = 0; r < 3; r++) {
            int y = 76 + r * 108, sel = r == s->row;
            gfx_round_rect(c, 14, y, 612, 92, 14, sel ? C_PANEL_HI : C_PANEL, 1.0f);
            if (sel) gfx_round_frame(c, 14, y, 612, 92, 14, 1.5f, C_GOLD, 1.0f);
            gfx_text(c, FONT_SMALL, 32, y + 28, tr(Q[r]), sel ? C_GOLD : C_MUTED);
            char v[120];
            snprintf(v, sizeof(v), "\xe2\x80\xb9  %s  \xe2\x80\xba", tr(opts[r][s->opt[r]]));
            gfx_text_center(c, FONT_TITLE, 320, y + 70, v, sel ? C_TEXT : C_MUTED);
        }
        hints(c, tr("su/giù domanda · sinistra/destra risposta · A consigliami · B esci"));
        return;
    }
    if (s->busy) {
        char t[120];
        snprintf(t, sizeof(t), "%s%.*s", tr("Cerco tra i tuoi giochi"), (int)(s->time * 3) % 4, "...");
        gfx_text_center(c, FONT_BODY, 320, 240, t, C_MUTED);
        hints(c, tr("B torna alle domande"));
        return;
    }
    for (int i = 0; i < s->npicks; i++) {
        const CollGame *g = &s->coll.g[s->picks[i]];
        int y = 60 + i * 96, sel = i == s->sel;
        gfx_round_rect(c, 10, y, 620, 90, 12, sel ? C_PANEL_HI : C_PANEL, 1.0f);
        if (sel) gfx_round_frame(c, 10, y, 620, 90, 12, 1.5f, C_GOLD, 1.0f);
        load_thumb(s, i);
        int tx = 22;
        if (s->thumb[i]) {
            blit(c, s->thumb[i], s->tw[i], s->th[i], 18 + (THUMB - s->tw[i]) / 2, y + 3 + (THUMB - s->th[i]) / 2);
            tx = 18 + THUMB + 12;
        }
        char title[200];
        snprintf(title, sizeof(title), "%s", g->title);
        while (gfx_text_width(FONT_BOLD, title) > 470 && strlen(title) > 4) { title[strlen(title) - 4] = 0; strcat(title, "..."); }
        gfx_text(c, FONT_BOLD, tx, y + 24, title, sel ? C_GOLD : C_TEXT);
        gfx_text_right(c, FONT_SMALL, 620, y + 24, s->coll.sys[g->sys].label, C_DIM);
        Wrap w = { 0 };
        wrap_plain(&w, s->why[i], 618 - tx);
        for (int k = 0; k < w.n && k < 3; k++) {
            char tmp[400];
            int n = w.line[k].len < (int)sizeof(tmp) - 1 ? w.line[k].len : (int)sizeof(tmp) - 1;
            memcpy(tmp, w.disp.p + w.line[k].start, (size_t)n);
            tmp[n] = 0;
            gfx_text(c, FONT_SMALL, tx, y + 46 + k * 19, tmp, C_MUTED);
        }
        wrap_free(&w);
    }
    if (s->error[0]) {
        gfx_round_rect(c, 14, 380, 612, 50, 12, C_PANEL, 1.0f);
        gfx_text_center(c, FONT_SMALL, 320, 410, s->error, C_RED);
    }
    hints(c, s->npicks ? tr("A gioca · X altri consigli · B torna alle domande") : tr("X riprova · B torna alle domande"));
}

/* prove */
void consigli_set_offline(Consigli *s, int offline) { s->offline = offline; }
int consigli_busy(const Consigli *s) { return s->busy; }
int consigli_count(const Consigli *s) { return s->npicks; }
int consigli_page(const Consigli *s) { return s->page; }
const char *consigli_error(const Consigli *s) { return s->error; }
const Collection *consigli_collection(const Consigli *s) { return &s->coll; }
void consigli_feed(Consigli *s, const char *sse, int fin)
{
    if (!s->busy) return;
    reply_feed(&s->reply, sse, strlen(sse));
    if (fin) { reply_finish(&s->reply, 0, ""); finish(s); }
}
