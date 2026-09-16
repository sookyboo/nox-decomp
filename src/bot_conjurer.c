#include "bot_conjurer.h"

#include "bot_engine.h"
#include "bot_team.h"

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
#define NOX_BOT_CONJURER_LOOT_RADIUS 75.0f
#define NOX_BOT_CONJURER_LOOT_SCAN_FRAMES 15u
#define NOX_BOT_CONJURER_WEAPON_PREFERENCE_SECONDS 10u

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
    NOX_BOT_CONJURER_SPELL_COUNTERSPELL_DEATHBALL,
    NOX_BOT_CONJURER_SPELL_INVERSION,
    NOX_BOT_CONJURER_SPELL_BLINK,
    NOX_BOT_CONJURER_SPELL_SUMMON_CREATURE,
    NOX_BOT_CONJURER_SPELL_STUN,
    NOX_BOT_CONJURER_SPELL_SLOW,
    NOX_BOT_CONJURER_SPELL_PIXIE_SWARM,
} nox_bot_conjurer_spell;

typedef enum nox_bot_conjurer_cast_kind {
    NOX_BOT_CONJURER_CAST_SELF = 0,
    NOX_BOT_CONJURER_CAST_OBJECT,
    NOX_BOT_CONJURER_CAST_POSITION,
    NOX_BOT_CONJURER_CAST_TRAP,
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
    { "COUNTERSPELL", 20, 20, 0, NOX_BOT_CONJURER_CAST_POSITION },
    { "INVERSION", 10, 1, 0, NOX_BOT_CONJURER_CAST_SELF },
    { "BLINK", 10, 1, 0, NOX_BOT_CONJURER_CAST_TRAP },
    { 0, 0, 0, 0, NOX_BOT_CONJURER_CAST_SELF },
    { "STUN", 10, 5, 0, NOX_BOT_CONJURER_CAST_OBJECT },
    { "SLOW", 10, 3, 0, NOX_BOT_CONJURER_CAST_OBJECT },
    { "PIXIE_SWARM", 30, 0, 0, NOX_BOT_CONJURER_CAST_SELF },
};

typedef enum nox_bot_conjurer_summon {
    NOX_BOT_CONJURER_SUMMON_NONE = 0,
    NOX_BOT_CONJURER_SUMMON_BOMBER,
    NOX_BOT_CONJURER_SUMMON_WASP,
    NOX_BOT_CONJURER_SUMMON_URCHIN,
    NOX_BOT_CONJURER_SUMMON_SMALL_SPIDER,
    NOX_BOT_CONJURER_SUMMON_SMALL_ALBINO_SPIDER,
    NOX_BOT_CONJURER_SUMMON_MECHANICAL_FLYER,
    NOX_BOT_CONJURER_SUMMON_IMP,
    NOX_BOT_CONJURER_SUMMON_GIANT_LEECH,
    NOX_BOT_CONJURER_SUMMON_BAT,
    NOX_BOT_CONJURER_SUMMON_GHOST,
    NOX_BOT_CONJURER_SUMMON_BLACK_BEAR,
    NOX_BOT_CONJURER_SUMMON_OGRE_BRUTE,
    NOX_BOT_CONJURER_SUMMON_BLACK_WOLF,
    NOX_BOT_CONJURER_SUMMON_WHITE_WOLF,
    NOX_BOT_CONJURER_SUMMON_WOLF,
    NOX_BOT_CONJURER_SUMMON_EVIL_CHERUB,
    NOX_BOT_CONJURER_SUMMON_OGRE,
    NOX_BOT_CONJURER_SUMMON_OGRE_WARLORD,
    NOX_BOT_CONJURER_SUMMON_ZOMBIE,
    NOX_BOT_CONJURER_SUMMON_VILE_ZOMBIE,
    NOX_BOT_CONJURER_SUMMON_EMBER_DEMON,
    NOX_BOT_CONJURER_SUMMON_SHADE,
    NOX_BOT_CONJURER_SUMMON_ALBINO_SPIDER,
    NOX_BOT_CONJURER_SUMMON_BEAR,
    NOX_BOT_CONJURER_SUMMON_SKELETON,
    NOX_BOT_CONJURER_SUMMON_SKELETON_LORD,
    NOX_BOT_CONJURER_SUMMON_SCORPION,
    NOX_BOT_CONJURER_SUMMON_SPIDER,
    NOX_BOT_CONJURER_SUMMON_SPITTING_SPIDER,
    NOX_BOT_CONJURER_SUMMON_TROLL,
    NOX_BOT_CONJURER_SUMMON_MECHANICAL_GOLEM,
    NOX_BOT_CONJURER_SUMMON_STONE_GOLEM,
    NOX_BOT_CONJURER_SUMMON_CARNIVOROUS_PLANT,
    NOX_BOT_CONJURER_SUMMON_WILLOWISP,
    NOX_BOT_CONJURER_SUMMON_MIMIC,
    NOX_BOT_CONJURER_SUMMON_BEHOLDER,
} nox_bot_conjurer_summon;

