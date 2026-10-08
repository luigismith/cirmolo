/* Cirmolo Synth - prove senza console: schermate (BMP), audio (WAV), statistiche e prestazioni.
 *
 * Uso: synth-test <cartella dei font> <cartella di uscita>
 * I font sono quelli di PyUI: BeVietnamPro-Regular.ttf e BeVietnamPro-SemiBold.ttf.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "app.h"
#include "gfx.h"
#include "synth.h"

#define W 640
#define H 480
#define SR 48000

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

/* Fa avanzare insieme interfaccia e audio, come farebbe il ciclo della console a 60 fps. */
static void run(App *a, Synth *s, float seconds, float *dst, int *pos, int cap)
{
    int frames = (int)(seconds * SR), block = SR / 60;
    float buf[2 * 1024];
    for (int done = 0; done < frames; done += block) {
        int n = block < frames - done ? block : frames - done;
        synth_render(s, buf, n);
        if (dst && *pos + n <= cap) { memcpy(dst + 2 * *pos, buf, sizeof(float) * 2 * n); *pos += n; }
        app_update(a, (float)n / SR);
    }
}

static void tap(App *a, Synth *s, int b, float hold, float *dst, int *pos, int cap)
{
    app_button(a, b, 1);
    run(a, s, hold, dst, pos, cap);
    app_button(a, b, 0);
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "uso: %s <cartella font> <cartella uscita>\n", argv[0]); return 2; }
    char p1[512], p2[512], out[600];
    snprintf(p1, sizeof(p1), "%s/BeVietnamPro-Regular.ttf", argv[1]);
    snprintf(p2, sizeof(p2), "%s/BeVietnamPro-SemiBold.ttf", argv[1]);
    if (gfx_load_fonts(p1, p2)) return 1;

    Synth *s = synth_create(SR);
    snprintf(out, sizeof(out), "%s/stato-prova.txt", argv[2]);
    remove(out);
    App *a = app_create(s, out);
    uint32_t *px = malloc(sizeof(uint32_t) * W * H);
    Canvas c = { px, W, H };
    int cap = SR * 40, pos = 0;
    float *wav = calloc((size_t)cap * 2, sizeof(float));
    int fail = 0;

    /* 1. Suona: accordo con R2 e levetta del filtro, poi una scala */
    app_button(a, PAD_R2, 1);
    app_axes(a, 0.0f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f);
    app_button(a, PAD_LEFT, 1);
    run(a, s, 1.2f, wav, &pos, cap);
    app_draw(a, &c);
    snprintf(out, sizeof(out), "%s/1-suona.bmp", argv[2]);
    write_bmp(out, &c);
    app_button(a, PAD_LEFT, 0);
    app_button(a, PAD_R2, 0);
    app_axes(a, 0, 0, 0, 0, 0, 0);
    run(a, s, 1.0f, wav, &pos, cap);
    static const int scale_btns[8] = { PAD_LEFT, PAD_DOWN, PAD_RIGHT, PAD_UP, PAD_Y, PAD_B, PAD_A, PAD_X };
    for (int i = 0; i < 8; i++) tap(a, s, scale_btns[i], 0.22f, wav, &pos, cap);
    run(a, s, 1.5f, wav, &pos, cap);

    /* 2. Suono: preset Basso Dolomiti e taglio del filtro */
    app_button(a, PAD_SELECT, 1); app_button(a, PAD_SELECT, 0);
    tap(a, s, PAD_RIGHT, 0.05f, wav, &pos, cap);                       /* Preset -> Basso Dolomiti */
    for (int i = 0; i < 10; i++) tap(a, s, PAD_DOWN, 0.03f, wav, &pos, cap);
    tap(a, s, PAD_A, 0.6f, wav, &pos, cap);
    run(a, s, 0.4f, wav, &pos, cap);
    app_draw(a, &c);
    snprintf(out, sizeof(out), "%s/2-suono.bmp", argv[2]);
    write_bmp(out, &c);

    /* 3. Sequenza: torna al pad, suona 8 secondi di pattern */
    app_load_preset(a, 0);
    app_button(a, PAD_SELECT, 1); app_button(a, PAD_SELECT, 0);
    tap(a, s, PAD_DOWN, 0.03f, wav, &pos, cap);
    tap(a, s, PAD_RIGHT, 0.03f, wav, &pos, cap);
    tap(a, s, PAD_RIGHT, 0.03f, wav, &pos, cap);
    tap(a, s, PAD_START, 0.03f, wav, &pos, cap);
    run(a, s, 1.3f, wav, &pos, cap);
    app_draw(a, &c);
    snprintf(out, sizeof(out), "%s/3-sequenza.bmp", argv[2]);
    write_bmp(out, &c);
    run(a, s, 7.0f, wav, &pos, cap);
    tap(a, s, PAD_START, 0.03f, wav, &pos, cap);
    run(a, s, 2.0f, wav, &pos, cap);

    /* 3b. Catena: A in catena, A copiato in B, B in catena; dopo una battuta deve suonare B */
    tap(a, s, PAD_UP, 0.03f, wav, &pos, cap);
    tap(a, s, PAD_UP, 0.03f, wav, &pos, cap);                          /* riga dei pattern */
    tap(a, s, PAD_A, 0.03f, wav, &pos, cap);                           /* A in catena */
    tap(a, s, PAD_B, 0.03f, wav, &pos, cap);                           /* copia A in B */
    tap(a, s, PAD_RIGHT, 0.03f, wav, &pos, cap);                       /* scegli B */
    tap(a, s, PAD_A, 0.03f, wav, &pos, cap);                           /* B in catena */
    if (synth_chain(s) != 3u) { fprintf(stderr, "ERRORE: catena %u invece di 3\n", synth_chain(s)); fail = 1; }
    tap(a, s, PAD_START, 0.03f, wav, &pos, cap);
    run(a, s, 0.5f, wav, &pos, cap);
    int first = synth_playing_pattern(s);
    float bar = 16.0f * 60.0f / synth_tempo(s) / 4.0f;
    run(a, s, bar, wav, &pos, cap);
    int second = synth_playing_pattern(s);
    printf("catena: suona %c, dopo una battuta %c\n", 'A' + first, 'A' + second);
    if (first != 0 || second != 1) { fprintf(stderr, "ERRORE: la catena non passa da A a B\n"); fail = 1; }
    app_draw(a, &c);
    snprintf(out, sizeof(out), "%s/5-pattern.bmp", argv[2]);
    write_bmp(out, &c);
    tap(a, s, PAD_START, 0.03f, wav, &pos, cap);
    run(a, s, 1.0f, wav, &pos, cap);

    /* 3c. Arpeggiatore e registrazione: una nota tenuta, arpeggio su due ottave, registrato con R3 */
    char rec_path[700];
    int before = 0;
    for (int i = 1; i < 1000; i++) {
        snprintf(rec_path, sizeof(rec_path), "%s/registrazioni/registrazione-%03d.wav", argv[2], i);
        FILE *t = fopen(rec_path, "rb");
        if (!t) break;
        fclose(t);
        before = i;
    }
    app_set_screen(a, SCREEN_PLAY);
    synth_patch(s)->arp_mode = ARP_UP;
    synth_patch(s)->arp_rate = 1.0f;
    synth_patch(s)->arp_octaves = 2;
    tap(a, s, PAD_R3, 0.03f, wav, &pos, cap);
    app_button(a, PAD_LEFT, 1);
    app_button(a, PAD_R2, 1);                                           /* accordo arpeggiato */
    run(a, s, 1.5f, wav, &pos, cap);
    app_draw(a, &c);
    snprintf(out, sizeof(out), "%s/6-arpeggio.bmp", argv[2]);
    write_bmp(out, &c);
    app_button(a, PAD_LEFT, 0);
    app_button(a, PAD_R2, 0);
    run(a, s, 0.5f, wav, &pos, cap);
    tap(a, s, PAD_R3, 0.03f, wav, &pos, cap);
    synth_patch(s)->arp_mode = ARP_OFF;
    snprintf(rec_path, sizeof(rec_path), "%s/registrazioni/registrazione-%03d.wav", argv[2], before + 1);
    FILE *rf = fopen(rec_path, "rb");
    long rsize = 0;
    if (rf) { fseek(rf, 0, SEEK_END); rsize = ftell(rf); fclose(rf); }
    printf("registrazione: %s, %ld byte (%.2f s)\n", rec_path, rsize, (rsize - 44) / 4.0 / SR);
    if (rsize < 44 + SR * 4) { fprintf(stderr, "ERRORE: registrazione mancante o troppo corta\n"); fail = 1; }

    /* 3d. Preset tuo: Y nella pagina Suono, poi lo stato riaperto deve ridare lo stesso suono */
    app_set_screen(a, SCREEN_SOUND);
    synth_patch(s)->cutoff = 0.37f;
    synth_patch(s)->osc2_semi = 5;
    tap(a, s, PAD_Y, 0.03f, wav, &pos, cap);
    SynthPatch saved = *synth_patch(s);
    snprintf(out, sizeof(out), "%s/stato-prova.txt", argv[2]);
    app_save(a);
    app_load_preset(a, 3);                                             /* cambia suono... */
    App *a2 = app_create(s, out);                                      /* ...e riapre lo stato salvato */
    printf("preset tuo: taglio %.2f, intervallo %+.0f (attesi %.2f, %+.0f)\n",
           synth_patch(s)->cutoff, synth_patch(s)->osc2_semi, saved.cutoff, saved.osc2_semi);
    if (fabsf(synth_patch(s)->cutoff - saved.cutoff) > 1e-4f || synth_patch(s)->osc2_semi != saved.osc2_semi) {
        fprintf(stderr, "ERRORE: il preset tuo non torna dopo la riapertura\n");
        fail = 1;
    }
    app_destroy(a2);
    snprintf(out, sizeof(out), "%s/preset-utente.txt", argv[2]);
    FILE *uf = fopen(out, "r");
    if (!uf) { fprintf(stderr, "ERRORE: preset-utente.txt non scritto\n"); fail = 1; } else fclose(uf);

    /* 3e. Tastiera MIDI (MPK Mini): note, pad, manopola del taglio, sustain, pitch bend */
    {
        app_set_screen(a, SCREEN_PLAY);
        app_load_preset(a, 3);                                         /* Pizzico */
        app_midi_status(a, "MPKminiIV");
        uint32_t kick0 = synth_drum_flash(s, DRUM_KICK), clap0 = synth_drum_flash(s, DRUM_CLAP);
        const unsigned char on[3] = { 0x90, 60, 110 }, off[3] = { 0x80, 60, 0 };
        const unsigned char pad1[3] = { 0x99, 36, 120 }, pad5[3] = { 0x99, 40, 90 };
        const unsigned char knob[3] = { 0xB0, 70, 127 }, sus_on[3] = { 0xB0, 64, 127 }, sus_off[3] = { 0xB0, 64, 0 };
        const unsigned char bend_up[3] = { 0xE0, 0x7F, 0x7F };
        app_midi(a, knob, 3);
        float cut = synth_patch(s)->cutoff;
        app_midi(a, on, 3);
        app_midi(a, pad1, 3);
        app_midi(a, pad5, 3);
        run(a, s, 0.3f, wav, &pos, cap);
        int voices_on = synth_active_voices(s);
        app_midi(a, sus_on, 3);
        app_midi(a, off, 3);                                           /* tenuta dal sustain */
        run(a, s, 0.3f, wav, &pos, cap);
        int voices_sus = synth_active_voices(s);
        app_midi(a, bend_up, 3);
        run(a, s, 0.1f, wav, &pos, cap);
        app_midi(a, sus_off, 3);                                       /* ora si rilascia */
        run(a, s, 1.5f, wav, &pos, cap);
        int voices_after = synth_active_voices(s);
        printf("MIDI: taglio %.2f, voci con la nota %d, tenuta dal sustain %d, dopo il rilascio %d, cassa %u, battimani %u\n",
               cut, voices_on, voices_sus, voices_after, synth_drum_flash(s, DRUM_KICK) - kick0, synth_drum_flash(s, DRUM_CLAP) - clap0);
        if (cut < 0.99f || voices_on < 1 || voices_sus < 1 || voices_after != 0 ||
            synth_drum_flash(s, DRUM_KICK) == kick0 || synth_drum_flash(s, DRUM_CLAP) == clap0) {
            fprintf(stderr, "ERRORE: MIDI non gestito come previsto\n");
            fail = 1;
        }
        app_draw(a, &c);
        snprintf(out, sizeof(out), "%s/7-midi.bmp", argv[2]);
        write_bmp(out, &c);
        app_midi_status(a, NULL);
    }

    /* 4. Richiesta di uscita */
    app_button(a, PAD_MENU, 1); app_button(a, PAD_MENU, 0);
    app_draw(a, &c);
    snprintf(out, sizeof(out), "%s/4-uscita.bmp", argv[2]);
    write_bmp(out, &c);
    app_button(a, PAD_A, 1); app_button(a, PAD_A, 0);
    if (!app_wants_quit(a)) { fprintf(stderr, "ERRORE: A nella finestra di uscita non esce\n"); fail = 1; }

    snprintf(out, sizeof(out), "%s/demo.wav", argv[2]);
    write_wav(out, wav, pos);

    /* statistiche dell'audio */
    double sum2 = 0;
    float peak = 0;
    int nans = 0, clip = 0;
    for (int i = 0; i < pos * 2; i++) {
        float v = wav[i];
        if (v != v) { nans++; continue; }
        if (fabsf(v) > peak) peak = fabsf(v);
        if (fabsf(v) > 0.99f) clip++;
        sum2 += (double)v * v;
    }
    printf("audio: %.1f s, picco %.3f, RMS %.3f, campioni NaN %d, sopra 0.99: %d (%.3f%%)\n",
           (double)pos / SR, peak, sqrt(sum2 / (pos * 2)), nans, clip, 100.0 * clip / (pos * 2));
    if (nans || peak < 0.05f) { fprintf(stderr, "ERRORE: audio assente o non valido\n"); fail = 1; }

    /* prestazioni: 8 voci con filtro, riverbero e delay, 10 secondi */
    app_load_preset(a, 5);
    for (int n = 0; n < 8; n++) synth_note_on(s, 48 + n * 3, 0.8f);
    float buf[2 * 512];
    clock_t t0 = clock();
    for (int i = 0; i < SR * 10 / 512; i++) synth_render(s, buf, 512);
    double secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
    printf("prestazioni: 10 s di audio con 8 voci in %.3f s (%.0fx il tempo reale), voci attive %d\n",
           secs, 10.0 / secs, synth_active_voices(s));

    /* disegno: tempo medio per fotogramma */
    app_set_screen(a, SCREEN_SEQ);
    t0 = clock();
    for (int i = 0; i < 60; i++) app_draw(a, &c);
    printf("disegno: %.2f ms per fotogramma\n", 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC / 60);

    app_destroy(a);
    synth_destroy(s);
    gfx_free_fonts();
    free(px);
    free(wav);
    return fail;
}
