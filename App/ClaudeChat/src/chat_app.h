/* Chiedi all'IA per Miyoo Flip - interfaccia: chat, tastiera a schermo, voce, impostazioni, salvataggi. */
#ifndef CLAUDECHAT_APP_H
#define CLAUDECHAT_APP_H

#include "gfx.h"
#include "llm.h"

enum { PAGE_CHAT, PAGE_SETTINGS };

typedef struct ChatApp ChatApp;

ChatApp *chat_create(float sample_rate, const char *state_path);
void chat_destroy(ChatApp *a);
void chat_audio(ChatApp *a, float *out, int frames);
void chat_capture(ChatApp *a, const float *in, int frames);
void chat_capture_status(ChatApp *a, const char *device, float rate);
void chat_button(ChatApp *a, int pad, int pressed);
void chat_axes(ChatApp *a, float lx, float ly, float rx, float ry, float l2, float r2);
void chat_text(ChatApp *a, const char *utf8, int special);
void chat_update(ChatApp *a, float dt);
void chat_draw(ChatApp *a, Canvas *c);
int  chat_needs_draw(ChatApp *a);
int  chat_wants_quit(const ChatApp *a);

/* Per le prove: senza rete, le risposte arrivano da chat_feed_reply e le trascrizioni da chat_feed_transcript. */
void chat_set_offline(ChatApp *a, int offline);
void chat_feed_reply(ChatApp *a, const char *sse, int finish);
void chat_feed_transcript(ChatApp *a, const char *text);
Conversation *chat_conversation(ChatApp *a);
const char *chat_input(const ChatApp *a);
int  chat_busy(const ChatApp *a);
void chat_set_view(ChatApp *a, int page, int typing);
void chat_set_key(ChatApp *a, const char *key);
const char *chat_toast(const ChatApp *a);
void chat_osk_pos(const ChatApp *a, int *layer, int *row, int *col);
int  chat_select_provider(ChatApp *a, const char *id);
const char *chat_model_id(ChatApp *a);
int  chat_voice_state(const ChatApp *a);           /* 0 fermo, 1 R2 premuto, 2 registra, 3 trascrive */
void chat_set_voice(ChatApp *a, const char *stt, const char *tts);
char *chat_speech_text(ChatApp *a);                 /* testo in attesa di essere letto (malloc) o NULL */
void chat_open_settings(ChatApp *a, int tab, int sel);
void chat_open_picker(ChatApp *a);
void chat_set_paths(ChatApp *a, const char *root, const char *play_cmd);
int  chat_wants_launch(const ChatApp *a);

#endif
