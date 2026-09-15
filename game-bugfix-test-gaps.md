# Surviving game bug fixes without regression tests

This inventory covers Sookyboo-authored gameplay/runtime fixes reachable from
the current combined branch. Architecture-only fixes and duplicate merge
commits are tracked separately in
[`architecture-compatibility-tests.md`](architecture-compatibility-tests.md).
None of the entries below currently has a dedicated automated regression test.

| Commit | Fix area | Regression test to add |
|---|---|---|
| `1247869` | Solo startup, case-sensitive asset lookup, and LAN setup | Build a synthetic mixed-case installation fixture; verify solo startup and LAN initialization resolve files consistently. |
| `2cc50d3` | Missing string functions affecting inventory-screen floats | Open an inventory fixture containing float-formatted values and assert rendered/text output. |
| `ac301fb` | Inventory interception while hidden | Toggle inventory visibility and dispatch inventory input; assert hidden inventories do not consume the event. |
| `ba20703` | Missing-map download from server to client | Mock the map transfer protocol, complete a download, and verify the map is stored and then loadable. |
| `2755c9b` | Audio artifacts and integer overflow | Feed boundary-sized audio buffers and assert sample counts, clipping, and no overflow. |
| `e68bba7` | Audio correctness issues | Decode representative tracks and compare deterministic sample format/length results. |
| `743f879` | Repeating-song jitter | Advance a deterministic audio scheduler through loop boundaries and assert gap-free track transitions. |
| `3302bad` | Durability strings in inventory | Construct items at minimum, maximum, and damaged durability and assert displayed strings. |
| `e5859eb` | God-mode spell changes and sage cheat | Execute both cheat commands against a spellbook fixture and assert the selected spell remains correct. |
| `d900cd7` | Gamepad radial-limit behavior | Feed stick positions at, inside, and outside the radial threshold and assert clamped mouse movement. |
| `8218c87` | Gamepad wordset input | Dispatch wordset/controller input events and verify the expected text/action is produced. |
| `43df06e` | Mouse scaling/sensitivity | Apply known logical-to-screen coordinates at several scales and assert deterministic transformed positions. |
| `7ff93d5` | Gamepad overlay release | Press/release overlay controls repeatedly and assert resources/state are released exactly once. |
| `64e57c7` | Gamepad overlay clear | Populate an overlay, clear it, and assert no stale controls remain visible or actionable. |
| `adfe080` | Summon crash | Use a minimal summon fixture with valid and invalid targets; assert failure returns safely without memory access errors. |
| `736e9f2` | Unlimited summon | Advance summon counts through the configured limit and assert creation is rejected after the limit. |
| `a1b2f49` | Summoning behavior | Execute a summon start/complete sequence and assert owner, position, and summoned-object state. |
| `ac10013` | Wizard chapter 3 archer/barrel cut scene | Replay the cut-scene trigger fixture and assert the archer/barrel event sequence completes once. |
| `850ade5` | Save/load with case-variant map names | Round-trip a save referring to differently cased map names and assert the same map is restored. |
| `6f96bcc` | Game-start crash | Start with the minimal primary-installation fixture and assert initialization reaches the first game state. |
| `d825fa9` | Windows movie loading | Open a representative movie fixture and assert successful demux/decode and cleanup. |
| `0b399ec` | Movie colors | Decode a known-color frame and compare RGB channel values against the expected conversion. |
| `1ba01e0` | Windows game colors | Render a palette fixture and assert expected color values on the compatibility path. |
| `6a6f369` | Joining network games | Mock server discovery/join packets and assert the client enters the joined-game state. |
| `9d08e53` | Public-game listing | Feed public-list responses, including malformed entries, and assert filtering and registration state. |
| `f08a713` | Gamepad skipping chapter cut scenes | Send the skip action during a cut scene and assert the next scene/state is selected exactly once. |
| `0f61a6e` | Font rendering at exact resolutions/integer scaling | Render text at exact-size surfaces with integer scaling enabled/disabled and compare glyph bounds. |
| `236b253` | Viewport math and oversized rendering | Exercise aspect ratios smaller/larger than the display and assert viewport remains inside bounds. |
| `5e69080` | Force-of-nature and mana-drain rendering distance order | Run deterministic spell render inputs and compare computed distances/visibility decisions. |
| `8653a5f` | Obliterate spell rendering | Render the spell fixture through its lifecycle and assert expected light/effect geometry. |
| `1cc83e6` | Laser direction/assembly matching | Compare laser endpoint and direction results for cardinal and diagonal inputs. |
| `41e87fd` | Crash when joining Korean servers | Feed Korean-server metadata to PC filtering and assert the entry is rejected without connecting. |
| `f14089f` | Cross-platform LAN hosting/lobby registration | Mock registration responses and assert host advertisement, retry, and cleanup state transitions. |

## Prioritization

Network join/registration, startup/save loading, summoning, and rendering
regressions should be prioritized because they previously caused crashes or
prevented gameplay. Audio, gamepad, and presentation tests can follow using
small synthetic fixtures and deterministic event progression.
