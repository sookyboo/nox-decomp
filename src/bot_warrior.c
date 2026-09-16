#include "bot_warrior.h"

#include "bot_engine.h"

#include <string.h>

#define NOX_BOT_BUFF_INVISIBLE 0
#define NOX_BOT_BUFF_INVULNERABLE 23
#define NOX_BOT_BUFF_HELD 5
#define NOX_BOT_WARRIOR_ESCAPE_SPELL "SLOW"
#define NOX_BOT_WARRIOR_BOMBER "Bomber"
#define NOX_BOT_WARRIOR_PROTECTED_HOLD_SECONDS 2u
#define NOX_BOT_WARRIOR_HEALTH 150
#define NOX_BOT_WARRIOR_POTION_HEALTH 100
#define NOX_BOT_WARRIOR_HEALTH_POTION "RedPotion"
#define NOX_BOT_LOST_SIGHT_DELAY 15u
#define NOX_BOT_WARRIOR_ABILITY_SCAN_RADIUS 150.0f
#define NOX_BOT_WARRIOR_LOOT_RADIUS 75.0f
#define NOX_BOT_WARRIOR_LOOT_SCAN_FRAMES 15u
#define NOX_BOT_WARRIOR_WEAPON_PREFERENCE_SECONDS 10u
#define NOX_BOT_WARRIOR_CHAKRAM "RoundChakram"
#define NOX_BOT_WARRIOR_CHAKRAM_COOLDOWN_SECONDS 10u
#define NOX_BOT_WARRIOR_POTION_SEEK_AGGRESSION 0.16f
#define NOX_BOT_WARRIOR_DEFAULT_AGGRESSION 0.83f
#define NOX_BOT_WARRIOR_CTF_GUARD_RADIUS 20.0f
#define NOX_BOT_WARRIOR_TELEPORT_WAKE "TeleportWake"
#define NOX_BOT_WARRIOR_TELEPORT_WAKE_RANGE 100.0f


static int nox_bot_warrior_in_range(
    float x1, float y1, float x2, float y2, float radius)
{
    float dx = x2 - x1;
    float dy = y2 - y1;

    return dx * dx + dy * dy <= radius * radius;
}

static uint32_t nox_bot_warrior_protected_hold_deadline(uint32_t frame)
{
    uint32_t duration = nox_bot_engine_fps() * NOX_BOT_WARRIOR_PROTECTED_HOLD_SECONDS;

    if (!duration)
        duration = 1;
    return frame + duration;
}

void nox_bot_warrior_observe_collision(
    int object, nox_bot_policy_state *state, int other, uint32_t frame)
{
    if (!object || !state || !state->active)
        return;

    /*
     * The native player collision hook runs before Charge collision handling.
     * Remember that ownership for one policy update; once native collision has
     * applied HELD, update() can distinguish the Warrior's own crash stun.
     */
    if (nox_bot_engine_ability_active(object, NOX_BOT_ABILITY_BERSERKER_CHARGE))
        state->warrior.charge_collision_pending = 1;

    /* Bot-Script deliberately lets enemy Bomber stun survive for two seconds. */
    if (other && nox_bot_engine_is_enemy(object, other) &&
        nox_bot_engine_is_object_type(other, NOX_BOT_WARRIOR_BOMBER))
        state->warrior.protected_hold_until =
            nox_bot_warrior_protected_hold_deadline(frame);
}

static void nox_bot_warrior_update_held_escape(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    int held = nox_bot_engine_has_buff(object, NOX_BOT_BUFF_HELD);

    if (state->warrior.charge_collision_pending) {
        /* Native Charge has completed by the time class policy runs. */
        state->warrior.charge_collision_pending = 0;
        if (held)
            state->warrior.protected_hold_until =
                nox_bot_warrior_protected_hold_deadline(frame);
    }

    if (!held) {
        if (state->warrior.protected_hold_until &&
            nox_bot_reaction_ready(frame, state->warrior.protected_hold_until))
            state->warrior.protected_hold_until = 0;
        return;
    }
    if (state->warrior.protected_hold_until &&
        !nox_bot_reaction_ready(frame, state->warrior.protected_hold_until))
        return;

    /*
     * Reproduce Warrior.Update(): direct NoxScript-style Slow on self, then
     * remove HELD. The Slow remains as the reference's reduced movement
     * penalty while ordinary non-protected stun is escaped.
     */
    nox_bot_engine_cast_script_self(object, NOX_BOT_WARRIOR_ESCAPE_SPELL);
    nox_bot_engine_remove_buff(object, NOX_BOT_BUFF_HELD);
    state->warrior.protected_hold_until = 0;
}

