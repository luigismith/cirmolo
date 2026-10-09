/* OpenOrc - prove senza console: teoria degli accordi, sessione simulata con la MPK mini IV, pulsanti
 * della Flip, motore (strum, arpeggio, pattern, intonazione, basso), looper, salvataggio, prestazioni,
 * audio dimostrativo (WAV) e schermate (BMP).
 *
 * Uso: orc-test <cartella dei font> <cartella di uscita> [preset della MPK mini IV]
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "chords.h"
#include "gfx.h"
#include "orc_app.h"
#include "orc_dsp.h"
#include "platform.h"

#define SR 48000
#define BLOCK 1024

static int checks, fails;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("ERRORE riga %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void write_bmp(const char *path, const Canvas *c)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
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

static void write_wav(const char *path, const float *lr, int frames)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    int data = frames * 4, riff = 36 + data, fmt = 16, sr = SR, br = SR * 4;
    short pcm = 1, ch = 2, bits = 16, ba = 4;
    unsigned char h[44] = { 'R', 'I', 'F', 'F' };
    memcpy(h + 4, &riff, 4); memcpy(h + 8, "WAVEfmt ", 8);
    memcpy(h + 16, &fmt, 4); memcpy(h + 20, &pcm, 2); memcpy(h + 22, &ch, 2); memcpy(h + 24, &sr, 4);
    memcpy(h + 28, &br, 4); memcpy(h + 32, &ba, 2); memcpy(h + 34, &bits, 2); memcpy(h + 36, "data", 4); memcpy(h + 40, &data, 4);
    fwrite(h, 1, 44, f);
    for (int i = 0; i < frames * 2; i++) {
        float v = lr[i] < -1 ? -1 : (lr[i] > 1 ? 1 : lr[i]);
        short s = (short)lrintf(v * 32767);
        fwrite(&s, 2, 1, f);
    }
    fclose(f);
}

/* ------------------------------------------------------------------ aiuti */
static float *g_rec;
static int g_rec_pos, g_rec_cap;

/* audio e interfaccia insieme, a blocchi come sulla console */
static void run(OrcApp *a, float seconds)
{
    static float buf[2 * BLOCK];
    int frames = (int)(seconds * SR);
    for (int done = 0; done < frames; done += BLOCK) {
        int n = frames - done < BLOCK ? frames - done : BLOCK;
        orcapp_audio(a, buf, n);
        if (g_rec && g_rec_pos + n <= g_rec_cap) { memcpy(g_rec + 2 * g_rec_pos, buf, sizeof(float) * 2 * (size_t)n); g_rec_pos += n; }
        orcapp_update(a, (float)n / SR);
    }
}

static void midi3(OrcApp *a, int port, int st, int d1, int d2)
{
    unsigned char m[3] = { (unsigned char)st, (unsigned char)d1, (unsigned char)d2 };
    orcapp_midi(a, port, m, 3);
}
static void pad(OrcApp *a, int note, int on) { midi3(a, 0, on ? 0x99 : 0x89, note, on ? 100 : 0); }
static void key(OrcApp *a, int note, int on) { midi3(a, 0, on ? 0x90 : 0x80, note, on ? 100 : 0); }
static void cc(OrcApp *a, int num, int val) { midi3(a, 0, 0xB0, num, val); }
static void btn(OrcApp *a, int b, int on) { orcapp_button(a, b, on); run(a, 0.03f); }

static int pcs_equal(const int *notes, int n, const int *want, int wn)
{
    int seen[12] = { 0 }, need[12] = { 0 };
    for (int i = 0; i < n; i++) seen[notes[i] % 12] = 1;
    for (int i = 0; i < wn; i++) need[want[i] % 12] = 1;
    return !memcmp(seen, need, sizeof(seen));
}

static float mean_note(const int *n, int c)
{
    float s = 0.0f;
    for (int i = 0; i < c; i++) s += (float)n[i];
    return c ? s / (float)c : 0.0f;
}

static double goertzel(const float *lr, int frames, float freq)
{
    double w = 2.0 * M_PI * freq / SR, coeff = 2.0 * cos(w), s1 = 0, s2 = 0;
    for (int i = 0; i < frames; i++) {
        double win = 0.5 - 0.5 * cos(2.0 * M_PI * i / (frames - 1));
        double s0 = 0.5 * (lr[2 * i] + lr[2 * i + 1]) * win + coeff * s1 - s2;
        s2 = s1; s1 = s0;
    }
    return sqrt(s1 * s1 + s2 * s2 - coeff * s1 * s2);
}

static float midi_hz(int n) { return 440.0f * powf(2.0f, (n - 69) / 12.0f); }

static OrcSound plain_sound(int engine, int perform, float amount)
{
    OrcSound s = { engine, 0.5f, 0.3f, 0.002f, 1.0f, 0.8f, 0.1f, 0.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.0f, perform, amount, 0.8f };
    return s;
}

static void render_until(Orc *o, double beat)
{
    float buf[2 * 64];
    for (int guard = 0; orc_beats(o) < beat && guard < 200000; guard++) orc_render(o, buf, 64);
}

static void render_seconds(Orc *o, float seconds, float *dst)
{
    float buf[2 * 64];
    int frames = (int)(seconds * SR);
    for (int done = 0; done < frames; done += 64) {
        int n = frames - done < 64 ? frames - done : 64;
        orc_render(o, dst ? dst + 2 * done : buf, n);
    }
}

