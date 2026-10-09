/* Chiedi all'IA - richieste HTTPS con curl (vedi net.h). */
#include "net.h"

#include "i18n.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *net_curl_message(int curl_exit)
{
    switch (curl_exit) {
    case 6: return N_("Nessuna connessione: attiva il Wi-Fi.");
    case 7: return N_("Il server non risponde.");
    case 28: return N_("Tempo scaduto: la connessione è lenta o assente.");
    case 35: case 60: case 77: return N_("Connessione sicura non riuscita: controlla data e ora della console.");
    case 52: case 56: case 18: return N_("Connessione interrotta.");
    case 26: return N_("Impossibile leggere il file da inviare.");
    case 127: return N_("curl non trovato.");
    }
    return NULL;
}

int net_http_line(const char *line)
{
    return !strncmp(line, "@@CIRMOLO_HTTP ", 15) ? atoi(line + 15) : 0;
}

int net_active(const Transfer *t) { return t->pid > 0; }

#ifndef _WIN32
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static int write_tmp(char *tmpl, const char *data, size_t n)
{
    int fd = mkstemp(tmpl);
    if (fd < 0) return -1;
    fchmod(fd, 0600);
    size_t done = 0;
    while (done < n) {
        ssize_t w = write(fd, data + done, n - done);
        if (w <= 0) { close(fd); unlink(tmpl); return -1; }
        done += (size_t)w;
    }
    close(fd);
    return 0;
}

static void cleanup(Transfer *t)
{
    if (t->out >= 0) close(t->out);
    if (t->err >= 0) close(t->err);
    t->out = t->err = -1;
    if (t->hdr[0]) unlink(t->hdr);
    if (t->body[0]) unlink(t->body);
    t->hdr[0] = t->body[0] = 0;
    buf_free(&t->errbuf);
}

int net_start(Transfer *t, const Request *rq, char *msg, size_t msgn)
{
    memset(t, 0, sizeof(*t));
    t->pid = -1;
    t->out = t->err = -1;
    const char *hdrs = rq->headers ? rq->headers : "";
    snprintf(t->hdr, sizeof(t->hdr), "/tmp/ia-h-XXXXXX");
    if (write_tmp(t->hdr, hdrs, strlen(hdrs))) {
        t->hdr[0] = 0;
        snprintf(msg, msgn, "%s", tr("Impossibile preparare la richiesta in /tmp."));
        cleanup(t);
        return -1;
    }
    if (rq->body) {
        snprintf(t->body, sizeof(t->body), "/tmp/ia-b-XXXXXX");
        if (write_tmp(t->body, rq->body, rq->body_len ? rq->body_len : strlen(rq->body))) {
            t->body[0] = 0;
            snprintf(msg, msgn, "%s", tr("Impossibile preparare la richiesta in /tmp."));
            cleanup(t);
            return -1;
        }
    }
    const char *curl = access("/mnt/SDCARD/spruce/bin64/curl", X_OK) == 0 ? "/mnt/SDCARD/spruce/bin64/curl" : "curl";
    const char *ca = access("/mnt/SDCARD/spruce/etc/ca-certificates.crt", R_OK) == 0 ? "/mnt/SDCARD/spruce/etc/ca-certificates.crt" : NULL;
    char hdrarg[80], bodyarg[80], maxt[16];
    snprintf(hdrarg, sizeof(hdrarg), "@%s", t->hdr);
    snprintf(bodyarg, sizeof(bodyarg), "@%s", t->body);
    snprintf(maxt, sizeof(maxt), "%d", rq->max_time > 0 ? rq->max_time : 900);
    const char *argv[64];
    int k = 0;
    argv[k++] = curl;
    argv[k++] = "-sS";
    argv[k++] = "-N";
    argv[k++] = "--connect-timeout"; argv[k++] = "20";
    argv[k++] = "--max-time"; argv[k++] = maxt;
    const char *method = rq->method ? rq->method : (rq->body || rq->extra ? "POST" : "GET");
    argv[k++] = "-X"; argv[k++] = method;
    argv[k++] = "-H"; argv[k++] = hdrarg;
    if (rq->body) { argv[k++] = "--data-binary"; argv[k++] = bodyarg; }
    for (int i = 0; rq->extra && rq->extra[i] && k < 56; i++) argv[k++] = rq->extra[i];
    argv[k++] = "-w"; argv[k++] = "\n@@CIRMOLO_HTTP %{http_code}\n";
    if (ca) { argv[k++] = "--cacert"; argv[k++] = ca; }
    argv[k++] = rq->url;
    argv[k] = NULL;
    int po[2], pe[2];
    if (pipe(po)) { snprintf(msg, msgn, "pipe: %s", strerror(errno)); cleanup(t); return -1; }
    if (pipe(pe)) { close(po[0]); close(po[1]); snprintf(msg, msgn, "pipe: %s", strerror(errno)); cleanup(t); return -1; }
    pid_t pid = fork();
    if (pid < 0) {
        close(po[0]); close(po[1]); close(pe[0]); close(pe[1]);
        snprintf(msg, msgn, "fork: %s", strerror(errno));
        cleanup(t);
        return -1;
    }
    if (pid == 0) {
        dup2(po[1], 1);
        dup2(pe[1], 2);
        for (int fd = 3; fd < 1024; fd++) close(fd);   /* niente audio, video o SD aperti nel figlio */
        execvp(curl, (char *const *)argv);
        _exit(127);
    }
    close(po[1]);
    close(pe[1]);
    fcntl(po[0], F_SETFL, fcntl(po[0], F_GETFL) | O_NONBLOCK);
    fcntl(pe[0], F_SETFL, fcntl(pe[0], F_GETFL) | O_NONBLOCK);
    t->pid = pid;
    t->out = po[0];
    t->err = pe[0];
    return 0;
}

