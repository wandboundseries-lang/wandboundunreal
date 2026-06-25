# Build/Test Report

Date of check: 2026-06-24 (America/New_York)

## Card Activation Affordability Query Pass

### Scope

This pass added read-only card activation affordability query scaffolding that computes external RR/RL affordability context for source gates.

Implemented:

- `FWBCardActivationAffordabilityRequest`
- `FWBCardActivationAffordabilityResult`
- `IWBCardActivationAffordabilityProvider`
- `FWBFixedCardActivationAffordabilityProvider`
- `WBCardActivationAffordability::QueryFromUnitRL`
- `WBCardActivationAffordability::ApplyResultToSourceGateContext`
- `WBCardActivationAffordability::BuildCandidateSourceWithAffordability`
- per-effect source-gate contexts on `FWBCardActivationCandidateSource`
- fixture operation `query_card_activation_affordability`
- fixture parser support for `effect_source_gate_contexts`

Not implemented:

- cost payment
- `RLUsed` mutation
- overflow or wand destruction
- production CardDB import
- production card zones
- activation `FWBAction`
- `WBActionCodec` activation ids
- response windows
- effect negation
- passives
- wands
- card-specific behavior
- UI, Blueprints, `.uasset`, `.umap`, VFX, audio, or 3D runtime work

### Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex -NoHotReloadFromIDE
```

Final result:

```text
Result: Succeeded
Total execution time: 126.26 seconds
```

### Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Final result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=565
succeededWithWarnings=0
failed=0
notRun=0
```

### New Tests Added

- `Wandbound.Core.CardActivationAffordability.QueryFromUnitRL`
- `Wandbound.Core.CardActivationAffordability.ContextPreparationAndCandidateFiltering`
- `Wandbound.Core.CardActivationAffordability.FixedProviderAndPerEffectContexts`
- `Wandbound.Core.CardActivationAffordability.FixtureScenarios`
- `Wandbound.Core.CardActivationAffordability.SeparateFromPaymentLegalActionsEffectRunnerAndCodec`

### New Golden Scenarios Added

- `card_activation_affordability_query_affordable.json`
- `card_activation_affordability_query_unaffordable.json`
- `card_activation_affordability_query_zero_rr.json`
- `card_activation_affordability_query_missing_source_fails.json`
- `card_activation_affordability_query_removed_source_fails.json`
- `card_activation_affordability_query_owner_mismatch_fails.json`
- `card_activation_affordability_context_feeds_cost_gate_success.json`
- `card_activation_affordability_context_feeds_cost_gate_excluded.json`
- `card_activation_affordability_does_not_mutate_rl.json`
- `card_activation_affordability_per_effect_costs_if_supported.json`

### Exact Errors

Final build and automation reported no errors.

### Notes

- `QueryFromUnitRL` treats current Unreal `RLTotal` as current RL until a base/current RL split exists.
- Available RL is computed as `max(0, RLTotal - RLUsed)`.
- Successful queries use `bOk = true` even when `bAffordable = false`.
- Affordability results can prepare source-gate cost context without payment.
- Candidate sources can now carry effect-specific source-gate contexts for different per-effect RR costs.
- Legal action generation and `WBActionCodec` remain unchanged.
- `git diff --check` reported no whitespace errors, only LF-to-CRLF working-copy notices.
- Pre-existing untracked `MaxHP` remains untouched.

### Risks / Unknowns

- Production affordability ownership is still future work.
- Actual RR/RL payment, `RLUsed` mutation, overflow, equipped wand fallout, and CardDB import remain deferred.
- Response windows, effect negation, passives, wands, and card-specific costs remain future work.

## Card Activation Cost Gates Pass

### Scope

This pass added fixture-driven card activation cost-gate scaffolding using externally supplied RR/RL affordability only.

Implemented:

- `FWBCardActivationCostGateDefinition`
- `FWBCardActivationCostGateContext`
- detailed cost-gate evaluation in `WBCardActivationSourceGate::Evaluate`
- fixture parser support for `source_gate.cost_gate`
- fixture parser support for `source_gate_context.cost_context`
- candidate filtering through the existing source-gate path
- fixture assertions that generation does not mutate RL/RLUsed
- source guards confirming candidate/source-gate layers do not call `WBEffectRunner`, `WBRules::GenerateLegalActions`, or `WBActionCodec`

Not implemented:

- real RR/RL payment
- `RLUsed` mutation
- overflow or wand destruction
- production card zones
- CardDB import
- activation `FWBAction`
- `WBActionCodec` activation ids
- response windows
- effect negation
- passives
- wands
- card-specific behavior
- UI, Blueprints, `.uasset`, `.umap`, VFX, audio, or 3D runtime work

### Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex -NoHotReloadFromIDE
```

Final result:

```text
Result: Succeeded
Total execution time: 61.63 seconds
```

### Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Final result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=560
succeededWithWarnings=0
failed=0
notRun=0
```

### New Tests Added

- `Wandbound.Core.CardActivationCostGate.EvaluationReasons`
- `Wandbound.Core.CardActivationCostGate.SimpleAndDetailedBothRequired`
- `Wandbound.Core.CardActivationCostGate.CandidateFilteringAndOrder`
- `Wandbound.Core.CardActivationCostGate.NoPaymentMutationAndOnceUsage`
- `Wandbound.Core.CardActivationCostGate.FixtureScenarios`
- `Wandbound.Core.CardActivationCostGate.SeparateFromLegalActionsEffectRunnerAndCodec`

### New Golden Scenarios Added

- `card_activation_cost_gate_affordable_success.json`
- `card_activation_cost_gate_missing_affordability_excluded.json`
- `card_activation_cost_gate_not_affordable_excluded.json`
- `card_activation_cost_gate_requirement_mismatch_excluded.json`
- `card_activation_cost_gate_available_rl_insufficient_excluded.json`
- `card_activation_cost_gate_zero_rr_success.json`
- `card_activation_cost_gate_simple_flag_and_detailed_gate_both_required.json`
- `card_activation_cost_gate_no_payment_mutation.json`
- `card_activation_cost_gate_order_preserved.json`
- `card_activation_cost_gate_with_once_per_turn_usage.json`

### Exact Errors

Final build and automation reported no errors.

### Notes

- External affordability is fixture-owned; the source gate does not compute real payment.
- `RequiredRR`, `SuppliedRequiredRR`, and `SuppliedAvailableRL` must be non-negative.
- If supplied available RL is lower than supplied required RR, the gate fails with `cost_not_affordable` even when the external boolean says affordable.
- The legacy `bRequiresCostsSatisfiedExternally` flag remains supported and composes with detailed cost gates.
- Candidate generation remains read-only and does not mark usage or mutate RL/RLUsed.
- Legal action generation and `WBActionCodec` remain unchanged.
- Pre-existing untracked `MaxHP` remains untouched.

### Risks / Unknowns

- Production affordability ownership is still future work.
- Actual RR/RL payment, `RLUsed` mutation, overflow, equipped wand fallout, and CardDB import remain deferred.
- Response windows, effect negation, passives, wands, and card-specific costs remain future work.

## Card Activation Command Scaffolding Pass

### Scope

This pass added deterministic card activation command scaffolding that consumes externally supplied `FWBEffectRequest` data and delegates mutation to the existing effect request dispatcher.

