# Wandbound Active Format v1 and Game-Start Implementation Report

## Result

Active Format v1, atomic Hero setup, player-relative board regions, the
first-player Turn 1 restrictions, and the first production match are
implemented in deterministic C++. Production startup reaches a public-safe
playable decision and writes `production_started`.

- Baseline automation: 1,633 succeeded.
- Final automation: 1,724 succeeded, 0 warnings, 0 failed, 0 not run.
- Editor build: succeeded.
- Game build: succeeded.
- Definitive BuildCookRun: succeeded in 154.59 seconds.
- Packaged development smoke: passed, exit 0.
- Packaged production startup: passed, exit 0.
- Replay verification: focused Hero setup replay tests passed; two packaged
  startup runs produced byte-identical result JSON.
- `git diff --check`: no whitespace errors.
- Exact final errors: none.

## Baseline

- Commit: `7a12c7e Add minimum canonical match planning and startup results`.
- Branch: `main`, synchronized with `origin/main`.
- Staged/LFS-staged files: none.
- Production definitions: 10.
- Production digest:
  `b406557f2f190818fe3460621bbbdfaf84abe53623ff26aa934588aad68bedde`.
- Startup block:
  `production_match_spec_blocked_by_canonical_deck_evidence`.

Pre-existing tracked changes were preserved in `Config/DefaultEditor.ini`,
`Docs/CardDB_Unreal_Bundle_Schema_Validation_Report.md`, the Card activation,
game-state, public-summary, resonance, runtime-result, equip header, and the
previously dirty equip/resonance/runtime test files. Pre-existing untracked
maps, Meshy content, two audit documents, `MaxHP`, `RLTotal`, and stray
terminal-named files were preserved. No task change overlaps those older hunks.

## Authoritative Rules Added

- Stored main decks contain 1-30 singleton definition IDs, require a supported
  non-Hybrid Character, and exclude Trap/NPC/unknown/unsupported definitions.
- Setup Kits contain exactly two supported Traps and two supported NPCs;
  repeated Setup Kit IDs are legal.
- Each evidenced, supported, non-Hybrid Hero appears once in its own deck and
  is removed before shuffle; six cards must remain.
- One seeded coin flip selects first player and deterministic seeded shuffles
  order both decks. Mirrored decks are legal.
- First-player markers are placed first; both Hero tiles are reserved.
- Both Heroes commit atomically before summon-trigger collection.
- Hero placement is a real summon. Own, Character, Unit, opponent, and faction
  summon observers are supported without Card ID special cases.
- Trigger batches resolve first-player then second-player. Multiple controlled
  triggers require a stable, replay-recorded ordering choice; one auto-resolves.
- Manual Reacts and priority passing are suppressed during setup while required
  and mandatory choices continue.
- Hero-effect draws precede the standard opening six and are not replaced.
- The 9x9 board uses four player-relative own rows, neutral row 4, and four
  opponent rows.
- Only the selected first player's Turn 1 is restricted: summon into own/neutral
  only; all relocation operations preserve region; neutral NPC attacks remain
  legal under ordinary range/line rules; opponent-controlled attacks are barred.

`game_start_turn_one_v1` supersedes only Rules Bible clauses that require
sequential Hero deployment or prohibit every first-player Turn 1 attack.
All non-conflicting Rules Bible clauses remain authoritative.

## Game-Start State Machine

1. `format_validation`
2. `first_player_selection`
3. `marker_placement`
4. `hero_atomic_spawn`
5. `hero_trigger_collection`
6. `first_player_hero_trigger_resolution`
7. `second_player_hero_trigger_resolution`
8. `opening_hand_draw`
9. `first_turn_ready`

Core owns every phase. Runtime loads validated documents and presents the
result; presentation never sequences or mutates setup legality.

The atomic trace order is `hero_spawn_batch_started`, two `hero_spawned`
events, `hero_spawn_batch_committed`, then
`hero_summon_triggers_collected`. The shared state contains both Heroes before
any trigger is collected or any presentation begins.

