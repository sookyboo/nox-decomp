# Nox Reloaded / Panic EUD MemoryHack compatibility

## Status

Post-Tier-5 recovery/string compatibility checkpoint: 2026-09-16.

Tier 1 implements checked BYTE/WORD/DWORD access to the preserved original Nox data image, direct `SetMemory`/`GetMemory` builtin handling, and semantic replacement of the four known non-DWORD helper targets.

Tier 2 adds portable compatibility-pointer tokens for the high-confidence live object/player paths used by Panic EUD, bounded script-runtime traversal needed to locate EUD local buffers, and semantic replacement of the known Panic v171 `UnitToPtr` helper.

Tier 3 recognizes exact Panic helper bytecode/signatures and replaces a high-confidence set of native-call shims with direct calls to the reconstructed engine. It covers spell lookup, object creation/allocation, removal of TreadLightly state, monster-action pushes, wall lookup/create/breakable registration, NPC equip/unequip and PlaySoundAround.

Tier 4 recognizes Panic's five common per-object callback thunks (`OnCollide`, `OnPickup`, `OnDiscard`, `OnDeath`, and `OnUseItem`) and replaces them with portable C handlers that call the existing NoxScript VM. Callback ids are held in compatibility shadow state, so no injected x86 is stored or executed. The original handler-dispatch points remain authoritative, preserving the fact that these EUD callbacks replace object handlers rather than acting as post-event notifications. Tier 4 also supports Panic's one-shot item-pickup callback field.

Tier 4.5 recognizes the fully patched generated `SetUnitCallbackOnDiscardBypass` thunk and replaces it with a portable chain: invoke the requested NoxScript callback and then invoke the exact previous discard handler. Previous native handlers are represented to EUD code by opaque compatibility tokens rather than executable host addresses.

Tier 5 adds a bounded portable EUD heap (`MemAlloc`/`MemFree`), semantic `DwordCopy`, SmartMemory cleanup interception, typed ThingDB/GameData traversal, safe translation of selected pointer-valued database fields, and the specific object-extension/update-function translation required by Panic's `MagicMissile`. EUD-owned heap token slots are quarantined until script reset so stale pointers cannot silently alias a later allocation.

The post-Tier-5 compatibility step adds an exact semantic replacement for Panic's `BindImpl`, replays the two known `recovery.h` destructor/list formats during EUD reset, and translates SpellDB/AbilityDB name/description pointers to bounded wide-string or heap tokens. The recovery implementation preserves Panic's distinction between the guarded normal helper and the unconditional map-changed helper without installing either generated x86 destructor.

The compatibility layer is opt-in via `USE_EUD_COMPAT`; the default build does not compile or use it. The focused memory test is registered for opt-in Linux and Windows test-runtime configurations and now covers recovery-list replay plus heap-backed SpellDB/AbilityDB string-pointer restoration.

Repository workflow and review requirements are defined by the root `AGENTS.md` and `CONTRIBUTING.md`; this document records the EUD subsystem-specific contract and limitations.

## Goal

Support Reloaded-era third-party maps that depend on Panic's EUD/MemoryHack extensions without executing arbitrary injected x86 code and without tying map compatibility to one host architecture.

The target is **Panic EUD API/behaviour compatibility**, not unrestricted compatibility with arbitrary writes into the current process address space.

## What has been established

### Panic EUD is more than a simple memory poke API

The original EUD libraries build several layers on top of known addresses in the 32-bit Nox executable:

1. raw DWORD/WORD/BYTE memory reads and writes;
2. pointer arithmetic over original Nox object/player structures;
3. temporary replacement of NoxScript/native function-table entries to reach internal engine operations;
4. installation of native x86 helper code and callback thunks.

Consequently, implementing only `GetMemory` and `SetMemory` is not enough for full compatibility. Real EUD maps can depend on event callbacks and redirected engine calls.

### The decomp still preserves the original global data-image layout

The current source declares three contiguous backing arrays:

```text
byte_581450  base 0x581450, size   23,472, ends at 0x587000
byte_587000  base 0x587000, size  316,820, ends at 0x5D4594
byte_5D4594  base 0x5D4594, size 3,844,309, ends at 0x97EE69
```

Together they represent every byte from legacy address `0x581450` through `0x97EE68`.

This is important because many Panic EUD absolute addresses fall directly into these arrays and can be translated deterministically without dereferencing those historical virtual addresses in the host process.

Examples:

| Legacy address | Current backing location |
| --- | --- |
| `0x5C308C` | `byte_587000[245900]` |
| `0x5C3204` | `byte_587000[246276]` |
| `0x5C336C` | `byte_587000[246636]` |
| `0x5C33E8` | `byte_587000[246760]` |
| `0x62F9E0` | `byte_5D4594[373836]` |
| `0x6552D8` | `byte_5D4594[527684]` |
| `0x750E5C` | `byte_5D4594[1558728]` |
| `0x753600` | `byte_5D4594[1568876]` |
| `0x75AE28` | `byte_5D4594[1599636]` |
| `0x979744` | `byte_5D4594[3822000]` |

This gives the first compatibility patch a high-confidence path for ordinary legacy data accesses.

It does **not** make it safe to execute values stored in those arrays as host function pointers. Historical function pointers and injected machine code still require semantic replacement.

Panic also deliberately abuses the native builtin table as an address calculation. NoxScript opcode `69` reaches `sub_508B70`, which treats the encoded builtin id as a scale-4 index from the original table base `0x5C308C`. Panic-generated code can therefore use synthetic ids far outside the real `0..210` builtin range to read an indirect call target from other legacy VM/EUD memory. The arithmetic is x86 32-bit effective-address arithmetic and wraps modulo `2^32`; it must not be widened to a checked 64-bit array index.

For example, builtin id `973229` resolves to legacy address `0x979740`. That address is also the base of the NoxScript value stack used by `sub_507230`/`script_pop` (`byte_5D4594[3821996]`), so this is a real indirect-call technique rather than malformed bytecode. In the decomp, directly evaluating `byte_587000[245900 + 4 * id]` crosses the C object boundary and is undefined/out-of-bounds even though the corresponding original 32-bit address is valid. EUD-enabled builtin lookup must instead resolve the wrapped legacy address through the compatibility memory layer.

`sub_508C30` and `sub_508C70` classify the selected builtin target against two native-target lists before `sub_508B70` saves/restores the current script string/caller context. They must use the same mapped lookup for synthetic ids; otherwise those classification checks reintroduce the out-of-bounds table access. After lookup, known Panic helpers are handled by semantic dispatch. The normal host-function fallback is permitted only when the wrapped effective address is one of the real 211 builtin-table slots; synthetic indirect slots are never executed as arbitrary host code.

### Original player/object offset chains also survive

Panic EUD obtains player-related state through an object-relative chain equivalent to:

```c
extension = *(void **)(object + 0x2EC);
playerInfo = *(void **)((char *)extension + 0x114);
```

The current decompiled source repeatedly contains the same offsets:

```text
object + 748   == object + 0x2EC
nested + 276  == nested + 0x114
```

For example, `src/GAME3.c` contains many accesses through `object + 748`, followed by accesses through the nested `+276` pointer.

