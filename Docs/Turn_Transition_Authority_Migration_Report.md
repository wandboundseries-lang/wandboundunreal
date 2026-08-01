# Wandbound Turn-Transition Authority Migration Report

## Result

- Editor build: succeeded in 17.55 seconds on the final source.
- Game build: succeeded in 108.98 seconds on the final source.
- Focused authority tests: 38 succeeded, 0 failed.
- Affected legacy groups: 660 succeeded, 0 failed.
- Initial full test count: 1,778.
- Final full automation: 1,816 succeeded, 0 failed, 0 warnings.
- Fresh BuildCookRun: succeeded in 166.12 seconds.
- Package: `Saved/PackagedBuilds/TurnAuthorityFinal_20260801_1055`.
- Packaged development smoke: passed, exit 0, generation 1, revision 4.
- Packaged production transition: passed through the runtime host with
  `action_submitted=true` and `end_turn_submitted=true`.
- Paused/resumed transition: passed in compiled coordinator/runtime/replay
  automation; no reroll, repeated status pass, attack reset, or wall
  restoration occurred.
- Packaged production startup: passed twice, exit 0, `production_started`,
  Player 1 active, Turn 1, turn start complete, playable decision reached.
- Repeated production startup comparison: byte-identical, SHA-256
  `cf7dc1956e3ee10035a585a9b9e64fea1e5436492ad83f17e453194dbc7ed004`.
- Replay verification: coordinator EndTurn and trigger-order action replay
  produced byte-equivalent trace JSON, action IDs, decision boundaries, and
  MP outcome.
- `git diff --check`: passed; line-ending notices only.
- Exact final errors: none.

Interim validation issues were corrected before the final run:

- The first focused run used a production fixture whose immediate EndTurn was
  not a complete transition setup; it was replaced by the established match
  coordinator fixture shape and all 38 tests passed.
- The first packaged production probe omitted production-data arguments, then
  the next omitted the bundle/match-spec arguments. Both timed-out processes
  were stopped. The corrected explicit production invocation passed twice.

## Baseline

- Commit: `f87005f403567f2f0d65059e5782f02320253a9e`
  (`Add deterministic turn-start sequence`).
- Branch: `main`, synchronized with `origin/main`.
- Staged files: none.
- LFS-staged files: none.
- Initial Wandbound automation: 1,778 succeeded.
- Production startup: `production_started`, Player 1 first and active, Turn 1,
  all turn-start milestones complete, playable decision exposed.
- Production semantic bundle digest:
  `87d2644aeb479e84a3e96967fd57901ac52aa7e283fd5cfee142d35e8659f00c`.
- Match-spec SHA-256:
  `5fd3ba8d78af9681e3acdc4c4b58e7c2934aacbcfed83cd3504a8a37e23b03da`.

Pre-existing tracked changes were preserved in:

- `Config/DefaultEditor.ini`
- `Docs/CardDB_Unreal_Bundle_Schema_Validation_Report.md`
- `Source/WandboundCore/Private/WBCardActivationAffordability.cpp`
- `Source/WandboundCore/Private/WBCardActivationCostPayment.cpp`
- `Source/WandboundCore/Private/WBGameStateData.cpp`
- `Source/WandboundCore/Private/WBPublicBoardSummary.cpp`
- `Source/WandboundCore/Private/WBResonanceLoad.cpp`
- `Source/WandboundCore/Private/WBRuntimeResultSerializer.cpp`
- `Source/WandboundCore/Public/WBEquipExecution.h`
- `Source/WandboundCore/Public/WBPublicBoardSummary.h`
- `Source/WandboundTests/Private/WBEquipExecutionTests.cpp`
- `Source/WandboundTests/Private/WBProductionEquipExecutionHandoffTests.cpp`
- `Source/WandboundTests/Private/WBProductionResonanceOverflowHandoffTests.cpp`
- `Source/WandboundTests/Private/WBProductionSummonEquipDataProviderTests.cpp`
- `Source/WandboundTests/Private/WBResonanceOverflowTests.cpp`
- `Source/WandboundTests/Private/WBRuntimeActivationPresentationModelTests.cpp`
- `Source/WandboundTests/Private/WBRuntimeDecisionLoopHarnessTests.cpp`
- `Source/WandboundTests/Private/WBRuntimeDecisionPointCoordinatorTests.cpp`
- `Source/WandboundTests/Private/WBRuntimeDecisionPointOwnerTests.cpp`

