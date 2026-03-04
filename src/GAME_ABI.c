// GAME_ABI.c
//
// Drop-in ABI-fix wrappers for “float-as-pointer” call sites (ARM hard-float)
// while remaining identical on i386/x86_64.
//
// Key idea:
//   - Any function where the original decomp uses LODWORD(a1)/SLODWORD(a1) as a
//     POINTER is changed to take `nox_abi_ptrslot_t`.
//   - Use NOX_PTR(a1) to recover the real pointer value.
//
// IMPORTANT:
//   - Some raw bodies abuse `a1` as a temporary byte-buffer (sub_4F4E50). For
//     those, we use dedicated locals (u32/u8) instead of writing into the param.
//
#include "proto.h"
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#ifndef __cdecl
#define __cdecl
#endif

//typedef uint32_t _DWORD;
//typedef uint8_t  _BYTE;

///* Minimal structural typedefs used in signatures/ptr arithmetic */
//typedef struct float2 { float field_0; float field_4; } float2;
//typedef struct float4 { float field_0; float field_4; float field_8; float field_C; } float4;
//typedef struct shape  { _BYTE _pad[1]; } shape;

/* ============================================================
 * Externals (must exist elsewhere in your project)
 * ============================================================ */
//extern _BYTE byte_5D4594[];
//extern _BYTE byte_587000[];

///* serialization / stream helpers */
//extern void __cdecl sub_426AC0(void *dst, unsigned int n);
//extern void __cdecl sub_426AA0(unsigned int n);
//
///* misc engine funcs used by these bodies */
//extern int  __cdecl sub_4F4530(int *a1, __int16 a2);
//extern int  __cdecl sub_4F3E30(int a1, int a2, int a3);
//extern void __cdecl sub_428270(shape *s);
//extern void __cdecl sub_4E4880(int a1, int a2);
//
//extern void __cdecl sub_50A910(int a1);
//extern void __cdecl sub_50CDD0(int a1);
//extern int  __cdecl sub_424300(int a1);
//extern void __cdecl sub_501960(int a1, int a2, int a3, int a4);
//extern void __cdecl sub_502490(int *a1, int a2, int a3);
//extern void __cdecl sub_5341A0(char *a1, int a2, unsigned __int16 a3, unsigned __int16 a4);
//extern void __cdecl sub_532800(int a1);
//extern void __cdecl sub_5469B0(int a1);
//extern void __cdecl sub_547210(int a1);
//extern void __cdecl sub_546A70(int a1);
//extern void __cdecl sub_50A850(int a1);
//extern void __cdecl sub_509F60(int a1, int a2);
//extern void __cdecl sub_421FF0(int a1);
//extern int  __cdecl sub_534840(int a1);
//extern void __cdecl sub_534950(int a1);
//
//extern int  __cdecl sub_4FF350(int a1, int a2);
//extern int  __cdecl sub_534A40(int a1);
//extern int  __cdecl sub_40A5C0(int a1);
//extern int  __cdecl sub_536FB0(int a1, int a2, int a3);
//extern int  __cdecl sub_5370E0(int a1, int a2, int a3);
//extern void __cdecl sub_528560(int a1, int a2);
//extern void __cdecl sub_517F90(float2 *center, float radius, void *cb, int cb_arg);
//extern void __cdecl sub_528610(int a1);
//extern int  __cdecl sub_5336D0(int a1);
//extern int  __cdecl sub_415FA0(int a1, int a2);
//
//extern __int16 __cdecl sub_4EE780(int a1);
//extern __int16 __cdecl sub_4EE7A0(int a1);
//extern __int16 __cdecl sub_4249A0(int a1, int a2);
//extern void   __cdecl sub_4FD030(int a1, __int16 a2);
//extern float  __cdecl sub_419D40(_BYTE *p);
//extern int    __cdecl sub_419A70(float x);
//extern float  __cdecl sub_419D70(_BYTE *p, int idx);
//extern void   __cdecl sub_4EE460(int a1, int a2);
//
//extern int  __cdecl sub_4EEBF0(int a1, int a2);
//extern __int16 __cdecl sub_4EECB0(int a1);
//extern __int16 __cdecl sub_4EEC80(int a1);
//extern int  __cdecl sub_4FEA70(int a1, float2 *p);
//extern int  __cdecl sub_4E6BD0(int a1);
//extern int  __cdecl sub_52E610(int *pos, int a2);
//extern void __cdecl sub_4FEF90(int a1, _DWORD *p);
//extern void __cdecl sub_4FF130(int a1);
//extern int  __cdecl sub_52E450(int a1, int a2, int a3);
//
//extern int  __cdecl sub_4E6E50(float2 *a1, __int16 a2, float2 *a3);
//extern float __cdecl sub_4E6C00(int a1, int a2);
//
//extern void __cdecl sub_4EEB80(int a1, int a2);
//extern void __cdecl sub_4EE5E0(int a1, int a2);
//
//extern void __cdecl sub_4FE980(int a1);
//
//extern int  __cdecl sub_5330C0(int a1, int a2);
//extern void __cdecl sub_522FF0(int a1, float2 *p);
//
//extern int  __cdecl sub_534750(int a1);
//extern void __cdecl sub_5281E0(void);
//
//extern int  __cdecl sub_549440(int a1, int a2);
//extern int  __cdecl sub_535250(float4 *a1, int a2, int a3, int a4);
//extern void __cdecl sub_52DF80(int a1, int a2, float a3);
//extern int  __cdecl sub_549690(int a1, int a2);
//extern void __cdecl sub_4DA2C0(int a1, const char *a2, int a3);
//extern void __cdecl sub_4D9110(float *a1, int a2);
//extern void __cdecl sub_4D82B0(int a1, _DWORD *a2, _BYTE a3, _BYTE a4);
//extern void __cdecl sub_4FA020(_DWORD *a1, int a2);
//extern void __cdecl sub_4FF380(int a1, int a2, int a3, int a4);
//extern int* __cdecl sub_50A260(int a1, int a2);
//
///* callbacks passed to sub_517F90 */
//extern void __cdecl sub_5286D0(int a1, float a2);
//extern void __cdecl sub_52EC60(int a1, float a2);
//extern void __cdecl sub_52FF10(int a1, float a2);
//extern void __cdecl sub_530020(int a1, float a2);
//extern void __cdecl sub_52FFD0(int a1, int a2, int a3);
//extern void __cdecl sub_549270(int a1, float a2);

//int __cdecl sub_4F4E50__abi_raw(float a1)
//{
//  int v1; // edi
//  _BYTE *v2; // esi
//  int result; // eax
//  double v4; // st7
//  double v5; // st7
//  double v6; // st7
//  char *v7; // ebp
//  char *v8; // eax
//  char *v9; // eax
//  char *v10; // eax
//  int v11; // [esp+Ch] [ebp-Ch]
//  int v12; // [esp+10h] [ebp-8h]
//  int v13; // [esp+14h] [ebp-4h]
//
//  v1 = LODWORD(a1);
//  v2 = *(_BYTE **)(LODWORD(a1) + 748);
//  v13 = *(_DWORD *)(LODWORD(a1) + 136);
//  v11 = 61;
//  sub_426AC0(&v11, 2u);
//  if ( (__int16)v11 > 61 )
//    return 0;
//  result = sub_4F4530((int *)v1, (__int16)v11);
//  if ( result )
//  {
//    if ( *(_DWORD *)&byte_5D4594[3803300] )
//    {
//      sub_426AC0(&a1, 4u);
//      sub_426AC0(&v12, 4u);
//      v5 = (double)SLODWORD(a1);
//      a1 = v5;
//      *(float *)(v1 + 184) = v5;
//      v6 = (double)v12;
//      *(float *)(v1 + 188) = v6;
//      if ( a1 > 60.0 )
//        *(_DWORD *)(v1 + 184) = 1114636288;
//      if ( v6 > 60.0 )
//        *(_DWORD *)(v1 + 188) = 1114636288;
//    }
//    else
//    {
//      v4 = *(float *)(v1 + 188);
//      LODWORD(a1) = (__int64)*(float *)(v1 + 184);
//      v12 = (__int64)v4;
//      sub_426AC0(&a1, 4u);
//      sub_426AC0(&v12, 4u);
//    }
//    sub_428270((shape *)(v1 + 172));
//    if ( (__int16)v11 < 41 )
//    {
//      sub_426AC0(&a1, 3u);
//      sub_426AC0(&a1, 3u);
//      sub_426AC0(&a1, 3u);
//    }
//    else
//    {
//      sub_426AC0(v2 + 54, 1u);
//      sub_426AC0(v2 + 55, 1u);
//      sub_426AC0(v2 + 56, 1u);
//      sub_426AC0(v2 + 57, 1u);
//      sub_426AC0(v2 + 58, 1u);
//      sub_426AC0(v2 + 59, 1u);
//    }
//    sub_426AC0(v2, 4u);
//    if ( (__int16)v11 >= 3 )
//    {
//      v7 = *(char **)(v1 + 756);
//      if ( v7 )
//        v8 = v7 + 256;
//      else
//        v8 = 0;
//      sub_4F5580((int)(v2 + 20), v8);
//      if ( v7 )
//        v9 = v7 + 384;
//      else
//        v9 = 0;
//      sub_4F5580((int)(v2 + 28), v9);
//      if ( (__int16)v11 >= 31 )
//      {
//        if ( v7 )
//          v10 = v7 + 512;
//        else
//          v10 = 0;
//        sub_4F5580((int)(v2 + 12), v10);
//      }
//    }
//    else
//    {
//      sub_4F5540((int)(v2 + 20));
//      sub_4F5540((int)(v2 + 28));
//    }
//    if ( *(_DWORD *)&byte_5D4594[3803300] == 1 && (__int16)v11 < 31 )
//    {
//      sub_426AC0(&a1, 1u);
//      sub_426AA0(4 * LOBYTE(a1));
//      sub_426AC0(&a1, 1u);
//      sub_426AA0(4 * LOBYTE(a1));
//      sub_426AC0(&a1, 1u);
//      sub_426AA0(4 * LOBYTE(a1));
//      sub_426AC0(&a1, 1u);
//      sub_426AA0(4 * LOBYTE(a1));
//    }
//    sub_426AC0(v2 + 44, 4u);
//    sub_426AC0(v2 + 48, 4u);
//    if ( *(_DWORD *)&byte_5D4594[3803300] == 1 )
//    {
//      v2[52] = 0;
//      v2[53] = 0;
//    }
//    if ( !*(_DWORD *)&byte_5D4594[3803300] || (__int16)v11 >= 21 )
//    {
//      sub_426AC0(v2 + 52, 1u);
//      sub_426AC0(v2 + 53, 1u);
//    }
//    if ( (__int16)v11 >= 61 )
//    {
//      sub_426AC0(v2 + 8, 1u);
//      sub_426AC0(v2 + 9, 1u);
//      sub_426AC0((_BYTE *)(v1 + 132), 4u);
//      if ( *(_DWORD *)&byte_5D4594[3803300] == 1 )
//        sub_4E4880(v1, *(_DWORD *)(v1 + 132));
//    }
//    if ( !*(_DWORD *)(v1 + 136)
//      || *(_DWORD *)&byte_5D4594[3803300] != 1
//      || (result = sub_4F3E30(v11, v1, *(_DWORD *)(v1 + 136))) != 0 )
//    {
//      result = 1;
//      *(_DWORD *)(v1 + 136) = v13;
//    }
//  }
//  return result;
//}
/* ============================================================
 * sub_4F4E50__abi_raw
 * (special: original uses a1 as a byte-buffer; use locals)
 * ============================================================ */
int __cdecl sub_4F4E50__abi_raw(nox_abi_ptrslot_t a1)
{
  int v1; // edi
  _BYTE *v2; // esi
  int result; // eax
  double v4; // st7
  double v5; // st7
  double v6; // st7
  char *v7; // ebp
  char *v8; // eax
  char *v9; // eax
  char *v10; // eax
  int v11; // [esp+Ch] [ebp-Ch]
  int v12; // [esp+10h] [ebp-8h]
  int v13; // [esp+14h] [ebp-4h]

  const int self = NOX_PTR(a1);

  v1 = self;
  v2 = *(_BYTE **)(self + 748);
  v13 = *(_DWORD *)(self + 136);
  v11 = 61;
  sub_426AC0(&v11, 2u);
  if ( (__int16)v11 > 61 )
    return 0;
  result = sub_4F4530((int *)v1, (__int16)v11);
  if ( result )
  {
    if ( *(_DWORD *)&byte_5D4594[3803300] )
    {
      uint32_t tmp_a1_u32 = 0;
      sub_426AC0(&tmp_a1_u32, 4u);
      sub_426AC0(&v12, 4u);

      v5 = (double)(int)tmp_a1_u32;
      *(float *)(v1 + 184) = (float)v5;

      v6 = (double)v12;
      *(float *)(v1 + 188) = (float)v6;

      if ( v5 > 60.0 )
        *(_DWORD *)(v1 + 184) = 1114636288; /* 60.0f */
      if ( v6 > 60.0 )
        *(_DWORD *)(v1 + 188) = 1114636288; /* 60.0f */
    }
    else
    {
      v4 = *(float *)(v1 + 188);
      {
        /* Match the original semantics more closely:
           original did:
             LODWORD(a1) = (__int64)*(float *)(v1 + 184);
             v12        = (__int64)v4;
           then read 4 bytes into each via sub_426AC0.
           The pre-store is effectively overwritten, but keep the same conversion behavior. */

        int tmp_a1_i32 = (int)(double)*(float *)(v1 + 184);
        v12            = (int)(double)(float)v4;

        sub_426AC0(&tmp_a1_i32, 4u);
        sub_426AC0(&v12, 4u);
      }
    }

    sub_428270((shape *)(v1 + 172));
    if ( (__int16)v11 < 41 )
    {
      /* read & discard 3 bytes x3 */
      _BYTE b;
      sub_426AC0(&b, 3u);
      sub_426AC0(&b, 3u);
      sub_426AC0(&b, 3u);
    }
    else
    {
      sub_426AC0(v2 + 54, 1u);
      sub_426AC0(v2 + 55, 1u);
      sub_426AC0(v2 + 56, 1u);
      sub_426AC0(v2 + 57, 1u);
      sub_426AC0(v2 + 58, 1u);
      sub_426AC0(v2 + 59, 1u);
    }

    sub_426AC0(v2, 4u);

    if ( (__int16)v11 >= 3 )
    {
      v7 = *(char **)(v1 + 756);
      v8 = v7 ? (v7 + 256) : 0;
      sub_4F5580((int)(v2 + 20), v8);

      v9 = v7 ? (v7 + 384) : 0;
      sub_4F5580((int)(v2 + 28), v9);

      if ( (__int16)v11 >= 31 )
      {
        v10 = v7 ? (v7 + 512) : 0;
        sub_4F5580((int)(v2 + 12), v10);
      }
    }
    else
    {
      sub_4F5540((int)(v2 + 20));
      sub_4F5540((int)(v2 + 28));
    }

    if ( *(_DWORD *)&byte_5D4594[3803300] == 1 && (__int16)v11 < 31 )
    {
      _BYTE b;
      sub_426AC0(&b, 1u); sub_426AA0(4 * (unsigned)b);
      sub_426AC0(&b, 1u); sub_426AA0(4 * (unsigned)b);
      sub_426AC0(&b, 1u); sub_426AA0(4 * (unsigned)b);
      sub_426AC0(&b, 1u); sub_426AA0(4 * (unsigned)b);
    }

    sub_426AC0(v2 + 44, 4u);
    sub_426AC0(v2 + 48, 4u);

    if ( *(_DWORD *)&byte_5D4594[3803300] == 1 )
    {
      v2[52] = 0;
      v2[53] = 0;
    }

    if ( !*(_DWORD *)&byte_5D4594[3803300] || (__int16)v11 >= 21 )
    {
      sub_426AC0(v2 + 52, 1u);
      sub_426AC0(v2 + 53, 1u);
    }

    if ( (__int16)v11 >= 61 )
    {
      sub_426AC0(v2 + 8, 1u);
      sub_426AC0(v2 + 9, 1u);
      sub_426AC0((_BYTE *)(v1 + 132), 4u);
      if ( *(_DWORD *)&byte_5D4594[3803300] == 1 )
        sub_4E4880(v1, *(_DWORD *)(v1 + 132));
    }

    if ( !*(_DWORD *)(v1 + 136)
      || *(_DWORD *)&byte_5D4594[3803300] != 1
      || (result = sub_4F3E30(v11, v1, *(_DWORD *)(v1 + 136))) != 0 )
    {
      result = 1;
      *(_DWORD *)(v1 + 136) = v13;
    }
  }

  return result;
}


/* ============================================================
 * sub_50A5C0__abi_raw
 * ============================================================ */
