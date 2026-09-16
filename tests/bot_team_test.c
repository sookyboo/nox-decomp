#include "bot_team.h"

#define SELF 100
#define OWN_FLAG 200
#define ENEMY_FLAG 300
#define OWN_CARRIER 400
#define ENEMY_CARRIER 500

static int ctf_mode;
static int carrying_flag;
static int own_flag;
static int enemy_flag;
static int own_carrier;
static int enemy_carrier;
static int own_flag_at_home;
static int walk_calls;
static int guard_calls;
static int aggression_calls;
static float last_x;
static float last_y;
static float last_radius;
static float last_aggression;

int nox_bot_engine_is_ctf(void)
{
    return ctf_mode;
}

int nox_bot_engine_carrying_ctf_flag(int object)
{
    return object == SELF && carrying_flag;
}

int nox_bot_engine_ctf_flag_world(int object, int own_team)
{
    if (object != SELF)
        return 0;
    return own_team ? own_flag : enemy_flag;
}

int nox_bot_engine_ctf_flag_carrier(int object, int own_team)
{
    if (object != SELF)
        return 0;
    return own_team ? own_carrier : enemy_carrier;
}

int nox_bot_engine_ctf_flag_at_home(int flag)
{
    return flag == OWN_FLAG && own_flag_at_home;
}

void nox_bot_engine_position(int object, float *x, float *y)
{
    float px = 0.0f;
    float py = 0.0f;

    switch (object) {
    case OWN_FLAG: px = 10.0f; py = 11.0f; break;
    case ENEMY_FLAG: px = 20.0f; py = 21.0f; break;
    case OWN_CARRIER: px = 30.0f; py = 31.0f; break;
    case ENEMY_CARRIER: px = 40.0f; py = 41.0f; break;
    default: break;
    }
    if (x)
        *x = px;
    if (y)
        *y = py;
}

int nox_bot_engine_set_aggression(int object, float aggression)
{
    if (object != SELF)
        return 0;
    ++aggression_calls;
    last_aggression = aggression;
    return 1;
}

void nox_bot_engine_walk_to(int object, float x, float y)
{
    if (object != SELF)
        return;
    ++walk_calls;
    last_x = x;
    last_y = y;
}

void nox_bot_engine_guard_position(int object, float x, float y, float radius)
{
    if (object != SELF)
        return;
    ++guard_calls;
    last_x = x;
    last_y = y;
    last_radius = radius;
}

static void reset_state(void)
{
    ctf_mode = 1;
    carrying_flag = 0;
    own_flag = OWN_FLAG;
    enemy_flag = ENEMY_FLAG;
    own_carrier = 0;
    enemy_carrier = 0;
    own_flag_at_home = 1;
    walk_calls = 0;
    guard_calls = 0;
    aggression_calls = 0;
    last_x = 0.0f;
    last_y = 0.0f;
    last_radius = 0.0f;
    last_aggression = 0.0f;
}


static int test_ctf_tank_is_enemy_flag_carrier(void)
{
    reset_state();
    if (nox_bot_team_is_ctf_tank(SELF))
        return 1;
    carrying_flag = 1;
    if (!nox_bot_team_is_ctf_tank(SELF))
        return 2;
    ctf_mode = 0;
    if (nox_bot_team_is_ctf_tank(SELF))
        return 3;
    return 0;
}

static int test_attack_enemy_flag(void)
{
    reset_state();
    if (!nox_bot_team_ctf_attack_or_defend(SELF))
        return 1;
    if (walk_calls != 1 || guard_calls || last_x != 20.0f || last_y != 21.0f)
        return 2;
    if (aggression_calls != 1 || last_aggression != 0.83f)
        return 3;
    return 0;
}

static int test_flag_carrier_guards_own_flag_target(void)
{
    reset_state();
    carrying_flag = 1;
    own_flag = 0;
    own_carrier = OWN_CARRIER;
    if (!nox_bot_team_ctf_attack_or_defend(SELF))
        return 10;
    if (guard_calls != 1 || walk_calls || last_x != 30.0f || last_y != 31.0f ||
        last_radius != 20.0f)
        return 11;
    if (last_aggression != 0.16f)
        return 12;
    return 0;
}

static int test_both_flags_carried_pursues_own_carrier(void)
{
    reset_state();
    own_flag = 0;
    enemy_flag = 0;
    own_carrier = OWN_CARRIER;
    enemy_carrier = ENEMY_CARRIER;
    if (!nox_bot_team_ctf_attack_or_defend(SELF))
        return 20;
    if (walk_calls != 1 || last_x != 30.0f || last_y != 31.0f)
        return 21;
    return 0;
}

static int test_walk_to_dropped_own_flag_then_fallback(void)
{
    reset_state();
    own_flag_at_home = 0;
    nox_bot_team_ctf_walk_to_own_flag(SELF);
    if (walk_calls != 1 || last_x != 10.0f || last_y != 11.0f ||
        last_aggression != 0.16f)
        return 30;

    reset_state();
    own_flag_at_home = 1;
    nox_bot_team_ctf_walk_to_own_flag(SELF);
    if (walk_calls != 1 || last_x != 20.0f || last_y != 21.0f ||
        last_aggression != 0.83f)
        return 31;
    return 0;
}

int main(void)
{
    int rc;

    rc = test_ctf_tank_is_enemy_flag_carrier();
    if (rc) return rc;
    rc = test_attack_enemy_flag();
    if (rc) return rc;
    rc = test_flag_carrier_guards_own_flag_target();
    if (rc) return rc;
    rc = test_both_flags_carried_pursues_own_carrier();
    if (rc) return rc;
    rc = test_walk_to_dropped_own_flag_then_fallback();
    if (rc) return rc;
    return 0;
}
