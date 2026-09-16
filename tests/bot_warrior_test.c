#include "../src/bot_warrior.h"
#include "../src/bot_engine.h"

#include <string.h>

static int ready_ability;
static int executed_ability;
static int execute_calls;
static int enemy_result;
static int interact_result;
static int target_max_health;
static int invisible_target;
static int invulnerable_target;
static int current_target;
static int face_calls;
static int active_abilities;
static int harpoon_attached_target;
static int self_health;
static int self_max_health;
static int target_health;
static int potion_use_calls;
static int potion_use_result;
static int harpoon_stop_calls;
static uint32_t engine_fps;
static float self_x;
static float self_y;
static float target_x;
static float target_y;
static const char *world_loot_type;
static int world_loot_item;
static int pickup_item_calls;
static int equip_weapon_calls;
static int equip_armor_calls;
static int inventory_greatsword;
static int inventory_warhammer;
static int inventory_longsword;
static int inventory_round_chakram;
static int last_equipped_item;
static int equipped_weapon;
static int interrupt_calls;
static int attack_start_calls;
static int attack_start_result;
static int attack_step_calls;
static int attack_step_result;
static int attack_step_removes_weapon;
static int nearest_type_item;
static float nearest_type_x;
static float nearest_type_y;
static int set_aggression_calls;
static float last_aggression;
static int walk_calls;
static float last_walk_x;
static float last_walk_y;
static int hunt_calls;
static int ctf_result;
static int carrying_flag_result;
static int teleport_wake_item;
static float teleport_wake_x;
static float teleport_wake_y;
static int attack_target_calls;
static int last_attack_target;
static int guard_calls;
static float last_guard_x;
static float last_guard_y;
static float last_guard_radius;
static int ctf_own_flag_world;
static int ctf_enemy_flag_world;
static int ctf_own_flag_carrier;
static int ctf_enemy_flag_carrier;
static int ctf_own_flag_at_home;
static float ctf_own_x;
static float ctf_own_y;
static float ctf_enemy_x;
static float ctf_enemy_y;

int nox_bot_engine_ability_ready(int object, int ability)
{
    (void)object;
    return ability == ready_ability;
}

int nox_bot_engine_ability_active(int object, int ability)
{
    (void)object;
    return (active_abilities & (1 << ability)) != 0;
}

int nox_bot_engine_execute_ability(int object, int ability)
{
    (void)object;
    executed_ability = ability;
    ++execute_calls;
    active_abilities |= 1 << ability;
    return 1;
}

int nox_bot_engine_is_enemy(int self, int other)
{
    (void)self;
    (void)other;
    return enemy_result;
}

int nox_bot_engine_can_interact(int self, int other)
{
    (void)self;
    (void)other;
    return interact_result;
}

int nox_bot_engine_has_buff(int object, int buff)
{
    if (buff == 0)
        return object == invisible_target;
    if (buff == 23)
        return object == invulnerable_target;
    return 0;
}

int nox_bot_engine_health(int object)
{
    return object == 1 ? self_health : target_health;
}

int nox_bot_engine_max_health(int object)
{
    return object == 1 ? self_max_health : target_max_health;
}

int nox_bot_engine_use_inventory_potion(int object, const char *type_name)
{
    if (object == 1 && strcmp(type_name, "RedPotion") == 0) {
        ++potion_use_calls;
        return potion_use_result;
    }
    return 0;
}

int nox_bot_engine_inventory_item(int object, const char *type_name)
{
    (void)object;
    if (strcmp(type_name, "GreatSword") == 0)
        return inventory_greatsword;
    if (strcmp(type_name, "WarHammer") == 0)
        return inventory_warhammer;
    if (strcmp(type_name, "Longsword") == 0)
        return inventory_longsword;
    if (strcmp(type_name, "RoundChakram") == 0)
        return inventory_round_chakram;
    return 0;
}

int nox_bot_engine_find_nearest_type(
    int object, const char *type_name, float max_distance)
{
    (void)object;
    (void)max_distance;
    if (nearest_type_item && strcmp(type_name, "RedPotion") == 0)
        return nearest_type_item;
    if (teleport_wake_item && strcmp(type_name, "TeleportWake") == 0)
        return teleport_wake_item;
    return 0;
}

int nox_bot_engine_find_nearest_visible_type(
    int object, const char *type_name, float max_distance)
{
    (void)object;
    (void)max_distance;
    if (world_loot_type && strcmp(type_name, world_loot_type) == 0)
        return world_loot_item;
    return 0;
}

int nox_bot_engine_pickup_item(int object, int item)
{
    (void)object;
    (void)item;
    ++pickup_item_calls;
    world_loot_type = 0;
    return 1;
}

