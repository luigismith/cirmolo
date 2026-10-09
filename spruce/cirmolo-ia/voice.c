/* Chiedi all'IA - voce (vedi voice.h). */
#include "voice.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i18n.h"

#define GEMINI_ROOT "https://generativelanguage.googleapis.com/v1beta"

/* ------------------------------------------------------------------ base64 */
static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char *b64_encode(const unsigned char *d, size_t n)
{
    size_t outn = (n + 2) / 3 * 4;
    char *o = malloc(outn + 1), *p = o;
    if (!o) abort();
    for (size_t i = 0; i < n; i += 3) {
        unsigned v = (unsigned)d[i] << 16 | (i + 1 < n ? (unsigned)d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
        *p++ = B64[v >> 18 & 63];
        *p++ = B64[v >> 12 & 63];
        *p++ = i + 1 < n ? B64[v >> 6 & 63] : '=';
        *p++ = i + 2 < n ? B64[v & 63] : '=';
    }
    *p = 0;
    return o;
}

unsigned char *b64_decode(const char *s, size_t n, size_t *out)
{
    unsigned char *o = malloc(n / 4 * 3 + 3), *p = o;
    if (!o) abort();
    unsigned v = 0;
    int bits = 0;
    for (size_t i = 0; i < n; i++) {
        const char *q = s[i] ? strchr(B64, s[i]) : NULL;
        int c = s[i] == '-' ? 62 : (s[i] == '_' ? 63 : (q ? (int)(q - B64) : -1));   /* anche base64url */
        if (c < 0) continue;                       /* '=', a capo, spazi */
        v = v << 6 | (unsigned)c;
        bits += 6;
        if (bits >= 8) { bits -= 8; *p++ = (unsigned char)(v >> bits & 255); }
    }
    *out = (size_t)(p - o);
    return o;
}

/* ------------------------------------------------------------------ lingue */
const char *lang_code(const char *language)
{
    static const char *MAP[][2] = {
        { "Italian", "it" }, { "English", "en" }, { "French", "fr" }, { "German", "de" }, { "Spanish", "es" },
        { "Portuguese (BR)", "pt" }, { "Portuguese (Eur)", "pt" }, { "Polish", "pl" }, { "Russian", "ru" },
        { "Ukrainian", "uk" }, { "Chinese (S)", "zh" }, { "Chinese (T)", "zh" }, { "Japanese", "ja" }, { "Korean", "ko" },
        { "Turkish", "tr" }, { "Greek", "el" }, { "Romanian", "ro" }, { "Croatian", "hr" }, { "Serbian", "sr" },
        { "Bosnian", "bs" }, { "Catalan", "ca" }, { "Vietnamese", "vi" }, { "Thai", "th" }, { "Hindi", "hi" },
        { "Azerbaijani", "az" }, { "Tagalog", "tl" }, { "Khmer", "km" }, { "Lao", "lo" },
    };
    for (size_t i = 0; language && i < sizeof(MAP) / sizeof(MAP[0]); i++) if (!strcmp(language, MAP[i][0])) return MAP[i][1];
    return "";
}

/* ------------------------------------------------------------------ registrazione */
void rec_init(Recorder *r)
{
    memset(r, 0, sizeof(*r));
    r->cap = 16000 * VOICE_MAX_SEC;
    r->pcm = malloc(sizeof(int16_t) * (size_t)r->cap);
    if (!r->pcm) abort();
    r->bt_pid = -1;
    r->bt_fd = -1;
}

void rec_free(Recorder *r)
{
    rec_stop(r);
    free(r->pcm);
    r->pcm = NULL;
}

int rec_start_usb(Recorder *r, float input_rate)
{
    if (input_rate <= 0) return -1;
    r->len = 0;
    r->rate = 16000;
    r->src_rate = input_rate;
    r->pos = 0;
    r->acc = 0;
    r->accn = 0;
    r->level = 0;
    __atomic_store_n(&r->on, 1, __ATOMIC_RELEASE);
    return 0;
}

static inline int16_t to16(float x)
{
    if (x > 1.0f) x = 1.0f;
    if (x < -1.0f) x = -1.0f;
    return (int16_t)lrintf(x * 32767.0f);
}

void rec_capture(Recorder *r, const float *in, int frames)
{
    if (!__atomic_load_n(&r->on, __ATOMIC_ACQUIRE) || r->bt_pid > 0) return;
    const float step = (float)r->rate / r->src_rate;
    float peak = r->level * 0.97f;
    int len = r->len;
    for (int i = 0; i < frames && len < r->cap; i++) {
        float x = in[i];
        if (fabsf(x) > peak) peak = fabsf(x);
        r->acc += x;
        r->accn++;
        r->pos += step;
        if (r->pos >= 1.0f) {                     /* media dei campioni del passo: filtro anti-aliasing semplice */
            r->pos -= 1.0f;
            r->pcm[len++] = to16(r->acc / (float)r->accn);
            r->acc = 0;
            r->accn = 0;
        }
    }
    r->level = peak;
    __atomic_store_n(&r->len, len, __ATOMIC_RELEASE);
}

float rec_seconds(const Recorder *r) { return r->rate ? (float)r->len / (float)r->rate : 0.0f; }

char *wav_encode(const int16_t *pcm, int n, int rate, size_t *len)
{
    size_t data = (size_t)n * 2, total = 44 + data;
    unsigned char *w = malloc(total);
    if (!w) abort();
    memcpy(w, "RIFF", 4);
    uint32_t v = (uint32_t)(total - 8);
    memcpy(w + 4, &v, 4);
    memcpy(w + 8, "WAVEfmt ", 8);
    v = 16; memcpy(w + 16, &v, 4);
    uint16_t s = 1; memcpy(w + 20, &s, 2);       /* PCM */
    s = 1; memcpy(w + 22, &s, 2);                 /* mono */
    v = (uint32_t)rate; memcpy(w + 24, &v, 4);
    v = (uint32_t)rate * 2; memcpy(w + 28, &v, 4);
    s = 2; memcpy(w + 32, &s, 2);
    s = 16; memcpy(w + 34, &s, 2);
    memcpy(w + 36, "data", 4);
    v = (uint32_t)data; memcpy(w + 40, &v, 4);
    memcpy(w + 44, pcm, data);                    /* la Flip e il PC sono little-endian, come il WAV */
    *len = total;
    return (char *)w;
}

#ifndef _WIN32
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

int rec_start_bt(Recorder *r, char *msg, size_t msgn)
{
    rec_stop(r);
    int po[2];
    if (pipe(po)) { snprintf(msg, msgn, "pipe: %s", strerror(errno)); return -1; }
    pid_t pid = fork();
    if (pid < 0) { close(po[0]); close(po[1]); snprintf(msg, msgn, "fork: %s", strerror(errno)); return -1; }
    if (pid == 0) {
        dup2(po[1], 1);
        int dn = open("/dev/null", O_WRONLY);
        if (dn >= 0) dup2(dn, 2);
        for (int fd = 3; fd < 1024; fd++) close(fd);
        /* profilo SCO (HFP) di bluealsa: CVSD a 8 kHz; 00:00:00:00:00:00 = le cuffie collegate */
        execlp("arecord", "arecord", "-q", "-D", "bluealsa:DEV=00:00:00:00:00:00,PROFILE=sco",
               "-f", "S16_LE", "-c", "1", "-r", "8000", "-t", "raw", (char *)NULL);
        _exit(127);
    }
    close(po[1]);
    fcntl(po[0], F_SETFL, fcntl(po[0], F_GETFL) | O_NONBLOCK);
    r->bt_pid = pid;
    r->bt_fd = po[0];
    r->len = 0;
    r->rate = 8000;
    r->level = 0;
    __atomic_store_n(&r->on, 1, __ATOMIC_RELEASE);
    return 0;
}

static int bt_read(Recorder *r)
{
    unsigned char buf[4096];
    static unsigned char half;
    static int has_half;
    int got_any = 0;
    for (;;) {
        ssize_t n = read(r->bt_fd, buf, sizeof(buf));
        if (n == 0) return -1;                    /* arecord e' finito */
        if (n < 0) return errno == EAGAIN || errno == EWOULDBLOCK ? got_any : -1;
        got_any = 1;
        float peak = r->level * 0.9f;
        for (ssize_t i = 0; i < n; i++) {
            if (!has_half) { half = buf[i]; has_half = 1; continue; }
            has_half = 0;
            int16_t s = (int16_t)(half | buf[i] << 8);
            if (r->len < r->cap) r->pcm[r->len++] = s;
            float a = fabsf(s / 32768.0f);
            if (a > peak) peak = a;
        }
        r->level = peak;
    }
}

void rec_poll(Recorder *r)
{
    if (r->bt_pid <= 0) return;
    if (bt_read(r) < 0) {
        int st;
        waitpid(r->bt_pid, &st, 0);
        close(r->bt_fd);
        r->bt_pid = -1;
        r->bt_fd = -1;
    }
}

void rec_stop(Recorder *r)
{
    __atomic_store_n(&r->on, 0, __ATOMIC_RELEASE);
    if (r->bt_pid > 0) {
        kill(r->bt_pid, SIGTERM);
        usleep(30000);
        bt_read(r);
        int st;
        for (int i = 0; i < 50 && waitpid(r->bt_pid, &st, WNOHANG) == 0; i++) usleep(10000);
        if (waitpid(r->bt_pid, &st, WNOHANG) == 0) { kill(r->bt_pid, SIGKILL); waitpid(r->bt_pid, &st, 0); }
        close(r->bt_fd);
        r->bt_pid = -1;
        r->bt_fd = -1;
    }
}

static int write_secret_file(const char *path, const char *data, size_t n)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return -1;
    size_t done = 0;
    while (done < n) {
        ssize_t w = write(fd, data + done, n - done);
        if (w <= 0) { close(fd); unlink(path); return -1; }
        done += (size_t)w;
    }
    close(fd);
    return 0;
}
#define REMOVE_FILE(p) unlink(p)

