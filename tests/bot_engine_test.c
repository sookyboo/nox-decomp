#include "../src/bot_engine.h"
#include "../src/defs.h"

#include <stdint.h>
#include <string.h>

unsigned __int8 byte_5D4594[3844309];
unsigned __int8 byte_581450[23472];

static int last_hunt_object;
static int last_walk_object;
static int last_walk_x_bits;
static int last_walk_y_bits;
static int last_interrupt_object;
static int last_action_object;
static int last_action;
static int last_cast_object;
static int last_cast_spell;
static int last_cast_target;
static int morph_from_calls;
static int morph_to_calls;
static int player_bot_create_calls;
static int spell_lookup_calls;
static const char *spell_lookup_name;
static int ability_lookup_calls;
static const char *ability_lookup_name;
static int ability_execute_calls;
static int last_ability_object;
static int last_ability;
static int ability_active[6];
static float last_face_x;
static float last_face_y;
static int potion_type_id = 77;
static int potion_use_calls;
static int harpoon_stop_calls;
static int last_potion_player;
static int last_potion_item;
static int world_head;
static int world_test_player;
static int world_hidden_object;
static int pickup_calls;
static int last_pickup_player;
static int last_pickup_item;
static int equip_weapon_calls;
static int last_equip_weapon_player;
static int last_equip_weapon_item;
static int equip_armor_calls;
static int last_equip_armor_player;
static int last_equip_armor_item;
static int player_attack_start_calls;
static int player_attack_step_calls;
static int player_attack_step_result = 1;
static int aggression_set_calls;
static float last_aggression;
static int game_flags;
static int last_attack_object;
static int last_attack_target;
static int last_guard_object;
static float last_guard_x1;
static float last_guard_y1;
static float last_guard_x2;
static float last_guard_y2;
static float last_guard_radius;
static int same_team_self;
static int same_team_other;
static int enemy_self;
static int enemy_other;
static int script_cast_calls;
static int script_cast_spell;
static int script_cast_object;
static int script_cast_target;
static float script_cast_x;
static float script_cast_y;
static int script_cast_category;
static int script_cast_runtime;
static int buff_remove_calls;
static int last_removed_buff;
static int mana_add_calls;
static int mana_sub_calls;
static int last_mana_sub_object;
static int last_mana_sub_amount;
static int summon_cage_used_value;
static int summon_limit_object;
static int summon_limit_index;
static int summon_limit_result;
static int random_minimum;
static int random_maximum;
static int random_result;
static int glyph_create_calls;
static int glyph_place_calls;
static int glyph_destroy_calls;
static int glyph_owner_calls;
static int glyph_owner_object;
static int glyph_owner_item;
static float glyph_x;
static float glyph_y;
static unsigned char glyph_object[800];
static unsigned char glyph_init[64];
static unsigned char bomber_object[800];
static unsigned char bomber_runtime[1600];
static int bomber_summon_calls;
static int bomber_summon_type;
static int bomber_summon_owner;
static unsigned char bomber_summon_direction;
static float bomber_summon_x;
static float bomber_summon_y;
static int bomber_inventory_calls;
static int bomber_inventory_object;
static int bomber_inventory_item;
static int bomber_inventory_flag;
static int bomber_follow_calls;
static int bomber_follow_object;
static int bomber_follow_target;
static int bomber_sound_lookup_calls;
static const char *bomber_sound_lookup_name;
static int bomber_sound_calls;
static int bomber_sound_id;
static int bomber_sound_object;
static int bomber_sound_arg3;
static int bomber_sound_arg4;
static int bomber_mark_update_calls;
static int bomber_mark_update_object;
static char *player_info_by_slot[32];
static unsigned char spawn_profile[97];
static unsigned char spawn_server_options[104];
static unsigned char spawn_info[2300];
static unsigned char spawn_object[800];
static unsigned char spawn_runtime[400];
static unsigned char team_red[80];
static unsigned char team_blue[80];
static int spawn_join_calls;
static int spawn_join_result = 1;
static int spawn_remove_calls;
static int spawn_last_slot;
static int has_solo_maps;
static unsigned char spawn_last_packet[153];

static unsigned char created_bot_ai[0x898];

char *__cdecl sub_417090(int slot)
{
    if (slot < 0 || slot >= 32)
        return 0;
    return player_info_by_slot[slot];
}

char *sub_431770(void)
{
    return (char *)spawn_profile;
}

char *sub_416640(void)
{
    return (char *)spawn_server_options;
}

_DWORD *__cdecl sub_43BEB0(_DWORD *x, _DWORD *y, _DWORD *unused)
{
    if (x)
        *x = 1280;
    if (y)
        *y = 720;
    if (unused)
        *unused = 0;
    return x;
}

_DWORD *__cdecl sub_4DD320(int slot, int packet)
{
    ++spawn_join_calls;
    spawn_last_slot = slot;
    memcpy(spawn_last_packet, (const void *)(uintptr_t)packet, sizeof(spawn_last_packet));
    memset(spawn_info, 0, sizeof(spawn_info));
    memset(spawn_object, 0, sizeof(spawn_object));
    memset(spawn_runtime, 0, sizeof(spawn_runtime));
    *(uint32_t *)(spawn_info + 2056) = (uint32_t)(uintptr_t)spawn_object;
    memcpy(spawn_info + 2185, spawn_last_packet, 97);
    spawn_info[2064] = (unsigned char)slot;
    *(uint32_t *)(spawn_object + 8) = 4;
    *(uint32_t *)(spawn_object + 36) = 0x2468;
    *(uint32_t *)(spawn_object + 748) = (uint32_t)(uintptr_t)spawn_runtime;
    *(uint32_t *)(spawn_runtime + 276) = (uint32_t)(uintptr_t)spawn_info;
    player_info_by_slot[slot] = (char *)spawn_info;
    return spawn_join_result ? (_DWORD *)(uintptr_t)0x2468 : 0;
}

char *__cdecl sub_4DE7C0(int slot)
{
    ++spawn_remove_calls;
    if (slot >= 0 && slot < 32) {
        player_info_by_slot[slot] = 0;
        *(uint32_t *)(spawn_info + 2056) = 0;
    }
    return 0;
}

char *sub_418B10(void)
{
    return team_red[64] ? (char *)team_red : 0;
}

char *__cdecl sub_418B60(int team)
{
    if (team == (int)(uintptr_t)team_red && team_blue[64])
        return (char *)team_blue;
    return 0;
}

BOOL __cdecl sub_419130(int member)
{
    return member && *(unsigned char *)(member + 4) != 0;
}

char *__cdecl sub_4191D0(unsigned __int8 team_id, int member, int a3, int a4, int a5)
{
    (void)a3;
    (void)a4;
    (void)a5;
    *(unsigned char *)(member + 4) = team_id;
    return (char *)(uintptr_t)member;
}

int __cdecl sub_4196D0(int member, int team, int a3, int a4)
{
    (void)a3;
    (void)a4;
    *(unsigned char *)(member + 4) = *(unsigned char *)(team + 57);
    return 1;
}

char __cdecl sub_4F8100(_DWORD *object)
{
    (void)object;
    return 0;
}

int __cdecl sub_4FAB20(_DWORD *object)
{
    (void)object;
    return 0;
}

int __cdecl sub_4FA700(int object)
{
    int runtime = *(int *)(object + 748);

    ++player_bot_create_calls;
    memset(created_bot_ai, 0, sizeof(created_bot_ai));
    *(uint32_t *)(created_bot_ai + 2180) = (uint32_t)runtime;
    *(uint32_t *)(runtime + 292) = (uint32_t)(uintptr_t)created_bot_ai;
    return 0;
}

char __cdecl sub_4FAAC0(_DWORD *object)
{
    int runtime;

    ++morph_from_calls;
    runtime = object[187];
    object[2] = (object[2] & ~4u) | 2u;
    object[187] = *(_DWORD *)(runtime + 292);
    object[3] = 16;
    return (char)object[2];
}

char __cdecl sub_4FAAF0(_DWORD *object)
{
    int ai;

    ++morph_to_calls;
    ai = object[187];
    object[2] = (object[2] & ~2u) | 4u;
    object[187] = *(_DWORD *)(ai + 2180);
    object[3] = 0;
    return (char)object[2];
}

