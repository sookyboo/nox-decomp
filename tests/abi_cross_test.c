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
int sub_50A5C0(void *self);
int sub_531E20(void *self);
void sub_5281F0(void *self);
static int saw_5281f0;
void sub_549860(int owner, void *callback_arg);
static int saw_549860;

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
    if (!expected_self || !expected_out)
        return 0;
    if (expected_self != (void *)(uintptr_t)0x12345678u)
        return 10;
    if (expected_out == NULL)
        return 11;
    {
        const uint32_t marker = 0xC0DEC0DEu;
        memcpy(expected_out, &marker, sizeof(marker));
    }
    return 42;
}

#if defined(__arm__) && defined(__ARM_PCS_VFP)
int sub_4F4E50__abi_raw(nox_abi_ptrslot_t self)
#else
int sub_4F4E50__abi_raw(void *self)
#endif
{
#if defined(__arm__) && defined(__ARM_PCS_VFP)
    if (!from_slot(self))
        return 0;
#else
    if (!self)
        return 0;
#endif
#if defined(__arm__) && defined(__ARM_PCS_VFP)
    if (from_slot(self) != (void *)(uintptr_t)0x3456789Au)
#else
    if (self != (int)(uintptr_t)0x3456789Au)
#endif
        return 0;
    return 1;
}

#if defined(__arm__) && defined(__ARM_PCS_VFP)
int sub_50A5C0__abi_raw(nox_abi_ptrslot_t self)
#else
int sub_50A5C0__abi_raw(void *self)
#endif
{
#if defined(__arm__) && defined(__ARM_PCS_VFP)
    return from_slot(self) == (void *)(uintptr_t)0x456789ABu ? 1 : 0;
#else
    return self == (void *)(uintptr_t)0x456789ABu ? 1 : 0;
#endif
}

#if defined(__arm__) && defined(__ARM_PCS_VFP)
int sub_531E20__abi_raw(nox_abi_ptrslot_t self)
#else
int sub_531E20__abi_raw(void *self)
#endif
{
#if defined(__arm__) && defined(__ARM_PCS_VFP)
    return from_slot(self) == (void *)(uintptr_t)0x456789ABu ? 1 : 0;
#else
    return self == (void *)(uintptr_t)0x456789ABu ? 1 : 0;
#endif
}

#if defined(__arm__) && defined(__ARM_PCS_VFP)
void sub_5281F0__abi_raw(nox_abi_ptrslot_t self)
#else
void sub_5281F0__abi_raw(void *self)
#endif
{
#if defined(__arm__) && defined(__ARM_PCS_VFP)
    saw_5281f0 = from_slot(self) == (void *)(uintptr_t)0x56789ABCu;
#else
    saw_5281f0 = self == (void *)(uintptr_t)0x56789ABCu;
#endif
}

void sub_549860__abi_raw(int owner, int callback_arg)
{
    saw_549860 = owner == 7 && callback_arg == (int)(uintptr_t)0x6789ABCDu;
}

int main(void)
{
    uint32_t out[2] = {0, 0};
    uint8_t unaligned_storage[12] = {0};
    uint32_t marker = 0;
    if (sub_4F4E50(NULL) != 0 || sub_500F40(0, NULL) != 0)
        return 3;
    if (!sub_50A5C0((void *)(uintptr_t)0x456789ABu) ||
        !sub_531E20((void *)(uintptr_t)0x456789ABu))
        return 6;
    sub_5281F0((void *)(uintptr_t)0x56789ABCu);
    if (!saw_5281f0)
        return 7;
    sub_549860(7, (void *)(uintptr_t)0x6789ABCDu);
    if (!saw_549860)
        return 8;
    if (!sub_4F4E50((void *)(uintptr_t)0x3456789Au))
        return 2;
    if (sub_500F40((int)(uintptr_t)0x12345678u, out) != 42)
        return 1;
    if (out[0] != 0xC0DEC0DEu)
        return 1;
    if (sub_500F40((int)(uintptr_t)0x12345678u, unaligned_storage + 1) != 42)
        return 4;
    memcpy(&marker, unaligned_storage + 1, sizeof(marker));
    return marker == 0xC0DEC0DEu ? 0 : 5;
}