#else

int rec_start_bt(Recorder *r, char *msg, size_t msgn) { (void)r; snprintf(msg, msgn, "Sul PC non ci sono cuffie Bluetooth."); return -1; }
void rec_poll(Recorder *r) { (void)r; }
void rec_stop(Recorder *r) { __atomic_store_n(&r->on, 0, __ATOMIC_RELEASE); }
static int write_secret_file(const char *path, const char *data, size_t n)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    int ok = fwrite(data, 1, n, f) == n;
    fclose(f);
    return ok ? 0 : -1;
}
#define REMOVE_FILE(p) remove(p)

#endif

/* ------------------------------------------------------------------ uscita di curl: corpo e stato */
static void sink_buf(void *ud, const char *d, size_t n)
{
    Buf *b = ud;
    if (b->len < 8u * 1024 * 1024) buf_add(b, d, n);
}

/* Toglie la riga @@CIRMOLO_HTTP in fondo e ne restituisce il codice. */
static int cut_status(Buf *b)
{
    static const char MARK[] = "\n@@CIRMOLO_HTTP ";
    const size_t ml = sizeof(MARK) - 1;
    if (!b->p || b->len < ml) return 0;
    char *s = NULL;                               /* dall'ultimo byte: l'audio puo' contenere zeri */
    for (size_t i = b->len - ml + 1; i-- > 0;) if (b->p[i] == '\n' && !memcmp(b->p + i, MARK, ml)) { s = b->p + i; break; }
    if (!s) return 0;
    int code = net_http_line(s + 1);
    *s = 0;
    b->len = (size_t)(s - b->p);
    return code;
}

