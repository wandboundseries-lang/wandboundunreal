# Build/Test Report

Date of check: 2026-06-16 (America/New_York)

## Scope

This pass added deterministic selected-action runtime execution integration while preserving legal action generation, `FWBAction` replay, and `WBActionCodec` action IDs.

Implemented:

- selected-action runtime execution audit
- pure C++ `WBSelectedActionExecutor`
- `FWBSelectedActionExecutionContext`
- selected `Move` delegation to `WBEffectRunner::ApplyMove`
- selected `EndTurn` basic delegation to `WBEffectRunner::ApplyEndTurn`
- selected `EndTurn` full transition delegation through `WBTurnController::ApplyTurnCommand`
- selected `PassResponse` delegation to `WBEffectRunner::ApplyPassResponse`
- fixture operation `apply_selected_action` for selected-action tests only
- selected-action GoldenScenarios fixtures
- automation coverage for direct executor behavior, fixture scenarios, legal generation stability, action codec stability, and replay `apply_action` stability

Not implemented:

- new gameplay mechanics
- legal action generation changes
- full transition as a player legal action
- `WBActionCodec` command encoding
- replay `apply_action` behavior changes
- random dice generation
- draw
- NPC phase
- attacks
- cards/effects
- UI/Blueprint/3D runtime

No `.uasset`, `.umap`, Blueprint, or Godot project files were changed.

## Selected-Action Gateway Notes

New API:

```text
FWBSelectedActionExecutionContext
WBSelectedActionExecutor::ApplySelectedAction(State, Action, Context)
```

Selected action behavior:

- `Move` delegates to `WBEffectRunner::ApplyMove`.
- `EndTurn` with full transition disabled delegates to `WBEffectRunner::ApplyEndTurn`.
- `EndTurn` with full transition enabled builds an `FWBTurnCommand` and delegates to `WBTurnController::ApplyTurnCommand`.
- `PassResponse` delegates to `WBEffectRunner::ApplyPassResponse`.

Full-transition selected `EndTurn` requires `NextPlayerExplicitMPRoll` from `1` to `6`. Invalid or missing rolls fail without mutation and do not fall back to basic end turn.

`apply_action` replay fixtures still use `WBActionCodec` plus `WBEffectRunner::ApplyAction`.

`WBActionCodec` remains scoped to player legal actions and still encodes `EndTurn` as `end_turn:p0`.

Legal action generation remains unchanged:

- movement first
- then `EndTurn`
- `PassResponse` in response phase
- no full-transition player action

## Implemented This Pass

- Added `Docs/Selected_Action_Runtime_Execution_Audit.md`.
- Added `Docs/Selected_Action_Runtime_Execution_Report.md`.
- Added `Source/WandboundCore/Public/WBSelectedActionExecutor.h`.
- Added `Source/WandboundCore/Private/WBSelectedActionExecutor.cpp`.
- Added `Source/WandboundTests/Private/WBSelectedActionExecutorTests.cpp`.
- Added GodotCanon fixtures:
  - `selected_action_end_turn_basic.json`
  - `selected_action_end_turn_full_transition.json`
  - `selected_action_end_turn_full_transition_invalid_roll.json`
  - `selected_action_move_unchanged.json`
  - `selected_action_pass_response_unchanged.json`

## Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex
```

Result:

```text
Result: Succeeded
Total execution time: 65.09 seconds
```

## Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=121
succeededWithWarnings=0
failed=0
notRun=0
```

New tests added:

- `Wandbound.Core.SelectedAction.ActionCodecUnchanged`
- `Wandbound.Core.SelectedAction.EndTurnBasic`
- `Wandbound.Core.SelectedAction.EndTurnBasicIgnoresRoll`
- `Wandbound.Core.SelectedAction.EndTurnFullTransition`
- `Wandbound.Core.SelectedAction.EndTurnFullTransitionInvalidRoll`
- `Wandbound.Core.SelectedAction.EndTurnFullTransitionMissingRoll`
- `Wandbound.Core.SelectedAction.FixtureScenarios`
- `Wandbound.Core.SelectedAction.LegalGenerationUnchanged`
- `Wandbound.Core.SelectedAction.MoveDelegatesToApplyMove`
- `Wandbound.Core.SelectedAction.PassResponseDelegates`
- `Wandbound.Core.SelectedAction.ReplayApplyActionUnchanged`

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

Build and automation reported no warnings. `git diff --check` should still be used before commit; prior passes have only shown LF-to-CRLF notices.

## Risks/Unknowns

- UI/runtime code is not wired to `WBSelectedActionExecutor` yet.
- Full-transition selected `EndTurn` still requires an explicit deterministic MP roll.
- Random MP generation is intentionally absent.
- Network replay semantics for selected-action execution are not defined yet.
- Full death/prevention is still absent after status ticks.
- Draw, hooks, card triggers, NPC phase, attacks, and card effects are still absent.

## Next Recommended Implementation Milestone

Add a deterministic runtime-facing turn resolution adapter that supplies explicit MP rolls to `WBSelectedActionExecutor` from a test-controlled source, then proves selected EndTurn can be resolved end-to-end without changing legal action IDs or replay `apply_action` semantics.