## Trigger And Opening Evidence

Trigger order decisions use stable IDs in the form
`setup_trigger:p{player}:l{local}:s{source}:{trigger}` and are included in
deterministic trace comparison. A Hero draw-one fixture resolves
`setup_trigger_draw` before six `opening_hand_draw` events and starts with
seven cards. Public traces and startup JSON contain no private card identity,
deck order, concealed marker identity, or hidden trigger payload.

## Board And Turn One

For Player 0, rows 5-8 are OwnHalf, row 4 is NeutralRow, and rows 0-3 are
OpponentHalf. Player 1 reverses the two halves. The shared relocation guard is
used by movement and represents teleport, push, pull, swap, forced movement,
and future multi-unit relocation steps. The exact diagnostics are:

- `first_player_turn_one_summon_into_opponent_half`
- `first_player_turn_one_protected_boundary_crossing`
- `first_player_turn_one_opponent_controlled_attack_forbidden`
- `setup_marker_on_reserved_hero_spawn_tile`
- `manual_react_not_allowed_during_initial_hero_setup`

## Active Format And Production Data

- Format semantic digest:
  `258c5925ae1af7a20663c96d225f21de97b339a523c4b968cc8c6d3a024529af`.
- Addendum semantic digest:
  `bdcd0d2fc19e853f874fc80eb58cb7967062faa3b8fe91f6c85a3289972d1e67`.
- Production bundle: 11 definitions, digest
  `87d2644aeb479e84a3e96967fd57901ac52aa7e283fd5cfee142d35e8659f00c`.
- `trap_generic_01` transferred with canonical identity, public text `TRAP`,
  typed damage 2, and provenance.
- Match-spec file SHA-256:
  `5fd3ba8d78af9681e3acdc4c4b58e7c2934aacbcfed83cd3504a8a37e23b03da`.

Player 0 uses Hero `char_test_01`; Player 1 uses Hero `char_test_03`. Each
stored seven-card deck contains its selected Hero plus
`char_new_1_6_18_3`, `char_new_2_4_20_2`, `char_new_3_4_16_2`,
`char_new_rl_duelist`, `char_new_rl_monolith`, and `char_test_02`.
Each Setup Kit has two Trap slots and two NPC slots. Seed `424242` selects
Player 1. Marker placement is first-player first on legal own-half tiles.

## Startup And Packaging

The definitive package is
`Saved/PackagedBuilds/ActiveFormatGameStartFinal_20260730_0252`.
Its startup result records all required safe fields with:

```text
bundle_loaded=true
match_initialized=true
hero_spawn_batch_committed=true
hero_setup_triggers_resolved=true
opening_hands_drawn=true
playable_decision_reached=true
blocked=false
result_code=production_started
first_player=1
generation=1
revision=2
```

Two packaged startup runs produced identical JSON with SHA-256
`36f3f471568bdb642888f6083aa356e52ab147cb4f9a66ec9ef4b641ed384420`.

Packaged CardDB files are the five production schemas; root manifest; bundle
manifest; lock; status; Active Format; addendum; match specification;
Character, NPC, and Trap definitions; and runtime README. Confirmed absent:
test fixtures, Godot source, Meshy files, character/source model bundles,
importer files, and unrelated audit reports.

The packaged smoke harness has no focused Turn 1 neutral-NPC scenario mode.
The exact Core legality and generation cases below provide that evidence; no
new package-only harness was invented.

## Tests

The requested 91 tests are registered exactly:

