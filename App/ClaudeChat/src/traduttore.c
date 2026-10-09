/* Cirmolo - traduttore dei giochi: un piccolo server per il "servizio IA" di RetroArch (1.22).
 *
 * RetroArch, con SELECT + giu' (lo imposta Chiedi all'IA, scheda Console), mette in pausa il gioco e manda
 * una POST a http://127.0.0.1:4404/?source_lang=..&target_lang=..&output=.. con il JSON
 * {"image": PNG in base64, "label": "<sistema>__<gioco>", "state": {...}}. Il server passa la schermata al
 * modello scelto per le immagini, disegna la traduzione in un riquadro su un PNG trasparente 640x480 (RetroArch
 * lo stende su tutto lo schermo) e risponde {"image": ...}, piu' {"sound": WAV} se e' attiva la voce.
 * Un testo da solo RetroArch non lo mostra (lo leggerebbe espeak, che sulla Flip non c'e'): per questo si
 * risponde sempre con un'immagine, anche per gli errori. SELECT + giu' di nuovo toglie il riquadro e
 * riprende il gioco.
 *
 * Impostazioni e chiavi come Chiedi all'IA (Saves/claude): ia.provider e ia.model (il modello per le
 * immagini; se mancano quelli della chat), traduzione.lingua, traduzione.voce, tts, voice.<fornitore>.
 * Ogni traduzione si aggiunge a Saves/claude/traduzioni/<gioco>.txt.
 *
 * Uso: ia-traduttore [porta]  (parte e si ferma con RetroArch, vedi spruce/scripts/emu/lib/ra_functions.sh)
 *      ia-traduttore --prova <schermata.png> <cartella di uscita>  (una richiesta finta, per le prove)
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

#include "ask.h"
#include "gfx.h"
#include "i18n.h"
#include "iaconf.h"
#include "json.h"
#include "png.h"
#include "providers.h"
#include "voice.h"

#define SAVES "/mnt/SDCARD/Saves/claude"
#define FONTS "/mnt/SDCARD/App/PyUI/fonts"
#define OUT_W 640
#define OUT_H 480

static char g_saves[400] = SAVES;

/* ------------------------------------------------------------------ lingue */
static const char *lang_name(const char *code)
{
    static const char *MAP[][2] = {
        { "it", "Italian" }, { "en", "English" }, { "es", "Spanish" }, { "fr", "French" }, { "de", "German" },
        { "pt", "Portuguese" }, { "ca", "Catalan" }, { "pl", "Polish" }, { "ro", "Romanian" }, { "tr", "Turkish" },
        { "ja", "Japanese" }, { "nl", "Dutch" }, { "ko", "Korean" }, { "zh-CN", "Simplified Chinese" },
        { "zh-TW", "Traditional Chinese" }, { "ru", "Russian" }, { "cs", "Czech" }, { "sv", "Swedish" },
    };
    for (size_t i = 0; code && i < sizeof(MAP) / sizeof(MAP[0]); i++) if (!strcmp(code, MAP[i][0])) return MAP[i][1];
    return NULL;
}

/* Valore di un parametro della query (?a=b&c=d); "" se manca. */
static void query_param(const char *query, const char *name, char *out, size_t n)
{
    out[0] = 0;
    size_t nl = strlen(name);
    for (const char *p = query; p && *p;) {
        if (!strncmp(p, name, nl) && p[nl] == '=') {
            const char *v = p + nl + 1;
            size_t k = strcspn(v, "& ");
            if (k >= n) k = n - 1;
            memcpy(out, v, k);
            out[k] = 0;
            return;
        }
        p = strchr(p, '&');
        if (p) p++;
    }
}

/* ------------------------------------------------------------------ disegno del riquadro */
#define C_PANEL RGB(16, 20, 28)
#define C_TEXT RGB(240, 240, 246)
#define C_GOLD RGB(242, 196, 92)
#define C_MUTED RGB(150, 160, 182)

typedef struct { const char *s; int n; } Line;

