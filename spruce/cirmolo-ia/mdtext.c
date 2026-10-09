/* Cirmolo IA - impaginazione di testi in Markdown leggero (vedi mdtext.h). */
#include "mdtext.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>


int style_font(int st) { return st == ST_HEAD ? FONT_BOLD : (st == ST_CODE ? FONT_SMALL : FONT_BODY); }
int style_height(int st) { return st == ST_HEAD ? 27 : (st == ST_CODE ? 20 : (st == ST_BLANK ? 9 : (st == ST_RULE ? 12 : 23))); }

int width_n(int font, const char *s, int n)
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

void wrap_text(Wrap *w, const char *src, int avail)
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

void wrap_free(Wrap *w) { buf_free(&w->disp); free(w->line); memset(w, 0, sizeof(*w)); }

/* Testo semplice (il messaggio in scrittura): gli offset restano quelli del testo di partenza. */
void wrap_plain(Wrap *w, const char *src, int avail)
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

