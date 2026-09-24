# Native startup compatibility

The normal Nox data image and runtime records use 32-bit fields. Native Linux
x86_64 builds therefore keep the recovered offsets and field widths, while
moving host-width pointers into side tables where a native pointer would
overlap an adjacent legacy field.

## Initialization flow

`sub_401070()` is the startup entry point reached from `WinMain`. After command
line and platform setup it loads the string resources, scans the available map
files, builds the built-in string index, and loads `nox.cfg`. The relevant
ownership is:

```text
sub_401070
  ├─ sub_40F300 / sub_40F830   CSF resource loading
  ├─ sub_4D07F0                 map catalog enumeration
  ├─ sub_40AED0                 built-in string index
  └─ sub_4317B0 / sub_431890    config parsing and dispatch
```

These function roles are inferred from the surrounding call sites and startup
logging; the decompiled names do not provide stable semantic names.

## Preserved layouts and native shadows

`sub_40F300()` reads the CSF counts from the existing 32-bit global image and
keeps the 0x34-byte CSF record layout. On native builds, the record array and
wide/narrow string pointer arrays are separate host-width allocations. The
file parser still consumes the original CSF stream. Its UTF-16 string payload
is explicitly converted into host `wchar_t` elements because Linux uses
four-byte `wchar_t`; the 32-bit path retains the existing loader.

`sub_4D07F0()` scans map files through the existing file-stream helpers. The
native path keeps `FILE *` and the map-directory linked list outside the
recovered three-dword map entry. The 32-bit path continues to use the original
map-list slots. File enumeration handles are widened only at the compatibility
API boundary so `FindFirstFileA` results are not truncated on x86_64.

`init_data()` initializes two packed pointer tables in the original image:

- the 0x3ff built-in string table used by `sub_40AED0()`, whose logical record
  is `{int index, const char *name}`;
- the startup config dispatch table used by `sub_431890()`, whose six logical
  records are `{const char *name, int value}`.

Both remain 32-bit tables in the normal build. Native builds populate shadow
records and make `sub_40AE90()`, `sub_40AEB0()`, `sub_40AF50()`,
`sub_40AF80()`, `sub_431890()`, and `sub_4332E0()` consume those shadows. This
avoids treating a recovered four-byte pointer slot as an eight-byte native
pointer and consuming the next field.

`sub_42CF50()` follows the same rule. The native path keeps the 16-byte
keybind records (137 entries) and 12-byte action records (41 entries) in their
recovered layout, while shadowing their host pointers and values. Its linked
config list uses a native sidecar node so the `prev` and `next` links are not
truncated. The CSF search count is decoded as a 32-bit value before passing it
to `bsearch`; adjacent legacy fields must not be read as one native `size_t`.

The initial graphics setup has another recovered DWORD-pointer boundary.
`sub_4861D0()` / `sub_486230()` and the display/gamma/palette setup allocate
temporary buffers whose addresses are stored in legacy slots and read by many
existing consumers. On Linux x86_64 those allocations use `MAP_32BIT`, with
matching native cleanup, so the original consumers and 32-bit layouts remain
unchanged. This is a compatibility boundary for temporary buffers, not a
change to the game record schema.

`proto.h` includes SDL before enabling the packed region used for the recovered
records. This keeps native SDL structs such as `SDL_Surface` at their platform
ABI offsets; only the recovered game records remain packed.

At startup, `sub_401070()` calls `sub_4101D0()` after loading the thing data.
The no-argument initializer allocates an 8192-entry recovered bucket-head
table, a 256-entry auxiliary table, and 8192 pool nodes, then calls
`sub_410160()` to prepare the free list and clear the table entries. The first
table is newly allocated storage, so its bucket heads must be zero before
`sub_410160()` follows any existing chains. Native x86_64's low-address
allocator happened to return zero-filled pages, but i386's `malloc()` could
reuse dirty memory; two startup cores followed one such stale chain to address
`0x0c`. `sub_4101D0()` now explicitly initializes all 8192 bucket heads before
the traversal. The focused `mod_hash_bucket_init_test` fills the table with a
nonzero pattern first and verifies that initialization clears exactly the
requested range. The externally visible effect is successful continuation to
the following `sub_410F60()` startup stage on both architectures.

