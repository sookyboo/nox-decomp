#include "bot_conjurer.h"
#include "bot_engine.h"

#include <stdint.h>
#include <string.h>

#define SELF 100
#define TARGET 200

#define ENCHANT_SLOWED 4
#define ENCHANT_HELD 5
#define ENCHANT_VAMPIRISM 13
#define ENCHANT_PROTECT_FIRE 17
#define ENCHANT_PROTECT_POISON 18
#define ENCHANT_PROTECT_SHOCK 20
#define ENCHANT_INFRAVISION 21
#define ENCHANT_SHOCK 22
#define ENCHANT_REFLECTIVE_SHIELD 27
#define ENCHANT_ANTI_MAGIC 29

static int self_health;
static int target_health;
static int target_max_health;
static int self_mana;
static uint32_t self_buffs;
static uint32_t target_buffs;
static int target_visible;
static int ctf_mode;
static int cast_calls;
static int cast_kind;
static int cast_target;
static float cast_x;
static float cast_y;
static char cast_name[64];
static int red_potion_calls;
static int blue_potion_calls;
static const char *available_loot_type;
static int pickup_calls;
static int equip_weapon_calls;
static int equip_armor_calls;
static int ctf_walk_own_flag_calls;
static int ctf_attack_or_defend_calls;
static int owned_pixies;

static uint32_t buff_mask(int buff)
{
    return 1u << (unsigned int)buff;
}

uint32_t nox_bot_engine_fps(void)
{
    return 30;
}

int nox_bot_engine_health(int object)
{
    if (object == SELF)
        return self_health;
    if (object == TARGET)
        return target_health;
    return 0;
}

int nox_bot_engine_max_health(int object)
{
    if (object == TARGET)
        return target_max_health;
    return object == SELF ? 100 : 0;
}

int nox_bot_engine_mana(int object)
{
    return object == SELF ? self_mana : 0;
}

int nox_bot_engine_mana_add(int object, int amount)
{
    if (object != SELF || amount < 0)
        return 0;
    self_mana += amount;
    return 1;
}

int nox_bot_engine_mana_sub(int object, int amount)
{
    if (object != SELF || amount < 0 || self_mana < amount)
        return 0;
    self_mana -= amount;
    return 1;
}

int nox_bot_engine_has_buff(int object, int buff)
{
    uint32_t mask = buff_mask(buff);

    if (object == SELF)
        return (self_buffs & mask) != 0;
    if (object == TARGET)
        return (target_buffs & mask) != 0;
    return 0;
}

int nox_bot_engine_can_interact(int self, int other)
{
    return self == SELF && other == TARGET && target_visible;
}

void nox_bot_engine_position(int object, float *x, float *y)
{
    if (x)
        *x = object == TARGET ? 30.0f : 10.0f;
    if (y)
        *y = object == TARGET ? 40.0f : 20.0f;
}

static void record_cast(const char *name, int kind, int target, float x, float y)
{
    ++cast_calls;
    cast_kind = kind;
    cast_target = target;
    cast_x = x;
    cast_y = y;
    strncpy(cast_name, name, sizeof(cast_name) - 1);
    cast_name[sizeof(cast_name) - 1] = 0;
}

int nox_bot_engine_cast_script_self(int object, const char *name)
{
    if (object != SELF)
        return 0;
    record_cast(name, 1, SELF, 10.0f, 20.0f);
    if (strcmp(name, "VAMPIRISM") == 0)
        self_buffs |= buff_mask(ENCHANT_VAMPIRISM);
    else if (strcmp(name, "INFRAVISION") == 0)
        self_buffs |= buff_mask(ENCHANT_INFRAVISION);
    else if (strcmp(name, "PROTECTION_FROM_ELECTRICITY") == 0)
        self_buffs |= buff_mask(ENCHANT_PROTECT_SHOCK);
    else if (strcmp(name, "PROTECTION_FROM_FIRE") == 0)
        self_buffs |= buff_mask(ENCHANT_PROTECT_FIRE);
    else if (strcmp(name, "PROTECTION_FROM_POISON") == 0)
        self_buffs |= buff_mask(ENCHANT_PROTECT_POISON);
    else if (strcmp(name, "PIXIE_SWARM") == 0)
        owned_pixies = 1;
    return 1;
}

int nox_bot_engine_cast_script_object(int object, const char *name, int target)
{
    if (object != SELF || target != TARGET)
        return 0;
    record_cast(name, 2, target, 30.0f, 40.0f);
    if (strcmp(name, "SLOW") == 0)
        target_buffs |= buff_mask(ENCHANT_SLOWED);
    else if (strcmp(name, "STUN") == 0)
        target_buffs |= buff_mask(ENCHANT_HELD);
    return 1;
}

