/* Cirmolo - prove senza rete della scheda del gioco: dati da PyUI, percorso della scheda, prompt,
 * scrittura in diretta, salvataggio e rilettura, errori, schermate (BMP).
 *
 * Uso: scheda-test <cartella dei font> <cartella di uscita> <cartella lang>
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx.h"
#include "i18n.h"
#include "platform.h"
#include "scheda_app.h"

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

static int checks, fails;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("ERRORE riga %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* sul PC niente SDL_image: una copertina finta con una sfumatura */
int cirmolo_load_image(const char *path, int maxw, int maxh, uint32_t **px, int *w, int *h)
{
    (void)path;
    int ow = maxw < 160 ? maxw : 160, oh = maxh < 220 ? maxh : 220;
    uint32_t *o = malloc(sizeof(uint32_t) * (size_t)ow * (size_t)oh);
    for (int y = 0; y < oh; y++)
        for (int x = 0; x < ow; x++) o[y * ow + x] = RGB(40 + x / 2, 60 + y / 3, 140);
    *px = o;
    *w = ow;
    *h = oh;
    return 0;
}

static void write_bmp(const char *path, const Canvas *c)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
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

static void write_text(const char *path, const char *s)
{
    FILE *f = fopen(path, "wb");
    if (f) { fputs(s, f); fclose(f); }
}

#define OA(t) "data: {\"choices\":[{\"index\":0,\"delta\":{\"content\":\"" t "\"}}],\"model\":\"glm-4.6v-flash\"}\n\n"

