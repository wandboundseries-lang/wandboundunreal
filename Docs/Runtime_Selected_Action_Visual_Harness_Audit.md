# Runtime Selected Action Visual Harness Audit

## Current Selected-Action Execution Path

`WBSelectedActionExecutor::ApplySelectedAction` is the core selected-action execution switch. It accepts mutable `FWBGameStateData` by reference, an externally supplied `FWBAction`, and execution context. It delegates mutation to existing rules/effect surfaces:

- Move: `WBEffectRunner::ApplyMove`
- Attack: `WBEffectRunner::ApplyAttackDeclare`
- EndTurn basic: `WBEffectRunner::ApplyEndTurn`
- EndTurn full transition: `WBTurnController::ApplyTurnCommand`
- PassResponse: `WBEffectRunner::ApplyPassResponse`
- Pass: `WBEffectRunner::ApplyPass`

This path executes an already selected action. It does not generate legal actions.

## Current Runtime Result Envelope Path

`WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult` wraps selected-action execution in `FWBRuntimeSelectedActionResult`.

The envelope contains:

- `ApplyResult`
- selected action type/id metadata
- MP roll consumption metadata
- `FinalPublicTurnSummary`
- `FinalPublicBoardSummary`

For full end-turn transitions, the adapter consumes the externally supplied `FWBRuntimeTurnResolutionContext::MPRollSource` when needed. If the roll source is missing or invalid, it returns a failed envelope and still rebuilds public summaries from the unchanged state.

## Current Visual Controller Shell Behavior

`UWBRuntimeVisualControllerComponent` accepts `FWBPublicBoardSummary` directly, or accepts `FWBRuntimeSelectedActionResult` only to read `FinalPublicBoardSummary`.

It forwards public board summary data to `UWBBoardViewStateApplierComponent`. The state applier then refreshes an assigned `AWBBoardViewActor` through the public board summary bridge.

The visual controller stores only public refresh metadata and whether a public summary was received. It does not own, cache, or mutate `FWBGameStateData`.

## Why Add A Selected-Action Visual Harness

The current runtime pieces exist, but there is no small C++ bridge that performs the future runtime flow in one call:

1. receive an externally selected action
2. execute it through the existing runtime adapter
3. forward the resulting public board summary to the visual controller
4. return both runtime and visual refresh results

The selected-action visual harness provides that seam without adding input, UI, camera logic, assets, or gameplay ownership.

## External State Boundary

The harness takes `FWBGameStateData&` so the existing core adapter can mutate authoritative rules state. The harness does not store the state, take ownership of it, or cache it after the call.

State ownership remains outside runtime visuals. Actors display state; they do not own rules truth.

## No Legal Action Generation

The harness executes an externally supplied `FWBAction`. It does not decide legality, generate action lists, call `WBRules::GenerateLegalActions`, or inspect legal action families.

Future input/UI code must provide an already selected action from the proper rules-facing surface.

## Hidden-Information Boundary

The harness does not inspect deck, hand, discard, pending hidden choices, hidden marker identity, or private replay data. Visual refresh uses only `FWBRuntimeSelectedActionResult::FinalPublicBoardSummary`.

Hidden data remains excluded from runtime visual reports, refresh counts, and tests.

## Intentionally Not Implemented

This pass intentionally does not implement:

- input
- tile picking
- selection
- UI
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
- network replay
- AI behavior
