/* Cirmolo Synth - disegno su un framebuffer ARGB8888 e testo con stb_truetype. */
#ifndef CIRMOLO_GFX_H
#define CIRMOLO_GFX_H

#include <stdint.h>

typedef struct {
    uint32_t *px;
    int w, h;
} Canvas;

enum { FONT_SMALL, FONT_BODY, FONT_BOLD, FONT_TITLE, FONT_BIG, FONT_COUNT };

#define RGB(r, g, b) (0xFF000000u | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

/* Carica i due pesi di Be Vietnam Pro; 0 se va tutto bene. */
int  gfx_load_fonts(const char *regular_ttf, const char *semibold_ttf);
void gfx_free_fonts(void);

void gfx_clear(Canvas *c, uint32_t color);
void gfx_rect(Canvas *c, int x, int y, int w, int h, uint32_t color);
void gfx_rect_alpha(Canvas *c, int x, int y, int w, int h, uint32_t color, float alpha);
void gfx_round_rect(Canvas *c, float x, float y, float w, float h, float r, uint32_t color, float alpha);
void gfx_round_frame(Canvas *c, float x, float y, float w, float h, float r, float thick, uint32_t color, float alpha);
void gfx_circle(Canvas *c, float cx, float cy, float r, uint32_t color, float alpha);
void gfx_line(Canvas *c, float x0, float y0, float x1, float y1, float width, uint32_t color, float alpha);
void gfx_vgradient(Canvas *c, int x, int y, int w, int h, uint32_t top, uint32_t bottom);

/* Testo UTF-8: y e' la linea di base. Restituiscono la larghezza in pixel. */
int  gfx_text(Canvas *c, int font, int x, int y, const char *utf8, uint32_t color);
int  gfx_text_width(int font, const char *utf8);
int  gfx_text_center(Canvas *c, int font, int cx, int y, const char *utf8, uint32_t color);
int  gfx_text_right(Canvas *c, int font, int right, int y, const char *utf8, uint32_t color);
int  gfx_font_ascent(int font);

uint32_t gfx_mix(uint32_t a, uint32_t b, float t);

#endif
