/* Cirmolo - prove senza rete di "Cosa gioco?": lettura della collezione da una SD finta (edizioni,
 * gamelist, sottocartelle), catalogo, ricerca dei titoli scritti dal modello, comando di avvio, prompt,
 * diario, interfaccia e avvio del gioco scelto, schermate (BMP).
 *
 * Uso: consigli-test <cartella dei font> <cartella di uscita> <cartella lang>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "collezione.h"
#include "consigli_app.h"
#include "diario.h"
#include "gfx.h"
#include "i18n.h"
#include "platform.h"

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

static int checks, fails;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("ERRORE riga %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int cirmolo_load_image(const char *path, int maxw, int maxh, uint32_t **px, int *w, int *h)
{
    (void)path;
    int ow = maxw, oh = maxh * 3 / 4;
    uint32_t *o = malloc(sizeof(uint32_t) * (size_t)ow * (size_t)oh);
    for (int i = 0; i < ow * oh; i++) o[i] = RGB(60 + i % ow, 80, 150);
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

static void put(const char *root, const char *rel, const char *text)
{
    char p[800];
    snprintf(p, sizeof(p), "%s/%s", root, rel);
    FILE *f = fopen(p, "wb");
    if (f) { fputs(text, f); fclose(f); }
}

static void dir(const char *root, const char *rel)
{
    char p[800];
    snprintf(p, sizeof(p), "%s/%s", root, rel);
    MKDIR(p);
}

#define OA(t) "data: {\"choices\":[{\"index\":0,\"delta\":{\"content\":\"" t "\"}}]}\n\n"

int main(int argc, char **argv)
{
    const char *fonts = argc > 1 ? argv[1] : ".", *out = argc > 2 ? argv[2] : ".", *lang = argc > 3 ? argv[3] : "lang";
    char f1[512], f2[512], root[600], saves[700], cmd[700], path[800];
    snprintf(f1, sizeof(f1), "%s/BeVietnamPro-Regular.ttf", fonts);
    snprintf(f2, sizeof(f2), "%s/BeVietnamPro-SemiBold.ttf", fonts);
    if (gfx_load_fonts(f1, f2)) return 1;
    i18n_set(lang, "Italian");

    /* una SD finta */
    snprintf(root, sizeof(root), "%s/sd-finta", out);
    MKDIR(root);
    const char *dirs[] = { "Emu", "Emu/SFC", "Emu/ARCADE", "Emu/VUOTO", "Emu/SENZAROMS", "Roms", "Roms/SFC", "Roms/SFC/Imgs",
                           "Roms/ARCADE", "Roms/ARCADE/CPS1+2", "Roms/VUOTO", NULL };
    for (int i = 0; dirs[i]; i++) dir(root, dirs[i]);
    put(root, "Emu/SFC/config.json", "{\"label\":\"SNES\",\"launch\":\"../../spruce/scripts/emu/standard_launch.sh\",\"extlist\":\"zip|smc|sfc\"}");
    put(root, "Emu/ARCADE/config.json", "{\"label\":\"Arcade\",\"launch\":\"launch.sh\",\"extlist\":\"zip\"}");
    put(root, "Emu/VUOTO/config.json", "{\"label\":\"Vuoto\",\"launch\":\"launch.sh\",\"extlist\":\"bin\"}");
    put(root, "Emu/SENZAROMS/config.json", "{\"label\":\"X\",\"launch\":\"launch.sh\"}");
    put(root, "Roms/SFC/Chrono Trigger (USA).sfc", "x");
    put(root, "Roms/SFC/Chrono Trigger (Japan).sfc", "x");
    put(root, "Roms/SFC/Super Mario World (Europe) (En,Fr,De,Es,It).smc", "x");
    put(root, "Roms/SFC/Super Mario World (USA).smc", "x");
    put(root, "Roms/SFC/Zelda $pecial `test`.sfc", "x");   /* le virgolette su Windows non si possono */
    put(root, "Roms/SFC/note.txt", "x");
    put(root, "Roms/SFC/Imgs/Chrono Trigger (USA).png", "x");
    put(root, "Roms/ARCADE/1941.zip", "x");
    put(root, "Roms/ARCADE/CPS1+2/sf2.zip", "x");
    put(root, "Roms/ARCADE/miyoogamelist.xml", "<gameList><game><name>1941: Counter Attack</name><path>./1941.zip</path></game></gameList>");
    put(root, "Roms/VUOTO/leggimi.txt", "x");

    Collection c;
    collection_scan(&c, root);
    CHECK(c.nsys == 2 && !strcmp(c.sys[0].id, "ARCADE") && !strcmp(c.sys[1].id, "SFC"), "sistemi con giochi: %d", c.nsys);
    CHECK(c.n == 5, "giochi (edizioni unite): %d", c.n);
    int ct = collection_find(&c, "SFC", "Chrono Trigger"), smw = collection_find(&c, "sfc", "super mario world");
    CHECK(ct >= 0 && strstr(c.g[ct].rom, "(USA)"), "Chrono Trigger, edizione USA preferita alla giapponese: %s", ct >= 0 ? c.g[ct].rom : "-");
    CHECK(smw >= 0 && strstr(c.g[smw].rom, "(En,Fr,De,Es,It)"), "Super Mario World, edizione con l'italiano: %s", smw >= 0 ? c.g[smw].rom : "-");
    CHECK(collection_find(&c, "ARCADE", "1941: Counter Attack") >= 0, "nome dal gamelist");
    CHECK(collection_find(&c, "ARCADE", "sf2") >= 0, "sottocartella CPS1+2");
    CHECK(collection_find(&c, "XYZ", "Chrono Trigger") == ct, "sistema sbagliato: si trova lo stesso");
    CHECK(collection_find(&c, "SFC", "Final Fantasy VI") < 0, "gioco che non c'e'");
    char *cat = collection_catalog(&c);
    CHECK(strstr(cat, "## SFC (SNES)\nChrono Trigger | Super Mario World | Zelda $pecial `test`") && strstr(cat, "## ARCADE (Arcade)\n1941: Counter Attack | sf2"),
          "catalogo:\n%s", cat);
    free(cat);
    int z = collection_find(&c, "SFC", "Zelda $pecial `test`");
    char *lc = z >= 0 ? collection_launch_cmd(&c, z) : NULL;
    CHECK(lc && strstr(lc, "chmod a+x \"") && strstr(lc, "/Emu/SFC/../../spruce/scripts/emu/standard_launch.sh\";\"") &&
          strstr(lc, "Zelda \\$pecial \\`test\\`.sfc\"\n"), "comando di avvio: %s", lc ? lc : "-");
    free(lc);
    char t[200];
    collection_clean_title("Pokemon_-_Edizione_Rossa_(Italy)_[!].gb", t, sizeof(t));
    CHECK(!strcmp(t, "Pokemon - Edizione Rossa"), "titolo pulito: %s", t);

    int games[5];
    char why[5][300];
    const char *rep = "Ecco: ```json\n{\"picks\":[{\"system\":\"SFC\",\"title\":\"Chrono Trigger\",\"why\":\"Una storia.\"},"
                      "{\"system\":\"SFC\",\"title\":\"Final Fantasy VI\",\"why\":\"no\"},{\"system\":\"ARCADE\",\"title\":\"1941\",\"why\":\"Veloce.\"},"
                      "{\"system\":\"SFC\",\"title\":\"Chrono Trigger\",\"why\":\"doppione\"}]}\n```";
    int n = consigli_parse(&c, rep, games, why, 5);
    CHECK(n == 2 && games[0] == ct && !strcmp(why[0], "Una storia.") && !strcmp(why[1], "Veloce."), "scelte riconosciute: %d", n);
    CHECK(consigli_parse(&c, "non ho capito", games, why, 5) == 0, "risposta senza JSON");
    char *sp = consigli_system_prompt("Italian", "## SFC (SNES)\nChrono Trigger\n", NULL);
    char *up = consigli_user_prompt(1, 2, 2, "Tetris");
    CHECK(strstr(sp, "in Italian") && strstr(sp, "COLLECTION") && strstr(sp, "none yet"), "istruzioni");
    CHECK(strstr(up, "about half an hour") && strstr(up, "a good story") && strstr(up, "only games from the play diary") && strstr(up, "Tetris"), "domanda: %s", up);
    free(sp);
    free(up);
    collection_free(&c);

    /* diario per i consigli */
    snprintf(saves, sizeof(saves), "%s/saves-consigli", out);
    MKDIR(saves);
    snprintf(path, sizeof(path), "%s/diario", saves);
    MKDIR(path);
    snprintf(path, sizeof(path), "%s/diario/diario.jsonl", saves);
    remove(path);
    DiaryEntry e = { "/mnt/SDCARD/Roms/SFC/Chrono Trigger (USA).sfc", "SFC", "Chrono Trigger (USA)", "", "Eri a Guardia.", 1760000000, 3600 };
    diary_append(saves, &e);
    e.start += 86400;
    e.secs = 600;
    snprintf(e.summary, sizeof(e.summary), "Eri a Truce.");
    diary_append(saves, &e);
    DiaryEntry e2 = { "/mnt/sdcard/Roms/GB/Tetris.gb", "GB", "Tetris", "", "", 1760050000, 300 };
    diary_append(saves, &e2);
    char *dg = diary_digest(saves, 10);
    CHECK(dg && strstr(dg, "SFC | Chrono Trigger (USA) | 2 sessions, 1 h 10 min in all") && strstr(dg, "| Eri a Truce.") &&
          strstr(dg, "Chrono") < strstr(dg, "Tetris"), "diario per i consigli:\n%s", dg ? dg : "-");
    free(dg);

    /* interfaccia */
    snprintf(path, sizeof(path), "%s/chiavi", saves);
    MKDIR(path);
    put(saves, "chiavi/zai.txt", "provaprova0123456789.abc");
    put(saves, "impostazioni.txt", "ia.provider=zai\nia.model=glm-4.6v-flash\n");
    snprintf(cmd, sizeof(cmd), "%s/ia-gioca.sh", out);
    uint32_t *px = malloc(sizeof(uint32_t) * 640 * 480);
    Canvas cv = { px, 640, 480 };