int nox_bot_engine_equip_weapon(int object, int item)
{
    (void)object;
    ++equip_weapon_calls;
    last_equipped_item = item;
    equipped_weapon = item;
    return 1;
}

int nox_bot_engine_equipped_weapon(int object)
{
    (void)object;
    return equipped_weapon;
}

void nox_bot_engine_interrupt(int object)
{
    (void)object;
    ++interrupt_calls;
}

int nox_bot_engine_start_player_attack(int object)
{
    (void)object;
    ++attack_start_calls;
    return attack_start_result;
}

int nox_bot_engine_player_attack_step(int object)
{
    (void)object;
    ++attack_step_calls;
    if (attack_step_removes_weapon)
        equipped_weapon = 0;
    return attack_step_result;
}

int nox_bot_engine_equip_armor(int object, int item)
{
    (void)object;
    ++equip_armor_calls;
    last_equipped_item = item;
    return 1;
}

int nox_bot_engine_current_target(int object)
{
    (void)object;
    return current_target;
}

int nox_bot_engine_harpoon_attached_target(int object)
{
    (void)object;
    return harpoon_attached_target;
}

int nox_bot_engine_stop_harpoon(int object)
{
    (void)object;
    ++harpoon_stop_calls;
    harpoon_attached_target = 0;
    active_abilities &= ~(1 << NOX_BOT_ABILITY_HARPOON);
    return 1;
}

uint32_t nox_bot_engine_fps(void)
{
    return engine_fps;
}

void nox_bot_engine_position(int object, float *x, float *y)
{
    float px;
    float py;

    if (object == 1) {
        px = self_x;
        py = self_y;
    } else if (object == nearest_type_item) {
        px = nearest_type_x;
        py = nearest_type_y;
    } else if (object == teleport_wake_item) {
        px = teleport_wake_x;
        py = teleport_wake_y;
    } else if (object == ctf_own_flag_world || object == ctf_own_flag_carrier) {
        px = ctf_own_x;
        py = ctf_own_y;
    } else if (object == ctf_enemy_flag_world || object == ctf_enemy_flag_carrier) {
        px = ctf_enemy_x;
        py = ctf_enemy_y;
    } else {
        px = target_x;
        py = target_y;
    }
    if (x)
        *x = px;
    if (y)
        *y = py;
}

int nox_bot_engine_set_aggression(int object, float aggression)
{
    (void)object;
    ++set_aggression_calls;
    last_aggression = aggression;
    return 1;
}

int nox_bot_engine_is_ctf(void)
{
    return ctf_result;
}

int nox_bot_engine_carrying_ctf_flag(int object)
{
    (void)object;
    return carrying_flag_result;
}

int nox_bot_engine_ctf_flag_world(int object, int own_team)
{
    (void)object;
    return own_team ? ctf_own_flag_world : ctf_enemy_flag_world;
}

int nox_bot_engine_ctf_flag_carrier(int object, int own_team)
{
    (void)object;
    return own_team ? ctf_own_flag_carrier : ctf_enemy_flag_carrier;
}

int nox_bot_engine_ctf_flag_at_home(int flag)
{
    return flag == ctf_own_flag_world && ctf_own_flag_at_home;
}

void nox_bot_engine_walk_to(int object, float x, float y)
{
    (void)object;
    ++walk_calls;
    last_walk_x = x;
    last_walk_y = y;
}

void nox_bot_engine_attack_target(int object, int target)
{
    (void)object;
    ++attack_target_calls;
    last_attack_target = target;
}

void nox_bot_engine_guard_position(int object, float x, float y, float radius)
{
    (void)object;
    ++guard_calls;
    last_guard_x = x;
    last_guard_y = y;
    last_guard_radius = radius;
}

void nox_bot_engine_hunt(int object)
{
    (void)object;
    ++hunt_calls;
}

void nox_bot_engine_face_target(int object, int target)
{
    (void)object;
    (void)target;
    ++face_calls;
}

