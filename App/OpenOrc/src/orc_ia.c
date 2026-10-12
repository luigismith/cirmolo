/* OpenOrc - configurazione di una tastiera MIDI con l'intelligenza artificiale (vedi orc_ia.h). */
#include "orc_ia.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"

/* ------------------------------------------------------------------ ascolto */
void ia_scan_reset(IaScan *s) { memset(s, 0, sizeof(*s)); }

static IaCtl *ctl_get(IaScan *s, int kind, int port, int ch, int num)
{
    for (int i = 0; i < s->n; i++) {
        IaCtl *c = &s->c[i];
        if (c->kind == kind && c->port == port && c->ch == ch && c->num == num) return c;
    }
    if (s->n >= IA_CTL_MAX) return NULL;
    IaCtl *c = &s->c[s->n++];
    memset(c, 0, sizeof(*c));
    c->kind = kind;
    c->port = port;
    c->ch = ch;
    c->num = num;
    c->vmin = 128;
    c->vmax = -1;
    c->order = s->order++;
    return c;
}

void ia_scan_add(IaScan *s, int port, int type, int ch, int d1, int d2)
{
    if (port < 0 || port > 7) port = 0;
    ch &= 15;
    d1 &= 127;
    d2 &= 127;
    switch (type) {
    case 0x90:
        if (!d2) return;                              /* rilascio */
        /* fallthrough */
    case 0xB0: {
        IaCtl *c = ctl_get(s, type == 0x90 ? IA_NOTE : IA_CC, port, ch, d1);
        if (!c) return;
        c->count++;
        if (d2 < c->vmin) c->vmin = d2;
        if (d2 > c->vmax) c->vmax = d2;
        if ((d2 >= 1 && d2 <= 15) || d2 >= 113) c->rel_small++;
        if (d2 >= 57 && d2 <= 71) c->rel_mid++;
        if (d2 == 0 || d2 == 127) c->zero_full++;
        c->vals[d2 >> 3] |= (unsigned char)(1u << (d2 & 7));
        s->total++;
        break;
    }
    case 0xE0: s->bend[port * 16 + ch] = 1; s->total++; break;
    case 0xA0:
    case 0xD0: s->aftertouch = 1; break;
    case 0xC0: s->program = 1; break;
    }
}

static int distinct(const IaCtl *c)
{
    int n = 0;
    for (int i = 0; i < 16; i++) for (int b = 0; b < 8; b++) n += (c->vals[i] >> b) & 1;
    return n;
}

enum { CC_ABS, CC_BUTTON, CC_REL, CC_REL64 };
static int cc_type(const IaCtl *c)
{
    int d = distinct(c);
    if (c->zero_full == c->count && d <= 2) return CC_BUTTON;
    if (c->count >= 3 && c->rel_small == c->count && d <= 14 && c->vmax >= 113 && c->vmin <= 15) return CC_REL;
    if (c->count >= 3 && c->rel_small == c->count && d <= 6) return CC_REL;   /* girata in un verso solo */
    if (c->count >= 3 && c->rel_mid == c->count && d <= 12 && c->vmin != c->vmax) return CC_REL64;
    return CC_ABS;
}

void ia_scan_counts(const IaScan *s, int *notes, int *knobs, int *buttons, int *bends)
{
    int nn = 0, kn = 0, bt = 0, bd = 0;
    for (int i = 0; i < s->n; i++) {
        const IaCtl *c = &s->c[i];
        if (c->kind == IA_NOTE) nn++;
        else if (cc_type(c) == CC_BUTTON) bt++;
        else kn++;
    }
    for (int i = 0; i < 8 * 16; i++) bd += s->bend[i];
    if (notes) *notes = nn;
    if (knobs) *knobs = kn;
    if (buttons) *buttons = bt;
    if (bends) *bends = bd;
}

/* Un gruppo di note vicine e' la tastiera da pianoforte? Sul canale 10 (quello della batteria) di solito ci
   sono i pad, anche 16 in fila (due banchi): li' servono piu' di 20 note. */
static int keys_like(int cnt, int span, int ch) { return (cnt >= 13 && span >= 12 && ch != 9) || cnt > 20; }

/* ------------------------------------------------------------------ descrizioni */
typedef struct { char *p; size_t len, cap; } Out;
static void out_printf(Out *o, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void out_printf(Out *o, const char *fmt, ...)
{
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n >= sizeof(tmp)) n = sizeof(tmp) - 1;
    if (o->len + (size_t)n + 1 > o->cap) {
        size_t cap = o->cap ? o->cap * 2 : 1024;
        while (cap < o->len + (size_t)n + 1) cap *= 2;
        char *p = realloc(o->p, cap);
        if (!p) return;
        o->p = p;
        o->cap = cap;
    }
    memcpy(o->p + o->len, tmp, (size_t)n + 1);
    o->len += (size_t)n;
}

