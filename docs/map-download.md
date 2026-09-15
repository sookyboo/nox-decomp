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

Run it with:

```sh
ctest --test-dir build-i386 -R map_download_dispatch_test --output-on-failure
```
