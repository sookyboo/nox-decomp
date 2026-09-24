# Native 64-bit compatibility handoff

This document is the continuation point for the native Linux x86_64 porting
work. The current validated boundary is recorded first; older checkpoints
below are historical and are superseded where they disagree with that status.

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

The untracked patch and GDB scripts in the current worktree are local/user-owned
artifacts; keep them out of source commits unless explicitly requested.

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
- the UI click/event callback at recovered window offset `+384`, assigned by
  `sub_46B070()` or the fourth argument of `sub_46B430()`. Native builds pass
  these function addresses through a host-width sidecar before the mouse
  dispatcher invokes them; the recovered DWORD remains present for the i386
  ABI. The focused map-dispatch test covers both assignment entry points and
  sidecar cleanup. This is a callback transport fix, not evidence that the
  server-startup macro now reaches a connected gameplay state.
- the input-event cursor used by `sub_437060()`, `map_download_loop()`, and
  the main loop. Native builds keep this temporary cursor in a host-width
  sidecar instead of writing its pointer into the recovered 32-bit image slot.
- the native `nox_legal_window` sidecar lifecycle: `sub_46C4E0()` clears the
  host-width global when the corresponding recovered window record is freed.
  Control-server caption scans use this sidecar on native builds, but must use
  the recovered DWORD slot at `byte_5D4594[1522892]` on i386: that slot is
  cleared by the shared root-teardown hook, while the native-global cleanup is
  intentionally compiled only for host-width builds. Reading the i386 global
  after teardown retained a freed pool record and dispatched through its
  `0xAC` fill pattern during the next caption scan;
- the transition records used by `sub_4AA270()`/`sub_4AA490()`, which retain
  their host pointers in sidecars, and the 32-bit transition callbacks are
  reconstructed before invocation. The same callback sidecar applies to the
  menu and map-download transition records created by `sub_4A1C00()` and
  `sub_4AA6B0()`; `nox_game3_pointer_from_32()` is a data-pointer decoder and
  must not be used to reconstruct host function addresses.
- the connect/host transition record used by `sub_4379F0()`,
  `sub_438330()`, and `sub_43B460()`. Its `+48`, `+52`, and `+56` callback
  fields also use the native transition sidecar; the recovered DWORD slots
  remain compatibility copies for i386 and low-address consumers.
- `sub_4379F0()` builds the NoxWorld server-browser window during the
  `sub_4AA490()` transition, initializes its controls, and sets the status
  text on widget 10011 from a localized string returned by `sub_40F1D0()`.
  Message 16385 routes that string through `sub_46B490()` to the static-text
  callback and ultimately the `sub_488D00()` renderer. The text argument must
  remain `uintptr_t` on native builds in both `sub_4379F0()` and the later
  status update in `sub_4383A0()`: casting it to `int` truncated the localized
  pointer and caused `sub_43F840()` to fault while rendering the
  server-browser transition. The cast remains ABI-equivalent on i386.
- the control-server caption scanner receives the result of message 16413
  (`get text`) as a host-width pointer from `sub_46B490()`. Keeping this result
  in `uintptr_t` is required while scanning the animated `Please wait` dialog;
  truncating it to `unsigned int` can make the scanner dereference an unrelated
  low address before the next menu caption becomes available.
- `sub_449A10()` creates or updates the modal dialog, records the active dialog
  root/callback state, and sends its optional title/body strings to widgets
  4005 and 4004 through message 16385. Its `a2` and `a3` inputs are text
  pointers, not integers; callers such as `sub_4378B0()` must preserve them as
  `uintptr_t` so the visible `Please wait` body remains valid during the host
  transition. The `a4` mode is forwarded to `sub_449EA0()`; its exact meanings
  remain reverse-engineered.
- the network callback registration in `sub_554B40()`: native builds pass the
  `sub_554FF0()` callback through `uintptr_t` into the host-width main-loop
  callback sidecar. The recovered call path previously cast this address to
  `int`, producing a non-canonical jump target from the first multiplayer
  setup network initialization.
- the sound-object link at `sub_43EFD0()` offset `+272`: native teardown now
  zero-extends the recovered DWORD before ending the sample. Reading this
  field as a host pointer consumed the adjacent DWORD and crashed while the
  server transition rebuilt `MainMenu.wnd`; this was observed after the
  scripted server macro completed, before map activation.
- the sample EOS callback in `sub_43EDB0()` and the sound-state callback at
  `sub_4BD9B0()` offset `+144`: both are recovered code-pointer DWORDs and
  must be reconstructed before native invocation. The transition probe
  reached the first fix and then exposed the second callback as the next
  truncated jump target.

