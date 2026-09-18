#include "../src/bot_conjurer.h"
#include "../src/bot_engine.h"

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
static int last_equipped_weapon;
static int equip_armor_calls;
static int crossbow_item;
static int infinite_pain_wand_item;
static int firestorm_wand_item;
static int force_wand_item;
static int equipped_weapon;
static int ctf_walk_own_flag_calls;
static int ctf_attack_or_defend_calls;
static int owned_pixies;
static int enemy_deathball;
static int any_deathball;
static int target_missile;
static int ctf_tank;
static int mana_source;
static int mana_source_require_visible;
static int aggression_calls;
static float aggression_value;
static int walk_calls;
static float walk_x;
static float walk_y;
static int trap_calls;
static char trap_name[64];
static int summon_cage_used;
static int summon_spell_fits;
static int owned_bombers;
static int bomber_fits;
static int bomber_create_calls;
static int random_values[4];
static int random_value_count;
static int random_value_index;
static int phoneme_calls;
static nox_bot_phoneme phoneme_log[96];

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
        *x = object == TARGET ? 30.0f : (object == mana_source ? 50.0f : 10.0f);
    if (y)
        *y = object == TARGET ? 40.0f : (object == mana_source ? 60.0f : 20.0f);
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

int nox_bot_engine_play_phoneme(int object, nox_bot_phoneme phoneme)
{
    if (object != SELF)
        return 0;
    if (phoneme_calls < (int)(sizeof(phoneme_log) / sizeof(phoneme_log[0])))
        phoneme_log[phoneme_calls] = phoneme;
    ++phoneme_calls;
    return 1;
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

int nox_bot_team_is_ctf_tank(int object)
{
    return object == SELF && ctf_mode && ctf_tank;
}

int nox_bot_engine_find_nearest_mana_source(
    int object, int minimum_mana, int require_visible)
{
    if (object != SELF || minimum_mana != 10)
        return 0;
    mana_source_require_visible = require_visible;
    return mana_source;
}

int nox_bot_engine_set_aggression(int object, float aggression)
{
    if (object != SELF)
        return 0;
    ++aggression_calls;
    aggression_value = aggression;
    return 1;
}

void nox_bot_engine_walk_to(int object, float x, float y)
{
    if (object != SELF)
        return;
    ++walk_calls;
    walk_x = x;
    walk_y = y;
}

int nox_bot_engine_create_spell_trap(int object, const char *spell_name)
{
    if (object != SELF || !spell_name)
        return 0;
    ++trap_calls;
    strncpy(trap_name, spell_name, sizeof(trap_name) - 1);
    trap_name[sizeof(trap_name) - 1] = '\0';
    return 700;
}

int nox_bot_engine_summon_cage_used(int object)
{
    return object == SELF ? summon_cage_used : 0;
}

int nox_bot_engine_summon_spell_fits(int object, const char *spell_name)
{
    return object == SELF && spell_name && *spell_name && summon_spell_fits;
}

int nox_bot_engine_bomber_fits(int object)
{
    return object == SELF && bomber_fits;
}

int nox_bot_engine_create_bomber(int object)
{
    if (object != SELF || !bomber_fits)
        return 0;
    ++bomber_create_calls;
    ++owned_bombers;
    return 902;
}

int nox_bot_engine_random_int(int minimum, int maximum)
{
    int value;

    if (random_value_index < random_value_count)
        value = random_values[random_value_index++];
    else
        value = minimum;
    if (value < minimum)
        return minimum;
    if (value > maximum)
        return maximum;
    return value;
}

int nox_bot_engine_find_nearest_world_type(
    int object, const char *type_name, float max_distance)
{
    return object == SELF && (any_deathball || enemy_deathball) && max_distance >= 500.0f &&
        strcmp(type_name, "DeathBall") == 0 ? 900 : 0;
}

int nox_bot_engine_find_nearest_enemy_owned_type(
    int object, const char *type_name, float max_distance)
{
    return object == SELF && enemy_deathball && max_distance >= 500.0f &&
        strcmp(type_name, "DeathBall") == 0 ? 900 : 0;
}

int nox_bot_engine_find_nearest_missile_owned_by(
    int object, int owner_target, float max_distance)
{
    return object == SELF && owner_target == TARGET && target_missile &&
        max_distance >= 500.0f ? 901 : 0;
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
    last_equipped_weapon = item;
    equipped_weapon = item;
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
    if (object != SELF || !type_name)
        return 0;
    if (strcmp(type_name, "CrossBow") == 0)
        return crossbow_item;
    if (strcmp(type_name, "InfinitePainWand") == 0)
        return infinite_pain_wand_item;
    if (strcmp(type_name, "FireStormWand") == 0)
        return firestorm_wand_item;
    if (strcmp(type_name, "ForceWand") == 0)
        return force_wand_item;
    return 0;
}

int nox_bot_engine_equipped_weapon(int object)
{
    return object == SELF ? equipped_weapon : 0;
}

int nox_bot_engine_owned_type_count(int object, const char *type_name)
{
    if (object != SELF || !type_name)
        return 0;
    if (strcmp(type_name, "Pixie") == 0)
        return owned_pixies;
    if (strcmp(type_name, "Bomber") == 0)
        return owned_bombers;
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

    self_health = 100;
    target_health = 100;
    target_max_health = 100;
    self_mana = 125;
    self_buffs = 0;
    target_buffs = 0;
    target_visible = 1;
    ctf_mode = 0;
    ctf_tank = 0;
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
    last_equipped_weapon = 0;
    equip_armor_calls = 0;
    crossbow_item = 0;
    infinite_pain_wand_item = 0;
    firestorm_wand_item = 0;
    force_wand_item = 0;
    equipped_weapon = 0;
    ctf_walk_own_flag_calls = 0;
    ctf_attack_or_defend_calls = 0;
    owned_pixies = 1;
    enemy_deathball = 0;
    any_deathball = 0;
    target_missile = 0;
    mana_source = 0;
    mana_source_require_visible = 0;
    aggression_calls = 0;
    aggression_value = 0.0f;
    walk_calls = 0;
    walk_x = 0.0f;
    walk_y = 0.0f;
    trap_calls = 0;
    trap_name[0] = '\0';
    summon_cage_used = 4;
    summon_spell_fits = 0;
    owned_bombers = 0;
    bomber_fits = 0;
    bomber_create_calls = 0;
    random_values[0] = 1;
    random_values[1] = 10;
    random_value_count = 2;
    random_value_index = 0;
    phoneme_calls = 0;
    memset(phoneme_log, 0, sizeof(phoneme_log));
    nox_bot_policy_reset_all();
    if (!nox_bot_policy_activate(0, difficulty, frame))
        return 0;
    state = nox_bot_policy_get(0);
    state->native_object = SELF;
    return state;
}

static uint32_t finish_pending_spell(nox_bot_policy_state *state)
{
    uint32_t frame = 0;
    int guard = 96;

    while (state && state->conjurer.pending_spell && guard-- > 0) {
        frame = state->conjurer.pending_cast_frame;
        nox_bot_conjurer_update(SELF, state, frame);
    }
    return frame;
}

static int test_enemy_sighted_force_of_nature(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_NORMAL, 100);
    uint32_t release;

    if (!state)
        return 1;
    nox_bot_policy_record_event(state, NOX_BOT_EVENT_ENEMY_SIGHTED, TARGET, 100);
    nox_bot_conjurer_update(SELF, state, 100);
    if (!state->conjurer.pending_spell || cast_calls || state->conjurer.target != TARGET)
        return 2;
    nox_bot_conjurer_update(SELF, state, 129);
    if (cast_calls || phoneme_calls)
        return 3;
    release = finish_pending_spell(state);
    if (release != 139 || cast_calls != 1 || strcmp(cast_name, "FORCE_OF_NATURE") != 0 ||
        cast_kind != 3 || cast_x != 30.0f || cast_y != 40.0f || self_mana != 65)
        return 4;
    if (state->conjurer.global_ready_frame != 142 ||
        state->conjurer.force_of_nature_ready_frame != 289)
        return 5;
    return 0;
}

