/* Chiedi all'IA - fornitori e modelli (vedi providers.h). */
#include "providers.h"

#include "i18n.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#define MKDIR(p) mkdir(p, 0755)
#endif

/* ------------------------------------------------------------------ elenco di base */
typedef struct { const char *id, *name; double in, out, cache_read; int flags; } PModel;
typedef struct {
    const char *id, *name;
    int proto;
    const char *base, *site, *note, *headers;
    int needs_key, stream_options, stt, tts;
    const char *stt_model, *tts_model, *voices;
    PModel models[8];
} PProvider;

/* Prezzi in dollari per milione di token, dalle pagine ufficiali del 09/10/2026 (-1 = non pubblicato). */
static const PProvider PRESETS[] = {
    { "anthropic", "Claude (Anthropic)", PROTO_ANTHROPIC, "https://api.anthropic.com/v1", "platform.claude.com",
      N_("Il più capace. A pagamento: meglio una chiave con un limite di spesa."), NULL, 1, 0, STT_NONE, TTS_NONE, NULL, NULL, NULL,
      { { "claude-opus-5-5", "Claude Opus 5.5", 4.0, 20.0, 0.20, MF_EFFORT | MF_FALLBACK },
        { "claude-sonnet-5-5", "Claude Sonnet 5.5", 2.0, 10.0, 0.20, MF_EFFORT | MF_FALLBACK },
        { "claude-haiku-5-5", "Claude Haiku 5.5", 0.10, 0.50, 0.01, MF_EFFORT },
        { "claude-fable-5-1", "Claude Fable 5.1", 10.0, 50.0, 0.25, MF_EFFORT | MF_FALLBACK } } },
    { "openai", "OpenAI", PROTO_OPENAI, "https://api.openai.com/v1", "platform.openai.com",
      N_("A pagamento. Fa anche voce: trascrizione e lettura ad alta voce."), NULL, 1, 1, STT_OPENAI, TTS_OPENAI,
      "gpt-4o-mini-transcribe", "gpt-4o-mini-tts", "marin,cedar,alloy,ash,ballad,coral,echo,fable,nova,onyx,sage,shimmer,verse",
      { { "gpt-5-nano", "GPT-5 nano", 0.05, 0.40, -1, 0 },
        { "gpt-6-luna", "GPT-6 Luna", -1, -1, -1, 0 },
        { "gpt-4.1-nano", "GPT-4.1 nano", -1, -1, -1, 0 },
        { "gpt-4o-mini", "GPT-4o mini", -1, -1, -1, 0 } } },
    { "gemini", "Google Gemini", PROTO_OPENAI, "https://generativelanguage.googleapis.com/v1beta/openai", "aistudio.google.com",
      N_("Piano gratuito con limiti (nello SEE i termini di Google chiedono quello a pagamento per le app)."), NULL, 1, 1, STT_GEMINI, TTS_GEMINI,
      "gemini-3.8-flash", "gemini-3.8-flash-tts", "Kore,Puck,Charon,Aoede,Fenrir,Leda,Orus,Zephyr",
      { { "gemini-3.8-flash", "Gemini 3.8 Flash", -1, -1, -1, 0 },
        { "gemini-3.5-flash-lite", "Gemini 3.5 Flash-Lite", -1, -1, -1, 0 },
        { "gemini-2.5-flash", "Gemini 2.5 Flash", -1, -1, -1, 0 } } },
    { "deepseek", "DeepSeek", PROTO_OPENAI, "https://api.deepseek.com/v1", "platform.deepseek.com",
      N_("Cinese, potente e molto economico (prezzi dimezzati fuori dalle ore di punta)."), NULL, 1, 1, STT_NONE, TTS_NONE, NULL, NULL, NULL,
      { { "deepseek-flash", "DeepSeek V4.1 Flash", 0.15, 0.60, -1, 0 },
        { "deepseek-v4-pro", "DeepSeek V4 Pro", 0.66, 1.98, -1, 0 } } },
    { "qwen", "Qwen (Alibaba)", PROTO_OPENAI, "https://dashscope-intl.aliyuncs.com/compatible-mode/v1", "modelstudio.console.alibabacloud.com",
      N_("Cinese. 1 milione di token gratis per modello per 90 giorni (attiva «Free Quota Only»)."), NULL, 1, 1, STT_NONE, TTS_NONE, NULL, NULL, NULL,
      { { "qwen3.8-max", "Qwen 3.8 Max", -1, -1, -1, 0 },
        { "qwen-plus", "Qwen Plus", -1, -1, -1, 0 },
        { "qwen-turbo", "Qwen Turbo", -1, -1, -1, 0 } } },
    { "zai", "GLM (Z.ai)", PROTO_OPENAI, "https://api.z.ai/api/paas/v4", "z.ai",
      N_("Cinese. I modelli Flash sono gratuiti."), NULL, 1, 1, STT_NONE, TTS_NONE, NULL, NULL, NULL,
      { { "glm-4.7-flash", "GLM-4.7 Flash", 0, 0, 0, MF_FREE },
        { "glm-4.6v-flash", "GLM-4.6V Flash (immagini)", 0, 0, 0, MF_FREE },
        { "glm-4.5-flash", "GLM-4.5 Flash", 0, 0, 0, MF_FREE },
        { "glm-5.3", "GLM-5.3", -1, -1, -1, 0 },
        { "glm-4.7", "GLM-4.7", -1, -1, -1, 0 } } },
    { "kimi", "Kimi (Moonshot)", PROTO_OPENAI, "https://api.moonshot.ai/v1", "platform.kimi.ai",
      N_("Cinese, a pagamento."), NULL, 1, 1, STT_NONE, TTS_NONE, NULL, NULL, NULL,
      { { "kimi-k3", "Kimi K3", 3.0, 15.0, -1, 0 },
        { "kimi-k2.6", "Kimi K2.6", 0.95, 4.0, -1, 0 } } },
    { "minimax", "MiniMax", PROTO_OPENAI, "https://api.minimax.io/v1", "platform.minimax.io",
      N_("Cinese, a pagamento."), NULL, 1, 1, STT_NONE, TTS_NONE, NULL, NULL, NULL,
      { { "MiniMax-M3", "MiniMax M3", -1, -1, -1, 0 },
        { "MiniMax-M2.7", "MiniMax M2.7", -1, -1, -1, 0 } } },
    { "groq", "Groq", PROTO_OPENAI, "https://api.groq.com/openai/v1", "console.groq.com",
      N_("Piano gratuito senza carta, velocissimo. Trascrive la voce con Whisper."), NULL, 1, 1, STT_OPENAI, TTS_NONE,
      "whisper-large-v3-turbo", NULL, NULL,
      { { "openai/gpt-oss-120b", "gpt-oss 120B", 0, 0, 0, MF_FREE },
        { "qwen/qwen3.8-27b", "Qwen 3.8 27B", 0, 0, 0, MF_FREE },
        { "llama-3.3-70b-versatile", "Llama 3.3 70B", 0, 0, 0, MF_FREE },
        { "minimaxai/minimax-m2.7", "MiniMax M2.7", 0, 0, 0, MF_FREE } } },
    { "openrouter", "OpenRouter", PROTO_OPENAI, "https://openrouter.ai/api/v1", "openrouter.ai",
      N_("Centinaia di modelli; quelli «:free» sono gratis (50 richieste al giorno)."),
      "HTTP-Referer: https://github.com/luigismith/cirmolo\nX-Title: Cirmolo\n", 1, 1, STT_NONE, TTS_NONE, NULL, NULL, NULL,
      { { "openrouter/free", "Gratis automatico", 0, 0, 0, MF_FREE },
        { "nvidia/nemotron-3-ultra-550b-a55b:free", "Nemotron 3 Ultra", 0, 0, 0, MF_FREE },
        { "google/gemma-4-31b-it:free", "Gemma 4 31B", 0, 0, 0, MF_FREE } } },
    { "mistral", "Mistral", PROTO_OPENAI, "https://api.mistral.ai/v1", "console.mistral.ai",
      N_("Europeo. Piano «Experiment» gratuito (serve il telefono)."), NULL, 1, 1, STT_NONE, TTS_NONE, NULL, NULL, NULL,
      { { "mistral-medium-latest", "Mistral Medium", -1, -1, -1, 0 },
        { "mistral-small-latest", "Mistral Small", -1, -1, -1, 0 } } },
    { "cerebras", "Cerebras", PROTO_OPENAI, "https://api.cerebras.ai/v1", "cloud.cerebras.ai",
      N_("Velocissimo. Prova gratuita con 5 $ di credito (serve una carta)."), NULL, 1, 1, STT_NONE, TTS_NONE, NULL, NULL, NULL,
      { { "gpt-oss-120b", "gpt-oss 120B", -1, -1, -1, 0 },
        { "qwen-3.8-27b", "Qwen 3.8 27B", -1, -1, -1, 0 } } },
};
#define NPRESETS ((int)(sizeof(PRESETS) / sizeof(PRESETS[0])))

