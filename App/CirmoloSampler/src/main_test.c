/* Cirmolo Sampler - prove senza console: WAV sintetici, caricamento, pad, pitch, ritaglio, gruppi,
 * sequencer, registrazione dal vivo, MIDI, stato, esportazione, registrazione dal microfono, schermate
 * (BMP), prestazioni e ridisegno.
 *
 * Uso: sampler-test <cartella dei font> <cartella di uscita>
 * I font sono quelli di PyUI: BeVietnamPro-Regular.ttf e BeVietnamPro-SemiBold.ttf.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "app.h"
#include "gfx.h"
#include "sampler.h"

#ifdef _WIN32
#include <direct.h>
#define make_dir(p) _mkdir(p)
#else
#include <sys/stat.h>
#define make_dir(p) mkdir(p, 0755)
#endif

#define W 640
#define H 480
#define SR 48000
#define PI 3.14159265358979

static int fail;

static void check(int ok, const char *what)
{
    if (!ok) { fprintf(stderr, "ERRORE: %s\n", what); fail = 1; }
}

static void write_bmp(const char *path, const Canvas *c)
{
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

/* WAV di prova: PCM a 8/16/24 bit o float (fmt 3), con i campioni float interlacciati. */
static void write_test_wav(const char *path, int rate, int channels, int bits, int fmt, int frames, const float *data)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    int bps = bits / 8;
    uint32_t dlen = (uint32_t)frames * (uint32_t)channels * (uint32_t)bps, riff = 36 + dlen, br = (uint32_t)rate * (uint32_t)channels * (uint32_t)bps;
    unsigned char h[44] = { 'R', 'I', 'F', 'F' };
    h[4] = riff; h[5] = riff >> 8; h[6] = riff >> 16; h[7] = riff >> 24;
    memcpy(h + 8, "WAVEfmt ", 8);
    h[16] = 16; h[20] = (unsigned char)fmt; h[22] = (unsigned char)channels;
    h[24] = rate; h[25] = rate >> 8; h[26] = rate >> 16; h[27] = rate >> 24;
    h[28] = br; h[29] = br >> 8; h[30] = br >> 16; h[31] = br >> 24;
    h[32] = (unsigned char)(channels * bps); h[34] = (unsigned char)bits;
    memcpy(h + 36, "data", 4);
    h[40] = dlen; h[41] = dlen >> 8; h[42] = dlen >> 16; h[43] = dlen >> 24;
    fwrite(h, 1, 44, f);
    for (int i = 0; i < frames * channels; i++) {
        float v = data[i] < -1 ? -1 : (data[i] > 1 ? 1 : data[i]);
        if (fmt == 3) fwrite(&v, 4, 1, f);
        else if (bits == 8) { unsigned char b = (unsigned char)(128 + lrintf(v * 127)); fwrite(&b, 1, 1, f); }
        else if (bits == 16) { int16_t s = (int16_t)lrintf(v * 32767); fwrite(&s, 2, 1, f); }
        else { int32_t s = (int32_t)lrint(v * 8388607.0); unsigned char b[3] = { s & 255, (s >> 8) & 255, (s >> 16) & 255 }; fwrite(b, 1, 3, f); }
    }
    fclose(f);
}

/* Frame del blocco dati di un WAV scritto dall'app (16 bit). */
static int wav_frames(const char *path, int *channels)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    unsigned char h[44];
    if (fread(h, 1, 44, f) != 44) { fclose(f); return -1; }
    fclose(f);
    *channels = h[22];
    uint32_t dlen = h[40] | h[41] << 8 | h[42] << 16 | (uint32_t)h[43] << 24;
    return (int)(dlen / (uint32_t)(*channels * 2));
}

static Sampler *g_s;
static App *g_a;
static float *g_wav;
static int g_pos, g_cap;
static float g_last_rms;

/* Fa avanzare insieme interfaccia e audio, come farebbe il ciclo della console a 60 fps; se dst non e'
   NULL ci copia anche l'audio reso. */
static void run_into(float seconds, float *dst, int dst_cap)
{
    int frames = (int)(seconds * SR + 0.5f), block = SR / 60;
    float buf[2 * 1024];
    double sum2 = 0;
    int n_all = 0, dpos = 0;
    for (int done = 0; done < frames; done += block) {
        int n = block < frames - done ? block : frames - done;
        sampler_render(g_s, buf, n);
        for (int i = 0; i < 2 * n; i++) sum2 += (double)buf[i] * buf[i];
        n_all += 2 * n;
        if (g_wav && g_pos + n <= g_cap) { memcpy(g_wav + 2 * g_pos, buf, sizeof(float) * 2 * n); g_pos += n; }
        if (dst && dpos + n <= dst_cap) { memcpy(dst + 2 * dpos, buf, sizeof(float) * 2 * n); dpos += n; }
        app_update(g_a, (float)n / SR);
    }
    g_last_rms = n_all ? (float)sqrt(sum2 / n_all) : 0.0f;
}

static void run(float seconds) { run_into(seconds, NULL, 0); }

static void tap(int b, float hold)
{
    app_button(g_a, b, 1);
    run(hold);
    app_button(g_a, b, 0);
}

static void shot(const char *dir, const char *name, Canvas *c)
{
    char out[700];
    app_draw(g_a, c);
    snprintf(out, sizeof(out), "%s/%s", dir, name);
    write_bmp(out, c);
}

/* Ultimo frame non silenzioso di un buffer stereo (-1 se tutto silenzio). */
static int last_sound(const float *lr, int frames)
{
    for (int i = frames - 1; i >= 0; i--) if (fabsf(lr[2 * i]) > 1e-3f || fabsf(lr[2 * i + 1]) > 1e-3f) return i;
    return -1;
}

static float rms_range(const float *lr, int from, int to)
{
    double s = 0;
    for (int i = from; i < to; i++) s += (double)lr[2 * i] * lr[2 * i];
    return to > from ? (float)sqrt(s / (to - from)) : 0.0f;
}

static int has_nan(const float *lr, int frames)
{
    for (int i = 0; i < 2 * frames; i++) if (lr[i] != lr[i]) return 1;
    return 0;
}