The font/resource setup at `sub_43F1C0()` selects one of two five-entry
dispatch tables. Each recovered entry is a fixed 12-byte record with pointer
values in 4-byte slots. Native builds use a host-width sidecar for the two
name pointers, the resource pointer, and the renderer callback. Loaded font
resources and their internal buffers use Linux `MAP_32BIT` allocations because
the remaining decompiled font consumers still receive the original DWORD
resource pointer. `sub_440900()` likewise treats the graphics row table as an
array of 4-byte pointer values and explicitly converts each value before
accessing the pixel row.

## Startup pointer lookups confirmed by native traces

The generic legacy-pointer decoder deliberately preserves values below its
low-allocation threshold. That is correct for `MAP_32BIT` objects but
ambiguous when ASLR places the game image at an address whose low 32 bits also
fall below that threshold. Static-data slots therefore need a separate
range-checked path: reconstruct the candidate address, accept it only when it
falls inside `byte_587000` or `byte_5D4594`, and otherwise use the generic
decoder. Function-pointer slots use code-address reconstruction, not either
data-pointer path.

`sub_4117E0(const char *name)` searches the static name chain rooted at
`byte_587000[26488]` and returns whether the input string matches an entry.
`sub_411540()` calls it while parsing `thing.bin`; a match sets a flag in the
current recovered thing record. The input is a pointer to the just-parsed
name, while the chain entries are pointers into the static image. Treating a
low-word static address as an allocation caused an intermittent `strcmp`
fault during `thing.bin` loading. The native static-pointer conversion now
recognizes both static data arrays before falling back to low-pointer
handling; i386 continues to read its original DWORD pointers directly.

`sub_4BD720(audio_state)` allocates and initializes a recovered 0x138-byte
audio/timer record, obtains its driver descriptor from `audio_state + 256`,
and invokes the descriptor's initialization callback at `+4`. A zero callback
result returns the initialized record; a nonzero result invokes
`sub_4BD7A0()` to call the descriptor's shutdown callback at `+8` and release
the record. The audio startup path reaches this through `sub_487750()` and
`sub_487790()`. The descriptor itself is static image data, while its `+4`
and `+8` fields are code pointers; native code now decodes those two pointer
kinds separately. The related driver callback table at audio-record offset
`+172` uses the same static-data resolution before dispatch. These fixes
preserve the recovered DWORD fields and leave the i386 path unchanged.

`sub_4A2210()` is called from the initial presentation/menu callback
`sub_43C060()`. It marks the menu state initialized, attaches handlers to the
server-menu root, then walks the static name list beginning at
`byte_587000[168832]`, looks up each name with `sub_42F970()`, and stores the
result in the corresponding 48-byte list record. Its observable effect is
that the menu/resource entries are ready before the first menu frame. Both
the first name and the linked names are static-image pointers; the native
path now validates them against the static data ranges instead of treating
low-word values as heap allocations.

`sub_4519C0()` runs from `mainloop()` to update the SoundSet playback list.
Its root at `byte_5D4594[840612]` is a recovered DWORD circular-list
sentinel; initialization makes its next/previous links point back to itself.
The native walk decodes the root and each link with the fixed-image resolver,
then obtains a playback record's SoundSet source through the host-width
sidecar maintained by `sub_452300()`. It advances the sample generation,
updates/removes completed records, and adjusts the shared audio timer. Treating
the sentinel's low word as an allocation made an empty list appear nonempty
under some ASLR layouts and faulted on the first playback lookup.

