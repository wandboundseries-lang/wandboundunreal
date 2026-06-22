# Runtime Decision-Point Owner Shell Audit

Date: 2026-06-21

## Current Decision-Point Coordinator Behavior

`UWBRuntimeDecisionPointCoordinatorComponent` accepts externally supplied legal actions and an externally supplied `FWBPublicBoardSummary`.

On refresh it delegates to `WBRuntimeInteractionRefreshAdapter::RefreshDecisionPoint`, stores the refresh result, updates public decision-point status, and exposes the current presentation snapshot through its configured turn interaction model.

On failed refresh it clears its current decision-point flag and stores the failure reason in status. It does not generate actions, decide legality, call `WBRules`, call `WBEffectRunner`, own state, inspect hidden zones, or implement input/UI behavior.

## Current Selected-Action Handoff Behavior

`UWBRuntimeDecisionPointCoordinatorComponent::ExecuteSelectedActionId` requires a current decision point and a configured `UWBRuntimeTurnInteractionModelComponent`.

It delegates selected action ID resolution and execution to the turn interaction model, which delegates to `WBRuntimeActionSelectionBridge`, then to `UWBRuntimeControllerFacadeComponent`, then through the selected-action visual/runtime adapter path.

After execution, the coordinator stores selected-action status but does not regenerate or refresh the legal action list.

## Current Test-Only Decision Loop Harness Behavior

`FWBDecisionLoopTestHarness` lives under `WandboundTests` only. It simulates an external rules/runtime owner by calling `WBRules::GenerateLegalActionsForPlayer` and `WBPublicBoardSummary::Build` in automation tests.

The harness proves the loop shape:

1. external owner supplies legal actions and public summary
2. coordinator refreshes
3. test code chooses a presented action id
4. coordinator executes the selected action id
5. state changes through the existing runtime execution path
6. the old presentation remains stale
7. external owner supplies updated legal actions/public summary
8. coordinator refreshes again

The harness is not production code and must not be included by `WandboundRuntime`.

## Why This Pass Adds A Production Owner Shell

The coordinator already handles one decision point. The runtime still needs a production C++ shell that groups the future-facing calls a runtime owner will make:

- refresh the coordinator from externally supplied legal actions/public summary
- execute a selected action id through the coordinator
- optionally perform an explicit post-action refresh from externally supplied updated data

This shell mirrors the tested decision-loop pattern without becoming the rules owner.

## External Data Only

The owner shell consumes legal actions and public board summaries supplied by another system. It must not create, validate, sort, filter, or infer those legal actions.

Future rules/runtime ownership remains outside this shell. That owner will generate legal actions and build public summaries, then pass them in.

## No Legal Action Generation Or Validation

The owner shell must not call:

- `WBRules::GenerateLegalActions`
- `WBRules` legality functions
- `WBEffectRunner`

It also must not decide whether a selected action is legal. It passes selected action ids to the coordinator, and the existing interaction model/action-selection bridge resolves them against the externally supplied legal action list.

## No Game State Ownership

The owner shell accepts `FWBGameStateData&` only as execution pass-through data for the coordinator handoff. It must not store, cache, copy, or expose `FWBGameStateData`.

Actors and runtime components display or coordinate public state; they do not own rules truth.

## Explicit Post-Action Refresh

Selected-action execution can mutate rules state through the existing core runtime adapter path. The owner shell cannot infer the next legal action list afterward.

Therefore post-action refresh requires the caller to provide updated legal actions and an updated public summary. The combined execute-and-refresh method may refresh only from that caller-supplied data, and only after execution succeeds.

If execution fails, the shell keeps the execution result and does not apply the post-action refresh.

If execution succeeds but the post-action refresh fails, the shell stores both the execution result and the refresh failure. Current status follows the coordinator refresh failure policy.

## Hidden-Information Boundary

The owner shell consumes `FWBPublicBoardSummary` for presentation/visual refresh and selected action ids from public presentation. Hidden deck, hand, discard, private pending choices, and hidden marker identity must not appear in shell status, snapshots, logs, reports, or serialized output.

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
- production legal action generation
- legality decisions
- automatic post-action legal-action generation
