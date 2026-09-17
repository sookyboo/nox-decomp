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
  ../../../build-linux64/src/out -serveronly Estate
```

`Estate` is the known-working map selected for this smoke test. Startup still
scans the complete map catalog before selecting the requested map, so a crash
before the config path completes does not yet establish that `Estate.map` was
opened for gameplay.

If this smoke test exits with signal 11, rerun the same command with `gdb -q
-batch`, `run`, and `bt` before the executable arguments. Keep the first
project frame in the backtrace as the owning boundary; do not widen every
nearby field as a workaround.

At the time of this document update, the native executable builds, loads the
Estate test data, completes config localization, and reaches OpenGL
initialization. The remaining x86_64 headless-startup fault is in the SDL
cursor compatibility path: the recovered cursor callback table still expects
32-bit pixel-row pointers while SDL owns native-width surface data. That is a
known follow-up, not a claim that the complete game startup path is fixed.

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
