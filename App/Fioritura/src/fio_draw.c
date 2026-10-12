/* Fioritura - effetti di luce (vedi fio_draw.h). */
#include "fio_draw.h"

#include <math.h>
#include <stddef.h>

/* Somma con saturazione di (colore * k / 256), k 0..256: rosso e blu insieme in una parola a 32 bit (il
   bit 24 e il bit 8 raccolgono il riporto di ciascuno), il verde a parte. */
static inline void add_px(uint32_t *p, int cr, int cg, int cb, int k)
{
    uint32_t crb = ((uint32_t)cr << 16) | (uint32_t)cb, cgg = (uint32_t)cg << 8;
    uint32_t v = *p;
    uint32_t rb = (v & 0xFF00FFu) + (((crb * (uint32_t)k) >> 8) & 0xFF00FFu);
    uint32_t g = (v & 0xFF00u) + (((cgg * (uint32_t)k) >> 8) & 0xFF00u);
    uint32_t over = rb & 0x01000100u;
    rb = (rb | (over - (over >> 8))) & 0xFF00FFu;
    if (g & 0x10000u) g = 0xFF00u;
    *p = 0xFF000000u | rb | g;
}

void fx_glow(Canvas *c, float cx, float cy, float r, uint32_t color, float alpha)
{
    if (r < 0.5f || alpha <= 0.004f) return;
    if (alpha > 1.0f) alpha = 1.0f;
    int x0 = (int)floorf(cx - r), x1 = (int)ceilf(cx + r), y0 = (int)floorf(cy - r), y1 = (int)ceilf(cy + r);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > c->w - 1) x1 = c->w - 1;
    if (y1 > c->h - 1) y1 = c->h - 1;
    if (x0 > x1 || y0 > y1) return;
    int cr = (int)((color >> 16) & 255), cg = (int)((color >> 8) & 255), cb = (int)(color & 255);
    float r2 = r * r, inv = 1.0f / r2, A = alpha * 256.0f;
    for (int y = y0; y <= y1; y++) {
        float dy = (float)y + 0.5f - cy, dy2 = dy * dy;
        if (dy2 >= r2) continue;
        float half = sqrtf(r2 - dy2);
        int xa = (int)floorf(cx - half), xb = (int)ceilf(cx + half);
        if (xa < x0) xa = x0;
        if (xb > x1) xb = x1;
        uint32_t *row = c->px + (size_t)y * (size_t)c->w;
        float dx = (float)xa + 0.5f - cx;
        for (int x = xa; x <= xb; x++, dx += 1.0f) {
            float d2 = dx * dx + dy2;
            if (d2 >= r2) continue;
            float t = 1.0f - d2 * inv;
            int k = (int)(t * t * A);
            if (k > 0) add_px(row + x, cr, cg, cb, k);
        }
    }
}

void fx_glow_soft(Canvas *c, float cx, float cy, float r, uint32_t color, float alpha)
{
    if (r < 2.0f || alpha <= 0.004f) return;
    if (alpha > 1.0f) alpha = 1.0f;
    int y0 = ((int)floorf(cy - r)) & ~1, y1 = (int)ceilf(cy + r);
    if (y0 < 0) y0 = 0;
    if (y1 > c->h - 2) y1 = c->h - 2;
    int cr = (int)((color >> 16) & 255), cg = (int)((color >> 8) & 255), cb = (int)(color & 255);
    float r2 = r * r, inv = 1.0f / r2, A = alpha * 256.0f;
    for (int y = y0; y <= y1; y += 2) {
        float dy = (float)y + 1.0f - cy, dy2 = dy * dy;
        if (dy2 >= r2) continue;
        float half = sqrtf(r2 - dy2);
        int xa = ((int)floorf(cx - half)) & ~1, xb = (int)ceilf(cx + half);
        if (xa < 0) xa = 0;
        if (xb > c->w - 2) xb = c->w - 2;
        uint32_t *row0 = c->px + (size_t)y * (size_t)c->w, *row1 = row0 + c->w;
        for (int x = xa; x <= xb; x += 2) {
            float dx = (float)x + 1.0f - cx, d2 = dx * dx + dy2;
            if (d2 >= r2) continue;
            float t = 1.0f - d2 * inv;
            int k = (int)(t * t * A);
            if (k <= 0) continue;
            add_px(row0 + x, cr, cg, cb, k);
            add_px(row0 + x + 1, cr, cg, cb, k);
            add_px(row1 + x, cr, cg, cb, k);
            add_px(row1 + x + 1, cr, cg, cb, k);
        }
    }
}

