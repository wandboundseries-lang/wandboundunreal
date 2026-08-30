# Vocabulary Foundation Reconciliation Audit

## Baseline

- Commit: `d6933215acf310bcfd3b5444387d980009677227`
- Subject: `Add production Terrain Cartographers`
- `HEAD` and `origin/main` matched before implementation.
- Baseline automation: 2,391 succeeded, 0 failed, 0 warnings, 0 not run.
- Replay schema: 1.
- `WBActionCodec` was not modified.
- No task files were staged, committed, or pushed.

## Canonical identity model

`FWBUnitState` now represents two independent identities:

- `OwnerPlayerId`: immutable player to whom the unit card fundamentally belongs.
- `ControllerPlayerId`: player with current gameplay authority over the board unit.
- `OwnerId`: deprecated compatibility mirror of current Controller. Legacy direct writes remain observable through `GetControllerPlayerIdForRules`; production creation and mutation use the explicit setters.

Ordinary units have Owner equal to Controller. Opponent-controlled units retain their original Owner and contribute only to their Controller's controlled-unit membership.

### Field classification

| Type / field | Classification | Notes |
| --- | --- | --- |
| `FWBUnitState::OwnerPlayerId` | immutable Owner | New canonical ownership authority. |
| `FWBUnitState::ControllerPlayerId` | current Controller | New canonical action/relationship authority. |
| `FWBUnitState::OwnerId` | compatibility | Mirrors Controller; retained for legacy fixtures and serializers. |
| `FWBCardInstanceRef::OwnerPlayerId` | immutable Owner | Existing exact card-instance ownership; unchanged by control. |
| `FWBBoardCardReference::OwnerPlayerId` | compatibility current-control context | Source-gate board references historically used this value for authority. |
| `FWBCardActivationFixtureZoneEntry::OwnerPlayerId` | zone-entry owner | Existing fixture/source-zone ownership. |
| `FWBCardZoneObservation` owner fields | immutable card owner | Viewer-safe card-zone observations. |
| `FWBCardActivationLegalAction::TargetOwnerPlayerId` | legacy target authority label | Existing legal-action payload; no new ownership semantics added. |
| `FWBUnitDestructionSnapshot` Owner/Controller | historical snapshot | Both values captured at destruction time. |
| `FWBActivatedEffectSourceSnapshot` Owner/Controller | historical snapshot | Both values captured when continuation state is created. |
| `FWBPostDestructionObserverSourceSnapshot` Owner/Controller | historical snapshot | Both values captured before later resolution. |
| `FWBCSNInheritanceSourceData::ControllerPlayerId` | historical Controller | Existing inheritance authority; immutable card ownership remains in exact instances. |
| `FWBPendingAttackState::AttackingPlayerId` | historical actor/controller | Not card ownership. |
| reaction origin/priority fields | action authority | Not ownership. |
| mandatory-choice Controller fields | decision authority | Private player allowed to choose. |
| NPC phase owner fields | phase authority | Turn/phase ownership, not card ownership. |
| marker owner fields | marker ownership | Existing marker identity; not spawned-unit Controller. |
| resonance summary `OwnerPlayerId` | compatibility current controller | Existing public/API naming retained; production values now come from explicit Controller. |
| public unit summary Owner/Controller | public identity | Both canonical values are serialized; legacy `owner_id` still mirrors Controller. |
| runtime presentation `OwnerId` | compatibility current controller | Presentation does not own rules truth. |

Production legality, friendly/enemy tests, unit-cap membership, activation authority, resonance authority, NPC classification, and runtime providers now call explicit identity accessors rather than interpreting `OwnerId` directly.

## Marrow Informant reference audit

The read-only Godot implementation treats its historical unit `owner_id` as current control. `query_summon_under_opponent_control` validates the opponent's unit cap and summon authority; reveal behavior follows the current controller. The Unreal foundation can represent Player 0 owning an exact Informant instance while Player 1 controls its unit. No Informant CardId branch or production card transfer was added.

## Declaration provenance

