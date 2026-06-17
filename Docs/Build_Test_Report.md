# Build/Test Report

Date of check: 2026-06-17 (America/New_York)

## Scope

This pass added deterministic public wall and terrain summaries to runtime selected-action results.

Implemented:

- public wall/terrain hidden-information audit
- `FWBPublicWallEdgeSummary`
- `FWBPublicTerrainTileSummary`
- `FWBPublicBoardSummary::Walls`
- `FWBPublicBoardSummary::TerrainTiles`
- `FWBPublicBoardSummary::DefaultTerrainId`
- minimal reporting-only terrain state in `FWBGameStateData`
- wall summary normalization and deterministic sorting
- sparse terrain summary with deterministic sorting
- runtime result JSON fields `walls`, `terrain_tiles`, and `default_terrain_id`
- fixture loading for `initial_state.default_terrain_id` and `initial_state.terrain_tiles`
- GoldenScenarios for public walls, terrain, hidden-data exclusion, move serialization, and full-turn serialization
- automation coverage for wall inclusion, normalization, sorting, invalid wall exclusion, terrain inclusion, default exclusion, terrain sorting, runtime JSON, and fixture scenarios

Not implemented:

- new gameplay mechanics
- movement legality changes
- wall blocking behavior changes
- terrain movement/effect behavior
- terrain effects
- wall placement/removal actions
- legal action generation changes
- `WBActionCodec` action ID changes
- replay `apply_action` behavior changes
- attacks
- cards/effects
- draw
- random dice
- NPC phase
- UI/Blueprint/3D runtime
- marker state or marker summary

No `.uasset`, `.umap`, Blueprint, or Godot project files were changed.

## Public Wall/Terrain Policy

Walls are public board state and now serialize as normalized wall edges.

Terrain is public board state and now serializes as sparse non-default terrain tiles.

Godot defaults terrain to `"none"`. Unreal uses `Normal` as the safe public default terrain id while terrain gameplay remains unimplemented.

Hidden marker identity remains excluded. Unreal marker state is not modeled yet, so no marker summary is emitted.

Deck, hand, discard, pending choices, runtime context internals, UObject pointers, resources, file paths, terrain effect ids, and terrain ticks remain excluded.

## Serialized Public Board Summary

Runtime selected-action result JSON includes:

```text
final_public_board_summary
```

Additional fields from this pass:

- `default_terrain_id`
- `walls`
- `walls[].ax`
- `walls[].ay`
- `walls[].bx`
- `walls[].by`
- `walls[].orientation`
- `terrain_tiles`
- `terrain_tiles[].x`
- `terrain_tiles[].y`
- `terrain_tiles[].terrain_id`

Existing unit summary fields are preserved.

## Implemented This Pass

- Added `Docs/Public_Wall_Terrain_Summary_Audit.md`.
- Added `Docs/Public_Wall_Terrain_Summary_Report.md`.
- Updated `Docs/Public_Board_Summary_Report.md`.
- Updated `Docs/Runtime_Result_Serialization_Report.md`.
- Updated `Docs/Wandbound_Unreal_Migration_Plan.md`.
- Updated `Source/WandboundCore/Public/WBGameStateData.h`.
- Updated `Source/WandboundCore/Private/WBGameStateData.cpp`.
- Updated `Source/WandboundCore/Public/WBPublicBoardSummary.h`.
- Updated `Source/WandboundCore/Private/WBPublicBoardSummary.cpp`.
- Updated `Source/WandboundCore/Private/WBRuntimeResultSerializer.cpp`.
- Updated `Source/WandboundTests/Private/WBReplayFixtureTestUtils.cpp`.
- Updated `Source/WandboundTests/Private/WBPublicBoardSummaryTests.cpp`.
- Updated `Source/WandboundTests/Private/WBRuntimeResultBoardSummarySerializationTests.cpp`.
- Added GodotCanon fixtures:
  - `public_board_summary_walls_sorted.json`
  - `public_board_summary_wall_edge_normalized.json`
  - `public_board_summary_terrain_sparse_sorted.json`
  - `public_board_summary_wall_terrain_hidden_data_excluded.json`
  - `runtime_result_serialization_public_wall_terrain_after_move.json`
  - `runtime_result_serialization_public_wall_terrain_after_full_turn.json`

## Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex
```

Result:

```text
Result: Succeeded
Total execution time: 46.02 seconds
```

## Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=175
succeededWithWarnings=0
failed=0
notRun=0
```

New tests added:

- `Wandbound.Core.PublicBoardSummary.DefaultTerrainExcluded`
- `Wandbound.Core.PublicBoardSummary.InvalidWallExcluded`
- `Wandbound.Core.PublicBoardSummary.TerrainIncluded`
- `Wandbound.Core.PublicBoardSummary.TerrainSortedDeterministically`
- `Wandbound.Core.PublicBoardSummary.WallEdgeNormalized`
- `Wandbound.Core.PublicBoardSummary.WallsIncluded`
- `Wandbound.Core.PublicBoardSummary.WallsSortedDeterministically`
- `Wandbound.Core.RuntimeResultBoardSummary.SparseTerrainSerialized`
- `Wandbound.Core.RuntimeResultBoardSummary.WallsSerialized`

Existing public board and runtime result fixture tests were expanded to include the new wall/terrain fixtures.

## Exact Errors

Final build and automation reported no errors.

## Generated File Tracking

Generated folders/artifacts remain untracked:

- `.vs/`
- `Binaries/`
- `Intermediate/`
- `Saved/`
- `DerivedDataCache/`
- common binary/log artifacts such as `*.pdb`, `*.obj`, `*.pch`, `*.ilk`, and `*.log`

