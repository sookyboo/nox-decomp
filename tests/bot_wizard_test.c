#include "bot_wizard.h"
#include "bot_engine.h"

#include <stdint.h>
#include <string.h>

#define SELF 100
#define TARGET 200

#define ENCHANT_INVISIBLE 0
#define ENCHANT_SLOWED 4
#define ENCHANT_HELD 5
#define ENCHANT_HASTED 9
#define ENCHANT_PROTECT_FIRE 17
#define ENCHANT_PROTECT_SHOCK 20
#define ENCHANT_SHOCK 22
#define ENCHANT_SHIELD 26
#define ENCHANT_REFLECTIVE_SHIELD 27
#define ENCHANT_ANTI_MAGIC 29

static int self_health;
static int target_health;
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
static char cast_name[48];
static int red_potion_calls;
static int blue_potion_calls;
static const char *available_loot_type;
static int pickup_calls;
static int equip_weapon_calls;
static int equip_armor_calls;
static int firestorm_item;
static int force_wand_item;
static int ctf_walk_own_flag_calls;
static int ctf_attack_or_defend_calls;

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
    if (strcmp(name, "SHIELD") == 0)
        self_buffs |= buff_mask(ENCHANT_SHIELD);
    else if (strcmp(name, "HASTE") == 0)
        self_buffs |= buff_mask(ENCHANT_HASTED);
    else if (strcmp(name, "SHOCK") == 0)
        self_buffs |= buff_mask(ENCHANT_SHOCK);
    else if (strcmp(name, "PROTECTION_FROM_ELECTRICITY") == 0)
        self_buffs |= buff_mask(ENCHANT_PROTECT_SHOCK);
    else if (strcmp(name, "PROTECTION_FROM_FIRE") == 0)
        self_buffs |= buff_mask(ENCHANT_PROTECT_FIRE);
    else if (strcmp(name, "INVISIBILITY") == 0)
        self_buffs |= buff_mask(ENCHANT_INVISIBLE);
    return 1;
}

int nox_bot_engine_cast_script_object(int object, const char *name, int target)
{
    if (object != SELF || target != TARGET)
        return 0;
    record_cast(name, 2, target, 30.0f, 40.0f);
    if (strcmp(name, "SLOW") == 0)
        target_buffs |= buff_mask(ENCHANT_SLOWED);
    return 1;
}

int nox_bot_engine_cast_script_position(int object, const char *name, float x, float y)
{
    if (object != SELF)
        return 0;
    record_cast(name, 3, 0, x, y);
    return 1;
}

void nox_bot_engine_face_position(int object, float x, float y)
{
    (void)object;
    (void)x;
    (void)y;
}

void nox_bot_engine_face_target(int object, int target)
{
    (void)object;
    (void)target;
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

int nox_bot_engine_inventory_item(int object, const char *type_name)
{
    if (object != SELF)
        return 0;
    if (strcmp(type_name, "FireStormWand") == 0)
        return firestorm_item;
    if (strcmp(type_name, "ForceWand") == 0)
        return force_wand_item;
    return 0;
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

    self_health = 150;
    target_health = 100;
    self_mana = 150;
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
    firestorm_item = 0;
    force_wand_item = 0;
    ctf_walk_own_flag_calls = 0;
    ctf_attack_or_defend_calls = 0;
    nox_bot_policy_reset_all();
    if (!nox_bot_policy_activate(0, difficulty, frame))
        return 0;
    state = nox_bot_policy_get(0);
    state->native_object = SELF;
    return state;
}

static int test_enemy_sighted_slow_then_death_ray(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_NORMAL, 100);

    if (!state)
        return 1;
    nox_bot_policy_record_event(state, NOX_BOT_EVENT_ENEMY_SIGHTED, TARGET, 100);
    nox_bot_wizard_update(SELF, state, 100);
    if (!state->wizard.pending_spell || cast_calls)
        return 2;
    nox_bot_wizard_update(SELF, state, 129);
    if (cast_calls)
        return 3;
    nox_bot_wizard_update(SELF, state, 130);
    if (cast_calls != 1 || strcmp(cast_name, "SLOW") != 0 || cast_kind != 2 ||
        cast_target != TARGET || self_mana != 140 || !(target_buffs & buff_mask(ENCHANT_SLOWED)))
        return 4;
    if (state->wizard.global_ready_frame != 133 || state->wizard.slow_ready_frame != 220)
        return 5;

    nox_bot_wizard_update(SELF, state, 133);
    if (!state->wizard.pending_spell || cast_calls != 1)
        return 6;
    nox_bot_wizard_update(SELF, state, 163);
    if (cast_calls != 2 || strcmp(cast_name, "DEATH_RAY") != 0 || cast_kind != 3 ||
        cast_x != 30.0f || cast_y != 40.0f || self_mana != 80)
        return 7;
    return 0;
}