typedef struct nox_bot_conjurer_summon_def {
    const char *name;
    unsigned char mana;
    unsigned char cooldown_seconds;
} nox_bot_conjurer_summon_def;

static const nox_bot_conjurer_summon_def nox_bot_conjurer_summons[] = {
    { 0, 0, 0 },
    { 0, 80, 0 }, /* Bot-Script custom Bomber; it checks mana but does not spend it. */
    { "SUMMON_WASP", 15, 2 },
    { "SUMMON_URCHIN", 30, 2 },
    { "SUMMON_SMALL_SPIDER", 15, 2 },
    { "SUMMON_SMALL_ALBINO_SPIDER", 15, 2 },
    { "SUMMON_MECHANICAL_FLYER", 30, 2 },
    { "SUMMON_IMP", 30, 2 },
    { "SUMMON_GIANT_LEECH", 30, 2 },
    { "SUMMON_BAT", 15, 2 },
    { "SUMMON_GHOST", 15, 2 },
    { "SUMMON_BLACK_BEAR", 60, 7 },
    { "SUMMON_OGRE_BRUTE", 60, 7 },
    { "SUMMON_BLACK_WOLF", 60, 7 },
    { "SUMMON_WHITE_WOLF", 30, 7 },
    { "SUMMON_WOLF", 30, 7 },
    { "SUMMON_EVIL_CHERUB", 60, 7 },
    { "SUMMON_OGRE", 30, 7 },
    { "SUMMON_OGRE_WARLORD", 85, 7 },
    { "SUMMON_ZOMBIE", 30, 7 },
    { "SUMMON_VILE_ZOMBIE", 60, 7 },
    { "SUMMON_EMBER_DEMON", 60, 7 },
    { "SUMMON_SHADE", 30, 7 },
    { "SUMMON_ALBINO_SPIDER", 30, 7 },
    { "SUMMON_BEAR", 60, 7 },
    { "SUMMON_SKELETON", 30, 7 },
    { "SUMMON_SKELETON_LORD", 60, 7 },
    { "SUMMON_SCORPION", 60, 7 },
    { "SUMMON_SPIDER", 30, 7 },
    { "SUMMON_SPITTING_SPIDER", 30, 7 },
    { "SUMMON_TROLL", 30, 7 },
    { "SUMMON_MECHANICAL_GOLEM", 85, 13 },
    { "SUMMON_STONE_GOLEM", 85, 13 },
    { "SUMMON_CARNIVOROUS_PLANT", 30, 13 },
    { "SUMMON_WILLOWISP", 60, 13 },
    { "SUMMON_MIMIC", 85, 13 },
    { "SUMMON_BEHOLDER", 60, 13 },
};

