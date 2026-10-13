/* Cirmolo Synth - groovebox: prove del motore senza finestra.
 *
 * Uso: gb-test [cartella di uscita]
 * Suona ogni modello, ogni kit e ogni ritmo in rendering offline e controlla che l'audio sia sano, che i
 * suoni partano e si spengano, che i livelli siano confrontabili; poi prova il sequencer (ratchet,
 * probabilita', condizioni, micro, lunghezze, swing, catena, slide, stutter, registrazione, clock) e
 * misura il tempo di calcolo. Con la cartella scrive anche dei WAV da ascoltare.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <time.h>
#endif

#include "gb_dsp.h"

#define SR GB_RATE

static int checks, fails;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("  ERRORE: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const char *outdir;

static double now_s(void)
{
#ifdef _WIN32
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#endif
}

static void write_wav(const char *name, const float *lr, int frames)
{
    if (!outdir) return;
    char path[700];
    snprintf(path, sizeof(path), "%s/%s", outdir, name);
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    int data = frames * 4;
    unsigned char h[44] = { 'R', 'I', 'F', 'F' };
    int riff = 36 + data, fmt = 16, sr = SR, br = SR * 4;
    short pcm = 1, ch = 2, bits = 16, ba = 4;
    memcpy(h + 4, &riff, 4); memcpy(h + 8, "WAVEfmt ", 8);
    memcpy(h + 16, &fmt, 4); memcpy(h + 20, &pcm, 2); memcpy(h + 22, &ch, 2); memcpy(h + 24, &sr, 4);
    memcpy(h + 28, &br, 4); memcpy(h + 32, &ba, 2); memcpy(h + 34, &bits, 2); memcpy(h + 36, "data", 4); memcpy(h + 40, &data, 4);
    fwrite(h, 1, 44, f);
    short *pcmbuf = malloc(sizeof(short) * 2 * (size_t)frames);
    for (int i = 0; i < frames * 2; i++) {
        float v = lr[i] < -1 ? -1 : (lr[i] > 1 ? 1 : lr[i]);
        pcmbuf[i] = (short)lrintf(v * 32767);
    }
    fwrite(pcmbuf, 2, (size_t)frames * 2, f);
    free(pcmbuf);
    fclose(f);
}

/* rende a blocchi di 256 come il thread audio */
static void render(Gb *g, float *dst, int frames)
{
    for (int done = 0; done < frames; done += 256) {
        int n = frames - done < 256 ? frames - done : 256;
        gb_render_offline(g, dst + 2 * done, n);
    }
}

typedef struct { float peak, rms; int bad, over; } Stats;

static Stats stats(const float *lr, int frames)
{
    Stats s = { 0 };
    double sum = 0;
    for (int i = 0; i < frames * 2; i++) {
        float v = lr[i];
        if (!isfinite(v)) { s.bad++; continue; }
        float a = fabsf(v);
        if (a > s.peak) s.peak = a;
        if (a > 0.85f) s.over++;
        sum += (double)v * v;
    }
    s.rms = (float)sqrt(sum / (frames * 2.0));
    return s;
}

static float db(float x) { return 20.0f * log10f(x > 1e-9f ? x : 1e-9f); }

/* Sonorita' di un colpo: segnale pesato K (BS.1770, 48 kHz), RMS massimo su finestre di 50 ms
   (circa il tempo in cui l'orecchio somma un suono breve). */
static float loudness(const float *lr, int frames)
{
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0, z1 = 0, z2 = 0, w1 = 0, w2 = 0;
    int win = SR * 50 / 1000, hop = SR / 200;
    float *k = malloc(sizeof(float) * (size_t)frames);
    for (int i = 0; i < frames; i++) {
        double x = 0.5 * (lr[2 * i] + lr[2 * i + 1]);
        double y = 1.53512485958697 * x - 2.69169618940638 * x1 + 1.19839281085285 * x2 + 1.69065929318241 * y1 - 0.73248077421585 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        double z = y - 2.0 * z1 + z2 + 1.99004745483398 * w1 - 0.99007225036621 * w2;
        z2 = z1; z1 = y; w2 = w1; w1 = z;
        k[i] = (float)z;
    }
    double best = 0;
    for (int s = 0; s + win <= frames; s += hop) {
        double sum = 0;
        for (int i = s; i < s + win; i++) sum += (double)k[i] * k[i];
        if (sum > best) best = sum;
    }
    free(k);
    return db((float)sqrt(best / win));
}

static Gb *fresh(void)
{
    Gb *g = gb_create(SR);
    GbMaster *m = gb_master(g);
    m->comp = 0; m->pump = 0; m->filter = 0; m->lofi = 0; m->tape = 0; m->volume = 1; m->swing = 0;
    for (int t = 0; t < GB_TRACKS; t++) { gb_mix(g, t)->level = 1; gb_mix(g, t)->delay_send = 0; gb_mix(g, t)->reverb_send = 0; }
    return g;
}

static void clear_pattern(GbPattern *p)
{
    for (int t = 0; t < GB_TRACKS; t++) {
        p->track[t].length = 16;
        p->track[t].speed = 0;
        for (int i = 0; i < GB_MAX_STEPS; i++) {
            GbStep *s = &p->track[t].step[i];
            memset(s, 0, sizeof(*s));
            s->vel = 100; s->prob = 100; s->ratchet = 1; s->len = 1;
            s->lock_tune = s->lock_decay = s->lock_tone = GB_LOCK_NONE;
        }
    }
}

/* inizio di un colpo: primo salto netto fra due campioni (la coda del colpo prima non conta) */
static int onset(const float *lr, int from, int to, float thr)
{
    if (from < 1) {
        if (fabsf(lr[0]) > thr) return 0;
        from = 1;
    }
    for (int i = from; i < to; i++) if (fabsf(lr[2 * i] - lr[2 * i - 2]) > thr) return i;
    return -1;
}

/* frequenza dai passaggi per lo zero in salita (con interpolazione) */
static float freq_at(const float *lr, int from, int to)
{
    double first = -1, last = -1;
    int n = 0;
    for (int i = from + 1; i < to; i++) {
        float a = lr[2 * (i - 1)], b = lr[2 * i];
        if (a < 0 && b >= 0) {
            double t = (i - 1) + a / (a - b);
            if (first < 0) first = t;
            last = t;
            n++;
        }
    }
    if (n < 2) return 0;
    return (float)((n - 1) * SR / (last - first));
}

static float rms_win(const float *lr, int from, int to)
{
    double s = 0;
    for (int i = from; i < to; i++) s += (double)lr[2 * i] * lr[2 * i];
    return (float)sqrt(s / (to - from));
}

/* brillantezza: RMS della differenza fra campioni diviso RMS (cresce con gli armonici acuti) */
static float brightness(const float *lr, int from, int to)
{
    double d = 0, s = 0;
    for (int i = from + 1; i < to; i++) {
        double x = lr[2 * i], y = x - lr[2 * i - 2];
        d += y * y;
        s += x * x;
    }
    return s > 0 ? (float)sqrt(d / s) : 0.0f;
}

static int cmp_float(const void *a, const void *b)
{
    float x = *(const float *)a, y = *(const float *)b;
    return x < y ? -1 : x > y;
}

static float median(const float *v, int n)
{
    float *c = malloc(sizeof(float) * (size_t)n);
    memcpy(c, v, sizeof(float) * (size_t)n);
    qsort(c, (size_t)n, sizeof(float), cmp_float);
    float m = n % 2 ? c[n / 2] : 0.5f * (c[n / 2 - 1] + c[n / 2]);
    free(c);
    return m;
}