The 32-bit branches retain the original fixed offsets and pointer-slot
layouts. Do not globally change `HANDLE` or convert all `_DWORD` fields to
pointer-sized types; those changes would alter the compatibility ABI.

## Current native pointer and menu-flow status (2026-09-24)

The native Linux x86_64 build now passes the previously reproduced setup
crashes in property lookup, map-rule parsing, multiplayer-root lookup, and
legacy pool-manager access. The fixed-width property/modifier lists used by
`sub_413250()` / `sub_413270()` retain their DWORD links while native builds
keep authoritative list heads at host width; traversal decodes each recovered
link. `sub_415C00()` likewise decodes the thing's `+692` property-state slot
and its recovered callback address before use. `init_data()` writes the six
pointer-table entries as DWORDs, preserving the adjacent `UserColor1` key.

During host setup, `sub_428CD0()` consumes map/rule section data and passes
addresses of in-memory fields and decoded string pointers to
`sub_4F5580()`. Those are process addresses, not serialized 32-bit values, so
the callee's field-address parameter and these call sites now use `uintptr_t`;
the string pointer stored in the legacy parser record is decoded from one
DWORD. The fixed-width pool-manager globals used by `GAME4.c` and `GAME5.c`
are similarly decoded before calling the pool allocation, release, and list
operations. These changes preserve the recovered record/global layout on
i386. The packet-send helper `sub_4E5390()` also receives stack-buffer
addresses at host width, including from its callers in `GAME3.c`.

The UI-only macro comparison is still not complete on x64. With identical
native Linux binaries' inputs and `nox.cfg`, i386 raises setup-flow flag
`0x800000`, dispatches event 31 via `sub_4DD180()`, creates
`window/ServOpts.wnd` through `sub_457500()`, and reaches server-name entry and
Escape. x64 injects its configured Chat Area popup click, but the trace does
not independently confirm that the modal closes; it does not raise that flag
or open `ServOpts.wnd`. `sub_43DEB0()` therefore skips its gated
validation/event path and the wait for widget 10101 times out. Source has
candidate flag setters in local setup `sub_435CC0()` and network packet case
`0x2B`, but the expected missing x64 path is not yet identified. Do not force
the flag or event as a workaround.

Both probes use Linux executables (`build-amd64/src/out` and
`build-i386/src/out`), a disposable game-data copy, and
`VideoMode = 1024 768 16`, `Fullscreen = 0`, `VideoSize = 75`; Xvfb is
`1024x768x24` and describes only the host display. The bounded x64 run remains
alive without SIGSEGV until its 120-second timeout, but does not complete the
macro. `defaultServerGame`, F1/console commands, and explicit target-map
selection are not part of this comparison. The engine's routine host setup
opens built-in `So_Druid` data; this is incidental initialization, not a
selected gameplay test map. Do not use a map requiring reloaded EUD support
for native 64-bit testing.

Both native targets build successfully. Full CTest passes on amd64 (30/30) and
i386 (41/41). The focused `native_fixed_pointer_test` checks that a recovered
pointer slot reads exactly one DWORD and decodes independently of the adjacent
DWORD.

An earlier standalone startup checkpoint (separate from the current menu-flow
comparison) preserved the fixed 16-byte
`sub_40ABF0()` file-reader record (four DWORD fields), reconstructs its cursor
fields during the `thing.bin` family parsers, and keeps related fixed-width
tables and video parser state below 4 GiB or in native sidecars. With stock
`CapFlag`, native startup now completes `sub_415470()`, `sub_430190()`,
`sub_4101D0()`, `sub_410F60()`, and `sub_431390()`, then enters the first
gameplay tick in that diagnostic run. Its remaining divergence was later in
the video/bag parser's
recovered 32-bit object layout; it is not a map-transfer or reloaded/EUD-map
failure.

## Validation status (historical checkpoints below are superseded above)

