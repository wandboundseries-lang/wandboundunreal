# Build/Test Report

Date of check: 2026-06-17 (America/New_York)

## Scope

This pass added deterministic JSON serialization for `FWBRuntimeSelectedActionResult`.

Implemented:

- runtime result serialization audit
- `WBRuntimeResultSerializer`
- runtime selected-action result JSON object serialization
- runtime selected-action result JSON string serialization
- schema version 1
- public trace event JSON object helper reuse through `WBReplayTrace::TraceEventToJsonObject`
- public turn summary serialization
- hidden deck/hand/discard/card-id exclusion coverage
- additive fixture operation `apply_runtime_selected_action_with_result_and_serialize`
- runtime result serialization GoldenScenarios fixtures
- automation coverage for full EndTurn, basic EndTurn, Move, PassResponse, failure reason serialization, hidden data exclusion, action ID stability, existing behavior, JSON roundtrip, and fixture scenarios

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

## Serialization Notes

New serializer API:

```text
WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonObject(Result)
WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Result, OutJson)
```

Serialized fields:

- `schema_version`
- `selected_action.type`
- `selected_action.action_id`
- `result.ok`
- `result.reason`
- `mp_roll.consumed`
- `mp_roll.value`
- `traces`
- `final_public_turn_summary`

`SelectedActionId` comes from the existing envelope field, which is populated from `WBActionCodec::MakeActionId`.

The serializer does not include runtime context, deck, hand, discard, private card IDs, hidden markers, pending choices, UObject pointers, resources, or file paths.

Trace generation is unchanged. Runtime result serialization reuses `WBReplayTrace::TraceEventToJsonObject`.

Replay `apply_action` remains unchanged and still uses `WBActionCodec` plus `WBEffectRunner::ApplyAction`.

## Implemented This Pass

- Added `Docs/Runtime_Result_Serialization_Audit.md`.
- Added `Docs/Runtime_Result_Serialization_Report.md`.
- Added `Source/WandboundCore/Public/WBRuntimeResultSerializer.h`.
- Added `Source/WandboundCore/Private/WBRuntimeResultSerializer.cpp`.
- Updated `Source/WandboundCore/Public/WBReplayTrace.h`.
- Updated `Source/WandboundCore/Private/WBReplayTrace.cpp`.
- Updated `Source/WandboundTests/Private/WBReplayFixtureTestUtils.h`.
- Updated `Source/WandboundTests/Private/WBReplayFixtureTestUtils.cpp`.
- Added `Source/WandboundTests/Private/WBRuntimeResultSerializationTests.cpp`.
- Added GodotCanon fixtures:
  - `runtime_result_serialization_full_transition.json`
  - `runtime_result_serialization_basic_end_turn.json`
  - `runtime_result_serialization_hidden_data_exclusion.json`
- Updated `Docs/Wandbound_Unreal_Migration_Plan.md`.

## Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex
```

Result:

```text
Result: Succeeded
Total execution time: 29.47 seconds
```

## Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=152
succeededWithWarnings=0
failed=0
notRun=0
```

New tests added:

- `Wandbound.Core.RuntimeResultSerialization.ActionIdStability`
- `Wandbound.Core.RuntimeResultSerialization.BasicEndTurnSerializesNoRoll`
- `Wandbound.Core.RuntimeResultSerialization.ExistingBehaviorUnchanged`
- `Wandbound.Core.RuntimeResultSerialization.FailureSerializesReason`
- `Wandbound.Core.RuntimeResultSerialization.FixtureScenarios`
- `Wandbound.Core.RuntimeResultSerialization.FullEndTurnSerializesActionIdAndRoll`
- `Wandbound.Core.RuntimeResultSerialization.HiddenDataExcluded`
- `Wandbound.Core.RuntimeResultSerialization.JsonParseRoundtripSmoke`
- `Wandbound.Core.RuntimeResultSerialization.MoveSerializesFinalMP`
- `Wandbound.Core.RuntimeResultSerialization.PassResponseSerializes`

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

- No public board summary exists yet.
- Network replay envelope semantics are not defined yet.
- UI/runtime code is not wired to consume serialized runtime results yet.
- Random MP generation is intentionally absent.
- Draw, hooks, card triggers, NPC phase, attacks, and card effects are still absent.

## Next Recommended Implementation Milestone

Add a deterministic public board summary for runtime results, covering only public unit positions and public combat/status fields after explicitly auditing hidden marker and card identity policy.