That provides strong evidence that semantic EUD accessors can be mapped onto the current runtime without inventing a new object model.

### Representative Panic EUD addresses/features identified

| Legacy address / offset | EUD use | Compatibility direction |
| --- | --- | --- |
| `0x5C308C` | NoxScript/native builtin table used for temporary replacement | Recognize known operations or route to native C helpers; do not invoke historical raw code pointers |
| `0x5C3204` | Function-pointer slot used by spell lookup helpers | Semantic spell lookup wrapper |
| `0x5C336C` | Function-pointer slot used by EUD utility code | Semantic wrapper |
| `0x5C33E8` | Slots used for non-DWORD memory helper installation | Native C BYTE/WORD compatibility operations |
| `0x62F9E0` | Player-table base | Legacy data mapping / player accessor |
| `0x6552D8` | Game-data database base | Legacy data mapping / game-data accessor |
| `0x753600` | Warrior ability cooldown data | Legacy data mapping initially; named accessor later |
| `0x75AE28` | Script runtime/function metadata | Dynamic script-call compatibility through the VM rather than raw code execution |
| `0x750E5C` | Storage used for injected EUD helper code | Never execute supplied x86; identify known installation patterns if needed |
| `0x979744` | EUD core loader/injection target | Same: semantic compatibility rather than x86 execution |
| object `+0x2EC` | Object extension/player-related pointer | Existing layout is still visible in nox-decomp |
| extension `+0x114` | Nested player-info pointer | Existing layout is still visible in nox-decomp |

### Real maps use the advanced parts

Panic EUD maps are not merely theoretical consumers of the memory library. Representative map source uses EUD helpers for operations including:

- direct player/client state access;
- object and item manipulation;
- collision callbacks;
- pickup callbacks;
- missile callbacks;
- internal engine operations that are not part of ordinary NoxScript.

Therefore callback and native-operation compatibility will eventually be required for broad map compatibility.

## High-confidence first patch boundary

The first implementation should concentrate on operations that can be reproduced without executing untrusted/architecture-specific code.

### 1. Legacy data-address translation

Add one helper that resolves an original Nox data address into the corresponding decompiled backing array when the full requested range is within:

```text
0x581450 .. 0x97EE68
```

The resolver should split the range across the same boundaries represented by `byte_581450`, `byte_587000`, and `byte_5D4594`.

The helper should perform bounds/width checks and return failure for addresses outside known mapped data.

### 2. DWORD, WORD and BYTE accessors

Build explicit little-endian 32-bit, 16-bit and 8-bit read/write helpers on top of the resolver.

These helpers should use byte copies/assembly rather than relying on unaligned host pointer casts. This keeps behaviour well-defined on ARM and other targets already supported by nox-decomp.

### 3. EUD bootstrap recognition where confidence is high

Panic EUD installs helper code/function-table entries as part of its bootstrap. The compatibility layer should allow the known bootstrap state/data writes needed for the script to proceed, while ensuring that historical x86 code addresses are never called as native host functions.

Only well-understood bootstrap cases should be handled in the first patch. Unknown execution attempts remain unsupported rather than guessed.

### 4. Preserve existing engine state as the source of truth

Because the decomp's backing arrays and original structure offsets are already actively used by the engine, compatible reads/writes should update those existing locations rather than maintaining a shadow EUD memory image.

This is preferable to a second state store because normal game code will immediately observe compatible EUD changes.

## Features requiring semantic compatibility rather than raw execution

These features must not be implemented by executing original EUD machine code:

- arbitrary x86 injection;
- arbitrary historical function-pointer invocation;
- dynamically generated callback chains other than the understood `SetUnitCallbackOnDiscardBypass` form;
- copied/re-written melee, potion-pickup and other native-code implementations not yet mapped semantically;
- arbitrary script callback invocation through raw script-runtime structures;
- unknown executable patches to renderer, UI, networking, or other client code.

Tier 4 now handles the five common object-handler replacements through a bounded callback registry and direct calls into the current NoxScript VM. More complex generated thunks remain deferred.

## Recommended compatibility tiers

| Tier | Examples | Confidence / difficulty | Implementation model |
| --- | --- | --- | --- |
| 1 - mapped data | existing globals, cooldown arrays, known tables, BYTE/WORD/DWORD accesses | High / low | Translate legacy address into existing backing arrays |
| 2 - object/player state | mana, health, level, strength, speed, weight, flags | High / low-medium | Named accessors using surviving structure layout |
| 3 - engine operations | spell lookup, object creation, monster actions, walls, NPC equipment, sound | High-medium / medium | Exact helper-signature recognition plus direct reconstructed C calls |
| 4 - callbacks | collide, pickup, discard, death, use-item | Medium-high / medium-high | Exact thunk recognition, handler replacement and NoxScript VM dispatch |
| 4.5 - callback chaining | `SetUnitCallbackOnDiscardBypass` | High for verified generated thunk | Semantic callback + previous-handler chain; opaque native-handler token |
| 5 - portable allocation/data helpers | MemAlloc/MemFree, DwordCopy, ThingDB/GameData, MagicMissile | Medium-high / medium | Bounded heap/tokens, typed field access and explicit pointer/function translation |
| 5.5 - recovery/string/runtime helpers | Bind, `recovery.h`, SpellDB/AbilityDB strings | High-medium / medium | Exact helper matching, semantic VM dispatch, recovery-list replay and typed wide-string pointers |
| Later - arbitrary native patches | melee/potion copied code, unknown injected x86 | Low as generic compatibility | Implement known behaviour individually; reject unknown code |

## Safety and portability rules

The compatibility work should preserve these invariants:

- never treat a legacy Nox virtual address as a real host pointer merely because the build is 32-bit;
- never execute EUD-provided x86 bytes;
- never call a raw historical function address;
- use explicit-width integer types for compatibility memory values;
- make unaligned BYTE/WORD/DWORD accesses safe on ARM;
- reject unknown/out-of-range memory operations deterministically;
- avoid debug logging in normal gameplay paths;
- prefer the existing decompiled backing storage over a duplicate compatibility-state copy.

## Focused regression coverage

When the feature is enabled, `eud_compat_memory_test` covers the mapped-memory contract. The broader compatibility surface should additionally cover:

1. address translation at the beginning/end of each of the three data arrays;
2. rejection immediately below `0x581450` and above `0x97EE68`;
3. BYTE reads/writes;
4. aligned and unaligned WORD reads/writes;
5. aligned and unaligned DWORD reads/writes;
6. cross-boundary access at `0x586FFF/0x587000` and `0x5D4593/0x5D4594` if the chosen resolver supports a multi-array width;
7. little-endian value behaviour independent of host alignment;
8. protection against integer overflow in `address + width` validation;
9. a known Panic EUD legacy data address resolving to the expected backing array offset;
10. verification that executable EUD callback/function-pointer behaviour is not accidentally invoked by the raw memory layer;
11. exact Panic recovery helpers restoring their linked-list targets at reset, including the normal-helper guard and map-changed unconditional path;
12. SpellDB/AbilityDB name/description fields round-tripping heap-backed UTF-16 pointers as compatibility tokens and restoring the original engine pointer at reset.