/* ------------------------------------------------------------------ teoria */
static void test_theory(void)
{
    struct { int root, q, ext, sp; const char *it, *en; } cases[] = {
        { 0, Q_MAJ, 0, SPELL_MIX, "Do", "C" },
        { 9, Q_MIN, 0, SPELL_MIX, "Lam", "Am" },
        { 0, Q_MAJ, EXT_MAJ7, SPELL_MIX, "Domaj7", "Cmaj7" },
        { 7, Q_MAJ, EXT_7, SPELL_MIX, "Sol7", "G7" },
        { 11, Q_DIM, EXT_7, SPELL_MIX, "Sim7b5", "Bm7b5" },
        { 2, Q_MIN, EXT_7 | EXT_9, SPELL_MIX, "Rem9", "Dm9" },
        { 0, Q_MAJ, EXT_6 | EXT_9, SPELL_MIX, "Do6/9", "C6/9" },
        { 0, Q_DIM, EXT_6, SPELL_MIX, "Dodim7", "Cdim7" },
        { 5, Q_SUS4, EXT_7, SPELL_MIX, "Fa7sus4", "F7sus4" },
        { 0, Q_AUG, 0, SPELL_MIX, "Doaug", "Caug" },
        { 2, Q_SUS2, 0, SPELL_MIX, "Resus2", "Dsus2" },
        { 10, Q_MAJ, 0, SPELL_MIX, "Sib", "Bb" },
        { 1, Q_MIN, 0, SPELL_MIX, "Do#m", "C#m" },
        { 3, Q_MAJ, 0, SPELL_FLAT, "Mib", "Eb" },
        { 6, Q_MAJ, 0, SPELL_SHARP, "Fa#", "F#" },
        { 0, Q_MAJ, EXT_9, SPELL_MIX, "Doadd9", "Cadd9" },
        { 9, Q_MIN, EXT_MAJ7, SPELL_MIX, "Lam(maj7)", "Am(maj7)" },
        { 7, Q_MAJ, EXT_7 | EXT_9 | EXT_6, SPELL_MIX, "Sol13", "G13" },
        { 0, Q_MAJ, EXT_6, SPELL_MIX, "Do6", "C6" },
        { 9, Q_MIN, EXT_6, SPELL_MIX, "Lam6", "Am6" },
    };
    char it[32], en[32];
    int iv[CHORD_MAX_IV], root;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        int n = chord_intervals(cases[i].q, cases[i].ext, iv);
        unsigned m = chord_mask(iv, n);
        chord_name(it, sizeof(it), cases[i].root, m, cases[i].sp);
        chord_symbol(en, sizeof(en), cases[i].root, m, cases[i].sp);
        CHECK(!strcmp(it, cases[i].it), "nome %s, atteso %s", it, cases[i].it);
        CHECK(!strcmp(en, cases[i].en), "simbolo %s, atteso %s", en, cases[i].en);
        for (int k = 1; k < n; k++) CHECK(iv[k] > iv[k - 1], "%s: intervalli non crescenti", it);
    }

    struct { int key, minor, ext; const char *want[7]; } keys[] = {
        { 0, 0, 0, { "Do", "Rem", "Mim", "Fa", "Sol", "Lam", "Sidim" } },
        { 0, 0, EXT_7, { "Domaj7", "Rem7", "Mim7", "Famaj7", "Sol7", "Lam7", "Sim7b5" } },
        { 9, 1, 0, { "Lam", "Sidim", "Do", "Rem", "Mim", "Fa", "Sol" } },
        { 9, 1, EXT_7, { "Lam7", "Sim7b5", "Domaj7", "Rem7", "Mim7", "Famaj7", "Sol7" } },
        { 5, 0, 0, { "Fa", "Solm", "Lam", "Sib", "Do", "Rem", "Midim" } },
        { 7, 0, EXT_7 | EXT_9, { "Solmaj9", "Lam9", "Sim7(b9)", "Domaj9", "Re9", "Mim9", "Fa#m7b5(b9)" } },
        { 2, 1, 0, { "Rem", "Midim", "Fa", "Solm", "Lam", "Sib", "Do" } },
        { 6, 0, 0, { "Fa#", "Sol#m", "La#m", "Si", "Do#", "Re#m", "Mi#dim" } },
    };
    for (size_t k = 0; k < sizeof(keys) / sizeof(keys[0]); k++)
        for (int d = 0; d < 7; d++) {
            int n = chord_diatonic(keys[k].key, keys[k].minor, d, keys[k].ext, &root, iv);
            chord_name(it, sizeof(it), root, chord_mask(iv, n), key_spelling(keys[k].key, keys[k].minor));
            if (k == 7 && d == 6) continue;       /* Mi# non c'e' nella tabella: va bene Fa (dim) */
            CHECK(!strcmp(it, keys[k].want[d]), "tonalita' %d%s grado %d: %s, atteso %s", keys[k].key, keys[k].minor ? "m" : "", d + 1, it, keys[k].want[d]);
        }

    struct { int degree, ext; const char *want; } numerals[] = {
        { 0, EXT_7, "Imaj7" }, { 1, EXT_7, "ii7" }, { 4, EXT_7, "V7" }, { 6, 0, "vii\xc2\xb0" }, { 6, EXT_7, "vii\xc3\xb8" "7" }, { 5, 0, "vi" },
    };
    for (size_t i = 0; i < sizeof(numerals) / sizeof(numerals[0]); i++) {
        int n = chord_diatonic(0, 0, numerals[i].degree, numerals[i].ext, &root, iv);
        chord_numeral(it, sizeof(it), numerals[i].degree, chord_mask(iv, n));
        CHECK(!strcmp(it, numerals[i].want), "numero romano %s, atteso %s", it, numerals[i].want);
    }

    CHECK(scale_degree(0, 0, 1) == 0, "Do# in Do maggiore -> I");
    CHECK(scale_degree(0, 0, 6) == 3, "Fa# in Do maggiore -> IV");
    CHECK(scale_degree(0, 0, 10) == 5, "Sib in Do maggiore -> vi");
    CHECK(scale_degree(9, 1, 8) == 6, "Sol# in La minore -> VII");
    CHECK(scale_pc(10, 0, 3) == 3, "IV di Sib e' Mib");

    /* modificatori */
    int n = chord_intervals(Q_MAJ, 0, iv);
    n = chord_swap_third(iv, n);
    chord_name(it, sizeof(it), 0, chord_mask(iv, n), SPELL_MIX);
    CHECK(!strcmp(it, "Dom"), "Do scambiato: %s", it);
    n = chord_diatonic(0, 0, 6, EXT_7, &root, iv);
    n = chord_swap_third(iv, n);
    chord_name(it, sizeof(it), root, chord_mask(iv, n), SPELL_MIX);
    CHECK(!strcmp(it, "Si7"), "Sim7b5 scambiato: %s", it);
    n = chord_intervals(Q_MIN, EXT_7, iv);
    n = chord_sus4(iv, n);
    chord_name(it, sizeof(it), 2, chord_mask(iv, n), SPELL_MIX);
    CHECK(!strcmp(it, "Re7sus4"), "Rem7 sus4: %s", it);

    /* voicing: registro, ordine, legato tra gli accordi */
    int notes[ORC_MAX_CHORD];
    const int prog[5][2] = { { 0, Q_MAJ }, { 5, Q_MAJ }, { 7, Q_MAJ }, { 9, Q_MIN }, { 0, Q_MAJ } };
    for (int sp = 0; sp < SPREAD_COUNT; sp++)
        for (float pos = 0.0f; pos <= 1.001f; pos += 0.25f) {
            int prev[ORC_MAX_CHORD], pn = 0;
            for (int c = 0; c < 5; c++) {
                n = chord_intervals(prog[c][1], c == 2 ? EXT_7 : 0, iv);
                int nn = chord_voicing(prog[c][0], iv, n, pos, sp, pn ? prev : NULL, pn, notes, ORC_MAX_CHORD);
                CHECK(nn >= n, "voicing con %d note su %d", nn, n);
                for (int k = 1; k < nn; k++) CHECK(notes[k] > notes[k - 1], "voicing non crescente");
                CHECK(notes[0] >= 28 && notes[nn - 1] <= 100, "voicing fuori registro %d..%d", notes[0], notes[nn - 1]);
                float target = 50.0f + pos * 26.0f, m = mean_note(notes, nn);
                CHECK(fabsf(m - target) <= 6.5f, "media %.1f lontana dal registro %.1f", m, target);
                if (sp == SPREAD_CLOSE && pn && nn == 3 && pn == 3) {
                    int move = 0;
                    for (int k = 0; k < 3; k++) move += abs(notes[k] - prev[k]);
                    CHECK(move <= 7, "voci troppo mobili (%d semitoni) all'accordo %d, registro %.2f", move, c, pos);
                }
                memcpy(prev, notes, sizeof(int) * (size_t)nn);
                pn = nn;
            }
        }
    n = chord_intervals(Q_MAJ, EXT_MAJ7, iv);
    int c1 = chord_voicing(0, iv, n, 0.5f, SPREAD_CLOSE, NULL, 0, notes, ORC_MAX_CHORD), w1 = notes[c1 - 1] - notes[0];
    int c2 = chord_voicing(0, iv, n, 0.5f, SPREAD_WIDE, NULL, 0, notes, ORC_MAX_CHORD), w2 = notes[c2 - 1] - notes[0];
    CHECK(w2 > w1 + 6, "voicing ampio (%d) non piu' largo dello stretto (%d)", w2, w1);
    CHECK(chord_bass(0) == 36 && chord_bass(11) == 47, "basso tra Do2 e Si2");
}

