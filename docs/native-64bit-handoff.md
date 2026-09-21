# Native 64-bit compatibility handoff

This document is the continuation point for the native Linux x86_64 porting
work. It records the current implementation boundary and the next confirmed
failure so a new session can resume without repeating the startup investigation.

## Repository state

The work is on branch `experiment64`.

Relevant commits:

- `5817288` — native map-transfer file state and ordered chunk buffering;
- `1a4e51c` — native network transfer-record ownership;
- `0594448` — native `load` command handoff into the production handler;
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

## Distribution launcher architecture override

The launchers in `dist-scripts/` keep their existing i386 default on x86_64
hosts. To select the native binary, change the hard-coded
`NOX_FORCE_64BIT="N"` assignment to `NOX_FORCE_64BIT="Y"` in the launcher being
used:

- `start.sh` selects `noxd.x86_64` and the x86_64 dynamic loader/library roots;
- `server.sh`, `startgen.sh`, and `portmasterstart.sh` select
  `noxd.x86_64` through `RUN_ARCH=x86_64`.

The override is only honored when the detected device architecture is
x86_64. ARM mappings remain ARMHF, and 32-bit x86 hosts remain i386. The
PortMaster launcher also limits its `PORT_32BIT` and `/usr/lib32` setup to the
i386 path.

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
- the native `nox_legal_window` sidecar lifecycle: `sub_46C4E0()` clears the
  host-width global when the corresponding recovered window record is freed;
- the transition records used by `sub_4AA270()`/`sub_4AA490()`, which retain
  their host pointers in sidecars, and the 32-bit transition callbacks are
  reconstructed before invocation.
- the network callback registration in `sub_554B40()`: native builds pass the
  `sub_554FF0()` callback through `uintptr_t` into the host-width main-loop
  callback sidecar. The recovered call path previously cast this address to
  `int`, producing a non-canonical jump target from the first multiplayer
  setup network initialization.

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
- native x86_64 CTest in `build-amd64`: 28/28 passed, including the callback
  transport regression and the map-download dispatch regression;
- i386 CTest in `build-i386`: 40/40 passed, including the same map-download
  dispatch regression; the executable is the existing 32-bit target in
  `build-i386/src/out`;
- the 32-bit trace reaches `macro end: startMultiplayerNetworkHost`,
  `macro end: newMultiNewCharacterWarrior`, `macro end: chatScreenPopUpClickOk`,
  `macro end: chatScreenServerName`, and then continues into
  `defaultServerGame`;
- native x86_64 reaches the same milestones through
  `macro end: chatScreenServerName`, after opening `gamedata.bin` and
  `monster.bin`; no signal occurs in the verified 35-second diagnostic run;
- an accelerated native/32-bit comparison reaches `macro end: defaultServerGame`
  and `macro end: server` on both builds, with no native signal reported
  before the controlled timeout;
- the earlier `sub_42FAE0(a1=0)` teardown failure and the later freed-window
  traversal were both fixed;
- the native callback crash at `mainloop()`'s tick callback was reproduced from
  a core dump as a truncated `sub_554FF0()` address registered by
  `sub_554B40()`. After widening that registration, native startup passes the
  complete initialization path and the callback transport regression without
  the invalid jump;
- the control server now exposes `console "..."`, which queues an ASCII command
  for the main-thread production console parser (`sub_443C80()`); on native
  builds, the `load` command bridges its 32-bit argv contract through low
  memory into `sub_4432B0()`, then preserves the server-side map-transition
  flag when the standalone host has no matching server-list entry;
- the native diagnostic sequence reaches and logs
  `map_download_start()`, then opens `window/mapdnld.wnd`; this verifies the
  startup-to-map-dispatch boundary rather than only macro completion;
- the map-download window render callback now uses native sidecars for the two
  recovered DWORD line-drawing callback slots; a 40-second bounded native run
  remains alive in the map-download screen after opening `mapdnld.wnd`;
- native network connection records created by `sub_553000()` now keep their
  recovered four-byte pointer slots valid through low-address allocation, and
  the map-download regression covers constructor/destructor cleanup before
  dispatching a high-bit transfer packet;
- the map-file consumer now keeps native `FILE *`/path state in sidecars and
  uses low-address storage for its recovered chunk queue; the regression
  drives the production `sub_48EA70()` `0xB8`/`0xB9` message cases and verifies
  non-FIFO chunks (`3,2,1`) are searched and written in sequence before
  finalization. Both the recovered 32-bit queue and the native sidecar queue
  now search for the next expected sequence, and the same fixture passes on
  i386 and x86_64;
- the native map-loader crash after transfer was traced to the startup table:
  its three recovered four-byte pointer slots at offsets `173412`, `173416`,
  and `173420` were widened to eight-byte writes, corrupting the adjacent
  strings and later causing `sub_4AC2B0()` to dereference an invalid pointer.
  Native startup now leaves those legacy writes disabled, and the loader reads
  the underlying in-blob strings directly; the i386 pointer-table behavior is
  unchanged;
- the next native map-loader crash was the same ABI issue in the 16-entry map
  section dispatch table at `byte_587000[70168]`: its 4-byte name/handler
  records were being populated with host-width pointers. Native code now uses
  a host-width sidecar for `sub_426E20()`, `sub_426EA0()`, and
  `sub_426F40()`, while i386 retains the recovered table. A headless GDB probe
  against the stock built-in `CapFlag.map` now passes the header and
  `ObjectData` dispatch and returns success from `sub_4AC2B0()`;
