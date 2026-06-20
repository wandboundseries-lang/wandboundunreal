# Build/Test Report

Date of check: 2026-06-19 (America/New_York)

## Runtime Board View State Applier Pass

### Scope

This pass added a read-only visual component that caches and applies the latest public board summary to a board view actor.

Implemented:

- `UWBBoardViewStateApplierComponent`
- latest `FWBPublicBoardSummary` storage
- optional public-summary build from const `FWBGameStateData` through `WBBoardSummaryBridge`
- explicit and automatic apply-to-board-view behavior
- state applier tests for initial state, summary storage, input immutability, null actor safety, actor count refresh, state conversion parity, state immutability, apply-on-set behavior, hidden data exclusion, and no action-generation source calls

Not implemented:

- gameplay rules
- legal action generation
- state mutation
- player input
- tile picking
- unit selection
- UI, UMG, cards, response UI, hand UI, or action menus
- camera logic
- animations, VFX, or sound
- Blueprints
- `.uasset` or `.umap` edits
- asset imports
- CardDB import
- marker visuals or hidden marker identity

The component stores `FWBPublicBoardSummary` only and does not store full `FWBGameStateData`.

### Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex -NoHotReloadFromIDE
```

Result:

```text
Result: Succeeded
Total execution time: 15.06 seconds
```

### Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Final result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=348
succeededWithWarnings=0
failed=0
notRun=0
```

### New Tests Added

- `Wandbound.Runtime.BoardViewStateApplier.*`

### Notes

- No Godot reference files were edited.
- No `.uasset`, `.umap`, Blueprint, UMG, or asset import work was added.
- The pre-existing untracked `MaxHP` file remains untouched.

## Runtime Demo Harness Pass

### Scope

This pass added a tiny C++ manual verification harness for the runtime board scaffold.

Implemented:

- `AWBBoardViewDemoHarnessActor`
- optional assigned-board-view rendering of `WBBoardViewDemoData`
- optional world lookup for an existing `AWBBoardViewActor`
- demo harness tests for class existence, null actor safety, count refresh, default no-render behavior, no state dependency source guard, and hidden data exclusion

Not implemented:

- gameplay rules
- legal action generation
- state mutation
- player input
- tile picking
- unit selection
- UI, UMG, cards, response UI, hand UI, or action menus
- camera logic
- animations, VFX, or sound
- Blueprints
- `.uasset` or `.umap` edits
- asset imports
- CardDB import
- marker visuals or hidden marker identity

The harness renders `WBBoardViewDemoData::MakeSmallDemoBoardSummary()` through `WBBoardSummaryBridge::RenderSummaryToBoardView`.

### Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex -NoHotReloadFromIDE
```

Result:

```text
Result: Succeeded
Total execution time: 40.25 seconds
```

### Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Final result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=337
succeededWithWarnings=0
failed=0
notRun=0
```

### New Tests Added

- `Wandbound.Runtime.BoardViewDemoHarness.*`

### Notes

- No Godot reference files were edited.
- No `.uasset`, `.umap`, Blueprint, UMG, or asset import work was added.
- The pre-existing untracked `MaxHP` file remains untouched.

## Runtime Board Summary Bridge Pass

### Scope

This pass added the minimal runtime bridge from rules state/public summaries to the read-only board view actor.

Implemented:

- `WBBoardSummaryBridge`
- `FWBBoardViewRefreshResult`
- bridge tests for public-summary parity, null actor safety, no state mutation, actor count refresh, hidden data exclusion, and no action-generation source calls

Not implemented:

- gameplay rules
- legal action generation
- state mutation
- player input
- tile picking
- unit selection
- UI, UMG, cards, response UI, hand UI, or action menus
- camera logic
- animations, VFX, or sound
- Blueprints
- `.uasset` or `.umap` edits
- asset imports
- CardDB import
- marker visuals or hidden marker identity

The bridge accepts `const FWBGameStateData&` only to delegate to `WBPublicBoardSummary::Build`, then renders `FWBPublicBoardSummary` into `AWBBoardViewActor`.

### Build

Initial command:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex
```

Initial result:

```text
Unable to build while Live Coding is active.
Result: Failed (OtherCompilationError)
```

Successful command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex -NoHotReloadFromIDE
```

Final result:

```text
Result: Succeeded
Total execution time: 35.86 seconds
```

### Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Final result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=331
succeededWithWarnings=0
failed=0
notRun=0
```

### New Tests Added

- `Wandbound.Runtime.BoardSummaryBridge.*`

### Notes

- No Godot reference files were edited.
- No `.uasset`, `.umap`, Blueprint, UMG, or asset import work was added.
- The pre-existing untracked `MaxHP` file remains untouched.

## Runtime Visual Scaffold Pass

### Scope

This pass added the first read-only runtime visual scaffold for public board summaries.

Implemented:

- `WandboundRuntime` runtime module
- `WBBoardViewTypes` board-to-world coordinate helper
- `AWBBoardViewActor` with instanced mesh components for tiles, units, walls, and terrain
- `WBBoardViewDemoData` public-summary demo helper
- runtime coordinate and board-view actor automation coverage

Not implemented:

- gameplay rules
- legal action generation
- state mutation
- player input
- tile picking
- unit selection
- UI, UMG, cards, response UI, hand UI, or action menus
- camera logic
- animations, VFX, or sound
- Blueprints
- `.uasset` or `.umap` edits
- asset imports
- CardDB import
- marker visuals or hidden marker identity

The runtime visual layer consumes `FWBPublicBoardSummary` only and does not query or hold `FWBGameStateData`.

### Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex
```

Result:

```text
Result: Succeeded
Total execution time: 5.40 seconds
```

A previous build attempt failed only in the new coordinate tests because UE 5.7 `FVector` components are double-backed and the expected literals were float literals. The tests were updated to use double literals, and the final rebuild passed.

### Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Final result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=325
succeededWithWarnings=0
failed=0
notRun=0
```

### New Tests Added

- `Wandbound.Runtime.BoardViewCoordinates.*`
- `Wandbound.Runtime.BoardViewActor.*`

### Notes

- Mesh properties are editable and null by default.
- Null meshes skip instance creation and do not crash.
- Rendered counts report intended public-summary counts even when no meshes are assigned.
- No Godot reference files were edited.

## Status Effect Request Scaffolding Pass

### Scope

This pass added deterministic generic status-effect request scaffolding for Unreal-owned fixture data.

Implemented:

- `WBStatusEffect`
- generic status operations for apply, remove, duration set/add/reduce, cleanse one, and cleanse all
- canonical public aliases for Burn, Poison, Rooted, Stunned, and Frozen
- `WBEffectRunner::ApplyStatusEffect`
- `status_modified` traces plus status-effect `status_removed` child traces
- replay trace fields for `status_effect_operation` and `removed_statuses`
- `status_effect` payload dispatch through `FWBEffectRequest`
- fixture utility support for standalone `apply_status_effect` and effect-request `status_effect` payloads
- standalone status-effect GodotCanon fixtures
- mixed armor/status effect-request fixtures
- runtime public serialization coverage after status effect requests

Not implemented:

- Godot CardDB import
- JSON card database loading
- real card activation timing
- target selection UI
- player legal action generation for effects
- response windows, effect negation, passives, wands, or card-specific status behavior
- UI, Blueprint, VFX, audio, 3D runtime, `.uasset`, or `.umap` work

No Godot reference files were edited.

### Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex
```

Result:

```text
Result: Succeeded
Total execution time: 36.04 seconds
```

After fixing three test assertions, the incremental rebuild also succeeded:

```text
Result: Succeeded
Total execution time: 5.47 seconds
```

### Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Final result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=311
succeededWithWarnings=0
failed=0
notRun=0
```

The first automation run after implementation reported 308 succeeded and 3 failed. Those failures were assertion issues in new tests only:

- runtime status JSON helper expected statuses as strings instead of public status objects
- the alias canonicalization test used case-insensitive `FName` containment to prove stored casing

Both were corrected, and the final full Wandbound run passed.

### New Tests Added

- `Wandbound.Core.StatusEffectScaffolding.*`
- additional `Wandbound.Core.EffectRequestScaffolding.*` status payload coverage
- fixture-driven status effect and mixed armor/status request coverage under `Reference/GodotCanon/GoldenScenarios/`

### Notes

- Existing Burn, Poison, Rooted, Stunned, and Frozen tick behavior remains unchanged.
- Applying Rooted or Stunned through status effects changes legal actions only through existing status legality checks.
- Applying Burn through status effects does not change legal action generation.
- `WBActionCodec` action ids remain unchanged.
- Runtime/public serialization includes public status summaries and excludes hidden deck, hand, discard, source card id, source effect id, private choices, and pending attack internals.

### Remaining Risks/Unknowns

- Godot uses `-1` for some untimed status paths, while this Unreal pass keeps `Duration == 0` as the requested untimed convention.
- Godot-specific status application side effects such as Frozen status-consume behavior and poison reapply immediate ticks remain deferred to card-specific behavior/import work.
- CardDB import, target selection, response windows, negation, passives, wands, and card-specific status cards remain future work.

## Card Effect Request Scaffolding Pass

### Scope

This pass added deterministic card-effect request scaffolding for Unreal-owned fixture data.

Implemented:

- `FWBEffectRequest`
- source and target refs for generic effect requests
- `FWBGenericEffectPayload`
- `WBRules::CanApplyEffectRequest`
- `WBEffectRunner::ApplyEffectRequest`
- generic `armor_effect` payload dispatch to the existing armor-effect runner
- atomic multi-payload behavior through a working state copy
- `effect_request_resolved` parent traces
- fixture utility support for `operation = apply_effect_request`
- GodotCanon effect request fixtures
- `Wandbound.Core.EffectRequestScaffolding.*` tests

Not implemented:

- Godot CardDB import
- JSON card database loading
- real card activation timing
- target selection UI
- player legal action generation for effects
- response windows, effect negation, passives, wands, or card-specific effects
- UI, Blueprint, VFX, audio, 3D runtime, `.uasset`, or `.umap` work

No Godot reference files were edited.

### Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex
```

Result:

```text
Result: Succeeded
Total execution time: 52.75 seconds
```

### Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=289
succeededWithWarnings=0
failed=0
notRun=0
```

### New Tests Added

- `Wandbound.Core.EffectRequestScaffolding.ActionCodecUnchanged`
- `Wandbound.Core.EffectRequestScaffolding.ArmorAddCurrentSucceeds`
- `Wandbound.Core.EffectRequestScaffolding.ArmorAddMaxThenRestoreSucceeds`
- `Wandbound.Core.EffectRequestScaffolding.ArmorReduceCurrentSucceeds`
- `Wandbound.Core.EffectRequestScaffolding.FixtureScenarios`
- `Wandbound.Core.EffectRequestScaffolding.HiddenDataExcludedFromRuntimeSerialization`
- `Wandbound.Core.EffectRequestScaffolding.LegalGenerationUnchanged`
- `Wandbound.Core.EffectRequestScaffolding.MissingTargetFailsWithoutMutation`
- `Wandbound.Core.EffectRequestScaffolding.MultiplePayloadsAtomicOnFailure`
- `Wandbound.Core.EffectRequestScaffolding.ParentTracePrecedesArmorModified`
- `Wandbound.Core.EffectRequestScaffolding.RemovedTargetFailsWithoutMutation`
- `Wandbound.Core.EffectRequestScaffolding.RuntimeSerializationFixture`
- `Wandbound.Core.EffectRequestScaffolding.SourceUnitOptionalSucceeds`
- `Wandbound.Core.EffectRequestScaffolding.SourceUnitRemovedFailsIfProvided`
- `Wandbound.Core.EffectRequestScaffolding.UnknownOperationFailsWithoutMutation`

### Exact Errors

Final build and automation reported no errors.

### Risks/Unknowns

- Only `armor_effect` payload dispatch is supported.
- `SourceCardId` and `SourceEffectId` are fixture/debug metadata only and are not used for lookup.
- Effect requests are not generated as legal player actions.
- CardDB import, card activation timing, target selection, response windows, negation, passives, wands, and card-specific behavior remain future work.

### Next Recommended Implementation Milestone

Add deterministic effect request payload support for one more generic operation after auditing Godot behavior, likely status application or direct damage, still without importing CardDB or opening response windows.

---

## Armor Effect Scaffolding Pass

### Scope

This pass added deterministic generic armor effect scaffolding from audited Godot behavior.

Implemented:

- `WBArmorEffect`
- `EWBArmorEffectOp`
- `FWBArmorEffectRequest`
- `FWBArmorEffectResult`
- generic current/max armor operations
- `WBEffectRunner::ApplyArmorEffect`
- `armor_modified` traces
- trace JSON fields for armor effect operation/amount and max armor before/after
- fixture utility support for `operation = apply_armor_effect`
- GodotCanon armor effect fixtures
- `Wandbound.Core.ArmorEffectScaffolding.*` tests

Not implemented:

- card-specific armor cards
- card database import
- effect activation timing
- target selection UI
- responses, counters, passives, wands
- UI, Blueprint, VFX, audio, 3D runtime, `.uasset`, or `.umap` work

No Godot reference files were edited.

### Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex
```

Result:

