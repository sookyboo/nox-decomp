#include "bot_engine.h"
#include "proto.h"

#include <stddef.h>
#include <stdint.h>
#include "netextras_types.h"
#include <string.h>

#define NOX_OBJECT_CATEGORY_OFFSET 8
#define NOX_OBJECT_UPDATE_OFFSET 744
#define NOX_OBJECT_RUNTIME_OFFSET 748
#define NOX_OBJECT_MONSTER_CATEGORY 0x02u
#define NOX_OBJECT_PLAYER_CATEGORY 0x04u

#define NOX_PLAYER_RUNTIME_MANA_OFFSET 4
#define NOX_PLAYER_RUNTIME_MAX_MANA_OFFSET 8
#define NOX_PLAYER_RUNTIME_INFO_OFFSET 276
#define NOX_PLAYER_RUNTIME_BOT_AI_OFFSET 292
#define NOX_PLAYER_RUNTIME_HARPOON_TARGET_OFFSET 132
#define NOX_PLAYER_RUNTIME_STATE_OFFSET 88
#define NOX_PLAYER_RUNTIME_EQUIPPED_WEAPON_OFFSET 104
#define NOX_PLAYER_INFO_SLOT_OFFSET 2064
#define NOX_PLAYER_INFO_CLASS_OFFSET 2251
#define NOX_PLAYER_BOT_AI_CURRENT_TARGET_OFFSET 1196
#define NOX_PLAYER_BOT_AI_PLAYER_RUNTIME_OFFSET 2180

#define NOX_OBJECT_TYPE_ID_OFFSET 4
#define NOX_OBJECT_X_OFFSET 56
#define NOX_OBJECT_Y_OFFSET 60
#define NOX_OBJECT_OWNER_OFFSET 492
#define NOX_OBJECT_INVENTORY_NEXT_OFFSET 496
#define NOX_OBJECT_INVENTORY_HEAD_OFFSET 504
#define NOX_OBJECT_STATE_FLAGS_OFFSET 16
#define NOX_OBJECT_STATE_REMOVED 0x20u
#define NOX_OBJECT_CLASS_CTF_FLAG 0x10000000u
#define NOX_CTF_FLAG_HOME_TOLERANCE_OFFSET 10160

#define NOX_GAME_FRAME_OFFSET 2598000
#define NOX_GAME_FPS_OFFSET 2649704
#define NOX_ABILITY_COOLDOWN_BASE_OFFSET 1568876
#define NOX_ABILITY_COUNT_WITH_INVALID 6


static int nox_bot_engine_ability_valid(int ability)
{
    return ability >= NOX_BOT_ABILITY_BERSERKER_CHARGE &&
           ability <= NOX_BOT_ABILITY_EYE_OF_THE_WOLF;
}

static int nox_bot_engine_update_is_native_bot(int object)
{
    if (!object)
        return 0;
    return *(int (__cdecl **)(_DWORD *))(object + NOX_OBJECT_UPDATE_OFFSET) == sub_4FAB20;
}

static int nox_bot_engine_player_runtime(int object)
{
    unsigned int category;
    int runtime;

    if (!object)
        return 0;
    category = *(unsigned char *)(object + NOX_OBJECT_CATEGORY_OFFSET);
    runtime = *(int *)(object + NOX_OBJECT_RUNTIME_OFFSET);
    if (!runtime)
        return 0;
    if (category & NOX_OBJECT_PLAYER_CATEGORY)
        return runtime;
    if ((category & NOX_OBJECT_MONSTER_CATEGORY) && nox_bot_engine_update_is_native_bot(object))
        return *(int *)(runtime + NOX_PLAYER_BOT_AI_PLAYER_RUNTIME_OFFSET);
    return 0;
}

static int nox_bot_engine_player_info(int object)
{
    int runtime = nox_bot_engine_player_runtime(object);

    if (!runtime)
        return 0;
    return *(int *)(runtime + NOX_PLAYER_RUNTIME_INFO_OFFSET);
}

/*
 * Native player bots are ordinary player objects outside monster-AI update.
 * Monster helpers require category 0x02 and object+748 to point at the native
 * monster-AI block, so direct policy actions temporarily enter the same view
 * used by nox_xxx_updatePlayerMonsterBot_4FAB20.
 */