static int test_visible_fireball_priority(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_HARDCORE, 200);

    if (!state)
        return 10;
    state->wizard.target = TARGET;
    nox_bot_wizard_update(SELF, state, 200);
    if (!state->wizard.pending_spell || cast_calls)
        return 11;
    nox_bot_wizard_update(SELF, state, 200);
    if (cast_calls != 1 || strcmp(cast_name, "FIREBALL") != 0 || cast_kind != 3 ||
        cast_x != 30.0f || cast_y != 40.0f || self_mana != 120)
        return 12;
    if (state->wizard.fireball_ready_frame != 350)
        return 13;
    return 0;
}

static int test_energy_bolt_reference_mana_quirk(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_HARDCORE, 250);

    if (!state)
        return 14;
    state->wizard.target = TARGET;
    self_mana = 11;
    target_buffs = buff_mask(ENCHANT_SLOWED);
    nox_bot_wizard_update(SELF, state, 250);
    if (!state->wizard.pending_spell || cast_calls)
        return 15;
    nox_bot_wizard_update(SELF, state, 250);
    if (cast_calls != 1 || strcmp(cast_name, "LIGHTNING") != 0 ||
        cast_kind != 2 || cast_target != TARGET || self_mana != 11 ||
        state->wizard.energy_bolt_ready_frame != 340)
        return 16;
    return 0;
}

static int test_ring_of_fire_reference_once_per_life_quirk(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_HARDCORE, 275);

    if (!state)
        return 17;
    state->wizard.target = TARGET;
    state->wizard.burn_ready_frame = 9999;
    self_mana = 60;
    target_buffs = buff_mask(ENCHANT_REFLECTIVE_SHIELD);
    nox_bot_wizard_update(SELF, state, 275);
    nox_bot_wizard_update(SELF, state, 275);
    if (cast_calls != 1 || strcmp(cast_name, "CLEANSING_FLAME") != 0 ||
        cast_kind != 1 || self_mana != 0 || !state->wizard.ring_of_fire_used)
        return 18;

    self_mana = 60;
    nox_bot_wizard_update(SELF, state, 278);
    if (cast_calls != 1 || state->wizard.pending_spell)
        return 19;
    return 0;
}

static int test_enemy_heard_hidden_target_invisibility(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_HARD, 290);

    if (!state)
        return 25;
    state->wizard.target = TARGET;
    target_visible = 0;
    nox_bot_policy_record_event(state, NOX_BOT_EVENT_ENEMY_HEARD, TARGET, 290);
    nox_bot_wizard_update(SELF, state, 290);
    if (!state->wizard.pending_spell ||
        nox_bot_policy_event_pending(state, NOX_BOT_EVENT_ENEMY_HEARD))
        return 26;
    nox_bot_wizard_update(SELF, state, 305);
    if (cast_calls != 1 || strcmp(cast_name, "INVISIBILITY") != 0 ||
        !(self_buffs & buff_mask(ENCHANT_INVISIBLE)) || self_mana != 120)
        return 27;
    return 0;
}

static int test_hidden_defensive_priority_and_ctf_invisibility_gap(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_HARDCORE, 300);

    if (!state)
        return 20;
    target_visible = 0;
    state->wizard.target = TARGET;
    self_buffs = buff_mask(ENCHANT_SHIELD) | buff_mask(ENCHANT_HASTED) | buff_mask(ENCHANT_SHOCK);
    nox_bot_wizard_update(SELF, state, 300);
    nox_bot_wizard_update(SELF, state, 300);
    if (cast_calls != 1 || strcmp(cast_name, "PROTECTION_FROM_ELECTRICITY") != 0 ||
        !(self_buffs & buff_mask(ENCHANT_PROTECT_SHOCK)))
        return 21;

    state->wizard.global_ready_frame = 0;
    state->wizard.protect_shock_ready_frame = 9999;
    self_mana = 150;
    nox_bot_wizard_update(SELF, state, 301);
    nox_bot_wizard_update(SELF, state, 301);
    if (cast_calls != 2 || strcmp(cast_name, "PROTECTION_FROM_FIRE") != 0 ||
        !(self_buffs & buff_mask(ENCHANT_PROTECT_FIRE)))
        return 22;

    state->wizard.global_ready_frame = 0;
    state->wizard.protect_fire_ready_frame = 9999;
    self_mana = 150;
    ctf_mode = 1;
    nox_bot_wizard_update(SELF, state, 302);
    if (state->wizard.pending_spell || cast_calls != 2)
        return 23;
    ctf_mode = 0;
    nox_bot_wizard_update(SELF, state, 303);
    nox_bot_wizard_update(SELF, state, 303);
    if (cast_calls != 3 || strcmp(cast_name, "INVISIBILITY") != 0 ||
        !(self_buffs & buff_mask(ENCHANT_INVISIBLE)))
        return 24;
    return 0;
}

static int test_antimagic_cancels_pending_without_spending_mana(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_HARD, 400);

    if (!state)
        return 30;
    state->wizard.target = TARGET;
    nox_bot_wizard_update(SELF, state, 400);
    if (!state->wizard.pending_spell)
        return 31;
    self_buffs |= buff_mask(ENCHANT_ANTI_MAGIC);
    nox_bot_wizard_update(SELF, state, 415);
    if (cast_calls || state->wizard.pending_spell || self_mana != 150 ||
        state->wizard.global_ready_frame != 430)
        return 32;
    return 0;
}

