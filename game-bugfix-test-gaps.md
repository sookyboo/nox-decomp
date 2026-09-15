# Surviving game bug fixes without regression tests

This inventory covers Sookyboo-authored gameplay/runtime fixes reachable from
the current combined branch. Architecture-only fixes and duplicate merge
commits are tracked separately in
[`architecture-compatibility-tests.md`](architecture-compatibility-tests.md).
Entries marked complete have a focused automated regression test; the remaining
rows still need dedicated coverage.

“Yes” in the final column means the proposed regression can drive an existing
production entry point or the normal game target; it does not imply that an
isolated unit test would be small or dependency-free. Extraction remains an
optional way to simplify some of those tests.

## Workflow for the next bug-fix test

Use this sequence when taking the next incomplete row:

1. Read `AGENTS.md`, `CONTRIBUTING.md`, this inventory, the nearest subsystem
   documentation, and the existing tests for that subsystem.
2. Select the first row that is not marked **Complete**. At the time of this
   writing, that is `2755c9b` (audio artifacts and integer overflow).
3. Inspect the historical fix before editing:

   ```sh
   git show --stat <commit>
   git show <commit> -- src
   ```

4. Reproduce the behavior through the owning production entry point using a
   deterministic, self-contained fixture. Do not extract code solely to make
   the test easier to write.
5. Add the regression test, then document the exercised decompiled function's
   observed inputs, state changes, callers/data flow, and externally visible
   result in the nearest subsystem document.
6. Update this table with the test name, its coverage limits, and a link to the
   documentation. Run the relevant tests, inspect the final diff, and check
   whitespace before committing the focused change.

Useful starting commands are:

```sh
cd /home/carolosf/dev/nox-decomp-main
sed -n '1,220p' AGENTS.md
sed -n '1,220p' CONTRIBUTING.md
sed -n '1,120p' game-bugfix-test-gaps.md
git status --short
```

## First-pass testing rule

For the first pass, write the regressions against the existing production
entry points and normal game target; do not extract code solely to make a test
easier to write. Extract a helper only after a focused integration fixture has
shown that the behavior cannot be tested reliably within the existing
subsystem boundaries.