static int cmp_ctl(const void *x, const void *y)
{
    const IaCtl *a = *(const IaCtl *const *)x, *b = *(const IaCtl *const *)y;
    if (a->kind != b->kind) return a->kind - b->kind;
    if (a->port != b->port) return a->port - b->port;
    if (a->ch != b->ch) return a->ch - b->ch;
    return a->num - b->num;
}

char *ia_scan_describe(const IaScan *s, int en)
{
    Out o = { 0 };
    const IaCtl *v[IA_CTL_MAX];
    int n = 0;
    for (int i = 0; i < s->n; i++) v[n++] = &s->c[i];
    qsort(v, (size_t)n, sizeof(v[0]), cmp_ctl);
    /* note: gruppi di numeri vicini (salti di al massimo 2) sulla stessa porta e lo stesso canale */
    for (int i = 0; i < n && v[i]->kind == IA_NOTE;) {
        int j = i, vmin = 128, vmax = -1, first = 1 << 30;
        while (j + 1 < n && v[j + 1]->kind == IA_NOTE && v[j + 1]->port == v[i]->port && v[j + 1]->ch == v[i]->ch &&
               v[j + 1]->num - v[j]->num <= 2)
            j++;
        for (int k = i; k <= j; k++) {
            if (v[k]->vmin < vmin) vmin = v[k]->vmin;
            if (v[k]->vmax > vmax) vmax = v[k]->vmax;
            if (v[k]->order < first) first = v[k]->order;
        }
        int cnt = j - i + 1, keys = keys_like(cnt, v[j]->num - v[i]->num, v[i]->ch);
        if (en)
            out_printf(&o, "- notes %d-%d (%d different), port %d, channel %d, velocity %d-%d, first moved at step %d%s\n",
                       v[i]->num, v[j]->num, cnt, v[i]->port + 1, v[i]->ch + 1, vmin, vmax, first + 1,
                       keys ? ": probably the piano keys" : (v[i]->ch == 9 ? ": probably drum pads (channel 10 is the drum channel)" : ""));
        else if (cnt == 1)
            out_printf(&o, "Nota %d · porta %d · canale %d\n", v[i]->num, v[i]->port + 1, v[i]->ch + 1);
        else
            out_printf(&o, "Note %d-%d (%d) · porta %d · canale %d%s\n", v[i]->num, v[j]->num, cnt, v[i]->port + 1,
                       v[i]->ch + 1, keys ? ": tasti" : ": pad o pulsanti");
        i = j + 1;
    }
    if (!en) {                                        /* schermo: un CC per tipo, porta e canale in una riga */
        static const char *T[4] = { "Manopole", "Pulsanti", "Manopole relative", "Manopole relative" };
        for (int i = 0; i < n; i++) {
            if (v[i]->kind != IA_CC) continue;
            int first = i == 0 || v[i - 1]->kind != IA_CC || v[i - 1]->port != v[i]->port || v[i - 1]->ch != v[i]->ch;
            if (!first) continue;
            for (int t = 0; t < 4; t++) {
                char nums[200] = "";
                size_t l = 0;
                int cnt = 0;
                for (int j = i; j < n && v[j]->kind == IA_CC && v[j]->port == v[i]->port && v[j]->ch == v[i]->ch; j++) {
                    if (cc_type(v[j]) != t || (t == 3 && cc_type(v[j]) != 3)) continue;
                    if (l + 6 < sizeof(nums)) l += (size_t)snprintf(nums + l, sizeof(nums) - l, "%s%d", cnt ? ", " : "", v[j]->num);
                    cnt++;
                }
                if (t == 2) {                         /* le relative 64 vanno con le relative */
                    for (int j = i; j < n && v[j]->kind == IA_CC && v[j]->port == v[i]->port && v[j]->ch == v[i]->ch; j++)
                        if (cc_type(v[j]) == 3 && l + 6 < sizeof(nums)) { l += (size_t)snprintf(nums + l, sizeof(nums) - l, "%s%d", cnt ? ", " : "", v[j]->num); cnt++; }
                }
                if (t == 3 || !cnt) continue;
                out_printf(&o, "%s · porta %d · canale %d: CC %s\n", T[t], v[i]->port + 1, v[i]->ch + 1, nums);
            }
        }
    }
    for (int i = 0; i < n && en; i++) {
        const IaCtl *c = v[i];
        if (c->kind != IA_CC) continue;
        int t = cc_type(c);
        if (en) {
            static const char *T[4] = { "absolute (knob, fader or wheel)", "only 0 and 127 (button)",
                                        "relative (encoder: 1-63 up, 65-127 down)", "relative around 64 (encoder)" };
            const char *hint = c->num == 1 ? ", maybe the modulation wheel" : (c->num == 64 ? ", maybe the sustain pedal" : "");
            out_printf(&o, "- cc %d, port %d, channel %d, %d messages, values %d-%d, first moved at step %d, %s%s\n", c->num,
                       c->port + 1, c->ch + 1, c->count, c->vmin, c->vmax, c->order + 1, T[t], hint);
        } else {
            static const char *T[4] = { "manopola", "pulsante", "manopola relativa", "manopola relativa" };
            out_printf(&o, "CC %d · porta %d · canale %d: %s\n", c->num, c->port + 1, c->ch + 1, T[t]);
        }
    }
    for (int i = 0; i < 8 * 16; i++)
        if (s->bend[i]) {
            if (en) out_printf(&o, "- pitch bend, port %d, channel %d\n", i / 16 + 1, i % 16 + 1);
            else out_printf(&o, "Pitch bend · porta %d · canale %d\n", i / 16 + 1, i % 16 + 1);
        }
    if (s->aftertouch && en) out_printf(&o, "- aftertouch\n");
    if (!o.p) out_printf(&o, "%s", en ? "- (nothing)\n" : "Ancora niente\n");
    return o.p;
}