/* ------------------------------------------------------------------ sessione con la MPK */
static void expect_chord(OrcApp *a, const char *want, int line)
{
    run(a, 0.03f);
    checks++;
    if (!orcapp_chord_live(a) || strcmp(orcapp_chord_name(a), want)) {
        fails++;
        printf("ERRORE riga %d: accordo %s%s, atteso %s\n", line, orcapp_chord_name(a), orcapp_chord_live(a) ? "" : " (spento)", want);
    }
}
#define EXPECT(a, name) expect_chord(a, name, __LINE__)

static void test_session(const char *outdir)
{
    char state[512];
    snprintf(state, sizeof(state), "%s/stato-prova.txt", outdir);
    remove(state);
    OrcApp *a = orcapp_create(SR, state);
    Orc *o = orcapp_engine(a);
    int notes[ORC_MAX_CHORD], bass, ln[ORC_MAX_CHORD], tag;

    pad(a, 42, 1); key(a, 60, 1);                 /* Maj + Do */
    EXPECT(a, "Do");
    int n = orcapp_chord_notes(a, notes, &bass);
    const int cmaj[3] = { 0, 4, 7 };
    CHECK(pcs_equal(notes, n, cmaj, 3), "note di Do");
    CHECK(bass == 36, "basso %d", bass);
    CHECK(orc_layer_notes(o, ORC_LIVE, ln, &tag) == n, "il motore tiene l'accordo");
    pad(a, 37, 1); EXPECT(a, "Do7");              /* + m7 */
    pad(a, 38, 1); EXPECT(a, "Domaj7");           /* M7 vince */
    pad(a, 37, 0); pad(a, 38, 0); pad(a, 39, 1); EXPECT(a, "Doadd9");
    pad(a, 39, 0); EXPECT(a, "Do");
    key(a, 60, 0); pad(a, 42, 0); run(a, 0.03f);
    CHECK(!orcapp_chord_live(a), "accordo spento al rilascio");
    CHECK(orc_layer_notes(o, ORC_LIVE, ln, &tag) == 0, "motore senza accordo");

    pad(a, 41, 1); key(a, 57, 1); EXPECT(a, "Lam");
    key(a, 62, 1); EXPECT(a, "Rem");             /* l'ultimo tasto vince */
    key(a, 62, 0); EXPECT(a, "Lam");             /* e al rilascio si torna al primo */
    key(a, 57, 0); pad(a, 41, 0);
    pad(a, 40, 1); pad(a, 42, 1); key(a, 60, 1); EXPECT(a, "Doaug");
    key(a, 60, 0); pad(a, 40, 0); pad(a, 42, 0);
    pad(a, 41, 1); pad(a, 43, 1); key(a, 62, 1); EXPECT(a, "Resus2");
    key(a, 62, 0); pad(a, 41, 0); pad(a, 43, 0);
    pad(a, 40, 1); key(a, 71, 1); EXPECT(a, "Sidim");
    pad(a, 36, 1); EXPECT(a, "Sidim7");
    key(a, 71, 0); pad(a, 40, 0); pad(a, 36, 0);
    pad(a, 37, 1); key(a, 67, 1); EXPECT(a, "Sol7");   /* solo un'estensione: maggiore con la settima */
    key(a, 67, 0); pad(a, 37, 0);
    run(a, 0.05f);

    /* senza pad: melodia */
    int starts = orc_voice_starts(o);
    key(a, 64, 1); run(a, 0.05f);
    CHECK(!orcapp_chord_live(a), "tasto senza pad: niente accordo");
    CHECK(orc_voice_starts(o) == starts + 1, "tasto senza pad: una voce (%d)", orc_voice_starts(o) - starts);
    key(a, 64, 0);

    /* modo tonalita' (pad 48): accordi di Do maggiore */
    pad(a, 48, 1); pad(a, 48, 0);
    CHECK(orcapp_keymode(a), "modo tonalita' acceso dal pad 48");
    key(a, 62, 1); EXPECT(a, "Rem"); key(a, 62, 0);
    key(a, 71, 1); EXPECT(a, "Sidim"); key(a, 71, 0);
    key(a, 66, 1); EXPECT(a, "Fa"); key(a, 66, 0);                 /* fuori scala: il grado sotto */
    pad(a, 37, 1); key(a, 67, 1); EXPECT(a, "Sol7"); key(a, 67, 0); pad(a, 37, 0);
    pad(a, 38, 1); key(a, 64, 1); EXPECT(a, "Mim7"); key(a, 64, 0); pad(a, 38, 0);   /* settima della scala */
    pad(a, 42, 1); key(a, 62, 1); EXPECT(a, "Re"); key(a, 62, 0); pad(a, 42, 0);       /* qualita' imposta */
    pad(a, 48, 1); pad(a, 48, 0);
    CHECK(!orcapp_keymode(a), "modo tonalita' spento");

    /* K1 (CC 24): registro del voicing */
    pad(a, 42, 1); key(a, 60, 1);
    cc(a, 24, 0); run(a, 0.03f);
    n = orcapp_chord_notes(a, notes, NULL);
    float low = mean_note(notes, n);
    cc(a, 24, 127); run(a, 0.03f);
    n = orcapp_chord_notes(a, notes, NULL);
    float high = mean_note(notes, n);
    CHECK(high - low > 18.0f, "K1 sposta il registro: %.1f -> %.1f", low, high);
    cc(a, 24, 64);
    key(a, 60, 0); pad(a, 42, 0);

    /* K4 (CC 27): tono; rotelle */
    cc(a, 27, 0); run(a, 0.02f);
    CHECK(orcapp_sound(a)->tone < 0.01f, "CC 27 = 0 porta il tono a 0");
    cc(a, 27, 127); run(a, 0.02f);
    CHECK(orcapp_sound(a)->tone > 0.99f, "CC 27 = 127 porta il tono a 1");
    midi3(a, 0, 0xE0, 0, 127); midi3(a, 0, 0xB0, 1, 100); run(a, 0.05f);
    midi3(a, 0, 0xE0, 0, 64); midi3(a, 0, 0xB0, 1, 0);

    /* pedale: l'accordo resta dopo il rilascio */
    cc(a, 64, 127); pad(a, 41, 1); key(a, 69, 1); EXPECT(a, "Lam");
    key(a, 69, 0); pad(a, 41, 0); run(a, 0.05f);
    CHECK(orcapp_chord_live(a), "il pedale tiene l'accordo");
    cc(a, 64, 0); run(a, 0.03f);
    CHECK(!orcapp_chord_live(a), "pedale alzato: accordo spento");

    /* Flip: accordi della tonalita' (Do maggiore) */
    orcapp_set_page(a, PAGE_PLAY);
    btn(a, PAD_LEFT, 1); EXPECT(a, "Do");
    btn(a, PAD_L2, 1); EXPECT(a, "Domaj7");
    btn(a, PAD_R2, 1); EXPECT(a, "Domaj9");
    btn(a, PAD_L2, 0); btn(a, PAD_R2, 0); EXPECT(a, "Do");
    btn(a, PAD_L1, 1); EXPECT(a, "Dom"); btn(a, PAD_L1, 0);
    btn(a, PAD_R1, 1); EXPECT(a, "Dosus4"); btn(a, PAD_R1, 0);
    btn(a, PAD_LEFT, 0);
    CHECK(!orcapp_chord_live(a), "croce rilasciata: accordo spento");
    struct { int b; const char *want; } flip[] = {
        { PAD_UP, "Fa" }, { PAD_RIGHT, "Sol" }, { PAD_DOWN, "Lam" }, { PAD_Y, "Rem" }, { PAD_X, "Mim" }, { PAD_B, "Sidim" }, { PAD_A, "Sib" },
    };
    for (size_t i = 0; i < sizeof(flip) / sizeof(flip[0]); i++) {
        btn(a, flip[i].b, 1);
        EXPECT(a, flip[i].want);
        btn(a, flip[i].b, 0);
    }
    btn(a, PAD_RIGHT, 1); btn(a, PAD_L2, 1); EXPECT(a, "Sol7"); btn(a, PAD_L2, 0); btn(a, PAD_RIGHT, 0);

    /* program change: suono 4 = Cascata (arpeggio) */
    unsigned char pc[2] = { 0xC0, 4 };
    orcapp_midi(a, 0, pc, 2);
    CHECK(orcapp_sound(a)->perform == PERF_ARP, "program change 4 -> Cascata");

    /* apprendimento: CC 21 diventa K4 (riga 22 della pagina MIDI) */
    orcapp_set_page(a, PAGE_MIDI);
    orcapp_select(a, 22);
    btn(a, PAD_A, 1); btn(a, PAD_A, 0);
    cc(a, 21, 0); run(a, 0.02f);
    cc(a, 21, 10); run(a, 0.02f);
    CHECK(fabsf(orcapp_sound(a)->tone - 10.0f / 127.0f) < 0.01f, "CC 21 imparato come K4 (tono %.3f)", orcapp_sound(a)->tone);
    cc(a, 27, 127); run(a, 0.02f);
    CHECK(orcapp_sound(a)->tone < 0.1f, "il vecchio CC 27 non comanda piu' il tono");

    /* in fila: i pad delle funzioni sul canale 2, note 60-67 */
    orcapp_select(a, 9);
    btn(a, PAD_A, 1); btn(a, PAD_A, 0);
    for (int i = 0; i < 8; i++) { midi3(a, 0, 0x91, 60 + i, 90); midi3(a, 0, 0x81, 60 + i, 0); }
    CHECK(!orc_transport_on(o), "trasporto fermo prima della prova");
    midi3(a, 0, 0x91, 60, 90); midi3(a, 0, 0x81, 60, 0); run(a, 0.05f);
    CHECK(orc_transport_on(o), "nota 60 sul canale 2 ora accende la batteria");
    midi3(a, 0, 0x91, 60, 90); midi3(a, 0, 0x81, 60, 0); run(a, 0.05f);
    CHECK(!orc_transport_on(o), "e la rispegne");
    pad(a, 44, 1); pad(a, 44, 0); run(a, 0.05f);
    CHECK(!orc_transport_on(o), "la nota 44 non e' piu' la batteria");
    /* tasti di trasporto su un'altra porta, imparati in fila */
    orcapp_select(a, 28);
    btn(a, PAD_A, 1); btn(a, PAD_A, 0);
    for (int i = 0; i < 5; i++) midi3(a, 3, 0x90, 100 + i, 127);
    midi3(a, 3, 0x90, 100, 127); run(a, 0.05f);
    CHECK(orc_transport_on(o), "Play/Stop della porta 4 avvia il trasporto");
    midi3(a, 0, 0x90, 100, 127); run(a, 0.05f);
    CHECK(orc_transport_on(o), "la stessa nota sulla porta 1 e' un tasto");
    midi3(a, 0, 0x80, 100, 0);
    midi3(a, 3, 0x90, 100, 127); run(a, 0.05f);
    CHECK(!orc_transport_on(o), "Play/Stop ferma il trasporto");
    btn(a, PAD_Y, 1); btn(a, PAD_Y, 0);           /* valori della MPK */
    pad(a, 44, 1); pad(a, 44, 0); run(a, 0.05f);
    CHECK(orc_transport_on(o), "dopo Y la nota 44 e' di nuovo la batteria");
    pad(a, 44, 1); pad(a, 44, 0); run(a, 0.05f);

    /* salvataggio e ripresa */
    orcapp_select(a, 22);                         /* di nuovo CC 21 come K4, da salvare */
    btn(a, PAD_A, 1); btn(a, PAD_A, 0);
    cc(a, 21, 64);
    orcapp_set_page(a, PAGE_SOUND);
    orcapp_select(a, 5);                          /* Voicing */
    btn(a, PAD_RIGHT, 1); btn(a, PAD_RIGHT, 0);
    orcapp_destroy(a);
    a = orcapp_create(SR, state);
    CHECK(orcapp_sound(a)->perform == PERF_ARP, "suono ripreso dal file di stato");
    FILE *f = fopen(state, "r");
    char buf[4096] = "";
    size_t got = f ? fread(buf, 1, sizeof(buf) - 1, f) : 0;
    if (f) fclose(f);
    buf[got] = 0;
    CHECK(strstr(buf, "voicing=0.55") != NULL, "voicing salvato");
    CHECK(strstr(buf, "map19=2,0,-1,21") != NULL, "assegnazione imparata salvata");
    orcapp_destroy(a);
}

