# Card Activation Cost Payment Scaffolding Report

## Purpose

This pass adds fixture-owned RR payment scaffolding for card activation commands. It closes the current fixture loop: affordability queries compute available RL, cost gates filter unaffordable candidates, commands carry payment metadata, and successful activation execution can increase source `RLUsed` atomically.

## Command Metadata

Added `FWBCardActivationCostPaymentCommit` to `FWBCardActivationCommand`:

- `bPayCostOnSuccess`
- `PlayerId`
- `SourceUnitId`
- `RequiredRR`
- `CostKind`

The commit is scaffold metadata only. Existing commands without a payment commit keep previous behavior. It is not added to `FWBAction`, not encoded by `WBActionCodec`, and not displayed in public labels.

## Payment Helper

Added pure C++ `WBCardActivationCostPayment`:

- `CanPayCost` validates without mutating.
- `PayCost` mutates only source unit `RLUsed`.

Validation requires valid player id, nonnegative `RequiredRR`, existing source unit, ownership match, source not defeated or removed, and available RL at least the requested RR.

Available RL is computed as:

```text
max(0, RLTotal - RLUsed)
```

`RequiredRR == 0` succeeds as a no-op. Payment beyond available RL fails with `cost_not_affordable`.

## Expansion And Propagation

`WBCardActivationExpansion::BuildActivationCommand` now populates payment commit metadata when an effect's source gate has external affordability requirements or positive `RequiredRR`.

Candidate generation and activation legal action generation preserve the command by value, so payment commits survive:

- expansion to command
- command to candidate
- candidate to activation legal action

Neither generation path pays costs or mutates state.

## Apply Behavior

`WBRules::CanApplyCardActivationCommand` re-checks payment affordability when a command requests payment. Rules do not mutate state.

`WBEffectRunner::ApplyCardActivationCommand` now pays on a working copy before effect resolution, then applies the effect request and marks usage if requested. The original state is assigned only after the full activation succeeds.

Success trace order is:

```text
card_activation_resolved
card_activation_cost_paid
effect_request_resolved
child effect traces
card_activation_usage_marked
```

`card_activation_cost_paid` includes player id, source unit id, cost amount, previous/new `RLUsed`, available RL before/after, and cost kind.

## Atomicity

Payment is atomic with activation:

- payment failure prevents effect resolution.
- effect failure rolls back payment.
- usage marking happens after effect success.
- failed payment or failed effect marks no usage.
- explicit activation application can pay, but candidate generation remains read-only.

## Hidden Information

Payment traces do not serialize source card id, source effect id, debug activation id, deck, hand, discard, private choices, or usage keys.

## Out Of Scope

- production payment ownership
- Godot CardDB import
- real hand/deck/discard/equipped zones
- overflow cleanup
- wand destruction or equipped wand fallout
- response windows
- effect negation
- passives and wands
- card-specific behavior
- UI target selection
- activation merged into `FWBAction`
- `WBActionCodec` activation ids

## Validation

Build:

```text
Result: Succeeded
Total execution time: 12.65 seconds
```

Wandbound automation:

```text
succeeded=570
succeededWithWarnings=0
failed=0
notRun=0
```

An initial build surfaced a pre-existing unity-build helper-name collision between two private `MakeFailureResult` helpers. The runtime adapter helper was renamed to `MakeRuntimeResolutionFailureResult`; final build and automation passed.