static const unsigned char nox_bot_conjurer_small_summons[] = {
    NOX_BOT_CONJURER_SUMMON_WASP,
    NOX_BOT_CONJURER_SUMMON_URCHIN,
    NOX_BOT_CONJURER_SUMMON_SMALL_SPIDER,
    NOX_BOT_CONJURER_SUMMON_SMALL_ALBINO_SPIDER,
    NOX_BOT_CONJURER_SUMMON_MECHANICAL_FLYER,
    NOX_BOT_CONJURER_SUMMON_IMP,
    NOX_BOT_CONJURER_SUMMON_GIANT_LEECH,
    NOX_BOT_CONJURER_SUMMON_BAT,
    NOX_BOT_CONJURER_SUMMON_GHOST,
    NOX_BOT_CONJURER_SUMMON_BOMBER,
};

static const unsigned char nox_bot_conjurer_medium_summons[] = {
    NOX_BOT_CONJURER_SUMMON_BLACK_BEAR,
    NOX_BOT_CONJURER_SUMMON_OGRE_BRUTE,
    NOX_BOT_CONJURER_SUMMON_BLACK_WOLF,
    NOX_BOT_CONJURER_SUMMON_WHITE_WOLF,
    NOX_BOT_CONJURER_SUMMON_WOLF,
    NOX_BOT_CONJURER_SUMMON_EVIL_CHERUB,
    NOX_BOT_CONJURER_SUMMON_OGRE,
    NOX_BOT_CONJURER_SUMMON_OGRE_WARLORD,
    NOX_BOT_CONJURER_SUMMON_ZOMBIE,
    NOX_BOT_CONJURER_SUMMON_VILE_ZOMBIE,
    NOX_BOT_CONJURER_SUMMON_EMBER_DEMON,
    NOX_BOT_CONJURER_SUMMON_SHADE,
    NOX_BOT_CONJURER_SUMMON_ALBINO_SPIDER,
    NOX_BOT_CONJURER_SUMMON_BEAR,
    NOX_BOT_CONJURER_SUMMON_SKELETON,
    NOX_BOT_CONJURER_SUMMON_SKELETON_LORD,
    NOX_BOT_CONJURER_SUMMON_SCORPION,
    NOX_BOT_CONJURER_SUMMON_SPIDER,
    NOX_BOT_CONJURER_SUMMON_SPITTING_SPIDER,
    NOX_BOT_CONJURER_SUMMON_TROLL,
};

