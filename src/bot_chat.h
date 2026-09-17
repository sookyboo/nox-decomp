#ifndef NOX_BOT_CHAT_H
#define NOX_BOT_CHAT_H

#include <stdint.h>
#include <wchar.h>

/*
 * Cosmetic Bot-Script chat compatibility that is independent of teammate
 * orders. Incoming human chat may schedule a delayed response from the nearest
 * active native bot; the bot update path releases it on simulation time.
 */
void nox_bot_chat_on_message(int sender_object, const wchar_t *message);
void nox_bot_chat_update(int object, uint32_t frame);
void nox_bot_chat_forget_object(int object);

#endif
