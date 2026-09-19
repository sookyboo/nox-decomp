# Native Player-Bot Port: `nox-decomp` Function and Data-Structure Reference

## Purpose

This document records the `nox-decomp-experiment` engine functions, runtime structures, offsets, callback slots, and global state that the planned optional native Bot-Script-compatible player-bot feature would use.

It is intentionally an **integration reference**, not a replacement design for Nox systems.

The intended native bot implementation should:

- reuse the original Nox player-bot path;
- reuse native monster AI, movement, pathfinding, perception, spell execution, inventory, equipment, respawn, teams, CTF, and networking;
- add only the higher-level tactical policy needed for Bot-Script parity;
- remain excluded from normal builds unless the bot build option is explicitly enabled.

This document is based on the current `nox-decomp-experiment` snapshot and follows the repository rule that decompiled behavior must be documented with clear confidence levels.

For the Go Bot-Script behavioral reference and native-port parity goals, see [`../bot-script-go-reference.md`](../bot-script-go-reference.md).

---

# Current implementation status

The high-confidence substrate described here now has an opt-in implementation:

```text
USE_BOT_SUPPORT=OFF (default)
    -> bot sources and integration hooks are excluded

USE_BOT_SUPPORT=ON
    -> bot_engine.c + bot_policy.c + bot_runtime.c + bot_team.c + bot_warrior.c + bot_wizard.c + bot_conjurer.c are compiled
    -> NOX_BOT_SUPPORT is defined
    -> confirmed native player-bot event sites record server-local policy events
```

`src/bot_engine.c` isolates recovered engine entry points and raw runtime
offsets from future tactical code. It understands both views of a native
player bot: the normal player view and the temporary monster-AI view used by
`nox_xxx_updatePlayerMonsterBot_4FAB20`. Monster-only actions such as Hunt,
WalkTo, action queries, interruption, and monster casting temporarily enter the
same recovered morph view when called while the bot is in normal player form.

`src/bot_policy.c` owns only Bot-Script-specific server-local state, reaction
deadlines, orders, and pending event records. `src/bot_warrior.c` currently
implements the high-confidence native Harpoon, Berserker Charge, health-potion
and recovery movement, loot/equipment, RoundChakram, Eye of the Wolf, War Cry,
TeleportWake pursuit, and native-backed CTF objective-steering subset.
`src/bot_wizard.c` implements the first high-confidence Wizard direct-cast
priority slice while leaving spell effects and player mana authoritative in Nox.
`src/bot_conjurer.c` now applies the same ownership boundary to the first
high-confidence Conjurer spell-priority slice. `src/bot_team.c` owns only the
shared high-level CTF destination decision used by all three classes; native
flag state, teams, pathfinding, pickup/drop/capture, scoring, and guard/fight
actions remain engine-owned.
`src/bot_runtime.c` synchronizes that state with existing native player bots,
can attach/detach an **already-created** normal player from the recovered
player-monster update path, and now owns server-local lifecycle identity for the
experimental socketless `bot spawn`/`bot clear` attempt. Native construction and
removal still flow through `sub_4DD320` / `sub_4DE7C0`; the runtime does not
reimplement their player/object bookkeeping.

All ten monster event concepts used by the Go reference now have confirmed
native dispatch sites and are captured without changing their original callback
behavior:

```text
Looking For Enemy
Enemy Sighted
Change Focus
Is Hit
Retreat
Death
Collision
Enemy Heard
End Of Waypoint
Lost Sight
```

The lifecycle work has advanced to a complete **experimental** non-client attempt
(see sections 35 and 42). Free-slot ownership is recovered, a synthetic but
structurally correct `PlayerOpts` is fed into the full native player constructor,
and `bot clear` uses the normal leave owner. The remaining lifecycle work is
runtime verification/hardening from hosted traces, not a second fake-player
implementation.

## Remaining implementation gaps

The unresolved work after the current Warrior/native-runtime foundation is:

- **non-client player lifecycle:** the complete `sub_4DD320`/`sub_4DE7C0`
  socketless attempt is implemented, but hosted logs still need to verify peerless
  network sends, capacity/mode policy, visibility, removal, and same-slot reuse;
- **CTF objectives:** ordinary CTF flag mechanics remain native. The basic
  attack/defend/escort/return destination choice is now shared by Warrior,
  Wizard, and Conjurer; the active enemy-flag carrier is now recognized as the
  Bot-Script `TeamTank` for carrier-specific policy, while broader coordinated
  multi-bot strategy and teammate-directed orders remain pending;
- **Warrior lost-target behavior:** the Bot-Script `TeleportWake` pursuit/check
  loop is implemented; additional lost-target behavior is only pending where it
  depends on future shared team/order policy;
- **Wizard policy:** the core direct-cast visible-target priority now includes
  Energy Bolt and Ring of Fire, plus hidden Enemy-Heard Invisibility, basic
  defensive buffs/protections, potions, reaction delays, cooldowns, and native
  mana spending; native nearby loot/equip pickup, the reference
  FireStormWand/ForceWand preference, and shared CTF steering are also
  implemented. Hostile DeathBall Counterspell, generic target-owned missile
  Inversion, Bot-Script Blink-as-Glyph escape, the owned three-spell Glyph Trap,
  native mana-source routing, native Drain Mana triggers/transfer, and CTF
  carrier-role-aware Invisibility and Bot-Script phoneme sequencing/timing are
  also implemented. Broader team-role coordination remains;
- **Conjurer policy:** the direct-cast priority slice now includes Pixie Swarm
  gated by authoritative owned-Pixie world state, native mana/buff/spell
  ownership, hostile DeathBall Counterspell, generic target-owned missile
  Inversion, Bot-Script Blink-as-Glyph escape, native nearby loot/equip pickup,
  native random summon spells/cage accounting, the custom native-backed Bomber
  summon/Glyph path with native `BomberSummon` audio, Alert status, and
  Enemy Sighted/Enemy Heard/Lost Enemy action choreography, mana-source routing,
  the literal reference 10-second weapon preference, Bot-Script spell/summon
  phoneme sequencing, and shared CTF steering. Team roles and commands remain;
- **orders/commands:** spawn/clear, attach/detach, difficulty, trace control, and
  3v3 setup are implemented. Visible allied teammate movement chat orders now
  dispatch through native Follow/Hunt/Guard; class-specific ally spell commands
  and broader coordinated team strategy remain;
- **fidelity:** spell phoneme sequencing/timing, global Bot-Script greeting/`gg`
  responses, and the reference movement-order acknowledgement sets are
  implemented. Command-specific ally-spell mana warnings and remaining cosmetic
  behavior stay with the shared command layer;
- **production lifecycle integration tests:** current deterministic tests cover
  adapters and policy, but end-to-end spawn/removal coverage awaits the real
  non-client lifecycle.

---

# 1. Confidence terminology

The following labels are used throughout this document.

**Confirmed**

The role is directly established by the function body, call sites, recovered symbol name, or multiple independent uses.

**Strongly inferred**

The role is clear enough for implementation, but one or more field names are not yet formally recovered.

**Opaque / do not depend on**

The value is used internally by an engine routine but should not be exposed as a new bot API until it is better understood.

The bot implementation should prefer named recovered functions over direct offset manipulation wherever possible.

---

# 2. High-level native architecture

The most important discovery is that Nox already contains a player-bot mechanism.

A player bot is not merely an ordinary NPC. Instead, Nox temporarily allows a normal **player object** to be processed using the monster-AI subsystem.

The core flow is:

```text
normal player object
    │
    │ object + 748 points to normal player runtime data
    │
    ▼
nox_xxx_mobMorphFromPlayer_4FAAC0
    │
    │ object + 748 temporarily points to bot monster-AI state
    │ object category temporarily changes toward monster processing
    ▼
nox_xxx_unitUpdateMonster_50A5C0
    │
    │ normal native monster AI:
    │ perception, actions, movement, combat, casting
    ▼
nox_xxx_mobMorphToPlayer_4FAAF0
    │
    │ restore normal player runtime data
    ▼
nox_xxx_monsterActionToPlrState_4FABC0
    │
    │ convert current monster action to player animation/state
    ▼
normal player networking / CTF / inventory / equipment
```

The primary update routine is:

```text
nox_xxx_updatePlayerMonsterBot_4FAB20
```

This should remain the authoritative engine update path for native player bots.

---

# 3. Recommended ownership boundaries

The optional bot subsystem should own:

```text
Bot-Script-compatible tactical policy
reaction delay
per-bot tactical cooldown/deadline state
high-level attack/defend/escort objective choice
console/server commands for spawning and configuration
optional bot-specific tests and diagnostics
```

Existing Nox should continue to own:

```text
player objects
player slots
teams
monster AI state
action execution
pathfinding
movement
enemy perception
collision
damage
death
inventory
equipment
spell mechanics
buff mechanics
respawn
player starts
CTF rules
network replication
animation/state translation
```

---

# 4. Build-time integration

The experiment repository already uses an opt-in convention based on `USE_*` CMake options and `NOX_*` preprocessor definitions.

Existing examples include:

```cmake
option(USE_EUD_COMPAT "Build and enable Panic EUD/MemoryHack compatibility" OFF)
```

with conditional source inclusion in `src/CMakeLists.txt`.

The bot feature should follow the same convention:

```cmake
option(USE_BOT_SUPPORT "Build native player bot support" OFF)
```

and, when enabled:

```cmake
if (USE_BOT_SUPPORT)
    list(APPEND NOX_RUNTIME_SOURCES
        bot_engine.c
        bot_policy.c
        bot_runtime.c
        # class/team sources are added as their policy is implemented
    )
    add_compile_definitions(NOX_BOT_SUPPORT)
endif()
```

Existing engine integration calls should be wrapped in:

```c
#ifdef NOX_BOT_SUPPORT
...
#endif
```

`USE_BOT_SUPPORT=OFF` must remain the default and must exclude the native bot implementation from compilation.

---

# 5. Core player-bot functions

These are the most important functions for the feature.

## 5.1 `nox_xxx_playerBotCreate_4FA700`

Recovered declaration:

```c
int nox_xxx_playerBotCreate_4FA700(nox_object_t* a1);
```

Current implementation is in `src/GAME4.c`.

### Confirmed role

Initializes or resets the monster-AI backing state for an **already existing player object**.

It does **not** allocate the player object itself.

### Important behavior

The function:

1. reads the normal player runtime block from `object + 748`;
2. checks `player_runtime + 292` for an existing bot monster-AI block;
3. allocates `0x898` bytes when absent;
4. zeroes the block;
5. stores a back-pointer to the normal player runtime block;
6. initializes standard monster-AI fields;
7. captures initial object direction and position;
8. examines the player's class byte;
9. installs class-specific AI capability/timing configuration.

The initialization also sets the first 24-byte monster action record to action
`5`. `nox_xxx_unitHunt_5157A0` schedules the same action ID, confirming that a
newly initialized native player bot begins with the engine's normal **Hunt**
action already queued. New bot code does not need to add a second initial Hunt
merely to make the recovered player-bot update active.

### Key code relationship

```text
player object
  +748 -> normal player runtime
             +292 -> player-bot monster AI block (0x898)
```

### Class mapping

The class is read from:

```text
player runtime +276 -> player info
player info +2251   -> class byte
```

Confirmed values:

```text
0 = Warrior
1 = Wizard
2 = Conjurer
```

Class `0` receives no spell-oriented capability table from this function.

Classes `1` and `2` receive different native AI capability/timing configurations.

### Bot-port use

Call this after a player object has been converted/marked for server-controlled player-bot operation, and when the bot needs its native monster-AI state reset.

Do not duplicate this initialization in new bot code.

---

## 5.2 `nox_xxx_updatePlayerMonsterBot_4FAB20`

Recovered declaration:

```c
int nox_xxx_updatePlayerMonsterBot_4FAB20(uint32_t* a1);
```

### Confirmed role

The original Nox update function for a player object controlled through monster AI.

### Main flow

```text
ensure bot AI state exists
    ↓
nox_xxx_respawnPlayerBot_4FAC70
    ↓
mark monster AI state active
    ↓
nox_xxx_mobMorphFromPlayer_4FAAC0
    ↓
nox_xxx_unitUpdateMonster_50A5C0
    ↓
nox_xxx_mobMorphToPlayer_4FAAF0
    ↓
nox_xxx_monsterActionToPlrState_4FABC0
    ↓
copy resulting state/position into player runtime/player-info fields
```

### Important state propagation

After monster update:

```text
normal player runtime +88
    <- player state returned by nox_xxx_monsterActionToPlrState_4FABC0

normal player runtime +236
    <- byte from monster AI +481

player info +3632
    <- object X position

player info +3636
    <- object Y position
```

### Failure behavior

If the monster-AI backing state cannot be created, the object's update function at `object +744` is restored to:

```text
nox_xxx_updatePlayer_4F8100
```

### Bot-port use

This should be the engine update function assigned to native player bots.

The higher-level Bot-Script-compatible policy should feed decisions into native state/actions without replacing this core updater.

With `NOX_BOT_SUPPORT`, the optional integration runs the shared native
Harpoon owner pull/break step and then calls `nox_bot_runtime_update()` only
after the monster update has completed, the object has been restored to normal
player form, and translated player state / position have been copied back.
This ordering is required for authoritative player abilities such as Harpoon,
War Cry, and Eye of the Wolf, whose native executor/runtime state expects the
player category and normal player runtime at `object +748`. The optional calls
are skipped while `nox_xxx_respawnPlayerBot_4FAC70` reports the player as still
dead/respawning.

---

## 5.3 `nox_xxx_mobMorphFromPlayer_4FAAC0`

Recovered declaration:

```c
char nox_xxx_mobMorphFromPlayer_4FAAC0(uint32_t* a1);
```

### Confirmed role

Temporarily changes a player object into a monster-AI-processing view.

### Key behavior

When the object has the player category bit:

```text
object flags/category
    player-processing bit removed/replaced by monster-processing bit

object +748
    changed from normal player runtime
    to normal_player_runtime +292 bot-AI pointer

object +12
    set to 16
```

### Bot-port use

Do not call this independently from tactical code.

It belongs to the established `updatePlayerMonsterBot` lifecycle.

---

## 5.4 `nox_xxx_mobMorphToPlayer_4FAAF0`

Recovered declaration:

```c
char nox_xxx_mobMorphToPlayer_4FAAF0(uint32_t* a1);
```

### Confirmed role

Restores the object from the temporary monster-processing view back to normal player form.

### Important relationship

The monster-AI block contains a back-pointer at:

```text
monster AI +2180
```

which points to the normal player runtime structure.

The function restores:

```text
object +748 -> normal player runtime
object +12  -> 0
```

and restores player object-category processing.

### Bot-port use

Do not duplicate this state swap.

---

## 5.5 `nox_xxx_monsterActionToPlrState_4FABC0`

Recovered declaration:

```c
char nox_xxx_monsterActionToPlrState_4FABC0(int a1);
```

### Confirmed role

Maps the current monster action to a normal player animation/state byte.

### Input state

Reads the current action index from:

```text
monster AI +544
```

The action record is selected from the action stack and translated to a player state such as idle/move/attack/cast-like state.

### Bot-port use

No direct tactical use is needed.

It is important because it allows monster AI execution to remain visually/network compatible with normal player state.

---

## 5.6 `nox_xxx_respawnPlayerBot_4FAC70`

Recovered declaration:

```c
int nox_xxx_respawnPlayerBot_4FAC70(int a1);
```

