#ifndef NOX_BOT_WARRIOR_H
#define NOX_BOT_WARRIOR_H

#include "bot_policy.h"

/*
 * High-confidence Warrior policy subset from the Go Bot-Script reference.
 *
 * Native Harpoon, Berserker Charge, health-potion recovery, War Cry, Eye of
 * the Wolf, held-state escape, TeleportWake pursuit, and CTF steering are
 * enabled. Nox remains authoritative for abilities, movement/collision, spell
 * effects, enchants, inventory, and objective mechanics.
 */
void nox_bot_warrior_observe_collision(
    int object, nox_bot_policy_state *state, int other, uint32_t frame);
void nox_bot_warrior_update(int object, nox_bot_policy_state *state, uint32_t frame);

#endif