/* Spezza text in righe larghe al massimo w con il carattere font; restituisce quante (fino a max). */
static int wrap(const char *text, int font, int w, Line *out, int max)
{
    int n = 0;
    const char *p = text;
    char tmp[1024];
    while (*p && n < max) {
        const char *nl = strchr(p, '\n');
        const char *end = nl ? nl : p + strlen(p);
        const char *ls = p;
        while (ls < end) {
            const char *best = NULL, *q = ls;
            while (q <= end) {
                const char *sp = q;
                while (sp < end && *sp != ' ') sp++;
                size_t len = (size_t)(sp - ls);
                if (len >= sizeof(tmp)) len = sizeof(tmp) - 1;
                memcpy(tmp, ls, len);
                tmp[len] = 0;
                if (gfx_text_width(font, tmp) > w) break;
                best = sp;
                if (sp >= end) break;
                q = sp + 1;
            }
            if (!best) {                          /* una parola sola troppo lunga: si taglia a caratteri */
                const char *c = ls;
                while (c < end) {
                    const char *nx = c + 1;
                    while (nx < end && ((unsigned char)*nx & 0xC0) == 0x80) nx++;
                    size_t len = (size_t)(nx - ls);
                    if (len >= sizeof(tmp)) break;
                    memcpy(tmp, ls, len);
                    tmp[len] = 0;
                    if (gfx_text_width(font, tmp) > w && c > ls) break;
                    c = nx;
                }
                best = c > ls ? c : end;
            }
            if (n < max) out[n++] = (Line){ ls, (int)(best - ls) };
            ls = best;
            while (ls < end && *ls == ' ') ls++;
        }
        if (nl && ls == p && n < max) out[n++] = (Line){ p, 0 };   /* riga vuota */
        p = nl ? nl + 1 : end;
    }
    return n;
}

/* PNG 640x480 trasparente con un riquadro in basso: titolo e testo. */
static unsigned char *render_overlay(const char *title, const char *text, int error, size_t *png_len)
{
    static uint32_t px[OUT_W * OUT_H], mask[OUT_W * OUT_H];
    Canvas c = { px, OUT_W, OUT_H }, m = { mask, OUT_W, OUT_H };
    gfx_clear(&c, C_PANEL);
    gfx_clear(&m, RGB(0, 0, 0));
    enum { MAXL = 40 };
    Line lines[MAXL];
    int font = FONT_BODY, lh = 24, w = OUT_W - 2 * 10 - 2 * 18;
    int n = wrap(text, font, w, lines, MAXL);
    int maxh = OUT_H * 6 / 10;                    /* al massimo il 60% dello schermo */
    if (34 + n * lh + 16 > maxh) { font = FONT_SMALL; lh = 19; n = wrap(text, font, w, lines, MAXL); }
    int fit = (maxh - 34 - 16) / lh, cut = n > fit;
    if (cut) n = fit;
    int ph = 34 + n * lh + 16, py = OUT_H - 10 - ph;
    gfx_round_rect(&m, 10, py, OUT_W - 20, ph, 14, RGB(255, 255, 255), 1.0f);
    gfx_round_frame(&c, 10, py, OUT_W - 20, ph, 14, 1.5f, error ? RGB(236, 96, 104) : C_GOLD, 0.9f);
    gfx_text(&c, FONT_SMALL, 28, py + 24, title, error ? RGB(236, 96, 104) : C_GOLD);
    char tmp[1024];
    for (int i = 0; i < n; i++) {
        int len = lines[i].n < (int)sizeof(tmp) - 4 ? lines[i].n : (int)sizeof(tmp) - 4;
        memcpy(tmp, lines[i].s, (size_t)len);
        tmp[len] = 0;
        if (cut && i == n - 1) strcat(tmp, "\xe2\x80\xa6");
        gfx_text(&c, font, 28, py + 34 + (i + 1) * lh - 6, tmp, C_TEXT);
    }
    /* trasparenza dalla maschera: il riquadro quasi opaco, il resto invisibile */
    for (int i = 0; i < OUT_W * OUT_H; i++) {
        uint32_t a = mask[i] >> 16 & 255;
        px[i] = (px[i] & 0x00FFFFFFu) | a << 24;
    }
    return png_encode_argb(px, OUT_W, OUT_H, png_len);
}