static void reset_case(nox_bot_policy_state *state)
{
    memset(state, 0, sizeof(*state));
    state->active = 1;
    state->difficulty = NOX_BOT_DIFFICULTY_NORMAL;
    ready_ability = 0;
    executed_ability = 0;
    execute_calls = 0;
    enemy_result = 1;
    interact_result = 1;
    target_max_health = 100;
    invisible_target = 0;
    invulnerable_target = 0;
    current_target = 0;
    face_calls = 0;
    active_abilities = 0;
    harpoon_attached_target = 0;
    self_health = 150;
    self_max_health = 150;
    target_health = 100;
    potion_use_calls = 0;
    potion_use_result = 0;
    harpoon_stop_calls = 0;
    engine_fps = 30;
    self_x = 0.0f;
    self_y = 0.0f;
    target_x = 100.0f;
    target_y = 0.0f;
    world_loot_type = 0;
    world_loot_item = 0;
    pickup_item_calls = 0;
    equip_weapon_calls = 0;
    equip_armor_calls = 0;
    inventory_greatsword = 0;
    inventory_warhammer = 0;
    inventory_longsword = 0;
    inventory_round_chakram = 0;
    last_equipped_item = 0;
    equipped_weapon = 0;
    interrupt_calls = 0;
    attack_start_calls = 0;
    attack_start_result = 1;
    attack_step_calls = 0;
    attack_step_result = 1;
    attack_step_removes_weapon = 0;
    nearest_type_item = 0;
    nearest_type_x = 0.0f;
    nearest_type_y = 0.0f;
    set_aggression_calls = 0;
    last_aggression = 0.0f;
    walk_calls = 0;
    last_walk_x = 0.0f;
    last_walk_y = 0.0f;
    hunt_calls = 0;
    ctf_result = 0;
    carrying_flag_result = 0;
    teleport_wake_item = 0;
    teleport_wake_x = 0.0f;
    teleport_wake_y = 0.0f;
    attack_target_calls = 0;
    last_attack_target = 0;
    guard_calls = 0;
    last_guard_x = 0.0f;
    last_guard_y = 0.0f;
    last_guard_radius = 0.0f;
    ctf_own_flag_world = 0;
    ctf_enemy_flag_world = 0;
    ctf_own_flag_carrier = 0;
    ctf_enemy_flag_carrier = 0;
    ctf_own_flag_at_home = 0;
    ctf_own_x = 0.0f;
    ctf_own_y = 0.0f;
    ctf_enemy_x = 0.0f;
    ctf_enemy_y = 0.0f;
}

static int test_eye_reaction_delay(void)
{
    nox_bot_policy_state state;

    reset_case(&state);
    ready_ability = NOX_BOT_ABILITY_EYE_OF_THE_WOLF;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_LOOKING_FOR_ENEMY, 0, 100);
    nox_bot_warrior_update(1, &state, 129);
    if (execute_calls || !nox_bot_policy_event_pending(&state, NOX_BOT_EVENT_LOOKING_FOR_ENEMY))
        return 1;
    nox_bot_warrior_update(1, &state, 130);
    if (execute_calls != 1 || executed_ability != NOX_BOT_ABILITY_EYE_OF_THE_WOLF)
        return 2;
    if (nox_bot_policy_event_pending(&state, NOX_BOT_EVENT_LOOKING_FOR_ENEMY))
        return 3;
    return 0;
}

static int test_eye_heard_and_lost_sight(void)
{
    nox_bot_policy_state state;

    reset_case(&state);
    ready_ability = NOX_BOT_ABILITY_EYE_OF_THE_WOLF;
    invisible_target = 22;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_ENEMY_HEARD, 22, 200);
    nox_bot_warrior_update(1, &state, 230);
    if (execute_calls != 1)
        return 10;

    reset_case(&state);
    ready_ability = NOX_BOT_ABILITY_EYE_OF_THE_WOLF;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_LOST_SIGHT, 22, 300);
    nox_bot_warrior_update(1, &state, 344);
    if (execute_calls)
        return 11;
    nox_bot_warrior_update(1, &state, 345);
    if (execute_calls != 1)
        return 12;
    return 0;
}

static int test_health_potion_policy(void)
{
    nox_bot_policy_state state;

    reset_case(&state);
    self_health = 100;
    potion_use_result = 1;
    nox_bot_warrior_update(1, &state, 350);
    if (potion_use_calls != 1)
        return 20;

    reset_case(&state);
    self_health = 101;
    nox_bot_warrior_update(1, &state, 350);
    if (potion_use_calls)
        return 21;

    reset_case(&state);
    self_health = 100;
    self_max_health = 100;
    nox_bot_warrior_update(1, &state, 350);
    if (potion_use_calls)
        return 22;
    return 0;
}

static int test_harpoon_is_immediate_and_blocks_warcry(void)
{
    nox_bot_policy_state state;

    reset_case(&state);
    ready_ability = NOX_BOT_ABILITY_HARPOON;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_ENEMY_SIGHTED, 44, 400);
    nox_bot_warrior_update(1, &state, 400);
    if (execute_calls != 1 || executed_ability != NOX_BOT_ABILITY_HARPOON || face_calls != 1)
        return 30;
    if (nox_bot_policy_event_pending(&state, NOX_BOT_EVENT_ENEMY_SIGHTED))
        return 31;

    ready_ability = NOX_BOT_ABILITY_WARCRY;
    execute_calls = 0;
    face_calls = 0;
    nox_bot_warrior_update(1, &state, 430);
    if (execute_calls || face_calls)
        return 32;
    return 0;
}