static int test_held_target_meteor_priority(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_HARDCORE, 200);
    uint32_t release;

    if (!state)
        return 10;
    state->conjurer.target = TARGET;
    target_buffs = buff_mask(ENCHANT_HELD);
    nox_bot_conjurer_update(SELF, state, 200);
    release = finish_pending_spell(state);
    if (release != 206 || cast_calls != 1 || strcmp(cast_name, "METEOR") != 0 ||
        cast_kind != 3 || cast_x != 30.0f || cast_y != 40.0f || self_mana != 95)
        return 11;
    if (state->conjurer.meteor_ready_frame != 356)
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
    if (finish_pending_spell(state) != 306 || cast_calls != 1 || strcmp(cast_name, "STUN") != 0 ||
        cast_kind != 2 || !(target_buffs & buff_mask(ENCHANT_HELD)))
        return 21;

    state = reset_state(NOX_BOT_DIFFICULTY_HARDCORE, 310);
    if (!state)
        return 22;
    state->conjurer.target = TARGET;
    ctf_mode = 1;
    nox_bot_conjurer_update(SELF, state, 310);
    if (finish_pending_spell(state) != 319 || cast_calls != 1 || strcmp(cast_name, "SLOW") != 0 ||
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
    if (finish_pending_spell(state) != 409 || cast_calls != 1 || strcmp(cast_name, "LESSER_HEAL") != 0 ||
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
    if (finish_pending_spell(state) != 512 || cast_calls != 1 || strcmp(cast_name, "VAMPIRISM") != 0 ||
        !(self_buffs & buff_mask(ENCHANT_VAMPIRISM)) || self_mana != 105)
        return 41;

    state->conjurer.global_ready_frame = 0;
    nox_bot_conjurer_update(SELF, state, 513);
    if (finish_pending_spell(state) != 525 || cast_calls != 2 ||
        strcmp(cast_name, "PROTECTION_FROM_ELECTRICITY") != 0 ||
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
    if (finish_pending_spell(state) != 627 || cast_calls != 1 || strcmp(cast_name, "INFRAVISION") != 0 ||
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
    if (finish_pending_spell(state) != 837 || cast_calls != 1 || strcmp(cast_name, "PIXIE_SWARM") != 0 ||
        cast_kind != 1 || self_mana != 95 || owned_pixies != 1)
        return 86;

    state->conjurer.global_ready_frame = 0;
    nox_bot_conjurer_update(SELF, state, 838);
    if (state->conjurer.pending_spell || cast_calls != 1)
        return 87;
    return 0;
}

static int test_hostile_deathball_uses_long_counterspell_cooldown(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_HARDCORE, 840);
    uint32_t release;

    if (!state)
        return 88;
    state->conjurer.target = TARGET;
    enemy_deathball = 1;
    nox_bot_conjurer_update(SELF, state, 840);
    if (!state->conjurer.pending_spell || cast_calls)
        return 89;
    release = finish_pending_spell(state);
    if (release != 846 || cast_calls != 1 || strcmp(cast_name, "COUNTERSPELL") != 0 ||
        cast_kind != 3 || cast_x != 10.0f || cast_y != 20.0f || self_mana != 105)
        return 90;
    if (state->conjurer.counterspell_ready_frame != 1446)
        return 91;
    return 0;
}

static int test_generic_target_missile_inversion(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_HARD, 845);
    uint32_t release;

    if (!state)
        return 120;
    state->conjurer.target = TARGET;
    target_missile = 1;
    nox_bot_conjurer_update(SELF, state, 845);
    if (!state->conjurer.pending_spell || cast_calls)
        return 121;
    nox_bot_conjurer_update(SELF, state, 859);
    if (cast_calls || phoneme_calls)
        return 122;
    release = finish_pending_spell(state);
    if (release != 866 || cast_calls != 1 || strcmp(cast_name, "INVERSION") != 0 ||
        cast_kind != 1 || cast_target != SELF || self_mana != 115)
        return 123;
    if (state->conjurer.inversion_ready_frame != 896 || phoneme_calls != 2 ||
        phoneme_log[0] != NOX_BOT_PHONEME_UP_LEFT ||
        phoneme_log[1] != NOX_BOT_PHONEME_FEMALE_UP_RIGHT)
        return 124;
    return 0;
}

static int test_retreat_and_held_blink(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_HARD, 847);
    uint32_t release;

    if (!state)
        return 130;
    nox_bot_policy_record_event(state, NOX_BOT_EVENT_RETREAT, 0, 847);
    nox_bot_conjurer_update(SELF, state, 847);
    if (!state->conjurer.pending_spell || cast_calls ||
        nox_bot_policy_event_pending(state, NOX_BOT_EVENT_RETREAT))
        return 131;
    nox_bot_conjurer_update(SELF, state, 861);
    if (cast_calls || phoneme_calls)
        return 132;
    release = finish_pending_spell(state);
    if (release != 871 || cast_calls || trap_calls != 1 || strcmp(trap_name, "BLINK") != 0 ||
        self_mana != 115 || state->conjurer.blink_ready_frame != 901)
        return 133;

    state = reset_state(NOX_BOT_DIFFICULTY_HARDCORE, 848);
    if (!state)
        return 134;
    state->conjurer.target = TARGET;
    self_buffs = buff_mask(ENCHANT_HELD);
    nox_bot_conjurer_update(SELF, state, 848);
    if (finish_pending_spell(state) != 857 || cast_calls || trap_calls != 1 ||
        strcmp(trap_name, "BLINK") != 0)
        return 135;
    return 0;
}

