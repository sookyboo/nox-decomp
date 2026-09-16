#include "bot_conjurer.h"

#include "bot_engine.h"

#include <string.h>

#define NOX_BOT_ENCHANT_INVISIBLE 0
#define NOX_BOT_ENCHANT_SLOWED 4
#define NOX_BOT_ENCHANT_HELD 5
#define NOX_BOT_ENCHANT_VAMPIRISM 13
#define NOX_BOT_ENCHANT_PROTECT_FROM_FIRE 17
#define NOX_BOT_ENCHANT_PROTECT_FROM_POISON 18
#define NOX_BOT_ENCHANT_PROTECT_FROM_ELECTRICITY 20
#define NOX_BOT_ENCHANT_INFRAVISION 21
#define NOX_BOT_ENCHANT_SHOCK 22
#define NOX_BOT_ENCHANT_INVULNERABLE 23
#define NOX_BOT_ENCHANT_REFLECTIVE_SHIELD 27
#define NOX_BOT_ENCHANT_ANTI_MAGIC 29

#define NOX_BOT_CONJURER_GLOBAL_COOLDOWN_FRAMES 3u
#define NOX_BOT_CONJURER_MAX_MANA 125

typedef enum nox_bot_conjurer_spell {
    NOX_BOT_CONJURER_SPELL_NONE = 0,
    NOX_BOT_CONJURER_SPELL_FORCE_OF_NATURE,
    NOX_BOT_CONJURER_SPELL_INFRAVISION,
    NOX_BOT_CONJURER_SPELL_LESSER_HEAL,
    NOX_BOT_CONJURER_SPELL_VAMPIRISM,
    NOX_BOT_CONJURER_SPELL_PROTECT_SHOCK,
    NOX_BOT_CONJURER_SPELL_PROTECT_FIRE,
    NOX_BOT_CONJURER_SPELL_PROTECT_POISON,
    NOX_BOT_CONJURER_SPELL_METEOR,
    NOX_BOT_CONJURER_SPELL_TOXIC_CLOUD,
    NOX_BOT_CONJURER_SPELL_BURN,
    NOX_BOT_CONJURER_SPELL_COUNTERSPELL,
    NOX_BOT_CONJURER_SPELL_STUN,
    NOX_BOT_CONJURER_SPELL_SLOW,
} nox_bot_conjurer_spell;

typedef enum nox_bot_conjurer_cast_kind {
    NOX_BOT_CONJURER_CAST_SELF = 0,
    NOX_BOT_CONJURER_CAST_OBJECT,
    NOX_BOT_CONJURER_CAST_POSITION,
} nox_bot_conjurer_cast_kind;

typedef struct nox_bot_conjurer_spell_def {
    const char *name;
    unsigned short mana;
    unsigned short cooldown_seconds;
    unsigned short cooldown_frames;
    unsigned char cast_kind;
} nox_bot_conjurer_spell_def;

static const nox_bot_conjurer_spell_def nox_bot_conjurer_spells[] = {
    { 0, 0, 0, 0, 0 },
    { "FORCE_OF_NATURE", 60, 5, 0, NOX_BOT_CONJURER_CAST_POSITION },
    { "INFRAVISION", 30, 0, 0, NOX_BOT_CONJURER_CAST_SELF },
    { "LESSER_HEAL", 30, 0, 0, NOX_BOT_CONJURER_CAST_SELF },
    { "VAMPIRISM", 20, 0, 0, NOX_BOT_CONJURER_CAST_SELF },
    { "PROTECTION_FROM_ELECTRICITY", 30, 0, 0, NOX_BOT_CONJURER_CAST_SELF },
    { "PROTECTION_FROM_FIRE", 30, 0, 0, NOX_BOT_CONJURER_CAST_SELF },
    { "PROTECTION_FROM_POISON", 30, 0, 0, NOX_BOT_CONJURER_CAST_SELF },
    { "METEOR", 30, 5, 0, NOX_BOT_CONJURER_CAST_POSITION },
    { "TOXIC_CLOUD", 60, 5, 0, NOX_BOT_CONJURER_CAST_POSITION },
    { "BURN", 10, 0, 1, NOX_BOT_CONJURER_CAST_POSITION },
    { "COUNTERSPELL", 20, 5, 0, NOX_BOT_CONJURER_CAST_POSITION },
    { "STUN", 10, 5, 0, NOX_BOT_CONJURER_CAST_OBJECT },
    { "SLOW", 10, 3, 0, NOX_BOT_CONJURER_CAST_OBJECT },
};

