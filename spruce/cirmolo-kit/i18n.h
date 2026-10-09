/* Cirmolo kit - traduzioni delle app native.
 *
 * I testi nel codice sono in italiano, la lingua di partenza di Cirmolo. tr("testo") restituisce la
 * traduzione nella lingua scelta in PyUI (App/PyUI/py-ui-config.json, "language"), presa da
 * lang/<Lingua>.json nella cartella dell'app: un oggetto JSON piatto {"testo italiano": "traduzione"}.
 * Se per quella lingua non c'e' un file si usa English.json; un testo senza traduzione resta in italiano.
 * I segnaposto (%s, %d...) vanno lasciati uguali e nello stesso ordine.
 * CIRMOLO_LANG=<Lingua> forza la lingua (prove sul PC).
 */
#ifndef CIRMOLO_I18N_H
#define CIRMOLO_I18N_H

/* Carica le traduzioni da <dir>/<Lingua>.json; restituisce la lingua usata ("Italian" se nessuna). */
const char *i18n_init(const char *dir);
/* Come i18n_init, ma con una lingua precisa (NULL = nessuna traduzione). */
const char *i18n_set(const char *dir, const char *language);
const char *tr(const char *it);
/* Segna un testo da tradurre usato altrove con tr() (tabelle, messaggi salvati): non fa niente. */
#define N_(s) s
const char *i18n_language(void);
void i18n_free(void);

#endif
