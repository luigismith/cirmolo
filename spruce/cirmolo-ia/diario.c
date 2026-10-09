/* Cirmolo IA - diario delle partite (vedi diario.h). */
#include "diario.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "i18n.h"
#include "json.h"

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#define MKDIR(p) mkdir(p, 0755)
#endif

int diary_append(const char *saves_dir, const DiaryEntry *e)
{
    char path[520];
    snprintf(path, sizeof(path), "%s/diario", saves_dir);
    MKDIR(path);
    snprintf(path, sizeof(path), "%s/diario/diario.jsonl", saves_dir);
    Buf b = { 0 };
    buf_adds(&b, "{\"rom\":");
    json_escape(&b, e->rom);
    buf_adds(&b, ",\"system\":");
    json_escape(&b, e->system);
    buf_adds(&b, ",\"name\":");
    json_escape(&b, e->name);
    buf_printf(&b, ",\"start\":%ld,\"secs\":%ld,\"img\":", e->start, e->secs);
    json_escape(&b, e->img);
    buf_adds(&b, ",\"summary\":");
    json_escape(&b, e->summary);
    buf_adds(&b, "}\n");
    FILE *f = fopen(path, "ab");
    int ok = f && fwrite(b.p, 1, b.len, f) == b.len;
    if (f) ok &= fclose(f) == 0;
    buf_free(&b);
    return ok ? 0 : -1;
}

/* La SD e' FAT32: /mnt/sdcard/Roms/... (come lo scrive standard_launch.sh, con readlink) e
   /mnt/SDCARD/Roms/... (come lo passa PyUI) sono lo stesso file. */
static int same_path(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        unsigned char x = (unsigned char)*a, y = (unsigned char)*b;
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y) return 0;
    }
    return !*a && !*b;
}

int diary_last(const char *saves_dir, const char *rom, DiaryEntry *out, long *total_secs, int *sessions)
{
    char path[520];
    snprintf(path, sizeof(path), "%s/diario/diario.jsonl", saves_dir);
    memset(out, 0, sizeof(*out));
    if (total_secs) *total_secs = 0;
    if (sessions) *sessions = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    char line[4096];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        JNode *j = json_parse(line, strcspn(line, "\r\n"));
        const char *r = json_str(j, "rom");
        if (r && same_path(r, rom)) {
            const char *v;
            memset(out, 0, sizeof(*out));
            snprintf(out->rom, sizeof(out->rom), "%s", r);
            if ((v = json_str(j, "system"))) snprintf(out->system, sizeof(out->system), "%s", v);
            if ((v = json_str(j, "name"))) snprintf(out->name, sizeof(out->name), "%s", v);
            if ((v = json_str(j, "img"))) snprintf(out->img, sizeof(out->img), "%s", v);
            if ((v = json_str(j, "summary"))) snprintf(out->summary, sizeof(out->summary), "%s", v);
            out->start = (long)json_num(j, "start", 0);
            out->secs = (long)json_num(j, "secs", 0);
            if (total_secs) *total_secs += out->secs;
            if (sessions) (*sessions)++;
            found = 1;
        }
        json_free(j);
    }
    fclose(f);
    return found ? 0 : -1;
}

typedef struct { DiaryEntry last; long total; int sessions; } DiaryGame;

static int by_last_desc(const void *a, const void *b)
{
    long x = ((const DiaryGame *)a)->last.start, y = ((const DiaryGame *)b)->last.start;
    return x < y ? 1 : (x > y ? -1 : 0);
}

char *diary_digest(const char *saves_dir, int max)
{
    char path[520];
    snprintf(path, sizeof(path), "%s/diario/diario.jsonl", saves_dir);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    DiaryGame *g = NULL;
    int n = 0, cap = 0;
    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        JNode *j = json_parse(line, strcspn(line, "\r\n"));
        const char *r = json_str(j, "rom");
        if (r) {
            int k;
            for (k = 0; k < n && !same_path(g[k].last.rom, r); k++) {}
            if (k == n) {
                if (n == cap) { cap = cap ? cap * 2 : 32; g = realloc(g, sizeof(DiaryGame) * (size_t)cap); if (!g) abort(); }
                memset(&g[n], 0, sizeof(g[n]));
                n++;
            }
            DiaryEntry *e = &g[k].last;
            long start = (long)json_num(j, "start", 0), secs = (long)json_num(j, "secs", 0);
            g[k].total += secs;
            g[k].sessions++;
            if (start >= e->start) {
                const char *v;
                snprintf(e->rom, sizeof(e->rom), "%s", r);
                if ((v = json_str(j, "system"))) snprintf(e->system, sizeof(e->system), "%s", v);
                if ((v = json_str(j, "name"))) snprintf(e->name, sizeof(e->name), "%s", v);
                v = json_str(j, "summary");
                snprintf(e->summary, sizeof(e->summary), "%s", v ? v : "");
                e->start = start;
                e->secs = secs;
            }
        }
        json_free(j);
    }
    fclose(f);
    if (!n) { free(g); return NULL; }
    qsort(g, (size_t)n, sizeof(DiaryGame), by_last_desc);
    Buf b = { 0 };
    for (int i = 0; i < n && i < max; i++) {
        char tot[40], date[40];
        diary_duration(g[i].total, tot, sizeof(tot));
        diary_date(g[i].last.start, date, sizeof(date));
        buf_printf(&b, "%s | %s | %d sessions, %s in all, last on %s", g[i].last.system, g[i].last.name, g[i].sessions, tot, date);
        if (g[i].last.summary[0]) buf_printf(&b, " | %s", g[i].last.summary);
        buf_adds(&b, "\n");
    }
    free(g);
    return buf_steal(&b);
}

void diary_name_from_rom(const char *rom, char *out, size_t n)
{
    const char *base = strrchr(rom, '/');
    snprintf(out, n, "%s", base ? base + 1 : rom);
    char *dot = strrchr(out, '.');
    if (dot && dot != out) *dot = 0;
}

void diary_duration(long secs, char *out, size_t n)
{
    long m = (secs + 30) / 60;
    if (m < 1) snprintf(out, n, "%s", tr("meno di un minuto"));
    else if (m < 60) snprintf(out, n, "%ld min", m);
    else if (m % 60 == 0) snprintf(out, n, "%ld h", m / 60);
    else snprintf(out, n, "%ld h %ld min", m / 60, m % 60);
}

void diary_date(long epoch, char *out, size_t n)
{
    time_t t = (time_t)epoch;
    struct tm *tm = localtime(&t);
    out[0] = 0;
    if (tm) strftime(out, n, "%d/%m, %H:%M", tm);
}

char *diary_summary_prompt(const char *language, const DiaryEntry *e)
{
    Buf b = { 0 };
    buf_printf(&b,
        "This is the last frame of a play session of \"%s\" (%s), saved when the player quit after %ld minutes. "
        "In one or two short sentences in %s, speaking to the player in the second person, say where they were and "
        "what they were doing (place, level or chapter, situation, anything useful visible on screen), so they can "
        "pick up again next time. If it is a title screen, a menu or a pause screen, just say so briefly. Do not "
        "reveal anything that is not visible, and do not invent: if you cannot tell, say what you can see. Reply "
        "with the sentences only.", e->name, e->system, (e->secs + 30) / 60, language);
    return buf_steal(&b);
}
