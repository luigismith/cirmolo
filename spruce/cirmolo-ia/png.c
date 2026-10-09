/* Cirmolo IA - PNG minimo (vedi png.h). */
#include "png.h"

#include <stdlib.h>
#include <string.h>

static uint32_t crc_table[256];
static int crc_ready;

static uint32_t crc(const unsigned char *d, size_t n, uint32_t c)
{
    if (!crc_ready) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t v = i;
            for (int k = 0; k < 8; k++) v = v & 1 ? 0xEDB88320u ^ (v >> 1) : v >> 1;
            crc_table[i] = v;
        }
        crc_ready = 1;
    }
    c ^= 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) c = crc_table[(c ^ d[i]) & 255] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static void be32(unsigned char *p, uint32_t v) { p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16); p[2] = (unsigned char)(v >> 8); p[3] = (unsigned char)v; }

static unsigned char *chunk(unsigned char *o, const char *type, const unsigned char *data, size_t n)
{
    be32(o, (uint32_t)n);
    memcpy(o + 4, type, 4);
    if (n) memcpy(o + 8, data, n);
    be32(o + 8 + n, crc(o + 4, n + 4, 0));
    return o + 12 + n;
}

unsigned char *png_encode_argb(const uint32_t *px, int w, int h, size_t *len)
{
    size_t row = (size_t)w * 4 + 1, raw = row * (size_t)h;
    unsigned char *r = malloc(raw);
    if (!r) return NULL;
    for (int y = 0; y < h; y++) {
        unsigned char *d = r + row * (size_t)y;
        *d++ = 0;                                  /* nessun filtro */
        for (int x = 0; x < w; x++) {
            uint32_t p = px[(size_t)y * (size_t)w + (size_t)x];
            *d++ = (unsigned char)(p >> 16); *d++ = (unsigned char)(p >> 8); *d++ = (unsigned char)p; *d++ = (unsigned char)(p >> 24);
        }
    }
    /* zlib: blocchi "stored" da 65535 byte, poi adler-32 */
    size_t nblocks = raw / 65535 + 1, zlen = 2 + raw + nblocks * 5 + 4;
    unsigned char *z = malloc(zlen), *zp = z;
    if (!z) { free(r); return NULL; }
    *zp++ = 0x78; *zp++ = 0x01;
    size_t off = 0;
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw; i++) { a = (a + r[i]) % 65521; b = (b + a) % 65521; }
    do {
        size_t k = raw - off > 65535 ? 65535 : raw - off;
        *zp++ = off + k >= raw ? 1 : 0;
        *zp++ = (unsigned char)(k & 255); *zp++ = (unsigned char)(k >> 8);
        *zp++ = (unsigned char)(~k & 255); *zp++ = (unsigned char)((~k >> 8) & 255);
        memcpy(zp, r + off, k);
        zp += k;
        off += k;
    } while (off < raw);
    be32(zp, b << 16 | a);
    zp += 4;
    zlen = (size_t)(zp - z);
    free(r);
    size_t total = 8 + 25 + 12 + zlen + 12;
    unsigned char *o = malloc(total), *op = o;
    if (!o) { free(z); return NULL; }
    static const unsigned char SIG[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
    memcpy(op, SIG, 8);
    op += 8;
    unsigned char ihdr[13];
    be32(ihdr, (uint32_t)w);
    be32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;   /* 8 bit, RGBA */
    op = chunk(op, "IHDR", ihdr, 13);
    op = chunk(op, "IDAT", z, zlen);
    op = chunk(op, "IEND", NULL, 0);
    free(z);
    *len = (size_t)(op - o);
    return o;
}

int png_size(const unsigned char *d, size_t n, int *w, int *h)
{
    if (n < 24 || memcmp(d + 1, "PNG", 3) || memcmp(d + 12, "IHDR", 4)) return -1;
    *w = (int)((uint32_t)d[16] << 24 | (uint32_t)d[17] << 16 | (uint32_t)d[18] << 8 | d[19]);
    *h = (int)((uint32_t)d[20] << 24 | (uint32_t)d[21] << 16 | (uint32_t)d[22] << 8 | d[23]);
    return *w > 0 && *h > 0 ? 0 : -1;
}
