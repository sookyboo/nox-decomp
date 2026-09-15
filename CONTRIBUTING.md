# CONTRIBUTING.md

## Overview

Contributions should preserve the repository's architecture, keep changes focused, add regression coverage for changed behaviour, and document durable discoveries.

Before contributing, read:

- `AGENTS.md`;
- this file;
- the documentation nearest to the subsystem you are changing;
- existing tests covering that subsystem.

## Development Workflow

Use this workflow for feature work and bug fixes:

```text
understand expected behaviour
    ↓
identify the owning subsystem
    ↓
find or add a reproducer
    ↓
make the smallest correct change
    ↓
add/update tests
    ↓
update documentation
    ↓
run relevant validation
    ↓
review the final diff
```

### 1. Understand the Contract

Before editing code, determine:

- what behaviour is expected;
- which subsystem owns that behaviour;
- what data is authoritative;
- which public interfaces or persisted formats are involved;
- which existing tests and docs describe the area.

Do not start by patching the visible symptom if the incorrect state originates elsewhere.

### 2. Keep Changes Focused

Each contribution should address one coherent problem.

Avoid combining unrelated:

- cleanup;
- formatting;
- renaming;
- refactoring;
- dependency upgrades;
- features.

Small, focused diffs are easier to review, test, revert, and rebase.

### 3. Preserve Architectural Boundaries

Prefer one authoritative owner for each piece of state.

For cross-layer features, reason about the complete flow:

```text
data/input
    ↓
loading/parsing
    ↓
runtime state
    ↓
simulation
    ↓
serialization/networking
    ↓
client/presentation
```

A downstream layer should not silently compensate for incorrect upstream state unless that is explicitly its responsibility.

## Testing

### Regression Tests

Bug fixes should normally include a regression test.

The regression should reproduce the behaviour through the same production path that failed whenever practical.

Prefer:

```text
initial state
+
real action/input
+
production processing
+
state transition
+
observable result
```

over unit-testing only a helper introduced by the fix.

### Production Entry Points

Tests should drive real subsystem entry points where practical, including:

- orders/actions;
- parsers;
- scheduler ticks;
- event dispatch;
- callbacks;
- serialization APIs;
- public gameplay/application APIs.

Do not duplicate the production algorithm inside the test.

### Lifecycle Coverage

When a bug involves stateful behaviour, cover the relevant transitions.

Examples:

- start → complete;
- start → interrupt;
- pause → resume;
- apply → remove;
- create → destroy;
- save → load;
- encode → decode;
- valid → invalid;
- repeated invocation;
- restoration of prior state.

A success-path-only test is insufficient when the bug is specifically about interruption, cleanup, or restoration.

### Determinism

Tests must not rely on wall-clock sleeps.

For time-based systems, advance simulation time/ticks explicitly and assert state at deterministic points.

### Fixtures

Tests must be runnable from a clean checkout with documented dependencies.

Do not require:

- private/local application data;
- a developer's installed assets;
- ignored extraction directories;
- home-directory files;
- machine-specific paths.

Add minimal synthetic fixtures to the repository when production-shaped input is required.

If a fixture represents a real production file, keep its logical path and filename where practical and customize only its contents.

### Serialization and Persistence

Any change to networked or persisted state should include a round-trip test.

Example:

```text
construct
→ serialize
→ deserialize
→ compare semantics
```

This applies to:

- save data;
- network entity state;
- flags/packed values;
- IDs/references;
- callback/function identity;
- versioned file formats.

Keep serialization tables and tests synchronized with structure changes.

### Multiple Formats or Versions

If the project supports multiple file/schema versions, use fixtures that actually exercise their differences.

Do not claim compatibility merely by running the same modern fixture under differently named tests.

### Testing Layer Boundaries

For multi-layer features, separate tests can be preferable to one large end-to-end test.

For example:

```text
Test A: simulation produces correct state
Test B: serialization preserves that state
Test C: presentation consumes it correctly
```

