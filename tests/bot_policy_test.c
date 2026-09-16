#include "../src/bot_policy.h"

#include <stdint.h>

static int test_reaction_delays(void)
{
    if (nox_bot_reaction_frames(NOX_BOT_DIFFICULTY_HARDCORE) != 0)
        return 1;
    if (nox_bot_reaction_frames(NOX_BOT_DIFFICULTY_HARD) != 15)
        return 2;
    if (nox_bot_reaction_frames(NOX_BOT_DIFFICULTY_NORMAL) != 30)
        return 3;
    if (nox_bot_reaction_frames(NOX_BOT_DIFFICULTY_EASY) != 45)
        return 4;
    if (nox_bot_reaction_frames(NOX_BOT_DIFFICULTY_BEGINNER) != 60)
        return 5;
    return 0;
}

static int test_policy_lifecycle(void)
{
    nox_bot_policy_state *state;

    nox_bot_policy_reset_all();
    if (nox_bot_policy_get(-1) || nox_bot_policy_get(NOX_BOT_PLAYER_SLOTS))
        return 10;
    if (!nox_bot_policy_activate(7, NOX_BOT_DIFFICULTY_NORMAL, 100))
        return 11;
    state = nox_bot_policy_get(7);
    if (!state || !state->active)
        return 12;
    if (state->order != NOX_BOT_ORDER_AUTO)
        return 13;
    if (state->next_reaction_frame != 130)
        return 14;
    if (nox_bot_reaction_ready(129, state->next_reaction_frame))
        return 15;
    if (!nox_bot_reaction_ready(130, state->next_reaction_frame))
        return 16;

    if (!nox_bot_policy_set_difficulty(7, NOX_BOT_DIFFICULTY_HARD, 200))
        return 17;
    if (state->difficulty != NOX_BOT_DIFFICULTY_HARD || state->next_reaction_frame != 215)
        return 18;

    nox_bot_policy_deactivate(7);
    if (state->active)
        return 19;
    if (nox_bot_policy_set_difficulty(7, NOX_BOT_DIFFICULTY_EASY, 300))
        return 20;
    return 0;
}

static int test_clear_life_state_preserves_persistent_policy(void)
{
    nox_bot_policy_state *state;

    nox_bot_policy_reset_all();
    if (!nox_bot_policy_activate(3, NOX_BOT_DIFFICULTY_HARD, 50))
        return 21;
    state = nox_bot_policy_get(3);
    state->native_object = 123;
    state->order = NOX_BOT_ORDER_GUARD;
    state->ordered_target = 456;
    state->warrior.chakram_attack_active = 1;
    state->warrior.pending_ability = 2;
    state->wizard.pending_spell = 3;
    state->wizard.fireball_ready_frame = 900;
    nox_bot_policy_record_event(state, NOX_BOT_EVENT_ENEMY_SIGHTED, 789, 60);

    nox_bot_policy_clear_life_state(state);
    if (!state->active || state->difficulty != NOX_BOT_DIFFICULTY_HARD ||
        state->order != NOX_BOT_ORDER_GUARD || state->native_object != 123 ||
        state->ordered_target != 456)
        return 22;
    if (state->pending_events || state->warrior.chakram_attack_active ||
        state->warrior.pending_ability || state->wizard.pending_spell ||
        state->wizard.fireball_ready_frame)
        return 23;
    return 0;
}

static int test_tick_wrap(void)
{
    uint32_t start = UINT32_MAX - 10u;
    uint32_t deadline = nox_bot_reaction_deadline(start, NOX_BOT_DIFFICULTY_HARD);

    if (deadline != 4u)
        return 30;
    if (nox_bot_reaction_ready(3u, deadline))
        return 31;
    if (!nox_bot_reaction_ready(4u, deadline))
        return 32;
    return 0;
}


static int test_event_state(void)
{
    nox_bot_policy_state *state;
    int event;

    nox_bot_policy_reset_all();
    if (!nox_bot_policy_activate(2, NOX_BOT_DIFFICULTY_NORMAL, 10))
        return 40;
    state = nox_bot_policy_get(2);
    for (event = 0; event < NOX_BOT_EVENT_COUNT; ++event) {
        nox_bot_policy_record_event(state, (nox_bot_event)event, 1000 + event, 2000 + (uint32_t)event);
        if (!nox_bot_policy_event_pending(state, (nox_bot_event)event))
            return 41 + event * 4;
        if (nox_bot_policy_event_object(state, (nox_bot_event)event) != 1000 + event)
            return 42 + event * 4;
        if (nox_bot_policy_event_frame(state, (nox_bot_event)event) != 2000u + (uint32_t)event)
            return 43 + event * 4;
    }
    for (event = 0; event < NOX_BOT_EVENT_COUNT; ++event) {
        nox_bot_policy_clear_event(state, (nox_bot_event)event);
        if (nox_bot_policy_event_pending(state, (nox_bot_event)event))
            return 81 + event;
    }
    nox_bot_policy_record_event(state, NOX_BOT_EVENT_COUNT, 456, 100);
    if (state->pending_events)
        return 91;
    return 0;
}

int main(void)
{
    int result;

    result = test_reaction_delays();
    if (result)
        return result;
    result = test_policy_lifecycle();
    if (result)
        return result;
    result = test_clear_life_state_preserves_persistent_policy();
    if (result)
        return result;
    result = test_tick_wrap();
    if (result)
        return result;
    return test_event_state();
}
