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
| `0081aa0` | Makes rendering/cursor handling safe for ARM native-resolution surfaces. | **Complete:** `render_arch_test` creates a 641×479 source, exercises RGB555→RGBA5551 cursor conversion and scaled blitting, and verifies dimensions/pixel values on i386 and ARMHF under `qemu-arm`. |
| `e65aec8` | Prevents ARM compatibility changes from crashing i386. | **Complete:** `abi_cross_test` covers null pointers, aligned output, and deliberately unaligned output on i386 and ARMHF, asserting safe returns and preserved data. |
| `00584b2` | Adds the large i386 ABI-wrapper/raw-body split. | **Complete:** `abi_cross_test` exercises representative split wrappers (`sub_50A5C0` and `sub_531E20`) with sentinel pointers on i386 and ARMHF, comparing the raw-callee payload and result. |
| `e19d204` | Fixes broken ARMHF ABI handling in summon-related code. | **Complete:** `abi_cross_test` verifies pointer transport through `sub_50A5C0` and `sub_5281F0` on ARMHF and i386, covering the corrected call-site/raw-call boundary. |
| `ef42f7a` | Fixes summoning and monster attack-area ABI behavior on ARM and i386. | **Complete:** `abi_cross_test` exercises summon output and the `sub_549860` monster-area callback wrapper, verifying writable output and callback payloads on both targets. |
| `1301939` | Preserves float bit patterns in the mana-drain ABI path. | **Complete:** `mana_abi_bits_test` round-trips normal values, signed zero, NaN, infinity, and subnormal IEEE-754 patterns on i386 and ARMHF. |
| `0d451ef` | Fixes the rare summon segfault by passing a real output pointer on i386 while retaining ARM slot conversion. | **Existing `abi_cross_test`:** raw stub writes through `out_xy`; test verifies the marker on i386 and ARMHF (`qemu-arm`). |
| `3487996` | Adds Linux x86_64 compile guards/compatibility adjustments around the 32-bit code. | **Complete:** `x86_64_compat_test` configures on an x86_64 host with the project’s `-m32` flags and asserts 32-bit pointer/`uintptr_t` sizes. |
| `39e7c16` | Adjusts CMake/toolchain logic to restore ARM builds. | Configure with the ARMHF toolchain file, build the executable, and run `ctest -R abi_cross_test` under ARM emulation. |
| `f7c9535` | Explicitly rejects unsupported 64-bit ARM builds. | Configure with an aarch64 compiler and assert CMake fails with the documented 32-bit-ARM error; configure ARMHF and assert success. |

## Priority

The ABI harnesses (`d70c630` through `0d451ef`) are the highest-value tests:
they catch the pointer-as-float and calling-convention regressions that can
compile cleanly but crash only on one architecture.  The build/runtime tests
then protect the Docker and CMake paths that produce those binaries.