static int nox_bot_engine_begin_monster_view(int object, int *morphed)
{
    unsigned int category;
    int runtime;

    *morphed = 0;
    if (!object)
        return 0;
    category = *(unsigned char *)(object + NOX_OBJECT_CATEGORY_OFFSET);
    if (category & NOX_OBJECT_MONSTER_CATEGORY)
        return 1;
    if (!(category & NOX_OBJECT_PLAYER_CATEGORY) || !nox_bot_engine_update_is_native_bot(object))
        return 0;
    runtime = nox_bot_engine_player_runtime(object);
    if (!runtime || !*(int *)(runtime + NOX_PLAYER_RUNTIME_BOT_AI_OFFSET))
        return 0;
    sub_4FAAC0((_DWORD *)object);
    if (!(*(unsigned char *)(object + NOX_OBJECT_CATEGORY_OFFSET) & NOX_OBJECT_MONSTER_CATEGORY))
        return 0;
    *morphed = 1;
    return 1;
}

static void nox_bot_engine_end_monster_view(int object, int morphed)
{
    if (object && morphed)
        sub_4FAAF0((_DWORD *)object);
}

uint32_t nox_bot_engine_frame(void)
{
    return *(uint32_t *)&byte_5D4594[NOX_GAME_FRAME_OFFSET];
}

uint32_t nox_bot_engine_fps(void)
{
    return *(uint32_t *)&byte_5D4594[NOX_GAME_FPS_OFFSET];
}

int nox_bot_engine_is_native_player_bot(int object)
{
    unsigned int category;

    if (!nox_bot_engine_update_is_native_bot(object))
        return 0;
    category = *(unsigned char *)(object + NOX_OBJECT_CATEGORY_OFFSET);
    return (category & (NOX_OBJECT_PLAYER_CATEGORY | NOX_OBJECT_MONSTER_CATEGORY)) != 0;
}

int nox_bot_engine_player_slot(int object)
{
    int info = nox_bot_engine_player_info(object);
    int slot;

    if (!info)
        return -1;
    slot = *(unsigned char *)(info + NOX_PLAYER_INFO_SLOT_OFFSET);
    if (slot < 0 || slot >= 32)
        return -1;
    return slot;
}

int nox_bot_engine_player_class(int object)
{
    int info = nox_bot_engine_player_info(object);

    if (!info)
        return -1;
    return *(unsigned char *)(info + NOX_PLAYER_INFO_CLASS_OFFSET);
}

int nox_bot_engine_enable_existing_player_bot(int object)
{
    int runtime;
    int (__cdecl *update)(_DWORD *);

    if (!object || !(*(unsigned char *)(object + NOX_OBJECT_CATEGORY_OFFSET) & NOX_OBJECT_PLAYER_CATEGORY))
        return 0;
    runtime = nox_bot_engine_player_runtime(object);
    if (!runtime || !nox_bot_engine_player_info(object))
        return 0;
    update = *(int (__cdecl **)(_DWORD *))(object + NOX_OBJECT_UPDATE_OFFSET);
    if (update != (int (__cdecl *)(_DWORD *))sub_4F8100 && update != sub_4FAB20)
        return 0;
    if (update != sub_4FAB20 || !*(int *)(runtime + NOX_PLAYER_RUNTIME_BOT_AI_OFFSET))
        sub_4FA700(object);
    if (!*(int *)(runtime + NOX_PLAYER_RUNTIME_BOT_AI_OFFSET))
        return 0;
    *(int (__cdecl **)(_DWORD *))(object + NOX_OBJECT_UPDATE_OFFSET) = sub_4FAB20;
    return 1;
}

int nox_bot_engine_disable_existing_player_bot(int object)
{
    if (!object || !(*(unsigned char *)(object + NOX_OBJECT_CATEGORY_OFFSET) & NOX_OBJECT_PLAYER_CATEGORY))
        return 0;
    if (!nox_bot_engine_update_is_native_bot(object))
        return 0;
    *(char (__cdecl **)(_DWORD *))(object + NOX_OBJECT_UPDATE_OFFSET) = sub_4F8100;
    return 1;
}

int nox_bot_engine_spell_id(const char *name)
{
    if (!name || !*name)
        return 0;
    return sub_51E1D0(name);
}