int __cdecl sub_51E1D0(const char *name)
{
    ++spell_lookup_calls;
    spell_lookup_name = name;
    if (strcmp(name, "Fireball") == 0)
        return 44;
    if (strcmp(name, "SLOW") == 0)
        return 55;
    if (strcmp(name, "SUMMON_WASP") == 0)
        return 75;
    if (strcmp(name, "BLINK") == 0)
        return 66;
    if (strcmp(name, "CLEANSING_FLAME") == 0)
        return 67;
    if (strcmp(name, "MAGIC_MISSILE") == 0)
        return 68;
    if (strcmp(name, "SHOCK") == 0)
        return 69;
    if (strcmp(name, "BURN") == 0)
        return 70;
    if (strcmp(name, "TOXIC_CLOUD") == 0)
        return 71;
    if (strcmp(name, "STUN") == 0)
        return 72;
    return 0;
}

int __cdecl sub_424D80(const char *name)
{
    ++ability_lookup_calls;
    ability_lookup_name = name;
    return strcmp(name, "ABILITY_HARPOON") == 0 ? 3 : 0;
}

int __cdecl sub_4FDD20(int spell, _DWORD *object, int *arg)
{
    ++script_cast_calls;
    script_cast_spell = spell;
    script_cast_object = (int)(uintptr_t)object;
    script_cast_target = arg ? arg[0] : 0;
    script_cast_x = arg ? ((float *)arg)[1] : 0.0f;
    script_cast_y = arg ? ((float *)arg)[2] : 0.0f;
    script_cast_category = object ? object[2] : 0;
    script_cast_runtime = object ? object[187] : 0;
    return 1;
}

int __cdecl sub_4FF5B0(int object, int buff)
{
    ++buff_remove_calls;
    last_removed_buff = buff;
    return object;
}

int __cdecl sub_4E3AA0(char *name)
{
    if (!name)
        return 0;
    if (strcmp(name, "RedPotion") == 0)
        return potion_type_id;
    if (strcmp(name, "GreatSword") == 0)
        return 78;
    if (strcmp(name, "WarHammer") == 0)
        return 79;
    if (strcmp(name, "Longsword") == 0)
        return 80;
    if (strcmp(name, "Bomber") == 0)
        return 81;
    if (strcmp(name, "Pixie") == 0)
        return 82;
    if (strcmp(name, "DeathBall") == 0)
        return 83;
    return 0;
}

int sub_4DA790(void)
{
    return world_head;
}

int __cdecl sub_4DA7A0(int object)
{
    return object ? *(int *)(object + 444) : 0;
}

int __cdecl sub_4F36F0(int player, int item, int a3, int a4)
{
    (void)a3;
    (void)a4;
    ++pickup_calls;
    last_pickup_player = player;
    last_pickup_item = item;
    *(int *)(item + 492) = player;
    return 1;
}

int __cdecl sub_53A420(_DWORD *player, int item, int a3, int a4)
{
    (void)a3;
    (void)a4;
    ++equip_weapon_calls;
    last_equip_weapon_player = (int)(uintptr_t)player;
    last_equip_weapon_item = item;
    return 1;
}

int __cdecl sub_53E650(_DWORD *player, int item, int a3, int a4)
{
    (void)a3;
    (void)a4;
    ++equip_armor_calls;
    last_equip_armor_player = (int)(uintptr_t)player;
    last_equip_armor_item = item;
    return 1;
}

void __cdecl sub_4F9C70(_DWORD *object)
{
    int runtime = object ? object[187] : 0;

    ++player_attack_start_calls;
    if (runtime)
        *(unsigned char *)(runtime + 88) = 1;
}

BOOL __cdecl sub_538960(int object)
{
    (void)object;
    ++player_attack_step_calls;
    return player_attack_step_result;
}

int __cdecl sub_53EF70(int player, int potion)
{
    ++potion_use_calls;
    last_potion_player = player;
    last_potion_item = potion;
    return 1;
}

int __cdecl sub_57AEA0(int player_class, int spell)
{
    return player_class == 1 && spell == 44 ? 0 : 9;
}

int __cdecl sub_4252D0(int ability)
{
    return ability * 90;
}

int __cdecl sub_4FC250(int object, int ability)
{
    (void)object;
    return ability >= 0 && ability < 6 ? ability_active[ability] : 0;
}

void __cdecl sub_4FBB70(int object, int ability)
{
    int runtime = *(int *)(object + 748);
    int info = *(int *)(runtime + 276);
    int slot = *(unsigned char *)(info + 2064);

    ++ability_execute_calls;
    last_ability_object = object;
    last_ability = ability;
    *(int *)&byte_5D4594[1568876 + 4 * (ability + 6 * slot)] = sub_4252D0(ability);
    ability_active[ability] = 1;
}

int __cdecl sub_509ED0(float2 *delta)
{
    last_face_x = delta->field_0;
    last_face_y = delta->field_4;
    return 77;
}

signed int __cdecl sub_53C580(int object)
{
    return object != 0;
}

int __cdecl sub_500D10(int object)
{
    return object ? summon_cage_used_value : 0;
}

bool __cdecl sub_500D70(int object, int summon_index)
{
    summon_limit_object = object;
    summon_limit_index = summon_index;
    return summon_limit_result != 0;
}

int __cdecl sub_415FA0(int minimum, int maximum)
{
    random_minimum = minimum;
    random_maximum = maximum;
    return random_result;
}

_DWORD *__cdecl sub_4E3810(CHAR *name)
{
    if (!name || strcmp(name, "Glyph") != 0)
        return 0;
    ++glyph_create_calls;
    memset(glyph_object, 0, sizeof(glyph_object));
    memset(glyph_init, 0, sizeof(glyph_init));
    *(uint32_t *)(glyph_object + 692) = (uint32_t)(uintptr_t)glyph_init;
    return (_DWORD *)glyph_object;
}

char __cdecl sub_4DAA50(int object, int owner, float x, float y)
{
    if (object == (int)(uintptr_t)glyph_object && owner == 0) {
        ++glyph_place_calls;
        glyph_x = x;
        glyph_y = y;
    }
    return 1;
}

int __cdecl sub_4E38A0(int object)
{
    if (object == (int)(uintptr_t)glyph_object)
        ++glyph_destroy_calls;
    return 1;
}

void __cdecl sub_4EC290(int owner, int item)
{
    ++glyph_owner_calls;
    glyph_owner_object = owner;
    glyph_owner_item = item;
}

_DWORD *__cdecl sub_5016C0(int type_id, int *pos, int owner, unsigned __int8 direction)
{
    ++bomber_summon_calls;
    bomber_summon_type = type_id;
    bomber_summon_owner = owner;
    bomber_summon_direction = direction;
    bomber_summon_x = pos ? ((float *)pos)[0] : 0.0f;
    bomber_summon_y = pos ? ((float *)pos)[1] : 0.0f;
    memset(bomber_object, 0, sizeof(bomber_object));
    memset(bomber_runtime, 0, sizeof(bomber_runtime));
    *(uint32_t *)(bomber_object + 8) = 2;
    *(uint32_t *)(bomber_object + 492) = (uint32_t)owner;
    *(uint32_t *)(bomber_object + 748) = (uint32_t)(uintptr_t)bomber_runtime;
    return (_DWORD *)bomber_object;
}

char *__cdecl sub_4E8020(int object)
{
    ++bomber_mark_update_calls;
    bomber_mark_update_object = object;
    return 0;
}

int __cdecl sub_40AF50(void *name)
{
    const char *value = (const char *)name;

    ++bomber_sound_lookup_calls;
    bomber_sound_lookup_name = value;
    if (!value)
        return 0;
    if (strcmp(value, "BomberSummon") == 0)
        return 73;
    if (strcmp(value, "SpellPhonemeUp") == 0)
        return 74;
    if (strcmp(value, "SpellPhonemeDown") == 0)
        return 75;
    if (strcmp(value, "NPCSpellPhonemeLeft") == 0)
        return 76;
    if (strcmp(value, "NPCSpellPhonemeRight") == 0)
        return 77;
    if (strcmp(value, "NPCSpellPhonemeUpLeft") == 0)
        return 78;
    if (strcmp(value, "NPCSpellPhonemeUpRight") == 0)
        return 79;
    if (strcmp(value, "NPCSpellPhonemeDownLeft") == 0)
        return 80;
    if (strcmp(value, "NPCSpellPhonemeDownRight") == 0)
        return 81;
    if (strcmp(value, "FemaleSpellPhonemeUpRight") == 0)
        return 82;
    return 0;
}