Multiplayer setup probe (2026-09-24): the native `build-amd64/src/out` binary
builds and the guarded `multiplayerHostMenusBeforeGo` run now gets past the
earlier profile-file and host-player initialization faults. The trace reaches
the `New` control, selects Warrior, accepts class and character setup, then
passes player-list traversal and meter reset in `sub_4D22B0()` /
`sub_4EF7D0()`, and the seven-byte meter update through `sub_4D85C0()` /
`sub_4E5030()`. It also passes `sub_4D88C0()` after resolving player-info
through the auxiliary record's DWORD slot. The settings lookup in
`sub_4EF580()` now passes after correcting its 12-byte and 24-byte fixed-record
key reads. The nine-byte stack packet from `sub_4EFC30()` now reaches
`sub_4E5030()` without pointer truncation. The next failure was an x64-width
overwrite in `init_data()`: a native pointer store at `byte_587000 + 206396`
changed the adjacent `UserColor1` text at `+206400` to `UU`, so
`sub_413290()` returned sentinel 255. The six recovered DWORD pointer slots
are now written at their original width, and `sub_4EF7D0()` reads the selected
name through a fixed-width image-pointer decode. The regression test passes,
and GDB confirms the intact key resolves to property ID 179. The trace then
advances to a distinct segmentation fault in `sub_413270()` while resolving
`StreetPants` through `sub_4E3810()`; that lookup reads the property-list head
from `byte_5D4594 + 251608` and follows the recovered `+80` link. The exact
native head/link decode remains under investigation. The Chat Area popup and
server-name/Escape steps are not reached, and the macro has not completed.

The trace has verified progression through the preceding boundaries:
`sub_422140()` receives the full player-info pointer; reliable-update queue
removal in `sub_420A90()` uses its recovered DWORD node layout and
`sub_4209C0()` decodes the pool-manager slot; `sub_48EA70()` keeps the packet
cursor host-width through `sub_4C9BF0()`; and `sub_519830()` receives its
48-byte player-update record without pointer truncation. The `sub_4D22B0()`
list cursor and thing/auxiliary pointers are host-width, and the thing's `+556`
meter slot is decoded before its words are accessed in the player reset path.
The stack buffer from `sub_4D85C0()` is also passed to `sub_4E5390()` without
truncation now, and `sub_4D88C0()` resolves player-info via its pointer helper.
The `sub_415840()` and `sub_415CD0()` setting lookups also compare their
recovered DWORD fields rather than using host pointer arithmetic. These are
verified partial startup fixes, not a verified end-to-end menu or map-start
fix. `init_data()` now preserves the `UserColor1` text adjacent to the six
fixed DWORD pointer slots, and the `sub_4EF7D0()` lookup resolves it as property
ID 179. The immediate failing path is the property-list traversal in
`sub_413270()` during the `StreetPants` reset.

The rebuilt i386 executable completed `multiplayerHostMenusBeforeGo` through
server-name entry and Escape. Its `sub_413270(1133)` breakpoint returned a
valid property node, while the x64 probe faults at the same lookup. This is a
same-source architecture comparison and points to the fixed-width list access
as the next x64 issue. Neither run invokes `defaultServerGame` or loads a map.
The disposable game directory's `nox.cfg` requests `VideoMode = 1024 768 16`,
`Fullscreen = 0`, and `VideoSize = 75` (windowed 1024x768x16 game mode,
rendered at 75% size); Xvfb is `1024x768x24`, only the host display. The
64-bit probe uses the native Linux executable under GDB, not the Windows
version. No map requiring reloaded EUD support is used.

The path changes found earlier are distinct: on native builds, a freed window
record could be reused while its draw/message/event callback sidecars still
pointed at the old callbacks. The final message-2 teardown dispatch now runs
before all sidecars for that record are cleared and the record is returned to
the pool. The regression creates, destroys, and reuses a real window-pool
record, checking that its stale draw callback is not called. Other focused
regressions verify pointer-width preservation through window messages and
caption lookup. The UI-only runtime probe remains necessary because these
tests do not cover the SDL event loop, animation timing, or profile-file path.

For temporary startup/UI/input tracing, configure with
`-DNOX_TRACE_STARTUP_FLOW=ON`. It enables `[flow]` stderr records for the
effective video request/window size, macro wait/click and modal transitions,
injected input queue events, and the console-command dispatch. The option is
off by default, so ordinary builds compile these diagnostic call sites out.
`NOX_CONTROL_LOG=1` remains useful without this option for the control server's
concise `Please wait` modal appear/disappear records.

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
- an earlier native x86_64 trace reached the milestones through
  `macro end: chatScreenServerName`, after opening `gamedata.bin` and
  `monster.bin`; no signal occurred in that 35-second diagnostic run;
- an earlier accelerated native/32-bit comparison reached `macro end:
  defaultServerGame` and `macro end: server` on both builds, but that is not
  the current reproducible boundary and must not be used as proof of gameplay
  initialization. With normal timing, native reaches `window/ArnaMain.wnd`
  and remains in the transition until the bounded timeout; the i386 runtime
  comparison is being rebuilt separately from the native 64-bit investigation;
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
- for that standalone diagnostic, the native bridge sets the `0x100000`
  transition flag with `sub_40A4D0()` after the recovered handler finds no
  matching server-list entry. This deliberately reaches the production
  map-download window; it is not evidence that a connected server/client
  state machine has completed its registry/object-table setup;
