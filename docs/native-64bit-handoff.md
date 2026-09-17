# Native 64-bit compatibility handoff

This document is the continuation point for the native Linux x86_64 porting
work. It records the current implementation boundary and the next confirmed
failure so a new session can resume without repeating the startup investigation.

## Repository state

The work is on branch `experiment64`.

Relevant commits:

- `422035f` — native 64-bit startup pointer-table and map/CSF compatibility
  fixes;
- `f355655` — architecture, sandbox, and startup compatibility documentation.

The untracked file `0001-bot-native-player-bot-combined.patch` is user-owned and
must not be modified, staged, or deleted.

## Current objective

Make the decompiled game executable run through startup on native 64-bit Linux
without changing the recovered 32-bit Nox record layouts, while retaining the
normal i386 and ARMHF behaviour.

The governing rule is:

```text
host API/callback/temporary pointer transport → widen or shadow it
recovered record/global/file/wire layout      → preserve fixed-width fields
```

The general rationale is in
[`architecture-compatibility-tests.md`](../architecture-compatibility-tests.md).
The startup subsystem description is in
[`startup-compatibility.md`](startup-compatibility.md).

## Completed native fixes

The current branch contains native-width handling for:

- timer callback storage and invocation;
- `argv` transport in `sub_401070()` and `sub_4357D0()`;
- CSF file, record, and string-pointer state in `sub_40F300()` and
  `sub_40F830()`;
- native UTF-16-to-host-`wchar_t` conversion for CSF strings;
- map file streams and map-directory links in the startup map scan;
- file-enumeration handles at the `FindFirstFileA`/`FindNextFileA`/
  `FindClose` compatibility boundary;
- the 0x3ff built-in string index used by `sub_40AED0()`;
- the six-entry startup config dispatch table used by `sub_431890()` and
  `sub_4332E0()`.

The 32-bit branches retain the original fixed offsets and pointer-slot
layouts. Do not globally change `HANDLE` or convert all `_DWORD` fields to
pointer-sized types; those changes would alter the compatibility ABI.

## Validation status

Confirmed before the latest startup changes:

- native x86_64 CTest: 25/25 passed;
- i386 CTest: 27/27 passed;
- ARMHF/QEMU CTest: 27/27 passed.

After the latest source changes:

- native x86_64 target `out` builds successfully;
- i386 target `out` builds successfully;
- native x86_64 headless startup no longer fails in the timer, argv, CSF
  allocation, map scan, built-in string sort, or startup config dispatch
  boundaries;
- the full post-change CTest suites still need to be rerun.

The headless smoke test uses `Estate` because it is a known-working map. The
latest run with `-serveronly Estate` still reaches the same config-parser
SIGSEGV before gameplay map selection, so the map choice has not yet changed
the failing boundary.

## Reproduce the remaining failure

Run from the extracted game-data directory:

```sh
cd build-deps/gamefiles/app
timeout --signal=TERM 20s env \
  ALSOFT_DRIVERS=null LIBGL_ALWAYS_SOFTWARE=1 SDL_VIDEODRIVER=x11 \
  NOX_GAMEPAD=0 NOX_NO_INTERNET_SERVERS=1 NOX_UPNP_ENABLE=0 \
  NOX_CONTROL_SERVER=0 NOX_SKIP_INTRO_MOVIES=1 \
  xvfb-run -a -s '-screen 0 1280x720x24' \
  ../../../build-linux64/src/out -serveronly Estate
```

The current result is a native x86_64 SIGSEGV in the config-line parser:

```text
sub_401070
  → sub_4317B0
  → sub_4331E0
  → sub_42CF50
```

The confirmed crashing operation is a `strcmp()` in `sub_42CF50()`. The parser
uses recovered pointer tables around `byte_587000[73652]` and
`byte_587000[73672]`; those tables are still accessed as 32-bit packed data
after `init_data()` has installed native pointers elsewhere. The exact shadow
record shape and all consumers must be confirmed from the i386 layout before
changing the x64 path. This is the next owning boundary to investigate, not a
reason to widen the surrounding configuration records globally.

For a backtrace:

```sh
env ALSOFT_DRIVERS=null LIBGL_ALWAYS_SOFTWARE=1 SDL_VIDEODRIVER=x11 \
  NOX_GAMEPAD=0 NOX_NO_INTERNET_SERVERS=1 NOX_UPNP_ENABLE=0 \
  NOX_CONTROL_SERVER=0 NOX_SKIP_INTRO_MOVIES=1 \
  xvfb-run -a -s '-screen 0 1280x720x24' \
  gdb -q -batch -ex 'set pagination off' -ex run -ex bt --args \
  ../../../build-linux64/src/out -serveronly Estate
```

## Recommended next steps

1. Inspect `sub_42CF50()` completely, including every table access beginning
   at offsets `73652` and `73672`.
2. Run the equivalent i386 executable under GDB at `sub_42CF50()` and dump the
   table entries, record stride, terminator, and integer/function values.
3. Introduce the smallest x64 shadow table at the owning config-dispatch
   boundary. Keep the i386 branch byte-for-byte in layout and semantics.
4. Rebuild `build-linux64/src/out` and repeat the headless smoke test. If it
   reaches another fault, use the first project frame in the GDB backtrace to
   identify the next packed pointer boundary.
5. Run the complete native x64, i386, and ARMHF/QEMU CTest suites after the
   startup path is stable.
6. Update `startup-compatibility.md` with the final table shape and remove or
   revise this handoff's known-failure wording once the path is fixed.

Do not treat a timeout as a successful startup by itself: verify that the
process did not exit with SIGSEGV or SIGABRT and record the last completed
startup step.
