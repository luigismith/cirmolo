/* Cirmolo Sampler - logica dell'app e interfaccia (vedi app.h).
 *
 * Sei pagine: Pad (griglia 4x4 dei campioni, oscilloscopio, stato del sequencer), Modifica (forma
 * d'onda e parametri del pad scelto), Sequenza (16 passi x 16 pad, 4 pattern concatenabili),
 * Libreria (WAV delle registrazioni e della libreria di serie, con anteprima), Registra (microfono USB)
 * e Opzioni (tempo, swing, metronomo, volume, clock MIDI, esportazione). I pad della MPK mini IV
 * (canale 10) suonano i 16 pad, i tasti suonano cromaticamente il pad scelto.
 */
#include "app.h"

#include <dirent.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "midi.h"

#ifdef _WIN32
#include <direct.h>
#define make_dir(p) _mkdir(p)
#else
#define make_dir(p) mkdir(p, 0755)
#endif

/* ------------------------------------------------------------------ palette */
#define C_BG_TOP   RGB(13, 16, 22)
#define C_BG_BOT   RGB(18, 23, 32)
#define C_BAR      RGB(16, 20, 28)
#define C_PANEL    RGB(25, 31, 42)
#define C_PANEL_HI RGB(34, 42, 57)
#define C_LINE     RGB(48, 58, 78)
#define C_TEXT     RGB(236, 236, 244)
#define C_MUTED    RGB(138, 148, 170)
#define C_DIM      RGB(88, 98, 120)
#define C_VIOLET   RGB(150, 116, 242)
#define C_VIOLET_L RGB(196, 176, 250)
#define C_VIOLET_D RGB(84, 60, 160)
#define C_GREEN    RGB(92, 190, 126)
#define C_AMBER    RGB(244, 186, 92)
#define C_RED      RGB(236, 96, 104)
#define C_SCOPE_BG RGB(10, 13, 18)

/* ------------------------------------------------------------------ costanti */
/* Gli otto tasti che suonano i pad di un banco, nell'ordine dei pad: croce sinistra-giu'-destra-su, poi Y-B-A-X. */
static const int PAD_BTNS[8] = { PAD_LEFT, PAD_DOWN, PAD_RIGHT, PAD_UP, PAD_Y, PAD_B, PAD_A, PAD_X };
static const char *PAD_BTN_LABELS[8] = { "SX", "GIÙ", "DX", "SU", "Y", "B", "A", "X" };
static const char *MODE_NAMES[MODE_COUNT] = { "colpo singolo", "tenuto", "loop" };
static const char *MODE_SHORT[MODE_COUNT] = { "colpo", "tenuto", "loop" };

enum { ED_VOLUME, ED_PITCH, ED_START, ED_END, ED_MODE, ED_REVERSE, ED_GROUP, ED_MUTE, ED_SOLO, ED_COUNT };
static const char *ED_NAMES[ED_COUNT] = { "Volume", "Intonazione", "Inizio", "Fine", "Modo", "Inverso", "Gruppo di esclusione", "Muto nel sequencer", "Solo nel sequencer" };

enum { OPT_TEMPO, OPT_SWING, OPT_METRO, OPT_VOLUME, OPT_CLOCK, OPT_EXPORT, OPT_COUNT };
static const char *OPT_NAMES[OPT_COUNT] = { "Tempo", "Swing", "Metronomo", "Volume generale", "Segui il clock MIDI", "Esporta il pattern in WAV" };

static const float THRESHOLDS[4] = { 0.0f, 0.01f, 0.03f, 0.1f };
static const char *THRESHOLD_NAMES[4] = { "subito", "-40 dB", "-30 dB", "-20 dB" };

#define LIB_STOCK "/mnt/SDCARD/App/LittleGPTracker/samplelib"
#define WAVE_COLS 600

typedef struct { char name[160]; int is_dir; } LibEntry;

/* ------------------------------------------------------------------ stato */
struct App {
    Sampler *s;
    char path[512], dir[512], rec_dir[512], exp_dir[512];
    char lib_root[2][512];
    int page, quit_dialog, quit;
    int bank, sel;                        /* banco e pad scelto (0..15) */
    int down[PAD_COUNT];
    float rep[PAD_COUNT];
    int btn_pad[8];                       /* pad suonato da ogni tasto, -1 se no */
    int edit_sel;
    int cx, cy;                           /* pagina Sequenza: cy -1 = riga dei pattern */
    /* libreria */
    int lib_level, lib_root_i;            /* 0 = elenco delle radici */
    char lib_dir[512];
    LibEntry *entries;
    int n_entries, cap_entries, lib_sel, lib_scroll;
    /* registrazione */
    int rec_thr;
    char mic_name[64];
    char last_rec[600], last_export[600];
    /* opzioni */
    int opt_sel, clock_follow;
    /* MIDI */
    char midi_name[64];
    float midi_flash;
    int sustain;
    int chrom_slot[128];                  /* slot su cui suona ogni nota cromatica, -1 */
    int clock_ticks;
    float clock_start, clock_seen, clock_bpm;
    /* levette */
    float lx, ry;
    int nudge;
    /* interfaccia */
    char toast[96];
    float toast_t, time;
    float scope[1200];
    float glow[SP_PADS];
    uint32_t flash_seen[SP_PADS];
    const Sample *wave_smp;
    int wave_frames;
    float wave_min[WAVE_COLS], wave_max[WAVE_COLS];
    uint32_t drawn_key;
    float drawn_time;
};

static void toast(App *a, const char *msg)
{
    snprintf(a->toast, sizeof(a->toast), "%s", msg);
    a->toast_t = 2.5f;
}

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static int ci_cmp(const char *x, const char *y)
{
    for (;; x++, y++) {
        int cx = (unsigned char)*x, cy = (unsigned char)*y;
        if (cx >= 'A' && cx <= 'Z') cx += 32;
        if (cy >= 'A' && cy <= 'Z') cy += 32;
        if (cx != cy || !cx) return cx - cy;
    }
}

static int has_ext(const char *name, const char *ext)
{
    size_t n = strlen(name), e = strlen(ext);
    return n > e && !ci_cmp(name + n - e, ext);
}

static void timestamp(char *out, size_t n)
{
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    if (!tm || !strftime(out, n, "%Y-%m-%d-%H%M%S", tm)) snprintf(out, n, "%ld", (long)t);
}

/* Nome libero in dir con prefisso e data: dir/prefisso-AAAA-MM-GG-hhmmss.wav (con -2, -3 se esiste gia'). */
static void free_name(const char *dir, const char *prefix, char *out, size_t n)
{
    char ts[32];
    timestamp(ts, sizeof(ts));
    for (int i = 1; i < 100; i++) {
        if (i == 1) snprintf(out, n, "%s/%s-%s.wav", dir, prefix, ts);
        else snprintf(out, n, "%s/%s-%s-%d.wav", dir, prefix, ts, i);
        FILE *f = fopen(out, "rb");
        if (!f) return;
        fclose(f);
    }
}

/* ------------------------------------------------------------------ WAV in scrittura (16 bit) */
static void wav_header(FILE *f, uint32_t frames, int rate, int channels)
{
    uint32_t data = frames * (uint32_t)channels * 2, riff = 36 + data, br = (uint32_t)rate * (uint32_t)channels * 2;
    unsigned char h[44] = { 'R', 'I', 'F', 'F' };
    h[4] = riff; h[5] = riff >> 8; h[6] = riff >> 16; h[7] = riff >> 24;
    memcpy(h + 8, "WAVEfmt ", 8);
    h[16] = 16; h[20] = 1; h[22] = (unsigned char)channels;
    h[24] = rate; h[25] = rate >> 8; h[26] = rate >> 16; h[27] = rate >> 24;
    h[28] = br; h[29] = br >> 8; h[30] = br >> 16; h[31] = br >> 24;
    h[32] = (unsigned char)(channels * 2); h[34] = 16;
    memcpy(h + 36, "data", 4);
    h[40] = data; h[41] = data >> 8; h[42] = data >> 16; h[43] = data >> 24;
    fseek(f, 0, SEEK_SET);
    fwrite(h, 1, 44, f);
}

static int wav_write16(const char *path, const float *data, int frames, int channels, int rate)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    wav_header(f, (uint32_t)frames, rate, channels);
    int16_t buf[2048];
    int n = frames * channels, k = 0;
    for (int i = 0; i < n; i++) {
        float v = clampf(data[i], -1.0f, 1.0f);
        buf[k++] = (int16_t)lrintf(v * 32767.0f);
        if (k == 2048) { fwrite(buf, 2, (size_t)k, f); k = 0; }
    }
    if (k) fwrite(buf, 2, (size_t)k, f);
    fclose(f);
    return 0;
}

/* ------------------------------------------------------------------ pad */
int app_selected_pad(const App *a) { return a->sel; }
int app_bank(const App *a) { return a->bank; }

void app_select_pad(App *a, int pad)
{
    if (pad < 0 || pad >= SP_PADS) return;
    a->sel = pad;
}

static void pad_label(int pad, char *out, size_t n) { snprintf(out, n, "%c%d", 'A' + pad / 8, pad % 8 + 1); }

static const char *pad_name(const App *a, int pad)
{
    const Sample *s = sampler_sample(a->s, pad);
    return s ? s->name : "vuoto";
}

int app_assign_wav(App *a, int pad, const char *path)
{
    if (pad < 0 || pad >= SP_PADS) return -1;
    Sample *smp = sample_load_wav(path);
    if (!smp) return -1;
    sampler_set_sample(a->s, pad, smp);
    return 0;
}

static void play_pad(App *a, int pad, float vel)
{
    if (pad < 0 || pad >= SP_PADS) return;
    a->sel = pad;
    sampler_trigger(a->s, pad, vel, 0.0f, -1);
}

static void release_all(App *a)
{
    for (int i = 0; i < 8; i++) a->btn_pad[i] = -1;
    for (int n = 0; n < 128; n++) a->chrom_slot[n] = -1;
    sampler_release_all(a->s);
}

