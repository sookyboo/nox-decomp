#include "bot_wizard.h"

#include "bot_engine.h"
#include "bot_team.h"

#include <string.h>

#define NOX_BOT_ENCHANT_INVISIBLE 0
#define NOX_BOT_ENCHANT_SLOWED 4
#define NOX_BOT_ENCHANT_HELD 5
#define NOX_BOT_ENCHANT_HASTED 9
#define NOX_BOT_ENCHANT_PROTECT_FROM_FIRE 17
#define NOX_BOT_ENCHANT_PROTECT_FROM_ELECTRICITY 20
#define NOX_BOT_ENCHANT_SHOCK 22
#define NOX_BOT_ENCHANT_INVULNERABLE 23
#define NOX_BOT_ENCHANT_SHIELD 26
#define NOX_BOT_ENCHANT_REFLECTIVE_SHIELD 27
#define NOX_BOT_ENCHANT_ANTI_MAGIC 29

#define NOX_BOT_WIZARD_GLOBAL_COOLDOWN_FRAMES 3u
#define NOX_BOT_WIZARD_LOOT_RADIUS 75.0f
#define NOX_BOT_WIZARD_LOOT_SCAN_FRAMES 15u
#define NOX_BOT_WIZARD_WEAPON_PREFERENCE_SECONDS 10u

typedef enum nox_bot_wizard_spell {
    NOX_BOT_WIZARD_SPELL_NONE = 0,
    NOX_BOT_WIZARD_SPELL_SLOW,
    NOX_BOT_WIZARD_SPELL_DEATH_RAY,
    NOX_BOT_WIZARD_SPELL_FIREBALL,
    NOX_BOT_WIZARD_SPELL_BURN,
    NOX_BOT_WIZARD_SPELL_MAGIC_MISSILE,
    NOX_BOT_WIZARD_SPELL_COUNTERSPELL,
    NOX_BOT_WIZARD_SPELL_SHIELD,
    NOX_BOT_WIZARD_SPELL_LESSER_HEAL,
    NOX_BOT_WIZARD_SPELL_HASTE,
    NOX_BOT_WIZARD_SPELL_SHOCK,
    NOX_BOT_WIZARD_SPELL_PROTECT_SHOCK,
    NOX_BOT_WIZARD_SPELL_PROTECT_FIRE,
    NOX_BOT_WIZARD_SPELL_INVISIBILITY,
    NOX_BOT_WIZARD_SPELL_RING_OF_FIRE,
    NOX_BOT_WIZARD_SPELL_ENERGY_BOLT,
} nox_bot_wizard_spell;

typedef enum nox_bot_wizard_cast_kind {
    NOX_BOT_WIZARD_CAST_SELF = 0,
    NOX_BOT_WIZARD_CAST_OBJECT,
    NOX_BOT_WIZARD_CAST_POSITION,
} nox_bot_wizard_cast_kind;

typedef struct nox_bot_wizard_spell_def {
    const char *name;
    unsigned short mana_required;
    unsigned short mana_cost;
    unsigned short cooldown_seconds;
    unsigned short cooldown_frames;
    unsigned char cast_kind;
} nox_bot_wizard_spell_def;

static const nox_bot_wizard_spell_def nox_bot_wizard_spells[] = {
    { 0, 0, 0, 0, 0, 0 },
    { "SLOW", 10, 10, 3, 0, NOX_BOT_WIZARD_CAST_OBJECT },
    { "DEATH_RAY", 60, 60, 5, 0, NOX_BOT_WIZARD_CAST_POSITION },
    { "FIREBALL", 30, 30, 5, 0, NOX_BOT_WIZARD_CAST_POSITION },
    { "BURN", 10, 10, 0, 3, NOX_BOT_WIZARD_CAST_POSITION },
    { "MAGIC_MISSILE", 15, 15, 3, 0, NOX_BOT_WIZARD_CAST_OBJECT },
    { "COUNTERSPELL", 20, 20, 20, 0, NOX_BOT_WIZARD_CAST_POSITION },
    { "SHIELD", 80, 80, 10, 0, NOX_BOT_WIZARD_CAST_SELF },
    { "LESSER_HEAL", 30, 30, 1, 0, NOX_BOT_WIZARD_CAST_SELF },
    { "HASTE", 10, 10, 20, 0, NOX_BOT_WIZARD_CAST_SELF },
    { "SHOCK", 30, 30, 5, 0, NOX_BOT_WIZARD_CAST_SELF },
    { "PROTECTION_FROM_ELECTRICITY", 30, 30, 60, 0, NOX_BOT_WIZARD_CAST_SELF },
    { "PROTECTION_FROM_FIRE", 30, 30, 60, 0, NOX_BOT_WIZARD_CAST_SELF },
    { "INVISIBILITY", 30, 30, 60, 0, NOX_BOT_WIZARD_CAST_SELF },
    /* Bot-Script's RingOfFireReady is never re-enabled after the first cast. */
    { "CLEANSING_FLAME", 60, 60, 0, 0, NOX_BOT_WIZARD_CAST_SELF },
    /* Bot-Script checks mana > 10 for Energy Bolt but does not deduct mana. */
    { "LIGHTNING", 11, 0, 3, 0, NOX_BOT_WIZARD_CAST_OBJECT },
};