void fx_ring(Canvas *c, float cx, float cy, float r, float w, uint32_t color, float alpha)
{
    if (r < 0.5f || alpha <= 0.004f) return;
    if (alpha > 1.0f) alpha = 1.0f;
    float hw = w * 0.5f, ro = r + hw + 1.0f, ri = r - hw - 1.0f;
    int y0 = (int)floorf(cy - ro), y1 = (int)ceilf(cy + ro);
    if (y0 < 0) y0 = 0;
    if (y1 > c->h - 1) y1 = c->h - 1;
    int cr = (int)((color >> 16) & 255), cg = (int)((color >> 8) & 255), cb = (int)(color & 255);
    float A = alpha * 256.0f;
    for (int y = y0; y <= y1; y++) {
        float dy = (float)y + 0.5f - cy, dy2 = dy * dy;
        if (dy2 >= ro * ro) continue;
        float xo = sqrtf(ro * ro - dy2);
        float xi = (ri > 0.0f && dy2 < ri * ri) ? sqrtf(ri * ri - dy2) : -1.0f;
        uint32_t *row = c->px + (size_t)y * (size_t)c->w;
        /* due tratti: a sinistra e a destra del buco (uno solo dove la riga passa sopra il buco) */
        for (int side = 0; side < 2; side++) {
            float a0, a1;
            if (xi < 0.0f) { if (side) break; a0 = cx - xo; a1 = cx + xo; }
            else if (!side) { a0 = cx - xo; a1 = cx - xi; }
            else { a0 = cx + xi; a1 = cx + xo; }
            int xa = (int)floorf(a0), xb = (int)ceilf(a1);
            if (xa < 0) xa = 0;
            if (xb > c->w - 1) xb = c->w - 1;
            for (int x = xa; x <= xb; x++) {
                float dx = (float)x + 0.5f - cx;
                float d = sqrtf(dx * dx + dy2);
                float cov = hw + 0.5f - fabsf(d - r);
                if (cov <= 0.0f) continue;
                if (cov > 1.0f) cov = 1.0f;
                int k = (int)(cov * A);
                if (k > 0) add_px(row + x, cr, cg, cb, k);
            }
        }
    }
}

void fx_hline(Canvas *c, float y, int x0, int x1, uint32_t color, float alpha)
{
    if (alpha <= 0.004f) return;
    if (alpha > 1.0f) alpha = 1.0f;
    if (x0 < 0) x0 = 0;
    if (x1 > c->w - 1) x1 = c->w - 1;
    int yi = (int)floorf(y - 0.5f);
    float fr = (y - 0.5f) - (float)yi;
    int cr = (int)((color >> 16) & 255), cg = (int)((color >> 8) & 255), cb = (int)(color & 255);
    for (int k = 0; k < 2; k++) {
        int yy = yi + k;
        if (yy < 0 || yy >= c->h) continue;
        int a = (int)((k ? fr : 1.0f - fr) * alpha * 256.0f);
        if (a <= 0) continue;
        uint32_t *row = c->px + (size_t)yy * (size_t)c->w;
        for (int x = x0; x <= x1; x++) add_px(row + x, cr, cg, cb, a);
    }
}