int nox_bot_engine_ability_id(const char *name)
{
    if (!name || !*name)
        return 0;
    return sub_424D80(name);
}

int nox_bot_engine_ability_cooldown_duration(int ability)
{
    if (!nox_bot_engine_ability_valid(ability))
        return 0;
    return sub_4252D0(ability);
}

int nox_bot_engine_ability_cooldown_remaining(int object, int ability)
{
    int slot;

    if (!nox_bot_engine_ability_valid(ability))
        return 0;
    slot = nox_bot_engine_player_slot(object);
    if (slot < 0)
        return 0;
    return *(int *)&byte_5D4594[NOX_ABILITY_COOLDOWN_BASE_OFFSET +
        4 * (ability + NOX_ABILITY_COUNT_WITH_INVALID * slot)];
}

int nox_bot_engine_ability_active(int object, int ability)
{
    if (!object || !nox_bot_engine_ability_valid(ability))
        return 0;
    if (!(*(unsigned char *)(object + NOX_OBJECT_CATEGORY_OFFSET) & NOX_OBJECT_PLAYER_CATEGORY))
        return 0;
    return sub_4FC250(object, ability) != 0;
}

int nox_bot_engine_ability_ready(int object, int ability)
{
    if (!object || !nox_bot_engine_ability_valid(ability))
        return 0;
    if (nox_bot_engine_player_class(object) != 0)
        return 0;
    return nox_bot_engine_ability_cooldown_remaining(object, ability) == 0 &&
           !nox_bot_engine_ability_active(object, ability);
}

int nox_bot_engine_execute_ability(int object, int ability)
{
    int cooldown_before;
    int active_before;
    int cooldown_after;
    int active_after;

    if (!object || !nox_bot_engine_ability_valid(ability))
        return 0;
    /* Player abilities are authoritative only in the normal player view. */
    if (!(*(unsigned char *)(object + NOX_OBJECT_CATEGORY_OFFSET) & NOX_OBJECT_PLAYER_CATEGORY))
        return 0;
    if (nox_bot_engine_player_class(object) != 0)
        return 0;
    cooldown_before = nox_bot_engine_ability_cooldown_remaining(object, ability);
    active_before = nox_bot_engine_ability_active(object, ability);
    if (cooldown_before || active_before)
        return 0;
    sub_4FBB70(object, ability);
    cooldown_after = nox_bot_engine_ability_cooldown_remaining(object, ability);
    active_after = nox_bot_engine_ability_active(object, ability);
    return cooldown_after != cooldown_before || (!active_before && active_after);
}

void nox_bot_engine_face_target(int object, int target)
{
    float2 delta;

    if (!object || !target)
        return;
    delta.field_0 = *(float *)(target + NOX_OBJECT_X_OFFSET) -
        *(float *)(object + NOX_OBJECT_X_OFFSET);
    delta.field_4 = *(float *)(target + NOX_OBJECT_Y_OFFSET) -
        *(float *)(object + NOX_OBJECT_Y_OFFSET);
    if (delta.field_0 == 0.0f && delta.field_4 == 0.0f)
        return;
    *(short *)(object + 124) = (short)sub_509ED0(&delta);
}

void nox_bot_engine_face_position(int object, float x, float y)
{
    float2 delta;

    if (!object)
        return;
    delta.field_0 = x - *(float *)(object + NOX_OBJECT_X_OFFSET);
    delta.field_4 = y - *(float *)(object + NOX_OBJECT_Y_OFFSET);
    if (delta.field_0 == 0.0f && delta.field_4 == 0.0f)
        return;
    *(short *)(object + 124) = (short)sub_509ED0(&delta);
}

int nox_bot_engine_spell_allowed_for_class(int player_class, int spell)
{
    if (spell <= 0)
        return 0;
    return sub_57AEA0(player_class, spell) == 0;
}

int nox_bot_engine_is_enemy(int self, int other)
{
    if (!self || !other)
        return 0;
    return sub_5330C0(self, other) != 0;
}

int nox_bot_engine_same_team(int self, int other)
{
    if (!self || !other)
        return 0;
    return sub_4EC520(self, other) != 0;
}

int nox_bot_engine_has_buff(int object, int buff)
{
    if (!object || buff < 0 || buff > 255)
        return 0;
    return sub_4FF350(object, (char)buff) != 0;
}