/* ------------------------------------------------------------------ motore */
static void test_engine(void)
{
    static float audio[2 * SR * 2];
    /* intonazione: accordo di Do con l'organo, senza effetti */
    Orc *o = orc_create(SR);
    OrcSound s = plain_sound(ENG_ORGAN, PERF_CHORD, 0.3f);
    orc_set_sound(o, &s);
    int C[3] = { 60, 64, 67 };
    orc_chord_on(o, C, 3, -1, 0.8f, 0);
    render_seconds(o, 0.2f, NULL);
    render_seconds(o, 1.0f, audio);
    double in = goertzel(audio, SR, midi_hz(60)) + goertzel(audio, SR, midi_hz(64)) + goertzel(audio, SR, midi_hz(67));
    double out = goertzel(audio, SR, midi_hz(61)) + goertzel(audio, SR, midi_hz(63)) + goertzel(audio, SR, midi_hz(66));
    CHECK(in > 20.0 * out, "note dell'accordo %.1f contro vicine %.1f", in, out);
    float peak = 0.0f;
    int finite = 1;
    for (int i = 0; i < 2 * SR; i++) { finite &= isfinite(audio[i]) != 0; peak = fmaxf(peak, fabsf(audio[i])); }
    CHECK(finite, "campioni non finiti");
    CHECK(peak > 0.05f && peak <= 1.0f, "picco %.3f", peak);
    orc_destroy(o);

    /* basso: Do2 sotto l'accordo */
    o = orc_create(SR);
    s = plain_sound(ENG_VA, PERF_CHORD, 0.3f);
    s.bass_level = 0.9f;
    s.bass_tone = 0.2f;
    orc_set_sound(o, &s);
    orc_chord_on(o, C, 3, 36, 0.8f, 0);
    render_seconds(o, 0.2f, NULL);
    render_seconds(o, 1.0f, audio);
    double b_in = goertzel(audio, SR, midi_hz(36)), b_out = goertzel(audio, SR, midi_hz(37));
    CHECK(b_in > 10.0 * b_out, "basso Do2 %.1f contro Do#2 %.1f", b_in, b_out);
    orc_destroy(o);

    /* strum: le voci partono una dopo l'altra (98 ms) */
    o = orc_create(SR);
    s = plain_sound(ENG_VA, PERF_STRUM, 1.0f);
    orc_set_sound(o, &s);
    int C4[4] = { 60, 64, 67, 72 };
    orc_chord_on(o, C4, 4, -1, 0.9f, 0);
    const float at[4] = { 0.05f, 0.15f, 0.25f, 0.35f };
    float t = 0.0f;
    for (int i = 0; i < 4; i++) {
        render_seconds(o, at[i] - t, NULL);
        t = at[i];
        CHECK(orc_active_voices(o) == i + 1, "strum a %.2f s: %d voci, attese %d", at[i], orc_active_voices(o), i + 1);
    }
    orc_destroy(o);

    /* arpeggio a 1/16 e 120 BPM: 8 note al secondo, con e senza trasporto */
    for (int tr = 0; tr < 2; tr++) {
        o = orc_create(SR);
        s = plain_sound(ENG_VA, PERF_ARP, 0.65f);
        orc_set_sound(o, &s);
        orc_set_tempo(o, 120.0f);
        if (tr) orc_transport(o, 1);
        render_seconds(o, 0.01f, NULL);
        int st0 = orc_voice_starts(o);
        orc_chord_on(o, C, 3, -1, 0.8f, 0);
        render_seconds(o, 1.0f, NULL);
        int st = orc_voice_starts(o) - st0;
        CHECK(st >= 8 && st <= 9, "arpeggio %s: %d note in un secondo", tr ? "a tempo" : "libero", st);
        orc_destroy(o);
    }

    /* pattern 1 a 120 BPM: botta iniziale + 4 colpi in una battuta (3 note ciascuno) */
    o = orc_create(SR);
    s = plain_sound(ENG_VA, PERF_PATTERN, 0.0f);
    orc_set_sound(o, &s);
    orc_set_tempo(o, 120.0f);
    orc_transport(o, 1);
    render_seconds(o, 0.005f, NULL);
    int p0 = orc_voice_starts(o);
    orc_chord_on(o, C, 3, -1, 0.8f, 0);
    render_until(o, 3.98);
    CHECK(orc_voice_starts(o) - p0 == 15, "pattern: %d voci in una battuta, attese 15", orc_voice_starts(o) - p0);
    orc_destroy(o);

    /* batteria: passi che girano, suono presente */
    o = orc_create(SR);
    s = plain_sound(ENG_VA, PERF_CHORD, 0.3f);
    orc_set_sound(o, &s);
    orc_set_tempo(o, 120.0f);
    orc_set_beat(o, 1, BEAT_DISCO, 0.8f);
    orc_transport(o, 1);
    render_seconds(o, 0.01f, NULL);
    int steps_seen = 0, last = -2;
    float ms[2 * 64];
    double energy = 0.0;
    for (int i = 0; i < SR * 2 / 64; i++) {
        orc_render(o, ms, 64);
        for (int k = 0; k < 128; k++) energy += ms[k] * ms[k];
        int stp = orc_beat_step(o);
        if (stp != last) { steps_seen++; last = stp; }
    }
    CHECK(steps_seen >= 15, "batteria: %d passi in due secondi", steps_seen);
    CHECK(energy / (SR * 2) > 1e-3, "batteria udibile (%.5f)", energy / (SR * 2));
    orc_destroy(o);
}