void fx_wave(Canvas *c, float y, int x0, int x1, uint32_t color, float alpha, float as, int mode, float ps, float aw, float pw)
{
    /* tabelle per la larghezza della riga: seni dei tre modi e del vento (due lunghezze d'onda), cosi'
       per ogni pixel bastano somme e prodotti */
    enum { MAXW = 1024 };
    static float s1[MAXW], s2[MAXW], s3[MAXW], ws[MAXW], wc[MAXW];
    static int tw = -1;
    if (alpha <= 0.004f) return;
    if (alpha > 1.0f) alpha = 1.0f;
    if (x0 < 0) x0 = 0;
    if (x1 > c->w - 1) x1 = c->w - 1;
    int n = x1 - x0;
    if (n <= 0 || n >= MAXW) return;
    if (tw != n) {
        for (int i = 0; i <= n; i++) {
            float u = (float)i / (float)n, p = 3.14159265f * u;
            s1[i] = sinf(p); s2[i] = sinf(2.0f * p); s3[i] = sinf(3.0f * p);
            ws[i] = sinf(4.0f * p) * s1[i];
            wc[i] = cosf(4.0f * p) * s1[i];
        }
        tw = n;
    }
    const float *sm = mode == 2 ? s2 : (mode == 3 ? s3 : s1);
    float st = as * sinf(ps), cpw = aw * cosf(pw), spw = aw * sinf(pw);
    int cr = (int)((color >> 16) & 255), cg = (int)((color >> 8) & 255), cb = (int)(color & 255);
    float A = alpha * 256.0f;
    for (int i = 0; i <= n; i++) {
        /* sin(4pi u - pw) = sin(4pi u) cos(pw) - cos(4pi u) sin(pw), gia' smorzato agli estremi */
        float yy = y + st * sm[i] + ws[i] * cpw - wc[i] * spw - 0.5f;
        int yi = (int)floorf(yy);
        float fr = yy - (float)yi;
        int a0 = (int)((1.0f - fr) * A), a1 = (int)(fr * A);
        int x = x0 + i;
        if (yi >= 0 && yi < c->h && a0 > 0) add_px(c->px + (size_t)yi * (size_t)c->w + x, cr, cg, cb, a0);
        if (yi + 1 >= 0 && yi + 1 < c->h && a1 > 0) add_px(c->px + (size_t)(yi + 1) * (size_t)c->w + x, cr, cg, cb, a1);
    }
}

void fx_sky(uint32_t *dst, int w, int h, uint32_t top, uint32_t bottom, uint32_t haze)
{
    static const int BAYER[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
    float t0[3] = { (float)((top >> 16) & 255), (float)((top >> 8) & 255), (float)(top & 255) };
    float b0[3] = { (float)((bottom >> 16) & 255), (float)((bottom >> 8) & 255), (float)(bottom & 255) };
    float hz[3] = { (float)((haze >> 16) & 255), (float)((haze >> 8) & 255), (float)(haze & 255) };
    for (int y = 0; y < h; y++) {
        float v = (float)y / (float)(h - 1);
        v = v * v * (3.0f - 2.0f * v);
        for (int x = 0; x < w; x++) {
            /* alone sull'orizzonte: un'ellisse larga, bassa, al centro in fondo */
            float dx = ((float)x - (float)w * 0.5f) / ((float)w * 0.62f), dy = ((float)y - (float)h * 1.02f) / ((float)h * 0.55f);
            float d = dx * dx + dy * dy, glow = d < 1.0f ? (1.0f - d) * (1.0f - d) * 0.16f : 0.0f;
            /* vignetta leggera ai lati */
            float vx = ((float)x / (float)(w - 1)) * 2.0f - 1.0f, vig = 1.0f - 0.18f * vx * vx * vx * vx;
            float dith = ((float)BAYER[y & 3][x & 3] - 7.5f) / 16.0f;
            int ch[3];
            for (int k = 0; k < 3; k++) {
                float col = (t0[k] + (b0[k] - t0[k]) * v) * vig + hz[k] * glow + dith;
                int iv = (int)floorf(col + 0.5f);
                ch[k] = iv < 0 ? 0 : (iv > 255 ? 255 : iv);
            }
            dst[(size_t)y * (size_t)w + (size_t)x] = 0xFF000000u | ((uint32_t)ch[0] << 16) | ((uint32_t)ch[1] << 8) | (uint32_t)ch[2];
        }
    }
}

void fx_blend(uint32_t *dst, const uint32_t *a, const uint32_t *b, int n, float t)
{
    int k = (int)(t * 256.0f);
    if (k < 0) k = 0;
    if (k > 256) k = 256;
    for (int i = 0; i < n; i++) {
        uint32_t x = a[i], y = b[i];
        int r = (int)((x >> 16) & 255), g = (int)((x >> 8) & 255), bl = (int)(x & 255);
        r += (((int)((y >> 16) & 255) - r) * k) >> 8;
        g += (((int)((y >> 8) & 255) - g) * k) >> 8;
        bl += (((int)(y & 255) - bl) * k) >> 8;
        dst[i] = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl;
    }
}