static int test_warcry_target_gates(void)
{
    nox_bot_policy_state state;

    reset_case(&state);
    ready_ability = NOX_BOT_ABILITY_WARCRY;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_ENEMY_SIGHTED, 44, 400);
    nox_bot_warrior_update(1, &state, 430);
    if (execute_calls != 1 || executed_ability != NOX_BOT_ABILITY_WARCRY || face_calls != 1)
        return 40;

    reset_case(&state);
    ready_ability = NOX_BOT_ABILITY_WARCRY;
    target_max_health = 150;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_CHANGE_FOCUS, 44, 500);
    nox_bot_warrior_update(1, &state, 530);
    if (execute_calls)
        return 41;

    reset_case(&state);
    ready_ability = NOX_BOT_ABILITY_WARCRY;
    invulnerable_target = 44;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_ENEMY_SIGHTED, 44, 600);
    nox_bot_warrior_update(1, &state, 630);
    if (execute_calls)
        return 42;

    reset_case(&state);
    ready_ability = NOX_BOT_ABILITY_WARCRY;
    active_abilities = 1 << NOX_BOT_ABILITY_HARPOON;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_CHANGE_FOCUS, 44, 700);
    nox_bot_warrior_update(1, &state, 730);
    if (execute_calls)
        return 43;
    return 0;
}

static int test_berserker_charge_event_priority(void)
{
    nox_bot_policy_state state;

    reset_case(&state);
    ready_ability = NOX_BOT_ABILITY_BERSERKER_CHARGE;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_ENEMY_SIGHTED, 44, 800);
    nox_bot_warrior_update(1, &state, 829);
    if (execute_calls)
        return 50;
    nox_bot_warrior_update(1, &state, 830);
    if (execute_calls != 1 || executed_ability != NOX_BOT_ABILITY_BERSERKER_CHARGE)
        return 51;
    if (face_calls != 1 || nox_bot_policy_event_pending(&state, NOX_BOT_EVENT_ENEMY_SIGHTED))
        return 52;

    reset_case(&state);
    ready_ability = NOX_BOT_ABILITY_BERSERKER_CHARGE;
    active_abilities = 1 << NOX_BOT_ABILITY_HARPOON;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_CHANGE_FOCUS, 44, 900);
    nox_bot_warrior_update(1, &state, 930);
    if (execute_calls)
        return 53;

    reset_case(&state);
    ready_ability = NOX_BOT_ABILITY_BERSERKER_CHARGE;
    active_abilities = 1 << NOX_BOT_ABILITY_HARPOON;
    harpoon_attached_target = 44;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_CHANGE_FOCUS, 44, 1000);
    nox_bot_warrior_update(1, &state, 1030);
    if (execute_calls != 1 || executed_ability != NOX_BOT_ABILITY_BERSERKER_CHARGE)
        return 54;
    return 0;
}

static int test_berserker_charge_collision_delay(void)
{
    nox_bot_policy_state state;

    reset_case(&state);
    ready_ability = NOX_BOT_ABILITY_BERSERKER_CHARGE;
    current_target = 44;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_COLLISION, 44, 1100);
    nox_bot_warrior_update(1, &state, 1159);
    if (execute_calls)
        return 60;
    nox_bot_warrior_update(1, &state, 1160);
    if (execute_calls != 1 || executed_ability != NOX_BOT_ABILITY_BERSERKER_CHARGE)
        return 61;
    if (nox_bot_policy_event_pending(&state, NOX_BOT_EVENT_COLLISION))
        return 62;

    reset_case(&state);
    ready_ability = NOX_BOT_ABILITY_BERSERKER_CHARGE;
    current_target = 45;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_COLLISION, 44, 1200);
    nox_bot_warrior_update(1, &state, 1260);
    if (execute_calls || nox_bot_policy_event_pending(&state, NOX_BOT_EVENT_COLLISION))
        return 63;
    return 0;
}