### Confirmed role

Handles native player-bot respawn.

### Death detection

The player is considered dead when the health value reached through:

```text
object +556
```

is zero.

### Respawn delay

The function checks:

```text
current simulation frame - monsterAI death frame
    < 2 * gameFPS
```

and delays respawn until two seconds of simulation time have elapsed.

### Respawn flow

After the delay it calls:

```text
nox_xxx_playerBotCreate_4FA700
nox_xxx_playerMakeDefItems_4EF7D0
nox_xxx_mapFindPlayerStart_4F7AB0
nox_xxx_unitMove_4E7010
nox_xxx_aud_501960
```

It also applies an existing buff under a game-mode condition.

### Bot-port use

Reuse this instead of implementing a second respawn timer, spawn selector, or default-loadout reconstruction.

When the function returns `1`, the bot is still inside the native two-second
dead wait and `nox_xxx_updatePlayerMonsterBot_4FAB20` does not run the class
policy update. The optional bot runtime therefore clears only per-life pending
events and Warrior tactical deadlines at this owner boundary. Persistent slot
identity, difficulty, and teammate order remain intact for the respawned player.
This is necessary because class-local death cleanup cannot run during the early
return itself.

---

# 6. Normal player update fallback

## `nox_xxx_updatePlayer_4F8100`

Recovered declaration:

```c
void nox_xxx_updatePlayer_4F8100(nox_object_t* a1);
```

### Confirmed role

Normal non-bot player object update function.

### Bot-port relevance

A bot-enabled player object's update callback at:

```text
object +744
```

will need to use `nox_xxx_updatePlayerMonsterBot_4FAB20`.

If bot initialization fails, existing code falls back to `nox_xxx_updatePlayer_4F8100`.

The optional bot adapter can now safely assign `4FAB20` to an **already-created** normal player whose current update function is the recovered normal player updater, after ensuring the native `0x898` AI block exists. The unresolved lifecycle work is creation and authoritative cleanup of a server-controlled player slot/object without a human client.

### Harpoon owner upkeep

The normal player updater contains one owner-side Harpoon step after regular
player action processing. If `player runtime +132` contains an attached Harpoon
target, it:

1. reads the native Harpoon pull force from game data;
2. breaks the Harpoon with `nox_xxx_harpoonBreakForPlr_537520` when the target
   has the invalid/dead `0x20` object-state bit;
3. otherwise records the player interaction on player targets through
   `sub_4E7540`;
4. applies the configured negative force to the attached target using
   `nox_xxx_objectApplyForce_52DF80`.

The implementation now factors that exact block into
`nox_player_update_harpoon_pull()`. `nox_xxx_updatePlayer_4F8100` still invokes
it at the original location, preserving normal-player behavior, while the
optional `4FAB20` player-bot path invokes the same helper after restoring normal
player form. Harpoon projectile flight, collision, range/lifetime validation,
and attachment positioning remain owned by the existing native projectile
functions (`sub_538890`, `sub_4EB6A0`, and `sub_54F380`).

`sub_537520` is the native player-side Harpoon break/cleanup entry point. It
delegates to `sub_5374D0`, which clears `player runtime +132`, ends native
ability `3`, deletes the Harpoon projectile referenced by `player runtime +136`,
and then plays the native break sound. The bot adapter exposes this operation
only while the object is in normal player form. Warrior policy uses it for the
Go reference's Is Hit behavior instead of manually clearing Harpoon fields.

---

# 7. Monster update and action subsystem

## 7.1 `nox_xxx_unitUpdateMonster_50A5C0`

Recovered declaration:

```c
void nox_xxx_unitUpdateMonster_50A5C0(nox_object_t* a1);
```

### Confirmed role

Main native monster update path.

### Bot-port responsibility

This is the engine that should continue to own:

- native monster action processing;
- tactical low-level execution;
- movement state;
- combat execution;
- perception-driven AI;
- casting actions.

Bot-Script policy should influence the action choices rather than replace this routine.

---

## 7.2 `nox_xxx_monsterPushAction_50A260`

Recovered macro/declaration:

```c
#define nox_xxx_monsterPushAction_50A260(obj, a2) \
    nox_xxx_monsterPushAction_50A260_impl(obj, a2, __FILE__, __LINE__)

void* nox_xxx_monsterPushAction_50A260_impl(
    nox_object_t* a1p,
    int a2,
    char* file,
    int line);
```

### Confirmed role

Pushes a native monster action onto the AI action stack.

### Bot-port use

Prefer higher-level recovered functions such as `monsterWalkTo`, `unitHunt`, and `monsterCast`.

Use direct action pushes only for Bot-Script behavior that has no suitable existing native wrapper.

Any direct action IDs used by new bot code should be documented by semantic name before merge.

---

## 7.3 `nox_xxx_monsterPopAction_50A160`

Recovered declaration:

```c
int nox_xxx_monsterPopAction_50A160(nox_object_t* a1p);
```

### Confirmed role

Pops/removes the current monster action.

### Bot-port use

Potentially useful for explicit command interruption, but should not be the first choice when an existing action-changing function already performs the correct cleanup.

---

## 7.4 `nox_xxx_monsterClearActionStack_50A3A0`

Recovered declaration:

```c
void nox_xxx_monsterClearActionStack_50A3A0(nox_object_t* a1);
```

### Confirmed role

Clears existing monster actions.

### Bot-port use

Useful when a high-level teammate order must immediately supersede current AI behavior.

Avoid indiscriminate clearing every frame.

---

## 7.5 `nox_xxx_monsterIsActionScheduled_50A090`

Recovered declaration:

```c
int nox_xxx_monsterIsActionScheduled_50A090(int a1, int a2);
```

### Confirmed role

Checks whether a particular action is already scheduled.

### Bot-port use

Can prevent tactical policy from continually re-queueing equivalent actions.

---

# 8. Native movement and hunting

## 8.1 `nox_xxx_monsterWalkTo_514110`

Recovered declaration:

```c
void nox_xxx_monsterWalkTo_514110(
    nox_object_t* obj,
    float x,
    float y);
```

### Confirmed role

Schedules native monster movement to a world position.

### Observed implementation

It clears the action stack and schedules the existing movement actions, including the requested coordinates.

### Bot-port use

Primary primitive for:

- CTF objective movement;
- guard positioning;
- regrouping;
- escort movement;
- explicit player teammate commands.

No custom pathfinder should be added.

---

## 8.2 `nox_xxx_unitHunt_5157A0`

Recovered declaration:

```c
void nox_xxx_unitHunt_5157A0(nox_object_t* obj);
```

### Confirmed role

Places a unit into native Hunt behavior through the monster action stack.

### Bot-port use

Primary default combat/search behavior when a bot does not have a higher-priority Bot-Script objective.

---

## 8.3 `nox_xxx_mobSetFightTarg_515D30`

Recovered declaration:

```c
void nox_xxx_mobSetFightTarg_515D30(
    nox_object_t* unit,
    nox_object_t* target);
```

### Confirmed role

Sets a monster's explicit fight target through the native action system. For a
valid monster-view unit and non-self target, the function clears the existing
action stack, stores the target at AI `+1216`, refreshes native AI target state,
and schedules the normal fight/action records using the target's current
position and simulation frame.

### Callers/data flow

The original Nox script `Attack` entry point resolves its two object arguments
and calls this function. The optional bot adapter enters the recovered temporary
monster view before invoking it for a native player bot, then restores player
form immediately afterwards.

### Bot-port use

Used by the Warrior Lost Sight `TeleportWake` behavior. Policy retains the lost
enemy while native `WalkTo` approaches the wake; after the wake transition is
detected, this function resumes combat against that target. It remains a target
choice only; native fight actions own movement and combat execution.

---

## 8.4 `nox_xxx_monsterGoPatrol_515680`

Recovered declaration in the decompiled C ABI:

```c
void nox_xxx_monsterGoPatrol_515680(
    nox_object_t* unit,
    void* patrol_args);
```

### Confirmed role

Schedules the native Guard/Patrol action used by NoxScript. The five input
values are two world positions followed by the patrol/guard distance. The
function clears the monster action stack, pushes action `4` with the first
position and direction toward the second, and stores the distance at AI
`+1312`.

### Bot-port use

The CTF carrier branch uses the same native action with identical start/end
positions and radius `20`, matching Bot-Script's `Guard(base, base, 20)` choice.
The adapter constructs the recovered five-value argument block and does not add
a second guard state machine.

---

# 9. Enemy perception and hostility

## 9.1 `nox_xxx_monsterUpdateSeenEnemies_5286D0`

Recovered declaration:

```c
void nox_xxx_monsterUpdateSeenEnemies_5286D0(int a1, int a2);
```

### Confirmed role

Updates native seen-enemy state.

### Bot-port use

The bot tactical layer should consume native perceived-target state rather than running its own world scan every frame.

---

## 9.2 `nox_xxx_monsterVisionSeeEnemy_5287B0`

Recovered declaration:

```c
void nox_xxx_monsterVisionSeeEnemy_5287B0(int a1, int a2);
```

### Confirmed role

Processes an enemy becoming visible to monster AI and drives the existing sight-event path.

### Bot-port use

Relevant to the `EnemySighted` tactical response.

No duplicate LOS test should be added unless Bot-Script has a behavior that explicitly differs from Nox perception.

---

## 9.3 `nox_xxx_aiLostSight_528560`

Recovered declaration:

```c
int nox_xxx_aiLostSight_528560(int a1, int a2);
```

### Confirmed role

Processes loss of a previously visible enemy and drives the Lost Sight callback.

### Bot-port use

Use the existing event rather than polling visibility transitions.

---

## 9.4 `nox_xxx_unitIsEnemyTo_5330C0`

Recovered declaration:

```c
int nox_xxx_unitIsEnemyTo_5330C0(
    nox_object_t* a1,
    nox_object_t* a2);
```

### Confirmed role

Native hostility/enemy relationship test.

### Bot-port use

Use for tactical target validation.

---

## 9.5 `nox_xxx_unitsHaveSameTeam_4EC520`

Recovered declaration:

```c
int nox_xxx_unitsHaveSameTeam_4EC520(
    nox_object_t* a1,
    nox_object_t* a2);
```

### Confirmed role

Checks native team equality.

### Bot-port use

Use for:

- friendly teammate command selection;
- escort logic;
- avoiding friendly targets;
- team strategy.

Do not maintain a duplicate bot-only team model for identities already represented by Nox teams.

---

## 9.6 Tactical observation primitives

The optional adapter now exposes the native read-only state needed by later
class policy without duplicating authoritative gameplay state.

Recovered native helpers used are:

```c
short nox_xxx_unitGetHP_4EE780(nox_object_t* unit);
short nox_xxx_unitGetMaxHP_4EE7A0(int unit);
short nox_xxx_unitGetOldMana_4EEC80(int unit);
short nox_xxx_playerGetMaxMana_4EECB0(int unit);
int nox_xxx_unitCanInteractWith_5370E0(
    nox_object_t* self,
    nox_object_t* other,
    int flags);
```

### Confirmed roles and bot use

- `4EE780` / `4EE7A0` return current and maximum unit HP;
- `4EEC80` / `4EECB0` return current/old player mana and maximum player mana;
- `5370E0` is the native unit-interaction eligibility test used by monster AI;
- the current monster-AI focus/target is stored at AI offset `+1196`;
- world position remains authoritative on the object at float offsets `+56` and
  `+60`.

For a native player bot, reading AI `+1196` requires the same temporary
player-to-monster view used by the action adapter. `nox_bot_engine_current_target()`
performs that morph and restores player form before returning. HP, interaction,
and position remain normal engine reads. Current/max mana are stored at normal
player-runtime offsets `+4` and `+8`; the adapter resolves the normal player
runtime first so mana reads remain correct even when an event fires while the
object is temporarily in monster-AI form.

These helpers are observation only. They do not mutate health, mana, target, or
position state.

## 9.7 Native aggression and recovery movement

The native script aggression setter is recovered as `sub_515980`. For monster
objects it writes the same float value to monster-AI offsets `+1304` and `+1308`;
these correspond to the paired aggression fields used by the monster AI. The
optional adapter enters the normal player-bot monster view, calls that existing
setter, and restores player form instead of writing those offsets from Warrior
policy.

The Warrior recovery path uses the Go reference values directly:

```text
Is Hit + health < 100 + current target health > 10
    -> nearest unowned world RedPotion
    -> aggression 0.16
    -> native WalkTo(potion position)

End Of Waypoint
    -> aggression 0.83
    -> non-CTF: native Hunt
    -> CTF: native-backed attack/defend objective choice
```

Nearest-item discovery uses the same authoritative world-object list already
used for loot. Unlike the 75-unit loot sweep, this recovery lookup intentionally
does not require visibility and has no distance cap, matching the reference's
`FindClosestObject` behavior before pathfinding takes ownership. Removed or
already-owned objects remain excluded.

CTF mode is identified through the existing game-flag query
`sub_40A5C0(NOX_GF_MODE_CTF)`, where `NOX_GF_MODE_CTF` is the repository's
`0x0020` mode flag. Native CTF stores a carried flag in the player's ordinary
inventory and flag objects carry class bit `0x10000000`; the adapter uses that
authoritative inventory state to reproduce the reference carrier guard. A
non-carrier may route to the nearest potion, while a carrier only diverts when
the potion passes the normal interaction/visibility test. After the recovery
waypoint, aggression is restored to `0.83`; non-CTF bots return to Hunt, while
CTF bots re-enter the flag strategy documented in section 16.2.

---

# 10. Monster event callback system

The Go reference relies heavily on object/monster events.

Nox already has the matching callback descriptors inside the monster-AI block.

The callback dispatcher used in the native engine is currently still exposed as:

```text
sub_502490(callback_slot, event_object, self)
```

The bot implementation should ideally add a typed/documented wrapper before calling or replacing callback descriptors directly.

## Confirmed callback slots

| Monster-AI offset | Event meaning |
|---:|---|
| `+1224` | Looking For Enemy |
| `+1232` | Enemy Sighted |
| `+1240` | Change Focus |
| `+1248` | Is Hit |
| `+1256` | Retreat |
| `+1264` | Death |
| `+1272` | Collision |
| `+1280` | Enemy Heard |
| `+1288` | End Of Waypoint |
| `+1296` | Lost Sight Of Enemy |

These are the same event concepts used by the Go Bot-Script implementation.

### Confirmed native dispatch sites currently integrated

The optional bot runtime records policy events immediately alongside the
existing native callback dispatch. It does **not** replace or suppress
`sub_502490`.

| Event | AI callback slot | Confirmed native dispatch site(s) | Event object passed to policy |
|---|---:|---|---|
| Looking For Enemy | `+1224` | `sub_546850` | `0`, matching native callback |
| Enemy Sighted | `+1232` | `sub_5287B0`, `sub_533030` | perceived enemy |
| Change Focus | `+1240` | `nox_xxx_mobActionFightStart_531E20` (`sub_531E20__abi_raw`) | current focus from AI `+1196` |
| Is Hit | `+1248` | `nox_xxx_unitUpdateMonster_50A5C0` (`sub_50A5C0__abi_raw`) | native hit context at object `+520` |
| Retreat | `+1256` | `nox_xxx_monsterMainAIFn_547210` (`sub_547210`) | `0`, matching native callback |
| Death | `+1264` | `sub_544C40` | native death callback object |
| Collision | `+1272` | `sub_4E83B0`, `sub_4E96F0`; native player bots also observe `nox_xxx_collidePlayer_4E8460` | colliding object |
| Enemy Heard | `+1280` | `sub_50D110` | source resolved by `sub_4EC580` |
| End Of Waypoint | `+1288` | `sub_544FF0` | native waypoint callback object |
| Lost Sight | `+1296` | `sub_528560` | lost enemy |

