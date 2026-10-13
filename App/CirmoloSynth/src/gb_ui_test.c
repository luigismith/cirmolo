/* Cirmolo Synth - prove dell'interfaccia senza finestra: tasti su tutte le pagine, salvataggio e ripresa,
 * MIDI, esportazione in WAV e una schermata BMP per pagina (per guardarle).
 *   gb-ui-test <cartella dei font> <cartella di uscita> */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gb_app.h"
#include "gb_dsp.h"
#include "gfx.h"
#include "midi.h"

/* Quel che l'app manda al dispositivo MIDI finto. */
static unsigned char sent[8192];
static int sent_n;
static int fake_send(int port, const unsigned char *m, int len)
{
    for (int i = 0; i < len && sent_n < (int)sizeof(sent); i++) sent[sent_n++] = m[i];
    (void)port;
    return 0;
}

static int checks, fails;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("ERRORE riga %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

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

static void press(App *a, int b) { app_button(a, b, 1); app_button(a, b, 0); }

/* Fa passare del tempo: audio e interfaccia insieme, come sulla console. */
static void run(Gb *g, App *a, float seconds)
{
    static float buf[512 * 2];
    int blocks = (int)(seconds * GB_RATE / 512);
    for (int i = 0; i < blocks; i++) {
        gb_render(g, buf, 512);
        if (i % 8 == 0) app_update(a, 512 * 8.0f / GB_RATE);
    }
}

static void shot(App *a, Canvas *c, const char *out, const char *name)
{
    app_draw(a, c);
    char p[600];
    snprintf(p, sizeof(p), "%s/%s.bmp", out, name);
    write_bmp(p, c);
}

static long file_size(const char *p)
{
    FILE *f = fopen(p, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fclose(f);
    return n;
}

int main(int argc, char **argv)
{
    const char *fonts = argc > 1 ? argv[1] : ".", *out = argc > 2 ? argv[2] : ".";
    char f1[512], f2[512], state[600];
    snprintf(f1, sizeof(f1), "%s/BeVietnamPro-Regular.ttf", fonts);
    snprintf(f2, sizeof(f2), "%s/BeVietnamPro-SemiBold.ttf", fonts);
    if (gfx_load_fonts(f1, f2)) { printf("font non trovati in %s\n", fonts); return 1; }
    snprintf(state, sizeof(state), "%s/stato.txt", out);
    remove(state);
    Canvas c = { calloc(640 * 480, 4), 640, 480 };

    Gb *g = gb_create(GB_RATE);
    App *a = app_create(g, state);
    CHECK(app_screen(a) == SCR_RITMO, "pagina iniziale %d", app_screen(a));
    GbPattern *p0 = gb_pattern(g, 0);
    int hits = 0;
    for (int t = 0; t < GB_DRUMS; t++) for (int i = 0; i < 16; i++) hits += p0->track[t].step[i].on;
    CHECK(hits > 4, "primo avvio: il pattern 1 ha un ritmo (%d colpi)", hits);
    CHECK(p0->track[GB_ACID].step[0].on, "primo avvio: c'e' la linea di acido");

    /* Ritmo: accende un passo vuoto, lo modifica con B + croce, apre la scheda */
    app_set_cursor(a, 1, 1);
    GbStep *s = &p0->track[1].step[1];
    int was = s->on;
    press(a, PAD_A);
    CHECK(s->on == !was, "A accende/spegne il passo");
    if (!s->on) press(a, PAD_A);
    int vel = s->vel;
    app_button(a, PAD_B, 1);
    press(a, PAD_DOWN);
    press(a, PAD_RIGHT);
    press(a, PAD_RIGHT);
    app_button(a, PAD_B, 0);
    CHECK(s->vel < vel, "B + giu' abbassa la dinamica (%d -> %d)", vel, s->vel);
    CHECK(s->ratchet == 3, "B + destra due volte: 3 ripetizioni (%d)", s->ratchet);
    app_set_cursor(a, 4, GB_ACID);
    GbStep *ac = &p0->track[GB_ACID].step[4];
    if (!ac->on) press(a, PAD_A);
    int note = ac->note;
    app_button(a, PAD_B, 1);
    press(a, PAD_UP);
    app_button(a, PAD_B, 0);
    CHECK(ac->note == note + 1, "B + su alza la nota dell'acido (%d -> %d)", note, ac->note);
    app_set_cursor(a, 2, 1);
    press(a, PAD_X);
    press(a, PAD_DOWN);                       /* probabilita' (dopo dinamica) */
    press(a, PAD_LEFT);
    press(a, PAD_LEFT);
    CHECK(p0->track[1].step[2].on && p0->track[1].step[2].prob == 90, "scheda: probabilita' 90 (%d)", p0->track[1].step[2].prob);
    press(a, PAD_DOWN);                       /* condizione */
    press(a, PAD_RIGHT);
    CHECK(p0->track[1].step[2].cond == COND_FILL, "scheda: condizione fill (%d)", p0->track[1].step[2].cond);
    run(g, a, 0.2f);
    shot(a, &c, out, "ritmo-scheda");
    press(a, PAD_B);
    /* euclideo sulla traccia 11 */
    app_set_cursor(a, 0, 10);
    press(a, PAD_X);
    for (int i = 0; i < 30; i++) press(a, PAD_DOWN);   /* gira la lista fino a "Applica" */
    app_open_editor(a, 0);
    gb_euclid(&p0->track[10], 8, 3, 0, 100);
    CHECK(p0->track[10].step[0].on && !p0->track[10].step[1].on && p0->track[10].step[3].on, "euclideo 3 su 8");

    gb_play(g, 1);
    run(g, a, 1.0f);
    CHECK(gb_playing(g), "START: suona");
    shot(a, &c, out, "ritmo");
    press(a, PAD_R1);                        /* pagina 2 (se ce n'e' una sola resta sulla prima) */
    press(a, PAD_L1);

    /* Suoni */
    app_set_screen(a, SCR_SUONI);
    int kit = 0;
    press(a, PAD_R2);
    run(g, a, 0.3f);
    GbDrum *d0 = gb_drum(g, 0);
    float tune = d0->tune;
    app_set_cursor(a, 0, 0);
    press(a, PAD_DOWN);                      /* Altezza */
    press(a, PAD_RIGHT);
    CHECK(fabsf(d0->tune - (tune + 1)) < 0.01f, "Suoni: altezza +1 (%.1f -> %.1f)", tune, d0->tune);
    shot(a, &c, out, "suoni");
    press(a, PAD_L1);                        /* traccia prima della cassa: Piega */
    run(g, a, 0.2f);
    shot(a, &c, out, "suoni-piega");
    press(a, PAD_L1);
    shot(a, &c, out, "suoni-acido");
    (void)kit;

    /* Mixer */
    app_set_screen(a, SCR_MIXER);
    press(a, PAD_RIGHT);
    float lev = gb_mix(g, 1)->level;
    press(a, PAD_DOWN);
    CHECK(gb_mix(g, 1)->level < lev, "Mixer: giu' abbassa il volume");
    press(a, PAD_A);
    CHECK(gb_mix(g, 1)->mute, "Mixer: A zittisce");
    run(g, a, 0.5f);
    shot(a, &c, out, "mixer");
    press(a, PAD_A);
    for (int i = 0; i < 20; i++) press(a, PAD_RIGHT);   /* fino al master */
    press(a, PAD_UP);
    shot(a, &c, out, "mixer-master");

    /* Live */
    app_set_screen(a, SCR_LIVE);
    uint32_t f0 = gb_track_flash(g, 0);
    app_button(a, PAD_LEFT, 1);
    run(g, a, 0.05f);
    shot(a, &c, out, "live");
    app_button(a, PAD_LEFT, 0);
    CHECK(gb_track_flash(g, 0) != f0, "Live: sinistra suona la traccia 1");
    app_axes(a, -0.6f, 0, 0, -0.7f, 0, 0);
    run(g, a, 0.3f);
    CHECK(gb_master(g)->filter < -0.2f, "levetta sinistra: filtro DJ (%.2f)", gb_master(g)->filter);
    shot(a, &c, out, "live-levette");
    app_axes(a, 0, 0, 0, 0, 0, 0);
    CHECK(fabsf(gb_master(g)->filter) < 0.3f, "levetta al centro: filtro come nel mixer");
    press(a, PAD_L3);
    press(a, PAD_L3);
    shot(a, &c, out, "live-zittisci");
    press(a, PAD_L3);

    /* Pattern: catena, copia, ritmi pronti */
    app_set_screen(a, SCR_PATTERN);
    press(a, PAD_X);                          /* il pattern 1 in catena */
    press(a, PAD_Y);                          /* copia l'1 ... */
    press(a, PAD_RIGHT);
    press(a, PAD_Y);                          /* ... e lo incolla nel 2 */
    CHECK(!memcmp(gb_pattern(g, 1)->track, gb_pattern(g, 0)->track, sizeof(gb_pattern(g, 0)->track)), "copia del pattern 1 nel 2");
    press(a, PAD_X);
    CHECK(gb_chain_position(g) >= -1, "catena impostata");
    for (int i = 0; i < 4; i++) press(a, PAD_RIGHT);    /* lista dei ritmi pronti */
    press(a, PAD_DOWN);
    press(a, PAD_DOWN);
    run(g, a, 0.5f);
    shot(a, &c, out, "pattern");
    press(a, PAD_A);
    run(g, a, 0.3f);

    /* MIDI: pad sul canale 10, tastiera sull'acido */
    uint32_t fl = gb_track_flash(g, 2);
    unsigned char m1[3] = { 0x99, 38, 100 };
    app_midi_port(a, 0, m1, 3);
    run(g, a, 0.05f);
    CHECK(gb_track_flash(g, 2) != fl, "MIDI: nota 38 sul canale 10 = traccia 3");
    unsigned char m2[3] = { 0x90, 48, 100 }, m3[3] = { 0x80, 48, 0 };
    fl = gb_track_flash(g, GB_ACID);
    app_midi_port(a, 0, m2, 3);
    run(g, a, 0.05f);
    app_midi_port(a, 0, m3, 3);
    CHECK(gb_track_flash(g, GB_ACID) != fl, "MIDI: tastiera sull'acido");

    /* MPK mini Plus: clock MIDI in uscita, joystick, pad 13-16 */
    cirmolo_midi_set_device("MPK mini Plus", 2, fake_send);
    app_midi_status(a, "MPK mini Plus");
    if (gb_playing(g)) press(a, PAD_START);
    sent_n = 0;
    press(a, PAD_START);
    run(g, a, 1.0f);
    int fa = -1, ticks = 0;
    for (int i = 0; i < sent_n; i++) {
        if (sent[i] == 0xFA && fa < 0) fa = i;
        if (sent[i] == 0xF8) { ticks++; CHECK(fa >= 0 && fa < i, "Start prima del primo clock"); if (fa < 0) break; }
    }
    float want = gb_master(g)->bpm / 60.0f * 24.0f;
    CHECK(fa == 0 && fabsf(ticks - want) <= 24, "clock: Start e %d tick in 1 s (attesi ~%.0f)", ticks, want);
    sent_n = 0;
    press(a, PAD_START);
    CHECK(sent_n >= 1 && sent[sent_n - 1] == 0xFC, "clock: Stop");
    unsigned char pb[3] = { 0xE0, 0, 0 };
    app_midi_port(a, 0, pb, 3);
    CHECK(gb_master(g)->filter < -0.5f, "joystick a sinistra: filtro DJ sui bassi (%.2f)", gb_master(g)->filter);
    pb[2] = 0x40;
    app_midi_port(a, 0, pb, 3);
    CHECK(fabsf(gb_master(g)->filter) < 0.3f, "joystick al centro");
    uint32_t f7 = gb_track_flash(g, 7);
    unsigned char pad13[3] = { 0x99, 48, 100 }, pad13off[3] = { 0x89, 48, 0 };
    app_midi_port(a, 0, pad13, 3);
    run(g, a, 0.05f);
    CHECK(gb_track_flash(g, 7) == f7, "MPK mini Plus: il pad 13 fa il fill, non suona un tom");
    app_midi_port(a, 0, pad13off, 3);
    app_set_screen(a, SCR_MIXER);
    app_set_mixer(a, GB_TRACKS, 14);          /* master, Clock MIDI in uscita */
    sent_n = 0;
    press(a, PAD_UP);                         /* clock spento */
    press(a, PAD_START);
    run(g, a, 0.3f);
    CHECK(sent_n == 0, "clock spento: niente al dispositivo (%d byte)", sent_n);
    shot(a, &c, out, "mixer-clock");
    press(a, PAD_UP);
    press(a, PAD_START);
    cirmolo_midi_set_device(NULL, 0, NULL);
    app_midi_status(a, NULL);
    app_set_screen(a, SCR_PATTERN);

    /* esportazione */
    gb_play(g, 0);
    char wav[600];
    snprintf(wav, sizeof(wav), "%s/esportazione.wav", out);
    int er = app_export_wav(a, wav, 1);
    long sz = file_size(wav);
    CHECK(er == 0 && sz > 44 + GB_RATE * 4, "esportazione WAV (%ld byte)", sz);

    /* salvataggio e ripresa */
    GbStep before = gb_pattern(g, 0)->track[1].step[2];
    float bpm = gb_master(g)->bpm;
    app_destroy(a);
    gb_destroy(g);
    g = gb_create(GB_RATE);
    a = app_create(g, state);
    GbStep after = gb_pattern(g, 0)->track[1].step[2];
    CHECK(!memcmp(&before, &after, sizeof(before)), "ripresa: il passo con probabilita' e condizione e' uguale");
    CHECK(fabsf(gb_master(g)->bpm - bpm) < 0.01f, "ripresa: tempo");
    CHECK(gb_mix(g, 1)->level < 0.8f, "ripresa: mixer");
    CHECK(app_screen(a) == SCR_PATTERN, "ripresa: pagina");
    app_set_screen(a, SCR_RITMO);
    shot(a, &c, out, "ripresa");
    press(a, PAD_MENU);
    shot(a, &c, out, "esci");
    press(a, PAD_A);
    CHECK(app_wants_quit(a), "MENU + A esce");
    app_destroy(a);
    gb_destroy(g);
    free(c.px);
    gfx_free_fonts();
    printf("TOTALE: %d controlli, %d errori\n", checks, fails);
    printf("ESITO: %s\n", fails ? "ci sono errori" : "tutto bene");
    return fails ? 1 : 0;
}
