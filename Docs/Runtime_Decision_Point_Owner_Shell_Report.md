# Runtime Decision-Point Owner Shell Report

Date: 2026-06-21

## Scope

This pass adds `UWBRuntimeDecisionPointOwnerComponent`, a production C++ owner shell for runtime decision-point coordination.

The owner shell coordinates externally supplied decision data. It is not the rules owner.

## External-Data Flow

The expected refresh flow is:

1. a future rules/runtime owner generates legal actions
2. that owner builds a public board summary
3. the owner shell receives both values
4. the owner shell calls `UWBRuntimeDecisionPointCoordinatorComponent::RefreshDecisionPoint`
5. the coordinator refreshes the turn interaction model and optional visual controller
6. future UI reads coordinator/model presentation state

The owner shell does not create, validate, sort, filter, or infer legal actions.

## Selected-Action Flow

Selected action execution flows through existing runtime layers:

1. selected action id
2. owner shell
3. decision-point coordinator
4. turn interaction model
5. action-selection bridge
6. runtime controller facade
7. selected-action visual harness
8. runtime turn-resolution adapter

The owner shell delegates selected action ids only through the coordinator.

## Post-Action Refresh Flow

`ExecuteSelectedActionIdAndRefreshFromExternalData` executes the selected action id, then refreshes the coordinator only if execution succeeds.

The post-action refresh uses only caller-supplied legal actions and caller-supplied public board summary. If execution fails, no post-action refresh is attempted.

If execution succeeds but refresh fails, the execution result remains stored and the refresh failure becomes the latest refresh result/status.

## Null And Failure Policy

- Missing coordinator refresh fails with `decision_point_coordinator_missing`.
- Missing coordinator execution fails with `decision_point_coordinator_missing`.
- Missing runtime controller facade execution fails with `runtime_controller_facade_missing`.
- Missing current decision point execution fails with `no_current_decision_point`.
- Execution failure stores the last execution result and does not auto-refresh post-action data.

## Stale-State Policy

After `ExecuteSelectedActionId`, the current decision point is considered stale. It is not cleared, regenerated, or replaced automatically.

The caller should call `RefreshDecisionPointFromExternalData`, or use `ExecuteSelectedActionIdAndRefreshFromExternalData` with updated externally supplied data.

## Boundaries

The owner shell does not:

- generate legal actions
- decide legality
- call `WBRules`
- call `WBEffectRunner`
- own or cache `FWBGameStateData`
- inspect deck, hand, discard, private pending choices, or hidden marker identity
- implement input
- implement UI widgets
- implement camera behavior
- implement animation, VFX, or sound
- create or modify assets, Blueprints, UMG, `.uasset`, or `.umap` files

## Future Usage

Future runtime wiring can use this component as the shell around decision points:

1. rules owner generates legal actions/public summary
2. owner shell refreshes the coordinator
3. UI reads presentation entries
4. UI sends a selected action id to the owner shell
5. owner shell delegates execution through the coordinator
6. rules owner supplies post-action legal actions/public summary
7. owner shell refreshes the next decision point
