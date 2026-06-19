# Runtime Board Summary Bridge Report

## Scope

This pass adds the minimal runtime bridge from rules state/public summaries to the board view actor.

Added:

- `WBBoardSummaryBridge`
- `FWBBoardViewRefreshResult`
- bridge automation tests

Not implemented:

- gameplay rules
- legal action generation
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

## Bridge API

`WBBoardSummaryBridge::BuildPublicSummaryFromState` delegates to `WBPublicBoardSummary::Build`.

`WBBoardSummaryBridge::RenderStateToBoardView` builds a public summary from a const state reference, then forwards it to `RenderSummaryToBoardView`.

`WBBoardSummaryBridge::RenderSummaryToBoardView` safely handles a null `AWBBoardViewActor` and otherwise calls `AWBBoardViewActor::RenderPublicBoardSummary`.

`FWBBoardViewRefreshResult` reports:

- whether a board actor was rendered
- tile count
- unit count
- wall count
- terrain count

The result is reporting-only and contains no hidden data.

## Hidden-Information Boundary

The bridge does not store or inspect private rule state. It only accepts `FWBGameStateData` to call the existing public summary builder. Deck, hand, discard, pending hidden choices, and hidden marker identity remain outside the board view render path.

## No Gameplay Mutation

The bridge does not mutate `FWBGameStateData`, does not generate actions, and does not own gameplay truth. Actors remain display-only.

## Manual Future Use

After a rules action resolves, future runtime code can:

1. Build or receive the latest `FWBGameStateData`.
2. Call `WBBoardSummaryBridge::RenderStateToBoardView(State, BoardViewActor)`.
3. Let `AWBBoardViewActor` refresh placeholder visual instances from the public summary.

No input, UI, camera, animation, VFX, or asset work is required for this bridge.
