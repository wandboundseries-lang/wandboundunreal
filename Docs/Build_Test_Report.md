# Build/Test Report

Date of check: 2026-06-16 (America/New_York)

## Scope

This pass implemented deterministic end-of-turn status tick scaffolding in `WandboundCore`.

Implemented:

- end-turn status tick validation
- end-turn status tick mutation in `WBEffectRunner`
- Burn end-turn HP damage
- Burn timed duration decrement and expiration
- Rooted, Stunned, and Frozen timed duration decrement and expiration
- Poison intentionally not ticking or decaying at end turn
- optional trace flags for expired statuses and zero-HP Burn results
- GodotCanon end-turn status fixtures
- automation coverage for direct C++ cases and fixture-driven cases

Not implemented:

- full death/prevention
- unit removal from Burn damage
- end-turn card triggers
- NPC phase
- response windows
- card effects
- attacks/cards/effects/UI/3D runtime

No `.uasset`, `.umap`, Blueprint, or Godot project files were changed.

## Godot End-Turn Status Model Notes

Godot status reference inspection found:

- `autoload/Game.gd:end_turn_impl` calls `state.tick_statuses("turn_end", current_player)`.
- current Godot `tick_statuses` ignores `active_player_id`, so this Unreal pass ticks all board units with end-turn statuses.
- `burn`, `root`, `stun`, and `frozen` use `turn_end` timing.
- Burn calls `apply_damage(unit_id, 1, "burn")`.
- Burn damage bypasses armor in Godot.
- Burn, Root, Stun, and Frozen durations decay at end turn.
- Poison does not tick or decay at end turn.
- Godot clamps HP to zero and then may remove units through later death/prevention handling; this Unreal pass does not implement that death/prevention layer.

Full audit notes are in `Docs/Status_Model_Audit.md`.

## Implemented This Pass

- Added `WBRules::CanApplyEndOfTurnStatusTicks`.
- Added `WBEffectRunner::ApplyEndOfTurnStatusTicks`.
- Added `end_turn_status_ticks` parent trace events.
- Added Burn `status_tick` trace events.
- Added status expiration trace flag:
  - `expired_status`
- Added zero-HP trace flag:
  - `at_or_below_zero_hp`
- Added GodotCanon fixtures:
  - `end_turn_burn_deals_damage.json`
  - `end_turn_burn_duration_expires.json`
  - `end_turn_poison_does_not_tick.json`
  - `end_turn_rooted_duration_expires.json`
  - `end_turn_stunned_duration_expires.json`
  - `end_turn_status_ticks_invalid_player_fails.json`
  - `end_turn_frozen_duration_expires.json`

## Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex
```

Result:

```text
Result: Succeeded
Total execution time: 18.44 seconds
```

## Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=86
succeededWithWarnings=0
failed=0
notRun=0
```

New tests added:

- `Wandbound.Core.EndTurnStatusTicks.BurnDealsDamage`
- `Wandbound.Core.EndTurnStatusTicks.BurnCanDropHPToZero`
- `Wandbound.Core.EndTurnStatusTicks.BurnDurationExpires`
- `Wandbound.Core.EndTurnStatusTicks.PoisonDoesNotTick`
- `Wandbound.Core.EndTurnStatusTicks.RootedDurationExpires`
- `Wandbound.Core.EndTurnStatusTicks.StunnedDurationExpires`
- `Wandbound.Core.EndTurnStatusTicks.PermanentStatusesDoNotExpire`
- `Wandbound.Core.EndTurnStatusTicks.FrozenDurationExpires`
- `Wandbound.Core.EndTurnStatusTicks.AllBoardBurnTicks`
- `Wandbound.Core.EndTurnStatusTicks.InvalidPlayerFails`
- `Wandbound.Core.EndTurnStatusTicks.GameOverFails`
- `Wandbound.Core.EndTurnStatusTicks.FixtureScenarios`

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
- End-turn status ticks are not yet integrated into `WBEffectRunner::ApplyEndTurn`; they remain a separate deterministic phase operation until replay/turn orchestration is extended.
- Full death/prevention and unit removal remain intentionally absent after Burn HP reaches zero.
- Burn-triggered passive effects are intentionally absent from this scaffolding pass.

## Next Recommended Implementation Milestone

Add deterministic turn orchestration that composes end-turn status ticks, turn advancement, start-turn status ticks, and explicit resource setup without changing player action semantics or adding card triggers yet.
