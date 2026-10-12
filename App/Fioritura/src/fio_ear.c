/* Fioritura - l'orecchio del giardino (vedi fio_ear.h).
 *
 * Il suono si legge a passi di circa 5,3 ms (256 campioni a 48 kHz): livello, banda acuta (sopra 2 kHz,
 * per gli attacchi), rumore di fondo, parte dell'uscita che rientra dal microfono, soglia. Ogni 4 passi
 * (21 ms) YIN e le corde, su una finestra a meta' frequenza (media di coppie, come in Diapason).
 */
#include "fio_ear.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define RING 65536u                 /* campioni del microfono: 1,4 s a 48 kHz */
#define HOP_S 0.00533f
#define YIN_EVERY 4
#define YW 768                      /* finestra di YIN, a frequenza ridotta (32 ms a 24 kHz) */
#define TAU_CAP 400
#define GW 1024                     /* finestra delle corde (43 ms a 24 kHz) */
#define NWIN (YW + TAU_CAP > GW ? YW + TAU_CAP : GW)
#define F_LO 70.0f
#define F_HI 2000.0f
#define HIST 5
#define MAXQ 32
#define CALIBRA 0.6                 /* secondi iniziali in cui si impara il fondo, senza eventi */
#define PI_F 3.14159265f

struct FioEar {
    float ring[RING];
    unsigned wpos;                  /* scritto dal thread del microfono (atomico) */
    unsigned rpos;
    float rate, drate;
    int dec, hop, hop_n, tau_min, tau_max;
    long hops;
    double t;
    float sens;

    /* filtri e accumulatori del passo */
    float dc_x, dc_y, lp_hi, a_hi;
    double acc, acc_hi;

    /* livelli in dB */
    float floor_db, leak_db, out_hold, hold_k, odb, thr_db, lvl_db, recent_db, rec_db[YIN_EVERY], level;
    float ohist[6], mhist[8];       /* uscita e microfono degli ultimi passi */
    float hear, att_out, att_pre, att_mic;   /* il microfono sente l'altoparlante? (-1..1) e l'attacco in esame */
    int att_k, leak_n;
    float leak_s[5];                /* ultime prove di quanto rientra */
    float hi_hist[8];

    /* battito */
    int clap_wait, clap_k;
    float clap_peak, clap_tail;
    double clap_t, last_clap;

    /* soffio */
    int blow_run, blow_gap, blowing, blow_quiet;
    float blow;

    /* canto */
    int voiced, unv, run_n, has_note, hist_n;
    float conf, pitch, run_mean, run_lvl, note_on;
    double run_t0;
    float hist[HIST];

    /* corde */
    float snote[FEAR_STRINGS], sres[FEAR_STRINGS], scoef[FEAR_STRINGS], sdecay;
    int sok[FEAR_STRINGS], ns;
    float hann[GW];

    float raw[2 * NWIN + 8], win[NWIN + 4], df[TAU_CAP + 2];
    EarEvent q[MAXQ];
    int nq;
};

static inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static float db_of(double ms) { return 10.0f * log10f((float)ms + 1e-12f); }

FioEar *fear_create(void)
{
    FioEar *e = calloc(1, sizeof(FioEar));
    if (!e) return NULL;
    e->sens = 0.5f;
    for (int n = 0; n < GW; n++) e->hann[n] = 0.5f - 0.5f * cosf(2.0f * PI_F * (float)n / (float)(GW - 1));
    return e;
}

void fear_destroy(FioEar *e) { free(e); }

void fear_feed(FioEar *e, const float *in, int n)
{
    unsigned w = __atomic_load_n(&e->wpos, __ATOMIC_RELAXED);
    for (int i = 0; i < n; i++) e->ring[(w + (unsigned)i) & (RING - 1)] = in[i];
    __atomic_store_n(&e->wpos, w + (unsigned)n, __ATOMIC_RELEASE);
}

static void strings_coef(FioEar *e)
{
    for (int i = 0; i < e->ns; i++) {
        float f = 440.0f * powf(2.0f, (e->snote[i] - 69.0f) / 12.0f);
        e->sok[i] = e->drate > 0.0f && f < e->drate * 0.45f;
        e->scoef[i] = e->sok[i] ? 2.0f * cosf(2.0f * PI_F * f / e->drate) : 0.0f;
    }
}