int net_poll(Transfer *t, NetSink sink, void *ud, int *curl_exit, char *err, size_t errn)
{
    if (t->pid < 0) return 1;
    char buf[16384];
    int eof = 0;
    for (;;) {
        ssize_t n = read(t->out, buf, sizeof(buf));
        if (n > 0) { sink(ud, buf, (size_t)n); continue; }
        if (n == 0) eof = 1;
        else if (errno == EINTR) continue;
        break;
    }
    for (;;) {
        ssize_t n = read(t->err, buf, sizeof(buf));
        if (n > 0) { if (t->errbuf.len < 1024) buf_add(&t->errbuf, buf, (size_t)n); continue; }
        if (n < 0 && errno == EINTR) continue;
        break;
    }
    if (!eof) return 0;
    int status = 0;
    waitpid(t->pid, &status, 0);
    t->pid = -1;
    *curl_exit = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    if (err && errn) {
        err[0] = 0;
        if (t->errbuf.len) {
            snprintf(err, errn, "%s", t->errbuf.p);
            err[strcspn(err, "\r\n")] = 0;
            if (!strncmp(err, "curl: ", 6)) memmove(err, err + 6, strlen(err + 6) + 1);
        }
    }
    cleanup(t);
    return 1;
}

void net_cancel(Transfer *t)
{
    if (t->pid > 0) {
        kill(t->pid, SIGTERM);
        int status;
        for (int i = 0; i < 50 && waitpid(t->pid, &status, WNOHANG) == 0; i++) usleep(10000);
        if (waitpid(t->pid, &status, WNOHANG) == 0) { kill(t->pid, SIGKILL); waitpid(t->pid, &status, 0); }
    }
    t->pid = -1;
    cleanup(t);
}

#else  /* sul PC il client non si collega: le prove usano risposte registrate */

int net_start(Transfer *t, const Request *rq, char *msg, size_t msgn)
{
    (void)rq;
    memset(t, 0, sizeof(*t));
    t->pid = -1;
    snprintf(msg, msgn, "%s", tr("Sul PC questa app non si collega a Internet."));
    return -1;
}
int net_poll(Transfer *t, NetSink sink, void *ud, int *curl_exit, char *err, size_t errn)
{
    (void)t; (void)sink; (void)ud;
    *curl_exit = 0;
    if (err && errn) err[0] = 0;
    return 1;
}
void net_cancel(Transfer *t) { t->pid = -1; }

#endif