static int giro_frames(float bpm, int sixteenths) { return (int)lrint((double)SR * 60.0 / bpm / 4.0 * sixteenths); }

static void slug(char *dst, const char *src, int n)
{
    int j = 0;
    for (int i = 0; src[i] && j < n - 1; i++) {
        char c = src[i];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) dst[j++] = c;
        else if (c >= 'A' && c <= 'Z') dst[j++] = (char)(c - 'A' + 'a');
        else if (j > 0 && dst[j - 1] != '-') dst[j++] = '-';
    }
    while (j > 0 && dst[j - 1] == '-') j--;
    dst[j] = 0;
}

static int clock_ticks, clock_last_abs, clock_bad_gap, clock_block_start;
static void clock_hook(void *user, int off)
{
    int abs_pos = clock_block_start + off;
    if (clock_ticks > 0) {
        int gap = abs_pos - clock_last_abs;
        int want = *(int *)user;
        if (gap < want - 1 || gap > want + 1) clock_bad_gap++;
    }
    clock_last_abs = abs_pos;
    clock_ticks++;
}

int main(int argc, char **argv)
{
    outdir = argc > 1 ? argv[1] : NULL;
    const int MAXF = SR * 16;
    float *buf = malloc(sizeof(float) * 2 * (size_t)MAXF);
    float *big = malloc(sizeof(float) * 2 * (size_t)SR * 60);
    int bigpos = 0;

    /* ---------------------------------------------------------- 1. modelli */
    printf("1. Modelli (%d): colpo a vel 100, level 0.8\n", DM_COUNT);
    float loud[DM_COUNT];
    for (int m = 0; m < DM_COUNT; m++) {
        Gb *g = fresh();
        *gb_drum(g, 0) = (GbDrum){ m, 0, .5f, .5f, .5f, 0, .8f, 0, 0 };
        gb_hit(g, 0, 100 / 127.0f);
        int frames = 0, end = -1;
        while (frames < 13 * SR) {
            render(g, buf + 2 * frames, SR / 10);
            frames += SR / 10;
            if (gb_active_voices(g, 0) == 0) { end = frames; break; }
        }
        Stats s = stats(buf, frames);
        loud[m] = loudness(buf, frames);
        printf("  %-20s picco %.3f  sonorita' %6.1f dB  durata %5.2f s\n", gb_model_name(m), s.peak, loud[m], end < 0 ? -1.0 : (double)end / SR);
        CHECK(s.bad == 0, "%s: campioni non finiti", gb_model_name(m));
        CHECK(s.peak > 0.02f, "%s: non suona (picco %.4f)", gb_model_name(m), s.peak);
        CHECK(s.peak < 0.85f, "%s: picco %.3f oltre il limitatore", gb_model_name(m), s.peak);
        CHECK(end > 0, "%s: la voce non si spegne", gb_model_name(m));
        if (bigpos + SR * 7 / 10 < SR * 60) {
            int n = frames < SR * 7 / 10 ? frames : SR * 7 / 10;
            memcpy(big + 2 * bigpos, buf, sizeof(float) * 2 * (size_t)n);
            for (int i = n; i < SR * 7 / 10; i++) big[2 * (bigpos + i)] = big[2 * (bigpos + i) + 1] = 0;
            for (int i = n - 480; i < n && i >= 0; i++) {           /* sfuma la coda tagliata */
                float w = (float)(n - i) / 480.0f;
                big[2 * (bigpos + i)] *= w; big[2 * (bigpos + i) + 1] *= w;
            }
            bigpos += SR * 7 / 10;
        }
        gb_destroy(g);
    }
    write_wav("modelli.wav", big, bigpos);
    int only_models = argc > 2 && strcmp(argv[2], "modelli") == 0;
    float lmed = median(loud, DM_COUNT);
    printf("  sonorita' mediana %.1f dB; correzioni suggerite (x guadagno):\n  ", lmed);
    for (int m = 0; m < DM_COUNT; m++) printf("%s %.2f%s", gb_model_name(m), powf(10.0f, (lmed - loud[m]) / 20.0f), m % 6 == 5 ? "\n  " : ", ");
    printf("\n");
    for (int m = 0; m < DM_COUNT; m++)
        CHECK(fabsf(loud[m] - lmed) <= 3.0f, "%s: sonorita' %.1f dB, lontana dalla mediana %.1f", gb_model_name(m), loud[m], lmed);
    if (only_models) { printf("%d prove, %d errori\n", checks, fails); return fails ? 1 : 0; }

    /* ---------------------------------------------------------- 2. parametri estremi */
    printf("2. Parametri estremi\n");
    {
        static const float P[6][6] = {   /* tune, decay, tone, snap, drive, vel */
            { -24, 0, 0, 0, 0, 1 }, { 24, 1, 1, 1, 1, 1 }, { 24, 0, 1, 0, 1, .2f },
            { -24, 1, 0, 1, 1, 1 }, { 0, 1, 1, 1, 0, 1 }, { 12, .5f, 0, 1, .5f, .05f } };
        int bad = 0;
        float worst = 0;
        for (int m = 0; m < DM_COUNT; m++) {
            for (int c = 0; c < 6; c++) {
                Gb *g = fresh();
                *gb_drum(g, 0) = (GbDrum){ m, P[c][0], P[c][1], P[c][2], P[c][3], P[c][4], 1, 0, 0 };
                gb_hit(g, 0, P[c][5]);
                render(g, buf, SR);
                Stats s = stats(buf, SR);
                bad += s.bad;
                if (s.peak > worst) worst = s.peak;
                gb_destroy(g);
            }
        }
        printf("  %d modelli x 6 combinazioni: picco massimo %.3f\n", DM_COUNT, worst);
        CHECK(bad == 0, "parametri estremi: %d campioni non finiti", bad);
        CHECK(worst <= 1.0f, "parametri estremi: picco %.3f oltre 1", worst);
    }

    /* ---------------------------------------------------------- 3. kit */
    printf("3. Kit (%d): ogni strumento suona e si spegne\n", gb_kit_count());
    CHECK(gb_kit_count() == 26, "i kit sono %d invece di 26", gb_kit_count());
    for (int k = 0; k < gb_kit_count(); k++) {
        Gb *g = fresh();
        gb_load_kit(g, k);
        int silent = 0, endless = 0, bad = 0;
        float maxp = 0;
        for (int t = 0; t < GB_DRUMS; t++) {
            gb_hit(g, t, 1.0f);
            int frames = 0;
            float pk = 0;
            while (frames < 13 * SR) {
                render(g, buf, SR / 10);
                Stats s = stats(buf, SR / 10);
                bad += s.bad;
                if (s.peak > pk) pk = s.peak;
                frames += SR / 10;
                if (gb_active_voices(g, t) == 0) break;
            }
            if (pk < 0.02f) silent++;
            if (gb_active_voices(g, t) != 0) endless++;
            if (pk > maxp) maxp = pk;
            CHECK(gb_kit_track_name(k, t)[0] != 0 && gb_kit_track_name(k, t)[0] != '?', "kit %d traccia %d senza nome", k, t);
        }
        int hats = gb_drum(g, 4)->model == DM_HAT_CLOSED || gb_drum(g, 4)->model == DM_HAT_909 || gb_drum(g, 4)->model == DM_CHIP_HAT;
        printf("  %-20s picco %.3f%s\n", gb_kit_name(k), maxp, hats ? "" : " (senza charleston in 4)");
        CHECK(!silent && !endless && !bad, "kit %s: muti %d, senza fine %d, non finiti %d", gb_kit_name(k), silent, endless, bad);
        if (hats && (gb_drum(g, 5)->model == DM_HAT_OPEN || gb_drum(g, 5)->model == DM_HAT_909 || gb_drum(g, 5)->model == DM_CHIP_HAT))
            CHECK(gb_drum(g, 4)->choke != 0 && gb_drum(g, 4)->choke == gb_drum(g, 5)->choke, "kit %s: charleston senza choke", gb_kit_name(k));
        CHECK(maxp < 1.0f, "kit %s: picco %.3f", gb_kit_name(k), maxp);
        gb_destroy(g);
    }

    /* ---------------------------------------------------------- 4. ritmi pronti */
    int ng = gb_groove_count();
    printf("4. Ritmi pronti (%d): due giri col kit consigliato\n", ng);
    CHECK(ng >= 30, "solo %d ritmi", ng);
    float g_rms[64];
    int kit_has[64] = { 0 };
    for (int i = 0; i < ng; i++) {
        Gb *g = gb_create(SR);
        int k = gb_groove_kit(i);
        gb_load_kit(g, k);
        GbPattern *p = gb_pattern(g, 0);
        gb_groove_load(i, p);
        gb_master(g)->bpm = gb_groove_bpm(i);
        gb_master(g)->swing = gb_groove_swing(i);
        gb_mix(g, 1)->reverb_send = 0.15f;
        gb_mix(g, 3)->reverb_send = 0.15f;
        gb_mix(g, 5)->delay_send = 0.1f;
        int giro = 16;
        for (int t = 0; t < GB_DRUMS; t++) {
            static const double sf[4] = { 1, 0.5, 2, 4.0 / 3 };
            double d = p->track[t].length * sf[p->track[t].speed & 3];
            if (d > giro) giro = (int)lrint(d);
        }
        int frames = giro_frames(gb_groove_bpm(i), giro) * 2;
        if (frames > MAXF) frames = MAXF;
        gb_play(g, 1);
        render(g, buf, frames);
        Stats s = stats(buf, frames);
        g_rms[i] = db(s.rms);
        printf("  %-24s %-20s %3.0f bpm  picco %.3f  RMS %6.1f dB  oltre 0.85: %.2f%%\n", gb_groove_name(i), gb_kit_name(k),
               gb_groove_bpm(i), s.peak, g_rms[i], 100.0 * s.over / (frames * 2));
        CHECK(s.bad == 0, "%s: campioni non finiti", gb_groove_name(i));
        CHECK(s.peak <= 1.0f, "%s: picco %.3f", gb_groove_name(i), s.peak);
        CHECK(s.over < frames * 2 / 100, "%s: limitatore troppo impegnato (%.2f%%)", gb_groove_name(i), 100.0 * s.over / (frames * 2));
        CHECK(s.rms > 0.02f, "%s: troppo piano (RMS %.4f)", gb_groove_name(i), s.rms);
        char name[128], sl[80];
        slug(sl, gb_groove_name(i), sizeof(sl));
        snprintf(name, sizeof(name), "ritmo-%02d-%s.wav", i + 1, sl);
        write_wav(name, buf, frames);
        if (k >= 0 && k < 64 && !kit_has[k]) {      /* il primo ritmo di ogni kit: prima gli strumenti uno per uno */
            kit_has[k] = 1;
            Gb *h = gb_create(SR);
            gb_load_kit(h, k);
            int pos = 0, step = SR * 35 / 100;
            for (int t = 0; t < GB_DRUMS; t++) {
                gb_hit(h, t, 100 / 127.0f);
                render(h, big + 2 * pos, step);
                pos += step;
            }
            memcpy(big + 2 * pos, buf, sizeof(float) * 2 * (size_t)frames);
            pos += frames;
            slug(sl, gb_kit_name(k), sizeof(sl));
            snprintf(name, sizeof(name), "kit-%02d-%s.wav", k + 1, sl);
            write_wav(name, big, pos);
            gb_destroy(h);
        }
        gb_destroy(g);
    }
    for (int k = 0; k < gb_kit_count(); k++) CHECK(kit_has[k], "il kit %s non ha un ritmo consigliato", gb_kit_name(k));
    float gmed = median(g_rms, ng);
    float gmin = 1e9f, gmax = -1e9f;
    for (int i = 0; i < ng; i++) { if (g_rms[i] < gmin) gmin = g_rms[i]; if (g_rms[i] > gmax) gmax = g_rms[i]; }
    printf("  RMS dei ritmi: mediana %.1f dB, da %.1f a %.1f\n", gmed, gmin, gmax);
    for (int i = 0; i < ng; i++)
        CHECK(fabsf(g_rms[i] - gmed) <= 6.0f, "%s: RMS %.1f dB, oltre 6 dB dalla mediana %.1f", gb_groove_name(i), g_rms[i], gmed);

    /* ---------------------------------------------------------- 5. livelli fra i kit con lo stesso ritmo */
    printf("5. Livelli fra i kit: stesso ritmo di riferimento su tutte le 12 tracce\n");
    {
        static const char *REF[GB_DRUMS] = {
            "x...x...x...x...", "....x.......x...", "..x.......x.....", "............x...",
            "x.x.x.x.x.x.x.x.", "..............x.", "...........x....", "..........x.....",
            "x...............", "......x.........", "...x.......x....", ".......x.......x" };
        float kr[64];
        int nk = gb_kit_count();
        for (int k = 0; k < nk; k++) {
            Gb *g = gb_create(SR);
            gb_load_kit(g, k);
            GbPattern *p = gb_pattern(g, 0);
            clear_pattern(p);
            for (int t = 0; t < GB_DRUMS; t++)
                for (int i = 0; i < 16; i++) p->track[t].step[i].on = REF[t][i] == 'x';
            gb_play(g, 1);
            int frames = giro_frames(120, 32);
            render(g, buf, frames);
            Stats s = stats(buf, frames);
            kr[k] = db(s.rms);
            gb_destroy(g);
        }
        float km = median(kr, nk), kmin = 1e9f, kmax = -1e9f;
        for (int k = 0; k < nk; k++) { if (kr[k] < kmin) kmin = kr[k]; if (kr[k] > kmax) kmax = kr[k]; }
        printf("  RMS mediana %.1f dB, da %.1f a %.1f\n  ", km, kmin, kmax);
        for (int k = 0; k < nk; k++) printf("%s %+.1f%s", gb_kit_name(k), kr[k] - km, k % 6 == 5 ? "\n  " : ", ");
        printf("\n");
        for (int k = 0; k < nk; k++)
            CHECK(fabsf(kr[k] - km) <= 6.0f, "kit %s: %+.1f dB dalla mediana", gb_kit_name(k), kr[k] - km);
    }

    /* ---------------------------------------------------------- 6. sequencer */
    printf("6. Sequencer\n");
    const int S16 = SR * 60 / 120 / 4;     /* 6000 campioni per sedicesimo a 120 bpm */
    {   /* choke */
        Gb *g = fresh();
        gb_load_kit(g, 0);
        gb_hit(g, 5, 1);
        render(g, buf, SR / 20);
        int open_before = gb_active_voices(g, 5);
        gb_hit(g, 4, 1);
        render(g, buf, SR / 100);
        int open_after = gb_active_voices(g, 5);
        gb_drum(g, 4)->choke = 0;
        gb_hit(g, 5, 1);
        render(g, buf, SR / 20);
        gb_hit(g, 4, 1);
        render(g, buf, SR / 100);
        int open_free = gb_active_voices(g, 5);
        printf("  choke: aperto prima %d, dopo il chiuso %d, senza choke %d\n", open_before, open_after, open_free);
        CHECK(open_before > 0 && open_after == 0 && open_free > 0, "choke del charleston");
        gb_destroy(g);
    }
    {   /* ratchet */
        Gb *g = fresh();
        GbPattern *p = gb_pattern(g, 0);
        clear_pattern(p);
        *gb_drum(g, 0) = (GbDrum){ DM_CLAVE, 0, .2f, .5f, .5f, 0, .8f, 0, 0 };
        p->track[0].step[0].on = 1; p->track[0].step[0].ratchet = 4;
        p->track[0].step[2].on = 1; p->track[0].step[2].ratchet = 3;
        gb_play(g, 1);
        render(g, buf, S16 * 7 / 10);
        uint32_t a = gb_track_flash(g, 0);
        render(g, buf, S16 / 10 + 1);
        uint32_t b = gb_track_flash(g, 0);
        render(g, buf, S16 * 2);
        uint32_t c = gb_track_flash(g, 0);
        printf("  ratchet: 4 -> %u dopo 0.7 passi, %u dopo 0.8; poi 3 -> %u\n", a, b, c - b);
        CHECK(a == 3 && b == 4 && c - b == 3, "ratchet: colpi %u, %u, %u", a, b, c - b);
        gb_destroy(g);
    }
    {   /* ratchet senza clic: il ritaglio sfuma */
        Gb *g = fresh();
        GbPattern *p = gb_pattern(g, 0);
        clear_pattern(p);
        *gb_drum(g, 0) = (GbDrum){ DM_KICK_808, 0, 1, .5f, 0, 0, .8f, 0, 0 };   /* snap 0: niente clic voluto */
        p->track[0].step[0].on = 1; p->track[0].step[0].ratchet = 4;
        gb_play(g, 1);
        render(g, buf, S16);
        float maxjump = 0;
        for (int i = 1; i < S16; i++) { float d = fabsf(buf[2 * i] - buf[2 * i - 2]); if (d > maxjump) maxjump = d; }
        printf("  ratchet sulla 808: salto massimo fra campioni %.4f\n", maxjump);
        CHECK(maxjump < 0.05f, "ratchet: salto di %.4f (clic)", maxjump);
        gb_destroy(g);
    }
    {   /* probabilita' 100, 0, 50 */
        uint32_t got[3];
        int probs[3] = { 100, 0, 50 };
        for (int k = 0; k < 3; k++) {
            Gb *g = fresh();
            GbPattern *p = gb_pattern(g, 0);
            clear_pattern(p);
            *gb_drum(g, 0) = (GbDrum){ DM_CLAVE, 0, .2f, .5f, .5f, 0, .8f, 0, 0 };
            for (int i = 0; i < 16; i++) { p->track[0].step[i].on = 1; p->track[0].step[i].prob = (uint8_t)probs[k]; }
            gb_play(g, 1);
            for (int b = 0; b < 8; b++) render(g, buf, S16 * 16);
            got[k] = gb_track_flash(g, 0);
            gb_destroy(g);
        }
        printf("  probabilita': 100%% -> %u, 0%% -> %u, 50%% -> %u colpi su 128\n", got[0], got[1], got[2]);
        CHECK(got[0] == 128 && got[1] == 0 && got[2] > 40 && got[2] < 88, "probabilita'");
    }
    {   /* condizioni */
        Gb *g = fresh();
        GbPattern *p = gb_pattern(g, 0);
        clear_pattern(p);
        for (int t = 0; t < 4; t++) *gb_drum(g, t) = (GbDrum){ DM_CLAVE, 0, .2f, .5f, .5f, 0, .8f, 0, 0 };
        p->track[0].step[0].on = 1; p->track[0].step[0].cond = COND_FILL;
        p->track[1].step[0].on = 1; p->track[1].step[0].cond = COND_NOT_FILL;
        p->track[2].step[0].on = 1; p->track[2].step[0].cond = COND_1_2;
        p->track[3].step[0].on = 1; p->track[3].step[0].cond = COND_FIRST;
        p->track[4].step[0].on = 1; p->track[4].step[0].cond = COND_1_4;
        p->track[5].step[0].on = 1; p->track[5].step[0].cond = COND_2_2;
        *gb_drum(g, 4) = *gb_drum(g, 0); *gb_drum(g, 5) = *gb_drum(g, 0);
        gb_drum(g, 4)->choke = gb_drum(g, 5)->choke = 0;
        gb_play(g, 1);
        render(g, buf, S16 * 32);                  /* 2 giri senza fill */
        uint32_t f0 = gb_track_flash(g, 0), n0 = gb_track_flash(g, 1);
        gb_set_fill(g, 1);
        render(g, buf, S16 * 16);                  /* 1 giro col fill */
        gb_set_fill(g, 0);
        render(g, buf, S16 * 16);                  /* 4o giro */
        uint32_t f1 = gb_track_flash(g, 0), n1 = gb_track_flash(g, 1);
        uint32_t c12 = gb_track_flash(g, 2), c22 = gb_track_flash(g, 5), c14 = gb_track_flash(g, 4), cfirst = gb_track_flash(g, 3);
        int bars3 = gb_bar_count(g);
        render(g, buf, 1);                         /* il fine giro si conta al campione dopo */
        printf("  condizioni in 4 giri: FILL %u (fill sul 3o), NON FILL %u, 1:2 %u, 2:2 %u, 1:4 %u, PRIMO %u; giri %d poi %d\n",
               f1, n1, c12, c22, c14, cfirst, bars3, gb_bar_count(g));
        CHECK(f0 == 0 && f1 == 1, "COND_FILL: %u, %u", f0, f1);
        CHECK(n0 == 2 && n1 == 3, "COND_NOT_FILL: %u, %u", n0, n1);
        CHECK(c12 == 2 && c22 == 2, "COND_1_2 / 2_2");
        CHECK(c14 == 1 && cfirst == 1, "COND_1_4 / FIRST");
        CHECK(bars3 == 3 && gb_bar_count(g) == 4, "giri contati %d e %d invece di 3 e 4", bars3, gb_bar_count(g));
        gb_destroy(g);
    }
    {   /* micro, swing, velocita' della traccia: inizio dei colpi al campione */
        Gb *g = fresh();
        GbPattern *p = gb_pattern(g, 0);
        clear_pattern(p);
        *gb_drum(g, 0) = (GbDrum){ DM_CLAVE, 0, .1f, .5f, 0, 0, .8f, 0, 0 };
        int st[4] = { 0, 4, 8, 12 }, mi[4] = { 0, 25, -20, 50 };
        for (int k = 0; k < 4; k++) { p->track[0].step[st[k]].on = 1; p->track[0].step[st[k]].micro = (int8_t)mi[k]; }
        gb_play(g, 1);
        render(g, buf, S16 * 16);
        int worst = 0;
        for (int k = 0; k < 4; k++) {
            int want = st[k] * S16 + mi[k] * S16 / 100;
            int got = onset(buf, want - S16 / 2 > 0 ? want - S16 / 2 : 0, want + S16 / 2, 0.02f);
            int err = abs(got - want);
            if (err > 1) printf("  micro: passo %d atteso %d trovato %d\n", st[k], want, got);
            if (err > worst) worst = err;
        }
        printf("  micro (0, +25, -20, +50%%): errore massimo %d campioni\n", worst);
        CHECK(worst <= 1, "micro: errore %d campioni", worst);
        gb_destroy(g);

        g = fresh();
        p = gb_pattern(g, 0);
        clear_pattern(p);
        *gb_drum(g, 0) = (GbDrum){ DM_CLAVE, 0, .1f, .5f, 0, 0, .8f, 0, 0 };
        for (int i = 0; i < 4; i++) p->track[0].step[i].on = 1;
        gb_master(g)->swing = 0.5f;
        gb_play(g, 1);
        render(g, buf, S16 * 5);
        int o1 = onset(buf, S16 / 2, S16 * 2 - 10, 0.02f), o2 = onset(buf, o1 + S16 / 4, S16 * 3 - 10, 0.02f);
        int o3 = onset(buf, o2 + S16 / 4, S16 * 4, 0.02f);
        printf("  swing 0.5: colpi a %d, %d, %d (attesi %d, %d, %d)\n", o1, o2, o3, S16 * 3 / 2, S16 * 2, S16 * 7 / 2);
        CHECK(abs(o1 - S16 * 3 / 2) <= 1 && abs(o2 - S16 * 2) <= 1 && abs(o3 - S16 * 7 / 2) <= 1, "swing");
        gb_destroy(g);

        g = fresh();
        p = gb_pattern(g, 0);
        clear_pattern(p);
        for (int t = 0; t < 2; t++) *gb_drum(g, t) = (GbDrum){ DM_CLAVE, 0, .1f, .5f, 0, 0, .8f, 0, 0 };
        p->track[0].speed = 3; p->track[0].length = 12;  /* terzine */
        p->track[0].step[3].on = 1;
        p->track[1].speed = 1; p->track[1].length = 32;  /* trentaduesimi */
        p->track[1].step[9].on = 1;
        gb_play(g, 1);
        render(g, buf, S16 * 6);
        int ot = onset(buf, 0, S16 * 6, 0.02f);
        int o32 = onset(buf, ot + S16 / 4, S16 * 6, 0.02f);
        printf("  terzine: passo 3 a %d (atteso %d); trentaduesimi: passo 9 a %d (atteso %d)\n", ot, S16 * 4, o32, S16 * 9 / 2);
        CHECK(abs(ot - S16 * 4) <= 1 && abs(o32 - S16 * 9 / 2) <= 1, "velocita' della traccia");
        gb_destroy(g);
    }
    {   /* lunghezze diverse per traccia */
        Gb *g = fresh();
        GbPattern *p = gb_pattern(g, 0);
        clear_pattern(p);
        for (int t = 0; t < 2; t++) *gb_drum(g, t) = (GbDrum){ DM_CLAVE, 0, .1f, .5f, 0, 0, .8f, 0, 0 };
        p->track[0].length = 3; p->track[0].step[0].on = 1;
        p->track[1].length = 4; p->track[1].step[0].on = 1;
        gb_play(g, 1);
        render(g, buf, S16 * 11 + S16 / 2);
        uint32_t a = gb_track_flash(g, 0), b = gb_track_flash(g, 1);
        int sa = gb_track_step(g, 0), sb = gb_track_step(g, 1);
        printf("  lunghezze 3 e 4 in 12 passi: %u e %u colpi; passo corrente %d e %d\n", a, b, sa, sb);
        CHECK(a == 4 && b == 3 && sa == 2 && sb == 3, "lunghezze per traccia");
        gb_destroy(g);
    }
    {   /* traccia sfasata da una modifica dal vivo: a fine giro torna in fase */
        Gb *g = fresh();
        GbPattern *p = gb_pattern(g, 0);
        clear_pattern(p);
        for (int t = 0; t < 2; t++) {
            *gb_drum(g, t) = (GbDrum){ DM_CLAVE, 0, .1f, .5f, 0, 0, .8f, 0, 0 };
            p->track[t].step[0].on = 1;
        }
        gb_play(g, 1);
        render(g, buf, S16 * 6 + 10);
        p->track[1].length = 4;
        render(g, buf, S16 * 3);
        p->track[1].length = 16;
        render(g, buf, S16 * 4);
        int off = gb_track_step(g, 1) - gb_track_step(g, 0);
        render(g, buf, S16 * 4);                   /* passa il fine giro (16) */
        int s0 = gb_track_step(g, 0), s1 = gb_track_step(g, 1);
        printf("  traccia sfasata dal vivo: scarto %d passi, dopo il fine giro passi %d e %d\n", off, s0, s1);
        CHECK(off != 0 && s0 == s1, "riallineamento a fine giro");
        gb_destroy(g);
    }
    {   /* cambio di pattern a fine giro e catena */
        Gb *g = fresh();
        for (int k = 0; k < 4; k++) {
            GbPattern *p = gb_pattern(g, k);
            clear_pattern(p);
            p->track[k].step[0].on = 1;
            *gb_drum(g, k) = (GbDrum){ DM_CLAVE, 0, .1f, .5f, 0, 0, .8f, 0, 0 };
        }
        gb_pattern(g, 3)->track[3].length = 8;     /* il 3 ha un giro di 8 passi */
        gb_play(g, 1);
        render(g, buf, S16 * 6);
        gb_select_pattern(g, 1);
        render(g, buf, S16 * 4);
        int mid = gb_playing_pattern(g);
        render(g, buf, S16 * 6 + 10);
        int after = gb_playing_pattern(g);
        printf("  cambio: a meta' giro suona %d, dopo la fine %d (colpi traccia 1: %u)\n", mid, after, gb_track_flash(g, 1));
        CHECK(mid == 0 && after == 1 && gb_track_flash(g, 1) == 1, "cambio di pattern a fine giro");
        gb_play(g, 0);
        render(g, buf, 256);
        int chain[2] = { 2, 3 };
        gb_set_chain(g, chain, 2);
        render(g, buf, 256);
        gb_play(g, 1);
        int seq[5], pos[5];
        int at[5] = { 8, 20, 30, 44, 50 }, done = 0;  /* giri: 2 [0,16) 3 [16,24) 2 [24,40) 3 [40,48) 2 [48,64) */
        for (int k = 0; k < 5; k++) {
            render(g, buf, S16 * at[k] - done);
            done = S16 * at[k];
            seq[k] = gb_playing_pattern(g); pos[k] = gb_chain_position(g);
        }
        printf("  catena [2,3]: %d %d %d %d %d (posizioni %d %d %d %d %d)\n", seq[0], seq[1], seq[2], seq[3], seq[4],
               pos[0], pos[1], pos[2], pos[3], pos[4]);
        CHECK(seq[0] == 2 && seq[1] == 3 && seq[2] == 2 && seq[3] == 3 && seq[4] == 2, "catena");
        CHECK(pos[0] == 0 && pos[1] == 1 && pos[2] == 0 && pos[3] == 1 && pos[4] == 0, "posizione nella catena");
        gb_set_chain(g, NULL, 0);
        render(g, buf, 256);
        CHECK(gb_chain_position(g) == -1, "catena tolta");
        gb_destroy(g);
    }
    {   /* blocchi per passo: altezza e durata */
        Gb *g = fresh();
        GbPattern *p = gb_pattern(g, 0);
        clear_pattern(p);
        *gb_drum(g, 0) = (GbDrum){ DM_CLAVE, 0, .3f, 0, 0, 0, .8f, 0, 0 };
        p->track[0].step[0].on = 1;
        p->track[0].step[8].on = 1;
        p->track[0].step[8].lock_tune = -12;
        p->track[0].step[8].lock_decay = 100;
        gb_play(g, 1);
        render(g, buf, S16 * 8);
        float f0 = freq_at(buf, 10, 600);
        int alive0 = 0;
        for (int i = 0; i < S16 * 8; i++) if (fabsf(buf[2 * i]) > 1e-3f) alive0 = i;
        render(g, buf, S16 * 8);
        float f1 = freq_at(buf, 10, 600);
        int alive1 = 0;
        for (int i = 0; i < S16 * 8; i++) if (fabsf(buf[2 * i]) > 1e-3f) alive1 = i;
        printf("  blocchi: claves %.0f Hz, col blocco -12 %.0f Hz; durata %d -> %d campioni col blocco del decay\n", f0, f1, alive0, alive1);
        CHECK(fabsf(f0 - 2500) < 30 && fabsf(f1 - 1250) < 20 && alive1 > alive0 * 2, "blocchi per passo");
        gb_destroy(g);
    }
    {   /* delay ping-pong a tempo e riverbero */
        Gb *g = fresh();
        *gb_drum(g, 0) = (GbDrum){ DM_CLAVE, 0, .1f, .5f, 0, 0, .8f, 0, 0 };
        gb_mix(g, 0)->delay_send = 1;
        gb_master(g)->delay_time = 3;
        gb_master(g)->delay_feedback = .5f;
        gb_master(g)->delay_tone = 1;
        gb_hit(g, 0, 1);
        render(g, buf, SR);
        float l1 = 0, r1 = 0, l2 = 0, r2 = 0;
        for (int i = 3 * S16 - 200; i < 3 * S16 + 800; i++) { l1 = fmaxf(l1, fabsf(buf[2 * i])); r1 = fmaxf(r1, fabsf(buf[2 * i + 1])); }
        for (int i = 6 * S16 - 200; i < 6 * S16 + 800; i++) { l2 = fmaxf(l2, fabsf(buf[2 * i])); r2 = fmaxf(r2, fabsf(buf[2 * i + 1])); }
        int e1 = onset(buf, 3 * S16 - 500, 3 * S16 + 500, 0.005f);
        printf("  delay 3/16: primo eco a %d (atteso %d) sinistra %.3f destra %.3f; secondo eco sinistra %.3f destra %.3f\n",
               e1, 3 * S16, l1, r1, l2, r2);
        CHECK(abs(e1 - 3 * S16) <= 2 && l1 > 10 * r1 && r2 > 10 * l2, "delay ping-pong");
        gb_destroy(g);

        g = fresh();
        *gb_drum(g, 1) = (GbDrum){ DM_SNARE_909, 0, .3f, .5f, .5f, 0, .8f, 0, 0 };
        gb_mix(g, 1)->reverb_send = 1;
        gb_master(g)->reverb_size = .6f;
        gb_hit(g, 1, 1);
        render(g, buf, SR * 3);
        float a = rms_win(buf, SR / 2, SR), b = rms_win(buf, SR * 3 / 2, SR * 2), c = rms_win(buf, SR * 5 / 2, SR * 3);
        printf("  riverbero: coda 0.5-1 s %.1f dB, 1.5-2 s %.1f dB, 2.5-3 s %.1f dB\n", db(a), db(b), db(c));
        CHECK(a > 1e-3f && b < a && c < b && db(a) - db(b) > 3 && db(a) - db(b) < 30, "riverbero");
        write_wav("riverbero.wav", buf, SR * 3);
        gb_destroy(g);
    }
    {   /* scope, picco, livelli per traccia, carico */
        Gb *g = gb_create(SR);
        gb_groove_load(0, gb_pattern(g, 0));
        gb_load_kit(g, gb_groove_kit(0));
        gb_play(g, 1);
        render(g, buf, SR / 2 + 123);
        float sc[GB_SCOPE];
        gb_scope(g, sc, GB_SCOPE);
        float smax = 0;
        for (int i = 0; i < GB_SCOPE; i++) smax = fmaxf(smax, fabsf(sc[i]));
        int n_last = SR / 2 + 123;
        float match = fabsf(sc[GB_SCOPE - 1] - 0.5f * (buf[2 * (n_last - 1)] + buf[2 * (n_last - 1) + 1]));
        printf("  scope: massimo %.3f, ultimo campione scarto %.6f; picco %.3f; livello cassa %.3f, traccia muta %.3f\n",
               smax, match, gb_peak(g), gb_track_level(g, 0), gb_track_level(g, 7));
        CHECK(smax > 0.01f && match < 1e-6f && gb_peak(g) > 0.05f && gb_track_level(g, 0) > 0.05f, "scope, picco, livelli");
        gb_destroy(g);
    }
    {   /* euclideo */
        GbTrackPattern t;
        memset(&t, 0, sizeof(t));
        char s[65];
        gb_euclid(&t, 8, 3, 0, 100);
        for (int i = 0; i < 8; i++) s[i] = t.step[i].on ? 'x' : '.';
        s[8] = 0;
        printf("  euclideo 3 su 8: %s\n", s);
        CHECK(strcmp(s, "x..x..x.") == 0 && t.length == 8, "euclideo 3 su 8: %s", s);
        gb_euclid(&t, 16, 5, 2, 90);
        int hits = 0;
        for (int i = 0; i < 16; i++) { s[i] = t.step[i].on ? 'x' : '.'; hits += t.step[i].on; }
        s[16] = 0;
        printf("  euclideo 5 su 16 ruotato di 2: %s\n", s);
        CHECK(hits == 5 && t.step[2].on && t.step[2].vel == 90, "euclideo 5 su 16");
    }

    /* ---------------------------------------------------------- 7. acido */
    printf("7. Acido\n");
    {
        float f_slide_early = 0, f_slide_late = 0, f_jump = 0, r_before = 0, r_cross = 0, r_cross_ns = 0;
        for (int slide = 1; slide >= 0; slide--) {
            Gb *g = fresh();
            GbPattern *p = gb_pattern(g, 0);
            clear_pattern(p);
            *gb_acid(g) = (GbAcid){ 0, 0, 1, 0, 0, .5f, 0, 0, .8f, 0 };
            gb_master(g)->bpm = 60;
            GbTrackPattern *a = &p->track[GB_ACID];
            a->step[0].on = 1; a->step[0].note = 0; a->step[0].slide = (uint8_t)slide;
            a->step[1].on = 1; a->step[1].note = 12;
            gb_play(g, 1);
            int s16 = SR / 4;
            render(g, buf, s16 * 2);
            if (slide) {
                f_slide_early = freq_at(buf, s16 + 10, s16 + SR / 50);
                f_slide_late = freq_at(buf, s16 + SR / 10, s16 + SR * 13 / 100);
                r_before = rms_win(buf, s16 - SR / 50, s16 - SR / 100);
                r_cross = rms_win(buf, s16 - SR * 3 / 1000, s16 + SR * 3 / 1000);
            } else {
                f_jump = freq_at(buf, s16 + SR / 200, s16 + SR / 40);
                r_cross_ns = rms_win(buf, s16 - SR * 3 / 1000, s16);
            }
            gb_destroy(g);
        }
        printf("  slide: subito dopo %.0f Hz, dopo 100 ms %.0f Hz (Do3 130.8 -> Do4 261.6); senza slide %.0f Hz\n",
               f_slide_early, f_slide_late, f_jump);
        printf("  inviluppo al cambio: col legato %.3f (prima %.3f), senza %.4f\n", r_cross, r_before, r_cross_ns);
        CHECK(f_slide_early > 135 && f_slide_early < 245, "slide: la frequenza subito dopo dovrebbe stare in mezzo (%.0f)", f_slide_early);
        CHECK(fabsf(f_slide_late - 261.6f) < 4, "slide: arrivo %.1f Hz", f_slide_late);
        CHECK(fabsf(f_jump - 261.6f) < 4, "senza slide: salto %.1f Hz", f_jump);
        CHECK(r_cross > 0.5f * r_before && r_cross_ns < 0.2f * r_before, "slide: niente nuovo attacco, gate chiuso senza");
    }
    {   /* accento che si accumula: dopo tre accenti di fila il filtro si apre di piu' che con uno solo */
        float bright[2];
        for (int k = 0; k < 2; k++) {
            Gb *g = fresh();
            GbPattern *p = gb_pattern(g, 0);
            clear_pattern(p);
            *gb_acid(g) = (GbAcid){ 0, -12, .2f, .5f, .4f, .3f, 1, 0, .8f, 0 };
            GbTrackPattern *a = &p->track[GB_ACID];
            for (int i = 0; i < 4; i++) { a->step[i].on = 1; a->step[i].accent = (uint8_t)(k == 0 || i == 3); }
            gb_play(g, 1);
            render(g, buf, S16 * 4);
            bright[k] = brightness(buf, S16 * 3, S16 * 3 + S16 / 3);
            gb_destroy(g);
        }
        printf("  accento: brillantezza del 4o passo dopo 3 accenti %.3f, accento isolato %.3f\n", bright[0], bright[1]);
        CHECK(bright[0] > bright[1] * 1.1f, "l'accento non si accumula");
    }
    {   /* demo: giro acido con accenti e slide, filtro che si apre */
        Gb *g = gb_create(SR);
        gb_load_kit(g, gb_groove_kit(31));
        GbPattern *p = gb_pattern(g, 0);
        gb_groove_load(31, p);
        static const int notes[16] = { 0, 0, 12, 0, 3, 0, 15, 12, 0, 7, 0, 10, 12, 0, 3, 5 };
        static const char acc[] = "x...x..x..x...x.", sld[] = "..x....x.x....x.", on[] = "xxxxx.xxxx.xxxxx";
        GbTrackPattern *a = &p->track[GB_ACID];
        for (int i = 0; i < 16; i++) {
            a->step[i].on = on[i] == 'x';
            a->step[i].note = (int8_t)notes[i];
            a->step[i].accent = acc[i] == 'x';
            a->step[i].slide = sld[i] == 'x';
        }
        gb_master(g)->bpm = 128;
        gb_mix(g, GB_ACID)->delay_send = 0.2f;
        gb_play(g, 1);
        int giro = giro_frames(128, 16), pos = 0;
        for (int k = 0; k < 6; k++) {
            gb_acid(g)->cutoff = 0.15f + 0.12f * k;
            gb_acid(g)->wave = k >= 3;
            render(g, big + 2 * pos, giro);
            pos += giro;
        }
        Stats s = stats(big, pos);
        printf("  giro acido (6 giri, filtro che si apre): picco %.3f, RMS %.1f dB\n", s.peak, db(s.rms));
        CHECK(s.bad == 0 && s.peak <= 1.0f && s.rms > 0.02f && s.over < pos / 50, "giro acido");
        write_wav("acido.wav", big, pos);
        gb_destroy(g);
    }

    /* ---------------------------------------------------------- 8. Piega */
    printf("8. Piega\n");
    {
        Gb *g = fresh();
        int notes[5] = { 60, 64, 67, 71, 74 };
        for (int i = 0; i < 4; i++) gb_note_on(g, GB_FOLD, notes[i], 0.8f);
        render(g, buf, SR / 20);
        int v4 = gb_active_voices(g, GB_FOLD);
        gb_note_on(g, GB_FOLD, notes[4], 0.8f);
        render(g, buf, SR / 20);
        int v5 = gb_active_voices(g, GB_FOLD);
        for (int i = 0; i < 5; i++) gb_note_off(g, GB_FOLD, notes[i]);
        render(g, buf, SR * 4);
        int v0 = gb_active_voices(g, GB_FOLD);
        printf("  voci: 4 note -> %d, la 5a ruba -> %d, dopo il rilascio %d\n", v4, v5, v0);
        CHECK(v4 == 4 && v5 == 4 && v0 == 0, "Piega: 4 voci");
        gb_destroy(g);

        g = gb_create(SR);
        gb_load_kit(g, gb_groove_kit(26));
        GbPattern *p = gb_pattern(g, 0);
        gb_groove_load(26, p);
        static const int fn[16] = { 0, 7, 12, 3, 0, 10, 7, 15, 0, 7, 12, 19, 5, 10, 3, 7 };
        GbTrackPattern *f = &p->track[GB_FOLD];
        for (int i = 0; i < 16; i++) {
            f->step[i].on = (i % 3) != 2;
            f->step[i].note = (int8_t)fn[i];
            f->step[i].len = (uint8_t)(1 + (i % 4));
            f->step[i].accent = i % 4 == 0;
            if (i % 5 == 0) f->step[i].lock_tone = 90;
        }
        gb_master(g)->bpm = 96;
        gb_mix(g, GB_FOLD)->reverb_send = 0.3f;
        gb_mix(g, GB_FOLD)->delay_send = 0.2f;
        gb_play(g, 1);
        int giro = giro_frames(96, 16), pos = 0;
        for (int k = 0; k < 4; k++) {
            gb_fold(g)->fold = 0.15f + 0.25f * k;
            gb_fold(g)->mod_ratio = k < 2 ? 0.3f : 0.85f;
            gb_fold(g)->mod_amount = 0.15f * k;
            gb_fold(g)->symmetry = k == 3 ? 0.75f : 0.5f;
            render(g, big + 2 * pos, giro);
            pos += giro;
        }
        Stats s = stats(big, pos);
        printf("  giro Piega (4 giri, pieghe crescenti): picco %.3f, RMS %.1f dB\n", s.peak, db(s.rms));
        CHECK(s.bad == 0 && s.peak <= 1.0f && s.rms > 0.01f && s.over < pos / 50, "giro Piega");
        write_wav("piega.wav", big, pos);
        gb_destroy(g);
    }

    /* ---------------------------------------------------------- 9. stutter, registrazione, clock */
    printf("9. Stutter, registrazione, clock\n");
    {
        Gb *g = fresh();
        gb_load_kit(g, 1);
        gb_groove_load(0, gb_pattern(g, 0));
        gb_master(g)->bpm = 120;
        gb_play(g, 1);
        render(g, buf, SR);
        gb_set_repeat(g, 1.0f);
        render(g, buf, SR / 2);
        int L = S16, a0 = SR / 10;
        float diff = 0, ref = rms_win(buf, a0, a0 + L);
        for (int i = a0; i < a0 + L; i++) { float d = fabsf(buf[2 * i] - buf[2 * (i + L)]); if (d > diff) diff = d; }
        gb_set_repeat(g, 0);
        render(g, buf, SR / 2);
        float diff2 = 0;
        for (int i = a0; i < a0 + L; i++) { float d = fabsf(buf[2 * i] - buf[2 * (i + L)]); if (d > diff2) diff2 = d; }
        printf("  stutter 1/16: differenza fra ripetizioni %.6f (RMS %.3f); dopo averlo tolto %.3f\n", diff, ref, diff2);
        CHECK(ref > 0.01f && diff < 1e-4f && diff2 > 0.01f, "stutter");
        gb_destroy(g);
    }
    {
        Gb *g = fresh();
        gb_load_kit(g, 0);
        gb_groove_load(4, gb_pattern(g, 0));
        gb_play(g, 1);
        render(g, buf, SR / 3);
        gb_rec_start(g);
        render(g, buf, SR);
        int16_t *rec = malloc(sizeof(int16_t) * 2 * SR * 2);
        int n = gb_rec_read(g, rec, SR * 2);
        int maxerr = 0;
        for (int i = 0; i < n * 2 && i < SR * 2; i++) {
            int want = (int)(buf[i] * 32767.0f);
            int e = abs(want - rec[i]);
            if (e > maxerr) maxerr = e;
        }
        gb_rec_stop(g);
        render(g, buf, SR / 4);
        int n2 = gb_rec_read(g, rec, SR * 2);
        printf("  registrazione: %d frame letti (attesi %d), scarto massimo %d, dopo lo stop %d, persi %u\n", n, SR, maxerr, n2, gb_rec_dropped(g));
        CHECK(n == SR && maxerr <= 1 && n2 == 0 && gb_rec_dropped(g) == 0 && !gb_rec_active(g), "registrazione");
        free(rec);
        gb_destroy(g);
    }
    {
        Gb *g = fresh();
        int want = SR * 60 / 120 / 24;
        gb_set_clock_hook(g, clock_hook, &want);
        gb_play(g, 1);
        int frames = SR, done = 0;           /* 2 battiti a 120 bpm */
        while (done < frames) {
            int n = 300 < frames - done ? 300 : frames - done;
            clock_block_start = done;
            gb_render_offline(g, buf, n);
            done += n;
        }
        printf("  clock: %d tick in 2 battiti (attesi 48), intervalli sbagliati %d\n", clock_ticks, clock_bad_gap);
        CHECK(clock_ticks == 48 && clock_bad_gap == 0, "clock 24 ppq");
        gb_destroy(g);
    }

    /* ---------------------------------------------------------- 10. master al massimo, prestazioni */
    printf("10. Effetti al massimo e prestazioni\n");
    {
        Gb *g = gb_create(SR);
        static const int heavy[GB_DRUMS] = { DM_KICK_HARD, DM_SNARE_ACOUSTIC, DM_RIDE, DM_CYMBAL, DM_HAT_CLOSED, DM_HAT_OPEN,
                                             DM_TABLA, DM_TAMBOURINE, DM_CYMBAL, DM_TRIANGLE, DM_HAT_909, DM_RIDE };
        for (int t = 0; t < GB_DRUMS; t++) *gb_drum(g, t) = (GbDrum){ heavy[t], 0, 1, .5f, .5f, .5f, .8f, 0, 0 };
        GbPattern *p = gb_pattern(g, 0);
        clear_pattern(p);
        for (int t = 0; t < GB_TRACKS; t++) {
            for (int i = 0; i < 16; i++) {
                GbStep *s = &p->track[t].step[i];
                s->on = 1;
                s->ratchet = t < GB_DRUMS ? 4 : 1;
                s->note = (int8_t)((i * 5) % 24);
                s->slide = (uint8_t)(i & 1);
                s->accent = (uint8_t)(i % 3 == 0);
                s->len = 4;
            }
            gb_mix(g, t)->delay_send = 1;
            gb_mix(g, t)->reverb_send = 1;
            gb_mix(g, t)->level = 1;
        }
        GbMaster *m = gb_master(g);
        m->bpm = 140; m->swing = .3f; m->delay_feedback = .9f; m->delay_time = 3; m->reverb_size = 1; m->reverb_damp = 0;
        m->comp = 1; m->pump = 1; m->filter = .4f; m->filter_res = 1; m->lofi = 1; m->tape = 1; m->volume = 1;
        gb_fold(g)->fold = 1; gb_fold(g)->mod_amount = 1; gb_fold(g)->decay = 1; gb_fold(g)->release = 1;
        gb_acid(g)->resonance = 1; gb_acid(g)->drive = 1;
        gb_play(g, 1);
        render(g, buf, SR * 2);
        Stats s = stats(buf, SR * 2);
        printf("  tutto al massimo: picco %.3f, non finiti %d\n", s.peak, s.bad);
        CHECK(s.bad == 0 && s.peak <= 1.0f, "effetti al massimo");
        int busy = 0;
        for (int t = 0; t < GB_TRACKS; t++) busy += gb_active_voices(g, t);
        /* tre misure da 10 s: conta la migliore (il PC di prova ha altri processi che disturbano) */
        float blk[2 * 256];
        double el = 1e9, worst = 0;
        for (int k = 0; k < 3; k++) {
            double t0 = now_s();
            for (int i = 0; i < SR * 10 / 256; i++) gb_render(g, blk, 256);
            double e = now_s() - t0;
            if (e < el) el = e;
            if (e > worst) worst = e;
        }
        printf("  10 s di audio (14 tracce piene, ratchet 4, voci attive %d, effetti al massimo): %.1f ms (peggiore %.1f), cioe' %.2f ms per secondo (%.0fx il tempo reale)\n",
               busy, el * 1000.0, worst * 1000.0, el * 100.0, 10.0 / el);
        printf("  carico misurato dal motore: %.2f%%\n", gb_cpu_load(g) * 100.0f);
        CHECK(el * 100.0 < 20.0, "prestazioni: %.2f ms per secondo di audio (obiettivo < 20)", el * 100.0);
        CHECK(gb_cpu_load(g) > 0.0f && gb_cpu_load(g) < 0.5f, "carico CPU %.4f", gb_cpu_load(g));
        m->lofi = 0; m->tape = 0; m->filter = 0; m->comp = .3f;
        double t0 = now_s();
        for (int i = 0; i < SR * 10 / 256; i++) gb_render(g, blk, 256);
        el = now_s() - t0;
        printf("  stesso carico senza lo-fi, nastro e filtro: %.2f ms per secondo\n", el * 100.0);
        gb_destroy(g);
    }
    {   /* filtro DJ da un capo all'altro, senza NaN ne' clic enormi */
        Gb *g = gb_create(SR);
        gb_groove_load(1, gb_pattern(g, 0));
        gb_load_kit(g, gb_groove_kit(1));
        gb_master(g)->filter_res = 0.9f;
        gb_play(g, 1);
        int pos = 0;
        for (int k = 0; k <= 40; k++) {
            gb_master(g)->filter = -1.0f + k * 0.05f;
            render(g, big + 2 * pos, SR / 10);
            pos += SR / 10;
        }
        Stats s = stats(big, pos);
        printf("  filtro DJ da -1 a +1 con risonanza: picco %.3f\n", s.peak);
        CHECK(s.bad == 0 && s.peak <= 1.0f, "filtro DJ");
        write_wav("filtro-dj.wav", big, pos);
        gb_destroy(g);
    }

    printf("\n%d prove, %d errori\n", checks, fails);
    if (!fails) printf("ESITO: tutto bene\n");
    free(buf);
    free(big);
    return fails ? 1 : 0;
}
