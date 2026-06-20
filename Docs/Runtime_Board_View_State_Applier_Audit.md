# Runtime Board View State Applier Audit

## Existing Board View Actor

`AWBBoardViewActor` is a read-only display actor. It consumes `FWBPublicBoardSummary` through `RenderPublicBoardSummary`, clears previous visual instances, and updates public last-rendered counts for tiles, units, walls, and terrain.

The actor owns visual components only. It does not own rules truth, validate legality, generate actions, mutate state, or inspect private rule data.

## Existing Board Summary Bridge

`WBBoardSummaryBridge` provides the current public render path:

- `BuildPublicSummaryFromState` delegates to `WBPublicBoardSummary::Build`.
- `RenderStateToBoardView` builds a public summary from a const state reference.
- `RenderSummaryToBoardView` sends an already-public summary to `AWBBoardViewActor`.

The bridge returns `FWBBoardViewRefreshResult`, which contains reporting-only public render counts.

## Existing Demo Harness

`AWBBoardViewDemoHarnessActor` is a manual C++ verification helper. It renders `WBBoardViewDemoData::MakeSmallDemoBoardSummary()` through `WBBoardSummaryBridge::RenderSummaryToBoardView`.

It does not query or store `FWBGameStateData`.

## State Applier Plan

Add `UWBBoardViewStateApplierComponent` as a read-only visual component for future runtime/controller code.

The component should:

- hold an optional `AWBBoardViewActor` reference
- cache the latest `FWBPublicBoardSummary`
- optionally build a public summary from const `FWBGameStateData` through `WBBoardSummaryBridge`
- apply the latest public summary to the board view actor
- remember the last public refresh result

## Future Runtime Usage

After a rules action resolves, future runtime/controller code can build or receive a public board summary and call `SetLatestPublicBoardSummary`. If `bApplyOnSummarySet` is true, the board view refreshes immediately. Otherwise, runtime code can call `ApplyLatestPublicBoardSummary` explicitly.

The component exists to make the visual update path stable before adding any input, UI, camera, animation, or asset work.

## Public Summary Storage Boundary

It is acceptable for this component to cache `FWBPublicBoardSummary` because the summary is already display-safe. It is not acceptable for the component to cache full `FWBGameStateData`, decks, hands, discards, pending hidden choices, or marker identity.

`SetLatestPublicBoardSummaryFromState` may accept a const state reference only to delegate to `WBBoardSummaryBridge::BuildPublicSummaryFromState`.

## Hidden-Information Boundary

The component must not:

- store `FWBGameStateData`
- query decks, hands, discards, or pending hidden choices directly
- expose hidden marker identity
- log hidden data
- serialize private rule state
- generate actions
- own gameplay truth

## Intentionally Not Implemented

- player input
- tile picking
- unit selection
- action menus
- hand UI
- response UI
- card UI
- camera logic
- animations
- VFX
- sound
- Blueprints
- UMG widgets
- `.uasset` edits
- `.umap` edits
- asset imports
- CardDB import
- marker visuals
- hidden marker identity
- network replay
- AI behavior
- gameplay ownership