Pre-existing untracked content was preserved under `Content/Maps`,
`Content/MeshyImports`, `Plugins/meshy/Content`, two unrelated audit files,
`MaxHP`, `RLTotal`, and the pre-existing stray terminal-named files.

## Caller Inventory

The complete field-level inventory is in
`Docs/Turn_Transition_Authority_Migration_Audit.json`; its readable analysis is
in `Docs/Turn_Transition_Authority_Migration_Audit.md`.

Production chain:

```text
AWBRuntimePlayerController::ForwardEndTurn
UWBRuntimeMatchHUDWidget::RequestEndTurn
UWBRuntimeMatchHostComponent::EndTurn
UWBRuntimeMatchHostComponent::SubmitLegalActionAtRevision
WBMatchCoordinator::SubmitActionId
WBMatchCoordinator::ApplyTurnTransition
```

All are `AlreadyCoordinatorOwned`. Packaged local-play smoke reaches the same
host chain.

Focused authoritative primitives:

- `WBTurnStartSequence::Begin/SubmitChoice`
- `WBNPCPhaseResolution::ResolvePhase`
- `WBEffectRunner::ApplyEndTurn`
- turn-start `WBEffectRunner` methods
- `FWBGameStateData` turn mutation methods

These are `LowLevelPrimitiveOnly`; none chooses the complete production
transition independently.

Compatibility/replay callers:

- coordinator-injected `WBRuntimeTurnResolutionAdapter`:
  `AlreadyCoordinatorOwned`;
- raw-state `WBRuntimeTurnResolutionAdapter`: `CompatibilityOnly`;
- full-transition `WBSelectedActionExecutor`: `CompatibilityOnly`;
- full-transition `WBTurnController`: `SafeToDeprecate`;
- `WBEffectRunner::ApplyDeterministicTurnTransition`: `SafeToDeprecate`;
- generic low-level `WBReplayVerifier`: `UnsafeToRemove`;
- `WBReplayFixtureTestUtils`: `TestOnly`;
- uninstantiated visual/runtime shell chain: `UnusedConfirmed`.

Unknown full-transition callers: zero.

## Previous Authority Paths

The active production host already submitted EndTurn through the coordinator.
The bypass was the older raw-state chain:

```text
WBRuntimeTurnResolutionAdapter
WBSelectedActionExecutor
WBTurnController
WBEffectRunner::ApplyDeterministicTurnTransition
```

It selected MP externally and composed end status, player advance, resource
setup, and start status without NPC resolution, draw, terminal phase
orchestration, or durable trigger decisions. No active production actor owned
that chain, but it remained compiled and heavily fixture-tested.

Generic `WBReplayVerifier` replays low-level `FWBAction` mutations and never
claimed to reproduce a complete match transition. Complete EndTurn replay now
uses coordinator action submission in the authority suite.

## Coordinator API

`WBMatchCoordinator::SubmitActionId` remains the authoritative command API.
No duplicate EndTurn command representation was added.

`FWBMatchOperationResult` now exposes only public-safe coordination state:

- completed;
- terminal;
- pending decision;
- pending player;
- active player;
- turn number;
- trace begin/end indices.

It does not expose deck order, private draw identity, hidden marker identity,
concealed parameters, or RNG state.

The coordinator also exposes:

- `IsTurnTransitionInProgress`;
- `HasPendingTurnStartDecision`;
- `GetPendingTurnStartDecisionPlayerId`.

Pending state remains the existing durable `FWBTurnStartSequenceState`.
Submitting EndTurn while that decision is unresolved fails with
`turn_transition_pending_decision`. Existing architecture diagnostics remain
for wrong player, stale/illegal action, and terminal match.

## Production Caller Migration

No production host rewrite was needed: source and runtime tests proved it
already used `SubmitActionId`.

The coordinator-aware runtime adapter path is the migration bridge for older
selected-action integrations. When a coordinator is supplied it:

- submits every selected action ID to the coordinator;
- does not consume the adapter MP source;
- mirrors only committed coordinator state;
- returns coordinator trace and safe transition status.

The old raw-state branch remains only when no coordinator is supplied, keeping
legacy fixtures byte-compatible.

## Runtime Integration

Source guards scan production Runtime and WandboundUE C++ and reject:

- `WBTurnController::` calls;
- `ApplyDeterministicTurnTransition` calls;
- direct turn advancement;
- direct turn-start MP/resource mutation;
- runtime `TryGetNextMPRoll` ownership.

