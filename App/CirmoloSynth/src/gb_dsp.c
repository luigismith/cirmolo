/* Cirmolo Synth - groovebox: motore audio (vedi gb_dsp.h).
 *
 * Come gira un blocco: gb_render divide il blocco in sottoblocchi di al piu' 64 campioni che finiscono
 * esattamente sul prossimo evento del sequencer; cosi' ogni colpo cade sul campione giusto ma le voci si
 * calcolano a pezzi, molto piu' in fretta che campione per campione. Ogni traccia ha un buffer mono; il
 * mixer lo porta in stereo con volume e pan e ne manda una parte a delay e riverbero; poi il bus master
 * (pompa, stutter, compressore, filtro DJ, lo-fi, nastro, limitatore).
 *
 * Tempo musicale: T in sedicesimi "dritti", cresce di bpm*4/60 al secondo mentre suona. Lo swing ritarda i
 * sedicesimi dispari (il 2o, 4o... di ogni ottavo) di swing sedicesimi: 0.33 = terzina, come nel vecchio
 * synth; sui trentaduesimi in proporzione, sulle terzine niente. Ogni traccia corre con la sua lunghezza e
 * velocita'; il "giro" del pattern e' la traccia piu' lunga fra quelle con passi accesi. A fine giro si
 * cambia pattern (o si va avanti nella catena) e tutte le tracce ripartono insieme; se il pattern resta,
 * ripartono in fase solo le tracce che stanno un numero intero di volte nel giro, le altre (poliritmi, es.
 * 12 contro 16) continuano libere. Le condizioni 1_2, 2_2, 1_4 e FIRST contano i giri del pattern da
 * quando e' partito; prob 0 = mai.
 *
 * Voci di batteria: una per traccia (piu' quella che sta sfumando quando un colpo nuovo la taglia, ~1 ms,
 * cosi' ratchet e choke non fanno clic). I parametri si leggono al colpo; volume e pan nel mixer, dal vivo.
 * Ogni modello e' scritto a mano: inviluppi esponenziali (un moltiplicatore per campione), seno da tabella,
 * filtri SVF con coefficienti calcolati al colpo. La voce si spegne quando tutti gli inviluppi sono sotto
 * -74 dB. Niente allocazioni nel thread audio.
 */
#include "gb_dsp.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <time.h>
#endif
#if defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
#endif

#define PI_F        3.14159265358979f
#define BLOCK       64
#define QUEUE_SIZE  1024
#define SIN_N       4096
#define VOICE_END   2e-4f            /* -74 dB: sotto, la voce si spegne */
#define MAX_CHAIN   64
#define CHAIN_SLOTS 4
#define DELAY_SEC   3.4f             /* 8 sedicesimi a 40 bpm sono 3 s */
#define HIST_SEC    2.0f             /* stutter: 4 sedicesimi a 40 bpm sono 1,5 s */
#define TAPE_LEN    1024
#define FDN         8
#define DIFF        4
#define LOCK_OFF    (-1000.0f)
#define EPS         1e-7
#define MIX_GAIN    0.6f             /* margine del bus prima del master */
#define ACID_GAIN   0.7f             /* acido e Piega: stanno sopra le batterie senza coprirle */
#define FOLD_GAIN   0.55f

/* ------------------------------------------------------------------ utilita' */

static float sin_tab[SIN_N + 1];

static void tables_init(void)
{
    static int ready;
    if (ready) return;
    for (int i = 0; i <= SIN_N; i++) sin_tab[i] = (float)sin(6.283185307179586 * (double)i / SIN_N);
    ready = 1;
}

static inline float sin1(float ph)               /* sin(2 pi ph) da tabella, ph >= 0 */
{
    float x = ph * (float)SIN_N;
    int i = (int)x;
    float f = x - (float)i;
    i &= SIN_N - 1;
    return sin_tab[i] + f * (sin_tab[i + 1] - sin_tab[i]);
}

static inline float wrap1(float x) { return x - (float)(int)x; }      /* parte frazionaria, x >= 0 */
static inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static inline int clampi(int x, int lo, int hi) { return x < lo ? lo : (x > hi ? hi : x); }

static inline float softclip(float x)
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

static inline float noise(uint32_t *s)           /* xorshift32: rumore bianco -1..1 */
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return (float)(int32_t)x * (1.0f / 2147483648.0f);
}

