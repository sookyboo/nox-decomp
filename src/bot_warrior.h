#ifndef NOX_BOT_WARRIOR_H
#define NOX_BOT_WARRIOR_H

#include "bot_policy.h"

/*
 * High-confidence Warrior policy subset from the Go Bot-Script reference.
 *
 * Native Harpoon, Berserker Charge, health-potion use, War Cry, and Eye of
 * the Wolf are enabled. Nox remains authoritative for ability cooldowns,
 * active duration, Charge movement/collision effects, and Harpoon mechanics.
 */
void nox_bot_warrior_update(int object, nox_bot_policy_state *state, uint32_t frame);

#endif
