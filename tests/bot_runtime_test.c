#include "../src/bot_runtime.h"

#include <stdint.h>

static int native_bot;
static int player_slot;
static uint32_t current_frame;
static int enable_result;
static int disable_result;
static int enable_calls;
static int disable_calls;
static int player_class;
static int warrior_update_calls;
static int warrior_update_object;
static uint32_t warrior_update_frame;
static int wizard_update_calls;
static int wizard_update_object;
static uint32_t wizard_update_frame;
static int conjurer_update_calls;
static int conjurer_update_object;
static uint32_t conjurer_update_frame;
static int warrior_collision_observe_calls;
static int warrior_collision_object;
static int warrior_collision_other;
static uint32_t warrior_collision_frame;

int nox_bot_engine_enable_existing_player_bot(int object)
{
    (void)object;
    ++enable_calls;
    if (enable_result)
        native_bot = 1;
    return enable_result;
}

int nox_bot_engine_disable_existing_player_bot(int object)
{
    (void)object;
    ++disable_calls;
    if (disable_result)
        native_bot = 0;
    return disable_result;
}

int nox_bot_engine_is_native_player_bot(int object)
{
    (void)object;
    return native_bot;
}

int nox_bot_engine_player_slot(int object)
{
    (void)object;
    return player_slot;
}

uint32_t nox_bot_engine_frame(void)
{
    return current_frame;
}

int nox_bot_engine_player_class(int object)
{
    (void)object;
    return player_class;
}

void nox_bot_warrior_observe_collision(
    int object, nox_bot_policy_state *state, int other, uint32_t frame)
{
    (void)state;
    ++warrior_collision_observe_calls;
    warrior_collision_object = object;
    warrior_collision_other = other;
    warrior_collision_frame = frame;
}

void nox_bot_warrior_update(int object, nox_bot_policy_state *state, uint32_t frame)
{
    (void)state;
    ++warrior_update_calls;
    warrior_update_object = object;
    warrior_update_frame = frame;
}

void nox_bot_wizard_update(int object, nox_bot_policy_state *state, uint32_t frame)
{
    (void)state;
    ++wizard_update_calls;
    wizard_update_object = object;
    wizard_update_frame = frame;
}

void nox_bot_conjurer_update(int object, nox_bot_policy_state *state, uint32_t frame)
{
    (void)state;
    ++conjurer_update_calls;
    conjurer_update_object = object;
    conjurer_update_frame = frame;
}

static void reset_stubs(void)
{
    native_bot = 0;
    player_slot = 4;
    current_frame = 100;
    enable_result = 1;
    disable_result = 1;
    enable_calls = 0;
    disable_calls = 0;
    player_class = 0;
    warrior_update_calls = 0;
    warrior_update_object = 0;
    warrior_update_frame = 0;
    wizard_update_calls = 0;
    wizard_update_object = 0;
    wizard_update_frame = 0;
    conjurer_update_calls = 0;
    conjurer_update_object = 0;
    conjurer_update_frame = 0;
    warrior_collision_observe_calls = 0;
    warrior_collision_object = 0;
    warrior_collision_other = 0;
    warrior_collision_frame = 0;
    nox_bot_policy_reset_all();
}

static int test_attach_detach(void)
{
    nox_bot_policy_state *state;

    reset_stubs();
    if (!nox_bot_runtime_attach_existing_player(123, NOX_BOT_DIFFICULTY_HARD))
        return 1;
    state = nox_bot_policy_get(4);
    if (!native_bot || enable_calls != 1 || !state || !state->active)
        return 2;
    if (state->native_object != 123)
        return 6;
    if (state->difficulty != NOX_BOT_DIFFICULTY_HARD || state->next_reaction_frame != 115)
        return 3;
    if (!nox_bot_runtime_detach_existing_player(123))
        return 4;
    if (native_bot || disable_calls != 1 || state->active)
        return 5;
    return 0;
}

static int test_attach_rolls_back_without_slot(void)
{
    reset_stubs();
    player_slot = -1;
    if (nox_bot_runtime_attach_existing_player(123, NOX_BOT_DIFFICULTY_NORMAL))
        return 10;
    if (disable_calls != 1 || native_bot)
        return 11;
    return 0;
}

static int test_sync_preserves_existing_policy(void)
{
    nox_bot_policy_state *state;

    reset_stubs();
    native_bot = 1;
    current_frame = 200;
    if (!nox_bot_runtime_sync_native_player_bot(123))
        return 20;
    state = nox_bot_policy_get(4);
    if (!state || !state->active || state->difficulty != NOX_BOT_DIFFICULTY_NORMAL)
        return 21;
    if (state->native_object != 123)
        return 26;
    if (state->next_reaction_frame != 230)
        return 22;
    if (!nox_bot_policy_set_difficulty(4, NOX_BOT_DIFFICULTY_BEGINNER, 300))
        return 23;
    current_frame = 400;
    if (!nox_bot_runtime_sync_native_player_bot(123))
        return 24;
    if (state->difficulty != NOX_BOT_DIFFICULTY_BEGINNER || state->next_reaction_frame != 360)
        return 25;
    return 0;
}

