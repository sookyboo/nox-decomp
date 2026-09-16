#ifndef NOX_BOT_ENGINE_H
#define NOX_BOT_ENGINE_H

#include <stdint.h>

/*
 * Thin adapter over recovered Nox player/monster AI primitives.
 *
 * Bot policy must use this interface instead of depending on raw decompiler
 * names or byte_5D4594 offsets. Object values are the repository's existing
 * 32-bit runtime object handles/pointers.
 */
uint32_t nox_bot_engine_frame(void);
uint32_t nox_bot_engine_fps(void);

int nox_bot_engine_is_native_player_bot(int object);
int nox_bot_engine_player_slot(int object);
int nox_bot_engine_player_class(int object);

/*
 * These helpers only convert an already-created normal player object to/from
 * the original Nox player-monster update path. They do not allocate player
 * slots, create playerInfo, or free the native bot AI allocation.
 */
int nox_bot_engine_enable_existing_player_bot(int object);
int nox_bot_engine_disable_existing_player_bot(int object);

int nox_bot_engine_spell_id(const char *name);
int nox_bot_engine_ability_id(const char *name);

typedef enum nox_bot_native_ability {
    NOX_BOT_ABILITY_BERSERKER_CHARGE = 1,
    NOX_BOT_ABILITY_WARCRY = 2,
    NOX_BOT_ABILITY_HARPOON = 3,
    NOX_BOT_ABILITY_TREAD_LIGHTLY = 4,
    NOX_BOT_ABILITY_EYE_OF_THE_WOLF = 5,
} nox_bot_native_ability;

int nox_bot_engine_ability_cooldown_duration(int ability);
int nox_bot_engine_ability_cooldown_remaining(int object, int ability);
int nox_bot_engine_ability_active(int object, int ability);
int nox_bot_engine_ability_ready(int object, int ability);
int nox_bot_engine_execute_ability(int object, int ability);
void nox_bot_engine_face_target(int object, int target);

int nox_bot_engine_spell_allowed_for_class(int player_class, int spell);

int nox_bot_engine_is_enemy(int self, int other);
int nox_bot_engine_same_team(int self, int other);
int nox_bot_engine_has_buff(int object, int buff);
int nox_bot_engine_remove_buff(int object, int buff);
int nox_bot_engine_is_object_type(int object, const char *type_name);
/* Reproduces NoxScript CastSpell(source=object, target=object). */
int nox_bot_engine_cast_script_self(int object, const char *spell_name);
int nox_bot_engine_cast_script_object(int object, const char *spell_name, int target);
int nox_bot_engine_cast_script_position(int object, const char *spell_name, float x, float y);
void nox_bot_engine_face_position(int object, float x, float y);
int nox_bot_engine_current_target(int object);
int nox_bot_engine_harpoon_attached_target(int object);
int nox_bot_engine_stop_harpoon(int object);
int nox_bot_engine_health(int object);
int nox_bot_engine_max_health(int object);
int nox_bot_engine_mana(int object);
int nox_bot_engine_max_mana(int object);
int nox_bot_engine_mana_add(int object, int amount);
int nox_bot_engine_mana_sub(int object, int amount);
int nox_bot_engine_can_interact(int self, int other);
void nox_bot_engine_position(int object, float *x, float *y);
int nox_bot_engine_set_aggression(int object, float aggression);
int nox_bot_engine_is_ctf(void);
int nox_bot_engine_carrying_ctf_flag(int object);
int nox_bot_engine_ctf_flag_world(int object, int own_team);
int nox_bot_engine_ctf_flag_carrier(int object, int own_team);
int nox_bot_engine_ctf_flag_at_home(int flag);
int nox_bot_engine_use_inventory_potion(int object, const char *type_name);
int nox_bot_engine_inventory_item(int object, const char *type_name);
int nox_bot_engine_find_nearest_type(
    int object, const char *type_name, float max_distance);
int nox_bot_engine_find_nearest_visible_type(
    int object, const char *type_name, float max_distance);
int nox_bot_engine_find_nearest_world_type(
    int object, const char *type_name, float max_distance);
int nox_bot_engine_find_nearest_enemy_owned_type(
    int object, const char *type_name, float max_distance);
int nox_bot_engine_find_nearest_missile_owned_by(
    int object, int owner_target, float max_distance);
int nox_bot_engine_owned_type_count(int object, const char *type_name);
int nox_bot_engine_pickup_item(int object, int item);
int nox_bot_engine_equip_weapon(int object, int item);
int nox_bot_engine_equip_armor(int object, int item);
int nox_bot_engine_equipped_weapon(int object);
int nox_bot_engine_start_player_attack(int object);
int nox_bot_engine_player_attack_step(int object);

void nox_bot_engine_hunt(int object);
void nox_bot_engine_walk_to(int object, float x, float y);
void nox_bot_engine_attack_target(int object, int target);
void nox_bot_engine_guard_position(int object, float x, float y, float radius);
void nox_bot_engine_interrupt(int object);
int nox_bot_engine_action_scheduled(int object, int action);
void nox_bot_engine_cast(int object, int spell, int target);

#endif
