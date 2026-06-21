# Runtime Action Selection Bridge Report

## Scope

This pass adds a pure C++ runtime action-selection bridge for precomputed legal actions.

Added:

- `WBRuntimeActionSelectionBridge`
- `FWBResolvedRuntimeActionSelection`
- `FWBRuntimeActionSelectionExecutionResult`
- action-selection bridge automation tests
- action-selection bridge audit

Not added:

- gameplay rules
- legal action generation
- legality decisions
- state ownership
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

## Bridge Purpose

`WBRuntimeActionSelectionBridge` converts a selected action ID into an `FWBAction` by scanning an externally supplied legal action list. The bridge then delegates successful selections to `UWBRuntimeControllerFacadeComponent`.

The bridge does not know how legal actions were produced. It does not call `WBRules`, does not generate actions, and does not validate legality.

## Inputs

The bridge accepts:

- `const TArray<FWBAction>& PrecomputedLegalActions`
- `const FString& SelectedActionId`
- external mutable `FWBGameStateData&` only for successful execution delegation
- external `FWBRuntimeTurnResolutionContext&`
- non-null `UWBRuntimeControllerFacadeComponent*`

Selected action IDs are exactly `WBActionCodec::MakeActionId` values.

## Resolution Behavior

`ResolveSelectedActionId` scans only the provided action list.

Results:

- one match: `bOk=true` and `ResolvedAction` is copied
- no matches: `bOk=false`, `Reason=selected_action_id_not_found`
- multiple matches: `bOk=false`, `Reason=selected_action_id_ambiguous`

Duplicate IDs fail rather than choosing arbitrarily.

## Null Facade Policy

`ExecuteSelectedActionId` requires a non-null `UWBRuntimeControllerFacadeComponent`.

If the facade is null:

- `bOk=false`
- `Reason=runtime_controller_facade_missing`
- state is not mutated
- no execution occurs

Low-level execution without this bridge remains available through existing runtime adapter and harness paths.

## Execution Path

The runtime action-selection path is:

1. selected action ID
2. precomputed legal action list
3. `WBRuntimeActionSelectionBridge::ResolveSelectedActionId`
4. resolved `FWBAction`
5. `UWBRuntimeControllerFacadeComponent::ExecuteSelectedAction`
6. `WBSelectedActionVisualHarness`
7. `WBRuntimeTurnResolutionAdapter`
8. optional visual controller/state applier/board view actor

## Hidden-Information Boundary

The bridge compares action ID strings and copies an action from the supplied list. It does not inspect deck, hand, discard, private choices, hidden marker identity, or raw hidden state. Failure reasons are stable generic strings and do not contain legal action list contents.

## Future Usage

Future UI/input code can present action IDs from a legal action list produced elsewhere. When the user chooses one ID, runtime code can pass the same precomputed list and chosen ID to this bridge. The bridge resolves and executes through the facade without adding UI, input, camera, VFX, assets, or rules ownership.

`WBRuntimeLegalActionPresentation` can now sit upstream of this bridge. It consumes the same externally supplied legal action list plus a public board summary, then emits public labels and action IDs for future UI display before the chosen ID is passed back to this bridge.

`UWBRuntimeTurnInteractionModelComponent` now calls this bridge when executing a selected action id from its stored interaction state. The bridge remains the only selected-id-to-action resolver in the runtime path.
