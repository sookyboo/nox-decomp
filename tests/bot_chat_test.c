#include "../src/bot_chat.h"
#include "../src/bot_engine.h"
#include "../src/bot_policy.h"

#include <stdint.h>

#define SENDER 100
#define BOT_NEAR 200
#define BOT_FAR 300

static uint32_t current_frame;
static uint32_t current_fps;
static int random_value;
static int near_health;
static int far_health;
static int chat_calls;
static int last_chat_object;
static wchar_t last_chat[32];

static int text_equal(const wchar_t *a, const wchar_t *b)
{
    while (*a && *b && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}

uint32_t nox_bot_engine_frame(void)
{
    return current_frame;
}

uint32_t nox_bot_engine_fps(void)
{
    return current_fps;
}

int nox_bot_engine_random_int(int minimum, int maximum)
{
    if (random_value < minimum || random_value > maximum)
        return minimum;
    return random_value;
}

int nox_bot_engine_health(int object)
{
    if (object == BOT_NEAR)
        return near_health;
    if (object == BOT_FAR)
        return far_health;
    return 100;
}

void nox_bot_engine_position(int object, float *x, float *y)
{
    *y = 0.0f;
    if (object == SENDER)
        *x = 0.0f;
    else if (object == BOT_NEAR)
        *x = 10.0f;
    else if (object == BOT_FAR)
        *x = 100.0f;
    else
        *x = 1000.0f;
}

int nox_bot_engine_chat(int object, const wchar_t *message)
{
    int i = 0;

    ++chat_calls;
    last_chat_object = object;
    while (message[i] && i + 1 < (int)(sizeof(last_chat) / sizeof(last_chat[0]))) {
        last_chat[i] = message[i];
        ++i;
    }
    last_chat[i] = 0;
    return 1;
}

static void reset_state(void)
{
    nox_bot_policy_state *state;

    nox_bot_policy_reset_all();
    current_frame = 100;
    current_fps = 30;
    random_value = 1;
    near_health = 100;
    far_health = 100;
    chat_calls = 0;
    last_chat_object = 0;
    last_chat[0] = 0;

    nox_bot_policy_activate(1, NOX_BOT_DIFFICULTY_NORMAL, current_frame);
    state = nox_bot_policy_get(1);
    state->native_object = BOT_NEAR;
    nox_bot_policy_activate(2, NOX_BOT_DIFFICULTY_NORMAL, current_frame);
    state = nox_bot_policy_get(2);
    state->native_object = BOT_FAR;

    nox_bot_chat_forget_object(BOT_NEAR);
    nox_bot_chat_forget_object(BOT_FAR);
}

static int test_greeting_uses_nearest_active_bot_and_one_second_delay(void)
{
    reset_state();
    random_value = 2;
    nox_bot_chat_on_message(SENDER, L"Hello");
    nox_bot_chat_update(BOT_NEAR, 129);
    if (chat_calls)
        return 1;
    nox_bot_chat_update(BOT_FAR, 130);
    if (chat_calls)
        return 2;
    nox_bot_chat_update(BOT_NEAR, 130);
    if (chat_calls != 1 || last_chat_object != BOT_NEAR || !text_equal(last_chat, L"Hello!"))
        return 3;
    return 0;
}

static int test_reference_message_sets_and_random_choices(void)
{
    reset_state();
    random_value = 4;
    nox_bot_chat_on_message(SENDER, L"WHAT'S UP?");
    nox_bot_chat_update(BOT_NEAR, 130);
    if (chat_calls != 1 || !text_equal(last_chat, L"Greetings!"))
        return 4;

    current_frame = 200;
    random_value = 2;
    nox_bot_chat_on_message(SENDER, L"Good game!");
    nox_bot_chat_update(BOT_NEAR, 230);
    if (chat_calls != 2 || !text_equal(last_chat, L"Good game!"))
        return 5;

    current_frame = 300;
    nox_bot_chat_on_message(SENDER, L"not a bot greeting");
    nox_bot_chat_update(BOT_NEAR, 330);
    if (chat_calls != 2)
        return 6;
    return 0;
}

static int test_dead_bot_is_skipped_and_pending_chat_can_be_cancelled(void)
{
    reset_state();
    near_health = 0;
    random_value = 1;
    nox_bot_chat_on_message(SENDER, L"gg");
    nox_bot_chat_update(BOT_FAR, 130);
    if (chat_calls != 1 || last_chat_object != BOT_FAR || !text_equal(last_chat, L"GG!"))
        return 7;

    current_frame = 200;
    near_health = 100;
    nox_bot_chat_on_message(SENDER, L"hi");
    nox_bot_chat_forget_object(BOT_NEAR);
    nox_bot_chat_update(BOT_NEAR, 230);
    if (chat_calls != 1)
        return 8;
    return 0;
}

static int test_multiple_reference_responses_are_queued(void)
{
    reset_state();
    random_value = 1;
    nox_bot_chat_on_message(SENDER, L"hey");
    random_value = 2;
    nox_bot_chat_on_message(SENDER, L"gg");
    nox_bot_chat_update(BOT_NEAR, 130);
    if (chat_calls != 2 || !text_equal(last_chat, L"Good game!"))
        return 9;
    return 0;
}

int main(void)
{
    int result;

    result = test_greeting_uses_nearest_active_bot_and_one_second_delay();
    if (result)
        return result;
    result = test_reference_message_sets_and_random_choices();
    if (result)
        return result;
    result = test_dead_bot_is_skipped_and_pending_chat_can_be_cancelled();
    if (result)
        return result;
    return test_multiple_reference_responses_are_queued();
}