static uint32_t *nox_bot_conjurer_ready_frame(
    nox_bot_conjurer_policy_state *conjurer, nox_bot_conjurer_spell spell)
{
    switch (spell) {
    case NOX_BOT_CONJURER_SPELL_FORCE_OF_NATURE:
        return &conjurer->force_of_nature_ready_frame;
    case NOX_BOT_CONJURER_SPELL_METEOR:
        return &conjurer->meteor_ready_frame;
    case NOX_BOT_CONJURER_SPELL_TOXIC_CLOUD:
        return &conjurer->toxic_cloud_ready_frame;
    case NOX_BOT_CONJURER_SPELL_BURN:
        return &conjurer->burn_ready_frame;
    case NOX_BOT_CONJURER_SPELL_COUNTERSPELL:
        return &conjurer->counterspell_ready_frame;
    case NOX_BOT_CONJURER_SPELL_STUN:
        return &conjurer->stun_ready_frame;
    case NOX_BOT_CONJURER_SPELL_SLOW:
        return &conjurer->slow_ready_frame;
    default:
        return 0;
    }
}

static int nox_bot_conjurer_deadline_ready(uint32_t frame, uint32_t deadline)
{
    return !deadline || nox_bot_reaction_ready(frame, deadline);
}

static int nox_bot_conjurer_spell_ready(
    const nox_bot_conjurer_policy_state *conjurer,
    nox_bot_conjurer_spell spell,
    uint32_t frame)
{
    const uint32_t *ready = 0;

    switch (spell) {
    case NOX_BOT_CONJURER_SPELL_FORCE_OF_NATURE:
        ready = &conjurer->force_of_nature_ready_frame;
        break;
    case NOX_BOT_CONJURER_SPELL_METEOR:
        ready = &conjurer->meteor_ready_frame;
        break;
    case NOX_BOT_CONJURER_SPELL_TOXIC_CLOUD:
        ready = &conjurer->toxic_cloud_ready_frame;
        break;
    case NOX_BOT_CONJURER_SPELL_BURN:
        ready = &conjurer->burn_ready_frame;
        break;
    case NOX_BOT_CONJURER_SPELL_COUNTERSPELL:
        ready = &conjurer->counterspell_ready_frame;
        break;
    case NOX_BOT_CONJURER_SPELL_STUN:
        ready = &conjurer->stun_ready_frame;
        break;
    case NOX_BOT_CONJURER_SPELL_SLOW:
        ready = &conjurer->slow_ready_frame;
        break;
    default:
        return 1;
    }
    return nox_bot_conjurer_deadline_ready(frame, *ready);
}

static int nox_bot_conjurer_global_ready(
    const nox_bot_conjurer_policy_state *conjurer, uint32_t frame)
{
    return conjurer->pending_spell == NOX_BOT_CONJURER_SPELL_NONE &&
        nox_bot_conjurer_deadline_ready(frame, conjurer->global_ready_frame);
}

static int nox_bot_conjurer_schedule(
    int object,
    nox_bot_policy_state *state,
    uint32_t frame,
    nox_bot_conjurer_spell spell,
    int target,
    float x,
    float y)
{
    nox_bot_conjurer_policy_state *conjurer = &state->conjurer;
    const nox_bot_conjurer_spell_def *def;

    if (spell <= NOX_BOT_CONJURER_SPELL_NONE ||
        spell >= (int)(sizeof(nox_bot_conjurer_spells) / sizeof(nox_bot_conjurer_spells[0])))
        return 0;
    def = &nox_bot_conjurer_spells[spell];
    if (!nox_bot_conjurer_global_ready(conjurer, frame) ||
        !nox_bot_conjurer_spell_ready(conjurer, spell, frame) ||
        nox_bot_engine_mana(object) < def->mana)
        return 0;

    conjurer->pending_spell = (unsigned char)spell;
    conjurer->pending_target = target;
    conjurer->pending_x = x;
    conjurer->pending_y = y;
    conjurer->pending_cast_frame = nox_bot_reaction_deadline(
        frame, (nox_bot_difficulty)state->difficulty);
    return 1;
}