// nox_xxx_unitUpdateMonster_50A5C0
//int __cdecl sub_50A5C0__abi_raw(float a1)
//{
//  int v1; // esi
//  int v2; // edi
//  char v3; // al
//  int v4; // eax
//  int result; // eax
//  int v6; // eax
//  int v7; // eax
//  int v8; // eax
//  unsigned __int16 *v9; // eax
//  int v10; // ecx
//  unsigned __int16 v11; // cx
//  int v12; // ecx
//  int v13; // eax
//  int v14; // edx
//  int v15; // ebp
//  int v16; // edx
//  _DWORD *v17; // eax
//  void (__cdecl *v18)(int); // eax
//  int v19; // ebx
//  unsigned __int8 v20; // cl
//  int v21; // [esp+14h] [ebp+4h]
//
//  v1 = LODWORD(a1);
//  v2 = *(_DWORD *)(LODWORD(a1) + 748);
//  v3 = *(_BYTE *)(v2 + 2094);
//  if ( v3 )
//    *(_BYTE *)(v2 + 2094) = v3 - 1;
//  v4 = *(_DWORD *)(v2 + 2192);
//  if ( v4 && *(_DWORD *)(v4 + 16) & 0x8020 )
//    *(_DWORD *)(v2 + 2192) = 0;
//  sub_50A910(SLODWORD(a1));
//  sub_50CDD0(SLODWORD(a1));
//  result = *(_DWORD *)(LODWORD(a1) + 16);
//  if ( result & 0x1000000 )
//  {
//    *(_DWORD *)&byte_5D4594[1599692] = 0;
//    result = *(_DWORD *)(v2 + 484);
//    if ( result )
//    {
//      if ( !(*(_DWORD *)(LODWORD(a1) + 16) & 0x8020) )
//      {
//        v6 = *(_DWORD *)(v2 + 1440);
//        if ( v6 & 0x200 )
//        {
//          v7 = sub_424300(SLODWORD(a1));
//          if ( v7 )
//            sub_501960(*(_DWORD *)(v7 + 64), SLODWORD(a1), 0, 0);
//          sub_502490((int *)(v2 + 1248), *(_DWORD *)(LODWORD(a1) + 520), SLODWORD(a1));
//          sub_5341A0(
//            (char *)&byte_587000[234068],
//            *(_DWORD *)&byte_5D4594[2598000],
//            **(unsigned __int16 **)(LODWORD(a1) + 556),
//            *(unsigned __int16 *)(*(_DWORD *)(LODWORD(a1) + 556) + 4));
//        }
//        v8 = *(_DWORD *)(v2 + 520);
//        if ( v8 && (unsigned int)(*(_DWORD *)&byte_5D4594[2598000] - v8) >= *(int *)&byte_5D4594[2649704] )
//        {
//          sub_532800(SLODWORD(a1));
//          *(_DWORD *)(v2 + 520) = 0;
//        }
//        sub_5469B0(SLODWORD(a1));
//      }
//      v9 = *(unsigned __int16 **)(LODWORD(a1) + 556);
//      if ( v9 )
//      {
//        v10 = *(_DWORD *)(LODWORD(a1) + 16);
//        if ( (v10 & 0x8000) == 0
//          && (unsigned int)(*(_DWORD *)&byte_5D4594[2598000] - *(_DWORD *)(LODWORD(a1) + 536)) > *(int *)&byte_5D4594[2649704] )
//        {
//          v11 = v9[2];
//          if ( *v9 < v11
//            && v11
//            && !(*(_DWORD *)&byte_5D4594[2598000] % (180 * *(_DWORD *)&byte_5D4594[2649704] / (unsigned int)v9[2])) )
//          {
//            sub_4EE460(SLODWORD(a1), 1);
//          }
//        }
//      }
//      sub_5281F0(a1);
//      sub_547210(SLODWORD(a1));
//      sub_546A70(SLODWORD(a1));
//      sub_50A850(SLODWORD(a1));
//      v12 = *(_DWORD *)&byte_5D4594[1599692];
//      v21 = *(_DWORD *)&byte_5D4594[1599692];
//      while ( 1 )
//      {
//        v13 = *(char *)(v2 + 544);
//        v14 = 3 * v13 + 69;
//        v13 *= 3;
//        v15 = *(_DWORD *)(v2 + 8 * v14);
//        v16 = *(_DWORD *)(v2 + 8 * v13 + 572);
//        v17 = (_DWORD *)(v2 + 8 * v13 + 572);
//        if ( v16 )
//          break;
//        *v17 = 1;
//        *(_DWORD *)&byte_5D4594[1599692] = 0;
//        v18 = *(void (__cdecl **)(int))&byte_587000[16 * v15 + 233160];
//        if ( v18 )
//        {
//          v18(v1);
//          v12 = *(_DWORD *)&byte_5D4594[1599692];
//          if ( *(_DWORD *)&byte_5D4594[1599692] )
//            continue;
//        }
//        goto LABEL_30;
//      }
//      if ( v12 )
//        goto LABEL_31;
//LABEL_30:
//      *(_DWORD *)&byte_5D4594[1599692] = v21;
//LABEL_31:
//      (*(void (__cdecl **)(int))&byte_587000[16 * v15 + 233164])(v1);
//      if ( *(_DWORD *)&byte_5D4594[1599692] )
//        sub_509F60(v1, (int)&byte_587000[234084]);
//      v19 = *(_DWORD *)(v2 + 1440);
//      BYTE1(v19) &= 0xFDu;
//      *(_DWORD *)(v2 + 1440) = v19;
//      sub_421FF0(v1);
//      v20 = *(_BYTE *)(v2 + 1128);
//      if ( v20 < 0x64u )
//        *(_BYTE *)(v2 + 1128) = v20 + 0x64u / *(_DWORD *)&byte_5D4594[2649704];
//      if ( sub_534840(v1) )
//        sub_534950(v1);
//      result = *(_DWORD *)&byte_5D4594[2649704];
//      if ( *(_DWORD *)&byte_5D4594[2598000] - *(_DWORD *)(v1 + 536) > (unsigned int)(3 * *(_DWORD *)&byte_5D4594[2649704]) )
//        *(_DWORD *)(v2 + 1440) &= 0xFFF7FFFF;
//    }
//  }
//  return result;
//}
int __cdecl sub_50A5C0__abi_raw(nox_abi_ptrslot_t a1)
{
  int v1; // esi
  int v2; // edi
  char v3; // al
  int v4; // eax
  int result; // eax
  int v6; // eax
  int v7; // eax
  int v8; // eax
  unsigned __int16 *v9; // eax
  int v10; // ecx
  unsigned __int16 v11; // cx
  int v12; // ecx
  int v13; // eax
  int v14; // edx
  int v15; // ebp
  int v16; // edx
  _DWORD *v17; // eax
  void (__cdecl *v18)(int); // eax
  int v19; // ebx
  unsigned __int8 v20; // cl
  int v21; // [esp+14h] [ebp+4h]

  const int self = NOX_PTR(a1);

  v1 = self;
  v2 = *(_DWORD *)(self + 748);
  v3 = *(_BYTE *)(v2 + 2094);
  if ( v3 )
    *(_BYTE *)(v2 + 2094) = v3 - 1;
  v4 = *(_DWORD *)(v2 + 2192);
  if ( v4 && *(_DWORD *)(v4 + 16) & 0x8020 )
    *(_DWORD *)(v2 + 2192) = 0;
  sub_50A910(self);
  sub_50CDD0(self);

  result = *(_DWORD *)(self + 16);
  if ( result & 0x1000000 )
  {
    *(_DWORD *)&byte_5D4594[1599692] = 0;
    result = *(_DWORD *)(v2 + 484);
    if ( result )
    {
      if ( !(*(_DWORD *)(self + 16) & 0x8020) )
      {
        v6 = *(_DWORD *)(v2 + 1440);
        if ( v6 & 0x200 )
        {
          v7 = sub_424300(self);
          if ( v7 )
            sub_501960(*(_DWORD *)(v7 + 64), self, 0, 0);
          sub_502490((int *)(v2 + 1248), *(_DWORD *)(self + 520), self);
          sub_5341A0(
            (char *)&byte_587000[234068],
            *(_DWORD *)&byte_5D4594[2598000],
            **(unsigned __int16 **)(self + 556),
            *(unsigned __int16 *)(*(_DWORD *)(self + 556) + 4));
        }
        v8 = *(_DWORD *)(v2 + 520);
        if ( v8 && (unsigned int)(*(_DWORD *)&byte_5D4594[2598000] - v8) >= *(int *)&byte_5D4594[2649704] )
        {
          sub_532800(self);
          *(_DWORD *)(v2 + 520) = 0;
        }
        sub_5469B0(self);
      }

      v9 = *(unsigned __int16 **)(self + 556);
      if ( v9 )
      {
        v10 = *(_DWORD *)(self + 16);
        if ( (v10 & 0x8000) == 0
          && (unsigned int)(*(_DWORD *)&byte_5D4594[2598000] - *(_DWORD *)(self + 536)) > *(int *)&byte_5D4594[2649704] )
        {
          v11 = v9[2];
          if ( *v9 < v11
            && v11
            && !(*(_DWORD *)&byte_5D4594[2598000] % (180 * *(_DWORD *)&byte_5D4594[2649704] / (unsigned int)v9[2])) )
          {
            sub_4EE460(self, 1);
          }
        }
      }

      sub_5281F0((void*)self);
      sub_547210(self);
      sub_546A70(self);
      sub_50A850(self);

      v12 = *(_DWORD *)&byte_5D4594[1599692];
      v21 = *(_DWORD *)&byte_5D4594[1599692];

      while ( 1 )
      {
        v13 = *(char *)(v2 + 544);
        v14 = 3 * v13 + 69;
        v13 *= 3;
        v15 = *(_DWORD *)(v2 + 8 * v14);
        v16 = *(_DWORD *)(v2 + 8 * v13 + 572);
        v17 = (_DWORD *)(v2 + 8 * v13 + 572);
        if ( v16 )
          break;
        *v17 = 1;
        *(_DWORD *)&byte_5D4594[1599692] = 0;
        v18 = *(void (__cdecl **)(int))&byte_587000[16 * v15 + 233160];
        if ( v18 )
        {
          v18(v1);
          v12 = *(_DWORD *)&byte_5D4594[1599692];
          if ( *(_DWORD *)&byte_5D4594[1599692] )
            continue;
        }
        goto LABEL_30;
      }

      if ( v12 )
        goto LABEL_31;
LABEL_30:
      *(_DWORD *)&byte_5D4594[1599692] = v21;
LABEL_31:
      (*(void (__cdecl **)(int))&byte_587000[16 * v15 + 233164])(v1);

      if ( *(_DWORD *)&byte_5D4594[1599692] )
        sub_509F60(v1, (int)&byte_587000[234084]);

      v19 = *(_DWORD *)(v2 + 1440);
      BYTE1(v19) &= 0xFDu;
      *(_DWORD *)(v2 + 1440) = v19;

      sub_421FF0(v1);

      v20 = *(_BYTE *)(v2 + 1128);
      if ( v20 < 0x64u )
        *(_BYTE *)(v2 + 1128) = v20 + 0x64u / *(_DWORD *)&byte_5D4594[2649704];

      if ( sub_534840(v1) )
        sub_534950(v1);

      result = *(_DWORD *)&byte_5D4594[2649704];
      if ( *(_DWORD *)&byte_5D4594[2598000] - *(_DWORD *)(v1 + 536) > (unsigned int)(3 * *(_DWORD *)&byte_5D4594[2649704]) )
        *(_DWORD *)(v2 + 1440) &= 0xFFF7FFFF;
    }
  }
  return result;
}

/* ============================================================
 * sub_5281F0  (was non-__abi_raw in your dump; still float-as-pointer)
 * ============================================================ */
//void __cdecl sub_5281F0(float a1)
//  {
//    float v1; // edi
//    int v2; // eax
//    int v3; // ebp
//    double v4; // st7
//    int v5; // esi
//    int *v6; // ebx
//    double v7; // st7
//    double v8; // st6
//    double v9; // st7
//    double v10; // st6
//    int v11; // eax
//    int v12; // eax
//    int v13; // esi
//    int v14; // eax
//    int v15; // eax
//    int v16; // esi
//    int v17; // [esp+10h] [ebp-10h]
//    float v18; // [esp+10h] [ebp-10h]
//    int v19; // [esp+14h] [ebp-Ch]
//    float v20; // [esp+18h] [ebp-8h]
//    float v21; // [esp+24h] [ebp+4h]
//
//    v1 = a1;
//    v17 = 0;
//    v2 = *(_DWORD *)(LODWORD(a1) + 16);
//    v3 = *(_DWORD *)(LODWORD(a1) + 748);
//    if ( (v2 & 0x8000) != 0 && !sub_534A40(SLODWORD(a1)) )
//      return;
//    if ( sub_40A5C0(4096) )
//      v4 = 640.0;
//    else
//      v4 = 250.0;
//    if ( v4 >= *(float *)(v3 + 1312) )
//      v21 = v4;
//    else
//      v21 = *(float *)(v3 + 1312);
//    if ( *(_DWORD *)&byte_5D4594[2598000] - *(_DWORD *)(v3 + 1212) <= (unsigned int)(2 * *(_DWORD *)&byte_5D4594[2649704]) )
//    {
//      v19 = 0;
//    }
//    else
//    {
//      v19 = 1;
//      *(_DWORD *)(v3 + 1212) = *(_DWORD *)&byte_5D4594[2598000];
//    }
//    v5 = 0;
//    if ( *(_BYTE *)(v3 + 1129) )
//    {
//      v6 = (int *)(v3 + 1132);
//      do
//      {
//        if ( *(_DWORD *)(*v6 + 16) & 0x8020
//          || !sub_536FB0(SLODWORD(v1), *v6, 0)
//          || (v7 = *(float *)(LODWORD(v1) + 56) - *(float *)(*v6 + 56),
//              v8 = *(float *)(LODWORD(v1) + 60) - *(float *)(*v6 + 60),
//              v20 = (v21 + 30.0) * (v21 + 30.0),
//              v8 * v8 + v7 * v7 > v20)
//          || (v9 = *(float *)(LODWORD(v1) + 56) - *(float *)(LODWORD(v1) + 72),
//              v10 = *(float *)(LODWORD(v1) + 60) - *(float *)(LODWORD(v1) + 76),
//              v10 * v10 + v9 * v9 > 1000.0)
//          || v19 && !sub_5370E0(SLODWORD(v1), *v6, 0) )
//        {
//          sub_528560(SLODWORD(v1), v5--);
//          v17 = 1;
//          --v6;
//        }
//        ++v5;
//        ++v6;
//      }
//      while ( v5 < *(unsigned __int8 *)(v3 + 1129) );
//    }
//    v11 = *(_DWORD *)(v3 + 1196);
//    if ( v11 && sub_4FF350(v11, 28) )
//      v17 = 1;
//    if ( (!*(_DWORD *)(v3 + 1196)
//       || *(_DWORD *)&byte_5D4594[2598000] - *(_DWORD *)(v3 + 1204) > (unsigned int)(2 * *(_DWORD *)&byte_5D4594[2649704]))
//      && (*(_DWORD *)(v3 + 1208) <= *(int *)&byte_5D4594[2598000]
//       || *(_DWORD *)&byte_5D4594[2598000] == *(_DWORD *)&byte_5D4594[2487684]) )
//    {
//      sub_517F90((float2 *)(LODWORD(v1) + 56), v21, sub_5286D0, SLODWORD(v1));
//      *(_DWORD *)(v3 + 1204) = *(_DWORD *)&byte_5D4594[2598000];
//      *(_DWORD *)(v3 + 1212) = *(_DWORD *)&byte_5D4594[2598000];
//      goto LABEL_31;
//    }
//    if ( v17 )
//    {
//  LABEL_31:
//      v12 = *(_DWORD *)(v3 + 1196);
//      if ( v12 )
//        v13 = *(_DWORD *)(v12 + 36);
//      else
//        v13 = 0;
//      sub_528610(SLODWORD(v1));
//      v14 = *(_DWORD *)(v3 + 1196);
//      if ( v14 && v13 && v13 != *(_DWORD *)(v14 + 36) )
//        *(_DWORD *)(v3 + 1200) = v13;
//    }
//    if ( *(_DWORD *)(v3 + 1204) == *(_DWORD *)&byte_5D4594[2598000] )
//    {
//      v15 = *(_DWORD *)(v3 + 1440);
//      if ( v15 & 0x400 || sub_40A5C0(0x2000) || *(_DWORD *)(v3 + 1196) )
//      {
//        *(_DWORD *)(v3 + 1208) = *(_DWORD *)&byte_5D4594[2598000] + sub_415FA0(5, 10);
//      }
//      else
//      {
//        v16 = 5 * *(_DWORD *)&byte_5D4594[2649704];
//        v18 = sub_5336D0(SLODWORD(v1));
//        *(float *)(v3 + 524) = v18;
//        if ( v18 < 0.0 )
//        {
//          *(_DWORD *)(v3 + 1208) = v16 + *(_DWORD *)&byte_5D4594[2598000];
//        }
//        else if ( v18 > (double)v21 )
//        {
//          *(_DWORD *)(v3 + 1208) = (unsigned __int64)(__int64)((v18 - v21) * (double)v16 / (1000.0 - v21))
//                                 + 10
//                                 + *(_DWORD *)&byte_5D4594[2598000];
//        }
//        else
//        {
//          *(_DWORD *)(v3 + 1208) = sub_415FA0(5, 10) + *(_DWORD *)&byte_5D4594[2598000];
//        }
//      }
//    }
//  }
// nox_xxx_unitUpdateSightMB_5281F0
void __cdecl sub_5281F0__abi_raw(nox_abi_ptrslot_t a1)
{
  int v2; // eax
  int v3; // ebp
  double v4; // st7
  int v5; // esi
  int *v6; // ebx
  double v7; // st7
  double v8; // st6
  double v9; // st7
  double v10; // st6
  int v11; // eax
  int v12; // eax
  int v13; // esi
  int v14; // eax
  int v15; // eax
  int v16; // esi

  int v17;   // was [esp+10h] aliasing in decomp; keep as normal local
  float v18; // was [esp+10h] aliasing in decomp; keep as normal local

  int v19;   // [esp+14h] [ebp-Ch]
  float v20; // [esp+18h] [ebp-8h]
  float v21; // [esp+24h] [ebp+4h]

  const uintptr_t self = (uintptr_t)NOX_PTR(a1);

  v17 = 0;
  v2 = *(_DWORD *)((uintptr_t)self + 16);
  v3 = *(_DWORD *)((uintptr_t)self + 748);
  if ( (v2 & 0x8000) != 0 && !sub_534A40((int)self) )
    return;

  if ( sub_40A5C0(4096) )
    v4 = 640.0;
  else
    v4 = 250.0;

  if ( v4 >= *(float *)(v3 + 1312) )
    v21 = (float)v4;
  else
    v21 = *(float *)(v3 + 1312);

  if ( *(_DWORD *)&byte_5D4594[2598000] - *(_DWORD *)(v3 + 1212) <= (unsigned int)(2 * *(_DWORD *)&byte_5D4594[2649704]) )
  {
    v19 = 0;
  }
  else
  {
    v19 = 1;
    *(_DWORD *)(v3 + 1212) = *(_DWORD *)&byte_5D4594[2598000];
  }

  v5 = 0;
  if ( *(_BYTE *)(v3 + 1129) )
  {
    v6 = (int *)(v3 + 1132);
    do
    {
      if ( *(_DWORD *)(*v6 + 16) & 0x8020
        || !sub_536FB0((int)self, *v6, 0)
        || (v7 = *(float *)((uintptr_t)self + 56) - *(float *)(*v6 + 56),
            v8 = *(float *)((uintptr_t)self + 60) - *(float *)(*v6 + 60),
            v20 = (v21 + 30.0f) * (v21 + 30.0f),
            (float)(v8 * v8 + v7 * v7) > v20)
        || (v9 = *(float *)((uintptr_t)self + 56) - *(float *)((uintptr_t)self + 72),
            v10 = *(float *)((uintptr_t)self + 60) - *(float *)((uintptr_t)self + 76),
            (float)(v10 * v10 + v9 * v9) > 1000.0f)
        || (v19 && !sub_5370E0((int)self, *v6, 0)) )
      {
        sub_528560((int)self, v5--);
        v17 = 1;
        --v6;
      }
      ++v5;
      ++v6;
    }
    while ( v5 < *(unsigned __int8 *)(v3 + 1129) );
  }

  v11 = *(_DWORD *)(v3 + 1196);
  if ( v11 && sub_4FF350(v11, 28) )
    v17 = 1;

  if ( (!*(_DWORD *)(v3 + 1196)
     || *(_DWORD *)&byte_5D4594[2598000] - *(_DWORD *)(v3 + 1204) > (unsigned int)(2 * *(_DWORD *)&byte_5D4594[2649704]))
    && (*(_DWORD *)(v3 + 1208) <= *(int *)&byte_5D4594[2598000]
     || *(_DWORD *)&byte_5D4594[2598000] == *(_DWORD *)&byte_5D4594[2487684]) )
  {
    /* Prefer a real function-pointer type over (void*) if you know the signature.
       If not known yet, keep the minimal cast but at least isolate it here. */
    typedef void (__cdecl *sub_517F90_cb_t)(int);
    sub_517F90((float2 *)((uintptr_t)self + 56), v21, (sub_517F90_cb_t)sub_5286D0, (int)self);

    *(_DWORD *)(v3 + 1204) = *(_DWORD *)&byte_5D4594[2598000];
    *(_DWORD *)(v3 + 1212) = *(_DWORD *)&byte_5D4594[2598000];
    goto LABEL_31;
  }

  if ( v17 )
  {
LABEL_31:
    v12 = *(_DWORD *)(v3 + 1196);
    if ( v12 )
      v13 = *(_DWORD *)(v12 + 36);
    else
      v13 = 0;

    sub_528610((int)self);

    v14 = *(_DWORD *)(v3 + 1196);
    if ( v14 && v13 && v13 != *(_DWORD *)(v14 + 36) )
      *(_DWORD *)(v3 + 1200) = v13;
  }

  if ( *(_DWORD *)(v3 + 1204) == *(_DWORD *)&byte_5D4594[2598000] )
  {
    v15 = *(_DWORD *)(v3 + 1440);
    if ( v15 & 0x400 || sub_40A5C0(0x2000) || *(_DWORD *)(v3 + 1196) )
    {
      *(_DWORD *)(v3 + 1208) = *(_DWORD *)&byte_5D4594[2598000] + sub_415FA0(5, 10);
    }
    else
    {
      v16 = 5 * *(_DWORD *)&byte_5D4594[2649704];
      v18 = (float)sub_5336D0((int)self);
      *(float *)(v3 + 524) = v18;

      if ( v18 < 0.0f )
      {
        *(_DWORD *)(v3 + 1208) = v16 + *(_DWORD *)&byte_5D4594[2598000];
      }
      else if ( v18 > (double)v21 )
      {
        *(_DWORD *)(v3 + 1208) = (unsigned __int64)(__int64)((v18 - v21) * (double)v16 / (1000.0 - v21))
                               + 10
                               + *(_DWORD *)&byte_5D4594[2598000];
      }
      else
      {
        *(_DWORD *)(v3 + 1208) = sub_415FA0(5, 10) + *(_DWORD *)&byte_5D4594[2598000];
      }
    }
  }
}