`sub_43D6C0(music_entry)` receives a recovered four-DWORD music entry from
`sub_43D440()`. The first DWORD indexes a static string-pointer table at
`byte_587000[92792]`; the function builds a `music\\` path, opens the stream,
seeks to the entry's stored position, and starts playback. The table contains
static image strings, so the native path now uses static-range decoding for
the name. The returned `HSTREAM` is also copied into a local host-width
variable before seeking and starting. That copy was previously compiled only
for native x86_64, leaving i386 to use an uninitialized local; a core showed
that it had become a pointer into the function's stack buffer and was passed
to `AIL_set_stream_position()`. The assignment is now unconditional, with the
same pointer-sized representation as `HSTREAM` on i386. `audio_compat_test`
now opens its synthetic PCM fixture through the production AIL stream API,
sets its position, and reads the position back. The adjacent stream callback
path keeps its distinct pointer contracts: `sub_43ED00()` registers
`sub_43EDB0()` as the sample EOS callback
and feeds samples through `sub_43EE00()`, which refills sample buffers using
the callbacks at the owning audio record's `+276`/`+280` fields. Those are
code pointers and are reconstructed as code addresses; their sample buffers
remain recovered DWORD data pointers. `sub_43EFD0()` performs the matching
sample teardown and invokes the owner callback at `+284` once. These entry
points are reached as music and sound streams begin, after initial window
creation.

The `.wnd` resource path reaches `sub_4A0D80(FILE *input, char *line,
callback)` from `sub_4A0AD0()`. It parses the window definition, dispatches
property records, and creates the described widgets through `sub_4A1440()`
and `sub_4A1510()`. For a static-text record, `sub_4A10A0()` resolves the
localized text and returns the address of a recovered three-DWORD record:
text pointer, enabled flag, and wrapping flag. The parser forwards those
values when it creates the static-text widget with `sub_489300()`.

On x86_64, the localized string pointer can be above 4 GiB, so `sub_4A10A0()`
keeps a host-width copy while preserving the original low-DWORD image field.
`sub_4A0D80()` snapshots that pointer and the two flags into a native-width
three-value array before widget creation. `sub_489300()` associates the array
with the widget in a sidecar; its `sub_489390()` callback reads the text for
message `16386`, replaces it for message `16385`, and frees the array during
widget destruction. The `16385` dispatch in `sub_46B490()` must therefore
retain a pointer-width third argument on native builds. The legal-screen
setup in `sub_4CC4E0()` uses this setter for the localized welcome text and
the formatted server/version text. Two startup cores faulted in `sub_43F840()`
while rendering those strings: first the static-text value array had only
DWORD slots, then the formatted-text setter argument had been truncated
through `int`. The native path now preserves both values end to end; i386
retains the original three-DWORD array and callback ABI. The visible result is
that initial legal-screen text can be measured and rendered without
dereferencing a truncated string pointer.

## Diagnostics

Build the native executable with the opt-in 64-bit configuration described in
[`docker_sandbox.md`](../docker_sandbox.md). A headless startup smoke test can
then run from the game data directory:

```sh
../../../tools/run-native-probe.sh 15 env \
  ALSOFT_DRIVERS=null LIBGL_ALWAYS_SOFTWARE=1 SDL_VIDEODRIVER=x11 \
  NOX_GAMEPAD=0 NOX_NO_INTERNET_SERVERS=1 NOX_UPNP_ENABLE=0 \
  NOX_CONTROL_SERVER=0 NOX_SKIP_INTRO_MOVIES=1 \
  xvfb-run -a -s '-screen 0 1280x720x24' \
  ../../../build-amd64/src/out
```

Run this from `build-deps/gamefiles/app`. The game reads `nox.cfg` with
`VideoMode = 640 480 8` and `Fullscreen = 0`, so the game surface is windowed
640x480x8; Xvfb's 1280x720x24 setting is only the host display. The probe has
no map or server-mode arguments: startup scans the map catalog but does not
load a gameplay map. Its expected result is to survive until the wrapper's
15-second timeout (status 124); any earlier exit is a failure to investigate.
`run-native-probe.sh` enables core dumps and refuses to start if a plain `core`
would overwrite an existing dump. The workflow guard
[`verify-native-map-workflow.sh`](../tools/verify-native-map-workflow.sh)
checks both native ELF targets, unchanged hashes of the stock built-in CapFlag
files, the configured resolution, and focused transfer regressions; it does
not launch a map. No map requiring reloaded EUD support is used by these
startup checks. On the latest rebuilt source, both x86_64 and i386 survived
five consecutive 15-second no-map startup probes; each ended with the expected
wrapper timeout status `124` and produced no new core. The runtime probes are
needed because the unit tests cannot observe the complete SDL/window startup
path.

