/* Cirmolo - registra una partita nel diario (vedi spruce/cirmolo-ia/diario.h). Lo lancia ia-console.sh in
 * sottofondo quando RetroArch si chiude:
 *   ia-diario registra <rom> <sistema> <inizio> <fine> [ultima schermata .png]
 * Le partite sotto i 20 secondi (giochi aperti per sbaglio) non si registrano. Se nella scheda Console di
 * Chiedi all'IA e' attivo il diario con l'IA, il modello della console guarda l'ultima schermata e scrive in
 * una o due frasi dove eri rimasto.
 * Prove: CIRMOLO_SAVES cambia la cartella delle impostazioni, CIRMOLO_RISPOSTA_FINTA evita la rete. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "ask.h"
#include "diario.h"
#include "i18n.h"
#include "iaconf.h"
#include "json.h"
#include "voice.h"

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#define MKDIR(p) mkdir(p, 0755)
#endif

#define SAVES "/mnt/SDCARD/Saves/claude"

static char *read_all(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    Buf b = { 0 };
    char tmp[8192];
    size_t n;
    while ((n = fread(tmp, 1, sizeof(tmp), f)) > 0) buf_add(&b, tmp, n);
    fclose(f);
    *len = b.len;
    return buf_steal(&b);
}

static int copy_file(const char *from, const char *to)
{
    size_t n = 0;
    char *d = read_all(from, &n);
    if (!d) return -1;
    FILE *f = fopen(to, "wb");
    int ok = f && fwrite(d, 1, n, f) == n;
    if (f) ok &= fclose(f) == 0;
    free(d);
    return ok ? 0 : -1;
}

int main(int argc, char **argv)
{
    if (argc < 6 || strcmp(argv[1], "registra")) {
        fprintf(stderr, "uso: ia-diario registra <rom> <sistema> <inizio> <fine> [schermata.png]\n");
        return 2;
    }
    char dir[512];
    snprintf(dir, sizeof(dir), "%s", argv[0]);
    char *slash = strrchr(dir, '/');
    if (!slash) slash = strrchr(dir, '\\');
    if (slash) *slash = 0; else snprintf(dir, sizeof(dir), ".");
    char lang[600];
    snprintf(lang, sizeof(lang), "%s/lang", dir);
    i18n_init(lang);
    const char *saves = getenv("CIRMOLO_SAVES") && *getenv("CIRMOLO_SAVES") ? getenv("CIRMOLO_SAVES") : SAVES;

    DiaryEntry e;
    memset(&e, 0, sizeof(e));
    snprintf(e.rom, sizeof(e.rom), "%s", argv[2]);
    snprintf(e.system, sizeof(e.system), "%s", argv[3]);
    diary_name_from_rom(e.rom, e.name, sizeof(e.name));
    e.start = atol(argv[4]);
    e.secs = atol(argv[5]) - e.start;
    if (e.secs < 20) { fprintf(stderr, "partita di %ld s: non si registra\n", e.secs); return 0; }

    /* ultima schermata: la miniatura del salvataggio automatico, copiata nel diario */
    const char *shot = argc > 6 ? argv[6] : "";
    char img_rel[300] = "", img_abs[700] = "";
    if (shot[0]) {
        snprintf(img_abs, sizeof(img_abs), "%s/diario", saves);
        MKDIR(img_abs);
        snprintf(img_abs, sizeof(img_abs), "%s/diario/img", saves);
        MKDIR(img_abs);
        snprintf(img_rel, sizeof(img_rel), "img/%ld.png", e.start);
        snprintf(img_abs, sizeof(img_abs), "%s/diario/%s", saves, img_rel);
        if (!copy_file(shot, img_abs)) snprintf(e.img, sizeof(e.img), "%s", img_rel);
        else img_abs[0] = 0;
    }

    IaSettings st;
    ia_settings_read(saves, &st);
    if (st.diary_ai && e.img[0]) {
        Registry reg;
        registry_load(&reg, saves);
        const Provider *p;
        const Model *m;
        char key[256], err[300];
        if (!ia_console_model(&reg, &st, saves, &p, &m, key, sizeof(key), err, sizeof(err))) {
            size_t n = 0;
            char *png = read_all(img_abs, &n);
            char *b64 = png ? b64_encode((unsigned char *)png, n) : NULL;
            char *prompt = diary_summary_prompt(i18n_language(), &e);
            AskSpec q = { NULL, prompt, b64, "image/png", 300, "low" };
            const char *fake = getenv("CIRMOLO_RISPOSTA_FINTA");
            char *text = fake ? strdup(fake) : ask_blocking(p, m, key, &q, 60, err, sizeof(err));
            if (text) {
                char *s = text;
                while (*s == ' ' || *s == '\n') s++;
                snprintf(e.summary, sizeof(e.summary), "%s", s);
                for (char *c = e.summary; *c; c++) if (*c == '\n') *c = ' ';
                free(text);
            } else fprintf(stderr, "riassunto: %s\n", err);
            free(prompt);
            free(b64);
            free(png);
        } else fprintf(stderr, "riassunto: %s\n", err);
        memset(key, 0, sizeof(key));
        registry_free(&reg);
    }
    int rc = diary_append(saves, &e);
    fprintf(stderr, "diario: %s, %ld s%s\n", e.name, e.secs, e.summary[0] ? ", con riassunto" : "");
    i18n_free();
    return rc ? 1 : 0;
}
