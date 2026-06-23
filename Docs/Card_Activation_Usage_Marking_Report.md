# Card Activation Usage Marking Report

## Purpose

This pass completes the fixture-owned once-per-turn activation loop: source gates can reject used keys, generated activation commands can carry usage metadata, and successful command application marks usage so the same candidate is excluded until turn-start reset.

## Command Metadata

Added `FWBCardActivationUsageCommit`:

- `bMarkUsageOnSuccess`
- `PlayerId`
- `UsageKey`

`FWBCardActivationCommand` now carries `UsageCommit`. Existing fixtures without this metadata keep the previous behavior.

Usage keys are internal fixture/scaffold data. They are not serialized into replay traces, public summaries, runtime public JSON, public labels, or `WBActionCodec` action ids.

## Expansion And Propagation

`FWBCardActivationExpansionRequest` now carries `FWBCardActivationSourceGateContext`.

When the chosen `FWBCardEffectDefinition::SourceGate.bOncePerTurn` is true, `WBCardActivationExpansion::BuildActivationCommand` populates usage commit metadata using this priority:

1. `Request.SourceGateContext.ActivationUsageKey`
2. `Effect.SourceGate.OncePerTurnKey`
3. `WBCardActivationSourceGate::BuildDefaultUsageKey`

Expansion does not mutate state, mark usage, call `WBRules::GenerateLegalActions`, or call `WBEffectRunner`.

Candidate generation now passes the evaluated source-gate context into expansion. `FWBCardActivationCandidate::Command` and `FWBCardActivationLegalAction::Command` retain the usage commit by normal command copying.

## Apply Behavior

`WBRules::CanApplyCardActivationCommand` validates usage commit metadata when `bMarkUsageOnSuccess` is true:

- valid player id
- non-empty usage key
- key not already used for that player

Rules validation does not mutate usage.

`WBEffectRunner::ApplyCardActivationCommand` applies the effect request on a working copy. After `ApplyEffectRequest` succeeds, it validates the usage commit again, marks the usage key on the working state, then commits the working state to the original state.

If effect resolution fails, usage is not marked. If the usage commit is invalid, activation fails and the original state is not mutated. If the usage key is already marked, activation fails with `once_per_turn_already_used`.

## Trace Policy

Successful usage marking emits a safe trace:

```text
card_activation_usage_marked
```

The trace includes player id and ok flag only. It deliberately omits the usage key, source card id, source effect id, and debug activation id.

## Fixtures And Tests

Added golden scenarios:

- `card_activation_usage_marked_on_success.json`
- `card_activation_usage_not_marked_on_failure.json`
- `card_activation_usage_commit_invalid_fails_atomic.json`
- `card_activation_usage_already_used_fails.json`
- `card_activation_usage_candidate_excluded_after_success.json`
- `card_activation_usage_resets_on_turn_start.json`
- `card_activation_usage_not_public_trace.json`
- `card_activation_usage_preserved_through_legal_action_family.json`

Added `WBCardActivationUsageMarkingTests.cpp`, covering direct fixture application, candidate exclusion after success, turn-start reset, candidate/legal action usage commit preservation, trace hidden-key policy, rules validation read-only behavior, `WBRules::GenerateLegalActions` separation, and `WBActionCodec` separation.

## Out Of Scope

- production CardDB
- real card zones
- real costs and RL/RR payments
- response windows
- negation
- passives
- wands
- card-specific behavior
- UI target selection
- activation merged into `FWBAction`
- `WBActionCodec` activation ids