static uint32_t *nox_bot_wizard_ready_frame(
    nox_bot_wizard_policy_state *wizard, nox_bot_wizard_spell spell)
{
    switch (spell) {
    case NOX_BOT_WIZARD_SPELL_SLOW:
        return &wizard->slow_ready_frame;
    case NOX_BOT_WIZARD_SPELL_DEATH_RAY:
        return &wizard->death_ray_ready_frame;
    case NOX_BOT_WIZARD_SPELL_FIREBALL:
        return &wizard->fireball_ready_frame;
    case NOX_BOT_WIZARD_SPELL_BURN:
        return &wizard->burn_ready_frame;
    case NOX_BOT_WIZARD_SPELL_MAGIC_MISSILE:
        return &wizard->magic_missile_ready_frame;
    case NOX_BOT_WIZARD_SPELL_COUNTERSPELL:
        return &wizard->counterspell_ready_frame;
    case NOX_BOT_WIZARD_SPELL_SHIELD:
        return &wizard->shield_ready_frame;
    case NOX_BOT_WIZARD_SPELL_LESSER_HEAL:
        return &wizard->lesser_heal_ready_frame;
    case NOX_BOT_WIZARD_SPELL_HASTE:
        return &wizard->haste_ready_frame;
    case NOX_BOT_WIZARD_SPELL_SHOCK:
        return &wizard->shock_ready_frame;
    case NOX_BOT_WIZARD_SPELL_PROTECT_SHOCK:
        return &wizard->protect_shock_ready_frame;
    case NOX_BOT_WIZARD_SPELL_PROTECT_FIRE:
        return &wizard->protect_fire_ready_frame;
    case NOX_BOT_WIZARD_SPELL_INVISIBILITY:
        return &wizard->invisibility_ready_frame;
    case NOX_BOT_WIZARD_SPELL_ENERGY_BOLT:
        return &wizard->energy_bolt_ready_frame;
    default:
        return 0;
    }
}

static int nox_bot_wizard_deadline_ready(uint32_t frame, uint32_t deadline)
{
    return !deadline || nox_bot_reaction_ready(frame, deadline);
}

static int nox_bot_wizard_spell_ready(
    const nox_bot_wizard_policy_state *wizard, nox_bot_wizard_spell spell, uint32_t frame)
{
    const uint32_t *ready = 0;

    switch (spell) {
    case NOX_BOT_WIZARD_SPELL_SLOW:
        ready = &wizard->slow_ready_frame;
        break;
    case NOX_BOT_WIZARD_SPELL_DEATH_RAY:
        ready = &wizard->death_ray_ready_frame;
        break;
    case NOX_BOT_WIZARD_SPELL_FIREBALL:
        ready = &wizard->fireball_ready_frame;
        break;
    case NOX_BOT_WIZARD_SPELL_BURN:
        ready = &wizard->burn_ready_frame;
        break;
    case NOX_BOT_WIZARD_SPELL_MAGIC_MISSILE:
        ready = &wizard->magic_missile_ready_frame;
        break;
    case NOX_BOT_WIZARD_SPELL_COUNTERSPELL:
        ready = &wizard->counterspell_ready_frame;
        break;
    case NOX_BOT_WIZARD_SPELL_SHIELD:
        ready = &wizard->shield_ready_frame;
        break;
    case NOX_BOT_WIZARD_SPELL_LESSER_HEAL:
        ready = &wizard->lesser_heal_ready_frame;
        break;
    case NOX_BOT_WIZARD_SPELL_HASTE:
        ready = &wizard->haste_ready_frame;
        break;
    case NOX_BOT_WIZARD_SPELL_SHOCK:
        ready = &wizard->shock_ready_frame;
        break;
    case NOX_BOT_WIZARD_SPELL_PROTECT_SHOCK:
        ready = &wizard->protect_shock_ready_frame;
        break;
    case NOX_BOT_WIZARD_SPELL_PROTECT_FIRE:
        ready = &wizard->protect_fire_ready_frame;
        break;
    case NOX_BOT_WIZARD_SPELL_INVISIBILITY:
        ready = &wizard->invisibility_ready_frame;
        break;
    case NOX_BOT_WIZARD_SPELL_RING_OF_FIRE:
        return !wizard->ring_of_fire_used;
    case NOX_BOT_WIZARD_SPELL_ENERGY_BOLT:
        ready = &wizard->energy_bolt_ready_frame;
        break;
    default:
        break;
    }
    return ready && nox_bot_wizard_deadline_ready(frame, *ready);
}