`EWBDeclarationProvenance` has two states, `Automatic` and `PlayerDeclared`, for attack and target selection. Activation uses the independent three-state `EWBActivationProvenance`: `ResolutionOnly`, `AutomaticActivation`, and `PlayerDeclared`.

- Player attack: declared attack and declared target.
- NPC attack: automatic attack and automatic target.
- Counterattack: still an attack, but attack and target provenance are reset to automatic.
- Player card activation: activation exists and is declared.
- Automatic unit activation: activation exists but is not declared.
- Passive, observer, trigger-payload, and game-rule resolution: resolution-only unless an existing authority explicitly creates an activation.
- Explicit unit/tile/wall selection: declared target.
- Explicit auxiliary exact-card selection: declared target.
- Redirected, substituted, reflected, fixed, or automatically selected recipients: not declared targets.

Trace events serialize declaration flags only when true. This keeps default automatic events compact and deterministic.

## Required card classifications

- Terrain Cartographer: declared activation; chosen tile declared; tile is fixed before response processing.
- CSN Rook: destruction trigger is resolution-only; private mandatory exact Deck choice declared target; former tile fixed and not declared.
- CSN Patch: initial activation declared; later private mandatory exact Deck choice declared target; former tile fixed and not declared.
- CSN Crash-In: explicit exact Hand replacement choice declared target; defender/destination fixed by the effect and not newly declared.
- Counterattack: attack yes, declared attack no, declared target no.
- Blackcoin, Body Double, redirects, substitutions, reflection, and final-recipient transforms: automatic recipient changes remain non-declared.

## Caster mapping

Caster is the unit that activated an effect. Declaration provenance is independent of Caster provenance. Declared and automatic unit activations may both have a Caster; automatic resolution-only effects have no Caster unless a rule explicitly establishes an activation.

No redundant persisted Caster field was added. `GetCasterUnitId` returns `SourceUnitId` only when activation provenance establishes that an activation occurred and a source unit exists. A source unit alone does not imply Caster, and declared provenance on a non-unit activation does not fabricate one. Player-declared Character, Cartographer, and equipped-Wand activations retain their activating-unit Caster. Rook, Sable, Undertow, destruction observers, after-damage observers, and other existing automatic resolution paths remain `ResolutionOnly`; none was converted into an activation.

## Snapshots, replay, and privacy

Destruction, activated-effect, and post-destruction observer snapshots preserve both event-time Owner and Controller. State digests add immutable owner data only when it differs from Controller, preserving ordinary-state compatibility while distinguishing split identities. Pending attack declaration provenance participates in state digests.

Replay schema remains 1 and `WBActionCodec` remains unchanged. The public receipt remains eight fields. Private exact Deck/Hand instance choices stay in authoritative private traces and are not added to opponent observations or public receipts. Public board summaries expose board-unit Owner and Controller, which are public board identity facts, while retaining `owner_id` as the legacy Controller mirror.

## Sacrifice, NPC, and Trap confirmation

Sacrifice remains distinct from destruction. Patch and Hybrid sacrifice paths do not create destruction events, do not invoke Sable/Rook destruction triggers, and are not converted into destruction by this pass. NPC continues to mean a neutral spawned board unit; NPC marker identity remains separate. Trap marker identity and Trap resolution remain separate from spawned-unit control. No NPC or Trap gameplay was redesigned.

## Fail-closed and deferred rules

- Hero control changes remain unsupported because canon does not define them.
- Equipping a Wand to a unit whose Owner differs from Controller fails closed with `controlled_unit_wand_ownership_unsupported`; exact Wand ownership is never rewritten.
- A controlled-by-opponent unit's leaving-board destination is unresolved. No Owner/Controller discard rule was invented.
- Marrow Informant is foundation evidence only, not a production card transfer.
- Sealed is deferred. Its future pass must integrate RL use/modification/transfer, equip affordability, overflow, equipped-Wand eligibility, status application/removal, Cleanse, replay, public state, and AI/legal actions.

## Test coverage

New `Wandbound.Vocabulary` tests cover ordinary and split identity, exact card-instance stability, controlled/owned queries, move authority, friendly/enemy semantics, public identity, player and automatic attack provenance, declared Character and equipped-Wand Caster mapping, synthetic automatic unit activation, resolution-only source/no-Caster behavior, declared non-unit activation/no-Caster behavior, trace serialization, and state-digest distinction.

