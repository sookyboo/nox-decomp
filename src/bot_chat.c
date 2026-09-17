#include "bot_chat.h"

#include "bot_engine.h"
#include "bot_policy.h"

#include <stdint.h>

#define NOX_BOT_CHAT_PENDING_MAX 32

typedef enum nox_bot_chat_response {
    NOX_BOT_CHAT_RESPONSE_NONE = 0,
    NOX_BOT_CHAT_RESPONSE_HEY,
    NOX_BOT_CHAT_RESPONSE_HELLO,
    NOX_BOT_CHAT_RESPONSE_SUP,
    NOX_BOT_CHAT_RESPONSE_GREETINGS,
    NOX_BOT_CHAT_RESPONSE_GG,
    NOX_BOT_CHAT_RESPONSE_GOOD_GAME,
} nox_bot_chat_response;

typedef struct nox_bot_chat_pending {
    int object;
    uint32_t deadline;
    unsigned char response;
} nox_bot_chat_pending;

static nox_bot_chat_pending nox_bot_chat_pending_events[NOX_BOT_CHAT_PENDING_MAX];

static wchar_t nox_bot_chat_ascii_lower(wchar_t ch)
{
    if (ch >= L'A' && ch <= L'Z')
        return ch - L'A' + L'a';
    return ch;
}

static int nox_bot_chat_equal_ascii_ci(const wchar_t *a, const wchar_t *b)
{
    if (!a || !b)
        return 0;
    while (*a && *b) {
        if (nox_bot_chat_ascii_lower(*a) != nox_bot_chat_ascii_lower(*b))
            return 0;
        ++a;
        ++b;
    }
    return *a == 0 && *b == 0;
}

static int nox_bot_chat_message_kind(const wchar_t *message)
{
    static const wchar_t *const greetings[] = {
        L"hello", L"yo", L"what's up?", L"hi", L"hey", L"sup",
    };
    static const wchar_t *const good_games[] = {
        L"gg", L"gg!", L"good game!", L"good game",
    };
    unsigned int i;

    if (!message || !*message)
        return 0;
    for (i = 0; i < sizeof(greetings) / sizeof(greetings[0]); ++i) {
        if (nox_bot_chat_equal_ascii_ci(message, greetings[i]))
            return 1;
    }
    for (i = 0; i < sizeof(good_games) / sizeof(good_games[0]); ++i) {
        if (nox_bot_chat_equal_ascii_ci(message, good_games[i]))
            return 2;
    }
    return 0;
}

static int nox_bot_chat_nearest_active_bot(int sender_object)
{
    float sender_x;
    float sender_y;
    float best_distance = 3.4e38f;
    int best = 0;
    int slot;

    if (!sender_object)
        return 0;
    nox_bot_engine_position(sender_object, &sender_x, &sender_y);
    for (slot = 0; slot < NOX_BOT_PLAYER_SLOTS; ++slot) {
        nox_bot_policy_state *state = nox_bot_policy_get(slot);
        float bot_x;
        float bot_y;
        float dx;
        float dy;
        float distance;
        int object;

        if (!state || !state->active || !(object = state->native_object) ||
            object == sender_object || nox_bot_engine_health(object) <= 0)
            continue;
        nox_bot_engine_position(object, &bot_x, &bot_y);
        dx = bot_x - sender_x;
        dy = bot_y - sender_y;
        distance = dx * dx + dy * dy;
        if (distance >= best_distance)
            continue;
        best = object;
        best_distance = distance;
    }
    return best;
}

static int nox_bot_chat_schedule(int object, nox_bot_chat_response response)
{
    uint32_t fps;
    uint32_t frame;
    unsigned int i;

    if (!object || response == NOX_BOT_CHAT_RESPONSE_NONE)
        return 0;
    for (i = 0; i < NOX_BOT_CHAT_PENDING_MAX; ++i) {
        if (nox_bot_chat_pending_events[i].response != NOX_BOT_CHAT_RESPONSE_NONE)
            continue;
        frame = nox_bot_engine_frame();
        fps = nox_bot_engine_fps();
        if (!fps)
            fps = 30;
        nox_bot_chat_pending_events[i].object = object;
        nox_bot_chat_pending_events[i].deadline = frame + fps;
        nox_bot_chat_pending_events[i].response = (unsigned char)response;
        return 1;
    }
    return 0;
}

void nox_bot_chat_on_message(int sender_object, const wchar_t *message)
{
    int kind = nox_bot_chat_message_kind(message);
    int object;
    int choice;

    if (!kind)
        return;
    object = nox_bot_chat_nearest_active_bot(sender_object);
    if (!object)
        return;
    if (kind == 1) {
        choice = nox_bot_engine_random_int(1, 4);
        nox_bot_chat_schedule(object, (nox_bot_chat_response)(
            NOX_BOT_CHAT_RESPONSE_HEY + choice - 1));
    } else {
        choice = nox_bot_engine_random_int(1, 2);
        nox_bot_chat_schedule(object, (nox_bot_chat_response)(
            NOX_BOT_CHAT_RESPONSE_GG + choice - 1));
    }
}

void nox_bot_chat_update(int object, uint32_t frame)
{
    static const wchar_t *const messages[] = {
        0,
        L"Hey!",
        L"Hello!",
        L"Sup!",
        L"Greetings!",
        L"GG!",
        L"Good game!",
    };
    unsigned int i;

    if (!object)
        return;
    for (i = 0; i < NOX_BOT_CHAT_PENDING_MAX; ++i) {
        nox_bot_chat_pending *pending = &nox_bot_chat_pending_events[i];
        unsigned int response = pending->response;

        if (pending->object != object || response == NOX_BOT_CHAT_RESPONSE_NONE ||
            (int32_t)(frame - pending->deadline) < 0)
            continue;
        if (response < sizeof(messages) / sizeof(messages[0]) && messages[response])
            nox_bot_engine_chat(object, messages[response]);
        pending->object = 0;
        pending->deadline = 0;
        pending->response = NOX_BOT_CHAT_RESPONSE_NONE;
    }
}

void nox_bot_chat_forget_object(int object)
{
    unsigned int i;

    if (!object)
        return;
    for (i = 0; i < NOX_BOT_CHAT_PENDING_MAX; ++i) {
        if (nox_bot_chat_pending_events[i].object != object)
            continue;
        nox_bot_chat_pending_events[i].object = 0;
        nox_bot_chat_pending_events[i].deadline = 0;
        nox_bot_chat_pending_events[i].response = NOX_BOT_CHAT_RESPONSE_NONE;
    }
}
