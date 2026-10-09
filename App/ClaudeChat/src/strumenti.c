/* Chiedi all'IA - strumenti per agire sulla console (vedi strumenti.h). */
#include "strumenti.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diario.h"
#include "i18n.h"
#include "json.h"

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

typedef struct { const char *name, *desc, *params; } ToolDef;

static const ToolDef TOOLS[] = {
    { "stato_console",
      "Get the console status: battery level and charging, date and time, Wi-Fi network, free space on the SD card, "
      "CPU temperature, current volume (0-20) and screen brightness (0-10).",
      "{\"type\":\"object\",\"properties\":{}}" },
    { "imposta_volume",
      "Set the sound volume of the console, from 0 (mute) to 20 (max). Default is around 5.",
      "{\"type\":\"object\",\"properties\":{\"livello\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":20}},\"required\":[\"livello\"]}" },
    { "imposta_luminosita",
      "Set the screen brightness, from 0 (darkest) to 10 (brightest).",
      "{\"type\":\"object\",\"properties\":{\"livello\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":10}},\"required\":[\"livello\"]}" },
    { "cerca_giochi",
      "Search the games on the console's SD card by words of the title (partial words are fine), optionally in one "
      "system (folder id such as SFC, GBA, MD, PS, ARCADE). Returns up to 12 matches as 'SYSTEM | title'.",
      "{\"type\":\"object\",\"properties\":{\"testo\":{\"type\":\"string\"},\"sistema\":{\"type\":\"string\"}},\"required\":[\"testo\"]}" },
    { "avvia_gioco",
      "Start a game from the SD card. The chat closes and the game starts as soon as your reply ends, so tell the "
      "user in one short sentence. If you are not sure of the exact title, use cerca_giochi first.",
      "{\"type\":\"object\",\"properties\":{\"titolo\":{\"type\":\"string\"},\"sistema\":{\"type\":\"string\"}},\"required\":[\"titolo\"]}" },
    { "diario_partite",
      "Recent play sessions from the play diary, most recent first: system, game, number of sessions, total time, last "
      "session and where the player left off.",
      "{\"type\":\"object\",\"properties\":{}}" },
};
#define NTOOLS ((int)(sizeof(TOOLS) / sizeof(TOOLS[0])))

static char *build(int openai)
{
    Buf b = { 0 };
    buf_adds(&b, "[");
    for (int i = 0; i < NTOOLS; i++) {
        if (i) buf_adds(&b, ",");
        if (openai) {
            buf_adds(&b, "{\"type\":\"function\",\"function\":{\"name\":");
            json_escape(&b, TOOLS[i].name);
            buf_adds(&b, ",\"description\":");
            json_escape(&b, TOOLS[i].desc);
            buf_adds(&b, ",\"parameters\":");
            buf_adds(&b, TOOLS[i].params);
            buf_adds(&b, "}}");
        } else {
            buf_adds(&b, "{\"name\":");
            json_escape(&b, TOOLS[i].name);
            buf_adds(&b, ",\"description\":");
            json_escape(&b, TOOLS[i].desc);
            buf_adds(&b, ",\"input_schema\":");
            buf_adds(&b, TOOLS[i].params);
            buf_adds(&b, "}");
        }
    }
    buf_adds(&b, "]");
    return buf_steal(&b);
}

const char *strumenti_anthropic(void)
{
    static char *s;
    if (!s) s = build(0);
    return s;
}

const char *strumenti_openai(void)
{
    static char *s;
    if (!s) s = build(1);
    return s;
}

static char *dupstr(const char *s)
{
    size_t n = strlen(s);
    char *p = malloc(n + 1);
    if (!p) abort();
    memcpy(p, s, n + 1);
    return p;
}