static void http_error_text(int code, const char *body, const char *who, char *err, size_t errn)
{
    JNode *j = json_parse(body ? body : "", body ? strlen(body) : 0);
    const JNode *e = json_get(j, "error");
    if (!e && j && j->type == J_ARRAY && j->child) e = json_get(j->child, "error");
    const char *m = e && e->type == J_STRING ? e->str : json_str(e, "message");
    if (code == 401 || code == 403) snprintf(err, errn, tr("Chiave di %s non valida o senza accesso alla voce."), who);
    else if (code == 429) snprintf(err, errn, tr("Limite raggiunto su %s: riprova tra poco."), who);
    else if (m && *m) snprintf(err, errn, "%s: %.150s", who, m);
    else snprintf(err, errn, tr("Errore di %s (HTTP %d)."), who, code);
    json_free(j);
}

/* ------------------------------------------------------------------ trascrizione */
static const char *gemini_url(char *out, size_t n, const char *model, const char *method)
{
    snprintf(out, n, GEMINI_ROOT "/models/%s:%s", model, method);
    return out;
}

int stt_start(Stt *s, const Provider *p, const char *key, const char *model, const char *wav, size_t wav_len,
              const char *lang, char *msg, size_t msgn)
{
    memset(s, 0, sizeof(*s));
    s->xfer.pid = -1;
    Buf h = { 0 };
    char url[300];
    int rc;
    if (p->stt == STT_GEMINI) {
        buf_printf(&h, "x-goog-api-key: %s\nContent-Type: application/json\n", key);
        char *b64 = b64_encode((const unsigned char *)wav, wav_len);
        Buf body = { 0 };
        buf_adds(&body, "{\"contents\":[{\"parts\":[{\"text\":");
        char prompt[400];
        snprintf(prompt, sizeof(prompt), "Transcribe this recording word for word%s%s. Reply with the transcription only, "
                 "without comments or quotes. If there is no speech, reply with nothing.", *lang ? " in language " : "", lang);
        json_escape(&body, prompt);
        buf_adds(&body, "},{\"inline_data\":{\"mime_type\":\"audio/wav\",\"data\":\"");
        buf_adds(&body, b64);
        buf_adds(&body, "\"}}]}],\"generationConfig\":{\"temperature\":0}}");
        free(b64);
        Request rq = { gemini_url(url, sizeof(url), model, "generateContent"), h.p, body.p, body.len, NULL, NULL, 120 };
        rc = net_start(&s->xfer, &rq, msg, msgn);
        buf_free(&body);
    } else {
        provider_auth_headers(p, key, &h);
        snprintf(s->wav_path, sizeof(s->wav_path), "/tmp/ia-voce-%d.wav", (int)(rand() % 100000));
        if (write_secret_file(s->wav_path, wav, wav_len)) {
            buf_free(&h);
            snprintf(msg, msgn, "%s", tr("Impossibile salvare la registrazione in /tmp."));
            return -1;
        }
        char file_arg[100], model_arg[120], lang_arg[32];
        snprintf(file_arg, sizeof(file_arg), "file=@%s;type=audio/wav", s->wav_path);
        snprintf(model_arg, sizeof(model_arg), "model=%s", model);
        snprintf(lang_arg, sizeof(lang_arg), "language=%s", lang);
        const char *extra[] = { "-F", file_arg, "-F", model_arg, "-F", "response_format=json", *lang ? "-F" : NULL, lang_arg, NULL };
        snprintf(url, sizeof(url), "%s/audio/transcriptions", p->base);
        Request rq = { url, h.p, NULL, 0, "POST", extra, 120 };
        rc = net_start(&s->xfer, &rq, msg, msgn);
        if (rc) { REMOVE_FILE(s->wav_path); s->wav_path[0] = 0; }
    }
    buf_free(&h);
    s->active = rc == 0;
    return rc;
}

