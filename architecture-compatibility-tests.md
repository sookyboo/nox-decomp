# ARM32/i386 architecture-compatibility test plan

This tracks Sookyboo-authored non-Docker changes that remain reachable from the
current combined branch (`experiment`) and affect ARM hard-float, i386, or the
Linux build/runtime path. The proposed tests should be run for both targets
where the change is shared. “Existing” refers to `abi_cross_test`, which is
registered with CTest and runs on i386; its ARMHF build is also validated with
`qemu-arm`.

| Commit | Compatibility change | Regression test to write |
|---|---|---|
| `cde5a30` | Adds FFmpeg video support to the Linux targets. | Build/run a tiny FFmpeg probe for each target that opens a VQA/video stream and verifies the expected decoder libraries are linked for the target architecture. |
| `d70c630` | Introduces ARMHF ABI wrappers and ARM crash fixes. | **Complete:** `abi_cross_test` exercises the generic `sub_4F4E50` pointer wrapper on i386 and ARMHF, in addition to the summon wrapper. |
| `0081aa0` | Makes rendering/cursor handling safe for ARM native-resolution surfaces. | **Complete (i386):** `render_arch_test` creates a 641×479 source, exercises RGB555→RGBA5551 cursor conversion and scaled blitting, and verifies dimensions/pixel values. ARMHF execution requires the target SDL2 development package (not available in this sandbox). |
| `e65aec8` | Prevents ARM compatibility changes from crashing i386. | Run the wrapper harness under i386 with null, aligned, and unaligned pointer/output cases; assert no crash and correct return values. |
| `00584b2` | Adds the large i386 ABI-wrapper/raw-body split. | Compile and execute representative wrappers with a stub raw body on i386; cross-compile the identical source for ARMHF and compare argument payloads/results. |
| `e19d204` | Fixes broken ARMHF ABI handling in summon-related code. | Summon ABI test passes pointer bits through the ARM VFP slot and verifies the raw function sees the original pointer; run the direct-pointer i386 path too. |
| `ef42f7a` | Fixes summoning and monster attack-area ABI behavior on ARM and i386. | Exercise summon output and monster-area callback wrappers with writable buffers/callback arguments; compare output coordinates and callback payloads across targets. |
| `1301939` | Preserves float bit patterns in the mana-drain ABI path. | Feed values whose IEEE-754 bits differ from their integer value (including NaN, signed zero, and subnormal values) and verify identical stored float bits on ARMHF/i386. |
| `0d451ef` | Fixes the rare summon segfault by passing a real output pointer on i386 while retaining ARM slot conversion. | **Existing `abi_cross_test`:** raw stub writes through `out_xy`; test verifies the marker on i386 and ARMHF (`qemu-arm`). |
| `3487996` | Adds Linux x86_64 compile guards/compatibility adjustments around the 32-bit code. | Configure/build an x86_64 Linux target and run a startup smoke test; separately ensure ARMHF remains a 32-bit target and still passes the ABI suite. |
| `39e7c16` | Adjusts CMake/toolchain logic to restore ARM builds. | Configure with the ARMHF toolchain file, build the executable, and run `ctest -R abi_cross_test` under ARM emulation. |
| `f7c9535` | Explicitly rejects unsupported 64-bit ARM builds. | Configure with an aarch64 compiler and assert CMake fails with the documented 32-bit-ARM error; configure ARMHF and assert success. |

## Priority

The ABI harnesses (`d70c630` through `0d451ef`) are the highest-value tests:
they catch the pointer-as-float and calling-convention regressions that can
compile cleanly but crash only on one architecture.  The build/runtime tests
then protect the Docker and CMake paths that produce those binaries.