/* ------------------------------------------------------------------ salvataggio */
void app_save(App *a)
{
    if (!a->path[0]) return;
    char tmp[600];
    snprintf(tmp, sizeof(tmp), "%s.tmp", a->path);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    fprintf(f, "# Cirmolo Sampler: stato salvato automaticamente\nversion=1\nbank=%d\nselected=%d\npattern=%d\nchain=%u\ntempo=%g\nswing=%g\nvolume=%g\nmetronome=%d\nclock=%d\nthreshold=%d\n",
            a->bank, a->sel, sampler_selected_pattern(a->s), sampler_chain(a->s), sampler_tempo(a->s), sampler_swing(a->s),
            sampler_volume(a->s), sampler_metronome(a->s), a->clock_follow, a->rec_thr);
    for (int p = 0; p < SP_PADS; p++) {
        const Sample *smp = sampler_sample(a->s, p);
        const PadParams *pp = sampler_pad(a->s, p);
        fprintf(f, "pad%d.file=%s\npad%d.volume=%g\npad%d.pitch=%g\npad%d.start=%g\npad%d.end=%g\npad%d.mode=%d\npad%d.reverse=%d\npad%d.group=%d\npad%d.mute=%d\npad%d.solo=%d\n",
                p, smp ? smp->path : "", p, pp->volume, p, pp->pitch, p, pp->start, p, pp->end, p, pp->mode, p, pp->reverse, p, pp->group, p, pp->mute, p, pp->solo);
    }
    for (int pi = 0; pi < SP_PATTERNS; pi++) {
        const SeqPattern *pat = sampler_pattern_at(a->s, pi);
        fprintf(f, "p%d.length=%d\n", pi, pat->length);
        for (int p = 0; p < SP_PADS; p++) {
            int any = 0;
            for (int i = 0; i < SP_STEPS; i++) any |= pat->vel[p][i];
            if (!any) continue;
            fprintf(f, "p%d.pad%d=", pi, p);
            for (int i = 0; i < SP_STEPS; i++) fprintf(f, "%d%s", pat->vel[p][i], i < SP_STEPS - 1 ? "," : "\n");
        }
    }
    fclose(f);
    remove(a->path);
    rename(tmp, a->path);
}

static void load_state(App *a)
{
    FILE *f = fopen(a->path, "r");
    if (!f) return;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq || line[0] == '#') continue;
        *eq = 0;
        char *k = line, *v = eq + 1;
        v[strcspn(v, "\r\n")] = 0;
        if (!strcmp(k, "bank")) a->bank = atoi(v) ? 1 : 0;
        else if (!strcmp(k, "selected")) { int p = atoi(v); if (p >= 0 && p < SP_PADS) a->sel = p; }
        else if (!strcmp(k, "pattern")) sampler_select_pattern(a->s, atoi(v));
        else if (!strcmp(k, "chain")) sampler_set_chain(a->s, (unsigned)atoi(v));
        else if (!strcmp(k, "tempo")) sampler_set_tempo(a->s, (float)atof(v));
        else if (!strcmp(k, "swing")) sampler_set_swing(a->s, (float)atof(v));
        else if (!strcmp(k, "volume")) sampler_set_volume(a->s, (float)atof(v));
        else if (!strcmp(k, "metronome")) sampler_set_metronome(a->s, atoi(v));
        else if (!strcmp(k, "clock")) a->clock_follow = atoi(v) != 0;
        else if (!strcmp(k, "threshold")) { int t = atoi(v); if (t >= 0 && t < 4) a->rec_thr = t; }
        else if (!strncmp(k, "pad", 3)) {
            int p = atoi(k + 3);
            const char *dot = strchr(k, '.');
            if (p < 0 || p >= SP_PADS || !dot) continue;
            PadParams *pp = sampler_pad(a->s, p);
            dot++;
            if (!strcmp(dot, "file")) { if (*v && app_assign_wav(a, p, v)) fprintf(stderr, "pad %d: campione non caricato: %s\n", p + 1, v); }
            else if (!strcmp(dot, "volume")) pp->volume = clampf((float)atof(v), 0.0f, 1.5f);
            else if (!strcmp(dot, "pitch")) pp->pitch = clampf((float)atof(v), -12.0f, 12.0f);
            else if (!strcmp(dot, "start")) pp->start = clampf((float)atof(v), 0.0f, 1.0f);
            else if (!strcmp(dot, "end")) pp->end = clampf((float)atof(v), 0.0f, 1.0f);
            else if (!strcmp(dot, "mode")) { int m = atoi(v); if (m >= 0 && m < MODE_COUNT) pp->mode = m; }
            else if (!strcmp(dot, "reverse")) pp->reverse = atoi(v) != 0;
            else if (!strcmp(dot, "group")) { int g = atoi(v); if (g >= 0 && g <= SP_GROUPS) pp->group = g; }
            else if (!strcmp(dot, "mute")) pp->mute = atoi(v) != 0;
            else if (!strcmp(dot, "solo")) pp->solo = atoi(v) != 0;
        } else if (k[0] == 'p' && k[1] >= '0' && k[1] <= '3' && k[2] == '.') {
            SeqPattern *pat = sampler_pattern_at(a->s, k[1] - '0');
            const char *key = k + 3;
            if (!strcmp(key, "length")) { int l = atoi(v); if (l >= 1 && l <= SP_STEPS) pat->length = l; }
            else if (!strncmp(key, "pad", 3)) {
                int p = atoi(key + 3);
                if (p < 0 || p >= SP_PADS) continue;
                char *tok = v;
                for (int i = 0; i < SP_STEPS && tok; i++) {
                    int vel = atoi(tok);
                    pat->vel[p][i] = (uint8_t)(vel < 0 ? 0 : (vel > 127 ? 127 : vel));
                    tok = strchr(tok, ',');
                    if (tok) tok++;
                }
            }
        }
    }
    fclose(f);
}

/* ------------------------------------------------------------------ creazione */
App *app_create(Sampler *s, const char *state_path)
{
    App *a = calloc(1, sizeof(App));
    if (!a) return NULL;
    a->s = s;
    if (state_path) snprintf(a->path, sizeof(a->path), "%s", state_path);
    snprintf(a->dir, sizeof(a->dir), "%s", a->path);
    char *slash = strrchr(a->dir, '/');
    char *bslash = strrchr(a->dir, '\\');
    if (bslash && (!slash || bslash > slash)) slash = bslash;
    if (slash) *slash = 0;
    else snprintf(a->dir, sizeof(a->dir), ".");
    snprintf(a->rec_dir, sizeof(a->rec_dir), "%s/campioni", a->dir);
    snprintf(a->exp_dir, sizeof(a->exp_dir), "%s/esportazioni", a->dir);
    snprintf(a->lib_root[0], sizeof(a->lib_root[0]), "%s", a->rec_dir);
    snprintf(a->lib_root[1], sizeof(a->lib_root[1]), "%s", LIB_STOCK);
    a->clock_follow = 1;
    a->cy = -1;
    for (int i = 0; i < 8; i++) a->btn_pad[i] = -1;
    for (int n = 0; n < 128; n++) a->chrom_slot[n] = -1;
    if (a->path[0]) load_state(a);
    return a;
}

void app_destroy(App *a)
{
    if (!a) return;
    release_all(a);
    app_save(a);
    free(a->entries);
    free(a);
}

int app_wants_quit(const App *a) { return a->quit; }
int app_page(const App *a) { return a->page; }
int app_metronome(const App *a) { return sampler_metronome(a->s); }
int app_clock_follow(const App *a) { return a->clock_follow; }
const char *app_last_recording(const App *a) { return a->last_rec; }
const char *app_last_export(const App *a) { return a->last_export; }

void app_set_library(App *a, const char *recordings_dir, const char *stock_dir)
{
    if (recordings_dir) snprintf(a->lib_root[0], sizeof(a->lib_root[0]), "%s", recordings_dir);
    if (stock_dir) snprintf(a->lib_root[1], sizeof(a->lib_root[1]), "%s", stock_dir);
    a->lib_level = 0;
    a->lib_sel = a->lib_scroll = 0;
}

/* ------------------------------------------------------------------ libreria */
static void lib_add(App *a, const char *name, int is_dir)
{
    if (a->n_entries == a->cap_entries) {
        int cap = a->cap_entries ? a->cap_entries * 2 : 64;
        LibEntry *e = realloc(a->entries, sizeof(LibEntry) * (size_t)cap);
        if (!e) return;
        a->entries = e;
        a->cap_entries = cap;
    }
    LibEntry *e = &a->entries[a->n_entries++];
    snprintf(e->name, sizeof(e->name), "%s", name);
    e->is_dir = is_dir;
}

static int entry_after(const LibEntry *x, const LibEntry *y)
{
    if (x->is_dir != y->is_dir) return !x->is_dir;        /* cartelle prima */
    return ci_cmp(x->name, y->name) > 0;
}

/* Legge la cartella corrente: sottocartelle e file .wav, in ordine alfabetico. */
static void lib_read(App *a)
{
    a->n_entries = 0;
    a->lib_sel = a->lib_scroll = 0;
    if (a->lib_level == 0) {
        lib_add(a, "Le mie registrazioni", 1);
        lib_add(a, "Libreria di serie (LittleGPTracker)", 1);
        return;
    }
    DIR *d = opendir(a->lib_dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char full[1100];
        snprintf(full, sizeof(full), "%s/%s", a->lib_dir, e->d_name);
        struct stat st;
        if (stat(full, &st)) continue;
        int is_dir = S_ISDIR(st.st_mode);
        if (!is_dir && !has_ext(e->d_name, ".wav")) continue;
        lib_add(a, e->d_name, is_dir);
    }
    closedir(d);
    for (int i = 1; i < a->n_entries; i++) {               /* inserzione: poche centinaia di voci */
        LibEntry k = a->entries[i];
        int j = i - 1;
        while (j >= 0 && entry_after(&a->entries[j], &k)) { a->entries[j + 1] = a->entries[j]; j--; }
        a->entries[j + 1] = k;
    }
}

static void lib_enter(App *a)
{
    if (a->lib_sel < 0 || a->lib_sel >= a->n_entries) return;
    const LibEntry *e = &a->entries[a->lib_sel];
    if (!e->is_dir) return;
    if (a->lib_level == 0) {
        a->lib_root_i = a->lib_sel;
        snprintf(a->lib_dir, sizeof(a->lib_dir), "%s", a->lib_root[a->lib_sel]);
    } else {
        char next[512];
        snprintf(next, sizeof(next), "%s/%s", a->lib_dir, e->name);
        snprintf(a->lib_dir, sizeof(a->lib_dir), "%s", next);
    }
    a->lib_level++;
    lib_read(a);
}

