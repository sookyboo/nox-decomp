# Rendering regression coverage

## Force of Nature and mana drain distance

The Force-of-Nature effect update `sub_4CA650()` advances an effect object
toward its target using the shared integer distance helper `sub_48C6B0()`. It
reads the current position at offsets `+12/+16`, the target grid coordinates at
`+432/+434`, and the effect speed at `+443`; it either stores the next position
through `sub_49AA90()` or removes the effect through `sub_45A4E0()` when the
remaining path is too short or has passed the target. Mana-drain rendering uses
the same distance lookup helper. These roles are inferred from the decompiled
callers and field accesses.

`tests/force_nature_render_test.c` drives both production entry points with a
deterministic effect fixture. It verifies the corrected lookup-table indexing,
the resulting movement, and the short-distance cleanup path. It does not
claim to cover the complete spell dispatch, animation, or final GPU rendering
pipeline.

## Obliterate effect motion

The Obliterate effect update `sub_4CA720()` reads the effect age, target
coordinates, amplitude, direction-table index, and direction flag. It computes
a signed offset from the direction table, sends the next position through
`sub_49AA90()`, and mirrors that position into the effect state at `+32/+36`.
When the effect has reached its target neighborhood or exceeded its lifetime,
it removes the object through `sub_45A4E0()`. This role is inferred from the
decompiled function and its callback fields.

`tests/obliterate_render_test.c` drives this production update with a
deterministic direction-table fixture, checks the signed movement and mirrored
state, and covers target-neighborhood cleanup. Complete spell dispatch,
animation sequencing, and final GPU presentation remain integration coverage.
