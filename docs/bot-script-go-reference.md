# Go Bot Script Reference for the Native C Port

## Purpose

This document records the Go bot implementation used as the behavioural reference for the native C bot work in `nox-decomp`.

The reference source is the **Bot-Script** project included with the development materials as `nox-bot-scripts-main`. It implements Warrior, Conjurer, and Wizard bots for OpenNox by creating ordinary Nox NPCs and controlling them through the Go NoxScript API.

The goal of the native port is **behavioural parity, not a line-for-line translation**.

Where Nox already provides a system—monster perception, pathfinding, action execution, combat, spell execution, collision handling, team handling, or other engine behaviour—the native port should reuse that system rather than duplicate it.

The guiding rule is:

> **Port the Go bot's decisions, not the systems Nox already implements.**

For the corresponding recovered `nox-decomp` engine functions, structures, offsets, and native integration paths, see [`decomp/native-player-bot-reference.md`](decomp/native-player-bot-reference.md).

---

## Acknowledgements

A sincere thank you to **Ephreaym**, the author/maintainer identified by the included Bot-Script repository and its GitHub project, for implementing and sharing the bot scripts that form the reference for this work.

The project demonstrates a practical way to build convincing Nox bots by combining the existing monster AI with higher-level tactical logic, class-specific abilities, team strategy, reaction delays, and game-mode behaviour. That work provides an extremely useful behavioural specification for a native implementation.

Thanks also to the **OpenNox**, **NoxScript**, and **noxworld-dev** contributors whose APIs and engine work make the Go implementation possible. The reference scripts make extensive use of:

- `github.com/noxworld-dev/noxscript/ns/v4`
- `github.com/noxworld-dev/opennox-lib`
- OpenNox's native monster, object, spell, team, and event systems

This native port is intended to preserve and build on those ideas while integrating them directly into `nox-decomp`.

> Note: the supplied archive does not contain an AUTHORS, CONTRIBUTORS, or LICENSE file identifying additional Bot-Script contributors by name. If further contributor information is recovered from the upstream repository, this section should be expanded so everyone involved receives appropriate credit.

---

## Native port implementation status

The high-confidence native foundation is implemented behind the optional
`USE_BOT_SUPPORT` build flag. The default remains `OFF`.

Implemented so far:

- `[x]` build-time opt-in and `NOX_BOT_SUPPORT` definition;
- `[x]` morph-safe engine adapter for native player-bot metadata, monster actions, current target, HP/mana, interaction tests, and position;
- `[x]` player slot/class lookup in both normal-player and temporary monster-AI views;
- `[x]` safe conversion of an **already-created** normal player to/from the recovered native player-bot update path;
- `[x]` spell/ability-name lookup, native spell/class validation, and authoritative Warrior ability cooldown/execution helpers;
- `[x]` 32-slot server-local policy storage without duplicating authoritative Nox gameplay state;
- `[x]` Bot-Script reaction delays (`0/15/30/45/60` simulation frames), including wrap-safe deadline comparison;
- `[x]` server-local capture of all ten native event concepts used by the Go reference, alongside the original Nox callbacks;
- `[~]` Warrior tactical subset: native Harpoon, reaction-timed Berserker Charge, native RedPotion use/recovery movement, nearby loot pickup, the reference `GreatSword → WarHammer → Longsword` melee preference, native RoundChakram throwing with the reference 10-second cooldown, reaction-timed Eye of the Wolf/War Cry, the reference one-second close-range ability scan, Harpoon-hit-to-Charge scheduling, Harpoon break-on-hit, held-state escape with protected Charge/Bomber stun windows, TeleportWake pursuit, and native-backed CTF attack/defend/escort/return steering;
- `[x]` focused deterministic regression coverage for the adapter, runtime glue, policy state, event capture, and partial Warrior decisions.

Still intentionally not implemented where native ownership is not completely recovered:

- `[ ]` claiming/creating a free player slot and player object without a human network client;
- `[ ]` authoritative cleanup/freeing of that newly created player slot;
- `[~]` remaining Warrior policy (additional teammate/team coordination beyond the current native-backed CTF objective steering);
- `[ ]` Wizard and Conjurer Bot-Script tactical policy;
- `[~]` native-backed Warrior CTF strategy is implemented; shared multi-class team/objective coordination and bot commands remain pending;
- `[ ]` cosmetic spell-phoneme parity.

The implementation deliberately exposes no spawn command yet. An existing
player can be attached to the recovered native bot update path internally, but
that is only lifecycle substrate; it is not a substitute for correctly creating
a server-controlled player slot. See
[`decomp/native-player-bot-reference.md`](decomp/native-player-bot-reference.md)
for the recovered native lifecycle contract.

## Remaining implementation gaps