static void lib_up(App *a)
{
    if (a->lib_level <= 0) { app_set_page(a, PAGE_PADS); return; }
    a->lib_level--;
    char *slash = strrchr(a->lib_dir, '/');
    if (a->lib_level > 0 && slash) *slash = 0;
    lib_read(a);
}

static void lib_path(const App *a, char *out, size_t n)
{
    const LibEntry *e = &a->entries[a->lib_sel];
    snprintf(out, n, "%s/%s", a->lib_dir, e->name);
}

static void lib_preview(App *a)
{
    if (a->lib_sel < 0 || a->lib_sel >= a->n_entries) return;
    if (a->entries[a->lib_sel].is_dir) { lib_enter(a); return; }
    char p[1100];
    lib_path(a, p, sizeof(p));
    Sample *smp = sample_load_wav(p);
    if (!smp) { toast(a, "File non leggibile: non e' un WAV PCM"); return; }
    sampler_pad_defaults(sampler_pad(a->s, SP_PREVIEW));
    sampler_set_sample(a->s, SP_PREVIEW, smp);
    sampler_trigger(a->s, SP_PREVIEW, 0.9f, 0.0f, -1);
}

static void lib_assign(App *a)
{
    if (a->lib_sel < 0 || a->lib_sel >= a->n_entries || a->entries[a->lib_sel].is_dir) return;
    char p[1100], t[128], lbl[8];
    lib_path(a, p, sizeof(p));
    if (app_assign_wav(a, a->sel, p)) { toast(a, "File non leggibile: non e' un WAV PCM"); return; }
    pad_label(a->sel, lbl, sizeof(lbl));
    snprintf(t, sizeof(t), "%s nel pad %s", sampler_sample(a->s, a->sel)->name, lbl);
    toast(a, t);
    app_save(a);
}

/* ------------------------------------------------------------------ registrazione */
void app_capture(App *a, const float *in, int frames) { sampler_capture_feed(a->s, in, frames); }

void app_capture_status(App *a, const char *device, float rate)
{
    int was = a->mic_name[0] != 0;
    snprintf(a->mic_name, sizeof(a->mic_name), "%s", device ? device : "");
    sampler_capture_set_rate(a->s, device ? rate : 0.0f);
    if (!device) {
        sampler_rec_stop(a->s);                        /* quel che c'e' viene salvato */
        if (was) toast(a, "Microfono scollegato");
    } else {
        char t[96];
        snprintf(t, sizeof(t), "Microfono: %s", device);
        toast(a, t);
    }
}

static void rec_toggle(App *a)
{
    int st = sampler_rec_state(a->s);
    if (st == REC_ON || st == REC_WAIT) { sampler_rec_stop(a->s); return; }
    if (st != REC_IDLE) return;
    if (!a->mic_name[0]) { toast(a, "Microfono USB non collegato"); return; }
    sampler_rec_start(a->s, THRESHOLDS[a->rec_thr]);
    toast(a, a->rec_thr ? "In attesa del suono..." : "Registrazione in corso");
}

/* Chiamata quando la registrazione e' finita: salva il WAV e lo mette nel pad scelto. */
static void rec_finish(App *a)
{
    int frames = 0;
    const float *buf = sampler_rec_take(a->s, &frames);
    if (!buf) return;
    if (frames < SP_RATE / 100) { toast(a, "Registrazione vuota"); return; }
    make_dir(a->dir);
    make_dir(a->rec_dir);
    char path[600];
    free_name(a->rec_dir, "reg", path, sizeof(path));
    if (wav_write16(path, buf, frames, 1, SP_RATE)) { toast(a, "Impossibile scrivere la registrazione"); return; }
    snprintf(a->last_rec, sizeof(a->last_rec), "%s", path);
    char t[128], lbl[8];
    pad_label(a->sel, lbl, sizeof(lbl));
    if (app_assign_wav(a, a->sel, path)) { toast(a, "Registrazione salvata, ma non caricata"); return; }
    snprintf(t, sizeof(t), "Registrati %.1f s nel pad %s", (double)frames / SP_RATE, lbl);
    toast(a, t);
    app_save(a);
}

/* ------------------------------------------------------------------ esportazione */
int app_export_pattern(App *a)
{
    int frames = 0;
    float *buf = sampler_render_pattern(a->s, &frames);
    if (!buf || frames < 1) { free(buf); toast(a, "Niente da esportare: il pattern e' vuoto"); return -1; }
    make_dir(a->dir);
    make_dir(a->exp_dir);
    char prefix[24], path[600];
    unsigned chain = sampler_chain(a->s);
    if (chain) {
        snprintf(prefix, sizeof(prefix), "catena-");
        for (int p = 0; p < SP_PATTERNS; p++) if (chain & (1u << p)) { size_t l = strlen(prefix); prefix[l] = (char)('A' + p); prefix[l + 1] = 0; }
    } else snprintf(prefix, sizeof(prefix), "pattern-%c", 'A' + sampler_selected_pattern(a->s));
    free_name(a->exp_dir, prefix, path, sizeof(path));
    int r = wav_write16(path, buf, frames, 2, (int)sampler_sample_rate(a->s));
    free(buf);
    if (r) { toast(a, "Impossibile scrivere il file"); return -1; }
    snprintf(a->last_export, sizeof(a->last_export), "%s", path);
    char t[128];
    const char *base = strrchr(path, '/');
    snprintf(t, sizeof(t), "Esportato %s (%.1f s)", base ? base + 1 : path, (double)frames / sampler_sample_rate(a->s));
    toast(a, t);
    return 0;
}

/* ------------------------------------------------------------------ modifica del pad */
static void ed_text(const App *a, int i, char *out, size_t n)
{
    const PadParams *p = sampler_pad(a->s, a->sel);
    switch (i) {
    case ED_VOLUME: snprintf(out, n, "%d %%", (int)lrintf(p->volume * 100)); break;
    case ED_PITCH: snprintf(out, n, "%+.1f st", (double)p->pitch); break;
    case ED_START: snprintf(out, n, "%d %%", (int)lrintf(p->start * 100)); break;
    case ED_END: snprintf(out, n, "%d %%", (int)lrintf(p->end * 100)); break;
    case ED_MODE: snprintf(out, n, "%s", MODE_NAMES[p->mode]); break;
    case ED_REVERSE: snprintf(out, n, "%s", p->reverse ? "Sì" : "No"); break;
    case ED_GROUP: if (p->group) snprintf(out, n, "%d", p->group); else snprintf(out, n, "nessuno"); break;
    case ED_MUTE: snprintf(out, n, "%s", p->mute ? "Sì" : "No"); break;
    case ED_SOLO: snprintf(out, n, "%s", p->solo ? "Sì" : "No"); break;
    default: out[0] = 0;
    }
}

static void ed_adjust(App *a, int i, int dir, int big)
{
    PadParams *p = sampler_pad(a->s, a->sel);
    float k = big ? 10.0f : 1.0f;
    switch (i) {
    case ED_VOLUME: p->volume = clampf(roundf((p->volume + dir * 0.05f * (big ? 4.0f : 1.0f)) * 20.0f) / 20.0f, 0.0f, 1.5f); break;
    case ED_PITCH: p->pitch = clampf(roundf((p->pitch + dir * (big ? 12.0f : 1.0f)) * 10.0f) / 10.0f, -12.0f, 12.0f); break;
    case ED_START: p->start = clampf(roundf((p->start + dir * 0.01f * k) * 100.0f) / 100.0f, 0.0f, 0.99f); if (p->end < p->start + 0.01f) p->end = clampf(p->start + 0.01f, 0.0f, 1.0f); break;
    case ED_END: p->end = clampf(roundf((p->end + dir * 0.01f * k) * 100.0f) / 100.0f, 0.01f, 1.0f); if (p->start > p->end - 0.01f) p->start = clampf(p->end - 0.01f, 0.0f, 1.0f); break;
    case ED_MODE: p->mode = (p->mode + dir + MODE_COUNT) % MODE_COUNT; break;
    case ED_REVERSE: p->reverse = !p->reverse; break;
    case ED_GROUP: p->group = (p->group + dir + SP_GROUPS + 1) % (SP_GROUPS + 1); break;
    case ED_MUTE: p->mute = !p->mute; break;
    case ED_SOLO: p->solo = !p->solo; break;
    }
}

static void pitch_nudge(App *a, int dir)
{
    PadParams *p = sampler_pad(a->s, a->sel);
    p->pitch = clampf(roundf((p->pitch + dir * 0.1f) * 10.0f) / 10.0f, -12.0f, 12.0f);
}

/* ------------------------------------------------------------------ opzioni */
static void opt_text(const App *a, int i, char *out, size_t n)
{
    switch (i) {
    case OPT_TEMPO: snprintf(out, n, "%d BPM", (int)lrintf(sampler_tempo(a->s))); break;
    case OPT_SWING: snprintf(out, n, "%d %%", (int)lrintf(sampler_swing(a->s) * 100)); break;
    case OPT_METRO: snprintf(out, n, "%s", sampler_metronome(a->s) ? "Sì" : "No"); break;
    case OPT_VOLUME: snprintf(out, n, "%d %%", (int)lrintf(sampler_volume(a->s) * 100)); break;
    case OPT_CLOCK: snprintf(out, n, "%s", a->clock_follow ? "Sì" : "No"); break;
    case OPT_EXPORT: {
        unsigned chain = sampler_chain(a->s);
        if (chain) snprintf(out, n, "catena (A)");
        else snprintf(out, n, "pattern %c (A)", 'A' + sampler_selected_pattern(a->s));
        break;
    }
    default: out[0] = 0;
    }
}

static void opt_adjust(App *a, int i, int dir, int big)
{
    switch (i) {
    case OPT_TEMPO: sampler_set_tempo(a->s, roundf(sampler_tempo(a->s)) + dir * (big ? 10 : 1)); break;
    case OPT_SWING: sampler_set_swing(a->s, roundf((sampler_swing(a->s) + dir * 0.05f) * 20.0f) / 20.0f); break;
    case OPT_METRO: sampler_set_metronome(a->s, !sampler_metronome(a->s)); break;
    case OPT_VOLUME: sampler_set_volume(a->s, roundf((sampler_volume(a->s) + dir * 0.05f) * 20.0f) / 20.0f); break;
    case OPT_CLOCK: a->clock_follow = !a->clock_follow; break;
    case OPT_EXPORT: break;
    }
}