static int nox_bot_wizard_global_ready(
    const nox_bot_wizard_policy_state *wizard, uint32_t frame)
{
    return wizard->pending_spell == NOX_BOT_WIZARD_SPELL_NONE &&
        nox_bot_wizard_deadline_ready(frame, wizard->global_ready_frame);
}

static int nox_bot_wizard_schedule(
    int object,
    nox_bot_policy_state *state,
    uint32_t frame,
    nox_bot_wizard_spell spell,
    int target,
    float x,
    float y)
{
    nox_bot_wizard_policy_state *wizard = &state->wizard;
    const nox_bot_wizard_spell_def *def;

    if (spell <= NOX_BOT_WIZARD_SPELL_NONE ||
        spell >= (int)(sizeof(nox_bot_wizard_spells) / sizeof(nox_bot_wizard_spells[0])))
        return 0;
    def = &nox_bot_wizard_spells[spell];
    if (!nox_bot_wizard_global_ready(wizard, frame) ||
        !nox_bot_wizard_spell_ready(wizard, spell, frame) ||
        nox_bot_engine_mana(object) < def->mana_required)
        return 0;

    wizard->pending_spell = (unsigned char)spell;
    wizard->pending_target = target;
    wizard->pending_x = x;
    wizard->pending_y = y;
    wizard->pending_cast_frame = nox_bot_reaction_deadline(
        frame, (nox_bot_difficulty)state->difficulty);
    return 1;
}

static int nox_bot_wizard_pending_target_valid(
    int object, const nox_bot_wizard_policy_state *wizard, nox_bot_wizard_spell spell)
{
    if (!wizard->pending_target || nox_bot_engine_health(wizard->pending_target) <= 0)
        return 0;
    if (spell == NOX_BOT_WIZARD_SPELL_DEATH_RAY ||
        spell == NOX_BOT_WIZARD_SPELL_BURN ||
        spell == NOX_BOT_WIZARD_SPELL_RING_OF_FIRE ||
        spell == NOX_BOT_WIZARD_SPELL_COUNTERSPELL)
        return nox_bot_engine_can_interact(object, wizard->pending_target);
    return 1;
}

static void nox_bot_wizard_cancel_pending(
    nox_bot_policy_state *state, uint32_t frame)
{
    state->wizard.pending_spell = NOX_BOT_WIZARD_SPELL_NONE;
    state->wizard.pending_target = 0;
    state->wizard.global_ready_frame = nox_bot_reaction_deadline(
        frame, (nox_bot_difficulty)state->difficulty);
}