static void copy(char *dst, size_t n, const char *s) { snprintf(dst, n, "%s", s ? s : ""); }

static Model *add_model(Provider *p, const char *id, const char *name)
{
    Model *m = realloc(p->models, sizeof(Model) * (size_t)(p->nmodels + 1));
    if (!m) abort();
    p->models = m;
    m = &p->models[p->nmodels++];
    memset(m, 0, sizeof(*m));
    copy(m->id, sizeof(m->id), id);
    copy(m->name, sizeof(m->name), name && *name ? name : id);
    m->in = m->out = m->cache_read = -1;
    return m;
}

int provider_find_model(const Provider *p, const char *id)
{
    for (int i = 0; i < p->nmodels; i++) if (!strcmp(p->models[i].id, id)) return i;
    return -1;
}

static Provider *add_provider(Registry *r)
{
    Provider *p = realloc(r->p, sizeof(Provider) * (size_t)(r->n + 1));
    if (!p) abort();
    r->p = p;
    p = &r->p[r->n++];
    memset(p, 0, sizeof(*p));
    return p;
}

int registry_find(const Registry *r, const char *id)
{
    for (int i = 0; i < r->n; i++) if (!strcmp(r->p[i].id, id)) return i;
    return -1;
}

static char *read_all(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    Buf b = { 0 };
    char tmp[4096];
    size_t n;
    while ((n = fread(tmp, 1, sizeof(tmp), f)) > 0) buf_add(&b, tmp, n);
    fclose(f);
    buf_add(&b, "", 0);
    if (len) *len = b.len;
    return buf_steal(&b);
}

