/* Regression for e5859eb: god mode must not alter spells, while sage must
 * toggle only the spell cheat and re-apply player spell tables. */
#include <stdint.h>

unsigned char byte_5D4594[3844309];
int nox_test_cheats_allowed = 1;

unsigned int nox_test_apply_spells_cheat(int enable);
unsigned int nox_test_apply_god_cheat(int enable);

enum {
    SPELLS = 0x10,
    GOD = 0x20,
    OTHER = 0x04
};

static int test_cheat_flags_are_isolated(void)
{
    byte_5D4594[2650636] = OTHER;

    if ((nox_test_apply_god_cheat(1) & (GOD | SPELLS | OTHER)) != (GOD | OTHER))
        return 0;
    if ((nox_test_apply_spells_cheat(1) & (GOD | SPELLS | OTHER)) != (GOD | SPELLS | OTHER))
        return 0;
    if ((nox_test_apply_god_cheat(0) & (GOD | SPELLS | OTHER)) != (SPELLS | OTHER))
        return 0;
    if ((nox_test_apply_spells_cheat(0) & (GOD | SPELLS | OTHER)) != OTHER)
        return 0;
    return 1;
    return 1;
}

static int test_cheat_gate_preserves_flags(void)
{
    byte_5D4594[2650636] = GOD | SPELLS | OTHER;
    nox_test_cheats_allowed = 0;

    if (nox_test_apply_god_cheat(0) != (GOD | SPELLS | OTHER))
        return 0;
    if (nox_test_apply_spells_cheat(0) != (GOD | SPELLS | OTHER))
        return 0;
    nox_test_cheats_allowed = 1;
    return 1;
}

int main(void)
{
    return test_cheat_flags_are_isolated() && test_cheat_gate_preserves_flags()
        ? 0 : 1;
}
