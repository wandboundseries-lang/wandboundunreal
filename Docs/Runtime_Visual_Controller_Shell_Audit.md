# Runtime Visual Controller Shell Audit

## Existing Board View Actor

`AWBBoardViewActor` is a read-only placeholder visual actor. It owns instanced mesh components for tiles, units, walls, and terrain, and renders from `FWBPublicBoardSummary` through `RenderPublicBoardSummary`.

The actor clears prior instances, derives board dimensions from the public summary, and records rendered public counts. It does not own rules truth, does not inspect `FWBGameStateData`, and does not read deck, hand, discard, pending choices, or hidden marker identity.

## Existing Board Summary Bridge

`WBBoardSummaryBridge` provides the public-summary boundary between core state and runtime visuals.

- `BuildPublicSummaryFromState` delegates to `WBPublicBoardSummary::Build`.
- `RenderSummaryToBoardView` renders an already public summary into an `AWBBoardViewActor`.
- `RenderStateToBoardView` exists as a const-state convenience wrapper around public-summary construction.

The bridge returns `FWBBoardViewRefreshResult`, which contains public render metadata only.

## Existing State Applier

`UWBBoardViewStateApplierComponent` stores the latest `FWBPublicBoardSummary`, tracks whether one has been received, and applies it to an assigned `AWBBoardViewActor`.

It caches public summary data and refresh metadata only. It does not cache full `FWBGameStateData`, mutate state, generate actions, or own gameplay truth. Its optional const-state setter is a bridge convenience that immediately converts to public summary data.

## Existing Demo Harness

`AWBBoardViewDemoHarnessActor` renders `WBBoardViewDemoData::MakeSmallDemoBoardSummary()` through the bridge into an assigned board view actor. It is a manual C++ verification helper and does not add input, UI, camera, animation, VFX, Blueprints, UMG, assets, or gameplay ownership.

## Runtime Selected-Action Result Envelope

`FWBRuntimeSelectedActionResult` currently exposes:

- `ApplyResult`
- `SelectedActionType`
- `SelectedActionId`
- MP roll metadata
- `FinalPublicTurnSummary`
- `FinalPublicBoardSummary`

For visual refresh, only `FinalPublicBoardSummary` is needed. Trace data and action metadata may be useful for later UI or replay surfaces, but this pass does not consume them.

## Why Add A Visual Controller Shell

The runtime path now has a board actor, summary bridge, demo data, demo harness, and state applier. The missing piece is a small controller shell that future runtime code can call after a selected action result is produced.

This shell coordinates refresh calls into `UWBBoardViewStateApplierComponent` while staying outside rules ownership. It accepts public summaries directly, or accepts a runtime selected-action result only to read `FinalPublicBoardSummary`.

## Public Summary Boundary

The controller shell consumes public summary data only. It does not own or cache `FWBGameStateData`, does not inspect hidden state, does not reveal hidden marker identity, and does not serialize or log hidden deck, hand, discard, or pending-choice data.

## Intentionally Not Implemented

This pass intentionally does not implement:

- input
- tile picking
- unit selection
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
- gameplay rules
- legal action generation
- gameplay state ownership