/* Saves/claude/fornitori.json: {"fornitori":[{"id","nome","url","sito","nota","chiave":true,
   "intestazioni":"...","stream_options":true,"trascrizione":"openai","modello_trascrizione":"...",
   "sintesi":"openai","modello_sintesi":"...","voci":"...","modelli":[{"id","nome","gratis":true}]}]}
   Un id gia' presente cambia quel fornitore (per esempio solo "url"), uno nuovo lo aggiunge. */
static void load_custom(Registry *r, const char *dir)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/fornitori.json", dir);
    size_t len = 0;
    char *s = read_all(path, &len);
    if (!s) return;
    JNode *root = json_parse(s, len);
    const JNode *list = json_get(root, "fornitori");
    for (const JNode *f = list && list->type == J_ARRAY ? list->child : NULL; f; f = f->next) {
        const char *id = json_str(f, "id");
        if (!id || !*id || strchr(id, '/') || strchr(id, '\\') || strchr(id, '.')) continue;
        int k = registry_find(r, id);
        Provider *p = k >= 0 ? &r->p[k] : add_provider(r);
        if (k < 0) {
            copy(p->id, sizeof(p->id), id);
            copy(p->name, sizeof(p->name), id);
            p->proto = PROTO_OPENAI;
            p->stream_options = 1;
        }
        const char *v;
        if ((v = json_str(f, "nome"))) copy(p->name, sizeof(p->name), v);
        if ((v = json_str(f, "url"))) {
            copy(p->base, sizeof(p->base), v);
            size_t n = strlen(p->base);
            while (n && p->base[n - 1] == '/') p->base[--n] = 0;
        }
        if ((v = json_str(f, "protocollo"))) p->proto = !strcmp(v, "anthropic") ? PROTO_ANTHROPIC : PROTO_OPENAI;
        if ((v = json_str(f, "sito"))) copy(p->site, sizeof(p->site), v);
        if ((v = json_str(f, "nota"))) copy(p->note, sizeof(p->note), v);
        if ((v = json_str(f, "intestazioni"))) copy(p->headers, sizeof(p->headers), v);
        const JNode *b;
        if ((b = json_get(f, "chiave"))) p->needs_key = b->type == J_TRUE;
        else if (k < 0) p->needs_key = 0;
        if ((b = json_get(f, "stream_options"))) p->stream_options = b->type == J_TRUE;
        if ((v = json_str(f, "trascrizione"))) p->stt = !strcmp(v, "openai") ? STT_OPENAI : (!strcmp(v, "gemini") ? STT_GEMINI : STT_NONE);
        if ((v = json_str(f, "sintesi"))) p->tts = !strcmp(v, "openai") ? TTS_OPENAI : (!strcmp(v, "gemini") ? TTS_GEMINI : TTS_NONE);
        if ((v = json_str(f, "modello_trascrizione"))) copy(p->stt_model, sizeof(p->stt_model), v);
        if ((v = json_str(f, "modello_sintesi"))) copy(p->tts_model, sizeof(p->tts_model), v);
        if ((v = json_str(f, "voci"))) copy(p->voices, sizeof(p->voices), v);
        const JNode *ms = json_get(f, "modelli");
        for (const JNode *m = ms && ms->type == J_ARRAY ? ms->child : NULL; m; m = m->next) {
            const char *mid = m->type == J_STRING ? m->str : json_str(m, "id");
            if (!mid || !*mid || provider_find_model(p, mid) >= 0) continue;
            Model *x = add_model(p, mid, m->type == J_OBJECT ? json_str(m, "nome") : NULL);
            if (m->type == J_OBJECT) {
                const JNode *g = json_get(m, "gratis");
                if (g && g->type == J_TRUE) { x->flags |= MF_FREE; x->in = x->out = x->cache_read = 0; }
                x->in = json_num(m, "prezzo_ingresso", x->in);
                x->out = json_num(m, "prezzo_uscita", x->out);
            }
        }
        p->npreset = p->nmodels;
    }
    json_free(root);
    free(s);
}

