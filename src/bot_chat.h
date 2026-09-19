#ifndef NOX_BOT_CHAT_H
#define NOX_BOT_CHAT_H

#include <stdint.h>
#include <wchar.h>

/*
 * Bot-Script chat compatibility. Global greetings/good-game messages schedule
 * a delayed response from the nearest active bot; allied visible teammate
 * movement orders are applied immediately through native monster actions.
 */
void nox_bot_chat_on_message(int sender_object, const wchar_t *message);
void nox_bot_chat_update(int object, uint32_t frame);
void nox_bot_chat_forget_object(int object);

#endif