int nox_bot_engine_cast_script_position(int object, const char *name, float x, float y)
{
    if (object != SELF)
        return 0;
    record_cast(name, 3, 0, x, y);
    return 1;
}

int nox_bot_engine_use_inventory_potion(int object, const char *name)
{
    if (object != SELF)
        return 0;
    if (strcmp(name, "RedPotion") == 0) {
        ++red_potion_calls;
        self_health += 50;
        return 1;
    }
    if (strcmp(name, "BluePotion") == 0) {
        ++blue_potion_calls;
        self_mana += 50;
        return 1;
    }
    return 0;
}

int nox_bot_engine_is_ctf(void)
{
    return ctf_mode;
}

int nox_bot_engine_find_nearest_visible_type(
    int object, const char *type_name, float max_distance)
{
    (void)max_distance;
    if (object != SELF || !available_loot_type)
        return 0;
    return strcmp(type_name, available_loot_type) == 0 ? 700 : 0;
}

int nox_bot_engine_pickup_item(int object, int item)
{
    if (object != SELF || item != 700)
        return 0;
    ++pickup_calls;
    return 1;
}

int nox_bot_engine_equip_weapon(int object, int item)
{
    if (object != SELF || !item)
        return 0;
    ++equip_weapon_calls;
    return 1;
}

int nox_bot_engine_equip_armor(int object, int item)
{
    if (object != SELF || !item)
        return 0;
    ++equip_armor_calls;
    return 1;
}

int nox_bot_engine_owned_type_count(int object, const char *type_name)
{
    if (object != SELF || strcmp(type_name, "Pixie") != 0)
        return 0;
    return owned_pixies;
}

void nox_bot_team_ctf_walk_to_own_flag(int object)
{
    if (object == SELF)
        ++ctf_walk_own_flag_calls;
}

int nox_bot_team_ctf_attack_or_defend(int object)
{
    if (object != SELF)
        return 0;
    ++ctf_attack_or_defend_calls;
    return 1;
}

static nox_bot_policy_state *reset_state(nox_bot_difficulty difficulty, uint32_t frame)
{
    nox_bot_policy_state *state;

    self_health = 100;
    target_health = 100;
    target_max_health = 100;
    self_mana = 125;
    self_buffs = 0;
    target_buffs = 0;
    target_visible = 1;
    ctf_mode = 0;
    cast_calls = 0;
    cast_kind = 0;
    cast_target = 0;
    cast_x = 0.0f;
    cast_y = 0.0f;
    cast_name[0] = 0;
    red_potion_calls = 0;
    blue_potion_calls = 0;
    available_loot_type = 0;
    pickup_calls = 0;
    equip_weapon_calls = 0;
    equip_armor_calls = 0;
    ctf_walk_own_flag_calls = 0;
    ctf_attack_or_defend_calls = 0;
    owned_pixies = 1;
    nox_bot_policy_reset_all();
    if (!nox_bot_policy_activate(0, difficulty, frame))
        return 0;
    state = nox_bot_policy_get(0);
    state->native_object = SELF;
    return state;
}

static int test_enemy_sighted_force_of_nature(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_NORMAL, 100);

    if (!state)
        return 1;
    nox_bot_policy_record_event(state, NOX_BOT_EVENT_ENEMY_SIGHTED, TARGET, 100);
    nox_bot_conjurer_update(SELF, state, 100);
    if (!state->conjurer.pending_spell || cast_calls || state->conjurer.target != TARGET)
        return 2;
    nox_bot_conjurer_update(SELF, state, 129);
    if (cast_calls)
        return 3;
    nox_bot_conjurer_update(SELF, state, 130);
    if (cast_calls != 1 || strcmp(cast_name, "FORCE_OF_NATURE") != 0 ||
        cast_kind != 3 || cast_x != 30.0f || cast_y != 40.0f || self_mana != 65)
        return 4;
    if (state->conjurer.global_ready_frame != 133 ||
        state->conjurer.force_of_nature_ready_frame != 280)
        return 5;
    return 0;
}

static int test_held_target_meteor_priority(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_HARDCORE, 200);

    if (!state)
        return 10;
    state->conjurer.target = TARGET;
    target_buffs = buff_mask(ENCHANT_HELD);
    nox_bot_conjurer_update(SELF, state, 200);
    nox_bot_conjurer_update(SELF, state, 200);
    if (cast_calls != 1 || strcmp(cast_name, "METEOR") != 0 ||
        cast_kind != 3 || cast_x != 30.0f || cast_y != 40.0f || self_mana != 95)
        return 11;
    if (state->conjurer.meteor_ready_frame != 350)
        return 12;
    return 0;
}