void ia_device_longname(const char *id, char *out, size_t n)
{
    snprintf(out, n, "%s", id ? id : "");
    if (!id || !*id) return;
    FILE *f = fopen("/proc/asound/cards", "r");
    if (!f) return;
    char line[256], want[64];
    snprintf(want, sizeof(want), "[%s", id);
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        if (found) {                                  /* riga dopo: il nome lungo */
            char *p = line;
            while (*p == ' ' || *p == '\t') p++;
            char *at = strstr(p, " at usb");
            if (at) *at = 0;
            p[strcspn(p, "\r\n")] = 0;
            if (*p) snprintf(out, n, "%s", p);
            break;
        }
        char *q = strstr(line, want);
        if (q && (q[strlen(want)] == ' ' || q[strlen(want)] == ']')) {
            char *dash = strstr(q, " - ");
            if (dash) {
                dash += 3;
                dash[strcspn(dash, "\r\n")] = 0;
                if (*dash) snprintf(out, n, "%s", dash);
            }
            found = 1;
        }
    }
    fclose(f);
}

/* ------------------------------------------------------------------ domanda */
char *ia_system_prompt(void)
{
    const char *s =
        "You configure USB MIDI controllers for OpenOrc, a chord synthesizer on a handheld game console "
        "(Miyoo Flip). You receive the controller's USB name and the controls the user actually moved while "
        "exploring it, then you assign OpenOrc functions to those controls.\n"
        "Use what you know about this controller model (its physical layout, banks and factory MIDI settings) "
        "to keep a natural order: pads top row left to right, then bottom row; knobs K1 to K8 left to right.\n"
        "When you are not sure of the physical order, follow the order in which the user first moved the controls "
        "(\"first moved at step\"): people explore left to right and top to bottom.\n"
        "On MPC-style pads (notes 36-51, usually channel 10) note 36 is the bottom-left pad: bank A has 36-39 in the "
        "bottom row and 40-43 in the top row, bank B has 44-47 in the bottom row and 48-51 in the top row. Use bank A "
        "for the chord pads (top row: qualities, bottom row: extensions) and bank B for the function pads.\n"
        "Rules:\n"
        "1. Use only controls from the list, with their exact type, number, channel and port.\n"
        "2. Never assign the piano keys: they play chords and melody in OpenOrc.\n"
        "3. Each control at most once.\n"
        "4. Chord pads and function pads need momentary buttons: drum pads (notes) or buttons (cc with 0 and "
        "127). Knobs need continuous controls (cc). Transport functions need buttons, ideally the controller's "
        "own play, record, loop and undo buttons.\n"
        "5. Priority: the 8 chord pads first, then the 8 knobs, then the 8 function pads, then transport.\n"
        "6. The modulation wheel (cc 1), the sustain pedal (cc 64) and pitch bend already work without "
        "assignment: do not use them.\n"
        "7. If the same control appears on more than one port, use the lowest port.\n"
        "8. Leave a function null when no suitable control is left.\n"
        "Answer with JSON only, no other text, in this shape:\n"
        "{\"device\":\"friendly controller name\",\"knob_mode\":\"absolute|relative|relative64\","
        "\"map\":{\"<function name exactly as given>\":{\"type\":\"note|cc\",\"number\":0,\"channel\":1,\"port\":1}},"
        "\"notes_it\":\"at most two short sentences in Italian, for the user: what goes where\"}";
    return strdup(s);
}