```text
Wandbound.ActiveFormat.V1.StoredDeckMinimumAccepted
Wandbound.ActiveFormat.V1.StoredDeckMaximumAccepted
Wandbound.ActiveFormat.V1.MainDeckDuplicateRejected
Wandbound.ActiveFormat.V1.TrapInMainDeckRejected
Wandbound.ActiveFormat.V1.NPCInMainDeckRejected
Wandbound.ActiveFormat.V1.NonHybridCharacterRequired
Wandbound.ActiveFormat.V1.RepeatedSetupTrapAccepted
Wandbound.ActiveFormat.V1.RepeatedSetupNPCAccepted
Wandbound.ActiveFormat.V1.SevenCardLaunchDeckAccepted
Wandbound.ActiveFormat.V1.InsufficientOpeningHandCapacityRejected
Wandbound.ActiveFormat.V1.MirroredDecksAccepted
Wandbound.Setup.Markers.HeroSpawnTileRejected
Wandbound.Setup.Markers.OtherLegalOwnHalfTileAccepted
Wandbound.Setup.Markers.BothHeroSpawnTilesReserved
Wandbound.Setup.HeroSpawn.BothHeroesCommittedAtomically
Wandbound.Setup.HeroSpawn.NeitherHeroObservesPartialSpawnState
Wandbound.Setup.HeroSpawn.SharedPostSpawnTriggerCollection
Wandbound.Setup.HeroSpawn.ReplayStable
Wandbound.Setup.HeroSummon.OwnWhenSummonedTriggers
Wandbound.Setup.HeroSummon.CharacterSummonedObserverTriggers
Wandbound.Setup.HeroSummon.UnitSummonedObserverTriggers
Wandbound.Setup.HeroSummon.OpponentSummonedObserverTriggers
Wandbound.Setup.HeroSummon.FactionObserverTriggers
Wandbound.Setup.HeroSummon.NoCardIdHardCoding
Wandbound.Setup.HeroTriggers.FirstPlayerBatchResolvesFirst
Wandbound.Setup.HeroTriggers.SecondPlayerBatchResolvesSecond
Wandbound.Setup.HeroTriggers.ControllerChoosesMultipleTriggerOrder
Wandbound.Setup.HeroTriggers.OrderChoiceHasStableActionId
Wandbound.Setup.HeroTriggers.OrderChoiceReplayStable
Wandbound.Setup.HeroTriggers.SingleTriggerAutoResolves
Wandbound.Setup.Reacts.ManualReactNotGenerated
Wandbound.Setup.Reacts.ManualReactSubmissionRejected
Wandbound.Setup.Reacts.RequiredTriggerChoiceStillAllowed
Wandbound.Setup.Reacts.MandatoryNestedTriggerContinues
Wandbound.Setup.Reacts.NormalReactAvailabilityRestoredAfterSetup
Wandbound.Setup.OpeningHand.HeroDrawOccursBeforeOpeningSix
Wandbound.Setup.OpeningHand.HeroDrawOneProducesSevenCardStartingHand
Wandbound.Setup.OpeningHand.PreOpeningCardsNotReplaced
Wandbound.Setup.OpeningHand.PrivateCardIdentitiesNotPublic
Wandbound.Setup.OpeningHand.TraceReasonsDistinct
Wandbound.Rules.BoardRegion.Player0Perspective
Wandbound.Rules.BoardRegion.Player1Perspective
Wandbound.Rules.BoardRegion.MiddleRowNeutralForBoth
Wandbound.Rules.BoardRegion.HalvesReverseByPlayer
Wandbound.Rules.TurnOne.RestrictionAppliesOnlyToFirstPlayer
Wandbound.Rules.TurnOne.RestrictionEndsAfterFirstPlayersFirstTurn
Wandbound.Rules.TurnOne.SecondPlayersFirstTurnNotRestricted
Wandbound.Rules.TurnOne.SetupNotRestricted
Wandbound.Rules.TurnOne.SummonOwnHalfAccepted
Wandbound.Rules.TurnOne.SummonNeutralRowAccepted
Wandbound.Rules.TurnOne.SummonOpponentHalfRejected
Wandbound.Rules.TurnOne.SummonIsNotMovement
Wandbound.Rules.TurnOne.IllegalSummonNotGenerated
Wandbound.Rules.TurnOne.MoveWithinOwnHalfAccepted
Wandbound.Rules.TurnOne.MoveOwnHalfToNeutralRejected
Wandbound.Rules.TurnOne.MoveOwnHalfToOpponentHalfRejected
Wandbound.Rules.TurnOne.MultiTilePathCrossingNeutralRejected
Wandbound.Rules.TurnOne.TeleportCrossingBoundaryRejected
Wandbound.Rules.TurnOne.PushFriendlyAcrossBoundaryRejected
Wandbound.Rules.TurnOne.PushEnemyAcrossBoundaryRejected
Wandbound.Rules.TurnOne.PullNeutralAcrossBoundaryRejected
Wandbound.Rules.TurnOne.SwapCrossingEitherBoundaryRejected
Wandbound.Rules.TurnOne.RelocationEffectCannotBypassRestriction
Wandbound.Rules.TurnOne.EnemyUnitMovementWithinOpponentHalfNotRejectedByBoundaryRule
Wandbound.Rules.TurnOne.IllegalRelocationNotGenerated
Wandbound.Rules.TurnOne.NeutralNPCAttackOwnHalfAccepted
Wandbound.Rules.TurnOne.NeutralNPCAttackNeutralRowAccepted
Wandbound.Rules.TurnOne.NeutralNPCAttackOpponentHalfAccepted
Wandbound.Rules.TurnOne.OpponentHeroAttackRejected
Wandbound.Rules.TurnOne.OpponentCharacterAttackRejected
Wandbound.Rules.TurnOne.NeutralAttackStillRequiresRange
Wandbound.Rules.TurnOne.NeutralAttackStillRequiresLineOfSight
Wandbound.Rules.TurnOne.NeutralAttackGenerated
Wandbound.Rules.TurnOne.OpponentAttackNotGenerated
Wandbound.Production.ActiveFormatV1.Loads
Wandbound.Production.ActiveFormatV1.DigestPinned
Wandbound.Production.GameStartAddendum.Loads
Wandbound.Production.GameStartAddendum.DigestPinned
Wandbound.Production.InitialMatch.DecksValid
Wandbound.Production.InitialMatch.SetupKitsValid
Wandbound.Production.InitialMatch.HeroesSpawnAtomically
Wandbound.Production.InitialMatch.OpeningHandsDrawn
Wandbound.Production.InitialMatch.FirstDecisionReached
Wandbound.Production.InitialMatch.StartupResultProductionStarted
Wandbound.Authority.GameStart.CoreOwnsSetupRules
Wandbound.Authority.GameStart.RuntimeCannotMutateLegality
Wandbound.Authority.GameStart.PresentationCannotSequenceCoreSpawn
Wandbound.Authority.TurnOne.SharedRelocationGuardUsed
Wandbound.Authority.NoCharacterModelsImported
Wandbound.Authority.NoMeshyDependencyAdded
Wandbound.Authority.NoGodotFilesModified
```