/* ------------------------------------------------------------------ looper */
static void test_looper(void)
{
    Orc *o = orc_create(SR);
    OrcSound s = plain_sound(ENG_VA, PERF_CHORD, 0.3f);
    orc_set_sound(o, &s);
    orc_set_tempo(o, 120.0f);
    orc_loop_config(o, 1, 0.5f);
    orc_loop_record(o);                           /* trasporto fermo: parte subito */
    float buf[2 * 64];
    orc_render(o, buf, 64);
    OrcLoopInfo li;
    orc_loop_info(o, &li);
    CHECK(li.state == LOOP_RECORDING, "registra subito (stato %d)", li.state);
    CHECK(orc_transport_on(o), "la registrazione avvia il trasporto");
    int A[3] = { 57, 60, 64 }, B[3] = { 55, 59, 62 }, ln[ORC_MAX_CHORD], tag = 0;
    orc_chord_on(o, A, 3, 45, 0.8f, 111);
    render_until(o, 2.0);
    orc_chord_on(o, B, 3, 43, 0.8f, 222);
    render_until(o, 3.5);
    orc_chord_off(o);
    render_until(o, 4.05);
    orc_loop_info(o, &li);
    CHECK(li.state == LOOP_PLAYING, "dopo una battuta suona (stato %d)", li.state);
    CHECK(li.events == 3 && fabs(li.length - 4.0) < 1e-9, "eventi %d, lunghezza %.2f", li.events, li.length);
    render_until(o, 4.3);
    int n = orc_layer_notes(o, ORC_LOOP, ln, &tag);
    CHECK(n == 3 && tag == 111, "giro 2, battito 0: accordo A (n %d, tag %d)", n, tag);
    render_until(o, 6.3);
    n = orc_layer_notes(o, ORC_LOOP, ln, &tag);
    CHECK(n == 3 && tag == 222, "giro 2, battito 2: accordo B (n %d, tag %d)", n, tag);
    render_until(o, 7.8);
    CHECK(orc_layer_notes(o, ORC_LOOP, ln, &tag) == 0, "giro 2, battito 3,5: silenzio");
    CHECK(orc_layer_notes(o, ORC_LIVE, ln, &tag) == 0, "lo strato dal vivo non suona");

    /* sovraincisione di una nota al battito 0,5 */
    orc_loop_record(o);
    render_until(o, 8.5);
    orc_loop_info(o, &li);
    CHECK(li.state == LOOP_OVERDUB, "sovraincide (stato %d)", li.state);
    orc_note_on(o, 72, 0.9f);
    render_until(o, 9.0);
    orc_note_off(o, 72);
    render_until(o, 9.2);
    orc_loop_record(o);
    render_until(o, 9.3);
    orc_loop_info(o, &li);
    CHECK(li.state == LOOP_PLAYING && li.events == 5, "dopo la sovraincisione: stato %d, eventi %d", li.state, li.events);
    render_until(o, 12.3);
    int st0 = orc_voice_starts(o);
    render_until(o, 12.7);
    CHECK(orc_voice_starts(o) - st0 == 1, "la nota sovraincisa torna al giro dopo (%d)", orc_voice_starts(o) - st0);

    /* fermo e ripartenza; poi cancellazione */
    orc_loop_play(o);
    render_until(o, 13.0);
    orc_loop_info(o, &li);
    CHECK(li.state == LOOP_STOPPED, "fermo (stato %d)", li.state);
    CHECK(orc_layer_notes(o, ORC_LOOP, ln, &tag) == 0, "fermo: lo strato del loop tace");
    orc_loop_clear(o);
    render_until(o, 13.1);
    orc_loop_info(o, &li);
    CHECK(li.state == LOOP_EMPTY && li.events == 0, "cancellato (stato %d, eventi %d)", li.state, li.events);

    /* trasporto in moto: si aspetta la battuta */
    orc_loop_config(o, 4, 0.5f);
    render_until(o, 13.2);
    orc_loop_record(o);
    render_until(o, 15.9);
    orc_loop_info(o, &li);
    CHECK(li.state == LOOP_ARMED && li.position < 0.0, "attende la battuta (stato %d, pos %.2f)", li.state, li.position);
    render_until(o, 16.1);
    orc_loop_info(o, &li);
    CHECK(li.state == LOOP_RECORDING, "parte alla battuta 16 (stato %d)", li.state);
    orc_chord_on(o, A, 3, 45, 0.8f, 7);
    render_until(o, 23.8);                         /* circa due battute */
    orc_loop_record(o);
    render_until(o, 23.9);
    orc_loop_info(o, &li);
    CHECK(li.state == LOOP_PLAYING && fabs(li.length - 8.0) < 1e-9, "chiuso a due battute (stato %d, lunghezza %.1f)", li.state, li.length);
    orc_chord_off(o);
    render_until(o, 24.5);
    n = orc_layer_notes(o, ORC_LOOP, ln, &tag);
    CHECK(n == 3 && tag == 7, "l'accordo tenuto da prima suona nel giro (n %d, tag %d)", n, tag);
    orc_transport(o, 0);
    orc_render(o, buf, 64);
    CHECK(orc_layer_notes(o, ORC_LOOP, ln, &tag) == 0, "trasporto fermo: il loop tace");
    orc_loop_info(o, &li);
    CHECK(li.state == LOOP_PLAYING, "il loop resta pronto (stato %d)", li.state);
    orc_destroy(o);
}