int nox_bot_engine_remove_buff(int object, int buff)
{
    if (!object || buff < 0 || buff > 255)
        return 0;
    sub_4FF5B0(object, buff);
    return !nox_bot_engine_has_buff(object, buff);
}

int nox_bot_engine_is_object_type(int object, const char *type_name)
{
    int type_id;

    if (!object || !type_name || !*type_name)
        return 0;
    type_id = sub_4E3AA0((CHAR *)type_name);
    if (type_id <= 0)
        return 0;
    return *(unsigned short *)(object + NOX_OBJECT_TYPE_ID_OFFSET) ==
        (unsigned short)type_id;
}

static int nox_bot_engine_cast_script_arg(
    int object, const char *spell_name, int target, float x, float y)
{
    struct nox_bot_spell_accept_arg {
        int object;
        float x;
        float y;
    } arg;
    int morphed;
    int result;
    int spell;

    if (!object || !spell_name || !*spell_name)
        return 0;
    spell = nox_bot_engine_spell_id(spell_name);
    if (spell <= 0 || !nox_bot_engine_begin_monster_view(object, &morphed))
        return 0;

    /*
     * OpenNox/NoxScript SpellAcceptArg is {Object *Obj; Pointf Pos}. CastSpell
     * faces Pos, then calls the direct spell dispatcher. Entering the native
     * player-bot monster view makes sub_4FE7B0 read AI spell power 3, matching
     * NPC/script semantics instead of player learned-spell state.
     */
    nox_bot_engine_face_position(object, x, y);
    arg.object = target;
    arg.x = x;
    arg.y = y;
    result = sub_4FDD20(spell, (_DWORD *)object, (int *)&arg);
    nox_bot_engine_end_monster_view(object, morphed);
    return result != 0;
}

int nox_bot_engine_cast_script_self(int object, const char *spell_name)
{
    if (!object)
        return 0;
    return nox_bot_engine_cast_script_arg(
        object, spell_name, object,
        *(float *)(object + NOX_OBJECT_X_OFFSET),
        *(float *)(object + NOX_OBJECT_Y_OFFSET));
}

int nox_bot_engine_cast_script_object(int object, const char *spell_name, int target)
{
    if (!object || !target)
        return 0;
    return nox_bot_engine_cast_script_arg(
        object, spell_name, target,
        *(float *)(target + NOX_OBJECT_X_OFFSET),
        *(float *)(target + NOX_OBJECT_Y_OFFSET));
}

int nox_bot_engine_cast_script_position(int object, const char *spell_name, float x, float y)
{
    return nox_bot_engine_cast_script_arg(object, spell_name, 0, x, y);
}

int nox_bot_engine_current_target(int object)
{
    int morphed;
    int target;

    if (!nox_bot_engine_begin_monster_view(object, &morphed))
        return 0;
    target = *(int *)(*(int *)(object + NOX_OBJECT_RUNTIME_OFFSET) + NOX_PLAYER_BOT_AI_CURRENT_TARGET_OFFSET);
    nox_bot_engine_end_monster_view(object, morphed);
    return target;
}

int nox_bot_engine_harpoon_attached_target(int object)
{
    int runtime = nox_bot_engine_player_runtime(object);

    if (!runtime)
        return 0;
    return *(int *)(runtime + NOX_PLAYER_RUNTIME_HARPOON_TARGET_OFFSET);
}

int nox_bot_engine_stop_harpoon(int object)
{
    int runtime = nox_bot_engine_player_runtime(object);

    if (!object || !runtime)
        return 0;
    if (!(*(unsigned char *)(object + NOX_OBJECT_CATEGORY_OFFSET) & NOX_OBJECT_PLAYER_CATEGORY))
        return 0;
    if (!*(int *)(runtime + NOX_PLAYER_RUNTIME_HARPOON_TARGET_OFFSET))
        return 0;
    /* sub_537520 is the native Harpoon break/cleanup path. */
    sub_537520((_DWORD *)object);
    return *(int *)(runtime + NOX_PLAYER_RUNTIME_HARPOON_TARGET_OFFSET) == 0;
}

int nox_bot_engine_health(int object)
{
    if (!object)
        return 0;
    return (unsigned short)sub_4EE780(object);
}

int nox_bot_engine_max_health(int object)
{
    if (!object)
        return 0;
    return (unsigned short)sub_4EE7A0(object);
}

