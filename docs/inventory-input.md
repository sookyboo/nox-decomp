# Inventory input dispatch

The inventory hit filtering lives in the UI event path in `src/GAME2.c`.
`sub_46B740` resolves the widget under the pointer before dispatching the hit
and tooltip. When the inventory is fully hidden, it uses the existing
`ui_chain_contains` predicate to identify hits that still belong to the
inventory widget tree and skips dispatch for those hits. The status/minibar
state is intentionally handled separately so its tooltips remain available.

Commit `ac301fb` fixed ghost input from inventory widgets that remain in the
widget chain after the inventory is hidden. `tests/inventory_hit_chain_test.c`
compiles the existing `GAME2.c` translation unit and calls its production
static predicate directly; it does not copy or extract the widget-chain
algorithm. Function-section garbage collection keeps the focused test small
enough to link without constructing the complete game runtime.

The test covers descendant, root, unrelated-widget, and null-hit cases. It
does not exercise the complete game event loop, cursor handling, or tooltip
rendering; those remain integration coverage for a future broader fixture.

The inventory tooltip path is `sub_4627F0` in `src/GAME2.c`. When the selected
item has durability, it calls `sub_4633B0` to obtain current and maximum
values, then formats the localized durability string. The decompiler types
those stack values as `float`, but the producer writes integer bit patterns;
`inventory_format_durability` preserves those bits and passes them as `int`
arguments to `nox_swprintf`. This is based on surrounding call behavior and
the observed fix; the exact original temporary types remain reverse-engineered
rather than source-confirmed. `tests/inventory_durability_test.c` covers
minimum, damaged, near-maximum, and maximum values through the formatting
helper used by the production tooltip path.