- a combined native probe copied the stock `CapFlag.map` into a temporary
  fixture, delivered the exact stock `CapFlag.nxz` through
  `sub_4ABAD0()`/`sub_4AB7C0()` in 1,024-byte chunks, verified the finalized
  package byte-for-byte, and then loaded the fixture map successfully through
  `sub_4AC2B0()`. The temporary fixture was removed afterward; interactive
  gameplay and peer-supplied transfer remain separate coverage.
- a rebuilt native executable was exercised through the real
  `map_download_loop()`/`map_download_finish()` path with the exact stock
  `CapFlag.nxz` bytes fed through `sub_4AB7C0()`: all 79,398 bytes were written
  and finalized. Because `.nxz` is the companion script package rather than the
  map file, that transfer-only probe is not map activation evidence. The
  original `CapFlag` files were restored byte-for-byte after the probe. The
  temporary GDB packet injector still stalls for larger synthetic calls,
  although the production parser fixture covers 1,024-byte `0xB9` payloads on
  both architectures;
- the earlier direct codec comparison accidentally used the companion stock
  `.nxz` script package rather than the `.map` file consumed by
  `sub_4AC2B0()`. Because `.nxz` begins with its uncompressed payload length,
  its decoded first word is not evidence about the map-loader header. That
  comparison is discarded; future codec tests must use the stock built-in
  `.map` or a self-contained map fixture, without relying on a reloaded/EUD
  map.
- the callback transport regression invokes the production tick-callback
  storage on both architectures: the native build uses its host-width
  sidecar, while i386 uses the recovered callback slot;
- the native server-startup smoke now reaches `defaultServerGame` and the end
  of the scripted `server` macro after loading `gamedata.bin` and
  `monster.bin`; this verifies the native path through the server gameplay
  handoff without a signal before the controlled timeout;
- this still does not prove a complete peer-supplied map transfer. The
  map-dispatch smoke reaches `map_download_start()` and renders
  `window/mapdnld.wnd`, the deterministic transfer regression verifies the
  production parser and file consumer with ordered chunks, and the bounded
  native runtime probe now verifies the finalized file reaches the loader
  without the former native pointer crash. Successful map activation and a
  complete network transfer assertion still require a supplied map service or
  fixture.

The headless dependencies are installed: `xvfb`, `xauth`, Mesa software
OpenGL support, and `gdb`. The comparison command below remains diagnostic
because a timeout does not prove that the native process entered a game. The
map-dispatch smoke additionally enables `NOX_TRACE_MAP_DOWNLOAD=1` and checks
for `map_download_start()` followed by `window/mapdnld.wnd`.

To compare the focused transfer boundary on both architectures:

```sh
ctest --test-dir build-i386 -R map_download_dispatch_test --output-on-failure
ctest --test-dir build-amd64 -R map_download_dispatch_test --output-on-failure
```

## Reproduce the native server-startup smoke test

Run from the extracted game-data directory:

```sh
cd build-deps/gamefiles/app
timeout --foreground --signal=TERM 35s env \
  ALSOFT_DRIVERS=null LIBGL_ALWAYS_SOFTWARE=1 SDL_VIDEODRIVER=x11 \
  NOX_GAMEPAD=0 NOX_NO_INTERNET_SERVERS=0 NOX_UPNP_ENABLE=0 \
  NOX_CONTROL_SERVER=1 NOX_CONTROL_SERVER_PASSWORD=secret \
  NOX_CONTROL_SERVER_BIND=127.0.0.1 NOX_CONTROL_SERVER_PORT=2323 \
  NOX_SKIP_INTRO_MOVIES=1 NOX_CONTROL_SERVER_SLEEP_SCALE=6 \
  'NOX_CONTROL_SERVER_BOOT=sleep 5000; macro server;' \
  NOX_CONTROL_INJECT_LOG=0 NOX_CONTROL_LOG=1 \
  NOX_SERVER_NAME=NoxDecomp NOX_SERVER_SYSOP=secret \
  NOX_SERVER_LESSONS=15 NOX_SERVER_TIME=0 \
  NOX_SERVER_DEFAULT_MAP=Estate NOX_CAPTURE_INPUT=0 \
  NOX_LOBBY_REGISTER_ENABLE=0 \
  xvfb-run -a -s '-screen 0 1280x720x24' \
  ../../../build-amd64/src/out
```

The verified result is a native x86_64 server-mode startup that reaches the
main menu, executes the control-server `startMultiplayerNetworkHost` input
sequence, loads `gamedata.bin` and `monster.bin`, and completes the scripted
`server` macro through `defaultServerGame`. For the map-dispatch assertion,
use `NOX_CONTROL_SERVER_SLEEP_SCALE=0.1`, a `sleep 30000` bootstrap delay, and
`console "load Estate"`; set `NOX_TRACE_MAP_DOWNLOAD=1` and verify
`[map] map_download_start` and the subsequent `window/mapdnld.wnd` open. Do not
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

1. Supply a map-service fixture or equivalent automated network peer so the
   map-download packet path can be verified through gameplay state.
2. Run the complete i386 and ARMHF/QEMU CTest suites after the startup path is
   stable.
3. Update `startup-compatibility.md` with the final native audio transport
   boundary if the server path advances further.

Do not treat a timeout as a successful startup by itself: verify that the
process did not exit with SIGSEGV or SIGABRT and record the last completed
startup step.