/* ============================================================
 * sub_52DD50__abi_raw  (a5 is pointer-slot)
 * ============================================================ */
// int __cdecl sub_52DD50__abi_raw(int a1, int a2, int a3, int a4, float a5)
// {
//   float v5; // edi
//   int v6; // esi
//   __int16 v8; // bx
//   __int16 v9; // ax
//   char v10; // al
//   double v11; // st7
//   int v12; // eax
//   int v13; // eax
//   int v14; // [esp-Ch] [ebp-14h]
//   float v15; // [esp+1Ch] [ebp+14h]
//
//   v5 = a5;
//   v6 = *(_DWORD *)LODWORD(a5);
//   if ( !*(_DWORD *)LODWORD(a5) )
//     return 0;
//   v8 = sub_4EE780(*(_DWORD *)LODWORD(a5));
//   if ( v8 == sub_4EE7A0(v6) && a2 == *(_DWORD *)LODWORD(a5) )
//   {
//     v9 = sub_4249A0(a1, 1);
//     sub_4FD030(a3, v9);
//     return 1;
//   }
//   v15 = sub_419D40(&byte_587000[260252]);
//   if ( a3 && *(_BYTE *)(a3 + 8) & 4 )
//   {
//     v10 = *(_BYTE *)(*(_DWORD *)(*(_DWORD *)(a3 + 748) + 276) + 2251);
//     switch ( v10 )
//     {
//       case 0:
//         v11 = *(float *)&byte_587000[312784];
// LABEL_14:
//         v15 = v11 * v15;
//         break;
//       case 2:
//         v11 = *(float *)&byte_587000[312800];
//         goto LABEL_14;
//       case 1:
//         v11 = *(float *)&byte_587000[312816];
//         goto LABEL_14;
//     }
//   }
//   v12 = sub_419A70(v15);
//   sub_4EE460(*(_DWORD *)LODWORD(v5), v12);
//   v14 = *(_DWORD *)LODWORD(v5);
//   v13 = sub_424800(a1, 1);
//   sub_501960(v13, v14, 0, 0);
//   return 1;
// }
int __cdecl sub_52DD50__abi_raw(int a1, int a2, int a3, int a4, nox_abi_ptrslot_t a5)
{
  int v6; // esi
  __int16 v8; // bx
  __int16 v9; // ax
  char v10; // al
  double v11; // st7
  int v12; // eax
  int v13; // eax
  int v14; // [esp-Ch] [ebp-14h]
  float v15; // [esp+1Ch] [ebp+14h]

  const int pslot = NOX_PTR(a5);

  v6 = *(_DWORD *)pslot;
  if ( !*(_DWORD *)pslot )
    return 0;

  v8 = sub_4EE780(*(_DWORD *)pslot);
  if ( v8 == sub_4EE7A0(v6) && a2 == *(_DWORD *)pslot )
  {
    v9 = sub_4249A0(a1, 1);
    sub_4FD030(a3, v9);
    return 1;
  }

  v15 = sub_419D40(&byte_587000[260252]);
  if ( a3 && *(_BYTE *)(a3 + 8) & 4 )
  {
    v10 = *(_BYTE *)(*(_DWORD *)(*(_DWORD *)(a3 + 748) + 276) + 2251);
    switch ( v10 )
    {
      case 0:
        v11 = *(float *)&byte_587000[312784];
LABEL_14:
        v15 = (float)(v11 * v15);
        break;
      case 2:
        v11 = *(float *)&byte_587000[312800];
        goto LABEL_14;
      case 1:
        v11 = *(float *)&byte_587000[312816];
        goto LABEL_14;
    }
  }

  v12 = sub_419A70(v15);
  sub_4EE460(*(_DWORD *)pslot, v12);
  v14 = *(_DWORD *)pslot;
  v13 = sub_424800(a1, 1);
  sub_501960(v13, v14, 0, 0);
  (void)a4;
  return 1;
}

/* ============================================================
 * sub_52E210__abi_raw
 * ============================================================ */
//int __cdecl sub_52E210__abi_raw(float a1)
//{
//  int v1; // esi
//  int v2; // eax
//  int v3; // eax
//  float v4; // edx
//  int v5; // eax
//  int v7; // edi
//  unsigned __int16 v8; // bx
//  int v9; // eax
//  int v10; // ecx
//  double v11; // st7
//  double v12; // st7
//  int v13; // eax
//  int v14; // eax
//  _DWORD *v15; // ecx
//  double v16; // st7
//  _DWORD *v17; // eax
//  int v18; // eax
//  float2 v19; // [esp+8h] [ebp-8h]
//  float v21; // [esp+14h] [ebp+4h]
//  float v22; // [esp+14h] [ebp+4h]
//
//  v1 = LODWORD(a1);
//  v2 = *(_DWORD *)(LODWORD(a1) + 16);
//  if ( v2 )
//  {
//    if ( sub_4FF350(v2, 8) )
//      return 1;
//  }
//  else if ( !*(_DWORD *)(LODWORD(a1) + 20) )
//  {
//    return 1;
//  }
//  if ( *(_DWORD *)(LODWORD(a1) + 20) )
//  {
//    v3 = *(_DWORD *)(LODWORD(a1) + 16);
//    if ( v3 )
//    {
//      v19.field_0 = *(float *)(v3 + 56);
//      v4 = *(float *)(v3 + 60);
//    }
//    else
//    {
//      v4 = *(float *)(LODWORD(a1) + 32);
//      v19.field_0 = *(float *)(LODWORD(a1) + 28);
//    }
//    v19.field_4 = v4;
//    v5 = sub_52E610((int *)&v19, v3);
//    if ( v5 )
//    {
//      sub_4EEBF0(v5, 50);
//      return 1;
//    }
//    return 1;
//  }
//  v7 = *(_DWORD *)(LODWORD(a1) + 16);
//  if ( *(_BYTE *)(v7 + 8) & 4 )
//  {
//    v8 = sub_4EEC80(v7);
//    if ( v8 >= (unsigned __int16)sub_4EECB0(v7) )
//      return 1;
//  }
//  v9 = *(_DWORD *)(LODWORD(a1) + 16);
//  if ( *(_BYTE *)(v9 + 8) & 2 )
//  {
//    if ( sub_4FEA70(v9, (float2 *)(LODWORD(a1) + 28)) )
//      return 1;
//  }
//  v10 = *(_DWORD *)(LODWORD(a1) + 16);
//  v11 = *(float *)(LODWORD(a1) + 28) - *(float *)(v10 + 56);
//  if ( v11 < 0.0 )
//    v11 = -v11;
//  v19.field_0 = v11;
//  v12 = *(float *)(LODWORD(a1) + 32) - *(float *)(v10 + 60);
//  if ( v12 < 0.0 )
//    v12 = -v12;
//  v19.field_4 = v12;
//  if ( sub_4E6BD0(v10) || v19.field_0 >= 5.0 || v19.field_4 >= 5.0 )
//    return 1;
//  v13 = sub_52E610((int *)(*(_DWORD *)(LODWORD(a1) + 16) + 56), *(_DWORD *)(LODWORD(a1) + 16));
//  *(_DWORD *)(LODWORD(a1) + 48) = v13;
//  if ( !v13 )
//  {
//    if ( *(_DWORD *)(LODWORD(a1) + 36) )
//      sub_4FEF90(SLODWORD(a1), *(_DWORD **)(LODWORD(a1) + 36));
//    return 1;
//  }
//  v21 = *(float *)(LODWORD(a1) + 72);
//  v22 = sub_419D70(&byte_587000[260408], *(_DWORD *)(v1 + 8) - 1) + v21;
//  *(float *)&v14 = COERCE_FLOAT(sub_419A70(v22));
//  v15 = *(_DWORD **)(v1 + 48);
//  v19.field_0 = *(float *)&v14;
//  v16 = (double)v14;
//  v17 = *(_DWORD **)(v1 + 36);
//  *(float *)(v1 + 72) = v22 - v16;
//  if ( v15 != v17 )
//  {
//    if ( v17 )
//      sub_4FEF90(v1, v17);
//    sub_4FF130(v1);
//  }
//  v18 = sub_419A70(v22);
//  if ( sub_52E450(*(_DWORD *)(v1 + 16), *(_DWORD *)(v1 + 48), v18)
//    && !(*(_DWORD *)&byte_5D4594[2598000] % (*(_DWORD *)&byte_5D4594[2649704] >> 1)) )
//  {
//    sub_501960(230, *(_DWORD *)(v1 + 16), 0, 0);
//    sub_501960(229, *(_DWORD *)(v1 + 48), 0, 0);
//  }
//  *(_DWORD *)(v1 + 36) = *(_DWORD *)(v1 + 48);
//  return 0;
//}
//nox_xxx_spellDrainMana_52E210
int __cdecl sub_52E210__abi_raw(nox_abi_ptrslot_t a1)
{
  int v2; // eax
  int v3; // eax
  float v4; // edx
  int v5; // eax
  int v7; // edi
  unsigned __int16 v8; // bx
  int v9; // eax
  int v10; // ecx
  double v11; // st7
  double v12; // st7
  int v13; // eax
  int v14; // eax
  _DWORD *v15; // ecx
  double v16; // st7
  _DWORD *v17; // eax
  int v18; // eax
  float2 v19; // [esp+8h] [ebp-8h]
  float v21; // [esp+14h] [ebp+4h]
  float v22; // [esp+14h] [ebp+4h]

  const int self = NOX_PTR(a1);

  v2 = *(_DWORD *)(self + 16);
  if ( v2 )
  {
    if ( sub_4FF350(v2, 8) )
      return 1;
  }
  else if ( !*(_DWORD *)(self + 20) )
  {
    return 1;
  }

  if ( *(_DWORD *)(self + 20) )
  {
    v3 = *(_DWORD *)(self + 16);
    if ( v3 )
    {
      v19.field_0 = *(float *)(v3 + 56);
      v4 = *(float *)(v3 + 60);
    }
    else
    {
      v4 = *(float *)(self + 32);
      v19.field_0 = *(float *)(self + 28);
    }
    v19.field_4 = v4;
    v5 = sub_52E610((int *)&v19, v3);
    if ( v5 )
    {
      sub_4EEBF0(v5, 50);
      return 1;
    }
    return 1;
  }

  v7 = *(_DWORD *)(self + 16);
  if ( *(_BYTE *)(v7 + 8) & 4 )
  {
    v8 = sub_4EEC80(v7);
    if ( v8 >= (unsigned __int16)sub_4EECB0(v7) )
      return 1;
  }

  v9 = *(_DWORD *)(self + 16);
  if ( *(_BYTE *)(v9 + 8) & 2 )
  {
    if ( sub_4FEA70(v9, (float2 *)(self + 28)) )
      return 1;
  }

  v10 = *(_DWORD *)(self + 16);
  v11 = *(float *)(self + 28) - *(float *)(v10 + 56);
  if ( v11 < 0.0 )
    v11 = -v11;
  v19.field_0 = (float)v11;

  v12 = *(float *)(self + 32) - *(float *)(v10 + 60);
  if ( v12 < 0.0 )
    v12 = -v12;
  v19.field_4 = (float)v12;

  if ( sub_4E6BD0(v10) || v19.field_0 >= 5.0f || v19.field_4 >= 5.0f )
    return 1;

  v13 = sub_52E610((int *)(*(_DWORD *)(self + 16) + 56), *(_DWORD *)(self + 16));
  *(_DWORD *)(self + 48) = v13;

  if ( !v13 )
  {
    if ( *(_DWORD *)(self + 36) )
      sub_4FEF90(self, *(_DWORD **)(self + 36));
    return 1;
  }

  v21 = *(float *)(self + 72);
  v22 = sub_419D70(&byte_587000[260408], *(_DWORD *)(self + 8) - 1) + v21;
  int i14 = sub_419A70(v22);
  float f14;
  memcpy(&f14, &i14, sizeof(f14));

  v15 = *(_DWORD **)(self + 48);
  v19.field_0 = f14;
  v16 = (double)i14;
  v17 = *(_DWORD **)(self + 36);
  *(float *)(self + 72) = (float)(v22 - v16);

  if ( v15 != v17 )
  {
    if ( v17 )
      sub_4FEF90(self, v17);
    sub_4FF130(self);
  }

  v18 = sub_419A70(v22);
  if ( sub_52E450(*(_DWORD *)(self + 16), *(_DWORD *)(self + 48), v18)
    && !(*(_DWORD *)&byte_5D4594[2598000] % (*(_DWORD *)&byte_5D4594[2649704] >> 1)) )
  {
    sub_501960(230, *(_DWORD *)(self + 16), 0, 0);
    sub_501960(229, *(_DWORD *)(self + 48), 0, 0);
  }

  *(_DWORD *)(self + 36) = *(_DWORD *)(self + 48);
  return 0;
}

/* ============================================================
 * sub_52E850__abi_raw
 * ============================================================ */