The remaining work is intentionally separated by ownership so later patches do
not blur native engine mechanics with Bot-Script policy:

### Lifecycle and availability

- recover the authoritative server-side path that claims a free player slot,
  creates the complete player object/runtime without a human join packet, and
  assigns `nox_xxx_updatePlayerMonsterBot_4FAB20`;
- recover the matching authoritative removal/free path;
- only after those two paths are known, add user-facing `bot spawn`, `bot clear`,
  and multi-bot setup commands.

### Warrior parity

Implemented Warrior combat/recovery mechanics now include held-state escape:
ordinary `HELD` is converted to the reference Slow-on-self effect and removed,
while native Berserker Charge crash stun and enemy `Bomber` stun retain their
reference two-second protected window. Remaining Warrior-adjacent work is:

- low-health potion-seeking movement now feeds into the reference
  post-waypoint CTF objective choice instead of stopping after recovery;
- the Lost Sight `TeleportWake` pursuit/check loop is now implemented with the
  reference 100-unit wake check, native WalkTo, and native Fight-target action;
- basic CTF attack/defend/escort/return steering is now implemented directly
  from native flag world/inventory/carrier state. More coordinated team policy
  and teammate chat orders remain outside the Warrior class;
- exact cosmetic/chat parity and any starting-loadout differences not already
  supplied by native player defaults remain lower-priority fidelity work.

### Wizard and Conjurer

- the native spell execution substrate is available, but the Bot-Script tactical
  priorities, defensive/offensive spell selection, mana/healing decisions,
  summoning, traps, and class-specific teammate commands are not yet ported.

### Team and game-mode strategy

- native CTF pickup/drop/capture/scoring remains authoritative, while the
  Warrior now has the Bot-Script's basic high-level attack/defend/escort/return
  choice. Shared multi-class coordination and teammate orders remain pending;
- Team Arena can fall back to native Hunt, but coordinated team behavior is not
  yet ported;
- additional reference/planned modes such as King of the Realm remain future
  work.

### Commands and fidelity

- difficulty/spawn/team setup commands require the non-client lifecycle above;
- human teammate orders (`follow`, `attack`, `guard`, `stay`, `escort`) need an
  order executor on top of the existing policy enum;
- spell phoneme sequencing, chat responses, and other presentation details remain
  fidelity work after gameplay parity.

### Integration coverage

- deterministic adapter/policy tests exist, but true create → fight → die →
  respawn → remove integration coverage cannot be added through the production
  spawn path until non-client player creation/removal is recovered.

---

# Reference Source

The included repository contains two copies of the script implementation:

```text
nox-bot-scripts-main/
├── Estate/
│   ├── BotWars.go
│   ├── bot.go
│   ├── team.go
│   ├── spells.go
│   ├── warrior.go
│   ├── conjurer.go
│   ├── wizard.go
│   └── ...
│
└── installation_files/
    ├── BotWars.go
    ├── bot.go
    ├── team.go
    ├── spells.go
    ├── warrior.go
    ├── conjurer.go
    └── wizard.go
```

For the native port, **`Estate/` is the primary behavioural reference**.

It is newer and contains functionality not present in the reusable `installation_files/` copy. The installation copy remains useful for understanding the intended public deployment model.

The original project README points to:

```text
https://github.com/Ephreaym/Bot-Script
```

and describes the scripts as supporting Warrior, Conjurer, and Wizard bots with Team Arena and Capture the Flag gameplay.

---


# Build-Time Opt-In Requirement

The native bot feature must be **optional and disabled by default**.

The repository uses the existing optional-feature convention:

```cmake
option(USE_BOT_SUPPORT "Build native player bot support" OFF)
```

When disabled, the bot sources are not appended to `NOX_RUNTIME_SOURCES` and
no bot integration hooks are compiled into the decompiled engine files.

When enabled, the bot runtime currently adds:

```text
src/bot_engine.c
src/bot_policy.c
src/bot_runtime.c
src/bot_warrior.c
```

and defines:

```text
NOX_BOT_SUPPORT
```

Engine integration points use:

```c
#ifdef NOX_BOT_SUPPORT
...
#endif
```

so the default build remains behaviorally unchanged.

Build examples:

```bash
# Normal build: bot support excluded.
cmake -S . -B build
cmake --build build
```

```bash
# Explicit bot-enabled build.
cmake -S . -B build-bots -DUSE_BOT_SUPPORT=ON
cmake --build build-bots
```

Runtime configuration must never substitute for the compile-time opt-in.
`USE_BOT_SUPPORT=OFF` means the implementation is not built.

Testing should cover both configurations. Bot-specific tests are also only
registered when `USE_BOT_SUPPORT=ON`.

The default build remaining unchanged is part of the acceptance criteria for
every bot patch.

---

# 1. Reference Architecture