static inline float polyblep(float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

static inline float square_blep(float ph, float dt)
{
    float v = ph < 0.5f ? 1.0f : -1.0f;
    float t2 = ph + 0.5f;
    if (t2 >= 1.0f) t2 -= 1.0f;
    return v + polyblep(ph, dt) - polyblep(t2, dt);
}

static inline float tauk(float tau, float sr)    /* moltiplicatore per campione di un esponenziale */
{
    if (tau < 1e-4f) tau = 1e-4f;
    return expf(-1.0f / (tau * sr));
}

static inline float onepole(float fc, float sr) { return 1.0f - expf(-2.0f * PI_F * fc / sr); }

static inline uint32_t lfsr_step(uint32_t l, int short_mode)   /* rumore del NES: 15 bit, modo corto = metallico */
{
    uint32_t b = (l ^ (l >> (short_mode ? 6 : 1))) & 1u;
    return (l >> 1) | (b << 14);
}

/* Filtro a variabili di stato (Cytomic, trapezoidale): stabile anche con modulazioni veloci. */
typedef struct { float a1, a2, a3, k, ic1, ic2; } Svf;

static void svf_set(Svf *f, float fc, float k, float sr)
{
    fc = clampf(fc, 10.0f, 0.45f * sr);
    float g = tanf(PI_F * fc / sr);
    f->k = k;
    f->a1 = 1.0f / (1.0f + g * (g + k));
    f->a2 = g * f->a1;
    f->a3 = g * f->a2;
}

static inline float svf_run(Svf *f, float x, float *bp)
{
    float v3 = x - f->ic2;
    float v1 = f->a1 * f->ic1 + f->a2 * v3;
    float v2 = f->ic2 + f->a2 * f->ic1 + f->a3 * v3;
    f->ic1 = 2.0f * v1 - f->ic1;
    f->ic2 = 2.0f * v2 - f->ic2;
    *bp = v1;
    return v2;
}

static inline float svf_lp(Svf *f, float x) { float bp; return svf_run(f, x, &bp); }
static inline float svf_bp(Svf *f, float x) { float bp; svf_run(f, x, &bp); return bp * f->k; }  /* guadagno 1 al centro */
static inline float svf_hp(Svf *f, float x) { float bp, lp = svf_run(f, x, &bp); return x - f->k * bp - lp; }

/* ------------------------------------------------------------------ voci di batteria */

typedef struct {
    int on, model, t;
    float gain;                 /* modello x velocita' (volume e pan li mette il mixer) */
    float drive_g, drive_n;     /* saturazione: guadagno e compensazione; 0 = spenta */
    float e[4], k[4];           /* inviluppi: valore e moltiplicatore per campione (la voce vive finche' uno e' sopra soglia) */
    float att, att_k;           /* attacco morbido: l'ampiezza va per (1 - att) */
    float pe, pk, pa;           /* caduta d'altezza: inviluppo, moltiplicatore, quantita' */
    float ph[8], inc[8], amp[8];
    float c[6];                 /* costanti del modello */
    Svf f[2];
    float z[4];                 /* stati vari */
    uint32_t rng, lfsr;
    int cnt[3];
    float fade, fade_k;         /* voce tagliata: dissolvenza (fade_k > 0) */
} DVoice;

/* Volume di ogni modello: tarato con gb_test (sonorita' pesata K, finestra di 50 ms, parametri a meta') perche' a level 0.8
   i modelli suonino forti uguali. */
static const float MODEL_GAIN[DM_COUNT] = {
    [DM_KICK_808] = 0.97f, [DM_KICK_909] = 0.87f, [DM_KICK_ACOUSTIC] = 1.04f, [DM_KICK_FM] = 1.05f, [DM_KICK_HARD] = 0.56f,
    [DM_SNARE_808] = 0.77f, [DM_SNARE_909] = 0.89f, [DM_SNARE_ACOUSTIC] = 0.82f, [DM_SNARE_BRUSH] = 1.68f,
    [DM_RIM] = 1.43f, [DM_CLAP] = 2.00f, [DM_SNAP] = 1.56f,
    [DM_HAT_CLOSED] = 6.41f, [DM_HAT_OPEN] = 3.72f, [DM_HAT_909] = 1.19f, [DM_CYMBAL] = 4.37f, [DM_RIDE] = 1.39f,
    [DM_TOM] = 0.83f, [DM_TOM_ELECTRO] = 0.95f,
    [DM_CONGA] = 0.78f, [DM_BONGO] = 0.85f, [DM_TABLA] = 0.60f,
    [DM_COWBELL] = 2.15f, [DM_CLAVE] = 0.88f, [DM_WOODBLOCK] = 0.89f, [DM_SHAKER] = 2.46f, [DM_TAMBOURINE] = 1.23f,
    [DM_TRIANGLE] = 0.38f,
    [DM_CHIP_KICK] = 1.17f, [DM_CHIP_SNARE] = 0.55f, [DM_CHIP_HAT] = 1.11f,
    [DM_ZAP] = 0.54f, [DM_GLITCH] = 0.89f, [DM_NOISE_SWEEP] = 4.22f,
};

/* frequenze delle sei quadre del charleston e dei piatti della 808 */
static const float METAL_HZ[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };

static float rnd01(uint32_t *s) { return (noise(s) + 1.0f) * 0.5f; }

/* Prepara la voce per un colpo: tutti i calcoli costosi (esponenziali, tangenti dei filtri) stanno qui. */
static void drum_start(DVoice *v, const GbDrum *d, float vel, float sr, uint32_t seed)
{
    memset(v, 0, sizeof(*v));
    const int m = (d->model >= 0 && d->model < DM_COUNT) ? d->model : DM_KICK_808;
    const float T = exp2f(clampf(d->tune, -24.0f, 24.0f) / 12.0f);
    const float D = clampf(d->decay, 0.0f, 1.0f), C = clampf(d->tone, 0.0f, 1.0f), S = clampf(d->snap, 0.0f, 1.0f);
    const float D2 = D * D, inv = 1.0f / sr, sT = sqrtf(T);
    v->on = 1;
    v->model = m;
    v->fade = 1.0f;
    v->rng = seed * 2654435761u + 0x9E3779B9u;
    if (!v->rng) v->rng = 1;
    v->lfsr = 1;
    vel = clampf(vel, 0.0f, 1.0f);
    v->gain = MODEL_GAIN[m] * vel * sqrtf(vel);          /* velocita'^1.5: le note fantasma stanno sotto */
    float dr = clampf(d->drive, 0.0f, 1.0f);
    if (dr > 0.001f) {
        v->drive_g = 1.0f + 7.0f * dr;
        v->drive_n = (1.0f - 0.3f * dr) / softclip(v->drive_g);
    }
    v->pe = 1.0f;
    v->e[0] = 1.0f;
    switch (m) {
    case DM_KICK_808:           /* seno con caduta d'altezza lenta, coda lunga; tone = saturazione dell'attacco */
        v->inc[0] = 49.0f * T * inv;
        v->pa = 0.25f + 2.2f * S * S;
        v->pk = tauk(0.012f + 0.02f * (1.0f - S), sr);
        v->k[0] = tauk(0.07f + 1.3f * D2, sr);
        v->e[1] = 1.0f; v->k[1] = tauk(0.03f, sr);
        v->c[0] = C;
        v->e[2] = 0.3f * S; v->k[2] = tauk(0.0007f, sr);
        break;
    case DM_KICK_909:           /* caduta d'altezza veloce (punch) + clic di rumore e impulso */
        v->inc[0] = 52.0f * T * inv;
        v->pa = 1.2f + 4.0f * S;
        v->pk = tauk(0.007f, sr);
        v->z[0] = 1.0f; v->c[1] = 0.4f; v->c[2] = tauk(0.07f, sr);
        v->k[0] = tauk(0.05f + 0.75f * D2, sr);
        v->e[1] = 0.15f + 0.5f * C; v->k[1] = tauk(0.0035f, sr);
        v->e[2] = 0.9f * S; v->k[2] = tauk(0.00035f, sr);
        break;
    case DM_KICK_ACOUSTIC:      /* pelle (due modi) + battente di feltro (rumore filtrato) */
        v->inc[0] = 58.0f * T * inv;
        v->inc[1] = v->inc[0] * 1.47f;
        v->pa = 0.25f + 0.6f * S;
        v->pk = tauk(0.02f, sr);
        v->k[0] = tauk(0.06f + 0.4f * D2, sr);
        v->e[1] = 0.35f; v->k[1] = tauk(0.03f + 0.08f * D2, sr);
        v->e[2] = 0.5f + 0.7f * S; v->k[2] = tauk(0.004f, sr);
        v->c[0] = onepole(800.0f + 6000.0f * C, sr);
        break;
    case DM_KICK_FM:            /* portante sinusoidale modulata in fase; tone = rapporto, snap = indice */
        v->inc[0] = 50.0f * T * inv;
        v->c[0] = 0.5f + 2.0f * C;
        v->pa = 1.0f + 1.5f * S;
        v->pk = tauk(0.012f, sr);
        v->k[0] = tauk(0.06f + 0.8f * D2, sr);
        v->e[1] = 1.0f; v->k[1] = tauk(0.035f + 0.05f * D, sr);
        v->c[1] = 0.08f + 0.35f * S;
        break;
    case DM_KICK_HARD:          /* gabber: seno con grande caduta, distorto forte e filtrato */
        v->inc[0] = 55.0f * T * inv;
        v->pa = 2.5f + 5.0f * S;
        v->pk = tauk(0.011f, sr);
        v->z[0] = 1.0f; v->c[1] = 0.6f; v->c[2] = tauk(0.06f, sr);
        v->k[0] = tauk(0.1f + 0.55f * D2, sr);
        v->c[0] = 10.0f;
        v->e[1] = 0.2f + 0.6f * S; v->k[1] = tauk(0.002f, sr);
        svf_set(&v->f[0], 1800.0f + 7000.0f * C, 1.2f, sr);
        v->e[3] = 10.0f;
        break;
    case DM_SNARE_808:          /* due oscillatori a ponte T + rumore passa-alto; snap = "snappy" */
        v->inc[0] = 185.0f * T * inv;
        v->inc[1] = 330.0f * T * inv;
        v->amp[0] = 0.9f; v->amp[1] = 0.35f + 0.5f * C;
        v->k[0] = tauk(0.03f + 0.1f * D, sr);
        v->e[1] = 1.0f; v->k[1] = tauk(0.02f + 0.07f * D, sr);
        v->e[2] = 0.3f + 0.9f * S; v->k[2] = tauk(0.05f + 0.22f * D2, sr);
        svf_set(&v->f[0], 1800.0f, 1.4f, sr);
        svf_set(&v->f[1], 3500.0f + 6000.0f * C, 1.1f, sr);   /* rumore a 2 poli: brillante ma non sibilante */
        break;
    case DM_SNARE_909:          /* corpo con piccola caduta + rumore passa-basso e passa-alto */
        v->inc[0] = 180.0f * T * inv;
        v->inc[1] = 330.0f * T * inv;
        v->pa = 0.35f; v->pk = tauk(0.008f, sr);
        v->amp[1] = 0.5f;
        v->k[0] = tauk(0.045f + 0.09f * D, sr);
        v->e[1] = 1.0f; v->k[1] = tauk(0.03f + 0.06f * D, sr);
        v->e[2] = 0.35f + 0.85f * S; v->k[2] = tauk(0.06f + 0.3f * D2, sr);
        svf_set(&v->f[0], 6000.0f + 10000.0f * C, 1.2f, sr);
        v->c[0] = onepole(500.0f, sr);
        break;
    case DM_SNARE_ACOUSTIC:     /* tre modi della pelle + cordiera (rumore in passa-banda) + colpo di bacchetta */
        v->inc[0] = 190.0f * T * inv;
        v->inc[1] = v->inc[0] * 1.59f;
        v->inc[2] = v->inc[0] * 2.14f;
        v->pa = 0.12f; v->pk = tauk(0.01f, sr);
        v->c[1] = 1.0f - 0.45f * C;
        v->c[2] = 0.55f + 0.6f * C;
        v->k[0] = tauk(0.05f + 0.12f * D, sr);
        v->e[1] = 1.0f; v->k[1] = tauk(0.03f + 0.06f * D, sr);
        v->e[2] = 1.0f; v->k[2] = tauk(0.09f + 0.28f * D2, sr);
        v->e[3] = 0.6f * S; v->k[3] = tauk(0.0015f, sr);
        svf_set(&v->f[0], 2100.0f * (0.7f + 0.9f * C) * sT, 1.3f, sr);
        svf_set(&v->f[1], 900.0f, 1.4f, sr);
        break;
    case DM_SNARE_BRUSH:        /* spazzola: rumore granuloso con attacco strisciato; snap = colpo secco */
        svf_set(&v->f[0], (2500.0f + 4500.0f * C) * sT, 0.9f, sr);
        v->att = 1.0f; v->att_k = tauk(0.003f + 0.06f * (1.0f - S), sr);
        v->k[0] = tauk(0.05f + 0.5f * D2, sr);
        v->inc[0] = 190.0f * T * inv;
        v->e[1] = 0.15f; v->k[1] = tauk(0.03f, sr);
        v->z[0] = v->z[1] = 1.0f;
        break;
    case DM_RIM:                /* 808: due risonanze cortissime saturate + clic */
        v->inc[0] = 480.0f * T * inv;
        v->inc[1] = 1700.0f * T * inv;
        v->amp[0] = 0.75f - 0.5f * C; v->amp[1] = 0.4f + 0.6f * C;
        v->k[0] = tauk(0.006f + 0.03f * D, sr);
        v->e[1] = 1.0f; v->k[1] = tauk(0.004f + 0.02f * D, sr);
        v->e[2] = 0.3f + 0.6f * S; v->k[2] = tauk(0.0008f, sr);
        break;
    case DM_CLAP:               /* 3-5 scoppi di rumore in passa-banda e una coda; snap = scoppi piu' fitti */
        svf_set(&v->f[0], (750.0f + 1500.0f * C) * sT, 0.8f, sr);
        v->k[0] = tauk(0.0025f, sr);
        v->k[1] = tauk(0.04f + 0.45f * D2, sr);
        v->c[0] = (0.012f - 0.006f * S) * sr;
        v->cnt[0] = (int)v->c[0];
        v->cnt[1] = 2 + (int)(S * 2.99f);
        break;
    case DM_SNAP:               /* schiocco di dita: ping acuto + rumore stretto */
        v->inc[0] = 1900.0f * T * inv;
        v->k[0] = tauk(0.01f, sr);
        v->c[0] = 0.5f;
        svf_set(&v->f[0], (2400.0f + 2000.0f * C) * T, 0.35f, sr);
        v->e[1] = 1.0f; v->k[1] = tauk(0.012f + 0.06f * D, sr);
        v->e[2] = 0.4f + 0.6f * S; v->k[2] = tauk(0.0005f, sr);
        break;
    case DM_HAT_CLOSED:
    case DM_HAT_OPEN:           /* 808: sei quadre metalliche -> passa-banda -> passa-alto */
        for (int j = 0; j < 6; j++) { v->inc[j] = METAL_HZ[j] * T * inv; v->ph[j] = rnd01(&v->rng); }
        svf_set(&v->f[0], (6000.0f + 5000.0f * C) * sT, 0.9f, sr);
        svf_set(&v->f[1], 6500.0f * sT, 1.4f, sr);
        v->c[0] = 0.15f + 0.25f * C;
        v->k[0] = m == DM_HAT_CLOSED ? tauk(0.008f + 0.09f * D2, sr) : tauk(0.06f + 0.8f * D2, sr);
        v->e[1] = S; v->k[1] = tauk(0.0025f, sr);
        break;
    case DM_HAT_909:            /* due coppie FM inarmoniche moltiplicate fra loro + rumore, passa-alto */
        v->inc[0] = 2400.0f * T * inv; v->inc[1] = v->inc[0] * 1.4471f;
        v->inc[2] = 3500.0f * T * inv; v->inc[3] = v->inc[2] * 1.618f;
        for (int j = 0; j < 4; j++) { if (v->inc[j] > 0.45f) v->inc[j] = 0.45f; v->ph[j] = rnd01(&v->rng); }
        v->c[0] = 0.12f + 0.2f * C;
        v->c[1] = 0.25f;
        svf_set(&v->f[0], 6500.0f * sT, 1.2f, sr);
        v->k[0] = tauk(0.01f + 0.7f * D2, sr);
        v->e[1] = S; v->k[1] = tauk(0.002f, sr);
        break;
    case DM_CYMBAL:             /* crash: metallo e rumore su due bande, quella alta dura di piu' */
        for (int j = 0; j < 6; j++) { v->inc[j] = METAL_HZ[j] * 1.13f * T * inv; v->ph[j] = rnd01(&v->rng); }
        svf_set(&v->f[0], 3000.0f * sT, 0.7f, sr);
        svf_set(&v->f[1], 8000.0f * sT, 0.6f, sr);
        v->c[0] = 0.9f - 0.5f * C; v->c[1] = 0.35f + 0.45f * C; v->c[2] = 0.25f;
        v->k[0] = tauk(0.35f + 2.2f * D2, sr);
        v->e[1] = 1.0f; v->k[1] = tauk((0.35f + 2.2f * D2) * 0.4f, sr);
        v->e[2] = 0.8f * S; v->k[2] = tauk(0.012f, sr);
        v->att = 1.0f; v->att_k = tauk(0.0008f, sr);
        break;
    case DM_RIDE:               /* metallo in passa-banda + campana (snap) + tic della bacchetta */
        for (int j = 0; j < 6; j++) { v->inc[j] = METAL_HZ[j] * 1.37f * T * inv; v->ph[j] = rnd01(&v->rng); }
        v->inc[6] = 830.0f * T * inv; v->inc[7] = 830.0f * 2.39f * T * inv;
        v->amp[6] = 0.6f; v->amp[7] = v->inc[7] < 0.45f ? 0.4f : 0.0f;
        svf_set(&v->f[0], 5200.0f * sT * (0.8f + 0.4f * C), 0.7f, sr);
        svf_set(&v->f[1], 6000.0f, 1.4f, sr);
        v->c[0] = 0.15f;
        v->k[0] = tauk(0.3f + 1.6f * D2, sr);
        v->e[1] = 0.15f + 0.75f * S; v->k[1] = tauk(0.15f + 0.9f * D2, sr);
        v->e[2] = 0.5f; v->k[2] = tauk(0.0012f, sr);
        break;
    case DM_TOM:                /* seno con caduta d'altezza + secondo modo + pelle (rumore) */
        v->inc[0] = 105.0f * T * inv;
        v->inc[1] = v->inc[0] * 1.5f;
        v->pa = 0.2f + 0.9f * S; v->pk = tauk(0.045f, sr);
        v->k[0] = tauk(0.07f + 0.55f * D2, sr);
        v->e[1] = 0.25f; v->k[1] = tauk((0.07f + 0.55f * D2) * 0.35f, sr);
        v->e[2] = 0.15f + 0.5f * C; v->k[2] = tauk(0.006f, sr);
        v->c[0] = onepole(1200.0f + 5000.0f * C, sr);
        break;
    case DM_TOM_ELECTRO:        /* Simmons: grande caduta d'altezza lenta, seno/triangolo, rumore all'attacco */
        v->inc[0] = 95.0f * T * inv;
        v->pa = 0.4f + 2.6f * S; v->pk = tauk(0.1f + 0.35f * D, sr);
        v->c[0] = 0.6f * C;
        v->c[1] = onepole(3000.0f, sr);
        v->k[0] = tauk(0.08f + 0.65f * D2, sr);
        v->e[2] = 0.35f; v->k[2] = tauk(0.008f, sr);
        break;
    case DM_CONGA:
    case DM_BONGO: {            /* membrana: modi inarmonici, piccola caduta, schiaffo (snap) */
        int conga = m == DM_CONGA;
        v->inc[0] = (conga ? 255.0f : 410.0f) * T * inv;
        v->inc[1] = v->inc[0] * (conga ? 1.62f : 1.58f);
        v->inc[2] = v->inc[0] * 2.42f;
        v->amp[1] = 0.15f + 0.35f * C;
        v->amp[2] = conga ? 0.12f : 0.0f;
        for (int j = 1; j < 3; j++) if (v->inc[j] > 0.45f) { v->inc[j] = 0.0f; v->amp[j] = 0.0f; }
        v->pa = (conga ? 0.04f : 0.06f) + 0.3f * S; v->pk = tauk(conga ? 0.02f : 0.015f, sr);
        float tau = conga ? 0.07f + 0.45f * D2 : 0.045f + 0.28f * D2;
        v->k[0] = tauk(tau, sr);
        v->e[1] = 1.0f; v->k[1] = tauk(tau * 0.4f, sr);
        v->e[2] = 0.2f + 0.8f * S; v->k[2] = tauk(conga ? 0.0035f : 0.0025f, sr);
        svf_set(&v->f[0], (conga ? 1500.0f + 2500.0f * C : 2500.0f + 3000.0f * C) * sT, 0.9f, sr);
        break;
    }
    case DM_TABLA: {            /* armonici quasi interi (la "syahi"), con la caduta d'altezza tipica */
        static const float h[5] = { 1.0f, 0.6f, 0.38f, 0.25f, 0.15f };
        v->inc[0] = 300.0f * T * inv;
        for (int j = 0; j < 5; j++) {
            v->amp[j] = h[j] * (j ? 0.3f + 1.4f * C : 1.0f);
            if (v->inc[0] * (float)(j + 1) * (1.0f + 0.8f) > 0.45f) v->amp[j] = 0.0f;
        }
        v->pa = 0.08f + 0.7f * S; v->pk = tauk(0.07f, sr);
        v->k[0] = tauk(0.12f + 0.9f * D2, sr);
        v->e[1] = 1.0f; v->k[1] = tauk((0.12f + 0.9f * D2) * 0.5f, sr);
        v->e[2] = 0.3f; v->k[2] = tauk(0.002f, sr);
        break;
    }
    case DM_COWBELL:            /* 808: due quadre 540/800 Hz in passa-banda, inviluppo a due tempi */
        v->inc[0] = 540.0f * T * inv;
        v->inc[1] = 800.0f * T * inv;
        svf_set(&v->f[0], (1100.0f + 2000.0f * C) * T, 0.7f, sr);
        v->e[0] = 0.55f; v->k[0] = tauk(0.006f, sr);
        v->e[1] = 0.45f; v->k[1] = tauk(0.045f + 0.35f * D2, sr);
        break;
    case DM_CLAVE:
    case DM_WOODBLOCK: {        /* legni: due modi corti, un filo di caduta, clic */
        int cl = m == DM_CLAVE;
        v->inc[0] = (cl ? 2500.0f : 780.0f) * T * inv;
        v->inc[1] = v->inc[0] * (cl ? 1.83f : 2.63f);
        v->amp[1] = cl ? 0.25f * C : 0.25f + 0.4f * C;
        if (v->inc[0] > 0.45f) v->inc[0] = 0.45f;
        if (v->inc[1] > 0.45f) { v->inc[1] = 0.0f; v->amp[1] = 0.0f; }
        v->pa = cl ? 0.0f : 0.06f; v->pk = tauk(0.008f, sr);
        float tau = cl ? 0.01f + 0.045f * D : 0.018f + 0.09f * D;
        v->k[0] = tauk(tau, sr);
        v->e[1] = 1.0f; v->k[1] = tauk(tau * 0.5f, sr);
        v->e[2] = cl ? 0.3f * S : 0.25f + 0.5f * S; v->k[2] = tauk(cl ? 0.0004f : 0.0012f, sr);
        v->c[0] = onepole(cl ? 9000.0f : 4000.0f, sr);
        break;
    }
    case DM_SHAKER:             /* rumore passa-alto a grani; snap = attacco secco */
        svf_set(&v->f[0], (5000.0f + 5000.0f * C) * sT, 1.4f, sr);   /* passa-banda largo */
        v->att = 1.0f; v->att_k = tauk(0.002f + 0.03f * (1.0f - S), sr);
        v->k[0] = tauk(0.025f + 0.2f * D2, sr);
        v->z[0] = v->z[1] = 1.0f;
        break;
    case DM_TAMBOURINE:         /* sonagli: quadre acute + rumore, tre scosse ravvicinate e coda; snap = pelle */
        {
            static const float j_hz[4] = { 5300.0f, 6750.0f, 8200.0f, 9650.0f };
            for (int j = 0; j < 4; j++) { v->inc[j] = clampf(j_hz[j] * T * inv, 0.0f, 0.45f); v->ph[j] = rnd01(&v->rng); }
        }
        v->inc[4] = 260.0f * T * inv; v->amp[4] = 0.5f * S;
        svf_set(&v->f[0], (6000.0f + 4000.0f * C) * sT, 1.0f, sr);
        svf_set(&v->f[1], 4000.0f, 1.4f, sr);
        v->k[0] = tauk(0.006f, sr);
        v->k[1] = tauk(0.06f + 0.45f * D2, sr);
        v->e[2] = 1.0f; v->k[2] = tauk(0.02f, sr);
        v->cnt[0] = (int)(0.006f * sr); v->cnt[1] = 2;
        break;
    case DM_TRIANGLE: {         /* modi di una sbarra libera: 1, 2.76, 5.40, 8.93; lunghissimo */
        static const float r[4] = { 1.0f, 2.756f, 5.404f, 8.933f };
        float a[4] = { 0.35f, 1.0f, 0.2f + 0.7f * C, 0.45f * C };
        for (int j = 0; j < 4; j++) {
            v->inc[j] = 1150.0f * r[j] * T * inv;
            v->amp[j] = v->inc[j] < 0.45f ? a[j] : 0.0f;
            if (v->inc[j] >= 0.45f) v->inc[j] = 0.0f;
        }
        v->k[0] = tauk(0.4f + 2.8f * D2, sr);
        v->e[1] = 1.0f; v->k[1] = tauk((0.4f + 2.8f * D2) * 0.55f, sr);
        v->e[2] = 0.3f + 0.5f * S; v->k[2] = tauk(0.0007f, sr);
        break;
    }
    case DM_CHIP_KICK:          /* NES: triangolo a 4 bit con discesa d'altezza, tutto a scatti di 1/240 s */
        v->c[2] = sr / 240.0f;
        v->cnt[0] = 0;
        v->c[0] = 330.0f * T * (0.5f + S) * inv;
        v->c[1] = 48.0f * T * inv;
        v->pk = expf(-(1.0f / 240.0f) / 0.035f);
        v->k[0] = expf(-(1.0f / 240.0f) / (0.05f + 0.35f * D2));
        v->cnt[1] = (int)(2.0f * v->c[2]);
        v->c[3] = 0.5f * S;
        v->c[4] = 1.0f + 0.5f * C;          /* tone: un po' di quadra (canale impulsi) sopra il triangolo */
        break;
    case DM_CHIP_SNARE:         /* rumore LFSR lungo + bip di quadra che scende (snap) */
        v->c[2] = sr / 240.0f;
        v->c[0] = clampf((15000.0f + 120000.0f * C) * T * inv, 0.05f, 12.0f);
        v->inc[0] = 200.0f * T * inv;
        v->pk = expf(-(1.0f / 240.0f) / 0.02f);
        v->e[1] = 0.6f * S; v->k[1] = expf(-(1.0f / 240.0f) / 0.03f);
        v->k[0] = expf(-(1.0f / 240.0f) / (0.06f + 0.25f * D2));
        break;
    case DM_CHIP_HAT:           /* rumore LFSR acuto; snap oltre 0.5 = modo corto, metallico */
        v->c[2] = sr / 240.0f;
        v->c[0] = clampf((120000.0f + 320000.0f * C) * T * inv, 0.05f, 12.0f);
        v->cnt[2] = S >= 0.5f;
        v->k[0] = expf(-(1.0f / 240.0f) / (0.008f + 0.15f * D2));
        break;
    case DM_ZAP:                /* laser: seno che precipita da 2,6 kHz; tone = verso la quadra */
        v->c[0] = clampf(2600.0f * T * inv, 0.0f, 0.45f);
        v->c[1] = 45.0f * T * inv;
        v->pk = tauk(0.004f + 0.035f * (1.0f - S), sr);
        v->k[0] = tauk(0.04f + 0.4f * D2, sr);
        v->c[2] = 1.0f + 6.0f * C;
        v->c[3] = 1.0f / softclip(v->c[2]);
        break;
    case DM_GLITCH:             /* frammenti casuali di onde, ridotti di bit (tone) e di frequenza (snap) */
        v->c[0] = 1.0f + (float)(int)(S * 15.0f);
        v->c[1] = exp2f(2.0f + 6.0f * C);
        v->c[2] = T;
        v->c[3] = sr;
        v->k[0] = tauk(0.05f + 0.35f * D2, sr);
        v->cnt[0] = 1;
        break;
    case DM_NOISE_SWEEP: {      /* soffio: rumore in un filtro risonante che sale (snap > 0.5) o scende */
        float len = 0.15f + 2.5f * D2;
        v->c[0] = log2f(250.0f * T) + 3.0f * C;
        v->c[1] = 1.5f + 5.0f * fabsf(2.0f * S - 1.0f);
        v->cnt[1] = S >= 0.5f;
        v->c[2] = sr;
        v->pk = tauk(len * 0.45f, sr);
        v->att = 1.0f;
        v->att_k = tauk(v->cnt[1] ? len * 0.35f : 0.003f, sr);
        v->k[0] = tauk(v->cnt[1] ? len * 0.5f : len * 0.4f, sr);
        break;
    }
    }
}

static void r_kick808(DVoice *v, float *restrict o, int n)
{
    float ph = v->ph[0], inc = v->inc[0], pe = v->pe, pk = v->pk, pa = v->pa, tone = v->c[0];
    float a = v->e[0], ak = v->k[0], w = v->e[1], wk = v->k[1], cl = v->e[2], ck = v->k[2];
    for (int i = 0; i < n; i++) {
        ph += inc * (1.0f + pa * pe);
        if (ph >= 1.0f) ph -= 1.0f;
        pe *= pk;
        float s = sin1(ph);
        float m = tone * w;
        o[i] = (s + m * (softclip(3.0f * s) - s) + cl) * a;
        a *= ak; w *= wk; cl *= ck;
    }
    v->ph[0] = ph; v->pe = pe; v->e[0] = a; v->e[1] = w; v->e[2] = cl;
}

static void r_kick909(DVoice *v, float *restrict o, int n)
{
    float ph = v->ph[0], inc = v->inc[0], pe = v->pe, pk = v->pk, pa = v->pa;
    float pe2 = v->z[0], pa2 = v->c[1], pk2 = v->c[2], lp = v->z[1];
    float a = v->e[0], ak = v->k[0], nz = v->e[1], nk = v->k[1], im = v->e[2], ik = v->k[2];
    uint32_t r = v->rng;
    for (int i = 0; i < n; i++) {
        ph += inc * (1.0f + pa * pe + pa2 * pe2);
        if (ph >= 1.0f) ph -= 1.0f;
        pe *= pk; pe2 *= pk2;
        float s = softclip(1.5f * sin1(ph)) * 1.077f;
        float x = noise(&r);
        lp += 0.3f * (x - lp);
        o[i] = (s + (x - lp) * nz + im) * a;
        a *= ak; nz *= nk; im *= ik;
    }
    v->ph[0] = ph; v->pe = pe; v->z[0] = pe2; v->z[1] = lp; v->rng = r;
    v->e[0] = a; v->e[1] = nz; v->e[2] = im;
}

static void r_kick_acoustic(DVoice *v, float *restrict o, int n)
{
    float p0 = v->ph[0], p1 = v->ph[1], i0 = v->inc[0], i1 = v->inc[1], pe = v->pe, pk = v->pk, pa = v->pa;
    float a = v->e[0], ak = v->k[0], b = v->e[1], bk = v->k[1], bt = v->e[2], btk = v->k[2], lc = v->c[0], lp = v->z[0];
    uint32_t r = v->rng;
    for (int i = 0; i < n; i++) {
        float m = 1.0f + pa * pe;
        pe *= pk;
        p0 += i0 * m; if (p0 >= 1.0f) p0 -= 1.0f;
        p1 += i1 * m; if (p1 >= 1.0f) p1 -= 1.0f;
        lp += lc * (noise(&r) - lp);
        o[i] = sin1(p0) * a + sin1(p1) * b + lp * bt * 2.0f;
        a *= ak; b *= bk; bt *= btk;
    }
    v->ph[0] = p0; v->ph[1] = p1; v->pe = pe; v->z[0] = lp; v->rng = r;
    v->e[0] = a; v->e[1] = b; v->e[2] = bt;
}

static void r_kick_fm(DVoice *v, float *restrict o, int n)
{
    float p0 = v->ph[0], p1 = v->ph[1], inc = v->inc[0], ratio = v->c[0], idx = v->c[1];
    float pe = v->pe, pk = v->pk, pa = v->pa;
    float a = v->e[0], ak = v->k[0], ie = v->e[1], ik = v->k[1];
    for (int i = 0; i < n; i++) {
        float f = inc * (1.0f + pa * pe);
        pe *= pk;
        p0 += f; if (p0 >= 1.0f) p0 -= 1.0f;
        p1 += f * ratio; if (p1 >= 1.0f) p1 -= 1.0f;
        o[i] = sin1(wrap1(p0 + idx * ie * sin1(p1) + 2.0f)) * a;
        a *= ak; ie *= ik;
    }
    v->ph[0] = p0; v->ph[1] = p1; v->pe = pe; v->e[0] = a; v->e[1] = ie;
}

static void r_kick_hard(DVoice *v, float *restrict o, int n)
{
    float ph = v->ph[0], inc = v->inc[0], pe = v->pe, pk = v->pk, pa = v->pa;
    float pe2 = v->z[0], pa2 = v->c[1], pk2 = v->c[2], pre = v->c[0];
    float a = v->e[0], ak = v->k[0], cl = v->e[1], ck = v->k[1];
    Svf f = v->f[0];
    uint32_t r = v->rng;
    for (int i = 0; i < n; i++) {
        ph += inc * (1.0f + pa * pe + pa2 * pe2);
        if (ph >= 1.0f) ph -= 1.0f;
        pe *= pk; pe2 *= pk2;
        float x = softclip(sin1(ph) * a * pre) + noise(&r) * cl;
        o[i] = svf_lp(&f, x);
        a *= ak; cl *= ck;
    }
    v->ph[0] = ph; v->pe = pe; v->z[0] = pe2; v->f[0] = f; v->rng = r;
    v->e[0] = a; v->e[1] = cl; v->e[3] = a * pre;
}

static void r_snare808(DVoice *v, float *restrict o, int n)
{
    float p0 = v->ph[0], p1 = v->ph[1], i0 = v->inc[0], i1 = v->inc[1], a0 = v->amp[0], a1 = v->amp[1];
    float e0 = v->e[0], k0 = v->k[0], e1 = v->e[1], k1 = v->k[1], e2 = v->e[2], k2 = v->k[2];
    Svf h = v->f[0], lo = v->f[1];
    uint32_t r = v->rng;
    for (int i = 0; i < n; i++) {
        p0 += i0; if (p0 >= 1.0f) p0 -= 1.0f;
        p1 += i1; if (p1 >= 1.0f) p1 -= 1.0f;
        float nz = svf_hp(&h, svf_lp(&lo, noise(&r)));
        o[i] = sin1(p0) * a0 * e0 + sin1(p1) * a1 * e1 + nz * e2 * 1.3f;
        e0 *= k0; e1 *= k1; e2 *= k2;
    }
    v->ph[0] = p0; v->ph[1] = p1; v->f[0] = h; v->f[1] = lo; v->rng = r;
    v->e[0] = e0; v->e[1] = e1; v->e[2] = e2;
}

static void r_snare909(DVoice *v, float *restrict o, int n)
{
    float p0 = v->ph[0], p1 = v->ph[1], i0 = v->inc[0], i1 = v->inc[1], a1 = v->amp[1];
    float pe = v->pe, pk = v->pk, pa = v->pa;
    float e0 = v->e[0], k0 = v->k[0], e1 = v->e[1], k1 = v->k[1], e2 = v->e[2], k2 = v->k[2], hc = v->c[0], hl = v->z[0];
    Svf f = v->f[0];
    uint32_t r = v->rng;
    for (int i = 0; i < n; i++) {
        float m = 1.0f + pa * pe;
        pe *= pk;
        p0 += i0 * m; if (p0 >= 1.0f) p0 -= 1.0f;
        p1 += i1 * m; if (p1 >= 1.0f) p1 -= 1.0f;
        float nz = svf_lp(&f, noise(&r));
        hl += hc * (nz - hl);
        float x = sin1(p0) * e0 + sin1(p1) * a1 * e1 + (nz - hl) * e2;
        o[i] = softclip(1.2f * x) * 0.9f;
        e0 *= k0; e1 *= k1; e2 *= k2;
    }
    v->ph[0] = p0; v->ph[1] = p1; v->pe = pe; v->z[0] = hl; v->f[0] = f; v->rng = r;
    v->e[0] = e0; v->e[1] = e1; v->e[2] = e2;
}

static void r_snare_acoustic(DVoice *v, float *restrict o, int n)
{
    float p0 = v->ph[0], p1 = v->ph[1], p2 = v->ph[2], i0 = v->inc[0], i1 = v->inc[1], i2 = v->inc[2];
    float pe = v->pe, pk = v->pk, pa = v->pa, body = v->c[1], wires = v->c[2];
    float e0 = v->e[0], k0 = v->k[0], e1 = v->e[1], k1 = v->k[1], e2 = v->e[2], k2 = v->k[2], e3 = v->e[3], k3 = v->k[3];
    Svf b = v->f[0], h = v->f[1];
    uint32_t r = v->rng;
    for (int i = 0; i < n; i++) {
        float m = 1.0f + pa * pe;
        pe *= pk;
        p0 += i0 * m; if (p0 >= 1.0f) p0 -= 1.0f;
        p1 += i1 * m; if (p1 >= 1.0f) p1 -= 1.0f;
        p2 += i2 * m; if (p2 >= 1.0f) p2 -= 1.0f;
        float nz = noise(&r);
        float sn = svf_bp(&b, nz);
        float crack = svf_hp(&h, nz) * e3;
        o[i] = body * (sin1(p0) * e0 + (0.5f * sin1(p1) + 0.3f * sin1(p2)) * e1) + wires * sn * e2 * 1.4f + crack;
        e0 *= k0; e1 *= k1; e2 *= k2; e3 *= k3;
    }
    v->ph[0] = p0; v->ph[1] = p1; v->ph[2] = p2; v->pe = pe; v->f[0] = b; v->f[1] = h; v->rng = r;
    v->e[0] = e0; v->e[1] = e1; v->e[2] = e2; v->e[3] = e3;
}

/* grani: un'ampiezza casuale nuova ogni 1-2 ms, smussata (spazzola, shaker) */
static inline float grain(DVoice *v, uint32_t *r, float *tgt, float *cur, int *cnt)
{
    if (--*cnt <= 0) {
        *cnt = 48 + (int)(rnd01(r) * 48.0f);
        *tgt = 0.35f + 0.65f * rnd01(r);
    }
    *cur += 0.08f * (*tgt - *cur);
    return *cur;
}

static void r_brush(DVoice *v, float *restrict o, int n)
{
    float att = v->att, atk = v->att_k, a = v->e[0], ak = v->k[0];
    float ph = v->ph[0], inc = v->inc[0], b = v->e[1], bk = v->k[1];
    float gt = v->z[0], gc = v->z[1];
    int gn = v->cnt[0];
    Svf f = v->f[0];
    uint32_t r = v->rng;
    for (int i = 0; i < n; i++) {
        float gr = grain(v, &r, &gt, &gc, &gn);
        ph += inc; if (ph >= 1.0f) ph -= 1.0f;
        o[i] = svf_bp(&f, noise(&r)) * gr * a * (1.0f - att) * 1.6f + sin1(ph) * b;
        a *= ak; att *= atk; b *= bk;
    }
    v->att = att; v->e[0] = a; v->ph[0] = ph; v->e[1] = b; v->z[0] = gt; v->z[1] = gc; v->cnt[0] = gn;
    v->f[0] = f; v->rng = r;
}

static void r_rim(DVoice *v, float *restrict o, int n)
{
    float p0 = v->ph[0], p1 = v->ph[1], i0 = v->inc[0], i1 = v->inc[1], a0 = v->amp[0], a1 = v->amp[1];
    float e0 = v->e[0], k0 = v->k[0], e1 = v->e[1], k1 = v->k[1], e2 = v->e[2], k2 = v->k[2], lp = v->z[0];
    uint32_t r = v->rng;
    for (int i = 0; i < n; i++) {
        p0 += i0; if (p0 >= 1.0f) p0 -= 1.0f;
        p1 += i1; if (p1 >= 1.0f) p1 -= 1.0f;
        float nz = noise(&r);
        lp += 0.5f * (nz - lp);
        float x = sin1(p0) * a0 * e0 + sin1(p1) * a1 * e1 + (nz - lp) * e2;
        o[i] = softclip(2.2f * x) * 0.8f;
        e0 *= k0; e1 *= k1; e2 *= k2;
    }
    v->ph[0] = p0; v->ph[1] = p1; v->z[0] = lp; v->rng = r;
    v->e[0] = e0; v->e[1] = e1; v->e[2] = e2;
}

static void r_clap(DVoice *v, float *restrict o, int n)
{
    float b = v->e[0], bk = v->k[0], tl = v->e[1], tk = v->k[1];
    int next = v->cnt[0], left = v->cnt[1], sp = (int)v->c[0];
    Svf f = v->f[0];
    uint32_t r = v->rng;
    for (int i = 0; i < n; i++) {
        if (left > 0 && --next <= 0) {             /* nuovo scoppio; con l'ultimo parte la coda */
            b = 1.0f;
            next = sp;
            if (--left == 0) tl = 0.75f;
        }
        float env = b > tl ? b : tl;
        o[i] = svf_bp(&f, noise(&r)) * env * 1.7f;
        b *= bk; tl *= tk;
    }
    v->e[0] = b; v->e[1] = tl; v->cnt[0] = next; v->cnt[1] = left; v->f[0] = f; v->rng = r;
    if (left > 0 && v->e[0] < VOICE_END) v->e[0] = VOICE_END;   /* fra gli scoppi la voce resta viva */
}

static void r_snap(DVoice *v, float *restrict o, int n)
{
    float ph = v->ph[0], inc = v->inc[0], amp = v->c[0];
    float e0 = v->e[0], k0 = v->k[0], e1 = v->e[1], k1 = v->k[1], e2 = v->e[2], k2 = v->k[2];
    Svf f = v->f[0];
    uint32_t r = v->rng;
    for (int i = 0; i < n; i++) {
        ph += inc; if (ph >= 1.0f) ph -= 1.0f;
        float nz = noise(&r);
        o[i] = sin1(ph) * amp * e0 + svf_bp(&f, nz) * e1 * 2.0f + nz * e2;
        e0 *= k0; e1 *= k1; e2 *= k2;
    }
    v->ph[0] = ph; v->f[0] = f; v->rng = r;
    v->e[0] = e0; v->e[1] = e1; v->e[2] = e2;
}

static void r_hat(DVoice *v, float *restrict o, int n)
{
    float p[6], in[6];
    for (int j = 0; j < 6; j++) { p[j] = v->ph[j]; in[j] = v->inc[j]; }
    float a = v->e[0], ak = v->k[0], s = v->e[1], sk = v->k[1], nz = v->c[0];
    Svf b = v->f[0], h = v->f[1];
    uint32_t r = v->rng;
    for (int i = 0; i < n; i++) {
        float x = 0.0f;
        for (int j = 0; j < 6; j++) {
            p[j] += in[j];
            if (p[j] >= 1.0f) p[j] -= 1.0f;
            x += p[j] < 0.5f ? 1.0f : -1.0f;
        }
        x = x * (1.0f / 6.0f) + nz * noise(&r);
        o[i] = svf_hp(&h, svf_bp(&b, x)) * (a + s);
        a *= ak; s *= sk;
    }
    for (int j = 0; j < 6; j++) v->ph[j] = p[j];
    v->e[0] = a; v->e[1] = s; v->f[0] = b; v->f[1] = h; v->rng = r;
}

static void r_hat909(DVoice *v, float *restrict o, int n)
{
    float c1 = v->ph[0], m1 = v->ph[1], c2 = v->ph[2], m2 = v->ph[3];
    float ic1 = v->inc[0], im1 = v->inc[1], ic2 = v->inc[2], im2 = v->inc[3], idx = v->c[0], nzl = v->c[1];
    float a = v->e[0], ak = v->k[0], s = v->e[1], sk = v->k[1];
    Svf h = v->f[0];
    uint32_t r = v->rng;
    for (int i = 0; i < n; i++) {
        c1 += ic1; if (c1 >= 1.0f) c1 -= 1.0f;
        m1 += im1; if (m1 >= 1.0f) m1 -= 1.0f;
        c2 += ic2; if (c2 >= 1.0f) c2 -= 1.0f;
        m2 += im2; if (m2 >= 1.0f) m2 -= 1.0f;
        float y1 = sin1(wrap1(c1 + idx * sin1(m1) + 1.0f));
        float y2 = sin1(wrap1(c2 + idx * sin1(m2) + 1.0f));
        float x = y1 * y2 * 0.7f + (y1 + y2) * 0.15f + noise(&r) * nzl;
        o[i] = svf_hp(&h, x) * (a + s);
        a *= ak; s *= sk;
    }
    v->ph[0] = c1; v->ph[1] = m1; v->ph[2] = c2; v->ph[3] = m2;
    v->e[0] = a; v->e[1] = s; v->f[0] = h; v->rng = r;
}

static void r_cymbal(DVoice *v, float *restrict o, int n)
{
    float p[6], in[6];
    for (int j = 0; j < 6; j++) { p[j] = v->ph[j]; in[j] = v->inc[j]; }
    float lo_a = v->c[0], hi_a = v->c[1], nzl = v->c[2];
    float a = v->e[0], ak = v->k[0], l = v->e[1], lk = v->k[1], s = v->e[2], sk = v->k[2], att = v->att, atk = v->att_k;
    Svf b = v->f[0], h = v->f[1];
    uint32_t r = v->rng;
    for (int i = 0; i < n; i++) {
        float x = 0.0f;
        for (int j = 0; j < 6; j++) {
            p[j] += in[j];
            if (p[j] >= 1.0f) p[j] -= 1.0f;
            x += p[j] < 0.5f ? 1.0f : -1.0f;
        }
        x = x * (0.65f / 6.0f) + nzl * noise(&r);
        float y = svf_bp(&b, x) * lo_a * l + svf_bp(&h, x) * hi_a * (a + s);
        o[i] = y * (1.0f - att);
        a *= ak; l *= lk; s *= sk; att *= atk;
    }
    for (int j = 0; j < 6; j++) v->ph[j] = p[j];
    v->e[0] = a; v->e[1] = l; v->e[2] = s; v->att = att; v->f[0] = b; v->f[1] = h; v->rng = r;
}

static void r_ride(DVoice *v, float *restrict o, int n)
{
    float p[8], in[8];
    for (int j = 0; j < 8; j++) { p[j] = v->ph[j]; in[j] = v->inc[j]; }
    float b6 = v->amp[6], b7 = v->amp[7], nzl = v->c[0];
    float a = v->e[0], ak = v->k[0], bl = v->e[1], blk = v->k[1], st = v->e[2], stk = v->k[2];
    Svf b = v->f[0], h = v->f[1];
    uint32_t r = v->rng;
    for (int i = 0; i < n; i++) {
        float x = 0.0f;
        for (int j = 0; j < 6; j++) {
            p[j] += in[j];
            if (p[j] >= 1.0f) p[j] -= 1.0f;
            x += p[j] < 0.5f ? 1.0f : -1.0f;
        }
        p[6] += in[6]; if (p[6] >= 1.0f) p[6] -= 1.0f;
        p[7] += in[7]; if (p[7] >= 1.0f) p[7] -= 1.0f;
        float nz = noise(&r);
        x = x * (1.0f / 6.0f) + nzl * nz;
        o[i] = svf_bp(&b, x) * a * 1.3f + (sin1(p[6]) * b6 + sin1(p[7]) * b7) * bl + svf_hp(&h, nz) * st;
        a *= ak; bl *= blk; st *= stk;
    }
    for (int j = 0; j < 8; j++) v->ph[j] = p[j];
    v->e[0] = a; v->e[1] = bl; v->e[2] = st; v->f[0] = b; v->f[1] = h; v->rng = r;
}

static void r_tom(DVoice *v, float *restrict o, int n)
{
    float p0 = v->ph[0], p1 = v->ph[1], i0 = v->inc[0], i1 = v->inc[1], pe = v->pe, pk = v->pk, pa = v->pa;
    float a = v->e[0], ak = v->k[0], b = v->e[1], bk = v->k[1], hd = v->e[2], hk = v->k[2], lc = v->c[0], lp = v->z[0];
    uint32_t r = v->rng;
    for (int i = 0; i < n; i++) {
        float m = 1.0f + pa * pe;
        pe *= pk;
        p0 += i0 * m; if (p0 >= 1.0f) p0 -= 1.0f;
        p1 += i1 * m; if (p1 >= 1.0f) p1 -= 1.0f;
        lp += lc * (noise(&r) - lp);
        o[i] = sin1(p0) * a + sin1(p1) * b + lp * hd * 1.5f;
        a *= ak; b *= bk; hd *= hk;
    }
    v->ph[0] = p0; v->ph[1] = p1; v->pe = pe; v->z[0] = lp; v->rng = r;
    v->e[0] = a; v->e[1] = b; v->e[2] = hd;
}

static void r_tom_electro(DVoice *v, float *restrict o, int n)
{
    float ph = v->ph[0], inc = v->inc[0], pe = v->pe, pk = v->pk, pa = v->pa, tri = v->c[0], lc = v->c[1], lp = v->z[0];
    float a = v->e[0], ak = v->k[0], nz = v->e[2], nk = v->k[2];
    uint32_t r = v->rng;
    for (int i = 0; i < n; i++) {
        ph += inc * (1.0f + pa * pe);
        if (ph >= 1.0f) ph -= 1.0f;
        pe *= pk;
        float t = ph < 0.25f ? 4.0f * ph : (ph < 0.75f ? 2.0f - 4.0f * ph : 4.0f * ph - 4.0f);
        lp += lc * (noise(&r) - lp);
        float x = (sin1(ph) * (1.0f - tri) + t * tri) * a + lp * nz * 1.5f;
        o[i] = softclip(1.3f * x) * 0.85f;
        a *= ak; nz *= nk;
    }
    v->ph[0] = ph; v->pe = pe; v->z[0] = lp; v->rng = r; v->e[0] = a; v->e[2] = nz;
}

static void r_membrane(DVoice *v, float *restrict o, int n)
{
    float p0 = v->ph[0], p1 = v->ph[1], p2 = v->ph[2], i0 = v->inc[0], i1 = v->inc[1], i2 = v->inc[2];
    float a1 = v->amp[1], a2 = v->amp[2], pe = v->pe, pk = v->pk, pa = v->pa;
    float a = v->e[0], ak = v->k[0], b = v->e[1], bk = v->k[1], sl = v->e[2], sk = v->k[2];
    Svf f = v->f[0];
    uint32_t r = v->rng;
    for (int i = 0; i < n; i++) {
        float m = 1.0f + pa * pe;
        pe *= pk;
        p0 += i0 * m; if (p0 >= 1.0f) p0 -= 1.0f;
        p1 += i1 * m; if (p1 >= 1.0f) p1 -= 1.0f;
        p2 += i2 * m; if (p2 >= 1.0f) p2 -= 1.0f;
        o[i] = sin1(p0) * a + (sin1(p1) * a1 + sin1(p2) * a2) * b + svf_bp(&f, noise(&r)) * sl * 1.5f;
        a *= ak; b *= bk; sl *= sk;
    }
    v->ph[0] = p0; v->ph[1] = p1; v->ph[2] = p2; v->pe = pe; v->f[0] = f; v->rng = r;
    v->e[0] = a; v->e[1] = b; v->e[2] = sl;
}

static void r_tabla(DVoice *v, float *restrict o, int n)
{
    float ph = v->ph[0], inc = v->inc[0], pe = v->pe, pk = v->pk, pa = v->pa, lp = v->z[0];
    float h0 = v->amp[0], h1 = v->amp[1], h2 = v->amp[2], h3 = v->amp[3], h4 = v->amp[4];
    float a = v->e[0], ak = v->k[0], b = v->e[1], bk = v->k[1], fg = v->e[2], fk = v->k[2];
    uint32_t r = v->rng;
    for (int i = 0; i < n; i++) {
        ph += inc * (1.0f + pa * pe);
        if (ph >= 1.0f) ph -= 1.0f;
        pe *= pk;
        float harm = h1 * sin1(wrap1(2.0f * ph)) + h2 * sin1(wrap1(3.0f * ph)) + h3 * sin1(wrap1(4.0f * ph)) + h4 * sin1(wrap1(5.0f * ph));
        float nz = noise(&r);
        lp += 0.4f * (nz - lp);
        o[i] = h0 * sin1(ph) * a + harm * b + (nz - lp) * fg;
        a *= ak; b *= bk; fg *= fk;
    }
    v->ph[0] = ph; v->pe = pe; v->z[0] = lp; v->rng = r;
    v->e[0] = a; v->e[1] = b; v->e[2] = fg;
}

static void r_cowbell(DVoice *v, float *restrict o, int n)
{
    float p0 = v->ph[0], p1 = v->ph[1], i0 = v->inc[0], i1 = v->inc[1];
    float e0 = v->e[0], k0 = v->k[0], e1 = v->e[1], k1 = v->k[1];
    Svf f = v->f[0];
    for (int i = 0; i < n; i++) {
        p0 += i0; if (p0 >= 1.0f) p0 -= 1.0f;
        p1 += i1; if (p1 >= 1.0f) p1 -= 1.0f;
        float sq = (square_blep(p0, i0) + square_blep(p1, i1)) * 0.5f;
        o[i] = (svf_bp(&f, sq) * 1.4f + sq * 0.15f) * (e0 + e1);
        e0 *= k0; e1 *= k1;
    }
    v->ph[0] = p0; v->ph[1] = p1; v->e[0] = e0; v->e[1] = e1; v->f[0] = f;
}

static void r_ping(DVoice *v, float *restrict o, int n)       /* claves e woodblock */
{
    float p0 = v->ph[0], p1 = v->ph[1], i0 = v->inc[0], i1 = v->inc[1], a1 = v->amp[1], pe = v->pe, pk = v->pk, pa = v->pa;
    float a = v->e[0], ak = v->k[0], b = v->e[1], bk = v->k[1], cl = v->e[2], ck = v->k[2], lc = v->c[0], lp = v->z[0];
    uint32_t r = v->rng;
    for (int i = 0; i < n; i++) {
        float m = 1.0f + pa * pe;
        pe *= pk;
        p0 += i0 * m; if (p0 >= 1.0f) p0 -= 1.0f;
        p1 += i1 * m; if (p1 >= 1.0f) p1 -= 1.0f;
        lp += lc * (noise(&r) - lp);
        o[i] = sin1(p0) * a + sin1(p1) * a1 * b + lp * cl * 2.0f;
        a *= ak; b *= bk; cl *= ck;
    }
    v->ph[0] = p0; v->ph[1] = p1; v->pe = pe; v->z[0] = lp; v->rng = r;
    v->e[0] = a; v->e[1] = b; v->e[2] = cl;
}

static void r_shaker(DVoice *v, float *restrict o, int n)
{
    float att = v->att, atk = v->att_k, a = v->e[0], ak = v->k[0], gt = v->z[0], gc = v->z[1];
    int gn = v->cnt[0];
    Svf f = v->f[0];
    uint32_t r = v->rng;
    for (int i = 0; i < n; i++) {
        float gr = grain(v, &r, &gt, &gc, &gn);
        o[i] = svf_bp(&f, noise(&r)) * gr * a * (1.0f - att);
        a *= ak; att *= atk;
    }
    v->att = att; v->e[0] = a; v->z[0] = gt; v->z[1] = gc; v->cnt[0] = gn; v->f[0] = f; v->rng = r;
}

static void r_tambourine(DVoice *v, float *restrict o, int n)
{
    float p[5], in[5];
    for (int j = 0; j < 5; j++) { p[j] = v->ph[j]; in[j] = v->inc[j]; }
    float skin = v->amp[4];
    float b = v->e[0], bk = v->k[0], tl = v->e[1], tk = v->k[1], sk = v->e[2], skk = v->k[2];
    int next = v->cnt[0], left = v->cnt[1];
    Svf f = v->f[0], h = v->f[1];
    uint32_t r = v->rng;
    for (int i = 0; i < n; i++) {
        if (left > 0 && --next <= 0) {             /* i sonagli ribattono */
            b = 0.8f;
            next = 240 + (int)(rnd01(&r) * 200.0f);
            if (--left == 0) tl = 0.7f;
        }
        float x = 0.0f;
        for (int j = 0; j < 4; j++) {
            p[j] += in[j];
            if (p[j] >= 1.0f) p[j] -= 1.0f;
            x += p[j] < 0.5f ? 0.25f : -0.25f;
        }
        p[4] += in[4]; if (p[4] >= 1.0f) p[4] -= 1.0f;
        x = x * 0.5f + noise(&r) * 0.5f;
        float env = b > tl ? b : tl;
        o[i] = svf_hp(&h, svf_bp(&f, x)) * env * 1.6f + sin1(p[4]) * skin * sk;
        b *= bk; tl *= tk; sk *= skk;
    }
    for (int j = 0; j < 5; j++) v->ph[j] = p[j];
    v->e[0] = b; v->e[1] = tl; v->e[2] = sk; v->cnt[0] = next; v->cnt[1] = left; v->f[0] = f; v->f[1] = h; v->rng = r;
    if (left > 0 && v->e[0] < VOICE_END) v->e[0] = VOICE_END;
}

static void r_triangle(DVoice *v, float *restrict o, int n)
{
    float p0 = v->ph[0], p1 = v->ph[1], p2 = v->ph[2], p3 = v->ph[3];
    float i0 = v->inc[0], i1 = v->inc[1], i2 = v->inc[2], i3 = v->inc[3];
    float a0 = v->amp[0], a1 = v->amp[1], a2 = v->amp[2], a3 = v->amp[3];
    float a = v->e[0], ak = v->k[0], b = v->e[1], bk = v->k[1], cl = v->e[2], ck = v->k[2], lp = v->z[0];
    uint32_t r = v->rng;
    for (int i = 0; i < n; i++) {
        p0 += i0; if (p0 >= 1.0f) p0 -= 1.0f;
        p1 += i1; if (p1 >= 1.0f) p1 -= 1.0f;
        p2 += i2; if (p2 >= 1.0f) p2 -= 1.0f;
        p3 += i3; if (p3 >= 1.0f) p3 -= 1.0f;
        float nz = noise(&r);
        lp += 0.5f * (nz - lp);
        o[i] = (sin1(p0) * a0 + sin1(p1) * a1) * a + (sin1(p2) * a2 + sin1(p3) * a3) * b + (nz - lp) * cl;
        a *= ak; b *= bk; cl *= ck;
    }
    v->ph[0] = p0; v->ph[1] = p1; v->ph[2] = p2; v->ph[3] = p3; v->z[0] = lp; v->rng = r;
    v->e[0] = a; v->e[1] = b; v->e[2] = cl;
}

/* livello a 16 gradini come il registro di volume del NES */
static inline float chip_step(float e) { return (float)(int)(e * 15.0f + 0.5f) * (1.0f / 15.0f); }

static void r_chip_kick(DVoice *v, float *restrict o, int n)
{
    float ph = v->ph[0], inc = v->z[0], lev = v->z[1], fl = v->c[2], noise_amt = v->c[3], sq = v->c[4] - 1.0f;
    float pe = v->pe, e0 = v->e[0];
    int fc = v->cnt[0], nleft = v->cnt[1];
    uint32_t l = v->lfsr;
    for (int i = 0; i < n; i++) {
        if (--fc <= 0) {                           /* nuovo fotogramma: altezza e volume a scatti */
            fc = (int)fl;
            inc = v->c[1] + (v->c[0] - v->c[1]) * pe;
            pe *= v->pk;
            lev = chip_step(e0);
            e0 *= v->k[0];
            if (lev <= 0.0f) e0 = 0.0f;
        }
        ph += inc; if (ph >= 1.0f) ph -= 1.0f;
        int idx = (int)(ph * 32.0f) & 31;
        float tri = (float)(idx < 16 ? 15 - idx : idx - 16) * (1.0f / 7.5f) - 1.0f;
        float x = tri + sq * (ph < 0.5f ? 0.5f : -0.5f);
        if (nleft > 0) { nleft--; l = lfsr_step(l, 0); x += (l & 1u ? noise_amt : -noise_amt); }
        o[i] = x * lev * 0.8f;
    }
    v->ph[0] = ph; v->z[0] = inc; v->z[1] = lev; v->pe = pe; v->e[0] = e0; v->cnt[0] = fc; v->cnt[1] = nleft; v->lfsr = l;
}

static void r_chip_snare(DVoice *v, float *restrict o, int n)
{
    float rate = v->c[0], acc = v->z[2], lev = v->z[1], fl = v->c[2];
    float ph = v->ph[0], inc = v->inc[0], pe = v->pe, e0 = v->e[0], e1 = v->e[1], blev = v->z[3];
    float dx = v->amp[0], dy = v->amp[1];
    int fc = v->cnt[0];
    uint32_t l = v->lfsr;
    for (int i = 0; i < n; i++) {
        if (--fc <= 0) {
            fc = (int)fl;
            lev = chip_step(e0);
            blev = chip_step(e1);
            e0 *= v->k[0]; e1 *= v->k[1]; pe *= v->pk;
            if (lev <= 0.0f) e0 = 0.0f;
            if (blev <= 0.0f) e1 = 0.0f;
        }
        acc += rate;
        while (acc >= 1.0f) { acc -= 1.0f; l = lfsr_step(l, 0); }
        ph += inc * (1.0f + 2.0f * pe); if (ph >= 1.0f) ph -= 1.0f;
        float x = (l & 1u ? 0.7f : -0.7f) * lev + (ph < 0.5f ? 0.6f : -0.6f) * blev;
        dy = x - dx + 0.998f * dy;                 /* via la continua dei gradini */
        dx = x;
        o[i] = dy;
    }
    v->z[2] = acc; v->z[1] = lev; v->z[3] = blev; v->ph[0] = ph; v->pe = pe; v->e[0] = e0; v->e[1] = e1;
    v->cnt[0] = fc; v->lfsr = l; v->amp[0] = dx; v->amp[1] = dy;
}

static void r_chip_hat(DVoice *v, float *restrict o, int n)
{
    float rate = v->c[0], acc = v->z[2], lev = v->z[1], fl = v->c[2], e0 = v->e[0], dx = v->z[0], dy = v->z[3];
    int fc = v->cnt[0], sm = v->cnt[2];
    uint32_t l = v->lfsr;
    for (int i = 0; i < n; i++) {
        if (--fc <= 0) {
            fc = (int)fl;
            lev = chip_step(e0);
            e0 *= v->k[0];
            if (lev <= 0.0f) e0 = 0.0f;
        }
        acc += rate;
        while (acc >= 1.0f) { acc -= 1.0f; l = lfsr_step(l, sm); }
        float x = (l & 1u ? 0.6f : -0.6f) * lev;
        dy = x - dx + 0.998f * dy;                 /* il modo corto ha un po' di continua */
        dx = x;
        o[i] = dy;
    }
    v->z[2] = acc; v->z[1] = lev; v->e[0] = e0; v->cnt[0] = fc; v->lfsr = l; v->z[0] = dx; v->z[3] = dy;
}

static void r_zap(DVoice *v, float *restrict o, int n)
{
    float ph = v->ph[0], hi = v->c[0], lo = v->c[1], sh = v->c[2], nm = v->c[3], pe = v->pe, pk = v->pk;
    float a = v->e[0], ak = v->k[0];
    for (int i = 0; i < n; i++) {
        ph += lo + (hi - lo) * pe;
        if (ph >= 1.0f) ph -= 1.0f;
        pe *= pk;
        o[i] = softclip(sin1(ph) * sh) * nm * a;
        a *= ak;
    }
    v->ph[0] = ph; v->pe = pe; v->e[0] = a;
}

static void r_glitch(DVoice *v, float *restrict o, int n)
{
    float ph = v->ph[0], inc = v->inc[0], held = v->z[0], lev = v->c[1], a = v->e[0], ak = v->k[0];
    int left = v->cnt[0], wave = v->cnt[1], dec = v->cnt[2], decn = (int)v->c[0];
    uint32_t r = v->rng;
    for (int i = 0; i < n; i++) {
        if (--left <= 0) {                         /* frammento nuovo: lunghezza, altezza e forma a caso */
            left = (int)((0.0008f + 0.006f * rnd01(&r)) * v->c[3]);
            inc = clampf(80.0f * v->c[2] * exp2f(rnd01(&r) * 5.5f) / v->c[3], 0.0f, 0.45f);
            wave = (int)(rnd01(&r) * 4.99f);
        }
        ph += inc; if (ph >= 1.0f) ph -= 1.0f;
        float x;
        switch (wave) {
        case 0: x = ph < 0.5f ? 0.8f : -0.8f; break;
        case 1: x = 2.0f * ph - 1.0f; break;
        case 2: x = noise(&r); break;
        case 3: x = sin1(ph); break;
        default: x = 0.0f;
        }
        if (--dec <= 0) {                          /* meno campioni e meno bit */
            dec = decn;
            float q = x * lev;
            held = (float)(int)(q + (q >= 0.0f ? 0.5f : -0.5f)) / lev;
        }
        o[i] = held * a;
        a *= ak;
    }
    v->ph[0] = ph; v->inc[0] = inc; v->z[0] = held; v->e[0] = a; v->cnt[0] = left; v->cnt[1] = wave; v->cnt[2] = dec;
    v->rng = r;
}

static void r_sweep(DVoice *v, float *restrict o, int n)
{
    float pe = v->pe, pk = v->pk, att = v->att, atk = v->att_k, a = v->e[0], ak = v->k[0];
    int ctl = v->cnt[0];
    Svf f = v->f[0];
    uint32_t r = v->rng;
    for (int i = 0; i < n; i++) {
        if (--ctl <= 0) {                          /* il filtro si muove: coefficienti ogni 16 campioni */
            ctl = 16;
            float s = v->cnt[1] ? 1.0f - pe : pe;
            svf_set(&f, exp2f(v->c[0] + v->c[1] * s), 0.7f, v->c[2]);
        }
        o[i] = svf_bp(&f, noise(&r)) * a * (1.0f - att) * 2.0f;
        a *= ak; att *= atk;
        pe *= pk;
    }
    v->pe = pe; v->att = att; v->e[0] = a; v->cnt[0] = ctl; v->f[0] = f; v->rng = r;
}

/* Suona una voce di batteria per n campioni e la somma in out (con saturazione, volume e dissolvenza). */
static void drum_voice_render(DVoice *v, float *restrict out, float *restrict tmp, int n, int max_t)
{
    switch (v->model) {
    case DM_KICK_808: r_kick808(v, tmp, n); break;
    case DM_KICK_909: r_kick909(v, tmp, n); break;
    case DM_KICK_ACOUSTIC: r_kick_acoustic(v, tmp, n); break;
    case DM_KICK_FM: r_kick_fm(v, tmp, n); break;
    case DM_KICK_HARD: r_kick_hard(v, tmp, n); break;
    case DM_SNARE_808: r_snare808(v, tmp, n); break;
    case DM_SNARE_909: r_snare909(v, tmp, n); break;
    case DM_SNARE_ACOUSTIC: r_snare_acoustic(v, tmp, n); break;
    case DM_SNARE_BRUSH: r_brush(v, tmp, n); break;
    case DM_RIM: r_rim(v, tmp, n); break;
    case DM_CLAP: r_clap(v, tmp, n); break;
    case DM_SNAP: r_snap(v, tmp, n); break;
    case DM_HAT_CLOSED: case DM_HAT_OPEN: r_hat(v, tmp, n); break;
    case DM_HAT_909: r_hat909(v, tmp, n); break;
    case DM_CYMBAL: r_cymbal(v, tmp, n); break;
    case DM_RIDE: r_ride(v, tmp, n); break;
    case DM_TOM: r_tom(v, tmp, n); break;
    case DM_TOM_ELECTRO: r_tom_electro(v, tmp, n); break;
    case DM_CONGA: case DM_BONGO: r_membrane(v, tmp, n); break;
    case DM_TABLA: r_tabla(v, tmp, n); break;
    case DM_COWBELL: r_cowbell(v, tmp, n); break;
    case DM_CLAVE: case DM_WOODBLOCK: r_ping(v, tmp, n); break;
    case DM_SHAKER: r_shaker(v, tmp, n); break;
    case DM_TAMBOURINE: r_tambourine(v, tmp, n); break;
    case DM_TRIANGLE: r_triangle(v, tmp, n); break;
    case DM_CHIP_KICK: r_chip_kick(v, tmp, n); break;
    case DM_CHIP_SNARE: r_chip_snare(v, tmp, n); break;
    case DM_CHIP_HAT: r_chip_hat(v, tmp, n); break;
    case DM_ZAP: r_zap(v, tmp, n); break;
    case DM_GLITCH: r_glitch(v, tmp, n); break;
    default: r_sweep(v, tmp, n); break;
    }
    v->t += n;
    float g = v->gain;
    if (v->drive_g > 0.0f) {                 /* saturazione dopo il volume del modello: tutti i modelli la sentono uguale */
        float dg = v->drive_g * g, dn = v->drive_n;
        for (int i = 0; i < n; i++) tmp[i] = softclip(tmp[i] * dg) * dn;
        g = 1.0f;
    }
    if (v->fade_k > 0.0f) {
        float f = v->fade, fk = v->fade_k;
        for (int i = 0; i < n; i++) {
            out[i] += tmp[i] * g * f;
            f -= fk;
            if (f < 0.0f) f = 0.0f;
        }
        v->fade = f;
        if (f <= 0.0f) v->on = 0;
    } else {
        for (int i = 0; i < n; i++) out[i] += tmp[i] * g;
    }
    if ((v->e[0] < VOICE_END && v->e[1] < VOICE_END && v->e[2] < VOICE_END && v->e[3] < VOICE_END) || v->t > max_t)
        v->on = 0;
}

/* ------------------------------------------------------------------ voci melodiche */

typedef struct {
    int on, gate, slide;        /* slide: la nota sta scivolando verso la nuova altezza */
    int slide_next;             /* il passo appena suonato chiede il legato col prossimo */
    float note;                 /* nota MIDI (senza tune) */
    float freq, ph;
    float s1, s2, s3, s4;       /* stati del ladder */
    float G, B;                 /* coefficienti del ladder (ogni 8 campioni) */
    int ctl;
    float fenv, fk;             /* inviluppo del filtro */
    float amp, acc, acc_t, acc_cap, vel;
    double off_t;               /* fine del gate dal sequencer (sedicesimi), < 0 nessuna */
    int64_t off_clk;            /* fine del gate dell'anteprima (campioni), < 0 nessuna */
    float lock_tune, lock_cut;
    int held[8], held_n;        /* tasti tenuti dal vivo (l'ultimo vince) */
} AcidVoice;

typedef struct {
    int on, gate, attack, note;
    float vel, ph, mph, vac, dc_x, dc_y;
    Svf lpg;
    int ctl;
    double off_t;
    int64_t off_clk;
    uint32_t age;
    int live;
    float lock_tune, lock_fold, lock_dec;
} FoldVoice;

/* ------------------------------------------------------------------ stato */

enum { EV_HIT, EV_NOTE_ON, EV_NOTE_OFF, EV_ALL_OFF, EV_PLAY, EV_SELECT, EV_CHAIN };
typedef struct { int type, a, b; float f; } Event;

typedef struct {
    int idx;                    /* prossimo passo */
    double p;                   /* sua posizione dritta (sedicesimi) */
    double t;                   /* suo tempo con swing e micro */
    double sf;                  /* sedicesimi per passo */
    int rat_left, rat_idx;      /* ratchet in corso */
    double rat_t, rat_dt;
    float rat_vel;
} TrackSeq;

struct Gb {
    float sr, inv_sr;
    GbDrum drum[GB_DRUMS];
    GbAcid acid;
    GbFold fold;
    GbMix mix[GB_TRACKS];
    GbMaster master;
    GbPattern pat[GB_PATTERNS];

    Event q[QUEUE_SIZE];
    int q_head, q_tail;
    int chain_buf[CHAIN_SLOTS][MAX_CHAIN], chain_buf_n[CHAIN_SLOTS], chain_slot;

    /* voci */
    DVoice dv[GB_DRUMS][2];     /* [0] quella che suona, [1] quella tagliata che sfuma */
    AcidVoice ac;
    FoldVoice fv[GB_FOLD_VOICES];
    uint32_t age, rng, seed;
    int max_t;

    /* trasporto e sequencer */
    int play_ui, playing;
    double T, swing;
    int cur_pat, sel_pat;
    int chain[MAX_CHAIN], chain_n, chain_pos;
    double pat_start, giro_end, bound_t;   /* inizio e fine del giro in corso (sedicesimi), fine con lo swing */
    uint32_t pat_loop;
    int bar_count;
    TrackSeq ts[GB_TRACKS];
    int shown[GB_TRACKS];
    uint32_t flash[GB_TRACKS];
    int fill;
    float repeat_req;
    uint32_t clock_n;
    void (*hook)(void *, int);
    void *hook_user;
    int64_t clk;

    /* mixer */
    float gcur[GB_TRACKS][4];   /* guadagni attuali: L, R, mandata delay, mandata riverbero */
    float tlev[GB_TRACKS];
    float tb[GB_TRACKS][BLOCK];
    float tmp[BLOCK];
    float bus_l[BLOCK], bus_r[BLOCK], kick_l[BLOCK], kick_r[BLOCK], snd_d[BLOCK], snd_v[BLOCK];
    float glide_k;

    /* delay ping-pong */
    float *dl, *dr;
    int dlen, dpos, d_quiet;
    float dtime, dfl, dfr, dhl, dhr;

    /* riverbero: diffusori + rete di ritardi a retroazione (FDN 8) */
    float *fdn[FDN], *dif[DIFF];
    int fdn_len[FDN], fdn_pos[FDN], dif_len[DIFF], dif_pos[DIFF], r_quiet;
    float fdn_lp[FDN], fdn_g[FDN], r_size, r_time;

    /* bus master */
    float pump_env, pump_s;
    float dc_k, dc_xl, dc_yl, dc_xr, dc_yr;
    float c_env, c_gain, c_tgt;
    int c_ctl;
    Svf dj_l, dj_r;
    float dj_s;
    int dj_ctl, dj_mode;
    float lf_ph, lf_l, lf_r;
    float *tape_l, *tape_r;
    int tape_pos;
    float tape_s, wow_ph, flut_ph, hf_l, hf_r;
    float vol_s;

    /* stutter */
    float *hist_l, *hist_r;
    int hist_len, hist_pos, rep_on, rep_L, rep_i, rep_freeze;
    float rep_x;

    /* uscita per l'interfaccia */
    float scope[GB_SCOPE];
    int scope_pos;
    float peak, cpu;

    /* registrazione */
    int16_t *rec;
    uint32_t rec_w, rec_r, rec_drop;
    int rec_on;
};

static const double SPEED_SF[4] = { 1.0, 0.5, 2.0, 4.0 / 3.0 };

/* ------------------------------------------------------------------ colpi e note */

static void drum_cut(Gb *g, int tr, float ms)    /* la voce che suona passa a sfumare */
{
    DVoice *cur = &g->dv[tr][0];
    if (!cur->on) return;
    g->dv[tr][1] = *cur;
    g->dv[tr][1].fade_k = 1.0f / (ms * 0.001f * g->sr);
    cur->on = 0;
}

static void drum_trigger(Gb *g, int tr, float vel, const GbStep *st)
{
    GbDrum d = g->drum[tr];
    if (st) {
        if (st->lock_tune != GB_LOCK_NONE) d.tune = (float)st->lock_tune;
        if (st->lock_decay != GB_LOCK_NONE) d.decay = clampf(st->lock_decay * 0.01f, 0.0f, 1.0f);
        if (st->lock_tone != GB_LOCK_NONE) d.tone = clampf(st->lock_tone * 0.01f, 0.0f, 1.0f);
    }
    drum_cut(g, tr, 1.2f);
    if (d.choke) {
        for (int u = 0; u < GB_DRUMS; u++)
            if (u != tr && g->drum[u].choke == d.choke) drum_cut(g, u, 4.0f);
    }
    drum_start(&g->dv[tr][0], &d, vel, g->sr, ++g->seed);
    g->flash[tr]++;
    if (tr == 0) g->pump_env = 1.0f;
}

static float midi_hz(float note) { return 440.0f * exp2f((note - 69.0f) / 12.0f); }

/* Acido: legato = scivola senza nuovo attacco (slide della 303). */
static void acid_note(Gb *g, float note, float vel, int accent, int legato, const GbStep *st)
{
    AcidVoice *a = &g->ac;
    a->lock_tune = (st && st->lock_tune != GB_LOCK_NONE) ? (float)st->lock_tune : LOCK_OFF;
    a->lock_cut = (st && st->lock_tone != GB_LOCK_NONE) ? clampf(st->lock_tone * 0.01f, 0.0f, 1.0f) : LOCK_OFF;
    float decay = (st && st->lock_decay != GB_LOCK_NONE) ? clampf(st->lock_decay * 0.01f, 0.0f, 1.0f) : clampf(g->acid.decay, 0.0f, 1.0f);
    a->note = note;
    a->vel = 0.55f + 0.45f * clampf(vel, 0.0f, 1.0f);
    if (legato && a->gate && a->on) {
        a->slide = 1;
        a->acc = (float)accent;
        return;
    }
    if (!a->on) {
        a->s1 = a->s2 = a->s3 = a->s4 = 0.0f;
        a->amp = 0.0f;
        a->ph = 0.0f;
    }
    a->slide = 0;
    a->on = 1;
    a->gate = 1;
    a->fenv = 1.0f;
    a->acc = (float)accent;
    float tau = 0.05f + 0.6f * decay * decay;
    if (accent) {
        if (tau > 0.07f) tau = 0.07f;              /* la 303 accorcia l'inviluppo sulle note accentate */
        a->acc_t += (1.0f - a->acc_t) * 0.6f;      /* ...e il condensatore dell'accento si carica */
    }
    a->fk = tauk(tau, g->sr);
    a->ctl = 0;
}

static void acid_gate_off(Gb *g)
{
    g->ac.gate = 0;
    g->ac.off_t = -1.0;
    g->ac.off_clk = -1;
}

static FoldVoice *fold_alloc(Gb *g)
{
    FoldVoice *pick = NULL;
    for (int i = 0; i < GB_FOLD_VOICES && !pick; i++) if (!g->fv[i].on) pick = &g->fv[i];
    if (!pick) {                                    /* ruba: prima la piu' vecchia gia' rilasciata */
        for (int i = 0; i < GB_FOLD_VOICES; i++)
            if (!g->fv[i].gate && (!pick || g->fv[i].age < pick->age)) pick = &g->fv[i];
    }
    if (!pick) {
        pick = &g->fv[0];
        for (int i = 1; i < GB_FOLD_VOICES; i++) if (g->fv[i].age < pick->age) pick = &g->fv[i];
    }
    return pick;
}

static FoldVoice *fold_note(Gb *g, int note, float vel, const GbStep *st)
{
    FoldVoice *v = fold_alloc(g);
    if (!v->on) {
        v->ph = v->mph = 0.0f;
        v->vac = 0.0f;
        v->lpg.ic1 = v->lpg.ic2 = 0.0f;
        v->dc_x = v->dc_y = 0.0f;
    }
    v->on = 1;
    v->gate = 1;
    v->attack = 1;
    v->note = note;
    v->vel = clampf(vel, 0.0f, 1.0f);
    v->age = ++g->age;
    v->ctl = 0;
    v->live = 0;
    v->off_t = -1.0;
    v->off_clk = -1;
    v->lock_tune = (st && st->lock_tune != GB_LOCK_NONE) ? (float)st->lock_tune : LOCK_OFF;
    v->lock_fold = (st && st->lock_tone != GB_LOCK_NONE) ? clampf(st->lock_tone * 0.01f, 0.0f, 1.0f) : LOCK_OFF;
    v->lock_dec = (st && st->lock_decay != GB_LOCK_NONE) ? clampf(st->lock_decay * 0.01f, 0.0f, 1.0f) : LOCK_OFF;
    return v;
}

/* Anteprima di una traccia melodica (gb_hit): una nota breve. */
static void melodic_preview(Gb *g, int tr, float vel)
{
    int64_t off = g->clk + (int64_t)(0.22f * g->sr);
    if (tr == GB_ACID) {
        acid_note(g, 48.0f, vel, vel > 0.85f, 0, NULL);
        g->ac.off_clk = off;
        g->ac.off_t = -1.0;
    } else {
        FoldVoice *v = fold_note(g, 48, vel, NULL);
        v->off_clk = off;
    }
    g->flash[tr]++;
}

static void acid_live_on(Gb *g, int note, float vel)
{
    AcidVoice *a = &g->ac;
    int j = 0;
    for (int i = 0; i < a->held_n; i++) if (a->held[i] != note) a->held[j++] = a->held[i];
    a->held_n = j;
    if (a->held_n == 8) { memmove(a->held, a->held + 1, 7 * sizeof(int)); a->held_n = 7; }
    a->held[a->held_n++] = note;
    acid_note(g, (float)note, vel, vel > 0.85f, a->held_n > 1, NULL);
    a->off_t = -1.0;
    a->off_clk = -1;
    g->flash[GB_ACID]++;
}

static void acid_live_off(Gb *g, int note)
{
    AcidVoice *a = &g->ac;
    int j = 0;
    for (int i = 0; i < a->held_n; i++) if (a->held[i] != note) a->held[j++] = a->held[i];
    int was_last = a->held_n > 0 && j < a->held_n && (int)a->note == note;
    a->held_n = j;
    if (!was_last) return;
    if (a->held_n > 0) { a->note = (float)a->held[a->held_n - 1]; a->slide = 1; }
    else acid_gate_off(g);
}

/* ------------------------------------------------------------------ sequencer */

static inline double warp(double p, double sw)     /* swing: sposta i sedicesimi dispari */
{
    double k = 2.0 * floor(p * 0.5);
    double f = p - k;
    return p + sw * (f <= 1.0 ? f : 2.0 - f);
}

static inline double step_time(const Gb *g, double p, int speed) { return speed == 3 ? p : warp(p, g->swing); }

static double giro_len(const GbPattern *p)
{
    double G = 0.0;
    for (int t = 0; t < GB_TRACKS; t++) {
        const GbTrackPattern *tp = &p->track[t];
        int len = clampi(tp->length, 1, GB_MAX_STEPS), any = 0;
        for (int i = 0; i < len && !any; i++) any = tp->step[i].on;
        double d = len * SPEED_SF[tp->speed & 3];
        if (any && d > G) G = d;
    }
    return G > 0.0 ? G : 16.0;
}

static void track_schedule(Gb *g, int tr)
{
    TrackSeq *ts = &g->ts[tr];
    const GbTrackPattern *tp = &g->pat[g->cur_pat].track[tr];
    int len = clampi(tp->length, 1, GB_MAX_STEPS);
    if (ts->idx >= len) ts->idx = 0;
    int sp = tp->speed & 3;
    ts->sf = SPEED_SF[sp];
    ts->t = step_time(g, ts->p, sp) + clampi(tp->step[ts->idx].micro, -50, 50) * 0.01 * ts->sf;
}

static void tracks_reset(Gb *g, double at)
{
    for (int t = 0; t < GB_TRACKS; t++) {
        g->ts[t].idx = 0;
        g->ts[t].p = at;
        track_schedule(g, t);
    }
}

static int cond_ok(const Gb *g, int cond)
{
    switch (cond) {
    case COND_FILL: return g->fill;
    case COND_NOT_FILL: return !g->fill;
    case COND_1_2: return (g->pat_loop & 1u) == 0;
    case COND_2_2: return (g->pat_loop & 1u) == 1;
    case COND_1_4: return (g->pat_loop & 3u) == 0;
    case COND_FIRST: return g->pat_loop == 0;
    default: return 1;
    }
}

static int prob_ok(Gb *g, int prob)
{
    if (prob >= 100) return 1;
    if (prob <= 0) return 0;
    return (int)((noise(&g->rng) + 1.0f) * 50.0f) < prob;
}

/* Un colpo del sequencer (anche ogni colpo di un ratchet): t_ev e' il suo tempo, dur la sua durata. */
static void play_hit(Gb *g, int tr, const GbStep *st, float vel, double t_ev, double dur, int ratchet)
{
    if (tr < GB_DRUMS) {
        drum_trigger(g, tr, vel, st);
    } else if (tr == GB_ACID) {
        AcidVoice *a = &g->ac;
        int legato = a->slide_next && !ratchet;
        acid_note(g, 48.0f + clampi(st->note, -24, 48), vel, st->accent != 0, legato, st);
        a->slide_next = st->slide && !ratchet;
        a->off_clk = -1;
        a->off_t = a->slide_next ? t_ev + dur + 0.25 * g->ts[tr].sf : t_ev + dur * 0.55;
        g->flash[tr]++;
    } else {
        float v = st->accent ? clampf(vel * 1.25f, 0.0f, 1.0f) : vel;
        FoldVoice *fv = fold_note(g, 48 + clampi(st->note, -24, 48), v, st);
        int len = ratchet ? 1 : clampi(st->len, 1, 16);
        fv->off_t = t_ev + dur * len * (ratchet ? 0.8 : 0.92);
        g->flash[tr]++;
    }
}

static void track_fire(Gb *g, int tr)
{
    TrackSeq *ts = &g->ts[tr];
    const GbTrackPattern *tp = &g->pat[g->cur_pat].track[tr];
    int len = clampi(tp->length, 1, GB_MAX_STEPS);
    int idx = ts->idx < len ? ts->idx : 0;
    const GbStep st = tp->step[idx];
    int sp = tp->speed & 3;
    double sf = ts->sf, p = ts->p;
    double dur = step_time(g, p + sf, sp) - step_time(g, p, sp);
    g->shown[tr] = idx;
    if (st.on && cond_ok(g, st.cond) && prob_ok(g, st.prob)) {
        int r = clampi(st.ratchet, 1, 4);
        float vel = (float)clampi(st.vel, 1, 127) / 127.0f;
        play_hit(g, tr, &st, vel, ts->t, dur / r, r > 1);
        if (r > 1) {
            ts->rat_left = r - 1;
            ts->rat_dt = dur / r;
            ts->rat_t = ts->t + ts->rat_dt;
            ts->rat_idx = idx;
            ts->rat_vel = vel;
        }
    }
    ts->idx = idx + 1;
    ts->p = p + sf;
    if (ts->idx >= len) ts->idx = 0;
    track_schedule(g, tr);
}

static void giro_done(Gb *g)
{
    double B = g->giro_end, G = B - g->pat_start;
    g->bar_count++;
    g->pat_loop++;
    int next = g->sel_pat;
    if (g->chain_n > 0) {
        g->chain_pos = (g->chain_pos + 1) % g->chain_n;
        next = g->chain[g->chain_pos];
    }
    if (next != g->cur_pat) {
        g->cur_pat = next;
        g->pat_loop = 0;
        tracks_reset(g, B);
    } else {
        /* le tracce che stanno un numero intero di volte nel giro ripartono in fase (se una modifica dal vivo
           le ha sfasate); le altre sono poliritmi e continuano libere */
        for (int t = 0; t < GB_TRACKS; t++) {
            const GbTrackPattern *tp = &g->pat[g->cur_pat].track[t];
            double d = clampi(tp->length, 1, GB_MAX_STEPS) * SPEED_SF[tp->speed & 3], k = G / d;
            TrackSeq *ts = &g->ts[t];
            if (fabs(k - floor(k + 0.5)) < 1e-6 && (ts->idx != 0 || fabs(ts->p - B) > 1e-6)) {
                ts->idx = 0;
                ts->p = B;
                track_schedule(g, t);
            }
        }
    }
    g->pat_start = B;
    g->giro_end = B + giro_len(&g->pat[g->cur_pat]);
    g->bound_t = warp(g->giro_end, g->swing);
}

static void melodic_release_seq(Gb *g)
{
    if (g->ac.off_t >= 0.0 || (g->ac.gate && g->ac.held_n == 0 && g->ac.off_clk < 0)) acid_gate_off(g);
    g->ac.slide_next = 0;
    for (int i = 0; i < GB_FOLD_VOICES; i++)
        if (g->fv[i].on && !g->fv[i].live && g->fv[i].off_t >= 0.0) { g->fv[i].gate = 0; g->fv[i].off_t = -1.0; }
}

static void transport_start(Gb *g)
{
    melodic_release_seq(g);
    g->playing = 1;
    g->T = 0.0;
    g->clock_n = 0;
    g->bar_count = 0;
    g->pat_loop = 0;
    if (g->chain_n > 0) { g->chain_pos = 0; g->cur_pat = g->chain[0]; }
    else g->cur_pat = g->sel_pat;
    for (int t = 0; t < GB_TRACKS; t++) { g->ts[t].rat_left = 0; g->shown[t] = -1; }
    tracks_reset(g, 0.0);
    g->pat_start = 0.0;
    g->giro_end = giro_len(&g->pat[g->cur_pat]);
    g->bound_t = warp(g->giro_end, g->swing);
}

static void transport_stop(Gb *g)
{
    g->playing = 0;
    for (int t = 0; t < GB_TRACKS; t++) { g->ts[t].rat_left = 0; g->shown[t] = -1; }
    melodic_release_seq(g);
}

/* Tutti gli eventi del sequencer arrivati a T (offset: campione del blocco, per il clock esterno). */
static void seq_due(Gb *g, int offset)
{
    if (g->playing) {
        const double now = g->T + EPS;
        for (int guard = 0; guard < 64; guard++) {
            int any = 0;
            if (now >= g->bound_t) { giro_done(g); any = 1; }
            while (now >= g->clock_n / 6.0) {
                if (g->hook) g->hook(g->hook_user, offset);
                g->clock_n++;
            }
            for (int t = 0; t < GB_TRACKS; t++) {
                TrackSeq *ts = &g->ts[t];
                if (ts->rat_left > 0 && now >= ts->rat_t) {
                    const GbStep st = g->pat[g->cur_pat].track[t].step[ts->rat_idx];
                    play_hit(g, t, &st, ts->rat_vel, ts->rat_t, ts->rat_dt, 1);
                    ts->rat_left--;
                    ts->rat_t += ts->rat_dt;
                    any = 1;
                }
                if (now >= ts->t) { track_fire(g, t); any = 1; }
            }
            if (g->ac.off_t >= 0.0 && now >= g->ac.off_t) { acid_gate_off(g); g->ac.slide_next = 0; }
            for (int i = 0; i < GB_FOLD_VOICES; i++)
                if (g->fv[i].off_t >= 0.0 && now >= g->fv[i].off_t) { g->fv[i].gate = 0; g->fv[i].off_t = -1.0; }
            if (!any) break;
        }
    }
    if (g->ac.off_clk >= 0 && g->clk >= g->ac.off_clk) acid_gate_off(g);
    for (int i = 0; i < GB_FOLD_VOICES; i++)
        if (g->fv[i].off_clk >= 0 && g->clk >= g->fv[i].off_clk) { g->fv[i].gate = 0; g->fv[i].off_clk = -1; }
}

/* Campioni fino al prossimo evento (almeno 1, al piu' maxn). */
static int frames_to_next(const Gb *g, int maxn, double inc)
{
    int n = maxn;
    if (g->playing) {
        double tmin = g->bound_t, ct = g->clock_n / 6.0;
        if (ct < tmin) tmin = ct;
        for (int t = 0; t < GB_TRACKS; t++) {
            if (g->ts[t].t < tmin) tmin = g->ts[t].t;
            if (g->ts[t].rat_left > 0 && g->ts[t].rat_t < tmin) tmin = g->ts[t].rat_t;
        }
        if (g->ac.off_t >= 0.0 && g->ac.off_t < tmin) tmin = g->ac.off_t;
        for (int i = 0; i < GB_FOLD_VOICES; i++)
            if (g->fv[i].off_t >= 0.0 && g->fv[i].off_t < tmin) tmin = g->fv[i].off_t;
        double d = (tmin - g->T - EPS) / inc;
        if (d < (double)n) n = d <= 1.0 ? 1 : (int)ceil(d);
    }
    if (g->ac.off_clk >= 0 && g->ac.off_clk - g->clk < n) n = (int)(g->ac.off_clk - g->clk);
    for (int i = 0; i < GB_FOLD_VOICES; i++)
        if (g->fv[i].off_clk >= 0 && g->fv[i].off_clk - g->clk < n) n = (int)(g->fv[i].off_clk - g->clk);
    return n < 1 ? 1 : n;
}

/* ------------------------------------------------------------------ coda */

static void push(Gb *g, int type, int a, int b, float f)
{
    int h = g->q_head, n = (h + 1) % QUEUE_SIZE;
    if (n == __atomic_load_n(&g->q_tail, __ATOMIC_ACQUIRE)) return;   /* coda piena: si perde l'evento */
    g->q[h].type = type;
    g->q[h].a = a;
    g->q[h].b = b;
    g->q[h].f = f;
    __atomic_store_n(&g->q_head, n, __ATOMIC_RELEASE);
}

static void process_queue(Gb *g)
{
    int t = g->q_tail;
    while (t != __atomic_load_n(&g->q_head, __ATOMIC_ACQUIRE)) {
        Event e = g->q[t];
        switch (e.type) {
        case EV_HIT:
            if (e.a >= 0 && e.a < GB_DRUMS) drum_trigger(g, e.a, clampf(e.f, 0.0f, 1.0f), NULL);
            else if (e.a == GB_ACID || e.a == GB_FOLD) melodic_preview(g, e.a, clampf(e.f, 0.0f, 1.0f));
            break;
        case EV_NOTE_ON:
            if (e.a == GB_ACID) acid_live_on(g, e.b, e.f);
            else if (e.a == GB_FOLD) {
                FoldVoice *v = fold_note(g, e.b, e.f, NULL);
                v->live = 1;
                g->flash[GB_FOLD]++;
            } else if (e.a >= 0 && e.a < GB_DRUMS) drum_trigger(g, e.a, clampf(e.f, 0.0f, 1.0f), NULL);
            break;
        case EV_NOTE_OFF:
            if (e.a == GB_ACID) acid_live_off(g, e.b);
            else if (e.a == GB_FOLD) {
                for (int i = 0; i < GB_FOLD_VOICES; i++)
                    if (g->fv[i].on && g->fv[i].live && g->fv[i].gate && g->fv[i].note == e.b) g->fv[i].gate = 0;
            }
            break;
        case EV_ALL_OFF:
            g->ac.held_n = 0;
            acid_gate_off(g);
            for (int i = 0; i < GB_FOLD_VOICES; i++) { g->fv[i].gate = 0; g->fv[i].off_t = -1.0; g->fv[i].off_clk = -1; }
            break;
        case EV_PLAY:
            if (e.a) transport_start(g); else if (g->playing) transport_stop(g);
            break;
        case EV_SELECT:
            g->sel_pat = clampi(e.a, 0, GB_PATTERNS - 1);
            if (!g->playing && g->chain_n == 0) g->cur_pat = g->sel_pat;
            break;
        case EV_CHAIN: {
            int s = clampi(e.a, 0, CHAIN_SLOTS - 1), n = clampi(g->chain_buf_n[s], 0, MAX_CHAIN);
            for (int i = 0; i < n; i++) g->chain[i] = clampi(g->chain_buf[s][i], 0, GB_PATTERNS - 1);
            g->chain_n = n;
            if (n == 0) { g->chain_pos = -1; if (!g->playing) g->cur_pat = g->sel_pat; }
            else if (g->playing) g->chain_pos = -1;          /* al prossimo fine giro si parte dal primo */
            else { g->chain_pos = 0; g->cur_pat = g->chain[0]; }
            break;
        }
        }
        t = (t + 1) % QUEUE_SIZE;
        __atomic_store_n(&g->q_tail, t, __ATOMIC_RELEASE);
    }
}

/* ------------------------------------------------------------------ melodiche: suono */

static void acid_render(Gb *g, float *restrict o, int n)
{
    AcidVoice *a = &g->ac;
    const GbAcid p = g->acid;
    const float sr = g->sr, inv = g->inv_sr;
    float tune = a->lock_tune > LOCK_OFF ? a->lock_tune : clampf(p.tune, -24.0f, 24.0f);
    float target = midi_hz(a->note + tune);
    if (!a->slide) a->freq = target;
    float cut = a->lock_cut > LOCK_OFF ? a->lock_cut : clampf(p.cutoff, 0.0f, 1.0f);
    const float base = 5.0f + cut * 7.3f;                       /* log2: 32 Hz .. 5 kHz */
    const float envm = clampf(p.env_mod, 0.0f, 1.0f) * 5.0f;
    const float accm = clampf(p.accent, 0.0f, 1.0f);
    const float kres = clampf(p.resonance, 0.0f, 1.0f) * 3.85f;
    const float comp = 1.0f + kres * 0.45f;
    const float dr = clampf(p.drive, 0.0f, 1.0f), dg = 1.0f + 6.0f * dr, dn = (1.0f - 0.35f * dr) / softclip(dg);
    const float vca = a->vel * (1.0f + accm * 0.8f * a->acc);
    const float atk = 1.0f - expf(-1.0f / (0.003f * sr)), rel = expf(-1.0f / (0.008f * sr));
    const float acc_dec = expf(-1.0f / (0.22f * sr)), acc_rise = 1.0f - expf(-1.0f / (0.012f * sr));
    const float gk = g->glide_k, maxfc = 0.42f * sr;
    const int sq = p.wave == 1;
    float freq = a->freq, ph = a->ph, s1 = a->s1, s2 = a->s2, s3 = a->s3, s4 = a->s4, G = a->G, B = a->B;
    float fenv = a->fenv, fk = a->fk, amp = a->amp, acc_t = a->acc_t, cap = a->acc_cap;
    int ctl = a->ctl, gate = a->gate;
    for (int i = 0; i < n; i++) {
        freq += (target - freq) * gk;
        float dt = freq * inv;
        if (dt > 0.45f) dt = 0.45f;
        ph += dt;
        if (ph >= 1.0f) ph -= 1.0f;
        float x = sq ? square_blep(ph, dt) : 2.0f * ph - 1.0f - polyblep(ph, dt);
        if (--ctl <= 0) {
            ctl = 8;
            float fc = exp2f(base + envm * fenv + accm * cap * 2.5f);
            if (fc > maxfc) fc = maxfc;
            float gg = tanf(PI_F * fc * inv);
            G = gg / (1.0f + gg);
            B = 1.0f / (1.0f + gg);
        }
        /* ladder a 4 poli a ritardo zero: si risolve la retroazione, poi si satura l'ingresso */
        float G2 = G * G;
        float S = B * (G2 * G * s1 + G2 * s2 + G * s3 + s4);
        float u = softclip((x - kres * S) / (1.0f + kres * G2 * G2));
        float v1 = (u - s1) * G, y1 = v1 + s1; s1 = y1 + v1;
        float v2 = (y1 - s2) * G, y2 = v2 + s2; s2 = y2 + v2;
        float v3 = (y2 - s3) * G, y3 = v3 + s3; s3 = y3 + v3;
        float v4 = (y3 - s4) * G, y4 = v4 + s4; s4 = y4 + v4;
        if (gate) amp += (1.0f - amp) * atk; else amp *= rel;
        fenv *= fk;
        acc_t *= acc_dec;
        cap += (acc_t - cap) * acc_rise;
        o[i] = softclip(y4 * comp * amp * vca * dg) * dn * ACID_GAIN;
    }
    a->freq = freq; a->ph = ph; a->s1 = s1; a->s2 = s2; a->s3 = s3; a->s4 = s4; a->G = G; a->B = B;
    a->fenv = fenv; a->amp = amp; a->acc_t = acc_t; a->acc_cap = cap; a->ctl = ctl;
    if (!gate && amp < 1e-4f) a->on = 0;
}

static float fold_ratio(float x)                     /* 0..1 -> 0.5..8, a scatti armonici sotto 0.75 */
{
    static const float h[10] = { 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f };
    x = clampf(x, 0.0f, 1.0f);
    if (x < 0.75f) return h[clampi((int)(x / 0.75f * 10.0f), 0, 9)];
    return 0.5f * exp2f((x - 0.75f) * 16.0f);
}

static void fold_render(Gb *g, FoldVoice *v, float *restrict o, int n)
{
    const GbFold p = g->fold;
    const float sr = g->sr, inv = g->inv_sr;
    float tune = v->lock_tune > LOCK_OFF ? v->lock_tune : clampf(p.tune, -24.0f, 24.0f);
    float inc = clampf(midi_hz((float)v->note + tune) * inv, 0.0f, 0.45f);
    float minc = clampf(inc * fold_ratio(p.mod_ratio), 0.0f, 0.45f);
    float fold = v->lock_fold > LOCK_OFF ? v->lock_fold : clampf(p.fold, 0.0f, 1.0f);
    float gfold = 1.0f + fold * 7.0f, bias = (clampf(p.symmetry, 0.0f, 1.0f) - 0.5f) * 1.5f;
    float pm = clampf(p.mod_amount, 0.0f, 1.0f) * 0.6f;
    float dec = v->lock_dec > LOCK_OFF ? v->lock_dec : clampf(p.decay, 0.0f, 1.0f);
    float rel = clampf(p.release, 0.0f, 1.0f);
    float rd = 1.0f / ((0.04f + 2.0f * dec * dec) * sr), rr = 1.0f / ((0.01f + 1.5f * rel * rel) * sr);
    float rate = v->gate ? rd : (rr > rd ? rr : rd);
    float gc = clampf(p.gate_color, 0.0f, 1.0f);
    float ka = 1.0f - expf(-1.0f / (0.002f * sr));
    float ph = v->ph, mph = v->mph, vac = v->vac, dx = v->dc_x, dy = v->dc_y, vel = v->vel;
    int ctl = v->ctl, attack = v->attack;
    Svf f = v->lpg;
    for (int i = 0; i < n; i++) {
        ph += inc; if (ph >= 1.0f) ph -= 1.0f;
        mph += minc; if (mph >= 1.0f) mph -= 1.0f;
        float tri = mph < 0.5f ? 4.0f * mph - 1.0f : 3.0f - 4.0f * mph;     /* modulatore a triangolo */
        float s = sin1(wrap1(ph + pm * tri + 1.0f));                          /* portante sinusoidale */
        float y = sin1(wrap1((s * gfold + bias) * 0.25f + 8.0f));            /* wavefolder: sin(pi/2 x) */
        float yd = y - dx + 0.995f * dy;                                      /* via la continua della simmetria */
        dx = y; dy = yd;
        if (attack) {                               /* vactrol: si apre in un paio di ms... */
            vac += (1.05f - vac) * ka;
            if (vac >= 1.0f) { vac = 1.0f; attack = 0; }
        } else {                                    /* ...e si richiude piu' lento sul fondo */
            vac -= vac * rate * (0.35f + 0.65f * vac);
        }
        if (--ctl <= 0) {
            ctl = 8;
            float oct = (1.0f - gc) * 14.2f + gc * (5.3f + 8.9f * vac);
            svf_set(&f, exp2f(oct), 1.5f, sr);
        }
        o[i] += svf_lp(&f, yd) * vac * vel * FOLD_GAIN;
    }
    v->ph = ph; v->mph = mph; v->vac = vac; v->dc_x = dx; v->dc_y = dy; v->ctl = ctl; v->attack = attack; v->lpg = f;
    if (!v->gate && !attack && vac < 1e-4f) v->on = 0;
}

/* ------------------------------------------------------------------ mixer, effetti, master */

typedef struct {
    float spb16, dtarget, dfb, dta, dha;
    float rdamp;
    float comp, thr, ratio_e, makeup, c_att, c_rel;
    float pump, pump_k;
    float filt, fres;
    float lofi, lf_step, lf_lev;
    float tape, vol;
} MCtl;

static void fdn_update(Gb *g, float size)
{
    float rt60 = 0.4f + 7.6f * size * size;
    for (int j = 0; j < FDN; j++) g->fdn_g[j] = powf(10.0f, -3.0f * (float)g->fdn_len[j] / (rt60 * g->sr));
    g->r_size = size;
    g->r_time = rt60;
}

/* Guadagni voluti della traccia: L, R, mandata delay, mandata riverbero; restituisce il volume. */
static float mix_targets(const Gb *g, int tr, int any_solo, float *tgt)
{
    float lev, pan;
    if (tr < GB_DRUMS) { lev = g->drum[tr].level; pan = g->drum[tr].pan; }
    else if (tr == GB_ACID) { lev = g->acid.level; pan = g->acid.pan; }
    else { lev = g->fold.level; pan = g->fold.pan; }
    const GbMix *m = &g->mix[tr];
    lev = clampf(lev, 0.0f, 1.0f) * clampf(m->level, 0.0f, 1.0f);
    if (m->mute || (any_solo && !m->solo)) lev = 0.0f;
    pan = clampf(pan, -1.0f, 1.0f);
    tgt[0] = lev * (pan > 0.0f ? 1.0f - pan : 1.0f);
    tgt[1] = lev * (pan < 0.0f ? 1.0f + pan : 1.0f);
    tgt[2] = lev * clampf(m->delay_send, 0.0f, 1.0f);
    tgt[3] = lev * clampf(m->reverb_send, 0.0f, 1.0f);
    return lev;
}

static void mix_track(Gb *g, int tr, int active, int n, int any_solo)
{
    float tgt[4];
    float lev = mix_targets(g, tr, any_solo, tgt);
    float *cur = g->gcur[tr];
    if (!active) {
        for (int k = 0; k < 4; k++) cur[k] = tgt[k];
        g->tlev[tr] *= 1.0f - (float)n / (0.25f * g->sr);
        if (g->tlev[tr] < 0.0f) g->tlev[tr] = 0.0f;
        return;
    }
    float inv = 1.0f / (float)n, d[4];
    for (int k = 0; k < 4; k++) d[k] = (tgt[k] - cur[k]) * inv;
    float gl = cur[0], gr = cur[1], gd = cur[2], gv = cur[3], pk = 0.0f;
    const float *x = g->tb[tr];
    float *L = tr == 0 ? g->kick_l : g->bus_l, *R = tr == 0 ? g->kick_r : g->bus_r;
    float *DS = g->snd_d, *VS = g->snd_v;
    for (int i = 0; i < n; i++) {
        gl += d[0]; gr += d[1]; gd += d[2]; gv += d[3];
        float s = x[i];
        L[i] += s * gl;
        R[i] += s * gr;
        DS[i] += s * gd;
        VS[i] += s * gv;
        float a = fabsf(s);
        if (a > pk) pk = a;
    }
    for (int k = 0; k < 4; k++) cur[k] = tgt[k];
    pk *= lev;
    float l = g->tlev[tr] * (1.0f - (float)n / (0.25f * g->sr));
    g->tlev[tr] = pk > l ? pk : l;
}

static inline float limiter(float x)                 /* morbido sopra 0.85, mai oltre 1 */
{
    float a = fabsf(x);
    if (a <= 0.85f) return x;
    float y = 0.85f + 0.15f * softclip((a - 0.85f) / 0.15f);
    return x < 0.0f ? -y : y;
}

static void render_block(Gb *g, float *restrict out, int n, const MCtl *mc)
{
    const float sr = g->sr;
    memset(g->bus_l, 0, sizeof(float) * (size_t)n);
    memset(g->bus_r, 0, sizeof(float) * (size_t)n);
    memset(g->kick_l, 0, sizeof(float) * (size_t)n);
    memset(g->kick_r, 0, sizeof(float) * (size_t)n);
    memset(g->snd_d, 0, sizeof(float) * (size_t)n);
    memset(g->snd_v, 0, sizeof(float) * (size_t)n);

    /* voci */
    int active[GB_TRACKS], any_solo = 0;
    for (int t = 0; t < GB_TRACKS; t++) any_solo |= g->mix[t].solo;
    for (int t = 0; t < GB_DRUMS; t++) {
        active[t] = g->dv[t][0].on || g->dv[t][1].on;
        if (!active[t]) continue;
        memset(g->tb[t], 0, sizeof(float) * (size_t)n);
        for (int k = 0; k < 2; k++) if (g->dv[t][k].on) drum_voice_render(&g->dv[t][k], g->tb[t], g->tmp, n, g->max_t);
    }
    active[GB_ACID] = g->ac.on;
    if (g->ac.on) acid_render(g, g->tb[GB_ACID], n);
    active[GB_FOLD] = 0;
    for (int i = 0; i < GB_FOLD_VOICES; i++) {
        if (!g->fv[i].on) continue;
        if (!active[GB_FOLD]) { memset(g->tb[GB_FOLD], 0, sizeof(float) * (size_t)n); active[GB_FOLD] = 1; }
        fold_render(g, &g->fv[i], g->tb[GB_FOLD], n);
    }
    for (int t = 0; t < GB_TRACKS; t++) mix_track(g, t, active[t], n, any_solo);

    /* delay ping-pong a tempo: l'ingresso entra a sinistra, i ritorni si incrociano */
    int din = 0;
    for (int i = 0; i < n && !din; i++) din = g->snd_d[i] != 0.0f;
    g->d_quiet = din ? 0 : g->d_quiet + n;
    if (g->d_quiet < (int)(12.0f * sr)) {
        float *dl = g->dl, *dr = g->dr;
        int len = g->dlen, pos = g->dpos;
        float dt = g->dtime, fl = g->dfl, fr = g->dfr, hl = g->dhl, hr = g->dhr;
        for (int i = 0; i < n; i++) {
            dt += (mc->dtarget - dt) * 0.0007f;
            float rp = (float)pos - dt;
            if (rp < 0.0f) rp += (float)len;
            int i0 = (int)rp, i1 = i0 + 1 == len ? 0 : i0 + 1;
            float fr0 = rp - (float)i0;
            float rl = dl[i0] + fr0 * (dl[i1] - dl[i0]);
            float rr = dr[i0] + fr0 * (dr[i1] - dr[i0]);
            fl += mc->dta * (rl - fl); hl += mc->dha * (fl - hl);
            fr += mc->dta * (rr - fr); hr += mc->dha * (fr - hr);
            dl[pos] = softclip(g->snd_d[i] + (fr - hr) * mc->dfb);
            dr[pos] = softclip((fl - hl) * mc->dfb);
            if (++pos >= len) pos = 0;
            g->bus_l[i] += rl * 0.8f;
            g->bus_r[i] += rr * 0.8f;
        }
        g->dpos = pos; g->dtime = dt; g->dfl = fl; g->dfr = fr; g->dhl = hl; g->dhr = hr;
    } else {
        g->dtime = mc->dtarget;
    }

    /* riverbero: 4 allpass di diffusione, poi 8 ritardi con matrice di Householder e smorzamento */
    int rin = 0;
    for (int i = 0; i < n && !rin; i++) rin = g->snd_v[i] != 0.0f;
    g->r_quiet = rin ? 0 : g->r_quiet + n;
    if (g->r_quiet < (int)((g->r_time + 0.5f) * sr)) {
        static const float sign[FDN] = { 1, -1, 1, -1, -1, 1, -1, 1 };
        float lp[FDN];
        for (int j = 0; j < FDN; j++) lp[j] = g->fdn_lp[j];
        const float damp = mc->rdamp;
        for (int i = 0; i < n; i++) {
            float x = g->snd_v[i];
            for (int d = 0; d < DIFF; d++) {
                float *b = g->dif[d];
                int p = g->dif_pos[d];
                float bv = b[p];
                float w = x + 0.6f * bv;
                b[p] = w;
                x = bv - 0.6f * w;
                if (++p >= g->dif_len[d]) p = 0;
                g->dif_pos[d] = p;
            }
            float y[FDN], sum = 0.0f;
            for (int j = 0; j < FDN; j++) {
                y[j] = g->fdn[j][g->fdn_pos[j]];
                lp[j] += damp * (y[j] - lp[j]);
                sum += lp[j];
            }
            sum *= 2.0f / FDN;
            for (int j = 0; j < FDN; j++) {
                int p = g->fdn_pos[j];
                g->fdn[j][p] = (lp[j] - sum) * g->fdn_g[j] + x * sign[j] * 0.35f;
                if (++p >= g->fdn_len[j]) p = 0;
                g->fdn_pos[j] = p;
            }
            g->bus_l[i] += (y[0] - y[2] + y[4] - y[6]) * 0.3f;
            g->bus_r[i] += (y[1] - y[3] + y[5] - y[7]) * 0.3f;
        }
        for (int j = 0; j < FDN; j++) g->fdn_lp[j] = lp[j];
    }

    /* bus master */
    const int rec_on = __atomic_load_n(&g->rec_on, __ATOMIC_ACQUIRE);
    uint32_t rec_w = g->rec_w;
    const uint32_t rec_r = __atomic_load_n(&g->rec_r, __ATOMIC_ACQUIRE);
    float block_peak = 0.0f;
    float rq;
    __atomic_load(&g->repeat_req, &rq, __ATOMIC_RELAXED);
    if (rq > 0.0f) {
        int L = clampi((int)(rq * mc->spb16 + 0.5f), 96, g->hist_len - 1);
        if (!g->rep_on) {
            if (g->rep_x <= 0.0f) g->rep_freeze = g->hist_pos;   /* se stava ancora sfumando, stesso frammento */
            g->rep_on = 1;
            g->rep_L = L;
            g->rep_i = 0;
        } else if (L != g->rep_L) {
            g->rep_L = L;
            g->rep_i = 0;
        }
    } else {
        g->rep_on = 0;
    }
    const float pump_d = mc->pump * 0.85f;
    for (int i = 0; i < n; i++) {
        /* pompa: tutto tranne la cassa si abbassa a ogni colpo della traccia 0 */
        g->pump_env *= mc->pump_k;
        g->pump_s += (g->pump_env - g->pump_s) * 0.02f;
        float duck = 1.0f - pump_d * g->pump_s;
        float l = (g->kick_l[i] + g->bus_l[i] * duck) * MIX_GAIN;
        float r = (g->kick_r[i] + g->bus_r[i] * duck) * MIX_GAIN;
        float yl = l - g->dc_xl + g->dc_k * g->dc_yl, yr = r - g->dc_xr + g->dc_k * g->dc_yr;   /* via la continua (10 Hz) */
        g->dc_xl = l; g->dc_yl = yl; l = yl;
        g->dc_xr = r; g->dc_yr = yr; r = yr;

        /* stutter: si ripete l'ultimo frammento della storia (che intanto non si scrive) */
        if (g->rep_on || g->rep_x > 0.0f) {
            int L = g->rep_L, k = g->rep_i;
            int idx = g->rep_freeze - L + k;
            while (idx < 0) idx += g->hist_len;
            float w = 1.0f;
            if (k < 48) w = (float)k / 48.0f;
            else if (L - k < 48) w = (float)(L - k) / 48.0f;
            float sl = g->hist_l[idx] * w, srr = g->hist_r[idx] * w;
            if (++k >= L) k = 0;
            g->rep_i = k;
            g->rep_x += g->rep_on ? 1.0f / 96.0f : -1.0f / 96.0f;
            g->rep_x = clampf(g->rep_x, 0.0f, 1.0f);
            l += (sl - l) * g->rep_x;
            r += (srr - r) * g->rep_x;
        } else {
            g->hist_l[g->hist_pos] = l;
            g->hist_r[g->hist_pos] = r;
            if (++g->hist_pos >= g->hist_len) g->hist_pos = 0;
        }

        /* compressore del bus (stereo collegato) */
        if (mc->comp > 0.005f) {
            float lv = fabsf(l) > fabsf(r) ? fabsf(l) : fabsf(r);
            g->c_env = lv > g->c_env ? g->c_env + (lv - g->c_env) * mc->c_att : g->c_env * mc->c_rel;
            if (--g->c_ctl <= 0) {
                g->c_ctl = 16;
                g->c_tgt = g->c_env > mc->thr ? powf(g->c_env / mc->thr, mc->ratio_e) : 1.0f;
            }
            g->c_gain += (g->c_tgt - g->c_gain) * 0.02f;
            float cg = g->c_gain * mc->makeup;
            l *= cg; r *= cg;
        }

        /* filtro DJ: sotto zero passa-basso, sopra passa-alto; vicino a zero si sfuma verso il suono pulito */
        g->dj_s += (mc->filt - g->dj_s) * 0.002f;
        float fa = fabsf(g->dj_s);
        if (fa > 0.0005f) {
            int mode = g->dj_s > 0.0f;
            if (--g->dj_ctl <= 0 || mode != g->dj_mode) {
                g->dj_ctl = 16;
                g->dj_mode = mode;
                float fc = mode ? 20.0f * exp2f(g->dj_s * 9.5f) : 20000.0f * exp2f(g->dj_s * 9.0f);
                float k = 2.0f - 1.85f * mc->fres * clampf(fa * 10.0f, 0.0f, 1.0f);
                svf_set(&g->dj_l, fc, k, sr);
                g->dj_r = (Svf){ g->dj_l.a1, g->dj_l.a2, g->dj_l.a3, g->dj_l.k, g->dj_r.ic1, g->dj_r.ic2 };
            }
            float wl = mode ? svf_hp(&g->dj_l, l) : svf_lp(&g->dj_l, l);
            float wr = mode ? svf_hp(&g->dj_r, r) : svf_lp(&g->dj_r, r);
            float w = clampf(fa * 30.0f, 0.0f, 1.0f);
            l += (softclip(wl) - l) * w;
            r += (softclip(wr) - r) * w;
        } else {
            g->dj_l.ic1 = g->dj_l.ic2 = g->dj_r.ic1 = g->dj_r.ic2 = 0.0f;   /* escluso: riparte pulito */
        }

        /* lo-fi: campionamento piu' lento e meno bit */
        if (mc->lofi > 0.002f) {
            g->lf_ph += mc->lf_step;
            if (g->lf_ph >= 1.0f) {
                g->lf_ph -= 1.0f;
                float ql = l * mc->lf_lev, qr = r * mc->lf_lev;
                g->lf_l = (float)(int)(ql + (ql >= 0.0f ? 0.5f : -0.5f)) / mc->lf_lev;
                g->lf_r = (float)(int)(qr + (qr >= 0.0f ? 0.5f : -0.5f)) / mc->lf_lev;
            }
            l = g->lf_l; r = g->lf_r;
        }

        /* nastro: saturazione asimmetrica, wow e flutter, alti un po' smussati */
        g->tape_s += (mc->tape - g->tape_s) * 0.001f;
        if (g->tape_s > 0.001f) {
            float ts = g->tape_s, gt = 1.0f + 2.5f * ts, b = 0.15f * ts, sb = softclip(b), comp = 1.0f / (1.0f + 0.8f * ts);
            l = (softclip(l * gt + b) - sb) * comp;
            r = (softclip(r * gt + b) - sb) * comp;
            int p = g->tape_pos;
            g->tape_l[p] = l; g->tape_r[p] = r;
            g->wow_ph += 0.55f * g->inv_sr; if (g->wow_ph >= 1.0f) g->wow_ph -= 1.0f;
            g->flut_ph += 6.3f * g->inv_sr; if (g->flut_ph >= 1.0f) g->flut_ph -= 1.0f;
            float dly = ts * (90.0f + 60.0f * sin1(g->wow_ph) + 6.0f * sin1(g->flut_ph));
            float rp = (float)p - dly;
            if (rp < 0.0f) rp += TAPE_LEN;
            int i0 = (int)rp, i1 = (i0 + 1) & (TAPE_LEN - 1);
            float fr = rp - (float)i0;
            l = g->tape_l[i0] + fr * (g->tape_l[i1] - g->tape_l[i0]);
            r = g->tape_r[i0] + fr * (g->tape_r[i1] - g->tape_r[i0]);
            g->tape_pos = (p + 1) & (TAPE_LEN - 1);
            float ha = 1.0f - ts * 0.55f;
            g->hf_l += ha * (l - g->hf_l); g->hf_r += ha * (r - g->hf_r);
            l = g->hf_l; r = g->hf_r;
        }

        g->vol_s += (mc->vol - g->vol_s) * 0.002f;
        l = limiter(l * g->vol_s);
        r = limiter(r * g->vol_s);
        out[2 * i] = l;
        out[2 * i + 1] = r;
        if (rec_on) {
            if (rec_w - rec_r < GB_REC_FRAMES) {
                uint32_t k = rec_w % GB_REC_FRAMES;
                g->rec[2 * k] = (int16_t)(l * 32767.0f);
                g->rec[2 * k + 1] = (int16_t)(r * 32767.0f);
                rec_w++;
            } else {
                g->rec_drop++;
            }
        }
        float m = 0.5f * (l + r);
        g->scope[(g->scope_pos + i) % GB_SCOPE] = m;
        float am = fabsf(l) > fabsf(r) ? fabsf(l) : fabsf(r);
        if (am > block_peak) block_peak = am;
    }
    __atomic_store_n(&g->scope_pos, (g->scope_pos + n) % GB_SCOPE, __ATOMIC_RELEASE);
    __atomic_store_n(&g->rec_w, rec_w, __ATOMIC_RELEASE);
    g->peak = fmaxf(g->peak * (1.0f - (float)n / (0.3f * sr)), block_peak);
}

/* Tutto il lavoro di un blocco, senza misura del tempo. */
static void render_internal(Gb *g, float *out, int frames)
{
    process_queue(g);
    const GbMaster M = g->master;
    const float sr = g->sr;
    MCtl mc;
    float bpm = clampf(M.bpm, 40.0f, 240.0f);
    g->swing = clampf(M.swing, 0.0f, 0.75f);
    mc.spb16 = sr * 60.0f / bpm / 4.0f;
    mc.dtarget = clampf(clampf(M.delay_time, 1.0f, 8.0f) * mc.spb16, 4.0f, (float)g->dlen - 4.0f);
    mc.dfb = clampf(M.delay_feedback, 0.0f, 0.9f);
    mc.dta = onepole(1000.0f * exp2f(clampf(M.delay_tone, 0.0f, 1.0f) * 4.0f), sr);
    mc.dha = onepole(120.0f, sr);
    float size = clampf(M.reverb_size, 0.0f, 1.0f);
    if (fabsf(size - g->r_size) > 1e-4f) fdn_update(g, size);
    mc.rdamp = onepole(16000.0f * exp2f(-clampf(M.reverb_damp, 0.0f, 1.0f) * 3.6f), sr);
    mc.comp = clampf(M.comp, 0.0f, 1.0f);
    mc.thr = powf(10.0f, (-6.0f - 18.0f * mc.comp) / 20.0f);
    mc.ratio_e = 1.0f / (1.0f + 7.0f * mc.comp) - 1.0f;
    mc.makeup = 1.0f + 0.9f * mc.comp;
    mc.c_att = 1.0f - expf(-1.0f / (0.003f * sr));
    mc.c_rel = expf(-1.0f / (0.15f * sr));
    if (mc.comp <= 0.005f) { g->c_gain = 1.0f; g->c_env = 0.0f; g->c_tgt = 1.0f; }
    mc.pump = clampf(M.pump, 0.0f, 1.0f);
    mc.pump_k = expf(-1.0f / (0.18f * 60.0f / bpm * sr));
    mc.filt = clampf(M.filter, -1.0f, 1.0f);
    mc.fres = clampf(M.filter_res, 0.0f, 1.0f);
    mc.lofi = clampf(M.lofi, 0.0f, 1.0f);
    mc.lf_step = 1.0f / (1.0f + mc.lofi * mc.lofi * 20.0f);
    mc.lf_lev = exp2f(15.0f - 11.0f * mc.lofi);
    mc.tape = clampf(M.tape, 0.0f, 1.0f);
    mc.vol = clampf(M.volume, 0.0f, 1.0f);
    const double inc = (double)bpm / 15.0 / (double)sr;      /* sedicesimi per campione */

    int done = 0;
    while (done < frames) {
        seq_due(g, done);
        int n = frames - done;
        if (n > BLOCK) n = BLOCK;
        n = frames_to_next(g, n, inc);
        render_block(g, out + 2 * done, n, &mc);
        if (g->playing) g->T += n * inc;
        g->clk += n;
        done += n;
    }
}

/* ------------------------------------------------------------------ API */

static double now_seconds(void)
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

/* denormali a zero durante il render (si ripristina dopo, il thread resta com'era) */
#if defined(__x86_64__) || defined(__i386__)
typedef unsigned int FpState;
static FpState fp_enter(void) { FpState s = _mm_getcsr(); _mm_setcsr(s | 0x8040u); return s; }
static void fp_leave(FpState s) { _mm_setcsr(s); }
#elif defined(__aarch64__)
typedef uint64_t FpState;
static FpState fp_enter(void)
{
    uint64_t s;
    __asm__ volatile("mrs %0, fpcr" : "=r"(s));
    __asm__ volatile("msr fpcr, %0" : : "r"(s | (1ull << 24)));
    return s;
}
static void fp_leave(FpState s) { __asm__ volatile("msr fpcr, %0" : : "r"(s)); }
#else
typedef int FpState;
static FpState fp_enter(void) { return 0; }
static void fp_leave(FpState s) { (void)s; }
#endif

static void step_clear(GbStep *s)
{
    memset(s, 0, sizeof(*s));
    s->vel = 100;
    s->prob = 100;
    s->ratchet = 1;
    s->len = 1;
    s->lock_tune = s->lock_decay = s->lock_tone = GB_LOCK_NONE;
}

Gb *gb_create(float sample_rate)
{
    tables_init();
    Gb *g = calloc(1, sizeof(Gb));
    if (!g) return NULL;
    if (sample_rate < 8000.0f) sample_rate = GB_RATE;
    g->sr = sample_rate;
    g->inv_sr = 1.0f / sample_rate;
    g->rng = 0x12345678u;
    g->max_t = (int)(12.0f * sample_rate);
    g->glide_k = 1.0f - expf(-1.0f / (0.02f * sample_rate));
    for (int p = 0; p < GB_PATTERNS; p++) {
        for (int t = 0; t < GB_TRACKS; t++) {
            GbTrackPattern *tp = &g->pat[p].track[t];
            tp->length = 16;
            for (int i = 0; i < GB_MAX_STEPS; i++) step_clear(&tp->step[i]);
        }
    }
    for (int t = 0; t < GB_TRACKS; t++) { g->mix[t].level = 0.8f; g->shown[t] = -1; }
    g->acid = (GbAcid){ .wave = 0, .tune = -12.0f, .cutoff = 0.3f, .resonance = 0.65f, .env_mod = 0.6f, .decay = 0.4f,
                        .accent = 0.6f, .drive = 0.2f, .level = 0.8f, .pan = 0.0f };
    g->fold = (GbFold){ .tune = 0.0f, .fold = 0.35f, .symmetry = 0.5f, .mod_ratio = 0.25f, .mod_amount = 0.2f, .decay = 0.4f,
                        .gate_color = 0.7f, .release = 0.3f, .level = 0.8f, .pan = 0.0f };
    g->master = (GbMaster){ .bpm = 120.0f, .swing = 0.0f, .delay_time = 3.0f, .delay_feedback = 0.4f, .delay_tone = 0.5f,
                            .reverb_size = 0.5f, .reverb_damp = 0.5f, .comp = 0.3f, .pump = 0.0f, .filter = 0.0f,
                            .filter_res = 0.3f, .lofi = 0.0f, .tape = 0.0f, .volume = 0.8f };
    g->vol_s = 0.8f;
    g->dc_k = 1.0f - 2.0f * PI_F * 10.0f / sample_rate;
    g->c_gain = g->c_tgt = 1.0f;
    g->chain_pos = -1;
    g->ac.off_t = -1.0; g->ac.off_clk = -1;
    for (int i = 0; i < GB_FOLD_VOICES; i++) { g->fv[i].off_t = -1.0; g->fv[i].off_clk = -1; }

    g->dlen = (int)(sample_rate * DELAY_SEC);
    g->dl = calloc((size_t)g->dlen, sizeof(float));
    g->dr = calloc((size_t)g->dlen, sizeof(float));
    g->dtime = 3.0f * sample_rate * 60.0f / 120.0f / 4.0f;
    static const int fl[FDN] = { 1427, 1637, 1871, 2053, 2311, 2543, 2789, 3001 };
    static const int dfl[DIFF] = { 142, 107, 379, 277 };
    float k = sample_rate / 48000.0f;
    int ok = g->dl && g->dr;
    for (int j = 0; j < FDN; j++) {
        g->fdn_len[j] = (int)(fl[j] * k);
        g->fdn[j] = calloc((size_t)g->fdn_len[j], sizeof(float));
        ok = ok && g->fdn[j];
    }
    for (int j = 0; j < DIFF; j++) {
        g->dif_len[j] = (int)(dfl[j] * k) + 1;
        g->dif[j] = calloc((size_t)g->dif_len[j], sizeof(float));
        ok = ok && g->dif[j];
    }
    g->r_size = -1.0f;
    fdn_update(g, 0.5f);
    g->tape_l = calloc(TAPE_LEN, sizeof(float));
    g->tape_r = calloc(TAPE_LEN, sizeof(float));
    g->hist_len = (int)(sample_rate * HIST_SEC);
    g->hist_l = calloc((size_t)g->hist_len, sizeof(float));
    g->hist_r = calloc((size_t)g->hist_len, sizeof(float));
    g->rec = calloc((size_t)GB_REC_FRAMES * 2, sizeof(int16_t));
    ok = ok && g->tape_l && g->tape_r && g->hist_l && g->hist_r && g->rec;
    if (!ok) { gb_destroy(g); return NULL; }
    gb_load_kit(g, 0);
    for (int t = 0; t < GB_TRACKS; t++) mix_targets(g, t, 0, g->gcur[t]);   /* niente rampa al primo colpo */
    return g;
}

void gb_destroy(Gb *g)
{
    if (!g) return;
    free(g->dl); free(g->dr);
    for (int j = 0; j < FDN; j++) free(g->fdn[j]);
    for (int j = 0; j < DIFF; j++) free(g->dif[j]);
    free(g->tape_l); free(g->tape_r);
    free(g->hist_l); free(g->hist_r);
    free(g->rec);
    free(g);
}

float gb_sample_rate(const Gb *g) { return g->sr; }

void gb_render(Gb *g, float *out, int frames)
{
    if (frames <= 0) return;
    FpState fs = fp_enter();
    double t0 = now_seconds();
    render_internal(g, out, frames);
    double el = now_seconds() - t0;
    float load = (float)(el * g->sr / frames);
    g->cpu = g->cpu * 0.9f + load * 0.1f;
    fp_leave(fs);
}

void gb_render_offline(Gb *g, float *out, int frames)
{
    if (frames <= 0) return;
    FpState fs = fp_enter();
    render_internal(g, out, frames);
    fp_leave(fs);
}

GbDrum *gb_drum(Gb *g, int track) { return &g->drum[clampi(track, 0, GB_DRUMS - 1)]; }
GbAcid *gb_acid(Gb *g) { return &g->acid; }
GbFold *gb_fold(Gb *g) { return &g->fold; }
GbMix *gb_mix(Gb *g, int track) { return &g->mix[clampi(track, 0, GB_TRACKS - 1)]; }
GbMaster *gb_master(Gb *g) { return &g->master; }
GbPattern *gb_pattern(Gb *g, int index) { return &g->pat[clampi(index, 0, GB_PATTERNS - 1)]; }

void gb_hit(Gb *g, int track, float velocity) { push(g, EV_HIT, track, 0, velocity); }
void gb_note_on(Gb *g, int track, int midi_note, float velocity) { push(g, EV_NOTE_ON, track, midi_note, velocity); }
void gb_note_off(Gb *g, int track, int midi_note) { push(g, EV_NOTE_OFF, track, midi_note, 0.0f); }
void gb_all_notes_off(Gb *g) { push(g, EV_ALL_OFF, 0, 0, 0.0f); }

void gb_play(Gb *g, int on)
{
    __atomic_store_n(&g->play_ui, on ? 1 : 0, __ATOMIC_RELEASE);
    push(g, EV_PLAY, on ? 1 : 0, 0, 0.0f);
}

int gb_playing(const Gb *g) { return __atomic_load_n(&g->play_ui, __ATOMIC_ACQUIRE); }
void gb_select_pattern(Gb *g, int index) { push(g, EV_SELECT, clampi(index, 0, GB_PATTERNS - 1), 0, 0.0f); }
/* Il pattern che suona (da fermo: quello che partira'). */
int gb_playing_pattern(const Gb *g) { return g->cur_pat; }

void gb_set_chain(Gb *g, const int *patterns, int count)
{
    count = clampi(count, 0, MAX_CHAIN);
    int s = (g->chain_slot + 1) % CHAIN_SLOTS;
    g->chain_slot = s;
    for (int i = 0; i < count; i++) g->chain_buf[s][i] = clampi(patterns[i], 0, GB_PATTERNS - 1);
    g->chain_buf_n[s] = count;
    push(g, EV_CHAIN, s, 0, 0.0f);
}

int gb_chain_position(const Gb *g) { return g->chain_n > 0 ? g->chain_pos : -1; }
void gb_set_fill(Gb *g, int on) { __atomic_store_n(&g->fill, on ? 1 : 0, __ATOMIC_RELAXED); }

void gb_set_repeat(Gb *g, float div)
{
    float v = div > 0.0f ? clampf(div, 0.125f, 4.0f) : 0.0f;
    __atomic_store(&g->repeat_req, &v, __ATOMIC_RELAXED);
}

int gb_track_step(const Gb *g, int track)
{
    if (track < 0 || track >= GB_TRACKS || !g->playing) return -1;
    return g->shown[track];
}

uint32_t gb_track_flash(const Gb *g, int track) { return (track >= 0 && track < GB_TRACKS) ? g->flash[track] : 0; }
float gb_track_level(Gb *g, int track) { return (track >= 0 && track < GB_TRACKS) ? clampf(g->tlev[track], 0.0f, 1.0f) : 0.0f; }
int gb_bar_count(const Gb *g) { return g->bar_count; }

int gb_active_voices(const Gb *g, int track)
{
    if (track >= 0 && track < GB_DRUMS) return g->dv[track][0].on + g->dv[track][1].on;
    if (track == GB_ACID) return g->ac.on;
    if (track == GB_FOLD) {
        int n = 0;
        for (int i = 0; i < GB_FOLD_VOICES; i++) n += g->fv[i].on;
        return n;
    }
    return 0;
}

void gb_rec_start(Gb *g)
{
    __atomic_store_n(&g->rec_r, __atomic_load_n(&g->rec_w, __ATOMIC_ACQUIRE), __ATOMIC_RELEASE);
    g->rec_drop = 0;
    __atomic_store_n(&g->rec_on, 1, __ATOMIC_RELEASE);
}

void gb_rec_stop(Gb *g) { __atomic_store_n(&g->rec_on, 0, __ATOMIC_RELEASE); }
int gb_rec_active(const Gb *g) { return __atomic_load_n(&g->rec_on, __ATOMIC_ACQUIRE); }
uint32_t gb_rec_dropped(const Gb *g) { return g->rec_drop; }

int gb_rec_read(Gb *g, int16_t *dst, int max_frames)
{
    uint32_t w = __atomic_load_n(&g->rec_w, __ATOMIC_ACQUIRE), r = g->rec_r;
    int n = (int)(w - r);
    if (n > max_frames) n = max_frames;
    for (int i = 0; i < n; i++) {
        uint32_t k = (r + (uint32_t)i) % GB_REC_FRAMES;
        dst[2 * i] = g->rec[2 * k];
        dst[2 * i + 1] = g->rec[2 * k + 1];
    }
    __atomic_store_n(&g->rec_r, r + (uint32_t)n, __ATOMIC_RELEASE);
    return n;
}

void gb_set_clock_hook(Gb *g, void (*hook)(void *user, int frame_offset), void *user)
{
    g->hook_user = user;
    __atomic_store_n(&g->hook, hook, __ATOMIC_RELEASE);
}

void gb_scope(const Gb *g, float *dst, int n)
{
    if (n > GB_SCOPE) n = GB_SCOPE;
    int p = __atomic_load_n(&g->scope_pos, __ATOMIC_ACQUIRE);
    for (int i = 0; i < n; i++) dst[i] = g->scope[(p - n + i + GB_SCOPE) % GB_SCOPE];
}

float gb_peak(Gb *g) { return g->peak; }
float gb_cpu_load(const Gb *g) { return g->cpu; }

void gb_euclid(GbTrackPattern *t, int length, int hits, int rotate, int velocity)
{
    length = clampi(length, 1, GB_MAX_STEPS);
    hits = clampi(hits, 0, length);
    velocity = clampi(velocity, 1, 127);
    t->length = (uint8_t)length;
    for (int i = 0; i < length; i++) {
        int j = ((i - rotate) % length + length) % length;   /* rotate > 0 sposta a destra */
        int on = hits > 0 && (j * hits) % length < hits;
        GbStep *s = &t->step[i];
        s->on = (uint8_t)on;
        if (on) {
            s->vel = (uint8_t)velocity;
            s->prob = 100;
            s->cond = COND_ALWAYS;
            s->ratchet = 1;
            s->micro = 0;
        }
    }
}
