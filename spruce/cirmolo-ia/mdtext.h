/* Cirmolo IA - impaginazione di testi in Markdown leggero (titoli, elenchi, citazioni, codice, grassetto)
 * per gli schermi delle app: spezza in righe della larghezza voluta con i caratteri del kit. La usano la
 * chat di Chiedi all'IA e le schede dei giochi. */
#ifndef CIRMOLO_IA_MDTEXT_H
#define CIRMOLO_IA_MDTEXT_H

#include "gfx.h"
#include "json.h"

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

int  style_font(int st);
int  style_height(int st);
int  width_n(int font, const char *s, int n);
void wrap_text(Wrap *w, const char *src, int avail);     /* Markdown: i segni non si mostrano */
void wrap_plain(Wrap *w, const char *src, int avail);    /* testo semplice: offset uguali al sorgente */
void wrap_free(Wrap *w);

#endif
