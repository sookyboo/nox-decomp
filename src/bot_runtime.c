#include "bot_runtime.h"

#include "bot_engine.h"
#include "bot_chat.h"
#include "bot_conjurer.h"
#include "bot_trace.h"
#include "bot_warrior.h"
#include "bot_wizard.h"

#include <wchar.h>

static unsigned char nox_bot_server_created[NOX_BOT_PLAYER_SLOTS];
static int nox_bot_server_created_object[NOX_BOT_PLAYER_SLOTS];

static int nox_bot_runtime_handle_owned_bomber_event(
    int object, nox_bot_event event, int event_object)
{
    nox_bot_policy_state *state;
    int owner;
    int slot;
    int target;

    (void)event_object;
    if (!nox_bot_engine_is_object_type(object, "Bomber"))
        return 0;
    if (event != NOX_BOT_EVENT_ENEMY_SIGHTED &&
        event != NOX_BOT_EVENT_ENEMY_HEARD &&
        event != NOX_BOT_EVENT_LOST_SIGHT)
        return 0;

    owner = nox_bot_engine_owner_player(object);
    if (!owner || nox_bot_engine_player_class(owner) != 2)
        return 0;
    slot = nox_bot_engine_player_slot(owner);
    if (slot < 0)
        return 0;
    state = nox_bot_policy_get(slot);
    if (!state || !state->active || state->native_object != owner)
        return 0;

    /* Bot-Script's Bomber callbacks intentionally use the Conjurer's current
     * target rather than the event caller. Lost Enemy returns the Bomber to
     * Follow(con.unit). Native action helpers remain authoritative for the
     * resulting monster action stack. */
    if (event == NOX_BOT_EVENT_LOST_SIGHT) {
        nox_bot_engine_follow_target(object, owner);
        return 1;
    }
    target = state->conjurer.target;
    if (target)
        nox_bot_engine_attack_target(object, target);
    return 1;
}

static void nox_bot_runtime_spawn_name(
    wchar_t *out, int out_count, int player_class)
{
    const wchar_t *name;
    int i;

    if (!out || out_count <= 0)
        return;
    if (nox_bot_engine_teams_enabled()) {
        name = player_class == 0 ? L"Warrior Bot" :
            player_class == 1 ? L"Wizard Bot" : L"Conjurer Bot";
    } else {
        name = player_class == 0 ? L"Lance" :
            player_class == 1 ? L"Kirik" : L"Horst";
    }
    for (i = 0; name[i] && i + 1 < out_count; ++i)
        out[i] = name[i];
    out[i] = 0;
}

int nox_bot_runtime_attach_existing_player(int object, nox_bot_difficulty difficulty)
{
    int slot;

    nox_bot_tracef("bot", "attach-begin", "object=0x%08x difficulty=%d", object, (int)difficulty);
    if (!nox_bot_engine_enable_existing_player_bot(object)) {
        nox_bot_tracef("bot", "attach-failed", "object=0x%08x reason=engine-enable", object);
        return 0;
    }
    slot = nox_bot_engine_player_slot(object);
    if (slot < 0 || !nox_bot_policy_activate(slot, difficulty, nox_bot_engine_frame())) {
        nox_bot_tracef("bot", "attach-failed",
            "object=0x%08x slot=%d reason=policy-activate", object, slot);
        nox_bot_engine_disable_existing_player_bot(object);
        return 0;
    }
    nox_bot_policy_get(slot)->native_object = object;
    nox_bot_tracef("bot", "attach-ready",
        "slot=%d object=0x%08x class=%d frame=%u difficulty=%d", slot, object,
        nox_bot_engine_player_class(object), nox_bot_engine_frame(), (int)difficulty);
    return 1;
}

int nox_bot_runtime_detach_existing_player(int object)
{
    int slot = nox_bot_engine_player_slot(object);
    nox_bot_policy_state *state;

    nox_bot_tracef("bot", "detach-begin", "slot=%d object=0x%08x", slot, object);
    if (slot < 0 || !nox_bot_engine_disable_existing_player_bot(object)) {
        nox_bot_tracef("bot", "detach-failed", "slot=%d object=0x%08x", slot, object);
        return 0;
    }
    state = nox_bot_policy_get(slot);
    nox_bot_chat_forget_object(object);
    if (state && state->native_object == object)
        nox_bot_policy_deactivate(slot);
    nox_bot_tracef("bot", "detach-ready", "slot=%d object=0x%08x frame=%u",
        slot, object, nox_bot_engine_frame());
    return 1;
}

int nox_bot_runtime_set_difficulty(int object, nox_bot_difficulty difficulty)
{
    int slot = nox_bot_engine_player_slot(object);
    nox_bot_policy_state *state;

    if (slot < 0 || !nox_bot_engine_is_native_player_bot(object))
        return 0;
    state = nox_bot_policy_get(slot);
    if (!state || !state->active || state->native_object != object)
        return 0;
    return nox_bot_policy_set_difficulty(slot, difficulty, nox_bot_engine_frame());
}

