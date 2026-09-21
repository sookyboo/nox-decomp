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

The loader's `FADEBEEF`/`FADEFACE` header check applies to the stock `.map`
file, not its companion `.nxz` script package. The `.nxz` files begin with a
four-byte uncompressed payload length, so decoding that field is not a valid
map-loader codec test. An earlier direct comparison used `CapFlag.nxz` and is
discarded; a codec conclusion must use `CapFlag.map` or a self-contained map
fixture. The stock `.map` and `.nxz` files are both ordinary built-in assets;
no reloaded/EUD map is needed for this investigation.

After the section-dispatch sidecar fix, a headless native probe initializes the
normal startup state, opens the stock built-in `CapFlag.map`, and returns
success from `sub_4AC2B0()`. This verifies the map-loader entry point through
the `ObjectData` section on native Linux; it is narrower than a complete
server/gameplay session.

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
the header and section dispatch and returns success from `sub_4AC2B0()`; the
same probe cannot currently reach the loader on i386 because its unrelated
startup allocator fails earlier in `sub_4101D0()`.

Run it with:

```sh
ctest --test-dir build-amd64 -R map_download_dispatch_test --output-on-failure
```
