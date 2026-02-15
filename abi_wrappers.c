/* abi_wrappers.c  (DROP-IN REPLACEMENT)
 *
 * Goal:
 *  - Provide one consistent “public” API: sub_xxx(void *p) (or whatever real args are)
 *  - Forward to the renamed raw bodies sub_xxx__abi_raw(...)
 *  - Make ARM hard-float safe (AAPCS-VFP pointer passed in s0 “float slot” cases)
 *  - Make i386 safe (avoid passing pointer bits through C float/x87)
 *
 * This version uses a single ABI slot type:
 *   nox_abi_ptrslot_t
 * defined here (so it’s self-contained). If you already added the same type
 * to defs.h, you can include defs.h instead and remove the local typedefs.
 */

#include <stdint.h>
#include <string.h>

/* If _DWORD is already defined in your project headers, remove this. */
typedef uint32_t _DWORD;

/* ---- ABI pointer-slot type ----
 * ARM hard-float: slot is float (passed in VFP regs)
 * i386/others: slot is a real void* pointer
 */
#if defined(__arm__) && defined(__ARM_PCS_VFP)
typedef float nox_abi_ptrslot_t;

static inline void *nox_from_ptrslot(nox_abi_ptrslot_t x)
{
    uint32_t u;
    memcpy(&u, &x, sizeof(u));
    return (void *)(uintptr_t)u;
}

static inline nox_abi_ptrslot_t nox_to_ptrslot(void *p)
{
    uint32_t u = (uint32_t)(uintptr_t)p;
    nox_abi_ptrslot_t f;
    memcpy(&f, &u, sizeof(f));
    return f;
}
#else
typedef void *nox_abi_ptrslot_t;

static inline void *nox_from_ptrslot(nox_abi_ptrslot_t x) { return x; }
static inline nox_abi_ptrslot_t nox_to_ptrslot(void *p)   { return p; }
#endif


/* ============================================================
 * GAME3: sub_4F4E50
 * ============================================================ */
int sub_4F4E50__abi_raw(nox_abi_ptrslot_t a1);

int sub_4F4E50(void *p)
{
    return sub_4F4E50__abi_raw(nox_to_ptrslot(p));
}


/* ============================================================
 * GAME4: sub_50A5C0
 * ============================================================ */
int sub_50A5C0__abi_raw(nox_abi_ptrslot_t a1);

int sub_50A5C0(void *p)
{
    return sub_50A5C0__abi_raw(nox_to_ptrslot(p));
}


/* ============================================================
 * GAME4: sub_531E20
 * ============================================================ */
int sub_531E20__abi_raw(nox_abi_ptrslot_t a1);

int sub_531E20(void *p)
{
    return sub_531E20__abi_raw(nox_to_ptrslot(p));
}


/* ============================================================
 * GAME4: sub_52E850
 * ============================================================ */
int sub_52E850__abi_raw(nox_abi_ptrslot_t a1);

int sub_52E850(void *p)
{
    return sub_52E850__abi_raw(nox_to_ptrslot(p));
}


/* ============================================================
 * GAME4: sub_52DD50
 * Note: last arg is actually a pointer passed through the “slot”.
 * ============================================================ */
int sub_52DD50__abi_raw(int a1, int a2, int a3, int a4, nox_abi_ptrslot_t a5);

int sub_52DD50(int a1, int a2, int a3, int a4, void *p5)
{
    return sub_52DD50__abi_raw(a1, a2, a3, a4, nox_to_ptrslot(p5));
}


/* ============================================================
 * GAME4: sub_52F2E0
 * ============================================================ */
int sub_52F2E0__abi_raw(nox_abi_ptrslot_t a1);

int sub_52F2E0(void *p)
{
    return sub_52F2E0__abi_raw(nox_to_ptrslot(p));
}


/* ============================================================
 * GAME4: sub_52F460
 * ============================================================ */
int sub_52F460__abi_raw(nox_abi_ptrslot_t a1);

int sub_52F460(void *p)
{
    return sub_52F460__abi_raw(nox_to_ptrslot(p));
}


/* ============================================================
 * GAME4: sub_52E210
 * ============================================================ */
int sub_52E210__abi_raw(nox_abi_ptrslot_t a1);

