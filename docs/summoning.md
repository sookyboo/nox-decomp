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
