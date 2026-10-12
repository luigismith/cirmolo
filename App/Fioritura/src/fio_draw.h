/* Fioritura - effetti di luce sul framebuffer del kit: bagliori e anelli in somma (la luce si accumula
 * come sullo schermo di un oscilloscopio), cieli sfumati con dithering (lo schermo della Flip mostra le
 * bande sui gradienti scuri). Tutto a interi dove conta: girano a 30 fotogrammi sulla console. */
#ifndef FIORITURA_DRAW_H
#define FIORITURA_DRAW_H

#include <stdint.h>

#include "gfx.h"

/* Bagliore rotondo che sfuma verso il bordo ((1 - d^2/r^2)^2), sommato ai pixel. */
void fx_glow(Canvas *c, float cx, float cy, float r, uint32_t color, float alpha);
/* Come fx_glow ma a blocchi di 2x2 pixel: per gli aloni grandi e tenui (le nebbie), quattro volte meno
   calcoli, a occhio uguale. */
void fx_glow_soft(Canvas *c, float cx, float cy, float r, uint32_t color, float alpha);
/* Anello di raggio r e spessore w, con i bordi sfumati, sommato ai pixel. */
void fx_ring(Canvas *c, float cx, float cy, float r, float w, uint32_t color, float alpha);
/* Riga orizzontale sottile sommata (y frazionaria: si divide tra due righe di pixel). */
void fx_hline(Canvas *c, float y, int x0, int x1, uint32_t color, float alpha);
/* Cielo: sfumatura verticale da top a bottom, un alone sull'orizzonte e dithering ordinato. */
void fx_sky(uint32_t *dst, int w, int h, uint32_t top, uint32_t bottom, uint32_t haze);
/* dst = a + (b - a) * t, pixel per pixel. */
void fx_blend(uint32_t *dst, const uint32_t *a, const uint32_t *b, int n, float t);

#endif