int nox_bot_engine_mana(int object)
{
    int runtime = nox_bot_engine_player_runtime(object);

    if (runtime)
        return *(unsigned short *)(runtime + NOX_PLAYER_RUNTIME_MANA_OFFSET);
    if (!object)
        return 0;
    return (unsigned short)sub_4EEC80(object);
}

int nox_bot_engine_max_mana(int object)
{
    int runtime = nox_bot_engine_player_runtime(object);

    if (runtime)
        return *(unsigned short *)(runtime + NOX_PLAYER_RUNTIME_MAX_MANA_OFFSET);
    if (!object)
        return 0;
    return (unsigned short)sub_4EECB0(object);
}

int nox_bot_engine_mana_add(int object, int amount)
{
    int before;

    if (!object || amount < 0 || amount > 0x7FFF)
        return 0;
    before = nox_bot_engine_mana(object);
    if (!amount)
        return 1;
    sub_4EEB80(object, (short)amount);
    return nox_bot_engine_mana(object) > before;
}

int nox_bot_engine_mana_sub(int object, int amount)
{
    int before;

    if (!object || amount < 0)
        return 0;
    before = nox_bot_engine_mana(object);
    if (before < amount)
        return 0;
    if (!amount)
        return 1;
    sub_4EEBF0(object, amount);
    return nox_bot_engine_mana(object) == before - amount;
}

int nox_bot_engine_can_interact(int self, int other)
{
    if (!self || !other)
        return 0;
    return sub_5370E0(self, other, 0) != 0;
}

void nox_bot_engine_position(int object, float *x, float *y)
{
    if (x)
        *x = object ? *(float *)(object + NOX_OBJECT_X_OFFSET) : 0.0f;
    if (y)
        *y = object ? *(float *)(object + NOX_OBJECT_Y_OFFSET) : 0.0f;
}

int nox_bot_engine_set_aggression(int object, float aggression)
{
    int morphed;
    int bits;

    if (!nox_bot_engine_begin_monster_view(object, &morphed))
        return 0;
    memcpy(&bits, &aggression, sizeof(bits));
    sub_515980(object, (_DWORD *)&bits);
    nox_bot_engine_end_monster_view(object, morphed);
    return 1;
}

int nox_bot_engine_is_ctf(void)
{
    return sub_40A5C0(NOX_GF_MODE_CTF) != 0;
}

int nox_bot_engine_carrying_ctf_flag(int object)
{
    int item;

    if (!object || !(*(unsigned char *)(object + NOX_OBJECT_CATEGORY_OFFSET) & NOX_OBJECT_PLAYER_CATEGORY))
        return 0;
    for (item = *(int *)(object + NOX_OBJECT_INVENTORY_HEAD_OFFSET); item;
         item = *(int *)(item + NOX_OBJECT_INVENTORY_NEXT_OFFSET)) {
        if (*(uint32_t *)(item + NOX_OBJECT_CATEGORY_OFFSET) & NOX_OBJECT_CLASS_CTF_FLAG)
            return 1;
    }
    return 0;
}

static int nox_bot_engine_ctf_flag_matches_team(int object, int flag, int own_team)
{
    int same_team;

    if (!object || !flag ||
        !(*(uint32_t *)(flag + NOX_OBJECT_CATEGORY_OFFSET) & NOX_OBJECT_CLASS_CTF_FLAG))
        return 0;
    same_team = sub_4EC520(object, flag) != 0;
    return own_team ? same_team : !same_team;
}

int nox_bot_engine_ctf_flag_world(int object, int own_team)
{
    int flag;

    if (!object || !nox_bot_engine_is_ctf())
        return 0;
    for (flag = sub_4DA790(); flag; flag = sub_4DA7A0(flag)) {
        if ((*(unsigned char *)(flag + NOX_OBJECT_STATE_FLAGS_OFFSET) & NOX_OBJECT_STATE_REMOVED) ||
            !nox_bot_engine_ctf_flag_matches_team(object, flag, own_team))
            continue;
        return flag;
    }
    return 0;
}