static void nox_bot_wizard_finish_cast(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    nox_bot_wizard_policy_state *wizard = &state->wizard;
    nox_bot_wizard_spell spell = (nox_bot_wizard_spell)wizard->pending_spell;
    const nox_bot_wizard_spell_def *def;
    uint32_t *ready;
    uint32_t cooldown;

    if (spell == NOX_BOT_WIZARD_SPELL_NONE ||
        !nox_bot_reaction_ready(frame, wizard->pending_cast_frame))
        return;

    def = &nox_bot_wizard_spells[spell];
    if (nox_bot_engine_health(object) <= 0 ||
        nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_ANTI_MAGIC)) {
        nox_bot_wizard_cancel_pending(state, frame);
        return;
    }
    if (nox_bot_engine_mana(object) < def->mana_required) {
        nox_bot_wizard_cancel_pending(state, frame);
        return;
    }

    if ((def->cast_kind == NOX_BOT_WIZARD_CAST_OBJECT || wizard->pending_target) &&
        !nox_bot_wizard_pending_target_valid(object, wizard, spell)) {
        nox_bot_wizard_cancel_pending(state, frame);
        return;
    }

    /* The Go reference deducts bot-local mana immediately before CastSpell,
     * except Energy Bolt where the script checks mana but never subtracts it. */
    if (def->mana_cost)
        nox_bot_engine_mana_sub(object, def->mana_cost);
    if (spell == NOX_BOT_WIZARD_SPELL_RING_OF_FIRE && wizard->pending_target)
        nox_bot_engine_face_target(object, wizard->pending_target);
    if (def->cast_kind == NOX_BOT_WIZARD_CAST_SELF)
        nox_bot_engine_cast_script_self(object, def->name);
    else if (def->cast_kind == NOX_BOT_WIZARD_CAST_OBJECT)
        nox_bot_engine_cast_script_object(object, def->name, wizard->pending_target);
    else
        nox_bot_engine_cast_script_position(object, def->name, wizard->pending_x, wizard->pending_y);

    wizard->pending_spell = NOX_BOT_WIZARD_SPELL_NONE;
    wizard->pending_target = 0;
    wizard->global_ready_frame = frame + NOX_BOT_WIZARD_GLOBAL_COOLDOWN_FRAMES;

    if (spell == NOX_BOT_WIZARD_SPELL_RING_OF_FIRE) {
        /* Reference quirk: the five-second timer sets ShockReady, not
         * RingOfFireReady, so Ring of Fire remains unavailable this life. */
        wizard->ring_of_fire_used = 1;
        return;
    }

    ready = nox_bot_wizard_ready_frame(wizard, spell);
    if (!ready)
        return;
    cooldown = def->cooldown_frames;
    if (!cooldown && def->cooldown_seconds) {
        cooldown = nox_bot_engine_fps() * def->cooldown_seconds;
        if (!cooldown)
            cooldown = 1;
    }
    *ready = cooldown ? frame + cooldown : frame;
}

static int nox_bot_wizard_within_radius(int object, int target, float radius)
{
    float self_x;
    float self_y;
    float target_x;
    float target_y;
    float dx;
    float dy;

    if (!object || !target || radius < 0.0f)
        return 0;
    nox_bot_engine_position(object, &self_x, &self_y);
    nox_bot_engine_position(target, &target_x, &target_y);
    dx = target_x - self_x;
    dy = target_y - self_y;
    return dx * dx + dy * dy <= radius * radius;
}

static int nox_bot_wizard_try_slow(
    int object, nox_bot_policy_state *state, uint32_t frame, int target)
{
    if (!target || nox_bot_engine_health(target) <= 0 ||
        nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_INVISIBLE) ||
        nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_ANTI_MAGIC) ||
        !nox_bot_engine_can_interact(object, target) ||
        nox_bot_engine_has_buff(target, NOX_BOT_ENCHANT_SLOWED) ||
        nox_bot_engine_has_buff(target, NOX_BOT_ENCHANT_REFLECTIVE_SHIELD))
        return 0;
    return nox_bot_wizard_schedule(
        object, state, frame, NOX_BOT_WIZARD_SPELL_SLOW, target, 0.0f, 0.0f);
}