The Go implementation is not an external bot client.

It does not simulate keyboard or mouse input.

Instead, it runs as map-side OpenNox script code and creates ordinary Nox NPCs:

```text
OpenNox map
    │
    ├── BotWars.go
    │       global startup, update, settings and commands
    │
    ├── bot.go
    │       common bot list/update interface
    │
    ├── team.go
    │       team and CTF strategy
    │
    ├── warrior.go
    │       Warrior-specific tactical AI
    │
    ├── conjurer.go
    │       Conjurer-specific tactical AI
    │
    ├── wizard.go
    │       Wizard-specific tactical AI
    │
    └── spells.go
            shared spell-casting helpers
                │
                ▼
        NoxScript / OpenNox APIs
                │
                ▼
        native Nox monster systems
```

The bots therefore consist of two layers:

```text
high-level Go decisions
        +
native monster AI
```

The high-level code decides **what the bot wants to do**.

The Nox engine handles much of **how the NPC performs it**.

---

# 2. Core Design Principle

The reference bots deliberately reuse Nox's existing NPC systems.

They do not contain a replacement pathfinder or a complete independent combat simulation.

Typical responsibilities delegated to the engine include:

- movement;
- pathfinding;
- enemy perception;
- hunting;
- basic attack execution;
- collision;
- monster action processing;
- object interaction;
- spell execution;
- animation/state handling.

The Go layer mainly adds:

- class setup;
- tactical decisions;
- cooldowns;
- reaction delays;
- target selection;
- ability selection;
- potion use;
- equipment management;
- higher-level team strategy;
- CTF decisions;
- teammate commands;
- respawning.

The C port should keep the same division.

---

# 3. Bot Lifecycle

A bot is represented by a long-lived script object associated with a native Nox NPC.

Conceptually:

```text
create bot state
    ↓
create NPC
    ↓
configure stats
    ↓
give/equip items
    ↓
configure monster AI
    ↓
register behavioural callbacks
    ↓
run updates
    ↓
NPC dies
    ↓
clean up / handle objective state
    ↓
respawn or recreate NPC
```

The native port should keep the bot state separate from the engine object so that an NPC can die and be recreated without losing the logical bot configuration.

Suggested native representation:

```c
typedef struct nox_bot {
    bool active;

    nox_object_t *unit;
    nox_object_t *target;

    bot_class_t class_id;
    bot_difficulty_t difficulty;

    int team;

    uint32_t reaction_frames;
    uint32_t next_reaction_frame;

    union {
        bot_warrior_state_t warrior;
        bot_conjurer_state_t conjurer;
        bot_wizard_state_t wizard;
    } class_state;
} nox_bot_t;
```

A fixed-size pool is preferred initially:

```c
#define NOX_MAX_BOTS 32
```

This keeps lifetime management simple and predictable.

---

# 4. Global Update Flow

The Go reference has a per-frame entry point in `BotWars.go`.

At a high level it performs:

```text
team pre-update
    ↓
bot updates
    ↓
team post-update
```

Equivalent native flow:

```c
void bot_update_all(void)
{
    bot_team_pre_update();

    for (int i = 0; i < NOX_MAX_BOTS; ++i) {
        if (!g_bots[i].active) {
            continue;
        }

        bot_update(&g_bots[i]);
    }

    bot_team_post_update();
}
```

Not every decision should happen every frame. The Go implementation also relies heavily on event callbacks and timers.

The native port should retain that characteristic.

---

# 5. Difficulty and Reaction Time

The reference scripts model difficulty primarily using artificial reaction delay rather than stat inflation.

The approximate reference values are:

| Difficulty | Reaction delay |
|---|---:|
| Hardcore | 0 frames |
| Hard | 15 frames |
| Normal | 30 frames |
| Easy | 45 frames |
| Beginner | 60 frames |

The semantic model is:

```text
bot notices event
    ↓
reaction delay
    ↓
bot performs tactical response
```

Native code should use absolute frame deadlines rather than recreating Go callback timers:

```c
bot->next_reaction_frame =
    game_frame + bot->reaction_frames;
```

This should be used for tactical reactions where the Go implementation intentionally waits before responding.

---

# 6. Timers and Cooldowns

The Go scripts use `ns.NewTimer` extensively.

The native implementation should initially replace these with frame deadlines.

For example:

```c
typedef struct bot_warrior_state {
    uint32_t warcry_ready_at;
    uint32_t charge_ready_at;
    uint32_t harpoon_ready_at;
    uint32_t eye_of_wolf_ready_at;
} bot_warrior_state_t;
```

Then:

```c
if (game_frame >= state->warcry_ready_at) {
    /* War Cry is available. */
}
```

This avoids introducing a general callback scheduler solely to imitate the Go implementation.