static int nox_bot_conjurer_pending_target_valid(
    int object,
    const nox_bot_conjurer_policy_state *conjurer,
    nox_bot_conjurer_spell spell)
{
    if (!conjurer->pending_target || nox_bot_engine_health(conjurer->pending_target) <= 0)
        return 0;
    if (spell == NOX_BOT_CONJURER_SPELL_BURN ||
        spell == NOX_BOT_CONJURER_SPELL_COUNTERSPELL)
        return nox_bot_engine_can_interact(object, conjurer->pending_target);
    return 1;
}

static void nox_bot_conjurer_cancel_pending(
    nox_bot_policy_state *state, uint32_t frame)
{
    state->conjurer.pending_spell = NOX_BOT_CONJURER_SPELL_NONE;
    state->conjurer.pending_target = 0;
    state->conjurer.global_ready_frame = nox_bot_reaction_deadline(
        frame, (nox_bot_difficulty)state->difficulty);
}

static void nox_bot_conjurer_finish_cast(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    nox_bot_conjurer_policy_state *conjurer = &state->conjurer;
    nox_bot_conjurer_spell spell = (nox_bot_conjurer_spell)conjurer->pending_spell;
    const nox_bot_conjurer_spell_def *def;
    uint32_t *ready;
    uint32_t cooldown;

    if (spell == NOX_BOT_CONJURER_SPELL_NONE ||
        !nox_bot_reaction_ready(frame, conjurer->pending_cast_frame))
        return;

    def = &nox_bot_conjurer_spells[spell];
    if (nox_bot_engine_health(object) <= 0 ||
        nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_ANTI_MAGIC) ||
        nox_bot_engine_mana(object) < def->mana) {
        nox_bot_conjurer_cancel_pending(state, frame);
        return;
    }
    if ((def->cast_kind == NOX_BOT_CONJURER_CAST_OBJECT || conjurer->pending_target) &&
        !nox_bot_conjurer_pending_target_valid(object, conjurer, spell)) {
        nox_bot_conjurer_cancel_pending(state, frame);
        return;
    }

    nox_bot_engine_mana_sub(object, def->mana);
    if (def->cast_kind == NOX_BOT_CONJURER_CAST_SELF)
        nox_bot_engine_cast_script_self(object, def->name);
    else if (def->cast_kind == NOX_BOT_CONJURER_CAST_OBJECT)
        nox_bot_engine_cast_script_object(object, def->name, conjurer->pending_target);
    else
        nox_bot_engine_cast_script_position(object, def->name, conjurer->pending_x, conjurer->pending_y);

    conjurer->pending_spell = NOX_BOT_CONJURER_SPELL_NONE;
    conjurer->pending_target = 0;
    conjurer->global_ready_frame = frame + NOX_BOT_CONJURER_GLOBAL_COOLDOWN_FRAMES;

    ready = nox_bot_conjurer_ready_frame(conjurer, spell);
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

static int nox_bot_conjurer_try_force_of_nature(
    int object, nox_bot_policy_state *state, uint32_t frame, int target)
{
    float x;
    float y;

    if (!target || nox_bot_engine_health(target) <= 0 ||
        nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_ANTI_MAGIC))
        return 0;
    nox_bot_engine_position(target, &x, &y);
    return nox_bot_conjurer_schedule(
        object, state, frame, NOX_BOT_CONJURER_SPELL_FORCE_OF_NATURE, target, x, y);
}

static int nox_bot_conjurer_try_infravision(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    if (nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_ANTI_MAGIC) ||
        nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_INFRAVISION))
        return 0;
    return nox_bot_conjurer_schedule(
        object, state, frame, NOX_BOT_CONJURER_SPELL_INFRAVISION, object, 0.0f, 0.0f);
}

static int nox_bot_conjurer_try_lesser_heal(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    if (nox_bot_engine_mana(object) < 100 || nox_bot_engine_health(object) > 60 ||
        nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_ANTI_MAGIC))
        return 0;
    return nox_bot_conjurer_schedule(
        object, state, frame, NOX_BOT_CONJURER_SPELL_LESSER_HEAL, object, 0.0f, 0.0f);
}

