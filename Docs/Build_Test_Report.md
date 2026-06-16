# Build/Test Report

Date of check: 2026-06-16 (America/New_York)

## Scope

This pass implemented deterministic start-of-turn status tick scaffolding in `WandboundCore`.

Implemented:

- status duration metadata beside `FWBUnitState::Statuses`
- status helper APIs on `FWBUnitState`
- start-turn status tick validation
- start-turn status tick mutation in `WBEffectRunner`
- Poison MaxHP loss, HP clamp, duration decrement, and expiration
- Frozen pausing Poison tick/decay
- explicit status trace fields and serialization
- GodotCanon start-turn status fixtures
- automation coverage for direct C++ cases and fixture-driven cases

Not implemented:

- end-turn Burn damage
- end-turn Rooted/Stunned duration decay
- full death/prevention
- draw
- random MP generation
- card triggers
- NPC phase
- attacks/cards/effects/UI/3D runtime

No `.uasset`, `.umap`, Blueprint, or Godot project files were changed.

## Godot Status Model Notes

Godot status reference inspection found:

- `scripts/sim/game_state.gd` stores statuses in `statuses_by_unit`.
- status instances include timing and tick counters.
- `autoload/Game.gd:start_turn_impl` calls `state.tick_statuses("turn_start", current_player)`.
- current Godot `tick_statuses` ignores `active_player_id`, so this Unreal pass ticks all board units with start-turn statuses.
- `poison` ticks at `turn_start`.
- `burn`, `root`, and `stun` tick/decay at `turn_end`.
- Poison reduces MaxHP by one to a minimum of one and clamps current HP down to MaxHP.
- Frozen pauses Poison tick and duration decay.

Full audit notes are in `Docs/Status_Model_Audit.md`.

## Implemented This Pass

- Added `FWBUnitState::StatusTurnsRemaining`.
- Added status helpers:
  - `HasStatus`
  - `AddStatus`
  - `RemoveStatus`
  - `GetStatusTurnsRemaining`
  - `SetStatusTurnsRemaining`
  - `GetSortedStatusIdsForTrace`
- Preserved lowercase Godot aliases for existing fixtures:
  - `root` / `Rooted`
  - `stun` / `Stunned`
  - `burn` / `Burn`
  - `poison` / `Poison`
  - `frozen` / `Frozen`
- Updated movement blocking to use `FWBUnitState::HasStatus`.
- Added `WBRules::CanApplyStartOfTurnStatusTicks`.
- Added `WBEffectRunner::ApplyStartOfTurnStatusTicks`.
- Added status trace fields:
  - `status_id`
  - `target_unit_id`
  - `previous_hp`
  - `new_hp`
  - `previous_max_hp`
  - `new_max_hp`
  - `previous_status_turns`
  - `new_status_turns`
- Added trace event kinds:
  - `start_turn_status_ticks`
  - `status_tick`
  - `status_expired`
- Added GodotCanon fixtures:
  - `start_turn_poison_reduces_max_hp.json`
  - `start_turn_poison_clamps_hp_to_max.json`
  - `start_turn_burn_does_not_tick.json`
  - `start_turn_status_ticks_invalid_player_fails.json`
  - `start_turn_status_duration_expires.json`

## Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex
```

Result:

```text
Result: Succeeded
Total execution time: 17.27 seconds
```

## Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=74
succeededWithWarnings=0
failed=0
notRun=0
```

New tests added:

- `Wandbound.Core.StatusTicks.StatusHelperAliases`
- `Wandbound.Core.StatusTicks.PoisonReducesMaxHP`
- `Wandbound.Core.StatusTicks.PoisonKeepsHPUnderNewMax`
- `Wandbound.Core.StatusTicks.PoisonMinMaxHPIsOne`
- `Wandbound.Core.StatusTicks.BurnDoesNotTickAtStart`
- `Wandbound.Core.StatusTicks.AllBoardPoisonTicks`
- `Wandbound.Core.StatusTicks.PoisonPausedByFrozen`
- `Wandbound.Core.StatusTicks.PoisonDurationExpires`
- `Wandbound.Core.StatusTicks.RootedStunnedDoNotExpireAtStart`
- `Wandbound.Core.StatusTicks.InvalidPlayerFails`
- `Wandbound.Core.StatusTicks.GameOverFails`
- `Wandbound.Core.StatusTicks.FixtureScenarios`

## Generated File Tracking

Generated folders/artifacts remain untracked:

- `.vs/`
- `Binaries/`
- `Intermediate/`
- `Saved/`
- `DerivedDataCache/`
- common binary/log artifacts such as `*.pdb`, `*.obj`, `*.pch`, `*.ilk`, and `*.log`

## Remaining Warnings

`git diff --stat` reported only line-ending notices that Git will replace LF with CRLF on touched files. No build warnings or automation warnings were reported.

## Risks/Unknowns

- Godot currently ignores `active_player_id` during status ticks. If canon changes to active-player-only status ticks, Unreal fixtures and implementation should be updated together.
- `FWBUnitState::MPRemaining` remains a legacy fixture mirror from earlier movement migration work.
- End-turn status processing is still absent, so Burn and Rooted/Stunned duration decay are intentionally incomplete.
- Full death/prevention resolution is intentionally absent after Poison MaxHP changes.

## Next Recommended Implementation Milestone

Add deterministic end-of-turn status ticks for Burn and Rooted/Stunned duration decay, still without card triggers or full death/prevention.
