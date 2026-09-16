#ifndef NOX_BOT_RUNTIME_H
#define NOX_BOT_RUNTIME_H

#include "bot_engine.h"
#include "bot_policy.h"

/*
 * Runtime glue for the original Nox player-monster bot path.
 *
 * Existing-player attach/detach is the verified lifecycle subset. The spawn and
 * clear APIs are an explicitly experimental, traced attempt that reuses the
 * normal native join/leave owners without a remote socket. See the bot lifecycle
 * documentation for the assumptions that still require hosted-game logs.
 */
int nox_bot_runtime_attach_existing_player(int object, nox_bot_difficulty difficulty);
int nox_bot_runtime_detach_existing_player(int object);
int nox_bot_runtime_set_difficulty(int object, nox_bot_difficulty difficulty);
int nox_bot_runtime_spawn_attempt(
    nox_bot_spawn_team team, int player_class, nox_bot_difficulty difficulty, int *spawned_slot);
int nox_bot_runtime_is_server_created(int slot);
int nox_bot_runtime_clear_server_created(int slot);
int nox_bot_runtime_clear_all_server_created(void);
void nox_bot_runtime_note_player_removed(int slot, int object);
int nox_bot_runtime_sync_native_player_bot(int object);
void nox_bot_runtime_forget_native_player_bot(int object);
void nox_bot_runtime_event(int object, nox_bot_event event, int event_object);
void nox_bot_runtime_clear_life_state(int object);
int nox_bot_runtime_preserve_player_attack_state(int object);
void nox_bot_runtime_update(int object);

#endif