## Implementation sequence

The first three stages are now represented in source:

1. Tier 1: legacy data memory and BYTE/WORD/DWORD helpers.
2. Tier 2: object/player/script-runtime compatibility tokens and `UnitToPtr`.
3. Tier 3: exact-signature semantic wrappers for understood native helper shims.
4. Tier 4: callback registration and NoxScript VM dispatch for collide/pickup/discard/death/use-item handlers.
5. Tier 4.5: semantic chaining for the exact generated DiscardBypass thunk.
6. Tier 5: bounded EUD allocations, safe pointer-field translation, typed ThingDB/GameData access, semantic DwordCopy and the known MagicMissile path.
7. Post-Tier-5: exact semantic Bind, Panic recovery-list lifetime replay, and SpellDB/AbilityDB wide-string pointer translation.
8. Later compatibility: individually understood copied-code gameplay helpers; continue rejecting arbitrary native code.

## Progress summary

Completed investigation:

- identified Panic EUD as the concrete Reloaded-era MemoryHack system to target;
- established that it combines raw memory access, function-table patching and native x86 callbacks;
- identified representative fixed addresses used by its libraries;
- confirmed that all of those representative absolute addresses fall inside nox-decomp's preserved legacy data-image backing arrays;
- confirmed exact continuity of the `0x581450` through `0x97EE68` backing range;
- confirmed survival of the important object `+0x2EC` -> nested `+0x114` player-data layout chain;
- established that real EUD maps use callbacks and other advanced facilities;
- defined a portable first-patch boundary that can provide useful compatibility without debug logs or x86 execution.

Remaining after the current post-Tier-5 checkpoint:

- copied/re-written melee, potion-pickup and AbsolutePickup implementations;
- generic `callmethod`, generic `invokeRawCode`, arbitrary copied native functions, and unknown EUD native/x86 execution (intentionally unsupported);
- additional pointer-bearing database fields only when their semantics are confirmed from real map/library usage;
- map-level compatibility testing by the caller.


## Implementation progress: Tier 1 data memory and non-DWORD helpers

The first compatibility implementation now adds a portable legacy-data memory layer in `src/eud_compat.c` / `src/eud_compat.h`.

Implemented in this patch:

- checked translation of the original `0x581450 .. 0x97EE68` data-image range into the three existing decompiled backing arrays;
- explicit little-endian BYTE, WORD and DWORD reads and writes;
- accesses that cross the `0x587000` or `0x5D4594` backing-array boundaries;
- deterministic rejection of addresses outside the known data image;
- NoxScript builtin-dispatch interception for the four handlers installed by Panic EUD's `UOperatorInitNon4ByteHandler`;
- portable semantic implementations of the injected handlers at legacy addresses `0x750E5C`, `0x750E74`, `0x750E94`, and `0x750EAC`.

The four injected handlers were decoded before implementing them. They correspond to:

| Legacy target | Behaviour |
| --- | --- |
| `0x750E5C` | pop value and address, store low BYTE |
| `0x750E74` | pop address, read BYTE, push zero-extended value |
| `0x750E94` | pop value and address, store low WORD |
| `0x750EAC` | pop address, read WORD, push zero-extended value |

The compatibility dispatcher is called before the normal indirect NoxScript builtin call. Therefore a map may leave the historical target values in the builtin table exactly as Panic EUD expects, but nox-decomp does not jump into the x86 bytes written at those addresses.

This patch intentionally does not make arbitrary host memory readable or writable. In particular, a DWORD read that returns a live heap pointer does not grant a subsequent EUD operation permission to dereference that host pointer. Those object/player cases belong to Tier 2 and should be exposed through understood legacy structure mappings rather than unrestricted process-memory access.

Real generated EUD NoxScript also establishes the DWORD entry points directly:

| NoxScript builtin | Generated EUD name | Portable behaviour in this patch |
| --- | --- | --- |
| `0x59` (`Unused59`) | `SetMemory(addr, value)` | checked DWORD write to the preserved legacy data image |
| `0xB9` (`Unknownb9`) | `GetMemory(addr)` | checked DWORD read from the preserved legacy data image and push the value |

The dispatcher claims these otherwise-unused entry points only when the requested address and full DWORD width are inside `0x581450 .. 0x97EE68`. If not, it restores the popped VM arguments and falls through to the original builtin. This keeps the compatibility path narrow and avoids changing ordinary calls that are not recognizable as mapped EUD data access.

Together with the four BYTE/WORD helper targets, this makes the high-confidence fixed-address portion of Panic's EUD bootstrap/data access portable without executing the helper x86 that a Reloaded map may copy into the legacy data image. Writes to the legacy builtin table itself are therefore ordinary mapped DWORD writes; subsequent calls whose table entries contain one of the four known helper targets are intercepted semantically.

This still does **not** emulate arbitrary process memory. A DWORD value may represent a historical pointer, but a later access through that value is rejected unless it resolves to the preserved legacy data image. Supporting heap/object pointers is the Tier-2 task and should be based on understood object/player layouts rather than unrestricted host-memory access.

No debug logging was added. The compatibility layer is opt-in via the CMake `USE_EUD_COMPAT` build option and is not compiled or used by default.


## Current Tier-1 patch status

Implemented at source level:

- fixed-address BYTE/WORD/DWORD reads and writes across the preserved legacy data image;
- cross-array accesses with explicit little-endian assembly/disassembly;
- Panic EUD `SetMemory`/`GetMemory` compatibility through builtins `0x59`/`0xB9`;
- semantic interception of the four known non-DWORD injected helper targets;
- NoxScript dispatch integration before indirect builtin execution;
- fall-through to original builtin behavior when a DWORD address is outside the mapped data image.

Deferred intentionally:

- host heap-pointer dereference;
- object callback thunks and arbitrary x86;
- unknown function-pointer patches;
- map-level validation against a real EUD `.nxz`/`.map`.

Tier 2 below extends this fixed-address layer with targeted object/player pointer compatibility; Tier 3 then adds the understood semantic engine-operation helpers.

## Tier-2 implementation: live pointer and player-state compatibility

### Why Tier 2 needs pointer translation

Panic EUD does not keep all useful state in the original fixed data image. Its player helpers obtain a live unit/object pointer and then perform pointer arithmetic through the original runtime structures. A representative chain is:

```text
script object id
    |
    v
UnitToPtr
    |
    v
object
  +0x2EC -> player extension
                 |
                 +0x114 -> player-info/player record
```

The decomp still uses the same important offsets (`object + 748 == +0x2EC` and extension `+276 == +0x114`), but returning a current-process pointer directly to NoxScript would be unsafe and non-portable. It would also make compatibility depend on host pointer width/address layout.

The Tier-2 design therefore represents supported live pointers as **32-bit compatibility tokens**. EUD scripts can continue doing ordinary 32-bit pointer arithmetic on the token, while the compatibility layer resolves only explicitly allowed fields back to current engine storage.

### Compatibility-token design

Tier 2 uses a reserved positive 32-bit token range beginning at `0x20000000`, with a per-token stride large enough for the supported legacy structure spans. Tokens are never cast by the script VM to host pointers.

The working registry currently distinguishes:

