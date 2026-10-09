/* Chiedi all'IA - voce: registrazione dal microfono, trascrizione e lettura delle risposte, in cloud.
 *
 * - Registrazione: dal microfono USB (thread dell'ingresso del kit) o dalle cuffie Bluetooth (arecord dal
 *   profilo HFP di bluealsa, 8 kHz). Mono a 16 bit, al massimo VOICE_MAX_SEC secondi.
 * - Trascrizione: multipart /audio/transcriptions compatibile OpenAI (OpenAI, Groq...) o Gemini
 *   (generateContent con l'audio in base64).
 * - Lettura: /audio/speech in PCM a 24 kHz (OpenAI e compatibili, in streaming) o Gemini TTS (PCM in
 *   base64 nel JSON). Il testo si legge a frasi mentre arriva la risposta; il suono va in un anello
 *   letto dal thread audio, che lo ricampiona alla frequenza dell'uscita.
 */
#ifndef CLAUDECHAT_VOICE_H
#define CLAUDECHAT_VOICE_H

#include <stdint.h>

#include "json.h"
#include "net.h"
#include "providers.h"

#define VOICE_MAX_SEC 60

/* ------------------------------------------------------------------ registrazione */
typedef struct {
    int16_t *pcm;
    int cap;
    volatile int len;                  /* campioni scritti (thread dell'ingresso) */
    volatile int on;                   /* 1 mentre si registra */
    int rate;                          /* frequenza dei campioni salvati */
    float src_rate, pos;               /* ricampionamento dall'ingresso USB */
    float acc; int accn;
    volatile float level;              /* livello per l'indicatore (0..1) */
    int bt_pid, bt_fd;                 /* arecord per le cuffie Bluetooth */
} Recorder;

void rec_init(Recorder *r);
void rec_free(Recorder *r);
int  rec_start_usb(Recorder *r, float input_rate);          /* 16 kHz */
int  rec_start_bt(Recorder *r, char *msg, size_t msgn);      /* 0 se arecord e' partito */
void rec_capture(Recorder *r, const float *in, int frames);  /* dal thread dell'ingresso */
void rec_poll(Recorder *r);                                  /* legge arecord */
void rec_stop(Recorder *r);
float rec_seconds(const Recorder *r);
/* WAV mono 16 bit in memoria (malloc). */
char *wav_encode(const int16_t *pcm, int n, int rate, size_t *len);

/* ------------------------------------------------------------------ base64 */
char *b64_encode(const unsigned char *d, size_t n);
unsigned char *b64_decode(const char *s, size_t n, size_t *out);

/* ------------------------------------------------------------------ trascrizione */
typedef struct {
    Transfer xfer;
    Buf body;
    int active, http;
    char wav_path[64];
} Stt;

/* Avvia la trascrizione del WAV (lingua ISO 639-1, "" = automatica). */
int  stt_start(Stt *s, const Provider *p, const char *key, const char *model, const char *wav, size_t wav_len,
               const char *lang, char *msg, size_t msgn);
/* 1 quando e' finita: testo in out (vuoto se non ha capito) o errore in err. */
int  stt_poll(Stt *s, const Provider *p, char *out, size_t outn, char *err, size_t errn);
void stt_cancel(Stt *s);
/* Testo della risposta di trascrizione ({"text":...} o candidates di Gemini); NULL se manca. */
char *stt_parse(const char *json, size_t len, int gemini);

/* ------------------------------------------------------------------ lettura */
#define RING_BITS 22                    /* 4 Mi campioni: quasi 3 minuti a 24 kHz */
typedef struct {
    int16_t *ring;
    volatile unsigned wr, rd;          /* indici crescenti (anello di 1<<RING_BITS) */
    float frac;
    volatile int src_rate;
    volatile int stop;                 /* il thread audio svuota l'anello */
} Player;

void player_init(Player *pl);
void player_free(Player *pl);
void player_push(Player *pl, const int16_t *s, int n);
int  player_free_space(const Player *pl);
int  player_queued(const Player *pl);
void player_clear(Player *pl);
/* Thread audio: somma il parlato a out (stereo interlacciato) alla frequenza out_rate. */
void player_mix(Player *pl, float *out, int frames, float out_rate, float gain);

typedef struct {
    Transfer xfer;
    Buf pending;                       /* frasi da leggere */
    Buf body;                          /* risposta JSON di Gemini */
    int active, gemini, odd;           /* odd: byte rimasto a meta' di un campione */
    unsigned char half;
    int http;
    char err[200];
} Tts;

/* Testo pronto da leggere: senza Markdown, blocchi di codice detti a parole. */
char *tts_clean(const char *md);
/* Prossimo pezzo da leggere da text a partire da *pos: frasi intere fino a max caratteri;
   final = 1 se il testo e' completo (prende anche l'ultima frase senza punto). NULL se non c'e'. */
char *tts_next_chunk(const char *text, size_t *pos, size_t max, int final);
int  tts_start(Tts *t, const Provider *p, const char *key, const char *model, const char *voice,
               const char *text, const char *lang, Player *pl, char *msg, size_t msgn);
/* 1 quando la richiesta e' finita (err vuoto se e' andata bene). */
int  tts_poll(Tts *t, Player *pl, char *err, size_t errn);
void tts_cancel(Tts *t);

/* Sintesi bloccante (per i servizi in sottofondo): restituisce un WAV mono 16 bit (malloc) o NULL con
   l'errore in err. Usa un anello temporaneo, quindi va bene per testi brevi (meno di tre minuti). */
char *tts_wav_blocking(const Provider *p, const char *key, const char *model, const char *voice, const char *text,
                       size_t *wav_len, char *err, size_t errn);

/* Codice ISO della lingua di PyUI ("Italian" -> "it"); "" se non la conosce. */
const char *lang_code(const char *language);

#endif
