# Runtime Post-Activation Refresh Sequencing Audit

## Current Runtime Activation Execution

`WBRuntimeActivationExecutionBridge` is the current explicit runtime activation execution boundary. It accepts a resolved activation execution handoff and a mutable external `FWBGameStateData&`.

The bridge validates the handoff shape, then delegates to `WBEffectRunner::ApplyCardActivationCommand`. Cost payment, effect mutation, usage marking, trace emission, and rollback remain owned by `WandboundCore`.

The bridge does not generate legal actions, generate activation candidates, generate activation legal actions, merge activations into `FWBAction`, or modify `WBActionCodec`.

## Current Normal Decision-Point Refresh

`WBRuntimeInteractionRefreshAdapter::RefreshDecisionPoint` accepts externally supplied normal `FWBAction` legal actions and an externally supplied `FWBPublicBoardSummary`.

The adapter can refresh:

- `UWBRuntimeTurnInteractionModelComponent`
- `UWBRuntimeVisualControllerComponent`

It does not call `WBRules::GenerateLegalActions`, decide legality, inspect hidden zones, or own rules state. If required refresh targets are missing, it fails with a stable reason and does not synthesize replacements.

`UWBRuntimeDecisionPointOwnerComponent::RefreshDecisionPointFromExternalData` delegates to the coordinator and mirrors the resulting status and refresh result.

## Current Activation Presentation Refresh

`WBRuntimeActivationPresentationRefreshAdapter::RefreshActivationPresentation` accepts an externally supplied `FWBCardActivationLegalActionSet` and an externally supplied `FWBPublicBoardSummary`.

It refreshes `UWBRuntimeActivationPresentationModelComponent`, which stores:

- activation legal action set
- activation presentation snapshot
- activation target presentation snapshot
- activation presentation status

The model builds public presentation from the supplied public summary only. It does not inspect game state, hidden zones, fixture zone context, CardDB, deck, hand, discard, or equipped card storage.

## Current Owner Shell Behavior

The owner shell already supports normal selected-action execution followed by normal decision-point refresh from externally supplied data.

Activation execution currently stops at `ExecuteActivationActionId`, storing the activation execution result but leaving normal and activation presentation refresh to the caller.

This means activation can mutate the externally supplied rules state, but the runtime UI-facing presentation can remain stale until a caller explicitly refreshes it.

## Why Add Sequencing

This pass adds a small sequencing helper that performs the common activation flow:

1. execute selected activation id through the existing owner/coordinator/model/bridge/core path
2. if configured and execution succeeds, refresh normal action presentation from externally supplied post-action legal actions and public summary
3. if configured and execution succeeds, refresh activation presentation from externally supplied post-action activation actions and public summary

The sequencer gives future runtime/UI code one safe call path while preserving the rules boundary.

## External Post-Action Data Requirement

Post-action normal legal actions, activation legal actions, and public board summaries must be supplied externally because runtime is not the rules owner.

The sequencer must not compute fresh legal actions from state, build public summaries from state, or query hidden card zones. If the caller supplies stale post-action data, the refreshed presentation reflects that stale data by design.

## No Runtime Generation Boundary

The sequencer must not call:

- `WBRules::GenerateLegalActions`
- `WBCardActivationCandidateGenerator`
- `WBCardActivationLegalActionGenerator`
- `WBActionCodec`
- `WBEffectRunner` directly

Activation execution remains delegated through the existing owner activation execution path. Normal and activation presentation refresh remain delegated through the existing owner external-data refresh methods.

## Failure Refresh Policy

Refresh is not automatic on activation failure by default. A failed activation may leave state unchanged, may represent an invalid or stale selected activation id, or may fail due to payment/effect/usage validation.

Defaulting to no refresh avoids overwriting still-valid presentation with arbitrary caller-supplied fallback data. Optional refresh-on-failure flags are caller-driven for explicit recovery flows.

## Hidden-Information Boundary

The sequencer accepts only externally supplied public/runtime-safe data:

- `TArray<FWBAction>`
- `FWBCardActivationLegalActionSet`
- `FWBPublicBoardSummary`

It must not inspect deck, hand, discard, equipped zones, fixture zone context, or private marker data. It must not expose source effect ids, usage keys, debug activation ids, cost metadata, raw schema names, hook names, or hidden zone data through public labels, snapshots, traces, logs, or docs.

Downstream presentation snapshots remain responsible for public labels. Core activation traces remain responsible for trace hidden-information policy.

## Deferred Work

- production zones
- CardDB import
- UI widgets
- real target picking
- response windows
- effect negation
- passives
- wands
- overflow handling
- wand destruction or redirect
- visual animation
- VFX