static int nox_bot_warrior_ctf_attack_or_defend(int object)
{
    float x;
    float y;
    int enemy_flag;
    int enemy_target;
    int own_flag;
    int own_target;

    if (!nox_bot_engine_is_ctf())
        return 0;
    own_flag = nox_bot_engine_ctf_flag_world(object, 1);
    enemy_flag = nox_bot_engine_ctf_flag_world(object, 0);
    own_target = own_flag ? own_flag : nox_bot_engine_ctf_flag_carrier(object, 1);
    enemy_target = enemy_flag ? enemy_flag : nox_bot_engine_ctf_flag_carrier(object, 0);

    /* Go TeamTank: a flag carrier guards the current own-flag/base position. */
    if (nox_bot_engine_carrying_ctf_flag(object)) {
        if (!own_target)
            return 0;
        nox_bot_engine_position(own_target, &x, &y);
        nox_bot_engine_set_aggression(object, NOX_BOT_WARRIOR_POTION_SEEK_AGGRESSION);
        nox_bot_engine_guard_position(object, x, y, NOX_BOT_WARRIOR_CTF_GUARD_RADIUS);
        return 1;
    }

    /* Own flag present: attack the enemy flag, or escort its native carrier. */
    if (own_flag) {
        if (!enemy_target)
            return 0;
        nox_bot_engine_position(enemy_target, &x, &y);
        nox_bot_engine_set_aggression(object, NOX_BOT_WARRIOR_DEFAULT_AGGRESSION);
        nox_bot_engine_walk_to(object, x, y);
        return 1;
    }

    /* Both flags carried: pursue the native carrier of our own flag. */
    if (!enemy_flag && own_target) {
        nox_bot_engine_position(own_target, &x, &y);
        nox_bot_engine_set_aggression(object, NOX_BOT_WARRIOR_DEFAULT_AGGRESSION);
        nox_bot_engine_walk_to(object, x, y);
        return 1;
    }
    return 0;
}

static void nox_bot_warrior_ctf_walk_to_own_flag(int object)
{
    float x;
    float y;
    int own_flag;

    if (!nox_bot_engine_is_ctf())
        return;
    own_flag = nox_bot_engine_ctf_flag_world(object, 1);
    if (own_flag && !nox_bot_engine_ctf_flag_at_home(own_flag)) {
        nox_bot_engine_position(own_flag, &x, &y);
        nox_bot_engine_set_aggression(object, NOX_BOT_WARRIOR_POTION_SEEK_AGGRESSION);
        nox_bot_engine_walk_to(object, x, y);
        return;
    }
    nox_bot_warrior_ctf_attack_or_defend(object);
}

static void nox_bot_warrior_start_teleport_wake_pursuit(
    int object, nox_bot_policy_state *state)
{
    float self_x;
    float self_y;
    float wake_x;
    float wake_y;
    uint32_t event_frame;
    int target;
    int wake;

    if (!nox_bot_policy_event_pending(state, NOX_BOT_EVENT_LOST_SIGHT))
        return;
    event_frame = nox_bot_policy_event_frame(state, NOX_BOT_EVENT_LOST_SIGHT);
    if (state->warrior.teleport_wake_event_seen &&
        state->warrior.teleport_wake_event_frame == event_frame)
        return;
    state->warrior.teleport_wake_event_seen = 1;
    state->warrior.teleport_wake_event_frame = event_frame;
    state->warrior.lost_sight_ctf_handled = 0;
    state->warrior.teleport_wake_tracking = 0;
    state->warrior.teleport_wake_target = 0;

    target = nox_bot_policy_event_object(state, NOX_BOT_EVENT_LOST_SIGHT);
    if (!target)
        target = nox_bot_engine_current_target(object);
    wake = nox_bot_engine_find_nearest_type(
        object, NOX_BOT_WARRIOR_TELEPORT_WAKE, 0.0f);
    if (!wake)
        return;
    nox_bot_engine_position(object, &self_x, &self_y);
    nox_bot_engine_position(wake, &wake_x, &wake_y);

    /*
     * The reference calls Attack first when the wake is already >100 units,
     * then immediately replaces that action with WalkTo(wake). Preserve that
     * target side effect, but only keep polling when the wake starts nearby.
     */
    if (!nox_bot_warrior_in_range(
            self_x, self_y, wake_x, wake_y, NOX_BOT_WARRIOR_TELEPORT_WAKE_RANGE)) {
        if (target)
            nox_bot_engine_attack_target(object, target);
    } else {
        state->warrior.teleport_wake_tracking = 1;
        state->warrior.teleport_wake_target = target;
        state->warrior.teleport_wake_x = wake_x;
        state->warrior.teleport_wake_y = wake_y;
    }
    nox_bot_engine_walk_to(object, wake_x, wake_y);
}

