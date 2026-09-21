# Missing-map packet dispatch

Commit `ba20703` fixes the receive side of the missing-map transfer protocol.
The owning production entry point is `sub_552A80` in `src/GAME5.c`, the
connection receive dispatcher.

For a connection slot, `sub_552A80(a1, flags)` receives one UDP datagram into
the connection buffer, parses the three-byte packet header, and routes
high-bit channel packets to the channel's transfer state. The packet's second
byte is the expected transfer sequence. The transfer state stores its next
expected sequence at offset `+1`; after a match, the dispatcher increments it
and passes the packet payload to `sub_551EB0`, which advances the map download
state and invokes the connection callback for an accepted map chunk.

The important boundary is byte `0x80`: the packet sequence is an unsigned
byte, even though the decompiled local is declared as `char`. Comparing that
local as signed caused valid chunks with sequence values `0x80`–`0xff` to be
discarded. The increment is also kept explicitly byte-sized.

`tests/map_download_dispatch_test.c` drives this path through a loopback UDP
packet and the normal game network objects. It uses a synthetic connection and
transfer record, sends a packet with sequence `0x80`, and verifies that the
callback receives the chunk and the next expected sequence becomes `0x81`.
The fixture does not extract the comparison into a test-only helper.

The connection record remains the recovered 32-bit layout, including its
four-byte buffer and callback slots. The native 64-bit test allocates the
synthetic record and buffers below 4 GiB and uses a non-PIE test executable so
those legacy slots retain valid addresses; this keeps the test focused on the
production dispatcher rather than changing the wire/record contract.

The production constructor is `sub_553000()`, called by the normal network
socket setup (`sub_554380()`). It creates the `0xA4`-byte connection record,
its receive/send buffers, and optional user data; `sub_5531C0()` owns their
teardown. On native Linux these allocations use low-address mappings because
the recovered record stores all of those pointers in four-byte fields. The
`map_download_dispatch_test` fixture now constructs and destroys one through
these production entry points before exercising the `0x80` transfer packet,
so a future high-address regression fails at the owning network boundary.

The production message parser is `sub_48EA70()`. Its `0xB8` case calls
`sub_4ABAD0()` to open the temporary map package and establish the expected
chunk sequence; its `0xB9` case calls `sub_4AB7C0()` with the chunk sequence
and payload. `sub_4AB7C0()` writes an in-order chunk immediately, queues an
out-of-order chunk, and drains whichever queued chunk has the next expected
sequence. The queue is not assumed to be FIFO: network arrival order such as
`3,2,1` is valid and is covered by the fixture.
`sub_4AB580()` finalizes the temporary file, while `sub_4AB720()` aborts and
deletes it. Both architecture branches search the linked queue for the next
expected sequence; native builds additionally keep the `FILE *`, path, queue
head/tail, and queued payloads in host-width sidecars/low-address storage. The
test fixture drives `sub_48EA70()` with `0xB8` and `0xB9` messages, sends
sequences `3`, `2`, and `1`, and verifies the resulting file bytes are ordered
on both i386 and x86_64.

The native transfer boundary has also been exercised with the exact stock
`CapFlag.nxz` bytes: a temporary transfer wrote and finalized all 79,398 bytes.
That transfer-only probe does not establish map activation because `.nxz` is a
script package, not the `.map` consumed by `sub_4AC2B0()`. The probe uses a
temporary destination and restores the stock map files. The production parser
test covers 1,024-byte synthetic chunks; the GDB packet injector remains
diagnostic and stalls for larger synthetic calls.

The regression also covers the production completion transition: the final
`0xB9` chunk closes the temporary package and publishes the success state
without caller-side cleanup. This is checked on both i386 and native amd64;
manual `sub_4AB580()` cleanup remains covered for interrupted or test-fixture
transfers.

The loader's `FADEBEEF`/`FADEFACE` header check applies to the stock `.map`
file, not its companion `.nxz` script package. The `.nxz` files begin with a
four-byte uncompressed payload length, so decoding that field is not a valid
map-loader codec test. An earlier direct comparison used `CapFlag.nxz` and is
discarded; a codec conclusion must use `CapFlag.map` or a self-contained map
fixture. The stock `.map` and `.nxz` files are both ordinary built-in assets;
no reloaded/EUD map is needed for this investigation.

After the section-dispatch sidecar fix, the headless native probe reaches the
stock built-in `CapFlag.map` header and `ObjectData` callback without the prior
pointer-table crash. It still returns failure because the direct checkpoint has
not populated the object-ID table required by `sub_4AC610()`. The corresponding
i386 runtime probe stops earlier at its known `sub_410160()`/`sub_4101D0()`
startup fault, so it does not provide an end-to-end callback comparison; the
shared parser contract is covered by the parser-level regression instead.

The stronger combined probe copies the stock `.map` into a temporary fixture,
delivers the stock `.nxz` through `sub_4ABAD0()`/`sub_4AB7C0()` in 1,024-byte
chunks, finalizes it, and then calls `sub_4AC2B0()` for the fixture map. The
resulting `.nxz` matched the source byte-for-byte, but map activation remains
blocked at the same unpopulated object-ID table. The fixture is removed after
the probe; this remains headless runtime coverage rather than a full
interactive gameplay session.