```text
Result: Succeeded
Total execution time: 41.99 seconds
```

### Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=274
failed=0
warnings=0
errors=0
```

### New Tests Added

- `Wandbound.Core.ArmorEffectScaffolding.ActionCodecUnchanged`
- `Wandbound.Core.ArmorEffectScaffolding.AddCurrentClampsToMax`
- `Wandbound.Core.ArmorEffectScaffolding.AddCurrentMaxZeroStaysZero`
- `Wandbound.Core.ArmorEffectScaffolding.AddMaxDoesNotAutoFillCurrent`
- `Wandbound.Core.ArmorEffectScaffolding.EffectRunnerEmitsArmorModifiedTrace`
- `Wandbound.Core.ArmorEffectScaffolding.FixtureScenarios`
- `Wandbound.Core.ArmorEffectScaffolding.LegalGenerationUnchanged`
- `Wandbound.Core.ArmorEffectScaffolding.MissingTargetFailsWithoutMutation`
- `Wandbound.Core.ArmorEffectScaffolding.NegativeAmountFailsWithoutMutation`
- `Wandbound.Core.ArmorEffectScaffolding.PublicSummaryReflectsArmorChanges`
- `Wandbound.Core.ArmorEffectScaffolding.ReduceCurrentClampsToZero`
- `Wandbound.Core.ArmorEffectScaffolding.ReduceMaxClampsCurrent`
- `Wandbound.Core.ArmorEffectScaffolding.RemovedTargetFailsWithoutMutation`
- `Wandbound.Core.ArmorEffectScaffolding.RestoreArmorToMax`
- `Wandbound.Core.ArmorEffectScaffolding.RuntimeSerializationFixture`
- `Wandbound.Core.ArmorEffectScaffolding.SetCurrentClampsToMax`
- `Wandbound.Core.ArmorEffectScaffolding.SetMaxClampsCurrent`
- `Wandbound.Core.ArmorEffectScaffolding.UnknownOperationFailsWithoutMutation`

### Exact Errors

Final build and automation reported no errors.

### Risks/Unknowns

- `RestoreArmorToMax` is a generic Unreal operation; Godot `restore_armor` is amount-based and clamps up to max.
- Godot `restore_current_by_delta` / `also_restore_armor` behavior is documented but not exposed as a separate card-specific effect path in this pass.
- Armor-granting cards and card activation timing remain future work.

### Next Recommended Implementation Milestone

Add deterministic card-effect request scaffolding that can carry generic effect operations from Unreal-owned fixture data, without importing Godot CardDB or adding UI activation windows yet.

---

## Generic Armor Damage Handling Pass

### Scope

This pass implemented deterministic generic armor from Godot `current_armor` / `max_armor`.

Implemented:

- `FWBUnitState::CurrentArmor`
- `FWBUnitState::MaxArmor`
- armor normalization helpers
- non-bypass damage armor absorption
- bypass damage armor ignore
- attack damage as non-bypass armor damage
- Burn status damage as armor-bypass damage
- Poison unchanged as MaxHP status behavior
- armor trace fields
- public board summary armor fields
- runtime result JSON armor fields
- GodotCanon armor fixtures
- `Wandbound.Core.GenericArmor.*` tests

Not implemented:

- card-specific prevention/replacement
- armor-granting cards
- Oathchain, Backfill, Juno, Hybrid Hero replacement
- death triggers, discard movement, equipped wand fallout
- responses, counters, passives, wands
- UI, Blueprint, VFX, audio, 3D runtime, `.uasset`, or `.umap` work

No Godot reference files were edited.

### Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex
```

Result:

```text
Result: Succeeded
Total execution time: 62.12 seconds
```

### Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=256
failed=0
warnings=0
errors=0
```

### New Tests Added

- `Wandbound.Core.GenericArmor.ActionIdsUnchanged`
- `Wandbound.Core.GenericArmor.ArmorCannotGoNegative`
- `Wandbound.Core.GenericArmor.AttackAbsorbsAllDamage`
- `Wandbound.Core.GenericArmor.AttackNoArmorSameAsBefore`
- `Wandbound.Core.GenericArmor.AttackPartiallyAbsorbsDamage`
- `Wandbound.Core.GenericArmor.AttackTraceSerializesArmorFields`
- `Wandbound.Core.GenericArmor.BurnBypassesArmor`
- `Wandbound.Core.GenericArmor.FixtureScenarios`
- `Wandbound.Core.GenericArmor.HPClampsAndCleanupRemovesUnit`
- `Wandbound.Core.GenericArmor.LegalGenerationUnchanged`
- `Wandbound.Core.GenericArmor.PoisonDoesNotUseArmor`
- `Wandbound.Core.GenericArmor.PublicSummaryAndRuntimeJsonIncludeArmor`
- `Wandbound.Core.GenericArmor.RuntimeSerializationFixture`

### Exact Errors

Final build and automation reported no errors.

### Risks/Unknowns

- Burn can mark HP at or below zero in its trace, but status-tick zero-HP cleanup is still not composed into end-turn status ticks.
- `bypassed_armor` is serialized only when true, matching the existing optional false-flag trace style.
- Armor-granting and card-specific armor modification rules remain future work.
- Card-specific prevention/replacement remains future work.

### Next Recommended Implementation Milestone

Add deterministic armor-granting/effect-side armor modification scaffolding from card data only after auditing Godot card/effect sources, keeping generic damage resolution unchanged.

---

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

---

# Damage and Death Prevention Scaffolding Pass

## Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex
```

Result:

```text
Result: Succeeded
Total execution time: 24.81 seconds
```

The first build after adding the new resolver files also succeeded in 37.04 seconds.

## Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Final result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=242
succeededWithWarnings=0
failed=0
notRun=0
```

The first automation run found one test assertion issue in `Wandbound.Core.DamagePreventionScaffolding.AttackDamageUsesResolverTraceDefaults`: the test used brittle exact JSON substring checks for `final_damage_amount` and `prevented_damage_amount`. The assertion was changed to parse trace JSON structurally, and the final rerun passed.

## Notes

- Added `WBDamageResolution` as a deterministic damage-prevention and future armor seam.
- Added `WBDeathResolution` as a deterministic death-prevention and future replacement seam.
- Routed attack damage through the damage resolver.
- Zero-HP cleanup checks the default death-prevention seam before removal.
- Added trace fields for prevention defaults: `damage_prevented`, `prevented_damage_amount`, `final_damage_amount`, and `prevention_reason`.
- Normal attack damage, Frozen break, zero-HP cleanup, and simple Hero loss behavior remain unchanged.
- Godot audit confirmed generic armor state exists in Godot, but Unreal armor gameplay was not added in this pass.
- No `.uasset`, `.umap`, Blueprint, UI, VFX, response, counter, passive, wand, card-specific prevention, replacement, death-trigger, discard, draw, random dice, NPC phase, or 3D runtime work was added.

## New Tests Added

- `Wandbound.Core.DamagePreventionScaffolding.*`
- Fixture-driven coverage for the four new damage/death scaffolding scenarios under `Reference/GodotCanon/GoldenScenarios/`

## Remaining Risks/Unknowns

- Godot armor is real (`current_armor`/`max_armor`) but Unreal does not model active armor yet.
- Oathchain, Backfill, Juno, Hybrid Hero replacement, death triggers, equipped wand fallout, discard movement, responses, counters, passives, and wands remain future work.

---

# Runtime Visual Controller Shell Pass

## Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex -NoHotReloadFromIDE
```

First result:

```text
Result: Failed (OtherCompilationError)
```

The first build exposed pre-existing anonymous-namespace helper name collisions when Unreal grouped older `WandboundTests` files into a unity translation unit. `WBRuntimeVisualControllerShellTests.cpp` compiled separately; the failure was in existing test unity grouping. `WandboundTests.Build.cs` now sets `bUseUnity = false` so each test file keeps its private helpers private.

Final result:

```text
Result: Succeeded
Total execution time: 14.66 seconds
```

## Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=358
succeededWithWarnings=0
failed=0
notRun=0
```

## Notes

- Added `UWBRuntimeVisualControllerComponent`.
- The controller shell accepts `FWBPublicBoardSummary` directly.
- The controller shell accepts `FWBRuntimeSelectedActionResult` only to read `FinalPublicBoardSummary`.
- The component stores public refresh metadata only and does not own or mutate `FWBGameStateData`.
- No gameplay rules, legal action generation, input, UI, camera, animation, VFX, sound, Blueprints, UMG, `.uasset`, `.umap`, asset import, marker visuals, or hidden marker identity work was added.

## New Tests Added

- `Wandbound.Runtime.VisualController.*`

## Remaining Risks/Unknowns

- The visual controller shell is not wired to a real runtime selected-action producer yet.
- It refreshes placeholder board view counts only; real meshes, camera behavior, input, UI, VFX, marker visuals, and assets remain future work.