static int nox_bot_wizard_try_visible_target(
    int object, nox_bot_policy_state *state, uint32_t frame, int target)
{
    float x;
    float y;
    int self_invisible;

    if (!target || nox_bot_engine_health(target) <= 0 ||
        !nox_bot_engine_can_interact(object, target) ||
        nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_ANTI_MAGIC))
        return 0;

    nox_bot_engine_position(target, &x, &y);
    self_invisible = nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_INVISIBLE);

    if ((nox_bot_engine_has_buff(target, NOX_BOT_ENCHANT_HELD) ||
         nox_bot_engine_has_buff(target, NOX_BOT_ENCHANT_SLOWED) || self_invisible) &&
        !nox_bot_engine_has_buff(target, NOX_BOT_ENCHANT_INVULNERABLE) &&
        !nox_bot_engine_has_buff(target, NOX_BOT_ENCHANT_REFLECTIVE_SHIELD) &&
        nox_bot_wizard_schedule(
            object, state, frame, NOX_BOT_WIZARD_SPELL_DEATH_RAY, target, x, y))
        return 1;

    if (!nox_bot_engine_has_buff(target, NOX_BOT_ENCHANT_INVULNERABLE) &&
        !nox_bot_engine_has_buff(target, NOX_BOT_ENCHANT_REFLECTIVE_SHIELD) &&
        nox_bot_wizard_schedule(
            object, state, frame, NOX_BOT_WIZARD_SPELL_FIREBALL, target, x, y))
        return 1;

    if (!self_invisible &&
        nox_bot_engine_has_buff(target, NOX_BOT_ENCHANT_REFLECTIVE_SHIELD) &&
        !nox_bot_engine_has_buff(target, NOX_BOT_ENCHANT_INVULNERABLE) &&
        nox_bot_wizard_schedule(
            object, state, frame, NOX_BOT_WIZARD_SPELL_BURN, target, x, y))
        return 1;

    if (!self_invisible &&
        nox_bot_engine_has_buff(target, NOX_BOT_ENCHANT_REFLECTIVE_SHIELD) &&
        !nox_bot_engine_has_buff(target, NOX_BOT_ENCHANT_INVULNERABLE) &&
        nox_bot_wizard_within_radius(object, target, 40.0f) &&
        nox_bot_wizard_schedule(
            object, state, frame, NOX_BOT_WIZARD_SPELL_RING_OF_FIRE, target, x, y))
        return 1;

    if (!self_invisible && nox_bot_wizard_try_slow(object, state, frame, target))
        return 1;

    if (!self_invisible &&
        !nox_bot_engine_has_buff(target, NOX_BOT_ENCHANT_INVULNERABLE) &&
        !nox_bot_engine_has_buff(target, NOX_BOT_ENCHANT_REFLECTIVE_SHIELD) &&
        nox_bot_wizard_within_radius(object, target, 200.0f) &&
        nox_bot_wizard_schedule(
            object, state, frame, NOX_BOT_WIZARD_SPELL_ENERGY_BOLT, target, x, y))
        return 1;

    if (!self_invisible &&
        !nox_bot_engine_has_buff(target, NOX_BOT_ENCHANT_INVULNERABLE) &&
        !nox_bot_engine_has_buff(target, NOX_BOT_ENCHANT_REFLECTIVE_SHIELD) &&
        nox_bot_wizard_schedule(
            object, state, frame, NOX_BOT_WIZARD_SPELL_MAGIC_MISSILE, target, x, y))
        return 1;

    if (!self_invisible && nox_bot_engine_has_buff(target, NOX_BOT_ENCHANT_SHOCK)) {
        nox_bot_engine_position(object, &x, &y);
        if (nox_bot_wizard_schedule(
                object, state, frame, NOX_BOT_WIZARD_SPELL_COUNTERSPELL, target, x, y))
            return 1;
    }
    return 0;
}

static int nox_bot_wizard_try_self_buffs(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    int mana = nox_bot_engine_mana(object);

    if (nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_INVISIBLE) ||
        nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_ANTI_MAGIC))
        return 0;

    if (!nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_SHIELD) &&
        nox_bot_wizard_schedule(
            object, state, frame, NOX_BOT_WIZARD_SPELL_SHIELD, object, 0.0f, 0.0f))
        return 1;

    /* Wizard.Update() only enters this three-spell block at >= 140 mana. */
    if (mana < 140)
        return 0;
    if (nox_bot_engine_health(object) <= 60 &&
        nox_bot_wizard_schedule(
            object, state, frame, NOX_BOT_WIZARD_SPELL_LESSER_HEAL, object, 0.0f, 0.0f))
        return 1;
    if (!nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_HASTED) &&
        nox_bot_wizard_schedule(
            object, state, frame, NOX_BOT_WIZARD_SPELL_HASTE, object, 0.0f, 0.0f))
        return 1;
    if (!nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_SHOCK) &&
        nox_bot_wizard_schedule(
            object, state, frame, NOX_BOT_WIZARD_SPELL_SHOCK, object, 0.0f, 0.0f))
        return 1;
    return 0;
}