static int test_passive_mana_regen(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_NORMAL, 450);

    if (!state)
        return 35;
    self_mana = 149;
    target_visible = 0;
    self_buffs = buff_mask(ENCHANT_SHIELD) | buff_mask(ENCHANT_HASTED) |
        buff_mask(ENCHANT_SHOCK) | buff_mask(ENCHANT_PROTECT_SHOCK) |
        buff_mask(ENCHANT_PROTECT_FIRE) | buff_mask(ENCHANT_INVISIBLE);
    nox_bot_wizard_update(SELF, state, 450);
    if (self_mana != 149 || state->wizard.next_mana_regen_frame != 510)
        return 36;
    nox_bot_wizard_update(SELF, state, 509);
    if (self_mana != 149)
        return 37;
    nox_bot_wizard_update(SELF, state, 510);
    if (self_mana != 150 || state->wizard.next_mana_regen_frame != 570)
        return 38;
    return 0;
}

static int test_native_potion_thresholds(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_NORMAL, 500);

    if (!state)
        return 40;
    state->wizard.target = TARGET;
    self_health = 25;
    self_mana = 100;
    nox_bot_wizard_update(SELF, state, 500);
    if (red_potion_calls != 1 || blue_potion_calls != 1 || self_health != 75 || self_mana != 150)
        return 41;
    return 0;
}

static int test_loot_scan_and_weapon_preference(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_HARDCORE, 550);

    if (!state)
        return 45;
    self_buffs = buff_mask(ENCHANT_ANTI_MAGIC);
    available_loot_type = "WizardRobe";
    nox_bot_wizard_update(SELF, state, 550);
    if (pickup_calls != 1 || equip_armor_calls != 1 ||
        state->wizard.next_loot_scan_frame != 565)
        return 46;
    nox_bot_wizard_update(SELF, state, 564);
    if (pickup_calls != 1)
        return 47;

    state = reset_state(NOX_BOT_DIFFICULTY_HARDCORE, 560);
    if (!state)
        return 48;
    self_buffs = buff_mask(ENCHANT_ANTI_MAGIC);
    firestorm_item = 701;
    force_wand_item = 702;
    nox_bot_wizard_update(SELF, state, 560);
    if (equip_weapon_calls != 1 || state->wizard.next_weapon_preference_frame != 860)
        return 49;
    return 0;
}

static int test_ctf_objective_events(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_HARDCORE, 575);

    if (!state)
        return 52;
    ctf_mode = 1;
    self_buffs = buff_mask(ENCHANT_ANTI_MAGIC);
    nox_bot_policy_record_event(state, NOX_BOT_EVENT_LOST_SIGHT, TARGET, 575);
    nox_bot_wizard_update(SELF, state, 575);
    if (ctf_walk_own_flag_calls != 1 ||
        nox_bot_policy_event_pending(state, NOX_BOT_EVENT_LOST_SIGHT))
        return 53;
    nox_bot_policy_record_event(state, NOX_BOT_EVENT_END_OF_WAYPOINT, 0, 576);
    nox_bot_wizard_update(SELF, state, 576);
    if (ctf_attack_or_defend_calls != 1 ||
        nox_bot_policy_event_pending(state, NOX_BOT_EVENT_END_OF_WAYPOINT))
        return 54;
    return 0;
}

static int test_death_clears_wizard_life_state(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_NORMAL, 600);

    if (!state)
        return 50;
    state->wizard.target = TARGET;
    state->wizard.pending_spell = 3;
    state->wizard.fireball_ready_frame = 999;
    nox_bot_policy_record_event(state, NOX_BOT_EVENT_DEATH, 0, 600);
    nox_bot_wizard_update(SELF, state, 600);
    if (state->wizard.target || state->wizard.pending_spell || state->wizard.fireball_ready_frame ||
        nox_bot_policy_event_pending(state, NOX_BOT_EVENT_DEATH))
        return 51;
    return 0;
}

int main(void)
{
    int result;

    result = test_enemy_sighted_slow_then_death_ray();
    if (result)
        return result;
    result = test_visible_fireball_priority();
    if (result)
        return result;
    result = test_energy_bolt_reference_mana_quirk();
    if (result)
        return result;
    result = test_ring_of_fire_reference_once_per_life_quirk();
    if (result)
        return result;
    result = test_enemy_heard_hidden_target_invisibility();
    if (result)
        return result;
    result = test_hidden_defensive_priority_and_ctf_invisibility_gap();
    if (result)
        return result;
    result = test_antimagic_cancels_pending_without_spending_mana();
    if (result)
        return result;
    result = test_passive_mana_regen();
    if (result)
        return result;
    result = test_native_potion_thresholds();
    if (result)
        return result;
    result = test_loot_scan_and_weapon_preference();
    if (result)
        return result;
    result = test_ctf_objective_events();
    if (result)
        return result;
    return test_death_clears_wizard_life_state();
}