/* ------------------------------------------------------------------ prestazioni */
static void test_performance(void)
{
    Orc *o = orc_create(SR);
    OrcSound s = plain_sound(ENG_VA, PERF_HARP, 0.0f);
    s.chorus = 0.6f; s.delay = 0.4f; s.reverb = 0.6f; s.bass_level = 0.6f;
    orc_set_sound(o, &s);
    orc_set_tempo(o, 120.0f);
    orc_set_beat(o, 1, BEAT_PSYCH, 0.8f);
    orc_transport(o, 1);
    int ch[6] = { 48, 55, 60, 64, 67, 71 };
    float buf[2 * BLOCK];
    clock_t t0 = clock();
    for (int i = 0; i < 10 * SR / BLOCK; i++) {
        if (i % 20 == 0) orc_chord_on(o, ch, 6, 36, 0.9f, 0);
        orc_render(o, buf, BLOCK);
    }
    double secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
    printf("prestazioni: 10 s di audio (16 voci, batteria, effetti) in %.3f s (%.0f volte il tempo reale)\n", secs, 10.0 / (secs > 0 ? secs : 1e-9));
    orc_destroy(o);
}

/* ------------------------------------------------------------------ preset della tastiera */
static size_t slurp(const char *path, char *buf, size_t n)
{
    FILE *f = fopen(path, "r");
    size_t got = f ? fread(buf, 1, n - 1, f) : 0;
    if (f) fclose(f);
    buf[got] = 0;
    return got;
}