A generic timer system can be added later only if other native systems need it.

---

# 7. Engine Adapter

Bot code should not directly depend on raw decompiled offsets or implementation details.

Introduce a narrow native adapter:

```text
bot_engine.c
bot_engine.h
```

Example interface:

```c
void bot_engine_hunt(nox_object_t *unit);

void bot_engine_walk_to(
    nox_object_t *unit,
    float x,
    float y);

void bot_engine_attack(
    nox_object_t *unit,
    nox_object_t *target);

bool bot_engine_cast(
    nox_object_t *unit,
    int spell,
    nox_object_t *target);

bool bot_engine_can_see(
    nox_object_t *unit,
    nox_object_t *target);

bool bot_engine_same_team(
    nox_object_t *a,
    nox_object_t *b);
```

The adapter has three purposes:

1. keep bot code readable;
2. isolate decompiled engine details;
3. allow underlying engine functions to be renamed or cleaned up without rewriting bot AI.

---

# 8. Native Systems to Reuse

The current `nox-decomp` code already contains native functionality corresponding closely to several operations used by the Go scripts.

Known examples include:

```text
nox_xxx_unitHunt_5157A0
nox_xxx_monsterWalkTo_514110
nox_xxx_monsterCast_540A30
nox_xxx_monsterPushAction_50A260
nox_xxx_monsterPopAction_50A160
nox_xxx_monsterClearActionStack_50A3A0
nox_xxx_monsterIsActionScheduled_50A090
nox_xxx_monsterUpdateSeenEnemies_5286D0
nox_xxx_monsterVisionSeeEnemy_5287B0
nox_xxx_unitsHaveSameTeam_4EC520
nox_xxx_monsterMainAIFn_547210
nox_xxx_unitUpdateMonster_50A5C0
```

These names may change as decompilation improves.

Bot code should therefore reach them through the adapter layer rather than call them throughout class implementations.

---

# 9. Monster Action Stack

A major reason the native port is practical is that Nox already contains the low-level monster action system used to implement operations equivalent to the Go API.

For example, native movement and hunt behaviour are represented through existing monster actions.

The bot subsystem should therefore use existing engine operations for:

```text
Hunt
WalkTo
Attack
Follow
Guard
Flee
Cast
Look
```

wherever appropriate.

Do not create a second pathfinding or movement state machine inside the bot subsystem.

The intended model is:

```text
bot decision:
    "go there"
        ↓
existing monster action
        ↓
native movement/pathfinding
```

rather than:

```text
bot decision
    ↓
custom bot movement implementation
```

---

# 10. Events

The Go reference responds to object/monster events including:

```text
LookingForEnemy
EnemyHeard
EnemySighted
LostEnemy
EndOfWaypoint
ChangeFocus
Collision
IsHit
Retreat
Death
```

The initial native port does not need a generic event-bus abstraction.

Instead, use explicit hooks:

```c
void bot_on_enemy_sighted(
    nox_object_t *unit,
    nox_object_t *enemy);

void bot_on_enemy_lost(
    nox_object_t *unit,
    nox_object_t *enemy);

void bot_on_hit(
    nox_object_t *unit,
    nox_object_t *attacker);

void bot_on_collision(
    nox_object_t *unit,
    nox_object_t *other);

void bot_on_death(
    nox_object_t *unit);
```

These hooks should be called from suitable existing engine event paths only when the participating object belongs to a registered bot.

A reusable generic event system may be extracted later if it benefits other systems.

---

# 11. Warrior Behaviour

The Warrior should be the first complete class port.

It provides a vertical slice without requiring the large spell-decision matrices of the Conjurer and Wizard.

Reference responsibilities include:

- melee combat;
- hunting;
- weapon/equipment handling;
- health potion use;
- War Cry;
- Berserker Charge;
- Harpoon;
- Eye of the Wolf;
- Chakram use;
- reaction to invisible targets;
- reaction to hits/collisions;
- retreat/healing behaviour;
- objective behaviour;
- death and respawn.

The first Warrior milestone should intentionally be smaller:

```text
spawn Warrior
    ↓
correct team
    ↓
correct basic stats
    ↓
basic equipment
    ↓
native Hunt
    ↓
native melee combat
    ↓
death
    ↓
respawn
```

Once this works, tactical abilities can be layered on top.

### Current native Warrior subset

The opt-in implementation now ports these high-confidence Warrior decisions:

- **health potion use** checks the reference `<= 100` health threshold and then
  delegates `RedPotion` lookup/use to Nox's native player inventory and potion
  code, rather than duplicating healing or item-consumption rules;