_DWORD *__cdecl sub_501960(int sound, int object, int arg3, int arg4)
{
    ++bomber_sound_calls;
    bomber_sound_id = sound;
    bomber_sound_object = object;
    bomber_sound_arg3 = arg3;
    bomber_sound_arg4 = arg4;
    return 0;
}

void __cdecl sub_4F3070(int object, int item, int flag)
{
    ++bomber_inventory_calls;
    bomber_inventory_object = object;
    bomber_inventory_item = item;
    bomber_inventory_flag = flag;
}

void __cdecl sub_5158C0(int object, int target)
{
    ++bomber_follow_calls;
    bomber_follow_object = object;
    bomber_follow_target = target;
}

int __cdecl sub_5330C0(int self, int other)
{
    if (self == enemy_self && other == enemy_other)
        return 1;
    return self == 10 && other == 20;
}

int __cdecl sub_4EC520(int self, int other)
{
    if (self == same_team_self && other == same_team_other)
        return 1;
    return self == 30 && other == 40;
}

int __cdecl sub_4FF350(int object, char buff)
{
    return object == 50 && buff == 7;
}

__int16 __cdecl sub_4EE780(int object)
{
    return object ? 75 : 0;
}

__int16 __cdecl sub_4EE7A0(int object)
{
    return object ? 150 : 0;
}

__int16 __cdecl sub_4EEC80(int object)
{
    return object ? 777 : 0;
}

__int16 __cdecl sub_4EECB0(int object)
{
    return object ? 888 : 0;
}

unsigned __int16 __cdecl sub_4EEB80(int object, __int16 amount)
{
    int runtime = object ? *(int *)(object + 748) : 0;

    ++mana_add_calls;
    if (runtime)
        *(unsigned short *)(runtime + 4) += (unsigned short)amount;
    return runtime ? *(unsigned short *)(runtime + 4) : 0;
}

_DWORD *__cdecl sub_4EEBF0(int object, int amount)
{
    int runtime = object ? *(int *)(object + 748) : 0;

    ++mana_sub_calls;
    last_mana_sub_object = object;
    last_mana_sub_amount = amount;
    if (runtime && *(unsigned short *)(runtime + 4) >= amount)
        *(unsigned short *)(runtime + 4) -= (unsigned short)amount;
    return (_DWORD *)(uintptr_t)runtime;
}

int __cdecl sub_5370E0(int self, int other, char flags)
{
    if (world_test_player && self == world_test_player)
        return flags == 0 && other != world_hidden_object;
    return self == 60 && other == 70 && flags == 0;
}

int __cdecl sub_515980(int object, _DWORD *value)
{
    int runtime = object ? *(int *)(object + 748) : 0;

    ++aggression_set_calls;
    if (value)
        memcpy(&last_aggression, value, sizeof(last_aggression));
    if (runtime && value) {
        *(_DWORD *)(runtime + 1304) = *value;
        *(_DWORD *)(runtime + 1308) = *value;
    }
    return runtime;
}

BOOL __cdecl sub_40A5C0(int flag)
{
    return (game_flags & flag) != 0;
}

BOOL sub_40ABD0(void)
{
    return has_solo_maps != 0;
}

_DWORD *__cdecl sub_537520(_DWORD *object)
{
    int runtime = object ? object[187] : 0;

    ++harpoon_stop_calls;
    if (runtime)
        *(_DWORD *)(runtime + 132) = 0;
    return object;
}

void __cdecl sub_5157A0(int object)
{
    last_hunt_object = object;
}

void __cdecl sub_515D30(int object, int target)
{
    last_attack_object = object;
    last_attack_target = target;
}

void __cdecl sub_515680(int object, int args_ptr)
{
    int *args = (int *)args_ptr;

    last_guard_object = object;
    memcpy(&last_guard_x1, &args[0], sizeof(last_guard_x1));
    memcpy(&last_guard_y1, &args[1], sizeof(last_guard_y1));
    memcpy(&last_guard_x2, &args[2], sizeof(last_guard_x2));
    memcpy(&last_guard_y2, &args[3], sizeof(last_guard_y2));
    memcpy(&last_guard_radius, &args[4], sizeof(last_guard_radius));
}

int *__cdecl sub_514110(int object, int x_bits, int y_bits)
{
    last_walk_object = object;
    last_walk_x_bits = x_bits;
    last_walk_y_bits = y_bits;
    return 0;
}

void __cdecl sub_50A3A0(int object)
{
    last_interrupt_object = object;
}

int __cdecl sub_50A090(int object, int action)
{
    last_action_object = object;
    last_action = action;
    return action == 8;
}

int *__cdecl sub_540A30(int object, int spell, int target)
{
    last_cast_object = object;
    last_cast_spell = spell;
    last_cast_target = target;
    return 0;
}

static void make_native_bot(
    unsigned char *object,
    unsigned char *runtime,
    unsigned char *info,
    unsigned char *ai,
    int slot,
    int player_class)
{
    memset(object, 0, 800);
    memset(runtime, 0, 400);
    memset(info, 0, 2300);
    memset(ai, 0, 0x898);

    *(uint32_t *)(object + 8) = 4;
    *(uint32_t *)(object + 748) = (uint32_t)(uintptr_t)runtime;
    *(uint16_t *)(runtime + 4) = 42;
    *(uint16_t *)(runtime + 8) = 100;
    *(uint32_t *)(runtime + 276) = (uint32_t)(uintptr_t)info;
    *(uint32_t *)(runtime + 292) = (uint32_t)(uintptr_t)ai;
    *(uint32_t *)(ai + 2180) = (uint32_t)(uintptr_t)runtime;
    info[2064] = (unsigned char)slot;
    info[2251] = (unsigned char)player_class;
    *(int (__cdecl **)(_DWORD *))(object + 744) = sub_4FAB20;
}

static int test_timing(void)
{
    *(uint32_t *)&byte_5D4594[2598000] = 1234;
    *(uint32_t *)&byte_5D4594[2649704] = 30;
    if (nox_bot_engine_frame() != 1234)
        return 1;
    if (nox_bot_engine_fps() != 30)
        return 2;
    return 0;
}

