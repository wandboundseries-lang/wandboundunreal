# Runtime Turn Interaction Model Audit

## Action-Selection Bridge Behavior

`WBRuntimeActionSelectionBridge` accepts an externally supplied legal action list and a selected action id. It resolves the selected id by comparing against `WBActionCodec::MakeActionId` for each supplied action.

Resolution behavior:

- no match: `selected_action_id_not_found`
- duplicate matches: `selected_action_id_ambiguous`
- one match: copies the resolved `FWBAction`

`ExecuteSelectedActionId` delegates a resolved action to `UWBRuntimeControllerFacadeComponent`. A null facade fails with `runtime_controller_facade_missing` and does not mutate state. The bridge does not generate legal actions, call `WBRules`, decide legality, or own `FWBGameStateData`.

## Legal-Action Presentation Snapshot Behavior

`WBRuntimeLegalActionPresentation` consumes the same externally supplied legal action list plus `FWBPublicBoardSummary`. It preserves action order, builds stable action ids, maps action types to public labels, and fills public source/target card ids only from public unit summaries.

`TryFindEntryByActionId` succeeds only for exactly one matching presentation entry. Missing and duplicate entries return false. The snapshot builder does not sort, filter, generate actions, decide legality, or inspect hidden state.

## Runtime Controller Facade Behavior

`UWBRuntimeControllerFacadeComponent::ExecuteSelectedAction` accepts external mutable `FWBGameStateData&`, an external selected `FWBAction`, and an external runtime context. It delegates to `WBSelectedActionVisualHarness`, stores the latest runtime and visual refresh results, and exposes those retained results for future runtime/UI consumers.

The facade does not own or cache game state, generate legal actions, call `WBRules` legality APIs, inspect hidden state, or implement input/UI.

## Why Add A Turn Interaction Model

Future UI/runtime code needs a small stateful runtime model that can hold the current precomputed legal action list and its public presentation snapshot between calls. This provides a stable read surface for future UI without turning runtime visuals into rules owners.

The model also centralizes selected-action execution through the existing action-selection bridge, so the execution path remains:

1. precomputed legal action list
2. presentation snapshot for future UI
3. selected action id
4. action-selection bridge
5. runtime controller facade
6. selected-action visual harness
7. runtime adapter

## Why It Stores Externally Supplied Legal Actions

The model stores copied `FWBAction` values only after another system supplies them. This is acceptable runtime state because the list is treated as an external input, not generated locally.

The stored list may become stale after execution. Future runtime owners must call `SetCurrentInteractionState` again when rules state changes and a refreshed legal-action list/public board summary is needed.

## Why It Does Not Generate Or Validate Legal Actions

Generating or validating legal actions belongs to the rules layer. This model must not call `WBRules::GenerateLegalActions`, `QueryMove`, `CanMoveUnit`, `CanDeclareAttack`, or any other legality function.

The model trusts its supplied list and only stores it, builds presentation data from public summaries, and forwards selected action ids to the bridge.

## Why It Does Not Own Game State

`FWBGameStateData` remains external deterministic rules state. The model receives state by reference only when executing a selected action id through the bridge. It must not retain raw rules state or hidden data.

## Hidden-Information Boundary

The model stores:

- externally supplied `FWBAction` values
- `FWBPublicBoardSummary`
- `FWBRuntimeLegalActionPresentationSnapshot`
- last selection/execution result structs

It must not inspect or store deck, hand, discard, hidden marker identity, private pending choices, or raw hidden card data. Presentation card ids may come only from the public board summary.

## Intentionally Not Implemented

- input
- UI widgets
- tile selection
- unit selection
- action menus
- camera behavior
- animation
- VFX
- sound
- marker visuals
- hidden marker identity
- assets
- Blueprints
- UMG
- `.uasset` edits
- `.umap` edits
- gameplay ownership
- legal action generation
- legality decisions
- automatic legal-action refresh after execution