static void nox_bot_warrior_update_teleport_wake_pursuit(
    int object, nox_bot_policy_state *state)
{
    float self_x;
    float self_y;
    int target;

    if (!state->warrior.teleport_wake_tracking)
        return;
    target = state->warrior.teleport_wake_target;
    if (!target || nox_bot_engine_health(target) <= 0) {
        state->warrior.teleport_wake_tracking = 0;
        state->warrior.teleport_wake_target = 0;
        return;
    }
    nox_bot_engine_position(object, &self_x, &self_y);
    if (nox_bot_warrior_in_range(
            self_x,
            self_y,
            state->warrior.teleport_wake_x,
            state->warrior.teleport_wake_y,
            NOX_BOT_WARRIOR_TELEPORT_WAKE_RANGE))
        return;
    nox_bot_engine_attack_target(object, target);
    state->warrior.teleport_wake_tracking = 0;
    state->warrior.teleport_wake_target = 0;
}


static int nox_bot_warrior_pickup_type(int object, const char *type_name, int equip_kind)
{
    int item = nox_bot_engine_find_nearest_visible_type(
        object, type_name, NOX_BOT_WARRIOR_LOOT_RADIUS);

    if (!item || !nox_bot_engine_pickup_item(object, item))
        return 0;
    if (equip_kind == 1)
        nox_bot_engine_equip_weapon(object, item);
    else if (equip_kind == 2)
        nox_bot_engine_equip_armor(object, item);
    return 1;
}

static void nox_bot_warrior_loot_scan(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    static const char *const melee[] = {
        "GreatSword", "WarHammer", "MorningStar", "BattleAxe", "Sword", "OgreAxe"
    };
    static const char *const throwing[] = { "RoundChakram", "FanChakram" };
    static const char *const potions[] = { "RedPotion", "CurePoisonPotion" };
    static const char *const armor[] = {
        "Breastplate", "PlateLeggings", "PlateBoots", "PlateArms",
        "ChainTunic", "ChainLeggings", "LeatherArmoredBoots", "LeatherArmor",
        "LeatherLeggings", "LeatherArmbands", "LeatherBoots", "MedievalCloak",
        "MedievalShirt", "MedievalPants"
    };
    unsigned int i;

    if (state->warrior.next_loot_scan_frame &&
        !nox_bot_reaction_ready(frame, state->warrior.next_loot_scan_frame))
        return;
    state->warrior.next_loot_scan_frame = frame + NOX_BOT_WARRIOR_LOOT_SCAN_FRAMES;

    for (i = 0; i < sizeof(melee) / sizeof(melee[0]); ++i)
        nox_bot_warrior_pickup_type(object, melee[i], 1);
    for (i = 0; i < sizeof(throwing) / sizeof(throwing[0]); ++i)
        nox_bot_warrior_pickup_type(object, throwing[i], 0);
    for (i = 0; i < sizeof(potions) / sizeof(potions[0]); ++i)
        nox_bot_warrior_pickup_type(object, potions[i], 0);
    for (i = 0; i < sizeof(armor) / sizeof(armor[0]); ++i)
        nox_bot_warrior_pickup_type(object, armor[i], 2);
}

static int nox_bot_warrior_apply_weapon_preference(int object)
{
    int item = nox_bot_engine_inventory_item(object, "GreatSword");

    if (!item)
        item = nox_bot_engine_inventory_item(object, "WarHammer");
    if (!item)
        item = nox_bot_engine_inventory_item(object, "Longsword");
    return item && nox_bot_engine_equip_weapon(object, item);
}

static void nox_bot_warrior_weapon_preference(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    uint32_t interval;

    if (state->warrior.next_weapon_preference_frame &&
        !nox_bot_reaction_ready(frame, state->warrior.next_weapon_preference_frame))
        return;
    interval = nox_bot_engine_fps() * NOX_BOT_WARRIOR_WEAPON_PREFERENCE_SECONDS;
    if (!interval)
        interval = 1;
    state->warrior.next_weapon_preference_frame = frame + interval;
    nox_bot_warrior_apply_weapon_preference(object);
}

