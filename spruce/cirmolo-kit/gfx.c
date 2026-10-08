/* Cirmolo Synth - disegno (vedi gfx.h). */
#include "gfx.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "third_party/stb_truetype.h"

#define GLYPHS 384   /* Latino di base, Latin-1 e Latin Extended-A: basta per l'italiano */

typedef struct {
    int ready, w, h, x0, y0;
    float advance;
    unsigned char *bmp;
} Glyph;

typedef struct {
    stbtt_fontinfo *info;
    float scale, size;
    int ascent;
    Glyph g[GLYPHS];
} Font;

static unsigned char *font_data[2];
static stbtt_fontinfo font_info[2];
static Font fonts[FONT_COUNT];

static unsigned char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *b = malloc((size_t)n);
    if (b && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); b = NULL; }
    fclose(f);
    return b;
}

int gfx_load_fonts(const char *regular, const char *semibold)
{
    const char *paths[2] = { regular, semibold };
    for (int i = 0; i < 2; i++) {
        font_data[i] = read_file(paths[i]);
        if (!font_data[i] || !stbtt_InitFont(&font_info[i], font_data[i], stbtt_GetFontOffsetForIndex(font_data[i], 0))) {
            fprintf(stderr, "font non caricato: %s\n", paths[i]);
            return -1;
        }
    }
    static const struct { int face; float px; } spec[FONT_COUNT] = {
        [FONT_SMALL] = { 0, 15.0f }, [FONT_BODY] = { 0, 18.0f }, [FONT_BOLD] = { 1, 18.0f },
        [FONT_TITLE] = { 1, 25.0f }, [FONT_BIG] = { 1, 46.0f }, [FONT_HUGE] = { 1, 66.0f },
    };
    for (int i = 0; i < FONT_COUNT; i++) {
        Font *f = &fonts[i];
        memset(f, 0, sizeof(*f));
        f->info = &font_info[spec[i].face];
        f->size = spec[i].px;
        f->scale = stbtt_ScaleForPixelHeight(f->info, spec[i].px);
        int asc, desc, gap;
        stbtt_GetFontVMetrics(f->info, &asc, &desc, &gap);
        f->ascent = (int)lrintf(asc * f->scale);
    }
    return 0;
}

void gfx_free_fonts(void)
{
    for (int i = 0; i < FONT_COUNT; i++)
        for (int g = 0; g < GLYPHS; g++) { free(fonts[i].g[g].bmp); fonts[i].g[g].bmp = NULL; fonts[i].g[g].ready = 0; }
    for (int i = 0; i < 2; i++) { free(font_data[i]); font_data[i] = NULL; }
}

int gfx_font_ascent(int font) { return fonts[font].ascent; }

static Glyph *glyph(Font *f, int cp)
{
    if (cp < 0 || cp >= GLYPHS) cp = '?';
    Glyph *g = &f->g[cp];
    if (!g->ready) {
        int adv, lsb;
        stbtt_GetCodepointHMetrics(f->info, cp, &adv, &lsb);
        g->advance = adv * f->scale;
        g->bmp = stbtt_GetCodepointBitmap(f->info, f->scale, f->scale, cp, &g->w, &g->h, &g->x0, &g->y0);
        g->ready = 1;
    }
    return g;
}

static int utf8_next(const char **s)
{
    const unsigned char *p = (const unsigned char *)*s;
    int cp;
    if (p[0] < 0x80) { cp = p[0]; *s += 1; }
    else if ((p[0] & 0xE0) == 0xC0 && p[1]) { cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F); *s += 2; }
    else if ((p[0] & 0xF0) == 0xE0 && p[1] && p[2]) { cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); *s += 3; }
    else { cp = '?'; *s += 1; while ((**s & 0xC0) == 0x80) (*s)++; }
    return cp;
}

uint32_t gfx_mix(uint32_t a, uint32_t b, float t)
{
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255;
    int br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
    return RGB(ar + (int)((br - ar) * t), ag + (int)((bg - ag) * t), ab + (int)((bb - ab) * t));
}

static inline void blend(Canvas *c, int x, int y, uint32_t color, float a)
{
    if (x < 0 || y < 0 || x >= c->w || y >= c->h || a <= 0.0f) return;
    uint32_t *d = &c->px[y * c->w + x];
    *d = a >= 1.0f ? (color | 0xFF000000u) : gfx_mix(*d, color, a);
}

void gfx_clear(Canvas *c, uint32_t color)
{
    for (int i = 0; i < c->w * c->h; i++) c->px[i] = color;
}