static int test_experimental_player_lifecycle_adapter(void)
{
    const wchar_t name[] = L"RedWizard03";
    int object;
    int i;
    int screen_x;
    int screen_y;

    memset(player_info_by_slot, 0, sizeof(player_info_by_slot));
    memset(spawn_profile, 0, sizeof(spawn_profile));
    memset(spawn_server_options, 0, sizeof(spawn_server_options));
    memset(team_red, 0, sizeof(team_red));
    memset(team_blue, 0, sizeof(team_blue));
    spawn_profile[66] = 0;
    spawn_profile[67] = 1;
    team_red[56] = 1;
    team_red[57] = 11;
    team_red[64] = 1;
    team_blue[56] = 2;
    team_blue[57] = 12;
    team_blue[64] = 1;
    spawn_join_calls = 0;
    spawn_join_result = 1;
    spawn_remove_calls = 0;
    has_solo_maps = 0;
    game_flags = 4096;
    byte_5D4594[2660684] = 4;
    player_info_by_slot[0] = (char *)1;
    player_info_by_slot[1] = (char *)1;

    if (nox_bot_engine_find_free_player_slot() != 2)
        return 5;
    player_info_by_slot[0] = 0;
    player_info_by_slot[1] = 0;
    spawn_server_options[100] = 1u << 1;
    if (nox_bot_engine_spawn_player_attempt(2, 1, NOX_BOT_SPAWN_TEAM_RED, name) ||
        spawn_join_calls)
        return 6;
    spawn_server_options[100] = 0;
    object = nox_bot_engine_spawn_player_attempt(2, 1, NOX_BOT_SPAWN_TEAM_RED, name);
    if (!object || spawn_join_calls != 1 || spawn_last_slot != 2)
        return 7;
    if (spawn_last_packet[66] != 1 || spawn_last_packet[67] != 1 ||
        spawn_last_packet[152] != 0x81)
        return 8;
    if (strcmp((const char *)&spawn_last_packet[105], "BOT-03") != 0 ||
        spawn_last_packet[128] || spawn_last_packet[138] || spawn_last_packet[142])
        return 14;
    for (i = 0; name[i]; ++i) {
        uint16_t ch;

        memcpy(&ch, spawn_last_packet + i * 2, sizeof(ch));
        if (ch != (uint16_t)name[i])
            return 9;
    }
    memcpy(&screen_x, spawn_last_packet + 97, sizeof(screen_x));
    memcpy(&screen_y, spawn_last_packet + 101, sizeof(screen_y));
    if (screen_x != 1280 || screen_y != 720)
        return 10;
    if (*(unsigned char *)(object + 52) != 11)
        return 11;
    if (nox_bot_engine_player_object_by_slot(2) != object)
        return 12;
    if (!nox_bot_engine_remove_player_attempt(2, object) || spawn_remove_calls != 1 ||
        nox_bot_engine_player_object_by_slot(2))
        return 13;

    /* If the native constructor reports failure after creating a player object,
     * the experimental adapter must roll that partial lifecycle back. */
    spawn_join_result = 0;
    spawn_remove_calls = 0;
    if (nox_bot_engine_spawn_player_attempt(2, 1, NOX_BOT_SPAWN_TEAM_AUTO, name) ||
        spawn_remove_calls != 1 || nox_bot_engine_player_object_by_slot(2))
        return 15;
    spawn_join_result = 1;
    byte_5D4594[2660684] = 0;
    game_flags = 0;
    return 0;
}

static int test_player_metadata_in_both_views(void)
{
    unsigned char object[800];
    unsigned char runtime[400];
    unsigned char info[2300];
    unsigned char ai[0x898];

    make_native_bot(object, runtime, info, ai, 7, 2);

    if (nox_bot_engine_player_slot((int)(uintptr_t)object) != 7)
        return 10;
    if (nox_bot_engine_player_class((int)(uintptr_t)object) != 2)
        return 11;
    if (!nox_bot_engine_is_native_player_bot((int)(uintptr_t)object))
        return 12;
    *(uint32_t *)(runtime + 132) = 0x12345678u;
    if (nox_bot_engine_harpoon_attached_target((int)(uintptr_t)object) != (int)0x12345678u)
        return 16;
    harpoon_stop_calls = 0;
    if (!nox_bot_engine_stop_harpoon((int)(uintptr_t)object))
        return 18;
    if (harpoon_stop_calls != 1 || nox_bot_engine_harpoon_attached_target((int)(uintptr_t)object) != 0)
        return 19;
    *(uint32_t *)(runtime + 132) = 0x12345678u;

    *(uint32_t *)(object + 8) = 2;
    *(uint32_t *)(object + 748) = (uint32_t)(uintptr_t)ai;
    if (nox_bot_engine_player_slot((int)(uintptr_t)object) != 7)
        return 13;
    if (nox_bot_engine_player_class((int)(uintptr_t)object) != 2)
        return 14;
    if (!nox_bot_engine_is_native_player_bot((int)(uintptr_t)object))
        return 15;
    if (nox_bot_engine_harpoon_attached_target((int)(uintptr_t)object) != (int)0x12345678u)
        return 17;
    return 0;
}

static int test_native_player_bot_actions_morph_safely(void)
{
    unsigned char object[800];
    unsigned char runtime[400];
    unsigned char info[2300];
    unsigned char ai[0x898];
    float x = 12.5f;
    float y = -4.25f;
    int x_bits;
    int y_bits;
    int object_ptr;

    make_native_bot(object, runtime, info, ai, 3, 0);
    object_ptr = (int)(uintptr_t)object;
    memcpy(&x_bits, &x, sizeof(x_bits));
    memcpy(&y_bits, &y, sizeof(y_bits));
    morph_from_calls = 0;
    morph_to_calls = 0;

    nox_bot_engine_hunt(object_ptr);
    if (last_hunt_object != object_ptr || morph_from_calls != 1 || morph_to_calls != 1)
        return 20;
    nox_bot_engine_walk_to(object_ptr, x, y);
    if (last_walk_object != object_ptr || last_walk_x_bits != x_bits || last_walk_y_bits != y_bits)
        return 21;
    nox_bot_engine_interrupt(object_ptr);
    if (last_interrupt_object != object_ptr)
        return 22;
    if (!nox_bot_engine_action_scheduled(object_ptr, 8))
        return 23;
    nox_bot_engine_cast(object_ptr, 9, 74);
    if (last_cast_object != object_ptr || last_cast_spell != 9 || last_cast_target != 74)
        return 24;
    nox_bot_engine_attack_target(object_ptr, 81);
    if (last_attack_object != object_ptr || last_attack_target != 81)
        return 25;
    nox_bot_engine_guard_position(object_ptr, 2.0f, 3.0f, 20.0f);
    if (last_guard_object != object_ptr || last_guard_x1 != 2.0f || last_guard_y1 != 3.0f ||
        last_guard_x2 != 2.0f || last_guard_y2 != 3.0f || last_guard_radius != 20.0f)
        return 26;
    if (morph_from_calls != 7 || morph_to_calls != 7)
        return 27;
    if (*(uint32_t *)(object + 8) != 4 || *(uint32_t *)(object + 748) != (uint32_t)(uintptr_t)runtime)
        return 28;
    return 0;
}

static int test_native_monster_actions_do_not_morph(void)
{
    unsigned char object[800];
    int object_ptr;

    memset(object, 0, sizeof(object));
    *(uint32_t *)(object + 8) = 2;
    object_ptr = (int)(uintptr_t)object;
    morph_from_calls = 0;
    morph_to_calls = 0;

    nox_bot_engine_hunt(object_ptr);
    if (last_hunt_object != object_ptr)
        return 30;
    if (morph_from_calls || morph_to_calls)
        return 31;
    return 0;
}

static int test_existing_player_activation(void)
{
    unsigned char object[800];
    unsigned char runtime[400];
    unsigned char info[2300];
    int object_ptr;

    memset(object, 0, sizeof(object));
    memset(runtime, 0, sizeof(runtime));
    memset(info, 0, sizeof(info));
    *(uint32_t *)(object + 8) = 4;
    *(uint32_t *)(object + 748) = (uint32_t)(uintptr_t)runtime;
    *(uint32_t *)(runtime + 276) = (uint32_t)(uintptr_t)info;
    info[2064] = 5;
    *(char (__cdecl **)(_DWORD *))(object + 744) = sub_4F8100;
    object_ptr = (int)(uintptr_t)object;
    player_bot_create_calls = 0;

    if (!nox_bot_engine_enable_existing_player_bot(object_ptr))
        return 40;
    if (player_bot_create_calls != 1 || !nox_bot_engine_is_native_player_bot(object_ptr))
        return 41;
    if (!*(uint32_t *)(runtime + 292))
        return 42;
    if (!nox_bot_engine_disable_existing_player_bot(object_ptr))
        return 43;
    if (*(char (__cdecl **)(_DWORD *))(object + 744) != sub_4F8100)
        return 44;
    if (!*(uint32_t *)(runtime + 292))
        return 45;
    if (!nox_bot_engine_enable_existing_player_bot(object_ptr))
        return 46;
    if (player_bot_create_calls != 2)
        return 47;
    return 0;
}

