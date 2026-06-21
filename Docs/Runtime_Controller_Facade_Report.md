# Runtime Controller Facade Report

## Scope

This pass adds a C++ runtime controller facade component for externally selected actions.

Added:

- `UWBRuntimeControllerFacadeComponent`
- runtime controller facade automation tests
- runtime controller facade audit

Not added:

- optional actor shell
- gameplay rules
- legal action generation
- player input
- tile picking
- unit selection
- UI
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

## Component API

`UWBRuntimeControllerFacadeComponent` stores:

- `VisualController`
- `bRefreshVisualsOnActionSuccess`
- `bRefreshVisualsOnActionFailure`
- the last `FWBRuntimeSelectedActionResult`
- the last `FWBBoardViewRefreshResult`
- whether an action has been executed

The main execution API is:

```cpp
FWBSelectedActionVisualHarnessResult ExecuteSelectedAction(
	FWBGameStateData& State,
	const FWBAction& SelectedAction,
	FWBRuntimeTurnResolutionContext& RuntimeContext);
```

The component does not own or cache `FWBGameStateData`.

## Execution Path

The facade follows this path:

1. future input/UI chooses an already-validated `FWBAction`
2. `UWBRuntimeControllerFacadeComponent::ExecuteSelectedAction`
3. `WBSelectedActionVisualHarness::ApplySelectedActionAndRefreshVisuals`
4. `WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult`
5. optional `UWBRuntimeVisualControllerComponent`
6. optional `UWBBoardViewStateApplierComponent`
7. optional `AWBBoardViewActor`

The facade does not generate actions, decide legality, or call `WBRules` legality APIs. It only coordinates execution of an action selected elsewhere.

## Refresh Policy

The facade maps its booleans to `FWBSelectedActionVisualHarnessOptions`.

Defaults:

- success refresh enabled
- failure refresh disabled

If `VisualController` is null, selected actions still execute through the harness and the visual refresh result remains the default `bRendered=false` result.

## Result Retention

The facade retains only runtime output structs:

- `FWBRuntimeSelectedActionResult`
- `FWBBoardViewRefreshResult`

This is reporting/runtime metadata, not rules ownership. `ClearLastResults` resets the execution flag and both retained result structs.

## Hidden-Information Boundary

Runtime visual refresh still consumes public board summaries only. The facade does not inspect deck, hand, discard, private choices, hidden marker identity, or raw hidden state.

Visible unit `CardId` remains public through `FWBPublicBoardSummary`. Hidden marker identity remains out of scope until marker state exists.

## Future Usage

Future input/UI should:

1. request legal actions from the rules/action generation layer
2. let the player choose one legal action
3. pass that `FWBAction` and deterministic runtime context into the facade
4. consume the retained runtime result and visual refresh result for UI/runtime reporting

`WBRuntimeActionSelectionBridge` can now sit immediately upstream of the facade. It resolves a selected `WBActionCodec` action ID from an externally supplied legal action list, then calls `UWBRuntimeControllerFacadeComponent::ExecuteSelectedAction` with the resolved `FWBAction`.

`WBRuntimeLegalActionPresentation` remains farther upstream as a display snapshot only. The facade still receives only resolved selected actions and remains downstream of action ID presentation and selection.

`UWBRuntimeTurnInteractionModelComponent` is also upstream of the facade. It stores externally supplied legal actions and presentation snapshots, then routes selected action ids through `WBRuntimeActionSelectionBridge` before the facade receives the resolved action.

The facade is intentionally C++-only in this pass.
