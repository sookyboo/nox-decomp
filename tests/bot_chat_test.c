#include "../src/bot_chat.h"
#include "../src/bot_engine.h"
#include "../src/bot_policy.h"

#include <stdint.h>

#define SENDER 100
#define BOT_NEAR 200
#define BOT_FAR 300
#define BOT_WIZARD 400

static uint32_t current_frame;
static uint32_t current_fps;
static int random_value;
static int near_health;
static int far_health;
static int wizard_health;
static int near_visible;
static int far_visible;
static int wizard_visible;
static int near_same_team;
static int far_same_team;
static int wizard_same_team;
static int follow_calls;
static int hunt_calls;
static int guard_calls;
static int last_action_object;
static int last_follow_target;
static float last_guard_x;
static float last_guard_y;
static float last_guard_radius;
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
    if (object == BOT_WIZARD)
        return wizard_health;
    return 100;
}

int nox_bot_engine_player_class(int object)
{
    if (object == BOT_NEAR)
        return 0;
    if (object == BOT_WIZARD)
        return 1;
    if (object == BOT_FAR)
        return 2;
    return -1;
}

int nox_bot_engine_can_interact(int self, int other)
{
    if (other != SENDER)
        return 0;
    if (self == BOT_NEAR)
        return near_visible;
    if (self == BOT_FAR)
        return far_visible;
    if (self == BOT_WIZARD)
        return wizard_visible;
    return 0;
}

int nox_bot_engine_same_team(int self, int other)
{
    if (other != SENDER)
        return 0;
    if (self == BOT_NEAR)
        return near_same_team;
    if (self == BOT_FAR)
        return far_same_team;
    if (self == BOT_WIZARD)
        return wizard_same_team;
    return 0;
}

void nox_bot_engine_follow_target(int object, int target)
{
    ++follow_calls;
    last_action_object = object;
    last_follow_target = target;
}

void nox_bot_engine_hunt(int object)
{
    ++hunt_calls;
    last_action_object = object;
}

void nox_bot_engine_guard_position(int object, float x, float y, float radius)
{
    ++guard_calls;
    last_action_object = object;
    last_guard_x = x;
    last_guard_y = y;
    last_guard_radius = radius;
}

void nox_bot_engine_position(int object, float *x, float *y)
{
    *y = 0.0f;
    if (object == SENDER)
        *x = 0.0f;
    else if (object == BOT_NEAR)
        *x = 10.0f;
    else if (object == BOT_WIZARD)
        *x = 50.0f;
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
    wizard_health = 100;
    near_visible = 1;
    far_visible = 1;
    wizard_visible = 1;
    near_same_team = 1;
    far_same_team = 1;
    wizard_same_team = 1;
    follow_calls = 0;
    hunt_calls = 0;
    guard_calls = 0;
    last_action_object = 0;
    last_follow_target = 0;
    last_guard_x = 0.0f;
    last_guard_y = 0.0f;
    last_guard_radius = 0.0f;
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
    nox_bot_chat_forget_object(BOT_WIZARD);
}

static void activate_wizard(void)
{
    nox_bot_policy_state *state;

    nox_bot_policy_activate(3, NOX_BOT_DIFFICULTY_NORMAL, current_frame);
    state = nox_bot_policy_get(3);
    state->native_object = BOT_WIZARD;
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

static int test_follow_orders_apply_to_each_visible_ally_with_reference_chat_gates(void)
{
    reset_state();
    activate_wizard();
    random_value = 1;

    nox_bot_chat_on_message(SENDER, L"Follow");
    if (follow_calls != 3 || last_follow_target != SENDER || chat_calls != 3)
        return 10;
    if (!text_equal(last_chat, L"I'll follow you."))
        return 11;

    /* Warrior/Wizard Chatting gates suppress only acknowledgements for 2s;
     * Conjurer repeats its acknowledgement exactly as the reference does. */
    nox_bot_chat_on_message(SENDER, L"help");
    if (follow_calls != 6 || chat_calls != 4)
        return 12;

    current_frame = 159;
    nox_bot_chat_on_message(SENDER, L"escort");
    if (follow_calls != 9 || chat_calls != 5)
        return 13;

    current_frame = 160;
    random_value = 6;
    nox_bot_chat_on_message(SENDER, L"come");
    if (follow_calls != 12 || chat_calls != 8)
        return 14;
    /* Warrior has the two additional follow acknowledgements. */
    if (last_action_object != BOT_FAR)
        return 15;
    return 0;
}

static int test_attack_and_guard_orders_use_native_actions(void)
{
    reset_state();
    activate_wizard();
    random_value = 2;

    nox_bot_chat_on_message(SENDER, L"Attack");
    if (hunt_calls != 3 || follow_calls || guard_calls || chat_calls != 3)
        return 16;
    if (!text_equal(last_chat, L"Time to shine."))
        return 17;

    /* Move past Warrior/Wizard's Chatting gate before issuing guard. */
    current_frame = 160;
    random_value = 4;
    nox_bot_chat_on_message(SENDER, L"stay");
    if (guard_calls != 3 || chat_calls != 6 || last_action_object != BOT_FAR)
        return 18;
    if (last_guard_x != 100.0f || last_guard_y != 0.0f ||
        last_guard_radius != 300.0f || !text_equal(last_chat, L"I'll hold."))
        return 19;
    return 0;
}

static int test_orders_require_visibility_and_team_and_dead_warrior_is_ignored(void)
{
    reset_state();
    activate_wizard();
    near_health = 0;
    wizard_visible = 0;
    far_same_team = 0;

    nox_bot_chat_on_message(SENDER, L"go");
    if (hunt_calls || chat_calls)
        return 20;

    near_health = 100;
    wizard_visible = 1;
    far_same_team = 1;
    near_same_team = 0;
    nox_bot_chat_on_message(SENDER, L"guard");
    if (guard_calls != 2 || chat_calls != 2)
        return 21;
    return 0;
}

static int test_forget_clears_command_ack_gate(void)
{
    reset_state();
    random_value = 1;
    nox_bot_chat_on_message(SENDER, L"follow");
    if (chat_calls != 2)
        return 22;
    nox_bot_chat_forget_object(BOT_NEAR);
    nox_bot_chat_on_message(SENDER, L"follow");
    /* Warrior is allowed to acknowledge immediately again; Conjurer always does. */
    if (chat_calls != 4)
        return 23;
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
    result = test_multiple_reference_responses_are_queued();
    if (result)
        return result;
    result = test_follow_orders_apply_to_each_visible_ally_with_reference_chat_gates();
    if (result)
        return result;
    result = test_attack_and_guard_orders_use_native_actions();
    if (result)
        return result;
    result = test_orders_require_visibility_and_team_and_dead_warrior_is_ignored();
    if (result)
        return result;
    return test_forget_clears_command_ack_gate();
}
