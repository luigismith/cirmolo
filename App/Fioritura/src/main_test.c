/* Fioritura - prove senza console: strumenti, intonazione, tempi, effetti, bordone, registrazione,
 * giardino (ritorni, memoria, respiro, annulla, sogno), lucciole (regole e tempo), MIDI, Launchpad,
 * stato, pannello, schermate (BMP) e prestazioni.
 *
 * Uso: fio-test <cartella dei font> <cartella di uscita>
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "fio_app.h"
#include "fio_draw.h"
#include "fio_dsp.h"
#include "fio_lp.h"
#include "gfx.h"
#include "midi.h"
#include "platform.h"

#ifdef _WIN32
#include <direct.h>
#define make_dir(p) _mkdir(p)
#else
#include <sys/stat.h>
#define make_dir(p) mkdir(p, 0755)
#endif

#define SR 48000
#define BLOCK 1024
/* Soglie delle prestazioni. Sulla Flip dipendono dal modo della CPU: le app girano in «performance»
   (1,8 GHz, memoria veloce), ma lanciate da SSH con la console sul menu le prove trovano 1,1 GHz e la
   memoria a 324 MHz, e tutto costa quasi il doppio (12/10/2026: disegno 9,4 contro 18 ms). */
#ifdef __aarch64__
#define DRAW_MS 22.0
#define DSP_10S 3.0
#define WHERE "sulla Flip"
#else
#define DRAW_MS 3.0
#define DSP_10S 1.0
#define WHERE "sul PC"
#endif