static int nox_bot_conjurer_try_held_target(
    int object, nox_bot_policy_state *state, uint32_t frame, int target)
{
    float x;
    float y;

    if (!target || !nox_bot_engine_can_interact(object, target) ||
        !(nox_bot_engine_has_buff(target, NOX_BOT_ENCHANT_HELD) ||
          nox_bot_engine_has_buff(target, NOX_BOT_ENCHANT_SLOWED)) ||
        nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_ANTI_MAGIC))
        return 0;

    nox_bot_engine_position(target, &x, &y);
    if (nox_bot_conjurer_schedule(
            object, state, frame, NOX_BOT_CONJURER_SPELL_METEOR, target, x, y))
        return 1;
    if (nox_bot_conjurer_schedule(
            object, state, frame, NOX_BOT_CONJURER_SPELL_TOXIC_CLOUD, target, x, y))
        return 1;
    if (nox_bot_engine_has_buff(target, NOX_BOT_ENCHANT_REFLECTIVE_SHIELD) &&
        !nox_bot_engine_has_buff(target, NOX_BOT_ENCHANT_INVULNERABLE) &&
        nox_bot_conjurer_schedule(
            object, state, frame, NOX_BOT_CONJURER_SPELL_BURN, target, x, y))
        return 1;
    if (!nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_INVISIBLE) &&
        nox_bot_engine_has_buff(target, NOX_BOT_ENCHANT_SHOCK)) {
        nox_bot_engine_position(object, &x, &y);
        if (nox_bot_conjurer_schedule(
                object, state, frame, NOX_BOT_CONJURER_SPELL_COUNTERSPELL, target, x, y))
            return 1;
    }
    return 0;
}

static int nox_bot_conjurer_try_debuff(
    int object, nox_bot_policy_state *state, uint32_t frame, int target)
{
    if (!target || nox_bot_engine_health(target) <= 0 ||
        !nox_bot_engine_can_interact(object, target) ||
        nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_ANTI_MAGIC))
        return 0;

    if (!nox_bot_engine_is_ctf()) {
        if (!nox_bot_engine_has_buff(target, NOX_BOT_ENCHANT_HELD) &&
            !nox_bot_engine_has_buff(target, NOX_BOT_ENCHANT_SLOWED) &&
            nox_bot_engine_max_health(target) != 150 &&
            nox_bot_conjurer_schedule(
                object, state, frame, NOX_BOT_CONJURER_SPELL_STUN, target, 0.0f, 0.0f))
            return 1;
        return 0;
    }

    if (!nox_bot_engine_has_buff(target, NOX_BOT_ENCHANT_SLOWED) &&
        !nox_bot_engine_has_buff(target, NOX_BOT_ENCHANT_REFLECTIVE_SHIELD) &&
        !nox_bot_engine_has_buff(target, NOX_BOT_ENCHANT_HELD) &&
        nox_bot_conjurer_schedule(
            object, state, frame, NOX_BOT_CONJURER_SPELL_SLOW, target, 0.0f, 0.0f))
        return 1;
    return 0;
}

static int nox_bot_conjurer_try_hidden_buffs(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    if (nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_ANTI_MAGIC))
        return 0;

    if (!nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_VAMPIRISM) &&
        nox_bot_conjurer_schedule(
            object, state, frame, NOX_BOT_CONJURER_SPELL_VAMPIRISM, object, 0.0f, 0.0f))
        return 1;
    if (nox_bot_engine_mana(object) < 85)
        return 0;
    if (!nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_PROTECT_FROM_ELECTRICITY) &&
        nox_bot_conjurer_schedule(
            object, state, frame, NOX_BOT_CONJURER_SPELL_PROTECT_SHOCK, object, 0.0f, 0.0f))
        return 1;
    if (!nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_PROTECT_FROM_FIRE) &&
        nox_bot_conjurer_schedule(
            object, state, frame, NOX_BOT_CONJURER_SPELL_PROTECT_FIRE, object, 0.0f, 0.0f))
        return 1;
    if (!nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_PROTECT_FROM_POISON) &&
        nox_bot_conjurer_schedule(
            object, state, frame, NOX_BOT_CONJURER_SPELL_PROTECT_POISON, object, 0.0f, 0.0f))
        return 1;
    return 0;
}

static void nox_bot_conjurer_regen_mana(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    uint32_t interval = nox_bot_engine_fps() * 2u;
    int mana;

    if (!interval)
        interval = 1;
    if (!state->conjurer.next_mana_regen_frame) {
        state->conjurer.next_mana_regen_frame = frame + interval;
        return;
    }
    if (!nox_bot_reaction_ready(frame, state->conjurer.next_mana_regen_frame))
        return;
    state->conjurer.next_mana_regen_frame = frame + interval;
    mana = nox_bot_engine_mana(object);
    if (mana < NOX_BOT_CONJURER_MAX_MANA)
        nox_bot_engine_mana_add(object, 1);
}