char *stt_parse(const char *json, size_t len, int gemini)
{
    JNode *j = json_parse(json, len);
    if (!j) return NULL;
    Buf b = { 0 };
    buf_add(&b, "", 0);
    int found = 0;
    if (!gemini) {
        const char *t = json_str(j, "text");
        if (t) { buf_adds(&b, t); found = 1; }
    } else {
        const JNode *c = json_get(j, "candidates");
        const JNode *parts = c && c->child ? json_get(json_get(c->child, "content"), "parts") : NULL;
        for (const JNode *p = parts ? parts->child : NULL; p; p = p->next) {
            const char *t = json_str(p, "text");
            if (t) { buf_adds(&b, t); found = 1; }
        }
        if (c && c->child && !parts) found = 1;  /* nessuna parte: non ha sentito niente */
    }
    json_free(j);
    if (!found) { buf_free(&b); return NULL; }
    char *t = buf_steal(&b);
    size_t n = strlen(t), i = 0;                  /* spazi e virgolette intorno */
    while (i < n && (isspace((unsigned char)t[i]) || t[i] == '"')) i++;
    while (n > i && (isspace((unsigned char)t[n - 1]) || t[n - 1] == '"')) n--;
    memmove(t, t + i, n - i);
    t[n - i] = 0;
    return t;
}

