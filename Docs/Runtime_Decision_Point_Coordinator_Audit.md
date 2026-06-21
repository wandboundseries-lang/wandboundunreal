# Runtime Decision-Point Coordinator Audit

Date: 2026-06-21

## Current Interaction Refresh Adapter Behavior

`WBRuntimeInteractionRefreshAdapter` accepts a caller-supplied legal action list, a caller-supplied `FWBPublicBoardSummary`, an optional `UWBRuntimeTurnInteractionModelComponent`, an optional `UWBRuntimeVisualControllerComponent`, and refresh options.

The adapter validates enabled targets before mutation. It fails with `no_refresh_targets_enabled`, `turn_interaction_model_missing`, or `visual_controller_missing` before refreshing either component when required inputs are absent. When the interaction model is enabled, it calls `SetCurrentInteractionState`. When the visual controller is enabled, it calls `ApplyPublicBoardSummary` with the same public summary. It reports legal action count, presentation entry count, and visual refresh result.

The adapter does not generate legal actions, validate legality, execute actions, own state, or inspect hidden information.

## Current Turn Interaction Model Behavior

`UWBRuntimeTurnInteractionModelComponent` stores externally supplied legal actions and an externally supplied public board summary. It builds `FWBRuntimeLegalActionPresentationSnapshot` from those inputs and exposes the current legal action list, current presentation snapshot, presentation entry lookup, and last selected-action execution metadata.

The model can execute selected action ids through `WBRuntimeActionSelectionBridge`, but that execution path already exists and is intentionally not part of this coordinator pass.

## Current Legal-Action Presentation Snapshot Behavior

`WBRuntimeLegalActionPresentation` converts precomputed `FWBAction` values into public presentation entries. It preserves order, uses stable action ids from `WBActionCodec`, emits simple labels such as `Move`, `Attack`, `End Turn`, and `Pass`, and fills public source/target card ids only from `FWBPublicBoardSummary`.

It accepts no full game state and does not inspect hidden zones.

## Current Visual Controller Behavior

`UWBRuntimeVisualControllerComponent` consumes `FWBPublicBoardSummary`. It records that a summary was received, optionally forwards it to `UWBBoardViewStateApplierComponent`, and stores the latest `FWBBoardViewRefreshResult`.

If no applier or board actor path exists, the call remains safe and reports no rendered board. The visual controller does not generate actions, own rules state, or inspect hidden state.

## Why Add A Decision-Point Coordinator

The refresh adapter performs the refresh operation, but future UI/runtime code also needs a small read-only surface that answers questions such as:

- is there a current decision point?
- is the decision point ready?
- are there selectable actions?
- how many public units, walls, terrain tiles, legal actions, and presentation entries are available?
- what presentation entries can a future UI show?

The coordinator keeps those reporting fields together without becoming a rules owner, input handler, UI widget, or action executor.

## External Inputs Only

The coordinator consumes externally supplied legal actions and public board summaries because deterministic rules/runtime ownership remains outside runtime visual code. The coordinator must not create, validate, sort, filter, or infer legal actions. It only passes the supplied values into the refresh adapter and records public status.

## No Action Execution

This pass does not add selected-action execution to the coordinator. `UWBRuntimeTurnInteractionModelComponent`, `WBRuntimeActionSelectionBridge`, and `UWBRuntimeControllerFacadeComponent` already provide that separate execution path.

Keeping refresh/status separate from execution prevents the coordinator from becoming a gameplay owner.

## No Game State Ownership

The coordinator must not accept, store, cache, or mutate `FWBGameStateData`. It stores only:

- the latest refresh result
- a public decision-point status
- references to runtime presentation/visual components

Authoritative rules state stays with the rules/runtime owner that supplies legal actions and public summaries.

## Hidden-Information Boundary

The coordinator receives only `TArray<FWBAction>` and `FWBPublicBoardSummary`. It must not inspect deck, hand, discard, hidden marker identity, private pending choices, or raw hidden state. Future-facing labels remain those produced by `WBRuntimeLegalActionPresentation`.

## Intentionally Not Implemented

- input
- UI widgets
- tile selection
- unit selection
- action menus
- hand UI
- response UI
- camera
- animation
- VFX
- sound
- markers
- marker visuals
- hidden marker identity
- assets
- Blueprints
- UMG
- gameplay ownership
- legal action generation
- legality decisions
- action execution
