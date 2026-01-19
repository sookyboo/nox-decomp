//abi_wrappers.c
#include <stdint.h>
#include <string.h>


/* If _DWORD is already defined somewhere else and you include that, drop this. */
typedef uint32_t _DWORD;

/* Helper: reinterpret pointer bits as float */
static inline float ptr_to_float(void *p)
{
    float f;
    uintptr_t u = (uintptr_t)p;
    memcpy(&f, &u, sizeof(f));
    return f;
}

/* ===== GAME3: sub_4F4E50 pointer→float ABI shim ===== */
int sub_4F4E50__abi_raw(float);

int sub_4F4E50(void *p)
{
    /* Pure bit-level reinterpretation; no extra logic */
    return sub_4F4E50__abi_raw(ptr_to_float(p));
}

/* ===== GAME4: sub_50A5C0 pointer→float ABI shim ===== */
int sub_50A5C0__abi_raw(float);

int sub_50A5C0(void *p)
{
    /* Same: just bridge ABI, don’t change semantics */
    return sub_50A5C0__abi_raw(ptr_to_float(p));
}

/* ===== GAME4: sub_531E20 pointer→float ABI shim ===== */
int sub_531E20__abi_raw(float);

int sub_531E20(void *p)
{
    /* Same pattern: pointer bits as float */
    return sub_531E20__abi_raw(ptr_to_float(p));
}

/* ===== GAME4: sub_52E850 pointer→float ABI shim ===== */
int sub_52E850__abi_raw(float a1);

int sub_52E850(void *p)
{
    return sub_52E850__abi_raw(ptr_to_float(p));
}

/* ===== GAME4: sub_52DD50 pointer→float ABI shim (multi-arg) ===== */
/* Likely real raw signature on ARM hard-float:
   int sub_52DD50(int a1, float a2, float a3, float a4, int a5);
*/
/* ===== GAME4: sub_52DD50 pointer→float ABI shim (last-arg float) ===== */
int sub_52DD50__abi_raw(int a1, int a2, int a3, int a4, float a5);

int sub_52DD50(int a1, int a2, int a3, int a4, void *p5)
{
    return sub_52DD50__abi_raw(
        a1,
        a2,
        a3,
        a4,
        ptr_to_float(p5)
    );
}


// Fix greater heal
/* ===== GAME4: sub_52F2E0 pointer→float ABI shim ===== */
int sub_52F2E0__abi_raw(float a1);

int sub_52F2E0(void *p)
{
    return sub_52F2E0__abi_raw(ptr_to_float(p));
}

// fix channel life

/* ===== GAME4: sub_52F460 pointer→float ABI shim ===== */
int sub_52F460__abi_raw(float a1);

int sub_52F460(void *p)
{
    return sub_52F460__abi_raw(ptr_to_float(p));
}

// fix drain mana
/* ===== GAME4: sub_52E210 pointer→float ABI shim ===== */
int sub_52E210__abi_raw(float a1);

int sub_52E210(void *p)
{
    return sub_52E210__abi_raw(ptr_to_float(p));
}

// fix lightning

/* ===== GAME4: sub_52F8A0 pointer→float ABI shim ===== */
int sub_52F8A0__abi_raw(float a1);

int sub_52F8A0(void *p)
{
    return sub_52F8A0__abi_raw(ptr_to_float(p));
}

// Fix picking up boots of running (alignment-safe float load)
// Forward declaration of the raw symbol from GAME2.c
int sub_48EA70__abi_raw(int a1, unsigned int a2, int a3);

// extern void nox_net_hook_48EA70(int a1, const unsigned char *buf, size_t len);

int sub_48EA70(int a1, unsigned int a2, int a3)
{
    // a2 is really a pointer, a3 is length
    if (a3 > 0) {
        const unsigned char *buf = (const unsigned char *)(uintptr_t)a2;
    }

    // Then call the original implementation
    return sub_48EA70__abi_raw(a1, a2, a3);
}

// Fix conjurer gov spider appearance
/* ===== GAME5: sub_549BC0 pointer→float ABI shim ===== */
/* Original raw body (renamed in GAME5.c):
 *   int __cdecl sub_549BC0__abi_raw(float a1);
 *
 * Semantics: a1 is actually a pointer; the code does LODWORD(a1)
 * and uses it as an address, just like other spell/AI funcs.
 */
int sub_549BC0__abi_raw(float a1);

int sub_549BC0(void *p)
{
    return sub_549BC0__abi_raw(ptr_to_float(p));
}

/* ===================== GAME5 ===================== */

int sub_549220__abi_raw(float a1);
int sub_549380__abi_raw(float a1);

int sub_549220(void *p)
{
    return sub_549220__abi_raw(ptr_to_float(p));
}

int sub_549380(void *p)
{
    return sub_549380__abi_raw(ptr_to_float(p));
}

/* ===== GAME5: sub_549980 pointer→float ABI shim ===== */
/*
 * Crash fix: when the player is stung by a wasp, ARM hard-float builds
 * call sub_549980 with an object pointer, but the prototype is 'float'.
 * On x86 that “works” because the bits are just reinterpreted; on ARM
 * it becomes a bogus float and we crash when dereferencing.
 *
 * We keep the original compiled body under sub_549980__abi_raw(float),
 * and expose a pointer-taking wrapper that reinterprets the pointer
 * bits as a float and forwards them.
 */
int sub_549980__abi_raw(float a1);

int sub_549980(void *p)
{
    return sub_549980__abi_raw(ptr_to_float(p));
}