The optional integration records each event immediately alongside the existing
`sub_502490` dispatch. It does not consume, replace, reorder, or suppress the
native callback. This gives Bot-Script policy the same event concepts while
keeping original Nox behavior authoritative.

### Recommended port approach

Do not create a second event bus.

Either:

1. attach bot callbacks through the existing callback descriptors where safe; or
2. hook the existing native event sites under `#ifdef NOX_BOT_SUPPORT` and call class-policy functions.

The second approach may be easier initially because the decompiled callback descriptor representation is still fairly opaque.

---

## 10.1 `nox_xxx_collidePlayer_4E8460`

Recovered declaration:

```c
void nox_xxx_collidePlayer_4E8460(int a1, int a2);
```

### Confirmed role

Authoritative collision handler for normal player objects. In particular, when
Warrior ability `1` (Berserker Charge) is active it owns Charge impact/stop
behavior rather than the Bot-Script policy layer. Native player bots remain
player objects outside `4FAB20`'s temporary monster morph, so their physical
collisions use this path.

With `NOX_BOT_SUPPORT`, this function also records the Collision policy event
before preserving all existing native collision behavior.

---

## 10.2 `nox_xxx_collideMonsterEventProc_4E83B0`

Recovered declaration:

```c
unsigned char* nox_xxx_collideMonsterEventProc_4E83B0(
    int a1,
    int a2);
```

### Confirmed role

Dispatches the native monster Collision event through the callback descriptor at AI offset `+1272`.

### Bot-port use

Useful for Bot-Script behavior such as collision-sensitive Warrior actions and objective contact.

---

## 10.3 `nox_xxx_mobGenericDeath_544C40`

Recovered declaration:

```c
int nox_xxx_mobGenericDeath_544C40(int a1);
```

### Confirmed role

Generic monster death processing.

It invokes the monster-AI Death callback at offset `+1264`.

### Bot-port use

Tactical cleanup should use the established death event path rather than periodically polling for dead targets.

---

# 11. Native spell execution and ability lookup

## 11.1 `nox_xxx_monsterCast_540A30`

Recovered declaration:

```c
void nox_xxx_monsterCast_540A30(
    nox_object_t* a1,
    int a2,
    nox_object_t* a3);
```

### Confirmed role

Schedules/executes native monster spell casting using the regular monster action system.

The routine chooses native action forms according to spell metadata and target requirements.

### Bot-port use

This should be the main casting primitive for Wizard and Conjurer tactical policy.

The Bot-Script port should decide:

```text
which spell?
when?
against what target?
```

and leave spell mechanics to Nox.

---

## 11.2 `nox_xxx_abilityNameToN_424D80`

Recovered declaration:

```c
int nox_xxx_abilityNameToN_424D80(const char* name);
```

### Confirmed role

Resolves the engine's named player-ability identifiers to their native numeric
ability index. The optional adapter exposes this as
`nox_bot_engine_ability_id()` so Warrior policy does not embed raw ability
numbers. Unknown names resolve to the engine's zero/invalid result.

### Bot-port use

The adapter uses this only for name resolution; execution remains on the native
player ability path described below.

---

## 11.3 `nox_xxx_playerExecuteAbil_4FBB70`

Recovered declaration:

```c
void nox_xxx_playerExecuteAbil_4FBB70(nox_object_t* player, int ability);
```

### Confirmed role

This is the authoritative player Warrior-ability entry point. It validates the
player object and Warrior class, checks conflicting/active abilities, checks the
learned ability state where required by the active game mode, checks the native
per-player cooldown table, writes the ability cooldown, schedules active
duration state when applicable, dispatches the ability implementation, and
plays the normal ability audio/feedback.

The recovered numeric dispatch in `sub_4FBAF0` is:

| ID | Ability | Native implementation |
|---:|---|---|
| 1 | Berserker Charge | `sub_53FEB0` |
| 2 | War Cry | `sub_53FF40` |
| 3 | Harpoon | `sub_540070` |
| 4 | Tread Lightly | `sub_5400B0` |
| 5 | Eye of the Wolf | `sub_540110` |

### Cooldown state

The native cooldown table is indexed as:

```text
byte_5D4594[1568876 + 4 * (ability + 6 * player_slot)]
```

`nox_xxx_abilityCooldown_4252D0` returns the configured cooldown duration.
`nox_xxx_playerExecuteAbil_4FBB70` writes that duration into the table, and
`sub_4FBEE0` decrements the cooldowns from the global server/game update in
`GAME3.c`; cooldown ticking therefore continues for `4FAB20` player bots even
though they do not run the normal player updater.
`nox_common_playerIsAbilityActive_4FC250` reports active-duration state. The bot
adapter exposes these operations without maintaining a duplicate Bot-Script
cooldown table.

### Player-bot safety boundary

Harpoon, War Cry, and Eye of the Wolf can now use their authoritative native
paths after `nox_xxx_updatePlayerMonsterBot_4FAB20` restores normal player
form:

- Harpoon creates the native projectile through `sub_540070` / `sub_538890`;
  the projectile updater `sub_54F380` owns flight, attachment lifetime, range,
  line-of-sight break conditions, and projectile positioning. The only missing
  owner-side reel force was the small block in the normal player updater; that
  exact block is now shared with `4FAB20` under `NOX_BOT_SUPPORT`.
- War Cry performs its radius effect, removes the expected self buffs, and
  enters native ability-active state in its own execution path.
- Eye of the Wolf directly applies native buff/enchant `21` for the configured
  duration and enters native ability-active state.

### Berserker Charge ownership

Berserker Charge is now safe to use from the player-monster path without
running the broad normal-player input/combat updater.

`nox_xxx_playerExecuteAbil_4FBB70` starts ability `1` through `sub_53FEB0` and
registers the native active-duration/cooldown state. While ability `1` is
active, `nox_xxx_playerAttack_538960` enters a Charge-specific branch before
normal weapon processing. That branch derives forward velocity from the
player's facing/speed data and advances the original Charge animation frame.

`nox_xxx_updatePlayerMonsterBot_4FAB20` now shares only this already-active
Charge step after restoring normal player form. While Charge is active it also
preserves player state `1` and player attack-progress byte `+236` instead of
replacing them with monster-action state/progress; this lets
`nox_xxx_playerAttack_538960` advance the original Charge animation rather than
restarting or replaying animation-frame actions each tick. Normal
monster-action-to-player-state translation resumes when native Charge active
state ends. The bot path does not call the
rest of the normal player updater.

Impact behavior remains fully native in `nox_xxx_collidePlayer_4E8460`. When
ability `1` is active, that collision path stops Charge through
`sub_4FC300`, applies the original target/wall effects and damage, and handles
the original movement correction/stun behavior. `sub_4FBEE0` continues to own
ability cooldown and active-duration expiry globally.

Because native player bots collide through `nox_xxx_collidePlayer_4E8460`
outside the temporary monster-AI morph, that player collision path now also
records the Bot-Script Collision policy event. The existing monster callback
collision hooks remain unchanged for monster-form/native-monster dispatch.

The player runtime field at `+132` is the attached Harpoon target. The adapter
exposes it so Warrior policy can distinguish a Harpoon still flying (active
ability with no attached target) from the attached/reel state. The Go reference
blocks Charge while Harpoon is flying but permits Charge after attachment.

Current Charge policy covers Enemy Sighted and Change Focus after the configured
reaction delay, plus the Go collision-triggered follow-up after twice the
configured reaction delay when the caller remains the current target. It also
tracks the authoritative attached-Harpoon target at `player runtime +132`; a
new attachment schedules the Go Harpoon-hit-to-Charge follow-up after the
configured reaction delay.

Warrior policy also reproduces the Go reference's one-second `useAbilities`
loop. At each deterministic `gameFPS` interval, if the current native AI target
is alive and within 150 world units, policy considers Harpoon first, then
reaction-delayed Charge, then reaction-delayed War Cry. Only this scheduling
state is bot-local; target state, ability cooldowns, active ability state, and
execution remain native Nox state.

`sub_4FB9C0` is also not an execution helper: its observed behavior grants or
updates Warrior ability ownership/reward state. The bot implementation must not
use it as a shortcut for casting an ability.

---

## 11.4 `nox_xxx_playerCheckSpellClass_57AEA0`

Recovered declaration:

```c
int nox_xxx_playerCheckSpellClass_57AEA0(
    int player_class,
    int spell);
```

### Confirmed role

Checks whether a spell is valid for a given player class.

### Bot-port use

Useful for initialization validation and class-specific spell resolution.

Do not duplicate class/spell eligibility tables in bot code.

---

## 11.5 `nox_xxx_mapGenSpellIdByName_51E1D0`

Recovered declaration:

```c
int nox_xxx_mapGenSpellIdByName_51E1D0(const char* name);
```

The current C body is still named `sub_51E1D0` in `GAME4.c`, while `funcs.txt`
records the semantic name above.

### Confirmed role

Normalizes a spell name to the engine's expected uppercase spell lookup form
and resolves it through the authoritative spell table. The recovered stack frame
contains two consecutive 60-byte character buffers: one for the uppercased input
and one for the generated `SPELL_<NAME>` key. The decompiled C previously typed
the second buffer as a single `char`; `nox_sprintf()` therefore wrote beyond the
C object and could overlap the source buffer and saved stack state. A bot lookup
such as `SLOW` exposed this as an unbounded `SPELL_____...` format result followed
by a corrupted return address. Keep both recovered locals as arrays; the second
is not a scalar temporary.

`tests/spell_lookup_test.c` drives the production `sub_51E1D0()` entry point with
upper-, lower-, and mixed-case `SLOW` and asserts that the authoritative lookup
receives exactly `SPELL_SLOW`.

### Bot-port use

`bot_engine.c` exposes this as the single spell-name-to-ID lookup primitive.
Class policy should resolve names once rather than embed recovered numeric spell
IDs throughout the source.

---

## 11.5 Spell identifiers

Bot policy should not scatter raw numeric spell IDs throughout class source.

Recommended native bot state:

```c
typedef struct bot_spell_ids {
    int blink;
    int burn;
    int counterspell;
    int death_ray;
    int fireball;
    int force_field;
    int haste;
    int inversion;
    int invisibility;
    int lesser_heal;
    int magic_missiles;
    int ring_of_fire;
    int shock;
    int slow;
    int teleport;
    /* ...Conjurer spells... */
} bot_spell_ids_t;
```

Resolve these once through the game's authoritative spell/name tables.

---

## 11.6 Direct NoxScript-style casting and Warrior `HELD` escape

The Go Warrior does not queue a monster cast when escaping ordinary `HELD`. It
calls NoxScript `CastSpell(SLOW, self, self)` and then removes `HELD`. OpenNox's
NoxScript bridge confirms that this is the direct spell dispatcher path that
reaches `sub_4FDD20`. OpenNox defines the accepted payload as:

```c
struct SpellAcceptArg {
    Object *Obj;
    Pointf Pos;
};
```

On the 32-bit native ABI this is one object pointer/handle followed by two
`float` coordinates. NoxScript `CastSpell` faces `Pos` before dispatch. For an
object-target cast `Obj` is the target and `Pos` is its current position; for a
position cast `Obj` is null and `Pos` carries the requested cursor point.

For a normal native player object, `sub_4FE7B0` would derive spell power from
the player's learned-spell array. That is not equivalent to the reference NPC.
The recovered player-bot AI initialization in `nox_xxx_playerBotCreate_4FA700`
sets AI DWORD `510`, byte offset `+2040`, to `3`; while the player is in the
existing monster view, `sub_4FE7B0` reads that value as monster spell power.
`nox_bot_engine_cast_script_self()` therefore enters the same temporary monster
view already used by other bot adapters, builds the ordinary target/position
argument, calls `sub_4FDD20`, and restores player form. This reproduces the
script/NPC casting semantics without granting Slow to the player's learned
spell table.

Native enchant `HELD` is ID `5`. `sub_4FF5B0(object, 5)` is the authoritative
enchant removal path. Warrior policy performs the direct Slow self-cast first
and then removes `HELD`, matching the reference ordering.

The reference intentionally does not escape two collision-derived stuns:

- a Berserker Charge crash/immobile impact;
- collision with an enemy `Bomber`.

The player collision event is observed before `nox_xxx_collidePlayer_4E8460`
processes Charge. Policy records only that Charge owned the collision; on the
next class update it protects the hold only if native collision actually left
`HELD` active. This avoids treating normal Charge contact with a player/monster
as a crash stun. Enemy `Bomber` collision starts the same two-second protected
window used by the Bot-Script timer. Ordinary `HELD`, or a protected hold still
present after that window, is converted to Slow and removed.

Only the source/timer distinction is bot-local. The Slow spell effect, native
Charge collision stun, Bomber behavior, enchant lifetime, and enchant removal
remain engine-owned.

## 11.7 Wizard direct-cast policy and native mana

The same direct-script contract is now exposed for self, object, and position
targets. `nox_bot_engine_cast_script_object()` and
`nox_bot_engine_cast_script_position()` build the exact `SpellAcceptArg` shape,
face the target position, enter the existing player-bot monster view, call
`sub_4FDD20`, and restore normal player form. This is intentionally separate
from `nox_xxx_monsterCast_540A30`, which queues a monster cast action rather than
reproducing NoxScript's direct `CastSpell` semantics.

Wizard policy uses authoritative player mana rather than recreating the Go
script's `wiz.mana` value. `nox_xxx_playerManaSub_4EEBF0` (`sub_4EEBF0`) is the
native player mana-subtraction path; `nox_xxx_playerManaAdd_4EEB80` (`sub_4EEB80`)
is the corresponding native addition path. The adapter checks the normal player
runtime first and verifies the resulting native mana value. With the reference
default `BotMana=true`, Wizard policy uses the add path for one mana point every
two simulation seconds while below the reference 150-mana ceiling. Spell effects
themselves do not receive a second bot-owned mana model.

The Bot-Script concepts that do not exist as authoritative Nox state remain
server-local policy deadlines: its three-frame global spell gate, per-spell
cooldowns, difficulty reaction delay, pending target/cursor snapshot, and current
reference target. Runtime class dispatch now calls this policy for player class
`1` after `4FAB20` has restored normal player form.