int stt_poll(Stt *s, const Provider *p, char *out, size_t outn, char *err, size_t errn)
{
    if (!s->active) return 0;
    int code = 0;
    char cerr[200];
    if (!net_poll(&s->xfer, sink_buf, &s->body, &code, cerr, sizeof(cerr))) return 0;
    s->active = 0;
    if (s->wav_path[0]) { REMOVE_FILE(s->wav_path); s->wav_path[0] = 0; }
    out[0] = err[0] = 0;
    int http = cut_status(&s->body);
    if (code) {
        const char *m = net_curl_message(code);
        snprintf(err, errn, "%s", m ? tr(m) : cerr);
    } else if (http != 200) {
        http_error_text(http, s->body.p, p->name, err, errn);
    } else {
        char *t = stt_parse(s->body.p ? s->body.p : "", s->body.len, p->stt == STT_GEMINI);
        if (!t) snprintf(err, errn, "%s", tr("Risposta della trascrizione non riconosciuta."));
        else { snprintf(out, outn, "%s", t); free(t); }
    }
    buf_free(&s->body);
    return 1;
}

void stt_cancel(Stt *s)
{
    if (s->active) net_cancel(&s->xfer);
    s->active = 0;
    if (s->wav_path[0]) { REMOVE_FILE(s->wav_path); s->wav_path[0] = 0; }
    buf_free(&s->body);
}

/* ------------------------------------------------------------------ riproduzione */
#define RING_N (1u << RING_BITS)
#define RING_M (RING_N - 1)

void player_init(Player *pl)
{
    memset(pl, 0, sizeof(*pl));
    pl->ring = calloc(RING_N, sizeof(int16_t));
    if (!pl->ring) abort();
    pl->src_rate = 24000;
}

void player_free(Player *pl) { free(pl->ring); pl->ring = NULL; }

int player_queued(const Player *pl)
{
    return (int)(__atomic_load_n(&pl->wr, __ATOMIC_ACQUIRE) - __atomic_load_n(&pl->rd, __ATOMIC_ACQUIRE));
}

int player_free_space(const Player *pl) { return (int)RING_N - 1 - player_queued(pl); }

void player_push(Player *pl, const int16_t *s, int n)
{
    unsigned wr = pl->wr;
    int room = player_free_space(pl);
    if (n > room) n = room;
    for (int i = 0; i < n; i++) pl->ring[(wr + (unsigned)i) & RING_M] = s[i];
    __atomic_store_n(&pl->wr, wr + (unsigned)n, __ATOMIC_RELEASE);
}

void player_clear(Player *pl) { __atomic_store_n(&pl->stop, 1, __ATOMIC_RELEASE); }

void player_mix(Player *pl, float *out, int frames, float out_rate, float gain)
{
    if (__atomic_load_n(&pl->stop, __ATOMIC_ACQUIRE)) {
        __atomic_store_n(&pl->rd, __atomic_load_n(&pl->wr, __ATOMIC_ACQUIRE), __ATOMIC_RELEASE);
        pl->frac = 0;
        __atomic_store_n(&pl->stop, 0, __ATOMIC_RELEASE);
        return;
    }
    unsigned rd = pl->rd, wr = __atomic_load_n(&pl->wr, __ATOMIC_ACQUIRE);
    const float step = (float)pl->src_rate / out_rate;
    float frac = pl->frac;
    for (int i = 0; i < frames; i++) {
        if (wr - rd < 2) break;
        float a = pl->ring[rd & RING_M], b = pl->ring[(rd + 1) & RING_M];
        float s = (a + (b - a) * frac) * (gain / 32768.0f);
        out[2 * i] += s;
        out[2 * i + 1] += s;
        frac += step;
        while (frac >= 1.0f) { frac -= 1.0f; rd++; }
    }
    pl->frac = frac;
    __atomic_store_n(&pl->rd, rd, __ATOMIC_RELEASE);
}

/* ------------------------------------------------------------------ lettura: testo */
static void add_clean_inline(Buf *o, const char *p, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        char c = p[i];
        if (c == '*' || c == '_' || c == '`' || c == '#' || c == '~') continue;
        if (c == '|') { buf_adds(o, ", "); continue; }
        if (c == '[') {                           /* [testo](link) -> testo */
            const char *close = memchr(p + i, ']', n - i);
            if (close && (size_t)(close - p) + 1 < n && close[1] == '(') {
                const char *end = memchr(close, ')', n - (size_t)(close - p));
                if (end) { add_clean_inline(o, p + i + 1, (size_t)(close - p - (long)i - 1)); i = (size_t)(end - p); continue; }
            }
        }
        if ((c == 'h' && !strncmp(p + i, "http", 4) && i + 7 < n && (strncmp(p + i, "http://", 7) == 0 || strncmp(p + i, "https://", 8) == 0))) {
            while (i < n && !isspace((unsigned char)p[i])) i++;   /* gli indirizzi non si leggono */
            buf_adds(o, tr("(collegamento)"));
            i--;
            continue;
        }
        buf_add(o, &c, 1);
    }
}