The native window parser keeps recovered DWORD pointer fields decoded at the
boundary. Window records and their legacy arrays use low-address allocations
when existing consumers still read a 32-bit slot; callback and persistent
window handles use native-width sidecars. The shared pointer decoder preserves
low `MAP_32BIT` addresses and reconstructs high heap addresses from their
32-bit slot value. Repeated bounded no-map startup probes currently survive
through parsing the initial window resources. This validates idle startup
stability only; it does not establish successful multiplayer entry, map load,
or gameplay initialization.

With the control-server `server` macro enabled and
`NOX_SERVER_DEFAULT_MAP=CapFlag`, native startup reaches the host setup and
opens the multiplayer window resources. The control server's
`console "load CapFlag"` action invokes the production `sub_443C80()` parser on
the main thread. Because the recovered `sub_4432B0()` handler consumes a
32-bit argv record and interprets its argument as a server-list entry, the
native path allocates a low-address UTF-16 argument and argv record, calls that
handler, and then sets the normal map-transition flag for a standalone host
when no matching peer exists. The result reaches `map_download_start()` and
opens `window/mapdnld.wnd`. A separate native window-render callback transport
was corrected by moving the two recovered DWORD line-drawing callbacks to
native sidecars. A separate native callback fault was
confirmed in `sub_554B40()`: it passed `sub_554FF0()` through an `int` before
`sub_43DE20()` stored it in the host-width callback sidecar. The resulting
invalid tick callback jump is fixed and covered by
`callback_transport_test`. The bounded smoke now remains alive in the
map-download screen; actual map transfer and gameplay still need a map-service
fixture or an equivalent automated network test.

If this smoke test exits with signal 11, rerun the same command with `gdb -q
-batch`, `run`, and `bt` before the executable arguments. Keep the first
project frame in the backtrace as the owning boundary; do not widen every
nearby field as a workaround.

At the time of this document update, the native executable builds, loads the
CapFlag test data, completes config localization, reaches OpenGL initialization,
loads the font resources, completes the graphics row clear, completes video
index-table initialization, initializes the timer records, parses SoundSet.bin,
and reaches both Modifier.bin record parsers. The native Modifier dispatch
table, seven-entry COLOR table, three-entry class-name table, and 18-entry
damage-type table are sidecars; returned indices and the 0x58-byte modifier
record offsets remain the recovered 32-bit contract. Full runs can also show
an intermittent SDL/X11 allocator abort in `sub_43BF10()` before Modifier.bin;
when graphics startup completes, the next investigation continues at
`sub_412ED0()` rather than widening the record. The preceding timer boundary
transports record addresses as `uintptr_t`, the SoundSet boundary uses a native
name sidecar while retaining its packed 32-bit field-offset table, and the CSF
parser routes its stream through a native `FILE *` sidecar.