- live game objects;
- player-extension structures reached from `object + 0x2EC`;
- the NoxScript function table;
- per-function script locals reached through that table;
- the timer-node shape needed by known EUD bootstrap/runtime traversal.

Each token records the host object it represents plus enough identity information to revalidate it before use. Object and player-extension tokens are rechecked against the current object id / `sub_511B60` lookup rather than being trusted indefinitely.

### `UnitToPtr` dependency confirmed

Panic's player helpers depend on `UnitToPtr` before they can reach object-relative fields. The EUD bootstrap temporarily replaces NoxScript builtin `0xB8`; the historical helper then resolves a script object id to the native Nox object pointer.

Generated Panic v171 maps construct the `UnitToPtr` helper as a known 10-DWORD local array and temporarily write the address of that array into builtin `0xB8`. Tier 2 resolves the script-local address to a token and requires the exact known helper signature before intercepting the call. The compatibility path then:

1. pops the NoxScript object id;
2. resolves it through `sub_511B60`;
3. registers/obtains a compatibility token for the live object;
4. pushes the token instead of a host address.

No injected helper x86 is executed, and an arbitrary script-local buffer assigned to builtin `0xB8` is not treated as `UnitToPtr`.

### Fixed-address pointers that now become tokens

Tier 1 could safely return DWORDs from the preserved data image, but some of those DWORDs are live host pointers. Tier 2 now recognizes a narrow set of known pointer-bearing locations and translates the values when read:

| Legacy source | Meaning | Tier-2 treatment |
| --- | --- | --- |
| `0x62F9E0 + n * 0x12DC` | player object's pointer field in the original per-player record | validate live object, return object token |
| `0x75AE28` | NoxScript function-table pointer | return script-table token |
| `0x83395C` | timer/free-list pointer used by known EUD runtime traversal | return timer-node token |

The `0x12DC` stride is 4,828 bytes, matching the surviving per-player record size. `0x62F9E0` is not treated as a new shadow table; it is the historical pointer field inside that existing record layout, so current engine storage remains authoritative.

Tier 2 deliberately does **not** scan every DWORD in the data image and guess that it is a pointer. Pointer translation is source-address-specific or structure-field-specific to avoid misinterpreting ordinary scalar values.

### Object/player-extension traversal now modeled

For a valid object token, Tier 2 recognizes the high-confidence pointer fields needed by Panic's player helpers:

| Structure | Offset | Meaning / treatment |
| --- | ---: | --- |
| object | `0x1FC` | owner/object pointer; translated to another object token when valid |
| object | `0x2EC` | player extension; translated to a player-extension token |
| player extension | `0x68` | weapon object pointer; translated to object token |
| player extension | `0x6C` | next/alternate weapon object pointer; translated to object token |
| player extension | `0x114` | player-info pointer; translated back to the corresponding legacy global-data address when the target lies in the preserved data image |

This is the key bridge between Tier 1 and Tier 2: once `extension + 0x114` resolves to the original global-data address, existing Tier-1 `GetMemory`/`SetMemory` logic can operate directly on the authoritative player record rather than maintaining a duplicate player-state model.

### Confirmed player-record fields

The decompiled engine independently uses the following offsets in the player record reached through `object + 0x2EC -> +0x114`. These align with the fields targeted by Panic's player-info helpers and are strong Tier-2 candidates:

| Player-record offset | Confirmed engine use / EUD-facing meaning |
| ---: | --- |
| `2064` (`0x810`) | player index/id byte used throughout player routing |
| `2235` (`0x8BB`) | player speed/stat value used by player stat calculations |
| `2239` (`0x8BF`) | player strength/stat value; also feeds derived weight/stat calculations |
| `2284` (`0x8EC`) | mouse/cursor X world coordinate |
| `2288` (`0x8F0`) | mouse/cursor Y world coordinate |
| `3652` (`0xE44`) | 16-bit maximum-weight/carry-capacity value |
| `3684` (`0xE64`) | player level byte |

The earlier investigation's tentative `0x22xx` interpretation was incorrect for this record layout. The decomp shows these fields at offsets inside the existing `0x12DC`-byte record, so Tier 2 should use the offsets above rather than inventing a larger structure.

Mana-like values remain on the player-extension side of the chain rather than in these player-record offsets. The current working allow-list includes the small extension ranges used by the known EUD player helpers, but final semantic naming/access permissions should be kept as narrow as the confirmed library behaviour.

### Read/write allow-list strategy

Tier 2 does not grant arbitrary reads and writes within a token span. The current implementation has separate read and write allow-lists by token kind.

High-confidence object reads currently cover only the small scalar ranges used by known EUD helpers plus the owner and player-extension pointer fields. Player-extension reads cover the known mana/status scalar ranges, weapon pointers and player-info pointer. Writes are narrower still: selected scalar object/player-extension fields only.

Unsupported accesses to an address inside the compatibility-token range are **claimed and rejected/zeroed by the compatibility path**. They do not fall through to unrelated vanilla NoxScript builtins. This prevents a malformed or unsupported EUD pointer access from accidentally changing normal builtin behaviour.

### Script-runtime traversal discovered

A representative generated EUD script performs part of its bootstrap/runtime work through the NoxScript metadata rooted at `0x75AE28`. The working Tier-2 code therefore contains narrow token support for:

- the script function table;
- the locals pointer at record offset `0x1C` in a `0x30`-byte function record;
- locals span derived from the corresponding count field at record offset `0x10`.

This support exists to let understood EUD runtime traversal remain portable. It is **not** a generic permission to invoke arbitrary functions or patch native callbacks. Tier 4 handles only the five exact Panic object-handler thunks; arbitrary script-call/callback semantics remain unsupported.

### Safety refinements made during Tier-2 review

Two rules were tightened while implementing the token layer:

1. **Token-range failures do not fall through.** If an address is recognizably one of our compatibility tokens but the field/width is unsupported, the EUD compatibility layer handles it as an unsupported access instead of restoring arguments and calling some unrelated original builtin.
2. **Scalar DWORDs are not pointer-guessed.** Reading an ordinary DWORD from the legacy data image returns the scalar unchanged unless its source address is one of the explicitly known pointer-bearing locations. This avoids turning coincidental integer values into host-object capabilities.

These rules keep Tier 2 deterministic and make the supported attack surface much smaller than arbitrary original-process memory access.

### Tier-2 source scope

Implemented in Tier 2:

- compatibility-token registry and live-token validation;
- mapped-data host-address reverse translation for the `+0x114` player-info bridge;
- exact-signature semantic interception of Panic v171 `UnitToPtr` through builtin `0xB8`;
- translation of the known fixed player-object, script-table and timer-node pointer sources into tokens;
- object -> owner / player-extension pointer translation;
- player-extension -> equipped/next weapon and player-info pointer translation;
- allow-listed dynamic BYTE/WORD/DWORD object/player reads;
- narrower allow-listed object/player writes;
- bounded BYTE/WORD/DWORD access to script-owned local arrays used by EUD helper construction;
- script-table/script-local and timer-node traversal required by the confirmed Panic helper bootstrap;
- rejection of unsupported token accesses without falling through into unrelated vanilla builtins;
- no debug logging.