Implemented:

- `FWBCardActivationSource`
- `FWBCardActivationCommand`
- `FWBCardActivationCommandResult`
- `WBRules::CanApplyCardActivationCommand`
- `WBEffectRunner::ApplyCardActivationCommand`
- `card_activation_resolved` parent traces
- fixture operation `apply_card_activation_command`
- GodotCanon card activation command fixtures for armor, status, damage, heal, mixed atomicity, source failures, missing target, and hidden metadata exclusion

Not implemented:

- Godot CardDB import
- card activation legal action generation
- card ownership/cost/timing validation
- UI target selection
- response windows
- effect negation
- passives
- wands
- card-specific behavior
- Blueprints, `.uasset`, `.umap`, VFX, audio, or 3D runtime work

### Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex -NoHotReloadFromIDE
```

Final result:

```text
Result: Succeeded
Total execution time: 43.59 seconds
```

### Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Final result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=517
succeededWithWarnings=0
failed=0
notRun=0
```

### New Tests Added

- `Wandbound.Core.CardActivationCommandScaffolding.ValidationFillsSourceAndAllowsGlobal`
- `Wandbound.Core.CardActivationCommandScaffolding.SourceFailuresAndMismatchNoMutation`
- `Wandbound.Core.CardActivationCommandScaffolding.EffectFamiliesSucceed`
- `Wandbound.Core.CardActivationCommandScaffolding.MixedPayloadAtomicity`
- `Wandbound.Core.CardActivationCommandScaffolding.ParentTraceBeforeEffectTraces`
- `Wandbound.Core.CardActivationCommandScaffolding.HiddenDataExcludedFromTraceAndRuntimeSerialization`
- `Wandbound.Core.CardActivationCommandScaffolding.LegalGenerationAndActionCodecUnchanged`
- `Wandbound.Core.CardActivationCommandScaffolding.FixtureScenarios`
- `Wandbound.Core.CardActivationCommandScaffolding.RuntimeSerializationFixture`

### Exact Errors

Final build and automation reported no errors.

### Notes

- `card_activation_resolved` is emitted before `effect_request_resolved` and child payload traces.
- Missing effect-request source fields are filled from command source metadata before effect request dispatch.
- Source-less/global fixture commands are allowed with a valid player and `SourceUnitId == -1`.
- Source card id, source effect id, debug activation id, and hidden deck/hand/discard data remain excluded from traces and public runtime serialization.
- Legal action generation and `WBActionCodec` action IDs are unchanged.
- `git diff --check` reported no whitespace errors, only LF-to-CRLF working-copy notices.
- Pre-existing untracked `MaxHP` remains untouched.

### Risks / Unknowns

- Card activation commands are fixture/runtime scaffolding only; player legal action generation for cards does not exist yet.
- CardDB expansion, costs, ownership, timing windows, target selection, response windows, negation, passives, and wands remain future work.
- Source-less/global fixture effects are intentionally allowed for future controller-level effects, but should be revisited when real card ownership/timing is introduced.

## Damage/Heal Effect Request Scaffolding Pass

### Scope

This pass added deterministic generic `damage_effect` and `heal_effect` scaffolding for Unreal-owned fixture data.

Implemented:

- `FWBDamageEffectRequest`, `FWBDamageEffectResult`, and `WBDamageEffect::ApplyDamageEffect`
- `FWBHealEffectRequest`, `FWBHealEffectResult`, and `WBHealEffect::ApplyHealEffect`
- `WBEffectRunner::ApplyDamageEffect`
- `WBEffectRunner::ApplyHealEffect`
- `damage_effect_resolved` and `heal_effect_resolved` traces
- replay trace serialization for `heal_amount` and `effective_heal_amount`
- `FWBEffectRequest` payload support for `damage_effect` and `heal_effect`
- fixture parsing for standalone damage/heal effect operations and damage/heal effect request payloads
- standalone and effect-request GodotCanon fixtures for damage/heal scaffolding

Not implemented:

- Godot CardDB import
- JSON card database loading
- real card activation timing
- effect legal action generation
- target-selection UI
- response windows
- effect negation
- passives
- wands
- card-specific effects
- UI, Blueprint, `.uasset`, `.umap`, VFX, audio, or 3D runtime work

### Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex -NoHotReloadFromIDE
```

Final result:

```text
Result: Succeeded
Total execution time: 72.58 seconds
```

### Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Final result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=508
succeededWithWarnings=0
failed=0
notRun=0
```

### New Tests Added

- `Wandbound.Core.DamageHealEffectScaffolding.DamageBasicReducesHP`
- `Wandbound.Core.DamageHealEffectScaffolding.DamageUsesArmor`
- `Wandbound.Core.DamageHealEffectScaffolding.DamageBypassesArmor`
- `Wandbound.Core.DamageHealEffectScaffolding.DamageLethalRunsZeroHPCleanup`
- `Wandbound.Core.DamageHealEffectScaffolding.DamageZeroAmountNoop`
- `Wandbound.Core.DamageHealEffectScaffolding.DamageMissingTargetFailsWithoutMutation`
- `Wandbound.Core.DamageHealEffectScaffolding.DamageRemovedTargetFailsWithoutMutation`
- `Wandbound.Core.DamageHealEffectScaffolding.HealBasicRestoresHP`
- `Wandbound.Core.DamageHealEffectScaffolding.HealClampsToMaxHP`
- `Wandbound.Core.DamageHealEffectScaffolding.HealZeroAmountNoop`
- `Wandbound.Core.DamageHealEffectScaffolding.HealMissingTargetFailsWithoutMutation`
- `Wandbound.Core.DamageHealEffectScaffolding.HealRemovedTargetFailsWithoutMutation`
- `Wandbound.Core.DamageHealEffectScaffolding.EffectRequestDamagePayloadSucceeds`
- `Wandbound.Core.DamageHealEffectScaffolding.EffectRequestHealPayloadSucceeds`
- `Wandbound.Core.DamageHealEffectScaffolding.EffectRequestDamageThenHealAtomicSuccess`
- `Wandbound.Core.DamageHealEffectScaffolding.EffectRequestDamageThenHealAtomicFailure`
- `Wandbound.Core.DamageHealEffectScaffolding.MixedArmorStatusDamageHealAtomicSuccess`
- `Wandbound.Core.DamageHealEffectScaffolding.HiddenDataExcludedFromRuntimeSerialization`
- `Wandbound.Core.DamageHealEffectScaffolding.LegalGenerationUnchanged`
- `Wandbound.Core.DamageHealEffectScaffolding.ActionCodecUnchanged`
- `Wandbound.Core.DamageHealEffectScaffolding.FixtureScenarios`
- `Wandbound.Core.DamageHealEffectScaffolding.RuntimeSerializationFixture`

### Exact Errors

Final build and automation reported no errors.

### Notes

- Generic effect damage uses the existing damage resolver and armor absorption by default.
- `bBypassArmor` is fixture-owned scaffolding and does not import CardDB behavior.
- Lethal damage effects run the existing zero-HP cleanup wrapper after the damage trace.
- Generic healing clamps to `MaxHP`, does not change `MaxHP`, and does not revive removed/defeated units.
- Mixed `armor_effect`, `status_effect`, `damage_effect`, and `heal_effect` requests remain atomic.
- Legal action generation and `WBActionCodec` action IDs are unchanged.
- Hidden deck/hand/discard/source-card/source-effect data remains excluded from traces and public runtime utility serialization.
- `git diff --check` reported no whitespace errors, only LF-to-CRLF working-copy notices.
- Pre-existing dirty entries remain untouched: `MaxHP`, `Docs/Runtime_Legal_Action_Presentation_Snapshot_Report.md`, and `Docs/Runtime_Visual_Controller_Shell_Report.md`.