- **potion recovery movement** follows the reference `onHit()` path when health
  remains below 100 after inventory-potion use and the current target has more
  than 10 health. The bot finds the nearest world `RedPotion` through the native
  object list, lowers native monster aggression to `0.16`, and schedules native
  `WalkTo`. In CTF, native inventory state identifies whether the bot is carrying
  a flag; a carrier only diverts when the potion is visible/interactable, while a
  non-carrier may route normally. When the recovery waypoint ends aggression is
  restored to `0.83`; non-CTF bots resume native Hunt, while CTF bots re-enter
  the native-backed attack/defend objective choice;
- **held-state escape** reproduces `Warrior.Update()` without treating every
  stun alike. Ordinary native enchant `HELD` (`5`) is converted through the
  direct NoxScript-style `SLOW` self-cast and then removed. The collision hook
  records whether Berserker Charge owned the collision before native player
  collision processing; policy only protects that source when native collision
  actually left `HELD` active. Enemy `Bomber` collisions also start the
  reference two-second protected window. After that window expires, any
  remaining `HELD` is escaped normally. The Slow spell effect and enchant
  removal both use existing native spell/buff paths; policy does not synthesize
  movement penalties or enchant duration;
- **Lost Sight TeleportWake pursuit** resolves the nearest `TeleportWake`, uses
  native `WalkTo`, and retains the lost target until the bot has moved more than
  100 units from the wake position. At that transition it uses Nox's native
  monster Fight-target action, matching the reference's one-frame polling loop
  without adding a second pathfinder or general callback scheduler;
- **CTF objective steering** reads ordinary native Flag objects. A world flag
  is discovered from the server object list; a carried flag is discovered in a
  normal player inventory and the player becomes the tactical destination.
  This preserves the reference decisions: a carrier guards the current own-flag
  position used by the reference's moving `TeamBase`, a bot with its own flag
  present attacks or escorts toward the enemy
  flag/carrier, and when both flags are carried a non-carrier pursues the own
  flag carrier. Lost Sight first walks to a dropped own flag before falling back
  to the same attack/defend decision. Native pickup, return, capture, scoring,
  flag inventory transfer, and network notifications remain untouched;
- **nearby loot acquisition** runs every 15 simulation frames, matching the Go
  `findLoot()` timer. It scans the native world-object list inside 75 units,
  applies the normal Nox visibility test, and delegates pickup to the existing
  item pickup dispatcher. The reference melee weapons, Chakrams, potions, and
  armor are collected; melee weapons and armor use the normal player equip
  paths;
- **melee weapon preference** runs every 10 seconds and selects `GreatSword`,
  otherwise `WarHammer`, otherwise an inventory `Longsword`, matching the Go
  `WeaponPreference()` ordering;
- **RoundChakram throwing** is triggered by fresh Enemy Sighted and Enemy Heard
  events when the native inventory contains `RoundChakram` and the reference
  10-second cooldown has elapsed. The policy equips the existing item, faces the
  target, interrupts the monster action stack, and enters Nox's native player
  weapon-attack path. `nox_xxx_playerInputAttack_4F9C70` starts player attack
  state `1`, while `nox_xxx_playerAttack_538960` advances the real equipped
  Chakram attack until Nox moves the item into its in-motion projectile. At that
  point policy releases its attack-state ownership and immediately reapplies the
  existing melee preference. Projectile creation, modifiers, velocity,
  collision, return, and inventory transfer remain native. The Go script uses a
  fixed five-frame timer before restoring a melee weapon; the native port waits
  for the authoritative release transition instead so it cannot unequip the
  Chakram before Nox has launched it;
- **Harpoon** is attempted immediately on Enemy Sighted and Change Focus, as in
  the Go callbacks. It faces the event target and invokes the native Warrior
  ability executor. The original normal-player Harpoon reel/pull step has been
  factored into a shared helper and is also run by the optional `4FAB20` player
  bot path, while the existing projectile updater continues to own flight, hit,
  break, range, and attachment lifetime;
- **Eye of the Wolf** runs after Looking For Enemy, after hearing an invisible
  caller, and 15 frames after Lost Sight, with the configured Bot-Script
  reaction delay added before execution;
- **War Cry** runs after Enemy Sighted or Change Focus, after the configured
  reaction delay, when the target is an interactable enemy, is not
  invulnerable, and does not have the Warrior-like 150 maximum health skipped
  by the Go reference. War Cry is also suppressed while native Harpoon or
  Berserker Charge active state is present.

Harpoon, War Cry, and Eye of the Wolf go through
`nox_xxx_playerExecuteAbil_4FBB70`, so native learned-ability checks, conflicts,
cooldowns, active-state tracking, audio, and gameplay effects remain
authoritative.

