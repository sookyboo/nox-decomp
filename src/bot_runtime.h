#ifndef NOX_BOT_RUNTIME_H
#define NOX_BOT_RUNTIME_H

#include "bot_policy.h"

/*
 * Runtime glue for the original Nox player-monster bot path.
 *
 * Creation of a new player slot without a network client is deliberately not
 * handled here yet. Attach/detach only operate on an already-created player.
 */
int nox_bot_runtime_attach_existing_player(int object, nox_bot_difficulty difficulty);
int nox_bot_runtime_detach_existing_player(int object);
int nox_bot_runtime_sync_native_player_bot(int object);
void nox_bot_runtime_forget_native_player_bot(int object);
void nox_bot_runtime_event(int object, nox_bot_event event, int event_object);
void nox_bot_runtime_clear_life_state(int object);
int nox_bot_runtime_preserve_player_attack_state(int object);
void nox_bot_runtime_update(int object);

#endif