static int test_native_random_summon_and_cage_gate(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_HARDCORE, 849);
    uint32_t release;

    if (!state)
        return 136;
    target_visible = 0;
    self_buffs = buff_mask(ENCHANT_VAMPIRISM) | buff_mask(ENCHANT_PROTECT_SHOCK) |
        buff_mask(ENCHANT_PROTECT_FIRE) | buff_mask(ENCHANT_PROTECT_POISON);
    summon_cage_used = 0;
    summon_spell_fits = 1;
    random_values[0] = 3;
    random_values[1] = 1;
    random_value_count = 2;
    random_value_index = 0;
    nox_bot_conjurer_update(SELF, state, 849);
    if (!state->conjurer.pending_spell || !state->conjurer.pending_summon || cast_calls)
        return 137;
    release = finish_pending_spell(state);
    if (release != 867 || cast_calls != 1 || strcmp(cast_name, "SUMMON_MECHANICAL_GOLEM") != 0 ||
        cast_kind != 1 || self_mana != 40 || state->conjurer.summon_ready_frame != 1257 ||
        phoneme_calls != 6)
        return 138;

    state = reset_state(NOX_BOT_DIFFICULTY_HARDCORE, 850);
    if (!state)
        return 139;
    target_visible = 0;
    self_buffs = buff_mask(ENCHANT_VAMPIRISM) | buff_mask(ENCHANT_PROTECT_SHOCK) |
        buff_mask(ENCHANT_PROTECT_FIRE) | buff_mask(ENCHANT_PROTECT_POISON);
    summon_cage_used = 0;
    summon_spell_fits = 0;
    random_values[0] = 3;
    random_values[1] = 1;
    random_value_count = 2;
    random_value_index = 0;
    nox_bot_conjurer_update(SELF, state, 850);
    if (state->conjurer.pending_spell || cast_calls)
        return 140;
    return 0;
}