static int nox_bot_wizard_try_hidden_target_buffs(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    if (nox_bot_engine_mana(object) < 140 ||
        nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_ANTI_MAGIC))
        return 0;
    if (!nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_PROTECT_FROM_ELECTRICITY) &&
        nox_bot_wizard_schedule(
            object, state, frame, NOX_BOT_WIZARD_SPELL_PROTECT_SHOCK, object, 0.0f, 0.0f))
        return 1;
    if (!nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_PROTECT_FROM_FIRE) &&
        nox_bot_wizard_schedule(
            object, state, frame, NOX_BOT_WIZARD_SPELL_PROTECT_FIRE, object, 0.0f, 0.0f))
        return 1;
    if (!nox_bot_team_is_ctf_tank(object) &&
        !nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_INVISIBLE) &&
        nox_bot_wizard_schedule(
            object, state, frame, NOX_BOT_WIZARD_SPELL_INVISIBILITY, object, 0.0f, 0.0f))
        return 1;
    return 0;
}

static int nox_bot_wizard_try_deathball_counterspell(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    float x;
    float y;

    if (nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_ANTI_MAGIC) ||
        !nox_bot_engine_find_nearest_enemy_owned_type(object, "DeathBall", 500.0f))
        return 0;
    nox_bot_engine_position(object, &x, &y);
    return nox_bot_wizard_schedule(
        object, state, frame, NOX_BOT_WIZARD_SPELL_COUNTERSPELL, 0, x, y);
}

static int nox_bot_wizard_pickup_type(int object, const char *type_name, int equip_kind)
{
    int item = nox_bot_engine_find_nearest_visible_type(
        object, type_name, NOX_BOT_WIZARD_LOOT_RADIUS);

    if (!item || !nox_bot_engine_pickup_item(object, item))
        return 0;
    if (equip_kind == 1)
        nox_bot_engine_equip_weapon(object, item);
    else if (equip_kind == 2)
        nox_bot_engine_equip_armor(object, item);
    return 1;
}

static void nox_bot_wizard_loot_scan(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    static const char *const weapons[] = {
        "DeathRayWand", "FireStormWand", "LesserFireballWand", "ForceWand"
    };
    static const char *const armor[] = {
        "WizardRobe", "LeatherBoots", "MedievalCloak", "MedievalShirt", "MedievalPants"
    };
    static const char *const potions[] = {
        "RedPotion", "CurePoisonPotion", "BluePotion"
    };
    unsigned int i;

    if (state->wizard.next_loot_scan_frame &&
        !nox_bot_reaction_ready(frame, state->wizard.next_loot_scan_frame))
        return;
    state->wizard.next_loot_scan_frame = frame + NOX_BOT_WIZARD_LOOT_SCAN_FRAMES;

    for (i = 0; i < sizeof(weapons) / sizeof(weapons[0]); ++i)
        nox_bot_wizard_pickup_type(object, weapons[i], 1);
    for (i = 0; i < sizeof(armor) / sizeof(armor[0]); ++i)
        nox_bot_wizard_pickup_type(object, armor[i], 2);
    for (i = 0; i < sizeof(potions) / sizeof(potions[0]); ++i)
        nox_bot_wizard_pickup_type(object, potions[i], 0);
}

static int nox_bot_wizard_apply_weapon_preference(int object)
{
    int item = nox_bot_engine_inventory_item(object, "FireStormWand");

    if (!item)
        item = nox_bot_engine_inventory_item(object, "ForceWand");
    return item && nox_bot_engine_equip_weapon(object, item);
}

static void nox_bot_wizard_weapon_preference(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    uint32_t interval;

    if (state->wizard.next_weapon_preference_frame &&
        !nox_bot_reaction_ready(frame, state->wizard.next_weapon_preference_frame))
        return;
    interval = nox_bot_engine_fps() * NOX_BOT_WIZARD_WEAPON_PREFERENCE_SECONDS;
    if (!interval)
        interval = 1;
    state->wizard.next_weapon_preference_frame = frame + interval;
    nox_bot_wizard_apply_weapon_preference(object);
}