static int test_spell_and_relationship_wrappers(void)
{
    spell_lookup_calls = 0;
    spell_lookup_name = 0;
    if (nox_bot_engine_spell_id("Fireball") != 44)
        return 50;
    if (spell_lookup_calls != 1 || strcmp(spell_lookup_name, "Fireball") != 0)
        return 51;
    if (nox_bot_engine_spell_id(0) != 0 || nox_bot_engine_spell_id("") != 0)
        return 52;
    ability_lookup_calls = 0;
    ability_lookup_name = 0;
    if (nox_bot_engine_ability_id("ABILITY_HARPOON") != 3)
        return 53;
    if (ability_lookup_calls != 1 || strcmp(ability_lookup_name, "ABILITY_HARPOON") != 0)
        return 54;
    if (nox_bot_engine_ability_id(0) != 0 || nox_bot_engine_ability_id("") != 0)
        return 55;
    if (!nox_bot_engine_spell_allowed_for_class(1, 44))
        return 56;
    if (nox_bot_engine_spell_allowed_for_class(2, 44) || nox_bot_engine_spell_allowed_for_class(1, 0))
        return 57;
    if (!nox_bot_engine_is_enemy(10, 20) || nox_bot_engine_is_enemy(10, 30))
        return 58;
    if (!nox_bot_engine_same_team(30, 40) || nox_bot_engine_same_team(30, 50))
        return 59;
    if (!nox_bot_engine_has_buff(50, 7) || nox_bot_engine_has_buff(50, 8))
        return 60;
    return 0;
}

static int test_script_cast_buff_and_type_wrappers(void)
{
    unsigned char object[800];
    unsigned char runtime[400];
    unsigned char info[2300];
    unsigned char ai[0x898];
    int object_ptr;

    make_native_bot(object, runtime, info, ai, 9, 0);
    object_ptr = (int)(uintptr_t)object;
    *(unsigned short *)(object + 4) = 81;
    *(float *)(object + 56) = 3.0f;
    *(float *)(object + 60) = 4.0f;
    *(int *)(ai + 2040) = 3;
    script_cast_calls = 0;
    buff_remove_calls = 0;
    morph_from_calls = 0;
    morph_to_calls = 0;

    if (!nox_bot_engine_is_object_type(object_ptr, "Bomber") ||
        nox_bot_engine_is_object_type(object_ptr, "RedPotion"))
        return 61;
    if (!nox_bot_engine_cast_script_self(object_ptr, "SLOW"))
        return 62;
    if (script_cast_calls != 1 || script_cast_spell != 55 ||
        script_cast_object != object_ptr || script_cast_target != object_ptr ||
        script_cast_x != 3.0f || script_cast_y != 4.0f)
        return 63;
    if (!(script_cast_category & 2) || script_cast_runtime != (int)(uintptr_t)ai ||
        morph_from_calls != 1 || morph_to_calls != 1)
        return 64;
    if (*(uint32_t *)(object + 8) != 4 ||
        *(uint32_t *)(object + 748) != (uint32_t)(uintptr_t)runtime)
        return 65;
    {
        unsigned char target[800];
        int target_ptr;

        memset(target, 0, sizeof(target));
        target_ptr = (int)(uintptr_t)target;
        *(float *)(target + 56) = 8.0f;
        *(float *)(target + 60) = 10.0f;
        if (!nox_bot_engine_cast_script_object(object_ptr, "SLOW", target_ptr) ||
            script_cast_target != target_ptr || script_cast_x != 8.0f || script_cast_y != 10.0f)
            return 68;
        if (!nox_bot_engine_cast_script_position(object_ptr, "SLOW", 12.0f, 14.0f) ||
            script_cast_target != 0 || script_cast_x != 12.0f || script_cast_y != 14.0f)
            return 69;
        if (last_face_x != 9.0f || last_face_y != 10.0f)
            return 60;
    }
    if (!nox_bot_engine_remove_buff(object_ptr, 5))
        return 66;
    if (buff_remove_calls != 1 || last_removed_buff != 5)
        return 67;
    return 0;
}

static int test_tactical_observation_wrappers(void)
{
    unsigned char object[800];
    unsigned char runtime[400];
    unsigned char info[2300];
    unsigned char ai[0x898];
    float x = 0.0f;
    float y = 0.0f;
    int object_ptr;

    make_native_bot(object, runtime, info, ai, 8, 1);
    object_ptr = (int)(uintptr_t)object;
    *(uint32_t *)(ai + 1196) = 2468;
    *(float *)(object + 56) = 10.25f;
    *(float *)(object + 60) = -20.5f;
    morph_from_calls = 0;
    morph_to_calls = 0;

    if (nox_bot_engine_current_target(object_ptr) != 2468)
        return 70;
    if (morph_from_calls != 1 || morph_to_calls != 1)
        return 71;
    if (nox_bot_engine_health(object_ptr) != 75 || nox_bot_engine_max_health(object_ptr) != 150)
        return 72;
    if (nox_bot_engine_mana(object_ptr) != 42 || nox_bot_engine_max_mana(object_ptr) != 100)
        return 73;
    mana_add_calls = 0;
    if (!nox_bot_engine_mana_add(object_ptr, 3) || nox_bot_engine_mana(object_ptr) != 45 ||
        mana_add_calls != 1)
        return 80;
    mana_sub_calls = 0;
    if (!nox_bot_engine_mana_sub(object_ptr, 15) || nox_bot_engine_mana(object_ptr) != 30 ||
        mana_sub_calls != 1 || last_mana_sub_object != object_ptr || last_mana_sub_amount != 15)
        return 78;
    if (nox_bot_engine_mana_sub(object_ptr, 31) || mana_sub_calls != 1)
        return 79;
    *(uint32_t *)(object + 8) = 2;
    *(uint32_t *)(object + 748) = (uint32_t)(uintptr_t)ai;
    if (nox_bot_engine_mana(object_ptr) != 30 || nox_bot_engine_max_mana(object_ptr) != 100)
        return 74;
    *(uint32_t *)(object + 8) = 4;
    *(uint32_t *)(object + 748) = (uint32_t)(uintptr_t)runtime;
    if (!nox_bot_engine_can_interact(60, 70) || nox_bot_engine_can_interact(60, 71))
        return 75;
    nox_bot_engine_position(object_ptr, &x, &y);
    if (x != 10.25f || y != -20.5f)
        return 76;
    nox_bot_engine_position(0, &x, &y);
    if (x != 0.0f || y != 0.0f)
        return 77;
    return 0;
}


static int test_inventory_potion_wrapper(void)
{
    unsigned char object[800];
    unsigned char runtime[400];
    unsigned char info[2300];
    unsigned char ai[0x898];
    unsigned char wrong_item[600];
    unsigned char potion[600];
    int object_ptr;
    int potion_ptr;

    make_native_bot(object, runtime, info, ai, 9, 0);
    memset(wrong_item, 0, sizeof(wrong_item));
    memset(potion, 0, sizeof(potion));
    object_ptr = (int)(uintptr_t)object;
    potion_ptr = (int)(uintptr_t)potion;
    *(uint16_t *)(wrong_item + 4) = 12;
    *(uint16_t *)(potion + 4) = (uint16_t)potion_type_id;
    *(uint32_t *)(wrong_item + 496) = (uint32_t)(uintptr_t)potion;
    *(uint32_t *)(object + 504) = (uint32_t)(uintptr_t)wrong_item;
    potion_use_calls = 0;

    if (!nox_bot_engine_use_inventory_potion(object_ptr, "RedPotion"))
        return 80;
    if (potion_use_calls != 1 || last_potion_player != object_ptr || last_potion_item != potion_ptr)
        return 81;
    if (nox_bot_engine_use_inventory_potion(object_ptr, "BluePotion"))
        return 82;
    if (potion_use_calls != 1)
        return 83;
    *(uint32_t *)(object + 8) = 2;
    if (nox_bot_engine_use_inventory_potion(object_ptr, "RedPotion"))
        return 84;
    return 0;
}