char *tts_clean(const char *md)
{
    Buf o = { 0 };
    buf_add(&o, "", 0);
    int in_code = 0;
    const char *p = md;
    while (*p) {
        const char *e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        size_t lead = 0;
        while (lead < len && (p[lead] == ' ' || p[lead] == '\t')) lead++;
        if (len - lead >= 3 && !strncmp(p + lead, "```", 3)) {
            if (!in_code) { buf_adds(&o, tr("(segue del codice, da leggere sullo schermo)")); buf_adds(&o, "\n"); }
            in_code = !in_code;
        } else if (!in_code) {
            const char *q = p + lead;
            size_t qn = len - lead;
            while (qn && (*q == '#' || *q == '>')) { q++; qn--; }
            if (qn >= 2 && (*q == '-' || *q == '*' || *q == '+') && q[1] == ' ') { q += 2; qn -= 2; }
            while (qn && *q == ' ') { q++; qn--; }
            int rule = qn >= 3 && strspn(q, "-*_ ") >= qn;
            if (qn && !rule) {
                size_t before = o.len;
                add_clean_inline(&o, q, qn);
                /* una riga senza punto finale (titolo, voce di elenco) fa una pausa */
                char last = o.len > before ? o.p[o.len - 1] : 0;
                if (last && !strchr(".!?:;,", last)) buf_adds(&o, ".");
                buf_adds(&o, "\n");
            }
        }
        p = e ? e + 1 : p + len;
    }
    return buf_steal(&o);
}

static int is_boundary(const char *t, size_t i)
{
    char c = t[i];
    if (c == '\n') return 1;
    if (!strchr(".!?;:", c)) return 0;
    char n = t[i + 1];
    return n == ' ' || n == '\n' || n == 0;
}

char *tts_next_chunk(const char *text, size_t *pos, size_t max, int final)
{
    size_t len = strlen(text), i = *pos;
    while (i < len && isspace((unsigned char)text[i])) i++;
    if (i >= len) { *pos = len; return NULL; }
    size_t last_b = 0, last_sp = 0;
    size_t j;
    for (j = i; j < len && j - i < max; j++) {
        if (is_boundary(text, j) && (j + 1 < len || final)) last_b = j + 1;
        else if (text[j] == ' ') last_sp = j;
    }
    size_t end;
    if (j >= len && final) end = len;             /* tutto il resto ci sta */
    else if (last_b) end = last_b;
    else if (j - i >= max && last_sp > i) end = last_sp;   /* frase troppo lunga: si taglia a uno spazio */
    else if (j - i >= max) end = j;
    else return NULL;                             /* frase non ancora finita: si aspetta */
    while (end < len && ((unsigned char)text[end] & 0xC0) == 0x80) end++;   /* non a meta' di un carattere */
    size_t n = end - i;
    char *c = malloc(n + 1);
    if (!c) abort();
    memcpy(c, text + i, n);
    c[n] = 0;
    for (char *q = c; *q; q++) if (*q == '\n') *q = ' ';
    *pos = end;
    return c;
}