### Risks / Unknowns

- Damage/heal effect requests are fixture-owned only; no card activation command or legal player action exists yet.
- Card-specific damage/heal exceptions, effect negation, passives, wands, responses, prevention/replacement, and death triggers remain future work.
- Godot source ignore/protection behaviors around incoming effect sources are not implemented in this generic Unreal scaffold.

## Runtime Decision-Point Owner Shell Pass

### Scope

This pass added a production C++ decision-point owner shell for externally supplied runtime decision data.

Implemented:

- `UWBRuntimeDecisionPointOwnerComponent`
- external-data refresh through `RefreshDecisionPointFromExternalData`
- selected-action handoff through `ExecuteSelectedActionId`
- combined execute-and-post-action-refresh through `ExecuteSelectedActionIdAndRefreshFromExternalData`
- null/failure policies for missing coordinator, missing facade, and missing current decision point
- stale-state policy: execute-only does not refresh or regenerate legal actions
- success-only post-action refresh using caller-supplied post-action legal actions/public summary
- source guards for no legal action generation, no `WBRules`, no `WBEffectRunner`, and no `FWBGameStateData` ownership
- hidden data exclusion coverage

Not implemented:

- gameplay rules
- legal action generation
- legality decisions in runtime code
- direct `WBEffectRunner` calls
- runtime state ownership
- automatic legal-action generation after execution
- input
- tile picking
- unit selection
- UI widgets, UMG, cards, response UI, hand UI, or action menus
- camera logic
- animations, VFX, or sound
- Blueprints
- `.uasset` or `.umap` edits
- asset imports
- CardDB import
- marker visuals or hidden marker identity

### Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex -NoHotReloadFromIDE
```

Final result:

```text
Result: Succeeded
Total execution time: 8.10 seconds
```

The first build attempt failed in the new owner tests because test setup used const `GetPlayerById` where mutable `GetMutablePlayerById` was needed. Production code was unchanged; the test was corrected and the final rebuild succeeded.

### Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Final result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=486
succeededWithWarnings=0
failed=0
notRun=0
```

### New Tests Added

- `Wandbound.Runtime.DecisionPointOwner.ClassExists`
- `Wandbound.Runtime.DecisionPointOwner.InitialState`
- `Wandbound.Runtime.DecisionPointOwner.RefreshFromExternalDataSucceeds`
- `Wandbound.Runtime.DecisionPointOwner.RefreshMissingCoordinatorFails`
- `Wandbound.Runtime.DecisionPointOwner.ExecuteMissingCoordinatorFails`
- `Wandbound.Runtime.DecisionPointOwner.ExecuteMissingFacadeFails`
- `Wandbound.Runtime.DecisionPointOwner.ExecuteWithoutCurrentDecisionPointFails`
- `Wandbound.Runtime.DecisionPointOwner.ExecuteMoveSucceeds`
- `Wandbound.Runtime.DecisionPointOwner.ExecuteAndRefreshFromExternalDataSucceeds`
- `Wandbound.Runtime.DecisionPointOwner.ExecuteAndRefreshUsesSuppliedPostData`
- `Wandbound.Runtime.DecisionPointOwner.ExecuteAndRefreshSkipsRefreshOnFailure`
- `Wandbound.Runtime.DecisionPointOwner.ExecuteAndRefreshStoresRefreshFailure`
- `Wandbound.Runtime.DecisionPointOwner.FullEndTurnRefreshesNextPlayer`
- `Wandbound.Runtime.DecisionPointOwner.NoActionGenerationSourceGuard`
- `Wandbound.Runtime.DecisionPointOwner.NoEffectRunnerSourceGuard`
- `Wandbound.Runtime.DecisionPointOwner.NoGameStateOwnershipSourceGuard`
- `Wandbound.Runtime.DecisionPointOwner.HiddenDataExcluded`
- `Wandbound.Runtime.DecisionPointOwner.ClearClearsLocalAndCoordinatorState`

Existing decision-point coordinator, decision-loop harness, turn interaction model, interaction refresh adapter, action-selection bridge, legal-action presentation, visual/controller/facade, and core tests also pass in the full Wandbound automation run.

### Exact Errors

Final build and automation reported no errors.

### Risks / Unknowns

- The owner shell is not the future rules owner; another system must still generate legal actions and public summaries.
- `ExecuteSelectedActionIdAndRefreshFromExternalData` trusts caller-supplied post-action data and does not validate that it matches mutated state.
- The owner shell intentionally does not clear stale decision points after execute-only calls.
- Hidden marker identity remains unmodeled and excluded.
- Pre-existing untracked `MaxHP` remains untouched.

## Runtime Decision-Loop Test Harness Pass

### Scope

This pass added a test-only C++ decision-loop harness for runtime decision-point flow.

Implemented:

- `FWBDecisionLoopTestHarness` under `Source/WandboundTests/Private`
- external legal action generation simulation in tests only
- external public board summary simulation in tests only
- refresh -> selected-action execution -> explicit refresh coverage
- stale snapshot policy coverage
- two-decision loop coverage
- full EndTurn with deterministic MP roll coverage
- visual refresh participation coverage
- hidden data exclusion coverage across refresh and execution
- production runtime source guards confirming no legal-action generation, no `WBRules`, no `WBEffectRunner`, and no test harness dependency in `WandboundRuntime`

Not implemented:

- gameplay rules
- production runtime legal action generation
- legality decisions in runtime code
- runtime state ownership
- automatic post-action refresh
- input
- UI widgets
- camera behavior
- animation
- VFX
- sound
- Blueprints
- UMG
- `.uasset` edits
- `.umap` edits
- asset imports
- marker visuals or hidden marker identity

### Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex -NoHotReloadFromIDE
```

Final result:

```text
Result: Succeeded
Total execution time: 11.70 seconds
```

The first build after adding the new harness files also succeeded:

```text
Result: Succeeded
Total execution time: 32.64 seconds
```

### Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Final result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=468
succeededWithWarnings=0
failed=0
notRun=0
```

### New Tests Added

- `Wandbound.Runtime.DecisionLoopHarness.TestHelperIsTestOnly`
- `Wandbound.Runtime.DecisionLoopHarness.RefreshExposesMove`
- `Wandbound.Runtime.DecisionLoopHarness.ExecuteMoveLeavesStaleSnapshotUntilRefresh`
- `Wandbound.Runtime.DecisionLoopHarness.ExplicitPostActionRefreshReplacesSnapshot`
- `Wandbound.Runtime.DecisionLoopHarness.RefreshExecuteRefreshExecute`
- `Wandbound.Runtime.DecisionLoopHarness.FullEndTurnWithMPRollRefreshesNextPlayer`
- `Wandbound.Runtime.DecisionLoopHarness.VisualRefreshPathParticipates`
- `Wandbound.Runtime.DecisionLoopHarness.HiddenDataExcludedAcrossLoop`
- `Wandbound.Runtime.DecisionLoopHarness.ProductionRuntimeSourceGuard`

