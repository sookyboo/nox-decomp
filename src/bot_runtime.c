#include "bot_runtime.h"

#include "bot_engine.h"
#include "bot_warrior.h"

int nox_bot_runtime_attach_existing_player(int object, nox_bot_difficulty difficulty)
{
    int slot;

    if (!nox_bot_engine_enable_existing_player_bot(object))
        return 0;
    slot = nox_bot_engine_player_slot(object);
    if (slot < 0 || !nox_bot_policy_activate(slot, difficulty, nox_bot_engine_frame())) {
        nox_bot_engine_disable_existing_player_bot(object);
        return 0;
    }
    nox_bot_policy_get(slot)->native_object = object;
    return 1;
}

int nox_bot_runtime_detach_existing_player(int object)
{
    int slot = nox_bot_engine_player_slot(object);
    nox_bot_policy_state *state;

    if (slot < 0 || !nox_bot_engine_disable_existing_player_bot(object))
        return 0;
    state = nox_bot_policy_get(slot);
    if (state && state->native_object == object)
        nox_bot_policy_deactivate(slot);
    return 1;
}

int nox_bot_runtime_sync_native_player_bot(int object)
{
    int slot;
    nox_bot_policy_state *state;

    if (!nox_bot_engine_is_native_player_bot(object))
        return 0;
    slot = nox_bot_engine_player_slot(object);
    if (slot < 0)
        return 0;
    state = nox_bot_policy_get(slot);
    if (!state)
        return 0;
    if ((!state->active || state->native_object != object) && !nox_bot_policy_activate(
            slot, NOX_BOT_DIFFICULTY_NORMAL, nox_bot_engine_frame()))
        return 0;
    state = nox_bot_policy_get(slot);
    state->native_object = object;
    return 1;
}

void nox_bot_runtime_forget_native_player_bot(int object)
{
    int slot = nox_bot_engine_player_slot(object);
    nox_bot_policy_state *state;

    if (slot < 0)
        return;
    state = nox_bot_policy_get(slot);
    if (state && state->native_object == object)
        nox_bot_policy_deactivate(slot);
}

void nox_bot_runtime_event(int object, nox_bot_event event, int event_object)
{
    uint32_t frame;
    int slot;
    nox_bot_policy_state *state;

    if (!nox_bot_runtime_sync_native_player_bot(object))
        return;
    slot = nox_bot_engine_player_slot(object);
    state = nox_bot_policy_get(slot);
    frame = nox_bot_engine_frame();
    nox_bot_policy_record_event(state, event, event_object, frame);
    if (event == NOX_BOT_EVENT_COLLISION && nox_bot_engine_player_class(object) == 0)
        nox_bot_warrior_observe_collision(object, state, event_object, frame);
}

void nox_bot_runtime_clear_life_state(int object)
{
    int slot = nox_bot_engine_player_slot(object);
    nox_bot_policy_state *state;

    if (slot < 0)
        return;
    state = nox_bot_policy_get(slot);
    if (!state || !state->active || state->native_object != object)
        return;
    nox_bot_policy_clear_life_state(state);
}

int nox_bot_runtime_preserve_player_attack_state(int object)
{
    int slot = nox_bot_engine_player_slot(object);
    nox_bot_policy_state *state;

    if (slot < 0)
        return 0;
    state = nox_bot_policy_get(slot);
    if (!state || !state->active || state->native_object != object)
        return 0;
    return nox_bot_engine_player_class(object) == 0 && state->warrior.chakram_attack_active;
}

void nox_bot_runtime_update(int object)
{
    int slot;
    nox_bot_policy_state *state;

    if (!nox_bot_runtime_sync_native_player_bot(object))
        return;
    slot = nox_bot_engine_player_slot(object);
    state = nox_bot_policy_get(slot);
    if (!state || !state->active)
        return;
    switch (nox_bot_engine_player_class(object)) {
    case 0:
        nox_bot_warrior_update(object, state, nox_bot_engine_frame());
        break;
    default:
        break;
    }
}