/* ------------------------------------------------------------------ trasporto */
static void transport_toggle(App *a)
{
    int on = !sampler_playing(a->s);
    sampler_play(a->s, on);
    if (!on && sampler_record(a->s)) sampler_set_record(a->s, 0);
    toast(a, on ? "Sequencer in moto" : "Sequencer fermo");
}

static void live_rec_toggle(App *a)
{
    int on = !sampler_record(a->s);
    sampler_set_record(a->s, on);
    if (on && !sampler_playing(a->s)) sampler_play(a->s, 1);
    toast(a, on ? "Registrazione dal vivo: i colpi finiscono nel pattern" : "Registrazione dal vivo spenta");
}

/* ------------------------------------------------------------------ input */
void app_set_page(App *a, int page)
{
    page = (page % PAGE_COUNT + PAGE_COUNT) % PAGE_COUNT;
    if (page == a->page) return;
    for (int i = 0; i < 8; i++) if (a->btn_pad[i] >= 0) { sampler_release(a->s, a->btn_pad[i], -1); a->btn_pad[i] = -1; }
    a->page = page;
    if (page == PAGE_LIB) lib_read(a);
    if (page == PAGE_EDIT) sampler_set_cutoff(a->s, 1.0f);   /* la levetta destra qui ritocca l'intonazione */
}

static int repeats(int page, int b)
{
    if (page == PAGE_PADS) return 0;
    if (page == PAGE_SEQ) return b == PAD_L1 || b == PAD_R1;
    return b == PAD_UP || b == PAD_DOWN || b == PAD_LEFT || b == PAD_RIGHT;
}

static void pads_button(App *a, int b)
{
    switch (b) {
    case PAD_L1: a->bank = 0; return;
    case PAD_R1: a->bank = 1; return;
    case PAD_R2: live_rec_toggle(a); return;
    case PAD_L2: return;
    }
    for (int i = 0; i < 8; i++) {
        if (PAD_BTNS[i] != b) continue;
        int pad = a->bank * 8 + i;
        if (a->down[PAD_L2]) { a->sel = pad; return; }     /* L2 tenuto: sceglie senza suonare */
        a->btn_pad[i] = pad;
        play_pad(a, pad, 1.0f);
        return;
    }
}

static void edit_button(App *a, int b)
{
    switch (b) {
    case PAD_UP: a->edit_sel = (a->edit_sel + ED_COUNT - 1) % ED_COUNT; break;
    case PAD_DOWN: a->edit_sel = (a->edit_sel + 1) % ED_COUNT; break;
    case PAD_LEFT: ed_adjust(a, a->edit_sel, -1, 0); break;
    case PAD_RIGHT: ed_adjust(a, a->edit_sel, 1, 0); break;
    case PAD_L1: ed_adjust(a, a->edit_sel, -1, 1); break;
    case PAD_R1: ed_adjust(a, a->edit_sel, 1, 1); break;
    case PAD_L2: a->sel = (a->sel + SP_PADS - 1) % SP_PADS; break;
    case PAD_R2: a->sel = (a->sel + 1) % SP_PADS; break;
    case PAD_A: sampler_trigger(a->s, a->sel, 1.0f, 0.0f, -1); break;
    case PAD_Y:
        if (sampler_sample(a->s, a->sel)) { sampler_set_sample(a->s, a->sel, NULL); toast(a, "Pad svuotato"); app_save(a); }
        break;
    case PAD_B: app_set_page(a, PAGE_PADS); break;
    }
}

static void pattern_row_button(App *a, int b)
{
    int sel = sampler_selected_pattern(a->s);
    char t[64];
    switch (b) {
    case PAD_LEFT: sampler_select_pattern(a->s, sel - 1); break;
    case PAD_RIGHT: sampler_select_pattern(a->s, sel + 1); break;
    case PAD_A:
        sampler_set_chain(a->s, sampler_chain(a->s) ^ (1u << sel));
        snprintf(t, sizeof(t), "Pattern %c %s la catena", 'A' + sel, (sampler_chain(a->s) >> sel) & 1 ? "entra nella" : "esce dalla");
        toast(a, t);
        break;
    case PAD_B: {
        int dst = (sel + 1) % SP_PATTERNS;
        *sampler_pattern_at(a->s, dst) = *sampler_pattern_at(a->s, sel);
        snprintf(t, sizeof(t), "Pattern %c copiato in %c", 'A' + sel, 'A' + dst);
        toast(a, t);
        break;
    }
    case PAD_Y:
        sampler_pattern_clear(sampler_pattern(a->s));
        snprintf(t, sizeof(t), "Pattern %c svuotato", 'A' + sel);
        toast(a, t);
        break;
    }
}

static void seq_button(App *a, int b)
{
    SeqPattern *p = sampler_pattern(a->s);
    int x = a->cx, row = a->cy;
    if (b == PAD_UP) { a->cy = a->cy <= -1 ? SP_PADS - 1 : a->cy - 1; return; }
    if (b == PAD_DOWN) { a->cy = a->cy >= SP_PADS - 1 ? -1 : a->cy + 1; return; }
    if (b == PAD_L1) { sampler_set_tempo(a->s, roundf(sampler_tempo(a->s)) - 1); return; }
    if (b == PAD_R1) { sampler_set_tempo(a->s, roundf(sampler_tempo(a->s)) + 1); return; }
    if (b == PAD_L2) { if (p->length > 1) p->length--; return; }
    if (b == PAD_R2) { if (p->length < SP_STEPS) p->length++; return; }
    if (row < 0) { pattern_row_button(a, b); return; }
    switch (b) {
    case PAD_LEFT: a->cx = (a->cx + SP_STEPS - 1) % SP_STEPS; break;
    case PAD_RIGHT: a->cx = (a->cx + 1) % SP_STEPS; break;
    case PAD_A:
        p->vel[row][x] = p->vel[row][x] ? 0 : 100;
        if (p->vel[row][x] && !sampler_playing(a->s)) sampler_trigger(a->s, row, 100 / 127.0f, 0.0f, -1);
        a->sel = row;
        break;
    case PAD_B:
        if (p->vel[row][x]) p->vel[row][x] = p->vel[row][x] >= 127 ? 64 : (p->vel[row][x] >= 100 ? 127 : 100);
        break;
    case PAD_X: sampler_pad(a->s, row)->mute = !sampler_pad(a->s, row)->mute; break;
    case PAD_Y: sampler_pad(a->s, row)->solo = !sampler_pad(a->s, row)->solo; break;
    }
}

static void lib_button(App *a, int b)
{
    int n = a->n_entries;
    switch (b) {
    case PAD_UP: if (n) a->lib_sel = (a->lib_sel + n - 1) % n; break;
    case PAD_DOWN: if (n) a->lib_sel = (a->lib_sel + 1) % n; break;
    case PAD_LEFT: a->lib_sel = a->lib_sel - 8 < 0 ? 0 : a->lib_sel - 8; break;
    case PAD_RIGHT: a->lib_sel = a->lib_sel + 8 >= n ? n - 1 : a->lib_sel + 8; if (a->lib_sel < 0) a->lib_sel = 0; break;
    case PAD_A: lib_preview(a); break;
    case PAD_X: lib_assign(a); break;
    case PAD_B: lib_up(a); break;
    case PAD_L1: a->sel = (a->sel + SP_PADS - 1) % SP_PADS; break;
    case PAD_R1: a->sel = (a->sel + 1) % SP_PADS; break;
    }
}

static void rec_button(App *a, int b)
{
    switch (b) {
    case PAD_A: rec_toggle(a); break;
    case PAD_LEFT: a->rec_thr = (a->rec_thr + 3) % 4; break;
    case PAD_RIGHT: a->rec_thr = (a->rec_thr + 1) % 4; break;
    case PAD_L1: a->sel = (a->sel + SP_PADS - 1) % SP_PADS; break;
    case PAD_R1: a->sel = (a->sel + 1) % SP_PADS; break;
    case PAD_X: sampler_trigger(a->s, a->sel, 1.0f, 0.0f, -1); break;
    case PAD_B: app_set_page(a, PAGE_PADS); break;
    }
}

static void options_button(App *a, int b)
{
    switch (b) {
    case PAD_UP: a->opt_sel = (a->opt_sel + OPT_COUNT - 1) % OPT_COUNT; break;
    case PAD_DOWN: a->opt_sel = (a->opt_sel + 1) % OPT_COUNT; break;
    case PAD_LEFT: opt_adjust(a, a->opt_sel, -1, 0); break;
    case PAD_RIGHT: opt_adjust(a, a->opt_sel, 1, 0); break;
    case PAD_L1: opt_adjust(a, a->opt_sel, -1, 1); break;
    case PAD_R1: opt_adjust(a, a->opt_sel, 1, 1); break;
    case PAD_A:
        if (a->opt_sel == OPT_EXPORT) app_export_pattern(a);
        else if (a->opt_sel == OPT_METRO || a->opt_sel == OPT_CLOCK) opt_adjust(a, a->opt_sel, 1, 0);
        break;
    case PAD_B: app_set_page(a, PAGE_PADS); break;
    }
}

static void handle_press(App *a, int b)
{
    if (a->quit_dialog) {
        if (b == PAD_A || b == PAD_DOWN) { a->quit = 1; release_all(a); }
        else if (b == PAD_B || b == PAD_UP || b == PAD_MENU) a->quit_dialog = 0;
        return;
    }
    switch (b) {
    case PAD_MENU: a->quit_dialog = 1; release_all(a); return;
    case PAD_SELECT: app_set_page(a, a->page + 1); return;
    case PAD_START: transport_toggle(a); return;
    }
    switch (a->page) {
    case PAGE_PADS: pads_button(a, b); break;
    case PAGE_EDIT: edit_button(a, b); break;
    case PAGE_SEQ: seq_button(a, b); break;
    case PAGE_LIB: lib_button(a, b); break;
    case PAGE_REC: rec_button(a, b); break;
    default: options_button(a, b); break;
    }
}

