# Summoning limits

`sub_500D70()` (`nox_xxx_checkSummonedCreaturesLimit_500D70`) owns the
summoned-creature count check. It combines the owner's active summon count from
`sub_500D10(owner)` with the pending count for the requested creature type from
`sub_427460(summon_type)`. The result is true when the combined count is at
most four.

`sub_500DA0()` (`nox_xxx_summonStart_500DA0`) calls this check before resolving
the spawn position and creating the summoned object. When the check rejects the
request, the summon-start path sends the configured limit message and returns
without creating the creature. The `736e9f2` fix restored this check in the
production summon-start path; the earlier commented-out block could allow
unlimited summons.

`tests/summon_limit_test.c` links the production `sub_500D70()` and
`sub_500D10()` implementations with an empty active-creature list and supplies
deterministic pending-count fixtures. It verifies the inclusive limit and the
rejection of a fifth pending creature; the production count helper still owns
the active-creature contribution. Full spawn placement, messaging, and
creature creation remain game integration coverage.

## Summon start and completion

`sub_500DA0()` (`nox_xxx_summonStart_500DA0`) owns the pending summon action.
It validates the source/owner state, obtains a spawn position through
`sub_500F40()`, stores the summon type, position, owner byte, sequence ID, and
completion tick in the action record, and emits the start effect. The caller
then invokes `sub_5010D0()` on later simulation updates. At the recorded tick,
that function creates the summoned object through `sub_5016C0()`, places it at
the stored coordinates, marks it as summoned, and links it to the owner.

The `a1b2f49` fix corrected the ABI boundary for the output position passed to
`sub_500F40()`. The regression in `tests/summon_behavior_test.c` drives both
production entry points with a self-contained fixture and checks the stored
coordinates, object creation, and summoned flag. The action stores the two
position floats beginning at offset 74, so the test reads them with `memcpy`
to remain alignment-safe on ARMHF. The Windows build supplies a deterministic
position collaborator and a flag-safe fixture because the native PE runtime
uses the same pointer field for a bitmask check. Collision rejection,
interruption, and the full world/object database remain untested.