/* ------------------------------------------------------------------ traduzione */
static void log_translation(const char *game, const char *model, const char *text)
{
    char dir[480], path[700], name[200];
    snprintf(dir, sizeof(dir), "%s/traduzioni", g_saves);
    MKDIR(dir);
    snprintf(name, sizeof(name), "%s", game && *game ? game : "gioco");
    for (char *p = name; *p; p++) if (strchr("/\\:*?\"<>|", *p)) *p = '_';
    snprintf(path, sizeof(path), "%s/%s.txt", dir, name);
    FILE *f = fopen(path, "ab");
    if (!f) return;
    time_t now = time(NULL);
    char stamp[32] = "";
    struct tm *tm = localtime(&now);
    if (tm) strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M", tm);
    fprintf(f, "[%s, %s]\n%s\n\n", stamp, model, text);
    fclose(f);
}

/* Risposta JSON per RetroArch (malloc). */
static char *translate(const char *query, const char *body, size_t body_len)
{
    IaSettings st;
    ia_settings_read(g_saves, &st);
    char target[16], source[16], output[64];
    query_param(query, "target_lang", target, sizeof(target));
    query_param(query, "source_lang", source, sizeof(source));
    query_param(query, "output", output, sizeof(output));
    if (!target[0]) snprintf(target, sizeof(target), "%s", st.lang[0] ? st.lang : lang_code(i18n_language()));
    if (!target[0]) snprintf(target, sizeof(target), "it");
    const char *tname = lang_name(target) ? lang_name(target) : "Italian";
    JNode *j = json_parse(body, body_len);
    const char *image = json_str(j, "image"), *label = json_str(j, "label");
    char sys[64] = "", game[200] = "";
    if (label) {
        const char *sep = strstr(label, "__");
        if (sep) { snprintf(sys, sizeof(sys), "%.*s", (int)(sep - label), label); snprintf(game, sizeof(game), "%s", sep + 2); }
        else snprintf(game, sizeof(game), "%s", label);
    }
    char title[200], err[300] = "", *text = NULL;
    int error = 1;
    Registry reg;
    registry_load(&reg, g_saves);
    const Provider *p = NULL;
    const Model *m = NULL;
    char key[256] = "", cfg_err[300] = "";
    int cfg_ok = ia_console_model(&reg, &st, g_saves, &p, &m, key, sizeof(key), cfg_err, sizeof(cfg_err)) == 0;
    snprintf(title, sizeof(title), "%s", tr("Traduzione"));
    if (!image) snprintf(err, sizeof(err), "%s", tr("RetroArch non ha mandato la schermata."));
    else if (!cfg_ok) snprintf(err, sizeof(err), "%s", cfg_err);
    else {
        char sysmsg[900], prompt[600];
        snprintf(sysmsg, sizeof(sysmsg),
                 "You translate the text in video game screenshots for a player on a handheld console. Reply only "
                 "with the translation into %s: no comments, no quotes, no notes about the picture. Keep the order in "
                 "which the text appears and its line breaks; keep a speaker's name before the line as 'Name: text'. "
                 "Translate menus item by item, one per line. Keep it natural and faithful to the game's tone. If there "
                 "is no readable text, or it is already in %s, reply exactly NO_TEXT.", tname, tname);
        const char *sname = source[0] ? lang_name(source) : NULL;
        snprintf(prompt, sizeof(prompt), "Game: %s%s%s%s. Source language: %s. Translate the text in this screenshot into %s.",
                 game[0] ? game : "unknown", sys[0] ? " (" : "", sys, sys[0] ? ")" : "",
                 sname ? sname : "detect it (it is often Japanese)", tname);
        AskSpec q = { sysmsg, prompt, image, "image/png", 2048, "low" };
        time_t t0 = time(NULL);
        const char *fake = getenv("CIRMOLO_RISPOSTA_FINTA");     /* prove senza rete */
        text = fake ? strdup(fake) : ask_blocking(p, m, key, &q, 90, err, sizeof(err));
        if (text) {
            char *s = text;
            while (*s && isspace((unsigned char)*s)) s++;
            if (!strncmp(s, "NO_TEXT", 7)) { free(text); text = NULL; snprintf(err, sizeof(err), "%s", tr("Nessun testo da tradurre in questa schermata.")); }
            else {
                error = 0;
                snprintf(title, sizeof(title), tr("Traduzione in %ld s con %s · SELECT + giù per continuare"), (long)(time(NULL) - t0), m->name);
                log_translation(game, m->id, text);
            }
        }
    }
    memset(key, 0, sizeof(key));
    size_t png_len = 0;
    unsigned char *png = render_overlay(title, text ? text : err, error, &png_len);
    char *b64 = png ? b64_encode(png, png_len) : NULL;
    free(png);
    Buf out = { 0 };
    /* "press":"pause": per mostrare il riquadro RetroArch toglie la pausa e dovrebbe rimetterla dopo un
       fotogramma, ma la 1.22.2 della Flip non lo fa e il gioco ripartiva sotto la traduzione; i tasti di
       "press" si eseguono dopo l'immagine, cosi' il gioco resta fermo finche' non si preme SELECT + giu'. */
    buf_adds(&out, "{\"press\":\"pause\",\"image\":\"");
    if (b64) buf_adds(&out, b64);
    buf_adds(&out, "\"");
    free(b64);
    /* voce: se e' attiva nelle impostazioni o RetroArch e' in modalita' parlato */
    char tkey[256], voice[40];
    const Provider *tp = text && (st.speak || strstr(output, "sound")) ? ia_tts(&reg, &st, g_saves, tkey, sizeof(tkey), voice, sizeof(voice)) : NULL;
    if (tp) {
        char terr[200];
        size_t wl = 0;
        char *wav = tts_wav_blocking(tp, tkey, tp->tts_model, voice, text, &wl, terr, sizeof(terr));
        memset(tkey, 0, sizeof(tkey));
        if (wav) {
            char *wb = b64_encode((unsigned char *)wav, wl);
            buf_adds(&out, ",\"sound\":\"");
            buf_adds(&out, wb);
            buf_adds(&out, "\"");
            free(wb);
            free(wav);
        } else fprintf(stderr, "voce: %s\n", terr);
    }
    buf_adds(&out, "}");
    fprintf(stderr, "%s: %s\n", error ? "errore" : "tradotto", error ? err : (game[0] ? game : "?"));
    free(text);
    json_free(j);
    registry_free(&reg);
    return buf_steal(&out);
}