Existing focused tests were extended for Rook, Patch, Crash-In, Cartographer, and counterattack provenance. Existing CSN, sacrifice, Sable, Undertow, Vex, Blackcoin, redirect/substitute, NPC, Trap, replay, privacy, and receipt tests remain part of the full Wandbound suite.

## Validation evidence

- Editor non-unity compile: succeeded after two mechanical compile corrections; final identity compatibility and test rebuilds succeeded.
- Game non-unity compile: succeeded with a fresh 282-action graph.
- Final commit-gate Editor default compile: succeeded with 11 actions in 108.04 seconds.
- Final commit-gate Game default compile: succeeded with 9 actions in 106.33 seconds.
- Final commit-gate Editor forced-unity rebuild: succeeded with a fresh 243-action graph in 312.07 seconds.
- Final commit-gate Game forced-unity rebuild: succeeded with a fresh 218-action graph in 344.14 seconds.
- The first default Editor attempt exposed duplicate anonymous helper names in the baseline Terrain and Blackcoin smoke unity translation unit. Terrain-local helpers were given unique names only; smoke behavior is unchanged.
- Focused `Wandbound.Vocabulary`: 16/16 succeeded after the Caster correction.
- Focused `Wandbound.Core.CardActivation`: 101/101 succeeded.
- Focused `Wandbound.Reaction`: 37/37 succeeded.
- Focused `Wandbound.PendingEffect`: 24/24 succeeded.
- Focused `Wandbound.CSNRook`: 11/11 succeeded with an explicit resolution-only lifecycle assertion.
- Caster-correction Editor non-unity compile: succeeded with a fresh 297-action graph in 472.24 seconds.
- Final test-only Editor incremental compile: succeeded in 51.92 seconds after compiling the tightened Rook assertion.
- Focused `Wandbound.CSN`: 85/85 succeeded after compatibility correction.
- Final full automation after the Caster correction: 2,407 succeeded, 0 failed, 0 warnings, 0 not run; delta +16 from baseline and +2 from the prior reconciliation result.
- Caster-correction Game non-unity compile: succeeded with a fresh 282-action graph in 411.76 seconds.
- Final clean BuildCookRun after the Caster correction: succeeded in 609.42 seconds with a fresh 461-action Editor/Game graph, full cook, stage, package, and archive.
- Packaged Terrain smoke: two runs exited 0 with empty stderr and byte-identical archive, receipt, startup, state, trace, and replay hashes.
- Packaged archive SHA-256: `5953dab4f04cabe5e5d3ba24dd8421963562a59ddf20e3af84ec69d363bd5ced`.
- Packaged receipt SHA-256: `792e76edb52447f09523a8aa699d11c72385105674a5d3f7c7bb704ff82635fb`; exactly eight fields and privacy scan clean.
- Packaged startup SHA-256: `c1589b0b9f442008164c71854ffc1ded9dbf77f74160daef9a4ff550f194d99c`; unchanged from baseline.
- Final state digest: `dad14fc8bc0a0cf9c6fe3f9c63bdba52497a7e296502bf12ce3136e4eded03d9`; unchanged from baseline.
- Final trace digest: `2bd3ec042c7379fa1e49e7247c7b0849d750beb5010d6e7c218014d03a9ebe53`.
- Replay digest: `4ca4e049f130cb83392fc115f0148d3852879116a4bfe5f40f08d5380cd77799`.
- Fresh replay inside the packaged smoke matched the live final state and trace digests on both runs; each archive contains 22 accepted records and ends at generation/revision 1/23.
- Replay schema remains 1; first baseline archive difference is record 6, the Rimecall Cartographer activation. State, legal set, action ID, generation, and revision are unchanged; only the trace/record chain changes for the new declaration provenance.
- Source/package SHA-256 matches for the production root manifest and Terrain fixture.

## Exact changed-file manifest

The task-owned set is the two audit files, `Source/WandboundTests/Private/WBVocabularyFoundationTests.cpp`, and the following tracked source/test paths:

