# Runtime Interaction Refresh Adapter Audit

Date: 2026-06-20

## Current Turn Interaction Model Behavior

`UWBRuntimeTurnInteractionModelComponent` stores a caller-supplied legal action list and a caller-supplied `FWBPublicBoardSummary`. `SetCurrentInteractionState` copies both inputs, rebuilds a `FWBRuntimeLegalActionPresentationSnapshot` through `WBRuntimeLegalActionPresentation::BuildSnapshot`, marks current interaction state as present, and clears the last execution result.

The model can find presentation entries by action id and can execute a selected action id through `WBRuntimeActionSelectionBridge` and `UWBRuntimeControllerFacadeComponent`. It does not generate legal actions and does not decide whether an action should be legal. Existing stale-state behavior is explicit: after execution, the model keeps the previous legal action list and snapshot until an external owner replaces them.

## Current Legal-Action Presentation Snapshot Behavior

`WBRuntimeLegalActionPresentation` builds display-oriented entries from precomputed `FWBAction` values and a public board summary. It preserves input order, derives stable action ids through `WBActionCodec::MakeActionId`, assigns public labels such as `Move`, `Attack`, `End Turn`, and `Pass`, and fills visible source/target public card ids only when those public units exist in the supplied summary.

The presentation layer accepts no `FWBGameStateData`, does not inspect hidden player zones, and does not call rules or effect code.

## Current Visual Controller Behavior

`UWBRuntimeVisualControllerComponent` accepts `FWBPublicBoardSummary` through `ApplyPublicBoardSummary`. It records that a summary was received, forwards the summary to `UWBBoardViewStateApplierComponent` when one is configured, and stores the resulting `FWBBoardViewRefreshResult`.

If no board applier or board actor is present, the call is safe and returns a default non-rendered refresh result. When a board actor path exists, the summary is rendered through the board summary bridge and the result reports rendered tile, unit, wall, and terrain counts.

## Why Add A Refresh Adapter

The interaction model intentionally preserves stale legal actions after execution, and the visual controller only knows how to consume public board summaries. Future runtime/controller ownership needs one explicit decision-point operation that replaces the stale model state and optionally pushes the same public summary into visuals.

The refresh adapter is that narrow operation. It coordinates existing public-data consumers without becoming a rules owner, UI owner, input handler, or action executor.

## External Inputs Only

The adapter accepts externally supplied legal actions because rules generation belongs to the deterministic rules/runtime owner, not to visual runtime code. It accepts an externally supplied `FWBPublicBoardSummary` because runtime visual code must consume public summaries rather than full hidden game state.

The adapter does not generate or validate the legal action list. If the list is wrong, that is a rules-owner problem. The adapter only refreshes presentation state from the supplied list and public summary.

## No Game State Ownership

The adapter must not accept, store, cache, or mutate `FWBGameStateData`. It has no reason to hold authoritative state: the interaction model stores action/presentation state for future UI, and the visual controller displays public summaries.

## Hidden-Information Boundary

The adapter receives only `FWBAction` values and `FWBPublicBoardSummary`. It must not inspect deck, hand, discard, private choices, hidden marker identity, or any hidden game-state fields. Labels remain those produced by `WBRuntimeLegalActionPresentation`, and visual refresh remains summary-driven.

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