## Changed Files

All task files were clean or absent at baseline; none overlaps an older dirty
hunk.

Schema/data files provide strict contracts, the 11-definition ready bundle,
the first match, and updated synthetic Trap compatibility:

```text
Data/CardDB/ActiveFormat.schema.json
Data/CardDB/GameStartAddendum.schema.json
Data/CardDB/ProductionCardDB.schema.json
Data/CardDB/ProductionMatchSpec.schema.json
Data/CardDB/Production/InitialCanonical/README.md
Data/CardDB/Production/InitialCanonical/bundle_lock.json
Data/CardDB/Production/InitialCanonical/bundle_manifest.json
Data/CardDB/Production/InitialCanonical/match_status.json
Data/CardDB/Production/InitialCanonical/active_format_v1.json
Data/CardDB/Production/InitialCanonical/game_start_addendum_v1.json
Data/CardDB/Production/InitialCanonical/match_spec.json
Data/CardDB/Production/InitialCanonical/definitions/traps.json
Data/CardDB/TestFixtures/ProductionPipeline/bundles/markers.json
Data/CardDB/TestFixtures/ProductionPipeline/match_spec.json
```

Documentation files register canon, update the runtime contract, and record
validation:

```text
Docs/Active_Format_Game_Start_Implementation_Report.md
Docs/Build_Test_Report.md
Docs/CardDB_Production_Runtime.md
Docs/Wandbound_Active_Format_v1.json
Docs/Wandbound_Active_Format_v1.md
Docs/Wandbound_Game_Start_and_Turn_One_Addendum_v1.json
Docs/Wandbound_Game_Start_and_Turn_One_Addendum_v1.md
```