/* ------------------------------------------------------------------ lettura: richieste */
int tts_start(Tts *t, const Provider *p, const char *key, const char *model, const char *voice,
              const char *text, const char *lang, Player *pl, char *msg, size_t msgn)
{
    (void)lang;
    tts_cancel(t);
    memset(&t->xfer, 0, sizeof(t->xfer));
    t->xfer.pid = -1;
    t->gemini = p->tts == TTS_GEMINI;
    t->odd = 0;
    t->http = 0;
    t->err[0] = 0;
    buf_free(&t->body);
    Buf h = { 0 }, body = { 0 };
    char url[300];
    if (t->gemini) {
        buf_printf(&h, "x-goog-api-key: %s\nContent-Type: application/json\n", key);
        buf_adds(&body, "{\"contents\":[{\"parts\":[{\"text\":");
        json_escape(&body, text);
        buf_adds(&body, "}]}],\"generationConfig\":{\"responseModalities\":[\"AUDIO\"],\"speechConfig\":{\"voiceConfig\":"
                        "{\"prebuiltVoiceConfig\":{\"voiceName\":");
        json_escape(&body, voice);
        buf_adds(&body, "}}}}}");
        gemini_url(url, sizeof(url), model, "generateContent");
    } else {
        provider_auth_headers(p, key, &h);
        buf_adds(&h, "Content-Type: application/json\n");
        buf_adds(&body, "{\"model\":");
        json_escape(&body, model);
        buf_adds(&body, ",\"voice\":");
        json_escape(&body, voice);
        buf_adds(&body, ",\"response_format\":\"pcm\",\"input\":");
        json_escape(&body, text);
        if (strstr(model, "gpt-4o")) buf_adds(&body, ",\"instructions\":\"Speak naturally and warmly, at a relaxed pace.\"");
        buf_adds(&body, "}");
        snprintf(url, sizeof(url), "%s/audio/speech", p->base);
    }
    /* -i: le intestazioni della risposta arrivano prima dell'audio, cosi' un errore non si suona */
    static const char *const EXTRA[] = { "-i", NULL };
    Request rq = { url, h.p, body.p, body.len, NULL, t->gemini ? NULL : EXTRA, 180 };
    int rc = net_start(&t->xfer, &rq, msg, msgn);
    buf_free(&h);
    buf_free(&body);
    t->active = rc == 0;
    if (!t->gemini) pl->src_rate = 24000;
    return rc;
}

typedef struct { Tts *t; Player *pl; } TtsSink;

/* Audio OpenAI: intestazioni HTTP (con -i), poi PCM; gli ultimi byte si tengono da parte fino alla fine
   perche' curl ci aggiunge la riga @@CIRMOLO_HTTP. */
#define TAIL 32
static void tts_sink(void *ud, const char *d, size_t n)
{
    TtsSink *s = ud;
    Tts *t = s->t;
    if (t->gemini) { sink_buf(&t->body, d, n); return; }
    buf_add(&t->body, d, n);
    if (!t->http) {                               /* intestazioni ancora da leggere */
        for (;;) {
            char *end = t->body.p ? strstr(t->body.p, "\r\n\r\n") : NULL;
            if (!end) return;
            int code = 0;
            sscanf(t->body.p, "HTTP/%*s %d", &code);
            size_t cut = (size_t)(end + 4 - t->body.p);
            memmove(t->body.p, t->body.p + cut, t->body.len - cut);
            t->body.len -= cut;
            t->body.p[t->body.len] = 0;
            if (code >= 200) { t->http = code; break; }   /* 100 Continue: si salta */
        }
    }
    if (t->http != 200 || t->body.len <= TAIL) return;
    size_t avail = t->body.len - TAIL;
    unsigned char *b = (unsigned char *)t->body.p;
    size_t pairs = avail / 2;
    int16_t tmp[2048];
    size_t done = 0;
    while (done < pairs) {
        size_t k = pairs - done < 2048 ? pairs - done : 2048;
        for (size_t i = 0; i < k; i++) tmp[i] = (int16_t)(b[2 * (done + i)] | b[2 * (done + i) + 1] << 8);
        player_push(s->pl, tmp, (int)k);
        done += k;
    }
    size_t used = pairs * 2;
    memmove(t->body.p, t->body.p + used, t->body.len - used);
    t->body.len -= used;
    t->body.p[t->body.len] = 0;
}

static void push_bytes(Player *pl, const unsigned char *b, size_t n)
{
    int16_t tmp[2048];
    size_t pairs = n / 2, done = 0;
    while (done < pairs) {
        size_t k = pairs - done < 2048 ? pairs - done : 2048;
        for (size_t i = 0; i < k; i++) tmp[i] = (int16_t)(b[2 * (done + i)] | b[2 * (done + i) + 1] << 8);
        player_push(pl, tmp, (int)k);
        done += k;
    }
}

