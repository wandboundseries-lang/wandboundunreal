# Build/Test Report

Date of check: 2026-06-17 (America/New_York)

## Scope

This pass added a deterministic runtime turn-result envelope around selected-action execution. It reports selected action identity, consumed MP roll details, emitted traces through the existing apply result, and a final public turn summary.

Implemented:

- runtime turn result envelope audit
- `FWBPublicPlayerTurnSummary`
- `FWBPublicTurnSummary`
- `WBPublicTurnSummary::Build`
- `FWBRuntimeSelectedActionResult`
- `WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult`
- behavior-compatible `ApplyRuntimeSelectedAction` refactor through the envelope API
- additive fixture support for `apply_runtime_selected_action_with_result`
- runtime result GoldenScenarios fixtures
- automation coverage for full EndTurn, basic EndTurn, Move, PassResponse, missing/invalid roll failures, action ID stability, legacy adapter behavior, and fixture scenarios

Not implemented:

- new gameplay mechanics
- legal action generation changes
- full transition as a player legal action
- `WBActionCodec` action ID changes
- replay `apply_action` behavior changes
- random dice generation
- draw
- NPC phase
- attacks
- cards/effects
- full death/prevention
- UI/Blueprint/3D runtime

No `.uasset`, `.umap`, Blueprint, or Godot project files were changed.

## Result Envelope Notes

New public summary API:

```text
FWBPublicPlayerTurnSummary
FWBPublicTurnSummary
WBPublicTurnSummary::Build
```

New runtime envelope API:

```text
FWBRuntimeSelectedActionResult
WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult(State, SelectedAction, Context)
```

Envelope fields:

- `ApplyResult`
- `SelectedActionType`
- `SelectedActionId`
- `bConsumedMPRoll`
- `ConsumedMPRoll`
- `FinalPublicTurnSummary`

`SelectedActionId` uses the existing `WBActionCodec::MakeActionId` format. Runtime context and MP rolls are not encoded into action IDs.

The final public turn summary includes only turn and player resource fields. It does not include deck, hand, hidden choices, private card identities, or full hidden unit/card state.

Trace behavior is unchanged. The envelope reports traces through `ApplyResult.TraceEvents` and does not add wrapper traces.

`apply_action` replay fixtures still use `WBActionCodec` plus `WBEffectRunner::ApplyAction`.

Legal action generation remains unchanged:

- movement first
- then `EndTurn`
- `PassResponse` in response phase
- no full-transition or roll player action

## Implemented This Pass

- Added `Docs/Runtime_Turn_Result_Envelope_Audit.md`.
- Added `Docs/Runtime_Turn_Result_Envelope_Report.md`.
- Added `Source/WandboundCore/Public/WBPublicTurnSummary.h`.
- Added `Source/WandboundCore/Private/WBPublicTurnSummary.cpp`.
- Updated `Source/WandboundCore/Public/WBRuntimeTurnResolutionAdapter.h`.
- Updated `Source/WandboundCore/Private/WBRuntimeTurnResolutionAdapter.cpp`.
- Updated `Source/WandboundTests/Private/WBReplayFixtureTestUtils.h`.
- Updated `Source/WandboundTests/Private/WBReplayFixtureTestUtils.cpp`.
- Added `Source/WandboundTests/Private/WBRuntimeTurnResultEnvelopeTests.cpp`.
- Added GodotCanon fixtures:
  - `runtime_result_end_turn_full_transition_roll_4.json`
  - `runtime_result_end_turn_basic_no_roll.json`
  - `runtime_result_move_summary.json`
  - `runtime_result_pass_response_summary.json`
  - `runtime_result_invalid_roll_no_mutation.json`

## Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex
```

Result:

```text
Result: Succeeded
Total execution time: 30.06 seconds
```

## Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=142
succeededWithWarnings=0
failed=0
notRun=0
```

New tests added:

- `Wandbound.Core.RuntimeTurnResultEnvelope.BasicEndTurnReportsNoRoll`
- `Wandbound.Core.RuntimeTurnResultEnvelope.FixtureScenarios`
- `Wandbound.Core.RuntimeTurnResultEnvelope.FullEndTurnReportsActionIdAndRoll`
- `Wandbound.Core.RuntimeTurnResultEnvelope.InvalidQueuedRollFailure`
- `Wandbound.Core.RuntimeTurnResultEnvelope.LegacyAdapterStillWorks`
- `Wandbound.Core.RuntimeTurnResultEnvelope.MissingRollSourceFailure`
- `Wandbound.Core.RuntimeTurnResultEnvelope.MoveReportsNoRollAndFinalMP`
- `Wandbound.Core.RuntimeTurnResultEnvelope.PassResponseReportsNoRoll`
- `Wandbound.Core.RuntimeTurnResultEnvelope.SelectedActionIdHasNoRuntimeContext`

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

- UI/runtime code is not wired to consume the result envelope yet.
- Random MP generation is intentionally absent.
- Network replay semantics for runtime selected-action execution are not defined yet.
- Full death/prevention is still absent after status ticks.
- Draw, hooks, card triggers, NPC phase, attacks, and card effects are still absent.

## Next Recommended Implementation Milestone

Add deterministic runtime result serialization for `FWBRuntimeSelectedActionResult`, covering selected action id, consumed roll fields, trace events, and final public turn summary without including hidden card data.
