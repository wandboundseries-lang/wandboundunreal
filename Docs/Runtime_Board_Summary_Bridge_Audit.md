# Runtime Board Summary Bridge Audit

## Current Visual Scaffold

Visual Pass 1 added the `WandboundRuntime` module with:

- `WBBoardViewTypes`: visual-only board-to-world transform helpers.
- `AWBBoardViewActor`: read-only display actor with instanced mesh components for tiles, units, walls, and terrain.
- `WBBoardViewDemoData`: public demo summary for future manual smoke testing.

`AWBBoardViewActor::RenderPublicBoardSummary` already consumes `FWBPublicBoardSummary` and reports last rendered tile, unit, wall, and terrain counts. Mesh references are editable and null by default.

## Public Summary Builder

`WBPublicBoardSummary::Build(const FWBGameStateData& State)` is the existing Core-owned public data filter. It produces:

- board dimensions
- visible units
- public unit statuses
- public armor and combat fields
- normalized public walls
- sparse public terrain

Deck, hand, discard, pending hidden choices, and hidden marker identity are not part of `FWBPublicBoardSummary`.

## Bridge Plan

Add `WBBoardSummaryBridge` in `WandboundRuntime`.

The bridge should provide:

- `BuildPublicSummaryFromState`: delegates to `WBPublicBoardSummary::Build`.
- `RenderStateToBoardView`: builds a public summary, then renders it.
- `RenderSummaryToBoardView`: renders an already-public summary.
- `FWBBoardViewRefreshResult`: reporting-only public render counts.

## Public Summary Boundary

The bridge consumes `FWBPublicBoardSummary` instead of exposing `FWBGameStateData` to actors because public summaries are already filtered for display-safe data. This keeps runtime visuals from owning rules truth or inspecting private game state.

`RenderStateToBoardView` may accept `const FWBGameStateData&` only to call the existing public summary builder. It must not mutate, store, or directly inspect private fields such as deck, hand, discard, or marker identity.

## Hidden-Information Boundary

The bridge must not:

- store `FWBGameStateData`
- query decks, hands, discards, or pending hidden choices directly
- expose hidden marker identity
- log hidden data
- serialize private state
- generate actions

All data passed to `AWBBoardViewActor` must already be public-summary data.

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
