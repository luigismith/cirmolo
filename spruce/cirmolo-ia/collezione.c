/* Cirmolo IA - la collezione di giochi sulla SD (vedi collezione.h). */
#include "collezione.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "json.h"

static char *read_small(const char *path, size_t max)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    Buf b = { 0 };
    char tmp[8192];
    size_t n;
    while ((n = fread(tmp, 1, sizeof(tmp), f)) > 0 && b.len < max) buf_add(&b, tmp, n);
    fclose(f);
    buf_add(&b, "", 0);
    return buf_steal(&b);
}

static int is_dir(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

void collection_clean_title(const char *name, char *out, size_t n)
{
    char tmp[400];
    snprintf(tmp, sizeof(tmp), "%s", name);
    char *dot = strrchr(tmp, '.');
    if (dot && dot != tmp && strlen(dot) <= 5) *dot = 0;
    /* via tutto da " (" o " [" in poi: regione, versione, lingue, tag di dump */
    for (char *p = tmp; *p; p++)
        if ((p[0] == '(' || p[0] == '[') && p > tmp) { *p = 0; break; }
    size_t l = strlen(tmp);
    while (l && (tmp[l - 1] == ' ' || tmp[l - 1] == '_' || tmp[l - 1] == '-')) tmp[--l] = 0;
    for (char *p = tmp; *p; p++) if (*p == '_') *p = ' ';
    snprintf(out, n, "%s", tmp[0] ? tmp : name);
}

/* Quanto preferire un'edizione: italiana, poi europea, poi le altre; le non ufficiali in fondo. */
static int edition_pref(const char *file)
{
    int p = 0;
    if (strstr(file, "(It") || strstr(file, ",It") || strstr(file, "Italy")) p += 50;
    if (strstr(file, "Europe") || strstr(file, "(EU")) p += 30;
    if (strstr(file, "USA") || strstr(file, "World")) p += 20;
    if (strstr(file, "Japan")) p += 5;
    if (strstr(file, "Beta") || strstr(file, "Proto") || strstr(file, "Demo") || strstr(file, "(Unl)") || strstr(file, "Hack")) p -= 40;
    if (strstr(file, "[b")) p -= 60;
    return p;
}

static int ext_ok(const char *exts, const char *file)
{
    const char *dot = strrchr(file, '.');
    if (!dot) return 0;
    if (!exts || !*exts) {
        static const char *SKIP[] = { ".xml", ".txt", ".db", ".png", ".jpg", ".sav", ".srm", ".state", ".cfg", ".json", ".dat" };
        for (size_t i = 0; i < sizeof(SKIP) / sizeof(SKIP[0]); i++) if (!strcasecmp(dot, SKIP[i])) return 0;
        return 1;
    }
    size_t el = strlen(dot + 1);
    for (const char *p = exts; *p;) {
        const char *e = strchr(p, '|');
        size_t k = e ? (size_t)(e - p) : strlen(p);
        if (k == el && !strncasecmp(p, dot + 1, k)) return 1;
        p = e ? e + 1 : p + k;
    }
    return 0;
}

static char *dupn(const char *s, size_t n)
{
    char *p = malloc(n + 1);
    if (!p) abort();
    memcpy(p, s, n);
    p[n] = 0;
    return p;
}

/* Nomi dal gamelist: <path>./file</path> -> <name>. */
typedef struct { char *file, *name; } NameMap;

static int load_gamelist(const char *dir, NameMap **map)
{
    *map = NULL;
    char path[600];
    char *x = NULL;
    for (int i = 0; i < 2 && !x; i++) {
        snprintf(path, sizeof(path), "%s/%s", dir, i ? "gamelist.xml" : "miyoogamelist.xml");
        x = read_small(path, 8u * 1024 * 1024);
    }
    if (!x) return 0;
    int n = 0, cap = 0;
    for (char *g = strstr(x, "<game"); g; g = strstr(g + 5, "<game")) {
        char *end = strstr(g, "</game>");
        if (!end) break;
        char *p = strstr(g, "<path>"), *nm = strstr(g, "<name>");
        if (p && nm && p < end && nm < end) {
            p += 6;
            nm += 6;
            char *pe = strstr(p, "</path>"), *ne = strstr(nm, "</name>");
            if (pe && ne) {
                if (!strncmp(p, "./", 2)) p += 2;
                if (n == cap) {
                    cap = cap ? cap * 2 : 128;
                    NameMap *m = realloc(*map, sizeof(NameMap) * (size_t)cap);
                    if (!m) abort();
                    *map = m;
                }
                (*map)[n].file = dupn(p, (size_t)(pe - p));
                (*map)[n].name = dupn(nm, (size_t)(ne - nm));
                for (char *q = (*map)[n].name; *q; q++) if (!strncmp(q, "&amp;", 5)) memmove(q + 1, q + 5, strlen(q + 5) + 1);
                n++;
            }
        }
        g = end;
    }
    free(x);
    return n;
}

static void add_game(Collection *c, int sys, const char *title, const char *rom, int pref)
{
    for (int i = c->n - 1; i >= 0 && c->g[i].sys == sys; i--)
        if (!strcasecmp(c->g[i].title, title)) {          /* altra edizione dello stesso gioco */
            if (pref > c->g[i].pref) { snprintf(c->g[i].rom, sizeof(c->g[i].rom), "%s", rom); c->g[i].pref = pref; }
            return;
        }
    if (c->n == c->cap) {
        c->cap = c->cap ? c->cap * 2 : 512;
        CollGame *g = realloc(c->g, sizeof(CollGame) * (size_t)c->cap);
        if (!g) abort();
        c->g = g;
    }
    CollGame *g = &c->g[c->n++];
    g->sys = sys;
    snprintf(g->title, sizeof(g->title), "%s", title);
    snprintf(g->rom, sizeof(g->rom), "%s", rom);
    g->pref = pref;
}

static int cmp_name(const void *a, const void *b) { return strcasecmp(*(char *const *)a, *(char *const *)b); }

static void scan_dir(Collection *c, int sys, const char *dir, const char *exts, NameMap *map, int nmap, int depth)
{
    DIR *d = opendir(dir);
    if (!d) return;
    char **names = NULL;
    int n = 0, cap = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        if (n == cap) { cap = cap ? cap * 2 : 256; names = realloc(names, sizeof(char *) * (size_t)cap); if (!names) abort(); }
        names[n++] = strdup(e->d_name);
    }
    closedir(d);
    if (n > 1) qsort(names, (size_t)n, sizeof(char *), cmp_name);   /* in ordine: le edizioni stanno vicine */
    for (int i = 0; i < n; i++) {
        char path[700];
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
        if (!strcasecmp(names[i], "Imgs") || !strcasecmp(names[i], "Imgs_old")) { free(names[i]); continue; }
        if (is_dir(path)) {
            if (depth == 0) scan_dir(c, sys, path, exts, map, nmap, 1);
        } else if (ext_ok(exts, names[i])) {
            char title[200] = "";
            for (int k = 0; k < nmap; k++) if (!strcmp(map[k].file, names[i])) { collection_clean_title(map[k].name, title, sizeof(title)); break; }
            if (!title[0]) collection_clean_title(names[i], title, sizeof(title));
            add_game(c, sys, title, path, edition_pref(names[i]));
        }
        free(names[i]);
    }
    free(names);
}