static void nox_bot_conjurer_cap_mana(int object)
{
    int mana = nox_bot_engine_mana(object);

    if (mana > NOX_BOT_CONJURER_MAX_MANA)
        nox_bot_engine_mana_sub(object, mana - NOX_BOT_CONJURER_MAX_MANA);
}

static void nox_bot_conjurer_use_potions(int object, int target)
{
    if (!target || !nox_bot_engine_can_interact(object, target))
        return;
    if (nox_bot_engine_health(object) <= 25)
        nox_bot_engine_use_inventory_potion(object, "RedPotion");
    if (nox_bot_engine_mana(object) <= 100)
        nox_bot_engine_use_inventory_potion(object, "BluePotion");
}

static void nox_bot_conjurer_process_events(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    nox_bot_conjurer_policy_state *conjurer = &state->conjurer;
    int target;

    if (nox_bot_policy_event_pending(state, NOX_BOT_EVENT_ENEMY_SIGHTED)) {
        target = nox_bot_policy_event_object(state, NOX_BOT_EVENT_ENEMY_SIGHTED);
        conjurer->target = target;
        nox_bot_conjurer_try_force_of_nature(object, state, frame, target);
        nox_bot_policy_clear_event(state, NOX_BOT_EVENT_ENEMY_SIGHTED);
    }

    if (nox_bot_policy_event_pending(state, NOX_BOT_EVENT_ENEMY_HEARD)) {
        target = conjurer->target;
        if (target && !nox_bot_engine_can_interact(object, target)) {
            if (!nox_bot_conjurer_try_force_of_nature(object, state, frame, target))
                nox_bot_conjurer_try_infravision(object, state, frame);
        }
        nox_bot_policy_clear_event(state, NOX_BOT_EVENT_ENEMY_HEARD);
    }

    if (nox_bot_policy_event_pending(state, NOX_BOT_EVENT_LOOKING_FOR_ENEMY)) {
        nox_bot_conjurer_try_infravision(object, state, frame);
        nox_bot_policy_clear_event(state, NOX_BOT_EVENT_LOOKING_FOR_ENEMY);
    }

    if (nox_bot_policy_event_pending(state, NOX_BOT_EVENT_LOST_SIGHT)) {
        nox_bot_conjurer_try_infravision(object, state, frame);
        nox_bot_policy_clear_event(state, NOX_BOT_EVENT_LOST_SIGHT);
    }
}

void nox_bot_conjurer_update(int object, nox_bot_policy_state *state, uint32_t frame)
{
    nox_bot_conjurer_policy_state *conjurer;
    int target;

    if (!object || !state || !state->active)
        return;
    conjurer = &state->conjurer;

    if (nox_bot_engine_health(object) <= 0 ||
        nox_bot_policy_event_pending(state, NOX_BOT_EVENT_DEATH)) {
        memset(conjurer, 0, sizeof(*conjurer));
        nox_bot_policy_clear_event(state, NOX_BOT_EVENT_DEATH);
        return;
    }

    nox_bot_conjurer_regen_mana(object, state, frame);
    nox_bot_conjurer_process_events(object, state, frame);

    if (conjurer->pending_spell != NOX_BOT_CONJURER_SPELL_NONE) {
        nox_bot_conjurer_finish_cast(object, state, frame);
        if (conjurer->pending_spell != NOX_BOT_CONJURER_SPELL_NONE ||
            !nox_bot_conjurer_global_ready(conjurer, frame)) {
            nox_bot_conjurer_cap_mana(object);
            return;
        }
    }
    if (!nox_bot_conjurer_global_ready(conjurer, frame)) {
        nox_bot_conjurer_cap_mana(object);
        return;
    }

    target = conjurer->target;
    nox_bot_conjurer_use_potions(object, target);
    nox_bot_conjurer_cap_mana(object);

    if (target && nox_bot_engine_health(target) > 0 &&
        nox_bot_engine_can_interact(object, target)) {
        if (nox_bot_conjurer_try_lesser_heal(object, state, frame))
            return;
        if (nox_bot_conjurer_try_held_target(object, state, frame, target))
            return;
        nox_bot_conjurer_try_debuff(object, state, frame, target);
        return;
    }

    nox_bot_conjurer_try_hidden_buffs(object, state, frame);
}
