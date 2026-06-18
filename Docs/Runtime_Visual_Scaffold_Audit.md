# Runtime Visual Scaffold Audit

## Existing Modules

Current modules in `WandboundUE.uproject`:

- `WandboundUE`: primary runtime game module.
- `WandboundCore`: deterministic rules/state/actions/effects/replay/public summary module.
- `WandboundTests`: developer automation test module.

At the start of this pass, no `WandboundRuntime` module existed yet. No existing runtime Actor layer owned rules truth.

## Public Summary Types Available

`Source/WandboundCore/Public/WBPublicBoardSummary.h` already provides the read-only board data needed for a visual scaffold:

- `FWBPublicBoardSummary`
- `FWBPublicUnitBoardSummary`
- `FWBPublicWallEdgeSummary`
- `FWBPublicTerrainTileSummary`
- `FWBPublicUnitStatusSummary`

The public summary includes board size, visible units, walls, sparse public terrain, public statuses, armor fields, and visible combat fields.

## Visual Module Plan

Add a secondary runtime module named `WandboundRuntime`.

The module should depend on:

- `Core`
- `CoreUObject`
- `Engine`
- `WandboundCore`

The first runtime visual scaffold should provide:

- pure coordinate helpers for board-to-world transforms
- a read-only `AWBBoardViewActor`
- optional manual placeholder rendering through editable mesh properties
- demo public summary data for future manual smoke testing

No UMG, Slate, editor-only dependency, Blueprint asset, map, material, mesh asset, or imported asset is needed for this pass.

## Public Summary Boundary

Runtime visuals should consume `FWBPublicBoardSummary` instead of `FWBGameStateData` because public summaries are already filtered for display-safe data. This keeps the visual layer from seeing hidden deck, hand, discard, pending choice, or marker identity data.

Actors display state only. They do not own gameplay truth, mutate rules state, validate legality, generate actions, or select targets.

## Hidden-Information Boundary

The visual actor must not:

- hold `FWBGameStateData`
- query decks, hands, discards, or pending hidden choices
- expose hidden marker identity
- log hidden data
- serialize private rule state

Visible unit card identity, public statuses, armor, walls, and terrain are already part of the public board summary policy.

## Intentionally Not Implemented

- player input
- tile picking
- unit selection
- action menus
- hand UI
- response UI
- card UI
- animations
- VFX
- sound
- camera focus logic
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