void gfx_rect(Canvas *c, int x, int y, int w, int h, uint32_t color)
{
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > c->w ? c->w : x + w, y1 = y + h > c->h ? c->h : y + h;
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++) c->px[yy * c->w + xx] = color | 0xFF000000u;
}

void gfx_rect_alpha(Canvas *c, int x, int y, int w, int h, uint32_t color, float alpha)
{
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++) blend(c, xx, yy, color, alpha);
}

void gfx_vgradient(Canvas *c, int x, int y, int w, int h, uint32_t top, uint32_t bottom)
{
    for (int yy = 0; yy < h; yy++) gfx_rect(c, x, y + yy, w, 1, gfx_mix(top, bottom, h > 1 ? (float)yy / (h - 1) : 0.0f));
}

static inline float rr_dist(float px, float py, float cx, float cy, float hw, float hh, float r)
{
    float dx = fabsf(px - cx) - (hw - r), dy = fabsf(py - cy) - (hh - r);
    float ox = dx > 0 ? dx : 0, oy = dy > 0 ? dy : 0;
    float inside = (dx > dy ? dx : dy);
    return sqrtf(ox * ox + oy * oy) + (inside < 0 ? inside : 0) - r;
}

/* Riga y da xa a xb (escluso) con un colore a copertura a: veloce quando e' piena. */
static inline void span(Canvas *c, int y, int xa, int xb, uint32_t color, float a)
{
    if (y < 0 || y >= c->h || a <= 0.0f) return;
    if (xa < 0) xa = 0;
    if (xb > c->w) xb = c->w;
    uint32_t *d = &c->px[y * c->w];
    if (a >= 1.0f) {
        uint32_t col = color | 0xFF000000u;
        for (int x = xa; x < xb; x++) d[x] = col;
    } else {
        for (int x = xa; x < xb; x++) d[x] = gfx_mix(d[x], color, a);
    }
}

static inline float cov01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

/* Nei rettangoli arrotondati la distanza dal bordo si calcola solo vicino ai lati sinistro e destro:
   nelle colonne centrali la copertura dipende solo dalla riga (pieno al centro, sfumato in cima e in
   fondo). Con raggi sotto un pixel si torna al calcolo pixel per pixel. */
void gfx_round_rect(Canvas *c, float x, float y, float w, float h, float r, uint32_t color, float alpha)
{
    if (r > w * 0.5f) r = w * 0.5f;
    if (r > h * 0.5f) r = h * 0.5f;
    float cx = x + w * 0.5f, cy = y + h * 0.5f, hw = w * 0.5f, hh = h * 0.5f;
    int x0 = (int)floorf(x), x1 = (int)ceilf(x + w);
    int cl = x1, cr = x1;
    if (r >= 1.0f) {
        cl = (int)ceilf(x + r - 0.5f);
        cr = (int)floorf(x + w - r - 0.5f) + 1;
        if (cl < x0) cl = x0;
        if (cr > x1) cr = x1;
        if (cr < cl) cr = cl;
    }
    for (int yy = (int)floorf(y); yy < (int)ceilf(y + h); yy++) {
        if (yy < 0 || yy >= c->h) continue;
        if (cr > cl) {
            float dy = fabsf(yy + 0.5f - cy) - (hh - r);
            span(c, yy, cl, cr, color, (dy <= 0.0f ? 1.0f : cov01(0.5f - (dy - r))) * alpha);
        }
        for (int xx = x0; xx < x1; xx++) {
            if (xx == cl && cr > cl) xx = cr;
            if (xx >= x1) break;
            float cov = 0.5f - rr_dist(xx + 0.5f, yy + 0.5f, cx, cy, hw, hh, r);
            if (cov > 0.0f) blend(c, xx, yy, color, cov01(cov) * alpha);
        }
    }
}

