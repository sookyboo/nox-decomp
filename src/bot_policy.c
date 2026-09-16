#include "bot_policy.h"

#include <string.h>

static nox_bot_policy_state nox_bot_policy_slots[NOX_BOT_PLAYER_SLOTS];

static int nox_bot_difficulty_valid(nox_bot_difficulty difficulty)
{
    return difficulty >= NOX_BOT_DIFFICULTY_HARDCORE &&
           difficulty <= NOX_BOT_DIFFICULTY_BEGINNER;
}

uint32_t nox_bot_reaction_frames(nox_bot_difficulty difficulty)
{
    switch (difficulty) {
    case NOX_BOT_DIFFICULTY_HARDCORE:
        return 0;
    case NOX_BOT_DIFFICULTY_HARD:
        return 15;
    case NOX_BOT_DIFFICULTY_NORMAL:
        return 30;
    case NOX_BOT_DIFFICULTY_EASY:
        return 45;
    case NOX_BOT_DIFFICULTY_BEGINNER:
        return 60;
    default:
        return 30;
    }
}

uint32_t nox_bot_reaction_deadline(uint32_t frame, nox_bot_difficulty difficulty)
{
    return frame + nox_bot_reaction_frames(difficulty);
}

int nox_bot_reaction_ready(uint32_t frame, uint32_t deadline)
{
    /* Signed subtraction preserves the usual wrap-safe tick comparison. */
    return (int32_t)(frame - deadline) >= 0;
}

void nox_bot_policy_reset_all(void)
{
    memset(nox_bot_policy_slots, 0, sizeof(nox_bot_policy_slots));
}

void nox_bot_policy_reset(int player_slot)
{
    if (player_slot < 0 || player_slot >= NOX_BOT_PLAYER_SLOTS)
        return;
    memset(&nox_bot_policy_slots[player_slot], 0, sizeof(nox_bot_policy_slots[player_slot]));
}

nox_bot_policy_state *nox_bot_policy_get(int player_slot)
{
    if (player_slot < 0 || player_slot >= NOX_BOT_PLAYER_SLOTS)
        return 0;
    return &nox_bot_policy_slots[player_slot];
}

int nox_bot_policy_activate(int player_slot, nox_bot_difficulty difficulty, uint32_t frame)
{
    nox_bot_policy_state *state;

    if (!nox_bot_difficulty_valid(difficulty))
        return 0;
    state = nox_bot_policy_get(player_slot);
    if (!state)
        return 0;
    memset(state, 0, sizeof(*state));
    state->active = 1;
    state->difficulty = (unsigned char)difficulty;
    state->order = NOX_BOT_ORDER_AUTO;
    state->next_reaction_frame = nox_bot_reaction_deadline(frame, difficulty);
    return 1;
}

void nox_bot_policy_deactivate(int player_slot)
{
    nox_bot_policy_reset(player_slot);
}

void nox_bot_policy_clear_life_state(nox_bot_policy_state *state)
{
    if (!state || !state->active)
        return;
    state->pending_events = 0;
    memset(state->event_frame, 0, sizeof(state->event_frame));
    memset(state->event_object, 0, sizeof(state->event_object));
    memset(&state->warrior, 0, sizeof(state->warrior));
    memset(&state->wizard, 0, sizeof(state->wizard));
}

int nox_bot_policy_set_difficulty(int player_slot, nox_bot_difficulty difficulty, uint32_t frame)
{
    nox_bot_policy_state *state;

    if (!nox_bot_difficulty_valid(difficulty))
        return 0;
    state = nox_bot_policy_get(player_slot);
    if (!state || !state->active)
        return 0;
    state->difficulty = (unsigned char)difficulty;
    state->next_reaction_frame = nox_bot_reaction_deadline(frame, difficulty);
    return 1;
}

void nox_bot_policy_schedule_reaction(nox_bot_policy_state *state, uint32_t frame)
{
    if (!state || !state->active)
        return;
    state->next_reaction_frame = nox_bot_reaction_deadline(
        frame, (nox_bot_difficulty)state->difficulty);
}

static int nox_bot_event_valid(nox_bot_event event)
{
    return event >= NOX_BOT_EVENT_LOOKING_FOR_ENEMY && event < NOX_BOT_EVENT_COUNT;
}

void nox_bot_policy_record_event(
    nox_bot_policy_state *state,
    nox_bot_event event,
    int event_object,
    uint32_t frame)
{
    uint32_t mask;

    if (!state || !state->active || !nox_bot_event_valid(event))
        return;
    mask = 1u << (unsigned int)event;
    state->pending_events |= mask;
    state->event_object[event] = event_object;
    state->event_frame[event] = frame;
}

int nox_bot_policy_event_pending(const nox_bot_policy_state *state, nox_bot_event event)
{
    if (!state || !state->active || !nox_bot_event_valid(event))
        return 0;
    return (state->pending_events & (1u << (unsigned int)event)) != 0;
}

int nox_bot_policy_event_object(const nox_bot_policy_state *state, nox_bot_event event)
{
    if (!nox_bot_policy_event_pending(state, event))
        return 0;
    return state->event_object[event];
}

uint32_t nox_bot_policy_event_frame(const nox_bot_policy_state *state, nox_bot_event event)
{
    if (!nox_bot_policy_event_pending(state, event))
        return 0;
    return state->event_frame[event];
}

void nox_bot_policy_clear_event(nox_bot_policy_state *state, nox_bot_event event)
{
    uint32_t mask;

    if (!state || !nox_bot_event_valid(event))
        return;
    mask = 1u << (unsigned int)event;
    state->pending_events &= ~mask;
    state->event_object[event] = 0;
    state->event_frame[event] = 0;
}