static int test_custom_bomber_uses_native_summon_path(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_HARD, 910);
    uint32_t release;

    if (!state)
        return 145;
    target_visible = 0;
    self_buffs = buff_mask(ENCHANT_VAMPIRISM) | buff_mask(ENCHANT_PROTECT_SHOCK) |
        buff_mask(ENCHANT_PROTECT_FIRE) | buff_mask(ENCHANT_PROTECT_POISON);
    summon_cage_used = 3;
    bomber_fits = 1;
    random_values[0] = 1;
    random_value_count = 1;
    random_value_index = 0;
    nox_bot_conjurer_update(SELF, state, 910);
    if (!state->conjurer.pending_spell || !state->conjurer.pending_summon || bomber_create_calls)
        return 146;
    nox_bot_conjurer_update(SELF, state, 924);
    if (bomber_create_calls || phoneme_calls)
        return 147;
    release = finish_pending_spell(state);
    if (release != 973 || bomber_create_calls != 1 || owned_bombers != 1 || self_mana != 125 ||
        state->conjurer.summon_ready_frame != 976 || state->conjurer.global_ready_frame != 976 ||
        phoneme_calls != 13)
        return 148;

    state = reset_state(NOX_BOT_DIFFICULTY_HARDCORE, 930);
    if (!state)
        return 149;
    target_visible = 0;
    self_buffs = buff_mask(ENCHANT_VAMPIRISM) | buff_mask(ENCHANT_PROTECT_SHOCK) |
        buff_mask(ENCHANT_PROTECT_FIRE) | buff_mask(ENCHANT_PROTECT_POISON);
    summon_cage_used = 3;
    bomber_fits = 1;
    owned_bombers = 2;
    random_values[0] = 1;
    random_values[1] = 10;
    random_value_count = 2;
    random_value_index = 0;
    nox_bot_conjurer_update(SELF, state, 930);
    if (state->conjurer.pending_spell || bomber_create_calls)
        return 150;
    return 0;
}