static int test_periodic_close_range_scan(void)
{
    nox_bot_policy_state state;

    reset_case(&state);
    current_target = 44;
    ready_ability = NOX_BOT_ABILITY_HARPOON;
    nox_bot_warrior_update(1, &state, 1300);
    if (execute_calls != 1 || executed_ability != NOX_BOT_ABILITY_HARPOON)
        return 70;
    if (state.warrior.next_ability_scan_frame != 1330)
        return 71;

    reset_case(&state);
    current_target = 44;
    ready_ability = NOX_BOT_ABILITY_BERSERKER_CHARGE;
    nox_bot_warrior_update(1, &state, 1400);
    if (execute_calls || state.warrior.pending_ability != NOX_BOT_ABILITY_BERSERKER_CHARGE)
        return 72;
    nox_bot_warrior_update(1, &state, 1429);
    if (execute_calls)
        return 73;
    nox_bot_warrior_update(1, &state, 1430);
    if (execute_calls != 1 || executed_ability != NOX_BOT_ABILITY_BERSERKER_CHARGE)
        return 74;

    reset_case(&state);
    current_target = 44;
    ready_ability = NOX_BOT_ABILITY_WARCRY;
    nox_bot_warrior_update(1, &state, 1450);
    if (execute_calls || state.warrior.pending_ability != NOX_BOT_ABILITY_WARCRY)
        return 75;
    nox_bot_warrior_update(1, &state, 1480);
    if (execute_calls != 1 || executed_ability != NOX_BOT_ABILITY_WARCRY)
        return 76;

    reset_case(&state);
    current_target = 44;
    ready_ability = NOX_BOT_ABILITY_WARCRY;
    active_abilities = 1 << NOX_BOT_ABILITY_BERSERKER_CHARGE;
    nox_bot_warrior_update(1, &state, 1490);
    if (execute_calls || state.warrior.pending_ability)
        return 77;

    reset_case(&state);
    current_target = 44;
    target_x = 151.0f;
    ready_ability = NOX_BOT_ABILITY_HARPOON;
    nox_bot_warrior_update(1, &state, 1500);
    if (execute_calls || state.warrior.pending_ability)
        return 78;
    return 0;
}

static int test_harpoon_attachment_schedules_charge(void)
{
    nox_bot_policy_state state;

    reset_case(&state);
    ready_ability = NOX_BOT_ABILITY_BERSERKER_CHARGE;
    active_abilities = 1 << NOX_BOT_ABILITY_HARPOON;
    harpoon_attached_target = 44;
    state.warrior.next_ability_scan_frame = 2000;
    nox_bot_warrior_update(1, &state, 1600);
    if (!state.warrior.harpoon_charge_pending || state.warrior.harpoon_charge_target != 44)
        return 80;
    nox_bot_warrior_update(1, &state, 1629);
    if (execute_calls)
        return 81;
    nox_bot_warrior_update(1, &state, 1630);
    if (execute_calls != 1 || executed_ability != NOX_BOT_ABILITY_BERSERKER_CHARGE)
        return 82;
    if (state.warrior.harpoon_charge_pending)
        return 83;
    return 0;
}

static int test_hit_breaks_attached_harpoon(void)
{
    nox_bot_policy_state state;

    reset_case(&state);
    active_abilities = 1 << NOX_BOT_ABILITY_HARPOON;
    harpoon_attached_target = 44;
    state.warrior.next_ability_scan_frame = 2000;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_IS_HIT, 55, 1700);
    nox_bot_warrior_update(1, &state, 1700);
    if (harpoon_stop_calls != 1 || harpoon_attached_target != 0)
        return 90;
    if (nox_bot_policy_event_pending(&state, NOX_BOT_EVENT_IS_HIT))
        return 91;
    if (state.warrior.harpoon_charge_pending)
        return 92;
    return 0;
}

static int test_nearby_loot_scan_and_cadence(void)
{
    nox_bot_policy_state state;

    reset_case(&state);
    world_loot_type = "GreatSword";
    world_loot_item = 201;
    nox_bot_warrior_update(1, &state, 1800);
    if (pickup_item_calls != 1 || equip_weapon_calls != 1 || last_equipped_item != 201)
        return 100;
    if (state.warrior.next_loot_scan_frame != 1815)
        return 101;

    world_loot_type = "GreatSword";
    world_loot_item = 202;
    nox_bot_warrior_update(1, &state, 1814);
    if (pickup_item_calls != 1)
        return 102;
    nox_bot_warrior_update(1, &state, 1815);
    if (pickup_item_calls != 2 || equip_weapon_calls != 2 || last_equipped_item != 202)
        return 103;

    world_loot_type = "Breastplate";
    world_loot_item = 203;
    nox_bot_warrior_update(1, &state, 1830);
    if (pickup_item_calls != 3 || equip_armor_calls != 1 || last_equipped_item != 203)
        return 104;
    return 0;
}

static int test_weapon_preference_cadence(void)
{
    nox_bot_policy_state state;

    reset_case(&state);
    inventory_greatsword = 301;
    inventory_warhammer = 302;
    inventory_longsword = 303;
    nox_bot_warrior_update(1, &state, 1900);
    if (equip_weapon_calls != 1 || last_equipped_item != 301)
        return 110;
    if (state.warrior.next_weapon_preference_frame != 2200)
        return 111;
    nox_bot_warrior_update(1, &state, 2199);
    if (equip_weapon_calls != 1)
        return 112;
    nox_bot_warrior_update(1, &state, 2200);
    if (equip_weapon_calls != 2 || last_equipped_item != 301)
        return 113;

    reset_case(&state);
    inventory_warhammer = 302;
    inventory_longsword = 303;
    nox_bot_warrior_update(1, &state, 2300);
    if (equip_weapon_calls != 1 || last_equipped_item != 302)
        return 114;

    reset_case(&state);
    inventory_longsword = 303;
    nox_bot_warrior_update(1, &state, 2400);
    if (equip_weapon_calls != 1 || last_equipped_item != 303)
        return 115;
    return 0;
}

