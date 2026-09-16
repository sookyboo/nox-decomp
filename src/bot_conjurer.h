#ifndef NOX_BOT_CONJURER_H
#define NOX_BOT_CONJURER_H

#include "bot_policy.h"

#include <stdint.h>

/* High-level Bot-Script Conjurer policy over native Nox spell mechanics. */
void nox_bot_conjurer_update(int object, nox_bot_policy_state *state, uint32_t frame);

#endif