static int test_slot_reuse_resets_policy(void)
{
    nox_bot_policy_state *state;

    reset_stubs();
    native_bot = 1;
    if (!nox_bot_runtime_sync_native_player_bot(123))
        return 27;
    state = nox_bot_policy_get(4);
    if (!nox_bot_policy_set_difficulty(4, NOX_BOT_DIFFICULTY_BEGINNER, 200))
        return 28;
    current_frame = 500;
    if (!nox_bot_runtime_sync_native_player_bot(999))
        return 29;
    if (state->native_object != 999 || state->difficulty != NOX_BOT_DIFFICULTY_NORMAL)
        return 35;
    if (state->next_reaction_frame != 530)
        return 36;
    return 0;
}

static int test_event_capture(void)
{
    nox_bot_policy_state *state;
    int event;

    reset_stubs();
    native_bot = 1;
    for (event = 0; event < NOX_BOT_EVENT_COUNT; ++event) {
        current_frame = 777u + (uint32_t)event;
        nox_bot_runtime_event(123, (nox_bot_event)event, 456 + event);
    }
    state = nox_bot_policy_get(4);
    if (!state || !state->active)
        return 30;
    for (event = 0; event < NOX_BOT_EVENT_COUNT; ++event) {
        if (!nox_bot_policy_event_pending(state, (nox_bot_event)event))
            return 31 + event * 3;
        if (nox_bot_policy_event_object(state, (nox_bot_event)event) != 456 + event)
            return 32 + event * 3;
        if (nox_bot_policy_event_frame(state, (nox_bot_event)event) != 777u + (uint32_t)event)
            return 33 + event * 3;
    }
    if (warrior_collision_observe_calls != 1 || warrior_collision_object != 123 ||
        warrior_collision_other != 456 + NOX_BOT_EVENT_COLLISION ||
        warrior_collision_frame != 777u + NOX_BOT_EVENT_COLLISION)
        return 59;
    return 0;
}


static int test_runtime_clear_life_state(void)
{
    nox_bot_policy_state *state;

    reset_stubs();
    native_bot = 1;
    if (!nox_bot_runtime_sync_native_player_bot(123))
        return 60;
    state = nox_bot_policy_get(4);
    state->warrior.chakram_attack_active = 1;
    state->warrior.pending_ability = 2;
    nox_bot_policy_record_event(state, NOX_BOT_EVENT_ENEMY_SIGHTED, 44, 110);
    nox_bot_runtime_clear_life_state(123);
    if (!state->active || state->native_object != 123 || state->pending_events ||
        state->warrior.chakram_attack_active || state->warrior.pending_ability)
        return 61;
    return 0;
}

static int test_preserve_player_attack_state(void)
{
    nox_bot_policy_state *state;

    reset_stubs();
    native_bot = 1;
    if (!nox_bot_runtime_sync_native_player_bot(123))
        return 65;
    state = nox_bot_policy_get(4);
    state->warrior.chakram_attack_active = 1;
    if (!nox_bot_runtime_preserve_player_attack_state(123))
        return 66;
    state->warrior.chakram_attack_active = 0;
    if (nox_bot_runtime_preserve_player_attack_state(123))
        return 67;
    state->warrior.chakram_attack_active = 1;
    player_class = 1;
    if (nox_bot_runtime_preserve_player_attack_state(123))
        return 68;
    return 0;
}

static int test_runtime_update_dispatch(void)
{
    reset_stubs();
    native_bot = 1;
    current_frame = 900;
    nox_bot_runtime_update(123);
    if (warrior_update_calls != 1 || warrior_update_object != 123 || warrior_update_frame != 900)
        return 70;
    player_class = 1;
    current_frame = 901;
    nox_bot_runtime_update(123);
    if (warrior_update_calls != 1 || wizard_update_calls != 1 ||
        wizard_update_object != 123 || wizard_update_frame != 901)
        return 71;
    player_class = 2;
    current_frame = 902;
    nox_bot_runtime_update(123);
    if (warrior_update_calls != 1 || wizard_update_calls != 1 ||
        conjurer_update_calls != 1 || conjurer_update_object != 123 ||
        conjurer_update_frame != 902)
        return 72;
    return 0;
}

int main(void)
{
    int result;

    result = test_attach_detach();
    if (result)
        return result;
    result = test_attach_rolls_back_without_slot();
    if (result)
        return result;
    result = test_sync_preserves_existing_policy();
    if (result)
        return result;
    result = test_slot_reuse_resets_policy();
    if (result)
        return result;
    result = test_event_capture();
    if (result)
        return result;
    result = test_runtime_clear_life_state();
    if (result)
        return result;
    result = test_preserve_player_attack_state();
    if (result)
        return result;
    return test_runtime_update_dispatch();
}
