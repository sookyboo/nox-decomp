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
- `fbd2731` — native graphics low-address compatibility allocations.
- the current checkpoint fixes native window callbacks, font/text resource
  transport, executable metadata parsing, and the startup callback tables.

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
- the 137-entry keybind table and 41-entry action table consumed by
  `sub_42CF50()`, plus its native config-list sidecar;
- the CSF bsearch count and command-token localization tables used during
  startup;
- native x86_64 graphics allocations still consumed through recovered DWORD
  pointer slots: the pixel/row buffers in `sub_4861D0()` / `sub_486230()`,
  the display-state descriptors, gamma tables, and palette lookup table.
  Linux x86_64 uses `MAP_32BIT` for these temporary buffers while preserving
  the original slots and consumers.
- SDL headers are included before the repository-wide packed-record region, so
  `SDL_Surface` keeps its native ABI layout while recovered records remain
  packed.
- the font/resource dispatch table used by `sub_43F1C0()` and `sub_43F360()`;
  native table entries and the callback are shadowed, while loaded font
  resources use low-address allocations for existing DWORD consumers;
- `sub_440900()` indexes the graphics row table with its preserved 4-byte
  stride instead of applying native pointer arithmetic.
- the Modifier.bin parser's 19-entry dispatch table and 0x58-byte records;
  native builds keep handler/name pointers and record links in host-width
  sidecars or low-address allocations while retaining the recovered offsets;
- the seven-entry COLOR name table used by `sub_411C80()`, shadowed natively
  while preserving its 32-bit index result.
- the three-entry Modifier class-name table used by `sub_411E60()` and the
  18-entry damage-type table used by `sub_4E0A00()`;
- the sibling `sub_412ED0()` Modifier.bin record parser, using the same native
  dispatch and low-address record strategy as `sub_412D40()`.
- the generic 0x90-byte Modifier.bin records parsed by `sub_412AE0()`, its
  17-entry property-handler shadow, and the nested lookup/flag tables used by
  the native property handlers; recovered record offsets remain fixed-width.
- the native `.wnd` property path: its 18 handlers receive host-width window
  record pointers, the ACTIVE/type lookup tables use native-width sidecars,
  and the temporary parent-window stack uses a native pointer cursor.
- native window callback, widget font, static-text value, and text-buffer
  sidecars preserve host pointers that the recovered widget records store in
  DWORD slots; `sub_46AF00()`/`sub_46B490()` widen pointer-returning events.
- the fixed-width `gamedata.bin` parser path: native startup keeps the adjacent
  `gamedata.bin` string intact, preserves parser `FILE *` handles, uses
  low-address storage for the 32-bit category manager/table records, and
  decodes their 32-bit name/value entries explicitly.
- the `monster.bin` parser path: native startup keeps its parser `FILE *` and
  recovered 0xF8-byte records below 4 GiB, matching the legacy pointer slots.
- the timer-list walkers and timer-pool allocations used during startup and
  shutdown; fixed-width links are decoded explicitly and native pool records
  use low-address storage.
- startup metadata and callback inputs use host-width transports in
  `sub_4145F0()`/`sub_414B30()`, `sub_47FA80()`, and the main-loop callback
  slots. The legal-window root is also kept in a native pointer sidecar.
- `sub_46B740()` keeps the native input-state address wide across its legacy
  coordinate scratch/restore paths; the window tree's packed DWORD links are
  zero-extended before traversal, rather than being read as host-width
  pointers.
- The shared record-pool callers in `sub_431510()` and `sub_431540()` widen
  their packed DWORD pool-manager slots before calling `sub_4144D0()` or
  `sub_4142F0()`. The pool record layout and its 32-bit links remain
  unchanged.
- the first native render/menu path widens its 9-entry render pointer array,
  3-entry window text pointer table, wrapping buffer, SDL row table, window
  callbacks, menu-tree roots, and low-address button-list records.
- the server-startup audio path: the timer-manager pointer and audio callback
  table are reconstructed at their legacy DWORD-slot boundary; temporary audio
  list nodes and SoundSet index buffers use low-address allocations where the
  recovered list links still store 32-bit pointers; and MSS driver, dialog,
  and music-stream handles are retained in native sidecars. CSF narrow-string
  results and the shared audio-state pointer likewise retain host-width copies
  instead of being reread from truncated DWORD slots.
- the `sub_46AC60()` teardown fields, including the 32-bit value at `a1 + 92`,
  which must be tested as a DWORD rather than as a native pointer;
- the `sub_4A1D80()` menu-button callback slot, reconstructed from its stored
  32-bit value before invocation;
- the input-event cursor used by `sub_437060()`, `map_download_loop()`, and
  the main loop. Native builds keep this temporary cursor in a host-width
  sidecar instead of writing its pointer into the recovered 32-bit image slot.

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
- native x86_64 CTest in `build-amd64`: 25/25 passed;
- the source i386 target still has unrelated pre-existing compile errors in
  `GAME1.c`; the regular-flow comparison therefore uses the existing i386
  binary in `build-i386/src/out`;
- the 32-bit trace reaches `macro end: startMultiplayerNetworkHost`,
  `macro end: newMultiNewCharacterWarrior`, `macro end: chatScreenPopUpClickOk`,
  `macro end: chatScreenServerName`, and then continues into
  `defaultServerGame`;
- native x86_64 reaches the same milestones through
  `macro end: chatScreenServerName`, after opening `gamedata.bin` and
  `monster.bin`; no signal occurs in the verified 35-second diagnostic run;