Existing decision-point coordinator, turn interaction model, action-selection bridge, legal-action presentation, interaction refresh adapter, controller facade, selected-action visual harness, and visual controller tests also pass in the full Wandbound automation run.

### Exact Errors

Final build and automation reported no errors.

An initial automation run failed `Wandbound.Runtime.DecisionLoopHarness.RefreshExecuteRefreshExecute` because the test used the runtime context default full-transition EndTurn mode without an MP roll source. The test now explicitly uses basic EndTurn for the generic two-decision loop, while `FullEndTurnWithMPRollRefreshesNextPlayer` covers the full-transition path with a fixed roll.

### Risks / Unknowns

- The decision-loop helper is test-only; future production ownership for legal actions and public summaries still needs a real runtime owner.
- The coordinator intentionally does not auto-refresh after action execution.
- Hidden marker identity remains unmodeled and excluded.
- Pre-existing untracked `MaxHP` remains untouched.

## Decision-Point Selected-Action Handoff Pass

### Scope

This pass added selected-action handoff from the decision-point coordinator to the existing turn interaction model execution path.

Implemented:

- selected-action reporting fields on `FWBRuntimeDecisionPointStatus`
- `UWBRuntimeDecisionPointCoordinatorComponent::ExecuteSelectedActionId`
- `HasLastSelectedActionExecution`
- `GetLastSelectedActionExecutionResult`
- `ClearLastSelectedActionExecution`
- coordinator-level failure reason `no_current_decision_point`
- coordinator-level failure reason `turn_interaction_model_missing`
- delegated null-facade, missing-id, ambiguous-id, and runtime-context failure behavior through the existing model/bridge/facade path
- stale-state policy: execution does not regenerate legal actions or presentation snapshots
- selected-action handoff source guards for no legal-action generation, no direct bridge/harness/EffectRunner calls, and no state ownership

Not implemented:

- gameplay rules
- legal action generation
- legality decisions
- direct EffectRunner calls
- automatic legal-action refresh after execution
- authoritative state ownership
- player input
- tile picking
- unit selection
- UI widgets, UMG, cards, response UI, hand UI, or action menus
- camera logic
- animations, VFX, or sound
- Blueprints
- `.uasset` or `.umap` edits
- asset imports
- CardDB import
- marker visuals or hidden marker identity

The coordinator accepts `FWBGameStateData&` only for selected-action handoff to the turn interaction model. It does not store, cache, inspect, or directly mutate game state.

### Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex -NoHotReloadFromIDE
```

Result:

```text
Result: Succeeded
Total execution time: 48.33 seconds
```

### Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Final result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=459
succeededWithWarnings=0
failed=0
notRun=0
```

### New Tests Added

- `Wandbound.Runtime.DecisionPointCoordinator.ExecuteWithoutCurrentDecisionPointFails`
- `Wandbound.Runtime.DecisionPointCoordinator.ExecuteMissingModelFails`
- `Wandbound.Runtime.DecisionPointCoordinator.ExecuteMissingActionIdFails`
- `Wandbound.Runtime.DecisionPointCoordinator.ExecuteMoveSucceeds`
- `Wandbound.Runtime.DecisionPointCoordinator.ExecuteFullEndTurnSucceeds`
- `Wandbound.Runtime.DecisionPointCoordinator.ExecuteInvalidFullEndTurnContextFails`
- `Wandbound.Runtime.DecisionPointCoordinator.ClearLastSelectedActionExecution`
- `Wandbound.Runtime.DecisionPointCoordinator.ExecuteDoesNotRegeneratePresentationSnapshot`
- `Wandbound.Runtime.DecisionPointCoordinator.SelectedActionHandoffHiddenDataExcluded`

Existing decision-point refresh, turn interaction model, action-selection bridge, runtime facade, and visual runtime tests also pass in the full Wandbound automation run.

### Exact Errors

None.

### Risks / Unknowns

- Selected-action handoff intentionally leaves the current legal action list and presentation snapshot stale until the future runtime owner refreshes the decision point.
- `bLastSelectedActionResolved` reports selected-id resolution, not final runtime apply success.
- Future UI/input code still does not exist; this is C++ handoff only.
- Pre-existing untracked `MaxHP` remains untouched.

## Runtime Decision-Point Coordinator Pass

### Scope

This pass added a C++ read-only decision-point coordinator for externally supplied legal actions and public board summaries.

Implemented:

- `FWBRuntimeDecisionPointStatus`
- `UWBRuntimeDecisionPointCoordinatorComponent`
- configurable refresh targets for the turn interaction model and visual controller
- refresh through `WBRuntimeInteractionRefreshAdapter::RefreshDecisionPoint`
- current decision-point status retention
- last refresh result retention
- presentation snapshot access for future UI
- presentation entry lookup delegation to the turn interaction model
- clear behavior that resets coordinator status and clears the turn interaction model when assigned
- failed-refresh status reset to avoid exposing stale UI state
- source guards for no action generation, no legality calls, no action execution, no state ownership, and no hidden zone access

Not implemented:

- gameplay rules
- legal action generation
- legality decisions
- action execution
- authoritative state ownership
- player input
- tile picking
- unit selection
- UI widgets, UMG, cards, response UI, hand UI, or action menus
- camera logic
- animations, VFX, or sound
- Blueprints
- `.uasset` or `.umap` edits
- asset imports
- CardDB import
- marker visuals or hidden marker identity

The coordinator consumes only externally supplied legal actions and `FWBPublicBoardSummary`. It does not call `WBRules`, call `WBEffectRunner`, generate actions, execute actions, own game state, or expose hidden deck/hand/discard data.

### Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex -NoHotReloadFromIDE
```

Result:

```text
Result: Succeeded
Total execution time: 58.83 seconds
```

### Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Final result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=450
succeededWithWarnings=0
failed=0
notRun=0
```

### New Tests Added

- `Wandbound.Runtime.DecisionPointCoordinator.ClassExists`
- `Wandbound.Runtime.DecisionPointCoordinator.InitialState`
- `Wandbound.Runtime.DecisionPointCoordinator.RefreshBothTargets`
- `Wandbound.Runtime.DecisionPointCoordinator.RefreshModelOnly`
- `Wandbound.Runtime.DecisionPointCoordinator.RefreshVisualOnly`
- `Wandbound.Runtime.DecisionPointCoordinator.MissingModelFails`
- `Wandbound.Runtime.DecisionPointCoordinator.MissingVisualFails`
- `Wandbound.Runtime.DecisionPointCoordinator.NoTargetsFails`
- `Wandbound.Runtime.DecisionPointCoordinator.ClearDecisionPointClearsModel`
- `Wandbound.Runtime.DecisionPointCoordinator.TryFindPresentationEntryDelegates`
- `Wandbound.Runtime.DecisionPointCoordinator.FailedRefreshResetsStaleStatus`
- `Wandbound.Runtime.DecisionPointCoordinator.NoActionGenerationSourceGuard`
- `Wandbound.Runtime.DecisionPointCoordinator.NoActionExecutionSourceGuard`
- `Wandbound.Runtime.DecisionPointCoordinator.NoGameStateOwnershipSourceGuard`
- `Wandbound.Runtime.DecisionPointCoordinator.HiddenDataExcluded`