Current spell decisions implemented by this slice are Slow, Death Ray,
Fireball, Burn, Ring of Fire (`CLEANSING_FLAME`), Energy Bolt (`LIGHTNING`),
Magic Missile, Counterspell, Shield, Lesser Heal, Haste, Shock, Protection From
Electricity, Protection From Fire, and Invisibility. Enemy Heard also uses the
reachable reference Invisibility response when the remembered target is hidden, except when the Wizard is the active CTF
enemy-flag carrier (`TeamTank`). Native
spell/enchant mechanics remain authoritative. The policy now also performs the
reference 15-frame 75-unit nearby-loot scan through native pickup/equip paths,
uses the unambiguous 10-second `FireStormWand -> ForceWand` preference, and
reuses shared CTF destination steering on Lost Sight / End Of Waypoint.
Two Bot-Script quirks are preserved deliberately: Energy Bolt requires
`mana > 10` but never subtracts mana, and Ring of Fire becomes unavailable
for the rest of the life because the reference cooldown callback writes
`ShockReady=true` instead of restoring `RingOfFireReady`. The Enemy Heard
`castFireballAtHeard()` branch is not reproduced because its caller requires an
unseen target while the helper itself requires visibility, making that cast
unreachable as written. Bot-Script Blink is implemented as the script actually
uses it: a `Glyph` is created at the bot with `BLINK` as its sole trap spell,
rather than directly casting Blink on the player. The normal mana-obelisk route
is also implemented: low-mana hit/waypoint handling and missing core buffs choose
the nearest native mana source with at least 10 charge, lower aggression, and
walk there; an active CTF `TeamTank` only chooses a visible source. Native
`sub_53C580` remains authoritative for the actual 50-unit mana transfer and source
regeneration. Hidden-target Trap placement now mirrors the reference owned
three-spell `Glyph`: `CLEANSING_FLAME`, `MAGIC_MISSILE`, and `SHOCK`, with a
105-mana cost, five-second Trap cooldown, 15-frame global gate, and no more than
four active Glyphs owned by the Wizard. Drain Mana now preserves the reachable
Bot-Script triggers while leaving mechanics native: eligible visible 75/100-HP
targets and nearby visible mana sources schedule `DRAIN_MANA` after the configured
reaction delay, with the reference three-second cooldown. Native
`nox_xxx_spellDrainMana_52E210`/`sub_52E610` choose the actual source and
`sub_52E450` performs the transfer, so bot policy does not duplicate the Go
script's manual source/player mana mutation. Implemented Wizard spells now run
the reference phoneme sequence between the configured reaction deadline and the
native release, at three frames per phoneme; compound Trap additionally preserves
its three concentration pauses, producing a `reaction + 57` release deadline.
Hostile `DeathBall` reaction is implemented
by scanning the native world list within 500 units, following object owner links
at `+492`, requiring an enemy player/monster in that chain, and scheduling
Counterspell at the Wizard position. If no `DeathBall` is present, a class-`0x01`
missile whose owner chain reaches the current target schedules native Inversion
after the configured reaction delay; the Bot-Script 10-mana cost, one-second
cooldown, and three-frame global gate remain policy state.

---

## 11.8 Conjurer direct-cast policy and native mana

Native player class `2` now dispatches to `bot_conjurer.c` through the same
server-local policy/runtime boundary used by Warrior and Wizard. No new spell
mechanics are implemented in bot code: Conjurer casts go through the recovered
NoxScript-style direct dispatcher documented above, and mana is read/added/
subtracted through the normal player runtime helpers.

The first policy slice preserves the reference's deterministic choices where
the engine owner is already clear:

```text
Enemy Sighted
    -> Force of Nature at target position

Looking For Enemy / Lost Sight
    -> Infravision

visible Held/Slowed target
    -> Meteor -> Toxic Cloud -> Burn -> Counterspell

Pixie Swarm
    -> cast when no live native Pixie is owned by this Conjurer

ordinary visible target
    -> Stun outside CTF
    -> Slow in CTF

no visible target
    -> Vampirism -> random native summon
       -> Protection From Electricity -> Protection From Fire
       -> Protection From Poison

Retreat, visible Held/Slowed self, or hidden Slowed self
    -> Blink Glyph escape (except CTF TeamTank)
```

Lesser Heal is considered before visible-target offense when health is `<= 60`
and native mana is at least 100. Red/Blue potion use delegates to the existing
player inventory/potion path. Passive mana uses the reference default cadence of
one point every two simulation seconds and is capped at 125 for Conjurer policy.
Only reaction/cooldown deadlines and the remembered tactical target are
server-local; health, mana, enchant state, spell effects, damage, target
visibility, and Pixie ownership remain native Nox state. `bot_engine.c` counts
owned Pixies directly from the authoritative world-object list by type ID,
removed-state flag, and the native owner chain; no duplicate `PixieCount` is
stored.

The Conjurer now also reuses the native world-object iterator, pickup dispatcher,
and player equip paths for the reference 15-frame 75-unit loot loop: the listed
wands/bows/crossbow, Quiver, leather/cloth armor, and potions are acquired
without a bot-owned inventory model. Quiver remains an inventory item rather
than being forced through the weapon-equip function. Lost Sight and End Of
Waypoint in CTF call the shared `bot_team.c` destination policy; CTF mechanics
remain native.

Blink now follows the reference `NewTrap` semantics by creating a native `Glyph`
at the Conjurer and storing `BLINK` in its glyph init data. Random creature policy
uses the reference small/medium/large selection and cooldown families, but checks
capacity through native `sub_500D10`/`sub_500D70` rather than maintaining a second
`CreatureCage` counter. The ordinary mana-source route uses the same native
`sub_53C580` source state and transfer path as Wizard. The custom Bot-Script
Bomber branch is now implemented without inventing a second summon lifecycle:
native `sub_5016C0` constructs the Bomber using summon-guide capacity index `5`
from `sub_500D70`, establishing summoned state, owner/player bookkeeping and team
membership. Bot policy checks authoritative owned `Bomber` world objects, creates
the reference inventory `Glyph` carrying `BURN`, `TOXIC_CLOUD`, and `STUN`,
inserts it through `sub_4F3070`, invokes native Follow through `sub_5158C0`, and
looks up/dispatches `BomberSummon` through `sub_40AF50`/`sub_501960`. The
reference `mana >= 80` check/no-subtraction quirk and three-frame summon gate are
preserved. Creation now ORs `0x100` (`MonStatusAlert`) into the Bomber monster
runtime status word at `+1440` and calls `sub_4E8020` to mark the native monster
state update. This matches the recovered OpenNox `MonsterStatusEnable` layout
without introducing bot-local alert state.

Bomber event choreography reuses the native dispatch sites already intercepted
for player-bot event capture rather than fabricating NoxScript callback records:

- `sub_5287B0` / `sub_533030` — Enemy Sighted;
- `sub_50D110` — Enemy Heard;
- `sub_528560` — Lost Enemy/Lost Sight.

`nox_bot_runtime_event()` first checks whether the event receiver is a native
`Bomber` whose owner chain reaches an active Conjurer player bot. Enemy Sighted
and Enemy Heard call native `sub_515D30` through the bot-engine attack adapter
using the owning Conjurer's current Bot-Script target, exactly matching the
reference closures' `bomber.Attack(con.target)`. Lost Enemy calls native
`sub_5158C0` through the Follow adapter and restores `Follow(con.unit)`. The
normal `sub_502490` monster script callback still runs after the hook, so normal
monster/script behavior is not replaced and non-bot Bombers are unaffected.
Hostile `DeathBall` Counterspell is implemented through the same native owner-chain
search as Wizard policy; the Conjurer keeps the reference 20-second cooldown for
that reaction while its ordinary Counterspell remains 5 seconds. If no nearby
`DeathBall` exists, a class-`0x01` missile whose owner chain reaches the current
target schedules native Inversion using the reference 10-mana cost, one-second
cooldown, reaction delay, and shared three-frame spell gate. Pixie Swarm itself
is now direct-cast through native spell mechanics and uses
world ownership to decide whether another swarm should be created.
The reference `WeaponPreference()` remains internally inconsistent, but its
observable control flow is now preserved literally every 10 seconds instead of
being reinterpreted: a present `CrossBow` that is not the equipped weapon gates
an attempted `FireStormWand` equip; only when that branch does not apply does a
present unequipped `InfinitePainWand` gate an attempted `ForceWand` equip. The
native inventory and equipped-weapon adapters remain authoritative for those
checks and equipment transitions.

All currently implemented Conjurer spell and summon branches now preserve the
Bot-Script chant before native release. The scheduler starts after the configured
difficulty reaction delay, emits one reference phoneme every three frames, and
releases three frames after the final sound. The custom Bomber preserves its
four nested Stun/Burn/Toxic Cloud/Glyph chants plus the three explicit
concentration pauses, giving a `reaction + 48` release deadline. Ordinary native
summon spells use their exact Bot-Script summon chants. Inversion deliberately
uses `FemaleSpellPhonemeUpRight` for its second phoneme, matching the reference.

## 11.9 Shared native-backed CTF destination policy

`src/bot_team.c` factors the high-level Bot-Script `CheckAttackOrDefend` /
`WalkToOwnFlag` destination choice out of Warrior policy so all native player
bot classes use the same rule. The helper only reads the already-authoritative
world flag / carried-flag / team relationship state exposed by `bot_engine.c`
and issues native WalkTo or Guard actions. It never changes flag ownership,
score, capture state, or team state.

The shared decisions are:

```text
carrying enemy flag
    -> guard the current own-flag/TeamBase target

own flag present
    -> move toward enemy flag, or its native carrier

both flags carried
    -> pursue the native carrier of the own flag

Lost Sight with own flag dropped
    -> move to the dropped own flag first
    -> otherwise fall back to attack/defend
```

This is now used by Warrior and by Wizard/Conjurer Lost Sight / End Of Waypoint
CTF handling. One Bot-Script role is recoverable without adding duplicate team
state: when `CheckPickUpEnemyFlag` succeeds the script assigns `TeamTank` to that
carrier, while native CTF stores the enemy flag in the same player's inventory.
`nox_bot_team_is_ctf_tank()` therefore treats the current enemy-flag carrier as
the active `TeamTank`. Warrior Charge and Wizard Invisibility now preserve the
reference carrier-specific exclusions. Other coordinated team-role choices remain
policy-level work.

# 12. Name/type lookup used during native AI initialization

## 12.1 `nox_xxx_getNameId_4E3AA0`

Recovered declaration:

```c
int nox_xxx_getNameId_4E3AA0(char* a1);
```

### Confirmed role

Resolves a named game object/type entry to its native identifier.

### Bot-port relevance

`playerBotCreate` uses this as part of native monster-definition initialization.

New bot code should use authoritative game lookup functions instead of hard-coding object type IDs when resolving items/spells/templates.

---

## 12.2 `nox_xxx_monsterDefByTT_517560`

Recovered declaration:

```c
void* nox_xxx_monsterDefByTT_517560(int a1);
```

### Confirmed role

Returns native monster definition data by type/template identifier.

### Bot-port relevance

Already used internally by `playerBotCreate`.

Bot tactical code should not need to depend directly on the returned opaque structure unless later behavior requires monster-definition metadata.

---

# 13. Spawn and relocation

## 13.1 `nox_xxx_mapFindPlayerStart_4F7AB0`

Recovered declaration:

```c
void nox_xxx_mapFindPlayerStart_4F7AB0(
    float2* a1,
    nox_object_t* a2p);
```

### Confirmed role

Finds an appropriate native player start position for the given player object.

### Bot-port use

Reuse for native player-bot initial spawn/respawn unless a specific game mode has its own authoritative spawn path.

Do not maintain a separate Bot-Script spawn-point list for normal native modes.

---

## 13.2 `nox_xxx_unitMove_4E7010`

Recovered declaration:

```c
void nox_xxx_unitMove_4E7010(
    nox_object_t* obj,
    float2* a2);
```

### Confirmed role

Moves/places a unit at the requested position using normal native object relocation semantics.

### Bot-port use

Used indirectly by native bot respawn.

Could also be used by explicit admin/debug spawn commands after player creation if that is consistent with normal spawn handling.

---

# 14. Default player inventory and equipment

## 14.1 `nox_xxx_playerMakeDefItems_4EF7D0`

Recovered declaration:

```c
char nox_xxx_playerMakeDefItems_4EF7D0(
    int a1,
    int a2,
    int a3);
```

### Confirmed role

Creates the normal default player items/loadout.

### Bot-port use

Already invoked by `nox_xxx_respawnPlayerBot_4FAC70`.

The bot subsystem should reuse this rather than manually recreating default class equipment after every death.

---

## 14.2 `nox_xxx_playerEquipWeapon_53A420`

Recovered declaration:

```c
int nox_xxx_playerEquipWeapon_53A420(
    uint32_t* a1,
    nox_object_t* item,
    int a3,
    int a4);
```

### Confirmed role

Native player weapon equip path.

### Bot-port use

Use this if Bot-Script parity requires the tactical layer to switch/equip a particular acquired weapon.

Prefer player equipment APIs because native bots remain player objects.

---

## 14.3 `nox_xxx_playerEquipArmor_53E650`

Recovered declaration:

```c
int nox_xxx_playerEquipArmor_53E650(
    uint32_t* a1,
    nox_object_t* item,
    int a3,
    int a4);
```

### Confirmed role

Native player armor equip path.

### Bot-port use

Use when class policy intentionally changes equipped armor.

---

## 14.4 `nox_xxx_usePotion_53EF70`

Recovered declaration:

```c
int nox_xxx_usePotion_53EF70(
    nox_object_t* player,
    nox_object_t* potion);
```

### Confirmed role

Applies the authoritative native potion/item effect for a player object. The
item effect flags come from the potion object, while health/mana restoration
amounts are derived from the item's configured magnitude and player class. For
a health potion, the function checks current versus maximum health, calls the
native heal path, plays the normal potion audio, and consumes the item through
the regular object deletion path when an effect is applied.

### Inventory layout used by the adapter

For the existing player object representation:

```text
player object +504  -> first inventory object
inventory object +496 -> next inventory object
inventory object +4   -> 16-bit object/type ID
```

The optional bot adapter resolves a requested potion type with
`nox_xxx_getNameId_4E3AA0`, walks that existing inventory chain, and delegates
use to `nox_xxx_usePotion_53EF70`. It does not reproduce healing values or
consume items itself. The current Warrior policy uses this for `RedPotion` only
when health is `<= 100` and below maximum health, matching the Go policy trigger
while retaining native Nox potion mechanics.

---

### World loot iteration used by Warrior policy

The Go reference's `findLoot()` can be reproduced without adding a bot-owned
item registry. The native adapter uses:

```text
sub_4DA790 / nox_server_getFirstObject_4DA790
sub_4DA7A0 / nox_server_getNextObject_4DA7A0
sub_5370E0                                  visibility / interaction trace
sub_4F36F0                                  authoritative pickup dispatch
sub_53A420                                  player weapon equip
sub_53E650                                  player armor equip
```

World objects expose their native type identifier at object `+4`, world-list
next pointer at `+444`, owner at `+492`, and position at `+56/+60`. Native
`sub_4F3070` writes the receiving object into item `+492` when inventory ownership
is established, confirming the field's owner role. The adapter
resolves the requested type name with `sub_4E3AA0` and ignores removed/owned
objects. `nox_bot_engine_find_nearest_visible_type()` additionally applies the
normal visibility trace for nearby loot, while `nox_bot_engine_find_nearest_type()`
uses the same list/distance ownership rules without visibility for reference
behaviors such as long-range potion recovery. Pickup itself remains owned by
`sub_4F36F0`, so
capacity, item-specific pickup handlers, inventory linkage, and network-visible
state stay in Nox.

The same authoritative iterator/owner fields support two higher-level ownership
queries without introducing bot-local projectile or summon registries:

- `nox_bot_engine_owned_type_count()` resolves a named type, skips removed
  objects, and follows owner links until it finds the requested player. Conjurer
  Pixie policy uses it to replace the Go script's separately polled `PixieCount`
  with authoritative engine ownership state.
- `nox_bot_engine_find_nearest_world_type()` returns the nearest non-removed
  object of a named type without filtering ownership. Wizard/Conjurer use this
  first for Bot-Script's `DeathBall`-before-generic-missile branch.