static int test_non_ctf_stun_and_ctf_slow(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_HARDCORE, 300);

    if (!state)
        return 20;
    state->conjurer.target = TARGET;
    nox_bot_conjurer_update(SELF, state, 300);
    nox_bot_conjurer_update(SELF, state, 300);
    if (cast_calls != 1 || strcmp(cast_name, "STUN") != 0 ||
        cast_kind != 2 || !(target_buffs & buff_mask(ENCHANT_HELD)))
        return 21;

    state = reset_state(NOX_BOT_DIFFICULTY_HARDCORE, 310);
    if (!state)
        return 22;
    state->conjurer.target = TARGET;
    ctf_mode = 1;
    nox_bot_conjurer_update(SELF, state, 310);
    nox_bot_conjurer_update(SELF, state, 310);
    if (cast_calls != 1 || strcmp(cast_name, "SLOW") != 0 ||
        cast_kind != 2 || !(target_buffs & buff_mask(ENCHANT_SLOWED)))
        return 23;
    return 0;
}

static int test_lesser_heal_precedes_offense(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_HARDCORE, 400);

    if (!state)
        return 30;
    state->conjurer.target = TARGET;
    self_health = 60;
    target_buffs = buff_mask(ENCHANT_HELD);
    nox_bot_conjurer_update(SELF, state, 400);
    nox_bot_conjurer_update(SELF, state, 400);
    if (cast_calls != 1 || strcmp(cast_name, "LESSER_HEAL") != 0 ||
        cast_kind != 1 || self_mana != 95)
        return 31;
    return 0;
}

static int test_hidden_buff_priority(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_HARDCORE, 500);

    if (!state)
        return 40;
    target_visible = 0;
    state->conjurer.target = TARGET;
    nox_bot_conjurer_update(SELF, state, 500);
    nox_bot_conjurer_update(SELF, state, 500);
    if (cast_calls != 1 || strcmp(cast_name, "VAMPIRISM") != 0 ||
        !(self_buffs & buff_mask(ENCHANT_VAMPIRISM)) || self_mana != 105)
        return 41;

    state->conjurer.global_ready_frame = 0;
    nox_bot_conjurer_update(SELF, state, 501);
    nox_bot_conjurer_update(SELF, state, 501);
    if (cast_calls != 2 || strcmp(cast_name, "PROTECTION_FROM_ELECTRICITY") != 0 ||
        !(self_buffs & buff_mask(ENCHANT_PROTECT_SHOCK)) || self_mana != 75)
        return 42;
    return 0;
}

static int test_looking_for_enemy_infravision(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_HARD, 600);

    if (!state)
        return 50;
    nox_bot_policy_record_event(state, NOX_BOT_EVENT_LOOKING_FOR_ENEMY, 0, 600);
    nox_bot_conjurer_update(SELF, state, 600);
    nox_bot_conjurer_update(SELF, state, 615);
    if (cast_calls != 1 || strcmp(cast_name, "INFRAVISION") != 0 ||
        !(self_buffs & buff_mask(ENCHANT_INFRAVISION)) || self_mana != 95)
        return 51;
    return 0;
}

static int test_potions_and_mana_cap(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_NORMAL, 700);

    if (!state)
        return 60;
    state->conjurer.target = TARGET;
    self_health = 25;
    self_mana = 100;
    nox_bot_conjurer_update(SELF, state, 700);
    if (red_potion_calls != 1 || blue_potion_calls != 1 ||
        self_health != 75 || self_mana != 125)
        return 61;
    return 0;
}

static int test_passive_mana_regen(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_NORMAL, 800);

    if (!state)
        return 70;
    self_mana = 124;
    target_visible = 0;
    self_buffs = buff_mask(ENCHANT_VAMPIRISM) | buff_mask(ENCHANT_PROTECT_SHOCK) |
        buff_mask(ENCHANT_PROTECT_FIRE) | buff_mask(ENCHANT_PROTECT_POISON);
    nox_bot_conjurer_update(SELF, state, 800);
    if (self_mana != 124 || state->conjurer.next_mana_regen_frame != 860)
        return 71;
    nox_bot_conjurer_update(SELF, state, 859);
    if (self_mana != 124)
        return 72;
    nox_bot_conjurer_update(SELF, state, 860);
    if (self_mana != 125 || state->conjurer.next_mana_regen_frame != 920)
        return 73;
    return 0;
}