Existing interaction refresh adapter, turn interaction model, legal-action presentation, and visual controller tests also pass in the full Wandbound automation run.

### Exact Errors

None.

### Risks / Unknowns

- The coordinator exposes read-only status only; future UI still needs a separate selected-action handoff path.
- Failed refresh resets coordinator status but does not forcibly clear stale state inside the turn interaction model unless `ClearDecisionPoint` is called.
- Future runtime owners must still generate fresh legal actions and public summaries before calling the coordinator.
- Hidden marker state remains unmodeled and excluded.
- Pre-existing untracked `MaxHP` remains untouched.

## Runtime Interaction Refresh Adapter Pass

### Scope

This pass added a pure C++ decision-point refresh adapter for externally supplied legal actions and public board summaries.

Implemented:

- `FWBRuntimeInteractionRefreshOptions`
- `FWBRuntimeInteractionRefreshResult`
- `WBRuntimeInteractionRefreshAdapter`
- validation-first target checks
- turn interaction model refresh from precomputed legal actions and public summary
- optional visual controller refresh from the same public summary
- result reporting for legal action count, presentation entry count, and visual refresh result
- explicit no-target failure reason: `no_refresh_targets_enabled`
- explicit missing target failure reasons: `turn_interaction_model_missing`, `visual_controller_missing`
- stale legal action/snapshot replacement through the interaction model's existing `SetCurrentInteractionState`
- source guards for no action generation, no legality calls, no action execution, no state ownership, and no hidden zone access

Not implemented:

- gameplay rules
- legal action generation
- legality decisions
- action execution
- authoritative state ownership
- player input
- tile picking
- unit selection
- UI widgets, UMG, cards, response UI, hand UI, or action menus
- camera logic
- animations, VFX, or sound
- Blueprints
- `.uasset` or `.umap` edits
- asset imports
- CardDB import
- marker visuals or hidden marker identity

The adapter consumes only externally supplied legal actions and `FWBPublicBoardSummary`. It does not call `WBRules`, generate actions, execute actions, own game state, or expose hidden deck/hand/discard data.

### Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex -NoHotReloadFromIDE
```

Result:

```text
Result: Succeeded
Total execution time: 14.45 seconds
```

### Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Final result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=435
succeededWithWarnings=0
failed=0
notRun=0
```

### New Tests Added

- `Wandbound.Runtime.InteractionRefreshAdapter.ClassExists`
- `Wandbound.Runtime.InteractionRefreshAdapter.RefreshBothTargets`
- `Wandbound.Runtime.InteractionRefreshAdapter.RefreshInteractionModelOnly`
- `Wandbound.Runtime.InteractionRefreshAdapter.RefreshVisualControllerOnly`
- `Wandbound.Runtime.InteractionRefreshAdapter.NoTargetsFails`
- `Wandbound.Runtime.InteractionRefreshAdapter.MissingInteractionModelFailsValidationFirst`
- `Wandbound.Runtime.InteractionRefreshAdapter.MissingVisualControllerFailsValidationFirst`
- `Wandbound.Runtime.InteractionRefreshAdapter.ReplacesStaleLegalActions`
- `Wandbound.Runtime.InteractionRefreshAdapter.SamePublicSummaryFeedsModelAndVisuals`
- `Wandbound.Runtime.InteractionRefreshAdapter.NoActionGenerationSourceGuard`
- `Wandbound.Runtime.InteractionRefreshAdapter.NoActionExecutionSourceGuard`
- `Wandbound.Runtime.InteractionRefreshAdapter.NoGameStateOwnershipSourceGuard`
- `Wandbound.Runtime.InteractionRefreshAdapter.HiddenDataExcluded`

Existing turn interaction model, legal-action presentation, visual controller, and action-selection bridge tests also pass in the full Wandbound automation run.

### Exact Errors

None.

### Risks / Unknowns

- The adapter reports visual acceptance through `bOk`, while `bVisualControllerRefreshed` mirrors whether a board actor actually rendered.
- Future runtime owners must still generate fresh legal actions and public summaries before calling the adapter.
- Hidden marker state remains unmodeled and excluded.
- Pre-existing untracked `MaxHP` remains untouched.

## Runtime Turn Interaction Model Pass

### Scope

This pass added a pure C++ runtime turn interaction model component that stores externally supplied legal actions and a public presentation snapshot for future UI.

Implemented:

- `UWBRuntimeTurnInteractionModelComponent`
- current precomputed legal action storage
- current public board summary storage
- current `FWBRuntimeLegalActionPresentationSnapshot` storage
- selected action ID lookup through `WBRuntimeActionSelectionBridge`
- action execution through the existing runtime controller facade path
- last selected action ID, selection result, and execution result retention
- explicit stale-state policy after execution: the component preserves the supplied legal actions/snapshot until an external owner refreshes or clears them
- missing-current-state failure reason: `no_current_interaction_state`
- tests for class availability, initial/clear state, snapshot building, order preservation, presentation lookup, selected move/end-turn execution, null facade policy, hidden data exclusion, stale state behavior, and source guards

Not implemented:

- gameplay rules
- legal action generation
- legality decisions
- authoritative state ownership
- player input
- tile picking
- unit selection
- UI widgets, UMG, cards, response UI, hand UI, or action menus
- camera logic
- animations, VFX, or sound
- Blueprints
- `.uasset` or `.umap` edits
- asset imports
- CardDB import
- marker visuals or hidden marker identity

The component consumes only externally supplied legal actions and `FWBPublicBoardSummary`. It does not call `WBRules`, generate actions, own game state, or expose hidden deck/hand/discard data.

### Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex -NoHotReloadFromIDE
```

Result:

```text
Result: Succeeded
Total execution time: 33.90 seconds
```

### Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Final result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=422
succeededWithWarnings=0
failed=0
notRun=0
```

### New Tests Added

- `Wandbound.Runtime.TurnInteractionModel.ClassExists`
- `Wandbound.Runtime.TurnInteractionModel.InitialState`
- `Wandbound.Runtime.TurnInteractionModel.SetStateBuildsSnapshot`
- `Wandbound.Runtime.TurnInteractionModel.PreservesInputOrder`
- `Wandbound.Runtime.TurnInteractionModel.ClearCurrentInteractionState`
- `Wandbound.Runtime.TurnInteractionModel.TryFindPresentationEntrySuccess`
- `Wandbound.Runtime.TurnInteractionModel.TryFindPresentationEntryMissing`
- `Wandbound.Runtime.TurnInteractionModel.ExecuteWithoutStateFails`
- `Wandbound.Runtime.TurnInteractionModel.ExecuteMissingIdFails`
- `Wandbound.Runtime.TurnInteractionModel.ExecuteMoveSucceeds`
- `Wandbound.Runtime.TurnInteractionModel.ExecuteFullEndTurnSucceeds`
- `Wandbound.Runtime.TurnInteractionModel.NullFacadeFollowsBridgePolicy`
- `Wandbound.Runtime.TurnInteractionModel.NoActionGenerationSourceGuard`
- `Wandbound.Runtime.TurnInteractionModel.NoGameStateOwnershipSourceGuard`
- `Wandbound.Runtime.TurnInteractionModel.HiddenDataExcluded`
- `Wandbound.Runtime.TurnInteractionModel.StaleStatePolicyAfterExecution`

