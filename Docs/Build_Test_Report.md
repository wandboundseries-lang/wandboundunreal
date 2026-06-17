# Build/Test Report

Date of check: 2026-06-16 (America/New_York)

## Scope

This pass added a deterministic runtime-facing turn resolution adapter that supplies explicit MP rolls to selected `EndTurn` execution through a test-controlled roll source.

Implemented:

- runtime turn resolution adapter audit
- `IWBMPRollSource`
- `FWBFixedMPRollSource`
- `FWBQueuedMPRollSource`
- `FWBRuntimeTurnResolutionContext`
- `WBRuntimeTurnResolutionAdapter`
- additive fixture support for `apply_runtime_selected_action`
- runtime-selected action GoldenScenarios fixtures
- automation coverage for fixed rolls, queued roll consumption, missing/invalid roll failures, legal generation stability, action codec stability, and replay `apply_action` stability

Not implemented:

- random dice generation
- draw
- NPC phase
- attacks
- cards/effects
- full death/prevention
- UI/Blueprint/3D runtime
- legal action generation changes
- full transition as a player legal action
- `WBActionCodec` action ID changes
- replay `apply_action` behavior changes

No `.uasset`, `.umap`, Blueprint, or Godot project files were changed.

## Runtime Adapter Notes

New roll source API:

```text
IWBMPRollSource
FWBFixedMPRollSource
FWBQueuedMPRollSource
```

New runtime adapter API:

```text
FWBRuntimeTurnResolutionContext
WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedAction(State, SelectedAction, Context)
```

Runtime-selected behavior:

- `Move` delegates through `WBSelectedActionExecutor` and does not require or consume a roll.
- `PassResponse` delegates through `WBSelectedActionExecutor` and does not require or consume a roll.
- `EndTurn` with full transition disabled stays basic.
- `EndTurn` with full transition enabled requires a deterministic roll source and delegates through `WBSelectedActionExecutor`.

Roll source behavior:

- fixed roll source returns the configured roll if it is `1..6`
- queued roll source consumes exactly one valid roll for full selected `EndTurn`
- empty queue fails with `mp_roll_queue_empty`
- invalid roll fails with `invalid_mp_roll`
- invalid queued roll is not popped

`apply_action` replay fixtures still use `WBActionCodec` plus `WBEffectRunner::ApplyAction`.

`WBActionCodec` remains scoped to player legal actions and still encodes `EndTurn` as `end_turn:p0`.

Legal action generation remains unchanged:

- movement first
- then `EndTurn`
- `PassResponse` in response phase
- no full-transition or roll player action

## Implemented This Pass

- Added `Docs/Runtime_Turn_Resolution_Adapter_Audit.md`.
- Added `Docs/Runtime_Turn_Resolution_Adapter_Report.md`.
- Added `Source/WandboundCore/Public/WBMPRollSource.h`.
- Added `Source/WandboundCore/Private/WBMPRollSource.cpp`.
- Added `Source/WandboundCore/Public/WBRuntimeTurnResolutionAdapter.h`.
- Added `Source/WandboundCore/Private/WBRuntimeTurnResolutionAdapter.cpp`.
- Updated `Source/WandboundTests/Private/WBReplayFixtureTestUtils.h`.
- Updated `Source/WandboundTests/Private/WBReplayFixtureTestUtils.cpp`.
- Added `Source/WandboundTests/Private/WBRuntimeTurnResolutionAdapterTests.cpp`.
- Added GodotCanon fixtures:
  - `runtime_selected_end_turn_full_transition_roll_4.json`
  - `runtime_selected_end_turn_basic_no_roll.json`
  - `runtime_selected_end_turn_missing_roll_source_fails.json`
  - `runtime_selected_end_turn_invalid_roll_no_mutation.json`
  - `runtime_selected_move_does_not_consume_roll.json`
  - `runtime_selected_pass_response_does_not_consume_roll.json`

## Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex
```

Result:

```text
Result: Succeeded
Total execution time: 37.86 seconds
```

## Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=133
succeededWithWarnings=0
failed=0
notRun=0
```

New tests added:

- `Wandbound.Core.RuntimeTurnResolution.ActionCodecUnchanged`
- `Wandbound.Core.RuntimeTurnResolution.BasicEndTurnNoRoll`
- `Wandbound.Core.RuntimeTurnResolution.FixtureScenarios`
- `Wandbound.Core.RuntimeTurnResolution.FullEndTurnFixedRoll`
- `Wandbound.Core.RuntimeTurnResolution.InvalidFixedRollFails`
- `Wandbound.Core.RuntimeTurnResolution.LegalGenerationUnchanged`
- `Wandbound.Core.RuntimeTurnResolution.MissingRollSourceFails`
- `Wandbound.Core.RuntimeTurnResolution.MoveDoesNotRequireRollSource`
- `Wandbound.Core.RuntimeTurnResolution.PassResponseDoesNotConsumeRoll`
- `Wandbound.Core.RuntimeTurnResolution.QueuedInvalidRollDoesNotPop`
- `Wandbound.Core.RuntimeTurnResolution.QueuedRollConsumedByFullEndTurnOnly`
- `Wandbound.Core.RuntimeTurnResolution.ReplayApplyActionUnchanged`

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

- UI/runtime code is not wired to `WBRuntimeTurnResolutionAdapter` yet.
- Random MP generation is intentionally absent.
- Network replay semantics for runtime selected-action execution are not defined yet.
- Full death/prevention is still absent after status ticks.
- Draw, hooks, card triggers, NPC phase, attacks, and card effects are still absent.

## Next Recommended Implementation Milestone

Add a deterministic runtime turn-result envelope that reports the selected action ID, consumed MP roll, emitted traces, and final public turn summary without changing replay `apply_action` semantics or legal action generation.
