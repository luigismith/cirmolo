/* Cirmolo kit - prova di gfx_round_rect e gfx_round_frame veloci contro il calcolo pixel per pixel.
 * Uso: test_gfx  (esce con 1 se un pixel differisce di piu' di 1 livello per canale)
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx.h"

/* versioni di riferimento: la distanza dal bordo per ogni pixel */
static float rr_dist(float px, float py, float cx, float cy, float hw, float hh, float r)
{
    float dx = fabsf(px - cx) - (hw - r), dy = fabsf(py - cy) - (hh - r);
    float ox = dx > 0 ? dx : 0, oy = dy > 0 ? dy : 0;
    float inside = (dx > dy ? dx : dy);
    return sqrtf(ox * ox + oy * oy) + (inside < 0 ? inside : 0) - r;
}
static void blend(Canvas *c, int x, int y, uint32_t color, float a)
{
    if (x < 0 || y < 0 || x >= c->w || y >= c->h || a <= 0.0f) return;
    uint32_t *d = &c->px[y * c->w + x];
    *d = a >= 1.0f ? (color | 0xFF000000u) : gfx_mix(*d, color, a);
}
static void ref_round_rect(Canvas *c, float x, float y, float w, float h, float r, uint32_t color, float alpha)
{
    if (r > w * 0.5f) r = w * 0.5f;
    if (r > h * 0.5f) r = h * 0.5f;
    float cx = x + w * 0.5f, cy = y + h * 0.5f;
    for (int yy = (int)floorf(y); yy < (int)ceilf(y + h); yy++)
        for (int xx = (int)floorf(x); xx < (int)ceilf(x + w); xx++) {
            float cov = 0.5f - rr_dist(xx + 0.5f, yy + 0.5f, cx, cy, w * 0.5f, h * 0.5f, r);
            if (cov > 0.0f) blend(c, xx, yy, color, (cov > 1.0f ? 1.0f : cov) * alpha);
        }
}
static void ref_round_frame(Canvas *c, float x, float y, float w, float h, float r, float thick, uint32_t color, float alpha)
{
    if (r > w * 0.5f) r = w * 0.5f;
    if (r > h * 0.5f) r = h * 0.5f;
    float cx = x + w * 0.5f, cy = y + h * 0.5f;
    for (int yy = (int)floorf(y); yy < (int)ceilf(y + h); yy++)
        for (int xx = (int)floorf(x); xx < (int)ceilf(x + w); xx++) {
            float d = rr_dist(xx + 0.5f, yy + 0.5f, cx, cy, w * 0.5f, h * 0.5f, r);
            float outer = 0.5f - d, inner = 0.5f - (d + thick);
            outer = outer < 0 ? 0 : (outer > 1 ? 1 : outer);
            inner = inner < 0 ? 0 : (inner > 1 ? 1 : inner);
            float cov = outer - inner;
            if (cov > 0.0f) blend(c, xx, yy, color, cov * alpha);
        }
}

static unsigned rng = 1;
static float rnd(float lo, float hi) { rng = rng * 1103515245u + 12345u; return lo + (hi - lo) * ((rng >> 8) & 0xFFFF) / 65535.0f; }

int main(void)
{
    enum { W = 160, H = 120 };
    static uint32_t a[W * H], b[W * H];
    Canvas ca = { a, W, H }, cb = { b, W, H };
    int bad = 0, cases = 0;
    for (int t = 0; t < 4000; t++) {
        float x = rnd(-20, 140), y = rnd(-20, 100), w = rnd(1, 120), h = rnd(1, 90), r = rnd(0, 40), th = rnd(0.5f, 4);
        float al = t % 3 ? 1.0f : rnd(0.1f, 1.0f);
        uint32_t col = RGB((t * 37) & 255, (t * 91) & 255, (t * 13) & 255);
        for (int i = 0; i < W * H; i++) a[i] = b[i] = RGB(i & 255, (i >> 3) & 255, 90);
        if (t & 1) { gfx_round_frame(&ca, x, y, w, h, r, th, col, al); ref_round_frame(&cb, x, y, w, h, r, th, col, al); }
        else { gfx_round_rect(&ca, x, y, w, h, r, col, al); ref_round_rect(&cb, x, y, w, h, r, col, al); }
        cases++;
        for (int i = 0; i < W * H; i++) {
            int worst = 0;
            for (int s = 0; s < 24; s += 8) {
                int d = abs((int)((a[i] >> s) & 255) - (int)((b[i] >> s) & 255));
                if (d > worst) worst = d;
            }
            if (worst > 1) {
                if (bad < 10) printf("diverso: caso %d (%s x %.1f y %.1f w %.1f h %.1f r %.1f) pixel %d,%d\n", t, t & 1 ? "cornice" : "pieno", x, y, w, h, r, i % W, i / W);
                bad++;
                break;
            }
        }
    }
    printf("%d casi, %d diversi\n", cases, bad);
    printf("ESITO: %s\n", bad ? "ERRORI" : "tutto bene");
    return bad ? 1 : 0;
}