void collection_scan(Collection *c, const char *root)
{
    memset(c, 0, sizeof(*c));
    char emu[600], roms[600];
    snprintf(emu, sizeof(emu), "%s/Emu", root);
    DIR *d = opendir(emu);
    if (!d) return;
    char **ids = NULL;
    int nid = 0, cap = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        if (nid == cap) { cap = cap ? cap * 2 : 64; ids = realloc(ids, sizeof(char *) * (size_t)cap); if (!ids) abort(); }
        ids[nid++] = strdup(e->d_name);
    }
    closedir(d);
    if (nid > 1) qsort(ids, (size_t)nid, sizeof(char *), cmp_name);
    for (int i = 0; i < nid; i++) {
        char cfgp[700];
        snprintf(cfgp, sizeof(cfgp), "%s/%s/config.json", emu, ids[i]);
        snprintf(roms, sizeof(roms), "%s/Roms/%s", root, ids[i]);
        char *cfg = is_dir(roms) ? read_small(cfgp, 1u << 20) : NULL;
        JNode *j = cfg ? json_parse(cfg, strlen(cfg)) : NULL;
        const char *launch = json_str(j, "launch"), *label = json_str(j, "label"), *exts = json_str(j, "extlist");
        if (launch) {
            GameSystemInfo *s = realloc(c->sys, sizeof(GameSystemInfo) * (size_t)(c->nsys + 1));
            if (!s) abort();
            c->sys = s;
            s = &c->sys[c->nsys];
            snprintf(s->id, sizeof(s->id), "%s", ids[i]);
            snprintf(s->label, sizeof(s->label), "%s", label ? label : ids[i]);
            snprintf(s->launch, sizeof(s->launch), "%s/%s/%s", emu, ids[i], launch);
            NameMap *map;
            int nmap = load_gamelist(roms, &map);
            int before = c->n;
            scan_dir(c, c->nsys, roms, exts, map, nmap, 0);
            for (int k = 0; k < nmap; k++) { free(map[k].file); free(map[k].name); }
            free(map);
            if (c->n > before) c->nsys++;         /* sistemi senza giochi: non servono */
        }
        json_free(j);
        free(cfg);
        free(ids[i]);
    }
    free(ids);
}

