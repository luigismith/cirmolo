/* OpenOrc - teoria degli accordi (vedi chords.h). */
#include "chords.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int MAJOR[7] = { 0, 2, 4, 5, 7, 9, 11 };
static const int MINOR[7] = { 0, 2, 3, 5, 7, 8, 10 };

static const char *IT[3][12] = {
    { "Do", "Do#", "Re", "Mib", "Mi", "Fa", "Fa#", "Sol", "Lab", "La", "Sib", "Si" },
    { "Do", "Do#", "Re", "Re#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" },
    { "Do", "Reb", "Re", "Mib", "Mi", "Fa", "Solb", "Sol", "Lab", "La", "Sib", "Si" },
};
static const char *EN[3][12] = {
    { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" },
    { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" },
    { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" },
};

static int wrap12(int x) { return ((x % 12) + 12) % 12; }

static void sort_int(int *v, int n)
{
    for (int i = 1; i < n; i++) {
        int x = v[i], j = i - 1;
        while (j >= 0 && v[j] > x) { v[j + 1] = v[j]; j--; }
        v[j + 1] = x;
    }
}

static int dedupe(int *v, int n)
{
    sort_int(v, n);
    int m = 0;
    for (int i = 0; i < n; i++) if (!m || v[i] != v[m - 1]) v[m++] = v[i];
    return m;
}

const char *note_it(int pc, int spelling) { return IT[spelling < 0 || spelling > 2 ? 0 : spelling][wrap12(pc)]; }
const char *note_en(int pc, int spelling) { return EN[spelling < 0 || spelling > 2 ? 0 : spelling][wrap12(pc)]; }

int key_spelling(int key, int minor)
{
    static const signed char MAJ[12] = { SPELL_MIX, SPELL_FLAT, SPELL_SHARP, SPELL_FLAT, SPELL_SHARP, SPELL_FLAT,
                                         SPELL_SHARP, SPELL_SHARP, SPELL_FLAT, SPELL_SHARP, SPELL_FLAT, SPELL_SHARP };
    static const signed char MIN[12] = { SPELL_FLAT, SPELL_SHARP, SPELL_FLAT, SPELL_FLAT, SPELL_SHARP, SPELL_FLAT,
                                         SPELL_SHARP, SPELL_FLAT, SPELL_SHARP, SPELL_MIX, SPELL_FLAT, SPELL_SHARP };
    return minor ? MIN[wrap12(key)] : MAJ[wrap12(key)];
}

void key_name(char *out, size_t size, int key, int minor)
{
    snprintf(out, size, "%s %s", note_it(key, key_spelling(key, minor)), minor ? "minore" : "maggiore");
}

int chord_intervals(int q, int ext, int *iv)
{
    static const int TRIADS[Q_COUNT][3] = { { 0, 4, 7 }, { 0, 3, 7 }, { 0, 5, 7 }, { 0, 3, 6 }, { 0, 4, 8 }, { 0, 2, 7 } };
    if (q < 0 || q >= Q_COUNT) q = Q_MAJ;
    int n = 0, seventh = (ext & (EXT_7 | EXT_MAJ7)) != 0;
    for (int i = 0; i < 3; i++) iv[n++] = TRIADS[q][i];
    if (ext & EXT_6) iv[n++] = seventh && q != Q_DIM ? 21 : 9;   /* con la settima e' una tredicesima */
    if (ext & EXT_MAJ7) iv[n++] = 11;                            /* M7 vince su m7 */
    else if (ext & EXT_7) iv[n++] = 10;
    if (ext & EXT_9) iv[n++] = 14;
    return dedupe(iv, n);
}

int scale_pc(int key, int minor, int degree)
{
    const int *sc = minor ? MINOR : MAJOR;
    return wrap12(key + sc[((degree % 7) + 7) % 7]);
}

int scale_degree(int key, int minor, int pc)
{
    const int *sc = minor ? MINOR : MAJOR;
    int rel = wrap12(pc - key), d = 0;
    for (int i = 0; i < 7; i++) if (sc[i] <= rel) d = i;
    return d;
}

int chord_diatonic(int key, int minor, int degree, int ext, int *root, int *iv)
{
    const int *sc = minor ? MINOR : MAJOR;
    degree = ((degree % 7) + 7) % 7;
    *root = wrap12(key + sc[degree]);
#define STEP(k) (sc[(degree + (k)) % 7] + 12 * ((degree + (k)) / 7) - sc[degree])
    int n = 0, seventh = (ext & (EXT_7 | EXT_MAJ7)) != 0;
    iv[n++] = 0;
    iv[n++] = STEP(2);
    iv[n++] = STEP(4);
    if (ext & EXT_6) iv[n++] = seventh ? STEP(5) + 12 : STEP(5);
    if (seventh) iv[n++] = STEP(6);
    if (ext & EXT_9) iv[n++] = STEP(8);
#undef STEP
    return dedupe(iv, n);
}

int chord_swap_third(int *iv, int n)
{
    int has3 = 0, has4 = 0;
    for (int i = 0; i < n; i++) { has3 |= iv[i] == 3; has4 |= iv[i] == 4; }
    for (int i = 0; i < n; i++) {
        if (has4) { if (iv[i] == 4) iv[i] = 3; }
        else if (has3) {
            if (iv[i] == 3) iv[i] = 4;
            else if (iv[i] == 6) iv[i] = 7;       /* il diminuito diventa maggiore */
        }
    }
    return dedupe(iv, n);
}

int chord_sus4(int *iv, int n)
{
    for (int i = 0; i < n; i++) {
        if (iv[i] == 3 || iv[i] == 4) iv[i] = 5;
        else if (iv[i] == 6) iv[i] = 7;
    }
    return dedupe(iv, n);
}

unsigned chord_mask(const int *iv, int n)
{
    unsigned m = 0;
    for (int i = 0; i < n; i++) if (iv[i] >= 0 && iv[i] < 24) m |= 1u << iv[i];
    return m;
}

const char *chord_suffix(unsigned m)
{
#define H(x) ((((m) >> (x)) & 1u) || (((m) >> ((x) + 12)) & 1u))
    int M3 = H(4), m3 = H(3), p4 = H(5), p5 = H(7), b5 = H(6), s5 = H(8), M6 = H(9), b7 = H(10), M7 = H(11), b9 = H(1);
    int third = M3 || m3;
    int n9 = ((m >> 14) & 1u) || (((m >> 2) & 1u) && third);
    int s2 = ((m >> 2) & 1u) && !third;
#undef H
    if (m3 && !M3 && b5 && !p5) {                  /* diminuiti */
        if (b7) return n9 ? "m9b5" : (b9 ? "m7b5(b9)" : "m7b5");
        if (M6) return "dim7";
        return "dim";
    }
    if (M3 && s5 && !p5) {                         /* aumentati */
        if (b7) return n9 ? "9#5" : "7#5";
        if (M7) return "maj7#5";
        return "aug";
    }
    if (!third) {                                  /* sospesi */
        if (p4) {
            if (b7) return n9 || s2 ? "9sus4" : "7sus4";
            if (M7) return "maj7sus4";
            if (s2) return "sus4add9";
            if (M6) return "6sus4";
            return "sus4";
        }
        if (s2) return b7 ? "7sus2" : (M7 ? "maj7sus2" : (M6 ? "6sus2" : "sus2"));
        return "5";
    }
    if (M3) {
        if (b7) {
            if (n9 && M6) return "13";
            if (n9) return "9";
            if (M6) return "7(13)";
            if (b9) return "7(b9)";
            return "7";
        }
        if (M7) return n9 ? (M6 ? "maj13" : "maj9") : (M6 ? "maj7(13)" : "maj7");
        if (M6) return n9 ? "6/9" : "6";
        if (n9) return "add9";
        return b9 ? "(addb9)" : "";
    }
    if (b7) {                                      /* minori */
        if (n9 && M6) return "m13";
        if (n9) return "m9";
        if (M6) return "m7(13)";
        if (b9) return "m7(b9)";
        return "m7";
    }
    if (M7) return n9 ? "m(maj9)" : "m(maj7)";
    if (M6) return n9 ? "m6/9" : "m6";
    if (n9) return "m(add9)";
    return b9 ? "m(addb9)" : "m";
}

void chord_name(char *out, size_t size, int root, unsigned mask, int spelling)
{
    snprintf(out, size, "%s%s", note_it(root, spelling), chord_suffix(mask));
}

void chord_symbol(char *out, size_t size, int root, unsigned mask, int spelling)
{
    snprintf(out, size, "%s%s", note_en(root, spelling), chord_suffix(mask));
}

void chord_numeral(char *out, size_t size, int degree, unsigned mask)
{
    static const char *UP[7] = { "I", "II", "III", "IV", "V", "VI", "VII" };
    static const char *LO[7] = { "i", "ii", "iii", "iv", "v", "vi", "vii" };
    degree = ((degree % 7) + 7) % 7;
    const char *suf = chord_suffix(mask);
    int minor = suf[0] == 'm' && strncmp(suf, "maj", 3) != 0;
    if (!strcmp(suf, "dim")) snprintf(out, size, "%s\xc2\xb0", LO[degree]);
    else if (!strcmp(suf, "dim7")) snprintf(out, size, "%s\xc2\xb0" "7", LO[degree]);
    else if (!strncmp(suf, "m7b5", 4)) snprintf(out, size, "%s\xc3\xb8" "7", LO[degree]);
    else snprintf(out, size, "%s%s", minor ? LO[degree] : UP[degree], minor ? suf + 1 : suf);
}

int chord_bass(int root) { return 36 + wrap12(root); }

/* Allarga il voicing stretto v (n note): ritorna il nuovo numero di note. */
static int apply_spread(int *v, int n, int spread)
{
    if (spread == SPREAD_OPEN) {
        if (n >= 4) v[n - 2] -= 12;                       /* drop 2 */
        else if (n == 3) v[1] += 12;                      /* triade aperta */
    } else if (spread == SPREAD_WIDE) {
        if (n >= 4) { v[n - 2] -= 12; v[n - 4] -= 12; }   /* drop 2 e 4 */
        else if (n == 3) { int r = v[0]; v[1] += 12; v[3] = r + 24; n = 4; }
    }
    sort_int(v, n);
    return n;
}

/* Quanto si muovono le voci: voce per voce se le note sono tante quante prima, altrimenti la distanza
   dalla nota piu' vicina, nei due sensi. */
static float movement(const int *a, int an, const int *b, int bn)
{
    float m = 0.0f;
    if (an == bn) {
        for (int i = 0; i < an; i++) m += (float)abs(a[i] - b[i]);
        return m;
    }
    for (int pass = 0; pass < 2; pass++) {
        const int *x = pass ? b : a, *y = pass ? a : b;
        int xn = pass ? bn : an, yn = pass ? an : bn;
        for (int i = 0; i < xn; i++) {
            int best = 1000;
            for (int j = 0; j < yn; j++) if (abs(x[i] - y[j]) < best) best = abs(x[i] - y[j]);
            m += 0.5f * (float)best;
        }
    }
    return m;
}

int chord_voicing(int root, const int *iv, int n, float position, int spread, const int *prev, int prev_n,
                  int *out, int out_max)
{
    if (n <= 0) return 0;
    if (n > CHORD_MAX_IV) n = CHORD_MAX_IV;
    if (position < 0.0f) position = 0.0f;
    if (position > 1.0f) position = 1.0f;
    const float target = 50.0f + position * 26.0f;        /* Re3 .. Mi5 */
    int best[CHORD_MAX_IV + 2], bn = 0;
    float best_err = 1e9f;
    for (int inv = 0; inv < n; inv++) {
        int v[CHORD_MAX_IV + 2];
        for (int i = 0; i < n; i++) v[i] = iv[i] + (i < inv ? 12 : 0);    /* rivolto: le prime inv note salgono */
        sort_int(v, n);
        int m = apply_spread(v, n, spread);
        for (int oct = 0; oct <= 8; oct++) {
            int base = root % 12 + 12 * oct, lo = base + v[0], hi = base + v[m - 1];
            if (lo < 28 || hi > 100) continue;
            int cand[CHORD_MAX_IV + 2];
            float mean = 0.0f;
            for (int i = 0; i < m; i++) { cand[i] = base + v[i]; mean += (float)cand[i]; }
            mean /= (float)m;
            float err = fabsf(mean - target);
            if (prev && prev_n > 0) err += 0.35f * movement(cand, m, prev, prev_n);
            if (err < best_err - 1e-3f) {
                best_err = err;
                bn = m;
                memcpy(best, cand, sizeof(int) * (size_t)m);
            }
        }
    }
    if (!bn) {                                     /* non dovrebbe succedere: posizione fondamentale */
        bn = n;
        for (int i = 0; i < n; i++) best[i] = 48 + root % 12 + iv[i];
    }
    if (bn > out_max) bn = out_max;
    memcpy(out, best, sizeof(int) * (size_t)bn);
    return bn;
}