// int __cdecl sub_52E850__abi_raw(float a1)
// {
//   int v1; // esi
//   int v2; // eax
//   int result; // eax
//   int v4; // ecx
//   int v5; // eax
//   void (__cdecl **v6)(_DWORD, _DWORD, _DWORD, int, int); // edi
//   int v7; // eax
//   int v8; // eax
//   int v9; // eax
//   int v10; // eax
//   int v11; // eax
//   int v12; // ecx
//   int v13; // edi
//   int v14; // eax
//   int v15; // edx
//   float v16; // edi
//   int v17; // eax
//   _DWORD *v18; // ecx
//   double v19; // st7
//   _DWORD *v20; // eax
//   int v21; // eax
//   int v22; // eax
//   int v23; // eax
//   int v24; // eax
//   int v25; // ecx
//   int v26; // [esp-4h] [ebp-20h]
//   float v27; // [esp+0h] [ebp-1Ch]
//   float v28; // [esp+4h] [ebp-18h]
//   float2 v29; // [esp+14h] [ebp-8h]
//   float v31; // [esp+20h] [ebp+4h]
//   float v32; // [esp+20h] [ebp+4h]
//   float v33; // [esp+20h] [ebp+4h]
//   int v34; // [esp+20h] [ebp+4h]
//
//   v1 = LODWORD(a1);
//   v2 = *(_DWORD *)(LODWORD(a1) + 16);
//   if ( v2 )
//   {
//     if ( sub_4FF350(v2, 8) )
//       return 1;
//   }
//   else if ( !*(_DWORD *)(LODWORD(a1) + 20) )
//   {
//     return 1;
//   }
//   v31 = sub_419D40(&byte_587000[260440]);
//   if ( !*(_DWORD *)(v1 + 20) )
//   {
//     v9 = *(_DWORD *)(v1 + 16);
//     if ( v9 && *(_BYTE *)(v9 + 8) & 2 && sub_4FEA70(v9, (float2 *)(v1 + 28)) )
//       return 1;
//     if ( (unsigned int)(*(_DWORD *)&byte_5D4594[2598000] - *(_DWORD *)(v1 + 60)) > 2 && sub_4E6BD0(*(_DWORD *)(v1 + 16)) )
//       return 1;
//     v10 = *(_DWORD *)(v1 + 48);
//     if ( v10 )
//     {
//       if ( !(*(_DWORD *)(v10 + 16) & 0x8020)
//         && sub_4E6E50(
//              (float2 *)(*(_DWORD *)(v1 + 16) + 56),
//              *(__int16 *)(*(_DWORD *)(v1 + 16) + 124),
//              (float2 *)(v10 + 56)) & 1
//         && sub_4E6C00(*(_DWORD *)(v1 + 48), *(_DWORD *)(v1 + 16)) <= v31
//         && sub_5370E0(*(_DWORD *)(v1 + 16), *(_DWORD *)(v1 + 48), 0) )
//       {
//         goto LABEL_31;
//       }
//       *(_DWORD *)(v1 + 48) = 0;
//     }
//     v11 = *(_DWORD *)(v1 + 16);
//     if ( *(_BYTE *)(v11 + 8) & 4 )
//     {
//       v12 = *(_DWORD *)(v11 + 748);
//       v13 = *(_DWORD *)(v12 + 288);
//       if ( v13 )
//       {
//         if ( sub_5330C0(v11, *(_DWORD *)(v12 + 288)) && sub_4E6C00(*(_DWORD *)(v1 + 16), v13) <= v31 )
//           *(_DWORD *)(v1 + 48) = v13;
//       }
//     }
//     if ( *(_DWORD *)(v1 + 48) )
//       goto LABEL_32;
//     *(_DWORD *)&byte_5D4594[2487832] = 0;
//     *(_DWORD *)&byte_5D4594[2487880] = 0;
//     v14 = *(_DWORD *)(v1 + 16);
//     *(_DWORD *)&byte_5D4594[2487868] = *(_DWORD *)(v14 + 56);
//     v15 = *(_DWORD *)(v14 + 60);
//     *(float *)&byte_5D4594[2487884] = v31 * v31;
//     *(_DWORD *)&byte_5D4594[2487872] = v15;
//     sub_517F90((float2 *)(*(_DWORD *)(v1 + 16) + 56), v31, sub_52EC60, *(_DWORD *)(v1 + 16));
//     *(_DWORD *)(v1 + 48) = *(_DWORD *)&byte_5D4594[2487880];
// LABEL_31:
//     if ( !*(_DWORD *)(v1 + 48) )
//     {
//       if ( *(_DWORD *)(v1 + 36) )
//       {
//         sub_4FEF90(v1, *(_DWORD **)(v1 + 36));
//         *(_DWORD *)(v1 + 36) = 0;
//       }
//       return 0;
//     }
// LABEL_32:
//     v32 = *(float *)(v1 + 72);
//     v33 = sub_419D70(&byte_587000[260480], *(_DWORD *)(v1 + 8) - 1) + v32;
//     v16 = v33;
//     v17 = sub_419A70(v33);
//     v18 = *(_DWORD **)(v1 + 48);
//     LODWORD(v29.field_0) = v17;
//     v19 = (double)v17;
//     v20 = *(_DWORD **)(v1 + 36);
//     *(float *)(v1 + 72) = v33 - v19;
//     if ( v18 != v20 )
//     {
//       if ( v20 )
//         sub_4FEF90(v1, v20);
//       sub_4FF130(v1);
//     }
//     v34 = *(_DWORD *)(v1 + 48);
//     v21 = sub_419A70(v16);
//     (*(void (__cdecl **)(_DWORD, _DWORD, _DWORD, int, int))(v34 + 716))(
//       *(_DWORD *)(v1 + 48),
//       *(_DWORD *)(v1 + 16),
//       0,
//       v21,
//       17);
//     v22 = *(_DWORD *)(v1 + 48);
//     if ( *(_DWORD *)(v22 + 16) & 0x8020 )
//       sub_522FF0(130, (float2 *)(v22 + 56));
//     v23 = *(_DWORD *)(v1 + 16);
//     *(_DWORD *)(v1 + 36) = *(_DWORD *)(v1 + 48);
//     if ( *(_BYTE *)(v23 + 8) & 4 )
//       sub_4FA020((_DWORD *)v23, 10);
//     if ( !(*(_DWORD *)&byte_5D4594[2598000] % (*(_DWORD *)&byte_5D4594[2649704] / 3u)) )
//     {
//       sub_501960(32, *(_DWORD *)(v1 + 16), 0, 0);
//       sub_501960(32, *(_DWORD *)(v1 + 48), 0, 0);
//     }
//     v28 = sub_419D40(&byte_587000[260500]);
//     *(_DWORD *)(v1 + 68) = *(_DWORD *)&byte_5D4594[2598000] + sub_419A70(v28);
//     v24 = *(_DWORD *)(v1 + 16);
//     if ( *(_BYTE *)(v24 + 8) & 4 )
//     {
//       sub_4FA020((_DWORD *)v24, 10);
//       sub_4EEBF0(*(_DWORD *)(v1 + 16), 1);
//       if ( !sub_4EEC80(*(_DWORD *)(v1 + 16)) )
//         return 1;
//     }
//     v25 = *(_DWORD *)(*(_DWORD *)(v1 + 48) + 16);
//     if ( (v25 & 0x8000) != 0 )
//     {
//       result = 0;
//       *(_DWORD *)(v1 + 68) = *(_DWORD *)&byte_5D4594[2598000] + 1;
//       return result;
//     }
//     return 0;
//   }
//   v4 = *(_DWORD *)(v1 + 32);
//   v29.field_0 = *(float *)(v1 + 28);
//   *(_DWORD *)&byte_5D4594[2487868] = v29.field_0;
//   *(_DWORD *)&byte_5D4594[2487872] = v4;
//   *(_DWORD *)&byte_5D4594[2487880] = 0;
//   *(float *)&byte_5D4594[2487884] = v31 * v31;
//   *(_DWORD *)&byte_5D4594[2487832] = 1;
//   v5 = *(_DWORD *)(v1 + 16);
//   v29.field_4 = v4;
//   sub_517F90(&v29, v31, sub_52EC60, v5);
//   if ( *(_DWORD *)&byte_5D4594[2487880] )
//   {
//     v6 = (void (__cdecl **)(_DWORD, _DWORD, _DWORD, int, int))(*(_DWORD *)&byte_5D4594[2487880] + 716);
//     v27 = sub_419D40(&byte_587000[260456]);
//     v7 = sub_419A70(v27);
//     (*v6)(*(_DWORD *)&byte_5D4594[2487880], *(_DWORD *)(v1 + 12), 0, v7, 17);
//     v26 = *(_DWORD *)&byte_5D4594[2487880];
//     v8 = sub_424800(24, 0);
//     sub_501960(v8, v26, 0, 0);
//     sub_522FF0(130, (float2 *)(*(_DWORD *)&byte_5D4594[2487880] + 56));
//   }
//   return 1;
// }
int __cdecl sub_52E850__abi_raw(nox_abi_ptrslot_t a1)
{
  int v2; // eax
  int result; // eax
  int v4; // ecx
  int v5; // eax
  void (__cdecl **v6)(_DWORD, _DWORD, _DWORD, int, int); // edi
  int v7; // eax
  int v8; // eax
  int v9; // eax
  int v10; // eax
  int v11; // eax
  int v12; // ecx
  int v13; // edi
  int v14; // eax
  int v15; // edx
  float v16; // edi
  int v17; // eax
  _DWORD *v18; // ecx
  double v19; // st7
  _DWORD *v20; // eax
  int v21; // eax
  int v22; // eax
  int v23; // eax
  int v24; // eax
  int v25; // ecx
  int v26; // [esp-4h] [ebp-20h]
  float v27; // [esp+0h] [ebp-1Ch]
  float v28; // [esp+4h] [ebp-18h]
  float2 v29; // [esp+14h] [ebp-8h]
  float v31; // [esp+20h] [ebp+4h]
  float v32; // [esp+20h] [ebp+4h]
  float v33; // [esp+20h] [ebp+4h]
  int v34; // [esp+20h] [ebp+4h]

  const int self = NOX_PTR(a1);

  v2 = *(_DWORD *)(self + 16);
  if ( v2 )
  {
    if ( sub_4FF350(v2, 8) )
      return 1;
  }
  else if ( !*(_DWORD *)(self + 20) )
  {
    return 1;
  }

  v31 = sub_419D40(&byte_587000[260440]);

  if ( !*(_DWORD *)(self + 20) )
  {
    v9 = *(_DWORD *)(self + 16);
    if ( v9 && *(_BYTE *)(v9 + 8) & 2 && sub_4FEA70(v9, (float2 *)(self + 28)) )
      return 1;

    if ( (unsigned int)(*(_DWORD *)&byte_5D4594[2598000] - *(_DWORD *)(self + 60)) > 2 && sub_4E6BD0(*(_DWORD *)(self + 16)) )
      return 1;

    v10 = *(_DWORD *)(self + 48);
    if ( v10 )
    {
      if ( !(*(_DWORD *)(v10 + 16) & 0x8020)
        && sub_4E6E50(
             (float2 *)(*(_DWORD *)(self + 16) + 56),
             *(__int16 *)(*(_DWORD *)(self + 16) + 124),
             (float2 *)(v10 + 56)) & 1
        && sub_4E6C00(*(_DWORD *)(self + 48), *(_DWORD *)(self + 16)) <= v31
        && sub_5370E0(*(_DWORD *)(self + 16), *(_DWORD *)(self + 48), 0) )
      {
        goto LABEL_31;
      }
      *(_DWORD *)(self + 48) = 0;
    }

    v11 = *(_DWORD *)(self + 16);
    if ( *(_BYTE *)(v11 + 8) & 4 )
    {
      v12 = *(_DWORD *)(v11 + 748);
      v13 = *(_DWORD *)(v12 + 288);
      if ( v13 )
      {
        if ( sub_5330C0(v11, *(_DWORD *)(v12 + 288)) && sub_4E6C00(*(_DWORD *)(self + 16), v13) <= v31 )
          *(_DWORD *)(self + 48) = v13;
      }
    }

    if ( *(_DWORD *)(self + 48) )
      goto LABEL_32;

    *(_DWORD *)&byte_5D4594[2487832] = 0;
    *(_DWORD *)&byte_5D4594[2487880] = 0;
    v14 = *(_DWORD *)(self + 16);
    *(_DWORD *)&byte_5D4594[2487868] = *(_DWORD *)(v14 + 56);
    v15 = *(_DWORD *)(v14 + 60);
    *(float *)&byte_5D4594[2487884] = v31 * v31;
    *(_DWORD *)&byte_5D4594[2487872] = v15;
    sub_517F90((float2 *)(*(_DWORD *)(self + 16) + 56), v31, (void *)sub_52EC60, *(_DWORD *)(self + 16));
    *(_DWORD *)(self + 48) = *(_DWORD *)&byte_5D4594[2487880];

LABEL_31:
    if ( !*(_DWORD *)(self + 48) )
    {
      if ( *(_DWORD *)(self + 36) )
      {
        sub_4FEF90(self, *(_DWORD **)(self + 36));
        *(_DWORD *)(self + 36) = 0;
      }
      return 0;
    }

LABEL_32:
    v32 = *(float *)(self + 72);
    v33 = sub_419D70(&byte_587000[260480], *(_DWORD *)(self + 8) - 1) + v32;
    v16 = v33;
    v17 = sub_419A70(v33);

    v18 = *(_DWORD **)(self + 48);
    LODWORD(v29.field_0) = v17;
    v19 = (double)v17;
    v20 = *(_DWORD **)(self + 36);
    *(float *)(self + 72) = (float)(v33 - v19);

    if ( v18 != v20 )
    {
      if ( v20 )
        sub_4FEF90(self, v20);
      sub_4FF130(self);
    }

    v34 = *(_DWORD *)(self + 48);
    v21 = sub_419A70(v16);
    (*(void (__cdecl **)(_DWORD, _DWORD, _DWORD, int, int))(v34 + 716))(
      *(_DWORD *)(self + 48),
      *(_DWORD *)(self + 16),
      0,
      v21,
      17);

    v22 = *(_DWORD *)(self + 48);
    if ( *(_DWORD *)(v22 + 16) & 0x8020 )
      sub_522FF0(130, (float2 *)(v22 + 56));

    v23 = *(_DWORD *)(self + 16);
    *(_DWORD *)(self + 36) = *(_DWORD *)(self + 48);
    if ( *(_BYTE *)(v23 + 8) & 4 )
      sub_4FA020((_DWORD *)v23, 10);

    if ( !(*(_DWORD *)&byte_5D4594[2598000] % (*(_DWORD *)&byte_5D4594[2649704] / 3u)) )
    {
      sub_501960(32, *(_DWORD *)(self + 16), 0, 0);
      sub_501960(32, *(_DWORD *)(self + 48), 0, 0);
    }

    v28 = sub_419D40(&byte_587000[260500]);
    *(_DWORD *)(self + 68) = *(_DWORD *)&byte_5D4594[2598000] + sub_419A70(v28);

    v24 = *(_DWORD *)(self + 16);
    if ( *(_BYTE *)(v24 + 8) & 4 )
    {
      sub_4FA020((_DWORD *)v24, 10);
      sub_4EEBF0(*(_DWORD *)(self + 16), 1);
      if ( !sub_4EEC80(*(_DWORD *)(self + 16)) )
        return 1;
    }

    v25 = *(_DWORD *)(*(_DWORD *)(self + 48) + 16);
    if ( (v25 & 0x8000) != 0 )
    {
      result = 0;
      *(_DWORD *)(self + 68) = *(_DWORD *)&byte_5D4594[2598000] + 1;
      return result;
    }
    return 0;
  }

  /* branch: (self+20) != 0 */
  v4 = *(_DWORD *)(self + 32);
  v29.field_0 = *(float *)(self + 28);
  *(_DWORD *)&byte_5D4594[2487868] = v29.field_0;
  *(_DWORD *)&byte_5D4594[2487872] = v4;
  *(_DWORD *)&byte_5D4594[2487880] = 0;
  *(float *)&byte_5D4594[2487884] = v31 * v31;
  *(_DWORD *)&byte_5D4594[2487832] = 1;

  v5 = *(_DWORD *)(self + 16);
  v29.field_4 = (float)v4;
  sub_517F90(&v29, v31, (void *)sub_52EC60, v5);

  if ( *(_DWORD *)&byte_5D4594[2487880] )
  {
    v6 = (void (__cdecl **)(_DWORD, _DWORD, _DWORD, int, int))(*(_DWORD *)&byte_5D4594[2487880] + 716);
    v27 = sub_419D40(&byte_587000[260456]);
    v7 = sub_419A70(v27);
    (*v6)(*(_DWORD *)&byte_5D4594[2487880], *(_DWORD *)(self + 12), 0, v7, 17);
    v26 = *(_DWORD *)&byte_5D4594[2487880];
    v8 = sub_424800(24, 0);
    sub_501960(v8, v26, 0, 0);
    sub_522FF0(130, (float2 *)(*(_DWORD *)&byte_5D4594[2487880] + 56));
  }

  return 1;
}

/* ============================================================
 * sub_52F2E0__abi_raw
 * ============================================================ */

// int __cdecl sub_52F2E0__abi_raw(float a1)
// {
//   float v1; // esi
//   int v2; // eax
//   int v4; // eax
//   int v5; // eax
//   __int16 v6; // di
//   int v7; // eax
//   char v8; // al
//   double v9; // st7
//   int v10; // eax
//   float v11; // [esp+10h] [ebp+4h]
//
//   v1 = a1;
//   v2 = *(_DWORD *)(LODWORD(a1) + 48);
//   if ( !v2 )
//     return 1;
//   if ( *(_DWORD *)(v2 + 16) & 0x8020 )
//     return 1;
//   v4 = *(_DWORD *)(LODWORD(a1) + 16);
//   if ( v4 && sub_4FF350(v4, 8) )
//     return 1;
//   if ( !sub_5370E0(*(_DWORD *)(LODWORD(a1) + 16), *(_DWORD *)(LODWORD(a1) + 48), 0) )
//     return 1;
//   if ( !sub_4EEC80(*(_DWORD *)(LODWORD(a1) + 16)) )
//     return 1;
//   v5 = *(_DWORD *)(LODWORD(a1) + 16);
//   if ( *(_BYTE *)(v5 + 8) & 2 && sub_4FEA70(v5, (float2 *)(LODWORD(a1) + 28)) )
//     return 1;
//   if ( sub_4E6BD0(*(_DWORD *)(LODWORD(a1) + 16)) )
//     return 1;
//   v6 = sub_4EE7A0(*(_DWORD *)(LODWORD(a1) + 48));
//   if ( v6 == sub_4EE780(*(_DWORD *)(LODWORD(a1) + 48)) )
//     return 1;
//   v7 = *(_DWORD *)(LODWORD(a1) + 16);
//   v11 = *(float *)(LODWORD(a1) + 72) + *(float *)&byte_587000[4 * *(_DWORD *)(LODWORD(a1) + 8) + 260360];
//   if ( v7 && *(_BYTE *)(v7 + 8) & 4 )
//   {
//     v8 = *(_BYTE *)(*(_DWORD *)(*(_DWORD *)(v7 + 748) + 276) + 2251);
//     switch ( v8 )
//     {
//       case 0:
//         v9 = *(float *)&byte_587000[312784];
// LABEL_27:
//         v11 = v9 * v11;
//         break;
//       case 2:
//         v9 = *(float *)&byte_587000[312800];
//         goto LABEL_27;
//       case 1:
//         v9 = *(float *)&byte_587000[312816];
//         goto LABEL_27;
//     }
//   }
//   *(float *)(LODWORD(v1) + 72) = v11 - (double)sub_419A70(v11);
//   v10 = sub_419A70(v11);
//   sub_4EE460(*(_DWORD *)(LODWORD(v1) + 48), v10);
//   sub_4EEBF0(*(_DWORD *)(LODWORD(v1) + 16), 1);
//   return 0;
// }
int __cdecl sub_52F2E0__abi_raw(nox_abi_ptrslot_t a1)
{
  int v2; // eax
  int v4; // eax
  int v5; // eax
  __int16 v6; // di
  int v7; // eax
  char v8; // al
  double v9; // st7
  int v10; // eax
  float v11; // [esp+10h] [ebp+4h]

  const int self = NOX_PTR(a1);

  v2 = *(_DWORD *)(self + 48);
  if ( !v2 )
    return 1;
  if ( *(_DWORD *)(v2 + 16) & 0x8020 )
    return 1;

  v4 = *(_DWORD *)(self + 16);
  if ( v4 && sub_4FF350(v4, 8) )
    return 1;

  if ( !sub_5370E0(*(_DWORD *)(self + 16), *(_DWORD *)(self + 48), 0) )
    return 1;

  if ( !sub_4EEC80(*(_DWORD *)(self + 16)) )
    return 1;

  v5 = *(_DWORD *)(self + 16);
  if ( *(_BYTE *)(v5 + 8) & 2 && sub_4FEA70(v5, (float2 *)(self + 28)) )
    return 1;

  if ( sub_4E6BD0(*(_DWORD *)(self + 16)) )
    return 1;

  v6 = sub_4EE7A0(*(_DWORD *)(self + 48));
  if ( v6 == sub_4EE780(*(_DWORD *)(self + 48)) )
    return 1;

  v7 = *(_DWORD *)(self + 16);
  v11 = *(float *)(self + 72) + *(float *)&byte_587000[4 * *(_DWORD *)(self + 8) + 260360];

  if ( v7 && *(_BYTE *)(v7 + 8) & 4 )
  {
    v8 = *(_BYTE *)(*(_DWORD *)(*(_DWORD *)(v7 + 748) + 276) + 2251);
    switch ( v8 )
    {
      case 0:
        v9 = *(float *)&byte_587000[312784];
LABEL_27:
        v11 = (float)(v9 * v11);
        break;
      case 2:
        v9 = *(float *)&byte_587000[312800];
        goto LABEL_27;
      case 1:
        v9 = *(float *)&byte_587000[312816];
        goto LABEL_27;
    }
  }

  *(float *)(self + 72) = (float)(v11 - (double)sub_419A70(v11));
  v10 = sub_419A70(v11);
  sub_4EE460(*(_DWORD *)(self + 48), v10);
  sub_4EEBF0(*(_DWORD *)(self + 16), 1);
  return 0;
}

/* ============================================================
 * sub_52F460__abi_raw
 * ============================================================ */
// int __cdecl sub_52F460__abi_raw(float a1)
// {
//   float v1; // esi
//   int v2; // eax
//   int result; // eax
//   int v4; // eax
//   int v5; // eax
//   __int16 v6; // di
//   __int16 v7; // ax
//   float v8; // [esp+10h] [ebp+4h]
//   float v9; // [esp+10h] [ebp+4h]
//
//   v1 = a1;
//   v2 = *(_DWORD *)(LODWORD(a1) + 48);
//   if ( !v2 )
//     return 1;
//   if ( *(_DWORD *)(v2 + 16) & 0x8020 )
//     return 1;
//   if ( *(_DWORD *)(LODWORD(a1) + 20) )
//   {
//     sub_4EEB80(v2, 20);
//     sub_4EE5E0(*(_DWORD *)(LODWORD(a1) + 48), 20);
//     result = 1;
//   }
//   else
//   {
//     v4 = *(_DWORD *)(LODWORD(a1) + 16);
//     if ( v4 && sub_4FF350(v4, 8) )
//     {
//       result = 1;
//     }
//     else
//     {
//       v5 = *(_DWORD *)(LODWORD(a1) + 48);
//       if ( *(_BYTE *)(v5 + 8) & 2 && sub_4FEA70(v5, (float2 *)(LODWORD(a1) + 28)) )
//       {
//         result = 1;
//       }
//       else
//       {
//         v6 = sub_4EECB0(*(_DWORD *)(LODWORD(a1) + 48));
//         if ( v6 == sub_4EEC80(*(_DWORD *)(LODWORD(a1) + 48)) )
//         {
//           result = 1;
//         }
//         else if ( (unsigned __int16)sub_4EE780(*(_DWORD *)(LODWORD(a1) + 16)) > 1u )
//         {
//           if ( sub_4EE780(*(_DWORD *)(LODWORD(a1) + 16)) )
//           {
//             v8 = *(float *)(LODWORD(a1) + 72);
//             v9 = sub_419D70(&byte_587000[260660], *(_DWORD *)(LODWORD(v1) + 8) - 1) + v8;
//             *(float *)(LODWORD(v1) + 72) = v9 - (double)sub_419A70(v9);
//             v7 = sub_419A70(v9);
//             sub_4EEB80(*(_DWORD *)(LODWORD(v1) + 48), v7);
//             sub_4EE5E0(*(_DWORD *)(LODWORD(v1) + 16), 1);
//           }
//           result = 0;
//         }
//         else
//         {
//           result = 1;
//         }
//       }
//     }
//   }
//   return result;
// }
int __cdecl sub_52F460__abi_raw(nox_abi_ptrslot_t a1)
{
  int v2; // eax
  int result; // eax
  int v4; // eax
  int v5; // eax
  __int16 v6; // di
  __int16 v7; // ax
  float v8; // [esp+10h] [ebp+4h]
  float v9; // [esp+10h] [ebp+4h]

  const int self = NOX_PTR(a1);

  v2 = *(_DWORD *)(self + 48);
  if ( !v2 )
    return 1;
  if ( *(_DWORD *)(v2 + 16) & 0x8020 )
    return 1;

  if ( *(_DWORD *)(self + 20) )
  {
    sub_4EEB80(v2, 20);
    sub_4EE5E0(*(_DWORD *)(self + 48), 20);
    result = 1;
  }
  else
  {
    v4 = *(_DWORD *)(self + 16);
    if ( v4 && sub_4FF350(v4, 8) )
    {
      result = 1;
    }
    else
    {
      v5 = *(_DWORD *)(self + 48);
      if ( *(_BYTE *)(v5 + 8) & 2 && sub_4FEA70(v5, (float2 *)(self + 28)) )
      {
        result = 1;
      }
      else
      {
        v6 = sub_4EECB0(*(_DWORD *)(self + 48));
        if ( v6 == sub_4EEC80(*(_DWORD *)(self + 48)) )
        {
          result = 1;
        }
        else if ( (unsigned __int16)sub_4EE780(*(_DWORD *)(self + 16)) > 1u )
        {
          if ( sub_4EE780(*(_DWORD *)(self + 16)) )
          {
            v8 = *(float *)(self + 72);
            v9 = sub_419D70(&byte_587000[260660], *(_DWORD *)(self + 8) - 1) + v8;
            *(float *)(self + 72) = (float)(v9 - (double)sub_419A70(v9));
            v7 = (__int16)sub_419A70(v9);
            sub_4EEB80(*(_DWORD *)(self + 48), v7);
            sub_4EE5E0(*(_DWORD *)(self + 16), 1);
          }
          result = 0;
        }
        else
        {
          result = 1;
        }
      }
    }
  }
  return result;
}

