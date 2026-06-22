# Runtime Decision-Point Selected-Action Handoff Audit

Date: 2026-06-21

## Current Decision-Point Coordinator Behavior

`UWBRuntimeDecisionPointCoordinatorComponent` accepts externally supplied legal actions and a public board summary, builds refresh options from its component booleans, and calls `WBRuntimeInteractionRefreshAdapter::RefreshDecisionPoint`.

On successful refresh it stores the last refresh result, marks a current decision point as available, and exposes public status counts plus presentation snapshot access through the configured `UWBRuntimeTurnInteractionModelComponent`. On failed refresh it resets current status and clears the current decision-point flag so future UI does not read stale entries.

The coordinator currently does not execute selected actions.

## Current Turn Interaction Model Execution Behavior

`UWBRuntimeTurnInteractionModelComponent::ExecuteSelectedActionId` requires current interaction state. It stores the selected action id, resolves the id through `WBRuntimeActionSelectionBridge`, and stores the returned selection and execution result.

If no current interaction state exists, it returns `no_current_interaction_state` without mutating state. Otherwise it delegates resolution and execution to the action-selection bridge.

## Current Action-Selection Bridge Behavior

`WBRuntimeActionSelectionBridge` resolves selected action ids from an externally supplied legal action list using `WBActionCodec::MakeActionId`.

It returns:

- `selected_action_id_not_found` when no action matches
- `selected_action_id_ambiguous` when multiple actions match
- `runtime_controller_facade_missing` when a resolved action has no facade to execute through

The bridge does not generate legal actions or decide whether the supplied action list is correct.

## Current Runtime Controller Facade Behavior

`UWBRuntimeControllerFacadeComponent::ExecuteSelectedAction` delegates externally selected action execution to `WBSelectedActionVisualHarness`. It stores the latest runtime selected-action result and visual refresh result.

The facade does not generate legal actions and does not own rules state. It passes the caller-owned state reference through the selected-action execution path.

## Why Add Selected-Action Handoff

Future UI code will naturally read decision-point status and presentation entries from the coordinator. The selected-action handoff gives that future UI-facing coordinator a narrow way to pass a chosen action id into the existing execution path without implementing execution itself.

## Why Delegate Execution

The coordinator should stay a decision-point status and handoff surface. Execution policy already exists in the turn interaction model, action-selection bridge, runtime controller facade, selected-action visual harness, and runtime adapter. Delegating preserves those existing boundaries and keeps action resolution behavior consistent.

## No Legal Action Generation Or Validation

The coordinator still consumes the legal action list supplied by another system. It does not generate, validate, sort, filter, or infer legal actions. If a selected id is missing or ambiguous, the existing bridge policy reports that through the delegated path.

## No Game State Ownership

The selected-action handoff accepts `FWBGameStateData&` only as an externally owned reference to pass through to the interaction model. The coordinator must not store, cache, or inspect full game state.

## Hidden-Information Boundary

The coordinator status stores selected-action ids and public result reasons only. It must not expose deck, hand, discard, hidden marker identity, private pending choices, or raw hidden state. Presentation labels remain produced by `WBRuntimeLegalActionPresentation`.

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
- direct EffectRunner calls
- automatic legal-action refresh after execution