No new test framework was introduced because this snapshot has no dedicated EUD/unit-test harness and the task explicitly forbids local compile/test execution. The validation cases below are intended for the caller's build/run pass.

Per instruction, **no compile or test execution has been performed locally**.

### Suggested validation cases

After applying the patch and building externally, validate at least these behaviours with a Panic EUD map or equivalent script:

1. `GetMemory(0x62F9E0 + player * 0x12DC)` returns a usable compatibility pointer for a live player.
2. `UnitToPtr(unit)` returns a usable token and does not attempt to execute the helper's x86 bytes.
3. `GetMemory(ptr + 0x04)`, `GetMemory(ptr + 0x08)`, `GetMemory(ptr + 0x10)` and `SetMemory(ptr + 0x10, value)` preserve the expected unit id/class/flags behaviour.
4. `GetMemory(ptr + 0x1FC)` traverses owner -> object token -> `+0x2C` script id.
5. `GetMemory(ptr + 0x2EC)` traverses to the player extension; `+0x68/+0x6C` resolve equipped weapon objects and `+0x114` resolves the authoritative player record.
6. player-record reads at `+0x810`, `+0x8BB`, `+0x8BF`, `+0x8EC`, `+0x8F0`, `+0xE44` and `+0xE64` match the engine's current values.
7. player-extension mana fields at `+0x04/+0x06/+0x08` and action field at `+0x58` can be read/written with the appropriate BYTE/WORD/DWORD helper.
8. the generated script-table -> timer-node -> locals bootstrap returns the local-array token, and the exact Panic v171 `UnitToPtr` signature is recognized.
9. an unsupported offset inside a compatibility token is rejected instead of being passed to the original `Unused59`/`Unknownb9` builtin.
10. callback/function-pointer writes such as the known collide/death/pickup/use-item fields remain unsupported in Tier 2.

### Tier-2 boundary remains intentionally strict

Tier 2 itself does not broaden memory permissions for these behaviours:

- arbitrary host-pointer dereference;
- arbitrary reads/writes through a token merely because an offset is within its allocation;
- EUD-provided x86 execution;
- historical raw function-address invocation;
- arbitrary modifications to object callback/function-pointer fields;
- generic script-function invocation through raw runtime structures.

Tier 3 covers understood one-shot engine-operation shims, and Tier 4 separately handles the five known Panic object callback fields by exact helper matching plus portable wrappers. This keeps callback support out of the generic Tier-2 memory allow-list.

## Tier-3 implementation checkpoint

Tier 3 extends the same safety model to Panic helpers that historically installed small x86 shims into NoxScript builtin slots. The compatibility layer does not execute those shims. It resolves the patched target as a script-local compatibility token, verifies the exact helper signature (including decoded `CALL rel32` destinations where Panic relocates a helper), then performs the equivalent reconstructed engine call.

### Supported semantic helpers

| Panic/EUD operation | Patched builtin | Reconstructed operation | Tier-3 treatment |
| --- | ---: | --- | --- |
| spell-name lookup | `0x5E` | `sub_4243F0` | pop script string id, resolve bounded string-table entry, push spell id |
| CreateObjectAt | `0x35` | `sub_4E3810` + `sub_4DAA50` | create/place normally and return an object token |
| RemoveTreadLightly | `0xB8` | `sub_4FC300(object, 4)` | resolve object token and call directly |
| AllocateXtraObjectSpec | `0xB8` | `sub_4E3450` | allocate through the original object pool and return an object token without executing relocated helper code |
| MonsterActionPush | `0x4C` | `sub_50A260` | read the helper's local argument block, push the action through the engine and return a restricted monster-action token |
| wall lookup | `0x5E` raw helper | `sub_410580` | return a wall token keyed by wall coordinates |
| create magic wall | `0x1F` raw helper | `sub_4FFD00` | call the reconstructed wall creator using the four consumed arguments |
| add breakable wall | `0x1F` raw helper | `sub_410840` | resolve wall token and call directly |
| NPC equip / unequip | `0x5A` | `sub_4F2F70` / `sub_4F2FB0` | decode relocated helper target, resolve unit/item tokens, call selected operation |
| PlaySoundAround | `0x74` | `sub_501960` | resolve unit token and call the four-argument sound helper with the two zero arguments used by Panic |
| NetLoadFx | `0x1F` raw helper | `sub_523150` | copy the referenced two-float position through bounded EUD reads and call the reconstructed network-FX helper |
| PlaySummonEffect | `0x1F` raw helper | `sub_5236F0` | decode Panic's five-word argument block, copy the position locally and call the reconstructed summon-FX helper |

Builtin number alone is never sufficient for Tier-3 dispatch. Panic reuses some builtin slots for unrelated helpers, so a helper must also match its expected bytecode/call destinations. For example, unsupported FX code installed in builtin `0x5A` is not mistaken for the supported NPC equip/unequip shim.

### Tier-2.5 data/pointer expansion included with Tier 3

The Tier-3 patch also fills the high-confidence data accesses required by those helper libraries:

- fixes mapped DWORD reads/writes so a 4-byte access crossing `0x587000` or `0x5D4594` is assembled/disassembled one byte at a time rather than crossing C-array storage with a host pointer;
- recognizes monster extension fields used by Panic's spell/action helpers, including action-stack guard fields, scan/aim/flee/status values, spell delays/flags and spell level;
- adds a restricted monster-action token exposing only the action type, spell number and coordinate fields used by the library;
- adds wall and wall-details tokens exposing only the known flags/hitpoints/details fields;
- permits the additional direct object scalar fields used by `unitstruct.h` (subclass, flags, generic `+0x1C`, team id, mass and speed) while leaving raw pointer-bearing health/gold/script fields unsupported;
- validates object tokens against the engine object allocator's live-allocation list plus the object's script id, which also permits the unplaced object returned by `sub_4E3450` without weakening compatibility into arbitrary host-pointer access.

Wall token validation caches the wall coordinate key rather than dereferencing a possibly stale wall pointer to rediscover its coordinates. Monster-action tokens remain tied to their owning live object/extension and the known action-stack range.

### Unknown helper behavior remains blocked

If a patched builtin target falls inside the compatibility-token range but does not match a supported helper, `nox_eud_dispatch_builtin` claims the call without turning the token into a host instruction pointer. This is intentional: an unsupported Panic helper may fail semantically, but it cannot cause the reconstructed engine to jump into script-owned x86 bytes.

The following remain deliberately unsupported for later tiers:

- `SetUnitCallbackOnDiscardBypass` and other dynamically generated callback chaining thunks;
- missile/melee callback hooks;
- GreenExplosion, GreenLightning, LinearOrbMove and other FX shims not yet mapped completely;
- generic `callmethod` / arbitrary raw native invocation;
- semantic `MemAlloc`/`MemFree` token storage;
- broad GameData/ThingDB traversal beyond the already mapped fixed data image;
- arbitrary copied x86 or function-pointer rewriting.

### Tier-3 validation checklist

No compile or test execution was performed for this patch. When building externally, validate at least:

