# Runtime Interaction Refresh Adapter Report

## Scope

This pass adds a pure C++ decision-point refresh adapter for externally supplied legal actions and public board summaries.

Added:

- `FWBRuntimeInteractionRefreshOptions`
- `FWBRuntimeInteractionRefreshResult`
- `WBRuntimeInteractionRefreshAdapter`
- interaction refresh adapter automation tests
- interaction refresh adapter audit

Not added:

- gameplay rules
- legal action generation
- legality decisions
- action execution
- state ownership
- player input
- tile picking
- unit selection
- UI widgets
- camera logic
- animations
- VFX
- sound
- Blueprints
- UMG widgets
- `.uasset` edits
- `.umap` edits
- asset imports
- marker visuals
- hidden marker identity

## Adapter Purpose

`WBRuntimeInteractionRefreshAdapter` is the explicit decision-point refresh surface for future runtime/controller owners.

It accepts an externally generated legal action list and an externally supplied `FWBPublicBoardSummary`, then refreshes one or both public runtime consumers:

- `UWBRuntimeTurnInteractionModelComponent`
- `UWBRuntimeVisualControllerComponent`

The adapter does not generate legal actions, decide legality, execute actions, own game state, or inspect hidden information.

## Decision-Point Refresh Flow

Expected future flow:

1. A rules/runtime owner generates legal actions.
2. The owner builds a public board summary.
3. The owner calls `WBRuntimeInteractionRefreshAdapter::RefreshDecisionPoint`.
4. The adapter refreshes the turn interaction model with the legal actions and public summary.
5. The adapter optionally refreshes the visual controller with the same public summary.
6. Future UI reads the interaction model presentation snapshot.
7. Future UI sends a selected action id back to the interaction model/action bridge path.

## Options

`FWBRuntimeInteractionRefreshOptions` controls refresh targets:

- `bRefreshTurnInteractionModel`
- `bRefreshVisualController`

Both default to true.

If both are false, refresh fails with `no_refresh_targets_enabled`.

## Validation-First Policy

The adapter validates required targets before mutating either component.

Failure reasons:

- `turn_interaction_model_missing`
- `visual_controller_missing`
- `no_refresh_targets_enabled`

If both targets are enabled and one required component is missing, no partial refresh occurs.

## Interaction Model Refresh

When enabled, the adapter calls:

```cpp
TurnInteractionModel->SetCurrentInteractionState(PrecomputedLegalActions, PublicBoardSummary);
```

This replaces any previous legal action list, rebuilds the presentation snapshot from the same public summary, and clears any prior last execution result through existing interaction model behavior.

The adapter reports:

- `LegalActionCount`
- `PresentationEntryCount`
- `bInteractionModelRefreshed`

## Visual Controller Refresh

When enabled, the adapter calls:

```cpp
VisualController->ApplyPublicBoardSummary(PublicBoardSummary);
```

The resulting `FWBBoardViewRefreshResult` is stored in `VisualRefreshResult`.

Policy:

- `bOk` can be true when the visual controller accepts the summary.
- `bVisualControllerRefreshed` mirrors `VisualRefreshResult.bRendered`.
- If the controller exists but no board actor path is connected, the call can be accepted while `bRendered=false`.

## Stale-State Replacement

`RefreshDecisionPoint` is the preferred way to replace stale legal actions and snapshots after state changes. It does not execute any action and does not generate the next legal action list.

Future runtime ownership remains responsible for producing fresh legal actions and public summaries before calling the adapter again.

## Hidden-Information Boundary

The adapter consumes only:

- `TArray<FWBAction>`
- `FWBPublicBoardSummary`

It does not inspect deck, hand, discard, hidden marker identity, private choices, pending hidden data, or raw hidden state. Labels remain produced by `WBRuntimeLegalActionPresentation`.

## Future Usage

The adapter is intended for a later runtime decision loop:

- rules owner generates legal actions and public board summary at a decision point
- adapter refreshes the future UI model and board visuals
- future UI reads presentation snapshot entries
- future UI sends selected action id to the interaction model/action bridge

`UWBRuntimeDecisionPointCoordinatorComponent` now wraps this adapter for current interaction status. It calls the adapter with externally supplied legal actions and public summaries, retains the latest refresh result, and exposes read-only decision-point status for future UI.

This pass remains C++-only and adds no input, UI widgets, camera behavior, animation, VFX, assets, Blueprints, UMG, `.uasset`, or `.umap` work.