- `nox_bot_engine_find_nearest_enemy_owned_type()` applies the hostile owner-chain
  test to that nearest named object. This deliberately does not skip past a
  nearer friendly/unowned `DeathBall`, matching the reference `FindClosestObject`
  followed by `HasOwner(enemy)` ordering.
- `nox_bot_engine_find_nearest_missile_owned_by()` scans class bit `0x01`
  missiles, follows owner links up to 32 levels, and returns the nearest missile
  whose owner chain reaches the exact current target. Wizard and Conjurer use it
  for the reference 500-unit generic Inversion reaction.

These helpers intentionally follow the recovered owner chain rather than only the
immediate `+492` owner, matching NoxScript `HasOwner` semantics more closely.
Native `sub_52BE40`, called by the Inversion area scan in `sub_52BEB0`, confirms
that `object +8 & 0x01` is the missile-class test. It separately requires
`object +12 & 0x02` before transferring a missile's ownership/direction, so bot
policy intentionally filters only `ClassMissile` as the Go reference does and
leaves the actual invertibility/magic-missile decision to native Inversion.

The same adapter now exposes native mana-source and summon ownership without
creating parallel bot state:

- `nox_bot_engine_find_nearest_mana_source()` scans world objects whose update
  callback is `sub_53C580`, reads their native runtime charge at `+748`, requires
  at least the requested charge, and optionally applies the normal visibility
  trace for the CTF `TeamTank` rule. `sub_53C580(source)` takes the mana-source
  object, scans nearby eligible players within 50 world units, consumes source
  charge while increasing player mana, and regenerates an idle source toward 50;
  policy only chooses/walks to a source and never reproduces that transfer.
- Wizard Drain Mana deliberately uses a different native ownership path from
  ordinary obelisk routing. `nox_xxx_spellDrainMana_52E210` is the duration-spell
  update, `sub_52E610` scans `ManaDrainRange` for the best eligible source, and
  `sub_52E660` applies source/player/monster eligibility plus visibility/enemy
  checks. `sub_52E450` performs the actual source-to-caster mana transfer. Bot
  policy therefore only decides when to cast `DRAIN_MANA`; it does not replay the
  Go reference's direct mana writes on top of the authoritative native spell.
- `nox_bot_engine_summon_cage_used()` delegates to `sub_500D10(player)`, which
  walks the player's native controlled/summoned-object chain and sums each
  creature's cage weight. `nox_bot_engine_summon_spell_fits()` resolves the
  summon spell, converts the spell ID to the native summon index (`spell - 74`),
  and delegates to `sub_500D70(player, index)`, whose observable contract is
  `current cage weight + requested summon weight <= 4`. This keeps the engine's
  summon list and metadata authoritative.
- `nox_bot_engine_random_int()` delegates Conjurer summon selection to native
  `sub_415FA0(min, max)`. The decompiled helper advances the engine RNG table and
  returns an inclusive value in `[min, max]`, preserving the Bot-Script's
  `Random(1, N)` category/creature weighting without adding a second bot RNG.
- `nox_bot_engine_create_spell_trap()` mirrors NoxScript `NewTrap` for the bot
  escape case: create a native `Glyph`, place it at the bot, and populate the
  recovered glyph init layout (`Spells[5]`, count at `+20`, spell argument object
  at `+24`, position at `+28/+32`). Blink policy therefore creates a Blink glyph;
  it does not reinterpret `NewTrap(..., BLINK)` as a direct Blink cast.
- `nox_bot_engine_create_owned_spell_trap3()` uses that same recovered Glyph
  layout for Wizard Trap and then calls native `sub_4EC290(owner, glyph)`, the
  engine ownership path behind NoxScript `SetOwner`. The policy counts `Glyph`
  objects through the existing owner-chain world query instead of maintaining a
  second TrapCount, so native object ownership remains authoritative.
- `nox_bot_engine_bomber_fits()` hides the recovered native Bomber summon-guide
  index `5` behind `sub_500D70`. `nox_bot_engine_create_bomber()` then delegates
  object creation to `sub_5016C0`, which establishes summoned state, owner/player
  bookkeeping, team membership, and native acquisition/network state. The helper
  creates a `Glyph` with `BURN`, `TOXIC_CLOUD`, and `STUN`, inserts it in the
  Bomber inventory through `sub_4F3070`, calls `sub_5158C0` through the shared
  Follow adapter, enables `MonStatusAlert` in native monster status, resolves
  `BomberSummon` through `sub_40AF50`, and emits it on the new Bomber through
  `sub_501960`. Owned-Bomber limits still come from the existing authoritative
  world-owner query rather than a policy counter.
  `sub_40AF50(name)` is the native sound-name-to-ID lookup used throughout the
  game data paths; `sub_501960(sound, object, 0, 0)` dispatches that sound as an
  object-centered audio event. `nox_bot_engine_play_phoneme()` reuses this pair
  for Bot-Script `AudioEvent` spell chants, mapping the male NPC directional
  phonemes plus the reference Inversion-specific `FemaleSpellPhonemeUpRight`.
  Wizard/Conjurer policy owns only the deterministic three-frame chant scheduler;
  the sound system and final spell/summon mechanics remain native.
  `nox_bot_engine_owner_player()` follows the same bounded native owner chain
  used by the other bot world queries, while
  `nox_bot_engine_enable_monster_alert()` keeps the recovered status mutation and
  `sub_4E8020` update notification behind the adapter boundary.

The current Warrior policy performs the Go reference's 75-unit scan every 15
simulation frames for its listed melee weapons, Chakrams, potions, and armor.
It also runs the documented 10-second melee preference using the existing player
weapon equip path: `GreatSword`, then `WarHammer`, then `Longsword`. RoundChakram
throwing now uses the original player weapon-attack lifecycle documented below.

### Native RoundChakram player-attack lifecycle

The Go reference equips an inventory `RoundChakram`, faces its current target,
and calls `HitRanged()`. For a native **player** bot, the correct equivalent is
not `nox_xxx_monsterMissileAttack_515B80`: that routine consumes monster-AI
weapon/projectile definition state and does not represent the player's equipped
weapon at `player runtime +104`.

The native player path is instead:

```text
player runtime +104 -> equipped RoundChakram item
        ↓
nox_xxx_playerInputAttack_4F9C70
        ↓
player state +88 = 1
object +136 = native attack start frame
player runtime +236 = attack animation progress
        ↓ each bot update while the Chakram attack is owned
nox_xxx_playerAttack_538960
        ↓
original RoundChakram weapon branch releases the equipped item
        ↓
native in-motion Chakram object / collision / return lifecycle
```

`nox_xxx_playerInputAttack_4F9C70` is the authoritative normal-player attack
entry point. It performs the normal attack eligibility checks, initializes the
attack timestamp/progress, enters player state `1`, and applies the same native
attack-side buff/state changes as a human player.

`nox_xxx_playerAttack_538960` is then called once per bot simulation update
while bot policy owns the Chakram attack. The engine's existing RoundChakram
branch owns projectile creation, removal of the equipped item from the player,
transfer of the real item/modifiers into the in-motion object, launch velocity
and orientation, collision, and eventual return behavior. The bot layer does
not synthesize a missile.

The authoritative release signal used by policy is the equipped-weapon pointer
at `player runtime +104`: once native attack processing moves the original
Chakram out of that slot, bot policy clears its temporary player-attack state
and reapplies the already documented `GreatSword → WarHammer → Longsword`
preference. This deliberately replaces the Go implementation's fixed
five-frame restore timer with the native release transition so the port cannot
change weapons before Nox has actually launched the Chakram. The Go reference's
10-second RoundChakram tactical cooldown remains a server-local simulation-frame
deadline because Nox has no corresponding Warrior-ability cooldown entry for a
normal weapon throw.

While this native weapon attack is active, `4FAB20` must not overwrite player
state `1` or player attack-progress byte `+236` from monster AI. The optional
runtime therefore preserves both values for the active Warrior Chakram policy
window, then resumes ordinary monster-action-to-player-state/progress
translation immediately after release or attack termination. Charge and War Cry are not started while
that weapon attack owns state `1`; Harpoon remains engine-independent and may
coexist as in the reference script.

Relevant confirmed fields/functions:

```text
player runtime +88    player state (`1` while attacking)
player runtime +104   equipped weapon object
player runtime +236   attack animation/progress byte
object +136           native player attack start frame

nox_xxx_playerInputAttack_4F9C70
nox_xxx_playerAttack_538960
nox_xxx_playerEquipWeapon_53A420
```

## 14.5 NPC equipment functions

The engine also contains:

```c
int nox_xxx_NPCEquipWeapon_53A2C0(
    int a1,
    nox_object_t* item);

int nox_xxx_NPCEquipArmor_53E520(
    int a1,
    uint32_t* a2);
```

### Bot-port decision

These should **not** be the default bot equipment path because the planned bots use player objects, not ordinary NPCs.

They are relevant as reference implementations only.

---

# 15. Buffs and temporary effects

## 15.1 `nox_xxx_testUnitBuffs_4FF350`

Recovered declaration:

```c
int nox_xxx_testUnitBuffs_4FF350(
    nox_object_t* unit,
    char buff);
```

### Confirmed role

Checks whether a unit has a given native buff/enchantment.

### Bot-port use

Useful for Bot-Script decisions such as:

- whether an enemy is slowed/held;
- whether the bot already has a protection/buff;
- avoiding redundant buff casting.

---

## 15.2 `nox_xxx_buffApplyTo_4FF380`

Recovered declaration:

```c
void nox_xxx_buffApplyTo_4FF380(
    nox_object_t* unit,
    int buff,
    short dur,
    char power);
```

### Confirmed role

Applies an existing Nox buff.

### Bot-port use

Prefer normal spell/ability paths when reproducing gameplay.

Direct application should be reserved for an original native ability path that already uses it, not used as a shortcut around spell mechanics.

---

## 15.3 `nox_xxx_spellBuffOff_4FF5B0`

Recovered declaration:

```c
int nox_xxx_spellBuffOff_4FF5B0(
    nox_object_t* a1,
    int a2);
```

### Confirmed role

Removes/deactivates an existing buff.

### Bot-port use

Potentially useful for native cleanup/ability behavior, but tactical code should generally let native spell systems manage their own effects.

---

# 16. Native CTF integration

The CTF trace strongly favors player-based bots.

## 16.1 `nox_xxx_pickupFlagCtf_4EA490`

Recovered declaration:

```c
void nox_xxx_pickupFlagCtf_4EA490(
    int a1,
    int a2);
```

### Confirmed role

Native Capture-the-Flag collision/pickup/capture/return handling.

### Important eligibility behavior

The collision path only enters native CTF player handling when the colliding object has the native **player object category bit**.

This means:

```text
ordinary NPC
    → does not automatically use normal player CTF pickup path

native player-monster bot
    → remains a player object
    → can use native CTF handling
```

This is a major reason to use the original player-bot mechanism rather than creating NPC-only bots.

### Observed responsibilities

The function includes native handling for:

- own/enemy flag distinction;
- returning the team's flag;
- enemy flag attachment/carrying;
- capture conditions;
- score updates;
- team-state updates;
- inventory/object transitions;
- network/game notifications;
- effects/sounds;
- flag reset/placement.

### Bot-port use

Do **not** port the Go reference's custom CTF rules unless a native gap is demonstrated.

Port only the high-level strategy:

```text
attack enemy flag
defend home
escort carrier
return dropped flag
move to capture point
```

---

## 16.2 Native flag state used by tactical policy

The high-level CTF policy can derive the Bot-Script decisions directly from
ordinary native objects; it does not need a duplicate bot-owned flag model.

Confirmed representation:

```text
flag object +8, bit 0x10000000
    native Flag class bit

flag object +48 team chain
    compared through nox_xxx_unitsHaveSameTeam_4EC520

flag object +748
    flag update data; first two floats are the native home X/Y used by
    nox_xxx_pickupFlagCtf_4EA490 when deciding whether an own flag needs return

player object +504
    inventory head; a carried flag is linked into normal player inventory
```

When a flag is available in the world it appears in the normal server object
list. On enemy pickup, `nox_xxx_pickupFlagCtf_4EA490` removes it from the world
and inserts that same flag object into the player's normal inventory. Therefore:

- `nox_bot_engine_ctf_flag_world()` finds an own/enemy world flag by class bit
  and native team relation;
- `nox_bot_engine_ctf_flag_carrier()` scans ordinary player inventories and
  returns the player carrying the matching team flag;
- `nox_bot_engine_ctf_flag_at_home()` compares current flag position with those
  home coordinates using the same native tolerance value
  (`byte_581450[10160]`) used by `4EA490`.

Warrior policy then reproduces only Bot-Script's strategy layer:

```text
self carries enemy flag
    -> native Guard at the current own-flag/base target

own flag is in world
    -> WalkTo enemy flag, or the native player carrying it

both flags are carried
    -> WalkTo the native player carrying own flag

Lost Sight + own flag dropped away from home
    -> WalkTo own flag first
```

The Bot-Script `TeamBase` object is not a fixed spawn marker after startup. Its
`PreUpdate` path moves it to the team's current flag position every update; when
the flag is carried, the script first moves that disabled flag to its carrier.
The native port therefore resolves a world own flag or, when carried, the native
player carrying it and uses that current position for the carrier's Guard
destination. This preserves the reference behavior without creating the
script-only `ExtentBoxSmall` base proxy.

Native collision remains responsible for actual return, pickup, capture,
scoring, inventory transfer, effects, sounds, and network notifications.

---

## 16.3 `sub_4EA7A0`

Current recovered declaration:

```c
int sub_4EA7A0(int a1);
```

### Strongly inferred role

CTF-related cleanup/iteration over player slots after a flag transition.

It iterates all 32 player slots and checks/removes a particular effect/buff condition.

### Bot-port use

No direct bot call is currently expected.

Documented because it confirms CTF state is propagated through the normal player-slot/buff system.

---

# 17. CTF-supporting inventory/object functions

The native CTF routine uses several existing systems internally.

These are normally **indirect dependencies**, not direct bot APIs.

## `nox_xxx_invForceDropItem_4ED930`

```c
int nox_xxx_invForceDropItem_4ED930(
    int a1,
    uint32_t* a2);
```

Native forced inventory drop path.

## `nox_xxx_inventoryPutImpl_4F3070`

```c
void nox_xxx_inventoryPutImpl_4F3070(
    nox_object_t* a1,
    nox_object_t* item,
    int a3);
```

Native inventory insertion path.

## `nox_xxx_createAt_4DAA50`

```c
void nox_xxx_createAt_4DAA50(
    nox_object_t* obj,
    nox_object_t* owner,
    float a3,
    float a4);
```

Native object placement/creation helper used by CTF state transitions.

### Bot-port rule

Let `nox_xxx_pickupFlagCtf_4EA490` own these transitions.

Do not manually manipulate flag inventory/object ownership from tactical code.

---

# 18. Score and game notifications

Native CTF handling invokes existing score/network notification code.

Relevant recovered functions include:

```c
int nox_xxx_changeScore_4D8E90(int a1, int a2);
int nox_xxx_netReportLesson_4D8EF0(nox_object_t* a1p);
int nox_xxx_netInformTextMsg2_4DA180(int a1, uint8_t* a2);
```

### Bot-port rule

Tactical bot code should not directly increment CTF score.

Correct objective contact should flow through native CTF rules so normal scoring and replication occur automatically.

---

# 19. Team-related CTF helpers

## `sub_4ECBD0`

Current declaration:

```c
int sub_4ECBD0(int a1);
```