static int nox_bot_warrior_chakram_ready(
    const nox_bot_policy_state *state, uint32_t frame)
{
    return !state->warrior.chakram_ready_frame ||
        nox_bot_reaction_ready(frame, state->warrior.chakram_ready_frame);
}

static int nox_bot_warrior_try_chakram_target(
    int object, nox_bot_policy_state *state, uint32_t frame, int target)
{
    uint32_t interval;
    int item;
    int previous_weapon;

    if (!target || nox_bot_engine_health(target) <= 0 ||
        state->warrior.chakram_attack_active ||
        !nox_bot_warrior_chakram_ready(state, frame) ||
        nox_bot_engine_ability_active(object, NOX_BOT_ABILITY_BERSERKER_CHARGE) ||
        nox_bot_engine_ability_active(object, NOX_BOT_ABILITY_WARCRY))
        return 0;
    item = nox_bot_engine_inventory_item(object, NOX_BOT_WARRIOR_CHAKRAM);
    if (!item)
        return 0;
    previous_weapon = nox_bot_engine_equipped_weapon(object);
    if (!nox_bot_engine_equip_weapon(object, item))
        return 0;

    nox_bot_engine_face_target(object, target);
    nox_bot_engine_interrupt(object);
    if (!nox_bot_engine_start_player_attack(object)) {
        if (previous_weapon)
            nox_bot_engine_equip_weapon(object, previous_weapon);
        else
            nox_bot_warrior_apply_weapon_preference(object);
        return 0;
    }

    interval = nox_bot_engine_fps() * NOX_BOT_WARRIOR_CHAKRAM_COOLDOWN_SECONDS;
    if (!interval)
        interval = 1;
    state->warrior.chakram_ready_frame = frame + interval;
    state->warrior.chakram_item = item;
    state->warrior.chakram_attack_active = 1;
    return 1;
}

static int nox_bot_warrior_update_chakram_attack(
    int object, nox_bot_policy_state *state)
{
    int item;

    if (!state->warrior.chakram_attack_active)
        return 0;
    item = state->warrior.chakram_item;
    if (!nox_bot_engine_player_attack_step(object) ||
        nox_bot_engine_equipped_weapon(object) != item) {
        state->warrior.chakram_attack_active = 0;
        state->warrior.chakram_item = 0;
        nox_bot_warrior_apply_weapon_preference(object);
        return 0;
    }
    return 1;
}

static int nox_bot_warrior_event_due(
    const nox_bot_policy_state *state,
    nox_bot_event event,
    uint32_t frame,
    uint32_t extra_delay)
{
    uint32_t deadline;

    if (!nox_bot_policy_event_pending(state, event))
        return 0;
    deadline = nox_bot_policy_event_frame(state, event) + extra_delay +
        nox_bot_reaction_frames((nox_bot_difficulty)state->difficulty);
    return nox_bot_reaction_ready(frame, deadline);
}

static void nox_bot_warrior_try_eye(int object, nox_bot_policy_state *state, nox_bot_event event)
{
    if (nox_bot_engine_ability_ready(object, NOX_BOT_ABILITY_EYE_OF_THE_WOLF))
        nox_bot_engine_execute_ability(object, NOX_BOT_ABILITY_EYE_OF_THE_WOLF);
    nox_bot_policy_clear_event(state, event);
}

static int nox_bot_warrior_valid_harpoon_target(int object, int target)
{
    if (!target || !nox_bot_engine_is_enemy(object, target))
        return 0;
    if (!nox_bot_engine_can_interact(object, target))
        return 0;
    if (nox_bot_engine_has_buff(target, NOX_BOT_BUFF_INVULNERABLE))
        return 0;
    return !nox_bot_engine_ability_active(object, NOX_BOT_ABILITY_BERSERKER_CHARGE);
}

static int nox_bot_warrior_try_harpoon_target(int object, int target)
{
    if (!nox_bot_warrior_valid_harpoon_target(object, target) ||
        !nox_bot_engine_ability_ready(object, NOX_BOT_ABILITY_HARPOON))
        return 0;
    nox_bot_engine_face_target(object, target);
    return nox_bot_engine_execute_ability(object, NOX_BOT_ABILITY_HARPOON);
}