```text
Source/WandboundCore/Private/WBActivatedDeckSummonContinuation.cpp
Source/WandboundCore/Private/WBAfterDamageTrigger.cpp
Source/WandboundCore/Private/WBCSNInheritance.cpp
Source/WandboundCore/Private/WBCSNInheritanceTrigger.cpp
Source/WandboundCore/Private/WBCardActivationAffordability.cpp
Source/WandboundCore/Private/WBCardActivationCandidateGenerator.cpp
Source/WandboundCore/Private/WBCardActivationCostPayment.cpp
Source/WandboundCore/Private/WBCardActivationExpansion.cpp
Source/WandboundCore/Private/WBCardActivationSourceGate.cpp
Source/WandboundCore/Private/WBCardZoneState.cpp
Source/WandboundCore/Private/WBDeathResolution.cpp
Source/WandboundCore/Private/WBDeckSummon.cpp
Source/WandboundCore/Private/WBEffectRunner.cpp
Source/WandboundCore/Private/WBEquipExecution.cpp
Source/WandboundCore/Private/WBGameStateData.cpp
Source/WandboundCore/Private/WBHybridSummon.cpp
Source/WandboundCore/Private/WBInitialHeroSetup.cpp
Source/WandboundCore/Private/WBMandatoryDeckChoice.cpp
Source/WandboundCore/Private/WBMarkerResolution.cpp
Source/WandboundCore/Private/WBMatchCoordinator.cpp
Source/WandboundCore/Private/WBNPCPhaseResolution.cpp
Source/WandboundCore/Private/WBPostDestructionTrigger.cpp
Source/WandboundCore/Private/WBPreDamageAttackTrigger.cpp
Source/WandboundCore/Private/WBProductionMatchReplay.cpp
Source/WandboundCore/Private/WBPublicBoardSummary.cpp
Source/WandboundCore/Private/WBReplayTrace.cpp
Source/WandboundCore/Private/WBResonanceLoad.cpp
Source/WandboundCore/Private/WBResonanceOverflow.cpp
Source/WandboundCore/Private/WBResonanceRecalculation.cpp
Source/WandboundCore/Private/WBRules.cpp
Source/WandboundCore/Private/WBRuntimeResultSerializer.cpp
Source/WandboundCore/Private/WBSummonExecution.cpp
Source/WandboundCore/Private/WBTurnStartSequence.cpp
Source/WandboundCore/Private/WBUnitReplacementEffect.cpp
Source/WandboundCore/Private/WBUnitStatDelta.cpp
Source/WandboundCore/Private/WBUnitStatQuery.cpp
Source/WandboundCore/Public/WBCardActivationCommand.h
Source/WandboundCore/Public/WBEffectRequest.h
Source/WandboundCore/Public/WBGameStateData.h
Source/WandboundCore/Public/WBPublicBoardSummary.h
Source/WandboundCore/Public/WBReplayTrace.h
Source/WandboundCore/Public/WBTypes.h
Source/WandboundRuntime/Private/WBBoardViewDemoData.cpp
Source/WandboundRuntime/Private/WBProductionActivationDataProvider.cpp
Source/WandboundRuntime/Private/WBProductionResonanceOverflowHandoff.cpp
Source/WandboundRuntime/Private/WBProductionSummonEquipDataProvider.cpp
Source/WandboundRuntime/Private/WBProductionTerrainCartographerSmoke.cpp
Source/WandboundRuntime/Private/WBRuntimeMatchHostComponent.cpp
Source/WandboundTests/Private/WBCSNCrashInTests.cpp
Source/WandboundTests/Private/WBCSNPatchTests.cpp
Source/WandboundTests/Private/WBCSNRookTests.cpp
Source/WandboundTests/Private/WBNPCCombatAuthorityAndCounterabilityTests.cpp
Source/WandboundTests/Private/WBTerrainCartographerTests.cpp
```

Unrelated `Content/Maps/NewProjectTest.umap`, `Plugins/meshy/Content/Materials/M_MeshyPBR.uasset`, and `h origin main` remain untouched.