**Berserker Charge is now enabled through the native ability path.** The trace
shows that `nox_xxx_playerAttack_538960` checks active Charge before entering
normal weapon processing; that early branch owns the per-tick forward
velocity/animation step. While Charge is active, `4FAB20` also preserves the
player attack-state/timestamp established by native activation instead of
overwriting it with the current monster action; normal action-to-player-state
translation resumes as soon as Charge ends. `4FAB20` calls only the active
Charge step rather than the broad normal-player updater. Native
`nox_xxx_collidePlayer_4E8460`
continues to own Charge impact damage, wall/target stun behavior, and ability
termination, while the global Warrior ability updater owns active duration and
cooldown expiry.

The policy starts Charge after the configured reaction delay on Enemy Sighted
or Change Focus when Harpoon did not already consume the event. It also mirrors
the Go collision follow-up by waiting twice the configured reaction delay when
the collision caller is still the current target. An active Harpoon blocks
Charge while its projectile is flying, but an attached Harpoon target is
allowed so the native reel state can transition into Charge.

The native policy now also covers the two recurring/follow-up paths from the Go
reference. Once per simulated second it checks the current target inside the
reference 150-unit radius and preserves the reference priority order of
Harpoon, then reaction-delayed Charge, then reaction-delayed War Cry. The scan
uses the engine FPS value, so it remains tied to deterministic simulation ticks
rather than wall-clock time.

Per-life Warrior timers and pending events are cleared at the native respawn
owner boundary rather than relying only on `bot_warrior_update()`. During the
two-second dead wait, `nox_xxx_respawnPlayerBot_4FAC70` returns before class
policy runs; `4FAB20` now clears transient policy state in that path while
preserving the bot slot, difficulty, and persistent teammate order. This keeps
Charge/Harpoon/Chakram deadlines from leaking into the next life.

The adapter observes `player runtime +132`, the authoritative attached-Harpoon
target. A transition from no attached target to an attached target schedules the
reference Harpoon-hit-to-Charge follow-up after the configured reaction delay.
If the Warrior receives the native Is Hit event while a Harpoon is attached,
the policy calls Nox's existing Harpoon break/cleanup path (`sub_537520`) before
considering further Harpoon follow-up. Native Harpoon lifetime, pull force,
projectile cleanup, and Charge execution remain engine-owned.

---

# 12. Conjurer Behaviour

The Conjurer reference contains considerably more tactical spell logic.

Important categories include:

```text
survival
healing
mana management
mobility
defensive magic
counter-magic
crowd control
direct damage
area damage
summoning
protection
objective movement
```

Representative abilities include:

- Blink;
- Lesser Heal;
- Meteor;
- Toxic Cloud;
- Burn;
- Counterspell;
- Slow;
- stun effects;
- Vampirism;
- Pixie Swarm;
- summons;
- protection spells;
- mana interaction.

The C implementation should divide this into small deterministic decision helpers rather than reproduce one large update function.

Suggested structure:

```c
static void conjurer_update_survival(...);
static void conjurer_update_mana(...);
static void conjurer_update_defense(...);
static void conjurer_update_offense(...);
static void conjurer_update_summons(...);
```

Preserve the reference priority order unless there is a confirmed bug.

---

# 13. Wizard Behaviour

The Wizard reference contains another large tactical decision matrix.

Important areas include:

- Blink;
- Burn;
- Confuse;
- Counterspell;
- Death Ray;
- Drain Mana;
- Energy Bolt;
- Fireball;
- Force Field;
- Fumble;
- Haste;
- Inversion;
- Invisibility;
- Lesser Heal;
- Magic Missiles;
- protection spells;
- Ring of Fire;
- Shock;
- Slow;
- Trap;
- Teleport.

Representative reference behaviour includes:

```text
target held or slowed
    → consider Death Ray

incoming hostile projectile
    → consider Inversion / Counterspell

visible target
    → offensive spell priority

no immediate visible target
    → buffs / invisibility / traps / positioning
```

Again, preserve behaviour first and refactor later.

---

# 14. Spell Casting

The reference script uses the OpenNox spell API for actual spell execution.

The native port should prefer the existing monster spell path, where compatible, rather than simulate player keyboard spell entry.

Shared native helpers should handle:

```c
bool bot_spell_ready(
    const nox_bot_t *bot,
    int spell);

bool bot_cast_target(
    nox_bot_t *bot,
    int spell,
    nox_object_t *target);

void bot_spell_start_cooldown(
    nox_bot_t *bot,
    int spell,
    uint32_t cooldown_frames);
```

A centralized table may eventually replace class-specific cooldown members:

```c
uint32_t spell_ready_at[BOT_MAX_SPELLS];
```

---

# 15. Spell Phonemes

The Go reference includes a useful presentation detail: spell phonemes are played sequentially before the actual cast.

Conceptually:

```text
phoneme
    ↓ 3 frames
phoneme
    ↓ 3 frames
phoneme
    ↓
cast spell
```

This should be treated as a fidelity feature rather than an initial blocker.

