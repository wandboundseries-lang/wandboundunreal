# Public Board Summary Report

Date: 2026-06-17

## Purpose

This pass adds deterministic public board summaries for runtime selected-action results.

The summary is reporting-only. It does not mutate state, change legal action generation, change rules, or alter replay `apply_action`.

## Public Board Summary

Added:

```text
FWBPublicUnitStatusSummary
FWBPublicUnitBoardSummary
FWBPublicWallEdgeSummary
FWBPublicTerrainTileSummary
FWBPublicBoardSummary
WBPublicBoardSummary::Build
```

The summary includes:

- board width
- board height
- default terrain id
- visible unit id
- visible unit owner id
- visible unit card id
- unit x/y position
- HP/max HP
- ATK/AR
- RL total/used
- attacks left
- public status ids and turns remaining
- public wall edges
- sparse public non-default terrain tiles

Walls and terrain are public board state. Markers remain excluded until marker state is modeled in Unreal.

## Wall Summary

Each wall summary includes:

- `AX`
- `AY`
- `BX`
- `BY`
- `Orientation`

Wall endpoints are normalized so the same edge serializes identically whether it was authored A-B or B-A.

Wall summaries are sorted by:

1. `AY`
2. `AX`
3. `BY`
4. `BX`
5. `Orientation`

Invalid or non-orthogonal wall edges are excluded by the summary builder.

## Terrain Summary

Unreal now has minimal public terrain state for reporting/test scaffolding:

- `DefaultTerrainId`
- sparse `TerrainByTileIndex`
- `GetTerrainAt`
- `SetTerrainForTest`
- `ClearTerrainForTest`

The Unreal default terrain id is `Normal`. Godot defaults terrain to `"none"`; `Normal` is the safe Unreal baseline while terrain gameplay remains unimplemented.

Terrain summaries are sparse: only tiles whose terrain differs from `DefaultTerrainId` are listed.

Terrain tiles are sorted by:

1. `Y`
2. `X`
3. `TerrainId`

## Excluded Fields

Excluded:

- deck contents
- hand contents
- discard contents
- hidden marker identity
- hidden marker card ids
- pending choices
- private response options
- terrain effect ids
- terrain ticks
- passives
- equipped card/wand internals
- raw UObject pointers/resources
- file paths

## Visible Unit Card Identity Policy

The hidden-information audit found that Godot public unit observations include visible board unit `card_id`.

Unreal public board summaries therefore include `FWBUnitState::CardId` for visible board units.

## Hidden Marker Policy

Godot marker identity is player-perspective dependent. Hidden markers do not reveal card identity unless the marker belongs to the observing player or has been revealed.

Unreal does not yet have marker state, and this summary is not player-perspective scoped. This pass excludes marker fields entirely.

## Deterministic Ordering

Units are sorted by:

1. `y` ascending
2. `x` ascending
3. `unit_id` ascending

Statuses are canonicalized and sorted by public status id.

Current public status ids:

- `Burn`
- `Frozen`
- `Poison`
- `Rooted`
- `Stunned`

Status aliases such as `root` and `stun` are normalized to `Rooted` and `Stunned`.

## Serialization Integration

`FWBRuntimeSelectedActionResult` now includes:

```text
FinalPublicBoardSummary
```

`WBRuntimeResultSerializer` now serializes:

```text
final_public_board_summary
```

Runtime result failures summarize the unchanged board state.

## Hidden-Data Coverage

Tests assert that serialized runtime results do not contain secret tokens placed in:

- deck
- hand
- discard

Marker and pending-choice secret tokens are asserted absent. Unreal marker state is not modeled yet, so hidden marker identity cannot be serialized in this pass.

## Fixtures

Added:

- `public_board_summary_units_sorted.json`
- `public_board_summary_statuses_sorted.json`
- `public_board_summary_hidden_data_excluded.json`
- `runtime_result_serialization_public_board_summary_after_move.json`
- `runtime_result_serialization_public_board_summary_after_full_turn.json`
- `public_board_summary_walls_sorted.json`
- `public_board_summary_wall_edge_normalized.json`
- `public_board_summary_terrain_sparse_sorted.json`
- `public_board_summary_wall_terrain_hidden_data_excluded.json`
- `runtime_result_serialization_public_wall_terrain_after_move.json`
- `runtime_result_serialization_public_wall_terrain_after_full_turn.json`

## Future TODO

- Add revealed marker summaries after marker state exists.
- Add public wall placement/removal action traces after those actions exist.
- Add terrain movement/effect rules after behavior parity work begins.
- Add public discard viewer/summary after a discard visibility audit.
- Add network replay envelope.
- Add UI/runtime consumer.
