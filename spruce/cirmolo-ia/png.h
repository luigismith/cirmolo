/* Cirmolo IA - PNG minimo: scrive immagini RGBA (senza compressione, deflate "stored") e legge le
 * dimensioni di un PNG. Basta per le immagini sovrapposte al gioco del traduttore. */
#ifndef CIRMOLO_IA_PNG_H
#define CIRMOLO_IA_PNG_H

#include <stddef.h>
#include <stdint.h>

/* px: w*h pixel 0xAARRGGBB. Restituisce il file PNG (malloc) e la sua lunghezza. */
unsigned char *png_encode_argb(const uint32_t *px, int w, int h, size_t *len);
/* Dimensioni dall'intestazione IHDR; 0 se va bene. */
int png_size(const unsigned char *d, size_t n, int *w, int *h);

#endif
