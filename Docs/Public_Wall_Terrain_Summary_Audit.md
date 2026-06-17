# Public Wall/Terrain Summary Audit

Date: 2026-06-17

## Scope

This audit precedes the Unreal public wall and terrain summary pass. It covers reporting and serialization only. It does not authorize gameplay, legality, terrain effect, wall placement/removal, UI, Blueprint, or asset changes.

## Files Inspected

- `Reference/GodotCanon/README.md`
- `Reference/GodotCanon/GodotSourceIndex.md`
- `Reference/GodotProject/godotcanon/scripts/sim/game_state.gd`
- `Reference/GodotProject/godotcanon/scripts/sim/rules.gd`
- `Reference/GodotProject/godotcanon/scripts/ai/ai_env.gd`
- `Reference/GodotProject/godotcanon/scripts/ai/data/decision_snapshot_builder.gd`
- `Reference/GodotProject/godotcanon/autoload/Game.gd`
- `Source/WandboundCore/Public/WBGameStateData.h`
- `Source/WandboundCore/Private/WBGameStateData.cpp`
- `Source/WandboundCore/Public/WBTypes.h`
- `Source/WandboundCore/Public/WBRules.h`
- `Source/WandboundCore/Private/WBRules.cpp`
- `Source/WandboundCore/Public/WBPublicBoardSummary.h`
- `Source/WandboundCore/Private/WBPublicBoardSummary.cpp`
- `Source/WandboundCore/Private/WBRuntimeResultSerializer.cpp`
- `Source/WandboundTests/Private/WBPublicBoardSummaryTests.cpp`
- `Source/WandboundTests/Private/WBRuntimeResultBoardSummarySerializationTests.cpp`
- `Source/WandboundTests/Private/WBReplayFixtureTestUtils.cpp`

## Godot Findings

Godot walls are public board state. `GameState` stores wall edges in two public boolean arrays:

- `wall_v[x][y]`: wall between `(x,y)` and `(x+1,y)`
- `wall_h[x][y]`: wall between `(x,y)` and `(x,y+1)`

`ai_env.gd::build_observation` and `autoload/Game.gd::_pvp_build_public_observation_for_player_impl` include `wall_v` and `wall_h` in the public `board` observation payload.

Godot terrain is public board state. `GameState` stores a full board `terrain_map[x][y]` and `terrain_ticks[x][y]`. The default terrain value is `"none"`. `get_terrain_at` returns `"none"` out of bounds or for unset/default tiles. `set_terrain_at` writes a terrain id string and optional ticks. Public observations include the full `terrain_map`.

Godot terrain ids are normalized to lower-case strings. Canonical ids currently include `water`, `ice`, `mud`, and `lava`.

Godot hidden markers are perspective-gated. `_public_marker_summary` includes marker `card_id` and `revealed_card_id` only when identity is visible to the observing player. Otherwise marker kind is reported as `hidden` and card identity is omitted.

## Current Unreal Findings

Unreal wall state already exists in `FWBGameStateData::Walls` as `TArray<FWBWallEdge>`. `FWBWallEdge` stores two `FWBTile` endpoints. `WBRules::NormalizeWallEdge`, `WBRules::IsValidWallEdge`, `WBRules::AreSameWallEdge`, and `WBRules::HasWallBetween` already define wall validity, normalization, comparison, and movement blocking.

Unreal terrain state does not exist yet.

Unreal marker state does not exist yet.

The current public board summary includes units only. Visible unit `CardId` is already treated as public. Deck, hand, discard, pending choices, runtime context, UObject pointers, resources, and file paths remain excluded from runtime result serialization.

## Chosen Unreal Summary Policy

Walls are public and will be included in `FWBPublicBoardSummary::Walls`.

Wall summaries will use normalized endpoint coordinates:

- `AX`
- `AY`
- `BX`
- `BY`
- `Orientation`

Orientation will be:

- `horizontal` when the normalized endpoints share `Y` and differ in `X`
- `vertical` when the normalized endpoints share `X` and differ in `Y`

Invalid or non-orthogonal wall edges will be excluded from the public summary. Existing test helpers already reject invalid walls; this exclusion is an extra reporting guard.

Terrain is public and will be included in `FWBPublicBoardSummary::TerrainTiles` as a sparse list. Unreal will add minimal terrain state for reporting/test scaffolding:

- `DefaultTerrainId = "Normal"`
- `TerrainByTileIndex`, keyed by tile index
- `GetTerrainAt`
- `SetTerrainForTest`
- `ClearTerrainForTest`

The Godot default is `"none"`. Unreal uses `"Normal"` as the closest safe C++ baseline because no terrain gameplay exists yet and `"Normal"` reads as an explicit public default. The sparse terrain list will include only tiles whose `TerrainId` differs from `DefaultTerrainId`.

Terrain ids are public names only. Fixtures may use `Mud`, `Lava`, `Ice`, or `Water`. The summary does not include internal effect ids or terrain ticks.

Markers remain excluded from public board summary in this pass. Hidden marker identity cannot leak because Unreal marker state is not modeled and no marker summary is serialized.

## Included Fields

- board width and height
- default terrain id
- visible public unit summaries, unchanged
- normalized public wall edges
- sparse public non-default terrain tiles

## Excluded Fields

- hidden marker identity
- marker summaries
- deck contents
- hand contents
- discard contents
- pending choices
- runtime context internals
- UObject, asset, resource, or file paths
- terrain effect ids
- terrain ticks

## TODOs

- Add marker state and a perspective-aware public/revealed marker summary.
- Add public wall action traces when wall placement/removal actions exist.
- Add terrain effect rules after terrain behavior is ported and tested.
- Add terrain movement rules only when movement legality parity calls for them.
- Add runtime/UI/network consumers for public board summary JSON.