The control-server startup probe now also completes the native SoundSet and
Modifier work, opens the title stream, and reaches `window/MainMenu.wnd` using
the stock built-in `CapFlag` map selection. The audio subsystem retains its
packed pool/list records and uses native handling at these boundaries:
`sub_424170()` owns the SoundSet.bin name/record list; native allocation now
keeps its recovered four-byte links valid below 4 GiB, and `sub_4242C0()` frees
the same records through the matching low-address allocator.
`sub_486A10()` decodes the packed SoundSet base pointer before `bsearch`,
`sub_4BD2E0()`/`sub_425900()`/`sub_425920()` operate on DWORD links without
host-width dereferences. The native `sub_425920()` path also addresses the
second packed link at byte offset 4 rather than using 64-bit pointer
arithmetic. `sub_4BD470()` preserves the recovered 12-byte list-head offset
when calling `sub_425900()`; using `_DWORD ** + 3` on amd64 incorrectly moved
the head by 24 bytes into the record's refcount field. `sub_4866F0()` keeps its
bag `FILE *` in a native sidecar. `sub_451BE0()`/`sub_451DC0()`/`sub_451F30()`/`sub_452050()` decode
SoundSet record pointers before using them. `sub_452490()` likewise reconstructs
the packed audio-manager and selected-record pointers before entering
`sub_4BDB40()`. `sub_4BD470()` must read its manager's packed pool pointers and
maximum-buffer field by byte offset; native `_DWORD **` indexing otherwise
selects the wrong pool and corrupts the active records. In `sub_43EE00()`, the
MSS callback's sample-buffer table and leftover-buffer fields are also packed
32-bit pointers. Its partial-buffer path uses a local 0x4000-byte staging area
and reconstructs those pointers before copying. The SDL/MSS compatibility layer
allocates native `HSAMPLE` objects below 4 GiB because the recovered audio
state stores the handle in a DWORD, while the sample's internal driver and
OpenAL fields remain host-width. Native callback dispatch in
`sub_43EE00()`/`sub_4BD8C0()`/`sub_4BD940()` likewise decodes packed function
pointers at the callback boundary. The timer-driven `sub_4873C0()` path also
reconstructs packed audio-state pointers and the callback stored behind the
linked owner record's `+32` slot. With these boundaries fixed, the stock
playback records created by `sub_452300()` use a native sidecar for their
SoundSet source pointer; the recovered `v1[9]` DWORD remains the i386 storage
location. The per-frame SoundSet consumers use the sidecar on native builds.
`CapFlag` probe reaches the multiplayer-host path and loads `gamedata.bin`,
`monster.bin`, and `window/ArnaMain.wnd` without a native 64-bit crash during
the bounded probe. A direct production-console `load CapFlag` command now
continues through `map_download_start()` and opens `window/mapdnld.wnd`; the
map-download screen remains alive in the bounded native probe. The related
`sub_4BDA80()`/`sub_4BDAC0()`/`sub_4BDAF0()` lifecycle callbacks decode both
the callback at record offset `+148` and the owner callback table at `+172`
before dispatch. These observations are from the stock built-in `CapFlag` map;
no map requiring reloaded EUD support is used.

The map-loader logging boundary follows the same layout rule. `sub_451630()`
owns the process log stream and the recovered slot at
`byte_5D4594[839880]` is only four bytes wide. Native builds keep the actual
`FILE *` in `nox_log_file`; `sub_4515B0()`, `sub_4517A0()`, and the fatal-error
shutdown path in `sub_4516C0()` all use that sidecar. i386 continues to read and
write the original slot. This removes the native libc fault that occurred
immediately after the completed `CapFlag.nxz` transfer was reopened by the
production map parser.

The next map-loader cleanup boundary is also layout-sensitive. The compressed
video decoder receives a host pointer through `sub_578C10()`/
`sub_57EA80()`, so native callers pass `uintptr_t`; its recovered fixed-offset
allocation fields are still 32-bit addresses and are freed through the native
low-address legacy allocator. `sub_430DB0()`/`sub_430EC0()` keep the screenshot
buffer in a native sidecar, while the map object table and font/range table
used by `sub_4AF8D0()`/`sub_4B0220()` and `sub_4AEDF0()`/`sub_49F500()` are
allocated below 4 GiB because consumers directly dereference their recovered
DWORD slots. The stock `CapFlag` transfer probe now reaches the existing
loader fatal-error path without the former native invalid-pointer crashes;
i386 retains the original layout and allocator behavior.

## Native multiplayer host initialization pointer boundaries

`sub_4DD320(slot, packet)` creates a player object from an incoming host
slot/packet, initializes the associated player-info record, links it through
the object's auxiliary record, and applies the initial position/state. Its
caller `CONNECT_PREPARE` uses slot 31 for the local host. The trailing
`sub_422140(player_info)` writes sentinel values at player-info offsets 3660
and 3664; its pointer argument must remain host-width even though the record's
fields stay DWORD-sized.