int nox_bot_engine_ctf_flag_carrier(int object, int own_team)
{
    int carrier;
    int item;

    if (!object || !nox_bot_engine_is_ctf())
        return 0;
    for (carrier = sub_4DA790(); carrier; carrier = sub_4DA7A0(carrier)) {
        if (!(*(unsigned char *)(carrier + NOX_OBJECT_CATEGORY_OFFSET) & NOX_OBJECT_PLAYER_CATEGORY) ||
            (*(unsigned char *)(carrier + NOX_OBJECT_STATE_FLAGS_OFFSET) & NOX_OBJECT_STATE_REMOVED))
            continue;
        for (item = *(int *)(carrier + NOX_OBJECT_INVENTORY_HEAD_OFFSET); item;
             item = *(int *)(item + NOX_OBJECT_INVENTORY_NEXT_OFFSET)) {
            if (nox_bot_engine_ctf_flag_matches_team(object, item, own_team))
                return carrier;
        }
    }
    return 0;
}

int nox_bot_engine_ctf_flag_at_home(int flag)
{
    double tolerance;
    float dx;
    float dy;
    int update_data;

    if (!flag ||
        !(*(uint32_t *)(flag + NOX_OBJECT_CATEGORY_OFFSET) & NOX_OBJECT_CLASS_CTF_FLAG))
        return 0;
    update_data = *(int *)(flag + NOX_OBJECT_RUNTIME_OFFSET);
    if (!update_data)
        return 0;
    tolerance = *(double *)&byte_581450[NOX_CTF_FLAG_HOME_TOLERANCE_OFFSET];
    dx = *(float *)(flag + NOX_OBJECT_X_OFFSET) - *(float *)update_data;
    dy = *(float *)(flag + NOX_OBJECT_Y_OFFSET) - *(float *)(update_data + 4);
    if (dx < 0.0f)
        dx = -dx;
    if (dy < 0.0f)
        dy = -dy;
    return (double)dx <= tolerance && (double)dy <= tolerance;
}

int nox_bot_engine_inventory_item(int object, const char *type_name)
{
    int item;
    int type_id;

    if (!object || !type_name || !*type_name)
        return 0;
    if (!(*(unsigned char *)(object + NOX_OBJECT_CATEGORY_OFFSET) & NOX_OBJECT_PLAYER_CATEGORY))
        return 0;
    type_id = sub_4E3AA0((CHAR *)type_name);
    if (type_id <= 0)
        return 0;
    for (item = *(int *)(object + NOX_OBJECT_INVENTORY_HEAD_OFFSET); item;
         item = *(int *)(item + NOX_OBJECT_INVENTORY_NEXT_OFFSET)) {
        if (*(unsigned short *)(item + NOX_OBJECT_TYPE_ID_OFFSET) == (unsigned short)type_id)
            return item;
    }
    return 0;
}

int nox_bot_engine_use_inventory_potion(int object, const char *type_name)
{
    int item = nox_bot_engine_inventory_item(object, type_name);

    if (!item)
        return 0;
    /* nox_xxx_usePotion_53EF70 owns potion effects and item consumption. */
    return sub_53EF70(object, item) != 0;
}

static int nox_bot_engine_find_nearest_type_impl(
    int object, const char *type_name, float max_distance, int require_visible)
{
    float dx;
    float dy;
    float distance;
    float best_distance;
    int best = 0;
    int item;
    int type_id;

    if (!object || !type_name || !*type_name)
        return 0;
    type_id = sub_4E3AA0((CHAR *)type_name);
    if (type_id <= 0)
        return 0;
    best_distance = max_distance > 0.0f ? max_distance * max_distance : 3.4e38f;
    for (item = sub_4DA790(); item; item = sub_4DA7A0(item)) {
        if (item == object ||
            *(unsigned short *)(item + NOX_OBJECT_TYPE_ID_OFFSET) != (unsigned short)type_id ||
            (*(unsigned char *)(item + NOX_OBJECT_STATE_FLAGS_OFFSET) & NOX_OBJECT_STATE_REMOVED) ||
            *(int *)(item + NOX_OBJECT_OWNER_OFFSET))
            continue;
        dx = *(float *)(item + NOX_OBJECT_X_OFFSET) - *(float *)(object + NOX_OBJECT_X_OFFSET);
        dy = *(float *)(item + NOX_OBJECT_Y_OFFSET) - *(float *)(object + NOX_OBJECT_Y_OFFSET);
        distance = dx * dx + dy * dy;
        if (distance > best_distance || (require_visible && !sub_5370E0(object, item, 0)))
            continue;
        best = item;
        best_distance = distance;
    }
    return best;
}

