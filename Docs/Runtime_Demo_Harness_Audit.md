# Runtime Demo Harness Audit

## Existing Board View Actor

`AWBBoardViewActor` is the current read-only runtime visual scaffold. It owns instanced mesh components for tiles, units, walls, and terrain. It consumes `FWBPublicBoardSummary` through `RenderPublicBoardSummary`, clears previous instances, and updates last-rendered public counts.

Mesh properties are editable and null by default. Null meshes skip instance creation and do not crash.

## Existing Demo Summary Helper

`WBBoardViewDemoData::MakeSmallDemoBoardSummary` returns a public-only demo board summary:

- 9x9 board
- 2 visible public units
- 1 public wall
- 2 public terrain tiles

It does not construct or inspect `FWBGameStateData`, decks, hands, discards, hidden choices, or marker identity.

## Existing Bridge

`WBBoardSummaryBridge` provides the runtime-safe render path from public data to the board view actor:

- `BuildPublicSummaryFromState`
- `RenderStateToBoardView`
- `RenderSummaryToBoardView`

The bridge delegates state conversion to `WBPublicBoardSummary::Build` and sends only `FWBPublicBoardSummary` to `AWBBoardViewActor`.

## Demo Harness Plan

Add `AWBBoardViewDemoHarnessActor` as a tiny C++ manual verification helper. It should:

- hold an optional `AWBBoardViewActor` reference
- optionally find the first board view actor in the current world
- call `WBBoardViewDemoData::MakeSmallDemoBoardSummary`
- route the public summary through `WBBoardSummaryBridge`
- return `FWBBoardViewRefreshResult`

This lets a developer manually confirm the placeholder board rendering path in an editor level without adding Blueprints, assets, UI, or gameplay ownership.

## Public Summary Boundary

The harness consumes public summary data only. It should not query or store `FWBGameStateData`; the demo source is already `FWBPublicBoardSummary`.

## Hidden-Information Boundary

The harness must not:

- query decks, hands, discards, or pending hidden choices
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