static int test_world_loot_and_equipment_wrappers(void)
{
    unsigned char object[800];
    unsigned char runtime[400];
    unsigned char info[2300];
    unsigned char ai[0x898];
    unsigned char far_item[600];
    unsigned char hidden_item[600];
    unsigned char near_item[600];
    unsigned char armor_item[600];
    unsigned char pixie_a[600];
    unsigned char pixie_b[600];
    unsigned char deathball[600];
    unsigned char missile[600];
    unsigned char missile_owner_link[600];
    unsigned char enemy_owner[800];
    int object_ptr;
    int near_ptr;
    int armor_ptr;
    int enemy_owner_ptr;

    make_native_bot(object, runtime, info, ai, 10, 0);
    memset(far_item, 0, sizeof(far_item));
    memset(hidden_item, 0, sizeof(hidden_item));
    memset(near_item, 0, sizeof(near_item));
    memset(armor_item, 0, sizeof(armor_item));
    memset(pixie_a, 0, sizeof(pixie_a));
    memset(pixie_b, 0, sizeof(pixie_b));
    memset(deathball, 0, sizeof(deathball));
    memset(missile, 0, sizeof(missile));
    memset(missile_owner_link, 0, sizeof(missile_owner_link));
    memset(enemy_owner, 0, sizeof(enemy_owner));
    object_ptr = (int)(uintptr_t)object;
    near_ptr = (int)(uintptr_t)near_item;
    armor_ptr = (int)(uintptr_t)armor_item;
    enemy_owner_ptr = (int)(uintptr_t)enemy_owner;
    world_test_player = object_ptr;
    world_hidden_object = (int)(uintptr_t)hidden_item;
    world_head = (int)(uintptr_t)far_item;

    *(uint16_t *)(far_item + 4) = 78;
    *(float *)(far_item + 56) = 70.0f;
    *(uint32_t *)(far_item + 444) = (uint32_t)(uintptr_t)hidden_item;
    *(uint16_t *)(hidden_item + 4) = 78;
    *(float *)(hidden_item + 56) = 10.0f;
    *(uint32_t *)(hidden_item + 444) = (uint32_t)(uintptr_t)near_item;
    *(uint16_t *)(near_item + 4) = 78;
    *(float *)(near_item + 56) = 25.0f;

    if (nox_bot_engine_find_nearest_type(object_ptr, "GreatSword", 75.0f) !=
        (int)(uintptr_t)hidden_item)
        return 85;
    if (nox_bot_engine_find_nearest_visible_type(object_ptr, "GreatSword", 75.0f) != near_ptr)
        return 86;
    if (nox_bot_engine_find_nearest_visible_type(object_ptr, "GreatSword", 20.0f))
        return 87;

    *(uint32_t *)(near_item + 444) = (uint32_t)(uintptr_t)pixie_a;
    *(uint16_t *)(pixie_a + 4) = 82;
    *(uint32_t *)(pixie_a + 492) = (uint32_t)object_ptr;
    *(uint32_t *)(pixie_a + 444) = (uint32_t)(uintptr_t)pixie_b;
    *(uint16_t *)(pixie_b + 4) = 82;
    *(uint32_t *)(pixie_b + 492) = (uint32_t)(uintptr_t)pixie_a;
    if (nox_bot_engine_owned_type_count(object_ptr, "Pixie") != 2)
        return 95;
    *(unsigned char *)(pixie_b + 16) = 0x20;
    if (nox_bot_engine_owned_type_count(object_ptr, "Pixie") != 1)
        return 96;

    *(unsigned char *)(pixie_b + 16) = 0;
    *(uint32_t *)(pixie_b + 444) = (uint32_t)(uintptr_t)deathball;
    *(uint16_t *)(deathball + 4) = 83;
    *(float *)(deathball + 56) = 15.0f;
    *(uint32_t *)(deathball + 492) = (uint32_t)(uintptr_t)enemy_owner;
    *(uint32_t *)(deathball + 444) = (uint32_t)(uintptr_t)missile;
    *(uint32_t *)(enemy_owner + 8) = 2;
    *(uint32_t *)(missile + 8) = 1;
    *(float *)(missile + 56) = 20.0f;
    *(uint32_t *)(missile + 492) = (uint32_t)(uintptr_t)missile_owner_link;
    *(uint32_t *)(missile_owner_link + 492) = (uint32_t)enemy_owner_ptr;
    enemy_self = object_ptr;
    enemy_other = enemy_owner_ptr;
    if (nox_bot_engine_find_nearest_world_type(
            object_ptr, "DeathBall", 75.0f) != (int)(uintptr_t)deathball)
        return 97;
    if (nox_bot_engine_find_nearest_enemy_owned_type(
            object_ptr, "DeathBall", 75.0f) != (int)(uintptr_t)deathball)
        return 98;
    if (nox_bot_engine_find_nearest_missile_owned_by(
            object_ptr, enemy_owner_ptr, 75.0f) != (int)(uintptr_t)missile)
        return 99;
    if (nox_bot_engine_find_nearest_missile_owned_by(
            object_ptr, enemy_owner_ptr, 10.0f))
        return 118;
    *(uint32_t *)(deathball + 492) = (uint32_t)object_ptr;
    if (nox_bot_engine_find_nearest_enemy_owned_type(object_ptr, "DeathBall", 75.0f))
        return 119;
    enemy_self = 0;
    enemy_other = 0;

    pickup_calls = 0;
    if (!nox_bot_engine_pickup_item(object_ptr, near_ptr))
        return 88;
    if (pickup_calls != 1 || last_pickup_player != object_ptr || last_pickup_item != near_ptr)
        return 89;

    *(uint32_t *)(object + 504) = (uint32_t)(uintptr_t)near_item;
    *(uint32_t *)(near_item + 496) = 0;
    if (nox_bot_engine_inventory_item(object_ptr, "GreatSword") != near_ptr)
        return 90;
    equip_weapon_calls = 0;
    if (!nox_bot_engine_equip_weapon(object_ptr, near_ptr))
        return 91;
    if (equip_weapon_calls != 1 || last_equip_weapon_player != object_ptr ||
        last_equip_weapon_item != near_ptr)
        return 92;

    *(uint16_t *)(armor_item + 4) = 99;
    equip_armor_calls = 0;
    if (!nox_bot_engine_equip_armor(object_ptr, armor_ptr))
        return 93;
    if (equip_armor_calls != 1 || last_equip_armor_player != object_ptr ||
        last_equip_armor_item != armor_ptr)
        return 94;

    world_head = 0;
    world_test_player = 0;
    world_hidden_object = 0;
    return 0;
}

static int test_phoneme_audio_wrapper(void)
{
    static const char *const expected_names[] = {
        0,
        "SpellPhonemeUp",
        "SpellPhonemeDown",
        "NPCSpellPhonemeLeft",
        "NPCSpellPhonemeRight",
        "NPCSpellPhonemeUpLeft",
        "NPCSpellPhonemeUpRight",
        "NPCSpellPhonemeDownLeft",
        "NPCSpellPhonemeDownRight",
        "FemaleSpellPhonemeUpRight",
    };
    unsigned char object[800];
    int object_ptr = (int)(uintptr_t)object;
    int phoneme;

    memset(object, 0, sizeof(object));
    bomber_sound_lookup_calls = 0;
    bomber_sound_calls = 0;
    for (phoneme = NOX_BOT_PHONEME_UP;
         phoneme <= NOX_BOT_PHONEME_FEMALE_UP_RIGHT; ++phoneme) {
        if (!nox_bot_engine_play_phoneme(object_ptr, (nox_bot_phoneme)phoneme) ||
            bomber_sound_lookup_calls != phoneme ||
            !bomber_sound_lookup_name ||
            strcmp(bomber_sound_lookup_name, expected_names[phoneme]) != 0 ||
            bomber_sound_calls != phoneme || bomber_sound_id != 73 + phoneme ||
            bomber_sound_object != object_ptr || bomber_sound_arg3 || bomber_sound_arg4)
            return 126;
    }
    if (nox_bot_engine_play_phoneme(object_ptr, (nox_bot_phoneme)99))
        return 127;
    return 0;
}