static const unsigned char nox_bot_conjurer_large_summons[] = {
    NOX_BOT_CONJURER_SUMMON_MECHANICAL_GOLEM,
    NOX_BOT_CONJURER_SUMMON_STONE_GOLEM,
    NOX_BOT_CONJURER_SUMMON_CARNIVOROUS_PLANT,
    NOX_BOT_CONJURER_SUMMON_WILLOWISP,
    NOX_BOT_CONJURER_SUMMON_MIMIC,
    NOX_BOT_CONJURER_SUMMON_BEHOLDER,
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
    case NOX_BOT_CONJURER_SPELL_COUNTERSPELL_DEATHBALL:
        return &conjurer->counterspell_ready_frame;
    case NOX_BOT_CONJURER_SPELL_INVERSION:
        return &conjurer->inversion_ready_frame;
    case NOX_BOT_CONJURER_SPELL_BLINK:
        return &conjurer->blink_ready_frame;
    case NOX_BOT_CONJURER_SPELL_SUMMON_CREATURE:
        return &conjurer->summon_ready_frame;
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
    case NOX_BOT_CONJURER_SPELL_COUNTERSPELL_DEATHBALL:
        ready = &conjurer->counterspell_ready_frame;
        break;
    case NOX_BOT_CONJURER_SPELL_INVERSION:
        ready = &conjurer->inversion_ready_frame;
        break;
    case NOX_BOT_CONJURER_SPELL_BLINK:
        ready = &conjurer->blink_ready_frame;
        break;
    case NOX_BOT_CONJURER_SPELL_SUMMON_CREATURE:
        ready = &conjurer->summon_ready_frame;
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

static int nox_bot_conjurer_schedule_summon(
    int object,
    nox_bot_policy_state *state,
    uint32_t frame,
    nox_bot_conjurer_summon summon)
{
    nox_bot_conjurer_policy_state *conjurer = &state->conjurer;
    const nox_bot_conjurer_summon_def *def;

    if (summon <= NOX_BOT_CONJURER_SUMMON_NONE ||
        summon >= (int)(sizeof(nox_bot_conjurer_summons) / sizeof(nox_bot_conjurer_summons[0])))
        return 0;
    def = &nox_bot_conjurer_summons[summon];
    if (!nox_bot_conjurer_global_ready(conjurer, frame) ||
        !nox_bot_conjurer_deadline_ready(frame, conjurer->summon_ready_frame) ||
        nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_ANTI_MAGIC) ||
        nox_bot_engine_mana(object) < def->mana)
        return 0;
    if (summon == NOX_BOT_CONJURER_SUMMON_BOMBER) {
        if (nox_bot_engine_owned_type_count(object, "Bomber") > 1 ||
            !nox_bot_engine_bomber_fits(object))
            return 0;
    } else if (!nox_bot_engine_summon_spell_fits(object, def->name)) {
        return 0;
    }

    conjurer->pending_spell = NOX_BOT_CONJURER_SPELL_SUMMON_CREATURE;
    conjurer->pending_summon = (unsigned char)summon;
    conjurer->pending_target = 0;
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
    state->conjurer.pending_summon = NOX_BOT_CONJURER_SUMMON_NONE;
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

    if (spell == NOX_BOT_CONJURER_SPELL_SUMMON_CREATURE) {
        nox_bot_conjurer_summon summon = (nox_bot_conjurer_summon)conjurer->pending_summon;
        const nox_bot_conjurer_summon_def *summon_def;

        if (summon <= NOX_BOT_CONJURER_SUMMON_NONE ||
            summon >= (int)(sizeof(nox_bot_conjurer_summons) / sizeof(nox_bot_conjurer_summons[0]))) {
            nox_bot_conjurer_cancel_pending(state, frame);
            return;
        }
        summon_def = &nox_bot_conjurer_summons[summon];
        if (nox_bot_engine_health(object) <= 0 ||
            nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_ANTI_MAGIC) ||
            nox_bot_engine_mana(object) < summon_def->mana) {
            nox_bot_conjurer_cancel_pending(state, frame);
            return;
        }
        if (summon == NOX_BOT_CONJURER_SUMMON_BOMBER) {
            if (nox_bot_engine_owned_type_count(object, "Bomber") > 1 ||
                !nox_bot_engine_bomber_fits(object) ||
                !nox_bot_engine_create_bomber(object)) {
                nox_bot_conjurer_cancel_pending(state, frame);
                return;
            }
            /* Preserve Bot-Script's observable quirk: castBomber requires 80
             * mana but never subtracts it. Its summon gate reopens after three
             * frames rather than using a seconds-based summon cooldown. */
            cooldown = NOX_BOT_CONJURER_GLOBAL_COOLDOWN_FRAMES;
        } else {
            if (!nox_bot_engine_summon_spell_fits(object, summon_def->name)) {
                nox_bot_conjurer_cancel_pending(state, frame);
                return;
            }
            nox_bot_engine_mana_sub(object, summon_def->mana);
            nox_bot_engine_cast_script_self(object, summon_def->name);
            cooldown = nox_bot_engine_fps() * summon_def->cooldown_seconds;
            if (!cooldown)
                cooldown = 1;
        }
        conjurer->pending_spell = NOX_BOT_CONJURER_SPELL_NONE;
        conjurer->pending_summon = NOX_BOT_CONJURER_SUMMON_NONE;
        conjurer->pending_target = 0;
        conjurer->global_ready_frame = frame + NOX_BOT_CONJURER_GLOBAL_COOLDOWN_FRAMES;
        conjurer->summon_ready_frame = frame + cooldown;
        return;
    }

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
    else if (def->cast_kind == NOX_BOT_CONJURER_CAST_TRAP)
        nox_bot_engine_create_spell_trap(object, def->name);
    else
        nox_bot_engine_cast_script_position(object, def->name, conjurer->pending_x, conjurer->pending_y);

    conjurer->pending_spell = NOX_BOT_CONJURER_SPELL_NONE;
    conjurer->pending_summon = NOX_BOT_CONJURER_SUMMON_NONE;
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

