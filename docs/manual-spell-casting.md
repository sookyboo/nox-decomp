# Manual spell casting input

Manual spell casting is controlled by the CMake option
`USE_MANUAL_SPELL_CASTING`, which defaults to `ON`. Disable it with
`-DUSE_MANUAL_SPELL_CASTING=OFF` to build without the manual spell input
bindings, parser/serializer extensions, timeout override, and its regression
test.

Nox already contains the runtime path used for directional spell phonemes. The
input dispatcher in `src/input.c` has hidden action IDs 18 through 26 for the
eight phonemes and spell-pattern end. Those actions enqueue the original spell
gesture control events, so manual bindings use the same client phoneme sounds,
gesture animation, per-player phoneme tree, spell validation, targeting, mana,
and cast execution as ordinary spell casting.

`sub_42CF50` in `src/GAME1.c` parses `nox.cfg` bindings. The original action-name
table does not expose action IDs 18 through 26, so the parser recognizes the
following extension names directly. `sub_42CDF0`, which serializes bindings,
uses the same mapping so these names survive config save/rewrite cycles.

| Config action | Phoneme/direction | Default key |
| --- | --- | --- |
| `PhonemeKA` | KA / up-left | `KP7` |
| `PhonemeUN` | UN / up | `KP8` |
| `PhonemeIN` | IN / up-right | `KP9` |
| `PhonemeET` | ET / left | `KP4` |
| `SpellEnd` | commit the current sequence | `KP5` |
| `PhonemeCHA` | CHA / right | `KP6` |
| `PhonemeRO` | RO / down-left | `KP1` |
| `PhonemeZO` | ZO / down | `KP2` |
| `PhonemeDO` | DO / down-right | `KP3` |

The `KP1`-`KP9` key tokens are existing Nox config key names; no new keyboard
scanner codes are introduced by this feature.

## Automatic commit timeout

`sub_401070` initializes the timeout later consumed by the player update path in
`src/GAME4.c`. The original game initializes it to half the simulation tick
rate, approximately 0.5 seconds. The environment variable
`NOX_MANUAL_CAST_TIMEOUT` can override that value in seconds:

```sh
NOX_MANUAL_CAST_TIMEOUT=1.0 ./Nox
```

Unset, empty, malformed, or negative values preserve the original 0.5-second
default. `0` is accepted and makes a pending sequence eligible for automatic
commit on the next update tick. The value is converted to simulation ticks at
startup, so the update loop remains deterministic and does not use wall-clock
time.

This setting changes only the existing automatic commit delay. `SpellEnd`
always uses the original explicit pattern-end input path.