static int test_mana_source_and_summon_wrappers(void)
{
    unsigned char object[800];
    unsigned char runtime[400];
    unsigned char info[2300];
    unsigned char ai[0x898];
    unsigned char near_source[800];
    unsigned char far_source[800];
    int near_charge = 20;
    int far_charge = 30;
    int object_ptr;
    int near_ptr;
    int far_ptr;

    make_native_bot(object, runtime, info, ai, 10, 0);
    memset(near_source, 0, sizeof(near_source));
    memset(far_source, 0, sizeof(far_source));
    object_ptr = (int)(uintptr_t)object;
    near_ptr = (int)(uintptr_t)near_source;
    far_ptr = (int)(uintptr_t)far_source;
    *(float *)(object + 56) = 0.0f;
    *(float *)(object + 60) = 0.0f;
    *(float *)(near_source + 56) = 10.0f;
    *(float *)(far_source + 56) = 30.0f;
    *(uint32_t *)(near_source + 744) = (uint32_t)(uintptr_t)sub_53C580;
    *(uint32_t *)(far_source + 744) = (uint32_t)(uintptr_t)sub_53C580;
    *(uint32_t *)(near_source + 748) = (uint32_t)(uintptr_t)&near_charge;
    *(uint32_t *)(far_source + 748) = (uint32_t)(uintptr_t)&far_charge;
    *(uint32_t *)(near_source + 444) = (uint32_t)(uintptr_t)far_source;
    world_head = near_ptr;
    world_test_player = object_ptr;
    world_hidden_object = near_ptr;

    if (nox_bot_engine_find_nearest_mana_source(object_ptr, 10, 0) != near_ptr)
        return 120;
    if (nox_bot_engine_find_nearest_mana_source(object_ptr, 10, 1) != far_ptr)
        return 121;
    near_charge = 5;
    if (nox_bot_engine_find_nearest_mana_source(object_ptr, 10, 0) != far_ptr)
        return 122;

    summon_cage_used_value = 3;
    if (nox_bot_engine_summon_cage_used(object_ptr) != 3)
        return 124;
    summon_limit_result = 1;
    summon_limit_object = 0;
    summon_limit_index = 0;
    if (!nox_bot_engine_summon_spell_fits(object_ptr, "SUMMON_WASP") ||
        summon_limit_object != object_ptr || summon_limit_index != 1)
        return 125;
    if (nox_bot_engine_summon_spell_fits(object_ptr, "NOT_A_SUMMON"))
        return 126;
    summon_limit_object = 0;
    summon_limit_index = 0;
    if (!nox_bot_engine_bomber_fits(object_ptr) ||
        summon_limit_object != object_ptr || summon_limit_index != 5)
        return 130;

    random_result = 2;
    if (nox_bot_engine_random_int(1, 3) != 2 || random_minimum != 1 || random_maximum != 3)
        return 127;

    glyph_create_calls = 0;
    glyph_place_calls = 0;
    glyph_destroy_calls = 0;
    glyph_owner_calls = 0;
    glyph_owner_object = 0;
    glyph_owner_item = 0;
    if (nox_bot_engine_create_spell_trap(object_ptr, "BLINK") !=
            (int)(uintptr_t)glyph_object ||
        glyph_create_calls != 1 || glyph_place_calls != 1 || glyph_destroy_calls ||
        glyph_x != 0.0f || glyph_y != 0.0f ||
        *(uint32_t *)(glyph_init + 0) != 66 ||
        *(uint32_t *)(glyph_init + 20) != 1 ||
        *(uint32_t *)(glyph_init + 24) != 0 ||
        *(float *)(glyph_init + 28) != 0.0f ||
        *(float *)(glyph_init + 32) != 0.0f)
        return 128;

    if (nox_bot_engine_create_owned_spell_trap3(
            object_ptr, "CLEANSING_FLAME", "MAGIC_MISSILE", "SHOCK") !=
            (int)(uintptr_t)glyph_object ||
        glyph_create_calls != 2 || glyph_place_calls != 2 || glyph_destroy_calls ||
        glyph_owner_calls != 1 || glyph_owner_object != object_ptr ||
        glyph_owner_item != (int)(uintptr_t)glyph_object ||
        *(uint32_t *)(glyph_init + 0) != 67 ||
        *(uint32_t *)(glyph_init + 4) != 68 ||
        *(uint32_t *)(glyph_init + 8) != 69 ||
        *(uint32_t *)(glyph_init + 20) != 3 ||
        *(uint32_t *)(glyph_init + 24) != 0 ||
        *(float *)(glyph_init + 28) != 0.0f ||
        *(float *)(glyph_init + 32) != 0.0f)
        return 129;

    *(unsigned char *)(object + 124) = 33;
    bomber_summon_calls = 0;
    bomber_summon_type = 0;
    bomber_summon_owner = 0;
    bomber_summon_direction = 0;
    bomber_summon_x = 0.0f;
    bomber_summon_y = 0.0f;
    bomber_inventory_calls = 0;
    bomber_inventory_object = 0;
    bomber_inventory_item = 0;
    bomber_inventory_flag = 0;
    bomber_follow_calls = 0;
    bomber_follow_object = 0;
    bomber_follow_target = 0;
    bomber_sound_lookup_calls = 0;
    bomber_sound_lookup_name = 0;
    bomber_sound_calls = 0;
    bomber_sound_id = 0;
    bomber_sound_object = 0;
    bomber_sound_arg3 = 0;
    bomber_sound_arg4 = 0;
    bomber_mark_update_calls = 0;
    bomber_mark_update_object = 0;
    if (nox_bot_engine_create_bomber(object_ptr) != (int)(uintptr_t)bomber_object ||
        bomber_summon_calls != 1 || bomber_summon_type != 81 ||
        bomber_summon_owner != object_ptr || bomber_summon_direction != 33 ||
        bomber_summon_x != 0.0f || bomber_summon_y != 0.0f ||
        bomber_sound_lookup_calls != 1 || !bomber_sound_lookup_name ||
        strcmp(bomber_sound_lookup_name, "BomberSummon") != 0 ||
        bomber_sound_calls != 1 || bomber_sound_id != 73 ||
        bomber_sound_object != (int)(uintptr_t)bomber_object ||
        bomber_sound_arg3 || bomber_sound_arg4 ||
        bomber_inventory_calls != 1 ||
        bomber_inventory_object != (int)(uintptr_t)bomber_object ||
        bomber_inventory_item != (int)(uintptr_t)glyph_object ||
        bomber_inventory_flag != 1 || bomber_follow_calls != 1 ||
        bomber_follow_object != (int)(uintptr_t)bomber_object ||
        bomber_follow_target != object_ptr ||
        nox_bot_engine_owner_player((int)(uintptr_t)bomber_object) != object_ptr ||
        (*(uint32_t *)(bomber_runtime + 1440) & 0x100u) == 0 ||
        bomber_mark_update_calls != 1 ||
        bomber_mark_update_object != (int)(uintptr_t)bomber_object ||
        glyph_create_calls != 3 ||
        glyph_place_calls != 2 || glyph_destroy_calls ||
        *(uint32_t *)(glyph_init + 0) != 70 ||
        *(uint32_t *)(glyph_init + 4) != 71 ||
        *(uint32_t *)(glyph_init + 8) != 72 ||
        *(uint32_t *)(glyph_init + 20) != 3 ||
        *(float *)(glyph_init + 28) != 0.0f ||
        *(float *)(glyph_init + 32) != 0.0f)
        return 131;

    world_head = 0;
    world_test_player = 0;
    world_hidden_object = 0;
    summon_cage_used_value = 0;
    summon_limit_result = 0;
    return 0;
}

static int test_aggression_and_game_mode_wrappers(void)
{
    unsigned char object[800];
    unsigned char runtime[400];
    unsigned char info[2300];
    unsigned char ai[0x898];
    unsigned char flag_item[600];
    int object_ptr;

    make_native_bot(object, runtime, info, ai, 11, 0);
    memset(flag_item, 0, sizeof(flag_item));
    object_ptr = (int)(uintptr_t)object;
    aggression_set_calls = 0;
    last_aggression = 0.0f;
    morph_from_calls = 0;
    morph_to_calls = 0;
    if (!nox_bot_engine_set_aggression(object_ptr, 0.16f))
        return 100;
    if (aggression_set_calls != 1 || last_aggression != 0.16f ||
        *(float *)(ai + 1304) != 0.16f || *(float *)(ai + 1308) != 0.16f)
        return 101;
    if (morph_from_calls != 1 || morph_to_calls != 1)
        return 102;

    game_flags = 0;
    if (nox_bot_engine_is_ctf())
        return 103;
    game_flags = 0x20;
    if (!nox_bot_engine_is_ctf())
        return 104;
    if (nox_bot_engine_carrying_ctf_flag(object_ptr))
        return 105;
    *(uint32_t *)(flag_item + 8) = 0x10000000u;
    *(uint32_t *)(object + 504) = (uint32_t)(uintptr_t)flag_item;
    if (!nox_bot_engine_carrying_ctf_flag(object_ptr))
        return 106;
    *(uint32_t *)(object + 8) = 2;
    if (nox_bot_engine_carrying_ctf_flag(object_ptr))
        return 107;
    game_flags = 0;
    return 0;
}