void fear_set_rate(FioEar *e, float rate)
{
    e->rate = rate > 4000.0f ? rate : 0.0f;
    e->rpos = __atomic_load_n(&e->wpos, __ATOMIC_ACQUIRE);
    e->hop_n = 0;
    e->acc = e->acc_hi = 0.0;
    e->hops = 0;
    e->t = 0.0;
    e->voiced = e->unv = e->run_n = e->has_note = e->hist_n = 0;
    e->blowing = e->blow_run = e->blow_gap = e->blow_quiet = 0;
    e->blow = 0.0f;
    e->clap_wait = 0;
    e->last_clap = -1.0;
    e->level = 0.0f;
    e->floor_db = -50.0f;
    e->leak_db = -80.0f;               /* finche' non si sa, l'altoparlante non conta nella soglia */
    e->leak_n = 0;
    e->hear = 0.0f;                    /* non si sa ancora */
    e->att_k = -1;
    e->out_hold = 0.0f;
    for (int k = 0; k < 6; k++) e->ohist[k] = -120.0f;
    for (int k = 0; k < 8; k++) e->mhist[k] = -120.0f;
    e->dc_x = e->dc_y = e->lp_hi = 0.0f;
    for (int k = 0; k < 8; k++) e->hi_hist[k] = -120.0f;
    for (int k = 0; k < YIN_EVERY; k++) e->rec_db[k] = -120.0f;
    memset(e->sres, 0, sizeof(e->sres));
    if (!e->rate) { e->drate = 0.0f; return; }
    e->dec = e->rate >= 32000.0f ? 2 : 1;
    e->drate = e->rate / (float)e->dec;
    e->hop = (int)lroundf(e->rate * HOP_S);
    if (e->hop < 32) e->hop = 32;
    e->tau_max = (int)ceilf(e->drate / F_LO);
    if (e->tau_max > TAU_CAP) e->tau_max = TAU_CAP;
    e->tau_min = (int)floorf(e->drate / F_HI);
    if (e->tau_min < 2) e->tau_min = 2;
    float hs = (float)e->hop / e->rate;
    e->a_hi = 1.0f - expf(-2.0f * PI_F * 2000.0f / e->rate);
    e->hold_k = expf(-hs / 0.15f);
    e->sdecay = expf(-hs / 0.35f);
    strings_coef(e);
}

void fear_set_sensitivity(FioEar *e, float s) { e->sens = clampf(s, 0.0f, 1.0f); }

void fear_set_strings(FioEar *e, const float *notes, int n)
{
    if (n > FEAR_STRINGS) n = FEAR_STRINGS;
    if (n == e->ns && !memcmp(notes, e->snote, sizeof(float) * (size_t)n)) return;
    memcpy(e->snote, notes, sizeof(float) * (size_t)n);
    e->ns = n;
    memset(e->sres, 0, sizeof(e->sres));
    strings_coef(e);
}

static void push_ev(FioEar *e, int kind, float note, float strength, double t)
{
    if (e->t < CALIBRA || e->nq >= MAXQ) return;
    EarEvent *v = &e->q[e->nq++];
    v->kind = kind;
    v->note = note;
    v->strength = clampf(strength, 0.0f, 1.0f);
    v->t = t;
}

/* YIN su x (YW + tau_max campioni a drate): frequenza fondamentale o 0. */
static float yin(FioEar *e, const float *x, float *conf)
{
    float *df = e->df;
    df[0] = 1.0f;
    float running = 0.0f;
    int found = -1, top = e->tau_max;
    for (int tau = 1; tau <= e->tau_max; tau++) {
        float sum = 0.0f;
        for (int j = 0; j < YW; j++) {
            float d = x[j] - x[j + tau];
            sum += d * d;
        }
        running += sum;
        df[tau] = running > 0.0f ? sum * (float)tau / running : 1.0f;
        if (found < 0 && tau - 1 >= e->tau_min && tau > 2 && df[tau - 1] < 0.15f && df[tau] > df[tau - 1]) found = tau - 1;
        if (found >= 0 && tau >= found + 2) { top = tau; break; }   /* il resto non serve */
    }
    if (found < 0) {
        float best = 1.0f;
        for (int tau = e->tau_min; tau < e->tau_max; tau++) if (df[tau] < best) { best = df[tau]; found = tau; }
        if (found < 0 || best > 0.35f) { *conf = 0.0f; return 0.0f; }
    }
    *conf = 1.0f - df[found];
    float t = (float)found;
    if (found > 1 && found < top) {                    /* interpolazione parabolica */
        float a = df[found - 1], b = df[found], c = df[found + 1];
        float den = a - 2.0f * b + c;
        if (fabsf(den) > 1e-9f) t += 0.5f * (a - c) / den;
    }
    return t > 1.0f ? e->drate / t : 0.0f;
}