static void test_keyboard(const char *outdir, const char *repo)
{
    char state[512], mine[600], def[600], rep[600], bad[600], name[64] = "";
    static char b1[4096], b2[4096];
    snprintf(state, sizeof(state), "%s/stato-tastiera.txt", outdir);
    snprintf(mine, sizeof(mine), "%s/tastiere/mia-tastiera.txt", outdir);
    snprintf(def, sizeof(def), "%s/kbd-partenza.txt", outdir);
    snprintf(rep, sizeof(rep), "%s/kbd-repo.txt", outdir);
    snprintf(bad, sizeof(bad), "%s/kbd-rotto.txt", outdir);
    remove(state);
    remove(mine);
    OrcApp *a = orcapp_create(SR, state);
    Orc *o = orcapp_engine(a);

    /* il file della repo dice esattamente i valori di partenza dell'app */
    CHECK(!orcapp_save_keyboard(a, def, "x"), "mappa di partenza salvata");
    CHECK(!orcapp_load_keyboard(a, repo, name, sizeof(name)), "preset della repo letto (%s)", repo);
    CHECK(!strcmp(name, "Akai MPK mini IV"), "nome del preset: %s", name);
    orcapp_save_keyboard(a, rep, "x");
    slurp(def, b1, sizeof(b1));
    slurp(rep, b2, sizeof(b2));
    CHECK(b1[0] && !strcmp(b1, b2), "il preset della repo coincide con la mappa di partenza");

    /* R2 salva la mappa imparata, Y la ricarica */
    orcapp_set_page(a, PAGE_MIDI);
    orcapp_select(a, 10);                         /* Batteria */
    btn(a, PAD_A, 1); btn(a, PAD_A, 0);
    midi3(a, 0, 0x91, 60, 90); midi3(a, 0, 0x81, 60, 0);
    btn(a, PAD_R2, 1); btn(a, PAD_R2, 0);
    slurp(mine, b1, sizeof(b1));
    CHECK(strstr(b1, "Batteria = nota 60 canale 2 porta 1") != NULL, "R2 salva la batteria imparata");
    btn(a, PAD_X, 1); btn(a, PAD_X, 0);           /* tolta */
    btn(a, PAD_Y, 1); btn(a, PAD_Y, 0);           /* e ricaricata dal file */
    CHECK(!orc_transport_on(o), "trasporto fermo prima della prova");
    midi3(a, 0, 0x91, 60, 90); midi3(a, 0, 0x81, 60, 0); run(a, 0.05f);
    CHECK(orc_transport_on(o), "dopo Y la nota 60 sul canale 2 accende la batteria");
    midi3(a, 0, 0x91, 60, 90); midi3(a, 0, 0x81, 60, 0); run(a, 0.05f);

    /* un file sbagliato non tocca la mappa */
    FILE *f = fopen(bad, "w");
    if (f) { fputs("\xEF\xBB\xBFnome = rotto\nMaj = tasto 42\nK9 = cc 1\n", f); fclose(f); }
    CHECK(orcapp_load_keyboard(a, bad, name, sizeof(name)) != 0, "file senza controlli validi rifiutato");
    midi3(a, 0, 0x91, 60, 90); midi3(a, 0, 0x81, 60, 0); run(a, 0.05f);
    CHECK(orc_transport_on(o), "la mappa resta quella di prima");

    orcapp_destroy(a);
    remove(state); remove(mine); remove(def); remove(rep); remove(bad);

    /* stato delle versioni con le manopole sui CC 70-77: passa ai valori di fabbrica della MPK */
    f = fopen(state, "w");
    if (f) {
        for (int i = 0; i < 8; i++) fprintf(f, "map%d=2,0,-1,%d\n", 16 + i, 70 + i);
        for (int i = 24; i < 29; i++) fprintf(f, "map%d=0,-1,-1,0\n", i);
        fclose(f);
    }
    a = orcapp_create(SR, state);
    cc(a, 27, 0); run(a, 0.02f);
    CHECK(orcapp_sound(a)->tone < 0.01f, "stato vecchio: K4 passa al CC 27");
    cc(a, 73, 127); run(a, 0.02f);
    CHECK(orcapp_sound(a)->tone < 0.01f, "stato vecchio: il CC 73 non comanda piu' il tono");
    orcapp_destroy(a);
    remove(state);
}

/* ------------------------------------------------------------------ ridisegno solo se serve */
static void test_redraw(void)
{
    OrcApp *a = orcapp_create(SR, NULL);
    CHECK(orcapp_needs_draw(a), "il primo fotogramma si disegna");
    CHECK(!orcapp_needs_draw(a), "senza novita' non si ridisegna");
    pad(a, 41, 1);
    CHECK(orcapp_needs_draw(a), "un pad premuto si vede");
    run(a, 0.05f);
    orcapp_needs_draw(a);
    pad(a, 41, 0);
    key(a, 60, 1);
    CHECK(orcapp_needs_draw(a), "una nota di melodia si vede");
    run(a, 0.5f);
    CHECK(orcapp_needs_draw(a) && orcapp_needs_draw(a), "finche' suona l'oscilloscopio si muove");
    key(a, 60, 0);
    run(a, 12.0f);                                /* rilascio e riverbero si spengono */
    orcapp_needs_draw(a);
    CHECK(!orcapp_needs_draw(a), "tutto fermo di nuovo");
    run(a, 1.1f);
    CHECK(orcapp_needs_draw(a), "comunque una volta al secondo");
    orcapp_button(a, PAD_START, 1);
    orcapp_button(a, PAD_START, 0);
    run(a, 0.05f);
    CHECK(orcapp_needs_draw(a) && orcapp_needs_draw(a), "a trasporto in moto si disegna sempre");
    orcapp_destroy(a);
}