The current native interactive boundary was also verified without a reloaded
or EUD map: after the main menu opens, the production control-server command
`console "load CapFlag"` returns success, logs `map_download_start`, and opens
`window/mapdnld.wnd`. The bounded amd64 process remains alive in that screen.
The probe does not claim map activation or peer-supplied transfer completion;
those still require the map package and normal network lifecycle to finish.
The standalone native bridge reaches this checkpoint by calling the recovered
`sub_4432B0()` handler with a low-memory argv record and setting the transition
flag when `CapFlag` has no server-list entry. That makes it suitable for
map-window/loader diagnostics, but not for claiming a completed connected
gameplay lifecycle.

Do not repair this boundary by calling an initializer from the map window:
`sub_42BF10()` only creates a one-entry server table, while `sub_435CC0()`
faults in `sub_49A8E0()` when injected at that point. Both functions require
state established before the map-download flag is set; the owning fix belongs
in the state-machine transition, not in `sub_4AC2B0()` or the transfer loop.

The native transfer-to-loader probe has two distinct cases. Injecting
`CapFlag.map` bytes through the transfer API is intentionally not a valid
activation test: `sub_4ABAD0()` removes the local `.map` while opening the
temporary package, so that probe ends at `MapLoadError` after the pointer
cleanup path. The valid stock-package probe injects the exact 79,398-byte
`CapFlag.nxz`, restores the stock `CapFlag.map` before the next loop tick, and
reaches the `ObjectData` callback without a SIGSEGV. It then stops at the
existing lifecycle precondition because the direct checkpoint has no populated
object-ID table; this is shared loader behavior, not evidence of another
native pointer fault. Both cases use only stock built-in `CapFlag` files and no
reloaded-EUD map.

The crash root cause was native startup writing host-width pointers into the
recovered four-byte slots at `byte_587000[173412]`, `[173416]`, and `[173420]`.
Those writes overlapped the adjacent path strings used by `sub_4AC2B0()`;
native code now reads the strings by their authoritative in-blob addresses,
while i386 keeps the recovered pointer-slot behavior.

The next native loader crash had the same ownership pattern in the map section
dispatch table at `byte_587000[70168]`: each recovered record is a 4-byte name
pointer followed by a 4-byte handler pointer. Native startup now leaves those
legacy writes disabled and uses a host-width sidecar containing the 16 section
names and handlers. `sub_426E20()` serializes every section through that
sidecar, while `sub_426EA0()` and `sub_426F40()` use it for named section loads
and the `ObjectData` callback. The i386 path retains the recovered table. A
headless native GDB probe against the stock built-in `CapFlag.map` now passes
the header and reaches the `ObjectData` callback. The object-ID table at
`byte_5D4594[741676]` is another recovered four-byte pointer slot: native
code now allocates its two-byte entries below 4 GiB and reconstructs the
pointer from the slot before dereferencing it. Both architectures still
report the same lifecycle failure when this table has not been populated.
The broader direct gameplay-initialization probe also reaches the same
`sub_4D7C60()` registry precondition on both architectures after the native
legacy-record fixes; it is diagnostic only because that checkpoint does not
perform the normal `sub_4D1630()` registry setup.

Run it with:

```sh
ctest --test-dir build-amd64 -R map_download_dispatch_test --output-on-failure
```

The normal stock `CapFlag` startup path has since been traced farther: native
64-bit reaches `sub_431390()` and the first gameplay tick after the same
`sub_415470()`/object-table initialization as i386. The native path now also
passes the video-bag parser and reaches the shared frame decode path
(`sub_4C79F0()` and its `sub_4C80E0()`/`sub_4C8DF0()`/`sub_4C96A0()` callbacks).
Those callbacks use recovered DWORD cursor slots, so native reads must rebuild
the pointer from the slot before dereferencing it; callback addresses require
the same reconstruction. The native cursor renderer also keeps its source and
SDL pixel destinations in native shadows because the recovered DWORD slots
cannot hold host pointers. The stock native probe now reaches
`window/MainMenu.wnd`; this comparison uses the stock built-in `CapFlag` map
only, with no reloaded/EUD map required.

The 32-bit comparison also exposed a decompiler typing trap in
`sub_57EA60()`: its recovered fields are byte offsets, although the generated
parameter was typed as `_DWORD *`. Using `_DWORD` indexing passed the parser
reset object at the wrong address and made `sub_57DDD0()` clear `0xc` as a
pointer. The byte-offset form preserves the recovered layout on i386 and lets
the stock probe reach `window/MainMenu.wnd`.

`sub_48C200()`/`sub_48C320()` decode the cursor stream returned by the shared
`sub_42FB30()` bag callback, selecting a row destination and dispatching the
decoded runs to `sub_48C480()` or `sub_48C4D0()`. `sub_42FAE0()` owns cleanup of
the decoded bag record; native cleanup must use the same low-address allocator
and recorded output size as allocation. These roles are inferred from the
callers and recovered state slots.