/* ------------------------------------------------------------------ server */
#ifndef _WIN32
static int send_all(int fd, const char *d, size_t n)
{
    while (n) {
        ssize_t w = send(fd, d, n, MSG_NOSIGNAL);
        if (w <= 0) { if (w < 0 && errno == EINTR) continue; return -1; }
        d += w;
        n -= (size_t)w;
    }
    return 0;
}

static void serve(int fd)
{
    Buf in = { 0 };
    char buf[16384];
    size_t need = 0, hdr_end = 0;
    struct timeval tv = { 10, 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    for (;;) {
        ssize_t r = recv(fd, buf, sizeof(buf), 0);
        if (r <= 0) break;
        buf_add(&in, buf, (size_t)r);
        if (!hdr_end) {
            char *e = strstr(in.p, "\r\n\r\n");
            if (!e) { if (in.len > 65536) break; continue; }
            hdr_end = (size_t)(e - in.p) + 4;
            const char *cl = NULL;
            for (char *q = in.p; q < e; q++) if (!strncasecmp(q, "\r\nContent-Length:", 17)) { cl = q + 17; break; }
            need = cl ? (size_t)strtoul(cl, NULL, 10) : 0;
            if (need > 64u * 1024 * 1024) break;
        }
        if (hdr_end && in.len >= hdr_end + need) break;
    }
    if (hdr_end && in.len >= hdr_end + need) {
        char query[512] = "";
        const char *sp = strchr(in.p, ' ');
        const char *q = sp ? strchr(sp + 1, '?') : NULL;
        const char *end = sp ? strchr(sp + 1, ' ') : NULL;
        if (q && end && q < end) snprintf(query, sizeof(query), "%.*s", (int)(end - q - 1), q + 1);
        char *json = translate(query, in.p + hdr_end, need);
        char head[200];
        int hl = snprintf(head, sizeof(head), "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n", strlen(json));
        if (!send_all(fd, head, (size_t)hl)) send_all(fd, json, strlen(json));
        free(json);
    }
    buf_free(&in);
    close(fd);
}

static int run_server(int port)
{
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_DFL);
    int s = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in a = { 0 };
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t)port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   /* solo dalla console stessa */
    if (s < 0 || bind(s, (struct sockaddr *)&a, sizeof(a)) || listen(s, 4)) {
        fprintf(stderr, "porta %d non disponibile: %s\n", port, strerror(errno));
        return 1;
    }
    fprintf(stderr, "traduttore in ascolto su 127.0.0.1:%d\n", port);
    for (;;) {
        int fd = accept(s, NULL, NULL);
        if (fd < 0) { if (errno == EINTR) continue; break; }
        serve(fd);
    }
    close(s);
    return 0;
}
#else
static int run_server(int port) { fprintf(stderr, "sul PC il server non c'e' (porta %d): usa --prova\n", port); return 1; }
#endif

