#include "bot_team.h"

#include "bot_engine.h"

#define NOX_BOT_TEAM_DEFAULT_AGGRESSION 0.83f
#define NOX_BOT_TEAM_GUARD_AGGRESSION 0.16f
#define NOX_BOT_TEAM_CTF_GUARD_RADIUS 20.0f


int nox_bot_team_is_ctf_tank(int object)
{
    /* Bot-Script assigns TeamTank to the unit that picks up the enemy flag.
     * Native CTF already stores that flag in the carrier's player inventory. */
    return object && nox_bot_engine_is_ctf() &&
        nox_bot_engine_carrying_ctf_flag(object);
}

int nox_bot_team_ctf_attack_or_defend(int object)
{
    float x;
    float y;
    int enemy_flag;
    int enemy_target;
    int own_flag;
    int own_target;

    if (!object || !nox_bot_engine_is_ctf())
        return 0;

    own_flag = nox_bot_engine_ctf_flag_world(object, 1);
    enemy_flag = nox_bot_engine_ctf_flag_world(object, 0);
    own_target = own_flag ? own_flag : nox_bot_engine_ctf_flag_carrier(object, 1);
    enemy_target = enemy_flag ? enemy_flag : nox_bot_engine_ctf_flag_carrier(object, 0);

    /* Bot-Script TeamTank: an enemy-flag carrier guards TeamBase, whose
     * position follows the current own-flag object/carrier in team.go. */
    if (nox_bot_team_is_ctf_tank(object)) {
        if (!own_target)
            return 0;
        nox_bot_engine_position(own_target, &x, &y);
        nox_bot_engine_set_aggression(object, NOX_BOT_TEAM_GUARD_AGGRESSION);
        nox_bot_engine_guard_position(object, x, y, NOX_BOT_TEAM_CTF_GUARD_RADIUS);
        return 1;
    }

    /* Own flag present: attack the enemy flag, or escort its native carrier. */
    if (own_flag) {
        if (!enemy_target)
            return 0;
        nox_bot_engine_position(enemy_target, &x, &y);
        nox_bot_engine_set_aggression(object, NOX_BOT_TEAM_DEFAULT_AGGRESSION);
        nox_bot_engine_walk_to(object, x, y);
        return 1;
    }

    /* Both flags carried: pursue the native carrier of our own flag. */
    if (!enemy_flag && own_target) {
        nox_bot_engine_position(own_target, &x, &y);
        nox_bot_engine_set_aggression(object, NOX_BOT_TEAM_DEFAULT_AGGRESSION);
        nox_bot_engine_walk_to(object, x, y);
        return 1;
    }
    return 0;
}

void nox_bot_team_ctf_walk_to_own_flag(int object)
{
    float x;
    float y;
    int own_flag;

    if (!object || !nox_bot_engine_is_ctf())
        return;

    own_flag = nox_bot_engine_ctf_flag_world(object, 1);
    if (own_flag && !nox_bot_engine_ctf_flag_at_home(own_flag)) {
        nox_bot_engine_position(own_flag, &x, &y);
        nox_bot_engine_set_aggression(object, NOX_BOT_TEAM_GUARD_AGGRESSION);
        nox_bot_engine_walk_to(object, x, y);
        return;
    }
    nox_bot_team_ctf_attack_or_defend(object);
}