/* ============================================================
 * sub_52F8A0__abi_raw  (FULL BODY converted)
 * ============================================================ */
// int __cdecl sub_52F8A0__abi_raw(float a1)
// {
//   int v1; // esi
//   int v2; // eax
//   int v4; // eax
//   int v5; // eax
//   int v6; // edi
//   int v7; // ecx
//   int v8; // edx
//   int v9; // ebx
//   int v10; // eax
//   int v11; // ecx
//   int v12; // edi
//   int v13; // eax
//   int v14; // ecx
//   int v15; // ecx
//   int v16; // ecx
//   int v17; // ecx
//   int v18; // ecx
//   int v19; // eax
//   int v20; // ebx
//   _DWORD *v21; // edi
//   _DWORD *j; // ebp
//   int v23; // eax
//   int v24; // eax
//   int v25; // ecx
//   int v26; // eax
//   int v27; // edi
//   char v28; // al
//   unsigned __int8 v29; // al
//   int v30; // eax
//   int v31; // ebp
//   int i; // edi
//   int v33; // eax
//   int v34; // edi
//   float v35; // [esp+0h] [ebp-20h]
//   float v36; // [esp+0h] [ebp-20h]
//   float v37; // [esp+0h] [ebp-20h]
//   float v38; // [esp+0h] [ebp-20h]
//   float v39; // [esp+8h] [ebp-18h]
//   float v40; // [esp+1Ch] [ebp-4h]
//   float v41; // [esp+24h] [ebp+4h]
//   float v42; // [esp+24h] [ebp+4h]
//
//   v1 = LODWORD(a1);
//   v2 = *(_DWORD *)(LODWORD(a1) + 16);
//   if ( v2 )
//   {
//     if ( sub_4FF350(v2, 8) )
//       return 1;
//   }
//   else if ( !*(_DWORD *)(LODWORD(a1) + 20) )
//   {
//     return 1;
//   }
//   v41 = sub_419D40(&byte_587000[260712]);
//   if ( *(_DWORD *)(v1 + 20) )
//   {
//     *(_DWORD *)&byte_5D4594[2487820] = *(_DWORD *)(v1 + 28);
//     *(_DWORD *)&byte_5D4594[2487824] = *(_DWORD *)(v1 + 32);
//     sub_517F90((float2 *)(v1 + 28), v41, sub_530020, *(_DWORD *)(v1 + 16));
//     return 1;
//   }
//   if ( *(_BYTE *)(*(_DWORD *)(v1 + 16) + 8) & 4 && !sub_4EEC80(*(_DWORD *)(v1 + 16)) )
//     return 1;
//   if ( (unsigned int)(*(_DWORD *)&byte_5D4594[2598000] - *(_DWORD *)(v1 + 60)) > 2 && sub_4E6BD0(*(_DWORD *)(v1 + 16)) )
//     return 1;
//   v4 = *(_DWORD *)(v1 + 16);
//   if ( *(_BYTE *)(v4 + 8) & 2 && sub_4FEA70(v4, (float2 *)(v1 + 28)) )
//     return 1;
//   v5 = *(_DWORD *)(v1 + 104);
//   if ( v5 )
//   {
//     do
//     {
//       v6 = *(_DWORD *)(v5 + 116);
//       sub_4FE980(v5);
//       v5 = v6;
//     }
//     while ( v6 );
//   }
//   v7 = *(_DWORD *)(v1 + 8);
//   *(_DWORD *)(v1 + 104) = *(_DWORD *)(v1 + 108);
//   *(_DWORD *)(v1 + 108) = 0;
//   *(_DWORD *)&byte_5D4594[2487908] = 0;
//   *(_DWORD *)&byte_5D4594[2487904] = 0;
//   v8 = *(_DWORD *)(v1 + 16);
//   *(_DWORD *)&byte_5D4594[2487844] = 0;
//   v9 = *(_DWORD *)&byte_587000[4 * v7 + 260380];
//   *(_DWORD *)&byte_5D4594[2487848] = 0;
//   *(_DWORD *)&byte_5D4594[2487900] = v8;
//   *(_DWORD *)&byte_5D4594[2487852] = 0;
//   *(_DWORD *)&byte_5D4594[2487856] = 0;
//   *(_DWORD *)&byte_5D4594[2487860] = 0;
//   v10 = *(_DWORD *)(v1 + 16);
//   if ( !(*(_BYTE *)(v10 + 8) & 4)
//     || (v11 = *(_DWORD *)(v10 + 748), (v12 = *(_DWORD *)(v11 + 288)) == 0)
//     || (!sub_5330C0(v10, *(_DWORD *)(v11 + 288)) || sub_4E6C00(*(_DWORD *)(v1 + 16), v12) > v41 ? (v13 = *(_DWORD *)&byte_5D4594[2487908]) : (v13 = v12, *(_DWORD *)&byte_5D4594[2487908] = v12),
//         !v13) )
//   {
//     *(float *)&byte_5D4594[2487912] = v41 * v41;
//     sub_517F90((float2 *)(v1 + 28), v41, sub_52FF10, *(_DWORD *)(v1 + 16));
//     v13 = *(_DWORD *)&byte_5D4594[2487908];
//     if ( !*(_DWORD *)&byte_5D4594[2487908] )
//     {
//       for ( i = *(_DWORD *)(v1 + 104); i; i = *(_DWORD *)(i + 116) )
//       {
//         if ( *(_DWORD *)(i + 48) )
//           sub_4FEF90(i, *(_DWORD **)(i + 48));
//       }
//       v33 = *(_DWORD *)(v1 + 104);
//       if ( v33 )
//       {
//         do
//         {
//           v34 = *(_DWORD *)(v33 + 116);
//           sub_4FE980(v33);
//           v33 = v34;
//         }
//         while ( v34 );
//       }
//       *(_DWORD *)(v1 + 104) = 0;
//       return 0;
//     }
//   }
//   v14 = *(_DWORD *)&byte_5D4594[2487904];
//   *(_DWORD *)&byte_5D4594[4 * *(_DWORD *)&byte_5D4594[2487904] + 2487844] = v13;
//   *(_DWORD *)&byte_5D4594[2487904] = v14 + 1;
//   if ( v9 > 1 )
//   {
//     *(_DWORD *)&byte_5D4594[2487908] = 0;
//     *(float *)&byte_5D4594[2487912] = v41 * v41;
//     v35 = v41 * 0.94999999;
//     sub_517F90((float2 *)(*(_DWORD *)&byte_5D4594[2487844] + 56), v35, sub_52FF10, *(int *)&byte_5D4594[2487844]);
//     if ( *(_DWORD *)&byte_5D4594[2487908] )
//     {
//       v15 = *(_DWORD *)&byte_5D4594[2487904];
//       *(_DWORD *)&byte_5D4594[4 * *(_DWORD *)&byte_5D4594[2487904] + 2487844] = *(_DWORD *)&byte_5D4594[2487908];
//       *(_DWORD *)&byte_5D4594[2487904] = v15 + 1;
//     }
//   }
//   if ( v9 > 2 )
//   {
//     *(_DWORD *)&byte_5D4594[2487908] = 0;
//     *(float *)&byte_5D4594[2487912] = v41 * v41;
//     v36 = v41 * 0.89999998;
//     sub_517F90((float2 *)(*(_DWORD *)&byte_5D4594[2487844] + 56), v36, sub_52FF10, *(int *)&byte_5D4594[2487844]);
//     if ( *(_DWORD *)&byte_5D4594[2487908] )
//     {
//       v16 = *(_DWORD *)&byte_5D4594[2487904];
//       *(_DWORD *)&byte_5D4594[4 * *(_DWORD *)&byte_5D4594[2487904] + 2487844] = *(_DWORD *)&byte_5D4594[2487908];
//       *(_DWORD *)&byte_5D4594[2487904] = v16 + 1;
//     }
//   }
//   if ( *(_DWORD *)&byte_5D4594[2487848] )
//   {
//     if ( v9 > 3 )
//     {
//       *(_DWORD *)&byte_5D4594[2487908] = 0;
//       *(float *)&byte_5D4594[2487912] = v41 * v41;
//       v37 = v41 * 0.85000002;
//       sub_517F90((float2 *)(*(_DWORD *)&byte_5D4594[2487848] + 56), v37, sub_52FF10, *(int *)&byte_5D4594[2487848]);
//       if ( *(_DWORD *)&byte_5D4594[2487908] )
//       {
//         v17 = *(_DWORD *)&byte_5D4594[2487904];
//         *(_DWORD *)&byte_5D4594[4 * *(_DWORD *)&byte_5D4594[2487904] + 2487844] = *(_DWORD *)&byte_5D4594[2487908];
//         *(_DWORD *)&byte_5D4594[2487904] = v17 + 1;
//       }
//     }
//   }
//   if ( *(_DWORD *)&byte_5D4594[2487852] )
//   {
//     if ( v9 > 4 )
//     {
//       *(_DWORD *)&byte_5D4594[2487908] = 0;
//       v40 = v41 * v41;
//       *(float *)&byte_5D4594[2487912] = v40 * v40;
//       v38 = v41 * 0.80000001;
//       sub_517F90((float2 *)(*(_DWORD *)&byte_5D4594[2487852] + 56), v38, sub_52FF10, *(int *)&byte_5D4594[2487852]);
//       if ( *(_DWORD *)&byte_5D4594[2487908] )
//       {
//         v18 = *(_DWORD *)&byte_5D4594[2487904];
//         *(_DWORD *)&byte_5D4594[4 * *(_DWORD *)&byte_5D4594[2487904] + 2487844] = *(_DWORD *)&byte_5D4594[2487908];
//         *(_DWORD *)&byte_5D4594[2487904] = v18 + 1;
//       }
//     }
//   }
//   sub_52FFD0(v1, *(_DWORD *)(v1 + 16), *(int *)&byte_5D4594[2487844]);
//   if ( v9 > 1 && *(_DWORD *)&byte_5D4594[2487848] )
//     sub_52FFD0(v1, *(int *)&byte_5D4594[2487844], *(int *)&byte_5D4594[2487848]);
//   v19 = *(_DWORD *)&byte_5D4594[2487852];
//   if ( v9 > 2 && *(_DWORD *)&byte_5D4594[2487852] )
//   {
//     sub_52FFD0(v1, *(int *)&byte_5D4594[2487844], *(int *)&byte_5D4594[2487852]);
//     v19 = *(_DWORD *)&byte_5D4594[2487852];
//   }
//   if ( v9 > 3 && *(_DWORD *)&byte_5D4594[2487856] )
//   {
//     if ( *(_DWORD *)&byte_5D4594[2487848] )
//     {
//       sub_52FFD0(v1, *(int *)&byte_5D4594[2487848], *(int *)&byte_5D4594[2487856]);
// LABEL_54:
//       v19 = *(_DWORD *)&byte_5D4594[2487852];
//       goto LABEL_55;
//     }
//     if ( v19 )
//     {
//       sub_52FFD0(v1, v19, *(int *)&byte_5D4594[2487856]);
//       goto LABEL_54;
//     }
//   }
// LABEL_55:
//   if ( v9 > 4 )
//   {
//     if ( *(_DWORD *)&byte_5D4594[2487860] )
//     {
//       if ( v19 || (v19 = *(_DWORD *)&byte_5D4594[2487848]) != 0 )
//         sub_52FFD0(v1, v19, *(int *)&byte_5D4594[2487860]);
//     }
//   }
//   if ( !*(_DWORD *)&byte_5D4594[2487844] )
//     return 0;
//   v42 = sub_419D40(&byte_587000[260728]) + *(float *)(v1 + 76);
//   v20 = sub_419A70(v42);
//   *(float *)(v1 + 76) = v42 - (double)v20;
//   v21 = *(_DWORD **)(v1 + 108);
//   for ( j = *(_DWORD **)(v1 + 104); v21; v21 = (_DWORD *)v21[29] )
//   {
//     if ( j )
//     {
//       v23 = j[12];
//       if ( v21[12] != v23 || v21[4] != j[4] )
//       {
//         if ( v23 )
//           sub_4FEF90((int)j, (_DWORD *)j[12]);
//         sub_4FF130((int)v21);
//       }
//       j = (_DWORD *)j[29];
//     }
//     else
//     {
//       sub_4FF130((int)v21);
//     }
//     if ( v20 > 0 )
//       (*(void (__cdecl **)(_DWORD, _DWORD, _DWORD, int, int))(v21[12] + 716))(v21[12], *(_DWORD *)(v1 + 16), 0, v20, 17);
//     v24 = v21[12];
//     if ( *(_DWORD *)(v24 + 16) & 0x8020 )
//       sub_522FF0(129, (float2 *)(v24 + 56));
//   }
//   for ( ; j; j = (_DWORD *)j[29] )
//   {
//     if ( j[12] )
//       sub_4FEF90((int)j, (_DWORD *)j[12]);
//   }
//   v25 = *(_DWORD *)(v1 + 16);
//   if ( *(_BYTE *)(v25 + 8) & 4 )
//   {
//     v26 = *(_DWORD *)(v1 + 72);
//     if ( v26 )
//     {
//       v27 = *(_DWORD *)(v26 + 736);
//       v28 = *(_BYTE *)(v27 + 108);
//       if ( !v28 )
//         return 1;
//       v29 = v28 - 1;
//       *(_BYTE *)(v27 + 108) = v29;
//       *(_DWORD *)(v27 + 112) = 100 * v29 / *(unsigned __int8 *)(v27 + 109);
//       v30 = *(_DWORD *)(v1 + 16);
//       if ( v30 && *(_BYTE *)(v30 + 8) & 4 )
//       {
//         v31 = *(_DWORD *)(v30 + 748);
//         sub_4FA020((_DWORD *)v30, 22);
//         sub_4D82B0(
//           *(unsigned __int8 *)(*(_DWORD *)(v31 + 276) + 2064),
//           *(_DWORD **)(v1 + 72),
//           *(_BYTE *)(v27 + 108),
//           *(_BYTE *)(v27 + 109));
//       }
//       if ( !*(_BYTE *)(v27 + 108) )
//         return 1;
//     }
//     else
//     {
//       sub_4FA020((_DWORD *)v25, 10);
//       sub_4EEBF0(*(_DWORD *)(v1 + 16), 1);
//       if ( !sub_4EEC80(*(_DWORD *)(v1 + 16)) )
//         return 1;
//     }
//   }
//   if ( !(*(_DWORD *)&byte_5D4594[2598000] % (*(_DWORD *)&byte_5D4594[2649704] / 3u)) )
//   {
//     sub_501960(78, *(_DWORD *)(v1 + 16), 0, 0);
//     sub_501960(78, *(int *)&byte_5D4594[2487844], 0, 0);
//   }
//   v39 = sub_419D40(&byte_587000[260744]);
//   *(_DWORD *)(v1 + 68) = *(_DWORD *)&byte_5D4594[2598000] + sub_419A70(v39);
//   return 0;
// }
int __cdecl sub_52F8A0__abi_raw(nox_abi_ptrslot_t a1)
{
  int v2; // eax
  int v4; // eax
  int v5; // eax
  int v6; // edi
  int v7; // ecx
  int v8; // edx
  int v9; // ebx
  int v10; // eax
  int v11; // ecx
  int v12; // edi
  int v13; // eax
  int v14; // ecx
  int v15; // ecx
  int v16; // ecx
  int v17; // ecx
  int v18; // ecx
  int v19; // eax
  int v20; // ebx
  _DWORD *v21; // edi
  _DWORD *j; // ebp
  int v23; // eax
  int v24; // eax
  int v25; // ecx
  int v26; // eax
  int v27; // edi
  char v28; // al
  unsigned __int8 v29; // al
  int v30; // eax
  int v31; // ebp
  int i; // edi
  int v33; // eax
  int v34; // edi
  float v35; // [esp+0h] [ebp-20h]
  float v36; // [esp+0h] [ebp-20h]
  float v37; // [esp+0h] [ebp-20h]
  float v38; // [esp+0h] [ebp-20h]
  float v39; // [esp+8h] [ebp-18h]
  float v40; // [esp+1Ch] [ebp-4h]
  float v41; // [esp+24h] [ebp+4h]
  float v42; // [esp+24h] [ebp+4h]

  const int self = NOX_PTR(a1);

  v2 = *(_DWORD *)(self + 16);
  if ( v2 )
  {
    if ( sub_4FF350(v2, 8) )
      return 1;
  }
  else if ( !*(_DWORD *)(self + 20) )
  {
    return 1;
  }

  v41 = sub_419D40(&byte_587000[260712]);

  if ( *(_DWORD *)(self + 20) )
  {
    *(_DWORD *)&byte_5D4594[2487820] = *(_DWORD *)(self + 28);
    *(_DWORD *)&byte_5D4594[2487824] = *(_DWORD *)(self + 32);
    sub_517F90((float2 *)(self + 28), v41, (void *)sub_530020, *(_DWORD *)(self + 16));
    return 1;
  }

  if ( *(_BYTE *)(*(_DWORD *)(self + 16) + 8) & 4 && !sub_4EEC80(*(_DWORD *)(self + 16)) )
    return 1;

  if ( (unsigned int)(*(_DWORD *)&byte_5D4594[2598000] - *(_DWORD *)(self + 60)) > 2 && sub_4E6BD0(*(_DWORD *)(self + 16)) )
    return 1;

  v4 = *(_DWORD *)(self + 16);
  if ( *(_BYTE *)(v4 + 8) & 2 && sub_4FEA70(v4, (float2 *)(self + 28)) )
    return 1;

  v5 = *(_DWORD *)(self + 104);
  if ( v5 )
  {
    do
    {
      v6 = *(_DWORD *)(v5 + 116);
      sub_4FE980(v5);
      v5 = v6;
    }
    while ( v6 );
  }

  v7 = *(_DWORD *)(self + 8);
  *(_DWORD *)(self + 104) = *(_DWORD *)(self + 108);
  *(_DWORD *)(self + 108) = 0;
  *(_DWORD *)&byte_5D4594[2487908] = 0;
  *(_DWORD *)&byte_5D4594[2487904] = 0;
  v8 = *(_DWORD *)(self + 16);
  *(_DWORD *)&byte_5D4594[2487844] = 0;
  v9 = *(_DWORD *)&byte_587000[4 * v7 + 260380];
  *(_DWORD *)&byte_5D4594[2487848] = 0;
  *(_DWORD *)&byte_5D4594[2487900] = v8;
  *(_DWORD *)&byte_5D4594[2487852] = 0;
  *(_DWORD *)&byte_5D4594[2487856] = 0;
  *(_DWORD *)&byte_5D4594[2487860] = 0;

  v10 = *(_DWORD *)(self + 16);
  if ( !(*(_BYTE *)(v10 + 8) & 4)
    || (v11 = *(_DWORD *)(v10 + 748), (v12 = *(_DWORD *)(v11 + 288)) == 0)
    || (!sub_5330C0(v10, *(_DWORD *)(v11 + 288)) || sub_4E6C00(*(_DWORD *)(self + 16), v12) > v41
          ? (v13 = *(_DWORD *)&byte_5D4594[2487908])
          : (v13 = v12, *(_DWORD *)&byte_5D4594[2487908] = v12),
        !v13) )
  {
    *(float *)&byte_5D4594[2487912] = v41 * v41;
    sub_517F90((float2 *)(self + 28), v41, (void *)sub_52FF10, *(_DWORD *)(self + 16));
    v13 = *(_DWORD *)&byte_5D4594[2487908];
    if ( !*(_DWORD *)&byte_5D4594[2487908] )
    {
      for ( i = *(_DWORD *)(self + 104); i; i = *(_DWORD *)(i + 116) )
      {
        if ( *(_DWORD *)(i + 48) )
          sub_4FEF90(i, *(_DWORD **)(i + 48));
      }
      v33 = *(_DWORD *)(self + 104);
      if ( v33 )
      {
        do
        {
          v34 = *(_DWORD *)(v33 + 116);
          sub_4FE980(v33);
          v33 = v34;
        }
        while ( v34 );
      }
      *(_DWORD *)(self + 104) = 0;
      return 0;
    }
  }

  v14 = *(_DWORD *)&byte_5D4594[2487904];
  *(_DWORD *)&byte_5D4594[4 * *(_DWORD *)&byte_5D4594[2487904] + 2487844] = v13;
  *(_DWORD *)&byte_5D4594[2487904] = v14 + 1;

  if ( v9 > 1 )
  {
    *(_DWORD *)&byte_5D4594[2487908] = 0;
    *(float *)&byte_5D4594[2487912] = v41 * v41;
    v35 = v41 * 0.94999999f;
    sub_517F90((float2 *)(*(_DWORD *)&byte_5D4594[2487844] + 56), v35, (void *)sub_52FF10, *(int *)&byte_5D4594[2487844]);
    if ( *(_DWORD *)&byte_5D4594[2487908] )
    {
      v15 = *(_DWORD *)&byte_5D4594[2487904];
      *(_DWORD *)&byte_5D4594[4 * *(_DWORD *)&byte_5D4594[2487904] + 2487844] = *(_DWORD *)&byte_5D4594[2487908];
      *(_DWORD *)&byte_5D4594[2487904] = v15 + 1;
    }
  }

  if ( v9 > 2 )
  {
    *(_DWORD *)&byte_5D4594[2487908] = 0;
    *(float *)&byte_5D4594[2487912] = v41 * v41;
    v36 = v41 * 0.89999998f;
    sub_517F90((float2 *)(*(_DWORD *)&byte_5D4594[2487844] + 56), v36, (void *)sub_52FF10, *(int *)&byte_5D4594[2487844]);
    if ( *(_DWORD *)&byte_5D4594[2487908] )
    {
      v16 = *(_DWORD *)&byte_5D4594[2487904];
      *(_DWORD *)&byte_5D4594[4 * *(_DWORD *)&byte_5D4594[2487904] + 2487844] = *(_DWORD *)&byte_5D4594[2487908];
      *(_DWORD *)&byte_5D4594[2487904] = v16 + 1;
    }
  }

  if ( *(_DWORD *)&byte_5D4594[2487848] )
  {
    if ( v9 > 3 )
    {
      *(_DWORD *)&byte_5D4594[2487908] = 0;
      *(float *)&byte_5D4594[2487912] = v41 * v41;
      v37 = v41 * 0.85000002f;
      sub_517F90((float2 *)(*(_DWORD *)&byte_5D4594[2487848] + 56), v37, (void *)sub_52FF10, *(int *)&byte_5D4594[2487848]);
      if ( *(_DWORD *)&byte_5D4594[2487908] )
      {
        v17 = *(_DWORD *)&byte_5D4594[2487904];
        *(_DWORD *)&byte_5D4594[4 * *(_DWORD *)&byte_5D4594[2487904] + 2487844] = *(_DWORD *)&byte_5D4594[2487908];
        *(_DWORD *)&byte_5D4594[2487904] = v17 + 1;
      }
    }
  }

  if ( *(_DWORD *)&byte_5D4594[2487852] )
  {
    if ( v9 > 4 )
    {
      *(_DWORD *)&byte_5D4594[2487908] = 0;
      v40 = v41 * v41;
      *(float *)&byte_5D4594[2487912] = v40 * v40;
      v38 = v41 * 0.80000001f;
      sub_517F90((float2 *)(*(_DWORD *)&byte_5D4594[2487852] + 56), v38, (void *)sub_52FF10, *(int *)&byte_5D4594[2487852]);
      if ( *(_DWORD *)&byte_5D4594[2487908] )
      {
        v18 = *(_DWORD *)&byte_5D4594[2487904];
        *(_DWORD *)&byte_5D4594[4 * *(_DWORD *)&byte_5D4594[2487904] + 2487844] = *(_DWORD *)&byte_5D4594[2487908];
        *(_DWORD *)&byte_5D4594[2487904] = v18 + 1;
      }
    }
  }

  sub_52FFD0(self, *(_DWORD *)(self + 16), *(int *)&byte_5D4594[2487844]);
  if ( v9 > 1 && *(_DWORD *)&byte_5D4594[2487848] )
    sub_52FFD0(self, *(int *)&byte_5D4594[2487844], *(int *)&byte_5D4594[2487848]);

  v19 = *(_DWORD *)&byte_5D4594[2487852];
  if ( v9 > 2 && *(_DWORD *)&byte_5D4594[2487852] )
  {
    sub_52FFD0(self, *(int *)&byte_5D4594[2487844], *(int *)&byte_5D4594[2487852]);
    v19 = *(_DWORD *)&byte_5D4594[2487852];
  }

  if ( v9 > 3 && *(_DWORD *)&byte_5D4594[2487856] )
  {
    if ( *(_DWORD *)&byte_5D4594[2487848] )
    {
      sub_52FFD0(self, *(int *)&byte_5D4594[2487848], *(int *)&byte_5D4594[2487856]);
LABEL_54:
      v19 = *(_DWORD *)&byte_5D4594[2487852];
      goto LABEL_55;
    }
    if ( v19 )
    {
      sub_52FFD0(self, v19, *(int *)&byte_5D4594[2487856]);
      goto LABEL_54;
    }
  }

LABEL_55:
  if ( v9 > 4 )
  {
    if ( *(_DWORD *)&byte_5D4594[2487860] )
    {
      if ( v19 || (v19 = *(_DWORD *)&byte_5D4594[2487848]) != 0 )
        sub_52FFD0(self, v19, *(int *)&byte_5D4594[2487860]);
    }
  }

  if ( !*(_DWORD *)&byte_5D4594[2487844] )
    return 0;

  v42 = sub_419D40(&byte_587000[260728]) + *(float *)(self + 76);
  v20 = sub_419A70(v42);
  *(float *)(self + 76) = (float)(v42 - (double)v20);

  v21 = *(_DWORD **)(self + 108);
  for ( j = *(_DWORD **)(self + 104); v21; v21 = (_DWORD *)v21[29] )
  {
    if ( j )
    {
      v23 = j[12];
      if ( v21[12] != v23 || v21[4] != j[4] )
      {
        if ( v23 )
          sub_4FEF90((int)j, (_DWORD *)j[12]);
        sub_4FF130((int)v21);
      }
      j = (_DWORD *)j[29];
    }
    else
    {
      sub_4FF130((int)v21);
    }

    if ( v20 > 0 )
      (*(void (__cdecl **)(_DWORD, _DWORD, _DWORD, int, int))(v21[12] + 716))(v21[12], *(_DWORD *)(self + 16), 0, v20, 17);

    v24 = v21[12];
    if ( *(_DWORD *)(v24 + 16) & 0x8020 )
      sub_522FF0(129, (float2 *)(v24 + 56));
  }

  for ( ; j; j = (_DWORD *)j[29] )
  {
    if ( j[12] )
      sub_4FEF90((int)j, (_DWORD *)j[12]);
  }

  v25 = *(_DWORD *)(self + 16);
  if ( *(_BYTE *)(v25 + 8) & 4 )
  {
    v26 = *(_DWORD *)(self + 72);
    if ( v26 )
    {
      v27 = *(_DWORD *)(v26 + 736);
      v28 = *(_BYTE *)(v27 + 108);
      if ( !v28 )
        return 1;
      v29 = (unsigned __int8)(v28 - 1);
      *(_BYTE *)(v27 + 108) = v29;
      *(_DWORD *)(v27 + 112) = 100 * v29 / *(unsigned __int8 *)(v27 + 109);

      v30 = *(_DWORD *)(self + 16);
      if ( v30 && *(_BYTE *)(v30 + 8) & 4 )
      {
        v31 = *(_DWORD *)(v30 + 748);
        sub_4FA020((_DWORD *)v30, 22);
        sub_4D82B0(
          *(unsigned __int8 *)(*(_DWORD *)(v31 + 276) + 2064),
          *(_DWORD **)(self + 72),
          *(_BYTE *)(v27 + 108),
          *(_BYTE *)(v27 + 109));
      }

      if ( !*(_BYTE *)(v27 + 108) )
        return 1;
    }
    else
    {
      sub_4FA020((_DWORD *)v25, 10);
      sub_4EEBF0(*(_DWORD *)(self + 16), 1);
      if ( !sub_4EEC80(*(_DWORD *)(self + 16)) )
        return 1;
    }
  }

  if ( !(*(_DWORD *)&byte_5D4594[2598000] % (*(_DWORD *)&byte_5D4594[2649704] / 3u)) )
  {
    sub_501960(78, *(_DWORD *)(self + 16), 0, 0);
    sub_501960(78, *(int *)&byte_5D4594[2487844], 0, 0);
  }

  v39 = sub_419D40(&byte_587000[260744]);
  *(_DWORD *)(self + 68) = *(_DWORD *)&byte_5D4594[2598000] + sub_419A70(v39);
  return 0;
}

