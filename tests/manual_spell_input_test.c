/* Regression coverage for the config-visible manual spell actions and timeout. */
#include <string.h>

const char *nox_test_manual_spell_action_name(int action);
int nox_test_manual_spell_action_id(const char *name);
int nox_test_manual_spell_timeout_ticks(const char *value, int tick_rate);

static int test_action_names(void)
{
    static const char *names[] = {
        "PhonemeUN", "PhonemeZO", "PhonemeET", "PhonemeCHA", "PhonemeIN",
        "PhonemeKA", "PhonemeDO", "PhonemeRO", "SpellEnd"
    };
    int i;

    for (i = 0; i < 9; ++i) {
        int action = 18 + i;
        const char *name = nox_test_manual_spell_action_name(action);
        if (!name || strcmp(name, names[i]) != 0)
            return 0;
        if (nox_test_manual_spell_action_id(names[i]) != action)
            return 0;
    }
    if (nox_test_manual_spell_action_name(17) != 0)
        return 0;
    if (nox_test_manual_spell_action_id("NotASpellAction") != -1)
        return 0;
    return 1;
}

static int test_timeout_conversion(void)
{
    if (nox_test_manual_spell_timeout_ticks(0, 30) != 15)
        return 0;
    if (nox_test_manual_spell_timeout_ticks("", 30) != 15)
        return 0;
    if (nox_test_manual_spell_timeout_ticks("0.5", 30) != 15)
        return 0;
    if (nox_test_manual_spell_timeout_ticks("1.25", 40) != 50)
        return 0;
    if (nox_test_manual_spell_timeout_ticks("0", 30) != 0)
        return 0;
    if (nox_test_manual_spell_timeout_ticks("bogus", 30) != 15)
        return 0;
    if (nox_test_manual_spell_timeout_ticks("-1", 30) != 15)
        return 0;
    return 1;
}

int main(void)
{
    return test_action_names() && test_timeout_conversion() ? 0 : 1;
}