### Strongly inferred role

Resolves team identity/index used by CTF handling.

### Bot-port use

Prefer higher-level native team APIs where possible.

This helper is documented because CTF uses it internally, but bot policy should avoid depending on its exact return encoding until it is better named.

---

# 20. Player networking implications

The native player-bot design means no bot-specific network protocol should be required.

The bot remains a normal server-side player object.

After the monster update is converted back into player state, the existing player systems continue to own:

- position/state replication;
- equipment replication;
- team membership;
- CTF interactions;
- score changes;
- standard gameplay effects.

### Bot-port rule

The clients should not need `NOX_BOT_SUPPORT`.

The feature belongs on the authoritative server/game simulation side.

Any new persistent or network-visible state added by bot code should be avoided unless strictly necessary.

Bot-only tactical state should remain server-local.

---

# 21. Player/object data structures

The decompiled code does not yet expose one complete clean C structure for every runtime object involved.

The following layout is therefore documented as a set of **confirmed offsets used by the bot path**, not as permission to create duplicate packed structures.

---

## 21.1 `nox_object_t` fields relevant to bots

The player-bot functions access the following object offsets.

| Offset | Meaning | Confidence / evidence |
|---:|---|---|
| `+8` | object class/category flags; bit `0x01` identifies missiles in native Inversion, `0x02` monsters, and `0x04` players | Confirmed |
| `+12` | class-dependent state/subclass flags: player↔monster morph writes `16`/`0`; native Inversion tests missile bit `0x02` as the invertible magic-missile subclass | Confirmed functional use; keep class-specific semantics behind adapters |
| `+56` | world X position | Confirmed |
| `+60` | world Y position | Confirmed |
| `+124` | facing/orientation used during bot-AI initialization and respawn helper calls | Confirmed use; exact semantic type should remain engine-owned |
| `+136` | native player attack start frame used by `nox_xxx_playerAttack_538960` animation timing | Confirmed |
| `+492` | native object owner pointer; set by inventory insertion and followed by ownership/owner-chain checks | Confirmed functional use; keep behind engine adapters |
| `+496` | linked-object/inventory chain field used in native CTF traversal | Strongly inferred; indirect only |
| `+504` | object inventory/list head used by native CTF | Confirmed functional use; indirect only |
| `+556` | health-data reference; bot respawn dereferences it to determine zero health/death | Confirmed functional use |
| `+744` | object update function pointer | Confirmed |
| `+748` | runtime data pointer; player runtime normally, monster-AI block during bot morph | Confirmed and critical |

### Rule

New bot code should not repeatedly spell these offsets.

Where an existing recovered function exists, call it.

If direct field access is unavoidable, add a focused typed accessor and document it.

---

## 21.2 Normal player runtime block (`object +748` while in player form)

This is the structure referenced by `object +748` before and after the morph operation.

Relevant fields:

| Offset | Meaning | Confidence |
|---:|---|---|
| `+88` | player state/animation state written after monster action translation | Confirmed |
| `+104` | currently equipped player weapon object; used to detect native RoundChakram release | Confirmed |
| `+236` | byte synchronized from monster-AI offset `+481` after update | Confirmed copy; exact semantic name still uncertain |
| `+276` | pointer to player-info/profile/state structure | Confirmed |
| `+292` | pointer to the `0x898` player-bot monster-AI block | Confirmed |
| `+2180` | normal-player-specific field used when non-bot monster morph state is restored elsewhere; not the player-bot AI back-pointer itself | Opaque unless accessed through existing functions |

### Important relationship

```text
object +748
    ↓
normal player runtime
    +276 -> player info
    +292 -> player-bot monster AI
```

Bot code should normally retrieve these through helper/accessor functions rather than raw pointer arithmetic.

---

## 21.3 Player-info structure (`player runtime +276`)

Relevant fields:

| Offset | Meaning | Confidence |
|---:|---|---|
| `+2064` | player/network slot identifier used when sending player-specific messages | Strongly inferred from network call sites |
| `+2251` | player class byte: `0 Warrior`, `1 Wizard`, `2 Conjurer` | Confirmed |
| `+3632` | X position mirrored by player-bot update | Confirmed |
| `+3636` | Y position mirrored by player-bot update | Confirmed |
| nearby `+3696` array region | player-related indexed state used elsewhere | Not required by initial bot port |

### Bot-port use

The class byte is directly useful.

The mirrored position fields should be left to `updatePlayerMonsterBot`.

Do not create duplicate class or position state in the bot subsystem.

---

# 22. Player-bot monster-AI block

`nox_xxx_playerBotCreate_4FA700` allocates:

```text
0x898 bytes
```

for the player bot's monster-AI state.

This is the same style of state consumed by normal monster AI.

It is stored at:

```text
normal player runtime +292
```

and temporarily becomes:

```text
object +748
```

while `nox_xxx_unitUpdateMonster_50A5C0` runs.

---

## 22.1 Important player-bot AI fields

| Offset | Meaning | Confidence |
|---:|---|---|
| `+0` | initialized magic/sentinel value `-559023410` | Confirmed value; semantic purpose opaque |
| `+376` | initialized from object facing/orientation (`object +124`) | Confirmed |
| `+380` | initial X position copied from `object +56` | Confirmed |
| `+384` | initial Y position copied from `object +60`; note class setup also accesses capability arrays through DWORD indexing, so avoid manually overlaying meanings without typed recovery | Confirmed initialization; broader structure still partially decompiled |
| `+481` | byte copied back into player runtime `+236` after each bot update | Confirmed copy; exact meaning uncertain |
| `+544` | signed current-action index; `-1` means no current action | Confirmed by `monsterActionToPlrState` |
| `+548` | death/timestamp frame used by player-bot respawn delay | Confirmed functional use |
| `+552...` | monster action-record storage; action records are accessed with 24-byte stride | Confirmed |
| `+1224` | Looking For Enemy callback descriptor | Confirmed |
| `+1232` | Enemy Sighted callback descriptor | Confirmed |
| `+1240` | Change Focus callback descriptor | Confirmed |
| `+1248` | Is Hit callback descriptor | Confirmed |
| `+1256` | Retreat callback descriptor | Confirmed |
| `+1264` | Death callback descriptor | Confirmed |
| `+1272` | Collision callback descriptor | Confirmed |
| `+1280` | Enemy Heard callback descriptor | Confirmed |
| `+1288` | End Of Waypoint callback descriptor | Confirmed |
| `+1296` | Lost Sight callback descriptor | Confirmed |
| `+1324` | initialized to `30`; native AI tuning byte | Confirmed value; semantic label not yet safe |
| `+1332` | initialized to `-1`; native AI tuning/sentinel byte | Confirmed |
| `+1340` | initialized to `1` | Confirmed value; semantic label not yet safe |
| `+1348` | initialized to `1` | Confirmed value; semantic label not yet safe |
| `+1356` (`DWORD 339`) | class-dependent floating/tuning field; `0` for Warrior, nonzero for spell classes | Confirmed behavior; exact meaning unresolved |
| `+1440` | AI flags; updated before monster update and read by player-state conversion | Confirmed functional use |
| `+2180` (`DWORD 545`) | back-pointer to the normal player runtime block | Confirmed |

### Important caution

The monster-AI structure has overlapping conceptual regions that are still represented as raw arrays/offsets in decompiled code.

Do not define a speculative full public `struct` with guessed names.

Instead, recover fields incrementally as needed and keep opaque regions opaque.

---

# 23. Monster action records

`nox_xxx_monsterActionToPlrState_4FABC0` calculates the current action record using:

```text
24-byte stride
```

from the monster-AI action storage.

### Confirmed properties

- current action index lives at AI `+544`;
- `-1` means no current action;
- each action record is 24 bytes;
- the action type determines the corresponding player state.

### Bot-port rule

Prefer native wrappers that schedule actions.

If direct action manipulation becomes necessary, document:

- action ID;
- parameter layout;
- lifecycle;
- cancellation behavior;
- player-state mapping.

---

# 24. Class-specific native AI configuration

`nox_xxx_playerBotCreate_4FA700` installs different capability/timing data depending on `player_info +2251`.

## Warrior (`class 0`)

The function leaves the spell-oriented configuration field at zero and does not initialize the Wizard/Conjurer capability sets.

## Wizard (`class 1`)

Initializes one set of capability bitmasks/timers, including multiple cooldown durations expressed in game-FPS units.

## Conjurer (`class 2`)

Initializes a different set of capability bitmasks/timers.

### Bot-port implication

The original Nox bot system already knows class-specific AI behavior.

The Bot-Script tactical policy should be layered on top of this native capability setup rather than replacing the entire structure.

The exact meaning of every class capability array entry is **not required for the first port** and should not block implementation.

---

# 25. Timing globals

Two global values are important for deterministic bot timing.

## Current simulation frame

Current native uses show:

```text
byte_5D4594[2598000]
```

as the monotonically advancing simulation frame/tick count.

It is initialized/updated by the engine and is used throughout gameplay timing.

## Game FPS

Current native uses show:

```text
byte_5D4594[2649704]
```

as game simulation FPS.

It is initialized to:

```text
30
```

in the current source.

### Confirmed relationship

Native code expresses durations as:

```text
N * gameFPS
```

and compares them against frame differences.

Example native player-bot respawn:

```text
current_frame - death_frame >= 2 * gameFPS
```

### Bot-port use

The Bot-Script difficulty reaction delays:

```text
Hardcore   0 frames
Hard      15 frames
Normal    30 frames
Easy      45 frames
Beginner  60 frames
```

can therefore be represented directly in simulation frames.

Bot timing tests must advance deterministic ticks rather than use wall-clock sleeps.

### Recommended cleanup

If the bot feature needs direct use of these globals, add named accessors such as:

```c
uint32_t nox_game_frame(void);
uint32_t nox_game_fps(void);
```

rather than introducing more raw `byte_5D4594[...]` references.

---

# 26. Player-slot capacity

Multiple native systems, including CTF and player state handling, iterate:

```text
0..31
```

for a total of 32 player slots.

### Bot-port implication

Native player bots should consume normal player slots.

Do not maintain an unrelated independent capacity of 32 bots in addition to players.

Recommended server-local tactical state:

```c
typedef struct bot_policy_state {
    bool active;
    /* Bot-Script-specific tactical state only. */
} bot_policy_state_t;

static bot_policy_state_t g_bot_policy[32];
```

The current implementation also keeps a small Warrior-only policy sub-structure
for simulation deadlines/state that do not exist in native Nox state: the
next one-second ability scan, a reaction-delayed periodic Charge/War Cry
decision, a reaction-delayed Harpoon-attachment Charge follow-up, and the
Bot-Script RoundChakram 10-second tactical cooldown plus its short native
player-attack ownership window. These fields do not duplicate health, target,
Warrior ability cooldown, inventory contents, projectile state, or position. They are reset when the native Warrior is dead so a respawn cannot
inherit delayed tactical work from the previous life. The policy array remains
indexed by the existing native player slot.

The real available bot count is:

```text
32 - currently occupied/otherwise unavailable player slots
```

subject to any additional native game-mode/network restrictions found during player creation tracing.

---

# 27. CTF player category requirement

The native flag collision path explicitly checks whether the colliding object is a player before invoking normal CTF pickup behavior.

The key object flag is:

```text
object +8, bit 0x04
```

### Consequence

This validates the player-monster architecture.

If bots were ordinary NPC objects, custom CTF emulation would be required.

By retaining real player objects and using the native player-bot updater, native CTF remains available.

---

# 28. Functions likely called directly by new bot code

The initial bot subsystem should aim to call a relatively small set of recovered engine functions directly.

Recommended direct dependency set:

```text
nox_xxx_playerBotCreate_4FA700
nox_xxx_mobMorphFromPlayer_4FAAC0       [adapter enters monster view]
nox_xxx_mobMorphToPlayer_4FAAF0         [adapter restores player view]
nox_xxx_updatePlayerMonsterBot_4FAB20    [assigned as update function]
nox_xxx_updatePlayer_4F8100              [restored on detach/failure]
nox_xxx_monsterWalkTo_514110
nox_xxx_unitHunt_5157A0
nox_xxx_mobSetFightTarg_515D30            [native NoxScript Attack/Fight target]
nox_xxx_monsterGoPatrol_515680            [native NoxScript Guard/Patrol]
nox_xxx_monsterCast_540A30
sub_4FDD20                              [direct NoxScript-style spell execution]
sub_4FF5B0                              [native enchant removal]
nox_xxx_monsterClearActionStack_50A3A0  [only when interruption required]
nox_xxx_monsterIsActionScheduled_50A090
nox_xxx_unitIsEnemyTo_5330C0
nox_xxx_unitsHaveSameTeam_4EC520
nox_xxx_testUnitBuffs_4FF350
nox_xxx_unitGetHP_4EE780
nox_xxx_unitGetMaxHP_4EE7A0
nox_xxx_unitGetOldMana_4EEC80
nox_xxx_playerGetMaxMana_4EECB0
nox_xxx_playerManaAdd_4EEB80
nox_xxx_playerManaSub_4EEBF0
nox_xxx_unitCanInteractWith_5370E0
sub_515980                              [native aggression setter]
sub_40A5C0                             [game-mode flag query]
player inventory class bit 0x10000000     [native carried CTF flag detection]
nox_xxx_abilityNameToN_424D80
nox_xxx_abilityCooldown_4252D0
nox_xxx_playerExecuteAbil_4FBB70
nox_xxx_playerInputAttack_4F9C70     [native equipped-weapon attack entry for RoundChakram]
nox_xxx_playerAttack_538960          [active Charge upkeep and native equipped-weapon attack progression]
nox_common_playerIsAbilityActive_4FC250
nox_xxx_playerCheckSpellClass_57AEA0
nox_xxx_mapGenSpellIdByName_51E1D0
nox_xxx_getNameId_4E3AA0                 [inventory type lookup]
nox_xxx_usePotion_53EF70                  [native potion mechanics]
nox_xxx_playerEquipWeapon_53A420         [if tactical equipment switching needed]
nox_xxx_playerEquipArmor_53E650          [nearby armor loot equip]
sub_4DA790 / sub_4DA7A0                   [world-object iteration for loot]
sub_4F36F0                                [authoritative item pickup dispatch]
sub_5370E0                                [visibility/interact test for loot]
```

Spawn/respawn/CTF/network functions should mostly remain **indirect dependencies** behind the native engine paths.

---

# 29. Functions the feature relies on but should not normally call directly

```text
nox_xxx_mobMorphFromPlayer_4FAAC0
nox_xxx_mobMorphToPlayer_4FAAF0
nox_xxx_monsterActionToPlrState_4FABC0
nox_xxx_respawnPlayerBot_4FAC70
nox_xxx_unitUpdateMonster_50A5C0
nox_xxx_monsterUpdateSeenEnemies_5286D0
nox_xxx_monsterVisionSeeEnemy_5287B0
nox_xxx_aiLostSight_528560
nox_xxx_collidePlayer_4E8460
nox_xxx_collideMonsterEventProc_4E83B0
nox_xxx_mobGenericDeath_544C40
nox_xxx_mapFindPlayerStart_4F7AB0
nox_xxx_unitMove_4E7010
nox_xxx_playerMakeDefItems_4EF7D0
nox_xxx_pickupFlagCtf_4EA490
nox_xxx_inventoryPutImpl_4F3070
nox_xxx_invForceDropItem_4ED930
nox_xxx_changeScore_4D8E90
nox_xxx_netReportLesson_4D8EF0
nox_xxx_netInformTextMsg2_4DA180
```

