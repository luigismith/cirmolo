/* Cirmolo IA - impostazioni delle funzioni IA della console (vedi iaconf.h). */
#include "iaconf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i18n.h"

void ia_settings_read(const char *saves_dir, IaSettings *s)
{
    memset(s, 0, sizeof(*s));
    char path[480], chat_prov[32] = "", line[300];
    snprintf(path, sizeof(path), "%s/impostazioni.txt", saves_dir);
    FILE *f = fopen(path, "rb");
    if (!f) { snprintf(s->provider, sizeof(s->provider), "anthropic"); return; }   /* come Chiedi all'IA: Claude */
    char models[32][2][96], voices[32][2][40];
    int nm = 0, nv = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        char *eq = strchr(line, '=');
        if (!eq || line[0] == '#') continue;
        *eq = 0;
        const char *k = line, *v = eq + 1;
        if (!strcmp(k, "provider")) snprintf(chat_prov, sizeof(chat_prov), "%s", v);
        else if (!strcmp(k, "ia.provider")) snprintf(s->provider, sizeof(s->provider), "%s", v);
        else if (!strcmp(k, "ia.model")) snprintf(s->model, sizeof(s->model), "%s", v);
        else if (!strcmp(k, "traduzione.lingua")) snprintf(s->lang, sizeof(s->lang), "%s", v);
        else if (!strcmp(k, "traduzione.voce")) s->speak = atoi(v) != 0;
        else if (!strcmp(k, "tts")) snprintf(s->tts, sizeof(s->tts), "%s", v);
        else if (!strncmp(k, "model.", 6) && nm < 32) { snprintf(models[nm][0], 96, "%s", k + 6); snprintf(models[nm][1], 96, "%s", v); nm++; }
        else if (!strncmp(k, "voice.", 6) && nv < 32) { snprintf(voices[nv][0], 40, "%s", k + 6); snprintf(voices[nv][1], 40, "%s", v); nv++; }
    }
    fclose(f);
    if (!s->provider[0]) {                        /* senza scelta: il fornitore e il modello della chat */
        snprintf(s->provider, sizeof(s->provider), "%s", chat_prov[0] ? chat_prov : "anthropic");
        for (int i = 0; i < nm; i++) if (!strcmp(models[i][0], s->provider)) snprintf(s->model, sizeof(s->model), "%s", models[i][1]);
    }
    for (int i = 0; i < nv; i++) if (!strcmp(voices[i][0], s->tts)) snprintf(s->voice, sizeof(s->voice), "%s", voices[i][1]);
}

int ia_console_model(const Registry *reg, const IaSettings *s, const char *saves_dir,
                     const Provider **p, const Model **m, char *key, size_t keyn, char *err, size_t errn)
{
    *p = NULL;
    *m = NULL;
    key[0] = 0;
    int pi = registry_find(reg, s->provider);
    if (pi < 0 || !reg->p[pi].nmodels) {
        snprintf(err, errn, "%s", tr("Scegli il modello per le immagini in Chiedi all'IA, scheda Console."));
        return -1;
    }
    *p = &reg->p[pi];
    int mi = provider_find_model(*p, s->model);
    *m = &(*p)->models[mi >= 0 ? mi : 0];
    provider_key(*p, saves_dir, key, keyn);
    if ((*p)->needs_key && !key[0]) {
        snprintf(err, errn, tr("Manca la chiave API di %s: mettila in Chiedi all'IA."), (*p)->name);
        return -1;
    }
    return 0;
}

const Provider *ia_tts(const Registry *reg, const IaSettings *s, const char *saves_dir, char *key, size_t keyn,
                       char *voice, size_t voicen)
{
    int tp = registry_find(reg, s->tts);
    key[0] = 0;
    if (tp < 0 || !reg->p[tp].tts) return NULL;
    const Provider *p = &reg->p[tp];
    provider_key(p, saves_dir, key, keyn);
    if (p->needs_key && !key[0]) return NULL;
    snprintf(voice, voicen, "%s", s->voice);
    if (!voice[0]) { snprintf(voice, voicen, "%s", p->voices); voice[strcspn(voice, ",")] = 0; }
    return p;
}