static void nox_bot_warrior_try_harpoon_now(
    int object,
    nox_bot_policy_state *state,
    nox_bot_event event,
    uint32_t frame)
{
    int target;

    if (!nox_bot_policy_event_pending(state, event) ||
        nox_bot_policy_event_frame(state, event) != frame)
        return;
    target = nox_bot_policy_event_object(state, event);
    if (!target)
        target = nox_bot_engine_current_target(object);
    if (nox_bot_warrior_try_harpoon_target(object, target))
        nox_bot_policy_clear_event(state, event);
}

static int nox_bot_warrior_valid_charge_target(int object, int target)
{
    int harpoon_target;

    if (!target || !nox_bot_engine_is_enemy(object, target))
        return 0;
    if (!nox_bot_engine_can_interact(object, target))
        return 0;
    if (nox_bot_engine_has_buff(target, NOX_BOT_BUFF_INVULNERABLE))
        return 0;
    if (nox_bot_engine_ability_active(object, NOX_BOT_ABILITY_HARPOON)) {
        /* Go blocks Charge while Harpoon is flying, but permits the reel follow-up. */
        harpoon_target = nox_bot_engine_harpoon_attached_target(object);
        if (!harpoon_target || harpoon_target != target)
            return 0;
    }
    return 1;
}

static int nox_bot_warrior_try_charge_target(int object, int target)
{
    if (!nox_bot_warrior_valid_charge_target(object, target) ||
        !nox_bot_engine_ability_ready(object, NOX_BOT_ABILITY_BERSERKER_CHARGE))
        return 0;
    nox_bot_engine_face_target(object, target);
    return nox_bot_engine_execute_ability(object, NOX_BOT_ABILITY_BERSERKER_CHARGE);
}

static int nox_bot_warrior_try_charge(
    int object,
    nox_bot_policy_state *state,
    nox_bot_event event)
{
    int target = nox_bot_policy_event_object(state, event);

    if (!target)
        target = nox_bot_engine_current_target(object);
    if (!nox_bot_warrior_try_charge_target(object, target))
        return 0;
    nox_bot_policy_clear_event(state, event);
    return 1;
}

static void nox_bot_warrior_try_collision_charge(
    int object,
    nox_bot_policy_state *state,
    uint32_t frame)
{
    nox_bot_difficulty difficulty;
    int target;

    difficulty = (nox_bot_difficulty)state->difficulty;
    if (!nox_bot_warrior_event_due(
            state,
            NOX_BOT_EVENT_COLLISION,
            frame,
            nox_bot_reaction_frames(difficulty)))
        return;
    target = nox_bot_policy_event_object(state, NOX_BOT_EVENT_COLLISION);
    if (target && target == nox_bot_engine_current_target(object) &&
        nox_bot_warrior_valid_charge_target(object, target) &&
        nox_bot_engine_ability_ready(object, NOX_BOT_ABILITY_BERSERKER_CHARGE)) {
        nox_bot_engine_face_target(object, target);
        nox_bot_engine_execute_ability(object, NOX_BOT_ABILITY_BERSERKER_CHARGE);
    }
    nox_bot_policy_clear_event(state, NOX_BOT_EVENT_COLLISION);
}

static void nox_bot_warrior_use_potion(int object)
{
    int health = nox_bot_engine_health(object);
    int max_health = nox_bot_engine_max_health(object);

    if (health > 0 && health <= NOX_BOT_WARRIOR_POTION_HEALTH && health < max_health)
        nox_bot_engine_use_inventory_potion(object, NOX_BOT_WARRIOR_HEALTH_POTION);
}

static int nox_bot_warrior_valid_warcry_target(int object, int target)
{
    if (!target || !nox_bot_engine_is_enemy(object, target))
        return 0;
    if (!nox_bot_engine_can_interact(object, target))
        return 0;
    if (nox_bot_engine_has_buff(target, NOX_BOT_BUFF_INVULNERABLE))
        return 0;
    /* The Go reference deliberately avoids using War Cry against Warriors. */
    return nox_bot_engine_max_health(target) != NOX_BOT_WARRIOR_HEALTH;
}

static int nox_bot_warrior_try_warcry_target(int object, int target)
{
    if (nox_bot_engine_ability_active(object, NOX_BOT_ABILITY_BERSERKER_CHARGE) ||
        nox_bot_engine_ability_active(object, NOX_BOT_ABILITY_HARPOON) ||
        !nox_bot_warrior_valid_warcry_target(object, target) ||
        !nox_bot_engine_ability_ready(object, NOX_BOT_ABILITY_WARCRY))
        return 0;
    nox_bot_engine_face_target(object, target);
    return nox_bot_engine_execute_ability(object, NOX_BOT_ABILITY_WARCRY);
}