- a longer native run also reached `chatScreenServerName` completion and then
  remained in the scripted UI/input sequence until its timeout. The next
  unverified boundary is the transition into `defaultServerGame` and map
  startup, not the earlier `sub_42FAE0(a1=0)` teardown failure.

The headless dependencies are installed: `xvfb`, `xauth`, Mesa software
OpenGL support, and `gdb`. The comparison command below remains diagnostic
because a timeout does not yet prove that the native process entered a game;
the current verified assertion is the signal-free progression through
`chatScreenServerName`.

## Reproduce the native server-startup smoke test

Run from the extracted game-data directory:

```sh
cd build-deps/gamefiles/app
timeout --signal=TERM 35s env \
  ALSOFT_DRIVERS=null LIBGL_ALWAYS_SOFTWARE=1 SDL_VIDEODRIVER=x11 \
  NOX_GAMEPAD=0 NOX_NO_INTERNET_SERVERS=0 NOX_UPNP_ENABLE=0 \
  NOX_CONTROL_SERVER=1 NOX_CONTROL_SERVER_PASSWORD=secret \
  NOX_CONTROL_SERVER_BIND=127.0.0.1 NOX_CONTROL_SERVER_PORT=2323 \
  NOX_SKIP_INTRO_MOVIES=1 NOX_CONTROL_SERVER_SLEEP_SCALE=6 \
  'NOX_CONTROL_SERVER_BOOT=sleep 5000; macro server;' \
  NOX_CONTROL_INJECT_LOG=0 NOX_CONTROL_LOG=1 \
  NOX_SERVER_NAME=NoxDecomp NOX_SERVER_SYSOP=secret \
  NOX_SERVER_LESSONS=15 NOX_SERVER_TIME=0 \
  NOX_SERVER_DEFAULT_MAP=capflag NOX_CAPTURE_INPUT=0 \
  NOX_LOBBY_REGISTER_ENABLE=0 \
  xvfb-run -a -s '-screen 0 1280x720x24' \
  ../../../build-amd64/src/out
```

The verified result is a native x86_64 server-mode startup that reaches the
main menu, executes the control-server `startMultiplayerNetworkHost` input
sequence, loads `gamedata.bin` and `monster.bin`, and completes the scripted
`chatScreenServerName` macro without a signal in the diagnostic run. Do not
treat the timeout alone as success; verify the last completed macro and the
process exit reason.
The important native transport boundaries are:

```text
sub_401070
  → sub_43BF10
  → sub_4449D0
  → sub_42EE30 / sub_42F200
  → sub_4862E0
  → sub_424170
  → sub_412D40
  → sub_411C80 / sub_411E60 / sub_4E0A00
  → sub_412ED0
  → sub_4A0D80 / sub_4A1440
  → sub_46C3E0 / sub_46B490
  → sub_46C2E0 / sub_46C370
  → sub_49D190 / sub_49E060
  → sub_488D00 / sub_4892D0
  → sub_4A1C00 / sub_4CC6F0
```

The video fix uses low-address allocations for the recovered 36-byte index
records and frame buffers, a native sidecar for its `FILE *`, and explicit
32-bit decoding when consuming the record table. This preserves the original
record layout while allowing native x86_64 startup to complete that path.

The timer fix widened the `sub_4864A0()` to `sub_4862E0()` record address
transport to `uintptr_t`; the timer record itself remains a fixed-width 32-bit
layout. The SoundSet fix keeps its 19-entry name/field-offset table packed:
native builds use a sidecar for the names while preserving the adjacent 32-bit
field offsets. The CSF fix routes all parser stream accesses through its native
`FILE *` sidecar. The Modifier.bin fix keeps the recovered 0x58-byte record and
19-entry dispatch semantics, using low-address records for legacy DWORD fields
and a native dispatch sidecar; its COLOR lookup similarly shadows only the
pointer table, not the returned index. The class and damage-type lookups use
the same sidecar rule, and `sub_412ED0()` now preserves the same record/list
contract as `sub_412D40()`.
Native shutdown keeps the modifier and property-list heads separate from the
packed image slots and avoids walking fixed-width auxiliary teardown lists
whose adjacent 32-bit slots cannot encode a host pointer; those allocations
are process-lifetime state reclaimed when the headless process exits.

For a backtrace:

```sh
env ALSOFT_DRIVERS=null LIBGL_ALWAYS_SOFTWARE=1 SDL_VIDEODRIVER=x11 \
  NOX_GAMEPAD=0 NOX_NO_INTERNET_SERVERS=1 NOX_UPNP_ENABLE=0 \
  NOX_CONTROL_SERVER=0 NOX_SKIP_INTRO_MOVIES=1 \
  xvfb-run -a -s '-screen 0 1280x720x24' \
  gdb -q -batch -ex 'set pagination off' -ex run -ex bt --args \
  ../../../build-amd64/src/out -serveronly Estate
```

## Recommended next steps

1. Continue the headless smoke test through `defaultServerGame`, map
   selection, and gameplay startup.
2. Run the complete i386 and ARMHF/QEMU CTest suites after the startup path is
   stable.
3. Update `startup-compatibility.md` with the final native audio transport
   boundary if the server path advances further.

Do not treat a timeout as a successful startup by itself: verify that the
process did not exit with SIGSEGV or SIGABRT and record the last completed
startup step.