int nox_bot_engine_find_nearest_type(
    int object, const char *type_name, float max_distance)
{
    return nox_bot_engine_find_nearest_type_impl(object, type_name, max_distance, 0);
}

int nox_bot_engine_find_nearest_visible_type(
    int object, const char *type_name, float max_distance)
{
    return nox_bot_engine_find_nearest_type_impl(object, type_name, max_distance, 1);
}

int nox_bot_engine_find_nearest_enemy_owned_type(
    int object, const char *type_name, float max_distance)
{
    float dx;
    float dy;
    float distance;
    float best_distance;
    int best = 0;
    int item;
    int type_id;

    if (!object || !type_name || !*type_name)
        return 0;
    type_id = sub_4E3AA0((CHAR *)type_name);
    if (type_id <= 0)
        return 0;
    best_distance = max_distance > 0.0f ? max_distance * max_distance : 3.4e38f;
    for (item = sub_4DA790(); item; item = sub_4DA7A0(item)) {
        int owner;
        int enemy_owner = 0;
        int depth;

        if (*(unsigned short *)(item + NOX_OBJECT_TYPE_ID_OFFSET) != (unsigned short)type_id ||
            (*(unsigned char *)(item + NOX_OBJECT_STATE_FLAGS_OFFSET) & NOX_OBJECT_STATE_REMOVED))
            continue;
        for (owner = *(int *)(item + NOX_OBJECT_OWNER_OFFSET), depth = 0;
             owner && depth < 32; ++depth) {
            unsigned int category = *(unsigned char *)(owner + NOX_OBJECT_CATEGORY_OFFSET);

            if (owner == object)
                break;
            if ((category & (NOX_OBJECT_PLAYER_CATEGORY | NOX_OBJECT_MONSTER_CATEGORY)) &&
                sub_5330C0(object, owner)) {
                enemy_owner = 1;
                break;
            }
            owner = *(int *)(owner + NOX_OBJECT_OWNER_OFFSET);
        }
        if (!enemy_owner)
            continue;
        dx = *(float *)(item + NOX_OBJECT_X_OFFSET) - *(float *)(object + NOX_OBJECT_X_OFFSET);
        dy = *(float *)(item + NOX_OBJECT_Y_OFFSET) - *(float *)(object + NOX_OBJECT_Y_OFFSET);
        distance = dx * dx + dy * dy;
        if (distance > best_distance)
            continue;
        best = item;
        best_distance = distance;
    }
    return best;
}

int nox_bot_engine_owned_type_count(int object, const char *type_name)
{
    int count = 0;
    int item;
    int type_id;

    if (!object || !type_name || !*type_name)
        return 0;
    type_id = sub_4E3AA0((CHAR *)type_name);
    if (type_id <= 0)
        return 0;
    for (item = sub_4DA790(); item; item = sub_4DA7A0(item)) {
        int owner;
        int depth;

        if (*(unsigned short *)(item + NOX_OBJECT_TYPE_ID_OFFSET) != (unsigned short)type_id ||
            (*(unsigned char *)(item + NOX_OBJECT_STATE_FLAGS_OFFSET) & NOX_OBJECT_STATE_REMOVED))
            continue;
        /* NoxScript HasOwner follows the native owner chain. Keep a small
         * defensive bound so corrupt/cyclic ownership cannot hang policy. */
        for (owner = item, depth = 0; owner && depth < 32; ++depth) {
            if (owner == object) {
                ++count;
                break;
            }
            owner = *(int *)(owner + NOX_OBJECT_OWNER_OFFSET);
        }
    }
    return count;
}

int nox_bot_engine_pickup_item(int object, int item)
{
    if (!object || !item ||
        !(*(unsigned char *)(object + NOX_OBJECT_CATEGORY_OFFSET) & NOX_OBJECT_PLAYER_CATEGORY))
        return 0;
    /* sub_4F36F0 dispatches the item's normal native pickup implementation. */
    return sub_4F36F0(object, item, 1, 1) == 1;
}