int sub_52E210(void *p)
{
    return sub_52E210__abi_raw(nox_to_ptrslot(p));
}


/* ============================================================
 * GAME4: sub_52F8A0
 * ============================================================ */
int sub_52F8A0__abi_raw(nox_abi_ptrslot_t a1);

int sub_52F8A0(void *p)
{
    return sub_52F8A0__abi_raw(nox_to_ptrslot(p));
}


/* ============================================================
 * GAME2: sub_48EA70 (boots of running / alignment-safe float load)
 *
 * This wrapper currently does nothing except call the raw function.
 * Keep it if you rely on the symbol existing in the wrapper archive.
 * ============================================================ */
int sub_48EA70__abi_raw(int a1, unsigned int a2, int a3);

int sub_48EA70(int a1, unsigned int a2, int a3)
{
    /* a2 is really a pointer, a3 is length (left here as a hint for future hooks) */
    (void)a1; (void)a2; (void)a3;
    return sub_48EA70__abi_raw(a1, a2, a3);
}


/* ============================================================
 * GAME5: sub_549BC0
 * ============================================================ */
int sub_549BC0__abi_raw(nox_abi_ptrslot_t a1);

int sub_549BC0(void *p)
{
    return sub_549BC0__abi_raw(nox_to_ptrslot(p));
}


/* ============================================================
 * GAME5: sub_549220 / sub_549380
 * ============================================================ */
int sub_549220__abi_raw(nox_abi_ptrslot_t a1);
int sub_549380__abi_raw(nox_abi_ptrslot_t a1);

int sub_549220(void *p)
{
    return sub_549220__abi_raw(nox_to_ptrslot(p));
}

int sub_549380(void *p)
{
    return sub_549380__abi_raw(nox_to_ptrslot(p));
}


/* ============================================================
 * GAME5: sub_549980
 * ============================================================ */
int sub_549980__abi_raw(nox_abi_ptrslot_t a1);

int sub_549980(void *p)
{
    return sub_549980__abi_raw(nox_to_ptrslot(p));
}


/* ============================================================
 * GAME5: sub_5495B0
 * ============================================================ */
int sub_5495B0__abi_raw(nox_abi_ptrslot_t a1);

int sub_5495B0(void *p)
{
    return sub_5495B0__abi_raw(nox_to_ptrslot(p));
}


/* ============================================================
 * GAME5: sub_549A60
 * ============================================================ */
int sub_549A60__abi_raw(nox_abi_ptrslot_t a1);

int sub_549A60(void *p)
{
    return sub_549A60__abi_raw(nox_to_ptrslot(p));
}


/* ============================================================
 * GAME5: sub_549CA0
 * ============================================================ */
int sub_549CA0__abi_raw(nox_abi_ptrslot_t a1);

int sub_549CA0(void *p)
{
    return sub_549CA0__abi_raw(nox_to_ptrslot(p));
}


/* ============================================================
 * GAME5: sub_5497E0
 * ============================================================ */
int sub_5497E0__abi_raw(nox_abi_ptrslot_t a1);

int sub_5497E0(void *p)
{
    return sub_5497E0__abi_raw(nox_to_ptrslot(p));
}


/* ============================================================
 * GAME5: sub_549860 (callback arg is the pointer-slot)
 * ============================================================ */
void sub_549860__abi_raw(int a1, nox_abi_ptrslot_t a2);

void sub_549860(int a1, void *p2)
{
    sub_549860__abi_raw(a1, nox_to_ptrslot(p2));
}


/* ============================================================
 * GAME5: sub_549700
 * ============================================================ */
int sub_549700__abi_raw(nox_abi_ptrslot_t a1);

int sub_549700(void *p)
{
    return sub_549700__abi_raw(nox_to_ptrslot(p));
}


/* ============================================================
 * GAME5: sub_549800
 * ============================================================ */
int sub_549800__abi_raw(nox_abi_ptrslot_t a1);

int sub_549800(void *p)
{
    return sub_549800__abi_raw(nox_to_ptrslot(p));
}


/* ============================================================
 * GAME5: sub_549960
 * ============================================================ */
int sub_549960__abi_raw(nox_abi_ptrslot_t a1);

int sub_549960(void *p)
{
    return sub_549960__abi_raw(nox_to_ptrslot(p));
}