The runtime host refreshes observations from committed coordinator state.
Pending turn-start decisions expose only `TurnStartTrigger` actions; normal
actions return only after completion. Presentation remains locked by the
existing trace/decision sequence.

## Production Bootstrap

`WBProductionRuntimeBootstrap::Build` still creates the same
`FWBMatchInitializationRequest`. It does not execute a private transition
path. Startup initializes `WBMatchCoordinator`, preserves the
`production_started` contract, and produced byte-identical startup JSON in
compiled automation.

The semantic bundle and match-spec digests did not change.

## Replay

Two coordinators initialized from the same request replayed the same recorded
EndTurn action ID and optional trigger-order action ID. The runs matched on:

- serialized trace events;
- legal action IDs;
- pause boundary;
- trigger decision ID;
- MP result;
- resumed trace;
- resulting turn and active player.

The coordinator owns RNG; the coordinator-injected runtime adapter leaves its
legacy queued MP roll untouched.

## Compatibility Adapter

`WBTurnController` remains because tracked legacy tests and fixture utilities
still call its raw-state API. Its full-transition mode now delegates to
`WBMatchCoordinator::ApplyLegacyCompatibilityTurnTransition`.

`WBEffectRunner::ApplyDeterministicTurnTransition` delegates to the same
compatibility entry point. Neither class contains an independent full
transition implementation.

Compile-time deprecation was avoided to prevent warning spam across the large
legacy fixture suite. Source-level deprecation comments state:

```text
Compatibility only. Production full transitions use WBMatchCoordinator.
```

The compatibility behavior intentionally remains narrower than production
match orchestration and is not reachable from active production runtime
source.

## Duplicate-Orchestration Audit

After migration, only `WBMatchCoordinator::ApplyTurnTransition` combines
production end-status handling, NPC phase, player advancement,
coordinator-owned MP generation, the durable turn-start sequence, terminal
handling, and normal-action unlock.

Remaining combinations are:

- the explicitly named coordinator-owned legacy compatibility method;
- focused state/effect primitives;
- test fixture utilities.

No production duplicate remains.

## Behavior Preservation

Evidence:

- 1,778 pre-existing tests remain green;
- total automation increased to 1,816;
- affected legacy sweep passed 660/660;
- old compatibility traces remain byte-equivalent;
- turn-start sequence order remains Draw, MP, reset, status/death, effects;
- production startup contract and digests are unchanged;
- action IDs and replay trace JSON are stable;
- public observation structures were not changed;
- no Active Format, game-start, card-effect, Godot, Meshy, model, map, asset,
  Blueprint, networking, or save/load source was modified.

## Tests

Caller inventory:

- `Wandbound.TurnAuthority.Audit.AllTurnControllerCallersClassified`
- `Wandbound.TurnAuthority.Audit.AllProductionCallersCoordinatorOwned`
- `Wandbound.TurnAuthority.Audit.NoUnknownFullTransitionCaller`
- `Wandbound.TurnAuthority.Audit.CompatibilityCallersExplicit`

Coordinator authority and duplicate prevention:

- `Wandbound.TurnAuthority.Coordinator.OwnsEndTurnTransition`
- `Wandbound.TurnAuthority.Coordinator.OwnsNextPlayerAdvance`
- `Wandbound.TurnAuthority.Coordinator.OwnsMPRNG`
- `Wandbound.TurnAuthority.Coordinator.OwnsTurnStartSequence`
- `Wandbound.TurnAuthority.Coordinator.GeneratesFirstNormalDecision`
- `Wandbound.TurnAuthority.Coordinator.DuplicateEndTurnRejected`
- `Wandbound.TurnAuthority.Coordinator.ResumeDoesNotReroll`
- `Wandbound.TurnAuthority.Coordinator.ResumeDoesNotRetickStatuses`
- `Wandbound.TurnAuthority.Coordinator.ResumeDoesNotResetResourcesTwice`
- `Wandbound.TurnAuthority.Coordinator.StaleDecisionRejected`

Runtime and production:

- `Wandbound.TurnAuthority.Runtime.EndTurnUsesCoordinator`
- `Wandbound.TurnAuthority.Runtime.PendingDecisionKeepsInputLocked`
- `Wandbound.TurnAuthority.Runtime.CompletedTransitionUnlocksInput`
- `Wandbound.TurnAuthority.Runtime.NoDirectTurnControllerCall`
- `Wandbound.TurnAuthority.Runtime.NoGameplayRNGOwnership`
- `Wandbound.TurnAuthority.Production.BootstrapUsesCoordinator`
- `Wandbound.TurnAuthority.Production.StartupResultPreserved`
- `Wandbound.TurnAuthority.Production.FirstTurnDecisionPreserved`
- `Wandbound.TurnAuthority.Production.RepeatedResultByteIdentical`

Replay and compatibility:

- `Wandbound.TurnAuthority.Replay.EndTurnUsesCoordinator`
- `Wandbound.TurnAuthority.Replay.TraceEquivalent`
- `Wandbound.TurnAuthority.Replay.PendingTriggerDecisionPreserved`
- `Wandbound.TurnAuthority.Replay.RNGConsumptionStable`
- `Wandbound.TurnAuthority.Replay.ActionIdsStable`
- `Wandbound.TurnAuthority.Compatibility.AdapterDelegates`
- `Wandbound.TurnAuthority.Compatibility.NoIndependentFullAlgorithm`
- `Wandbound.TurnAuthority.Compatibility.DeprecationDocumented`
- `Wandbound.TurnAuthority.Compatibility.TestFixtureStillSupported`

Source guards:

- `Wandbound.Authority.TurnTransition.NoProductionDirectTurnControllerCaller`
- `Wandbound.Authority.TurnTransition.NoRuntimeTurnMutation`
- `Wandbound.Authority.TurnTransition.NoDuplicateRNGPath`
- `Wandbound.Authority.TurnTransition.NoGodotChanges`
- `Wandbound.Authority.TurnTransition.NoMeshyChanges`
- `Wandbound.Authority.TurnTransition.NoModelOrMapChanges`

## Changed Files

All task source files were clean at baseline; there was no hunk overlap.

| Path | Purpose | Impact |
| --- | --- | --- |
| `Source/WandboundCore/Public/WBMatchCoordinator.h` | safe result/status and compatibility API | Core/runtime/replay |
| `Source/WandboundCore/Private/WBMatchCoordinator.cpp` | result status, duplicate guard, compatibility semantics | Core/runtime/replay |
| `Source/WandboundCore/Public/WBEffectRunner.h` | source deprecation marker | Compatibility |
| `Source/WandboundCore/Private/WBEffectRunner.cpp` | delegate old full helper | Compatibility |
| `Source/WandboundCore/Public/WBTurnController.h` | compatibility-only marker | Compatibility |
| `Source/WandboundCore/Private/WBTurnController.cpp` | delegate full command | Compatibility |
| `Source/WandboundCore/Public/WBRuntimeTurnResolutionAdapter.h` | coordinator injection and safe status | Runtime/replay |
| `Source/WandboundCore/Private/WBRuntimeTurnResolutionAdapter.cpp` | coordinator submission path | Runtime/replay |
| `Source/WandboundTests/Private/WBTurnTransitionAuthorityTests.cpp` | 38 focused tests and guards | Tests |
| `Docs/Turn_Transition_Authority_Migration_Audit.md` | readable caller inventory | Docs |
| `Docs/Turn_Transition_Authority_Migration_Audit.json` | machine-readable caller inventory | Docs/tests |
| `Docs/Turn_Transition_Authority_Migration_Report.md` | implementation evidence | Docs |
| `Docs/Build_Test_Report.md` | validation ledger | Docs |

## Git Status

- Task files: the 13 paths above.
- Pre-existing tracked changes: preserved as listed in Baseline.
- Pre-existing untracked files: preserved as listed in Baseline.
- Staged files: none.
- LFS-staged files: none.
- Generated output: Editor/Game binaries, automation reports, logs, cooked,
  staged, and packaged output under ignored generated directories.

No source-control mutation command was run.

## Remaining Risks

- The generic `WBReplayVerifier` remains a low-level action verifier and cannot
  represent full match-transition pause state. Complete match replay must use
  coordinator action submission.
- Compatibility raw-state callers cannot represent NPC, draw, terminal phase,
  or turn-start trigger decisions. They remain test/fixture-only and must not
  be reintroduced into production.
- A packaged command-line hook for a synthetic paused trigger choice does not
  exist; pause/resume is validated through compiled automation.

## Recommended Next Task

Add a coordinator-owned production match replay log that records and replays
all match action families, including durable turn-start trigger decisions,
without changing `WBActionCodec` or exposing private state.