#define SHOT(name) do { consigli_update(s, 0.016f); consigli_draw(s, &cv); snprintf(path, sizeof(path), "%s/%s.bmp", out, name); write_bmp(path, &cv); } while (0)
#define TAP(b) do { consigli_button(s, b, 1); consigli_update(s, 0.016f); consigli_button(s, b, 0); } while (0)
    Consigli *s = consigli_create(root, saves, cmd);
    consigli_set_offline(s, 1);
    for (int i = 0; i < 10; i++) consigli_update(s, 0.016f);
    CHECK(consigli_collection(s)->n == 5, "collezione letta all'avvio");
    TAP(PAD_RIGHT);
    TAP(PAD_DOWN);
    TAP(PAD_RIGHT); TAP(PAD_RIGHT);
    SHOT("consigli-1-domande");
    TAP(PAD_A);
    CHECK(consigli_busy(s) && consigli_page(s) == 1, "richiesta partita (%s)", consigli_error(s));
    SHOT("consigli-2-cerco");
    consigli_feed(s, OA("{\\\"picks\\\":[{\\\"system\\\":\\\"SFC\\\",\\\"title\\\":\\\"Chrono Trigger\\\",\\\"why\\\":\\\"Hai mezz'ora e voglia di una storia: riprendi da Truce, dove eri rimasto.\\\"},")
                      OA("{\\\"system\\\":\\\"SFC\\\",\\\"title\\\":\\\"Super Mario World\\\",\\\"why\\\":\\\"Livelli brevi, perfetti per una pausa.\\\"},")
                      OA("{\\\"system\\\":\\\"ARCADE\\\",\\\"title\\\":\\\"1941: Counter Attack\\\",\\\"why\\\":\\\"Sparatutto a scorrimento, si gioca anche in due.\\\"}]}")
                      "data: [DONE]\n\n", 1);
    CHECK(!consigli_busy(s) && consigli_count(s) == 3, "tre consigli: %d (%s)", consigli_count(s), consigli_error(s));
    SHOT("consigli-3-risultati");
    TAP(PAD_DOWN);
    TAP(PAD_A);
    CHECK(consigli_wants_quit(s), "A avvia il gioco");
    FILE *f = fopen(cmd, "rb");
    char buf[1000] = "";
    if (f) { size_t got = fread(buf, 1, sizeof(buf) - 1, f); buf[got] = 0; fclose(f); }
    CHECK(strstr(buf, "standard_launch.sh") && strstr(buf, "Super Mario World (Europe) (En,Fr,De,Es,It).smc"), "comando scritto: %s", buf);
    consigli_destroy(s);

    /* "altri consigli" e risposte senza giochi della collezione */
    s = consigli_create(root, saves, cmd);
    consigli_set_offline(s, 1);
    for (int i = 0; i < 10; i++) consigli_update(s, 0.016f);
    TAP(PAD_A);
    consigli_feed(s, OA("{\\\"picks\\\":[{\\\"system\\\":\\\"PS\\\",\\\"title\\\":\\\"Gioco inventato\\\",\\\"why\\\":\\\"x\\\"}]}") "data: [DONE]\n\n", 1);
    CHECK(consigli_count(s) == 0 && strstr(consigli_error(s), "collezione"), "nessun gioco valido: %s", consigli_error(s));
    SHOT("consigli-4-errore");
    TAP(PAD_B);
    CHECK(consigli_page(s) == 0, "B torna alle domande");
    consigli_destroy(s);

    /* in inglese */
    i18n_set(lang, "English");
    s = consigli_create(root, saves, cmd);
    for (int i = 0; i < 10; i++) consigli_update(s, 0.016f);
    SHOT("consigli-5-inglese");
    consigli_destroy(s);

    free(px);
    gfx_free_fonts();
    printf("TOTALE: %d controlli, %d errori\n", checks, fails);
    return fails ? 1 : 0;
}
