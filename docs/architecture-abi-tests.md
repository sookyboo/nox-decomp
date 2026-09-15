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