static char *read_all(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    Buf b = { 0 };
    char tmp[8192];
    size_t n;
    while ((n = fread(tmp, 1, sizeof(tmp), f)) > 0) buf_add(&b, tmp, n);
    fclose(f);
    *len = b.len;
    return buf_steal(&b);
}

int main(int argc, char **argv)
{
    /* traduzioni e caratteri: dalla cartella del programma (App/ClaudeChat) e da PyUI */
    char dir[512];
    snprintf(dir, sizeof(dir), "%s", argv[0]);
    char *slash = strrchr(dir, '/');
    if (!slash) slash = strrchr(dir, '\\');
    if (slash) *slash = 0; else snprintf(dir, sizeof(dir), ".");
    char lang[600], f1[600], f2[600];
    snprintf(lang, sizeof(lang), "%s/lang", dir);
    i18n_init(lang);
    const char *fonts = getenv("CIRMOLO_FONTS") ? getenv("CIRMOLO_FONTS") : FONTS;
    snprintf(f1, sizeof(f1), "%s/BeVietnamPro-Regular.ttf", fonts);
    snprintf(f2, sizeof(f2), "%s/BeVietnamPro-SemiBold.ttf", fonts);
    if (gfx_load_fonts(f1, f2)) { fprintf(stderr, "caratteri non trovati in %s\n", fonts); return 1; }
    if (getenv("CIRMOLO_SAVES")) snprintf(g_saves, sizeof(g_saves), "%s", getenv("CIRMOLO_SAVES"));
    if (argc >= 4 && !strcmp(argv[1], "--prova")) {
        /* una richiesta come quella di RetroArch, con la risposta e il riquadro salvati in uscita */
        size_t n = 0;
        char *png = read_all(argv[2], &n);
        if (!png) { fprintf(stderr, "non trovo %s\n", argv[2]); return 1; }
        char *b64 = b64_encode((unsigned char *)png, n);
        Buf body = { 0 };
        buf_adds(&body, "{\"image\":\"");
        buf_adds(&body, b64);
        buf_adds(&body, "\",\"label\":\"snes__Prova\",\"state\":{\"paused\":1}}");
        char *json = translate("output=image,png,png-a&target_lang=it", body.p, body.len);
        JNode *j = json_parse(json, strlen(json));
        const char *img = json_str(j, "image");
        size_t pl = 0;
        unsigned char *ov = img ? b64_decode(img, strlen(img), &pl) : NULL;
        char out[700];
        snprintf(out, sizeof(out), "%s/riquadro.png", argv[3]);
        FILE *f = fopen(out, "wb");
        if (f && ov) fwrite(ov, 1, pl, f);
        if (f) fclose(f);
        printf("risposta: %zu byte, riquadro %s (%zu byte)%s\n", strlen(json), out, pl, json_get(j, "sound") ? ", con voce" : "");
        free(ov);
        json_free(j);
        free(json);
        buf_free(&body);
        free(b64);
        free(png);
        return 0;
    }
    int port = argc > 1 ? atoi(argv[1]) : 4404;
    int rc = run_server(port > 0 ? port : 4404);
    gfx_free_fonts();
    i18n_free();
    return rc;
}