/* ============================================================
 * sub_531E20__abi_raw
 * ============================================================ */
// int __cdecl sub_531E20__abi_raw(float a1)
// {
//   int *v1; // edi
//   int v2; // eax
//   int v3; // eax
//
//   v1 = *(int **)(LODWORD(a1) + 748);
//   v2 = sub_424300(SLODWORD(a1));
//   if ( v2 )
//     sub_501960(*(_DWORD *)(v2 + 20), SLODWORD(a1), 0, 0);
//   sub_502490(v1 + 310, v1[299], SLODWORD(a1));
//   v3 = v1[360];
//   BYTE1(v3) |= 1u;
//   v1[360] = v3;
//   sub_5281E0();
//   sub_5281F0(a1);
//   return sub_534750(SLODWORD(a1));
// }
void __cdecl sub_5281F0__abi_raw(nox_abi_ptrslot_t a1);
int __cdecl sub_531E20__abi_raw(nox_abi_ptrslot_t a1)
{
  int *v1; // edi
  int v2; // eax
  int v3; // eax

  const int self = NOX_PTR(a1);

  v1 = *(int **)(self + 748);
  v2 = sub_424300(self);
  if ( v2 )
    sub_501960(*(_DWORD *)(v2 + 20), self, 0, 0);
  sub_502490(v1 + 310, v1[299], self);
  v3 = v1[360];
  BYTE1(v3) |= 1u;
  v1[360] = v3;
  sub_5281E0();
  sub_5281F0__abi_raw(a1);
  return sub_534750(self);
}

/* ============================================================
 * sub_549220__abi_raw
 * ============================================================ */
// int __cdecl sub_549220__abi_raw(float a1)
// {
//   float2 *v1; // eax
//   double v2; // st7
//   int v4; // [esp-4h] [ebp-4h]
//   float v5; // [esp+4h] [ebp+4h]
//
//   v4 = LODWORD(a1);
//   v1 = (float2 *)(LODWORD(a1) + 56);
//   v2 = *(float *)(*(_DWORD *)(*(_DWORD *)(LODWORD(a1) + 748) + 484) + 112) + *(float *)(LODWORD(a1) + 176);
//   *(_DWORD *)&byte_5D4594[2491556] = 0;
//   v5 = v2 + *(float *)&byte_587000[287328];
//   sub_517F90(v1, v5, (int)sub_549270, v4);
//   return *(_DWORD *)&byte_5D4594[2491556];
// }
int __cdecl sub_549220__abi_raw(nox_abi_ptrslot_t a1)
{
  float2 *v1; // eax
  double v2; // st7
  int v4; // [esp-4h] [ebp-4h]
  float v5; // [esp+4h] [ebp+4h]

  const int self = NOX_PTR(a1);

  v4 = self;
  v1 = (float2 *)(self + 56);
  v2 = *(float *)(*(_DWORD *)(*(_DWORD *)(self + 748) + 484) + 112) + *(float *)(self + 176);
  *(_DWORD *)&byte_5D4594[2491556] = 0;
  v5 = (float)(v2 + *(float *)&byte_587000[287328]);
  sub_517F90(v1, v5, (void *)sub_549270, v4);
  return *(_DWORD *)&byte_5D4594[2491556];
}

/* ============================================================
 * sub_5495B0__abi_raw
 * ============================================================ */
//int __cdecl sub_5495B0__abi_raw(float a1)
//{
//  int v1; // edi
//  int v2; // ebp
//  int v3; // esi
//  float v4; // eax
//  float v5; // edx
//  float v6; // eax
//  int result; // eax
//  float4 v8; // [esp+10h] [ebp-10h]
//  float v9; // [esp+24h] [ebp+4h]
//
//  v1 = LODWORD(a1);
//  v2 = *(_DWORD *)(LODWORD(a1) + 748);
//  v3 = sub_549440(SLODWORD(a1), 0);
//  if ( v3 )
//  {
//    v4 = *(float *)(LODWORD(a1) + 56);
//    v5 = *(float *)(v3 + 56);
//    v8.field_4 = *(float *)(LODWORD(a1) + 60);
//    v8.field_0 = v4;
//    v6 = *(float *)(v3 + 60);
//    v8.field_8 = v5;
//    v8.field_C = v6;
//    result = sub_535250(&v8, 0, 0, 5);
//    if ( !result )
//      return result;
//    (*(void (__cdecl **)(int, _DWORD, _DWORD, _DWORD, _DWORD))(v3 + 716))(
//      v3,
//      LODWORD(a1),
//      LODWORD(a1),
//      *(_DWORD *)(*(_DWORD *)(v2 + 484) + 116),
//      *(_DWORD *)(*(_DWORD *)(v2 + 484) + 124));
//    v9 = *(float *)(*(_DWORD *)(v2 + 484) + 120);
//    if ( v9 > 0.0 )
//      sub_52DF80(v1 + 56, v3, v9);
//    if ( sub_549690(v1, v3) )
//      sub_4DA2C0(v3, (const char *)&byte_587000[287760], 0);
//  }
//  return 1;
//}
int __cdecl sub_5495B0__abi_raw(nox_abi_ptrslot_t a1)
{
  int v2; // ebp
  int v3; // esi
  float v4; // eax
  float v5; // edx
  float v6; // eax
  int result; // eax
  float4 v8; // [esp+10h] [ebp-10h]
  float v9; // [esp+24h] [ebp+4h]

  const int self = NOX_PTR(a1);

  v2 = *(_DWORD *)(self + 748);
  v3 = sub_549440(self, 0);
  if ( v3 )
  {
    v4 = *(float *)(self + 56);
    v5 = *(float *)(v3 + 56);
    v8.field_4 = *(float *)(self + 60);
    v8.field_0 = v4;
    v6 = *(float *)(v3 + 60);
    v8.field_8 = v5;
    v8.field_C = v6;
    result = sub_535250(&v8, 0, 0, 5);
    if ( !result )
      return result;
    (*(void (__cdecl **)(int, _DWORD, _DWORD, _DWORD, _DWORD))(v3 + 716))(
      v3,
      self,
      self,
      *(_DWORD *)(*(_DWORD *)(v2 + 484) + 116),
      *(_DWORD *)(*(_DWORD *)(v2 + 484) + 124));
    v9 = *(float *)(*(_DWORD *)(v2 + 484) + 120);
    if ( v9 > 0.0f )
      sub_52DF80(self + 56, v3, v9);
    if ( sub_549690(self, v3) )
      sub_4DA2C0(v3, (const char *)&byte_587000[287760], 0);
  }
  return 1;
}

/* ============================================================
 * sub_549700__abi_raw
 * ============================================================ */