static int test_ctf_flag_state_wrappers(void)
{
    unsigned char object[800];
    unsigned char runtime[400];
    unsigned char info[2300];
    unsigned char ai[0x898];
    unsigned char own_flag[800];
    unsigned char enemy_flag[800];
    unsigned char carried_flag[800];
    unsigned char flag_runtime[16];
    unsigned char carrier[800];
    int object_ptr;
    int own_flag_ptr;
    int enemy_flag_ptr;
    int carrier_ptr;
    double tolerance = 0.5;

    make_native_bot(object, runtime, info, ai, 12, 0);
    memset(own_flag, 0, sizeof(own_flag));
    memset(enemy_flag, 0, sizeof(enemy_flag));
    memset(carried_flag, 0, sizeof(carried_flag));
    memset(flag_runtime, 0, sizeof(flag_runtime));
    memset(carrier, 0, sizeof(carrier));
    object_ptr = (int)(uintptr_t)object;
    own_flag_ptr = (int)(uintptr_t)own_flag;
    enemy_flag_ptr = (int)(uintptr_t)enemy_flag;
    carrier_ptr = (int)(uintptr_t)carrier;
    game_flags = 0x20;

    *(uint32_t *)(own_flag + 8) = 0x10000000u;
    *(uint32_t *)(enemy_flag + 8) = 0x10000000u;
    *(uint32_t *)(own_flag + 444) = (uint32_t)(uintptr_t)enemy_flag;
    world_head = own_flag_ptr;
    same_team_self = object_ptr;
    same_team_other = own_flag_ptr;
    if (nox_bot_engine_ctf_flag_world(object_ptr, 1) != own_flag_ptr)
        return 108;
    if (nox_bot_engine_ctf_flag_world(object_ptr, 0) != enemy_flag_ptr)
        return 109;

    memcpy(&byte_581450[10160], &tolerance, sizeof(tolerance));
    *(uint32_t *)(own_flag + 748) = (uint32_t)(uintptr_t)flag_runtime;
    *(float *)(flag_runtime + 0) = 10.0f;
    *(float *)(flag_runtime + 4) = 20.0f;
    *(float *)(own_flag + 56) = 10.25f;
    *(float *)(own_flag + 60) = 19.75f;
    if (!nox_bot_engine_ctf_flag_at_home(own_flag_ptr))
        return 110;
    *(float *)(own_flag + 56) = 11.0f;
    if (nox_bot_engine_ctf_flag_at_home(own_flag_ptr))
        return 111;
    *(uint32_t *)(carrier + 8) = 4;
    *(uint32_t *)(carried_flag + 8) = 0x10000000u;
    *(uint32_t *)(carrier + 504) = (uint32_t)(uintptr_t)carried_flag;
    world_head = carrier_ptr;
    same_team_other = (int)(uintptr_t)carried_flag;
    if (nox_bot_engine_ctf_flag_carrier(object_ptr, 1) != carrier_ptr)
        return 112;
    if (nox_bot_engine_ctf_flag_carrier(object_ptr, 0))
        return 113;

    game_flags = 0;
    world_head = 0;
    same_team_self = 0;
    same_team_other = 0;
    return 0;
}

static int test_player_weapon_attack_wrappers(void)
{
    unsigned char object[800];
    unsigned char runtime[400];
    unsigned char info[2300];
    unsigned char ai[0x898];
    int object_ptr;

    make_native_bot(object, runtime, info, ai, 6, 0);
    object_ptr = (int)(uintptr_t)object;
    *(uint32_t *)(runtime + 104) = 12345;
    player_attack_start_calls = 0;
    player_attack_step_calls = 0;
    player_attack_step_result = 1;

    if (nox_bot_engine_equipped_weapon(object_ptr) != 12345)
        return 94;
    if (!nox_bot_engine_start_player_attack(object_ptr))
        return 95;
    if (player_attack_start_calls != 1 || *(unsigned char *)(runtime + 88) != 1)
        return 96;
    if (!nox_bot_engine_player_attack_step(object_ptr) || player_attack_step_calls != 1)
        return 97;

    player_attack_step_result = 0;
    if (nox_bot_engine_player_attack_step(object_ptr) || player_attack_step_calls != 2)
        return 98;

    *(uint32_t *)(object + 8) = 2;
    if (nox_bot_engine_start_player_attack(object_ptr) ||
        nox_bot_engine_player_attack_step(object_ptr))
        return 99;
    return 0;
}

static int test_native_ability_wrappers(void)
{
    unsigned char object[800];
    unsigned char target[800];
    unsigned char runtime[400];
    unsigned char info[2300];
    unsigned char ai[0x898];
    int object_ptr;
    int target_ptr;
    int *cooldown;

    make_native_bot(object, runtime, info, ai, 6, 0);
    memset(target, 0, sizeof(target));
    object_ptr = (int)(uintptr_t)object;
    target_ptr = (int)(uintptr_t)target;
    cooldown = (int *)&byte_5D4594[1568876 +
        4 * (NOX_BOT_ABILITY_WARCRY + 6 * 6)];
    *cooldown = 0;
    memset(ability_active, 0, sizeof(ability_active));
    ability_execute_calls = 0;

    if (nox_bot_engine_ability_cooldown_duration(NOX_BOT_ABILITY_WARCRY) != 180)
        return 90;
    if (!nox_bot_engine_ability_ready(object_ptr, NOX_BOT_ABILITY_WARCRY))
        return 91;
    if (!nox_bot_engine_execute_ability(object_ptr, NOX_BOT_ABILITY_WARCRY))
        return 92;
    if (ability_execute_calls != 1 || last_ability_object != object_ptr ||
        last_ability != NOX_BOT_ABILITY_WARCRY)
        return 93;
    if (nox_bot_engine_ability_cooldown_remaining(object_ptr, NOX_BOT_ABILITY_WARCRY) != 180)
        return 94;
    if (!nox_bot_engine_ability_active(object_ptr, NOX_BOT_ABILITY_WARCRY))
        return 95;
    if (nox_bot_engine_ability_ready(object_ptr, NOX_BOT_ABILITY_WARCRY))
        return 96;

    *(float *)(object + 56) = 10.0f;
    *(float *)(object + 60) = 20.0f;
    *(float *)(target + 56) = 13.0f;
    *(float *)(target + 60) = 26.0f;
    nox_bot_engine_face_target(object_ptr, target_ptr);
    if (last_face_x != 3.0f || last_face_y != 6.0f || *(short *)(object + 124) != 77)
        return 97;

    *(uint32_t *)(object + 8) = 2;
    if (nox_bot_engine_execute_ability(object_ptr, NOX_BOT_ABILITY_EYE_OF_THE_WOLF))
        return 98;
    return 0;
}

int main(void)
{
    int result;

    result = test_timing();
    if (result)
        return result;
    result = test_experimental_player_lifecycle_adapter();
    if (result)
        return result;
    result = test_player_metadata_in_both_views();
    if (result)
        return result;
    result = test_native_player_bot_actions_morph_safely();
    if (result)
        return result;
    result = test_native_monster_actions_do_not_morph();
    if (result)
        return result;
    result = test_existing_player_activation();
    if (result)
        return result;
    result = test_spell_and_relationship_wrappers();
    if (result)
        return result;
    result = test_script_cast_buff_and_type_wrappers();
    if (result)
        return result;
    result = test_tactical_observation_wrappers();
    if (result)
        return result;
    result = test_inventory_potion_wrapper();
    if (result)
        return result;
    result = test_world_loot_and_equipment_wrappers();
    if (result)
        return result;
    result = test_phoneme_audio_wrapper();
    if (result)
        return result;
    result = test_mana_source_and_summon_wrappers();
    if (result)
        return result;
    result = test_aggression_and_game_mode_wrappers();
    if (result)
        return result;
    result = test_ctf_flag_state_wrappers();
    if (result)
        return result;
    result = test_player_weapon_attack_wrappers();
    if (result)
        return result;
    return test_native_ability_wrappers();
}