- the native diagnostic sequence reaches and logs
  `map_download_start()`, then opens `window/mapdnld.wnd`; this verifies the
  startup-to-map-dispatch boundary rather than only macro completion;
- the map-download window render callback now uses native sidecars for the two
  recovered DWORD line-drawing callback slots; a 40-second bounded native run
  remains alive in the map-download screen after opening `mapdnld.wnd`;
- the current native transition-callback correction builds on both targets and
  preserves the direct `CapFlag` map-download checkpoint. The accelerated
  server macro still returns to repeated `window/MainMenu.wnd` loads after
  `defaultServerGame`; this is the next gameplay-handoff boundary, not a
  verified connected-game fix. The i386 comparison still stops earlier at its
  known `sub_4101D0()` startup fault.
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
  against the stock built-in `CapFlag.map` now passes the header and reaches
  `ObjectData` dispatch, but the direct native checkpoint has not populated the
  object-ID table required by `sub_4AC610()`. The corresponding i386 runtime
  probe stops earlier at its known `sub_410160()`/`sub_4101D0()` startup fault
  and does not reach this callback;
- a combined native probe copied the stock `CapFlag.map` into a temporary
  fixture, delivered the exact stock `CapFlag.nxz` through
  `sub_4ABAD0()`/`sub_4AB7C0()` in 1,024-byte chunks, verified the finalized
  package byte-for-byte, and then reached the same unpopulated object-ID table
  while loading the fixture through `sub_4AC2B0()`. The temporary fixture was
  removed afterward; interactive gameplay and peer-supplied transfer remain
  separate coverage.
- the object-ID table at `byte_5D4594[741676]` is also a recovered four-byte
  pointer slot. Native allocation now uses a low-address mapping and all
  pointer reads reconstruct the address from the four-byte slot; i386 keeps
  `malloc()` and the original pointer-width behavior. This removes the native
  crash in `sub_42C2B0()` and exposes the shared lifecycle precondition instead
  of masking it;
- the native gameplay-initialization probe then exposed the same ABI pattern in
  the legacy thing/modifier setup: `sub_56F1C0()` list nodes, `sub_4E2B60()`
  thing records, the `motd.txt` buffer in `sub_4463E0()`, and several recovered
  static pointer slots were being widened or reloaded as host-width pointers.
  Native allocations and pointer reads now preserve those four-byte contracts.
  The probe reaches the same `sub_4D7C60()` registry precondition as i386;
  it is not a successful gameplay-session result because the direct checkpoint
  does not run the preceding `sub_4D1630()` registry setup.
- the direct stock `CapFlag` startup comparison then isolated another native
  loader boundary in `sub_424170()`: SoundSet records and names were allocated
  with ordinary host-width `calloc()`/`malloc()` but retained in recovered
  four-byte list links. Native code now uses the existing low-address legacy
  allocator for those records and frees them with the matching size-aware
  path; i386 keeps the original layout and allocator. After this fix amd64
  completes `sub_415470()` and reaches `window/MainMenu.wnd`, matching the
  i386 boundary. The later i386 `sub_4101D0()` crash remains a separate,
  pre-existing comparison point.
- the next transfer-to-loader probe isolated the log stream in
  `sub_451630()`: it opened `log` and stored the host-width `FILE *` in the
  recovered four-byte slot at `byte_5D4594[839880]`. Native
  `sub_4515B0()`/`sub_4517A0()` and `sub_4516C0()` now use a pointer-sized
  `nox_log_file` sidecar, while i386 preserves the recovered slot. The native
  startup comparison now passes `sub_4101D0()` and opens `window/MainMenu.wnd`;
  the i386 comparison still stops at its known later `sub_4101D0()` fault.
- the stock map-transfer probe then exposed four more recovered-pointer
  boundaries. The compressed-video input passed to `sub_578C10()`/
  `sub_57EA80()` is now `uintptr_t`; their fixed-layout cleanup paths rebuild
  low 32-bit allocation addresses before freeing them. The screenshot buffer
  owned by `sub_430DB0()`/`sub_430EC0()` uses a native sidecar, while the map
  object tables owned by `sub_4AF8D0()`/`sub_4B0220()` and the font/range table
  owned by `sub_4AEDF0()`/`sub_49F500()` use low-address storage because their
  consumers still read recovered DWORD slots. The i386 paths retain the
  original allocations and slot behavior.