int nox_bot_runtime_spawn_attempt(
    nox_bot_spawn_team team, int player_class, nox_bot_difficulty difficulty, int *spawned_slot)
{
    wchar_t name[25];
    int slot;
    int object;

    if (player_class < 0 || player_class > 2)
        return 0;
    slot = nox_bot_engine_find_free_player_slot();
    if (slot < 0) {
        nox_bot_tracef("spawn", "failed", "reason=no-free-player-slot");
        return 0;
    }
    nox_bot_runtime_spawn_name(name, 25, player_class);
    nox_bot_tracef("spawn", "begin",
        "slot=%d class=%d team=%d difficulty=%d", slot, player_class, (int)team, (int)difficulty);
    object = nox_bot_engine_spawn_player_attempt(slot, player_class, team, name);
    if (!object) {
        nox_bot_tracef("spawn", "failed", "slot=%d reason=native-constructor", slot);
        return 0;
    }
    if (!nox_bot_engine_finish_spawn_transition(object)) {
        nox_bot_tracef("spawn", "rollback",
            "slot=%d object=0x%08x reason=spawn-transition", slot, object);
        nox_bot_engine_remove_player_attempt(slot, object);
        return 0;
    }
    if (!nox_bot_runtime_attach_existing_player(object, difficulty)) {
        nox_bot_tracef("spawn", "rollback",
            "slot=%d object=0x%08x reason=bot-activation", slot, object);
        nox_bot_engine_remove_player_attempt(slot, object);
        return 0;
    }
    nox_bot_server_created[slot] = 1;
    nox_bot_server_created_object[slot] = object;
    if (spawned_slot)
        *spawned_slot = slot;
    nox_bot_tracef("spawn", "ready",
        "slot=%d object=0x%08x class=%d team=%d difficulty=%d", slot, object,
        nox_bot_engine_player_class(object), (int)team, (int)difficulty);
    return 1;
}

int nox_bot_runtime_is_server_created(int slot)
{
    if (slot < 0 || slot >= NOX_BOT_PLAYER_SLOTS)
        return 0;
    return nox_bot_server_created[slot] != 0;
}

int nox_bot_runtime_clear_server_created(int slot)
{
    nox_bot_policy_state *state;
    nox_bot_difficulty difficulty = NOX_BOT_DIFFICULTY_NORMAL;
    int object;
    int result;

    if (slot < 0 || slot >= NOX_BOT_PLAYER_SLOTS || !nox_bot_server_created[slot])
        return 0;
    object = nox_bot_server_created_object[slot];
    if (!object || nox_bot_engine_player_object_by_slot(slot) != object) {
        nox_bot_tracef("spawn", "clear-rejected",
            "slot=%d expected=0x%08x actual=0x%08x reason=ownership-mismatch", slot,
            object, nox_bot_engine_player_object_by_slot(slot));
        return 0;
    }
    state = nox_bot_policy_get(slot);
    if (state && state->active && state->native_object == object)
        difficulty = state->difficulty;
    nox_bot_tracef("spawn", "clear-begin", "slot=%d object=0x%08x", slot, object);

    /* Restore the ordinary player updater before entering the normal leave
     * owner. This is conservative: the leave path was recovered for ordinary
     * players, while native player-bot AI remains runtime-owned. */
    if (nox_bot_engine_is_native_player_bot(object))
        nox_bot_runtime_detach_existing_player(object);
    else if (state && state->active && state->native_object == object)
        nox_bot_policy_deactivate(slot);

    result = nox_bot_engine_remove_player_attempt(slot, object);
    if (result) {
        nox_bot_server_created[slot] = 0;
        nox_bot_server_created_object[slot] = 0;
        nox_bot_tracef("spawn", "clear-ready", "slot=%d", slot);
        return 1;
    }

    /* A failed/partial removal is safer to keep marked as bot-owned. If the
     * object is still intact, try to restore bot control so a second clear or
     * additional trace run remains possible. */
    if (nox_bot_engine_player_object_by_slot(slot) == object)
        nox_bot_runtime_attach_existing_player(object, difficulty);
    nox_bot_tracef("spawn", "clear-failed", "slot=%d object=0x%08x", slot, object);
    return 0;
}

int nox_bot_runtime_clear_all_server_created(void)
{
    int slot;
    int cleared = 0;

    for (slot = 0; slot < NOX_BOT_PLAYER_SLOTS; ++slot) {
        if (nox_bot_server_created[slot] && nox_bot_runtime_clear_server_created(slot))
            ++cleared;
    }
    return cleared;
}

void nox_bot_runtime_note_player_removed(int slot, int object)
{
    nox_bot_policy_state *state;

    if (slot < 0 || slot >= NOX_BOT_PLAYER_SLOTS)
        return;
    state = nox_bot_policy_get(slot);
    nox_bot_chat_forget_object(object);
    if (state && state->native_object == object)
        nox_bot_policy_deactivate(slot);
    if (nox_bot_server_created[slot] && nox_bot_server_created_object[slot] == object) {
        nox_bot_server_created[slot] = 0;
        nox_bot_server_created_object[slot] = 0;
        nox_bot_tracef("spawn", "ownership-released", "slot=%d object=0x%08x", slot, object);
    }
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
    nox_bot_chat_forget_object(object);
    if (state && state->native_object == object)
        nox_bot_policy_deactivate(slot);
}

void nox_bot_runtime_event(int object, nox_bot_event event, int event_object)
{
    uint32_t frame;
    int slot;
    nox_bot_policy_state *state;

    if (nox_bot_runtime_handle_owned_bomber_event(object, event, event_object))
        return;
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
    nox_bot_chat_forget_object(object);
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
    uint32_t frame;
    int slot;
    nox_bot_policy_state *state;

    if (!nox_bot_runtime_sync_native_player_bot(object))
        return;
    slot = nox_bot_engine_player_slot(object);
    state = nox_bot_policy_get(slot);
    if (!state || !state->active)
        return;
    frame = nox_bot_engine_frame();
    nox_bot_chat_update(object, frame);
    switch (nox_bot_engine_player_class(object)) {
    case 0:
        nox_bot_warrior_update(object, state, frame);
        break;
    case 1:
        nox_bot_wizard_update(object, state, frame);
        break;
    case 2:
        nox_bot_conjurer_update(object, state, frame);
        break;
    default:
        break;
    }
}