### Exact Errors

None.

### Risks / Unknowns

- Current legal actions and presentation snapshots are intentionally not refreshed by the component after execution; the future UI/controller owner must supply fresh legal actions after state changes.
- The component stores a public board summary for presentation only. It is not authoritative state.
- Hidden marker state remains unmodeled and is still excluded from public presentation.
- Pre-existing untracked `MaxHP` remains untouched.

## Runtime Legal Action Presentation Snapshot Pass

### Scope

This pass added a pure C++ presentation snapshot for precomputed legal actions.

Implemented:

- `EWBRuntimeActionPresentationType`
- `FWBRuntimeActionPresentationEntry`
- `FWBRuntimeLegalActionPresentationSnapshot`
- `WBRuntimeLegalActionPresentation`
- stable action IDs through `WBActionCodec::MakeActionId`
- simple public labels for Move, Attack, End Turn, Pass, and unknown Action entries
- public source/target unit lookup from `FWBPublicBoardSummary`
- duplicate action id lookup failure in `TryFindEntryByActionId`
- presentation tests for ordering, labels, public source/target lookup, missing public units, duplicate/missing lookup, source guards, stable action IDs, and hidden data exclusion

Not implemented:

- gameplay rules
- legal action generation
- legality decisions
- state ownership
- player input
- tile picking
- unit selection
- UI widgets, UMG, cards, response UI, hand UI, or action menus
- camera logic
- animations, VFX, or sound
- Blueprints
- `.uasset` or `.umap` edits
- asset imports
- CardDB import
- marker visuals or hidden marker identity
- JSON serialization

The snapshot consumes only externally supplied legal actions and `FWBPublicBoardSummary`. It does not call `WBRules` or accept `FWBGameStateData`.

### Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex -NoHotReloadFromIDE
```

Result:

```text
Result: Succeeded
Total execution time: 15.27 seconds
```

### Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Final result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=406
succeededWithWarnings=0
failed=0
notRun=0
```

### New Tests Added

- `Wandbound.Runtime.LegalActionPresentation.PreservesActionOrder`
- `Wandbound.Runtime.LegalActionPresentation.MoveEntry`
- `Wandbound.Runtime.LegalActionPresentation.AttackEntry`
- `Wandbound.Runtime.LegalActionPresentation.EndTurnEntry`
- `Wandbound.Runtime.LegalActionPresentation.PassEntries`
- `Wandbound.Runtime.LegalActionPresentation.MissingPublicSourceUnit`
- `Wandbound.Runtime.LegalActionPresentation.RemovedTargetNotInPublicSummary`
- `Wandbound.Runtime.LegalActionPresentation.TryFindSuccess`
- `Wandbound.Runtime.LegalActionPresentation.TryFindMissing`
- `Wandbound.Runtime.LegalActionPresentation.TryFindDuplicate`
- `Wandbound.Runtime.LegalActionPresentation.NoActionGenerationSourceGuard`
- `Wandbound.Runtime.LegalActionPresentation.NoHiddenStateApi`
- `Wandbound.Runtime.LegalActionPresentation.ActionIdsStable`

### Notes

- `PassResponse` presents as `Pass` for player-facing clarity.
- Snapshot ordering preserves the supplied action order exactly.
- No Godot reference files were edited.
- No `.uasset`, `.umap`, Blueprint, UMG, or asset import work was added.
- The pre-existing untracked `MaxHP` file remains untouched.

### Exact Errors

Final build and automation reported no errors.

### Remaining Risks/Unknowns

- Future runtime/UI still needs an owner to produce current legal actions and a public board summary.
- Labels are intentionally minimal and may need localization or richer copy later.
- Real input, UI widgets, camera behavior, VFX, marker visuals, assets, and network replay remain future work.

## Runtime Action Selection Bridge Pass

### Scope

This pass added a pure C++ runtime bridge from selected action IDs to the runtime controller facade.

Implemented:

- `WBRuntimeActionSelectionBridge`
- `FWBResolvedRuntimeActionSelection`
- `FWBRuntimeActionSelectionExecutionResult`
- selected action ID resolution through `WBActionCodec::MakeActionId`
- missing ID failure with `selected_action_id_not_found`
- duplicate matching ID failure with `selected_action_id_ambiguous`
- null facade failure with `runtime_controller_facade_missing`
- execution delegation to `UWBRuntimeControllerFacadeComponent`
- bridge tests for resolution, execution, no mutation on failure, no MP roll consumption on failed resolution, source guards, stable action IDs, and hidden data exclusion

Not implemented:

- gameplay rules
- legal action generation
- legality decisions
- state ownership
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

The bridge consumes only an externally supplied legal action list and selected action ID. It does not call `WBRules` or generate actions.

### Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex -NoHotReloadFromIDE
```

Result:

```text
Result: Succeeded
Total execution time: 35.78 seconds
```

### Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Final result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=393
succeededWithWarnings=0
failed=0
notRun=0
```

### New Tests Added

- `Wandbound.Runtime.ActionSelectionBridge.ResolveMove`
- `Wandbound.Runtime.ActionSelectionBridge.ResolveAttack`
- `Wandbound.Runtime.ActionSelectionBridge.ResolveEndTurn`
- `Wandbound.Runtime.ActionSelectionBridge.MissingActionIdFails`
- `Wandbound.Runtime.ActionSelectionBridge.AmbiguousActionIdFails`
- `Wandbound.Runtime.ActionSelectionBridge.EmptyLegalListFailsSafely`
- `Wandbound.Runtime.ActionSelectionBridge.NullFacadeFailsWithoutMutation`
- `Wandbound.Runtime.ActionSelectionBridge.ExecuteMoveThroughFacade`
- `Wandbound.Runtime.ActionSelectionBridge.ExecuteFullEndTurnThroughFacade`
- `Wandbound.Runtime.ActionSelectionBridge.FailedResolutionDoesNotConsumeRoll`
- `Wandbound.Runtime.ActionSelectionBridge.NoActionGenerationSourceGuard`
- `Wandbound.Runtime.ActionSelectionBridge.NoLegalityCallsSourceGuard`
- `Wandbound.Runtime.ActionSelectionBridge.ActionIdsStable`
- `Wandbound.Runtime.ActionSelectionBridge.HiddenDataExcluded`

### Notes

- Null facade execution is a hard failure and does not mutate state.
- Duplicate action IDs are treated as ambiguous.
- No Godot reference files were edited.
- No `.uasset`, `.umap`, Blueprint, UMG, or asset import work was added.
- The pre-existing untracked `MaxHP` file remains untouched.

### Exact Errors

Final build and automation reported no errors.

### Remaining Risks/Unknowns

- Future runtime/UI still needs an owner to produce and hold the precomputed legal action list.
- The bridge trusts the provided legal action list and does not prove that it is current.
- Real input, UI, camera behavior, VFX, marker visuals, assets, and network replay remain future work.

## Runtime Controller Facade Pass

### Scope

This pass added a C++ runtime controller facade component for externally selected actions.

Implemented:

- `UWBRuntimeControllerFacadeComponent`
- selected-action delegation to `WBSelectedActionVisualHarness`
- configurable success/failure visual refresh policy
- latest runtime result retention
- latest visual refresh result retention
- clear/reset API for retained results
- facade tests for null visual controller safety, visual controller refresh, deterministic full end turn, failure refresh policies, result clearing, source guards, and hidden data exclusion