- With those fixes, a native GDB probe using the stock built-in `CapFlag.map`
  and an exact-size transfer reaches the existing map-loader fatal
  error/reporting path without a native SIGSEGV. The probe is diagnostic: it
  does not claim successful map activation, and it does not use a reloaded-EUD
  map.
- A follow-up probe uses the exact stock `CapFlag.nxz` package, restores the
  stock `.map` after `sub_4ABAD0()` removes it, and reaches the production
  `ObjectData` callback. The callback returns the same failure when the direct
  checkpoint has no populated object-ID table. Calling the full gameplay
  initializer from that checkpoint is not a valid workaround: it faults in a
  separate precondition before map parsing. The remaining work is therefore
  to reach the normal registry/object-table lifecycle before activation, not
  to bypass the map loader or widen another recovered field.
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
- the clean native server UI trace enters `sub_4D1630()` with both server-mode
  flags set, but the direct `load CapFlag` checkpoint does not enter
  `sub_4D1660()` or `sub_42BF10()` first. Calling `sub_4D1660()` manually at
  that point faults in `sub_414DB0()` because later gameplay preconditions are
  absent; this is diagnostic evidence, not a production workaround. The
  object table must be created by the normal state-machine ordering;
- the latest accelerated native trace reaches the same server setup boundary,
  opens `window/ArnaMain.wnd`, and then remains before any
  `CONNECT_RESULT`, `sub_4D1660()`, or `sub_42BF10()` call. The scripted UI
  macro therefore does not currently establish a positive connection-result
  transition; do not treat a bounded timeout at this point as successful
  gameplay initialization;
- with `NOX_CONTROL_SERVER_SLEEP_SCALE=0.1`, the complete built-in `server`
  macro now reaches and logs the ends of `startMultiplayerNetworkHost`,
  `newMultiNewCharacterWarrior`, `chatScreenPopUpClickOk`,
  `chatScreenServerName`, and `defaultServerGame` without a native signal.
  After the macro completes, the native trace repeatedly reloads
  `window/MainMenu.wnd` instead of entering map activation. This is a
  state-machine/UI handoff boundary, not evidence that `CapFlag` was loaded.
  The native `-serveronly CapFlag` entry point likewise reaches the server
  loop and remains alive without entering the client map-download path.
- a transition-tick trace confirms that this bounded stop is not an idle
  main-loop scheduler: `sub_43C380()` continues to run and the active
  low-address transition records advance through their normal state values.
  The older list head can remain at state `0` while newer server-screen
  records are active, so inspecting only the head is insufficient evidence
  that `sub_4AA450()` was skipped. Future probes must enumerate the full
  transition list before attributing the stop to transition dispatch;
- enumerating the list immediately after `sub_4AA270()` creates the server
  screen shows both new low-address records and their populated `+48`/`+56`
  transition callbacks. This rules out missing callback installation at the
  creation boundary; the next probe must cover the subsequent UI event or
  control-macro progression that changes those records from their initial
  state.
- the callback trace separates two event paths after the handoff: the
  MainMenu `16391` events arrive through the generic `sub_4A7F50()`/window
  dispatcher, while `sub_4AA4D0()` receives the server-screen construction
  events (`1` and `22`). No server-screen `16391` was observed after
  `window/ArnaMain.wnd` opened, including a post-load control-server click;
  breakpoints on the generic dispatcher alone therefore cannot prove that the
  server-screen action was accepted.
- the native UI handoff itself is now traced through the production callback
  chain: MainMenu widget 112 dispatches event `16391` to the MainMenu
  callback, `sub_4D1630()` returns success, and transition callback
  `sub_4AA270()` runs. Break on `sub_46B490()` to observe this event;
  `sub_46B4C0()` is the separate message-callback path and will not show the
  button's `16391` dispatch. The remaining native boundary is after
  `window/ArnaMain.wnd` opens, before the positive connection-result/gameplay
  state; do not inject state to bypass that ordering;
- a control-server click sent immediately after startup can be consumed by
  the temporary legal/overlay window (widget `9901`) rather than by the
  MainMenu. Its native draw callback decrements the recovered countdown from
  300 to 0, after which the same `(250,165)` click reaches the normal
  `16391` dispatch. A failed early click is therefore a probe-timing failure,
  not evidence that the native hit-test or MainMenu callback is broken;