void collection_free(Collection *c)
{
    free(c->sys);
    free(c->g);
    memset(c, 0, sizeof(*c));
}

char *collection_catalog(const Collection *c)
{
    Buf b = { 0 };
    int cur = -1;
    for (int i = 0; i < c->n; i++) {
        const CollGame *g = &c->g[i];
        if (g->sys != cur) {
            cur = g->sys;
            buf_printf(&b, "%s## %s (%s)\n", b.len ? "\n" : "", c->sys[cur].id, c->sys[cur].label);
        } else buf_adds(&b, " | ");
        buf_adds(&b, g->title);
    }
    buf_adds(&b, "\n");
    return buf_steal(&b);
}

/* Confronto tollerante: solo lettere e cifre, senza maiuscole. */
static void squash(const char *s, char *out, size_t n)
{
    size_t k = 0;
    for (; *s && k + 1 < n; s++) {
        unsigned char ch = (unsigned char)*s;
        if (isalnum(ch)) out[k++] = (char)tolower(ch);
        else if (ch >= 0x80) out[k++] = (char)ch;
    }
    out[k] = 0;
}

int collection_find(const Collection *c, const char *system, const char *title)
{
    char want[200], have[200];
    squash(title, want, sizeof(want));
    if (!want[0]) return -1;
    int best = -1, best_score = 0;
    for (int i = 0; i < c->n; i++) {
        const CollGame *g = &c->g[i];
        int same_sys = system && !strcasecmp(c->sys[g->sys].id, system);
        squash(g->title, have, sizeof(have));
        int score = 0;
        if (!strcmp(have, want)) score = 100;
        else if (strlen(want) >= 4 && (strstr(have, want) || strstr(want, have))) score = 60;
        if (!score) continue;
        if (same_sys) score += 50;
        if (score > best_score) { best_score = score; best = i; }
    }
    return best;
}

int collection_search(const Collection *c, const char *text, const char *system, int *out, int max)
{
    char words[8][64];
    int nw = 0;
    char tmp[200];
    snprintf(tmp, sizeof(tmp), "%s", text ? text : "");
    for (char *w = strtok(tmp, " -:,.'"); w && nw < 8; w = strtok(NULL, " -:,.'")) {
        squash(w, words[nw], sizeof(words[nw]));
        if (words[nw][0]) nw++;
    }
    if (!nw) return 0;
    int n = 0;
    char have[200];
    for (int i = 0; i < c->n && n < max; i++) {
        if (system && *system && strcasecmp(c->sys[c->g[i].sys].id, system)) continue;
        squash(c->g[i].title, have, sizeof(have));
        int ok = 1;
        for (int k = 0; k < nw && ok; k++) if (!strstr(have, words[k])) ok = 0;
        if (ok) out[n++] = i;
    }
    return n;
}

char *collection_launch_cmd(const Collection *c, int game)
{
    Buf b = { 0 };
    const char *launch = c->sys[c->g[game].sys].launch;
    /* come PyUI (miyoo_trim_common.run_game): virgolette, con $ ` " \ protetti nel nome del file */
    buf_printf(&b, "chmod a+x \"%s\";\"%s\" \"", launch, launch);
    for (const char *p = c->g[game].rom; *p; p++) {
        if (strchr("$`\"\\", *p)) buf_add(&b, "\\", 1);
        buf_add(&b, p, 1);
    }
    buf_adds(&b, "\"\n");
    return buf_steal(&b);
}
