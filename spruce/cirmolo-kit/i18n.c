/* Cirmolo kit - traduzioni delle app native (vedi i18n.h). */
#include "i18n.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PYUI_CONFIG "/mnt/SDCARD/App/PyUI/py-ui-config.json"

typedef struct { char *key, *val; } Pair;
static Pair *g_pairs;
static int g_n, g_cap;
static char g_lang[48] = "Italian";

static char *read_all(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0 || n > 4 * 1024 * 1024) { fclose(f); return NULL; }
    char *s = malloc((size_t)n + 1);
    if (!s) { fclose(f); return NULL; }
    size_t got = fread(s, 1, (size_t)n, f);
    fclose(f);
    s[got] = 0;
    return s;
}

static void put_utf8(char **o, unsigned cp)
{
    char *p = *o;
    if (cp < 0x80) *p++ = (char)cp;
    else if (cp < 0x800) { *p++ = (char)(0xC0 | cp >> 6); *p++ = (char)(0x80 | (cp & 63)); }
    else if (cp < 0x10000) { *p++ = (char)(0xE0 | cp >> 12); *p++ = (char)(0x80 | (cp >> 6 & 63)); *p++ = (char)(0x80 | (cp & 63)); }
    else { *p++ = (char)(0xF0 | cp >> 18); *p++ = (char)(0x80 | (cp >> 12 & 63)); *p++ = (char)(0x80 | (cp >> 6 & 63)); *p++ = (char)(0x80 | (cp & 63)); }
    *o = p;
}

static unsigned hex4(const char *s)
{
    unsigned v = 0;
    for (int i = 0; i < 4; i++) {
        char c = s[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
        else return 0xFFFFFFFFu;
    }
    return v;
}

/* Legge una stringa JSON che inizia in *p (sulle virgolette); NULL se non e' valida. */
static char *read_string(const char **p)
{
    const char *s = *p;
    if (*s != '"') return NULL;
    s++;
    size_t cap = strlen(s) + 1;
    char *out = malloc(cap), *o = out;
    if (!out) return NULL;
    while (*s && *s != '"') {
        if (*s != '\\') { *o++ = *s++; continue; }
        s++;
        switch (*s) {
        case 'n': *o++ = '\n'; break;
        case 't': *o++ = '\t'; break;
        case 'r': *o++ = '\r'; break;
        case 'b': *o++ = '\b'; break;
        case 'f': *o++ = '\f'; break;
        case 'u': {
            unsigned cp = hex4(s + 1);
            if (cp == 0xFFFFFFFFu) { free(out); return NULL; }
            s += 4;
            if (cp >= 0xD800 && cp < 0xDC00 && s[1] == '\\' && s[2] == 'u') {
                unsigned lo = hex4(s + 3);
                if (lo >= 0xDC00 && lo < 0xE000) { cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); s += 6; }
            }
            put_utf8(&o, cp);
            break;
        }
        case 0: free(out); return NULL;
        default: *o++ = *s;
        }
        s++;
    }
    if (*s != '"') { free(out); return NULL; }
    *o = 0;
    *p = s + 1;
    return out;
}

static void skip_ws(const char **p) { while (**p == ' ' || **p == '\n' || **p == '\r' || **p == '\t') (*p)++; }

static int cmp_pair(const void *a, const void *b) { return strcmp(((const Pair *)a)->key, ((const Pair *)b)->key); }

static int load_file(const char *path)
{
    char *s = read_all(path);
    if (!s) return -1;
    const char *p = s;
    skip_ws(&p);
    if (*p == '\xEF') p += 3;                     /* BOM */
    skip_ws(&p);
    if (*p != '{') { free(s); return -1; }
    p++;
    for (;;) {
        skip_ws(&p);
        if (*p == '}') break;
        char *k = read_string(&p);
        if (!k) break;
        skip_ws(&p);
        if (*p != ':') { free(k); break; }
        p++;
        skip_ws(&p);
        char *v = read_string(&p);
        if (!v) { free(k); break; }
        if (*v) {
            if (g_n == g_cap) {
                g_cap = g_cap ? g_cap * 2 : 128;
                Pair *x = realloc(g_pairs, sizeof(Pair) * (size_t)g_cap);
                if (!x) abort();
                g_pairs = x;
            }
            g_pairs[g_n++] = (Pair){ k, v };
        } else { free(k); free(v); }
        skip_ws(&p);
        if (*p == ',') p++;
    }
    free(s);
    qsort(g_pairs, (size_t)g_n, sizeof(Pair), cmp_pair);
    return 0;
}

void i18n_free(void)
{
    for (int i = 0; i < g_n; i++) { free(g_pairs[i].key); free(g_pairs[i].val); }
    free(g_pairs);
    g_pairs = NULL;
    g_n = g_cap = 0;
}

const char *i18n_set(const char *dir, const char *language)
{
    i18n_free();
    snprintf(g_lang, sizeof(g_lang), "Italian");
    if (!language || !*language || !strcmp(language, "Italian")) return g_lang;
    char path[600];
    snprintf(path, sizeof(path), "%s/%s.json", dir, language);
    if (!load_file(path)) { snprintf(g_lang, sizeof(g_lang), "%s", language); return g_lang; }
    snprintf(path, sizeof(path), "%s/English.json", dir);
    if (!load_file(path)) snprintf(g_lang, sizeof(g_lang), "English");
    return g_lang;
}

static void pyui_language(char *out, size_t n)
{
    out[0] = 0;
    char *s = read_all(PYUI_CONFIG);
    if (!s) return;
    const char *p = strstr(s, "\"language\"");
    if (p) {
        p += 10;
        skip_ws(&p);
        if (*p == ':') {
            p++;
            skip_ws(&p);
            char *v = read_string(&p);
            if (v) { snprintf(out, n, "%s", v); free(v); }
        }
    }
    free(s);
}

const char *i18n_init(const char *dir)
{
    char lang[48];
    const char *env = getenv("CIRMOLO_LANG");
    if (env && *env) snprintf(lang, sizeof(lang), "%s", env);
    else pyui_language(lang, sizeof(lang));
    return i18n_set(dir, lang[0] ? lang : "Italian");
}

const char *tr(const char *it)
{
    if (!g_n || !it) return it;
    Pair key = { (char *)it, NULL };
    const Pair *f = bsearch(&key, g_pairs, (size_t)g_n, sizeof(Pair), cmp_pair);
    return f ? f->val : it;
}

const char *i18n_language(void) { return g_lang; }