int main(int argc, char **argv)
{
    const char *fonts = argc > 1 ? argv[1] : ".", *out = argc > 2 ? argv[2] : ".", *lang = argc > 3 ? argv[3] : "lang";
    char f1[512], f2[512], path[700], saves[600];
    snprintf(f1, sizeof(f1), "%s/BeVietnamPro-Regular.ttf", fonts);
    snprintf(f2, sizeof(f2), "%s/BeVietnamPro-SemiBold.ttf", fonts);
    if (gfx_load_fonts(f1, f2)) return 1;
    i18n_set(lang, "Italian");

    /* dati da PyUI */
    snprintf(path, sizeof(path), "%s/richiesta.json", out);
    write_text(path, "{\"rom\":\"/mnt/SDCARD/Roms/SFC/Chrono Trigger (USA).sfc\",\"system\":\"SFC\",\"system_name\":\"Super Nintendo\","
                     "\"name\":\"Chrono Trigger\",\"image\":\"/mnt/SDCARD/Roms/SFC/Imgs/Chrono Trigger (USA).png\"}");
    GameInfo g;
    CHECK(scheda_read_request(path, &g) == 0 && !strcmp(g.name, "Chrono Trigger") && !strcmp(g.system_name, "Super Nintendo"), "richiesta letta");
    write_text(path, "{\"rom\":\"/mnt/SDCARD/Roms/GBA/Golden Sun (Europe).gba\",\"system\":\"GBA\"}");
    GameInfo g2;
    CHECK(scheda_read_request(path, &g2) == 0 && !strcmp(g2.name, "Golden Sun (Europe)") && !strcmp(g2.system_name, "GBA"), "nome dal file: %s", g2.name);
    CHECK(scheda_read_request("non-esiste.json", &g2) != 0, "richiesta mancante");
    char card[700];
    scheda_card_path("/S", &g, card, sizeof(card));
    CHECK(!strcmp(card, "/S/schede/SFC/Chrono Trigger (USA).md"), "percorso: %s", card);
    char *sp = scheda_system_prompt("Italian"), *up = scheda_user_prompt(&g);
    CHECK(strstr(sp, "Write in Italian") && strstr(sp, "Never invent facts") && strstr(sp, "Tips to get started"), "istruzioni");
    CHECK(strstr(up, "Game: Chrono Trigger\nSystem: Super Nintendo\nFile: Chrono Trigger (USA).sfc"), "domanda: %s", up);
    free(sp);
    free(up);

    /* la prima volta: si scrive in diretta e si salva */
    snprintf(saves, sizeof(saves), "%s/saves-scheda", out);
    MKDIR(saves);
    snprintf(path, sizeof(path), "%s/chiavi", saves);
    MKDIR(path);
    snprintf(path, sizeof(path), "%s/chiavi/zai.txt", saves);
    write_text(path, "provaprova0123456789.abc");
    snprintf(path, sizeof(path), "%s/impostazioni.txt", saves);
    write_text(path, "provider=anthropic\nia.provider=zai\nia.model=glm-4.6v-flash\n");
    scheda_card_path(saves, &g, card, sizeof(card));
    remove(card);
    uint32_t *px = malloc(sizeof(uint32_t) * 640 * 480);
    Canvas cv = { px, 640, 480 };
#define SHOT(s, name) do { scheda_update(s, 0.016f); scheda_draw(s, &cv); snprintf(path, sizeof(path), "%s/%s.bmp", out, name); write_bmp(path, &cv); } while (0)
    Scheda *s = scheda_create(48000.0f, saves, &g);
    scheda_set_offline(s, 1);
    for (int i = 0; i < 20; i++) scheda_update(s, 0.016f);
    CHECK(scheda_busy(s), "la scheda si scrive da sola alla prima apertura (%s)", scheda_error(s));
    SHOT(s, "scheda-1-attesa");
    scheda_feed(s, OA("## In breve\\n1995, Square, gioco di ruolo, 1 giocatore.\\n\\n## Di cosa parla\\nCrono, un ragazzo di Truce, ") , 0);
    SHOT(s, "scheda-2-scrive");
    scheda_feed(s, OA("finisce per sbaglio nel passato insieme a Marle e deve salvare il futuro.\\n\\n## Come si gioca\\nSi esplora "
                      "la mappa e si combatte a turni attivi: le tecniche doppie e triple uniscono i personaggi.\\n\\n"
                      "## Consigli per iniziare\\n- Parla con tutti alla fiera di Leene.\\n- Salva spesso.\\n- Prova le tecniche combinate.\\n\\n"
                      "## Curiosità\\n- Ha più finali diversi.\\n- La musica è di Yasunori Mitsuda.")
                    "data: [DONE]\n\n", 1);
    CHECK(!scheda_busy(s) && strstr(scheda_text(s), "## Come si gioca") && strstr(scheda_text(s), "Scheda scritta da GLM-4.6V Flash"), "scheda finita: %s", scheda_error(s));
    SHOT(s, "scheda-3-fatta");
    for (int i = 0; i < 10; i++) { scheda_button(s, PAD_DOWN, 1); scheda_update(s, 0.016f); scheda_button(s, PAD_DOWN, 0); }
    SHOT(s, "scheda-4-fondo");
    scheda_button(s, PAD_B, 1);
    CHECK(scheda_wants_quit(s), "B torna ai giochi");
    scheda_destroy(s);
    FILE *f = fopen(card, "rb");
    CHECK(f != NULL, "scheda salvata in %s", card);
    if (f) fclose(f);

    /* la volta dopo: si legge dal file, senza richieste */
    s = scheda_create(48000.0f, saves, &g);
    scheda_set_offline(s, 1);
    for (int i = 0; i < 20; i++) scheda_update(s, 0.016f);
    CHECK(!scheda_busy(s) && strstr(scheda_text(s), "Yasunori Mitsuda"), "scheda riletta");
    /* Y la riscrive; un errore del fornitore si mostra */
    scheda_button(s, PAD_Y, 1);
    scheda_update(s, 0.016f);
    CHECK(scheda_busy(s), "Y riscrive");
    scheda_feed(s, "{\"error\":{\"message\":\"Rate limit\",\"code\":\"1302\"}}\n\n@@CIRMOLO_HTTP 429\n", 1);
    CHECK(!scheda_busy(s) && strstr(scheda_error(s), "Limite"), "errore: %s", scheda_error(s));
    SHOT(s, "scheda-5-errore");
    scheda_destroy(s);

    /* senza chiave: messaggio chiaro e niente richiesta */
    remove(path);
    snprintf(path, sizeof(path), "%s/chiavi/zai.txt", saves);
    remove(path);
    remove(card);
    s = scheda_create(48000.0f, saves, &g);
    scheda_set_offline(s, 1);
    for (int i = 0; i < 20; i++) scheda_update(s, 0.016f);
    CHECK(!scheda_busy(s) && strstr(scheda_error(s), "chiave") , "senza chiave: %s", scheda_error(s));
    SHOT(s, "scheda-6-senza-chiave");
    scheda_destroy(s);

    /* in inglese */
    i18n_set(lang, "English");
    snprintf(path, sizeof(path), "%s/chiavi/zai.txt", saves);
    write_text(path, "provaprova0123456789.abc");
    s = scheda_create(48000.0f, saves, &g);
    scheda_set_offline(s, 1);
    for (int i = 0; i < 20; i++) scheda_update(s, 0.016f);
    SHOT(s, "scheda-7-inglese");
    scheda_destroy(s);

    /* diario: due partite dello stesso gioco, il riquadro in cima alla scheda e il promemoria */
    i18n_set(lang, "Italian");
    snprintf(path, sizeof(path), "%s/diario", saves);
    MKDIR(path);
    snprintf(path, sizeof(path), "%s/diario/img", saves);
    MKDIR(path);
    snprintf(path, sizeof(path), "%s/diario/diario.jsonl", saves);
    write_text(path,
        "{\"rom\":\"/mnt/SDCARD/Roms/SFC/Chrono Trigger (USA).sfc\",\"system\":\"SFC\",\"name\":\"Chrono Trigger (USA)\",\"start\":1760000000,\"secs\":1500,\"img\":\"\",\"summary\":\"\"}\n"
        "{\"rom\":\"/mnt/SDCARD/Roms/GBA/Altro.gba\",\"system\":\"GBA\",\"name\":\"Altro\",\"start\":1760001000,\"secs\":60,\"img\":\"\",\"summary\":\"\"}\n"
        "{\"rom\":\"/mnt/SDCARD/Roms/SFC/Chrono Trigger (USA).sfc\",\"system\":\"SFC\",\"name\":\"Chrono Trigger (USA)\",\"start\":1760086400,\"secs\":4200,"
        "\"img\":\"img/1760086400.png\",\"summary\":\"Eri nella foresta di Guardia con Lucca, appena dopo il salvataggio vicino all'uscita nord.\"}\n");
    snprintf(path, sizeof(path), "%s/diario/img/1760086400.png", saves);
    write_text(path, "finta");                   /* l'immagine la "carica" la copertina finta qui sopra */
    write_text(card, "## In breve\n1995, Square, gioco di ruolo.\n\n## Di cosa parla\nViaggi nel tempo per salvare il futuro.\n");
    s = scheda_create(48000.0f, saves, &g);
    scheda_set_offline(s, 1);
    for (int i = 0; i < 20; i++) scheda_update(s, 0.016f);
    CHECK(scheda_diary_sessions(s) == 2, "partite nel diario: %d", scheda_diary_sessions(s));
    {
        GameInfo low = g;                        /* /mnt/sdcard (standard_launch.sh) = /mnt/SDCARD (PyUI) */
        memcpy(low.rom, "/mnt/sdcard", 11);
        Scheda *t = scheda_create(48000.0f, saves, &low);
        CHECK(scheda_diary_sessions(t) == 2, "percorso in minuscolo: %d partite", scheda_diary_sessions(t));
        scheda_destroy(t);
    }
    SHOT(s, "scheda-8-diario");
    scheda_destroy(s);
    GameInfo r = g;
    snprintf(r.mode, sizeof(r.mode), "promemoria");
    s = scheda_create(48000.0f, saves, &r);
    scheda_set_offline(s, 1);
    for (int i = 0; i < 60; i++) scheda_update(s, 0.016f);
    CHECK(!scheda_busy(s) && !scheda_wants_quit(s), "promemoria aperto, senza richieste al modello");
    SHOT(s, "scheda-9-promemoria");
    for (int i = 0; i < 500; i++) scheda_update(s, 0.016f);
    CHECK(scheda_wants_quit(s), "il promemoria si chiude da solo");
    scheda_destroy(s);
    s = scheda_create(48000.0f, saves, &r);
    scheda_update(s, 0.016f);
    scheda_button(s, PAD_START, 1);
    CHECK(scheda_wants_quit(s), "un tasto chiude il promemoria");
    scheda_destroy(s);
    GameInfo nuovo = g;
    snprintf(nuovo.rom, sizeof(nuovo.rom), "/mnt/SDCARD/Roms/SFC/Mai giocato.sfc");
    snprintf(nuovo.mode, sizeof(nuovo.mode), "promemoria");
    s = scheda_create(48000.0f, saves, &nuovo);
    scheda_update(s, 0.016f);
    CHECK(scheda_wants_quit(s), "gioco mai giocato: niente promemoria");
    scheda_destroy(s);

    free(px);
    gfx_free_fonts();
    printf("TOTALE: %d controlli, %d errori\n", checks, fails);
    return fails ? 1 : 0;
}