These are documented because they form the lifecycle/runtime contract that makes the bot feature work.

---

# 30. Proposed native bot adapter

Because many recovered names remain decompiler-oriented, new tactical source should use a narrow adapter.

Example:

```c
bool bot_engine_is_enemy(
    nox_object_t* self,
    nox_object_t* other);

bool bot_engine_same_team(
    nox_object_t* self,
    nox_object_t* other);

int bot_engine_current_target(nox_object_t* unit);
int bot_engine_health(nox_object_t* unit);
int bot_engine_max_health(nox_object_t* unit);
int bot_engine_mana(nox_object_t* unit);
int bot_engine_max_mana(nox_object_t* unit);
bool bot_engine_can_interact(nox_object_t* self, nox_object_t* other);
void bot_engine_position(nox_object_t* unit, float* x, float* y);

void bot_engine_hunt(
    nox_object_t* unit);

void bot_engine_walk_to(
    nox_object_t* unit,
    float x,
    float y);

bool bot_engine_cast(
    nox_object_t* unit,
    int spell,
    nox_object_t* target);

int bot_engine_ability_id(
    const char* name);

bool bot_engine_has_buff(
    nox_object_t* unit,
    int buff);

void bot_engine_interrupt(
    nox_object_t* unit);
```

### Adapter rules

The adapter must:

- remain thin;
- not duplicate simulation state;
- not implement new pathfinding;
- not implement new combat rules;
- not implement custom spell mechanics;
- isolate raw `sub_*` names and field offsets from Bot-Script policy code;
- disappear from normal builds when `USE_BOT_SUPPORT=OFF`.

---

# 31. Proposed bot-specific data structures

The native engine already owns most bot runtime state.

New structures should therefore contain **only state that does not already exist in Nox**.

## 31.1 Per-player policy state

```c
typedef enum bot_difficulty {
    BOT_DIFFICULTY_HARDCORE,
    BOT_DIFFICULTY_HARD,
    BOT_DIFFICULTY_NORMAL,
    BOT_DIFFICULTY_EASY,
    BOT_DIFFICULTY_BEGINNER,
} bot_difficulty_t;

typedef enum bot_order {
    BOT_ORDER_AUTO,
    BOT_ORDER_FOLLOW,
    BOT_ORDER_ATTACK,
    BOT_ORDER_GUARD,
    BOT_ORDER_STAY,
    BOT_ORDER_ESCORT,
} bot_order_t;

typedef struct bot_policy_state {
    bool active;

    bot_difficulty_t difficulty;
    bot_order_t order;

    uint32_t next_reaction_frame;

    nox_object_t* ordered_target;
    float ordered_x;
    float ordered_y;

    union {
        bot_warrior_policy_t warrior;
        bot_wizard_policy_t wizard;
        bot_conjurer_policy_t conjurer;
    } class_state;
} bot_policy_state_t;
```

This is **not** a replacement for the native monster-AI block.

---

## 31.2 Class policy state

Only Bot-Script-specific deadlines/state should be stored.

Example Warrior state:

```c
typedef struct bot_warrior_policy {
    uint32_t warcry_ready_at;
    uint32_t charge_ready_at;
    uint32_t harpoon_ready_at;
    uint32_t eye_of_wolf_ready_at;

    nox_object_t* tactical_target;
} bot_warrior_policy_t;
```

Wizard/Conjurer should follow the same principle.

If a cooldown already exists authoritatively in native spell state, consume that instead of duplicating it.

---

# 32. Reaction delays

Difficulty should preserve the reference Bot-Script values:

```c
static uint32_t bot_reaction_frames(bot_difficulty_t difficulty)
{
    switch (difficulty) {
    case BOT_DIFFICULTY_HARDCORE:
        return 0;
    case BOT_DIFFICULTY_HARD:
        return 15;
    case BOT_DIFFICULTY_NORMAL:
        return 30;
    case BOT_DIFFICULTY_EASY:
        return 45;
    case BOT_DIFFICULTY_BEGINNER:
        return 60;
    }
    return 30;
}
```

These are simulation ticks, matching Nox's native timing model.

---

# 33. Tactical event integration

The policy layer needs the same event concepts as the Go reference.

Suggested native-facing functions:

```c
void bot_policy_on_enemy_sighted(
    nox_object_t* unit,
    nox_object_t* enemy);

void bot_policy_on_looking_for_enemy(
    nox_object_t* unit);

void bot_policy_on_change_focus(
    nox_object_t* unit,
    nox_object_t* target);

void bot_policy_on_hit(
    nox_object_t* unit,
    nox_object_t* attacker);

void bot_policy_on_retreat(
    nox_object_t* unit);

void bot_policy_on_collision(
    nox_object_t* unit,
    nox_object_t* other);

void bot_policy_on_enemy_heard(
    nox_object_t* unit,
    nox_object_t* enemy);

void bot_policy_on_end_waypoint(
    nox_object_t* unit);

void bot_policy_on_lost_sight(
    nox_object_t* unit,
    nox_object_t* enemy);

void bot_policy_on_death(
    nox_object_t* unit);
```

These should be driven from existing event transitions, not by duplicate polling.

---

# 34. Data the tactical layer should never own

Do not add bot copies of:

```text
health
mana
world position
facing
inventory
equipped weapon
equipped armor
team ID
CTF flag ownership
score
player class
native spell state
native buffs
native enemy seen list
native action stack
native respawn state
native player/network slot
```

Those already belong to Nox.

Bot policy can cache a pointer/reference for short-lived decision purposes, but authoritative state remains in the engine.

---

# 35. Player creation/activation trace

`nox_xxx_playerBotCreate_4FA700` initializes bot AI for an **existing player
object**. It does not allocate the player object or player-info slot itself. The
normal player constructor has now been recovered far enough to support a traced
experimental socketless spawn attempt without duplicating its object/runtime
initialization.

Confirmed lifecycle facts:

- `sub_417090(slot)` checks the fixed player-info record's active field at
  `+2092`; it returns `NULL` for an inactive/free slot and returns the 4,828-byte
  player-info record for an occupied slot;
- `sub_417000(slot)` resets/activates that fixed player-info record and stores
  the slot at `+2064`;
- slots `0..30` are the remote-player range used by the join path and slot 31 is
  the native host/local-player slot;
- `sub_4DD320(slot, PlayerOpts)` creates `Player`, `NewPlayer`, or
  `PlayerFemale`, links player runtime ↔ player-info, copies profile data,
  initializes protected gameplay fields, broadcasts player state, chooses a
  native player start, and moves the player there;
- `sub_418B10`/`sub_418B60` enumerate active native teams. Team `+56` is the
  color selector, `+57` is the membership ID used by object team state, and
  `+60` is the external team key resolved by `sub_418AE0`;
- `sub_4DF3C0(player_info)` consumes player-info `+2068` as that external team
  key before falling back to team creation/selection. This establishes the
  corresponding `PlayerOpts +138` field as a pre-constructor team hint rather
  than generic account data;
- `sub_4DDA00(player_info)` disambiguates duplicate visible player names, so
  Bot-Script's repeated class names do not need bot-local slot suffixes;
- `sub_4DE7C0(slot)` is the core removal owner called by the normal network
  `0x22` leave packet. It tears down runtime/player resources, deletes the player
  object, clears player-info `+2056`, and updates multiplayer state.

## 35.1 Recovered `PlayerOpts` shape

The normal connection/host preparation in `sub_435A10` builds 153 bytes:

```text
0..96     PlayerInfo/profile template
97..100   screen X
101..104  screen Y
105..127  serial
128..137  client/account field copied to player-info +2096
138..141  external team key copied to player-info +2068
142..151  fallback team-name field copied/formatted into player-info +2072
152       mode/client flags; bit 7 is consumed by the quest-mode join check
```

The experimental bot constructor mirrors that exact structural layout. It copies
the native host profile as a valid profile/appearance template, replaces the
name and class, uses current screen dimensions, gives each bot a unique synthetic
`BOT-xx` serial, and leaves the client/account-only `+2096` field empty. For an
explicit red/blue request it resolves the already-existing native team by color
and writes that team's `+60` external key at `PlayerOpts +138`; `+142` stays
empty because no new team needs to be named/created. `auto` leaves both team
fields empty so native mode logic retains ownership. Byte 152 follows the normal
builder: the low value is `!sub_40ABD0()` and bit 7 follows local save flag
`0x4`.

Spawn display names now preserve Bot-Script identity rather than synthesizing
slot-number names. With no active native teams the three classes are `Lance`,
`Kirik`, and `Horst`; when `sub_418B10()` reports teams they are `Warrior Bot`,
`Wizard Bot`, and `Conjurer Bot`. If more than one same-class bot is present,
`sub_4DDA00` remains the native duplicate-name disambiguation owner.

This is a deliberate **working attempt**, not a claim that every pre-join network
admission step is unnecessary. `sub_4DD320` normally runs after a real client has
already passed admission and has a network peer. The socketless path reuses it
because it is the most complete authoritative constructor currently recovered,
and the lifecycle trace is designed to expose any assumptions that fail.

## 35.2 Experimental spawn/clear lifecycle

`nox_bot_runtime_spawn_attempt()` performs:

```text
find first inactive slot 0..30 via sub_417090
    ↓
choose Bot-Script class display name from native team presence
    ↓
for explicit red/blue: resolve existing team + seed its external key
    ↓
build synthetic 153-byte PlayerOpts
    ↓
sub_4DD320(slot, opts)
    ↓
verify constructor return + player-info object pointer
    ↓
verify/finalize requested red/blue membership, or leave native choice for auto
    ↓
if update == sub_4E62F0: sub_4E6AA0(player_info) → sub_4F8100
    ↓
nox_bot_runtime_attach_existing_player
    ↓
sub_4FA700 + object update = sub_4FAB20
    ↓
mark slot BOT_SLOT_SERVER_CREATED in server-local lifecycle state
```

If constructor result, team assignment, native spawn-state completion, or bot
activation fails, the path attempts to roll the created player back through
`sub_4DE7C0`. `bot spawn 3v3` is six calls to the same primitive and clears
already-created members if a later member fails.

The first hosted lifecycle capture established one previously unverified state
transition: `sub_4DD320` can return a fully linked/positioned player while its
object updater is still `sub_4E62F0`. Source recovery shows that `sub_4E6860`
installs this pre-active join/respawn updater and that normal gameplay later
re-enters `sub_4E6AA0(player_info)`, which restores `sub_4F8100`. A socketless
bot has no client input stream to drive `sub_4E62F0`, so the experimental spawn
path now explicitly completes that same native transition before calling
`nox_xxx_playerBotCreate_4FA700`. It does not bypass the transition by simply
overwriting the update function.

`bot clear` acts only on slots created by this runtime. It never removes an
arbitrary connected human or a player temporarily controlled through `bot
attach`. It detaches native bot control, enters `sub_4DE7C0`, and clears
server-local lifecycle ownership only when the native player object is gone.
`sub_4DE7C0` itself notifies the bot runtime immediately after clearing
player-info `+2056`, so forced/native removals cannot leave stale bot ownership.

## 35.3 Lifecycle comparison tracing

Tracing is disabled by default. Enable it with:

```text
NOX_BOT_LIFECYCLE_TRACE=1
```

or at runtime on the authoritative server console:

```text
bot trace on
bot trace status
bot trace off
```

Each diagnostic goes to `stderr`, is flushed immediately, starts with
`[bot-lifecycle]`, and contains `path=` and `phase=`. It is lifecycle-only; there
is no per-frame AI logging.

Normal network join/removal emits:

```text
path=network phase=join-packet
path=network phase=join-object-created | join-object-create-failed
path=network phase=join-slot-initialized
path=network phase=join-runtime-linked
path=network phase=join-positioned
path=network phase=join-result
path=network phase=leave-packet
path=network phase=leave-result
```

Server-created bot attempts use the same internal constructor checkpoints but
label them `path=spawn`, making direct comparison with `path=network` possible:

```text
path=spawn phase=begin
path=spawn phase=synthetic-join-begin
path=spawn phase=join-object-created | join-object-create-failed
path=spawn phase=join-slot-initialized
path=spawn phase=join-runtime-linked
path=spawn phase=join-positioned
path=spawn phase=synthetic-join-ready | synthetic-join-failed
path=spawn phase=team-assigned | team-unavailable
path=bot   phase=attach-begin / engine-enable-* / attach-*
path=spawn phase=ready
path=spawn phase=rollback | rollback-failed
path=spawn phase=clear-begin / clear-native-begin / clear-native-* / clear-*
path=spawn phase=ownership-released
```

Native summon creation is also traced as a comparison example:

```text
path=summon phase=start
path=summon phase=object-created
path=summon phase=player-owner-linked
path=summon phase=complete | complete-failed
```

Summons are not player slots; this path exists only to compare another
server-owned object constructor/owner-link flow.

## 35.4 Runtime assumptions still unverified

The following points are intentionally documented rather than hidden behind a
"working" label:

1. `sub_4DD320` performs some sends/bookkeeping for its slot. A hosted run must
   prove those calls safely tolerate a remote slot with no socket/client.
2. Network admission normally happens before `sub_4DD320`, but the recovered
   handshake checks live peer capacity, password/account identity, spectator
   admission, ping state, disabled classes, and whether a requested team may be
   created. Those checks are not a clean gameplay-player-capacity contract and
   are intentionally not replayed for a socketless server-owned bot. Free-slot
   state and the constructor's disabled-class contract remain authoritative here.
3. Explicit red/blue requests now seed the existing team's external key before
   construction and verify/finalize object membership afterward. Logs/gameplay
   still need to verify cross-client presentation and mode-specific side effects;
   `auto` leaves constructor/native mode behavior untouched.
4. Synthetic account fields are empty and serial is `BOT-xx`. This deliberately
   avoids aliasing the host identity, but persistence/ranking/quest modes may
   expect additional identity state.
5. `sub_4DE7C0` is confirmed as the core used by a normal explicit leave packet,
   but a socketless slot never had all peer/network state. A create → clear →
   reuse-the-same-slot run must confirm complete cleanup.
6. The initial pre-active updater transition is now understood and handled through
   native `sub_4E6AA0`; cross-client visibility, scoreboards, team presentation,
   CTF interaction, death/native bot respawn, and late-join visibility still
   require integration observation because standalone adapter tests cannot prove
   them.

Recommended first capture:

```text
1. build with USE_BOT_SUPPORT=ON
2. host a multiplayer game
3. `bot trace on`
4. let one real remote client join (network baseline)
5. `bot spawn auto warrior hardcore`
6. if visible, observe movement/combat/death/respawn
7. `bot clear <slot>`
8. `bot spawn auto wizard normal` and confirm the same freed slot can be reused
9. let the real remote client leave
10. capture stderr lines containing `[bot-lifecycle]`
```

If red/blue teams are active, repeat with `bot spawn red warrior hardcore` and
`bot spawn blue wizard hardcore`. The first useful failure is the last emitted
phase before the engine stops progressing or the first phase whose object/slot
state differs from the real network join.

---

# 36. Commands and configuration

The optional bot build hooks the existing console dispatcher after tokenization
and before the original static command table. Only the literal `bot` top-level
command is consumed; all other commands fall through unchanged. The hook also
refuses the recovered player-issued command context (`byte_5D4594[823692]` is
non-zero), so it does not bypass the original permissions used for commands
originating from a player. Lifecycle mutation additionally requires host/server
game flag `1`.

