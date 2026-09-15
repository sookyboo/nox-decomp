# Cut-scene resource assembly

`sub_40F120()` is the chunk-assembly entry point used by the game-2 loading
path (`sub_52911` calls it). It selects a resource stream from the global
stream table, clears the shared 0x800-byte scratch buffer, copies successive
chunks into that buffer, and returns the buffer plus the assembled byte count.
When a chunk would exceed the remaining capacity, the current implementation
copies only the fitting prefix, releases/advances that chunk through
`sub_420940()`, and returns the bounded result. The exact resource meaning is
inferred from the caller and remains decompilation-dependent.

The `ac10013` chapter-3 wizard cut-scene fix is associated with this boundary,
alongside ABI and coordinate fixes in the surrounding cut-scene code. The
regression in `tests/cutscene_chunk_test.c` drives the production
`sub_40F120()` function with a deterministic oversized chunk and verifies the
returned buffer, bounded length, copied prefix, and release callback. It does
not cover the full chapter trigger, rendering, or resource decoding sequence.