static int checks, fails;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("  ERRORE: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const char *OUT = ".";

/* ------------------------------------------------------------------ aiuti */
static void write_bmp(const char *name, const Canvas *c)
{
    char path[600];
    snprintf(path, sizeof(path), "%s/%s", OUT, name);
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    int row = (c->w * 3 + 3) & ~3, size = 54 + row * c->h;
    unsigned char hdr[54] = { 'B', 'M' };
    hdr[2] = size; hdr[3] = size >> 8; hdr[4] = size >> 16; hdr[5] = size >> 24;
    hdr[10] = 54; hdr[14] = 40;
    hdr[18] = c->w; hdr[19] = c->w >> 8; hdr[22] = c->h; hdr[23] = c->h >> 8;
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

static void write_wav(const char *name, const float *lr, int frames)
{
    char path[600];
    snprintf(path, sizeof(path), "%s/%s", OUT, name);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    unsigned int data = (unsigned)frames * 4, v;
    fwrite("RIFF", 1, 4, f); v = 36 + data; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f);
    unsigned short s = 1; fwrite(&s, 2, 1, f); s = 2; fwrite(&s, 2, 1, f);
    v = SR; fwrite(&v, 4, 1, f); v = SR * 4; fwrite(&v, 4, 1, f);
    s = 4; fwrite(&s, 2, 1, f); s = 16; fwrite(&s, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
    for (int i = 0; i < frames * 2; i++) {
        float x = lr[i] < -1 ? -1 : (lr[i] > 1 ? 1 : lr[i]);
        short q = (short)lrintf(x * 32767.0f);
        fwrite(&q, 2, 1, f);
    }
    fclose(f);
}

static void render(Fio *f, float sec, float *dst)
{
    static float blk[2 * BLOCK];
    int frames = (int)(sec * SR);
    for (int done = 0; done < frames; done += BLOCK) {
        int n = frames - done < BLOCK ? frames - done : BLOCK;
        fio_render(f, blk, n);
        if (dst) memcpy(dst + 2 * (size_t)done, blk, sizeof(float) * 2 * (size_t)n);
    }
}

/* L'app come sulla console: aggiornamento e un blocco audio per volta. */
static void run(FioApp *a, float sec, float *dst)
{
    static float blk[2 * BLOCK];
    int frames = (int)(sec * SR);
    for (int done = 0; done < frames; done += BLOCK) {
        int n = frames - done < BLOCK ? frames - done : BLOCK;
        fioapp_update(a, (float)n / SR);
        fioapp_audio(a, blk, n);
        if (dst) memcpy(dst + 2 * (size_t)done, blk, sizeof(float) * 2 * (size_t)n);
    }
}

/* Come run, ma annota l'istante di ogni voce che parte (un campione per blocco: fio_last_start). */
static int run_starts(FioApp *a, float sec, long long *starts, int max)
{
    static float blk[2 * BLOCK];
    Fio *f = fioapp_engine(a);
    int frames = (int)(sec * SR), n = 0, prev = fio_voice_starts(f);
    for (int done = 0; done < frames; done += BLOCK) {
        int k = frames - done < BLOCK ? frames - done : BLOCK;
        fioapp_update(a, (float)k / SR);
        fioapp_audio(a, blk, k);
        int now = fio_voice_starts(f);
        if (now != prev && n < max) starts[n++] = fio_last_start(f);
        prev = now;
    }
    return n;
}

static double rms(const float *lr, int frames)
{
    double e = 0;
    for (int i = 0; i < 2 * frames; i++) e += (double)lr[i] * lr[i];
    return frames ? sqrt(e / (2.0 * frames)) : 0.0;
}
static double max_rms50(const float *lr, int frames)
{
    int w = SR / 20;
    double best = 0;
    for (int s = 0; s + w <= frames; s += w / 2) { double r = rms(lr + 2 * (size_t)s, w); if (r > best) best = r; }
    return best;
}
static float peak_of(const float *lr, int frames, int *finite)
{
    float p = 0;
    *finite = 1;
    for (int i = 0; i < 2 * frames; i++) { if (!isfinite(lr[i])) *finite = 0; else if (fabsf(lr[i]) > p) p = fabsf(lr[i]); }
    return p;
}
/* Energia della componente a hz (Goertzel sul canale sinistro + destro). */
static double goertzel(const float *lr, int frames, double hz)
{
    double w = 2.0 * M_PI * hz / SR, cw = 2.0 * cos(w), s1 = 0, s2 = 0;
    for (int i = 0; i < frames; i++) { double x = lr[2 * i] + lr[2 * i + 1], s0 = x + cw * s1 - s2; s2 = s1; s1 = s0; }
    return sqrt(s1 * s1 + s2 * s2 - cw * s1 * s2) / frames;
}
/* Fondamentale per autocorrelazione normalizzata, con interpolazione parabolica. */
static double estimate_f0(const float *lr, int frames, double fmin, double fmax)
{
    int lo = (int)(SR / fmax), hi = (int)(SR / fmin);
    static double mono[SR];
    int n = frames < SR ? frames : SR;
    for (int i = 0; i < n; i++) mono[i] = lr[2 * i] + lr[2 * i + 1];
    double best = -1, bv = 0;
    int bl = lo;
    static double r[4000];
    for (int L = lo; L <= hi && L < 4000; L++) {
        double s = 0, e0 = 0, e1 = 0;
        for (int i = 0; i + L < n; i++) { s += mono[i] * mono[i + L]; e0 += mono[i] * mono[i]; e1 += mono[i + L] * mono[i + L]; }
        r[L] = s / sqrt(e0 * e1 + 1e-12);
    }
    /* il primo massimo vicino al migliore (evita le ottave sotto) */
    for (int L = lo + 1; L < hi && L < 3999; L++) if (r[L] > bv) { bv = r[L]; bl = L; }
    for (int L = lo + 1; L < hi && L < 3999; L++)
        if (r[L] > 0.9 * bv && r[L] >= r[L - 1] && r[L] >= r[L + 1]) { bl = L; break; }
    double a = r[bl - 1], b = r[bl], c = r[bl + 1], den = a - 2 * b + c;
    best = bl + (fabs(den) > 1e-12 ? 0.5 * (a - c) / den : 0.0);
    return SR / best;
}
static double cents(double f, double ref) { return 1200.0 * log2(f / ref); }
static double midi_hz(double n) { return 440.0 * pow(2.0, (n - 69.0) / 12.0); }

static FioMix dry_mix(void)
{
    FioMix m;
    memset(&m, 0, sizeof(m));
    m.volume = 1.0f;
    m.delay_time = 0.5f;
    m.drone_note = 38.0f;
    return m;
}
static FioNote note_of(int timbre, float note, float vel)
{
    FioNote n;
    n.timbre = timbre; n.note = note; n.vel = vel; n.pan = 0.0f; n.bright = 0.5f; n.length = 3.0f;
    return n;
}

/* ------------------------------------------------------------------ motore */
static void test_timbres(void)
{
    static float audio[2 * SR * 4];
    static float all[2 * SR * 4 * TIM_COUNT];
    double loud[TIM_COUNT];
    for (int t = 0; t < TIM_COUNT; t++) {
        Fio *f = fio_create(SR);
        FioMix m = dry_mix();
        fio_set_mix(f, &m);
        FioNote n = note_of(t, 64, 0.8f);
        fio_play(f, 0, &n);
        render(f, 4.0f, audio);
        memcpy(all + (size_t)t * 2 * SR * 4, audio, sizeof(float) * 2 * SR * 4);
        int fin;
        float pk = peak_of(audio, SR * 4, &fin);
        loud[t] = max_rms50(audio, SR * 4);
        CHECK(fin && pk <= 1.0f && pk > 0.02f, "%s: picco %.3f", fio_timbre_name(t), pk);
        CHECK(fio_voice_starts(f) == 1, "%s: una voce partita (%d)", fio_timbre_name(t), fio_voice_starts(f));
        render(f, 8.0f, NULL);
        CHECK(fio_active_voices(f) == 0, "%s: la voce finisce (%d attive dopo 12 s)", fio_timbre_name(t), fio_active_voices(f));
        fio_destroy(f);
    }
    double lo = 1e9, hi = 0;
    for (int t = 0; t < TIM_COUNT; t++) { if (loud[t] < lo) lo = loud[t]; if (loud[t] > hi) hi = loud[t]; }
    printf("  volume dei sei strumenti (RMS max su 50 ms):");
    for (int t = 0; t < TIM_COUNT; t++) printf(" %s %.3f", fio_timbre_name(t), loud[t]);
    printf("\n");
    CHECK(hi / lo < 2.5, "strumenti bilanciati: rapporto %.2f", hi / lo);
    write_wav("fioritura-strumenti.wav", all, SR * 4 * TIM_COUNT);
}

static void test_pitch(void)
{
    static float audio[2 * SR];
    int tims[3] = { TIM_ARPA, TIM_SOFFIO, TIM_GOCCIA };
    float notes[3] = { 45, 64, 81 };
    for (int k = 0; k < 3; k++)
        for (int j = 0; j < 3; j++) {
            Fio *f = fio_create(SR);
            FioMix m = dry_mix();
            fio_set_mix(f, &m);
            FioNote n = note_of(tims[k], notes[j], 0.8f);
            n.bright = 0.2f;
            fio_play(f, 0, &n);
            render(f, tims[k] == TIM_SOFFIO ? 0.6f : 0.1f, NULL);
            render(f, 0.25f, audio);
            double hz = estimate_f0(audio, SR / 4, midi_hz(notes[j]) * 0.7, midi_hz(notes[j]) * 1.4);
            double c = cents(hz, midi_hz(notes[j]));
            CHECK(fabs(c) < 8.0, "%s nota %.0f: %.2f Hz invece di %.2f (%+.1f cent)", fio_timbre_name(tims[k]), notes[j], hz, midi_hz(notes[j]), c);
            fio_destroy(f);
        }
}

static void test_timing(void)
{
    Fio *f = fio_create(SR);
    FioMix m = dry_mix();
    fio_set_mix(f, &m);
    render(f, 0.1f, NULL);
    long long at = fio_clock(f) + 10000;
    FioNote n = note_of(TIM_GOCCIA, 72, 0.9f);
    fio_play(f, at, &n);
    static float audio[2 * SR];
    long long base = fio_clock(f);
    render(f, 0.5f, audio);
    long long onset = -1;
    for (int i = 0; i < SR / 2; i++) if (fabsf(audio[2 * i]) > 1e-4f || fabsf(audio[2 * i + 1]) > 1e-4f) { onset = base + i; break; }
    CHECK(onset >= at && onset - at <= 2, "nota a tempo: chiesta a %lld, sentita a %lld", at, onset);
    CHECK(fio_last_start(f) == at, "voce partita al campione giusto (%lld per %lld)", fio_last_start(f), at);
    /* coda piena: niente blocchi, le voci restano 32 */
    int refused = 0;
    for (int i = 0; i < 1500; i++) { FioNote q = note_of(i % TIM_COUNT, 50 + i % 30, 0.5f); refused += fio_play(f, 0, &q) != 0; }
    CHECK(refused > 0, "coda piena: %d note rifiutate", refused);
    render(f, 0.05f, NULL);
    CHECK(fio_active_voices(f) <= FIO_VOICES, "voci al massimo %d (%d)", FIO_VOICES, fio_active_voices(f));
    int fin;
    render(f, 0.5f, audio);
    float pk = peak_of(audio, SR / 2, &fin);
    CHECK(fin && pk <= 1.0f, "mille note insieme: picco %.3f, nessun valore strano", pk);
    fio_destroy(f);
}

static void test_effects(void)
{
    static float audio[2 * SR * 2];
    /* riverbero: coda dopo la nota */
    double tail[2];
    for (int k = 0; k < 2; k++) {
        Fio *f = fio_create(SR);
        FioMix m = dry_mix();
        m.reverb = k ? 0.9f : 0.0f;
        fio_set_mix(f, &m);
        FioNote n = note_of(TIM_GOCCIA, 72, 0.9f);
        fio_play(f, 0, &n);
        render(f, 1.0f, NULL);
        render(f, 0.5f, audio);
        tail[k] = rms(audio, SR / 2);
        fio_destroy(f);
    }
    CHECK(tail[1] > tail[0] * 20.0 + 1e-5, "riverbero: coda %.5f contro %.5f", tail[1], tail[0]);

    /* eco: dopo delay_time torna la nota */
    {
        Fio *f = fio_create(SR);
        FioMix m = dry_mix();
        m.delay = 1.0f;
        m.delay_time = 0.4f;
        fio_set_mix(f, &m);
        render(f, 0.02f, NULL);
        FioNote n = note_of(TIM_GOCCIA, 76, 0.9f);
        fio_play(f, 0, &n);
        render(f, 1.0f, audio);
        double before = rms(audio + 2 * (int)(0.30 * SR), SR / 20), at = rms(audio + 2 * (int)(0.40 * SR), SR / 20);
        CHECK(at > before * 3.0, "eco a 0,4 s: %.4f contro %.4f prima", at, before);
        fio_destroy(f);
    }

    /* shimmer: l'ottava sopra nella coda, e il giro resta stabile al massimo */
    double ratio[2];
    for (int k = 0; k < 2; k++) {
        Fio *f = fio_create(SR);
        FioMix m = dry_mix();
        m.reverb = 0.9f;
        m.shimmer = (float)k;
        fio_set_mix(f, &m);
        FioNote n = note_of(TIM_SOFFIO, 69, 0.9f);
        n.length = 1.0f;
        fio_play(f, 0, &n);
        render(f, 2.5f, NULL);
        render(f, 1.0f, audio);
        ratio[k] = goertzel(audio, SR, 880.0) / (goertzel(audio, SR, 440.0) + 1e-9);
        fio_destroy(f);
    }
    CHECK(ratio[1] > ratio[0] * 2.0, "shimmer: ottava sopra %.3f contro %.3f", ratio[1], ratio[0]);
    {
        Fio *f = fio_create(SR);
        FioMix m = dry_mix();
        m.reverb = 1.0f;
        m.shimmer = 1.0f;
        fio_set_mix(f, &m);
        for (int i = 0; i < 8; i++) { FioNote n = note_of(i % TIM_COUNT, 60 + 3 * i, 1.0f); fio_play(f, 0, &n); }
        render(f, 1.0f, audio);
        int fin;
        float early = peak_of(audio, SR, &fin);
        render(f, 12.0f, NULL);
        render(f, 1.0f, audio);
        float late = peak_of(audio, SR, &fin);
        CHECK(fin && late < early * 0.5f, "shimmer e spazio al massimo: la coda si spegne (%.3f dopo 13 s, %.3f all'inizio)", late, early);
        fio_destroy(f);
    }
}

static void test_drone(void)
{
    static float audio[2 * SR * 3];
    Fio *f = fio_create(SR);
    FioMix m = dry_mix();
    m.drone = 0.6f;
    m.drone_note = 38.0f;
    fio_set_mix(f, &m);
    render(f, 1.5f, audio);
    double on = rms(audio + SR, SR / 2);
    CHECK(on > 0.01, "bordone acceso: RMS %.4f", on);
    m.drone_note = 45.0f;                       /* la tonica cambia: scivola senza scatti */
    fio_set_mix(f, &m);
    render(f, 2.0f, audio);
    float jump = 0;
    for (int i = 2; i < 2 * SR * 2; i += 2) jump = fmaxf(jump, fabsf(audio[i] - audio[i - 2]));
    CHECK(jump < 0.05f, "bordone che cambia nota: salto massimo fra campioni %.4f", jump);
    render(f, 3.0f, audio);
    double e_new = goertzel(audio + 2 * SR * 2, SR, midi_hz(45)), e_old = goertzel(audio + 2 * SR * 2, SR, midi_hz(38));
    CHECK(e_new > 4.0 * e_old, "bordone arrivato sul La: %.5f contro %.5f sul Re di prima", e_new, e_old);
    m.drone = 0.0f;
    fio_set_mix(f, &m);
    render(f, 3.0f, NULL);
    render(f, 3.0f, audio);
    CHECK(rms(audio + 2 * SR * 2, SR) < 1e-4, "bordone spento: silenzio (%.6f)", rms(audio + 2 * SR * 2, SR));
    fio_destroy(f);
}

static void test_tap(void)
{
    Fio *f = fio_create(SR);
    FioMix m = dry_mix();
    m.drone = 0.5f;
    fio_set_mix(f, &m);
    render(f, 0.2f, NULL);
    fio_tap(f, 1);
    static float audio[2 * SR], got[2 * SR];
    render(f, 0.5f, audio);
    int n = fio_tap_read(f, got, SR);
    CHECK(n == SR / 2, "registrazione: %d frames su %d", n, SR / 2);
    CHECK(!memcmp(got, audio, sizeof(float) * 2 * (size_t)(n < SR / 2 ? n : SR / 2)), "registrazione identica all'uscita");
    fio_tap(f, 0);
    render(f, 0.2f, NULL);
    CHECK(fio_tap_read(f, got, SR) == 0, "rubinetto chiuso: niente");
    fio_destroy(f);
}

/* ------------------------------------------------------------------ giardino */
static void fresh_state(const char *sub, char *path, size_t n)
{
    char dir[600];
    snprintf(dir, sizeof(dir), "%s/%s", OUT, sub);
    make_dir(dir);
    snprintf(path, n, "%s/stato.txt", dir);
    remove(path);
}

static void press(FioApp *a, int b) { fioapp_button(a, b, 1); fioapp_update(a, 0.016f); fioapp_button(a, b, 0); fioapp_update(a, 0.016f); }
static void hold(FioApp *a, int b, float sec)
{
    fioapp_button(a, b, 1);
    for (float t = 0; t < sec; t += 0.016f) fioapp_update(a, 0.016f);
    fioapp_button(a, b, 0);
    fioapp_update(a, 0.016f);
}

static void test_garden(void)
{
    char path[600];
    fresh_state("prova-giardino", path, sizeof(path));
    FioApp *a = fioapp_create(SR, path);
    FioGlobal *g = fioapp_global(a);
    g->respiro = 4.0f; g->memoria = 3; g->vento = 0.0f; g->sogno = 0;
    fioapp_global_changed(a);
    run(a, 0.1f, NULL);
    fioapp_set_cursor(a, 0.3f, 0.5f);
    fioapp_button(a, PAD_A, 1);
    fioapp_button(a, PAD_A, 0);
    long long st[16];
    int n = run_starts(a, 17.0f, st, 16);
    CHECK(n == 4, "un fiore con memoria 3: %d note (1 + 3 ritorni)", n);
    for (int i = 1; i < n && i < 4; i++) {
        double d = (double)(st[i] - st[i - 1]) / SR;
        CHECK(fabs(d - 4.0) < 0.002, "ritorno %d dopo %.4f s (respiro 4 s)", i, d);
    }
    CHECK(fioapp_seed_count(a) == 0, "dopo la memoria il fiore se ne va (%d)", fioapp_seed_count(a));

    /* per sempre, poi il respiro cambia a meta' */
    g->memoria = 0;
    fioapp_global_changed(a);
    press(a, PAD_A);
    n = run_starts(a, 9.0f, st, 16);
    CHECK(n == 3 && fioapp_seed_count(a) == 1, "per sempre: %d note in 9 s, fiori %d", n, fioapp_seed_count(a));
    g->respiro = 6.0f;
    fioapp_global_changed(a);
    n = run_starts(a, 13.0f, st, 16);
    CHECK(n == 2 || n == 3, "respiro 6 s: %d ritorni in 13 s", n);
    for (int i = 1; i < n; i++) {
        double d = (double)(st[i] - st[i - 1]) / SR;
        CHECK(fabs(d - 6.0) < 0.002, "ritorno dopo %.4f s (respiro 6 s)", d);
    }

    /* l'interfaccia si blocca mezzo secondo: il ritorno arriva in ritardo ma non si perde */
    {
        int before = fio_voice_starts(fioapp_engine(a));
        static float blk[2 * BLOCK];
        for (int i = 0; i < (int)(6.5 * SR / BLOCK); i++) fioapp_audio(a, blk, BLOCK);   /* niente update */
        run(a, 0.2f, NULL);
        int got = fio_voice_starts(fioapp_engine(a)) - before;
        CHECK(got == 1, "interfaccia ferma 6,5 s: il ritorno arriva comunque, una volta (%d)", got);
    }

    /* annulla e svuota */
    fioapp_set_cursor(a, 0.6f, 0.8f); press(a, PAD_A);
    fioapp_set_cursor(a, 0.7f, 0.2f); press(a, PAD_A);
    CHECK(fioapp_seed_count(a) == 3, "tre fiori (%d)", fioapp_seed_count(a));
    press(a, PAD_B);
    CHECK(fioapp_seed_count(a) == 2, "B toglie l'ultimo (%d)", fioapp_seed_count(a));
    hold(a, PAD_B, 1.0f);
    CHECK(fioapp_seed_count(a) == 0, "B tenuto svuota (%d)", fioapp_seed_count(a));

    /* A tenuto e croce: ogni corda nuova e' un fiore */
    fioapp_set_cursor(a, 0.5f, 0.3f);
    fioapp_button(a, PAD_A, 1);
    fioapp_update(a, 0.016f);
    for (int i = 0; i < 4; i++) { fioapp_button(a, PAD_UP, 1); fioapp_update(a, 0.06f); fioapp_button(a, PAD_UP, 0); fioapp_update(a, 0.06f); }
    fioapp_button(a, PAD_A, 0);
    fioapp_update(a, 0.016f);
    CHECK(fioapp_seed_count(a) == 5, "glissando con A tenuto: %d fiori (1 + 4 corde)", fioapp_seed_count(a));

    /* sogno: da fermo pianta da solo, un tasto lo sveglia */
    hold(a, PAD_B, 1.0f);
    g->sogno = 1; g->memoria = 0;
    fioapp_global_changed(a);
    run(a, 14.0f, NULL);
    CHECK(fioapp_dreaming(a) && fioapp_seed_count(a) >= 3, "sogno: %d fiori piantati da solo", fioapp_seed_count(a));
    press(a, PAD_RIGHT);
    CHECK(!fioapp_dreaming(a), "un tasto sveglia il giardino");

    /* MIDI: una nota della scala diventa un fiore e suona subito */
    int before = fio_voice_starts(fioapp_engine(a)), seeds = fioapp_seed_count(a);
    unsigned char on[3] = { 0x90, 62, 100 };
    fioapp_midi(a, 0, on, 3);
    run(a, 0.05f, NULL);
    CHECK(fioapp_seed_count(a) == seeds + 1 && fio_voice_starts(fioapp_engine(a)) > before, "nota MIDI: un fiore in piu' (%d) e il suono", fioapp_seed_count(a));
    fioapp_destroy(a);
}

/* ------------------------------------------------------------------ lucciole */
static void test_fireflies_rules(void)
{
    int hits[16], n;
    Firefly f1[1] = { { 6, 3, 0 } };
    n = lucciole_step(f1, 1, hits, 16);
    CHECK(n == 0 && f1[0].r == 7 && f1[0].c == 3, "una lucciola sale (%d,%d)", f1[0].r, f1[0].c);
    n = lucciole_step(f1, 1, hits, 16);
    CHECK(n == 1 && hits[0] == 3 && f1[0].d == 2 && f1[0].r == 7, "sul bordo in alto suona la colonna 3 e torna giu' (colpo %d, dir %d)", n ? hits[0] : -1, f1[0].d);
    n = lucciole_step(f1, 1, hits, 16);
    CHECK(n == 0 && f1[0].r == 6, "poi scende (%d)", f1[0].r);

    Firefly f2[2] = { { 3, 2, 1 }, { 3, 4, 3 } };
    n = lucciole_step(f2, 2, hits, 16);
    CHECK(f2[0].r == 3 && f2[0].c == 3 && f2[1].c == 3 && f2[0].d == 2 && f2[1].d == 0, "due lucciole si incontrano e girano a destra (%d %d)", f2[0].d, f2[1].d);

    Firefly f3[2] = { { 7, 5, 0 }, { 7, 5, 0 } };
    n = lucciole_step(f3, 2, hits, 16);
    CHECK(n == 1 && hits[0] == 5, "due colpi sullo stesso bordo: una nota sola (%d)", n);

    Firefly f4[1] = { { 2, 0, 3 } };
    n = lucciole_step(f4, 1, hits, 16);
    CHECK(n == 1 && hits[0] == 3 * 8 + 2 && f4[0].d == 1, "bordo sinistro: la riga 2 (colpo %d)", n ? hits[0] : -1);

    Firefly f5[1] = { { 0, 0, 2 } };
    n = lucciole_step(f5, 1, hits, 16);
    CHECK(n == 1 && hits[0] == 2 * 8 + 0, "bordo in basso: la colonna 0 (colpo %d)", n ? hits[0] : -1);
}

static void test_fireflies_app(void)
{
    char path[600];
    fresh_state("prova-lucciole", path, sizeof(path));
    FioApp *a = fioapp_create(SR, path);
    FioGlobal *g = fioapp_global(a);
    g->sogno = 0; g->tempo = 120.0f;
    fioapp_global_changed(a);
    fioapp_set_mode(a, MODE_FIREFLIES);
    fioapp_set_cell(a, 6, 3);
    press(a, PAD_A);
    CHECK(fioapp_firefly_count(a) == 1 && fioapp_fireflies(a)[0].d == 0, "A: nasce una lucciola verso l'alto");
    press(a, PAD_A);
    CHECK(fioapp_fireflies(a)[0].d == 1, "A di nuovo: gira a destra");
    press(a, PAD_A); press(a, PAD_A); press(a, PAD_A);
    CHECK(fioapp_firefly_count(a) == 0, "al quinto colpo vola via (%d)", fioapp_firefly_count(a));
    press(a, PAD_A);
    press(a, PAD_Y);
    CHECK(fioapp_playing(a), "Y: in volo");
    long long st[32];
    int n = run_starts(a, 2.1f, st, 32);
    /* a 120 BPM un passo e' 0,25 s: dalla riga 6 la lucciola tocca il bordo in alto al secondo passo, poi
       scende per 7 passi fino al bordo in basso: in 2 s una nota sola, poi la seconda a 2,25 s */
    CHECK(n == 1, "una nota nei primi 2 s (%d)", n);
    n = run_starts(a, 0.5f, st, 32);
    CHECK(n == 1, "la seconda al bordo in basso (%d)", n);
    press(a, PAD_Y);
    CHECK(!fioapp_playing(a), "Y: ferme");
    fioapp_set_cell(a, 2, 2);
    press(a, PAD_A);
    hold(a, PAD_B, 1.0f);
    CHECK(fioapp_firefly_count(a) == 0, "B tenuto: tutte libere (%d)", fioapp_firefly_count(a));
    fioapp_destroy(a);
}

/* ------------------------------------------------------------------ Launchpad */
static unsigned char sent[64][420];
static int sent_len[64], nsent;
static int hook(int port, const unsigned char *msg, int len)
{
    (void)port;
    if (nsent < 64 && len <= 420) { memcpy(sent[nsent], msg, (size_t)len); sent_len[nsent++] = len; }
    return 0;
}
static int find_sysex(unsigned char cmd, int arg)
{
    for (int i = 0; i < nsent; i++)
        if (sent_len[i] >= 9 && sent[i][0] == 0xF0 && sent[i][5] == 0x0D && sent[i][6] == cmd && (arg < 0 || sent[i][7] == arg)) return i;
    return -1;
}

static void test_launchpad(void)
{
    char path[600];
    fresh_state("prova-launchpad", path, sizeof(path));
    cirmolo_midi_send_hook = hook;
    cirmolo_midi_set_device("LaunchpadMiniMK", 2, NULL);
    FioApp *a = fioapp_create(SR, path);
    fioapp_global(a)->sogno = 0;
    nsent = 0;
    fioapp_midi_status(a, "LaunchpadMiniMK");
    CHECK(fioapp_lp_connected(a), "Launchpad riconosciuto");
    CHECK(find_sysex(0x0E, 1) >= 0, "modalita' Programmer chiesta");
    run(a, 0.1f, NULL);
    int led = find_sysex(0x03, -1);
    CHECK(led >= 0 && sent_len[led] > 7 + 64 * 5, "LED: un SysEx RGB con tutta la griglia (%d byte)", led >= 0 ? sent_len[led] : 0);
    int before = nsent;
    run(a, 0.2f, NULL);
    int again = 0;
    for (int i = before; i < nsent; i++) again += sent[i][6] == 0x03;
    CHECK(again <= 2, "LED: senza cambiamenti non si rimanda tutto (%d SysEx)", again);
    unsigned char pad[3] = { 0x90, 11 + 3 * 10 + 4, 127 };
    fioapp_midi(a, 1, pad, 3);
    CHECK(fioapp_seed_count(a) == 1, "pad del Launchpad: un fiore (%d)", fioapp_seed_count(a));
    unsigned char session[3] = { 0xB0, 95, 127 };
    fioapp_midi(a, 1, session, 3);
    CHECK(fioapp_mode(a) == MODE_FIREFLIES, "Session: lucciole");
    unsigned char pad2[3] = { 0x90, 11, 127 };
    fioapp_midi(a, 1, pad2, 3);
    CHECK(fioapp_firefly_count(a) == 1 && fioapp_fireflies(a)[0].r == 0 && fioapp_fireflies(a)[0].c == 0, "pad in basso a sinistra: una lucciola");
    unsigned char side[3] = { 0xB0, 89 - 10 * 3, 127 };
    fioapp_midi(a, 1, side, 3);
    CHECK(fioapp_mood(a) == 3, "tasto a destra 4: atmosfera %s", fioapp_mood_name(fioapp_mood(a)));
    nsent = 0;
    fioapp_destroy(a);
    CHECK(find_sysex(0x0E, 0) >= 0, "all'uscita il Launchpad torna in Live");
    cirmolo_midi_set_device("", 0, NULL);
    cirmolo_midi_send_hook = NULL;
}

/* ------------------------------------------------------------------ stato, pannello, registrazione */
static void test_state_panel(void)
{
    char path[600], recdir[600];
    fresh_state("prova-stato", path, sizeof(path));
    FioApp *a = fioapp_create(SR, path);
    fioapp_global(a)->sogno = 0;
    fioapp_set_mood(a, 2);
    fioapp_set_cursor(a, 0.2f, 0.4f); press(a, PAD_A);
    fioapp_set_cursor(a, 0.8f, 0.7f); press(a, PAD_A);
    fioapp_set_mode(a, MODE_FIREFLIES);
    fioapp_set_cell(a, 4, 4); press(a, PAD_A); press(a, PAD_A);
    /* pannello: strumento */
    press(a, PAD_START);
    CHECK(fioapp_panel(a) == 0, "START apre il pannello");
    int tim = fioapp_mood_timbre(a);
    press(a, PAD_DOWN);
    press(a, PAD_RIGHT);
    CHECK(fioapp_panel(a) == 1 && fioapp_mood_timbre(a) == (tim + 1) % TIM_COUNT, "pannello: strumento cambiato");
    /* registrazione: due righe sopra la prima c'e' «Registra in WAV» */
    snprintf(recdir, sizeof(recdir), "%s/prova-stato/registrazioni", OUT);
    fioapp_set_record_dir(a, recdir);
    press(a, PAD_UP); press(a, PAD_UP); press(a, PAD_UP);
    CHECK(fioapp_panel(a) == 16, "riga Registra (%d)", fioapp_panel(a));
    press(a, PAD_A);
    CHECK(fioapp_recording(a), "registrazione partita");
    run(a, 1.0f, NULL);
    press(a, PAD_A);
    CHECK(!fioapp_recording(a), "registrazione ferma");
    FILE *f = fopen(fioapp_record_path(a), "rb");
    long size = 0;
    if (f) { fseek(f, 0, SEEK_END); size = ftell(f); fclose(f); }
    long want = 44 + 4L * SR;
    CHECK(f && labs(size - want) <= 4 * 2 * BLOCK, "file WAV di circa un secondo: %ld byte (%s)", size, fioapp_record_path(a));
    press(a, PAD_B);
    CHECK(fioapp_panel(a) == -1, "B chiude il pannello");
    /* aiuto, cambio di modo, uscita */
    hold(a, PAD_SELECT, 0.0f);
    CHECK(fioapp_mode(a) == MODE_GARDEN, "SELECT: giardino");
    fioapp_button(a, PAD_SELECT, 1);
    for (int i = 0; i < 30; i++) fioapp_update(a, 0.016f);
    CHECK(fioapp_help(a), "SELECT tenuto: aiuto");
    fioapp_button(a, PAD_SELECT, 0);
    fioapp_update(a, 0.016f);
    CHECK(!fioapp_help(a) && fioapp_mode(a) == MODE_GARDEN, "lasciato SELECT: l'aiuto si chiude e il modo resta");
    press(a, PAD_MENU);
    press(a, PAD_B);
    CHECK(!fioapp_wants_quit(a), "MENU poi B: si resta");
    press(a, PAD_MENU);
    press(a, PAD_A);
    CHECK(fioapp_wants_quit(a), "MENU poi A: si esce");
    int seeds = fioapp_seed_count(a), ffs = fioapp_firefly_count(a), d0 = fioapp_fireflies(a)[0].d, timb = fioapp_mood_timbre(a);
    fioapp_destroy(a);

    a = fioapp_create(SR, path);
    CHECK(fioapp_mood(a) == 2, "stato: atmosfera ripresa (%d)", fioapp_mood(a));
    CHECK(fioapp_seed_count(a) == seeds, "stato: %d fiori ripresi su %d", fioapp_seed_count(a), seeds);
    CHECK(fioapp_firefly_count(a) == ffs && fioapp_fireflies(a)[0].d == d0, "stato: lucciole riprese");
    CHECK(fioapp_mood_timbre(a) == timb, "stato: strumento dell'atmosfera ripreso");
    int before = fio_voice_starts(fioapp_engine(a));
    run(a, 15.0f, NULL);
    CHECK(fio_voice_starts(fioapp_engine(a)) > before, "il giardino ripreso suona da solo");
    fioapp_destroy(a);
}

/* ------------------------------------------------------------------ schermate e prestazioni */
static double ms_of(clock_t t0, int n) { return (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC / n; }
static void bench_primitives(void)
{
    static uint32_t px[640 * 480], src[640 * 480];
    Canvas c = { px, 640, 480 };
    int N = 200;
    clock_t t0 = clock();
    for (int i = 0; i < N; i++) memcpy(px, src, sizeof(px));
    printf("  memcpy schermo: %.3f ms\n", ms_of(t0, N));
    float r[] = { 3, 6, 12, 24, 40 };
    for (int k = 0; k < 5; k++) {
        t0 = clock();
        for (int i = 0; i < N * 10; i++) fx_glow(&c, 100.0f + (float)(i % 400), 200.0f, r[k], RGB(120, 200, 255), 0.3f);
        printf("  bagliore r=%.0f: %.4f ms\n", r[k], ms_of(t0, N * 10));
    }
    t0 = clock();
    for (int i = 0; i < N; i++) fx_ring(&c, 320.0f, 240.0f, 70.0f, 2.0f, RGB(120, 200, 255), 0.3f);
    printf("  anello r=70: %.4f ms\n", ms_of(t0, N));
    t0 = clock();
    for (int i = 0; i < N; i++) fx_hline(&c, 100.5f + (float)(i % 50), 34, 606, RGB(120, 200, 255), 0.05f);
    printf("  corda: %.4f ms\n", ms_of(t0, N));
    t0 = clock();
    for (int i = 0; i < 20; i++) fx_glow_soft(&c, 320.0f, 240.0f, 180.0f, RGB(120, 200, 255), 0.05f);
    printf("  nebbia r=180: %.3f ms\n", ms_of(t0, 20));
    t0 = clock();
    for (int i = 0; i < N; i++) gfx_text(&c, FONT_SMALL, 20, 470, "A pianta   B togli   X atmosfera   Y sogno   L2 R2 memoria   SELECT lucciole", RGB(200, 200, 200));
    printf("  riga di testo: %.4f ms\n", ms_of(t0, N));

    /* l'app come sulla console per 20 s: aggiornamento a 60 Hz, disegno quando lo chiede, audio a blocchi;
       un giardino tipico (dodici fiori) con il sogno acceso e nessuno che tocca */
    char path[600];
    snprintf(path, sizeof(path), "%s/prova-consumo/stato.txt", OUT);
    { char d[600]; snprintf(d, sizeof(d), "%s/prova-consumo", OUT); make_dir(d); remove(path); }
    FioApp *a = fioapp_create(SR, path);
    FioGlobal *g = fioapp_global(a);
    g->respiro = 12.0f; g->memoria = 6; g->sogno = 1;
    fioapp_global_changed(a);
    for (int i = 0; i < 12; i++) { fioapp_set_cursor(a, 0.08f * (float)i + 0.05f, (float)((i * 7) % 12) / 12.0f + 0.04f); press(a, PAD_A); }
    static float blk[2 * BLOCK];
    long long audio_done = 0;
    int draws = 0;
    t0 = clock();
    for (int f = 0; f < 60 * 20; f++) {
        fioapp_update(a, 1.0f / 60.0f);
        if (fioapp_needs_draw(a)) { fioapp_draw(a, &c); draws++; }
        while (audio_done < (long long)(f + 1) * SR / 60) { fioapp_audio(a, blk, BLOCK); audio_done += BLOCK; }
    }
    double cpu = (double)(clock() - t0) / CLOCKS_PER_SEC;
    printf("  app per 20 s (%d disegni, %d fiori alla fine): %.2f s di CPU = %.0f%% di un core %s, schermo a parte\n",
           draws, fioapp_seed_count(a), cpu, cpu / 20.0 * 100.0, WHERE);
    fioapp_destroy(a);
}

static void test_screens(void)
{
    char path[600];
    fresh_state("prova-schermate", path, sizeof(path));
    static uint32_t px[640 * 480];
    Canvas c = { px, 640, 480 };
    FioApp *a = fioapp_create(SR, path);
    FioGlobal *g = fioapp_global(a);
    g->sogno = 0; g->respiro = 9.0f; g->memoria = 8;
    fioapp_global_changed(a);
    /* un giardino: una ventina di fiori sparsi, poi qualche secondo perche' tornino */
    unsigned seed = 7;
    for (int i = 0; i < 22; i++) {
        seed = seed * 1103515245u + 12345u;
        float x = (float)((seed >> 8) % 1000) / 1000.0f;
        seed = seed * 1103515245u + 12345u;
        float y = (float)((seed >> 8) % 1000) / 1000.0f;
        fioapp_set_cursor(a, 0.05f + 0.9f * x, 0.05f + 0.9f * y);
        fioapp_button(a, PAD_A, 1); fioapp_button(a, PAD_A, 0);
        run(a, 0.35f, NULL);
    }
    fioapp_set_cursor(a, 0.62f, 0.55f);
    run(a, 1.2f, NULL);
    fioapp_draw(a, &c);
    write_bmp("1-giardino.bmp", &c);
    for (int m = 1; m < fioapp_mood_count(); m++) {
        fioapp_set_mood(a, m);
        run(a, 2.0f, NULL);
        fioapp_draw(a, &c);
        char name[64];
        snprintf(name, sizeof(name), "2-atmosfera-%d-%s.bmp", m, fioapp_mood_name(m));
        write_bmp(name, &c);
    }
    fioapp_set_mood(a, 3);
    run(a, 7.0f, NULL);                         /* senza toccare: la cornice sparisce */
    fioapp_draw(a, &c);
    write_bmp("3-giardino-pulito.bmp", &c);
    press(a, PAD_START);
    press(a, PAD_DOWN);
    run(a, 0.3f, NULL);
    fioapp_draw(a, &c);
    write_bmp("4-pannello.bmp", &c);
    press(a, PAD_B);
    fioapp_button(a, PAD_SELECT, 1);
    for (int i = 0; i < 30; i++) fioapp_update(a, 0.016f);
    fioapp_draw(a, &c);
    write_bmp("5-aiuto-giardino.bmp", &c);
    fioapp_button(a, PAD_SELECT, 0);
    fioapp_update(a, 0.016f);

    /* lucciole */
    fioapp_set_mode(a, MODE_FIREFLIES);
    int cells[][3] = { { 1, 1, 0 }, { 5, 2, 1 }, { 3, 6, 2 }, { 6, 4, 3 }, { 2, 3, 1 }, { 7, 7, 2 }, { 0, 5, 0 } };
    for (int i = 0; i < 7; i++) {
        fioapp_set_cell(a, cells[i][0], cells[i][1]);
        for (int k = 0; k <= cells[i][2]; k++) press(a, PAD_A);
    }
    fioapp_set_cell(a, 4, 3);
    press(a, PAD_Y);
    run(a, 3.13f, NULL);
    fioapp_draw(a, &c);
    write_bmp("6-lucciole.bmp", &c);
    fioapp_button(a, PAD_SELECT, 1);
    for (int i = 0; i < 30; i++) fioapp_update(a, 0.016f);
    fioapp_draw(a, &c);
    write_bmp("7-aiuto-lucciole.bmp", &c);
    fioapp_button(a, PAD_SELECT, 0);
    fioapp_update(a, 0.016f);
    fioapp_set_mode(a, MODE_GARDEN);
    press(a, PAD_MENU);
    fioapp_draw(a, &c);
    write_bmp("8-uscita.bmp", &c);
    press(a, PAD_B);

    /* prestazioni: disegno con il giardino pieno e il motore con 32 voci */
    fioapp_set_mood(a, 0);
    for (int i = 0; i < 30; i++) { fioapp_set_cursor(a, (float)(i % 10) / 10.0f + 0.05f, (float)(i / 10) / 3.0f + 0.1f); press(a, PAD_A); }
    run(a, 0.5f, NULL);
    clock_t t0 = clock();
    for (int i = 0; i < 100; i++) { fioapp_update(a, 1.0f / 30.0f); fioapp_draw(a, &c); }
    double ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC / 100.0;
    printf("  disegno: %.2f ms a fotogramma %s (giardino con %d fiori)\n", ms, WHERE, fioapp_seed_count(a));
    CHECK(ms < DRAW_MS, "disegno abbastanza leggero: %.2f ms", ms);
    fioapp_destroy(a);

    Fio *f = fio_create(SR);
    FioMix m = dry_mix();
    m.reverb = 0.8f; m.shimmer = 0.5f; m.delay = 0.4f; m.drone = 0.5f;
    fio_set_mix(f, &m);
    t0 = clock();
    for (int s = 0; s < 10; s++) {
        for (int i = 0; i < FIO_VOICES; i++) { FioNote n = note_of(i % TIM_COUNT, 48 + i, 0.6f); n.length = 6.0f; fio_play(f, 0, &n); }
        render(f, 1.0f, NULL);
    }
    double sec = (double)(clock() - t0) / CLOCKS_PER_SEC;
    printf("  motore: 10 s con 32 voci ed effetti in %.3f s %s (%.1f%% del tempo reale)\n", sec, WHERE, sec * 10.0);
    CHECK(sec < DSP_10S, "motore leggero: %.3f s per 10 s", sec);
    fio_destroy(f);
}

int main(int argc, char **argv)
{
    if (argc > 2) OUT = argv[2];
    make_dir(OUT);
    if (argc > 1) {
        char r[600], b[600];
        snprintf(r, sizeof(r), "%s/BeVietnamPro-Regular.ttf", argv[1]);
        snprintf(b, sizeof(b), "%s/BeVietnamPro-SemiBold.ttf", argv[1]);
        if (gfx_load_fonts(r, b)) printf("font non trovati in %s: le schermate saranno senza testo\n", argv[1]);
    }
    struct { const char *name; void (*fn)(void); } T[] = {
        { "strumenti", test_timbres }, { "intonazione", test_pitch }, { "tempi e coda", test_timing },
        { "effetti", test_effects }, { "bordone", test_drone }, { "registrazione", test_tap },
        { "giardino", test_garden }, { "regole delle lucciole", test_fireflies_rules }, { "lucciole", test_fireflies_app },
        { "Launchpad", test_launchpad }, { "stato e pannello", test_state_panel }, { "schermate e prestazioni", test_screens },
        { "primitive", bench_primitives },
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++) {
        int c0 = checks, f0 = fails;
        T[i].fn();
        printf("%s: %d controlli, %d errori\n", T[i].name, checks - c0, fails - f0);
    }
    printf("TOTALE: %d controlli, %d errori\n", checks, fails);
    return fails ? 1 : 0;
}