The first native spell implementation may cast immediately.

Once combat behaviour is stable, add a small casting-sequence state machine if required for parity.

---

# 16. Team Layer

`team.go` implements higher-level strategy shared between bots.

Reference responsibilities include:

- team identity;
- enemy team;
- team spawn points;
- base locations;
- flag state;
- flag carrier;
- attack/defend decisions;
- escort behaviour;
- flag return;
- capture detection.

Suggested native structure:

```c
typedef struct bot_team {
    int team_id;

    nox_object_t *flag;
    nox_object_t *flag_carrier;

    float flag_home_x;
    float flag_home_y;

    bool flag_at_home;

    bot_spawn_point_t spawns[BOT_MAX_SPAWNS];
    int spawn_count;
} bot_team_t;
```

Class-specific combat logic must stay outside this module.

---

# 17. Capture the Flag

Before reproducing the Go CTF implementation directly, inspect how much native Nox CTF handling already works for monster-controlled objects.

The ideal implementation is:

```text
bot chooses enemy flag
    ↓
native WalkTo
    ↓
native flag pickup
    ↓
bot chooses own base
    ↓
native capture/scoring
```

Only missing behaviour should be implemented in the bot layer.

The Go reference manually tracks several flag operations because of the environment in which it runs. The C port should not duplicate those systems unnecessarily if native Nox already handles them correctly.

---

# 18. Commands

The Go scripts expose server/chat commands for spawning and configuring bots.

Reference examples include:

```text
server spawn red war
server spawn red con
server spawn red wiz

server spawn blue war
server spawn blue con
server spawn blue wiz

server spawn bots 3v3

server hard bots
server normal bots
server easy bots
server beginner bots
```

For the native port, prefer the existing `nox-decomp` server/console command infrastructure first.

Suggested commands:

```text
bot spawn red warrior
bot spawn red conjurer
bot spawn red wizard

bot spawn blue warrior
bot spawn blue conjurer
bot spawn blue wizard

bot spawn 3v3
bot clear

bot difficulty beginner
bot difficulty easy
bot difficulty normal
bot difficulty hard
bot difficulty hardcore
```

Player-facing chat aliases can be added after the core system is stable.

---

# 19. Teammate Commands

The reference implementation also allows human players to direct friendly bots.

Representative commands include:

```text
follow
come
escort

attack
go

guard
stay
```

Class-specific commands also exist for certain buffs and abilities.

This functionality should come after reliable class combat and team/objective behaviour.

The underlying native API should be independent of chat syntax:

```c
bot_order_follow(bot, player);
bot_order_attack(bot, target);
bot_order_guard(bot, position);
bot_order_stay(bot);
```

Chat or console commands should merely call these functions.

---

# 20. Player-Monster Bot Path

The current decompiled source contains an especially interesting routine:

```text
nox_xxx_updatePlayerMonsterBot_4FAB20
```

Initial inspection suggests that it links player state with monster-style updates.

This should be investigated before implementing:

- fake player records;
- bot scoreboard entries;
- player-style names;
- player network state;
- player appearance handling;
- player-state animation replication.

Do not assume ordinary NPCs need to be manually transformed into fake players until this path is understood.

This may represent original Nox functionality that can substantially simplify deeper bot integration.

---

# 21. Porting Order

The recommended port sequence is:

## Tier 1 — engine adapter

Add clean wrappers around existing monster/object functionality.

No class AI yet.

## Tier 2 — lifecycle

Add:

- bot pool;
- spawn;
- update;
- delete;
- difficulty;
- target state;
- respawn foundation.

## Tier 3 — basic Warrior

Implement:

- Warrior spawn;
- stats;
- team;
- basic equipment;
- native Hunt;
- melee combat;
- death;
- respawn.

This is the first playable milestone.

## Tier 4 — event integration

Connect:

- enemy sighted;
- enemy lost;
- hit;
- collision;
- death;
- retreat where needed.

## Tier 5 — full Warrior tactics

Port:

- War Cry;
- Berserker Charge;
- Harpoon;
- Eye of the Wolf;
- Chakram;
- potion logic;
- tactical delays.

## Tier 6 — Conjurer

Port the Conjurer spell and tactical state.

## Tier 7 — Wizard

Port the Wizard spell and tactical state.

## Tier 8 — team and objectives

Port shared team strategy and only the CTF behaviour not already handled by Nox.

## Tier 9 — commands and teammate control

Add server commands and friendly bot orders.

## Later fidelity work

After the main system is functional:

- casting phonemes;
- richer bot identities;
- scoreboard integration;
- additional game modes;
- exact chat compatibility;
- additional cosmetic behaviour.

---

# 22. First Playable Acceptance Test

The first major milestone should be intentionally simple.

Given:

