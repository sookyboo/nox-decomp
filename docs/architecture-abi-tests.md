# Architecture ABI regression tests

This document records how the two ABI regression tests were written. They
cover the calling-convention changes used by the ARMHF and i386 builds,
without requiring Nox game assets or a running game.

## Test location and entry point

The test is [`tests/abi_cross_test.c`](../tests/abi_cross_test.c). The root
[`CMakeLists.txt`](../CMakeLists.txt) registers it as the CTest test
`abi_cross_test` and builds it together with `abi_wrappers.c`.

The test calls the production wrapper entry points:

- `sub_4F4E50(void *)`, representing the generic pointer wrapper introduced
  for ARM compatibility;
- `sub_500F40(int, void *)`, representing the summon/output-pointer path fixed
  by the later i386/ARM ABI changes.

## What the targeted functions do

These names and roles come from the decompilation comments and surrounding
call sites; they are gameplay-facing descriptions rather than new test-only
behavior.

### `sub_4F4E50` (`nox_xxx_unitTriggerXfer_4F4E50`)

This is a unit “transfer”/state-update routine. It reads a versioned record
from the game's stream helpers (`sub_426AC0`), updates unit fields such as
position-related values and shape/appearance data, and refreshes associated
unit resources. The function is used through callback tables (the call site
compares the callback against `sub_4F4E50`), so receiving the wrong unit pointer
can corrupt arbitrary unit state or crash while processing a transfer.

### `sub_500F40` (summon placement helper)

`sub_500DA0` (the summon-start path) calls this routine before creating the
summoned object. It derives a valid spawn position from the source unit and its
owner/target coordinates, limits the search distance, performs collision/line
checks, and writes the resulting `(x, y)` coordinates to the caller-provided
output buffer. A failure returns zero and can trigger the summon-failure path.
Passing that output buffer with the wrong ABI was the cause of the rare summon
segfault fixed by `0d451ef`; the test therefore writes through the received
buffer rather than checking only the return code.

The raw functions are supplied as test stubs. Link-time garbage collection
(`-ffunction-sections` and `--gc-sections`) retains the wrappers used by the
test and discards unrelated wrappers whose raw implementations are not linked.

## Architecture-specific contract

`abi_wrappers.c` uses `nox_abi_ptrslot_t`:

- ARM hard-float (`__arm__` + `__ARM_PCS_VFP`): a pointer is bit-cast into a
  `float` slot so the AAPCS-VFP calling convention transports the original
  32-bit payload through VFP registers;
- i386: the same value is passed as a normal pointer/integer argument.

The test duplicates only this ABI declaration boundary—not the production
algorithm—and checks the observable contract at the raw callee.

## The two checks

1. The first test verifies pointer payload preservation. A sentinel `self`
   value is passed through `sub_4F4E50`; the raw stub decodes the slot on ARM
   (or reads the pointer directly on i386) and checks it is unchanged.

2. The summon test verifies the output-pointer fix. `sub_500F40` receives a
   writable two-word buffer. The raw stub decodes the output pointer, writes
   `0xC0DEC0DE`, and the caller verifies the marker. This catches the old
   pointer-as-float/signature mismatch that could cause the rare summon
   segfault.

3. The i386-safety checks pass null pointers and a deliberately unaligned
   output address through both wrappers. Null inputs must return safely, while
   the unaligned output must still receive the marker through byte-wise access.
   This covers the crash class addressed by `e65aec8` without relying on
   architecture-specific unaligned-load behavior.

4. The wrapper/raw-body split checks call `sub_50A5C0` and `sub_531E20`, two
   representative one-pointer entry points from `00584b2`. Their stubs accept
   the architecture-specific raw signature and return success only when the
   original sentinel pointer arrives intact.

5. The ARMHF call-site correction from `e19d204` is covered by invoking
   `sub_50A5C0` and `sub_5281F0` with sentinels. The raw stubs record whether
   those pointers arrived intact, catching the old float-typed call boundary.

6. The monster-area callback bridge from `ef42f7a` is exercised through
   `sub_549860`. Its owner ID and callback-context pointer are checked at the
   raw callee, ensuring the callback payload is not misread as a float or
   widened pointer on either 32-bit target.

7. The mana-drain path `sub_52E210` uses integer reads whose bits are then
   interpreted as a float. `mana_abi_bits_test` locks down that contract by
   round-tripping representative IEEE-754 bit patterns with `memcpy`, avoiding
   undefined aliasing and proving that ARMHF and i386 preserve the same bits.

8. `x86_64_compat_test` verifies the host-compatibility rule from `3487996`:
   even when CMake identifies an x86_64 host, the game target remains a
   32-bit ABI with 4-byte pointers and `uintptr_t`.

Both checks also validate the expected sentinel `self` value and return status,
so a test cannot pass merely because the wrapper returns without invoking the
raw function.

## Validation commands

i386 (configured build directory):

```sh
cmake --build build-i386 --target abi_cross_test -j"$(nproc)"
ctest --test-dir build-i386 --output-on-failure -R abi_cross_test
```

ARMHF cross-build and emulated execution:

```sh
arm-linux-gnueabihf-gcc -mfloat-abi=hard -O2 -ffunction-sections \
  tests/abi_cross_test.c abi_wrappers.c \
  -Wl,--gc-sections -o /tmp/abi-cross-armhf
qemu-arm -L /usr/arm-linux-gnueabihf /tmp/abi-cross-armhf
```

The test contains a 32-bit pointer-size assertion. It is intentionally not an
amd64 test: the game ABI and the compatibility fixes under test are 32-bit.
The harness validates argument transport and pointer safety; it does not claim
to validate the full summon or monster simulation logic, which requires game
fixtures/assets and separate integration coverage.

Similarly, `render_arch_test` is intentionally a focused headless SDL test.
It validates the 16-bit surface dimensions, scaling, and RGB555-to-RGBA5551
pixel conversion used by the architecture fix, but it does not create an
OpenGL context or execute the complete `draw.c` presentation pipeline. GPU,
window-system, and final on-screen rendering behavior still require separate
platform integration testing.

## Summoned-unit update guard

`sub_5281F0__abi_raw()` is the decompiled summoned-unit sight/update routine
called from monster updates. It reads the unit flags at offset `+16`; when the
`0x8000` guarded state is present and `sub_534A40()` rejects the update, the
function returns before dereferencing the optional unit state at `+748` or
iterating summon targets. This early return is the minimal safe boundary
covered by `tests/summon_update_test.c` for `adfe080`.

The test uses the public `sub_5281F0()` ABI wrapper around the production
decompiled function and stubs only the guard decision. Calling the raw body
directly would bypass the ARMHF VFP pointer transport and is not a valid test
of the production entry point. The test verifies that a minimal guarded
summoned-unit fixture reaches the guard and returns safely. Target-list
filtering, sight refresh callbacks, and full summon lifecycle behavior remain
game integration coverage.

## String-format regression

`string_format_test` covers the production `nox_snprintf` entry point used by
inventory and other UI code to turn gameplay values into text. The regression
for commit `2cc50d3` checks the `%f` path with fixed precision, including the
returned character count and resulting bounded string (`Mana 12.50`). The test
supplies only the compatibility-layer stubs needed to link `src/string.c` in
isolation; inventory layout and rendering remain integration coverage.