The reliable-update reader `sub_40ED60()` removes chunks through
`sub_420A90()`. Queue managers and nodes use their recovered four-byte fields:
head/tail at offsets 0/4, previous/next links at 8/12, and count/queued-byte
totals at 16/20. `sub_420A90()` returns the chunk payload, unlinks its node,
updates those totals, then passes the node to `sub_4209C0()`. The pool-manager
handle stored at queue offset 12 is also one DWORD. Native code must decode
that slot and must not use host-pointer indexing for either the queue links or
the payload cursor; i386 keeps the original layout.

`sub_48EA70(player, packet, length)` consumes received multiplayer packet
records. The packet cursor and end address are host pointers on native x64,
while the packet bytes and lengths retain their protocol representation.
Opcode `0xA9` delegates to `sub_4C9BF0(packet_record)`, which reads the record
type/fields and returns the number of bytes consumed. Both interfaces must
preserve the packet pointer rather than carrying its low DWORD into the
parser.

During `CONNECT_RESULT`, `sub_4D17F0()` invokes `sub_519870()`, which resets
the fixed 48-byte player-update records through `sub_519830(record, slot)`.
The record fields remain at their original offsets; only the record address
passed between routines is pointer-sized. `sub_4D22B0()` then walks the
player-update records returned by `sub_416EA0()`, resolves the thing at record
offset `+2056` and its auxiliary record at thing offset `+748`, clears a
transient auxiliary pointer at `+280`, and resets each player's per-tick state
through `sub_4EF7D0()`. Its list cursor and decoded thing/auxiliary pointers
must be host-width; the fields in the records remain DWORDs. When that reset
handles a thing's 20-byte meter record at `+556`, `sub_4EF7D0()`,
`sub_4EE6F0()`, and `sub_4E4560()` must decode the DWORD slot before reading or
writing the meter words. The trace passed both pointer boundaries after these
fixes.

The reset reports meter state through `sub_4D85C0()`, which builds a seven-byte
update in a stack buffer and submits it with `sub_4E5390()`. `sub_4E5030()`
copies this payload into the reliable-update queue. On native x86_64, the
decompiled `(int)v4` call-site cast truncated the stack address before it
reached the pointer-width `sub_4E5390()` argument. Widening this call-site
conversion lets the trace pass the queue copy; i386 retains its original
32-bit-compatible representation.

`sub_4D88C0(slot, thing)` is called by the player reset after the meter update.
When the thing has the relevant state bit set and player-info offset 2251 is
enabled, it builds a seven-byte update (opcode `0xDE`, thing identifier, and
two auxiliary-record words at `+4` and `+8`) and submits it through
`sub_4E5450()`. Its thing argument and auxiliary record are now kept
pointer-width, and player-info is resolved through
`nox_game3_thing_player_info()` rather than dereferencing the auxiliary record's
DWORD at `+276` as a host pointer. The trace passed this packet path after the
fix.

The next reset step calls `sub_4EFC30(thing, flag)`, which constructs a
nine-byte update (opcode `0xE9`, thing type at `+36`, current game tick, the
mode/status mask from `sub_4EF580()`, and the caller's flag) and sends it to
all clients through `sub_4E5390()`. `sub_4EF580()` obtains eight setting
indices through `sub_415CD0()` / `sub_415840()` and reads their values using
`sub_4E3BA0(index)`. That accessor indexes the table rooted at
`byte_5D4594 + 1563456` and returns the selected entry's DWORD at `+16`. The
latest trace stops in `sub_4E3BA0(0)` during this query. The table root and
indexed entry both use recovered DWORD pointers; GDB has not yet established
which dereference is invalid. The Chat Area and server-name UI stages remain
unreached.

## Compatibility rule

When fixing another startup failure, first classify the value:

1. If it crosses a host API, callback, allocator, or function boundary, use
   `intptr_t`/`uintptr_t` or a native shadow object.
2. If it belongs to a recovered record, global slot, wire packet, or file
   schema, keep its original fixed-width representation and decode it
   explicitly.
3. Add the native shadow only at the owning subsystem boundary, and preserve
   the original branch under the 32-bit build condition.
4. Verify both the native x64 build/runtime path and the normal i386/ARMHF
   suites after changing startup state.
