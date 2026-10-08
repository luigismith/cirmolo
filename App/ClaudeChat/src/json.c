/* Chiedi a Claude - JSON minimo (vedi json.h). */
#include "json.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ Buf */
static void buf_grow(Buf *b, size_t need)
{
    if (b->len + need + 1 <= b->cap) return;
    size_t cap = b->cap ? b->cap : 256;
    while (cap < b->len + need + 1) cap *= 2;
    char *p = realloc(b->p, cap);
    if (!p) abort();
    b->p = p;
    b->cap = cap;
}

void buf_add(Buf *b, const char *s, size_t n)
{
    buf_grow(b, n);
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = 0;
}

void buf_adds(Buf *b, const char *s) { buf_add(b, s, strlen(s)); }

void buf_printf(Buf *b, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    buf_grow(b, (size_t)n);
    va_start(ap, fmt);
    vsnprintf(b->p + b->len, (size_t)n + 1, fmt, ap);
    va_end(ap);
    b->len += (size_t)n;
}

void buf_free(Buf *b) { free(b->p); b->p = NULL; b->len = b->cap = 0; }
void buf_clear(Buf *b) { b->len = 0; if (b->p) b->p[0] = 0; }

char *buf_steal(Buf *b)
{
    char *p = b->p ? b->p : calloc(1, 1);
    b->p = NULL;
    b->len = b->cap = 0;
    return p;
}

void json_escape_n(Buf *b, const char *s, size_t n)
{
    buf_add(b, "\"", 1);
    size_t run = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        const char *esc = NULL;
        char tmp[8];
        switch (c) {
        case '"': esc = "\\\""; break;
        case '\\': esc = "\\\\"; break;
        case '\n': esc = "\\n"; break;
        case '\r': esc = "\\r"; break;
        case '\t': esc = "\\t"; break;
        case '\b': esc = "\\b"; break;
        case '\f': esc = "\\f"; break;
        default:
            if (c < 0x20) { snprintf(tmp, sizeof(tmp), "\\u%04x", c); esc = tmp; }
        }
        if (!esc) { run++; continue; }
        if (run) buf_add(b, s + i - run, run);
        run = 0;
        buf_adds(b, esc);
    }
    if (run) buf_add(b, s + n - run, run);
    buf_add(b, "\"", 1);
}

void json_escape(Buf *b, const char *s) { json_escape_n(b, s ? s : "", s ? strlen(s) : 0); }

/* ------------------------------------------------------------------ lettura */
typedef struct { const char *s; size_t n, i; int depth; } P;

static void ws(P *p) { while (p->i < p->n && (p->s[p->i] == ' ' || p->s[p->i] == '\t' || p->s[p->i] == '\n' || p->s[p->i] == '\r')) p->i++; }

static void put_utf8(Buf *b, unsigned cp)
{
    char u[4];
    if (cp < 0x80) { u[0] = (char)cp; buf_add(b, u, 1); }
    else if (cp < 0x800) { u[0] = (char)(0xC0 | cp >> 6); u[1] = (char)(0x80 | (cp & 0x3F)); buf_add(b, u, 2); }
    else if (cp < 0x10000) { u[0] = (char)(0xE0 | cp >> 12); u[1] = (char)(0x80 | ((cp >> 6) & 0x3F)); u[2] = (char)(0x80 | (cp & 0x3F)); buf_add(b, u, 3); }
    else { u[0] = (char)(0xF0 | cp >> 18); u[1] = (char)(0x80 | ((cp >> 12) & 0x3F)); u[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); u[3] = (char)(0x80 | (cp & 0x3F)); buf_add(b, u, 4); }
}

static int hex4(P *p, unsigned *out)
{
    if (p->i + 4 > p->n) return -1;
    unsigned v = 0;
    for (int k = 0; k < 4; k++) {
        char c = p->s[p->i++];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
        else return -1;
    }
    *out = v;
    return 0;
}