static void gemini_audio(Tts *t, Player *pl)
{
    JNode *j = json_parse(t->body.p ? t->body.p : "", t->body.len);
    const JNode *c = json_get(j, "candidates");
    const JNode *parts = c && c->child ? json_get(json_get(c->child, "content"), "parts") : NULL;
    int any = 0;
    for (const JNode *p = parts ? parts->child : NULL; p; p = p->next) {
        const JNode *in = json_get(p, "inlineData");
        if (!in) in = json_get(p, "inline_data");
        const char *data = json_str(in, "data"), *mime = json_str(in, "mimeType");
        if (!data) continue;
        size_t n = 0;
        unsigned char *pcm = b64_decode(data, strlen(data), &n);
        const unsigned char *s = pcm;
        int rate = 24000;
        const char *r = mime ? strstr(mime, "rate=") : NULL;
        if (r) rate = atoi(r + 5);
        if (n > 44 && !memcmp(pcm, "RIFF", 4)) {   /* WAV: si cerca il blocco dei dati */
            uint32_t sr;
            memcpy(&sr, pcm + 24, 4);
            rate = (int)sr;
            size_t off = 12;
            while (off + 8 <= n) {
                uint32_t sz;
                memcpy(&sz, pcm + off + 4, 4);
                if (!memcmp(pcm + off, "data", 4)) { s = pcm + off + 8; n = sz < n - off - 8 ? sz : n - off - 8; break; }
                off += 8 + sz;
            }
        }
        if (rate > 0) pl->src_rate = rate;
        push_bytes(pl, s, n);
        free(pcm);
        any = 1;
    }
    if (!any) snprintf(t->err, sizeof(t->err), "%s", tr("La sintesi vocale non ha restituito audio."));
    json_free(j);
}

int tts_poll(Tts *t, Player *pl, char *err, size_t errn)
{
    if (!t->active) return 0;
    TtsSink s = { t, pl };
    int code = 0;
    char cerr[200];
    if (!net_poll(&t->xfer, tts_sink, &s, &code, cerr, sizeof(cerr))) return 0;
    t->active = 0;
    err[0] = 0;
    int http = cut_status(&t->body);
    if (code) {
        const char *m = net_curl_message(code);
        snprintf(err, errn, "%s", m ? tr(m) : cerr);
    } else if (t->gemini) {
        if (http != 200) http_error_text(http, t->body.p, "Gemini", err, errn);
        else { gemini_audio(t, pl); snprintf(err, errn, "%s", t->err); }
    } else if (t->http != 200) {
        http_error_text(t->http ? t->http : http, t->body.p, tr("sintesi vocale"), err, errn);
    } else {
        push_bytes(pl, (const unsigned char *)(t->body.p ? t->body.p : ""), t->body.len);   /* la coda tenuta da parte */
    }
    buf_free(&t->body);
    return 1;
}

void tts_cancel(Tts *t)
{
    if (t->active) net_cancel(&t->xfer);
    t->active = 0;
    buf_free(&t->body);
}

/* ------------------------------------------------------------------ sintesi bloccante */
#ifndef _WIN32
#define TTS_NAP() usleep(20000)
#else
#define TTS_NAP() ((void)0)
#endif

char *tts_wav_blocking(const Provider *p, const char *key, const char *model, const char *voice, const char *text,
                       size_t *wav_len, char *err, size_t errn)
{
    Player pl;
    Tts t;
    memset(&t, 0, sizeof(t));
    t.xfer.pid = -1;
    player_init(&pl);
    char *wav = NULL;
    err[0] = 0;
    if (tts_start(&t, p, key, model, voice, text, "", &pl, err, errn) == 0) {
        while (!tts_poll(&t, &pl, err, errn)) TTS_NAP();
        int n = player_queued(&pl);
        if (!err[0] && n > 0) {
            int16_t *pcm = malloc(sizeof(int16_t) * (size_t)n);
            if (!pcm) abort();
            for (int i = 0; i < n; i++) pcm[i] = pl.ring[(pl.rd + (unsigned)i) & RING_M];
            wav = wav_encode(pcm, n, pl.src_rate, wav_len);
            free(pcm);
        } else if (!err[0]) snprintf(err, errn, "%s", tr("La sintesi vocale non ha restituito audio."));
    }
    tts_cancel(&t);
    buf_free(&t.pending);
    player_free(&pl);
    return wav;
}