void gfx_round_frame(Canvas *c, float x, float y, float w, float h, float r, float thick, uint32_t color, float alpha)
{
    if (r > w * 0.5f) r = w * 0.5f;
    if (r > h * 0.5f) r = h * 0.5f;
    float cx = x + w * 0.5f, cy = y + h * 0.5f, hw = w * 0.5f, hh = h * 0.5f;
    int x0 = (int)floorf(x), x1 = (int)ceilf(x + w);
    int cl = x1, cr = x1;
    if (r >= thick + 1.0f) {                      /* dentro la cornice, al centro, non c'e' niente da fare */
        cl = (int)ceilf(x + r - 0.5f);
        cr = (int)floorf(x + w - r - 0.5f) + 1;
        if (cl < x0) cl = x0;
        if (cr > x1) cr = x1;
        if (cr < cl) cr = cl;
    }
    for (int yy = (int)floorf(y); yy < (int)ceilf(y + h); yy++) {
        if (yy < 0 || yy >= c->h) continue;
        if (cr > cl) {
            float dy = fabsf(yy + 0.5f - cy) - (hh - r);
            if (dy > 0.0f) {
                float d = dy - r;
                span(c, yy, cl, cr, color, (cov01(0.5f - d) - cov01(0.5f - (d + thick))) * alpha);
            }
        }
        for (int xx = x0; xx < x1; xx++) {
            if (xx == cl && cr > cl) xx = cr;
            if (xx >= x1) break;
            float d = rr_dist(xx + 0.5f, yy + 0.5f, cx, cy, hw, hh, r);
            float cov = cov01(0.5f - d) - cov01(0.5f - (d + thick));
            if (cov > 0.0f) blend(c, xx, yy, color, cov * alpha);
        }
    }
}

void gfx_circle(Canvas *c, float cx, float cy, float r, uint32_t color, float alpha)
{
    for (int yy = (int)floorf(cy - r - 1); yy <= (int)ceilf(cy + r + 1); yy++)
        for (int xx = (int)floorf(cx - r - 1); xx <= (int)ceilf(cx + r + 1); xx++) {
            float dx = xx + 0.5f - cx, dy = yy + 0.5f - cy;
            float cov = r + 0.5f - sqrtf(dx * dx + dy * dy);
            if (cov > 0.0f) blend(c, xx, yy, color, (cov > 1.0f ? 1.0f : cov) * alpha);
        }
}

void gfx_line(Canvas *c, float x0, float y0, float x1, float y1, float width, uint32_t color, float alpha)
{
    float hw = width * 0.5f;
    float minx = fminf(x0, x1) - hw - 1, maxx = fmaxf(x0, x1) + hw + 1;
    float miny = fminf(y0, y1) - hw - 1, maxy = fmaxf(y0, y1) + hw + 1;
    float dx = x1 - x0, dy = y1 - y0, len2 = dx * dx + dy * dy;
    for (int yy = (int)floorf(miny); yy <= (int)ceilf(maxy); yy++)
        for (int xx = (int)floorf(minx); xx <= (int)ceilf(maxx); xx++) {
            float px = xx + 0.5f - x0, py = yy + 0.5f - y0;
            float t = len2 > 0 ? (px * dx + py * dy) / len2 : 0;
            t = t < 0 ? 0 : (t > 1 ? 1 : t);
            float ex = px - dx * t, ey = py - dy * t;
            float cov = hw + 0.5f - sqrtf(ex * ex + ey * ey);
            if (cov > 0.0f) blend(c, xx, yy, color, (cov > 1.0f ? 1.0f : cov) * alpha);
        }
}

static int text_impl(Canvas *c, int font, int x, int y, const char *s, uint32_t color)
{
    Font *f = &fonts[font];
    if (!f->info) return 0;
    float pen = (float)x;
    int prev = 0;
    while (*s) {
        int cp = utf8_next(&s);
        if (cp >= GLYPHS) cp = '?';
        if (prev) pen += stbtt_GetCodepointKernAdvance(f->info, prev, cp) * f->scale;
        Glyph *g = glyph(f, cp);
        if (c && g->bmp) {
            int gx = (int)lrintf(pen) + g->x0, gy = y + g->y0;
            for (int j = 0; j < g->h; j++)
                for (int i = 0; i < g->w; i++) {
                    unsigned char a = g->bmp[j * g->w + i];
                    if (a) blend(c, gx + i, gy + j, color, a / 255.0f);
                }
        }
        pen += g->advance;
        prev = cp;
    }
    return (int)lrintf(pen) - x;
}

int gfx_text(Canvas *c, int font, int x, int y, const char *s, uint32_t color) { return text_impl(c, font, x, y, s, color); }
int gfx_text_width(int font, const char *s) { return text_impl(NULL, font, 0, 0, s, 0); }

int gfx_text_center(Canvas *c, int font, int cx, int y, const char *s, uint32_t color)
{
    int w = gfx_text_width(font, s);
    return text_impl(c, font, cx - w / 2, y, s, color);
}

int gfx_text_right(Canvas *c, int font, int right, int y, const char *s, uint32_t color)
{
    int w = gfx_text_width(font, s);
    return text_impl(c, font, right - w, y, s, color);
}
