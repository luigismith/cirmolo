/* Chiedi all'IA - richieste HTTPS con il curl di spruce, in un processo figlio che non blocca l'interfaccia.
 *
 * Le intestazioni (con la chiave) e il corpo vanno in file temporanei 0600 in /tmp, che sta in RAM:
 * la chiave non compare mai nella riga di comando. In fondo all'uscita curl aggiunge una riga
 * "@@CIRMOLO_HTTP <codice>" con lo stato HTTP. Sul PC (Windows) non si collega: le prove usano dati registrati.
 */
#ifndef CLAUDECHAT_NET_H
#define CLAUDECHAT_NET_H

#include "json.h"

typedef struct {
    int pid, out, err;
    char hdr[64], body[64];
    Buf errbuf;
} Transfer;

typedef struct {
    const char *url;
    const char *headers;               /* "Nome: valore\n"... (va nel file temporaneo) */
    const char *body;                  /* corpo POST o NULL */
    size_t body_len;                   /* 0 = strlen(body) */
    const char *method;                /* NULL = POST se c'e' un corpo, altrimenti GET */
    const char *const *extra;          /* altri argomenti di curl (per esempio -F), terminati da NULL */
    int max_time;                      /* secondi, 0 = 900 */
} Request;

/* Avvia la richiesta: 0 se curl e' partito, altrimenti -1 con il motivo in msg. */
int net_start(Transfer *t, const Request *rq, char *msg, size_t msgn);
/* Passa a sink quello che e' arrivato; 1 quando e' finito, con l'exit code di curl e il suo errore. */
typedef void (*NetSink)(void *ud, const char *data, size_t len);
int net_poll(Transfer *t, NetSink sink, void *ud, int *curl_exit, char *err, size_t errn);
void net_cancel(Transfer *t);
int net_active(const Transfer *t);

/* Messaggio in italiano per un errore di curl (NULL se non ce n'e' uno adatto). */
const char *net_curl_message(int curl_exit);
/* Stato HTTP letto dalla riga @@CIRMOLO_HTTP (0 se manca). */
int net_http_line(const char *line);

#endif
