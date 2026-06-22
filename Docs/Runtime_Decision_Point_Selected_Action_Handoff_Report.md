# Runtime Decision-Point Selected-Action Handoff Report

## Scope

This pass adds selected-action handoff from `UWBRuntimeDecisionPointCoordinatorComponent` to the existing turn interaction model execution path.

Added:

- selected-action reporting fields on `FWBRuntimeDecisionPointStatus`
- `UWBRuntimeDecisionPointCoordinatorComponent::ExecuteSelectedActionId`
- last selected-action execution result retention
- `HasLastSelectedActionExecution`
- `GetLastSelectedActionExecutionResult`
- `ClearLastSelectedActionExecution`
- selected-action handoff automation coverage
- selected-action handoff audit

Not added:

- gameplay rules
- legal action generation
- legality decisions
- direct EffectRunner calls
- state ownership
- automatic legal-action refresh after execution
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

## Handoff Purpose

The coordinator already exposes current decision-point status and presentation entries for future UI. This pass lets a future UI-facing caller submit a selected action id through the coordinator while preserving the existing execution path.

The coordinator does not implement execution. It delegates to `UWBRuntimeTurnInteractionModelComponent::ExecuteSelectedActionId`.

## Execution Path

The selected-action path is:

1. selected action id
2. decision-point coordinator
3. turn interaction model
4. action-selection bridge
5. runtime controller facade
6. selected-action visual harness
7. runtime adapter
8. optional visual refresh

The coordinator requires a current decision point and a configured turn interaction model. It passes through the caller-owned `FWBGameStateData&`, `FWBRuntimeTurnResolutionContext&`, and `UWBRuntimeControllerFacadeComponent*`.

## Selected-Action Status Fields

`FWBRuntimeDecisionPointStatus` now reports:

- `bHasLastSelectedAction`
- `LastSelectedActionId`
- `bLastSelectedActionResolved`
- `LastSelectedActionReason`

These are public/reporting fields only. They do not include hidden deck, hand, discard, private pending choices, hidden marker identity, or full game state.

`bLastSelectedActionResolved` reports whether the selected action id resolved through the interaction model/bridge. Runtime execution can still fail after resolution; in that case `LastSelectedActionReason` stores the runtime apply failure reason.

## Failure Behavior

Coordinator-level failures:

- `no_current_decision_point`
- `turn_interaction_model_missing`

Both return without mutating state.

Delegated failures keep existing policy:

- missing selected action id uses `selected_action_id_not_found`
- ambiguous selected action id uses `selected_action_id_ambiguous`
- null runtime controller facade uses `runtime_controller_facade_missing`
- invalid runtime context, such as full EndTurn without an MP roll source, uses the existing runtime adapter reason

All attempted selected action ids are retained in selected-action status so future UI can inspect the last attempt.

## Clear Behavior

`ClearLastSelectedActionExecution` clears only selected-action result/status fields. It does not clear the current decision point and does not clear the current presentation snapshot.

`ClearDecisionPoint` still clears the decision point, last refresh result, current status, selected-action status, and the turn interaction model state when available.

## Stale-State Policy

After selected-action execution:

- current legal actions remain unchanged
- current presentation snapshot remains unchanged
- current decision-point counts remain from the last refresh
- the coordinator records the last selected-action attempt/result

This is intentional. The coordinator does not generate legal actions and does not own rules state. A future runtime owner must call `RefreshDecisionPoint` with fresh externally generated legal actions and a fresh public summary after action resolution.

## Boundaries

The coordinator still does not:

- generate legal actions
- validate legality
- call `WBRules`
- call `WBEffectRunner`
- mutate state directly
- own or cache `FWBGameStateData`
- inspect hidden information
- implement input
- implement UI widgets
- implement camera behavior
- implement animation, VFX, or sound
- create or modify assets, Blueprints, UMG, `.uasset`, or `.umap` files

## Future Usage

Future UI can:

1. read coordinator status and presentation entries
2. let the user choose an action id
3. call coordinator `ExecuteSelectedActionId`
4. inspect selected-action status/result
5. ask the rules/runtime owner to produce fresh legal actions and a public summary
6. call `RefreshDecisionPoint` for the next decision point

This pass remains C++-only and adds no input, UI widgets, camera behavior, animation, VFX, assets, Blueprints, UMG, `.uasset`, or `.umap` work.

## Decision-Loop Harness Follow-Up

The runtime decision-loop test harness now verifies the selected-action handoff in the larger refresh/execute/explicit-refresh flow. The harness lives under `WandboundTests` only, simulates an external rules owner there, and confirms production runtime still does not generate legal actions or automatically replace stale snapshots after execution.