static int test_enemy_events_start_native_chakram_attack(void)
{
    nox_bot_policy_state state;

    reset_case(&state);
    inventory_round_chakram = 401;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_ENEMY_SIGHTED, 44, 2500);
    nox_bot_warrior_update(1, &state, 2500);
    if (!state.warrior.chakram_attack_active || state.warrior.chakram_item != 401)
        return 120;
    if (state.warrior.chakram_ready_frame != 2800 || attack_start_calls != 1 ||
        interrupt_calls != 1 || equipped_weapon != 401 || face_calls != 1)
        return 121;

    reset_case(&state);
    current_target = 45;
    inventory_round_chakram = 402;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_ENEMY_HEARD, 99, 2600);
    nox_bot_warrior_update(1, &state, 2600);
    if (!state.warrior.chakram_attack_active || state.warrior.chakram_item != 402 ||
        attack_start_calls != 1 || equipped_weapon != 402)
        return 122;
    if (!nox_bot_policy_event_pending(&state, NOX_BOT_EVENT_ENEMY_HEARD))
        return 123;
    return 0;
}

static int test_failed_chakram_attack_restores_previous_weapon(void)
{
    nox_bot_policy_state state;

    reset_case(&state);
    inventory_round_chakram = 401;
    equipped_weapon = 777;
    attack_start_result = 0;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_ENEMY_SIGHTED, 44, 2700);
    nox_bot_warrior_update(1, &state, 2700);
    if (attack_start_calls != 1 || state.warrior.chakram_attack_active ||
        state.warrior.chakram_ready_frame)
        return 125;
    if (equipped_weapon != 777 || last_equipped_item != 777)
        return 126;
    return 0;
}

static int test_chakram_cooldown_and_native_release(void)
{
    nox_bot_policy_state state;

    reset_case(&state);
    inventory_round_chakram = 401;
    state.warrior.chakram_ready_frame = 3000;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_ENEMY_SIGHTED, 44, 2999);
    nox_bot_warrior_update(1, &state, 2999);
    if (attack_start_calls || state.warrior.chakram_attack_active)
        return 130;

    reset_case(&state);
    inventory_round_chakram = 401;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_ENEMY_SIGHTED, 44, 3100);
    nox_bot_warrior_update(1, &state, 3100);
    if (!state.warrior.chakram_attack_active)
        return 131;
    inventory_greatsword = 301;
    attack_step_removes_weapon = 1;
    nox_bot_warrior_update(1, &state, 3101);
    if (attack_step_calls != 1 || state.warrior.chakram_attack_active ||
        state.warrior.chakram_item)
        return 132;
    if (equipped_weapon != 301 || last_equipped_item != 301)
        return 133;
    return 0;
}

static int test_chakram_attack_blocks_conflicting_abilities(void)
{
    nox_bot_policy_state state;

    reset_case(&state);
    state.warrior.chakram_attack_active = 1;
    state.warrior.chakram_item = 401;
    equipped_weapon = 401;
    ready_ability = NOX_BOT_ABILITY_BERSERKER_CHARGE;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_ENEMY_SIGHTED, 44, 3200);
    nox_bot_warrior_update(1, &state, 3230);
    if (attack_step_calls != 1 || execute_calls)
        return 140;
    if (!nox_bot_policy_event_pending(&state, NOX_BOT_EVENT_ENEMY_SIGHTED))
        return 141;
    return 0;
}