CardDB files implement typed format/addendum loading, match v2 validation,
Trap semantics, and packaged staging:

```text
Source/WandboundCardDB/Private/WBActiveFormat.cpp
Source/WandboundCardDB/Private/WBGameStartAddendum.cpp
Source/WandboundCardDB/Private/WBProductionCardDatabase.cpp
Source/WandboundCardDB/Private/WBProductionMatchSpecificationV2.cpp
Source/WandboundCardDB/Public/WBActiveFormat.h
Source/WandboundCardDB/Public/WBGameStartAddendum.h
Source/WandboundCardDB/Public/WBProductionMatchSpecification.h
Source/WandboundCardDB/WandboundCardDB.Build.cs
```

Core files own setup, board regions, legality, and deterministic mutation:

```text
Source/WandboundCore/Private/WBBoardRegion.cpp
Source/WandboundCore/Private/WBInitialHeroSetup.cpp
Source/WandboundCore/Private/WBMarkerResolution.cpp
Source/WandboundCore/Private/WBMatchCoordinator.cpp
Source/WandboundCore/Private/WBRules.cpp
Source/WandboundCore/Private/WBSummonExecution.cpp
Source/WandboundCore/Private/WBTurnOneRestrictions.cpp
Source/WandboundCore/Public/WBBoardRegion.h
Source/WandboundCore/Public/WBCardDefinition.h
Source/WandboundCore/Public/WBGameStateData.h
Source/WandboundCore/Public/WBInitialHeroSetup.h
Source/WandboundCore/Public/WBMatchCoordinator.h
Source/WandboundCore/Public/WBTurnOneRestrictions.h
```

Runtime files validate production handoff and write the public-safe startup
result without owning rules:

```text
Source/WandboundRuntime/Private/WBProductionRuntimeBootstrap.cpp
Source/WandboundRuntime/Private/WBProductionStartupResult.cpp
Source/WandboundRuntime/Private/WBRuntimeMatchBootstrapActor.cpp
Source/WandboundRuntime/Private/WBRuntimeMatchHostComponent.cpp
Source/WandboundRuntime/Public/WBProductionRuntimeBootstrap.h
Source/WandboundRuntime/Public/WBProductionStartupResult.h
```

Test files provide the 91 exact cases and update superseded expectations:

```text
Source/WandboundTests/Private/WBActiveFormatGameStartTests.cpp
Source/WandboundTests/Private/WBInitialCanonicalCardDBTests.cpp
Source/WandboundTests/Private/WBMarkerResolutionTests.cpp
Source/WandboundTests/Private/WBMatchCoordinatorTests.cpp
Source/WandboundTests/Private/WBMinimumCanonicalMatchAuditTests.cpp
Source/WandboundTests/Private/WBNPCPhaseResolutionTests.cpp
Source/WandboundTests/Private/WBProductionCardDatabaseTests.cpp
Source/WandboundTests/Private/WBRuntimeMatchHostTests.cpp
```

## Git Status

No files are staged, LFS-staged, committed, pushed, deleted, restored, reset,
or cleaned. Generated package, staging, build, log, and automation output stays
ignored. The unrelated pre-existing tracked and untracked entries listed in
the baseline remain present and untouched.

## Remaining Risks

- Setup trigger payload support currently covers deterministic draw effects;
  target/mode payloads beyond the represented trigger definition still need
  production mechanics before corresponding cards can be admitted.
- The packaged smoke coordinator cannot directly select the focused Turn 1
  neutral-NPC attack scenario, although Core legality and generation are
  covered and passing.

## Recommended Next Task

Implement production turn-start resource setup and draw from the initialized
Active Format match, preserving the new setup trace and public-safe startup
contract.