int nox_bot_engine_equip_weapon(int object, int item)
{
    if (!object || !item ||
        !(*(unsigned char *)(object + NOX_OBJECT_CATEGORY_OFFSET) & NOX_OBJECT_PLAYER_CATEGORY))
        return 0;
    return sub_53A420((_DWORD *)object, item, 1, 1) != 0;
}

int nox_bot_engine_equip_armor(int object, int item)
{
    if (!object || !item ||
        !(*(unsigned char *)(object + NOX_OBJECT_CATEGORY_OFFSET) & NOX_OBJECT_PLAYER_CATEGORY))
        return 0;
    return sub_53E650((_DWORD *)object, item, 1, 1) != 0;
}

int nox_bot_engine_equipped_weapon(int object)
{
    int runtime = nox_bot_engine_player_runtime(object);

    if (!runtime)
        return 0;
    return *(int *)(runtime + NOX_PLAYER_RUNTIME_EQUIPPED_WEAPON_OFFSET);
}

int nox_bot_engine_start_player_attack(int object)
{
    int runtime = nox_bot_engine_player_runtime(object);

    if (!object || !runtime ||
        !(*(unsigned char *)(object + NOX_OBJECT_CATEGORY_OFFSET) & NOX_OBJECT_PLAYER_CATEGORY))
        return 0;
    /* nox_xxx_playerInputAttack_4F9C70 owns native player attack start/state. */
    sub_4F9C70((_DWORD *)object);
    return *(unsigned char *)(runtime + NOX_PLAYER_RUNTIME_STATE_OFFSET) == 1;
}

int nox_bot_engine_player_attack_step(int object)
{
    int runtime = nox_bot_engine_player_runtime(object);

    if (!object || !runtime ||
        !(*(unsigned char *)(object + NOX_OBJECT_CATEGORY_OFFSET) & NOX_OBJECT_PLAYER_CATEGORY))
        return 0;
    /* nox_xxx_playerAttack_538960 owns the equipped-weapon attack lifecycle. */
    return sub_538960(object) != 0;
}

void nox_bot_engine_hunt(int object)
{
    int morphed;

    if (!nox_bot_engine_begin_monster_view(object, &morphed))
        return;
    sub_5157A0(object);
    nox_bot_engine_end_monster_view(object, morphed);
}

void nox_bot_engine_walk_to(int object, float x, float y)
{
    int morphed;
    int x_bits;
    int y_bits;

    if (!nox_bot_engine_begin_monster_view(object, &morphed))
        return;
    memcpy(&x_bits, &x, sizeof(x_bits));
    memcpy(&y_bits, &y, sizeof(y_bits));
    sub_514110(object, x_bits, y_bits);
    nox_bot_engine_end_monster_view(object, morphed);
}

void nox_bot_engine_attack_target(int object, int target)
{
    int morphed;

    if (!target || !nox_bot_engine_begin_monster_view(object, &morphed))
        return;
    sub_515D30(object, target);
    nox_bot_engine_end_monster_view(object, morphed);
}

void nox_bot_engine_guard_position(int object, float x, float y, float radius)
{
    int morphed;
    int args[5];

    if (!nox_bot_engine_begin_monster_view(object, &morphed))
        return;
    memcpy(&args[0], &x, sizeof(x));
    memcpy(&args[1], &y, sizeof(y));
    memcpy(&args[2], &x, sizeof(x));
    memcpy(&args[3], &y, sizeof(y));
    memcpy(&args[4], &radius, sizeof(radius));
    sub_515680(object, (int)(uintptr_t)args);
    nox_bot_engine_end_monster_view(object, morphed);
}

void nox_bot_engine_interrupt(int object)
{
    int morphed;

    if (!nox_bot_engine_begin_monster_view(object, &morphed))
        return;
    sub_50A3A0(object);
    nox_bot_engine_end_monster_view(object, morphed);
}

int nox_bot_engine_action_scheduled(int object, int action)
{
    int morphed;
    int result;

    if (!nox_bot_engine_begin_monster_view(object, &morphed))
        return 0;
    result = sub_50A090(object, action) != 0;
    nox_bot_engine_end_monster_view(object, morphed);
    return result;
}

void nox_bot_engine_cast(int object, int spell, int target)
{
    int morphed;

    if (!nox_bot_engine_begin_monster_view(object, &morphed))
        return;
    sub_540A30(object, spell, target);
    nox_bot_engine_end_monster_view(object, morphed);
}