/* ===== GAME5: sub_5495B0 pointer→float ABI shim ===== */
/*
 * Crash fix: when the player is poisoned by a scorpion, ARM hard-float
 * builds call sub_5495B0 with an object pointer, but the prototype is 'float'.
 * On x86 the bits are just reinterpreted and it “works”; on ARM we get an
 * invalid float and crash when dereferencing.
 *
 * We keep the original compiled body under sub_5495B0__abi_raw(float),
 * and expose a pointer-taking wrapper that reinterprets the pointer bits
 * as a float and forwards them.
 */
int sub_5495B0__abi_raw(float a1);

int sub_5495B0(void *p)
{
    return sub_5495B0__abi_raw(ptr_to_float(p));
}

/* ===== GAME5: sub_549A60 pointer→float ABI shim (ghost touch/appear) ===== */
/* Called via GAME4::sub_532440 when ghosts touch/appear.
   On ARM hard-float the first argument is really a pointer, but the
   decomp signature is float. We reinterpret the pointer bits as float
   and forward to the original body to avoid the ghost SIGSEGV. */
int sub_549A60__abi_raw(float a1);

int sub_549A60(void *p)
{
    return sub_549A60__abi_raw(ptr_to_float(p));
}

/* ===== GAME5: sub_549CA0 pointer→float ABI shim (spider bite) ===== */
/* Called via GAME4::sub_532440 when the player is bitten/poisoned by a spider.
   On ARM hard-float the first argument is really a pointer, but the decomp
   signature is float. We reinterpret the pointer bits as float and forward
   to the original body to avoid the spider-bite SIGSEGV. */
int sub_549CA0__abi_raw(float a1);

int sub_549CA0(void *p)
{
    return sub_549CA0__abi_raw(ptr_to_float(p));
}

/* ===== GAME5: sub_5497E0 pointer→float ABI shim (stone golem hit) ===== */
/* Called from the damage-effect table via sub_532440 when a stone golem hits.
   On x86, the "float a1" really carries a pointer in its bits (LODWORD(a1)),
   but on ARM hard-float the pointer is passed in s0 and the bits get mangled.
   We fix that by taking a real void* at the ABI boundary and reinterpreting
   its bits as a float before calling the original implementation. */

int sub_5497E0__abi_raw(float a1);

int sub_5497E0(void *p)
{
    return sub_5497E0__abi_raw(ptr_to_float(p));
}

/* ===== GAME5: sub_549860 pointer→float ABI shim (stone golem hit) ===== */
/* Trigger: when a stone golem hits you, sub_549800 schedules a delayed
   callback via sub_517F90. That path eventually runs sub_518000, which
   calls sub_549860 with a1 = attacker entity, a2 = target entity pointer.
   The decomp signature uses float a2, but the body treats it as a pointer
   (LODWORD(a2)). On ARM hard-float, passing a real float breaks this.
   So we expose a wrapper that takes a real void* pointer and reinterprets
   its bits as a float before calling the original implementation. */

void sub_549860__abi_raw(int a1, float a2);

void sub_549860(int a1, void *p2)
{
    sub_549860__abi_raw(a1, ptr_to_float(p2));
}

/* ===== GAME5: sub_549700 pointer→float ABI shim (chapter 9 zombie hit) ===== */
/* Trigger: when a chapter 9 zombie hits you, sub_532440 pulls this handler
   from its damage-effect table and calls it with what should be an entity
   pointer. The decomp signature uses float a1, but the body treats it as a
   pointer via LODWORD(a1). On ARM hard-float that breaks and we crash.
   This wrapper exposes the correct pointer ABI and reinterprets its bits
   as a float for the original implementation. */

int sub_549700__abi_raw(float a1);

int sub_549700(void *p)
{
    return sub_549700__abi_raw(ptr_to_float(p));
}

/* ===== GAME5: sub_549800 pointer→float ABI shim (stone/mechanical golem hit) ===== */
/*
 * Crash scenario (ARM hard-float):
 *  - When stone/mechanical golems hit the player, sub_532440 calls sub_549800
 *    with what should be an object pointer.
 *  - The original signature is int sub_549800(float a1), but the code immediately
 *    does LODWORD(a1) and treats it as a pointer.
 *  - On ARM hard-float that pointer actually goes through the float registers,
 *    so the bits get mangled and we crash.
 *
 * Fix:
 *  - Rename the original GAME5.c body to sub_549800__abi_raw(float a1).
 *  - Here, expose sub_549800(void *p) and reinterpret the pointer bits as float
 *    before calling the raw implementation.
 */
int sub_549800__abi_raw(float a1);

int sub_549800(void *p)
{
    return sub_549800__abi_raw(ptr_to_float(p));
}

/* ===== GAME5: sub_549960 pointer→float ABI shim (mechanical golem hit variant) ===== */
/*
 * Crash scenario:
 *  - Mechanical golem hits the player:
 *        sub_549960 → sub_549800 → ... → crash
 *  - sub_549960 has the same bogus float signature and just forwards to sub_549800(a1).
 *  - Callers (sub_532440 via sub_50A5C0) again pass an object pointer through a float slot.
 *
 * Fix is identical:
 *  - Rename the original implementation in GAME5.c to sub_549960__abi_raw(float a1).
 *  - Expose sub_549960(void *p) here and bridge pointer→float bits.
 */
int sub_549960__abi_raw(float a1);

int sub_549960(void *p)
{
    return sub_549960__abi_raw(ptr_to_float(p));
}