char *ia_user_prompt(const IaScan *s, const char *device, const char *longname, const char *const *targets,
                     const char *const *descr, int ntargets)
{
    Out o = { 0 };
    char *seen = ia_scan_describe(s, 1);
    out_printf(&o, "Controller USB id: \"%s\"\n", device ? device : "");
    if (longname && *longname && strcmp(longname, device ? device : "")) out_printf(&o, "Controller name: \"%s\"\n", longname);
    out_printf(&o, "\nControls the user moved (ports and channels count from 1):\n");
    /* la descrizione puo' essere lunga: si copia a pezzi */
    for (const char *p = seen; p && *p;) {
        const char *e = strchr(p, '\n');
        size_t l = e ? (size_t)(e - p + 1) : strlen(p);
        out_printf(&o, "%.*s", (int)(l > 400 ? 400 : l), p);
        p += l;
    }
    out_printf(&o, "\nOpenOrc functions (use these names exactly as map keys):\n");
    for (int i = 0; i < ntargets; i++) out_printf(&o, "- \"%s\": %s\n", targets[i], descr[i]);
    free(seen);
    return o.p;
}

/* ------------------------------------------------------------------ risposta */
static int lower_eq(const char *a, const char *b)
{
    while (*a && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { a++; b++; }
    return !*a && !*b;
}

static int target_index(const char *k, const char *const *targets, int n)
{
    for (int i = 0; i < n; i++) if (!strcmp(k, targets[i])) return i;
    int found = -1;
    for (int i = 0; i < n; i++)
        if (lower_eq(k, targets[i])) { if (found >= 0) return -1; found = i; }     /* m7 e M7: serve quello esatto */
    return found;
}

static const IaCtl *observed(const IaScan *s, int kind, int num, int ch, int port)
{
    const IaCtl *best = NULL;
    for (int pass = 0; pass < 2 && !best; pass++)
        for (int i = 0; i < s->n; i++) {
            const IaCtl *c = &s->c[i];
            if (c->kind != kind || c->num != num) continue;
            if (ch >= 0 && c->ch != ch) continue;
            if (pass == 0 && port >= 0 && c->port != port) continue;
            if (!best || c->port < best->port) best = c;
        }
    return best;
}

/* La nota fa parte di un gruppo che sembra la tastiera da pianoforte (almeno 13 note vicine su un'ottava)? */
static int is_key_note(const IaScan *s, int port, int ch, int num)
{
    unsigned char has[128] = { 0 };
    for (int i = 0; i < s->n; i++) {
        const IaCtl *c = &s->c[i];
        if (c->kind == IA_NOTE && c->port == port && c->ch == ch) has[c->num] = 1;
    }
    if (!has[num]) return 0;
    int lo = num, hi = num;
    for (;;) {                                        /* si allarga finche' i buchi sono di al massimo 2 */
        int step = 0;
        for (int d = 1; d <= 3 && lo - d >= 0; d++) if (has[lo - d]) { lo -= d; step = 1; break; }
        for (int d = 1; d <= 3 && hi + d <= 127; d++) if (has[hi + d]) { hi += d; step = 1; break; }
        if (!step) break;
    }
    int cnt = 0;
    for (int k = lo; k <= hi; k++) cnt += has[k];
    return keys_like(cnt, hi - lo, ch);
}

static int parse_one(const JNode *v, int *kind, int *num, int *ch, int *port)
{
    if (!v || v->type != J_OBJECT) return 0;
    const char *t = json_str(v, "type");
    if (!t) t = json_str(v, "tipo");
    if (!t) return 0;
    if (lower_eq(t, "note") || lower_eq(t, "nota")) *kind = IA_NOTE;
    else if (lower_eq(t, "cc") || lower_eq(t, "control") || lower_eq(t, "controller")) *kind = IA_CC;
    else return 0;
    double n = json_num(v, "number", -1);
    if (n < 0) n = json_num(v, "num", -1);
    if (n < 0 || n > 127) return 0;
    *num = (int)n;
    double c = json_num(v, "channel", 0), p = json_num(v, "port", 0);
    *ch = c >= 1 && c <= 16 ? (int)c - 1 : -1;
    *port = p >= 1 && p <= 8 ? (int)p - 1 : -1;
    return 1;
}

int ia_parse_reply(const char *text, const IaScan *s, const char *const *targets, int ntargets, IaMap *map,
                   int *knob_mode, char *name, size_t namen, char *note, size_t noten, char *err, size_t errn)
{
    for (int i = 0; i < ntargets; i++) map[i] = (IaMap){ IA_NONE, -1, -1, 0 };
    if (knob_mode) *knob_mode = -1;
    if (name && namen) name[0] = 0;
    if (note && noten) note[0] = 0;
    const char *a = text ? strchr(text, '{') : NULL, *b = text ? strrchr(text, '}') : NULL;
    if (!a || !b || b < a) { snprintf(err, errn, "La risposta dell'IA non contiene una configurazione."); return -1; }
    JNode *root = json_parse(a, (size_t)(b - a + 1));
    if (!root || root->type != J_OBJECT) { json_free(root); snprintf(err, errn, "La risposta dell'IA non è leggibile."); return -1; }
    const char *dev = json_str(root, "device"), *km = json_str(root, "knob_mode"), *nt = json_str(root, "notes_it");
    if (dev && name) snprintf(name, namen, "%s", dev);
    if (nt && note) snprintf(note, noten, "%s", nt);
    if (km && knob_mode) *knob_mode = lower_eq(km, "relative") ? 1 : (lower_eq(km, "relative64") ? 2 : (lower_eq(km, "absolute") ? 0 : -1));
    const JNode *m = json_get(root, "map");
    int assigned = 0;
    const IaCtl *used[64];
    int nused = 0;
    if (m && m->type == J_OBJECT) {
        for (const JNode *v = m->child; v; v = v->next) {
            if (!v->key) continue;
            int t = target_index(v->key, targets, ntargets), kind, num, ch, port;
            if (t < 0 || map[t].kind != IA_NONE || !parse_one(v, &kind, &num, &ch, &port)) continue;
            const IaCtl *c = observed(s, kind, num, ch, port);
            if (!c) continue;                         /* non e' tra i controlli mossi: inventato */
            if (c->kind == IA_NOTE && is_key_note(s, c->port, c->ch, c->num)) continue;   /* i tasti suonano */
            int dup = 0;
            for (int k = 0; k < nused; k++)
                dup |= used[k]->kind == c->kind && used[k]->num == c->num && used[k]->ch == c->ch;
            if (dup || nused >= 64) continue;
            used[nused++] = c;
            map[t] = (IaMap){ kind, c->port, c->ch, num };
            assigned++;
        }
    }
    json_free(root);
    if (!assigned) { snprintf(err, errn, "L'IA non ha assegnato nessun controllo tra quelli che hai mosso."); return -1; }
    return assigned;
}

int ia_guess_knob_mode(const IaScan *s, const IaMap *knobs, int n)
{
    int votes[4] = { 0 };
    for (int i = 0; i < n; i++) {
        if (knobs[i].kind != IA_CC) continue;
        const IaCtl *c = observed(s, IA_CC, knobs[i].num, knobs[i].ch, knobs[i].port);
        if (c && c->count >= 3) votes[cc_type(c)]++;
    }
    int abs_ = votes[CC_ABS], rel = votes[CC_REL], rel64 = votes[CC_REL64];
    if (!abs_ && !rel && !rel64) return -1;
    if (rel >= abs_ && rel >= rel64) return 1;
    if (rel64 > abs_) return 2;
    return 0;
}

static void squash(const char *s, char *out, size_t n)
{
    size_t j = 0;
    for (; s && *s && j + 1 < n; s++)
        if (isalnum((unsigned char)*s)) out[j++] = (char)tolower((unsigned char)*s);
    out[j] = 0;
}

int ia_same_device(const char *a, const char *b)
{
    char x[96], y[96];
    squash(a, x, sizeof(x));
    squash(b, y, sizeof(y));
    if (!x[0] || !y[0]) return 0;
    if (!strcmp(x, y)) return 1;
    size_t lx = strlen(x), ly = strlen(y), l = lx < ly ? lx : ly;
    return l >= 8 && !strncmp(x, y, l);              /* gli id ALSA sono tagliati a 15 caratteri */
}