1. unaligned DWORD accesses at `0x586FFD`, `0x586FFE`, `0x586FFF`, `0x5D4591`, `0x5D4592` and `0x5D4593`;
2. positive and negative exact helper matching, including a different helper installed into the same builtin slot;
3. spell lookup by a known spell string;
4. `CreateObjectAt` returns a token whose `+0x2C` matches the created object's script id;
5. `AllocateXtraObjectSpec` returns a usable token before placement;
6. `MonsterActionPush` returns a token and writes to `+0x04/+0x0C/+0x10` affect the pushed action only;
7. stale object/action tokens are rejected after their underlying object/action is no longer valid;
8. wall lookup/details/hitpoint access and create/add-breakable helpers;
9. NPC equip and unequip with valid object tokens;
10. `PlaySoundAround` on a live unit;
11. `NetLoadFx`/GreenSpark and `PlaySummonEffect` reproduce their visible network effects;
12. an unsupported helper target in script-local storage is blocked and never executed as native code.

### Tier-4 callback compatibility

Panic's common callback APIs do not add listeners after an engine event. They overwrite existing function pointers in each object's runtime record. Tier 4 preserves that replacement model without executing the injected x86 helper.

The recognized handler fields and callback-id storage are:

| Panic API | Object handler field | EUD callback-id slot | Portable treatment |
|---|---:|---:|---|
| `SetUnitCallbackOnCollide` | `+0x2B8` | shadow `+0x2FC` | install C collide wrapper |
| `SetUnitCallbackOnPickup` | `+0x2C4` | shadow `+0x0B8` | install C pickup wrapper |
| `SetUnitCallbackOnDiscard` | `+0x2C8` | shadow `+0x090` | install C discard wrapper |
| `SetUnitCallbackOnDeath` | `+0x2D4` | shadow `+0x228` | install C death wrapper |
| `SetUnitCallbackOnUseItem` | `+0x2DC` | shadow `+0x2FC` | install C use-item wrapper |

The helper value written into a handler field must match the exact Panic v171 DWORD sequence for that event. A handler write is kept pending until a valid NoxScript function index is written to the corresponding callback-id slot. Only then is the current native handler replaced with the portable C wrapper. Unknown code values written to these known handler fields are claimed and ignored rather than executed.

`OnCollide` and `OnUseItem` intentionally share Panic's `+0x2FC` callback slot. Updating that shadow slot therefore changes the callback seen by both installed handlers, matching the original EUD library's storage convention. The compatibility layer does not place that callback id into the real object field, because the reconstructed engine also uses the physical `+0x2FC/+0x300` pair for its ordinary one-shot script callback descriptor.

The portable wrappers call the existing `sub_507310(callback, caller, trigger)` VM entry point using Panic's original caller/trigger order:

- collide: `(callback, other, self)`;
- pickup: `(callback, holder, item)`;
- discard: `(callback, holder, item)`;
- death: `(callback, 0, self)`;
- use item: `(callback, user, item)`.

Pickup and discard return the value returned by `sub_507310`, matching the original helper thunks. Collision and use-item invoke the script callback and return success; the original x86 helpers restored their incoming `EAX`, which is a register-preservation artifact rather than a portable script return convention. Death ignores the VM return, as the original handler path does.

Panic's separate `RegistItemPickupCallback` writes a one-shot function index to object `+0x300`. Tier 4 validates that function index and writes it to the real field so the existing `sub_4F36F0` success path continues to call `sub_502490(object + 0x2FC, holder, item)` and then reset the callback to `-1`. This remains distinct from replacing the object's pickup handler.

Tier 4 also adds `nox_eud_reset()`. NoxScript load and teardown call it so any still-live object whose handler still points at a Tier-4 wrapper is restored to its pre-EUD handler before compatibility tokens and callback shadow state are discarded. Tracked one-shot pickup callbacks are cleared when they still contain the registered function index.

### Tier-4 validation checklist

No compile or test execution was performed for this patch. When building externally, validate at least:

1. each of the five exact Panic callback helpers is recognized and a one-DWORD change is rejected;
2. handler replacement occurs only after a valid callback function index is supplied;
3. `GetMemory` on an installed/pending handler returns the EUD helper token rather than the portable C function pointer;
4. collide calls NoxScript with `GetTrigger=self` and `GetCaller=other`;
5. pickup/discard call with `GetTrigger=item` and `GetCaller=holder`, and their handler return follows `sub_507310`;
6. death calls with `GetTrigger=self`, `GetCaller=0`, and does not automatically invoke the old/default death handler afterward;
7. use-item calls with `GetTrigger=item` and `GetCaller=user`;
8. collide and use-item observe the same shadow `+0x2FC` callback id;
9. `RegistItemPickupCallback` fires only through the normal successful-pickup path and resets to `-1`;
10. object deletion/reuse invalidates the old registry entry by object id;
11. script reload/teardown restores installed handlers and clears EUD tokens/callback state;
12. `SetUnitCallbackOnDiscardBypass`, missile/melee thunks, and unknown script-local code remain blocked and are never executed.

## Implementation progress: Tier 4.5 callback chaining and Tier 5 portable data helpers

The current patch extends the portable compatibility layer without introducing an x86 emulator or a generic historical-function trampoline.

### Tier 4.5: DiscardBypass

Panic's generated DiscardBypass helper is accepted only when the 44-byte allocation-backed thunk matches the known fixed instruction template, contains a valid NoxScript callback id, and carries a previous discard-handler value that can be resolved safely. The compatibility layer then installs a C discard wrapper which invokes the EUD callback and chains to either the previous Tier-4 EUD discard callback or the exact previous native discard handler.

When EUD code reads an unmodified native discard-handler field, the compatibility layer returns an opaque `NATIVE_HANDLER` token scoped to that object rather than exposing an executable C address. That token is accepted only when installing a matching DiscardBypass thunk for the same live object and while the handler field still contains that exact handler. Unknown generated code remains rejected.

### Tier 5A: portable EUD heap

Known Panic `MemAlloc` and `MemFree` helpers are recognized by helper bytecode plus their reconstructed call targets. Allocations are host-owned buffers represented by `ALLOC` tokens. Supported sizes are `1 .. 0x1FFFF` bytes so every allocation remains inside one compatibility-token stride. `MemFree` accepts only the base token. Freed token slots are quarantined until `nox_eud_reset()` to prevent a stale EUD pointer from aliasing a later allocation.

`nox_eud_reset()` restores translated engine references before freeing all remaining EUD allocations, then clears compatibility tokens, callback state, SmartMemory state, and the quarantine.

### SmartMemory lifecycle

Panic SmartMemory writes a generated cleanup routine and patches the legacy function slot at `0x59824C`. The compatibility layer preserves the EUD-visible write/read state but never places that generated code into the live reconstructed function slot. All live EUD allocations are already released by `nox_eud_reset()`, and the mapped SmartMemory list head at `0x5956DC` is cleared at reset.

The corresponding recovery hook at `0x59821C` remains protected from generated-code installation. Tier 5 originally shadowed that target only; the post-Tier-5 compatibility step described below now recognizes Panic's exact recovery helpers and replays their linked restoration lists semantically before EUD-owned allocations are freed. The internal pointer-field restore journal remains separate and exists to ensure that freeing an EUD allocation cannot leave a dangling C pointer in engine data.