static void nox_bot_wizard_process_events(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    if (nox_bot_policy_event_pending(state, NOX_BOT_EVENT_ENEMY_HEARD)) {
        /* onEnemyHeard only enters when the current target cannot be seen. Its
         * FireballAtHeard helper then requires CanSee(target), so that branch
         * cannot fire as written; the subsequent Invisibility attempt is the
         * observable high-confidence response. CTF only suppresses this for
         * TeamTank, which is the native enemy-flag carrier. */
        if (state->wizard.target &&
            !nox_bot_engine_can_interact(object, state->wizard.target) &&
            !nox_bot_team_is_ctf_tank(object) &&
            !nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_INVISIBLE) &&
            !nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_ANTI_MAGIC))
            nox_bot_wizard_schedule(
                object, state, frame, NOX_BOT_WIZARD_SPELL_INVISIBILITY,
                object, 0.0f, 0.0f);
        nox_bot_policy_clear_event(state, NOX_BOT_EVENT_ENEMY_HEARD);
    }
    if (nox_bot_policy_event_pending(state, NOX_BOT_EVENT_LOST_SIGHT)) {
        if (nox_bot_engine_is_ctf())
            nox_bot_team_ctf_walk_to_own_flag(object);
        nox_bot_policy_clear_event(state, NOX_BOT_EVENT_LOST_SIGHT);
    }
    if (nox_bot_policy_event_pending(state, NOX_BOT_EVENT_END_OF_WAYPOINT)) {
        if (nox_bot_engine_is_ctf())
            nox_bot_team_ctf_attack_or_defend(object);
        nox_bot_policy_clear_event(state, NOX_BOT_EVENT_END_OF_WAYPOINT);
    }
}

static void nox_bot_wizard_regen_mana(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    uint32_t interval = nox_bot_engine_fps() * 2u;

    if (!interval)
        interval = 1;
    if (!state->wizard.next_mana_regen_frame) {
        state->wizard.next_mana_regen_frame = frame + interval;
        return;
    }
    if (!nox_bot_reaction_ready(frame, state->wizard.next_mana_regen_frame))
        return;
    state->wizard.next_mana_regen_frame = frame + interval;
    if (nox_bot_engine_mana(object) < 150)
        nox_bot_engine_mana_add(object, 1);
}

static void nox_bot_wizard_use_potions(int object, int target)
{
    if (!target || !nox_bot_engine_can_interact(object, target))
        return;
    if (nox_bot_engine_health(object) <= 25)
        nox_bot_engine_use_inventory_potion(object, "RedPotion");
    if (nox_bot_engine_mana(object) <= 100)
        nox_bot_engine_use_inventory_potion(object, "BluePotion");
}

void nox_bot_wizard_update(int object, nox_bot_policy_state *state, uint32_t frame)
{
    nox_bot_wizard_policy_state *wizard;
    int target;

    if (!object || !state || !state->active)
        return;
    wizard = &state->wizard;

    if (nox_bot_engine_health(object) <= 0 ||
        nox_bot_policy_event_pending(state, NOX_BOT_EVENT_DEATH)) {
        memset(wizard, 0, sizeof(*wizard));
        nox_bot_policy_clear_event(state, NOX_BOT_EVENT_DEATH);
        return;
    }

    nox_bot_wizard_regen_mana(object, state, frame);
    nox_bot_wizard_loot_scan(object, state, frame);
    nox_bot_wizard_weapon_preference(object, state, frame);
    nox_bot_wizard_process_events(object, state, frame);

    if (nox_bot_policy_event_pending(state, NOX_BOT_EVENT_ENEMY_SIGHTED)) {
        target = nox_bot_policy_event_object(state, NOX_BOT_EVENT_ENEMY_SIGHTED);
        wizard->target = target;
        if (!nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_INVISIBLE))
            nox_bot_wizard_try_slow(object, state, frame, target);
        nox_bot_policy_clear_event(state, NOX_BOT_EVENT_ENEMY_SIGHTED);
    }

    if (wizard->pending_spell != NOX_BOT_WIZARD_SPELL_NONE) {
        nox_bot_wizard_finish_cast(object, state, frame);
        if (wizard->pending_spell != NOX_BOT_WIZARD_SPELL_NONE ||
            !nox_bot_wizard_global_ready(wizard, frame))
            return;
    }
    if (!nox_bot_wizard_global_ready(wizard, frame))
        return;

    if (nox_bot_wizard_try_deathball_counterspell(object, state, frame))
        return;

    target = wizard->target;
    nox_bot_wizard_use_potions(object, target);

    if (target && nox_bot_engine_health(target) > 0 &&
        nox_bot_engine_can_interact(object, target)) {
        if (nox_bot_wizard_try_visible_target(object, state, frame, target))
            return;
        nox_bot_wizard_try_self_buffs(object, state, frame);
        return;
    }

    if (nox_bot_wizard_try_self_buffs(object, state, frame))
        return;
    nox_bot_wizard_try_hidden_target_buffs(object, state, frame);
}
