# Build/Test Report

Date of check: 2026-06-16 (America/New_York)

## Scope

This pass implemented deterministic turn-start resource setup in `WandboundCore`: explicit MP roll application, player MP pool migration, attack reset, wall placement/removal allowance reset, turn/priority normalization, and trace coverage.

No attacks, cards/effects, summons, equipment/RL, random dice generation, draw, status ticks, start-of-turn triggers, NPC phase, 3D runtime, UI, Blueprints, `.uasset`, `.umap`, or Godot project files were changed.

## MP Model Audit

Godot reference inspection found that canonical runtime movement uses player-level MP:

- `scripts/sim/game_state.gd` stores `mp_pool`.
- `autoload/Game.gd:start_turn_impl` writes the MP roll into `state.mp_pool[current_player]`.
- `scripts/sim/rules.gd` movement application checks and decrements `gs.mp_pool[player_id]`.

Unreal movement has migrated to player `FWBPlayerStateData::RemainingMP` as the rules truth. `FWBUnitState::MPRemaining` remains only as a legacy fixture mirror while older movement/replay fixtures migrate.

Full audit notes are in `Docs/MP_Model_Audit.md`.

## Implemented This Pass

- Added player resource fields:
  - `LastMPRoll`
  - `WallRemovalsLeft`
- Added unit attack reset field:
  - `MaxAttacksPerTurn`
- Added state helpers:
  - `GetUnitsForPlayer`
  - `GetMutableUnitsForPlayer`
  - `ResetActionResourcesForPlayer`
  - `ApplyTurnStartResourceSetupForPlayer`
- Added rules validation:
  - `WBRules::CanApplyTurnStartResourceSetup`
- Added deterministic effect API:
  - `WBEffectRunner::ApplyTurnStartResourceSetup(State, PlayerId, ExplicitMPRoll)`
- Updated movement legality and spending to use player `RemainingMP`.
- Updated legal action generation through the movement legality path so no move actions are generated when player MP is `0`.
- Added trace fields for resource setup:
  - `mp_roll`
  - `remaining_mp`
  - `walls_left`
  - `wall_removals_left`
- Added `turn_start_resource_setup` trace events.
- Added GodotCanon turn-start fixtures:
  - `turn_start_resource_setup_roll_4.json`
  - `turn_start_resource_setup_invalid_roll_fails.json`
  - `turn_start_resource_setup_resets_attacks_and_walls.json`
- Updated `replay_multi_unit_order_end_turn.json` so its second decision legal actions match player-pool MP behavior.

## Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex
```

Final result:

```text
Result: Succeeded
Total execution time: 3.65 seconds
```

An earlier build in this pass also succeeded after the initial implementation:

```text
Result: Succeeded
Total execution time: 13.87 seconds
```

## Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Final result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=62
succeededWithWarnings=0
failed=0
notRun=0
```

New tests added:

- `Wandbound.Core.TurnStart.ValidRollAppliesMP`
- `Wandbound.Core.TurnStart.InvalidLowRollFails`
- `Wandbound.Core.TurnStart.InvalidHighRollFails`
- `Wandbound.Core.TurnStart.WallsReset`
- `Wandbound.Core.TurnStart.AttacksReset`
- `Wandbound.Core.TurnStart.MovementUsesResetMP`
- `Wandbound.Core.TurnStart.FailedSetupDoesNotMutate`
- `Wandbound.Core.TurnStart.NoMovesWhenNoMP`
- `Wandbound.Core.TurnStart.FixtureScenarios`

## Generated File Tracking

Generated folders/artifacts remain untracked:

- `.vs/`
- `Binaries/`
- `Intermediate/`
- `Saved/`
- `DerivedDataCache/`
- common binary/log artifacts such as `*.pdb`, `*.obj`, `*.pch`, `*.ilk`, and `*.log`

`git ls-files` returned no tracked files for those generated paths/patterns.

## Remaining Warnings

`git diff --check` reported only line-ending notices that Git will replace LF with CRLF on touched files. No whitespace errors, build warnings, or automation warnings were reported.

## Risks/Unknowns

- `FWBUnitState::MPRemaining` is still decremented as a legacy fixture mirror. It should be removed from fixture assertions after all movement/replay fixtures migrate to player `remaining_mp`.
- Wall removal reset currently follows the observed Godot baseline of `TurnNumber >= 30`; future mode-specific policy may need a dedicated rule field.
- MP modifiers, random roll generation, draw, status ticks, start-of-turn hooks, and NPC phase are intentionally not implemented yet.
- `Docs/Codex_Project_Audit_Report.md` may still describe earlier pre-module project state.

## Next Recommended Implementation Milestone

Add deterministic start-of-turn status tick scaffolding using explicit fixture data, with no card triggers or draw yet.
