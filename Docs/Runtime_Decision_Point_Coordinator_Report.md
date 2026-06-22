# Runtime Decision-Point Coordinator Report

## Scope

This pass adds a C++ runtime decision-point coordinator component for externally supplied legal actions and public board summaries.

Added:

- `FWBRuntimeDecisionPointStatus`
- `UWBRuntimeDecisionPointCoordinatorComponent`
- decision-point coordinator automation tests
- decision-point coordinator audit
- selected-action handoff to the turn interaction model

Not added:

- gameplay rules
- legal action generation
- legality decisions
- direct action execution
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

## Coordinator Purpose

`UWBRuntimeDecisionPointCoordinatorComponent` is a read-only runtime coordination component for future UI-facing decision points. It accepts externally generated legal actions and an externally supplied public board summary, calls `WBRuntimeInteractionRefreshAdapter`, and stores a compact public status describing the current decision point.

It does not generate legal actions, decide legality, own state, or inspect hidden information. Selected-action execution is delegated to `UWBRuntimeTurnInteractionModelComponent`; the coordinator does not implement execution itself.

## Refresh Flow

Expected future flow:

1. A rules/runtime owner generates legal actions.
2. The same owner builds a public board summary.
3. The owner calls `UWBRuntimeDecisionPointCoordinatorComponent::RefreshDecisionPoint`.
4. The coordinator builds `FWBRuntimeInteractionRefreshOptions` from its refresh booleans.
5. The coordinator calls `WBRuntimeInteractionRefreshAdapter::RefreshDecisionPoint`.
6. The adapter refreshes the turn interaction model and optionally the visual controller.
7. The coordinator stores the last refresh result and public decision-point status.
8. Future UI reads status and presentation snapshot entries.
9. Future UI sends a selected action id to the turn interaction model or action-selection bridge path.

`ExecuteSelectedActionId` now performs that future-facing handoff by requiring a current decision point, requiring a configured turn interaction model, and delegating to `UWBRuntimeTurnInteractionModelComponent::ExecuteSelectedActionId`.

## Current Status Fields

`FWBRuntimeDecisionPointStatus` reports only public decision-point facts:

- `bReady`
- `bHasActions`
- `LegalActionCount`
- `PresentationEntryCount`
- `PublicUnitCount`
- `PublicWallCount`
- `PublicTerrainCount`
- `LastRefreshReason`
- `bHasLastSelectedAction`
- `LastSelectedActionId`
- `bLastSelectedActionResolved`
- `LastSelectedActionReason`

It does not include deck, hand, discard, private pending choices, hidden marker identity, or full `FWBGameStateData`.

Selected-action fields report the last attempted action id and result reason only. They do not refresh or regenerate the current legal action list.

## Missing Component Policy

The coordinator follows the refresh adapter's validation-first policy.

If turn interaction model refresh is enabled, the model is required. If visual controller refresh is enabled, the visual controller is required. If both refresh booleans are false, refresh fails with `no_refresh_targets_enabled`.

On failed refresh, the coordinator resets current status to non-ready and clears `bHasCurrentDecisionPoint` so future UI does not read stale decision-point entries. The failure reason is retained in `LastRefreshReason`.

## Clear Behavior

`ClearDecisionPoint` clears:

- current decision-point flag
- current status
- last refresh result
- current turn interaction model state, when a model is assigned

After clear, `GetCurrentPresentationSnapshot` returns `nullptr` and presentation entry lookup fails.

`ClearLastSelectedActionExecution` clears only the selected-action handoff result and selected-action status fields. It does not clear the current decision point or presentation snapshot.

## Selected-Action Stale-State Policy

After selected-action handoff, the coordinator does not regenerate legal actions and does not rebuild the presentation snapshot. Current decision-point counts remain from the last refresh, and the caller must provide fresh legal actions and a public summary through `RefreshDecisionPoint`.

## Hidden-Information Boundary

The coordinator consumes only:

- `TArray<FWBAction>`
- `FWBPublicBoardSummary`

Presentation labels and public card ids come from `WBRuntimeLegalActionPresentation` through the turn interaction model. Hidden deck, hand, discard, private pending choices, and hidden marker identity remain outside this layer.

## Explicit Non-Goals

The coordinator does not:

- generate legal actions
- validate legality
- implement selected-action execution directly
- call `WBRules`
- call `WBEffectRunner`
- store or cache `FWBGameStateData`
- implement input
- implement UI widgets
- implement camera behavior
- implement animation, VFX, or sound
- create or modify assets, Blueprints, UMG, `.uasset`, or `.umap` files

## Future Usage

Future runtime decision flow can use the coordinator as the read-only bridge between rules output and UI presentation:

- rules owner supplies legal actions and public summary at a decision point
- coordinator refreshes UI-facing status and snapshot
- future UI checks `bReady` and `bHasActions`
- future UI displays presentation entries
- future UI submits the selected action id through coordinator `ExecuteSelectedActionId`
- rules/runtime owner refreshes the coordinator after action resolution with fresh legal actions and a public summary

This remains C++-only and adds no input, UI widgets, camera behavior, animation, VFX, assets, Blueprints, UMG, `.uasset`, or `.umap` work.