static void nox_bot_warrior_try_warcry(
    int object,
    nox_bot_policy_state *state,
    nox_bot_event event)
{
    int target = nox_bot_policy_event_object(state, event);

    if (!target)
        target = nox_bot_engine_current_target(object);
    nox_bot_warrior_try_warcry_target(object, target);
    nox_bot_policy_clear_event(state, event);
}

static int nox_bot_warrior_target_in_scan_range(int object, int target)
{
    float self_x;
    float self_y;
    float target_x;
    float target_y;
    float dx;
    float dy;

    if (!target || nox_bot_engine_health(target) <= 0)
        return 0;
    nox_bot_engine_position(object, &self_x, &self_y);
    nox_bot_engine_position(target, &target_x, &target_y);
    dx = target_x - self_x;
    dy = target_y - self_y;
    return dx * dx + dy * dy <=
        NOX_BOT_WARRIOR_ABILITY_SCAN_RADIUS * NOX_BOT_WARRIOR_ABILITY_SCAN_RADIUS;
}

static void nox_bot_warrior_start_potion_seek(
    int object, nox_bot_policy_state *state)
{
    float x;
    float y;
    int potion;
    int target;

    if (state->warrior.seeking_potion ||
        nox_bot_engine_health(object) >= NOX_BOT_WARRIOR_POTION_HEALTH)
        return;
    target = nox_bot_engine_current_target(object);
    if (!target || nox_bot_engine_health(target) <= 10)
        return;
    potion = nox_bot_engine_find_nearest_type(
        object, NOX_BOT_WARRIOR_HEALTH_POTION, 0.0f);
    if (!potion)
        return;
    if (nox_bot_engine_is_ctf() && nox_bot_engine_carrying_ctf_flag(object) &&
        !nox_bot_engine_can_interact(object, potion))
        return;
    nox_bot_engine_position(potion, &x, &y);
    if (!nox_bot_engine_set_aggression(object, NOX_BOT_WARRIOR_POTION_SEEK_AGGRESSION))
        return;
    state->warrior.seeking_potion = 1;
    nox_bot_engine_walk_to(object, x, y);
}

static void nox_bot_warrior_process_hit(int object, nox_bot_policy_state *state)
{
    if (!nox_bot_policy_event_pending(state, NOX_BOT_EVENT_IS_HIT))
        return;
    if (nox_bot_engine_harpoon_attached_target(object))
        nox_bot_engine_stop_harpoon(object);
    nox_bot_warrior_start_potion_seek(object, state);
    nox_bot_policy_clear_event(state, NOX_BOT_EVENT_IS_HIT);
}

static void nox_bot_warrior_process_end_waypoint(
    int object, nox_bot_policy_state *state)
{
    if (!nox_bot_policy_event_pending(state, NOX_BOT_EVENT_END_OF_WAYPOINT))
        return;
    state->warrior.seeking_potion = 0;
    nox_bot_engine_set_aggression(object, NOX_BOT_WARRIOR_DEFAULT_AGGRESSION);
    if (nox_bot_engine_is_ctf())
        nox_bot_warrior_ctf_attack_or_defend(object);
    else
        nox_bot_engine_hunt(object);
    nox_bot_policy_clear_event(state, NOX_BOT_EVENT_END_OF_WAYPOINT);
}

static void nox_bot_warrior_process_lost_sight(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    uint32_t ctf_deadline;

    if (!nox_bot_policy_event_pending(state, NOX_BOT_EVENT_LOST_SIGHT))
        return;
    ctf_deadline = nox_bot_policy_event_frame(state, NOX_BOT_EVENT_LOST_SIGHT) +
        NOX_BOT_LOST_SIGHT_DELAY;
    if (nox_bot_engine_is_ctf() && !state->warrior.lost_sight_ctf_handled &&
        nox_bot_reaction_ready(frame, ctf_deadline)) {
        nox_bot_warrior_ctf_walk_to_own_flag(object);
        state->warrior.lost_sight_ctf_handled = 1;
    }
    if (!nox_bot_warrior_event_due(
            state, NOX_BOT_EVENT_LOST_SIGHT, frame, NOX_BOT_LOST_SIGHT_DELAY))
        return;
    if (nox_bot_engine_ability_ready(object, NOX_BOT_ABILITY_EYE_OF_THE_WOLF))
        nox_bot_engine_execute_ability(object, NOX_BOT_ABILITY_EYE_OF_THE_WOLF);
    state->warrior.teleport_wake_event_seen = 0;
    state->warrior.teleport_wake_event_frame = 0;
    nox_bot_policy_clear_event(state, NOX_BOT_EVENT_LOST_SIGHT);
}