This makes failures easier to diagnose.

## Manual Verification

Manual runtime testing is useful for behaviour that automated tests cannot reasonably observe, such as:

- final rendering appearance;
- GPU behaviour;
- platform window/input integration;
- other OS-specific integration.

It should not replace a logic regression test.

If manual verification is required, document what property still needs to be checked manually.

## Documentation

Documentation changes are expected when a contribution establishes new durable knowledge.

Document:

- subsystem ownership;
- authoritative data and lookup paths;
- architecture and data flow;
- lifecycle/state transitions;
- schemas and important fields;
- serialization/network contracts;
- diagnostic commands;
- confirmed root causes;
- known limitations;
- important pitfalls.

Do not document:

- unverified hypotheses as facts;
- raw debug logs;
- temporary implementation plans that no longer match the code;
- information already obvious from a single local line of code.

### Decompiled Functions

Whenever a decompiled function is modified or covered by a new test, include
documentation describing what the function does in the game. Record its
inputs, key state transitions, callers/data flow, and observable result using
the surrounding implementation as evidence. Clearly qualify behavior that is
still uncertain from reverse engineering, and place the explanation in the
nearest subsystem document.

### Where Documentation Goes

Prefer this order:

1. Update an existing subsystem document.
2. Otherwise create a focused document under `docs/`.
3. Link new documents from the nearest useful index/README.
4. Keep `AGENTS.md` and `CONTRIBUTING.md` focused on stable repository-wide rules.

Use code comments for local constraints. Use docs for broader behaviour and architecture.

### Recommended Documentation Shape

A useful subsystem document often contains:

```markdown
# Feature / Subsystem

## Contract
Expected externally observable behaviour.

## Ownership
Which subsystem is authoritative.

## Data Flow
Source -> parser -> runtime -> consumer.

## Lifecycle
Important state transitions.

## Data / Schema
Fields, IDs, flags, defaults and mappings.

## Diagnostics
Useful bounded commands and evidence.

## Known Pitfalls
Incorrect assumptions and recurring failure modes.

## Verification
Relevant automated tests and remaining manual-only checks.
```

Only include sections that provide useful information.

## Debugging and Instrumentation

Temporary instrumentation is welcome while investigating, but remove it before submitting unless it is deliberately becoming a supported diagnostic facility.

This includes temporary:

- logs;
- counters;
- probes;
- dumps;
- debug commands;
- debug configuration variables.

Supported diagnostics should be bounded, useful beyond the immediate bug, and documented.

## Validation Before Submission

Run the checks appropriate to the files you changed.

For code changes, this normally means:

- build affected targets;
- run focused tests;
- run the wider relevant test suite;
- inspect warnings/errors;
- check the final diff.

For documentation-only changes:

- review formatting;
- verify relative links;
- run repository whitespace/diff checks;
- ensure examples and paths match the repository.

If your environment cannot run a required check, state that clearly rather than claiming it passed.

If a maintainer explicitly asks you not to compile or run tests, follow that instruction; still add/update the appropriate tests.

## Final Review Checklist

Before submitting:

- [ ] The change fixes the owning subsystem rather than masking a symptom.
- [ ] The diff contains no unrelated changes.
- [ ] Existing conventions and architecture are preserved.
- [ ] Changed behaviour has regression coverage where practical.
- [ ] Tests use real production paths instead of duplicated logic.
- [ ] Fixtures are repository-owned and self-contained.
- [ ] Time-based tests are deterministic.
- [ ] Persisted/network state has round-trip coverage.
- [ ] Temporary debugging has been removed.
- [ ] Durable discoveries have been documented.
- [ ] New docs are linked from the appropriate index.
- [ ] Documentation matches the final implementation.
- [ ] Relevant validation has been run, or any unrun checks are explicitly identified.

## Definition of Done

A contribution is complete when both of these are true:

```text
Future contributors can understand the important behaviour
without repeating the investigation.

Future regressions are caught automatically
where the behaviour can reasonably be tested.
```