### DwordCopy

The known Panic `REP MOVSD` helper used through `invokeRawCode` is replaced semantically. Source, destination and count are read through the compatibility memory layer and copied forward one DWORD at a time. It therefore works across mapped legacy memory and supported compatibility tokens without executing the helper's x86. The transfer count is capped before iteration.

### ThingDB and GameData

Tier 5 recognizes the known ThingDB server/sprite table roots and the GameData root. Raw host pointers are converted into typed compatibility tokens. Access is deliberately field-bounded to the server/sprite fields used by the inspected Panic libraries and to the understood GameData descriptor/subtable/value layout.

Pointer-bearing fields that are explicitly supported never receive an EUD token directly. When a map stores an `ALLOC` token in an understood engine pointer field, the compatibility layer stores the corresponding host pointer internally and returns the same token representation on EUD reads. Server/sprite fields translated this way are restored before the backing allocation is freed or at script reset.

### MagicMissile

Panic's MagicMissile helper allocates a 20-byte extension, stores object pointers in its first two DWORDs, assigns that allocation to object `+0x2EC`, and changes object update `+0x2E8` to legacy function `0x53BDA0`. The patch recognizes that exact legacy update function and maps it to reconstructed `sub_53BDA0`. The `+0x2EC` assignment is kept pending until that exact update-function write commits the known MagicMissile layout; object tokens in the first two extension DWORDs are then converted to actual engine pointers. No arbitrary function-pointer write is enabled.

The normal object finalizer frees `+0x2EC` with `free()`. Tier 5 therefore notifies the compatibility layer immediately before that free so an attached EUD allocation token is invalidated/quarantined instead of being freed a second time at script reset. If reset occurs first, the compatibility layer detaches the extension and frees it itself.

### Deliberately unsupported after Tier 5

The compatibility layer still does not execute arbitrary allocations as code, call arbitrary legacy function addresses, expose native pointers as EUD integers, or accept generic writes to function-pointer fields. `Bind` is supported only for Panic's exact known helper and routes through the existing NoxScript VM; generic `callmethod`/`invokeRawCode` behavior is not enabled. Large libraries which copy and rewrite native implementations (`meleeattack.h`, `potionpickup.h`, `absolutelypickup.h`) remain future semantic ports rather than generic x86 compatibility.

### Validation checklist for the caller

Build with `-DUSE_EUD_COMPAT=ON` and run `eud_compat_memory_test` for mapped-memory boundaries, little-endian unaligned access, rejection of out-of-range operations, wrapped synthetic builtin lookup/dispatch, Panic recovery-list reset behavior, and heap-backed SpellDB/AbilityDB string-pointer restoration. Further validation should exercise semantic `Bind` from a real compiled Panic script; MemAlloc/SetMemory/GetMemory/MemFree including stale-token rejection; SmartMemory allocation followed by map/script reset; DwordCopy across two EUD allocations and between allocation/mapped data; ThingDB/GameData reads used by a real Panic map; pointer-field replacement followed by MemFree/reset; MagicMissile creation/update; normal Tier-4 callbacks; and DiscardBypass chaining over both an original native discard handler and an existing Tier-4 EUD discard handler.

## Implementation progress: semantic Bind, recovery replay and wide-string database fields

This post-Tier-5 step closes three dependencies used together by Panic's database-editing libraries without enabling generic script/native code execution.

### Exact semantic `Bind`

Panic's `Bind` temporarily places an 88-byte helper into NoxScript builtin slot `0xA5`. The compatibility layer accepts that target only when the fixed instruction bytes and all five relocated call destinations match the known helper: three calls to `script_pop` (`0x507250`), one argument push call (`0x507230`), and the final VM dispatch call (`0x507310`). It then performs the same operation in C: pop the argument-array token, function id and function-record token; validate that the record is the requested entry in the current `0x30`-byte script table; push the function arguments in reverse order; and call the current VM with the existing caller/trigger context. An unknown slot-`0xA5` target is not treated as Bind.

The reconstructed `sub_507310(function_id, caller, trigger)` is the authoritative VM entry used here. Observed behavior from `GAME4.c`: it stores `caller` and `trigger` in the legacy globals at `0x979720/0x979724`, reads the selected function's argument count from script-record `+0x08`, pops that many VM-stack values into the function's local argument storage, then interprets the function bytecode and returns the VM result. Existing engine callback paths also call this routine, so the compatibility layer reuses it rather than implementing a second script executor.

### Panic `recovery.h` lifetime replay

Panic builds two allocation-backed destructor helpers and installs them through legacy slot `0x59821C`. Each helper walks a singly linked list of 12-byte recovery nodes containing `(target, original_value, next)`, restores each target, restores the previous destructor target, and returns to that previous target. The normal 52-byte helper is guarded by a DWORD read from `0x852980`; `recovery.h` initially embeds a different address in its byte template but overwrites that operand before installation. The 44-byte map-changed helper has no guard. `AllocSmartMemEx` returns the user pointer eight bytes into its allocation, so helper/list-holder/node tokens are intentionally accepted at valid nonzero offsets inside an `ALLOC` token.

The compatibility layer never installs these x86 helpers. A write to `0x59821C` is accepted only when the complete helper shape, list-head holder and previous-helper chain are valid. During `nox_eud_reset()`, before EUD allocations are freed, the chain is decoded and each trusted allocation-backed list is replayed through `nox_eud_write_u32`. The normal helper restores only while the mapped `0x852980` guard is nonzero; the map-changed helper restores unconditionally. Recovery nodes themselves must live in EUD allocations, but their targets still pass through the ordinary compatibility write layer so mapped data and supported typed-token fields retain their normal safety rules.

This makes the script-side `setRecoveryData`/`SetRecoveryDataType2` logic authoritative for deduplication and saved values; no second recovery journal is invented for Panic semantics. The older internal pointer-field restore journal remains only a memory-safety mechanism for translated host-pointer fields.

### SpellDB and AbilityDB wide strings

Panic treats SpellDB records as 80-byte entries rooted at legacy `0x663EF0` and AbilityDB records as five 52-byte entries rooted at `0x666A24`. The compatibility layer recognizes only the name/description pointer fields at record offsets `+0x00` and `+0x04`. Reads translate a host pointer back to an existing EUD allocation token when possible; otherwise they create a bounded `WSTRING` token. Writes accept only `ALLOC` or `WSTRING` compatibility tokens and store the resolved host pointer into the preserved legacy data image. Heap-backed writes are registered with the existing restore mechanism so an allocation cannot be freed while the engine still points into it.

`WSTRING` tokens include the UTF-16 terminator plus the following DWORD because Panic's `SpellDbCheckRemovable` probes a marker immediately after the terminator before deciding whether an older dynamically allocated spell string may be freed. Unterminated strings exceeding the bounded scan are rejected.

The decompiled database accessors corroborate these layouts. `sub_424930` accepts spell ids `1..136`, uses an 80-byte record stride, checks the record's populated marker and returns its first pointer; `sub_424960` searches the same 80-byte records by the first wide-string pointer. `sub_425290` searches five 52-byte ability records beginning at `0x666A24` by their first wide-string pointer. These routines remain unchanged; the EUD layer only makes the pointer values stored in their existing backing records safe to manipulate from EUD code.

