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