Implemented commands:

```text
bot spawn <red|blue|auto> <warrior|wizard|conjurer> [hardcore|hard|normal|easy|beginner]
bot spawn 3v3 [hardcore|hard|normal|easy|beginner]
bot clear [all|slot]
bot attach <slot> [hardcore|hard|normal|easy|beginner]
bot detach <slot>
bot difficulty <slot> <hardcore|hard|normal|easy|beginner>
bot trace <on|off|status>
```

`attach`, `detach`, and difficulty changes operate on already-created players and
are the high-confidence lifecycle subset. `spawn`/`clear` are the runtime-
unverified non-client attempt described above. `clear` is ownership-safe: only
slots created by `bot spawn` are eligible.

Shared teammate movement orders are handled separately by the native chat
compatibility layer below; the lifecycle console remains concerned only with
server construction/configuration.

## 36.1 Global Bot-Script chat-response fidelity

The independent global `OnChat` behavior from `BotWars.go` is implemented without
adding teammate-order parsing. The original server receive owner
`sub_51BAD0(slot, packet, length)` handles `MSG_TEXT_MESSAGE` (`0xA8`), normalizes
the incoming text into a wide string, validates the sender, and forwards the
original packet globally or to its resolved team. With `NOX_BOT_SUPPORT`, that
normalized string is additionally passed to `nox_bot_chat_on_message()` before
normal forwarding. The hook does not rewrite, mute, consume, or retarget the
player message.

The compatibility layer recognizes only the reference-global greeting and good-
game phrases. It selects the nearest living active native bot to the sender, uses
native `sub_415FA0` through `nox_bot_engine_random_int()` for the same response
choice ranges, and schedules the chosen response for exactly one simulation second
later (`current frame + game FPS`). `nox_bot_runtime_update()` releases due
responses, so no wall-clock timer or new asynchronous subsystem is introduced. A
bounded 32-entry pending queue allows normal bursts of reference timers while
keeping cosmetic state finite; death/life reset, detach, and removal discard that
bot's pending responses.

The final chat presentation remains native:

```text
sub_528AC0(object, wide_text, 0)
    -> builds MSG_TEXT_MESSAGE with object netcode/position
    -> broadcasts it to active players
```

OpenNox exposes the same recovered function as `nox_xxx_netSendChat_528AC0`,
which independently confirms the role. The same normalized incoming message now
also reproduces the reference's class-local teammate movement callbacks. For
every active bot, the command is
applied only when the sender is visible through `sub_5370E0(..., 0)` and is on
the same native team through `sub_4EC520`:

```text
help / follow / escort / come -> sub_5158C0(bot, sender)   (Follow)
attack / go                   -> sub_5157A0(bot)           (Hunt)
guard / stay                  -> sub_515680(...)           (Guard radius 300)
```

The movement action is immediate, exactly as in the Go callbacks. Warrior and
Wizard preserve the reference two-second `Chatting` gate for acknowledgement
messages; the Conjurer intentionally acknowledges every accepted command because
its reference callback has no such gate. Warrior additionally ignores movement
commands while dead, matching `onWarCommand`. The Go callbacks also assign
`Escorting`/`Guarding` bookkeeping fields, but those fields are not read by any
other current reference path; duplicating them in native policy would create a
second source of truth with no observable behavior, so the native action stack
remains authoritative.

Class-specific ally Shield/Haste/Invisibility/Vampirism chat commands are not
part of this movement-order slice because they alter the existing reaction/
phoneme spell scheduler and mana-warning feedback; they remain separate shared
command work.

No command manually synthesizes CTF scoring, movement, combat, respawn, or spell
state; those remain native systems once the player object exists.

---

# 37. Testing implications

Tests should be divided by ownership.

## Engine integration tests

Cover:

```text
existing player object
    → player-bot AI initialization
    → monster update morph cycle
    → valid player state restored
```

```text
dead player bot
    → advance deterministic frames
    → no respawn before 2 * gameFPS
    → respawn at/after threshold
```

```text
player bot in CTF
    → normal player collision path
    → native flag pickup/capture behavior
```

## Tactical policy tests

Test decision functions with deterministic snapshots:

```text
EnemySighted + Harpoon ready
    → Harpoon decision

enemy held/slowed + Wizard Death Ray available
    → Death Ray priority

low health + valid healing option
    → healing decision
```

## Build tests

Both configurations matter:

```bash
cmake -S . -B build-default
cmake --build build-default
```

and:

```bash
cmake -S . -B build-bots -DUSE_BOT_SUPPORT=ON
cmake --build build-bots
```

The default build must remain free of bot sources/behavior.

---

# 38. Functions requiring improved names/types before or during the port

The following are known well enough functionally to use indirectly, but should be improved if directly touched:

```text
sub_502490    native monster callback/event dispatcher
sub_4ECBD0    team identity/index helper used by CTF
sub_4EA7A0    CTF post-transition player/buff cleanup
sub_4E82C0    CTF/team-state notification helper
```

Follow `AGENTS.md`:

When modifying or testing one of these decompiled functions, record:

- observed inputs;
- state changes;
- callers/data flow;
- visible behavior;
- remaining uncertainty.

---

# 39. Function inventory summary

## Player-bot lifecycle

```text
nox_xxx_playerBotCreate_4FA700
nox_xxx_mobMorphFromPlayer_4FAAC0
nox_xxx_mobMorphToPlayer_4FAAF0
nox_xxx_updatePlayerMonsterBot_4FAB20
nox_xxx_monsterActionToPlrState_4FABC0
nox_xxx_respawnPlayerBot_4FAC70
nox_xxx_updatePlayer_4F8100
sub_417090                         [active player-info slot lookup/free test]
sub_417000                         [player-info slot initialization]
sub_418B10 / sub_418B60            [active native team enumeration]
sub_418AE0                         [team lookup by external key at team +60]
sub_4DF3C0                         [player-info team initialization/selection]
sub_4DDA00                         [duplicate visible player-name disambiguation]
sub_4DD320                         [full native player constructor normally entered from join]
sub_4DE7C0                         [core normal leave/removal owner]
```

## Monster actions / update

```text
nox_xxx_monsterIsActionScheduled_50A090
nox_xxx_monsterPopAction_50A160
nox_xxx_monsterPushAction_50A260
nox_xxx_monsterClearActionStack_50A3A0
nox_xxx_unitUpdateMonster_50A5C0
nox_xxx_monsterWalkTo_514110
nox_xxx_unitHunt_5157A0
nox_xxx_mobSetFightTarg_515D30
nox_xxx_monsterGoPatrol_515680
```

## Perception/events

```text
nox_xxx_aiLostSight_528560
nox_xxx_monsterUpdateSeenEnemies_5286D0
nox_xxx_monsterVisionSeeEnemy_5287B0
nox_xxx_collideMonsterEventProc_4E83B0
nox_xxx_mobGenericDeath_544C40
sub_4E96F0                         [alternate Collision callback dispatch]
sub_533030                         [alternate Enemy Sighted callback dispatch]
sub_544FF0                         [End Of Waypoint callback dispatch]
sub_502490                         [native callback dispatcher]
```

## Teams/targets

```text
nox_xxx_unitIsEnemyTo_5330C0
nox_xxx_unitsHaveSameTeam_4EC520
sub_4ECBD0
```

## Tactical observation

```text
nox_xxx_unitGetHP_4EE780
nox_xxx_unitGetMaxHP_4EE7A0
nox_xxx_unitGetOldMana_4EEC80
nox_xxx_playerGetMaxMana_4EECB0
nox_xxx_unitCanInteractWith_5370E0
sub_515980                              [native aggression setter]
sub_40A5C0                             [game-mode flag query]
player inventory class bit 0x10000000     [native carried CTF flag detection]
nox_xxx_harpoonBreakForPlr_537520     [native attached-Harpoon cleanup]
```

## Casting/buffs

```text
nox_xxx_monsterCast_540A30
sub_4FDD20                         [direct script/NoxScript spell cast; SpellAcceptArg {Obj, Pos}]
sub_4FF5B0                         [native enchant removal]
nox_xxx_abilityNameToN_424D80
nox_xxx_abilityCooldown_4252D0
nox_xxx_playerExecuteAbil_4FBB70
nox_common_playerIsAbilityActive_4FC250
sub_4FBAF0                         [Warrior ability dispatch]
nox_xxx_mapGenSpellIdByName_51E1D0
nox_xxx_playerCheckSpellClass_57AEA0
nox_xxx_testUnitBuffs_4FF350
nox_xxx_buffApplyTo_4FF380
nox_xxx_spellBuffOff_4FF5B0
sub_52BEB0                         [native Inversion area scan]
sub_52BE40                         [native missile inversion/ownership transfer]
sub_53C580                         [native mana-source transfer/regeneration update]
nox_xxx_spellDrainMana_52E210      [native Drain Mana duration/update]
sub_52E610                         [Drain Mana source selection in ManaDrainRange]
sub_52E660                         [Drain Mana source eligibility/visibility filtering]
sub_52E450                         [authoritative Drain Mana transfer]
sub_500D10                         [native controlled-summon cage weight]
sub_500D70                         [native requested-summon capacity check]
sub_5016C0                         [native summoned-monster constructor used for Bomber]
sub_4F3070                         [native inventory insertion used for Bomber Glyph]
sub_5158C0                         [native Follow action used by Bomber]
sub_40AF50                         [native sound-name to sound-ID lookup]
sub_501960                         [native object-centered audio event dispatch]
sub_415FA0                         [native inclusive RNG used by random summon policy]
sub_4E3810 / sub_4DAA50            [native object creation/placement used by NewTrap parity]
sub_4EC290                          [native SetOwner path used by Wizard Trap Glyph]
```

## Spawn/respawn

```text
nox_xxx_mapFindPlayerStart_4F7AB0
nox_xxx_unitMove_4E7010
nox_xxx_respawnPlayerImpl_53FBC0
nox_xxx_aud_501960
```

`nox_xxx_respawnPlayerImpl_53FBC0` is called conditionally during native bot respawn-related handling and remains an engine-owned implementation detail.

## Inventory/equipment

```text
nox_xxx_playerMakeDefItems_4EF7D0
nox_xxx_usePotion_53EF70
nox_xxx_playerEquipWeapon_53A420
nox_xxx_playerEquipArmor_53E650
sub_4DA790 / nox_server_getFirstObject_4DA790
sub_4DA7A0 / nox_server_getNextObject_4DA7A0
sub_4F36F0                                [native pickup dispatcher]
nox_xxx_NPCEquipWeapon_53A2C0     [reference only]
nox_xxx_NPCEquipArmor_53E520      [reference only]
nox_xxx_inventoryPutImpl_4F3070   [native CTF/internal]
nox_xxx_invForceDropItem_4ED930   [native CTF/internal]
```

## CTF/game state/network

```text
nox_xxx_pickupFlagCtf_4EA490
sub_4EA7A0
nox_xxx_changeScore_4D8E90
nox_xxx_netReportLesson_4D8EF0
nox_xxx_netInformTextMsg2_4DA180
nox_xxx_createAt_4DAA50
```

## Lookup/native definitions

```text
nox_xxx_getNameId_4E3AA0
nox_xxx_monsterDefByTT_517560
```

---

# 40. Minimal implementation dependency set

If the port is kept clean, Bot-Script-derived policy itself should depend on only a small adapter API.

Conceptually:

```text
bot policy
    │
    ├─ current frame / FPS
    ├─ player class
    ├─ enemy/team tests
    ├─ target/buff state
    ├─ Hunt
    ├─ WalkTo
    ├─ Cast
    ├─ interrupt current actions when necessary
    └─ native teammate/objective information
```

Everything else should continue to flow through the original Nox player-bot architecture.

---

# 41. Implementation rules derived from this trace

1. **Use player objects, not ordinary NPCs.**
2. **Reuse `nox_xxx_updatePlayerMonsterBot_4FAB20`.**
3. **Reuse the `0x898` native monster-AI block created by `4FA700`.**
4. **Do not create another action system.**
5. **Do not create another perception system.**
6. **Do not create another pathfinder.**
7. **Do not create another respawn system.**
8. **Do not emulate CTF when native player CTF already applies.**
9. **Keep tactical Bot-Script state server-local.**
10. **Use normal player slots and the existing 32-slot architecture.**
11. **Use deterministic simulation-frame deadlines.**
12. **Hide decompiled offsets behind accessors/adapters.**
13. **Only recover additional raw monster-AI fields when the feature truly needs them.**
14. **Do not guess names for opaque fields and then make those guesses architectural dependencies.**
15. **Keep all new bot sources and hooks behind `USE_BOT_SUPPORT` / `NOX_BOT_SUPPORT`.**

---

# 42. Experimental spawn status and remaining verification

The lifecycle blocker has moved from "unknown constructor" to "runtime validation
of the recovered constructor without a peer". The current attempt deliberately
uses native owners instead of copying their internal writes:

```text
sub_417090(slot)                    inactive/free test via player-info +2092
sub_4DD320(slot, synthetic opts)    full native player constructor
sub_4FA700(object)                  native player-bot AI creation/reset
object +744 = sub_4FAB20             native bot update activation
sub_4DE7C0(slot)                    normal leave-packet core removal owner
```

The synthetic `PlayerOpts` builder and server-local ownership/rollback layer are
implemented behind `USE_BOT_SUPPORT`. Explicit teams are now resolved before the
constructor and seeded through the recovered external-team-key field, while
Bot-Script class names are used for spawned identity. What remains is evidence
from hosted logs, not another speculative constructor rewrite. In particular,
verify socketless network sends, game-mode-specific activation, cross-client
team/name presentation, clear/reuse, and native bot death/respawn. The recovered
pre-join peer-admission checks are deliberately not copied as a bot capacity
policy because they mix socket count, authentication, spectator, ping, and team
creation concerns.

If logs show that `sub_4DD320` requires a peer-only prerequisite, recover only
that prerequisite or extract the narrow native constructor boundary demonstrated
by the trace. Do **not** respond by manually cloning more `sub_4DD320` offsets
into bot code. Likewise, if `sub_4DE7C0` leaves socketless state behind, identify
the missing native leave/slot reset owner from the normal-versus-bot comparison
before adding ad-hoc clears.

The current implementation is therefore intentionally suitable for the next
iteration: it is a complete attempt that may already produce visible bots, while
its remaining assumptions are bounded and observable in one run.

---

# 43. Source locations

The main implementation areas inspected for this reference are:

```text
src/GAME4.c
    player-bot create/update/morph/respawn
    monster action/update logic
    enemy perception
    monster casting
    event callback slots

src/GAME3.c
    normal network player join (`sub_4DD320`)
    player-info slot initialization (`sub_417000` call path)
    CTF collision/pickup/capture
    inventory transitions
    player equipment call paths

src/GAME1.c / GAME2.c / GAME_ABI.c
    timing/global initialization
    update-function tables
    supporting native systems

funcs.txt
    recovered function names/signatures

CMakeLists.txt
src/CMakeLists.txt
    optional feature/build conventions
```

---

# 44. Maintenance note

This document should be updated whenever one of the listed decompiled functions is:

- renamed;
- retyped;
- moved;
- given a recovered structure field;
- modified for the bot feature;
- covered by a new regression test.

When an opaque `sub_*` function is confidently understood, promote it to a recovered semantic name and replace the old name in this reference.

The objective is that future contributors should not need to rediscover the player-bot architecture from raw offsets again.