void app_button(App *a, int b, int pressed)
{
    if (b < 0 || b >= PAD_COUNT) return;
    if (pressed) {
        if (a->down[b]) app_button(a, b, 0);   /* mai un tasto bloccato come premuto: prima lo rilascia */
        a->down[b] = 1;
        a->rep[b] = 0.0f;
        handle_press(a, b);
    } else {
        if (!a->down[b]) return;
        a->down[b] = 0;
        if (a->quit_dialog) return;
        if (a->page == PAGE_PADS) {
            for (int i = 0; i < 8; i++)
                if (PAD_BTNS[i] == b && a->btn_pad[i] >= 0) { sampler_release(a->s, a->btn_pad[i], -1); a->btn_pad[i] = -1; }
        } else if ((a->page == PAGE_EDIT || a->page == PAGE_REC) && (b == PAD_A || b == PAD_X)) {
            sampler_release(a->s, a->sel, -1);
        }
    }
}

static float deadzone(float v)
{
    if (fabsf(v) < 0.15f) return 0.0f;
    return (v - (v > 0 ? 0.15f : -0.15f)) / 0.85f;
}

void app_axes(App *a, float lx, float ly, float rx, float ry, float l2, float r2)
{
    a->lx = deadzone(lx);
    a->ry = deadzone(ry);
    sampler_set_bend(a->s, a->lx * 2.0f);
    if (a->page == PAGE_EDIT) {
        /* levetta destra in su/giu': intonazione di un decimo alla volta, una per ogni spinta */
        if (fabsf(a->ry) > 0.6f && !a->nudge) { a->nudge = a->ry < 0 ? 1 : -1; pitch_nudge(a, a->nudge); }
        else if (fabsf(a->ry) < 0.3f) a->nudge = 0;
    } else {
        a->nudge = 0;
        sampler_set_cutoff(a->s, a->ry < 0 ? 1.0f + a->ry : 1.0f);   /* in su chiude il filtro */
    }
}

/* ------------------------------------------------------------------ MIDI */
void app_midi_status(App *a, const char *device)
{
    int was = a->midi_name[0] != 0;
    snprintf(a->midi_name, sizeof(a->midi_name), "%s", device ? device : "");
    if (!device) {
        if (was) { release_all(a); toast(a, "MIDI scollegato"); }
    } else {
        char t[96];
        snprintf(t, sizeof(t), "MIDI collegato: %s", device);
        toast(a, t);
    }
}

static void realtime(App *a, int st)
{
    switch (st) {
    case 0xF8:                                    /* clock: 24 per semiminima */
        if (a->clock_seen > 0.5f || !a->clock_ticks) { a->clock_ticks = 0; a->clock_start = a->time; }
        a->clock_seen = 0.0f;
        if (++a->clock_ticks >= 97) {
            float span = a->time - a->clock_start;
            if (span > 0.2f) {
                float bpm = 60.0f * (float)(a->clock_ticks - 1) / 24.0f / span;
                a->clock_bpm = a->clock_bpm > 0.0f ? 0.6f * a->clock_bpm + 0.4f * bpm : bpm;
                if (a->clock_follow && fabsf(roundf(a->clock_bpm) - sampler_tempo(a->s)) >= 1.0f)
                    sampler_set_tempo(a->s, roundf(a->clock_bpm));
            }
            a->clock_ticks = 1;
            a->clock_start = a->time;
        }
        break;
    case 0xFA: sampler_play(a->s, 1); break;
    case 0xFB: sampler_resume(a->s); break;
    case 0xFC: sampler_play(a->s, 0); break;
    }
}

static void midi_cc(App *a, int cc, int v)
{
    PadParams *p = sampler_pad(a->s, a->sel);
    switch (cc) {
    case 64: a->sustain = v >= 64; sampler_set_sustain(a->s, a->sustain); break;
    case 7: sampler_set_volume(a->s, v / 127.0f); break;
    case 70: p->volume = v / 127.0f * 1.5f; break;
    case 71: p->pitch = roundf((v / 127.0f * 24.0f - 12.0f) * 10.0f) / 10.0f; break;
    case 72: p->start = clampf(v / 127.0f, 0.0f, 0.99f); if (p->end < p->start + 0.01f) p->end = p->start + 0.01f; break;
    case 73: p->end = clampf(v / 127.0f, 0.01f, 1.0f); if (p->start > p->end - 0.01f) p->start = p->end - 0.01f; break;
    case 74: sampler_set_tempo(a->s, 40.0f + roundf(v / 127.0f * 200.0f)); break;
    case 75: sampler_set_swing(a->s, v / 127.0f * 0.6f); break;
    case 76: sampler_set_volume(a->s, v / 127.0f); break;
    case 77: sampler_set_cutoff(a->s, v / 127.0f); break;
    case 120: case 123: release_all(a); break;
    }
}

void app_midi(App *a, const unsigned char *m, int len)
{
    if (len < 1) return;
    int st = m[0];
    if (st >= 0xF8) { realtime(a, st); return; }
    if (st >= 0xF0) return;
    int type = MIDI_TYPE(m), ch = MIDI_CHANNEL(m);
    int d1 = len > 1 ? m[1] : 0, d2 = len > 2 ? m[2] : 0;
    a->midi_flash = 1.0f;
    switch (type) {
    case MIDI_NOTE_ON:
    case MIDI_NOTE_OFF: {
        int on = type == MIDI_NOTE_ON && d2 > 0;
        if (ch == 9) {                                      /* pad della MPK: canale 10, note 36-51 */
            int pad = d1 - 36;
            if (pad < 0 || pad >= SP_PADS) return;
            if (on) play_pad(a, pad, d2 / 127.0f);
            else sampler_release(a->s, pad, -1);
        } else {                                            /* tasti: il pad scelto, cromatico attorno al Do4 */
            if (on) {
                if (a->chrom_slot[d1] >= 0) sampler_release(a->s, a->chrom_slot[d1], d1);
                a->chrom_slot[d1] = a->sel;
                sampler_trigger(a->s, a->sel, d2 / 127.0f, (float)(d1 - 60), d1);
            } else if (a->chrom_slot[d1] >= 0) {
                sampler_release(a->s, a->chrom_slot[d1], d1);
                a->chrom_slot[d1] = -1;
            }
        }
        break;
    }
    case MIDI_CC: midi_cc(a, d1, d2); break;
    case MIDI_PITCH_BEND: sampler_set_bend(a->s, (((d2 << 7) | d1) - 8192) / 8192.0f * 2.0f); break;
    case MIDI_PROGRAM:
        if (ch == 9) a->bank = d1 & 1;
        else sampler_select_pattern(a->s, d1 % SP_PATTERNS);
        break;
    default: break;
    }
}

/* ------------------------------------------------------------------ aggiornamento */
void app_update(App *a, float dt)
{
    a->time += dt;
    a->toast_t = fmaxf(0.0f, a->toast_t - dt);
    a->midi_flash = fmaxf(0.0f, a->midi_flash - dt * 4.0f);
    a->clock_seen += dt;
    if (a->clock_seen > 1.0f) { a->clock_ticks = 0; a->clock_bpm = 0.0f; }
    for (int p = 0; p < SP_PADS; p++) {
        uint32_t f = sampler_slot_flash(a->s, p);
        if (f != a->flash_seen[p]) { a->flash_seen[p] = f; a->glow[p] = 1.0f; }
        a->glow[p] = fmaxf(0.0f, a->glow[p] - dt * 4.0f);
    }
    sampler_collect(a->s);
    if (sampler_rec_state(a->s) == REC_DONE) rec_finish(a);
    if (a->quit_dialog) return;
    for (int b = 0; b < PAD_COUNT; b++) {
        if (!a->down[b] || !repeats(a->page, b)) continue;
        a->rep[b] += dt;
        while (a->rep[b] > 0.38f) {
            a->rep[b] -= 0.06f;
            handle_press(a, b);
        }
    }
}

static uint32_t fnv(uint32_t h, const void *p, size_t n)
{
    const unsigned char *b = p;
    while (n--) { h ^= *b++; h *= 16777619u; }
    return h;
}

/* Si ridisegna quando qualcosa si muove (suono, luci, sequencer, registrazione, messaggi) o quando cambia
   lo stato mostrato; comunque una volta al secondo. */
int app_needs_draw(App *a)
{
    if (a->toast_t > 0.0f || a->midi_flash > 0.0f || a->quit_dialog) return 1;
    if (sampler_active_voices(a->s) > 0 || sampler_peak(a->s) > 0.002f) return 1;
    for (int p = 0; p < SP_PADS; p++) if (a->glow[p] > 0.0f) return 1;
    int rs = sampler_rec_state(a->s);
    if (rs == REC_ON || rs == REC_WAIT) return 1;
    if (a->page == PAGE_REC && a->mic_name[0]) return 1;        /* il misuratore di livello si muove */
    int v[] = { a->page, a->sel, a->bank, a->edit_sel, a->cx, a->cy, a->lib_sel, a->lib_scroll, a->lib_level, a->opt_sel, a->rec_thr,
                a->clock_follow, sampler_selected_pattern(a->s), (int)sampler_chain(a->s), sampler_metronome(a->s), sampler_record(a->s), a->sustain,
                sampler_current_step(a->s), sampler_playing_pattern(a->s) };
    uint32_t h = fnv(2166136261u, v, sizeof(v));
    float fv[] = { sampler_tempo(a->s), sampler_swing(a->s), sampler_volume(a->s) };
    h = fnv(h, fv, sizeof(fv));
    for (int p = 0; p < SP_PADS; p++) {
        h = fnv(h, sampler_pad(a->s, p), sizeof(PadParams));
        const Sample *smp = sampler_sample(a->s, p);
        h = fnv(h, &smp, sizeof(smp));
    }
    for (int p = 0; p < SP_PATTERNS; p++) h = fnv(h, sampler_pattern_at(a->s, p), sizeof(SeqPattern));
    h = fnv(h, a->down, sizeof(a->down));
    h = fnv(h, a->midi_name, strlen(a->midi_name));
    h = fnv(h, a->mic_name, strlen(a->mic_name));
    if (h == a->drawn_key && a->time - a->drawn_time < 1.0f) return 0;
    a->drawn_key = h;
    a->drawn_time = a->time;
    return 1;
}