- the built-in `startMultiplayerNetworkHost` macro now waits long enough after
  the MainMenu handoff for `window/ArnaMain.wnd` to finish loading and clicks
  its native-LAN button 421 rectangle `(237..403,256..291)` at `(300,270)`;
  the local `y=16..51` coordinate is offset by parent 420's global `y=240`.
  The previous macro used this coordinate before the screen had finished
  loading, while the intermediate `(300,200)` WOLAPI click entered the
  platform-specific `sub_40CE60()` path that is unavailable on native Linux.
  With the corrected timing and LAN coordinate, the native trace reaches the
  server-screen `16391` dispatch without selecting WOLAPI or using Windows;
- `noxworld.wnd` places the local Host Game button 10002 at rectangle
  `(40..190,49..81)` inside ArnaMain's lower panel 420, whose global y origin
  is 240. Its effective global rectangle is therefore `(40..190,289..321)`;
  the macro uses `(60,300)`, not the file-local `(60,60)`. It waits three
  scaled seconds after `noxworld.wnd` opens before that click, and three more
  scaled seconds after it, allowing the native host transition to consume the
  action before the character/map steps begin.
- `sub_4AA270()` creates the server gameplay screen and stores its root in the
  recovered DWORD slot at `byte_5D4594[1309716]`. On native builds that root is
  a host-width window pointer, so the slot is only a compatibility copy;
  `nox_server_screen_root` is the authoritative sidecar used by the child
  lookup/event calls and by `sub_4AA490()` when the transition closes the
  screen. i386 continues to use the recovered slot directly. This is the same
  lifecycle boundary as the existing menu-root sidecars and must be preserved
  when adding another server-screen callback or teardown path;
- `sub_4AA450()` is the transition-state callback installed by
  `sub_4AA270()`. Its native branch must update the two transition records
  through `nox_server_transition_left/right`; using the recovered slots at
  `1309708` and `1309712` would dereference truncated record addresses.
  The related `sub_4AA4D0()` event callback must use those same sidecars when
  checking or replacing the transition callback, and must post its dialog
  through `nox_server_screen_root`; the i386 branches retain the recovered
  layout.
- `sub_4AA270()` must also initialize the transition state and callbacks
  through those native transition sidecars. The recovered DWORD slots are
  compatibility copies only; dereferencing them during creation works by
  accident when an allocation is below 4 GiB and faults when the transition
  record is above it. The i386 path continues to initialize the recovered
  records directly.
- `sub_4E2B60()` is the gameplay `thing.bin` initializer: it creates the
  native object records, builds the 27 per-letter lookup buckets, and then
  calls `sub_42BF10()` to create the map-object ID table. Those bucket entries
  are recovered four-byte pointer slots, so native builds now keep their
  full-width addresses in a sidecar and use low-address storage on Linux.
  `map_download_dispatch_test` exercises the bucket allocation/release
  lifecycle on both architectures; this is a pointer-ownership fix, not an
  initializer call injected into the map loader;
- the same gameplay record path also copies parser-owned variable-size data
  into recovered DWORD pointer slots at `sub_4E3470()` offsets `+556`, `+692`,
  `+700`, `+736`, `+748`, and `+756`. Native Linux now allocates those copies
  below 4 GiB and records their mapping size for teardown. The corresponding
  `thing.bin` parser callbacks in `GAME4.c` use the same ownership rule, while
  i386 retains the original `calloc`/`free` behavior. The focused dispatch test
  invokes the production `sub_535A60()` callback twice and then the normal
  `sub_4E2A20()` destructor, covering replacement and release of the fixed-size
  `+136` parser pointer; this is another ownership fix, not a map-loader
  initializer injection;
- the `thing.bin` parser dispatch table has the same recovered DWORD pointer
  representation for callback names and handlers. `sub_4E3220()` now widens
  both table reads at the call boundary, and `sub_5360A0()` widens the related
  object-callback lookup before `sub_4E3470()` stores it. The object callback
  itself is retained in a native sidecar and all gameplay callers use the
  widened accessor; i386 continues to read the recovered slots directly;
- the post-fix bounded native amd64 server probe reaches the production
  `sub_415470('thing.bin')` parser and continues through `sub_4101D0()` and
  `sub_410F60()` without a callback-table fault. It still stops at the known
  pre-connection UI boundary, so this verifies parser startup only and does
  not establish successful map activation;
- calling only `sub_42BF10()` at the map window allocates a one-entry table
  from the server registry (`count=1`) but does not activate the stock map;
  calling the broader `sub_435CC0()` there faults in `sub_49A8E0()`. These
  probes confirm that table allocation and gameplay initialization depend on
  earlier state-machine preconditions; neither call belongs in the map loader.