static int test_non_ctf_potion_seek_and_waypoint_resume(void)
{
    nox_bot_policy_state state;

    reset_case(&state);
    self_health = 90;
    current_target = 44;
    target_health = 50;
    nearest_type_item = 66;
    nearest_type_x = 25.0f;
    nearest_type_y = -15.0f;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_IS_HIT, 55, 3300);
    nox_bot_warrior_update(1, &state, 3300);
    if (!state.warrior.seeking_potion || set_aggression_calls != 1 ||
        last_aggression != 0.16f || walk_calls != 1 ||
        last_walk_x != 25.0f || last_walk_y != -15.0f)
        return 150;
    if (nox_bot_policy_event_pending(&state, NOX_BOT_EVENT_IS_HIT))
        return 151;

    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_END_OF_WAYPOINT, 0, 3301);
    nox_bot_warrior_update(1, &state, 3301);
    if (state.warrior.seeking_potion || set_aggression_calls != 2 ||
        last_aggression != 0.83f || hunt_calls != 1)
        return 152;
    if (nox_bot_policy_event_pending(&state, NOX_BOT_EVENT_END_OF_WAYPOINT))
        return 153;

    reset_case(&state);
    self_health = 90;
    current_target = 44;
    target_health = 50;
    nearest_type_item = 66;
    ctf_result = 1;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_IS_HIT, 55, 3400);
    nox_bot_warrior_update(1, &state, 3400);
    if (!state.warrior.seeking_potion || walk_calls != 1)
        return 154;

    reset_case(&state);
    self_health = 90;
    current_target = 44;
    target_health = 50;
    nearest_type_item = 66;
    ctf_result = 1;
    carrying_flag_result = 1;
    interact_result = 0;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_IS_HIT, 55, 3450);
    nox_bot_warrior_update(1, &state, 3450);
    if (state.warrior.seeking_potion || walk_calls)
        return 155;

    reset_case(&state);
    self_health = 90;
    current_target = 44;
    target_health = 50;
    nearest_type_item = 66;
    ctf_result = 1;
    carrying_flag_result = 1;
    interact_result = 1;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_IS_HIT, 55, 3475);
    nox_bot_warrior_update(1, &state, 3475);
    if (!state.warrior.seeking_potion || walk_calls != 1)
        return 156;

    reset_case(&state);
    self_health = 90;
    current_target = 44;
    target_health = 10;
    nearest_type_item = 66;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_IS_HIT, 55, 3500);
    nox_bot_warrior_update(1, &state, 3500);
    if (state.warrior.seeking_potion || walk_calls)
        return 157;
    return 0;
}

static int test_lost_sight_teleport_wake_pursuit(void)
{
    nox_bot_policy_state state;

    reset_case(&state);
    teleport_wake_item = 66;
    teleport_wake_x = 50.0f;
    teleport_wake_y = 0.0f;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_LOST_SIGHT, 44, 3600);
    nox_bot_warrior_update(1, &state, 3600);
    if (!state.warrior.teleport_wake_tracking ||
        state.warrior.teleport_wake_target != 44 || walk_calls != 1 ||
        last_walk_x != 50.0f || last_walk_y != 0.0f || attack_target_calls)
        return 160;

    self_x = 151.0f;
    nox_bot_warrior_update(1, &state, 3601);
    if (state.warrior.teleport_wake_tracking || attack_target_calls != 1 ||
        last_attack_target != 44)
        return 161;

    reset_case(&state);
    teleport_wake_item = 67;
    teleport_wake_x = 150.0f;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_LOST_SIGHT, 45, 3650);
    nox_bot_warrior_update(1, &state, 3650);
    if (state.warrior.teleport_wake_tracking || attack_target_calls != 1 ||
        last_attack_target != 45 || walk_calls != 1 || last_walk_x != 150.0f)
        return 162;
    return 0;
}

static int test_ctf_end_waypoint_strategy(void)
{
    nox_bot_policy_state state;

    reset_case(&state);
    ctf_result = 1;
    ctf_own_flag_world = 70;
    ctf_enemy_flag_world = 71;
    ctf_enemy_x = 300.0f;
    ctf_enemy_y = -20.0f;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_END_OF_WAYPOINT, 0, 3700);
    nox_bot_warrior_update(1, &state, 3700);
    if (walk_calls != 1 || guard_calls || last_walk_x != 300.0f ||
        last_walk_y != -20.0f || last_aggression != 0.83f)
        return 170;

    reset_case(&state);
    ctf_result = 1;
    carrying_flag_result = 1;
    ctf_own_flag_world = 70;
    ctf_own_x = 20.0f;
    ctf_own_y = 30.0f;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_END_OF_WAYPOINT, 0, 3710);
    nox_bot_warrior_update(1, &state, 3710);
    if (guard_calls != 1 || walk_calls || last_guard_x != 20.0f ||
        last_guard_y != 30.0f || last_guard_radius != 20.0f ||
        last_aggression != 0.16f)
        return 171;

    /* Reference TeamBase follows the current own flag, including its carrier. */
    reset_case(&state);
    ctf_result = 1;
    carrying_flag_result = 1;
    ctf_own_flag_carrier = 72;
    ctf_own_x = -25.0f;
    ctf_own_y = 35.0f;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_END_OF_WAYPOINT, 0, 3715);
    nox_bot_warrior_update(1, &state, 3715);
    if (guard_calls != 1 || walk_calls || last_guard_x != -25.0f ||
        last_guard_y != 35.0f || last_guard_radius != 20.0f ||
        last_aggression != 0.16f)
        return 172;

    reset_case(&state);
    ctf_result = 1;
    ctf_own_flag_carrier = 72;
    ctf_enemy_flag_carrier = 73;
    ctf_own_x = -40.0f;
    ctf_own_y = 12.0f;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_END_OF_WAYPOINT, 0, 3720);
    nox_bot_warrior_update(1, &state, 3720);
    if (walk_calls != 1 || last_walk_x != -40.0f || last_walk_y != 12.0f ||
        last_aggression != 0.83f)
        return 173;

    reset_case(&state);
    ctf_result = 1;
    ctf_own_flag_carrier = 72;
    ctf_enemy_flag_world = 71;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_END_OF_WAYPOINT, 0, 3730);
    nox_bot_warrior_update(1, &state, 3730);
    if (walk_calls || guard_calls)
        return 174;
    return 0;
}

