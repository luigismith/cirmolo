/* Cirmolo IA - una domanda sola, con un'immagine facoltativa, per i programmi che non sono una chat
 * (traduttore dei giochi, schede dei giochi, diario...). Usa il fornitore e la chiave come Chiedi all'IA.
 *
 * Funziona con tutti e due i protocolli: Anthropic (blocco "image" in base64) e compatibile OpenAI
 * ("image_url" con un indirizzo data:). Il modello deve saper leggere le immagini (Claude, GPT, Gemini,
 * GLM-4.6V-Flash, Qwen-VL...): se non sa, il fornitore risponde con un errore.
 */
#ifndef CIRMOLO_IA_ASK_H
#define CIRMOLO_IA_ASK_H

#include "llm.h"
#include "providers.h"

typedef struct {
    const char *system;                /* istruzioni (NULL = nessuna) */
    const char *prompt;                /* la domanda */
    const char *image_b64;             /* immagine in base64 (senza "data:") o NULL */
    const char *image_mime;            /* "image/png" (predefinito), "image/jpeg"... */
    int max_tokens;                    /* 0 = 4096 */
    const char *effort;                /* solo Claude: "low", "medium", "high" (NULL = predefinito) */
    int cache;                         /* solo Claude: cache automatica (istruzioni lunghe e sempre uguali) */
} AskSpec;

/* Corpo JSON della richiesta (stream) e URL; restituisce il corpo (malloc). */
char *ask_request(const Provider *p, const Model *m, const AskSpec *q, char *url, size_t urln);

/* Domanda bloccante (per i servizi in sottofondo): testo della risposta (malloc) o NULL con l'errore in err.
   timeout in secondi (0 = 120). */
char *ask_blocking(const Provider *p, const Model *m, const char *key, const AskSpec *q, int timeout,
                   char *err, size_t errn);

#endif