/* Corde: quanto suono c'e' alla frequenza di ciascuna (Goertzel con finestra di Hann), rispetto a tutto
   il suono della finestra. Un seno esattamente su una corda vale 1. */
static void strings_listen(FioEar *e, const float *x, float rms, float gate)
{
    if (!e->ns || gate <= 0.0f) return;
    float norm = 2.0f / ((float)GW * 0.5f), ref = rms * 1.41421356f + 1e-6f;
    for (int i = 0; i < e->ns; i++) {
        if (!e->sok[i]) continue;
        float c = e->scoef[i], s1 = 0.0f, s2 = 0.0f;
        for (int n = 0; n < GW; n++) {
            float s0 = x[n] * e->hann[n] + c * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        float p = s1 * s1 + s2 * s2 - c * s1 * s2;
        float r = clampf(sqrtf(p > 0.0f ? p : 0.0f) * norm / ref, 0.0f, 1.0f);
        float v = r * sqrtf(r) * gate;
        if (v > e->sres[i]) e->sres[i] = v;
    }
}

static void analyze(FioEar *e)
{
    int nd = YW + e->tau_max > GW ? YW + e->tau_max : GW;
    int nraw = nd * e->dec;
    for (int i = 0; i < nraw; i++) e->raw[i] = e->ring[(e->rpos - (unsigned)nraw + (unsigned)i) & (RING - 1)];
    if (e->dec == 2) for (int i = 0; i < nd; i++) e->win[i] = 0.5f * (e->raw[2 * i] + e->raw[2 * i + 1]);
    else memcpy(e->win, e->raw, sizeof(float) * (size_t)nd);
    float mean = 0.0f;
    for (int i = 0; i < nd; i++) mean += e->win[i];
    mean /= (float)nd;
    float ms = 0.0f;
    for (int i = 0; i < nd; i++) { e->win[i] -= mean; if (i >= nd - GW) ms += e->win[i] * e->win[i]; }
    float rms = sqrtf(ms / (float)GW);

    float over = e->recent_db - e->thr_db;
    strings_listen(e, e->win + (nd - GW), rms, clampf((over + 6.0f) / 18.0f, 0.0f, 1.0f));

    float f = 0.0f, conf = 0.0f;
    if (over > 0.0f) f = yin(e, e->win + (nd - YW - e->tau_max), &conf);
    if (f >= F_LO && f <= F_HI && conf >= 0.80f - 0.08f * e->sens) {
        float m = 69.0f + 12.0f * log2f(f / 440.0f);
        if (e->hist_n < HIST) e->hist[e->hist_n++] = m;
        else { memmove(e->hist, e->hist + 1, sizeof(float) * (HIST - 1)); e->hist[HIST - 1] = m; }
        float s[HIST];
        memcpy(s, e->hist, sizeof(float) * (size_t)e->hist_n);
        for (int i = 1; i < e->hist_n; i++) for (int j = i; j > 0 && s[j - 1] > s[j]; j--) { float t = s[j]; s[j] = s[j - 1]; s[j - 1] = t; }
        float med = s[e->hist_n / 2];
        e->voiced = 1;
        e->unv = 0;
        e->conf = conf;
        e->pitch = med;
        /* una nota conta quando resta ferma (mezzo semitono) per sei analisi, circa 120 ms: le vocali del
           parlato di solito scivolano prima */
        if (e->run_n == 0 || fabsf(med - e->run_mean) > 0.5f) {
            e->run_mean = med;
            e->run_n = 1;
            e->run_lvl = e->recent_db;
            e->run_t0 = e->t - 0.03;                /* la finestra di analisi vede l'inizio con un po' di ritardo */
        } else {
            e->run_n++;
            e->run_mean += (med - e->run_mean) * 0.3f;
            if (e->recent_db > e->run_lvl) e->run_lvl = e->recent_db;
        }
        if (e->run_n >= 6 && (!e->has_note || fabsf(e->run_mean - e->note_on) > 0.75f)) {
            push_ev(e, EAR_NOTE, e->run_mean, (e->run_lvl - e->thr_db) / 30.0f + 0.3f, e->run_t0);
            e->has_note = 1;
            e->note_on = e->run_mean;
        }
    } else if (++e->unv >= 3) {
        e->voiced = 0;
        e->has_note = 0;
        e->run_n = 0;
        e->hist_n = 0;
    }
}

static void hop_end(FioEar *e, float out_level)
{
    float hs = (float)e->hop / e->rate, s = e->sens;
    e->t += hs;
    e->hops++;
    float db = db_of(e->acc / e->hop), hdb = db_of(e->acc_hi / e->hop);
    e->lvl_db = db;
    e->rec_db[e->hops % YIN_EVERY] = db;
    e->recent_db = e->rec_db[0];
    for (int k = 1; k < YIN_EVERY; k++) if (e->rec_db[k] > e->recent_db) e->recent_db = e->rec_db[k];

    /* l'uscita del giardino, tenuta un poco per coprire il ritardo tra altoparlante e microfono */
    e->out_hold = fmaxf(out_level, e->out_hold * e->hold_k);
    e->odb = 20.0f * log10f(e->out_hold + 1e-6f);

    /* rumore di fondo: scende in fretta e sale piano (all'inizio lo si impara in fretta) */
    if (e->t < CALIBRA) e->floor_db += (db - e->floor_db) * 0.06f;
    else if (db < e->floor_db) e->floor_db += (db - e->floor_db) * 0.08f;
    else e->floor_db += (db - e->floor_db) * 0.0006f;
    e->floor_db = clampf(e->floor_db, -90.0f, 0.0f);

    /* Il microfono sente l'altoparlante? A ogni attacco del giardino (l'uscita sale di 4,5 dB in 30 ms)
       si guarda se tra 40 e 250 ms dopo il microfono sale anche lui (il ritardo dell'altoparlante sta li'
       dentro). Se si', si annota di quanto: e' la parte dell'uscita che rientra (mediana delle ultime
       prove). In cuffia non sale mai, salvo quando qualcuno comincia a cantare proprio li': la media delle
       prove lo dice dopo pochi attacchi. */
    float oinst = 20.0f * log10f(out_level + 1e-6f);
    float omin = e->ohist[0];
    for (int k = 1; k < 6; k++) if (e->ohist[k] < omin) omin = e->ohist[k];
    memmove(e->ohist + 1, e->ohist, sizeof(float) * 5);
    e->ohist[0] = oinst;
    float mpre = 0.0f;
    for (int k = 0; k < 8; k++) mpre += e->mhist[k];
    mpre /= 8.0f;
    if (e->att_k >= 0) {
        e->att_k++;
        if (e->att_k <= 4 && oinst > e->att_out) e->att_out = oinst;
        if (e->att_k >= 8 && db > e->att_mic) e->att_mic = db;
        if (e->att_k >= 47) {
            /* «non sentito» e' una prova solo se prima il microfono era quasi in silenzio: se era gia'
               pieno di suono (il giardino stesso, o qualcuno che canta) la salita non si vedrebbe */
            int heard = e->att_mic - e->att_pre > 4.0f;
            if (heard || e->att_pre < e->floor_db + 6.0f) e->hear += ((heard ? 1.0f : -1.0f) - e->hear) * 0.25f;
            if (heard) {
                e->leak_s[e->leak_n++ % 5] = e->att_mic - e->att_out;
                int n = e->leak_n < 5 ? e->leak_n : 5;
                float v[5];
                memcpy(v, e->leak_s, sizeof(float) * (size_t)n);
                for (int i = 1; i < n; i++) for (int j = i; j > 0 && v[j - 1] > v[j]; j--) { float t = v[j]; v[j] = v[j - 1]; v[j - 1] = t; }
                e->leak_db = v[n / 2];
            }
            e->att_k = -1;
        }
    } else if (e->t > CALIBRA && oinst > -45.0f && oinst - omin > 4.5f) {
        e->att_k = 0;
        e->att_out = oinst;
        e->att_pre = mpre;
        e->att_mic = -120.0f;
    }
    memmove(e->mhist + 1, e->mhist, sizeof(float) * 7);
    e->mhist[0] = db;
    e->leak_db = clampf(e->leak_db, -80.0f, 20.0f);
    /* soglia: ben sopra il fondo; se il microfono sente l'altoparlante, anche sopra il giardino che rientra
       (li' aiuta il filtro delle note appena suonate, in fio_app.c); mai sotto un minimo assoluto */
    float thr = fmaxf(e->floor_db + 20.0f - 12.0f * s, -64.0f + 18.0f * (1.0f - s));
    if (e->hear > 0.2f) thr = fmaxf(thr, e->odb + e->leak_db + 12.0f - 8.0f * s);
    e->thr_db = thr;
    e->level += (clampf((db - e->thr_db + 6.0f) / 30.0f, 0.0f, 1.0f) - e->level) * 0.25f;
    for (int i = 0; i < e->ns; i++) e->sres[i] *= e->sdecay;

    /* battito: la banda acuta salta in alto di colpo e dopo 20-40 ms e' gia' spenta */
    float ref = 0.25f * (e->hi_hist[1] + e->hi_hist[2] + e->hi_hist[3] + e->hi_hist[4]);
    if (e->clap_wait) {
        e->clap_k++;
        if (e->clap_k == 1 && db > e->clap_peak) e->clap_peak = db;
        if (e->clap_k >= 4) e->clap_tail += db;
        if (e->clap_k == 7) {
            if (e->clap_tail / 4.0f < e->clap_peak - 9.0f) {
                push_ev(e, EAR_CLAP, 0.0f, (e->clap_peak - e->thr_db) / 24.0f + 0.2f, e->clap_t);
                e->last_clap = e->clap_t;
            }
            e->clap_wait = 0;
        }
    } else if (e->hops > 8 && e->t - e->last_clap > 0.09 && hdb - ref > 15.0f - 5.0f * s && db > e->thr_db + 4.0f && hdb > -52.0f) {
        e->clap_wait = 1;
        e->clap_k = 0;
        e->clap_peak = db;
        e->clap_tail = 0.0f;
        e->clap_t = e->t - hs;
    }
    memmove(e->hi_hist + 1, e->hi_hist, sizeof(float) * 7);
    e->hi_hist[0] = hdb;

    /* soffio: forte, senza altezza, per piu' di 150 ms (brevi buchi ammessi) */
    if (db > e->thr_db + 8.0f && !e->voiced) { e->blow_run++; e->blow_gap = 0; }
    else if (e->blow_run > 0 && ++e->blow_gap > 6) { e->blow_run = 0; e->blow_gap = 0; }
    float k = clampf((db - e->thr_db - 8.0f) / 24.0f, 0.0f, 1.0f);
    if (!e->blowing && e->blow_run >= 28) {
        e->blowing = 1;
        e->blow_quiet = 0;
        e->blow = fmaxf(k, 0.15f);
        push_ev(e, EAR_BLOW_START, 0.0f, e->blow, e->t);
    }
    if (e->blowing) {
        e->blow += (fmaxf(k, 0.1f) - e->blow) * 0.12f;
        if (db < e->thr_db + 4.0f || e->voiced) {
            if (++e->blow_quiet > 14) {
                e->blowing = 0;
                e->blow_run = 0;
                push_ev(e, EAR_BLOW_END, 0.0f, e->blow, e->t);
            }
        } else {
            e->blow_quiet = 0;
        }
    } else {
        e->blow *= 0.85f;
    }

    /* mentre si soffia basta la meta' delle analisi (YIN sul rumore e' il caso piu' caro) */
    if (e->hops % (e->blowing ? 2 * YIN_EVERY : YIN_EVERY) == 0) analyze(e);
}

int fear_poll(FioEar *e, float out_level, EarEvent *ev, int max)
{
    e->nq = 0;
    if (e->rate <= 0.0f) return 0;
    unsigned w = __atomic_load_n(&e->wpos, __ATOMIC_ACQUIRE);
    if (w - e->rpos > RING / 2) e->rpos = w - RING / 4;       /* rimasti indietro: si salta avanti */
    while (e->rpos != w) {
        float x = e->ring[e->rpos & (RING - 1)];
        e->rpos++;
        if (!(x > -4.0f && x < 4.0f)) x = 0.0f;
        float y = x - e->dc_x + 0.995f * e->dc_y;          /* via la continua */
        e->dc_x = x;
        e->dc_y = y;
        e->lp_hi += (y - e->lp_hi) * e->a_hi;
        float hi = y - e->lp_hi;
        e->acc += (double)y * y;
        e->acc_hi += (double)hi * hi;
        if (++e->hop_n >= e->hop) {
            hop_end(e, out_level);
            e->hop_n = 0;
            e->acc = e->acc_hi = 0.0;
        }
    }
    int n = e->nq < max ? e->nq : max;
    if (n > 0) memcpy(ev, e->q, sizeof(EarEvent) * (size_t)n);
    return n;
}

double fear_time(const FioEar *e) { return e->t; }
int   fear_voiced(const FioEar *e) { return e->rate > 0.0f && e->voiced; }
float fear_pitch(const FioEar *e) { return e->pitch; }
float fear_level(const FioEar *e) { return e->rate > 0.0f ? e->level : 0.0f; }
float fear_blow(const FioEar *e) { return e->rate > 0.0f && e->blowing ? e->blow : 0.0f; }
float fear_string(const FioEar *e, int i) { return i >= 0 && i < e->ns ? e->sres[i] : 0.0f; }
int   fear_hears_output(const FioEar *e) { return e->rate <= 0.0f ? -1 : (e->hear > 0.2f ? 1 : (e->hear < -0.3f ? -1 : 0)); }
float fear_threshold_db(const FioEar *e) { return e->thr_db; }
