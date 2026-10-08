/* Chiedi a Claude per Miyoo Flip - interfaccia: chat, tastiera a schermo, impostazioni, salvataggi. */
#ifndef CLAUDECHAT_APP_H
#define CLAUDECHAT_APP_H

#include "claude.h"
#include "gfx.h"

enum { PAGE_CHAT, PAGE_SETTINGS };

typedef struct ChatApp ChatApp;

ChatApp *chat_create(float sample_rate, const char *state_path);
void chat_destroy(ChatApp *a);
void chat_audio(ChatApp *a, float *out, int frames);
void chat_button(ChatApp *a, int pad, int pressed);
void chat_axes(ChatApp *a, float lx, float ly, float rx, float ry, float l2, float r2);
void chat_text(ChatApp *a, const char *utf8, int special);
void chat_update(ChatApp *a, float dt);
void chat_draw(ChatApp *a, Canvas *c);
int  chat_needs_draw(ChatApp *a);
int  chat_wants_quit(const ChatApp *a);

/* Per le prove: senza rete, le risposte arrivano da chat_feed_reply. */
void chat_set_offline(ChatApp *a, int offline);
void chat_feed_reply(ChatApp *a, const char *sse, int finish);
Conversation *chat_conversation(ChatApp *a);
const char *chat_input(const ChatApp *a);
int  chat_busy(const ChatApp *a);
void chat_set_view(ChatApp *a, int page, int typing);
void chat_set_key(ChatApp *a, const char *key);
const char *chat_toast(const ChatApp *a);
void chat_osk_pos(const ChatApp *a, int *layer, int *row, int *col);

#endif