```text
bot spawn red warrior
bot spawn blue warrior
```

the engine should create two bots which:

1. have the correct class setup;
2. belong to opposing teams;
3. perceive each other using native monster perception;
4. use native Hunt/pathfinding;
5. enter melee combat;
6. take and deal ordinary Nox damage;
7. die normally;
8. respawn correctly;
9. do not leak stale object references;
10. continue functioning after repeated deaths.

At this point the architecture is proven.

Advanced abilities are not required for this milestone.

---

# 23. Behavioural Parity Policy

The Go implementation is the reference for tactical intent.

When porting behaviour:

1. preserve existing priorities;
2. preserve reaction-delay behaviour;
3. preserve cooldown behaviour;
4. preserve class differences;
5. preserve team strategy;
6. preserve player command semantics where practical.

However, do not preserve implementation details merely because Go needed them.

For example:

```text
Go callback timer
```

may become:

```text
native frame deadline
```

and:

```text
Go wrapper around Hunt()
```

may become:

```text
direct native monster Hunt action
```

The objective is the same observed result, using the most appropriate native implementation.

---

# 24. Testing Strategy

Decision logic should be separated from engine mutation wherever practical.

For example:

```c
bot_warrior_action_t bot_warrior_choose_action(
    const bot_warrior_inputs_t *in);
```

can be tested with deterministic inputs.

Examples:

```text
low health + potion available
    → use potion

invisible enemy detected
    → Eye of the Wolf

Harpoon ready + valid distance
    → Harpoon

Charge ready + appropriate target
    → Berserker Charge

carrying enemy flag
    → prioritize capture objective
```

Engine integration tests should separately cover:

- spawn;
- team assignment;
- movement;
- Hunt;
- combat;
- casting;
- collision;
- death;
- respawn.

This makes it possible to test tactical parity without running a complete match for every decision.

---

# 25. Documentation During the Port

As behaviour is ported, this document should be updated to indicate status.

Recommended notation:

```text
[ ] not implemented
[~] partial
[x] native parity implemented
```

Current Warrior status:

```text
Warrior
[ ] non-client player creation / spawn command
[~] equipment parity (native nearby loot pickup and melee preference are implemented; exact starting-loadout parity remains native)
[x] native Hunt adapter
[x] health potion policy
[x] potion-seeking movement (native WalkTo/aggression, CTF carrier-aware diversion, and post-waypoint objective selection)
[x] Harpoon (event triggers + native mechanics + periodic scan + break-on-hit)
[~] Berserker Charge (event/collision triggers + native movement/impact + periodic scan + Harpoon-hit follow-up)
[~] War Cry (event triggers + periodic close-range scan)
[x] Eye of the Wolf event policy
[x] TeleportWake lost-target pursuit
[~] CTF objective policy (native-backed attack/defend/escort/return steering implemented; shared teammate coordination/chat orders pending)
[x] held-state escape (native direct Slow self-cast + HELD removal, with two-second Charge/Bomber protection)
[x] RoundChakram / weapon-preference / loot policy (native throw lifecycle + 10-second cooldown + nearby loot + melee preference)
```

When native behaviour intentionally differs from the Go reference, document:

1. the reference behaviour;
2. the native behaviour;
3. why the difference exists;
4. whether it is considered an improvement, engine constraint, or known gap.

---

# 26. Non-Goals for the Initial Port

The initial implementation should **not** attempt to:

- embed Go;
- implement a Go interpreter/runtime;
- emulate the full NoxScript API;
- replace Nox pathfinding;
- replace monster perception;
- replace the monster action stack;
- recreate every OpenNox scripting abstraction;
- redesign all bot tactics;
- implement every game mode simultaneously;
- make bot support part of the default build.

Those directions would greatly increase scope without improving the first playable result.

---

# 27. Definition of Success

The port is successful when native bots can participate in normal Nox matches with behaviour recognizably equivalent to the Go Bot-Script implementation while relying on native Nox systems wherever possible.

The desired end state, **when built with `-DUSE_BOT_SUPPORT=ON`**, is:

```text
native Nox object
       +
native monster AI
       +
native bot tactical layer
       +
class-specific behaviour
       +
team/objective strategy
```

with no Go runtime dependency and no external bot process.

With `USE_BOT_SUPPORT=OFF` (the default), the bot subsystem is not compiled into the executable.

The Go project remains an important reference implementation and behavioural specification even after native parity is reached.

---

## Upstream Reference

Original Bot-Script project identified by the supplied source:

```text
https://github.com/Ephreaym/Bot-Script
```

OpenNox:

```text
https://github.com/noxworld-dev/opennox
```

NoxScript:

```text
https://github.com/noxworld-dev/noxscript
```

These projects deserve credit for the implementation and infrastructure that made the reference bot system possible.