/* ------------------------------------------------------------------ disegno */
/* Logo: quattro pad con una piccola onda sopra. */
static void draw_logo(Canvas *c, float x, float y, float s)
{
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++)
            gfx_round_rect(c, x - 11 * s + i * 12 * s, y - 2 * s + j * 8 * s, 10 * s, 6 * s, 1.5f * s, (i + j) % 2 ? C_VIOLET : C_VIOLET_L, 1.0f);
    gfx_line(c, x - 11 * s, y - 7 * s, x - 6 * s, y - 12 * s, 1.5f * s, C_AMBER, 1.0f);
    gfx_line(c, x - 6 * s, y - 12 * s, x - 1 * s, y - 4 * s, 1.5f * s, C_AMBER, 1.0f);
    gfx_line(c, x - 1 * s, y - 4 * s, x + 5 * s, y - 11 * s, 1.5f * s, C_AMBER, 1.0f);
    gfx_line(c, x + 5 * s, y - 11 * s, x + 11 * s, y - 7 * s, 1.5f * s, C_AMBER, 1.0f);
}

static void draw_header(App *a, Canvas *c)
{
    gfx_rect(c, 0, 0, c->w, 46, C_BAR);
    gfx_rect(c, 0, 46, c->w, 1, C_LINE);
    draw_logo(c, 26, 23, 1.1f);
    gfx_text(c, FONT_TITLE, 48, 31, "Sampler", C_TEXT);
    static const char *tabs[PAGE_COUNT] = { "Pad", "Modifica", "Sequenza", "Libreria", "Registra", "Opzioni" };
    int x = 142;
    for (int i = 0; i < PAGE_COUNT; i++) {
        int w = gfx_text_width(FONT_SMALL, tabs[i]) + 16;
        if (i == a->page) gfx_round_rect(c, x, 11, w, 24, 12, C_VIOLET_D, 1.0f);
        gfx_text_center(c, FONT_SMALL, x + w / 2, 28, tabs[i], i == a->page ? C_TEXT : C_MUTED);
        x += w + 2;
    }
    int connected = a->midi_name[0] != 0;
    gfx_circle(c, 578, 23, 5.0f, connected ? gfx_mix(C_GREEN, C_TEXT, a->midi_flash) : C_DIM, 1.0f);
    gfx_text(c, FONT_SMALL, 588, 28, "MIDI", connected ? C_TEXT : C_DIM);
}

static void draw_hints(Canvas *c, const char *text)
{
    gfx_rect(c, 0, 446, c->w, 34, C_BAR);
    gfx_rect(c, 0, 446, c->w, 1, C_LINE);
    gfx_text_center(c, FONT_SMALL, c->w / 2, 468, text, C_MUTED);
}

static void draw_scope(App *a, Canvas *c, int x, int y, int w, int h, uint32_t color)
{
    gfx_round_rect(c, x, y, w, h, 10, C_SCOPE_BG, 1.0f);
    gfx_line(c, x + 8, y + h / 2.0f, x + w - 8, y + h / 2.0f, 1.0f, C_LINE, 0.8f);
    int n = w - 16;
    if (n > 600) n = 600;
    sampler_scope(a->s, a->scope, n * 2);
    int start = 0;
    for (int i = 1; i < n; i++)
        if (a->scope[i - 1] <= 0.0f && a->scope[i] > 0.0f) { start = i; break; }
    float peak = 0.05f;
    for (int i = 0; i < n; i++) peak = fmaxf(peak, fabsf(a->scope[start + i]));
    float gain = (h * 0.42f) / fmaxf(peak, 0.25f);
    float px = 0, py = 0;
    for (int i = 0; i < n; i++) {
        float xx = x + 8 + i, yy = y + h / 2.0f - a->scope[start + i] * gain;
        if (i) {
            gfx_line(c, px, py, xx, yy, 4.0f, color, 0.18f);
            gfx_line(c, px, py, xx, yy, 1.6f, color, 1.0f);
        }
        px = xx;
        py = yy;
    }
}

/* Testo tagliato per stare in maxw pixel (con "…" in coda). */
static void fit_text(int font, const char *s, int maxw, char *out, size_t n)
{
    snprintf(out, n, "%s", s);
    if (gfx_text_width(font, out) <= maxw) return;
    size_t l = strlen(out);
    while (l > 1) {
        l--;
        while (l > 0 && (out[l] & 0xC0) == 0x80) l--;       /* non spezzare un carattere UTF-8 */
        out[l] = 0;
        char t[256];
        snprintf(t, sizeof(t), "%s…", out);
        if (gfx_text_width(font, t) <= maxw) { snprintf(out, n, "%s", t); return; }
    }
}

static void draw_step_dots(App *a, Canvas *c, int x, int y, int w)
{
    int step = sampler_current_step(a->s), len = sampler_pattern(a->s)->length;
    int pp = sampler_playing_pattern(a->s);
    if (pp >= 0) len = sampler_pattern_at(a->s, pp)->length;
    float dw = (float)w / SP_STEPS;
    for (int i = 0; i < SP_STEPS; i++) {
        uint32_t col = i >= len ? C_PANEL : (i % 4 == 0 ? C_DIM : C_PANEL_HI);
        if (i == step) col = C_GREEN;
        gfx_round_rect(c, x + i * dw + 1, y, dw - 2, 8, 3, col, 1.0f);
    }
}

static void draw_pads(App *a, Canvas *c)
{
    for (int p = 0; p < SP_PADS; p++) {
        int row = p / 4, col = p % 4;
        int x = 16 + col * 94, y = 56 + row * 72, w = 90, h = 68;
        int bank_row = row / 2 == a->bank, lit = sampler_slot_voices(a->s, p) > 0;
        const Sample *smp = sampler_sample(a->s, p);
        const PadParams *pp = sampler_pad(a->s, p);
        uint32_t fill = lit ? C_VIOLET : (bank_row ? C_PANEL_HI : C_PANEL);
        fill = gfx_mix(fill, C_VIOLET_L, a->glow[p] * 0.6f);
        gfx_round_rect(c, x, y, w, h, 10, fill, 1.0f);
        if (p == a->sel) gfx_round_frame(c, x, y, w, h, 10, 2.5f, C_AMBER, 1.0f);
        else if (bank_row) gfx_round_frame(c, x, y, w, h, 10, 1.0f, C_LINE, 1.0f);
        char lbl[8], t[64];
        pad_label(p, lbl, sizeof(lbl));
        gfx_text(c, FONT_SMALL, x + 8, y + 18, lbl, bank_row ? C_AMBER : C_DIM);
        gfx_text_right(c, FONT_SMALL, x + w - 8, y + 18, PAD_BTN_LABELS[p % 8], bank_row ? C_TEXT : C_DIM);
        if (smp) {
            fit_text(FONT_BOLD, smp->name, w - 12, t, sizeof(t));
            gfx_text_center(c, FONT_BOLD, x + w / 2, y + 42, t, lit ? C_TEXT : (bank_row ? C_TEXT : C_MUTED));
            if (fabsf(pp->pitch) > 0.05f) snprintf(t, sizeof(t), "%s %+.1f", MODE_SHORT[pp->mode], (double)pp->pitch);
            else snprintf(t, sizeof(t), "%s", MODE_SHORT[pp->mode]);
            if (pp->mute) strncat(t, " · muto", sizeof(t) - strlen(t) - 1);
            else if (pp->solo) strncat(t, " · solo", sizeof(t) - strlen(t) - 1);
            gfx_text_center(c, FONT_SMALL, x + w / 2, y + 60, t, C_MUTED);
        } else {
            gfx_text_center(c, FONT_BODY, x + w / 2, y + 46, "vuoto", C_DIM);
        }
    }
    /* pannello di destra: oscilloscopio e stato */
    draw_scope(a, c, 400, 56, 224, 96, C_VIOLET);
    gfx_round_rect(c, 400, 160, 224, 180, 12, C_PANEL, 1.0f);
    char t[160];
    int playing = sampler_playing(a->s), pp = sampler_playing_pattern(a->s);
    snprintf(t, sizeof(t), "%d BPM", (int)lrintf(sampler_tempo(a->s)));
    gfx_text(c, FONT_BIG, 414, 192, t, C_TEXT);
    int sw = (int)lrintf(sampler_swing(a->s) * 100);
    if (playing) snprintf(t, sizeof(t), "pattern %c in moto · swing %d %%", 'A' + (pp >= 0 ? pp : sampler_selected_pattern(a->s)), sw);
    else snprintf(t, sizeof(t), "fermo · pattern %c · swing %d %%", 'A' + sampler_selected_pattern(a->s), sw);
    gfx_text(c, FONT_SMALL, 414, 214, t, playing ? C_GREEN : C_MUTED);
    draw_step_dots(a, c, 414, 222, 196);
    snprintf(t, sizeof(t), "Banco %c · pad %c%d scelto · %d voci", 'A' + a->bank, 'A' + a->sel / 8, a->sel % 8 + 1, sampler_active_voices(a->s));
    gfx_text(c, FONT_SMALL, 414, 252, t, C_MUTED);
    if (sampler_record(a->s)) gfx_text(c, FONT_BOLD, 414, 278, "REC dal vivo (R2)", C_RED);
    else gfx_text(c, FONT_SMALL, 414, 278, "R2: registra dal vivo", C_DIM);
    gfx_text(c, FONT_SMALL, 414, 304, a->midi_name[0] ? a->midi_name : "nessun MIDI", a->midi_name[0] ? C_GREEN : C_DIM);
    gfx_text(c, FONT_SMALL, 414, 326, a->mic_name[0] ? "microfono pronto" : "nessun microfono", a->mic_name[0] ? C_GREEN : C_DIM);
    /* banchi */
    for (int b = 0; b < 2; b++) {
        int x = 400 + b * 116;
        gfx_round_rect(c, x, 350, 108, 40, 10, b == a->bank ? C_VIOLET_D : C_PANEL, 1.0f);
        snprintf(t, sizeof(t), "Banco %c", 'A' + b);
        gfx_text_center(c, FONT_BOLD, x + 54, 370, t, b == a->bank ? C_TEXT : C_MUTED);
        gfx_text_center(c, FONT_SMALL, x + 54, 385, b ? "R1" : "L1", C_MUTED);
    }
    draw_hints(c, "croce e Y B A X suonano · L1/R1 banco · L2+pad sceglie · R2 rec dal vivo · START play · SELECT pagina");
}

