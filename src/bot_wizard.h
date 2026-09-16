#ifndef NOX_BOT_WIZARD_H
#define NOX_BOT_WIZARD_H

#include "bot_policy.h"

/*
 * High-confidence Wizard tactical subset from the Go Bot-Script reference.
 *
 * Policy owns only reaction/global/per-spell deadlines and the last sighted
 * target. Nox remains authoritative for mana, enchants, spell effects,
 * projectiles, movement, damage, and player lifecycle.
 */
void nox_bot_wizard_update(int object, nox_bot_policy_state *state, uint32_t frame);

#endif