// int __cdecl sub_549700__abi_raw(float a1)
// {
//   int v1; // edi
//   int v2; // ebp
//   int v3; // esi
//   float v4; // eax
//   float v5; // edx
//   float v6; // eax
//   int result; // eax
//   float4 v8; // [esp+10h] [ebp-10h]
//   float v9; // [esp+24h] [ebp+4h]
//
//   v1 = LODWORD(a1);
//   v2 = *(_DWORD *)(LODWORD(a1) + 748);
//   v3 = sub_549440(SLODWORD(a1), 0);
//   if ( v3 )
//   {
//     v4 = *(float *)(LODWORD(a1) + 56);
//     v5 = *(float *)(v3 + 56);
//     v8.field_4 = *(float *)(LODWORD(a1) + 60);
//     v8.field_0 = v4;
//     v6 = *(float *)(v3 + 60);
//     v8.field_8 = v5;
//     v8.field_C = v6;
//     result = sub_535250(&v8, 0, 0, 5);
//     if ( !result )
//       return result;
//     (*(void (__cdecl **)(int, _DWORD, _DWORD, _DWORD, _DWORD))(v3 + 716))(
//       v3,
//       LODWORD(a1),
//       LODWORD(a1),
//       *(_DWORD *)(*(_DWORD *)(v2 + 484) + 116),
//       *(_DWORD *)(*(_DWORD *)(v2 + 484) + 124));
//     v9 = *(float *)(*(_DWORD *)(v2 + 484) + 120);
//     if ( v9 > 0.0 )
//       sub_52DF80(v1 + 56, v3, v9);
//     if ( sub_549690(v1, v3) )
//       sub_4DA2C0(v3, (const char *)&byte_587000[287836], 0);
//   }
//   return 1;
// }
int __cdecl sub_549700__abi_raw(nox_abi_ptrslot_t a1)
{
  int v2; // ebp
  int v3; // esi
  float v4; // eax
  float v5; // edx
  float v6; // eax
  int result; // eax
  float4 v8; // [esp+10h] [ebp-10h]
  float v9; // [esp+24h] [ebp+4h]

  const int self = NOX_PTR(a1);

  v2 = *(_DWORD *)(self + 748);
  v3 = sub_549440(self, 0);
  if ( v3 )
  {
    v4 = *(float *)(self + 56);
    v5 = *(float *)(v3 + 56);
    v8.field_4 = *(float *)(self + 60);
    v8.field_0 = v4;
    v6 = *(float *)(v3 + 60);
    v8.field_8 = v5;
    v8.field_C = v6;
    result = sub_535250(&v8, 0, 0, 5);
    if ( !result )
      return result;
    (*(void (__cdecl **)(int, _DWORD, _DWORD, _DWORD, _DWORD))(v3 + 716))(
      v3,
      self,
      self,
      *(_DWORD *)(*(_DWORD *)(v2 + 484) + 116),
      *(_DWORD *)(*(_DWORD *)(v2 + 484) + 124));
    v9 = *(float *)(*(_DWORD *)(v2 + 484) + 120);
    if ( v9 > 0.0f )
      sub_52DF80(self + 56, v3, v9);
    if ( sub_549690(self, v3) )
      sub_4DA2C0(v3, (const char *)&byte_587000[287836], 0);
  }
  return 1;
}

/* ============================================================
 * sub_5497E0__abi_raw / sub_549800__abi_raw / sub_549860__abi_raw / sub_549960__abi_raw
 * ============================================================ */
int __cdecl sub_549800__abi_raw(nox_abi_ptrslot_t a1);
// int __cdecl sub_549800__abi_raw(float a1);

//   int __cdecl sub_5497E0__abi_raw(float a1)
//   {
//     *(_DWORD *)&byte_5D4594[2491560] = 0;
//     return sub_549800__abi_raw(a1);
//   }

int __cdecl sub_5497E0__abi_raw(nox_abi_ptrslot_t a1)
{
  *(_DWORD *)&byte_5D4594[2491560] = 0;
  return sub_549800__abi_raw(a1);
}

//int __cdecl sub_549800__abi_raw(float a1)
//{
//  float2 *v1; // esi
//  double v2; // st7
//  int v4; // [esp-4h] [ebp-8h]
//  float v5; // [esp+8h] [ebp+4h]
//
//  v4 = LODWORD(a1);
//  v1 = (float2 *)(LODWORD(a1) + 56);
//  v2 = *(float *)(*(_DWORD *)(*(_DWORD *)(LODWORD(a1) + 748) + 484) + 112) + *(float *)(LODWORD(a1) + 176);
//  *(_DWORD *)&byte_5D4594[2491576] = 0;
//  v5 = v2 + *(float *)&byte_587000[287328];
//  sub_517F90(v1, v5, (int)sub_549860, v4);
//  sub_4D9110(&v1->field_0, 30);
//  return *(_DWORD *)&byte_5D4594[2491576];
//}
void __cdecl sub_549860__abi_raw(int a1, int a2);
int __cdecl sub_549800__abi_raw(nox_abi_ptrslot_t a1)
{
  float2 *v1; // esi
  double v2; // st7
  int v4; // [esp-4h] [ebp-8h]
  float v5; // [esp+8h] [ebp+4h]

  const int self = NOX_PTR(a1);

  v4 = self;
  v1 = (float2 *)(self + 56);
  v2 = *(float *)(*(_DWORD *)(*(_DWORD *)(self + 748) + 484) + 112) + *(float *)(self + 176);
  *(_DWORD *)&byte_5D4594[2491576] = 0;
  v5 = (float)(v2 + *(float *)&byte_587000[287328]);
  sub_517F90(v1, v5, (void *)sub_549860__abi_raw, v4);
  sub_4D9110(&v1->field_0, 30);
  return *(_DWORD *)&byte_5D4594[2491576];
}

//void __cdecl sub_549860__abi_raw(int a1, float a2)
//{
//  int v2; // esi
//  bool v3; // zf
//  float v4; // ecx
//  float v5; // eax
//  float v6; // ecx
//  float4 v7; // [esp+10h] [ebp-10h]
//  int v8; // [esp+28h] [ebp+8h]
//  float v9; // [esp+28h] [ebp+8h]
//
//  v2 = LODWORD(a2);
//  v3 = LODWORD(a2) == a1;
//  v8 = *(_DWORD *)(LODWORD(a2) + 748);
//  if ( !v3 )
//  {
//    if ( sub_4E6E50((float2 *)(v2 + 56), *(__int16 *)(v2 + 124), (float2 *)(a1 + 56)) & 1 )
//    {
//      if ( sub_4E6C00(v2, a1) <= *(float *)(*(_DWORD *)(v8 + 484) + 112) )
//      {
//        v4 = *(float *)(v2 + 56);
//        v5 = *(float *)(a1 + 56);
//        v7.field_4 = *(float *)(v2 + 60);
//        v7.field_0 = v4;
//        v6 = *(float *)(a1 + 60);
//        v7.field_8 = v5;
//        v7.field_C = v6;
//        if ( sub_535250(&v7, 0, 0, 5) )
//        {
//          (*(void (__cdecl **)(int, int, int, _DWORD, _DWORD))(a1 + 716))(
//            a1,
//            v2,
//            v2,
//            *(_DWORD *)(*(_DWORD *)(v8 + 484) + 116),
//            *(_DWORD *)(*(_DWORD *)(v8 + 484) + 124));
//          if ( *(_BYTE *)(a1 + 8) & 6 )
//            *(_DWORD *)&byte_5D4594[2491576] = 1;
//          v9 = *(float *)(*(_DWORD *)(v8 + 484) + 120);
//          if ( v9 > 0.0 )
//            sub_52DF80(v2 + 56, a1, v9);
//        }
//      }
//    }
//  }
//}
void __cdecl sub_549860__abi_raw(int a1, int a2)
{
  int v2; // esi
  bool v3; // zf
  float v4; // ecx
  float v5; // eax
  float v6; // ecx
  float4 v7; // [esp+10h] [ebp-10h]
  int v8; // [esp+28h] [ebp+8h]
  float v9; // [esp+28h] [ebp+8h]

  v2 = a2;
  v3 = (v2 == a1);
  v8 = *(_DWORD *)(v2 + 748); // line 2979
  if ( !v3 )
  {
    if ( sub_4E6E50((float2 *)(v2 + 56), *(__int16 *)(v2 + 124), (float2 *)(a1 + 56)) & 1 )
    {
      if ( sub_4E6C00(v2, a1) <= *(float *)(*(_DWORD *)(v8 + 484) + 112) )
      {
        v4 = *(float *)(v2 + 56);
        v5 = *(float *)(a1 + 56);
        v7.field_4 = *(float *)(v2 + 60);
        v7.field_0 = v4;
        v6 = *(float *)(a1 + 60);
        v7.field_8 = v5;
        v7.field_C = v6;
        if ( sub_535250(&v7, 0, 0, 5) )
        {
          (*(void (__cdecl **)(int, int, int, _DWORD, _DWORD))(a1 + 716))(
            a1,
            v2,
            v2,
            *(_DWORD *)(*(_DWORD *)(v8 + 484) + 116),
            *(_DWORD *)(*(_DWORD *)(v8 + 484) + 124));
          if ( *(_BYTE *)(a1 + 8) & 6 )
            *(_DWORD *)&byte_5D4594[2491576] = 1;
          v9 = *(float *)(*(_DWORD *)(v8 + 484) + 120);
          if ( v9 > 0.0f )
            sub_52DF80(v2 + 56, a1, v9);
        }
      }
    }
  }
}


int __cdecl sub_549960__abi_raw(nox_abi_ptrslot_t a1)
{
  *(_DWORD *)&byte_5D4594[2491560] = 1;
  return sub_549800__abi_raw(a1);
}

/* ============================================================
 * sub_549980__abi_raw
 * ============================================================ */
//int __cdecl sub_549980__abi_raw(float a1)
//{
//  float v1; // edi
//  int v2; // ebp
//  int v3; // esi
//  float v4; // eax
//  float v5; // edx
//  float v6; // eax
//  float4 v8; // [esp+10h] [ebp-10h]
//  float v9; // [esp+24h] [ebp+4h]
//
//  v1 = a1;
//  v2 = *(_DWORD *)(LODWORD(a1) + 748);
//  v3 = sub_549440(SLODWORD(a1), 0);
//  if ( !v3 )
//    return 0;
//  v4 = *(float *)(LODWORD(a1) + 56);
//  v5 = *(float *)(v3 + 56);
//  v8.field_4 = *(float *)(LODWORD(a1) + 60);
//  v8.field_0 = v4;
//  v6 = *(float *)(v3 + 60);
//  v8.field_8 = v5;
//  v8.field_C = v6;
//  if ( !sub_535250(&v8, 0, 0, 5) )
//    return 0;
//  (*(void (__cdecl **)(int, _DWORD, _DWORD, _DWORD, _DWORD))(v3 + 716))(
//    v3,
//    LODWORD(a1),
//    LODWORD(a1),
//    *(_DWORD *)(*(_DWORD *)(v2 + 484) + 116),
//    *(_DWORD *)(*(_DWORD *)(v2 + 484) + 124));
//  if ( sub_549690(SLODWORD(a1), v3) )
//    sub_4DA2C0(v3, (const char *)&byte_587000[287864], 0);
//  v9 = *(float *)(*(_DWORD *)(v2 + 484) + 120);
//  if ( v9 > 0.0 )
//    sub_52DF80(LODWORD(v1) + 56, v3, v9);
//  return 1;
//}
int __cdecl sub_549980__abi_raw(nox_abi_ptrslot_t a1)
{
  int v2; // ebp
  int v3; // esi
  float v4; // eax
  float v5; // edx
  float v6; // eax
  float4 v8; // [esp+10h] [ebp-10h]
  float v9; // [esp+24h] [ebp+4h]

  const int self = NOX_PTR(a1);

  v2 = *(_DWORD *)(self + 748);
  v3 = sub_549440(self, 0);
  if ( !v3 )
    return 0;

  v4 = *(float *)(self + 56);
  v5 = *(float *)(v3 + 56);
  v8.field_4 = *(float *)(self + 60);
  v8.field_0 = v4;
  v6 = *(float *)(v3 + 60);
  v8.field_8 = v5;
  v8.field_C = v6;

  if ( !sub_535250(&v8, 0, 0, 5) )
    return 0;

  (*(void (__cdecl **)(int, _DWORD, _DWORD, _DWORD, _DWORD))(v3 + 716))(
    v3,
    self,
    self,
    *(_DWORD *)(*(_DWORD *)(v2 + 484) + 116),
    *(_DWORD *)(*(_DWORD *)(v2 + 484) + 124));

  if ( sub_549690(self, v3) )
    sub_4DA2C0(v3, (const char *)&byte_587000[287864], 0);

  v9 = *(float *)(*(_DWORD *)(v2 + 484) + 120);
  if ( v9 > 0.0f )
    sub_52DF80(self + 56, v3, v9);

  return 1;
}

/* ============================================================
 * sub_549A60__abi_raw
 * ============================================================ */
//int __cdecl sub_549A60__abi_raw(float a1)
//{
//  int v1; // edi
//  int v2; // ebp
//  int v3; // esi
//  float v4; // eax
//  float v5; // edx
//  float v6; // eax
//  int result; // eax
//  int *v8; // eax
//  int *v9; // ebx
//  int *v10; // eax
//  float4 v11; // [esp+10h] [ebp-10h]
//  float v12; // [esp+24h] [ebp+4h]
//
//  v1 = LODWORD(a1);
//  v2 = *(_DWORD *)(LODWORD(a1) + 748);
//  v3 = sub_549440(SLODWORD(a1), 0);
//  if ( v3 )
//  {
//    v4 = *(float *)(LODWORD(a1) + 56);
//    v5 = *(float *)(v3 + 56);
//    v11.field_4 = *(float *)(LODWORD(a1) + 60);
//    v11.field_0 = v4;
//    v6 = *(float *)(v3 + 60);
//    v11.field_8 = v5;
//    v11.field_C = v6;
//    result = sub_535250(&v11, 0, 0, 5);
//    if ( !result )
//      return result;
//    (*(void (__cdecl **)(int, _DWORD, _DWORD, _DWORD, _DWORD))(v3 + 716))(
//      v3,
//      LODWORD(a1),
//      LODWORD(a1),
//      *(_DWORD *)(*(_DWORD *)(v2 + 484) + 116),
//      *(_DWORD *)(*(_DWORD *)(v2 + 484) + 124));
//    v12 = *(float *)(*(_DWORD *)(v2 + 484) + 120);
//    if ( v12 > 0.0 )
//      sub_52DF80(v1 + 56, v3, v12);
//    sub_4FF380(v3, 5, 2 * *(_WORD *)&byte_5D4594[2649704], 3);
//    v8 = sub_50A260(v1, 25);
//    if ( v8 )
//    {
//      v8[1] = *(_DWORD *)(v3 + 56);
//      v8[2] = *(_DWORD *)(v3 + 60);
//    }
//    v9 = sub_50A260(v1, 41);
//    if ( v9 )
//      v9[1] = *(_DWORD *)&byte_5D4594[2598000]
//            + sub_415FA0(2 * *(_DWORD *)&byte_5D4594[2649704], 4 * *(_DWORD *)&byte_5D4594[2649704]);
//    v10 = sub_50A260(v1, 24);
//    if ( v10 )
//    {
//      v10[1] = *(_DWORD *)(v3 + 56);
//      v10[2] = *(_DWORD *)(v3 + 60);
//      v10[3] = 0;
//    }
//  }
//  return 1;
//}

int __cdecl sub_549A60__abi_raw(nox_abi_ptrslot_t a1)
{
  int v2; // ebp
  int v3; // esi
  float v4; // eax
  float v5; // edx
  float v6; // eax
  int result; // eax
  int *v8; // eax
  int *v9; // ebx
  int *v10; // eax
  float4 v11; // [esp+10h] [ebp-10h]
  float v12; // [esp+24h] [ebp+4h]

  const int self = NOX_PTR(a1);

  v2 = *(_DWORD *)(self + 748);
  v3 = sub_549440(self, 0);
  if ( v3 )
  {
    v4 = *(float *)(self + 56);
    v5 = *(float *)(v3 + 56);
    v11.field_4 = *(float *)(self + 60);
    v11.field_0 = v4;
    v6 = *(float *)(v3 + 60);
    v11.field_8 = v5;
    v11.field_C = v6;
    result = sub_535250(&v11, 0, 0, 5);
    if ( !result )
      return result;

    (*(void (__cdecl **)(int, _DWORD, _DWORD, _DWORD, _DWORD))(v3 + 716))(
      v3,
      self,
      self,
      *(_DWORD *)(*(_DWORD *)(v2 + 484) + 116),
      *(_DWORD *)(*(_DWORD *)(v2 + 484) + 124));

    v12 = *(float *)(*(_DWORD *)(v2 + 484) + 120);
    if ( v12 > 0.0f )
      sub_52DF80(self + 56, v3, v12);

    sub_4FF380(v3, 5, 2 * *(_WORD *)&byte_5D4594[2649704], 3);

    v8 = sub_50A260(self, 25);
    if ( v8 )
    {
      v8[1] = *(_DWORD *)(v3 + 56);
      v8[2] = *(_DWORD *)(v3 + 60);
    }

    v9 = sub_50A260(self, 41);
    if ( v9 )
      v9[1] = *(_DWORD *)&byte_5D4594[2598000]
            + sub_415FA0(2 * *(_DWORD *)&byte_5D4594[2649704], 4 * *(_DWORD *)&byte_5D4594[2649704]);

    v10 = sub_50A260(self, 24);
    if ( v10 )
    {
      v10[1] = *(_DWORD *)(v3 + 56);
      v10[2] = *(_DWORD *)(v3 + 60);
      v10[3] = 0;
    }
  }
  return 1;
}

/* ============================================================
 * sub_549BC0__abi_raw
 * ============================================================ */
//int __cdecl sub_549BC0__abi_raw(float a1)
//{
//  int v1; // edi
//  int v2; // ebp
//  int v3; // esi
//  float v4; // eax
//  float v5; // edx
//  float v6; // eax
//  float4 v8; // [esp+10h] [ebp-10h]
//  float v9; // [esp+24h] [ebp+4h]
//
//  v1 = LODWORD(a1);
//  v2 = *(_DWORD *)(LODWORD(a1) + 748);
//  v3 = sub_549440(SLODWORD(a1), 0);
//  if ( !v3 )
//    return 0;
//  v4 = *(float *)(LODWORD(a1) + 56);
//  v5 = *(float *)(v3 + 56);
//  v8.field_4 = *(float *)(LODWORD(a1) + 60);
//  v8.field_0 = v4;
//  v6 = *(float *)(v3 + 60);
//  v8.field_8 = v5;
//  v8.field_C = v6;
//  if ( !sub_535250(&v8, 0, 0, 5) )
//    return 0;
//  (*(void (__cdecl **)(int, _DWORD, _DWORD, _DWORD, _DWORD))(v3 + 716))(
//    v3,
//    LODWORD(a1),
//    LODWORD(a1),
//    *(_DWORD *)(*(_DWORD *)(v2 + 484) + 116),
//    *(_DWORD *)(*(_DWORD *)(v2 + 484) + 124));
//  v9 = *(float *)(*(_DWORD *)(v2 + 484) + 120);
//  if ( v9 > 0.0 )
//    sub_52DF80(v1 + 56, v3, v9);
//  if ( sub_549690(v1, v3) )
//    sub_4DA2C0(v3, (const char *)&byte_587000[287936], 0);
//  return 1;
//}
int __cdecl sub_549BC0__abi_raw(nox_abi_ptrslot_t a1)
{
  int v2; // ebp
  int v3; // esi
  float v4; // eax
  float v5; // edx
  float v6; // eax
  float4 v8; // [esp+10h] [ebp-10h]
  float v9; // [esp+24h] [ebp+4h]

  const int self = NOX_PTR(a1);

  v2 = *(_DWORD *)(self + 748);
  v3 = sub_549440(self, 0);
  if ( !v3 )
    return 0;

  v4 = *(float *)(self + 56);
  v5 = *(float *)(v3 + 56);
  v8.field_4 = *(float *)(self + 60);
  v8.field_0 = v4;
  v6 = *(float *)(v3 + 60);
  v8.field_8 = v5;
  v8.field_C = v6;

  if ( !sub_535250(&v8, 0, 0, 5) )
    return 0;

  (*(void (__cdecl **)(int, _DWORD, _DWORD, _DWORD, _DWORD))(v3 + 716))(
    v3,
    self,
    self,
    *(_DWORD *)(*(_DWORD *)(v2 + 484) + 116),
    *(_DWORD *)(*(_DWORD *)(v2 + 484) + 124));

  v9 = *(float *)(*(_DWORD *)(v2 + 484) + 120);
  if ( v9 > 0.0f )
    sub_52DF80(self + 56, v3, v9);

  if ( sub_549690(self, v3) )
    sub_4DA2C0(v3, (const char *)&byte_587000[287936], 0);

  return 1;
}

