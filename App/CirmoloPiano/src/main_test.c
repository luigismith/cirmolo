/* Cirmolo Piano - prove senza console: SoundFont, tasti, MIDI, stato salvato, schermate (BMP),
 * audio (WAV), statistiche e prestazioni.
 *
 * Uso: piano-test <cartella dei font> <cartella di uscita> <file .sf2>
 * I font sono quelli di PyUI: BeVietnamPro-Regular.ttf e BeVietnamPro-SemiBold.ttf.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "app.h"
#include "gfx.h"
#include "piano.h"

#define W 640
#define H 480
#define SR 48000

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

static void write_wav(const char *path, const float *lr, int frames)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    int data = frames * 4;
    unsigned char h[44] = { 'R', 'I', 'F', 'F' };
    int riff = 36 + data;
    memcpy(h + 4, &riff, 4); memcpy(h + 8, "WAVEfmt ", 8);
    int fmt = 16; short pcm = 1, ch = 2, bits = 16; int sr = SR, br = SR * 4; short ba = 4;
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

static Piano *g_p;
static App *g_a;
static float *g_wav;
static int g_pos, g_cap;
static float g_last_rms;   /* RMS dell'ultimo tratto reso */

/* Fa avanzare insieme interfaccia e audio, come farebbe il ciclo della console a 60 fps. */
static void run(float seconds)
{
    int frames = (int)(seconds * SR), block = SR / 60;
    float buf[2 * 1024];
    double sum2 = 0;
    int n_all = 0;
    for (int done = 0; done < frames; done += block) {
        int n = block < frames - done ? block : frames - done;
        piano_render(g_p, buf, n);
        for (int i = 0; i < 2 * n; i++) sum2 += (double)buf[i] * buf[i];
        n_all += 2 * n;
        if (g_wav && g_pos + n <= g_cap) { memcpy(g_wav + 2 * g_pos, buf, sizeof(float) * 2 * n); g_pos += n; }
        app_update(g_a, (float)n / SR);
    }
    g_last_rms = n_all ? (float)sqrt(sum2 / n_all) : 0.0f;
}

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

