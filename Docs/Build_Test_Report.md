# Build/Test Report

Date of check: 2026-06-17 (America/New_York)

## Scope

This pass added deterministic public board summary reporting and serialization for runtime selected-action results.

Implemented:

- hidden-information audit for public unit card identity and marker identity policy
- `FWBPublicBoardSummary`, `FWBPublicUnitBoardSummary`, and `FWBPublicUnitStatusSummary`
- `WBPublicBoardSummary::Build`
- runtime selected-action result envelope field `FinalPublicBoardSummary`
- runtime result JSON field `final_public_board_summary`
- public unit position, owner, visible card identity, combat fields, action counters, and statuses
- deterministic public unit ordering by `Y`, then `X`, then `UnitId`
- deterministic public status ordering with canonical public status IDs
- fixture state support for public combat fields `atk`, `ar`, `rl_total`, and `rl_used`
- public board summary GoldenScenarios fixtures
- runtime result serialization fixtures for public board summary after move and full turn
- automation coverage for direct board summary building, runtime result serialization, hidden-data exclusion, fixture loading, failed-result summaries, and JSON field presence

Not implemented:

- new gameplay mechanics
- legal action generation changes
- wall or terrain public summary
- marker state or marker visibility summary
- player-perspective marker reveal policy in Unreal
- deck, hand, discard, pending choice, or private-card serialization
- network replay envelope semantics
- UI/Blueprint/3D runtime

No `.uasset`, `.umap`, Blueprint, or Godot project files were changed.

## Hidden-Information Policy

Visible board unit `CardId` is public and included in the public board summary.

Hidden marker card identity remains private. Unreal does not model markers yet, so marker summary fields were not added in this pass. When marker state is introduced, unrevealed enemy marker identity must remain excluded from public summaries.

Deck, hand, discard, pending choices, runtime context, UObject pointers, resources, and file paths remain excluded from runtime result serialization.

## Serialized Public Board Summary

Runtime selected-action result JSON now includes:

```text
final_public_board_summary
```

Serialized fields:

- `board_width`
- `board_height`
- `units`
- `units[].unit_id`
- `units[].owner_id`
- `units[].card_id`
- `units[].x`
- `units[].y`
- `units[].hp`
- `units[].max_hp`
- `units[].atk`
- `units[].ar`
- `units[].rl_total`
- `units[].rl_used`
- `units[].attacks_left`
- `units[].statuses`
- `units[].statuses[].status_id`
- `units[].statuses[].turns_remaining`

Status IDs are canonical public names such as `Burn`, `Poison`, `Rooted`, `Stunned`, and `Frozen`.

## Implemented This Pass

- Added `Docs/Public_Board_Summary_Hidden_Info_Audit.md`.
- Added `Docs/Public_Board_Summary_Report.md`.
- Added `Source/WandboundCore/Public/WBPublicBoardSummary.h`.
- Added `Source/WandboundCore/Private/WBPublicBoardSummary.cpp`.
- Updated `Source/WandboundCore/Public/WBRuntimeTurnResolutionAdapter.h`.
- Updated `Source/WandboundCore/Private/WBRuntimeTurnResolutionAdapter.cpp`.
- Updated `Source/WandboundCore/Private/WBRuntimeResultSerializer.cpp`.
- Updated `Source/WandboundTests/Private/WBReplayFixtureTestUtils.cpp`.
- Updated `Source/WandboundTests/Private/WBRuntimeResultSerializationTests.cpp`.
- Added `Source/WandboundTests/Private/WBPublicBoardSummaryTests.cpp`.
- Added `Source/WandboundTests/Private/WBRuntimeResultBoardSummarySerializationTests.cpp`.
- Updated `Reference/GodotCanon/GoldenScenarios/runtime_result_serialization_hidden_data_exclusion.json`.
- Added GodotCanon fixtures:
  - `public_board_summary_units_sorted.json`
  - `public_board_summary_statuses_sorted.json`
  - `public_board_summary_hidden_data_excluded.json`
  - `runtime_result_serialization_public_board_summary_after_move.json`
  - `runtime_result_serialization_public_board_summary_after_full_turn.json`
- Updated `Docs/Runtime_Result_Serialization_Report.md`.
- Updated `Docs/Wandbound_Unreal_Migration_Plan.md`.

## Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex
```

Result:

```text
Result: Succeeded
Total execution time: 7.38 seconds
```

## Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=166
succeededWithWarnings=0
failed=0
notRun=0
```

New tests added:

- `Wandbound.Core.PublicBoardSummary.FixtureScenarios`
- `Wandbound.Core.PublicBoardSummary.HiddenDataExcluded`
- `Wandbound.Core.PublicBoardSummary.IncludesCombatFields`
- `Wandbound.Core.PublicBoardSummary.IncludesStatuses`
- `Wandbound.Core.PublicBoardSummary.IncludesVisibleUnitPosition`
- `Wandbound.Core.PublicBoardSummary.StatusesSortedDeterministically`
- `Wandbound.Core.PublicBoardSummary.UnitsSortedDeterministically`
- `Wandbound.Core.PublicBoardSummary.VisibleUnitCardIdentityPolicy`
- `Wandbound.Core.RuntimeResultBoardSummary.AfterFullEndTurn`
- `Wandbound.Core.RuntimeResultBoardSummary.AfterMove`
- `Wandbound.Core.RuntimeResultBoardSummary.FailedInvalidRollSummarizesUnchangedBoard`
- `Wandbound.Core.RuntimeResultBoardSummary.FixtureScenarios`
- `Wandbound.Core.RuntimeResultBoardSummary.HiddenDataExcluded`
- `Wandbound.Core.RuntimeResultBoardSummary.SerializedJsonHasField`

## Exact Errors

Final build and automation reported no errors.

An initial automation run exposed an `FName` display-number serialization issue where status IDs emitted as values like `Burn_588`; this was fixed before the final successful run by canonicalizing status names through plain public strings.

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

`git diff --stat` reported only LF-to-CRLF working-copy notices for touched text files.

## Risks/Unknowns

- Unreal marker state does not exist yet, so hidden marker summary policy is documented but not implemented.
- Public wall and terrain summaries are not included yet.
- No player-perspective marker visibility summary exists yet.
- Runtime/UI/network consumers are not wired to consume the serialized public board summary yet.
- Discard visibility policy remains outside this board-summary pass.

## Next Recommended Implementation Milestone

Add deterministic public wall and terrain summaries to runtime results, preserving the current public board unit summary and keeping hidden marker identity out until marker state is modeled.