static int test_low_mana_hit_routes_to_native_obelisk(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_HARDCORE, 851);

    if (!state)
        return 141;
    self_mana = 20;
    mana_source = 600;
    self_buffs = buff_mask(ENCHANT_VAMPIRISM) | buff_mask(ENCHANT_PROTECT_SHOCK) |
        buff_mask(ENCHANT_PROTECT_FIRE) | buff_mask(ENCHANT_PROTECT_POISON);
    nox_bot_policy_record_event(state, NOX_BOT_EVENT_IS_HIT, TARGET, 851);
    nox_bot_conjurer_update(SELF, state, 851);
    if (!state->conjurer.mana_route_active || state->conjurer.mana_source != 600 ||
        mana_source_require_visible || aggression_calls != 1 || aggression_value != 0.16f ||
        walk_calls != 1 || walk_x != 50.0f || walk_y != 60.0f ||
        nox_bot_policy_event_pending(state, NOX_BOT_EVENT_IS_HIT))
        return 142;

    state = reset_state(NOX_BOT_DIFFICULTY_HARDCORE, 852);
    if (!state)
        return 143;
    self_mana = 20;
    mana_source = 600;
    ctf_mode = 1;
    ctf_tank = 1;
    self_buffs = buff_mask(ENCHANT_VAMPIRISM) | buff_mask(ENCHANT_PROTECT_SHOCK) |
        buff_mask(ENCHANT_PROTECT_FIRE) | buff_mask(ENCHANT_PROTECT_POISON);
    nox_bot_policy_record_event(state, NOX_BOT_EVENT_IS_HIT, TARGET, 852);
    nox_bot_conjurer_update(SELF, state, 852);
    if (!mana_source_require_visible || walk_calls != 1)
        return 144;
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

static int test_literal_weapon_preference(void)
{
    nox_bot_policy_state *state = reset_state(NOX_BOT_DIFFICULTY_HARDCORE, 880);

    if (!state)
        return 170;
    self_buffs = buff_mask(ENCHANT_ANTI_MAGIC);
    crossbow_item = 701;
    firestorm_wand_item = 702;
    infinite_pain_wand_item = 703;
    force_wand_item = 704;
    nox_bot_conjurer_update(SELF, state, 880);
    if (equip_weapon_calls != 1 || last_equipped_weapon != firestorm_wand_item ||
        state->conjurer.next_weapon_preference_frame != 1180)
        return 171;

    nox_bot_conjurer_update(SELF, state, 1179);
    if (equip_weapon_calls != 1)
        return 172;

    /* Once CrossBow itself is equipped, the literal else-if branch may test
     * InfinitePainWand and equip ForceWand instead. */
    equipped_weapon = crossbow_item;
    nox_bot_conjurer_update(SELF, state, 1180);
    if (equip_weapon_calls != 2 || last_equipped_weapon != force_wand_item ||
        state->conjurer.next_weapon_preference_frame != 1480)
        return 173;

    state = reset_state(NOX_BOT_DIFFICULTY_HARDCORE, 1190);
    if (!state)
        return 174;
    self_buffs = buff_mask(ENCHANT_ANTI_MAGIC);
    crossbow_item = 701;
    infinite_pain_wand_item = 703;
    force_wand_item = 704;
    /* The first guard wins even when FireStormWand is absent; the reference's
     * else-if therefore does not fall through to ForceWand in this case. */
    nox_bot_conjurer_update(SELF, state, 1190);
    if (equip_weapon_calls || state->conjurer.next_weapon_preference_frame != 1490)
        return 175;
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
    result = test_hostile_deathball_uses_long_counterspell_cooldown();
    if (result)
        return result;
    result = test_generic_target_missile_inversion();
    if (result)
        return result;
    result = test_retreat_and_held_blink();
    if (result)
        return result;
    result = test_native_random_summon_and_cage_gate();
    if (result)
        return result;
    result = test_custom_bomber_uses_native_summon_path();
    if (result)
        return result;
    result = test_low_mana_hit_routes_to_native_obelisk();
    if (result)
        return result;
    result = test_loot_scan();
    if (result)
        return result;
    result = test_literal_weapon_preference();
    if (result)
        return result;
    result = test_ctf_objective_events();
    if (result)
        return result;
    return test_death_clears_conjurer_life_state();
}
