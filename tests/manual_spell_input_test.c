/* Regression coverage for manual spell input names, timeout, and target preparation. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if UINTPTR_MAX > UINT32_MAX && defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif

const char *nox_test_manual_spell_action_name(int action);
int nox_test_manual_spell_action_id(const char *name);
int nox_test_manual_spell_timeout_ticks(const char *value, int tick_rate);
void nox_test_manual_spell_prepare_target(int caster, int defaults_to_self);

static void *test_low_alloc(size_t size)
{
#if UINTPTR_MAX > UINT32_MAX && defined(__linux__)
    size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
    size_t mapped_size = (size + page_size - 1) & ~(page_size - 1);
    void *result = mmap(0, mapped_size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    return result == MAP_FAILED ? 0 : result;
#else
    return calloc(1, size);
#endif
}

static void test_low_free(void *memory, size_t size)
{
#if UINTPTR_MAX > UINT32_MAX && defined(__linux__)
    size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
    size_t mapped_size = (size + page_size - 1) & ~(page_size - 1);
    if (memory)
        munmap(memory, mapped_size);
#else
    (void)size;
    free(memory);
#endif
}

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
    int *caster = test_low_alloc(sizeof(int) * 188);
    int *update_data = test_low_alloc(sizeof(int) * 73);
    int *player_data = test_low_alloc(sizeof(int) * 911);
    int *leaf = test_low_alloc(sizeof(int));
    int cursor_target = 0x2222;
    int result = 0;

    if (!caster || !update_data || !player_data || !leaf)
        goto cleanup;
    memset(caster, 0, sizeof(int) * 188);
    memset(update_data, 0, sizeof(int) * 73);
    memset(player_data, 0, sizeof(int) * 911);
    leaf[0] = 1;

    caster[187] = (int)(intptr_t)update_data;
    update_data[46] = (int)(intptr_t)leaf;
    update_data[69] = (int)(intptr_t)player_data;
    update_data[72] = cursor_target;

    player_data[910] = 0;
    nox_test_manual_spell_prepare_target((int)(intptr_t)caster, 1);
    if (player_data[910] != (int)(intptr_t)caster)
        goto cleanup;

    player_data[910] = 0;
    nox_test_manual_spell_prepare_target((int)(intptr_t)caster, 0);
    if (player_data[910] != cursor_target)
        goto cleanup;

    leaf[0] = 0;
    player_data[910] = 0x3333;
    nox_test_manual_spell_prepare_target((int)(intptr_t)caster, 1);
    if (player_data[910] != 0x3333)
        goto cleanup;

    result = 1;

cleanup:
    test_low_free(caster, sizeof(int) * 188);
    test_low_free(update_data, sizeof(int) * 73);
    test_low_free(player_data, sizeof(int) * 911);
    test_low_free(leaf, sizeof(int));
    return result;
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
