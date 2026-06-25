# Runtime Activation Presentation Handoff Report

## Purpose

This pass adds a runtime handoff/storage layer for externally supplied activation legal actions and their public presentation snapshots.

Future UI/runtime code can now store and read activation legal action presentation entries and activation target presentation entries without merging activation into normal `FWBAction` presentation.

## Activation Presentation Model

Added `UWBRuntimeActivationPresentationModelComponent`.

It stores:

- externally supplied `FWBCardActivationLegalActionSet`
- `FWBCardActivationLegalActionPresentationSnapshot`
- `FWBCardActivationTargetPresentationSnapshot`
- `FWBRuntimeActivationPresentationStatus`

`SetCurrentActivationPresentationState` copies the supplied activation action set and builds both snapshots from the supplied `FWBPublicBoardSummary`.

`ClearCurrentActivationPresentationState` clears the action set, snapshots, and status.

Lookup helpers delegate to the existing core presentation helpers and fail when action ids are missing or duplicated.

## Target Presentation Snapshot Storage

The runtime model stores the activation target presentation snapshot produced by `WBCardActivationTargetPresentation`.

This preserves deterministic order from the externally supplied activation legal action set and exposes target kind, public activation label, public target label, public source CardId, and public unit-target CardId.

## Refresh Adapter

Added `WBRuntimeActivationPresentationRefreshAdapter`.

`RefreshActivationPresentation` validates that an activation presentation model exists, then calls `SetCurrentActivationPresentationState` and returns public counts:

- activation action count
- activation presentation entry count
- target presentation entry count

If the model is missing, it fails with `activation_presentation_model_missing`.

## Coordinator Integration

`UWBRuntimeDecisionPointCoordinatorComponent` now has an optional activation presentation model.

Added:

- `SetActivationPresentationModel`
- `GetActivationPresentationModel`
- `RefreshActivationPresentationFromExternalData`
- `ClearActivationPresentation`

Activation refresh is separate from normal `RefreshDecisionPoint`. Missing activation presentation storage does not break normal decision-point refresh.

Normal `ClearDecisionPoint` clears only normal `FWBAction` decision-point state. Activation presentation is cleared through `ClearActivationPresentation`.

## Owner Shell Integration

`UWBRuntimeDecisionPointOwnerComponent` now delegates activation presentation refresh to the coordinator through:

- `RefreshActivationPresentationFromExternalData`
- `ClearActivationPresentation`

The owner mirrors the coordinator's public status counts after successful activation presentation refresh. It does not store hidden state, execute activation actions, generate candidates, or mutate rules state.

## Stale-State Policy

Activation presentation state is explicitly refreshed by the external caller.

It is not automatically regenerated after normal selected-action execution.

It is not automatically cleared when normal `FWBAction` decision-point refresh or clear runs.

The future runtime owner must refresh activation presentation with a fresh externally generated activation legal action set and public summary, or explicitly clear it.

## Hidden Metadata Exclusions

Runtime activation presentation snapshots expose public labels and CardIds only through existing core presentation snapshots.

Tests confirm public snapshot text excludes hidden/source metadata such as source effect ids, usage keys, debug activation ids, and cost metadata.

## Separation From Normal Action Presentation

Activation legal actions remain separate from:

- `FWBAction`
- `EWBActionType`
- `WBRuntimeLegalActionPresentation`
- `WBActionCodec`
- `WBRules::GenerateLegalActions`

Normal movement, attack, end-turn, pass, and pass-response presentation remains unchanged.

## External-Data Driven Boundary

Runtime activation presentation consumes externally supplied activation legal actions and public board summaries. It does not generate activation candidates, filter targets, decide legality, query source gates, inspect `FWBGameStateData`, or look at hidden card zones.

## Non-Execution Boundary

This pass does not execute activations.

Activation execution remains outside this runtime presentation layer and still requires explicit effect-runner paths in other test-only activation replay scaffolding.

## Deferred Target Picking

This pass stores target presentation entries for already-generated activation legal actions. It does not implement real UI target picking, target discovery, target retargeting, widgets, or response windows.

## Fixtures

No new runtime-facing GodotCanon fixtures were added in this pass.

The existing fixture utilities exercise data-level operations. This pass adds transient Unreal runtime components, so the behavior is covered by C++ automation tests instead of JSON fixture operations.

## Out Of Scope

- production zones
- CardDB import
- activation `FWBAction`
- activation codec ids
- response windows
- effect negation
- UI widgets
- passives
- wands