static int test_pixie_swarm_uses_native_owned_pixie_state(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_HARDCORE, 825);

    if (!state)
        return 74;
    owned_pixies = 0;
    target_visible = 0;
    self_buffs = buff_mask(ENCHANT_VAMPIRISM) | buff_mask(ENCHANT_PROTECT_SHOCK) |
        buff_mask(ENCHANT_PROTECT_FIRE) | buff_mask(ENCHANT_PROTECT_POISON);
    nox_bot_conjurer_update(SELF, state, 825);
    if (!state->conjurer.pending_spell || cast_calls)
        return 85;
    nox_bot_conjurer_update(SELF, state, 825);
    if (cast_calls != 1 || strcmp(cast_name, "PIXIE_SWARM") != 0 ||
        cast_kind != 1 || self_mana != 95 || owned_pixies != 1)
        return 86;

    state->conjurer.global_ready_frame = 0;
    nox_bot_conjurer_update(SELF, state, 826);
    if (state->conjurer.pending_spell || cast_calls != 1)
        return 87;
    return 0;
}

static int test_loot_scan(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_HARDCORE, 850);

    if (!state)
        return 75;
    self_buffs = buff_mask(ENCHANT_ANTI_MAGIC);
    available_loot_type = "CrossBow";
    nox_bot_conjurer_update(SELF, state, 850);
    if (pickup_calls != 1 || equip_weapon_calls != 1 ||
        state->conjurer.next_loot_scan_frame != 865)
        return 76;
    nox_bot_conjurer_update(SELF, state, 864);
    if (pickup_calls != 1)
        return 77;

    state = reset_state(NOX_BOT_DIFFICULTY_HARDCORE, 860);
    if (!state)
        return 78;
    self_buffs = buff_mask(ENCHANT_ANTI_MAGIC);
    available_loot_type = "Quiver";
    nox_bot_conjurer_update(SELF, state, 860);
    if (pickup_calls != 1 || equip_weapon_calls || equip_armor_calls)
        return 79;
    return 0;
}

static int test_ctf_objective_events(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_HARDCORE, 875);

    if (!state)
        return 82;
    ctf_mode = 1;
    self_buffs = buff_mask(ENCHANT_ANTI_MAGIC);
    nox_bot_policy_record_event(state, NOX_BOT_EVENT_LOST_SIGHT, TARGET, 875);
    nox_bot_conjurer_update(SELF, state, 875);
    if (ctf_walk_own_flag_calls != 1 ||
        nox_bot_policy_event_pending(state, NOX_BOT_EVENT_LOST_SIGHT))
        return 83;
    nox_bot_policy_record_event(state, NOX_BOT_EVENT_END_OF_WAYPOINT, 0, 876);
    nox_bot_conjurer_update(SELF, state, 876);
    if (ctf_attack_or_defend_calls != 1 ||
        nox_bot_policy_event_pending(state, NOX_BOT_EVENT_END_OF_WAYPOINT))
        return 84;
    return 0;
}

static int test_death_clears_conjurer_life_state(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_NORMAL, 900);

    if (!state)
        return 80;
    state->conjurer.target = TARGET;
    state->conjurer.pending_spell = 1;
    state->conjurer.meteor_ready_frame = 999;
    nox_bot_policy_record_event(state, NOX_BOT_EVENT_DEATH, 0, 900);
    nox_bot_conjurer_update(SELF, state, 900);
    if (state->conjurer.target || state->conjurer.pending_spell ||
        state->conjurer.meteor_ready_frame ||
        nox_bot_policy_event_pending(state, NOX_BOT_EVENT_DEATH))
        return 81;
    return 0;
}

int main(void)
{
    int result;

    result = test_enemy_sighted_force_of_nature();
    if (result)
        return result;
    result = test_held_target_meteor_priority();
    if (result)
        return result;
    result = test_non_ctf_stun_and_ctf_slow();
    if (result)
        return result;
    result = test_lesser_heal_precedes_offense();
    if (result)
        return result;
    result = test_hidden_buff_priority();
    if (result)
        return result;
    result = test_looking_for_enemy_infravision();
    if (result)
        return result;
    result = test_potions_and_mana_cap();
    if (result)
        return result;
    result = test_passive_mana_regen();
    if (result)
        return result;
    result = test_pixie_swarm_uses_native_owned_pixie_state();
    if (result)
        return result;
    result = test_loot_scan();
    if (result)
        return result;
    result = test_ctf_objective_events();
    if (result)
        return result;
    return test_death_clears_conjurer_life_state();
}