/* ============================================================
 * sub_549CA0__abi_raw
 * ============================================================ */
 //int __cdecl sub_549CA0__abi_raw(float a1)
//   {
//     int v1; // edi
//     int v2; // ebp
//     int v3; // esi
//     float v4; // eax
//     float v5; // edx
//     float v6; // eax
//     float4 v8; // [esp+10h] [ebp-10h]
//     float v9; // [esp+24h] [ebp+4h]
//
//     v1 = LODWORD(a1);
//     v2 = *(_DWORD *)(LODWORD(a1) + 748);
//     v3 = sub_549440(SLODWORD(a1), 0);
//     if ( !v3 )
//       return 0;
//     v4 = *(float *)(LODWORD(a1) + 56);
//     v5 = *(float *)(v3 + 56);
//     v8.field_4 = *(float *)(LODWORD(a1) + 60);
//     v8.field_0 = v4;
//     v6 = *(float *)(v3 + 60);
//     v8.field_8 = v5;
//     v8.field_C = v6;
//     if ( !sub_535250(&v8, 0, 0, 5) )
//       return 0;
//     (*(void (__cdecl **)(int, _DWORD, _DWORD, _DWORD, _DWORD))(v3 + 716))(
//       v3,
//       LODWORD(a1),
//       LODWORD(a1),
//       *(_DWORD *)(*(_DWORD *)(v2 + 484) + 116),
//       *(_DWORD *)(*(_DWORD *)(v2 + 484) + 124));
//     v9 = *(float *)(*(_DWORD *)(v2 + 484) + 120);
//     if ( v9 > 0.0 )
//       sub_52DF80(v1 + 56, v3, v9);
//     if ( sub_549690(v1, v3) )
//       sub_4DA2C0(v3, (const char *)&byte_587000[287956], 0);
//     return 1;
//   }
int __cdecl sub_549CA0__abi_raw(nox_abi_ptrslot_t a1)
{
  int v2; // ebp
  int v3; // esi
  float v4; // eax
  float v5; // edx
  float v6; // eax
  float4 v8; // [esp+10h] [ebp-10h]
  float v9; // [esp+24h] [ebp+4h]

  const int self = NOX_PTR(a1);

  v2 = *(_DWORD *)(self + 748);
  v3 = sub_549440(self, 0);
  if ( !v3 )
    return 0;

  v4 = *(float *)(self + 56);
  v5 = *(float *)(v3 + 56);
  v8.field_4 = *(float *)(self + 60);
  v8.field_0 = v4;
  v6 = *(float *)(v3 + 60);
  v8.field_8 = v5;
  v8.field_C = v6;

  if ( !sub_535250(&v8, 0, 0, 5) )
    return 0;

  (*(void (__cdecl **)(int, _DWORD, _DWORD, _DWORD, _DWORD))(v3 + 716))(
    v3,
    self,
    self,
    *(_DWORD *)(*(_DWORD *)(v2 + 484) + 116),
    *(_DWORD *)(*(_DWORD *)(v2 + 484) + 124));

  v9 = *(float *)(*(_DWORD *)(v2 + 484) + 120);
  if ( v9 > 0.0f )
    sub_52DF80(self + 56, v3, v9);

  if ( sub_549690(self, v3) )
    sub_4DA2C0(v3, (const char *)&byte_587000[287956], 0);

  return 1;
}

//int __cdecl sub_500F40(int a1, float a2)
//{
//  _DWORD *v2; // esi
//  int v3; // eax
//  float v4; // edi
//  float v5; // edx
//  float v6; // ecx
//  double v7; // st7
//  double v8; // st6
//  long double v9; // st6
//  float v10; // edx
//  int result; // eax
//  int v12; // esi
//  int v13; // esi
//  float v14; // edx
//  float v15; // eax
//  float v16; // ecx
//  int v17; // eax
//  int v18; // edx
//  float4 v19; // [esp+8h] [ebp-10h]
//
//  v2 = (_DWORD *)a1;
//  if ( *(float *)&a1 == 0.0 )
//    return 0;
//  v3 = *(_DWORD *)(a1 + 16);
//  if ( !v3 )
//    return 0;
//  v4 = a2;
//  if ( a2 == 0.0 )
//    return 0;
//  if ( *(_BYTE *)(v3 + 8) & 4 )
//  {
//    v19.field_0 = *(float *)(v3 + 56);
//    v5 = *(float *)(v3 + 60);
//    v6 = *(float *)(a1 + 56);
//    v19.field_8 = *(float *)(a1 + 52);
//    v19.field_C = v6;
//    v7 = v19.field_8 - v19.field_0;
//    v19.field_4 = v5;
//    v8 = v6 - v5;
//    *(float *)&a1 = v8;
//    v9 = sqrt(v8 * *(float *)&a1 + v7 * v7);
//    a2 = v9;
//    if ( v9 > 50.0 )
//    {
//      v19.field_8 = v7 * 50.0 / a2 + v19.field_0;
//      v19.field_C = *(float *)&a1 * 50.0 / a2 + v19.field_4;
//    }
//    if ( sub_535250(&v19, 0, 0, 9) && !sub_411A90((float2 *)&v19.field_8) )
//    {
//      v10 = v19.field_C;
//      *(_DWORD *)LODWORD(v4) = LODWORD(v19.field_8);
//      *(float *)(LODWORD(v4) + 4) = v10;
//      return 1;
//    }
//    v12 = v2[4];
//    if ( *(_BYTE *)(v12 + 8) & 4 )
//    {
//      v13 = *(_DWORD *)(v12 + 748);
//      a1 = 2;
//      sub_4DA0F0(*(unsigned __int8 *)(*(_DWORD *)(v13 + 276) + 2064), 0, &a1);
//    }
//    return 0;
//  }
//  v19.field_0 = *(float *)(v3 + 56);
//  v14 = *(float *)(v3 + 60);
//  v15 = *(float *)(a1 + 52);
//  v16 = *(float *)(a1 + 56);
//  v19.field_4 = v14;
//  v19.field_8 = v15;
//  v19.field_C = v16;
//  if ( sub_535250(&v19, 0, 0, 9) )
//  {
//    *(_DWORD *)LODWORD(v4) = v2[13]; // this is line 9045
//    *(_DWORD *)(LODWORD(v4) + 4) = v2[14];
//    result = 1;
//  }
//  else
//  {
//    v17 = v2[4];
//    *(_DWORD *)LODWORD(v4) = *(_DWORD *)(v17 + 56);
//    v18 = *(_DWORD *)(v17 + 60);
//    result = 1;
//    *(_DWORD *)(LODWORD(v4) + 4) = v18;
//  }
//  return result;
//}
int __cdecl sub_500F40__abi_raw(
#if defined(__arm__) && defined(__ARM_PCS_VFP)
    nox_abi_ptrslot_t a1,
    nox_abi_ptrslot_t a2
#else
    int a1,
    float a2
#endif
)
{
#if defined(__arm__) && defined(__ARM_PCS_VFP)
  /* ---- ARM hard-float path: your current ABI-fixed version ---- */

  _DWORD *v2; // esi
  int v3; // eax
  float v5; // edx
  float v6; // ecx
  double v7; // st7
  double v8; // st6
  long double v9; // st6
  float v10; // edx
  int v12; // esi
  int v13; // esi
  float v14; // edx
  float v15; // eax
  float v16; // ecx
  int v17; // eax
  int v18; // edx
  float4 v19; // [esp+8h] [ebp-10h]

  const int self = NOX_PTR(a1);

  /* a2 is pointer to out float2 (x,y) */
  uint32_t *out = (uint32_t *)(uintptr_t)nox_from_ptrslot(a2);

  v2 = (_DWORD *)self;

  /* Original:
     if (*(float *)&a1 == 0.0) return 0;
     if (a2 == 0.0) return 0;
   */
  if (!self)
    return 0;
  if (!out)
    return 0;

  v3 = *(_DWORD *)(self + 16);
  if (!v3)
    return 0;

  if (*(_BYTE *)(v3 + 8) & 4)
  {
    v19.field_0 = *(float *)(v3 + 56);
    v5         = *(float *)(v3 + 60);
    v6         = *(float *)(self + 56);
    v19.field_8 = *(float *)(self + 52);
    v19.field_C = v6;
    v7          = (double)(v19.field_8 - v19.field_0);
    v19.field_4 = v5;
    v8          = (double)(v6 - v5);

    /* original did: *(float *)&a1 = v8; sqrt(v8 * *(float *)&a1 + v7*v7) */
    v9 = sqrt(v8 * v8 + v7 * v7);

    /* original clamps to 50.0 using a2 as the distance */
    if (v9 > 50.0)
    {
      const double inv = 50.0 / (double)v9;
      v19.field_8 = (float)(v7 * inv + (double)v19.field_0);
      v19.field_C = (float)(v8 * inv + (double)v19.field_4);
    }

    if (sub_535250(&v19, 0, 0, 9) && !sub_411A90((float2 *)&v19.field_8))
    {
      /* original:
           *(_DWORD *)LODWORD(v4) = LODWORD(v19.field_8);
           *(float *)(LODWORD(v4) + 4) = v19.field_C;
         i.e., write raw float bits for x, and float for y.
         We write raw bits for both (safe + equivalent for consumers expecting float storage).
      */
      v10 = v19.field_C;

      uint32_t xbits, ybits;
      memcpy(&xbits, &v19.field_8, sizeof(xbits));
      memcpy(&ybits, &v10,        sizeof(ybits));

      out[0] = xbits;
      out[1] = ybits;
      return 1;
    }

    v12 = v2[4];
    if (*(_BYTE *)(v12 + 8) & 4)
    {
      v13 = *(_DWORD *)(v12 + 748);
      int tmp = 2; /* original: a1 = 2; sub_4DA0F0(...,&a1); */
      sub_4DA0F0(*(unsigned __int8 *)(*(_DWORD *)(v13 + 276) + 2064), 0, &tmp);
    }
    return 0;
  }

  /* else branch (no flag 4) */
  v19.field_0 = *(float *)(v3 + 56);
  v14         = *(float *)(v3 + 60);
  v15         = *(float *)(self + 52);
  v16         = *(float *)(self + 56);
  v19.field_4 = v14;
  v19.field_8 = v15;
  v19.field_C = v16;

  if (sub_535250(&v19, 0, 0, 9))
  {
    /* original “line 9045”: write v2[13], v2[14] */
    out[0] = (uint32_t)v2[13];
    out[1] = (uint32_t)v2[14];
    return 1;
  }
  else
  {
    v17   = v2[4];
    out[0] = *(uint32_t *)(v17 + 56);
    out[1] = *(uint32_t *)(v17 + 60); /* FIX: store raw bits exactly like original */
    (void)v18;                        /* keep decls stable/minimal-diff */
    return 1;
  }

#else
  /* ---- Non-ARM (i386 etc.): original x86 body, unchanged ---- */

  _DWORD *v2; // esi
  int v3; // eax
  float v4; // edi
  float v5; // edx
  float v6; // ecx
  double v7; // st7
  double v8; // st6
  long double v9; // st6
  float v10; // edx
  int result; // eax
  int v12; // esi
  int v13; // esi
  float v14; // edx
  float v15; // eax
  float v16; // ecx
  int v17; // eax
  int v18; // edx
  float4 v19; // [esp+8h] [ebp-10h]

  v2 = (_DWORD *)a1;
  if ( *(float *)&a1 == 0.0 )
    return 0;
  v3 = *(_DWORD *)(a1 + 16);
  if ( !v3 )
    return 0;
  v4 = a2;
  if ( a2 == 0.0 )
    return 0;
  if ( *(_BYTE *)(v3 + 8) & 4 )
  {
    v19.field_0 = *(float *)(v3 + 56);
    v5 = *(float *)(v3 + 60);
    v6 = *(float *)(a1 + 56);
    v19.field_8 = *(float *)(a1 + 52);
    v19.field_C = v6;
    v7 = v19.field_8 - v19.field_0;
    v19.field_4 = v5;
    v8 = v6 - v5;
    *(float *)&a1 = v8;
    v9 = sqrt(v8 * *(float *)&a1 + v7 * v7);
    a2 = v9;
    if ( v9 > 50.0 )
    {
      v19.field_8 = v7 * 50.0 / a2 + v19.field_0;
      v19.field_C = *(float *)&a1 * 50.0 / a2 + v19.field_4;
    }
    if ( sub_535250(&v19, 0, 0, 9) && !sub_411A90((float2 *)&v19.field_8) )
    {
      v10 = v19.field_C;
      *(_DWORD *)LODWORD(v4) = LODWORD(v19.field_8);
      *(float *)(LODWORD(v4) + 4) = v10;
      return 1;
    }
    v12 = v2[4];
    if ( *(_BYTE *)(v12 + 8) & 4 )
    {
      v13 = *(_DWORD *)(v12 + 748);
      a1 = 2;
      sub_4DA0F0(*(unsigned __int8 *)(*(_DWORD *)(v13 + 276) + 2064), 0, &a1);
    }
    return 0;
  }
  v19.field_0 = *(float *)(v3 + 56);
  v14 = *(float *)(v3 + 60);
  v15 = *(float *)(a1 + 52);
  v16 = *(float *)(a1 + 56);
  v19.field_4 = v14;
  v19.field_8 = v15;
  v19.field_C = v16;
  if ( sub_535250(&v19, 0, 0, 9) )
  {
    *(_DWORD *)LODWORD(v4) = v2[13]; // line 3695
    *(_DWORD *)(LODWORD(v4) + 4) = v2[14];
    result = 1;
  }
  else
  {
    v17 = v2[4];
    *(_DWORD *)LODWORD(v4) = *(_DWORD *)(v17 + 56);
    v18 = *(_DWORD *)(v17 + 60);
    result = 1;
    *(_DWORD *)(LODWORD(v4) + 4) = v18;
  }
  return result;

#endif
}


//----- (0040F120) --------------------------------------------------------
// seg fault archer in wiz campaign sequence
//unsigned __int8 *__cdecl sub_40F120(int a1, _DWORD *a2)
//{
//  int ***v2; // ebp
//  int v3; // ebx
//  int **v4; // eax
//  unsigned int v5; // edx
//  unsigned __int8 *v6; // edi
//  unsigned __int8 *result; // eax
//
//  v2 = *(int ****)&byte_5D4594[4 * a1 + 210292];
//  v3 = 0;
//  memset(&byte_5D4594[207988], 0, 0x800u);
//  v4 = sub_420A90(v2, &a1);
//  if ( v4 )
//  {
//    v5 = a1;
//    while ( 1 )
//    {
//      qmemcpy(&byte_5D4594[v3 + 207988], v4, 4 * (v5 >> 2));
//      v6 = &byte_5D4594[4 * (v5 >> 2) + 207988 + v3];
//      v3 += v5;
//      qmemcpy(v6, &v4[v5 >> 2], v5 & 3);
//      v4 = sub_420A90(v2, &a1);
//      if ( !v4 )
//        break;
//      v5 = a1;
//      if ( (unsigned int)(a1 + v3) > 0x800 )
//      {
//        sub_420940((int)v2, (int)v4, a1, 0);
//        result = &byte_5D4594[207988];
//        *a2 = v3;
//        return result;
//      }
//    }
//    *a2 = v3;
//    result = &byte_5D4594[207988];
//  }
//  else
//  {
//    *a2 = 0;
//    result = &byte_5D4594[207988];
//  }
//  return result;
//}
//unsigned __int8 *__cdecl sub_40F120(int a1, _DWORD *a2)
//{
//    int ***v2 = *(int ****)&byte_5D4594[4 * a1 + 210292];
//    int v3 = 0;
//    unsigned __int8 *dst = &byte_5D4594[207988];
//
//    memset(dst, 0, 0x800u);
//
//    int **v4 = sub_420A90(v2, &a1);
//    if (!v4) {
//        *a2 = 0;
//        return dst;
//    }
//
//    unsigned int v5 = (unsigned)a1;
//
//    for (;;)
//    {
//        // Safety: prevent current chunk from overflowing (original assumes it won't)
//        if ((unsigned int)(v3 + (int)v5) > 0x800u) {
//            sub_420940((int)v2, (int)v4, (int)v5, 0);
//            *a2 = v3;
//            return dst;
//        }
//
//        memcpy(dst + v3, (const unsigned __int8 *)v4, v5);
//        v3 += (int)v5;
//
//        // Fetch next chunk (this is what the original does next)
//        v4 = sub_420A90(v2, &a1);
//        if (!v4) {
//            *a2 = v3;
//            return dst;
//        }
//
//        v5 = (unsigned)a1;
//
//        // Original "lookahead" overflow check happens here (on the *next* chunk)
//        if ((unsigned int)(v3 + (int)v5) > 0x800u) {
//            sub_420940((int)v2, (int)v4, (int)v5, 0);
//            *a2 = v3;
//            return dst;
//        }
//    }
//}

// Drop-in, semantics-preserving (as close as possible) replacement.
//
// Goal:
// - Keep the original "lookahead" overflow behavior (overflow check applies to the *next* chunk).
// - Prevent segfaults if a chunk would overflow the 0x800 scratch buffer.
// - If the *current* chunk itself is too large to fit, copy what we can (to preserve prefix semantics),
//   then consume/advance the remainder via sub_420940 and return.
//
// Notes:
// - byte_5D4594 layout/offsets are kept identical to the original decomp.
// - a1 is used as both: (1) incoming table index, and (2) out-param for chunk size from sub_420A90.
//
// Signature must match callers.
unsigned __int8 *__cdecl sub_40F120(int a1, _DWORD *a2)
{
    int ***v2 = *(int ****)&byte_5D4594[4 * a1 + 210292];
    int v3 = 0;
    unsigned __int8 *dst = &byte_5D4594[207988];

    // Original clears the whole 0x800 scratch area every call.
    memset(dst, 0, 0x800u);

    // sub_420A90 returns a pointer to a chunk and writes chunk length back into a1.
    int **v4 = sub_420A90(v2, &a1);
    if (!v4) {
        *a2 = 0;
        return dst;
    }

    unsigned int v5 = (unsigned int)a1;

    for (;;)
    {
        // --- SAFETY: If THIS chunk won't fit, do a bounded copy (preserve prefix),
        //             then consume/advance the chunk the same way overflow handling does.
        unsigned int remaining = 0x800u - (unsigned int)v3;
        if (v5 > remaining)
        {
            if (remaining)
            {
                memcpy(dst + v3, (const unsigned __int8 *)v4, remaining);
                v3 += (int)remaining;
            }

            // Consume/advance the oversized chunk we couldn't fully copy.
            sub_420940((int)v2, (int)v4, (int)v5, 0);

            *a2 = v3;
            return dst;
        }

        // Normal copy (safe: v5 <= remaining).
        memcpy(dst + v3, (const unsigned __int8 *)v4, v5);
        v3 += (int)v5;

        // Fetch next chunk (original behavior).
        v4 = sub_420A90(v2, &a1);
        if (!v4) {
            *a2 = v3;
            return dst;
        }

        v5 = (unsigned int)a1;

        // Original lookahead overflow check happens here (on the *next* chunk).
        // If it won't fit, we do NOT copy it; we consume/advance it and return what we already built.
        if ((unsigned int)(v3 + (int)v5) > 0x800u)
        {
            sub_420940((int)v2, (int)v4, (int)v5, 0);
            *a2 = v3;
            return dst;
        }
    }
}