static void nox_bot_warrior_track_harpoon_charge(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    int target = nox_bot_engine_harpoon_attached_target(object);

    if (target && target != state->warrior.last_harpoon_target &&
        nox_bot_warrior_valid_charge_target(object, target) &&
        nox_bot_engine_ability_ready(object, NOX_BOT_ABILITY_BERSERKER_CHARGE)) {
        state->warrior.harpoon_charge_pending = 1;
        state->warrior.harpoon_charge_target = target;
        state->warrior.harpoon_charge_frame = frame +
            nox_bot_reaction_frames((nox_bot_difficulty)state->difficulty);
    }
    state->warrior.last_harpoon_target = target;
}

static void nox_bot_warrior_process_harpoon_charge(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    int target;

    if (!state->warrior.harpoon_charge_pending ||
        !nox_bot_reaction_ready(frame, state->warrior.harpoon_charge_frame))
        return;
    target = state->warrior.harpoon_charge_target;
    state->warrior.harpoon_charge_pending = 0;
    state->warrior.harpoon_charge_target = 0;
    state->warrior.harpoon_charge_frame = 0;
    nox_bot_warrior_try_charge_target(object, target);
}

static void nox_bot_warrior_process_periodic_action(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    int ability;
    int target;

    if (!state->warrior.pending_ability ||
        !nox_bot_reaction_ready(frame, state->warrior.pending_ability_frame))
        return;
    ability = state->warrior.pending_ability;
    target = state->warrior.pending_ability_target;
    state->warrior.pending_ability = 0;
    state->warrior.pending_ability_target = 0;
    state->warrior.pending_ability_frame = 0;
    if (ability == NOX_BOT_ABILITY_BERSERKER_CHARGE)
        nox_bot_warrior_try_charge_target(object, target);
    else if (ability == NOX_BOT_ABILITY_WARCRY)
        nox_bot_warrior_try_warcry_target(object, target);
}

static void nox_bot_warrior_periodic_ability_scan(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    uint32_t interval;
    int target;

    if (state->warrior.next_ability_scan_frame &&
        !nox_bot_reaction_ready(frame, state->warrior.next_ability_scan_frame))
        return;
    interval = nox_bot_engine_fps();
    if (!interval)
        interval = 1;
    state->warrior.next_ability_scan_frame = frame + interval;
    /* The Go timer still fires every second while another ability is reacting. */
    if (state->warrior.pending_ability)
        return;
    target = nox_bot_engine_current_target(object);
    if (!nox_bot_warrior_target_in_scan_range(object, target))
        return;

    /* Go checks Harpoon, Charge, then War Cry once per second at close range. */
    if (nox_bot_warrior_try_harpoon_target(object, target))
        return;
    if (nox_bot_warrior_valid_charge_target(object, target) &&
        nox_bot_engine_ability_ready(object, NOX_BOT_ABILITY_BERSERKER_CHARGE)) {
        state->warrior.pending_ability = NOX_BOT_ABILITY_BERSERKER_CHARGE;
    } else if (!nox_bot_engine_ability_active(
                   object, NOX_BOT_ABILITY_BERSERKER_CHARGE) &&
               !nox_bot_engine_ability_active(object, NOX_BOT_ABILITY_HARPOON) &&
               nox_bot_warrior_valid_warcry_target(object, target) &&
               nox_bot_engine_ability_ready(object, NOX_BOT_ABILITY_WARCRY)) {
        state->warrior.pending_ability = NOX_BOT_ABILITY_WARCRY;
    } else {
        return;
    }
    state->warrior.pending_ability_target = target;
    state->warrior.pending_ability_frame = frame +
        nox_bot_reaction_frames((nox_bot_difficulty)state->difficulty);
}

