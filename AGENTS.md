# AGENTS.md

## Purpose

This file defines the rules for coding agents working in this repository.

Agents should make changes that are small, reviewable, documented, and covered by regression tests where practical.

The guiding principle is:

```text
understand the contract
    ↓
reproduce the behaviour
    ↓
fix the owning subsystem
    ↓
add/update tests
    ↓
capture durable knowledge in docs
```

## Read Before Editing

Before making changes:

1. Read this file.
2. Read `CONTRIBUTING.md`.
3. Read the documentation nearest to the subsystem being changed.
4. Inspect existing tests for that subsystem before inventing a new testing pattern.
5. Follow existing architecture, naming, ownership, and data-flow conventions.

Do not bypass established subsystem boundaries simply because a local workaround is easier.

## Scope and Ownership

Fix behaviour in the subsystem that owns it.

Avoid:

- UI code compensating for incorrect simulation state;
- render code becoming authoritative for gameplay;
- tests reimplementing production algorithms;
- hard-coded data when the repository already has an authoritative data source;
- duplicate state representing the same concept in multiple layers.

When ownership is unclear, trace the full path:

```text
authoritative data/input
    ↓
parser/loading
    ↓
runtime state
    ↓
simulation/logic
    ↓
serialization if applicable
    ↓
presentation/consumer
```

Prefer correcting the earliest incorrect owner rather than masking the symptom downstream.

## Keep Changes Focused

Make the smallest coherent change that fixes the issue correctly.

Do not mix unrelated cleanup, formatting, renaming, refactoring, or feature work into the same patch unless required for correctness.

Preserve existing public interfaces and data formats unless the task requires changing them.

## Documentation Is Part of the Change

If implementation required discovering information that a future contributor would otherwise need to rediscover, update the repository documentation.

Document durable facts such as:

- subsystem ownership;
- authoritative data sources;
- important state transitions;
- lifecycle rules;
- file/schema mappings;
- serialization or network contracts;
- diagnostic commands;
- confirmed root causes;
- non-obvious constraints;
- misleading approaches that should not be repeated;
- known limitations.

Do not turn temporary debugging notes, hypotheses, or raw logs into permanent documentation.

### Decompiled Function Documentation

When changing a decompiled function, or adding a test that exercises one,
document the function's observed role in the game: its inputs, important state
changes, callers/data flow, and externally visible result. Base the description
on surrounding code and call sites, and label uncertain reverse-engineered
behavior as such. Keep this explanation in the nearest subsystem document (or
add a focused document when none exists).

Prefer updating an existing subsystem document. If none exists, add a focused document in the appropriate `docs/` area and link it from the nearest index.

Code comments should explain local implementation constraints. Broader architecture and workflow knowledge belongs in documentation.

## Tests Are Part of the Change

A bug fix should normally include a regression test.

Prefer tests that reproduce real behaviour through production entry points:

- orders/actions;
- parser input;
- event dispatch;
- scheduler/simulation ticks;
- callbacks;
- serialization;
- public subsystem APIs.

A good regression test captures:

```text
initial state
+
real input/event
+
production code path
+
relevant lifecycle transitions
+
observable result
```

Do not merely test a helper introduced by the fix if the original bug occurred through a wider production path.

## Reproduce Before Fixing

For bugs:

1. Establish the expected contract.
2. Add or identify a test that reproduces the failure.
3. Fix the owning implementation.
4. Verify the regression.
5. Run the appropriate wider test set.

Where relevant, cover lifecycle edges such as:

- interruption;
- cancellation;
- pause/resume;
- repeated invocation;
- invalid input;
- entity removal/death;
- save/load;
- state restoration.

## Deterministic Tests

Do not use wall-clock sleeps for simulation tests.

Prefer deterministic progression:

```text
advance N ticks
assert state
```

Tests should be fast, reproducible, and independent of machine timing.

## Test Fixtures

Tests must be self-contained.

Do not depend on:

- a developer's local game/application installation;
- files in a home directory;
- ignored extraction folders;
- private assets;
- machine-specific configuration.

When production data is needed, add the smallest representative fixture to the repository's test resources.

When a fixture intentionally stands in for a real production file, prefer the real logical path/name with synthetic contents rather than inventing a special test-only filename.

## Serialization and Network Changes

Whenever persistent or network-visible state changes, add or update a round-trip test:

```text
construct state
    ↓
serialize / encode
    ↓
deserialize / decode
    ↓
assert semantic equality
```

Keep serializers, field tables, flags, versioning, and tests synchronized.

## Manual Testing

Do not use manual application/gameplay testing as the default regression strategy.

Use it only for properties automation cannot yet observe reliably, such as:

- rendering/framebuffer output;
- GPU-specific behaviour;
- OS/window/input integration;
- platform-specific integration.

When manual testing remains necessary, state exactly what automated tests do not cover.

A manually reproduced bug should become an automated regression test where practical.

## Debug Code

Temporary debug logging, probes, CVars, dumps, and instrumentation should not remain in the final patch unless they are intentionally useful diagnostics and are documented as such.

Remove temporary debugging before completion.

## Validation

Before considering a change complete:

- inspect the final diff;
- ensure unrelated changes are absent;
- run relevant tests/build checks when the workflow allows it;
- validate documentation links when docs changed;
- ensure new persisted/network state has round-trip coverage;
- ensure the documentation describes the final implementation, not an abandoned plan;
- run whitespace/diff checks appropriate to the repository.

If you were explicitly instructed not to compile or test locally, do not do so. Still update the tests that should cover the change and state what the maintainer should run.

## Completion Standard

A change is strongest when:

```text
a future agent can understand the subsystem
without repeating today's investigation

and

a future regression is caught
without manually reproducing today's bug
```