static void load_fetched(Provider *p, const char *dir)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/modelli/%s.txt", dir, p->id);
    char *s = read_all(path, NULL);
    if (!s) return;
    for (char *line = strtok(s, "\r\n"); line; line = strtok(NULL, "\r\n")) {
        if (!*line || provider_find_model(p, line) >= 0) continue;
        Model *m = add_model(p, line, NULL);
        if (strstr(line, ":free")) { m->flags |= MF_FREE; m->in = m->out = m->cache_read = 0; }
    }
    free(s);
}

void registry_load(Registry *r, const char *dir)
{
    memset(r, 0, sizeof(*r));
    for (int i = 0; i < NPRESETS; i++) {
        const PProvider *s = &PRESETS[i];
        Provider *p = add_provider(r);
        copy(p->id, sizeof(p->id), s->id);
        copy(p->name, sizeof(p->name), s->name);
        p->proto = s->proto;
        copy(p->base, sizeof(p->base), s->base);
        copy(p->site, sizeof(p->site), s->site);
        copy(p->note, sizeof(p->note), s->note);
        copy(p->headers, sizeof(p->headers), s->headers);
        p->needs_key = s->needs_key;
        p->stream_options = s->stream_options;
        p->stt = s->stt;
        p->tts = s->tts;
        copy(p->stt_model, sizeof(p->stt_model), s->stt_model);
        copy(p->tts_model, sizeof(p->tts_model), s->tts_model);
        copy(p->voices, sizeof(p->voices), s->voices);
        for (int k = 0; k < 8 && s->models[k].id; k++) {
            const PModel *pm = &s->models[k];
            Model *m = add_model(p, pm->id, pm->name);
            m->in = pm->in;
            m->out = pm->out;
            m->cache_read = pm->cache_read;
            m->flags = pm->flags;
        }
        p->npreset = p->nmodels;
    }
    load_custom(r, dir);
    for (int i = 0; i < r->n; i++) load_fetched(&r->p[i], dir);
}