/* Suona un pad da solo e restituisce la durata del suono reso in frame. */
static int sound_length(int pad, float pitch, float semis, int key, float *tmp, int cap)
{
    sampler_pad(g_s, pad)->pitch = pitch;
    sampler_trigger(g_s, pad, 1.0f, semis, key);
    run_into((float)cap / SR, tmp, cap);
    if (key >= 0) sampler_release(g_s, pad, key);
    run(0.05f);
    return last_sound(tmp, cap) + 1;
}

static void midi3(int a, int b, int c) { unsigned char m[3] = { (unsigned char)a, (unsigned char)b, (unsigned char)c }; app_midi(g_a, m, 3); }
static void midi2(int a, int b) { unsigned char m[2] = { (unsigned char)a, (unsigned char)b }; app_midi(g_a, m, 2); }
static void midi1(int a) { unsigned char m[1] = { (unsigned char)a }; app_midi(g_a, m, 1); }

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "uso: %s <cartella font> <cartella uscita>\n", argv[0]); return 2; }
    const char *out = argv[2];
    char p1[512], p2[512], state[600], wavdir[600], recdir[600];
    snprintf(p1, sizeof(p1), "%s/BeVietnamPro-Regular.ttf", argv[1]);
    snprintf(p2, sizeof(p2), "%s/BeVietnamPro-SemiBold.ttf", argv[1]);
    if (gfx_load_fonts(p1, p2)) return 1;
    make_dir(out);
    snprintf(wavdir, sizeof(wavdir), "%s/wav-prova", out);
    snprintf(recdir, sizeof(recdir), "%s/campioni", out);
    make_dir(wavdir);
    snprintf(state, sizeof(state), "%s/stato-prova.txt", out);
    remove(state);

    /* 1. WAV sintetici: mono 16 bit 44,1 kHz, stereo 24 bit 48 kHz, 8 bit 22 kHz, float, rampa, lungo */
    char f_mono[700], f_stereo[700], f_8bit[700], f_float[700], f_rampa[700], f_lungo[700];
    snprintf(f_mono, sizeof(f_mono), "%s/sinus-mono-16bit-44k.wav", wavdir);
    snprintf(f_stereo, sizeof(f_stereo), "%s/stereo-24bit-48k.wav", wavdir);
    snprintf(f_8bit, sizeof(f_8bit), "%s/otto-bit-22k.wav", wavdir);
    snprintf(f_float, sizeof(f_float), "%s/float-32-48k.wav", wavdir);
    snprintf(f_rampa, sizeof(f_rampa), "%s/rampa-discendente.wav", wavdir);
    snprintf(f_lungo, sizeof(f_lungo), "%s/lungo-5s.wav", wavdir);
    {
        int n = 22050;                                   /* 0,5 s a 44,1 kHz */
        float *d = malloc(sizeof(float) * (size_t)n * 2);
        for (int i = 0; i < n; i++) d[i] = 0.8f * (float)sin(2 * PI * 440 * i / 44100.0);
        write_test_wav(f_mono, 44100, 1, 16, 1, n, d);
        n = 14400;                                       /* 0,3 s stereo a 48 kHz */
        for (int i = 0; i < n; i++) { d[2 * i] = 0.7f * (float)sin(2 * PI * 220 * i / 48000.0); d[2 * i + 1] = 0.7f * (float)sin(2 * PI * 330 * i / 48000.0); }
        write_test_wav(f_stereo, 48000, 2, 24, 1, n, d);
        n = 4410;                                        /* 0,2 s a 22,05 kHz, 8 bit */
        for (int i = 0; i < n; i++) d[i] = 0.9f * (float)sin(2 * PI * 660 * i / 22050.0);
        write_test_wav(f_8bit, 22050, 1, 8, 1, n, d);
        n = 9600;                                        /* 0,2 s float */
        for (int i = 0; i < n; i++) d[i] = 0.5f * (float)sin(2 * PI * 880 * i / 48000.0);
        write_test_wav(f_float, 48000, 1, 32, 3, n, d);
        n = 24000;                                       /* 0,5 s: sinusoide che si spegne linearmente */
        for (int i = 0; i < n; i++) d[i] = (1.0f - (float)i / n) * (float)sin(2 * PI * 500 * i / 48000.0);
        write_test_wav(f_rampa, 48000, 1, 16, 1, n, d);
        free(d);
        n = 48000 * 5;                                   /* 5 s di sinusoide a 110 Hz */
        d = malloc(sizeof(float) * (size_t)n);
        for (int i = 0; i < n; i++) d[i] = 0.6f * (float)sin(2 * PI * 110 * i / 48000.0);
        write_test_wav(f_lungo, 48000, 1, 16, 1, n, d);
        free(d);
    }
    {
        Sample *m = sample_load_wav(f_mono), *st = sample_load_wav(f_stereo), *b8 = sample_load_wav(f_8bit), *fl = sample_load_wav(f_float);
        check(m && st && b8 && fl, "WAV di prova non caricati");
        if (m && st && b8 && fl) {
            float pk = 0;
            for (int i = 0; i < m->frames; i++) pk = fmaxf(pk, fabsf(m->data[i]));
            int nan = 0;
            for (int i = 0; i < st->frames * 2; i++) nan += st->data[i] != st->data[i];
            printf("WAV: mono %d frame (44,1k -> 48k, %d bit, picco %.2f), stereo %d frame x %d canali (%d bit), 8 bit %d frame, float %d frame\n",
                   m->frames, m->src_bits, pk, st->frames, st->channels, st->src_bits, b8->frames, fl->frames);
            check(abs(m->frames - 24000) < 240 && m->channels == 1 && m->src_rate == 44100 && m->src_bits == 16 && fabsf(pk - 0.8f) < 0.03f, "WAV mono 16 bit 44,1 kHz non convertito bene");
            check(st->frames == 14400 && st->channels == 2 && st->src_bits == 24 && !nan, "WAV stereo 24 bit 48 kHz non convertito bene");
            check(abs(b8->frames - 9600) < 100 && b8->src_bits == 8 && b8->src_rate == 22050, "WAV 8 bit 22 kHz non convertito bene");
            check(fl->frames == 9600 && fabsf(fl->data[12] - 0.5f * (float)sin(2 * PI * 880 * 12 / 48000.0)) < 1e-4f, "WAV float non convertito bene");
        }
        sample_free(m); sample_free(st); sample_free(b8); sample_free(fl);
        char bad[700];
        snprintf(bad, sizeof(bad), "%s/non-esiste.wav", wavdir);
        check(sample_load_wav(bad) == NULL, "un file inesistente non da' NULL");
        check(sample_load_wav(argv[0]) == NULL, "un file che non e' WAV non da' NULL");
    }

    /* 2. Motore e app: pad, velocity, assenza di NaN */
    Sampler *s = g_s = sampler_create(SR);
    App *a = g_a = app_create(s, state);
    app_set_library(a, recdir, wavdir);
    uint32_t *px = malloc(sizeof(uint32_t) * W * H);
    Canvas c = { px, W, H };
    g_cap = SR * 60;
    g_wav = calloc((size_t)g_cap * 2, sizeof(float));
    int tmp_cap = SR * 2;
    float *tmp = calloc((size_t)tmp_cap * 2, sizeof(float));
    check(app_assign_wav(a, 0, f_mono) == 0 && app_assign_wav(a, 1, f_stereo) == 0 && app_assign_wav(a, 2, f_rampa) == 0 && app_assign_wav(a, 3, f_8bit) == 0,
          "assegnazione dei WAV ai pad");
    for (int p = 4; p < 8; p++) app_assign_wav(a, p, f_lungo);
    run(0.05f);
    sampler_trigger(s, 0, 1.0f, 0.0f, -1);
    run_into(1.0f, tmp, tmp_cap);
    float rms_full = rms_range(tmp, 0, SR / 2);
    check(!has_nan(tmp, SR) && rms_full > 0.1f, "il pad 1 non suona o produce NaN");
    sampler_trigger(s, 0, 0.5f, 0.0f, -1);
    run_into(1.0f, tmp, tmp_cap);
    float rms_half = rms_range(tmp, 0, SR / 2);
    sampler_trigger(s, 1, 1.0f, 0.0f, -1);          /* stereo: canali diversi */
    run_into(0.5f, tmp, tmp_cap);
    float diff = 0;
    for (int i = 0; i < 4000; i++) diff += fabsf(tmp[2 * i] - tmp[2 * i + 1]);
    printf("pad: RMS con velocity 1 %.3f, con 0,5 %.3f (rapporto %.2f); stereo L-R differenza media %.3f\n", rms_full, rms_half, rms_half / rms_full, diff / 4000);
    check(rms_half / rms_full > 0.4f && rms_half / rms_full < 0.6f, "la velocity non scala il volume");
    check(diff / 4000 > 0.05f, "il campione stereo esce mono");

    /* 3. Pitch: +12 semitoni = durata dimezzata, -12 = raddoppiata; bend */
    int len0 = sound_length(0, 0.0f, 0.0f, -1, tmp, tmp_cap);
    int len_up = sound_length(0, 12.0f, 0.0f, -1, tmp, tmp_cap);
    int len_down = sound_length(0, -12.0f, 0.0f, -1, tmp, tmp_cap);
    sampler_pad(g_s, 0)->pitch = 0.0f;
    sampler_set_bend(s, 12.0f);
    int len_bend = sound_length(0, 0.0f, 0.0f, -1, tmp, tmp_cap);
    sampler_set_bend(s, 0.0f);
    printf("pitch: durata %d frame a 0 st, %d a +12, %d a -12, %d con bend +12\n", len0, len_up, len_down, len_bend);
    check(abs(len0 - 24000) < 500, "la durata base non e' 0,5 s");
    check(fabsf((float)len_up / len0 - 0.5f) < 0.03f, "+12 semitoni non dimezza la durata");
    check(fabsf((float)len_down / len0 - 2.0f) < 0.06f, "-12 semitoni non raddoppia la durata");
    check(fabsf((float)len_bend / len0 - 0.5f) < 0.03f, "il pitch bend non agisce");

    /* 4. Ritaglio e inverso */
    {
        PadParams *p = sampler_pad(s, 2);
        p->start = 0.25f; p->end = 0.5f;
        int len_cut = sound_length(2, 0.0f, 0.0f, -1, tmp, tmp_cap);
        p->start = 0.0f; p->end = 1.0f;
        sampler_trigger(s, 2, 1.0f, 0.0f, -1);
        run_into(0.6f, tmp, tmp_cap);
        float fwd_head = rms_range(tmp, 0, SR / 10), fwd_tail = rms_range(tmp, (int)(SR * 0.35f), (int)(SR * 0.45f));
        p->reverse = 1;
        sampler_trigger(s, 2, 1.0f, 0.0f, -1);
        run_into(0.6f, tmp, tmp_cap);
        float rev_head = rms_range(tmp, 0, SR / 10), rev_tail = rms_range(tmp, (int)(SR * 0.35f), (int)(SR * 0.45f));
        p->reverse = 0;
        printf("ritaglio 25-50 %%: %d frame; rampa in avanti inizio %.3f fine %.3f, al contrario inizio %.3f fine %.3f\n", len_cut, fwd_head, fwd_tail, rev_head, rev_tail);
        check(abs(len_cut - 6000) < 300, "il ritaglio non accorcia come deve");
        check(fwd_head > 3 * fwd_tail && rev_tail > 3 * rev_head, "l'inversione non rovescia il campione");
    }

    /* 5. Gruppo di esclusione e modi tenuto / loop, sustain */
    {
        sampler_pad(s, 4)->group = 1;
        sampler_pad(s, 5)->group = 1;
        sampler_trigger(s, 4, 1.0f, 0.0f, -1);
        run(0.2f);
        int v4 = sampler_slot_voices(s, 4);
        sampler_trigger(s, 5, 1.0f, 0.0f, -1);
        run(0.05f);
        int v4b = sampler_slot_voices(s, 4), v5 = sampler_slot_voices(s, 5);
        sampler_release_all(s);
        run(0.1f);
        sampler_pad(s, 4)->group = 0;
        sampler_pad(s, 5)->group = 0;
        sampler_trigger(s, 4, 1.0f, 0.0f, -1);
        sampler_trigger(s, 5, 1.0f, 0.0f, -1);
        run(0.05f);
        int both = sampler_active_voices(s);
        sampler_release_all(s);
        run(0.1f);
        printf("gruppo: pad 5 suona (%d voce), pad 6 dello stesso gruppo lo spegne (%d, %d); senza gruppo %d voci\n", v4, v4b, v5, both);
        check(v4 == 1 && v4b == 0 && v5 == 1 && both == 2, "il gruppo di esclusione non funziona");
        /* tenuto */
        sampler_pad(s, 6)->mode = MODE_HOLD;
        sampler_trigger(s, 6, 1.0f, 0.0f, -1);
        run(0.2f);
        int h1 = sampler_slot_voices(s, 6);
        sampler_release(s, 6, -1);
        run(0.1f);
        int h2 = sampler_slot_voices(s, 6);
        /* sustain tiene i pad tenuti */
        sampler_set_sustain(s, 1);
        sampler_trigger(s, 6, 1.0f, 0.0f, -1);
        sampler_release(s, 6, -1);
        run(0.2f);
        int h3 = sampler_slot_voices(s, 6);
        sampler_set_sustain(s, 0);
        run(0.1f);
        int h4 = sampler_slot_voices(s, 6);
        /* loop: 0,1 s ripetuti, a colpi alterni */
        PadParams *p7 = sampler_pad(s, 7);
        p7->mode = MODE_LOOP; p7->end = 0.02f;
        sampler_trigger(s, 7, 1.0f, 0.0f, -1);
        run(1.0f);
        int l1 = sampler_slot_voices(s, 7);
        float l_rms = g_last_rms;
        sampler_trigger(s, 7, 1.0f, 0.0f, -1);
        run(0.1f);
        int l2 = sampler_slot_voices(s, 7);
        p7->mode = MODE_ONESHOT; p7->end = 1.0f;
        printf("tenuto: %d voce, dopo il rilascio %d; con sustain %d, tolto il pedale %d; loop dopo 1 s %d voce (RMS %.3f), secondo colpo %d\n", h1, h2, h3, h4, l1, l_rms, l2);
        check(h1 == 1 && h2 == 0 && h3 == 1 && h4 == 0, "il modo tenuto o il sustain non funzionano");
        check(l1 == 1 && l_rms > 0.05f && l2 == 0, "il loop non gira o non si ferma al secondo colpo");
    }

    /* 6. Sequencer: passi, mute, solo, swing, lunghezza, catena */
    {
        for (int i = 0; i < SP_PATTERNS; i++) sampler_pattern_clear(sampler_pattern_at(s, i));
        SeqPattern *pa = sampler_pattern_at(s, 0), *pb = sampler_pattern_at(s, 1);
        for (int i = 0; i < 16; i += 4) pa->vel[0][i] = 100;
        pa->vel[1][2] = 127;
        pb->vel[3][0] = 100;
        sampler_select_pattern(s, 0);
        sampler_set_chain(s, 0);
        sampler_set_tempo(s, 120.0f);
        sampler_set_swing(s, 0.0f);
        uint32_t f0 = sampler_slot_flash(s, 0), f1 = sampler_slot_flash(s, 1);
        sampler_play(s, 1);
        run(1.9f);
        int hits0 = (int)(sampler_slot_flash(s, 0) - f0), hits1 = (int)(sampler_slot_flash(s, 1) - f1), step = sampler_current_step(s);
        /* mute */
        f0 = sampler_slot_flash(s, 0);
        sampler_pad(s, 0)->mute = 1;
        run(2.0f);
        int hits_mute = (int)(sampler_slot_flash(s, 0) - f0);
        sampler_pad(s, 0)->mute = 0;
        /* solo sul pad 2: il pad 1 tace */
        f0 = sampler_slot_flash(s, 0); f1 = sampler_slot_flash(s, 1);
        sampler_pad(s, 1)->solo = 1;
        run(2.0f);
        int hits_solo0 = (int)(sampler_slot_flash(s, 0) - f0), hits_solo1 = (int)(sampler_slot_flash(s, 1) - f1);
        sampler_pad(s, 1)->solo = 0;
        /* swing: non fa crash e i colpi restano */
        sampler_set_swing(s, 0.5f);
        f0 = sampler_slot_flash(s, 0);
        run_into(2.0f, tmp, tmp_cap);
        int hits_swing = (int)(sampler_slot_flash(s, 0) - f0), nan_swing = has_nan(tmp, tmp_cap);
        sampler_set_swing(s, 0.0f);
        /* lunghezza 4: il pad 1 batte ogni mezzo secondo */
        pa->length = 4;
        sampler_play(s, 1);
        f0 = sampler_slot_flash(s, 0);
        run(0.95f);
        int hits_len4 = (int)(sampler_slot_flash(s, 0) - f0);
        pa->length = 16;
        /* catena A+B: la seconda battuta e' B */
        sampler_set_chain(s, 0x3);
        sampler_play(s, 1);
        run(0.1f);
        int pat_first = sampler_playing_pattern(s);
        run(2.0f);
        int pat_second = sampler_playing_pattern(s);
        run(2.0f);
        int pat_third = sampler_playing_pattern(s);
        sampler_set_chain(s, 0);
        sampler_play(s, 0);
        run(0.3f);
        printf("sequencer: in 1,9 s pad 1 %d colpi, pad 2 %d, passo %d; muto %d; solo: pad 1 %d pad 2 %d; swing %d colpi (NaN %d); lunghezza 4: %d colpi in 0,95 s; catena %c %c %c; fermo: passo %d\n",
               hits0, hits1, step, hits_mute, hits_solo0, hits_solo1, hits_swing, nan_swing, hits_len4, 'A' + pat_first, 'A' + pat_second, 'A' + pat_third, sampler_current_step(s));
        check(hits0 == 4 && hits1 == 1 && step >= 0 && step < 16, "i passi del sequencer non suonano come previsto");
        check(hits_mute == 0, "il mute non tace il pad");
        check(hits_solo0 == 0 && hits_solo1 == 1, "il solo non isola il pad");
        check(hits_swing >= 3 && hits_swing <= 5 && !nan_swing, "lo swing rompe il sequencer");
        check(hits_len4 == 2, "la lunghezza del pattern non vale");
        check(pat_first == 0 && pat_second == 1 && pat_third == 0, "la catena non alterna i pattern");
        check(sampler_current_step(s) == -1, "da fermo il passo non e' -1");
    }

    /* 7. Registrazione dal vivo quantizzata */
    {
        SeqPattern *pa = sampler_pattern_at(s, 0);
        memset(pa->vel[3], 0, sizeof(pa->vel[3]));
        sampler_set_record(s, 1);
        sampler_play(s, 1);
        run(1.0f / 60);                                /* passo 0 all'8 %: cade sul passo 0 */
        sampler_trigger(s, 3, 0.8f, 0.0f, -1);
        run(5.0f / 60);                                /* passo 0 all'80 %: cade sul passo 1 */
        uint32_t f3 = sampler_slot_flash(s, 3);
        sampler_trigger(s, 3, 1.0f, 0.0f, -1);
        run(0.3f);
        int hits = (int)(sampler_slot_flash(s, 3) - f3);   /* solo il colpo dal vivo: il passo 1 non lo ripete */
        sampler_set_record(s, 0);
        sampler_play(s, 0);
        run(0.2f);
        printf("registrazione dal vivo: passo 0 = %d, passo 1 = %d, colpi sentiti %d\n", pa->vel[3][0], pa->vel[3][1], hits);
        check(pa->vel[3][0] == 102 && pa->vel[3][1] == 127, "i colpi non finiscono nei passi giusti");
        check(hits == 1, "il colpo quantizzato in avanti viene suonato due volte");
        /* il pattern registrato suona davvero al giro dopo */
        uint32_t fb = sampler_slot_flash(s, 3);
        sampler_play(s, 1);
        run(0.3f);
        sampler_play(s, 0);
        run(0.2f);
        check(sampler_slot_flash(s, 3) - fb == 2, "il pattern registrato non suona i colpi");
        memset(pa->vel[3], 0, sizeof(pa->vel[3]));
    }

    /* 8. Tasti della Flip: pagina Pad, banchi, L2 sceglie senza suonare, R2 rec dal vivo, START */
    {
        app_set_page(a, PAGE_PADS);
        uint32_t f0 = sampler_slot_flash(s, 0);
        tap(PAD_LEFT, 0.05f);                           /* pad A1 */
        int sel_a = app_selected_pad(a);
        uint32_t hits_a1 = sampler_slot_flash(s, 0) - f0;
        tap(PAD_R1, 0.02f);
        app_button(a, PAD_L2, 1);
        tap(PAD_A, 0.02f);                              /* B7 scelto senza suonare */
        app_button(a, PAD_L2, 0);
        int sel_b = app_selected_pad(a), bank_b = app_bank(a);
        uint32_t f14 = sampler_slot_flash(s, 14);
        tap(PAD_L1, 0.02f);
        tap(PAD_START, 0.02f);
        int playing = sampler_playing(s);
        tap(PAD_R2, 0.02f);
        int rec = sampler_record(s);
        tap(PAD_R2, 0.02f);
        tap(PAD_START, 0.02f);
        run(0.2f);
        printf("tasti: sinistra suona il pad A1 (scelto %d, colpi %u), R1 + L2 + A sceglie il pad %d del banco %c senza suonarlo (colpi %u), START %d, R2 %d\n",
               sel_a, hits_a1, sel_b + 1, 'A' + bank_b, sampler_slot_flash(s, 14) - f14, playing, rec);
        check(sel_a == 0 && hits_a1 == 1, "la croce non suona il pad A1");
        check(sel_b == 14 && bank_b == 1 && sampler_slot_flash(s, 14) == f14, "L2 + pad non sceglie senza suonare");
        check(playing && rec && !sampler_playing(s) && !sampler_record(s), "START o R2 non fanno quel che devono");
        shot(out, "1-pad.bmp", &c);
    }

    /* 9. MIDI: pad del canale 10, tasti cromatici, CC, program change, clock */
    {
        app_midi_status(a, "MPKminiIV");
        app_select_pad(a, 0);
        sampler_pad(s, 0)->pitch = 0.0f;
        uint32_t f0 = sampler_slot_flash(s, 0), f1 = sampler_slot_flash(s, 1);
        midi3(0x99, 36, 100);                           /* pad 1 */
        midi3(0x99, 37, 100);                           /* pad 2 */
        midi3(0x89, 36, 0);
        run(0.1f);
        int pad_hits = (int)(sampler_slot_flash(s, 0) - f0 + sampler_slot_flash(s, 1) - f1);
        run(0.6f);
        /* tasti: il pad scelto, cromatico (Do5 = +12 -> durata dimezzata) */
        midi3(0x90, 72, 100);
        run_into(1.0f, tmp, tmp_cap);
        int len_c5 = last_sound(tmp, SR) + 1;
        midi3(0x80, 72, 0);
        midi3(0x90, 60, 100);
        run_into(1.0f, tmp, tmp_cap);
        int len_c4 = last_sound(tmp, SR) + 1;
        midi3(0x80, 60, 0);
        /* CC delle manopole: agiscono sul pad scelto (l'ultimo suonato, qui di nuovo A1) */
        app_select_pad(a, 0);
        midi3(0xB0, 70, 64);
        float vol_cc = sampler_pad(s, 0)->volume;
        midi3(0xB0, 71, 127);
        float pitch_cc = sampler_pad(s, 0)->pitch;
        midi3(0xB0, 74, 127);
        float bpm_cc = sampler_tempo(s);
        midi3(0xB0, 75, 127);
        float swing_cc = sampler_swing(s);
        midi3(0xB0, 76, 64);
        float master_cc = sampler_volume(s);
        midi3(0xB0, 76, 102);
        midi3(0xB0, 74, 51);                             /* 120 BPM */
        midi3(0xB0, 75, 0);
        midi3(0xB0, 71, 64);
        sampler_pad(s, 0)->pitch = 0.0f;
        midi3(0xB0, 70, 85);
        /* program change: canale 10 banco, altri pattern */
        midi2(0xC9, 1);
        int bank_pc = app_bank(a);
        midi2(0xC0, 2);
        int pat_pc = sampler_selected_pattern(s);
        midi2(0xC9, 0);
        midi2(0xC0, 0);
        /* clock MIDI a 140 BPM: 97 tick */
        sampler_set_tempo(s, 120.0f);
        midi1(0xFA);
        int started = sampler_playing(s);
        for (int i = 0; i < 97; i++) { midi1(0xF8); run(60.0f / 140.0f / 24.0f); }
        float bpm_clock = sampler_tempo(s);
        midi1(0xFC);
        int stopped = !sampler_playing(s);
        run(0.3f);
        printf("MIDI: pad canale 10 %d colpi; tasti: Do5 %d frame, Do4 %d; CC70 volume %.2f, CC71 pitch %.1f, CC74 %.0f BPM, CC75 swing %.2f, CC76 volume %.2f; PC banco %c pattern %c; start %d, clock -> %.0f BPM, stop %d\n",
               pad_hits, len_c5, len_c4, vol_cc, pitch_cc, bpm_cc, swing_cc, master_cc, 'A' + bank_pc, 'A' + pat_pc, started, bpm_clock, stopped);
        check(pad_hits == 2, "i pad MIDI del canale 10 non suonano i pad");
        check(fabsf((float)len_c5 / len_c4 - 0.5f) < 0.03f, "i tasti non suonano il pad in modo cromatico");
        check(fabsf(vol_cc - 0.756f) < 0.01f && fabsf(pitch_cc - 12.0f) < 0.01f && bpm_cc == 240.0f && fabsf(swing_cc - 0.6f) < 0.01f && fabsf(master_cc - 0.504f) < 0.01f, "le manopole CC70-77 non agiscono");
        check(bank_pc == 1 && pat_pc == 2, "il program change non cambia banco o pattern");
        check(started && fabsf(bpm_clock - 140.0f) <= 1.0f && stopped, "il clock MIDI non sincronizza il sequencer");
        app_midi_status(a, NULL);
        shot(out, "2-modifica.bmp", &c);
    }

    /* 10. Pagine Modifica e Sequenza con i tasti; schermate */
    {
        app_set_page(a, PAGE_EDIT);
        app_select_pad(a, 2);
        float pitch0 = sampler_pad(s, 2)->pitch;
        tap(PAD_DOWN, 0.02f);                           /* intonazione */
        tap(PAD_RIGHT, 0.02f);
        float pitch1 = sampler_pad(s, 2)->pitch;
        tap(PAD_L1, 0.02f);
        float pitch2 = sampler_pad(s, 2)->pitch;
        app_axes(a, 0, 0, 0, -1.0f, 0, 0);              /* levetta destra in su: +0,1 */
        app_axes(a, 0, 0, 0, 0, 0, 0);
        float pitch3 = sampler_pad(s, 2)->pitch;
        sampler_pad(s, 2)->pitch = 0.0f;
        uint32_t f2 = sampler_slot_flash(s, 2);
        tap(PAD_A, 0.05f);
        shot(out, "2-modifica.bmp", &c);
        run(0.6f);
        printf("modifica: intonazione %.1f -> destra %.1f -> L1 %.1f -> levetta %.1f; A ascolta (colpi %u)\n", pitch0, pitch1, pitch2, pitch3, sampler_slot_flash(s, 2) - f2);
        check(pitch1 == pitch0 + 1.0f && pitch2 == pitch1 - 12.0f && fabsf(pitch3 - (pitch2 + 0.1f)) < 0.01f, "la pagina Modifica non cambia l'intonazione");
        check(sampler_slot_flash(s, 2) - f2 == 1, "A nella pagina Modifica non fa ascoltare il pad");
        app_set_page(a, PAGE_SEQ);
        SeqPattern *pa = sampler_pattern_at(s, 0);
        sampler_select_pattern(s, 0);
        sampler_set_tempo(s, 120.0f);
        tap(PAD_DOWN, 0.02f);                           /* riga del pad A1 */
        tap(PAD_DOWN, 0.02f);                           /* A2 */
        tap(PAD_DOWN, 0.02f);                           /* A3 */
        tap(PAD_RIGHT, 0.02f);
        tap(PAD_A, 0.02f);                              /* passo 2 acceso */
        int v_on = pa->vel[2][1];
        tap(PAD_B, 0.02f);                              /* forza */
        int v_acc = pa->vel[2][1];
        tap(PAD_X, 0.02f);
        int muted = sampler_pad(s, 2)->mute;
        tap(PAD_X, 0.02f);
        tap(PAD_R1, 0.02f);
        float bpm_r1 = sampler_tempo(s);
        tap(PAD_L1, 0.02f);
        tap(PAD_L2, 0.02f);
        int len_l2 = pa->length;
        tap(PAD_R2, 0.02f);
        tap(PAD_START, 0.02f);
        run(0.3f);
        shot(out, "3-sequenza.bmp", &c);
        tap(PAD_START, 0.02f);
        run(0.3f);
        printf("sequenza: A accende il passo (%d), B forza (%d), X muto %d, R1 tempo %.0f, L2 passi %d\n", v_on, v_acc, muted, bpm_r1, len_l2);
        check(v_on == 100 && v_acc == 127 && muted == 1 && bpm_r1 == 121.0f && len_l2 == 15 && pa->length == 16, "la pagina Sequenza non modifica il pattern");
        tap(PAD_A, 0.02f);                              /* spegne il passo */
    }

    /* 11. Libreria: radici, cartella dei WAV di prova, anteprima con A, X assegna */
    {
        app_set_page(a, PAGE_LIB);
        app_select_pad(a, 9);
        tap(PAD_DOWN, 0.02f);                           /* libreria di serie (= wav di prova) */
        tap(PAD_A, 0.02f);                              /* entra */
        shot(out, "4-libreria.bmp", &c);
        uint32_t fp = sampler_slot_flash(s, SP_PREVIEW);
        tap(PAD_A, 0.05f);                              /* anteprima del primo file */
        run(0.3f);
        int preview = (int)(sampler_slot_flash(s, SP_PREVIEW) - fp);
        tap(PAD_X, 0.02f);
        run(0.05f);
        const Sample *assigned = sampler_sample(s, 9);
        printf("libreria: anteprima %d colpo, X assegna \"%s\" al pad B2\n", preview, assigned ? assigned->name : "(niente)");
        check(preview == 1 && assigned != NULL, "la libreria non fa ascoltare o assegnare");
        tap(PAD_B, 0.02f);
        tap(PAD_B, 0.02f);
        check(app_page(a) == PAGE_PADS, "B dalla radice della libreria non torna ai pad");
    }

    /* 12. Registrazione dal microfono: livello, soglia, salvataggio e assegnazione al pad */
    {
        app_set_page(a, PAGE_REC);
        shot(out, "5-registra-senza-microfono.bmp", &c);
        app_select_pad(a, 10);
        tap(PAD_A, 0.02f);
        int none = sampler_rec_state(s) == REC_IDLE;
        app_capture_status(a, "Microfono di prova", 44100.0f);
        float cap[1024];
        double ph = 0;
        tap(PAD_A, 0.02f);                              /* avvia */
        int st_on = sampler_rec_state(s);
        for (int done = 0; done < 44100; done += 1024) {   /* 1 s di sinusoide a 44,1 kHz */
            int n = 1024 < 44100 - done ? 1024 : 44100 - done;
            for (int i = 0; i < n; i++) { cap[i] = 0.5f * (float)sin(ph); ph += 2 * PI * 300 / 44100.0; }
            app_capture(a, cap, n);
            run(n / 44100.0f);
        }
        float level = sampler_capture_level(s);
        shot(out, "5-registra.bmp", &c);
        tap(PAD_A, 0.02f);                              /* ferma: salva e assegna */
        run(0.1f);
        const Sample *rec = sampler_sample(s, 10);
        int ch = 0, frames_file = app_last_recording(a)[0] ? wav_frames(app_last_recording(a), &ch) : -1;
        printf("registrazione: senza microfono resta ferma %d; con microfono a 44,1 kHz stato %d, livello %.2f, file %s (%d frame, %d canale), pad B3 %d frame\n",
               none, st_on, level, app_last_recording(a), frames_file, ch, rec ? rec->frames : -1);
        check(none, "senza microfono la registrazione parte lo stesso");
        check(st_on == REC_ON && level > 0.4f && level < 0.6f, "il livello del microfono non viene misurato");
        check(frames_file > 0 && abs(frames_file - 48000) < 2400 && ch == 1 && strstr(app_last_recording(a), "/campioni/reg-") != NULL, "il WAV registrato non e' come previsto");
        check(rec && abs(rec->frames - 48000) < 2400, "la registrazione non finisce nel pad scelto");
        /* soglia: aspetta il suono */
        tap(PAD_RIGHT, 0.02f);
        tap(PAD_RIGHT, 0.02f);                          /* -30 dB */
        tap(PAD_A, 0.02f);
        memset(cap, 0, sizeof(cap));
        for (int k = 0; k < 10; k++) { app_capture(a, cap, 1024); run(0.02f); }
        int waiting = sampler_rec_state(s) == REC_WAIT && sampler_rec_frames(s) == 0;
        for (int i = 0; i < 1024; i++) cap[i] = 0.3f * (float)sin(2 * PI * 300 * i / 44100.0);
        for (int k = 0; k < 10; k++) { app_capture(a, cap, 1024); run(0.02f); }
        int going = sampler_rec_state(s) == REC_ON && sampler_rec_frames(s) > 5000;
        tap(PAD_A, 0.02f);
        run(0.1f);
        printf("soglia: in attesa %d, poi registra %d; pad B3 ora %d frame\n", waiting, going, sampler_sample(s, 10) ? sampler_sample(s, 10)->frames : -1);
        check(waiting && going, "la soglia di avvio non aspetta il suono");
        app_capture_status(a, NULL, 0.0f);
        tap(PAD_LEFT, 0.02f);
        tap(PAD_LEFT, 0.02f);
    }

    /* 13. Opzioni: metronomo, volume, esportazione in WAV */
    {
        app_set_page(a, PAGE_OPTIONS);
        tap(PAD_DOWN, 0.02f);
        tap(PAD_DOWN, 0.02f);
        tap(PAD_A, 0.02f);                              /* metronomo */
        int metro = sampler_metronome(s);
        sampler_play(s, 1);
        run_into(0.3f, tmp, tmp_cap);
        float rms_click = rms_range(tmp, 0, 2000);
        sampler_play(s, 0);
        tap(PAD_A, 0.02f);
        tap(PAD_DOWN, 0.02f);
        tap(PAD_LEFT, 0.02f);
        float vol = sampler_volume(s);
        tap(PAD_RIGHT, 0.02f);
        shot(out, "6-opzioni.bmp", &c);
        /* esportazione del pattern A (pad 1 a quattro passi, pad 2 a uno) */
        sampler_select_pattern(s, 0);
        sampler_set_chain(s, 0);
        sampler_set_tempo(s, 120.0f);
        int r = app_export_pattern(a);
        int ch = 0, ef = app_last_export(a)[0] ? wav_frames(app_last_export(a), &ch) : -1;
        sampler_select_pattern(s, 3);
        int r_empty = app_export_pattern(a);
        sampler_select_pattern(s, 0);
        run(0.3f);
        printf("opzioni: metronomo %d (RMS del click %.3f), volume con sinistra %.2f; esportazione %d -> %s (%.2f s, %d canali); pattern vuoto %d\n",
               metro, rms_click, vol, r, app_last_export(a), (double)ef / SR, ch, r_empty);
        check(metro == 1 && rms_click > 0.02f && !sampler_metronome(s), "il metronomo non suona o non si spegne");
        check(fabsf(vol - 0.75f) < 0.01f, "sinistra non abbassa il volume generale");
        check(r == 0 && ef >= 2 * SR && ef <= (int)(4.6f * SR) && ch == 2 && strstr(app_last_export(a), "/esportazioni/pattern-A-") != NULL, "l'esportazione del pattern non va");
        check(r_empty != 0, "un pattern vuoto viene esportato");
    }

    /* 14. Stato: salvataggio, modifica e ricaricamento; WAV mancante */
    {
        app_select_pad(a, 2);
        sampler_pad(s, 2)->pitch = -3.5f;
        sampler_pad(s, 2)->mode = MODE_HOLD;
        sampler_pad(s, 2)->group = 2;
        sampler_set_tempo(s, 133.0f);
        sampler_set_swing(s, 0.2f);
        sampler_pattern_at(s, 2)->length = 12;
        sampler_pattern_at(s, 2)->vel[5][7] = 99;
        app_save(a);
        sampler_pad(s, 2)->pitch = 0.0f;
        sampler_pad(s, 2)->mode = MODE_ONESHOT;
        sampler_set_tempo(s, 90.0f);
        sampler_pattern_clear(sampler_pattern_at(s, 2));
        sampler_set_sample(s, 0, NULL);
        run(0.05f);
        App *a2 = app_create(s, state);
        run(0.05f);
        const Sample *s0 = sampler_sample(s, 0);
        printf("stato: pad A3 pitch %.1f modo %d gruppo %d, tempo %.0f, swing %.2f, pattern C %d passi (pad 6 passo 8 = %d), pad A1 \"%s\"\n",
               sampler_pad(s, 2)->pitch, sampler_pad(s, 2)->mode, sampler_pad(s, 2)->group, sampler_tempo(s), sampler_swing(s),
               sampler_pattern_at(s, 2)->length, sampler_pattern_at(s, 2)->vel[5][7], s0 ? s0->name : "(niente)");
        check(sampler_pad(s, 2)->pitch == -3.5f && sampler_pad(s, 2)->mode == MODE_HOLD && sampler_pad(s, 2)->group == 2 && sampler_tempo(s) == 133.0f &&
              fabsf(sampler_swing(s) - 0.2f) < 1e-4f && sampler_pattern_at(s, 2)->length == 12 && sampler_pattern_at(s, 2)->vel[5][7] == 99 &&
              s0 && !strcmp(s0->path, f_mono), "lo stato riaperto non corrisponde");
        app_destroy(a2);
        g_a = a;
        /* WAV mancante: il pad resta vuoto */
        char st2[600];
        snprintf(st2, sizeof(st2), "%s/stato-mancante.txt", out);
        FILE *f = fopen(st2, "w");
        fprintf(f, "version=1\npad0.file=%s/non-esiste.wav\npad0.pitch=2\npad1.file=%s\n", wavdir, f_8bit);
        fclose(f);
        Sampler *s2 = sampler_create(SR);
        App *a3 = app_create(s2, st2);
        float buf[2 * 256];
        sampler_render(s2, buf, 256);
        check(sampler_sample(s2, 0) == NULL && sampler_sample(s2, 1) != NULL && sampler_pad(s2, 0)->pitch == 2.0f, "un WAV mancante non lascia il pad vuoto");
        app_destroy(a3);
        sampler_destroy(s2);
        remove(st2);
    }

    char outw[700];
    snprintf(outw, sizeof(outw), "%s/demo.wav", out);
    {
        FILE *f = fopen(outw, "wb");
        if (f) {
            uint32_t dlen = (uint32_t)g_pos * 4, riff = 36 + dlen, br = SR * 4;
            unsigned char h[44] = { 'R', 'I', 'F', 'F' };
            h[4] = riff; h[5] = riff >> 8; h[6] = riff >> 16; h[7] = riff >> 24;
            memcpy(h + 8, "WAVEfmt ", 8);
            h[16] = 16; h[20] = 1; h[22] = 2; h[24] = SR & 255; h[25] = (SR >> 8) & 255; h[28] = br & 255; h[29] = (br >> 8) & 255; h[30] = (br >> 16) & 255; h[32] = 4; h[34] = 16;
            memcpy(h + 36, "data", 4);
            h[40] = dlen; h[41] = dlen >> 8; h[42] = dlen >> 16; h[43] = dlen >> 24;
            fwrite(h, 1, 44, f);
            for (int i = 0; i < g_pos * 2; i++) { float v = g_wav[i] < -1 ? -1 : (g_wav[i] > 1 ? 1 : g_wav[i]); int16_t sv = (int16_t)lrintf(v * 32767); fwrite(&sv, 2, 1, f); }
            fclose(f);
        }
    }
    {
        double sum2 = 0;
        float peak = 0;
        int nans = 0;
        for (int i = 0; i < g_pos * 2; i++) {
            float v = g_wav[i];
            if (v != v) { nans++; continue; }
            if (fabsf(v) > peak) peak = fabsf(v);
            sum2 += (double)v * v;
        }
        printf("audio: %.1f s, picco %.3f, RMS %.3f, campioni NaN %d\n", (double)g_pos / SR, peak, sqrt(sum2 / (g_pos * 2)), nans);
        check(!nans && peak > 0.05f && peak <= 1.0f, "audio assente o non valido");
    }

    /* 16. Prestazioni: 16 voci in loop, 10 secondi */
    {
        for (int p = 0; p < SP_PADS; p++) {
            app_assign_wav(a, p, f_lungo);
            PadParams *pp = sampler_pad(s, p);
            sampler_pad_defaults(pp);
            pp->mode = MODE_LOOP;
            pp->pitch = (float)(p % 5) - 2.0f;
        }
        float buf[2 * 1024];
        sampler_render(s, buf, 64);
        for (int p = 0; p < SP_PADS; p++) sampler_trigger(s, p, 0.8f, 0.0f, -1);
        sampler_render(s, buf, 64);
        int v_perf = sampler_active_voices(s);
        clock_t t0 = clock();
        for (int i = 0; i < SR * 10 / 1024; i++) sampler_render(s, buf, 1024);
        double secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
        printf("prestazioni: 10 s di audio con %d voci in %.3f s (%.0fx il tempo reale)\n", v_perf, secs, 10.0 / secs);
        check(v_perf == 16 && secs < 10.0, "16 voci non stanno nel tempo reale");
        sampler_release_all(s);
        sampler_render(s, buf, 1024);
        sampler_render(s, buf, 1024);
    }

    /* 17. Ridisegno solo quando serve e tempo di disegno */
    {
        g_a = a;
        app_set_page(a, PAGE_PADS);
        run(3.5f);                                      /* passano messaggi, luci e code */
        app_needs_draw(a);
        int again = app_needs_draw(a);
        app_select_pad(a, 5);
        int changed = app_needs_draw(a);
        int after = app_needs_draw(a);
        printf("ridisegno: da fermo %d, dopo un cambio %d, subito dopo %d\n", again, changed, after);
        check(!again && changed && !after, "il ridisegno non e' limitato ai cambiamenti");
        clock_t t0 = clock();
        for (int i = 0; i < 60; i++) app_draw(a, &c);
        printf("disegno: %.2f ms per fotogramma\n", 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC / 60);
    }

    /* 18. Uscita (per ultima: la finestra resta aperta mentre l'app chiude) */
    tap(PAD_MENU, 0.03f);
    shot(out, "7-uscita.bmp", &c);
    tap(PAD_B, 0.03f);
    check(!app_wants_quit(a), "B nella finestra di uscita esce");
    tap(PAD_MENU, 0.03f);
    tap(PAD_A, 0.03f);
    check(app_wants_quit(a), "A nella finestra di uscita non esce");

    app_destroy(a);
    sampler_destroy(s);
    gfx_free_fonts();
    free(px);
    free(g_wav);
    free(tmp);
    printf("%s\n", fail ? "PROVE FALLITE" : "tutte le prove sono passate");
    return fail;
}
