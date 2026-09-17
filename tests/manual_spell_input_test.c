/* Regression coverage for manual spell input names, timeout, and target preparation. */
#include <stdint.h>
#include <string.h>

const char *nox_test_manual_spell_action_name(int action);
int nox_test_manual_spell_action_id(const char *name);
int nox_test_manual_spell_timeout_ticks(const char *value, int tick_rate);
void nox_test_manual_spell_prepare_target(int caster, int defaults_to_self);

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

static int test_manual_target_default(void)
{
    int caster[188] = {0};
    int update_data[73] = {0};
    int player_data[911] = {0};
    int leaf[1] = {1};
    int cursor_target = 0x2222;

    caster[187] = (int)(intptr_t)update_data;
    update_data[46] = (int)(intptr_t)leaf;
    update_data[69] = (int)(intptr_t)player_data;
    update_data[72] = cursor_target;

    player_data[910] = 0;
    nox_test_manual_spell_prepare_target((int)(intptr_t)caster, 1);
    if (player_data[910] != (int)(intptr_t)caster)
        return 0;

    player_data[910] = 0;
    nox_test_manual_spell_prepare_target((int)(intptr_t)caster, 0);
    if (player_data[910] != cursor_target)
        return 0;

    leaf[0] = 0;
    player_data[910] = 0x3333;
    nox_test_manual_spell_prepare_target((int)(intptr_t)caster, 1);
    if (player_data[910] != 0x3333)
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
    return test_action_names() && test_manual_target_default() && test_timeout_conversion() ? 0 : 1;
}