Not implemented:

- gameplay rules
- legal action generation
- target legality decisions
- state ownership
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

The facade accepts external mutable `FWBGameStateData&`, an externally selected `FWBAction`, and an external `FWBRuntimeTurnResolutionContext`. It does not own rules state or generate actions.

### Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex -NoHotReloadFromIDE
```

Result:

```text
Result: Succeeded
Total execution time: 18.29 seconds
```

### Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Final result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=379
succeededWithWarnings=0
failed=0
notRun=0
```

### New Tests Added

- `Wandbound.Runtime.ControllerFacade.ClassExists`
- `Wandbound.Runtime.ControllerFacade.InitialState`
- `Wandbound.Runtime.ControllerFacade.MoveNullVisualControllerSafe`
- `Wandbound.Runtime.ControllerFacade.MoveRefreshesVisualController`
- `Wandbound.Runtime.ControllerFacade.FullEndTurnUsesDeterministicRoll`
- `Wandbound.Runtime.ControllerFacade.FailureSkipsRefreshByDefault`
- `Wandbound.Runtime.ControllerFacade.FailureCanRefreshWhenRequested`
- `Wandbound.Runtime.ControllerFacade.ClearLastResults`
- `Wandbound.Runtime.ControllerFacade.NoGameStateOwnershipSourceGuard`
- `Wandbound.Runtime.ControllerFacade.NoActionGenerationSourceGuard`
- `Wandbound.Runtime.ControllerFacade.HiddenDataExcluded`

### Notes

- Optional actor shell was intentionally skipped.
- No Godot reference files were edited.
- No `.uasset`, `.umap`, Blueprint, UMG, or asset import work was added.
- The pre-existing untracked `MaxHP` file remains untouched.

### Exact Errors

Final build and automation reported no errors.

### Remaining Risks/Unknowns

- The facade still requires future input/UI to supply a selected legal action.
- The visual path still refreshes placeholder board-view counts only.
- Real meshes, camera behavior, input, UI, VFX, marker visuals, assets, and network replay remain future work.

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

---

# Runtime Selected Action Visual Harness Pass

## Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex -NoHotReloadFromIDE
```

Result:

```text
Result: Succeeded
Total execution time: 16.05 seconds
```

## Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=368
succeededWithWarnings=0
failed=0
notRun=0
```

## Notes

- Added `WBSelectedActionVisualHarness`.
- The harness executes externally supplied selected actions through `WBRuntimeTurnResolutionAdapter::ApplyRuntimeSelectedActionWithResult`.
- The harness can refresh `UWBRuntimeVisualControllerComponent` from `FinalPublicBoardSummary`.
- Default refresh policy is success-only; failure refresh requires explicit opt-in.
- The harness does not own or cache `FWBGameStateData`.
- The harness does not generate legal actions or inspect hidden state.
- No input, UI, camera, animation, VFX, sound, Blueprints, UMG, `.uasset`, `.umap`, asset import, marker visual, hidden marker identity, network replay, or AI work was added.

## New Tests Added

- `Wandbound.Runtime.SelectedActionVisualHarness.*`

## Remaining Risks/Unknowns

- Future runtime/input code still needs to supply a selected legal action.
- The harness currently refreshes placeholder public board counts only through the existing visual controller path.
- Real input, UI, camera, VFX, assets, marker visuals, and network replay remain future work.

---

# Card Definition Activation Expansion Pass

## Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex
```

Result:

```text
Result: Succeeded
Total execution time: 10.93 seconds
```

Earlier full compile after adding the new source files also succeeded:

```text
Result: Succeeded
Total execution time: 136.62 seconds
```

## Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=527
succeededWithWarnings=1
failed=0
notRun=0
```

Warning detail:

```text
Wandbound.Core.Movement.RootedUnitCannotMove logged one EOS datarouter retry warning.
```

## Notes

- Added fixture-owned card definition and activated-effect structs.
- Added `WBCardActivationExpansion` to build `FWBCardActivationCommand` values from fixture definitions.
- Added deterministic candidate generation in effect order then target order.
- Added fixture operations for build-only and build-and-apply card definition activation flows.
- Added fixture-driven golden scenarios for build, apply, failure, and hidden metadata exclusion.
- The expansion layer does not import Godot CardDB, production-load card JSON, generate legal actions, call rules, call effect runner, or add UI/runtime activation windows.
- No input, UI, camera, animation, VFX, sound, Blueprints, UMG, `.uasset`, `.umap`, asset import, marker visual, hidden marker identity, network replay, or AI work was added.

## New Tests Added

- `Wandbound.Core.CardDefinitionActivationExpansion.*`

## Remaining Risks/Unknowns

- Card ownership, costs, timing, once-per-turn gates, and legal activation player actions remain future work.
- Godot CardDB import remains intentionally out of scope.
- Current generic payload coverage is limited to armor, status, damage, and heal.

---

# Card Activation Candidate Generation Pass

## Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex -NoHotReloadFromIDE
```

Result:

```text
Result: Succeeded
Total execution time: 4.69 seconds
```

Initial full compile after adding source files also succeeded:

```text
Result: Succeeded
Total execution time: 38.02 seconds
```

## Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=537
succeededWithWarnings=0
failed=0
notRun=0
```

Focused candidate-generation automation also passed:

```text
Wandbound.Core.CardActivationCandidateGeneration
```

## Notes

- Added deterministic fixture-driven card activation candidate generation.
- Added `FWBCardActivationCandidate` and `WBCardActivationCandidateGenerator`.
- Added `generate_card_activation_candidates` fixture operation support.
- Added 10 GodotCanon golden candidate fixtures.
- Added 9 `Wandbound.Core.CardActivationCandidateGeneration.*` automation tests.
- Candidate generation is build-only, does not mutate state, and applies only when the caller explicitly runs the wrapped `FWBCardActivationCommand`.
- Existing `WBRules::GenerateLegalActions` and `WBActionCodec` output are unchanged.
- No Godot CardDB import, production card loading, activation legal actions, target UI, response windows, negation, passives, wands, card-specific behavior, Blueprints, `.uasset`, or `.umap` work was added.

## Remaining Risks/Unknowns

- Production CardDB import remains future work.
- Real activation legal actions, card zones, costs, timing, and once-per-turn gates remain future work.
- Response windows, negation, passives, wands, and card-specific behavior remain future work.

---

# Card Activation Legal Action Family Pass

## Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex -NoHotReloadFromIDE
```

Result:

```text
Result: Succeeded
Total execution time: 45.74 seconds
```

## Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=545
succeededWithWarnings=0
failed=0
notRun=0
```

## Notes

- Added `FWBCardActivationLegalAction`, action sets, generation result, and `WBCardActivationLegalActionGenerator`.
- Added an activation-only presentation snapshot and deterministic lookup.
- Added fixture operation `generate_card_activation_legal_actions`.
- Added 8 GodotCanon activation legal action fixtures.
- Added `Wandbound.Core.CardActivationLegalActionGeneration.*` automation coverage.
- Activation legal actions remain separate from `FWBAction`.
- Existing `WBRules::GenerateLegalActions` output remains unchanged.
- Existing `WBActionCodec` output remains unchanged.
- Activation application remains explicit through `WBEffectRunner::ApplyCardActivationCommand`.
- No Godot CardDB import, production card loading, activation costs/zones, target UI, response windows, negation, passives, wands, card-specific behavior, Blueprints, `.uasset`, or `.umap` work was added.

