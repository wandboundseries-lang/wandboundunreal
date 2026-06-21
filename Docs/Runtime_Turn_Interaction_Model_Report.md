# Runtime Turn Interaction Model Report

## Scope

This pass adds a C++ runtime turn interaction model component.

Added:

- `UWBRuntimeTurnInteractionModelComponent`
- current legal action list storage
- current public board summary storage
- current legal-action presentation snapshot storage
- selected action id execution through `WBRuntimeActionSelectionBridge`
- last selected action id storage
- last selection result storage
- last execution result storage
- turn interaction model automation tests
- turn interaction model audit

Not added:

- gameplay rules
- legal action generation
- legality decisions
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

## Model Purpose

`UWBRuntimeTurnInteractionModelComponent` stores externally supplied legal actions and a public presentation snapshot for future UI/runtime consumption.

It does not produce legal actions and does not decide whether actions are legal. Future rules/runtime code remains responsible for supplying a current legal action list and public board summary.

## Stored Interaction State

`SetCurrentInteractionState` copies:

- `TArray<FWBAction>`
- `FWBPublicBoardSummary`

It then builds `FWBRuntimeLegalActionPresentationSnapshot` through `WBRuntimeLegalActionPresentation::BuildSnapshot`.

`ClearCurrentInteractionState` clears the action list, public summary, presentation snapshot, state flag, and last execution metadata.

## Presentation Snapshot Access

The component exposes const access to:

- the current legal action list
- the current presentation snapshot

`TryFindPresentationEntryByActionId` delegates to `WBRuntimeLegalActionPresentation::TryFindEntryByActionId`.

## Selected Action Execution

`ExecuteSelectedActionId` requires current interaction state.

If no current state exists:

- selection fails with `no_current_interaction_state`
- state is not mutated
- no MP roll is consumed

If current state exists, the component delegates to `WBRuntimeActionSelectionBridge::ExecuteSelectedActionId`, passing the stored legal actions and selected action id.

The component stores the selected id, selection result, and execution result for future runtime/UI consumers.

## Stale-State Policy

After execution, the model does not regenerate legal actions and does not rebuild the presentation snapshot. `bHasCurrentInteractionState` remains true, but the stored action list and snapshot may be stale after the rules state mutates.

Future runtime owners must explicitly call `SetCurrentInteractionState` again after action resolution when refreshed legal actions and presentation data are needed.

## Hidden-Information Boundary

The model stores only externally supplied actions, public board summary data, public presentation entries, and runtime result metadata. It does not inspect deck, hand, discard, hidden marker identity, private choices, or raw hidden state.

Visible card ids in presentation entries come only from `FWBPublicBoardSummary`.

## Future Usage

Future flow:

1. rules/runtime owner generates legal actions
2. runtime owner builds a public board summary
3. turn interaction model stores the list and presentation snapshot
4. future UI reads snapshot entries
5. future UI chooses an action id
6. model executes the selected id through the action-selection bridge/facade path

This remains C++-only and adds no input, UI widgets, camera behavior, animation, VFX, assets, Blueprints, UMG, `.uasset`, or `.umap` work.
