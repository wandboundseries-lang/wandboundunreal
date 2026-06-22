# Runtime Decision-Loop Harness Report

Date: 2026-06-21

## Scope

This pass adds a C++ test-only runtime decision-loop harness under `WandboundTests`.

The harness proves this runtime flow:

1. an external owner supplies legal actions and a public board summary
2. `UWBRuntimeDecisionPointCoordinatorComponent::RefreshDecisionPoint` refreshes the decision point
3. test code chooses a presented action id
4. `UWBRuntimeDecisionPointCoordinatorComponent::ExecuteSelectedActionId` hands the id to the turn interaction model
5. the action executes through the existing runtime selected-action path
6. the presentation snapshot remains stale until an explicit refresh
7. an external owner supplies new legal actions/public summary and refreshes again

## Test-Only Helper

`FWBDecisionLoopTestHarness` lives only in `Source/WandboundTests/Private`.

It may call `WBRules::GenerateLegalActionsForPlayer` and `WBPublicBoardSummary::Build` because it simulates a future external rules/runtime owner in automation tests.

`WandboundRuntime` does not include this helper and production runtime code must not use it.

## External Rules Owner Simulation

The helper intentionally generates legal actions and public summaries outside runtime production code. This preserves the current runtime boundary:

- rules/state/effects remain in deterministic C++
- runtime visual/coordinator code consumes externally supplied legal actions
- runtime visual/coordinator code consumes public summaries
- runtime visual/coordinator code does not decide legality
- runtime visual/coordinator code does not own or cache `FWBGameStateData`

## Two-Decision Loop

Automation coverage exercises:

- refresh exposing Move
- Move execution through selected-action handoff
- stale snapshot remaining after execution
- explicit post-action refresh replacing that snapshot
- a second decision executing EndTurn
- full EndTurn with deterministic MP roll advancing to the next player and refreshing next-player actions
- optional visual refresh participation through the existing board actor/state applier/visual controller path

## Hidden-Information Boundary

The tests seed hidden deck, hand, and discard tokens, then inspect public snapshots and runtime result public summaries. Those hidden tokens remain absent.

Visible public unit card ids remain public. Hidden marker identity remains excluded because marker state is not modeled yet.

## Production Boundary

The runtime source guard confirms `Source/WandboundRuntime` does not contain:

- `GenerateLegalActions`
- `WBRules`
- `WBEffectRunner`
- `WBRuntimeDecisionLoopTestHarness`

## Explicit Non-Goals

This pass does not add:

- gameplay rules
- production legal action generation in runtime code
- legality decisions in runtime code
- state ownership in runtime components
- automatic post-action refresh
- input
- tile picking
- unit selection
- UI widgets
- camera logic
- animation
- VFX
- sound
- marker visuals
- hidden marker identity
- Blueprints
- UMG
- `.uasset` edits
- `.umap` edits
- asset imports

## Future Usage

Future runtime/UI ownership can follow this tested shape:

1. rules/runtime owner generates legal actions and a public summary
2. coordinator refreshes the decision point
3. UI displays presentation entries
4. UI submits a selected action id
5. coordinator executes the handoff
6. rules/runtime owner explicitly refreshes the next decision point

## Owner Shell Follow-Up

`UWBRuntimeDecisionPointOwnerComponent` now mirrors this tested decision-loop pattern in production C++ without generating actions itself. It accepts externally supplied legal actions/public summaries, delegates selected action ids through the coordinator, and can apply an explicit post-action refresh from caller-supplied data.