/* ------------------------------------------------------------------ demo e schermate */
static void demo(OrcApp *a, const char *outdir)
{
    char path[512];
    g_rec_cap = SR * 26;
    g_rec = calloc((size_t)g_rec_cap * 2, sizeof(float));
    g_rec_pos = 0;
    pad(a, 48, 1); pad(a, 48, 0);                 /* modo tonalita' */
    cc(a, 31, 100);
    pad(a, 44, 1); pad(a, 44, 0);                 /* batteria */
    const int prog[8] = { 60, 67, 69, 65, 60, 67, 69, 65 };   /* I V vi IV */
    for (int i = 0; i < 8; i++) {
        if (i == 4) { pad(a, 51, 1); pad(a, 51, 0); pad(a, 51, 1); pad(a, 51, 0); pad(a, 51, 1); pad(a, 51, 0); pad(a, 51, 1); pad(a, 51, 0); }
        key(a, prog[i], 1);
        run(a, 60.0f / 96.0f * 4.0f - 0.05f);
        key(a, prog[i], 0);
        run(a, 0.05f);
    }
    run(a, 1.5f);
    snprintf(path, sizeof(path), "%s/openorc-demo.wav", outdir);
    write_wav(path, g_rec, g_rec_pos);
    float peak = 0.0f;
    double rms = 0.0;
    for (int i = 0; i < g_rec_pos * 2; i++) { peak = fmaxf(peak, fabsf(g_rec[i])); rms += g_rec[i] * g_rec[i]; }
    rms = sqrt(rms / (g_rec_pos * 2 > 0 ? g_rec_pos * 2 : 1));
    printf("demo: %.1f s, picco %.3f, RMS %.3f -> %s\n", (float)g_rec_pos / SR, peak, rms, path);
    CHECK(peak <= 1.0f && rms > 0.03, "demo: picco %.3f, RMS %.3f", peak, rms);
    free(g_rec);
    g_rec = NULL;
    pad(a, 44, 1); pad(a, 44, 0);
    pad(a, 48, 1); pad(a, 48, 0);
    run(a, 0.2f);
}

static void screenshots(OrcApp *a, const char *outdir)
{
    uint32_t *px = malloc(sizeof(uint32_t) * 640 * 480);
    Canvas c = { px, 640, 480 };
    char path[512];
#define SHOT(name) do { orcapp_draw(a, &c); snprintf(path, sizeof(path), "%s/%s.bmp", outdir, name); write_bmp(path, &c); } while (0)
    orcapp_midi_status(a, "MPK mini IV");
    run(a, 3.0f);                                 /* il messaggio di collegamento sparisce */
    orcapp_set_page(a, PAGE_PLAY);
    SHOT("orc-1-vuota");

    /* loop di quattro accordi in Do maggiore, poi un accordo dal vivo */
    pad(a, 48, 1); pad(a, 48, 0);
    unsigned char rec[3] = { 0x99, 45, 100 };
    orcapp_midi(a, 0, rec, 3);
    rec[0] = 0x89;
    orcapp_midi(a, 0, rec, 3);
    const int prog[4] = { 60, 67, 69, 65 };
    for (int i = 0; i < 4; i++) { key(a, prog[i], 1); run(a, 60.0f / 96.0f * 4.0f * 0.95f); key(a, prog[i], 0); run(a, 60.0f / 96.0f * 4.0f * 0.05f); }
    run(a, 0.3f);
    pad(a, 37, 1);
    key(a, 69, 1);
    cc(a, 27, 90);
    run(a, 0.15f);
    SHOT("orc-2-suona");
    key(a, 69, 0);
    pad(a, 37, 0);
    run(a, 1.0f);

    orcapp_set_page(a, PAGE_SOUND);
    orcapp_select(a, 7);
    SHOT("orc-3-suono");
    orcapp_select(a, 16);
    SHOT("orc-3b-suono-timbro");
    orcapp_set_page(a, PAGE_RHYTHM);
    pad(a, 44, 1); pad(a, 44, 0);
    run(a, 0.7f);
    SHOT("orc-4-ritmo");
    pad(a, 44, 1); pad(a, 44, 0);
    orcapp_set_page(a, PAGE_MIDI);
    orcapp_select(a, 5);
    btn(a, PAD_A, 1); btn(a, PAD_A, 0);
    midi3(a, 0, 0x99, 38, 77);
    btn(a, PAD_B, 1); btn(a, PAD_B, 0);
    orcapp_select(a, 20);
    btn(a, PAD_A, 1); btn(a, PAD_A, 0);
    run(a, 2.5f);
    SHOT("orc-5-midi");
    btn(a, PAD_B, 1); btn(a, PAD_B, 0);
    orcapp_set_page(a, PAGE_PLAY);
    orcapp_button(a, PAD_MENU, 1);
    orcapp_button(a, PAD_MENU, 0);
    SHOT("orc-6-uscita");
    orcapp_button(a, PAD_B, 1);
    orcapp_button(a, PAD_B, 0);
    btn(a, PAD_DOWN, 1);
    btn(a, PAD_L2, 1);
    run(a, 0.1f);
    SHOT("orc-7-flip");
    btn(a, PAD_L2, 0);
    btn(a, PAD_DOWN, 0);
    /* senza la MPK: i tasti della Flip grandi */
    orcapp_midi_status(a, NULL);
    run(a, 2.5f);
    btn(a, PAD_RIGHT, 1);
    btn(a, PAD_R1, 1);
    run(a, 0.1f);
    SHOT("orc-8-solo-flip");
    btn(a, PAD_R1, 0);
    btn(a, PAD_RIGHT, 0);
    free(px);
}

int main(int argc, char **argv)
{
    const char *fonts = argc > 1 ? argv[1] : ".", *outdir = argc > 2 ? argv[2] : ".";
    char f1[512], f2[512];
    snprintf(f1, sizeof(f1), "%s/BeVietnamPro-Regular.ttf", fonts);
    snprintf(f2, sizeof(f2), "%s/BeVietnamPro-SemiBold.ttf", fonts);
    if (gfx_load_fonts(f1, f2)) return 1;

    test_theory();
    printf("teoria: %d controlli, %d errori\n", checks, fails);
    test_session(outdir);
    printf("sessione MPK e Flip: %d controlli, %d errori\n", checks, fails);
    test_engine();
    printf("motore: %d controlli, %d errori\n", checks, fails);
    test_looper();
    printf("looper: %d controlli, %d errori\n", checks, fails);
    test_redraw();
    printf("ridisegno: %d controlli, %d errori\n", checks, fails);
    test_keyboard(outdir, argc > 3 ? argv[3] : "../tastiere/akai-mpk-mini-iv.txt");
    printf("preset della tastiera: %d controlli, %d errori\n", checks, fails);
    test_performance();

    char state[512];
    snprintf(state, sizeof(state), "%s/stato-demo.txt", outdir);
    remove(state);
    OrcApp *a = orcapp_create(SR, state);
    demo(a, outdir);
    screenshots(a, outdir);
    orcapp_destroy(a);

    printf("TOTALE: %d controlli, %d errori\n", checks, fails);
    gfx_free_fonts();
    return fails ? 1 : 0;
}