| Commit | Fix area | Regression test to add | Test without extraction (integration) |
|---|---|---|---|
| `1247869` | Solo startup, case-sensitive asset lookup, and LAN setup | Build a synthetic mixed-case installation fixture; verify solo startup and LAN initialization resolve files consistently. | **Yes:** normal-target integration fixture; extraction is optional. |
| `2cc50d3` | Missing string functions affecting inventory-screen floats | **Complete (formatting):** `string_format_test` calls the production `nox_snprintf` entry point and verifies deterministic float formatting. Full inventory rendering remains integration coverage. | **Yes:** already uses the production entry point. |
| `ac301fb` | Inventory interception while hidden | **Complete (chain filtering):** `inventory_hit_chain_test` compiles the existing `GAME2.c` UI code and verifies hidden-inventory descendants resolve through the production widget chain. Full event dispatch remains integration coverage. | **Yes:** existing `GAME2.c` path; no extraction. |
| `ba20703` | Missing-map download from server to client | **Complete:** `map_download_dispatch_test` sends a high-bit-sequence map chunk through the production UDP receive dispatcher and verifies callback delivery plus sequence advancement; full map storage/load remains integration coverage. See [`map-download.md`](docs/map-download.md). | **Yes:** normal game network objects and loopback UDP fixture; no extraction. |
| `2755c9b` | Audio artifacts and integer overflow | **Complete:** `audio_compat_test` drives the production ADPCM decoders with boundary-sized mono/stereo blocks, invalid indexes, clipping, and a partial stereo tail. See [`audio-compatibility.md`](docs/audio-compatibility.md). Full OpenAL queueing and stream playback remain integration coverage. | **Yes:** production decoder path; OpenAL queueing remains untested. |
| `e68bba7` | Audio correctness issues | **Complete (PCM stream boundary):** `audio_compat_test` opens a synthetic WAV through `AIL_open_stream`, verifies the RIFF `size + 8` file bound, and processes a final zero-length data chunk through the production decoder. MP3-specific playback and OpenAL lifecycle behavior remain integration coverage. See [`audio-compatibility.md`](docs/audio-compatibility.md). | **Yes:** production stream entry point; MP3/OpenAL behavior remains untested. |
| `743f879` | Repeating-song jitter | **Complete (loop seek state):** `audio_compat_test` drives public `AIL_set_stream_position` with an MP3 stream state and verifies buffered bytes and RIFF chunk position/size are reset before the next decode. Actual MP3 frame continuity and OpenAL gaplessness remain integration coverage. See [`audio-compatibility.md`](docs/audio-compatibility.md). | **Yes:** production stream-position entry point; decoder/device gaplessness remains untested. |
| `3302bad` | Durability strings in inventory | **Complete:** `inventory_durability_test` exercises the production inventory formatting helper with minimum, damaged, near-maximum, and maximum integer durability values. Full item construction and tooltip rendering remain integration coverage. See [`inventory-input.md`](docs/inventory-input.md). | **Yes:** production formatting path; full UI rendering remains untested. |
| `e5859eb` | God-mode spell changes and sage cheat | **Complete (cheat ownership):** `cheat_spell_test` drives the production flag helpers used by the god and sage commands and verifies isolated flag changes plus the cheats-allowed gate. Full player spell-table contents and console/UI dispatch remain integration coverage. See [`cheats.md`](docs/cheats.md). | **Yes:** production cheat flag path; spellbook/UI integration remains untested. |
| `d900cd7` | Gamepad radial-limit behavior | Feed stick positions at, inside, and outside the radial threshold and assert clamped mouse movement. | **Yes:** gamepad input path. |
| `8218c87` | Gamepad wordset input | Dispatch wordset/controller input events and verify the expected text/action is produced. | **Yes:** controller event path. |
| `43df06e` | Mouse scaling/sensitivity | Apply known logical-to-screen coordinates at several scales and assert deterministic transformed positions. | **Yes:** input transformation path. |
| `7ff93d5` | Gamepad overlay release | Press/release overlay controls repeatedly and assert resources/state are released exactly once. | **Yes:** overlay event path. |
| `64e57c7` | Gamepad overlay clear | Populate an overlay, clear it, and assert no stale controls remain visible or actionable. | **Yes:** overlay lifecycle path. |
| `adfe080` | Summon crash | Use a minimal summon fixture with valid and invalid targets; assert failure returns safely without memory access errors. | **Yes:** summon production entry point. |
| `736e9f2` | Unlimited summon | Advance summon counts through the configured limit and assert creation is rejected after the limit. | **Yes:** summon action/tick path. |
| `a1b2f49` | Summoning behavior | Execute a summon start/complete sequence and assert owner, position, and summoned-object state. | **Yes:** summon action/tick path. |
| `ac10013` | Wizard chapter 3 archer/barrel cut scene | Replay the cut-scene trigger fixture and assert the archer/barrel event sequence completes once. | **Yes:** cut-scene trigger path. |
| `850ade5` | Save/load with case-variant map names | Round-trip a save referring to differently cased map names and assert the same map is restored. | **Yes:** save/load entry points. |
| `6f96bcc` | Game-start crash | Start with the minimal primary-installation fixture and assert initialization reaches the first game state. | **Yes:** startup integration fixture. |
| `d825fa9` | Windows movie loading | Open a representative movie fixture and assert successful demux/decode and cleanup. | **Yes:** movie loading API. |
| `0b399ec` | Movie colors | Decode a known-color frame and compare RGB channel values against the expected conversion. | **Yes:** movie decode/render path. |
| `1ba01e0` | Windows game colors | Render a palette fixture and assert expected color values on the compatibility path. | **Yes:** compatibility render path. |
| `6a6f369` | Joining network games | Mock server discovery/join packets and assert the client enters the joined-game state. | **Yes:** network packet path. |
| `9d08e53` | Public-game listing | Feed public-list responses, including malformed entries, and assert filtering and registration state. | **Yes:** network listing path. |
| `f08a713` | Gamepad skipping chapter cut scenes | Send the skip action during a cut scene and assert the next scene/state is selected exactly once. | **Yes:** input/cut-scene path. |
| `0f61a6e` | Font rendering at exact resolutions/integer scaling | Render text at exact-size surfaces with integer scaling enabled/disabled and compare glyph bounds. | **Yes:** rendering entry point. |
| `236b253` | Viewport math and oversized rendering | Exercise aspect ratios smaller/larger than the display and assert viewport remains inside bounds. | **Yes:** rendering entry point. |
| `5e69080` | Force-of-nature and mana-drain rendering distance order | Run deterministic spell render inputs and compare computed distances/visibility decisions. | **Yes:** spell-render path. |
| `8653a5f` | Obliterate spell rendering | Render the spell fixture through its lifecycle and assert expected light/effect geometry. | **Yes:** spell-render lifecycle. |
| `1cc83e6` | Laser direction/assembly matching | Compare laser endpoint and direction results for cardinal and diagonal inputs. | **Yes:** laser-render path. |
| `41e87fd` | Crash when joining Korean servers | Feed Korean-server metadata to PC filtering and assert the entry is rejected without connecting. | **Yes:** network metadata path. |
| `f14089f` | Cross-platform LAN hosting/lobby registration | Mock registration responses and assert host advertisement, retry, and cleanup state transitions. | **Yes:** network registration path. |

## Prioritization

Network join/registration, startup/save loading, summoning, and rendering
regressions should be prioritized because they previously caused crashes or
prevented gameplay. Audio, gamepad, and presentation tests can follow using
small synthetic fixtures and deterministic event progression.

## Testing constraints discovered

`1247869` is not currently suitable for an isolated `compat.c` unit-test
target. The compatibility source is compiled as part of the full game target
and assumes the project's custom headers and compile context (including
`src/string.h`, Windows compatibility types, and existing pthread wrappers).
Compiling it directly for a small test exposes incompatible pthread callback
types and header collisions before the case-insensitive path helper can be
exercised.

The eventual regression should therefore be an integration fixture built with
the normal `src` target, or the path resolver should first be extracted behind
a small production API with its own testable implementation. Do not duplicate
the resolver algorithm in a test: that would only prove the test copy works,
not that the game startup/file-loading path is fixed.