static void wave_cache(App *a, const Sample *smp)
{
    if (a->wave_smp == smp && smp && a->wave_frames == smp->frames) return;
    a->wave_smp = smp;
    a->wave_frames = smp ? smp->frames : 0;
    if (!smp) return;
    for (int col = 0; col < WAVE_COLS; col++) {
        int i0 = (int)((long long)col * smp->frames / WAVE_COLS), i1 = (int)((long long)(col + 1) * smp->frames / WAVE_COLS);
        if (i1 <= i0) i1 = i0 + 1;
        if (i1 > smp->frames) i1 = smp->frames;
        float mn = 1.0f, mx = -1.0f;
        int stride = (i1 - i0) > 64 ? (i1 - i0) / 64 : 1;
        for (int i = i0; i < i1; i += stride) {
            float v = smp->channels == 2 ? 0.5f * (smp->data[2 * i] + smp->data[2 * i + 1]) : smp->data[i];
            if (v < mn) mn = v;
            if (v > mx) mx = v;
        }
        a->wave_min[col] = mn;
        a->wave_max[col] = mx;
    }
}

static void draw_wave(App *a, Canvas *c, int x, int y, int w, int h)
{
    const Sample *smp = sampler_sample(a->s, a->sel);
    const PadParams *p = sampler_pad(a->s, a->sel);
    gfx_round_rect(c, x, y, w, h, 10, C_SCOPE_BG, 1.0f);
    if (!smp) {
        gfx_text_center(c, FONT_BODY, x + w / 2, y + h / 2 + 6, "Pad vuoto: scegli un campione nella Libreria o registralo", C_MUTED);
        return;
    }
    wave_cache(a, smp);
    int x0 = x + 4, ww = w - 8;
    float sx = x0 + p->start * ww, ex = x0 + p->end * ww;
    gfx_rect_alpha(c, (int)sx, y + 2, (int)(ex - sx), h - 4, C_VIOLET_D, 0.35f);
    float mid = y + h / 2.0f, g = h * 0.46f;
    for (int col = 0; col < WAVE_COLS; col++) {
        int xx = x0 + col * ww / WAVE_COLS;
        int inside = xx >= sx && xx <= ex;
        float top = mid - a->wave_max[col] * g, bot = mid - a->wave_min[col] * g;
        if (bot - top < 1.0f) bot = top + 1.0f;
        gfx_rect(c, xx, (int)top, 1, (int)(bot - top) + 1, inside ? C_VIOLET_L : C_DIM);
    }
    gfx_line(c, sx, y + 2, sx, y + h - 2, 2.0f, C_AMBER, 1.0f);
    gfx_line(c, ex, y + 2, ex, y + h - 2, 2.0f, C_AMBER, 1.0f);
    float pos = sampler_slot_position(a->s, a->sel);
    if (pos >= 0.0f) {
        float px = sx + pos * (ex - sx);
        gfx_line(c, px, y + 2, px, y + h - 2, 2.0f, C_GREEN, 1.0f);
    }
}

static void draw_edit(App *a, Canvas *c)
{
    const Sample *smp = sampler_sample(a->s, a->sel);
    char t[200], lbl[8];
    pad_label(a->sel, lbl, sizeof(lbl));
    snprintf(t, sizeof(t), "Pad %s  ·  %s", lbl, smp ? smp->name : "vuoto");
    gfx_text(c, FONT_TITLE, 16, 76, t, C_TEXT);
    if (smp) {
        snprintf(t, sizeof(t), "%.2f s · %s · %d bit · %d Hz", (double)smp->frames / SP_RATE, smp->src_channels >= 2 ? "stereo" : "mono", smp->src_bits, smp->src_rate);
        gfx_text_right(c, FONT_SMALL, 624, 76, t, C_MUTED);
    }
    draw_wave(a, c, 16, 86, 608, 100);
    int row_h = 26, top = 194;
    for (int i = 0; i < ED_COUNT; i++) {
        int y = top + i * row_h, selected = i == a->edit_sel;
        if (selected) gfx_round_rect(c, 16, y, 608, row_h - 2, 8, C_VIOLET_D, 1.0f);
        gfx_text(c, FONT_BODY, 30, y + 18, ED_NAMES[i], selected ? C_TEXT : RGB(205, 208, 220));
        char v[48];
        ed_text(a, i, v, sizeof(v));
        gfx_text_right(c, FONT_BOLD, 608, y + 18, v, selected ? C_TEXT : C_MUTED);
    }
    draw_hints(c, "su/giù parametro · sinistra/destra cambia · L1/R1 passi grandi · L2/R2 pad · A ascolta · Y svuota · B torna");
}

static void draw_seq(App *a, Canvas *c)
{
    int gx = 118, gy = 92, cw = 31, rh = 20;
    int sel = sampler_selected_pattern(a->s), playing_pat = sampler_playing_pattern(a->s);
    int step = playing_pat == sel ? sampler_current_step(a->s) : -1;
    const SeqPattern *p = sampler_pattern(a->s);
    unsigned chain = sampler_chain(a->s);
    gfx_text_right(c, FONT_SMALL, gx - 10, 74, "Pattern", a->cy < 0 ? C_TEXT : C_MUTED);
    for (int i = 0; i < SP_PATTERNS; i++) {
        float x = gx + i * 56.0f;
        char lbl[2] = { (char)('A' + i), 0 };
        gfx_round_rect(c, x, 54, 50, 26, 8, i == sel ? C_VIOLET_D : C_PANEL_HI, 1.0f);
        gfx_text_center(c, FONT_BOLD, (int)x + 25, 73, lbl, C_TEXT);
        if (i == playing_pat) gfx_circle(c, x + 42, 61, 3.5f, C_GREEN, 1.0f);
        if ((chain >> i) & 1) gfx_round_rect(c, x + 10, 76, 30, 3, 1.5f, C_AMBER, 1.0f);
        if (a->cy < 0 && i == sel) gfx_round_frame(c, x - 2, 52, 54, 30, 9, 2.5f, C_AMBER, 1.0f);
    }
    char t[128];
    snprintf(t, sizeof(t), "%d BPM · swing %d %% · %d passi · %s", (int)lrintf(sampler_tempo(a->s)), (int)lrintf(sampler_swing(a->s) * 100),
             p->length, sampler_playing(a->s) ? (sampler_record(a->s) ? "REC dal vivo" : "in moto") : "fermo");
    gfx_text_right(c, FONT_SMALL, 624, 73, t, sampler_record(a->s) ? C_RED : (sampler_playing(a->s) ? C_GREEN : C_MUTED));
    for (int i = 0; i < SP_STEPS; i++)
        if (i % 4 == 0) gfx_rect_alpha(c, gx + i * cw, gy - 2, cw * 4 - 2, rh * SP_PADS + 4, C_PANEL, i / 4 % 2 ? 0.35f : 0.55f);
    for (int r = 0; r < SP_PADS; r++) {
        int y = gy + r * rh;
        const PadParams *pp = sampler_pad(a->s, r);
        char lbl[8], name[64];
        pad_label(r, lbl, sizeof(lbl));
        snprintf(t, sizeof(t), "%s %s", lbl, pad_name(a, r));
        fit_text(FONT_SMALL, t, gx - 28, name, sizeof(name));
        uint32_t lc = r == a->cy ? C_TEXT : (sampler_sample(a->s, r) ? C_MUTED : C_DIM);
        if (pp->mute) lc = C_RED;
        else if (pp->solo) lc = C_AMBER;
        gfx_text_right(c, FONT_SMALL, gx - 8, y + 14, name, gfx_mix(lc, C_GREEN, a->glow[r]));
        for (int i = 0; i < SP_STEPS; i++) {
            int x = gx + i * cw, vel = p->vel[r][i], in_len = i < p->length;
            uint32_t fill = !in_len ? C_BG_BOT : (vel ? (vel >= 127 ? C_VIOLET_L : (vel >= 100 ? C_VIOLET : C_VIOLET_D)) : C_PANEL_HI);
            if (i == step) fill = gfx_mix(fill, C_TEXT, vel ? 0.45f : 0.12f);
            gfx_round_rect(c, x + 2, y + 2, cw - 4, rh - 4, 5, fill, 1.0f);
            if (r == a->cy && i == a->cx) gfx_round_frame(c, x, y, cw, rh, 6, 2.0f, C_AMBER, 1.0f);
        }
    }
    if (a->cy < 0) draw_hints(c, "sinistra/destra pattern · A in catena · B copia nel successivo · Y svuota · L1/R1 tempo · L2/R2 passi");
    else draw_hints(c, "A passo on/off · B forza · X muto · Y solo · L1/R1 tempo · L2/R2 passi · R2 in Pad: rec dal vivo · START play");
}

static void draw_lib(App *a, Canvas *c)
{
    char t[600], lbl[8];
    pad_label(a->sel, lbl, sizeof(lbl));
    if (a->lib_level == 0) snprintf(t, sizeof(t), "Libreria");
    else fit_text(FONT_SMALL, a->lib_dir, 420, t, sizeof(t));
    gfx_text(c, FONT_SMALL, 16, 70, t, C_MUTED);
    char r[64];
    snprintf(r, sizeof(r), "X assegna al pad %s (%s)", lbl, pad_name(a, a->sel));
    gfx_text_right(c, FONT_SMALL, 624, 70, r, C_AMBER);
    int y0 = 80, y1 = 440, rh = 24, vis = (y1 - y0) / rh, n = a->n_entries, sel = a->lib_sel;
    if (n <= 0) {
        gfx_text_center(c, FONT_BODY, c->w / 2, 240, "Cartella vuota o non raggiungibile", C_MUTED);
    } else {
        if (sel < a->lib_scroll) a->lib_scroll = sel;
        if (sel >= a->lib_scroll + vis) a->lib_scroll = sel - vis + 1;
        if (a->lib_scroll > n - vis) a->lib_scroll = n - vis;
        if (a->lib_scroll < 0) a->lib_scroll = 0;
        for (int i = a->lib_scroll; i < n && i < a->lib_scroll + vis; i++) {
            int y = y0 + (i - a->lib_scroll) * rh, base = y + 17;
            const LibEntry *e = &a->entries[i];
            if (i == sel) gfx_round_rect(c, 16, y + 1, 608, rh - 2, 8, C_PANEL_HI, 1.0f);
            if (e->is_dir) gfx_round_rect(c, 28, y + 6, 14, 11, 2, C_AMBER, 1.0f);
            else gfx_circle(c, 35, y + rh / 2.0f, 4, i == sel ? C_VIOLET_L : C_VIOLET_D, 1.0f);
            char name[200];
            fit_text(FONT_BODY, e->name, 560, name, sizeof(name));
            gfx_text(c, FONT_BODY, 52, base, name, i == sel ? C_TEXT : C_MUTED);
        }
        if (n > vis) {
            float h = (float)(y1 - y0) * vis / n, y = y0 + (float)(y1 - y0 - h) * a->lib_scroll / (n - vis);
            gfx_round_rect(c, 627, y, 4, h, 2, C_LINE, 1.0f);
        }
    }
    draw_hints(c, "su/giù scegli · sinistra/destra salta di 8 · A apre o ascolta · X assegna al pad · B indietro · L1/R1 altro pad");
}

