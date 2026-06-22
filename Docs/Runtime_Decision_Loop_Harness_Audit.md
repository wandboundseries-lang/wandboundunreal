# Runtime Decision-Loop Harness Audit

Date: 2026-06-21

## Current Decision-Point Coordinator Refresh Behavior

`UWBRuntimeDecisionPointCoordinatorComponent` accepts externally supplied legal actions and an externally supplied `FWBPublicBoardSummary`. It builds refresh options from its component booleans and calls `WBRuntimeInteractionRefreshAdapter::RefreshDecisionPoint`.

On success, the coordinator stores the latest refresh result, marks the decision point as current, and exposes public status plus the current presentation snapshot through the turn interaction model. On refresh failure, it clears the current decision-point flag and resets status so stale presentation entries are not exposed through the coordinator.

## Current Selected-Action Handoff Behavior

`UWBRuntimeDecisionPointCoordinatorComponent::ExecuteSelectedActionId` requires a current decision point and a configured `UWBRuntimeTurnInteractionModelComponent`. It delegates selected-action execution to `UWBRuntimeTurnInteractionModelComponent::ExecuteSelectedActionId`, passing through the externally owned `FWBGameStateData&`, runtime context, and controller facade.

The interaction model resolves against its stored legal actions through `WBRuntimeActionSelectionBridge`, and the bridge delegates execution to `UWBRuntimeControllerFacadeComponent`.

## Current Stale-State Policy

After selected-action execution, the coordinator does not regenerate legal actions and does not rebuild the presentation snapshot. The current legal action list and presentation entries remain from the last explicit refresh.

This is intentional because the coordinator does not own rules state and cannot safely infer the next legal action list. A runtime owner must explicitly refresh the coordinator with new legal actions and a new public summary after state changes.

## Why This Pass Adds A Test Harness Only

The runtime production path already has the required refresh and handoff surfaces. What remains is proving the intended loop:

1. external rules owner supplies legal actions and public summary
2. coordinator refreshes the decision point
3. selected action id is chosen by test code
4. coordinator hands the id to the interaction model
5. state mutates through the existing runtime execution path
6. stale presentation remains until an explicit post-action refresh
7. external rules owner supplies the next legal action list and public summary

This pass adds a test-only harness under `WandboundTests` to prove that loop without adding UI, input, camera, assets, or production rules generation.

## External Rules Owner Simulation

Tests may call `WBRules::GenerateLegalActions` because the harness simulates a future rules/runtime owner. The harness also may call `WBPublicBoardSummary::Build` to simulate a future public summary provider.

This test helper must not be included by `WandboundRuntime` and must not be used by production code.

## Production Runtime Boundary

Production runtime code still must not call `WBRules::GenerateLegalActions`, `WBRules` legality helpers, or `WBEffectRunner` directly from the visual/coordinator path. Runtime code consumes externally supplied legal actions and public summaries only.

## Hidden-Information Boundary

The loop consumes public board summaries for presentation and visual refresh. Hidden deck, hand, discard, private pending choices, and hidden marker identity must not appear in snapshots, status, visual results, or reports.

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
- production legal action generation in runtime visual code
- legality decisions in runtime visual code
- automatic post-action refresh
