/* Diapason - prove senza console: rilevamento delle note su segnali sintetici, metronomo, schermate.
 * Uso: diapason-test <cartella dei font> <cartella di uscita>
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diapason.h"
#include "gfx.h"
#include "platform.h"

#define SR 48000
#define BLOCK 1024

static void write_bmp(const char *path, const Canvas *c)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    int row = (c->w * 3 + 3) & ~3, size = 54 + row * c->h;
    unsigned char hdr[54] = { 'B', 'M' };
    hdr[2] = size; hdr[3] = size >> 8; hdr[4] = size >> 16; hdr[5] = size >> 24;
    hdr[10] = 54; hdr[14] = 40; hdr[18] = c->w; hdr[19] = c->w >> 8; hdr[22] = c->h; hdr[23] = c->h >> 8;
    hdr[26] = 1; hdr[28] = 24;
    fwrite(hdr, 1, 54, f);
    unsigned char *line = calloc(1, (size_t)row);
    for (int y = c->h - 1; y >= 0; y--) {
        for (int x = 0; x < c->w; x++) {
            uint32_t p = c->px[y * c->w + x];
            line[x * 3] = p & 255; line[x * 3 + 1] = (p >> 8) & 255; line[x * 3 + 2] = (p >> 16) & 255;
        }
        fwrite(line, 1, (size_t)row, f);
    }
    free(line);
    fclose(f);
}

static unsigned rng = 12345;
static float noise(void) { rng = rng * 1103515245u + 12345u; return ((rng >> 9) & 0xFFFF) / 32768.0f - 1.0f; }

/* "corda": armoniche 1..harm con ampiezza 1/k, smorzamento lento, rumore */
static void feed(Diapason *d, float freq, float seconds, int harm, float noise_amp, float amp)
{
    static float buf[BLOCK];
    static double t;
    int blocks = (int)(seconds * SR / BLOCK);
    for (int b = 0; b < blocks; b++) {
        for (int i = 0; i < BLOCK; i++) {
            double tt = t + (double)i / SR;
            float s = 0.0f;
            for (int k = 1; k <= harm; k++) s += sinf((float)(2 * M_PI * freq * k * tt)) / k;
            buf[i] = amp * s * expf(-(float)(b * BLOCK + i) / SR * 0.8f) + noise_amp * noise();
        }
        t += (double)BLOCK / SR;
        diapason_capture(d, buf, BLOCK);
        diapason_update(d, (float)BLOCK / SR);
    }
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "uso: %s <font> <uscita>\n", argv[0]); return 2; }
    char p1[512], p2[512], out[600];
    snprintf(p1, sizeof(p1), "%s/BeVietnamPro-Regular.ttf", argv[1]);
    snprintf(p2, sizeof(p2), "%s/BeVietnamPro-SemiBold.ttf", argv[1]);
    if (gfx_load_fonts(p1, p2)) return 1;
    snprintf(out, sizeof(out), "%s/diapason-stato.txt", argv[2]);
    remove(out);
    Diapason *d = diapason_create(SR, out);
    uint32_t *px = malloc(sizeof(uint32_t) * 640 * 480);
    Canvas c = { px, 640, 480 };
    int fail = 0;

    /* senza microfono */
    diapason_draw(d, &c);
    snprintf(out, sizeof(out), "%s/d1-senza-microfono.bmp", argv[2]);
    write_bmp(out, &c);

    diapason_capture_status(d, "Microfono USB di prova", SR);
    diapason_set_instrument(d, 0);                               /* cromatico */
    static const float tests[] = { 41.20f, 55.0f, 82.41f, 110.0f, 196.0f, 261.63f, 440.0f, 659.26f, 987.77f, 1318.5f };
    printf("cromatico, corda con 6 armoniche e rumore:\n");
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        feed(d, tests[i], 0.6f, 6, 0.02f, 0.3f);
        float f = diapason_freq(d);
        float err = 1200.0f * log2f(f / tests[i]);
        printf("  %8.2f Hz -> %8.2f Hz (%+.2f cent), nota %d, scarto mostrato %+.1f cent\n", tests[i], f, err, diapason_note(d), diapason_cents(d));
        if (!(fabsf(err) < 3.0f)) { fprintf(stderr, "ERRORE: %g Hz letto come %g Hz\n", tests[i], f); fail = 1; }
    }
    /* nota calante di 23 centesimi */
    feed(d, 440.0f * powf(2.0f, -23.0f / 1200.0f), 0.6f, 4, 0.01f, 0.3f);
    printf("La calante di 23 cent: nota %d, %+.1f cent\n", diapason_note(d), diapason_cents(d));
    if (diapason_note(d) != 69 || fabsf(diapason_cents(d) + 23.0f) > 2.0f) { fprintf(stderr, "ERRORE: scarto sbagliato\n"); fail = 1; }
    /* chitarra: 112 Hz e' la corda La (110 Hz) crescente di 31 cent */
    diapason_set_instrument(d, 1);
    feed(d, 112.0f, 0.6f, 6, 0.02f, 0.3f);
    printf("chitarra, 112 Hz: nota %d (La2 = 45), %+.1f cent\n", diapason_note(d), diapason_cents(d));
    if (diapason_note(d) != 45 || fabsf(diapason_cents(d) - 31.2f) > 2.0f) { fprintf(stderr, "ERRORE: corda sbagliata\n"); fail = 1; }
    feed(d, 440.0f * powf(2.0f, 3.0f / 1200.0f), 0.6f, 4, 0.01f, 0.3f);
    diapason_set_instrument(d, 0);
    feed(d, 440.0f * powf(2.0f, 3.0f / 1200.0f), 0.6f, 4, 0.01f, 0.3f);
    for (int i = 0; i < 30; i++) diapason_update(d, 1.0f / 60);
    diapason_draw(d, &c);
    snprintf(out, sizeof(out), "%s/d2-accordatore.bmp", argv[2]);
    write_bmp(out, &c);
    diapason_set_instrument(d, 1);
    feed(d, 82.0f, 0.6f, 6, 0.02f, 0.3f);
    for (int i = 0; i < 30; i++) diapason_update(d, 1.0f / 60);
    diapason_draw(d, &c);
    snprintf(out, sizeof(out), "%s/d3-chitarra.bmp", argv[2]);
    write_bmp(out, &c);
    /* silenzio: dopo un po' la nota sparisce */
    feed(d, 440.0f, 2.0f, 1, 0.0005f, 0.0f);
    printf("silenzio: nota %d\n", diapason_note(d));
    if (diapason_note(d) != -1) { fprintf(stderr, "ERRORE: nota rilevata nel silenzio\n"); fail = 1; }

    /* metronomo: 4 secondi a 120 BPM devono dare 8 battiti */
    diapason_set_page(d, 1);
    diapason_button(d, PAD_UP, 1); diapason_button(d, PAD_UP, 0);   /* 90 -> 91 */
    for (int i = 0; i < 3; i++) { diapason_button(d, PAD_R1, 1); diapason_button(d, PAD_R1, 0); } /* 121 */
    diapason_button(d, PAD_DOWN, 1); diapason_button(d, PAD_DOWN, 0);  /* 120 */
    diapason_button(d, PAD_A, 1); diapason_button(d, PAD_A, 0);
    static float audio[2 * BLOCK];
    uint32_t s0 = diapason_beat_serial(d);
    int peaks = 0;
    float prev = 0.0f;
    for (int b = 0; b < 4 * SR / BLOCK; b++) {
        diapason_audio(d, audio, BLOCK);
        for (int i = 0; i < BLOCK; i++) { float v = fabsf(audio[2 * i]); if (v > 0.3f && prev <= 0.3f) peaks++; prev = v > 0.3f ? v : 0.0f; }
        diapason_update(d, (float)BLOCK / SR);
        if (b == 40) {
            diapason_draw(d, &c);
            snprintf(out, sizeof(out), "%s/d4-metronomo.bmp", argv[2]);
            write_bmp(out, &c);
        }
    }
    uint32_t beats = diapason_beat_serial(d) - s0;
    printf("metronomo 120 BPM per 4 s: %u battiti\n", beats);
    if (beats < 7 || beats > 9) { fprintf(stderr, "ERRORE: battiti sbagliati\n"); fail = 1; }
    (void)peaks;

    diapason_destroy(d);
    gfx_free_fonts();
    free(px);
    printf(fail ? "ESITO: errori\n" : "ESITO: tutto bene\n");
    return fail;
}