int main(int argc, char **argv)
{
    if (argc < 4) { fprintf(stderr, "uso: %s <cartella font> <cartella uscita> <file sf2>\n", argv[0]); return 2; }
    char p1[512], p2[512], state[600];
    snprintf(p1, sizeof(p1), "%s/BeVietnamPro-Regular.ttf", argv[1]);
    snprintf(p2, sizeof(p2), "%s/BeVietnamPro-SemiBold.ttf", argv[1]);
    if (gfx_load_fonts(p1, p2)) return 1;

    /* 1. SoundFont: deve avere i 128 strumenti GM nel banco 0 e una batteria nel banco 128 */
    clock_t t0 = clock();
    Piano *p = g_p = piano_create(SR);
    check(piano_load(p, argv[3]) == 0, "SoundFont non caricato");
    double load_s = (double)(clock() - t0) / CLOCKS_PER_SEC;
    int gm = 0;
    for (int i = 0; i < 128; i++) if (piano_find_preset(p, 0, i) >= 0) gm++;
    printf("SoundFont: %d preset, %d/128 strumenti GM, batteria %s, caricato in %.2f s\n",
           piano_preset_count(p), gm, piano_find_preset(p, 128, 0) >= 0 ? "si'" : "no", load_s);
    check(gm == 128, "mancano strumenti GM nel banco 0");
    check(piano_find_preset(p, 128, 0) >= 0, "manca la batteria (banco 128)");
    check(!strcmp(piano_preset_name(p, piano_channel_preset(p, 9)), piano_preset_name(p, piano_find_preset(p, 128, 0))),
          "il canale 10 non parte con la batteria");

    snprintf(state, sizeof(state), "%s/stato-prova.txt", argv[2]);
    remove(state);
    App *a = g_a = app_create(p, state, argv[3]);
    uint32_t *px = malloc(sizeof(uint32_t) * W * H);
    Canvas c = { px, W, H };
    g_cap = SR * 40;
    g_wav = calloc((size_t)g_cap * 2, sizeof(float));
    check(!strncmp(piano_preset_name(p, app_instrument(a)), "Grand Piano", 11) || piano_preset_program(p, app_instrument(a)) == 0,
          "lo strumento iniziale non e' il programma 1");

    /* 2. Suona: la tastierina della Flip, una nota tenuta e la scala */
    app_button(a, PAD_LEFT, 1);
    run(0.3f);
    int v_held = piano_active_voices(p);
    check(v_held > 0, "la nota della croce non suona");
    check(app_key_lit(a, 60), "il Do4 non si accende sulla tastiera");
    shot(argv[2], "1-suona.bmp", &c);
    app_button(a, PAD_LEFT, 0);
    run(0.2f);
    check(!app_key_lit(a, 60), "il Do4 resta acceso dopo il rilascio");
    static const int scale_btns[8] = { PAD_LEFT, PAD_DOWN, PAD_RIGHT, PAD_UP, PAD_Y, PAD_B, PAD_A, PAD_X };
    for (int i = 0; i < 8; i++) tap(scale_btns[i], 0.22f);
    float rms_scale = g_last_rms;
    run(1.0f);
    printf("tastierina: %d voci con una nota, RMS della scala %.3f\n", v_held, rms_scale);
    check(rms_scale > 0.005f, "la scala e' silenziosa");

    /* 3. START: accordo di prova (4 note), che si spegne da solo */
    tap(PAD_START, 0.03f);
    run(0.3f);
    int lit_chord = 0;
    for (int k = 0; k < 128; k++) lit_chord += app_key_lit(a, k);
    int v_chord = piano_active_voices(p);
    run(2.5f);
    int lit_after = 0;
    for (int k = 0; k < 128; k++) lit_after += app_key_lit(a, k);
    printf("accordo di prova: %d tasti accesi, %d voci; dopo 2,5 s tasti accesi %d\n", lit_chord, v_chord, lit_after);
    check(lit_chord == 4 && v_chord >= 4 && lit_after == 0, "l'accordo di prova non va come previsto");

    /* 4. L1/R1 e L2/R2 cambiano strumento; L3/R3 l'ottava */
    int prog0 = piano_preset_program(p, app_instrument(a));
    tap(PAD_R1, 0.03f);
    int prog1 = piano_preset_program(p, app_instrument(a));
    tap(PAD_R2, 0.03f);
    int prog9 = piano_preset_program(p, app_instrument(a));
    tap(PAD_L2, 0.03f);
    tap(PAD_L1, 0.03f);
    int prog_back = piano_preset_program(p, app_instrument(a));
    tap(PAD_R3, 0.03f);
    int oct_up = app_octave(a);
    tap(PAD_L3, 0.03f);
    printf("strumento: programma %d -> R1 %d -> R2 %d -> L2+L1 %d; ottava con R3 %d\n", prog0 + 1, prog1 + 1, prog9 + 1, prog_back + 1, oct_up);
    check(prog1 == prog0 + 1 && prog9 == prog0 + 9 && prog_back == prog0 && oct_up == 5, "L1/R1, L2/R2 o L3/R3 non fanno quel che devono");

    /* 5. Pagina Strumenti: elenco, scelta con A (strumento della famiglia Archi) */
    tap(PAD_SELECT, 0.03f);
    check(app_page(a) == PAGE_INSTR, "SELECT non porta alla pagina Strumenti");
    for (int i = 0; i < 5; i++) tap(PAD_RIGHT, 0.03f);                /* 40 strumenti piu' avanti: Violin */
    shot(argv[2], "2-strumenti.bmp", &c);
    tap(PAD_A, 0.03f);
    run(0.3f);
    int chosen = app_instrument(a);
    printf("pagina Strumenti: scelto %s (programma %d), voci della nota di prova %d\n",
           piano_preset_name(p, chosen), piano_preset_program(p, chosen) + 1, piano_active_voices(p));
    check(piano_preset_program(p, chosen) == 40 && piano_preset_bank(p, chosen) == 0, "A non sceglie il Violin (programma 41)");
    run(1.0f);
    tap(PAD_B, 0.03f);
    check(app_page(a) == PAGE_PLAY, "B non torna a Suona");

    /* 6. Tastiera MIDI: note, velocity, sustain, pitch bend, modulazione, program change, batteria */
    {
        app_midi_status(a, "MPKminiIV");
        const unsigned char on[3] = { 0x90, 64, 100 }, off[3] = { 0x80, 64, 0 };
        const unsigned char sus_on[3] = { 0xB0, 64, 127 }, sus_off[3] = { 0xB0, 64, 0 };
        const unsigned char bend_up[3] = { 0xE0, 0x7F, 0x7F }, bend_mid[3] = { 0xE0, 0x00, 0x40 };
        const unsigned char mod_on[3] = { 0xB0, 1, 127 }, mod_off[3] = { 0xB0, 1, 0 };
        const unsigned char prog[2] = { 0xC0, 0 }, vol[3] = { 0xB0, 7, 100 };
        const unsigned char pad[3] = { 0x99, 36, 120 }, pad_off[3] = { 0x89, 36, 0 };
        const unsigned char all_off[3] = { 0xB0, 123, 0 };
        app_midi(a, prog, 2);                                           /* Grand Piano */
        check(piano_preset_program(p, app_instrument(a)) == 0, "il program change non cambia strumento");
        app_midi(a, vol, 3);
        app_midi(a, on, 3);
        run(0.3f);
        int v_on = piano_active_voices(p);
        int lit_on = app_key_lit(a, 64);
        app_midi(a, sus_on, 3);
        app_midi(a, off, 3);                                            /* tenuta dal pedale */
        run(0.5f);
        int v_sus = piano_active_voices(p);
        float rms_sus = g_last_rms;
        app_midi(a, bend_up, 3);
        app_midi(a, mod_on, 3);
        run(0.3f);
        app_midi(a, bend_mid, 3);
        app_midi(a, mod_off, 3);
        app_midi(a, sus_off, 3);                                        /* ora si rilascia */
        run(3.0f);
        int v_after = piano_active_voices(p);
        app_midi(a, pad, 3);
        run(0.05f);                                                     /* la cassa e' corta: si guarda subito */
        int drum_lit = app_key_lit(a, 36), v_drum = piano_active_voices(p);
        float rms_drum = g_last_rms;
        shot(argv[2], "3-midi.bmp", &c);
        run(0.2f);
        app_midi(a, pad_off, 3);
        app_midi(a, all_off, 3);
        run(1.5f);
        printf("MIDI: voci con la nota %d (accesa %d), tenuta dal sustain %d (RMS %.3f), dopo il rilascio %d; pad: acceso %d, voci %d, RMS %.3f\n",
               v_on, lit_on, v_sus, rms_sus, v_after, drum_lit, v_drum, rms_drum);
        check(v_on > 0 && lit_on && v_sus > 0 && rms_sus > 0.002f && v_after == 0 && drum_lit && v_drum > 0 && rms_drum > 0.002f,
              "MIDI non gestito come previsto");
        app_midi_status(a, NULL);
    }

    /* 7. Opzioni: volume e ottava, salvataggio e riapertura dello stato */
    tap(PAD_SELECT, 0.03f);
    tap(PAD_SELECT, 0.03f);
    check(app_page(a) == PAGE_OPTIONS, "la terza pagina non e' Opzioni");
    float vol0 = app_volume(a);
    tap(PAD_LEFT, 0.03f);
    tap(PAD_LEFT, 0.03f);
    float vol1 = app_volume(a);
    tap(PAD_DOWN, 0.03f);
    tap(PAD_RIGHT, 0.03f);                                              /* ottava 5 */
    shot(argv[2], "4-opzioni.bmp", &c);
    tap(PAD_A, 0.03f);                                                  /* nota di prova */
    run(0.6f);
    app_set_instrument(a, piano_find_preset(p, 0, 24));                 /* Nylon Guitar */
    app_save(a);
    App *a2 = app_create(p, state, argv[3]);
    printf("stato: volume %.2f -> %.2f, riaperto %.2f; ottava %d; strumento riaperto %s\n",
           vol0, vol1, app_volume(a2), app_octave(a2), piano_preset_name(p, app_instrument(a2)));
    check(fabsf(vol1 - (vol0 - 0.1f)) < 1e-3f, "sinistra non abbassa il volume di 5 %% alla volta");
    check(fabsf(app_volume(a2) - vol1) < 1e-4f && app_octave(a2) == 5 && piano_preset_program(p, app_instrument(a2)) == 24,
          "lo stato riaperto non corrisponde");
    app_destroy(a2);
    g_a = a;
    tap(PAD_B, 0.03f);

    /* 8. Richiesta di uscita */
    tap(PAD_MENU, 0.03f);
    shot(argv[2], "5-uscita.bmp", &c);
    tap(PAD_B, 0.03f);
    check(!app_wants_quit(a), "B nella finestra di uscita esce");
    tap(PAD_MENU, 0.03f);
    tap(PAD_A, 0.03f);
    check(app_wants_quit(a), "A nella finestra di uscita non esce");

    char out[700];
    snprintf(out, sizeof(out), "%s/demo.wav", argv[2]);
    write_wav(out, g_wav, g_pos);

    /* statistiche dell'audio */
    double sum2 = 0;
    float peak = 0;
    int nans = 0, clip = 0;
    for (int i = 0; i < g_pos * 2; i++) {
        float v = g_wav[i];
        if (v != v) { nans++; continue; }
        if (fabsf(v) > peak) peak = fabsf(v);
        if (fabsf(v) > 0.99f) clip++;
        sum2 += (double)v * v;
    }
    printf("audio: %.1f s, picco %.3f, RMS %.3f, campioni NaN %d, sopra 0.99: %d (%.3f%%)\n",
           (double)g_pos / SR, peak, sqrt(sum2 / (g_pos * 2)), nans, clip, 100.0 * clip / (g_pos * 2));
    check(!nans && peak > 0.05f, "audio assente o non valido");

    /* prestazioni: 24 note di pianoforte tenute, 10 secondi */
    app_set_instrument(a, piano_find_preset(p, 0, 0));
    for (int n = 0; n < 24; n++) piano_note_on(p, 0, 36 + n * 2, 0.8f);
    float buf[2 * 1024];
    piano_render(p, buf, 64);
    int v_perf = piano_active_voices(p);
    t0 = clock();
    for (int i = 0; i < SR * 10 / 1024; i++) piano_render(p, buf, 1024);
    double secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
    printf("prestazioni: 10 s di audio con %d voci in %.3f s (%.0fx il tempo reale)\n", v_perf, secs, 10.0 / secs);
    piano_all_sounds_off(p);
    piano_render(p, buf, 64);

    /* disegno: tempo medio per fotogramma */
    app_set_page(a, PAGE_PLAY);
    t0 = clock();
    for (int i = 0; i < 60; i++) app_draw(a, &c);
    printf("disegno: %.2f ms per fotogramma\n", 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC / 60);

    app_destroy(a);
    piano_destroy(p);
    gfx_free_fonts();
    free(px);
    free(g_wav);
    printf("%s\n", fail ? "PROVE FALLITE" : "tutte le prove sono passate");
    return fail;
}