static int nox_bot_conjurer_try_pixie_swarm(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    if (nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_ANTI_MAGIC) ||
        nox_bot_engine_owned_type_count(object, "Pixie") != 0)
        return 0;
    return nox_bot_conjurer_schedule(
        object, state, frame, NOX_BOT_CONJURER_SPELL_PIXIE_SWARM,
        object, 0.0f, 0.0f);
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

static int nox_bot_conjurer_try_blink(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    if (nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_ANTI_MAGIC) ||
        nox_bot_team_is_ctf_tank(object))
        return 0;
    return nox_bot_conjurer_schedule(
        object, state, frame, NOX_BOT_CONJURER_SPELL_BLINK, 0, 0.0f, 0.0f);
}

static int nox_bot_conjurer_go_to_mana_source(
    int object, nox_bot_policy_state *state)
{
    nox_bot_conjurer_policy_state *conjurer = &state->conjurer;
    float x;
    float y;
    int source;

    if (conjurer->mana_route_active)
        return conjurer->mana_source != 0;
    conjurer->mana_route_active = 1;
    nox_bot_engine_set_aggression(object, 0.16f);
    source = nox_bot_engine_find_nearest_mana_source(
        object, 10, nox_bot_team_is_ctf_tank(object));
    conjurer->mana_source = source;
    if (!source)
        return 0;
    nox_bot_engine_position(source, &x, &y);
    nox_bot_engine_walk_to(object, x, y);
    return 1;
}

static void nox_bot_conjurer_maybe_seek_mana(
    int object, nox_bot_policy_state *state)
{
    if (!nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_VAMPIRISM) ||
        !nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_PROTECT_FROM_POISON) ||
        !nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_PROTECT_FROM_ELECTRICITY) ||
        !nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_PROTECT_FROM_FIRE) ||
        nox_bot_engine_summon_cage_used(object) <= 3)
        nox_bot_conjurer_go_to_mana_source(object, state);
}

static int nox_bot_conjurer_try_random_summon(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    int cage;
    int category;
    int choice;
    nox_bot_conjurer_summon summon = NOX_BOT_CONJURER_SUMMON_NONE;

    if (!nox_bot_conjurer_deadline_ready(frame, state->conjurer.summon_ready_frame) ||
        nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_ANTI_MAGIC) ||
        nox_bot_engine_mana(object) < 85)
        return 0;

    cage = nox_bot_engine_summon_cage_used(object);
    category = nox_bot_engine_random_int(1, 3);
    if (category == 1) {
        /* The reference gives Bomber priority when the cage is exactly 3. If
         * that attempt cannot start, it still falls through to the ordinary
         * small-creature roll. */
        if (cage == 3 && nox_bot_conjurer_schedule_summon(
                object, state, frame, NOX_BOT_CONJURER_SUMMON_BOMBER))
            return 1;
        choice = nox_bot_engine_random_int(1, 10);
        summon = (nox_bot_conjurer_summon)nox_bot_conjurer_small_summons[choice - 1];
    } else if (category == 2) {
        choice = nox_bot_engine_random_int(1, 20);
        summon = (nox_bot_conjurer_summon)nox_bot_conjurer_medium_summons[choice - 1];
    } else if (category == 3) {
        choice = nox_bot_engine_random_int(1, 6);
        summon = (nox_bot_conjurer_summon)nox_bot_conjurer_large_summons[choice - 1];
    }
    if (summon == NOX_BOT_CONJURER_SUMMON_NONE)
        return 0;
    return nox_bot_conjurer_schedule_summon(object, state, frame, summon);
}

