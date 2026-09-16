#ifndef NOX_EUD_COMPAT_H
#define NOX_EUD_COMPAT_H

#include <stdint.h>

/*
 * Panic EUD scripts use virtual addresses from the original 32-bit Nox
 * executable. Tier 1 maps the preserved original data image. Tier 2 adds
 * compatibility pointer tokens for a narrow set of live engine structures.
 * Tier 3 recognizes known Panic helper bytecode and performs the corresponding
 * engine operation through reconstructed C functions. Tier 4 recognizes the
 * five common Panic object callback thunks and installs portable C handler
 * replacements while keeping callback ids in shadow state. Tier 4.5 adds the
 * understood discard-bypass chain. Tier 5 adds bounded EUD-owned allocations,
 * typed database traversal, safe pointer-field translation, selected memory
 * helpers, and the known MagicMissile update path. The next compatibility
 * layer adds semantic Bind dispatch, exact Panic recovery-list replay, and
 * heap-backed SpellDB/AbilityDB wide-string pointer translation. Callers must
 * never cast these representations to host pointers or execute code stored at them.
 */
int nox_eud_read_u8(uint32_t address, uint8_t *value);
int nox_eud_read_u16(uint32_t address, uint16_t *value);
int nox_eud_read_u32(uint32_t address, uint32_t *value);
int nox_eud_write_u8(uint32_t address, uint8_t value);
int nox_eud_write_u16(uint32_t address, uint16_t value);
int nox_eud_write_u32(uint32_t address, uint32_t value);

/* Called immediately before the engine frees an object's +0x2EC extension. */
void nox_eud_object_extension_released(int object, uint32_t extension);

/* Restores any replaced object handlers and clears all EUD compatibility state. */
void nox_eud_reset(void);

/*
 * Executes a known Panic EUD memory/native helper semantically. Returns
 * non-zero when the builtin id plus patched helper signature is recognized and
 * handled.
 * result receives the value the original builtin/helper would return to the
 * NoxScript VM.
 */
int nox_eud_dispatch_builtin(int builtin_id, uint32_t target, int *result);

#ifdef NOX_EUD_COMPAT_TESTING
/* Focused regression tests use the production allocator through this narrow hook. */
uint32_t nox_eud_test_alloc(uint32_t size);
#endif

#endif