static char *string(P *p)
{
    if (p->i >= p->n || p->s[p->i] != '"') return NULL;
    p->i++;
    Buf b = { 0 };
    buf_add(&b, "", 0);
    while (p->i < p->n) {
        char c = p->s[p->i++];
        if (c == '"') return buf_steal(&b);
        if ((unsigned char)c < 0x20) break;
        if (c != '\\') {
            size_t start = p->i - 1;
            while (p->i < p->n && p->s[p->i] != '"' && p->s[p->i] != '\\' && (unsigned char)p->s[p->i] >= 0x20) p->i++;
            buf_add(&b, p->s + start, p->i - start);
            continue;
        }
        if (p->i >= p->n) break;
        c = p->s[p->i++];
        unsigned cp;
        switch (c) {
        case '"': buf_add(&b, "\"", 1); break;
        case '\\': buf_add(&b, "\\", 1); break;
        case '/': buf_add(&b, "/", 1); break;
        case 'b': buf_add(&b, "\b", 1); break;
        case 'f': buf_add(&b, "\f", 1); break;
        case 'n': buf_add(&b, "\n", 1); break;
        case 'r': buf_add(&b, "\r", 1); break;
        case 't': buf_add(&b, "\t", 1); break;
        case 'u':
            if (hex4(p, &cp)) goto bad;
            if (cp >= 0xD800 && cp < 0xDC00 && p->i + 6 <= p->n && p->s[p->i] == '\\' && p->s[p->i + 1] == 'u') {
                unsigned lo;
                p->i += 2;
                if (hex4(p, &lo)) goto bad;
                if (lo >= 0xDC00 && lo < 0xE000) cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                else { put_utf8(&b, 0xFFFD); cp = lo; }
            }
            if (cp >= 0xD800 && cp < 0xE000) cp = 0xFFFD;
            put_utf8(&b, cp);
            break;
        default:
            goto bad;
        }
    }
bad:
    buf_free(&b);
    return NULL;
}

static JNode *value(P *p);

static JNode *node(int type, int start)
{
    JNode *n = calloc(1, sizeof(JNode));
    if (!n) abort();
    n->type = type;
    n->start = start;
    return n;
}

static JNode *value(P *p)
{
    ws(p);
    if (p->i >= p->n || ++p->depth > 64) return NULL;
    int start = (int)p->i;
    char c = p->s[p->i];
    JNode *n = NULL;
    if (c == '{' || c == '[') {
        int obj = c == '{';
        n = node(obj ? J_OBJECT : J_ARRAY, start);
        p->i++;
        JNode **tail = &n->child;
        ws(p);
        if (p->i < p->n && p->s[p->i] == (obj ? '}' : ']')) { p->i++; goto done; }
        for (;;) {
            char *key = NULL;
            if (obj) {
                ws(p);
                key = string(p);
                if (!key) goto fail;
                ws(p);
                if (p->i >= p->n || p->s[p->i] != ':') { free(key); goto fail; }
                p->i++;
            }
            JNode *v = value(p);
            if (!v) { free(key); goto fail; }
            v->key = key;
            *tail = v;
            tail = &v->next;
            ws(p);
            if (p->i >= p->n) goto fail;
            if (p->s[p->i] == ',') { p->i++; continue; }
            if (p->s[p->i] == (obj ? '}' : ']')) { p->i++; break; }
            goto fail;
        }
    } else if (c == '"') {
        n = node(J_STRING, start);
        n->str = string(p);
        if (!n->str) goto fail;
    } else if (c == 't' && p->i + 4 <= p->n && !memcmp(p->s + p->i, "true", 4)) {
        n = node(J_TRUE, start); p->i += 4;
    } else if (c == 'f' && p->i + 5 <= p->n && !memcmp(p->s + p->i, "false", 5)) {
        n = node(J_FALSE, start); p->i += 5;
    } else if (c == 'n' && p->i + 4 <= p->n && !memcmp(p->s + p->i, "null", 4)) {
        n = node(J_NULL, start); p->i += 4;
    } else if (c == '-' || (c >= '0' && c <= '9')) {
        char tmp[64];
        size_t k = 0;
        while (p->i < p->n && k < sizeof(tmp) - 1 && strchr("+-0123456789.eE", p->s[p->i])) tmp[k++] = p->s[p->i++];
        tmp[k] = 0;
        char *end;
        double v = strtod(tmp, &end);
        if (end == tmp) return NULL;
        n = node(J_NUMBER, start);
        n->num = v;
    } else {
        return NULL;
    }
done:
    n->end = (int)p->i;
    p->depth--;
    return n;
fail:
    json_free(n);
    return NULL;
}

JNode *json_parse(const char *text, size_t len)
{
    P p = { text, len, 0, 0 };
    JNode *n = value(&p);
    if (!n) return NULL;
    ws(&p);
    if (p.i != p.n) { json_free(n); return NULL; }
    return n;
}

void json_free(JNode *n)
{
    while (n) {
        JNode *next = n->next;
        json_free(n->child);
        free(n->str);
        free(n->key);
        free(n);
        n = next;
    }
}

const JNode *json_get(const JNode *obj, const char *key)
{
    if (!obj || obj->type != J_OBJECT) return NULL;
    for (const JNode *c = obj->child; c; c = c->next)
        if (c->key && !strcmp(c->key, key)) return c;
    return NULL;
}

const char *json_str(const JNode *obj, const char *key)
{
    const JNode *n = json_get(obj, key);
    return n && n->type == J_STRING ? n->str : NULL;
}

double json_num(const JNode *obj, const char *key, double def)
{
    const JNode *n = json_get(obj, key);
    return n && n->type == J_NUMBER ? n->num : def;
}

const char *json_path_str(const JNode *obj, const char *a, const char *b) { return json_str(json_get(obj, a), b); }