The pre-existing untracked `MaxHP` file remains untouched.

## Remaining Warnings

Build and automation reported no warnings.

## Risks/Unknowns

- Unreal marker state does not exist yet, so hidden marker summary policy is documented but not implemented.
- Terrain state is reporting/test scaffolding only and is not connected to movement or effects.
- Wall placement/removal actions and wall action traces are not implemented.
- Runtime/UI/network consumers are not wired to consume the serialized public wall/terrain summaries yet.
- Godot exposes a full terrain map with default `"none"`; Unreal intentionally serializes sparse terrain with default `Normal`.

## Next Recommended Implementation Milestone

Add a deterministic revealed-marker public summary model once Unreal marker state exists, preserving hidden enemy marker identity and keeping marker summaries player-perspective scoped.

---

# Attack Declaration Legality Pass

## Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex
```

Result:

```text
Result: Succeeded
Total execution time: 55.54 seconds
```

## Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=201
succeededWithWarnings=0
failed=0
notRun=0
```

## Notes

- Added deterministic attack declaration legality and declaration application.
- Added `attack_declared` traces with source, target, tiles, and attacks-left before/after fields.
- Added attack action IDs using `attack:p{player}:u{source}:t{target}`.
- Legal action generation now preserves movement first, appends attacks, and keeps EndTurn last.
- Runtime selected attack results serialize selected action, trace, public board summary, and no MP roll consumption.
- Damage, response windows, counterattacks, card/passive exceptions, wands, terrain attack modifiers, and UI/VFX remain future work.

## New Tests Added

- `Wandbound.Core.AttackDeclaration.*`
- `Wandbound.Core.AttackLegalActionGeneration.*`
- Runtime result fixture coverage for `runtime_result_attack_declare_summary.json`

## Remaining Risks/Unknowns

- Friendly frozen attack/break-Frozen behavior is now covered by the attack damage resolution pass.
- Diagonal, ignore-wall, through-wall, ignore-unit, and passive-modified attacks remain out of scope.
- Neutral target behavior is currently treated as enemy targeting when the acting unit belongs to the active player.

---

# Attack Damage Resolution Pass

## Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex
```

Result:

```text
Result: Succeeded
Total execution time: 5.85 seconds
```

The first full build after adding the attack damage files also succeeded in 39.02 seconds.

## Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=215
succeededWithWarnings=0
failed=0
notRun=0
```

## Notes

- Added deterministic pending attack state staged by attack declaration.
- Added `WBEffectRunner::ApplyPendingAttackDamage`.
- Attack damage uses current non-negative attacker `ATK`, reduces defender HP, and clamps HP at zero.
- Frozen defenders break before damage, removing `Frozen` and applying no HP damage.
- Friendly attacks remain illegal except for the Godot-confirmed friendly Frozen break case.
- Added `attack_damage_resolved` traces, Frozen `status_removed` traces, and `damage_amount` trace serialization.
- Pending attack state stays out of public runtime summaries and is not generated as a player-selectable legal action.
- No `.uasset`, `.umap`, Blueprint, UI, VFX, response-window, counter, passive, wand, armor, prevention, death/removal, or terrain attack modifier work was added.

## New Tests Added

- `Wandbound.Core.AttackDamage.*`
- Fixture-driven coverage for attack damage success/failure cases under `Reference/GodotCanon/GoldenScenarios/`

## Remaining Risks/Unknowns

- Godot stores declaration-time attack damage in pending attack state; Unreal currently resolves from current attacker `ATK` because this pass kept pending attack data minimal.
- Armor, prevention, replacement, replacement-aware death handling, counterattacks, passives, wands, terrain attack modifiers, and response windows remain future work.
- Friendly Frozen targeting is implemented only for the narrow break-Frozen case verified in the Godot audit.

---

# Zero-HP Death Removal Pass

## Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex
```

Result:

```text
Result: Succeeded
Total execution time: 6.45 seconds
```

The first build for this pass failed on the `TArray<FWBUnitState*>::Sort` predicate signature in `WBEffectRunner.cpp`; the predicate was corrected and the final rebuild succeeded.

## Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=233
succeededWithWarnings=0
failed=0
notRun=0
```

## Notes

- Added deterministic zero-HP defeat/removal scaffolding.
- Removed units keep unit records but clear board position to `X = -1`, `Y = -1`.
- Removed units no longer occupy, block, move, attack, become targets, generate legal actions, or appear in public board summaries.
- Lethal attack damage now emits `attack_damage_resolved`, then zero-HP cleanup traces.
- Simple no-replacement Hero loss sets `bGameOver` and `WinnerPlayerId`.
- Runtime public turn summary JSON includes `winner_player_id`.
- No `.uasset`, `.umap`, Blueprint, UI, VFX, response, counter, passive, wand, prevention, replacement, death-trigger, card/effect, discard, draw, random dice, NPC phase, or 3D runtime work was added.

## New Tests Added

- `Wandbound.Core.ZeroHPDeathRemoval.*`
- Fixture-driven coverage for zero-HP cleanup and lethal attack cleanup under `Reference/GodotCanon/GoldenScenarios/`

## Remaining Risks/Unknowns

- Godot erases destroyed units from its `units` array; Unreal preserves records with defeated/removed flags for replay/debug safety.
- Hero loss is simple no-replacement loss only; Hybrid Hero replacement and death prevention remain future work.
- Discard movement, equipped wand fallout, death triggers, and on-destroy buffs remain future work.