void registry_free(Registry *r)
{
    for (int i = 0; i < r->n; i++) free(r->p[i].models);
    free(r->p);
    memset(r, 0, sizeof(*r));
}

void provider_set_fetched(Provider *p, const char *dir, const char *const *ids, int n)
{
    p->nmodels = p->npreset;                      /* via i modelli scaricati prima */
    Buf b = { 0 };
    for (int i = 0; i < n; i++) {
        buf_adds(&b, ids[i]);
        buf_adds(&b, "\n");
        if (provider_find_model(p, ids[i]) >= 0) continue;
        Model *m = add_model(p, ids[i], NULL);
        if (strstr(ids[i], ":free")) { m->flags |= MF_FREE; m->in = m->out = m->cache_read = 0; }
    }
    char path[512];
    snprintf(path, sizeof(path), "%s/modelli", dir);
    MKDIR(path);
    snprintf(path, sizeof(path), "%s/modelli/%s.txt", dir, p->id);
    FILE *f = fopen(path, "wb");
    if (f) { fwrite(b.p ? b.p : "", 1, b.len, f); fclose(f); }
    buf_free(&b);
}

/* ------------------------------------------------------------------ chiavi */
/* Lettere, cifre e - _ . : (niente spazi o a capo nell'intestazione HTTP). Le chiavi di Z.ai hanno un punto. */
int clean_key(const char *in, char *out, size_t n)
{
    size_t k = 0;
    for (const char *p = in; *p; p++) {
        if (isspace((unsigned char)*p)) continue;
        if (!isalnum((unsigned char)*p) && !strchr("-_.:", *p)) return -1;
        if (k + 1 >= n) return -1;
        out[k++] = *p;
    }
    out[k] = 0;
    return k >= 16 ? 0 : -1;
}

void provider_key(const Provider *p, const char *dir, char *out, size_t n)
{
    char path[512];
    out[0] = 0;
    snprintf(path, sizeof(path), "%s/chiavi/%s.txt", dir, p->id);
    char *s = read_all(path, NULL);
    if (!s && !strcmp(p->id, "anthropic")) {      /* dove la metteva la 0.2 */
        snprintf(path, sizeof(path), "%s/chiave.txt", dir);
        s = read_all(path, NULL);
    }
    if (s && clean_key(s, out, n)) out[0] = 0;
    free(s);
}

int provider_save_key(const Provider *p, const char *dir, const char *key)
{
    char path[512], tmp[520];
    snprintf(path, sizeof(path), "%s/chiavi", dir);
    MKDIR(path);
    snprintf(path, sizeof(path), "%s/chiavi/%s.txt", dir, p->id);
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return -1;
    int ok = fwrite(key, 1, strlen(key), f) == strlen(key);
    ok &= fclose(f) == 0;
    if (!ok) { remove(tmp); return -1; }
#ifdef _WIN32
    remove(path);
#endif
    return rename(tmp, path);
}

