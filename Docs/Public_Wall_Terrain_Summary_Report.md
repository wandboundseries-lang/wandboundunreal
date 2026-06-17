# Public Wall/Terrain Summary Report

Date: 2026-06-17

## Purpose

This pass adds deterministic public wall and terrain summaries to runtime selected-action results.

The work is reporting-only. It does not change movement legality, wall blocking, terrain behavior, action generation, action IDs, replay `apply_action`, or gameplay effects.

## Public Wall Summary

Walls are public board state.

Each serialized wall contains:

- `ax`
- `ay`
- `bx`
- `by`
- `orientation`

Wall edges are normalized through the existing Unreal wall normalization path. A wall authored as A-B or B-A produces the same public summary.

Invalid or non-orthogonal wall edges are excluded from public summaries.

Wall summaries are sorted by:

1. `ay`
2. `ax`
3. `by`
4. `bx`
5. `orientation`

## Public Terrain Summary

Terrain is public board state.

Unreal now has minimal terrain state for reporting/test scaffolding:

- `DefaultTerrainId`
- `TerrainByTileIndex`
- `GetTerrainAt`
- `SetTerrainForTest`
- `ClearTerrainForTest`

This state is not used by movement legality or effects in this pass.

## Default Terrain Policy

Godot terrain defaults to `"none"` in `terrain_map[x][y]`.

Unreal uses `Normal` as the public default terrain id. This is the safe baseline while terrain gameplay is not implemented.

## Sparse Terrain Policy

Runtime result JSON includes:

```text
default_terrain_id
terrain_tiles
```

Only tiles whose terrain differs from `default_terrain_id` are serialized. This keeps the public board summary compact and deterministic.

Terrain tiles are sorted by:

1. `y`
2. `x`
3. `terrain_id`

Known public terrain ids follow Godot's lower-case convention:

- `mud`
- `lava`
- `ice`
- `water`

## Hidden Information

Hidden marker identity remains excluded. Unreal marker state is not modeled yet, so no marker summary is emitted.

Deck, hand, discard, pending choices, runtime context internals, UObject pointers, asset paths, terrain effect ids, and terrain ticks remain excluded.

## Fixtures

Added:

- `public_board_summary_walls_sorted.json`
- `public_board_summary_wall_edge_normalized.json`
- `public_board_summary_terrain_sparse_sorted.json`
- `public_board_summary_wall_terrain_hidden_data_excluded.json`
- `runtime_result_serialization_public_wall_terrain_after_move.json`
- `runtime_result_serialization_public_wall_terrain_after_full_turn.json`

## Future TODO

- Add revealed marker summary after marker state exists.
- Add public wall placement/removal traces after wall actions exist.
- Add terrain effect rules.
- Add terrain movement rules.
- Add network replay envelope.
- Add UI/runtime consumers.
