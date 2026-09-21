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

The font/resource setup at `sub_43F1C0()` selects one of two five-entry
dispatch tables. Each recovered entry is a fixed 12-byte record with pointer
values in 4-byte slots. Native builds use a host-width sidecar for the two
name pointers, the resource pointer, and the renderer callback. Loaded font
resources and their internal buffers use Linux `MAP_32BIT` allocations because
the remaining decompiled font consumers still receive the original DWORD
resource pointer. `sub_440900()` likewise treats the graphics row table as an
array of 4-byte pointer values and explicitly converts each value before
accessing the pixel row.

## Diagnostics

Build the native executable with the opt-in 64-bit configuration described in
[`docker_sandbox.md`](../docker_sandbox.md). A headless startup smoke test can
then run from the game data directory:

```sh
timeout --signal=TERM 20s env \
  ALSOFT_DRIVERS=null LIBGL_ALWAYS_SOFTWARE=1 SDL_VIDEODRIVER=x11 \
  NOX_GAMEPAD=0 NOX_NO_INTERNET_SERVERS=1 NOX_UPNP_ENABLE=0 \
  NOX_CONTROL_SERVER=0 NOX_SKIP_INTRO_MOVIES=1 \
  xvfb-run -a -s '-screen 0 1280x720x24' \
  ../../../build-amd64/src/out -serveronly Estate
```

`Estate` is the known-working map selected for this smoke test. Startup still
scans the complete map catalog before selecting the requested map, so a crash
before the config path completes does not yet establish that `Estate.map` was
opened for gameplay.

The native window parser keeps recovered DWORD pointer fields decoded at the
boundary. Window records and their legacy arrays use low-address allocations
when existing consumers still read a 32-bit slot; callback and persistent
window handles use native-width sidecars. The shared pointer decoder preserves
low `MAP_32BIT` addresses and reconstructs high heap addresses from their
32-bit slot value. This gets native startup through parsing
`MainMenu.wnd`, `noxworld.wnd`, and `filter.wnd`; later window teardown and
ownership paths still need the same audit before native multiplayer startup is
considered stable.

With the control-server `server` macro enabled and
`NOX_SERVER_DEFAULT_MAP=Estate`, native startup reaches the host setup and
opens the multiplayer window resources. The control server's
`console "load Estate"` action invokes the production `sub_443C80()` parser on
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
Estate test data, completes config localization, reaches OpenGL initialization,
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
`sub_486A10()` decodes the packed SoundSet base pointer before `bsearch`,
`sub_4BD2E0()`/`sub_425900()`/`sub_425920()` operate on DWORD links without
host-width dereferences, and `sub_4866F0()` keeps its bag `FILE *` in a native
sidecar. `sub_451BE0()`/`sub_451DC0()`/`sub_451F30()`/`sub_452050()` decode
SoundSet record pointers before using them. This probe does not use maps that
require reloaded EUD support; the next remaining failure is in the native
audio pool lookup after sample initialization. The SDL/MSS compatibility layer
allocates native `HSAMPLE` objects below 4 GiB because the recovered audio
state stores the handle in a DWORD, while the sample's internal driver and
OpenAL fields remain host-width. Native callback dispatch in
`sub_43EE00()`/`sub_4BD8C0()`/`sub_4BD940()` likewise decodes packed function
pointers at the callback boundary.

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