- the native startup audio boundary also uses a host-width sidecar:
  `nox_mss_digital_handle` is authoritative for the AIL digital-driver
  lifecycle, while `byte_5D4594[816432]` remains the recovered i386 slot and
  native compatibility copy. The release/reacquire, close, presence, and
  preference helpers now use the sidecar. This removes a startup SIGSEGV in
  `AIL_digital_handle_release()` before the main menu and lets the native probe
  reach `window/MainMenu.wnd`; the i386 path remains unchanged.
- native server discovery reaches the shared packed-list insertion helper
  `sub_4258E0()` with recovered 32-bit node addresses. Its native branch now
  reconstructs the list anchor before reading or updating the `+4` link; using
  the encoded low value as a host pointer crashed while processing the first
  server-info response. The callback registration and packet argument use
  host-width sidecars/types as well, while i386 retains the original slots.
- `sub_554D70()` performs a FIONREAD check before `recvfrom()`. On native
  Linux, another discovery callback could consume the datagram between those
  operations, leaving the main loop blocked in `recvfrom()` and starving the
  control/UI pump. `sub_554B40()` now makes the native amd64 discovery socket
  nonblocking immediately after bind/setup; the recovered i386 path remains
  unchanged. This is why the native UI macro can reliably reach the Host Game
  transition before later gameplay-state work.
- the same discovery-list walk uses `sub_425940()` to reconstruct the next
  node. Its return type must remain host-width on native builds; truncating the
  reconstructed pointer back to `int` caused the duplicate server-name check
  in `sub_4A0410()` to call `strcmp()` on an invalid node. The shared walker
  now returns `uintptr_t`, preserving the original 32-bit result on i386.
- the server-info list retains its recovered two-DWORD link layout on native
  Linux. `sub_425760()`/`sub_425770()` and the native branch of
  `sub_4258E0()` write encoded 32-bit links, while a sidecar maps live native
  node addresses back to those encodings. Insertions resolve both neighbors
  before changing either link; `sub_425920()` similarly refuses to write when
  either neighbor is unresolved, and the server-list destructor removes a
  freed node from the sidecar. This prevents the repeated discovery refresh
  from corrupting the native heap. The 32-bit branches remain the original
  direct-pointer implementation.
- the native server-list UI had a second pointer-width boundary after
  discovery: `sub_43B7C0()` passed temporary wide-string buffers to event
  `16397` through `(int)`, and the callback path (`sub_46B490()` →
  `sub_439050()` → `sub_4A30D0()`) consequently received an invalid low address
  on amd64. The native path now preserves that payload, reconstructs the list
  window's embedded `+8`/`+24` pointers in `sub_4A3AC0()` and `sub_4A3A70()`,
  and keeps i386's original 32-bit behavior. The GDB smoke reaches live
  server-info row updates without the former `sub_4A3AC0()` SIGSEGV.
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

The runtime tests use the checked-in `build-deps/gamefiles/app/nox.cfg`: its
`VideoMode = 640 480 8` and `Fullscreen = 0` select a windowed 640x480x8 game
surface. The native probe logs the actual video request (`width=640
height=480 flags=0x8`) and the backbuffer dimensions. Xvfb is started at
1280x720 only as the host display; it does not make the game UI 1280x720.
Macro coordinates and UI-button rectangles must therefore be interpreted in the
game's 640x480 coordinate space, and any test changing `nox.cfg` must record the
replacement `VideoMode`/`Fullscreen` values.

This test configuration is separate from the launcher default: `dist-scripts/server.sh`
starts with `VideoMode = 1024 768 16` and `Fullscreen = 1` when no
`NOX_GAME_*` overrides or applicable display dimensions are supplied. Its
resolution-selection comment must stay aligned with those effective defaults;
do not infer test UI coordinates from the server launcher configuration.

To compare the focused transfer boundary on both architectures:

```sh
ctest --test-dir build-i386 -R map_download_dispatch_test --output-on-failure
ctest --test-dir build-amd64 -R map_download_dispatch_test --output-on-failure
```

The repeatable preflight for this workflow is:

```sh
tools/verify-native-map-workflow.sh
```

It verifies that `build-amd64/src/out` and `build-i386/src/out` are native ELF
executables (and explicitly rejects PE/Wine output), prints the checked-in
`nox.cfg` `VideoMode`/`Fullscreen` values, checks the byte hashes of the stock
built-in `CapFlag.map`/`CapFlag.nxz` fixture, and runs the focused transfer
regression on both builds. Run it after any diagnostic that writes
under `build-deps/gamefiles/app`; a failed hash check means the fixture must be
restored before interpreting another map result. This workflow deliberately
does not use maps that require reloaded EUD support.