static int nox_bot_conjurer_try_missile_reaction(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    float x;
    float y;
    int target = state->conjurer.target;

    if (nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_ANTI_MAGIC))
        return 0;

    /* Preserve Bot-Script's DeathBall-first branch. Any nearby DeathBall
     * prevents the generic missile/Inversion branch for this update. */
    if (nox_bot_engine_find_nearest_world_type(object, "DeathBall", 500.0f)) {
        if (!nox_bot_engine_find_nearest_enemy_owned_type(object, "DeathBall", 500.0f))
            return 0;
        nox_bot_engine_position(object, &x, &y);
        return nox_bot_conjurer_schedule(
            object, state, frame, NOX_BOT_CONJURER_SPELL_COUNTERSPELL_DEATHBALL,
            0, x, y);
    }

    if (!target ||
        !nox_bot_engine_find_nearest_missile_owned_by(object, target, 500.0f))
        return 0;
    return nox_bot_conjurer_schedule(
        object, state, frame, NOX_BOT_CONJURER_SPELL_INVERSION,
        object, 0.0f, 0.0f);
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
    if (nox_bot_conjurer_try_random_summon(object, state, frame))
        return 1;
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

static int nox_bot_conjurer_pickup_type(int object, const char *type_name, int equip_kind)
{
    int item = nox_bot_engine_find_nearest_visible_type(
        object, type_name, NOX_BOT_CONJURER_LOOT_RADIUS);

    if (!item || !nox_bot_engine_pickup_item(object, item))
        return 0;
    if (equip_kind == 1)
        nox_bot_engine_equip_weapon(object, item);
    else if (equip_kind == 2)
        nox_bot_engine_equip_armor(object, item);
    return 1;
}

static void nox_bot_conjurer_loot_scan(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    static const char *const weapons[] = {
        "InfinitePainWand", "LesserFireballWand", "CrossBow", "Bow"
    };
    static const char *const armor[] = {
        "LeatherArmoredBoots", "LeatherArmor", "LeatherLeggings", "LeatherArmbands",
        "LeatherBoots", "MedievalCloak", "MedievalShirt", "MedievalPants"
    };
    static const char *const potions[] = {
        "RedPotion", "CurePoisonPotion", "BluePotion"
    };
    unsigned int i;

    if (state->conjurer.next_loot_scan_frame &&
        !nox_bot_reaction_ready(frame, state->conjurer.next_loot_scan_frame))
        return;
    state->conjurer.next_loot_scan_frame = frame + NOX_BOT_CONJURER_LOOT_SCAN_FRAMES;

    for (i = 0; i < sizeof(weapons) / sizeof(weapons[0]); ++i)
        nox_bot_conjurer_pickup_type(object, weapons[i], 1);
    /* The Go reference includes Quiver in its weapon search and then picks it
     * up again without equipping it. Keep the authoritative Nox quiver item in
     * inventory rather than forcing it through the player weapon equip path. */
    nox_bot_conjurer_pickup_type(object, "Quiver", 0);
    for (i = 0; i < sizeof(armor) / sizeof(armor[0]); ++i)
        nox_bot_conjurer_pickup_type(object, armor[i], 2);
    for (i = 0; i < sizeof(potions) / sizeof(potions[0]); ++i)
        nox_bot_conjurer_pickup_type(object, potions[i], 0);
}

static int nox_bot_conjurer_apply_weapon_preference(int object)
{
    int equipped = nox_bot_engine_equipped_weapon(object);
    int guard = nox_bot_engine_inventory_item(object, "CrossBow");
    int item;

    /* Preserve the Bot-Script reference literally. Its first guard checks for
     * a CrossBow but equips FireStormWand, and its second guard checks for
     * InfinitePainWand but equips ForceWand. Do not reinterpret those guards
     * as the items being equipped here. */
    if (guard && equipped != guard) {
        item = nox_bot_engine_inventory_item(object, "FireStormWand");
        return item ? nox_bot_engine_equip_weapon(object, item) : 0;
    }

    guard = nox_bot_engine_inventory_item(object, "InfinitePainWand");
    if (guard && equipped != guard) {
        item = nox_bot_engine_inventory_item(object, "ForceWand");
        return item ? nox_bot_engine_equip_weapon(object, item) : 0;
    }
    return 0;
}