static int test_ctf_lost_sight_returns_dropped_own_flag(void)
{
    nox_bot_policy_state state;

    reset_case(&state);
    ctf_result = 1;
    ctf_own_flag_world = 70;
    ctf_enemy_flag_world = 71;
    ctf_own_flag_at_home = 0;
    ctf_own_x = 40.0f;
    ctf_own_y = 45.0f;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_LOST_SIGHT, 44, 3800);
    nox_bot_warrior_update(1, &state, 3814);
    if (walk_calls)
        return 180;
    nox_bot_warrior_update(1, &state, 3815);
    if (walk_calls != 1 || last_walk_x != 40.0f || last_walk_y != 45.0f ||
        last_aggression != 0.16f ||
        !nox_bot_policy_event_pending(&state, NOX_BOT_EVENT_LOST_SIGHT))
        return 181;
    nox_bot_warrior_update(1, &state, 3845);
    if (nox_bot_policy_event_pending(&state, NOX_BOT_EVENT_LOST_SIGHT))
        return 182;

    reset_case(&state);
    ctf_result = 1;
    ctf_own_flag_world = 70;
    ctf_enemy_flag_world = 71;
    ctf_own_flag_at_home = 1;
    ctf_enemy_x = 200.0f;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_LOST_SIGHT, 44, 3900);
    nox_bot_warrior_update(1, &state, 3915);
    if (walk_calls != 1 || last_walk_x != 200.0f || last_aggression != 0.83f)
        return 183;
    return 0;
}

static int test_death_clears_warrior_tactical_state(void)
{
    nox_bot_policy_state state;

    reset_case(&state);
    self_health = 0;
    state.warrior.pending_ability = NOX_BOT_ABILITY_WARCRY;
    state.warrior.harpoon_charge_pending = 1;
    state.warrior.chakram_attack_active = 1;
    state.warrior.chakram_item = 401;
    nox_bot_policy_record_event(&state, NOX_BOT_EVENT_ENEMY_SIGHTED, 44, 1800);
    nox_bot_warrior_update(1, &state, 1800);
    if (state.pending_events || state.warrior.pending_ability ||
        state.warrior.harpoon_charge_pending || state.warrior.chakram_attack_active ||
        state.warrior.chakram_item || execute_calls)
        return 100;
    return 0;
}

int main(void)
{
    int result;

    result = test_eye_reaction_delay();
    if (result)
        return result;
    result = test_eye_heard_and_lost_sight();
    if (result)
        return result;
    result = test_health_potion_policy();
    if (result)
        return result;
    result = test_harpoon_is_immediate_and_blocks_warcry();
    if (result)
        return result;
    result = test_warcry_target_gates();
    if (result)
        return result;
    result = test_berserker_charge_event_priority();
    if (result)
        return result;
    result = test_berserker_charge_collision_delay();
    if (result)
        return result;
    result = test_periodic_close_range_scan();
    if (result)
        return result;
    result = test_harpoon_attachment_schedules_charge();
    if (result)
        return result;
    result = test_hit_breaks_attached_harpoon();
    if (result)
        return result;
    result = test_nearby_loot_scan_and_cadence();
    if (result)
        return result;
    result = test_weapon_preference_cadence();
    if (result)
        return result;
    result = test_enemy_events_start_native_chakram_attack();
    if (result)
        return result;
    result = test_failed_chakram_attack_restores_previous_weapon();
    if (result)
        return result;
    result = test_chakram_cooldown_and_native_release();
    if (result)
        return result;
    result = test_chakram_attack_blocks_conflicting_abilities();
    if (result)
        return result;
    result = test_non_ctf_potion_seek_and_waypoint_resume();
    if (result)
        return result;
    result = test_lost_sight_teleport_wake_pursuit();
    if (result)
        return result;
    result = test_ctf_end_waypoint_strategy();
    if (result)
        return result;
    result = test_ctf_lost_sight_returns_dropped_own_flag();
    if (result)
        return result;
    return test_death_clears_warrior_tactical_state();
}