### Regression coverage added for this step

The focused opt-in `eud_compat_memory_test` now constructs the exact allocation-backed recovery helper/list shape through the production memory API and covers: guarded normal restoration, normal-helper skip when the `0x852980` guard is zero, unconditional map-changed restoration, SmartMemory-style `+8` allocation offsets, SpellDB heap-backed name/description pointer round-trip/reset, AbilityDB heap-backed name/description pointer round-trip/reset, and wrapped synthetic builtin lookup. On 32-bit test builds it also drives the real `sub_508B70` path with a non-empty script context so `sub_508C30`/`sub_508C70` exercise the same synthetic lookup before a non-native null target is safely rejected. A narrow test-only allocator entry point is compiled only into the opt-in test runtime so the regression uses the production EUD allocation/token implementation rather than duplicating it.

Per repository workflow, these tests are added but were not compiled or executed while preparing the patch because the maintainer explicitly requested no local compile/test run.

## Map-driven compatibility pass: fixed FX and `Monster` unit customization

The public Panic `fxeffect.h`, `unitstruct.h`, and `monster.c` sources expose a
set of smaller compatibility gaps which are independent of the deliberately
unsupported copied-native-code libraries. This pass handles only operations
whose legacy helper and reconstructed engine target can both be identified
exactly.

### Remaining fixed `fxeffect.h` helpers

Three fixed helpers are translated semantically instead of being executed:

- `GreenExplosion(x, y)` temporarily installs a 17-DWORD helper in builtin
  `0x5A`. The helper pops the two float bit patterns, builds a two-float point,
  calls legacy `0x523200`, then frees its temporary buffer. The compatibility
  layer verifies the complete helper and calls reconstructed
  `sub_523200(point, 200)` directly.
- `LinearOrbMove(unit, x_vect, y_vect, speed, time)` writes object fields
  `+0x50`, `+0x54`, and `+0x70`, installs a six-DWORD helper in builtin `0xB8`,
  and calls legacy `0x523530`. Those three scalar fields are now allowed on a
  validated object token and the exact helper dispatches directly to
  reconstructed `sub_523530(object)`.
- `GreenLightningFx(x1, y1, x2, y2, time)` installs a 21-DWORD helper in
  builtin `0x64`, packs four integer coordinates, and calls legacy `0x523790`
  with the duration. The compatibility layer verifies that exact helper and
  calls reconstructed `sub_523790` with a local `int4`, so no EUD code or
  temporary native pointer is executed/exposed.

The previously supported `PlaySoundAround`, `NetLoadFx`, and
`PlaySummonEffect` paths remain unchanged. Helpers which copy or rewrite large
native implementations are still rejected.

### Engine-owned object backing data

Panic's `unitstruct.h` obtains several engine-owned pointer fields through
`GetMemory`. Returning their host pointers would violate the compatibility
boundary, while returning the raw value would make the library unusable. This
pass therefore adds owner-scoped typed tokens for the specific structures used
by the public library:

| Object field | Observed use | Portable access |
|---|---|---|
| `+0x22C` | max/current health backing data | DWORD `+0` and `+4` |
| `+0x2B4` | pickup/gold backing data | DWORD `+0` |
| `+0x2E0` | item/wand backing data | DWORD `+0`; wand class also `+108` |

Each token remains valid only while the owning live object's corresponding
field still contains the same host pointer and the object's script id still
matches. The reconstructed object finalizer `sub_4E38A0` frees all three
fields (`+556`, `+692`, and `+736` respectively), corroborating that these are
engine-owned allocations rather than general-purpose EUD memory. The EUD layer
never frees them.

This covers `SetUnitMaxHealth`, `UnitStructGetGoldAmount` /
`UnitStructSetGoldAmount`, potion backing values, and the wand charge field
used by `Monster` (`amount[27]`).

### Voice records and monster behavior-table pointers

Panic `VoiceList()` starts at legacy `0x663EEC` and follows record `+0x4C`.
The reconstructed `sub_424170` loads `SoundSet.bin` into `0x54`-byte linked
records, stores the list head in that same legacy global, and links records at
`+76` (`0x4C`); `sub_4242C0` later frees the list. Reads from `0x663EEC` are
therefore translated to a bounded `VOICE_RECORD` token, and only the known
`+0x4C` next-link traversal is exposed.

For live monster extensions:

- `+0x1E8` accepts only a validated `VOICE_RECORD` token (or zero), translating
  it to the current host pointer;
- `+0x1E4` accepts only a validated current-script `SCRIPT_LOCALS` token (or
  zero), including an interior offset, for Panic's custom bin-script arrays.

Both pointer fields are recorded in the existing restore journal so script
teardown cannot leave a live engine object pointing into expired EUD/script
storage. Reads translate the host pointer back to the corresponding token.

The public `Monster` source also writes scalar monster-extension fields
`+0x178`, `+0x538`, `+0x540`, and the five color DWORDs `+0x81C..+0x82C`.
Those exact fields are now allowlisted. The monster extension token span is
raised to `0x888`; reconstructed engine code accesses monster-extension
`+2180` (`0x884`), so this does not infer storage beyond an observed engine
boundary.

### `Monster` object fields and exact native handlers

The map changes Maiden object data at `+0x230..+0x2AC` (32 aligned DWORDs) and
writes the object thing id at `+0x04`; these exact scalar ranges are now
allowlisted. No callback/function-pointer fields are included in that generic
range.

Two native handler values used directly by public Panic sources are mapped
individually:

- object update `+0x2E8 = 0x53AC10` maps only to reconstructed
  `sub_53AC10`, the projectile-update routine used by `BlueOrbSummon`;
- pickup handler `+0x2C4 = 0x53A720` maps only to reconstructed
  `sub_53A720`, while legacy `0x4F3A60` maps only to reconstructed
  `sub_4F3A60` so `UnitStructGetGoldAmount` can recognize gold objects.

Reads of those known live handlers return their legacy values. Arbitrary
function-pointer values remain claimed/rejected, and the existing exact
`0x53BDA0` MagicMissile path remains separate.

### Scope intentionally left for later semantic ports

The public `Monster` map also imports code-copying facilities, notably its
player-update replacement. The larger Panic `meleeattack.h`, `potionpickup.h`,
`absolutelypickup.h`, generic `callmethod`, and arbitrary `invokeRawCode`
features remain unsupported. This pass does not make copied x86 executable;
it closes only the map-visible gaps which can be represented by existing
reconstructed C functions and bounded data fields.

### Validation for the maintainer

Per the requested workflow, this patch adds/updates regression coverage but is
not compiled or executed while being prepared. In addition to the existing EUD
memory tests, build the opt-in `eud_monster_map_test` and run `Monster.map` with
particular attention to custom monster health/bin tables/voices, Maiden color
changes, blue projectile collision/update, Oblivion pickup/use behavior, wand
charges, and geometry-ring `LinearOrbMove`. A failure in the copied
player-update/melee/potion replacement paths should still be treated as a
known later semantic-port gap rather than a reason to enable raw code.
