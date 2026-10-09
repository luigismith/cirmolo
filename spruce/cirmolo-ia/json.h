/* Chiedi a Claude - JSON minimo: lettura in un albero di nodi e scrittura di stringhe con escape.
 *
 * Ogni nodo ricorda la sua posizione nel testo sorgente, cosi' un pezzo di JSON si puo' ricopiare
 * identico (serve per rimandare all'API i messaggi gia' inviati senza cambiarne un byte).
 */
#ifndef CLAUDECHAT_JSON_H
#define CLAUDECHAT_JSON_H

#include <stddef.h>

/* Testo che cresce. */
typedef struct { char *p; size_t len, cap; } Buf;
void buf_add(Buf *b, const char *s, size_t n);
void buf_adds(Buf *b, const char *s);
void buf_printf(Buf *b, const char *fmt, ...);
void buf_free(Buf *b);
void buf_clear(Buf *b);
char *buf_steal(Buf *b);                 /* restituisce la stringa (malloc) e svuota il buffer */
void json_escape(Buf *b, const char *s); /* aggiunge "s" come stringa JSON */
void json_escape_n(Buf *b, const char *s, size_t n);

enum { J_NULL, J_FALSE, J_TRUE, J_NUMBER, J_STRING, J_ARRAY, J_OBJECT };
typedef struct JNode {
    int type;
    int start, end;                      /* [start, end) nel testo sorgente */
    double num;
    char *str;                           /* stringa decodificata (UTF-8) */
    char *key;                           /* chiave, se il nodo e' un membro di un oggetto */
    struct JNode *child, *next;
} JNode;

JNode *json_parse(const char *text, size_t len);    /* NULL se il testo non e' JSON valido */
void json_free(JNode *n);
const JNode *json_get(const JNode *obj, const char *key);
const char *json_str(const JNode *obj, const char *key);              /* NULL se manca o non e' una stringa */
double json_num(const JNode *obj, const char *key, double def);
const char *json_path_str(const JNode *obj, const char *a, const char *b);   /* obj.a.b */

#endif