Before a headless gameplay probe, stop any prior `out`, `xvfb-run`, or GDB
processes. An orphaned process can retain the control port or consume the
shared X display and make UI macro timing appear to be a native gameplay bug.
Use [`tools/run-native-probe.sh`](/c/Users/carol/dev/nox-decomp/tools/run-native-probe.sh)
to put the complete `xvfb-run` command in one process group and terminate the
whole group on timeout. Verify that the target has exited before starting the
32-bit comparison; wrapping only `out` in `timeout` can leave `xvfb-run` and
the game alive.

## Reproduce the native server-startup smoke test

Run from the extracted game-data directory:

```sh
cd build-deps/gamefiles/app
env \
  ALSOFT_DRIVERS=null LIBGL_ALWAYS_SOFTWARE=1 SDL_VIDEODRIVER=x11 \
  NOX_GAMEPAD=0 NOX_NO_INTERNET_SERVERS=0 NOX_UPNP_ENABLE=0 \
  NOX_CONTROL_SERVER=1 NOX_CONTROL_SERVER_PASSWORD=secret \
  NOX_CONTROL_SERVER_BIND=127.0.0.1 NOX_CONTROL_SERVER_PORT=2323 \
  NOX_SKIP_INTRO_MOVIES=1 NOX_CONTROL_SERVER_SLEEP_SCALE=6 \
  'NOX_CONTROL_SERVER_BOOT=sleep 5000; macro server;' \
  NOX_CONTROL_INJECT_LOG=0 NOX_CONTROL_LOG=1 \
  NOX_SERVER_NAME=NoxDecomp NOX_SERVER_SYSOP=secret \
  NOX_SERVER_LESSONS=15 NOX_SERVER_TIME=0 \
  NOX_SERVER_DEFAULT_MAP=CapFlag NOX_CAPTURE_INPUT=0 \
  NOX_LOBBY_REGISTER_ENABLE=0 \
  ../../../tools/run-native-probe.sh 35 xvfb-run -a \
  -s '-screen 0 1280x720x24' ../../../build-amd64/src/out
```

The current verified result is a native x86_64 server-mode startup that reaches
the main menu, executes the host setup far enough to load `gamedata.bin`,
`monster.bin`, and `window/ArnaMain.wnd`, then remains before the positive
connection-result state transition. Do not treat the timeout alone as success;
verify the last completed macro and the process exit reason. For the separate
map-dispatch assertion, use `NOX_CONTROL_SERVER_SLEEP_SCALE=0.1`, a
`sleep 30000` bootstrap delay, and `console "load CapFlag"`; set
`NOX_TRACE_MAP_DOWNLOAD=1` and verify `[map] map_download_start` followed by
`window/mapdnld.wnd`.

The control-server absolute-click path is now deterministic across the native
and i386 builds: `ACT_HOME` completes all three large relative moves before
the queued click is released. The old implementation injected only the first
move and then advanced the queue, making later macro coordinates depend on the
previous cursor position. Runtime verification with native amd64 and stock
`CapFlag` shows three top-left re-anchor moves before each scripted click;
macro completion still does not prove that the UI accepted the click or that
the positive connection-result transition occurred.
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

`sub_4519C0()` is the per-frame SoundSet update called from `mainloop`: it
advances each SoundSet state, updates its audio timing, and returns the status
used by the frame before `sub_4312C0()` runs. Its recovered list head and links
are DWORD slots, so native code must reconstruct them before every traversal.
`sub_452300()` also stores the source SoundSet record in playback-record slot
`v1[9]`; that slot is another recovered DWORD and cannot retain a host pointer.
Native playback records now use a sidecar association for that source pointer,
and `sub_451BE0()`, `sub_451DC0()`, `sub_451F30()`, `sub_452050()`,
`sub_452120()`, `sub_452580()`, `sub_452770()`, and `sub_452F10()` consume the
sidecar. Without it, the second native frame reconstructed a null source and
crashed while updating SoundSet timing. The i386 path retains the original
packed slot.
The second traversal previously reloaded the head as a native pointer, and a
later callback still read the head and next link with native-width pointer
loads. After the server screen opened, that could leave the main loop spinning
on an orphaned singleton instead of reaching the next event-pump call. Native
now reconstructs those links and terminates a self-linked recovered node for
the current frame; the i386 path retains the original sentinel traversal.
This role and root cause are based on the native trace; positive gameplay
connection success remains unproven.

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
  ../../../build-amd64/src/out -serveronly CapFlag
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