static void nox_bot_conjurer_weapon_preference(
    int object, nox_bot_policy_state *state, uint32_t frame)
{
    uint32_t interval;

    if (state->conjurer.next_weapon_preference_frame &&
        !nox_bot_reaction_ready(frame, state->conjurer.next_weapon_preference_frame))
        return;
    interval = nox_bot_engine_fps() * NOX_BOT_CONJURER_WEAPON_PREFERENCE_SECONDS;
    if (!interval)
        interval = 1;
    state->conjurer.next_weapon_preference_frame = frame + interval;
    nox_bot_conjurer_apply_weapon_preference(object);
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

    if (nox_bot_policy_event_pending(state, NOX_BOT_EVENT_IS_HIT)) {
        if (nox_bot_engine_mana(object) <= 20)
            nox_bot_conjurer_go_to_mana_source(object, state);
        nox_bot_policy_clear_event(state, NOX_BOT_EVENT_IS_HIT);
    }

    if (nox_bot_policy_event_pending(state, NOX_BOT_EVENT_RETREAT)) {
        nox_bot_conjurer_try_blink(object, state, frame);
        nox_bot_policy_clear_event(state, NOX_BOT_EVENT_RETREAT);
    }

    if (nox_bot_policy_event_pending(state, NOX_BOT_EVENT_LOST_SIGHT)) {
        nox_bot_conjurer_try_infravision(object, state, frame);
        if (nox_bot_engine_is_ctf())
            nox_bot_team_ctf_walk_to_own_flag(object);
        nox_bot_policy_clear_event(state, NOX_BOT_EVENT_LOST_SIGHT);
    }

    if (nox_bot_policy_event_pending(state, NOX_BOT_EVENT_END_OF_WAYPOINT)) {
        conjurer->mana_route_active = 0;
        conjurer->mana_source = 0;
        nox_bot_engine_set_aggression(object, 0.83f);
        if (nox_bot_engine_mana(object) <= 49)
            nox_bot_conjurer_go_to_mana_source(object, state);
        else if (nox_bot_engine_is_ctf())
            nox_bot_team_ctf_attack_or_defend(object);
        nox_bot_policy_clear_event(state, NOX_BOT_EVENT_END_OF_WAYPOINT);
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
    nox_bot_conjurer_loot_scan(object, state, frame);
    nox_bot_conjurer_weapon_preference(object, state, frame);
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

    if (nox_bot_conjurer_try_missile_reaction(object, state, frame)) {
        nox_bot_conjurer_cap_mana(object);
        return;
    }

    nox_bot_conjurer_maybe_seek_mana(object, state);
    target = conjurer->target;
    nox_bot_conjurer_use_potions(object, target);
    nox_bot_conjurer_cap_mana(object);

    /* Bot-Script checks Pixie Swarm before Lesser Heal/offense and only casts
     * when it cannot find an owned Pixie. Native world ownership is the
     * authoritative replacement for the script-local PixieCount flag. */
    if (nox_bot_conjurer_try_pixie_swarm(object, state, frame))
        return;

    if (target && nox_bot_engine_health(target) > 0 &&
        nox_bot_engine_can_interact(object, target)) {
        if (nox_bot_conjurer_try_lesser_heal(object, state, frame))
            return;
        if ((nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_HELD) ||
             nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_SLOWED)) &&
            nox_bot_conjurer_try_blink(object, state, frame))
            return;
        if (nox_bot_conjurer_try_held_target(object, state, frame, target))
            return;
        nox_bot_conjurer_try_debuff(object, state, frame, target);
        return;
    }

    if (nox_bot_engine_has_buff(object, NOX_BOT_ENCHANT_SLOWED) &&
        nox_bot_conjurer_try_blink(object, state, frame))
        return;
    nox_bot_conjurer_try_hidden_buffs(object, state, frame);
}
