#ifndef NOX_BOT_POLICY_H
#define NOX_BOT_POLICY_H

#include <stdint.h>

#define NOX_BOT_PLAYER_SLOTS 32

typedef enum nox_bot_difficulty {
    NOX_BOT_DIFFICULTY_HARDCORE = 0,
    NOX_BOT_DIFFICULTY_HARD,
    NOX_BOT_DIFFICULTY_NORMAL,
    NOX_BOT_DIFFICULTY_EASY,
    NOX_BOT_DIFFICULTY_BEGINNER,
} nox_bot_difficulty;

typedef enum nox_bot_order {
    NOX_BOT_ORDER_AUTO = 0,
    NOX_BOT_ORDER_FOLLOW,
    NOX_BOT_ORDER_ATTACK,
    NOX_BOT_ORDER_GUARD,
    NOX_BOT_ORDER_STAY,
    NOX_BOT_ORDER_ESCORT,
} nox_bot_order;

typedef enum nox_bot_event {
    NOX_BOT_EVENT_LOOKING_FOR_ENEMY = 0,
    NOX_BOT_EVENT_ENEMY_SIGHTED,
    NOX_BOT_EVENT_CHANGE_FOCUS,
    NOX_BOT_EVENT_IS_HIT,
    NOX_BOT_EVENT_RETREAT,
    NOX_BOT_EVENT_DEATH,
    NOX_BOT_EVENT_COLLISION,
    NOX_BOT_EVENT_ENEMY_HEARD,
    NOX_BOT_EVENT_END_OF_WAYPOINT,
    NOX_BOT_EVENT_LOST_SIGHT,
    NOX_BOT_EVENT_COUNT,
} nox_bot_event;

typedef struct nox_bot_wizard_policy_state {
    uint32_t global_ready_frame;
    uint32_t next_mana_regen_frame;
    uint32_t slow_ready_frame;
    uint32_t death_ray_ready_frame;
    uint32_t fireball_ready_frame;
    uint32_t burn_ready_frame;
    uint32_t magic_missile_ready_frame;
    uint32_t counterspell_ready_frame;
    uint32_t shield_ready_frame;
    uint32_t lesser_heal_ready_frame;
    uint32_t haste_ready_frame;
    uint32_t shock_ready_frame;
    uint32_t protect_shock_ready_frame;
    uint32_t protect_fire_ready_frame;
    uint32_t invisibility_ready_frame;
    uint32_t pending_cast_frame;
    int target;
    int pending_target;
    float pending_x;
    float pending_y;
    unsigned char pending_spell;
} nox_bot_wizard_policy_state;

typedef struct nox_bot_warrior_policy_state {
    uint32_t next_ability_scan_frame;
    uint32_t next_loot_scan_frame;
    uint32_t next_weapon_preference_frame;
    uint32_t chakram_ready_frame;
    int chakram_item;
    unsigned char chakram_attack_active;
    uint32_t pending_ability_frame;
    int pending_ability_target;
    int pending_ability;
    uint32_t harpoon_charge_frame;
    int harpoon_charge_target;
    int last_harpoon_target;
    unsigned char harpoon_charge_pending;
    unsigned char seeking_potion;
    unsigned char teleport_wake_tracking;
    unsigned char teleport_wake_event_seen;
    unsigned char lost_sight_ctf_handled;
    unsigned char charge_collision_pending;
    uint32_t protected_hold_until;
    uint32_t teleport_wake_event_frame;
    int teleport_wake_target;
    float teleport_wake_x;
    float teleport_wake_y;
} nox_bot_warrior_policy_state;

typedef struct nox_bot_policy_state {
    unsigned char active;
    unsigned char difficulty;
    unsigned char order;
    unsigned char reserved;
    uint32_t next_reaction_frame;
    int native_object;
    int ordered_target;
    float ordered_x;
    float ordered_y;
    uint32_t pending_events;
    uint32_t event_frame[NOX_BOT_EVENT_COUNT];
    int event_object[NOX_BOT_EVENT_COUNT];
    nox_bot_warrior_policy_state warrior;
    nox_bot_wizard_policy_state wizard;
} nox_bot_policy_state;

uint32_t nox_bot_reaction_frames(nox_bot_difficulty difficulty);
uint32_t nox_bot_reaction_deadline(uint32_t frame, nox_bot_difficulty difficulty);
int nox_bot_reaction_ready(uint32_t frame, uint32_t deadline);

void nox_bot_policy_reset_all(void);
void nox_bot_policy_reset(int player_slot);
nox_bot_policy_state *nox_bot_policy_get(int player_slot);
int nox_bot_policy_activate(int player_slot, nox_bot_difficulty difficulty, uint32_t frame);
void nox_bot_policy_deactivate(int player_slot);
void nox_bot_policy_clear_life_state(nox_bot_policy_state *state);
int nox_bot_policy_set_difficulty(int player_slot, nox_bot_difficulty difficulty, uint32_t frame);
void nox_bot_policy_schedule_reaction(nox_bot_policy_state *state, uint32_t frame);

void nox_bot_policy_record_event(
    nox_bot_policy_state *state,
    nox_bot_event event,
    int event_object,
    uint32_t frame);
int nox_bot_policy_event_pending(const nox_bot_policy_state *state, nox_bot_event event);
int nox_bot_policy_event_object(const nox_bot_policy_state *state, nox_bot_event event);
uint32_t nox_bot_policy_event_frame(const nox_bot_policy_state *state, nox_bot_event event);
void nox_bot_policy_clear_event(nox_bot_policy_state *state, nox_bot_event event);

#endif