void provider_auth_headers(const Provider *p, const char *key, Buf *h)
{
    if (p->proto == PROTO_ANTHROPIC) buf_printf(h, "x-api-key: %s\nanthropic-version: 2023-06-01\n", key);
    else if (key && *key) buf_printf(h, "Authorization: Bearer %s\n", key);
    if (p->headers[0]) {
        buf_adds(h, p->headers);
        if (p->headers[strlen(p->headers) - 1] != '\n') buf_adds(h, "\n");
    }
}

/* ------------------------------------------------------------------ elenco dei modelli scaricato */
static int chat_model(const char *id)
{
    static const char *SKIP[] = { "embed", "whisper", "tts", "transcribe", "dall-e", "image", "moderation", "realtime",
                                  "audio", "rerank", "guard", "vision-preview", "imagen", "veo", "aqa", "babbage",
                                  "davinci", "omni-moderation", "speech", "ocr", "sora", "search-preview", "computer-use" };
    char low[128];
    snprintf(low, sizeof(low), "%s", id);
    for (char *p = low; *p; p++) *p = (char)tolower((unsigned char)*p);
    for (size_t i = 0; i < sizeof(SKIP) / sizeof(SKIP[0]); i++) if (strstr(low, SKIP[i])) return 0;
    return 1;
}

static int cmp_str(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

int parse_model_list(const char *json, size_t len, char ***ids)
{
    *ids = NULL;
    JNode *root = json_parse(json, len);
    const JNode *data = json_get(root, "data");
    if (!data) data = json_get(root, "models");                 /* Ollama nativo e altri */
    int n = 0, cap = 0;
    char **out = NULL;
    for (const JNode *m = data && data->type == J_ARRAY ? data->child : NULL; m; m = m->next) {
        const char *id = json_str(m, "id");
        if (!id) id = json_str(m, "name");
        if (!id || !*id) continue;
        if (!strncmp(id, "models/", 7)) id += 7;                /* Gemini */
        if (!chat_model(id)) continue;
        if (n == cap) {
            cap = cap ? cap * 2 : 32;
            char **x = realloc(out, sizeof(char *) * (size_t)cap);
            if (!x) abort();
            out = x;
        }
        size_t l = strlen(id);
        out[n] = malloc(l + 1);
        if (!out[n]) abort();
        memcpy(out[n++], id, l + 1);
    }
    int valid = root != NULL;
    json_free(root);
    if (n > 1) qsort(out, (size_t)n, sizeof(char *), cmp_str);
    *ids = out;
    return valid ? n : -1;
}

/* ------------------------------------------------------------------ nomi e costi */
const char *model_display_name(const char *id)
{
    static const char *OTHERS[][2] = { { "claude-opus-5", "Claude Opus 5" }, { "claude-opus-4-8", "Claude Opus 4.8" },
                                       { "claude-sonnet-5", "Claude Sonnet 5" }, { "claude-fable-5", "Claude Fable 5" } };
    for (int i = 0; i < NPRESETS; i++)
        for (int k = 0; k < 8 && PRESETS[i].models[k].id; k++)
            if (!strcmp(id, PRESETS[i].models[k].id)) return PRESETS[i].models[k].name;
    for (size_t i = 0; i < sizeof(OTHERS) / sizeof(OTHERS[0]); i++) if (!strcmp(id, OTHERS[i][0])) return OTHERS[i][1];
    return id;
}

double model_cost(const Model *m, long in, long out, long cache_read, long cache_write)
{
    if (!m || m->in < 0 || m->out < 0) return 0.0;
    double cr = m->cache_read >= 0 ? m->cache_read : m->in;
    return (in * m->in + out * m->out + cache_read * cr + cache_write * m->in * 1.25) / 1e6;
}