static void draw_rec(App *a, Canvas *c)
{
    char t[200], lbl[8];
    pad_label(a->sel, lbl, sizeof(lbl));
    int st = sampler_rec_state(a->s), mic = a->mic_name[0] != 0;
    gfx_round_rect(c, 16, 58, 608, 120, 14, C_PANEL, 1.0f);
    if (mic) snprintf(t, sizeof(t), "Microfono: %s (%d Hz)", a->mic_name, (int)sampler_capture_rate(a->s));
    else snprintf(t, sizeof(t), "Microfono USB non collegato");
    gfx_text(c, FONT_TITLE, 32, 90, t, mic ? C_TEXT : C_RED);
    if (!mic) gfx_text(c, FONT_SMALL, 32, 114, "Collega un microfono o una scheda audio USB: viene cercato ogni 3 secondi.", C_MUTED);
    /* misuratore di livello */
    float lev = sampler_capture_level(a->s);
    float db = lev > 0.00001f ? 20.0f * log10f(lev) : -100.0f;
    float fr = clampf((db + 60.0f) / 60.0f, 0.0f, 1.0f);
    gfx_text(c, FONT_SMALL, 32, 144, "Livello", C_MUTED);
    gfx_round_rect(c, 96, 134, 500, 14, 7, C_PANEL_HI, 1.0f);
    gfx_round_rect(c, 96, 134, fmaxf(14.0f, fr * 500), 14, 7, db > -6.0f ? C_RED : (db > -20.0f ? C_AMBER : C_GREEN), 1.0f);
    if (a->rec_thr) {
        float tx = 96 + clampf((20.0f * log10f(THRESHOLDS[a->rec_thr]) + 60.0f) / 60.0f, 0.0f, 1.0f) * 500;
        gfx_line(c, tx, 130, tx, 152, 2.0f, C_TEXT, 1.0f);
    }
    snprintf(t, sizeof(t), "%.0f dB", (double)(db < -60 ? -60 : db));
    gfx_text_right(c, FONT_SMALL, 608, 144, t, C_MUTED);
    gfx_text(c, FONT_SMALL, 32, 168, "Soglia di avvio (sinistra/destra):", C_MUTED);
    gfx_text(c, FONT_BOLD, 250, 168, THRESHOLD_NAMES[a->rec_thr], C_TEXT);
    /* stato */
    gfx_round_rect(c, 16, 190, 608, 110, 14, C_PANEL, 1.0f);
    const char *state = st == REC_ON ? "Registrazione in corso" : (st == REC_WAIT ? "In attesa del suono..." : "Pronto: A avvia la registrazione");
    gfx_text(c, FONT_TITLE, 32, 232, state, st == REC_ON ? C_RED : (st == REC_WAIT ? C_AMBER : C_TEXT));
    float secs = (float)sampler_rec_frames(a->s) / SP_RATE;
    snprintf(t, sizeof(t), "%.1f / %d s", (double)secs, SP_REC_SECONDS);
    gfx_text_right(c, FONT_BIG, 608, 232, t, st == REC_ON ? C_TEXT : C_DIM);
    gfx_round_rect(c, 32, 250, 576, 10, 5, C_PANEL_HI, 1.0f);
    if (secs > 0.0f) gfx_round_rect(c, 32, 250, fmaxf(10.0f, secs / SP_REC_SECONDS * 576), 10, 5, C_RED, 1.0f);
    snprintf(t, sizeof(t), "Va nel pad %s (%s) · mono 16 bit 48 kHz · massimo %d secondi", lbl, pad_name(a, a->sel), SP_REC_SECONDS);
    gfx_text(c, FONT_SMALL, 32, 286, t, C_MUTED);
    /* ultima registrazione */
    gfx_round_rect(c, 16, 312, 608, 60, 14, C_PANEL, 1.0f);
    gfx_text(c, FONT_SMALL, 32, 334, "Ultima registrazione", C_MUTED);
    if (a->last_rec[0]) {
        const char *base = strrchr(a->last_rec, '/');
        gfx_text(c, FONT_BODY, 32, 358, base ? base + 1 : a->last_rec, C_TEXT);
    } else gfx_text(c, FONT_BODY, 32, 358, "nessuna in questa sessione", C_DIM);
    snprintf(t, sizeof(t), "I file vanno in %s", a->rec_dir);
    fit_text(FONT_SMALL, t, 600, t, sizeof(t));
    gfx_text(c, FONT_SMALL, 16, 390, t, C_DIM);
    draw_hints(c, "A avvia/ferma · sinistra/destra soglia · L1/R1 pad di destinazione · X ascolta il pad · B torna");
}

static void draw_options(App *a, Canvas *c)
{
    gfx_round_rect(c, 16, 58, 330, 220, 14, C_PANEL, 1.0f);
    int row_h = 34, top = 66;
    for (int i = 0; i < OPT_COUNT; i++) {
        int y = top + i * row_h, selected = i == a->opt_sel;
        if (selected) gfx_round_rect(c, 22, y + 1, 318, row_h - 2, 9, C_VIOLET_D, 1.0f);
        gfx_text(c, FONT_BODY, 34, y + 23, OPT_NAMES[i], selected ? C_TEXT : RGB(205, 208, 220));
        char v[48];
        opt_text(a, i, v, sizeof(v));
        gfx_text_right(c, FONT_BOLD, 330, y + 23, v, selected ? C_TEXT : C_MUTED);
    }
    if (a->clock_ticks && a->clock_seen < 0.5f) {
        char t[64];
        snprintf(t, sizeof(t), "clock MIDI in arrivo: %.0f BPM", (double)a->clock_bpm);
        gfx_text(c, FONT_SMALL, 32, 300, t, C_GREEN);
    }
    char t[200];
    snprintf(t, sizeof(t), "Esportazioni in %s", a->exp_dir);
    fit_text(FONT_SMALL, t, 330, t, sizeof(t));
    gfx_text(c, FONT_SMALL, 16, 322, t, C_DIM);
    if (a->last_export[0]) {
        const char *base = strrchr(a->last_export, '/');
        snprintf(t, sizeof(t), "Ultima: %s", base ? base + 1 : a->last_export);
        fit_text(FONT_SMALL, t, 330, t, sizeof(t));
        gfx_text(c, FONT_SMALL, 16, 342, t, C_MUTED);
    }
    /* mappa MIDI */
    gfx_round_rect(c, 360, 58, 264, 380, 14, C_PANEL, 1.0f);
    gfx_text(c, FONT_BOLD, 376, 82, "MIDI USB (MPK mini e simili)", C_TEXT);
    static const char *rows[] = {
        "Pad canale 10, note 36-51: pad 1-16",
        "Tasti (altri canali): il pad scelto,",
        "   cromatico attorno al Do4",
        "CC70 volume del pad scelto",
        "CC71 intonazione del pad scelto",
        "CC72 inizio · CC73 fine del pad",
        "CC74 tempo (40-240 BPM)",
        "CC75 swing · CC76 volume generale",
        "CC77 filtro passa-basso",
        "CC64 sustain: tiene i pad tenuti",
        "CC7 volume · CC120/123 tutto spento",
        "Pitch bend: ±2 semitoni",
        "Program change: canale 10 banco A/B,",
        "   altri canali pattern A-D",
        "Clock, start, stop: sincronizzano il",
        "   sequencer (se «Segui il clock»)",
    };
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) gfx_text(c, FONT_SMALL, 376, 106 + (int)i * 20, rows[i], C_MUTED);
    draw_hints(c, "su/giù scegli · sinistra/destra cambia · L1/R1 tempo a passi di 10 · A esporta o attiva · B torna");
}

static void draw_quit(Canvas *c)
{
    gfx_rect_alpha(c, 0, 0, c->w, c->h, RGB(0, 0, 0), 0.6f);
    gfx_round_rect(c, 150, 170, 340, 140, 18, C_PANEL_HI, 1.0f);
    gfx_round_frame(c, 150, 170, 340, 140, 18, 2, C_VIOLET, 1.0f);
    gfx_text_center(c, FONT_TITLE, 320, 222, "Uscire dal sampler?", C_TEXT);
    gfx_text_center(c, FONT_BODY, 320, 256, "Pad, pattern e opzioni restano salvati.", C_MUTED);
    gfx_text_center(c, FONT_BOLD, 320, 290, "A o giù: esci   ·   B o su: resta", C_AMBER);
}

void app_draw(App *a, Canvas *c)
{
    gfx_vgradient(c, 0, 0, c->w, c->h, C_BG_TOP, C_BG_BOT);
    draw_header(a, c);
    switch (a->page) {
    case PAGE_PADS: draw_pads(a, c); break;
    case PAGE_EDIT: draw_edit(a, c); break;
    case PAGE_SEQ: draw_seq(a, c); break;
    case PAGE_LIB: draw_lib(a, c); break;
    case PAGE_REC: draw_rec(a, c); break;
    default: draw_options(a, c); break;
    }
    if (a->toast_t > 0.0f && a->toast[0]) {
        float al = fminf(1.0f, a->toast_t * 3.0f);
        int w = gfx_text_width(FONT_BOLD, a->toast) + 36;
        if (w > 620) w = 620;
        gfx_round_rect(c, (c->w - w) / 2.0f, 400, w, 34, 17, C_PANEL_HI, 0.95f * al);
        gfx_round_frame(c, (c->w - w) / 2.0f, 400, w, 34, 17, 1.5f, C_VIOLET, al);
        gfx_text_center(c, FONT_BOLD, c->w / 2, 423, a->toast, gfx_mix(C_BG_BOT, C_TEXT, al));
    }
    if (a->quit_dialog) draw_quit(c);
}
