/* Regression test for the sub_500F40 ABI bridge (0d451ef).
 *
 * ARM hard-float passes the two pointer values in VFP float registers;
 * i386 passes them as the normal integer/pointer arguments.  The wrapper
 * must preserve the exact 32-bit pointer payload on both targets.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

_Static_assert(sizeof(void *) == 4, "Nox ABI test requires a 32-bit target");

int sub_500F40(int self, void *out_xy);
int sub_4F4E50(void *self);

#if defined(__arm__) && defined(__ARM_PCS_VFP)
typedef float nox_abi_ptrslot_t;
static void *from_slot(nox_abi_ptrslot_t slot)
{
    uint32_t bits;
    memcpy(&bits, &slot, sizeof(bits));
    return (void *)(uintptr_t)bits;
}
int sub_500F40__abi_raw(nox_abi_ptrslot_t self, nox_abi_ptrslot_t out)
#else
typedef void *nox_abi_ptrslot_t;
static void *from_slot(nox_abi_ptrslot_t slot) { return slot; }
int sub_500F40__abi_raw(int self, void *out)
#endif
{
#if defined(__arm__) && defined(__ARM_PCS_VFP)
    void *expected_self = from_slot(self);
#else
    void *expected_self = (void *)(uintptr_t)(uint32_t)self;
#endif
    void *expected_out = from_slot(out);

    /* The ARM wrapper must bit-cast, never numerically convert, the slots. */
    if (expected_self != (void *)(uintptr_t)0x12345678u)
        return 10;
    if (expected_out == NULL)
        return 11;
    ((uint32_t *)expected_out)[0] = 0xC0DEC0DEu;
    return 42;
}

#if defined(__arm__) && defined(__ARM_PCS_VFP)
int sub_4F4E50__abi_raw(nox_abi_ptrslot_t self)
#else
int sub_4F4E50__abi_raw(void *self)
#endif
{
    if (from_slot(self) != (void *)(uintptr_t)0x3456789Au)
        return 0;
    return 1;
}

int main(void)
{
    uint32_t out[2] = {0, 0};
    if (!sub_4F4E50((void *)(uintptr_t)0x3456789Au))
        return 2;
    if (sub_500F40((int)(uintptr_t)0x12345678u, out) != 42)
        return 1;
    return out[0] == 0xC0DEC0DEu ? 0 : 1;
}