/* ia-azioni.sh <azione> [numero]: una riga di risposta. */
static char *run_action(ToolCtx *ctx, const char *action, int value, int has_value)
{
    char cmd[600];
    if (has_value) snprintf(cmd, sizeof(cmd), "sh \"%s%sia-azioni.sh\" %s %d 2>/dev/null", ctx->app_dir, *ctx->app_dir ? "/" : "", action, value);
    else snprintf(cmd, sizeof(cmd), "sh \"%s%sia-azioni.sh\" %s 2>/dev/null", ctx->app_dir, *ctx->app_dir ? "/" : "", action);
    FILE *p = popen(cmd, "r");
    if (!p) return dupstr("error: the console did not answer");
    Buf b = { 0 };
    char tmp[512];
    size_t n;
    while ((n = fread(tmp, 1, sizeof(tmp), p)) > 0) buf_add(&b, tmp, n);
    pclose(p);
    buf_add(&b, "", 0);
    char *s = buf_steal(&b);
    s[strcspn(s, "\r\n")] = 0;
    if (!s[0]) { free(s); return dupstr("error: the console did not answer"); }
    return s;
}

static void ensure_collection(ToolCtx *ctx)
{
    if (!*ctx->coll_loaded) {
        collection_scan(ctx->coll, ctx->root);
        *ctx->coll_loaded = 1;
    }
}

char *strumento_esegui(ToolCtx *ctx, const char *name, const char *args_json)
{
    JNode *a = json_parse(args_json && *args_json ? args_json : "{}", args_json && *args_json ? strlen(args_json) : 2);
    char *out = NULL;
    ctx->note[0] = 0;
    if (!strcmp(name, "stato_console")) {
        out = run_action(ctx, "stato", 0, 0);
    } else if (!strcmp(name, "imposta_volume") || !strcmp(name, "imposta_luminosita")) {
        int vol = name[8] == 'v';
        double v = json_num(a, "livello", -1);
        int lv = (int)(v + 0.5), max = vol ? 20 : 10;
        if (v < 0 || lv > max) {
            char e[80];
            snprintf(e, sizeof(e), "error: livello must be 0-%d", max);
            out = dupstr(e);
        } else {
            out = run_action(ctx, vol ? "volume" : "luminosita", lv, 1);
            if (strncmp(out, "error", 5)) snprintf(ctx->note, sizeof(ctx->note), vol ? tr("Volume a %d su 20") : tr("Luminosità a %d su 10"), lv);
        }
    } else if (!strcmp(name, "cerca_giochi")) {
        ensure_collection(ctx);
        int idx[12];
        int n = collection_search(ctx->coll, json_str(a, "testo"), json_str(a, "sistema"), idx, 12);
        Buf b = { 0 };
        if (!n) buf_adds(&b, "no games found on the SD card for that search");
        for (int i = 0; i < n; i++)
            buf_printf(&b, "%s%s | %s", i ? "\n" : "", ctx->coll->sys[ctx->coll->g[idx[i]].sys].id, ctx->coll->g[idx[i]].title);
        out = buf_steal(&b);
    } else if (!strcmp(name, "avvia_gioco")) {
        ensure_collection(ctx);
        const char *title = json_str(a, "titolo");
        int g = title ? collection_find(ctx->coll, json_str(a, "sistema"), title) : -1;
        if (g < 0) {
            out = dupstr("not found on the SD card: search with cerca_giochi and use a title from the results");
        } else {
            char *cmd = collection_launch_cmd(ctx->coll, g);
            FILE *f = fopen(ctx->play_cmd, "wb");
            if (f) {
                fputs(cmd, f);
                fclose(f);
                ctx->launched = 1;
                Buf b = { 0 };
                buf_printf(&b, "ok: %s | %s will start when your reply ends", ctx->coll->sys[ctx->coll->g[g].sys].id, ctx->coll->g[g].title);
                out = buf_steal(&b);
                snprintf(ctx->note, sizeof(ctx->note), tr("Avvio %s"), ctx->coll->g[g].title);
            } else out = dupstr("error: could not prepare the game launch");
            free(cmd);
        }
    } else if (!strcmp(name, "diario_partite")) {
        char *d = diary_digest(ctx->saves, 20);
        out = d ? d : dupstr("the play diary is empty");
    } else {
        out = dupstr("error: unknown tool");
    }
    json_free(a);
    return out;
}