void nox_bot_warrior_update(int object, nox_bot_policy_state *state, uint32_t frame)
{
    int heard;
    int heard_chakram_target = 0;
    int sighted_chakram_target = 0;

    if (!object || !state || !state->active)
        return;
    if (nox_bot_engine_health(object) <= 0) {
        state->pending_events = 0;
        memset(&state->warrior, 0, sizeof(state->warrior));
        return;
    }

    nox_bot_warrior_use_potion(object);
    nox_bot_warrior_update_held_escape(object, state, frame);
    nox_bot_warrior_start_teleport_wake_pursuit(object, state);
    nox_bot_warrior_update_teleport_wake_pursuit(object, state);
    if (!nox_bot_warrior_update_chakram_attack(object, state)) {
        nox_bot_warrior_loot_scan(object, state, frame);
        nox_bot_warrior_weapon_preference(object, state, frame);
    }
    nox_bot_warrior_process_hit(object, state);
    nox_bot_warrior_process_end_waypoint(object, state);
    nox_bot_warrior_track_harpoon_charge(object, state, frame);
    if (!state->warrior.chakram_attack_active)
        nox_bot_warrior_process_harpoon_charge(object, state, frame);

    /*
     * Enemy Sighted calls Harpoon, Charge, War Cry, then ThrowChakram in the
     * Go reference. Preserve the event target before Harpoon may consume it so
     * the native player attack can still throw the Chakram in that same tick.
     */
    if (nox_bot_policy_event_pending(state, NOX_BOT_EVENT_ENEMY_SIGHTED) &&
        nox_bot_policy_event_frame(state, NOX_BOT_EVENT_ENEMY_SIGHTED) == frame) {
        sighted_chakram_target =
            nox_bot_policy_event_object(state, NOX_BOT_EVENT_ENEMY_SIGHTED);
        if (!sighted_chakram_target)
            sighted_chakram_target = nox_bot_engine_current_target(object);
    }
    if (nox_bot_policy_event_pending(state, NOX_BOT_EVENT_ENEMY_HEARD) &&
        nox_bot_policy_event_frame(state, NOX_BOT_EVENT_ENEMY_HEARD) == frame)
        heard_chakram_target = nox_bot_engine_current_target(object);

    /* Harpoon is immediate in the Go event callbacks; consider each event once. */
    nox_bot_warrior_try_harpoon_now(object, state, NOX_BOT_EVENT_ENEMY_SIGHTED, frame);
    nox_bot_warrior_try_harpoon_now(object, state, NOX_BOT_EVENT_CHANGE_FOCUS, frame);

    if (nox_bot_warrior_event_due(
            state, NOX_BOT_EVENT_LOOKING_FOR_ENEMY, frame, 0))
        nox_bot_warrior_try_eye(object, state, NOX_BOT_EVENT_LOOKING_FOR_ENEMY);

    if (nox_bot_warrior_event_due(state, NOX_BOT_EVENT_ENEMY_HEARD, frame, 0)) {
        heard = nox_bot_policy_event_object(state, NOX_BOT_EVENT_ENEMY_HEARD);
        if (heard && nox_bot_engine_has_buff(heard, NOX_BOT_BUFF_INVISIBLE))
            nox_bot_warrior_try_eye(object, state, NOX_BOT_EVENT_ENEMY_HEARD);
        else
            nox_bot_policy_clear_event(state, NOX_BOT_EVENT_ENEMY_HEARD);
    }

    nox_bot_warrior_process_lost_sight(object, state, frame);

    if (!state->warrior.chakram_attack_active) {
        nox_bot_warrior_try_collision_charge(object, state, frame);

        if (nox_bot_warrior_event_due(state, NOX_BOT_EVENT_ENEMY_SIGHTED, frame, 0) &&
            !nox_bot_warrior_try_charge(object, state, NOX_BOT_EVENT_ENEMY_SIGHTED))
            nox_bot_warrior_try_warcry(object, state, NOX_BOT_EVENT_ENEMY_SIGHTED);

        if (nox_bot_warrior_event_due(state, NOX_BOT_EVENT_CHANGE_FOCUS, frame, 0) &&
            !nox_bot_warrior_try_charge(object, state, NOX_BOT_EVENT_CHANGE_FOCUS))
            nox_bot_warrior_try_warcry(object, state, NOX_BOT_EVENT_CHANGE_FOCUS);

        if (sighted_chakram_target)
            nox_bot_warrior_try_chakram_target(
                object, state, frame, sighted_chakram_target);
        if (!state->warrior.chakram_attack_active && heard_chakram_target)
            nox_bot_warrior_try_chakram_target(
                object, state, frame, heard_chakram_target);
    }

    /* A native player weapon attack owns player state 1 until it releases. */
    if (state->warrior.chakram_attack_active)
        return;

    nox_bot_warrior_process_periodic_action(object, state, frame);
    nox_bot_warrior_periodic_ability_scan(object, state, frame);
    /* A zero-reaction periodic decision is eligible in the same simulation tick. */
    nox_bot_warrior_process_periodic_action(object, state, frame);
}