## Remaining Risks/Unknowns

- Production CardDB import remains future work.
- Real activation legality still needs card zones, costs, timing, and once-per-turn gates.
- Response windows, negation, passives, wands, and card-specific behavior remain future work.
- This pass accepts non-`activate:` candidate ids because the candidate source owns this separate id namespace for now.

---

# Card Activation Source Gates Pass

## Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex -NoHotReloadFromIDE
```

Result:

```text
Result: Succeeded
Total execution time: 96.61 seconds
```

## Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=550
succeededWithWarnings=0
failed=0
notRun=0
```

## Notes

- Added `WBCardActivationSourceGate`.
- Added source gate definitions to `FWBCardEffectDefinition`.
- Added source gate contexts to `FWBCardActivationCandidateSource`.
- Added fixture-only activation usage keys on `FWBGameStateData`.
- Candidate generation now skips effects whose source gate fails.
- Added `evaluate_card_activation_source_gate` fixture operation.
- Added 12 GodotCanon source-gate fixtures.
- Added `Wandbound.Core.CardActivationSourceGate.*` automation coverage.
- Existing `WBRules::GenerateLegalActions` and `WBActionCodec` output remain unchanged.
- No Godot CardDB import, production card loading, real zones/cost payment, response windows, negation, passives, wands, card-specific behavior, Blueprints, `.uasset`, or `.umap` work was added.

## Remaining Risks/Unknowns

- Production CardDB import remains future work.
- Real hand/deck/discard/equipped zone legality remains future work.
- Real RL/RR cost payment remains future work.
- Production activation execution beyond fixture commands remains future work. Fixture card activation usage marking is covered by the later Card Activation Usage Marking pass.
- Response windows, negation, passives, wands, and card-specific behavior remain future work.

---

# Card Activation Usage Marking Pass

## Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex -NoHotReloadFromIDE
```

Result:

```text
Result: Succeeded
Total execution time: 145.91 seconds
```

## Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=554
succeededWithWarnings=0
failed=0
notRun=0
```

## Notes

- Added `FWBCardActivationUsageCommit`.
- Activation expansion now populates usage commit metadata for once-per-turn fixture effects.
- Candidate generation passes source-gate context into expansion.
- Activation candidates and separate activation legal actions preserve usage commit metadata.
- `WBRules::CanApplyCardActivationCommand` validates usage commits without mutating state.
- `WBEffectRunner::ApplyCardActivationCommand` marks usage only after successful effect request resolution.
- Failed activation does not mark usage.
- Invalid usage commit fails atomically.
- Already-used usage keys fail with `once_per_turn_already_used`.
- Turn-start resource setup clears the player's activation usage keys.
- Added 8 GodotCanon activation usage marking fixtures.
- Added `Wandbound.Core.CardActivationUsageMarking.*` automation coverage.
- Existing `WBRules::GenerateLegalActions` and `WBActionCodec` output remain unchanged.
- No Godot CardDB import, production card zones/costs, activation `FWBAction`, response windows, negation, passives, wands, card-specific behavior, Blueprints, `.uasset`, or `.umap` work was added.

## Remaining Risks/Unknowns

- Godot has pending-response and negation semantics that this fixture scaffold intentionally does not model yet.
- The Red Ledger deferred usage-marking special case remains future card-specific behavior.
- Production card zones, costs, and CardDB import remain future work.

---

# Card Activation Cost Payment Scaffolding Pass

## Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex -NoHotReloadFromIDE
```

Final result:

```text
Result: Succeeded
Total execution time: 12.65 seconds
```

An initial build surfaced a unity-build helper-name collision between existing private `MakeFailureResult` helpers in `WBTurnController.cpp` and `WBRuntimeTurnResolutionAdapter.cpp`. The runtime adapter helper was renamed to `MakeRuntimeResolutionFailureResult`; the final build succeeded.

## Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Result from `Saved/AutomationReports/Wandbound/index.json`:

```text
succeeded=570
succeededWithWarnings=0
failed=0
notRun=0
```

## Notes

- Added fixture-owned activation cost payment commit metadata.
- Added `WBCardActivationCostPayment::CanPayCost` and `PayCost`.
- Payment validates source, ownership, source board/defeat state, nonnegative RR, and available RL.
- `WBEffectRunner::ApplyCardActivationCommand` pays on a working copy and commits payment only after activation success.
- Failed payment and failed effects leave original `RLUsed` unchanged.
- Payment plus usage marking is atomic with successful activation.
- Added safe `card_activation_cost_paid` trace fields.
- Added 10 GodotCanon cost payment fixtures.
- Added `Wandbound.Core.CardActivationCostPayment.*` automation coverage.
- Existing `WBRules::GenerateLegalActions` output remains unchanged.
- Existing `WBActionCodec` output remains unchanged.
- No overflow, wand destruction, CardDB import, production zones, activation `FWBAction`, response windows, negation, passives, wands, card-specific behavior, Blueprints, `.uasset`, or `.umap` work was added.

## Remaining Risks/Unknowns

- Godot response/negation timing remains future work.
- Production CardDB import and card-zone legality remain future work.
- Real payment ownership for production hand/equipped zones remains future work.
- Overflow and equipped wand fallout remain future work.

---

# Card Activation Cost Payment Replay Verifier Pass

## Build

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' WandboundUEEditor Win64 Development -Project='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -WaitMutex -NoHotReloadFromIDE
```

Final result:

```text
Result: Succeeded
Total execution time: 12.27 seconds
```

An initial build exposed existing anonymous helper name collisions in `WBReplayTrace.cpp` under Unity compilation. The replay-trace private helpers were renamed locally; the final build succeeded.

## Wandbound Automation Tests

Command used:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\WandboundUE.uproject' -unattended -nop4 -NullRHI -nosplash -ExecCmds='Automation RunTests Wandbound; Quit' -TestExit='Automation Test Queue Empty' -ReportExportPath='C:\Users\rnhof\OneDrive\Documents\Unreal Projects\WandboundUE\Saved\AutomationReports\Wandbound'
```

Final result:

```text
succeeded=575
succeededWithWarnings=0
failed=0
notRun=0
```

## Notes

- Added `FWBExpectedActivationCostPaymentState`.
- Added test-only `FWBCardActivationCostPaymentVerifier`.
- Extended replay fixture utilities with optional RL state, cost-paid trace, forbidden trace substring, and payment commit checks.
- Added 8 GodotCanon cost-payment replay verifier fixtures.
- Added `Wandbound.Core.CardActivationCostPaymentReplayVerifier.*` automation coverage.
- Existing `WBRules::GenerateLegalActions` output remains unchanged.
- Existing `WBActionCodec` output remains unchanged.
- No gameplay, runtime, UI, Blueprint, `.uasset`, or `.umap` work was added.

## Remaining Risks/Unknowns

- Production CardDB import and card-zone legality remain future work.
- Response windows, effect negation, passives, wands, and card-specific costs remain future work.
- Overflow and equipped wand fallout remain future work.
