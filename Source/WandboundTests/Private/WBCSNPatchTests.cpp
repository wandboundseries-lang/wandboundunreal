#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#include "WBActivatedDeckSummonContinuation.h"
#include "WBCardDefinitionRepository.h"
#include "WBCardZoneObservation.h"
#include "WBCardZoneState.h"
#include "WBDeathResolution.h"
#include "WBMandatoryDeckChoice.h"
#include "WBMatchCoordinator.h"
#include "WBPostDestructionTrigger.h"
#include "WBProductionCardDatabase.h"
#include "WBProductionCSNCrashInSmoke.h"
#include "WBProductionMatchReplay.h"
#include "WBUnitStatQuery.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 PatchSourceId = 10;

FWBGenericEffectPayload MakeDeckSummonPayload()
{
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::
		SacrificeSourceThenSummonCharacterFromDeckToSourceTile;
	Payload.RequiredSourceFaction = TEXT("csn");
	Payload.RequiredReplacementFaction = TEXT("csn");
	Payload.RequiredReplacementKind = EWBEffectReplacementCardKind::Character;
	Payload.InheritancePolicy = EWBEffectInheritancePolicy::
		TransferEquippedWandsAndAddSourceCurrentRL;
	return Payload;
}

FWBCardEffectDefinition MakeDeckSummonEffect()
{
	FWBCardEffectDefinition Effect;
	Effect.EffectId = TEXT("semantic_self_sacrifice_deck_summon");
	Effect.PublicLabel = TEXT("Sacrifice to summon");
	Effect.TargetRequirement = EWBCardEffectTargetRequirement::None;
	Effect.SourceGate.RequiredZone = EWBCardActivationSourceZone::Board;
	Effect.SourceGate.Timing =
		EWBCardActivationTimingRequirement::NormalTurnPriority;
	Effect.SourceGate.bRequiresFixtureZoneOwnership = true;
	Effect.SourceGate.bRequiresSourceUnit = true;
	Effect.SourceGate.bRequiresSourceUnitOwnership = true;
	Effect.SourceGate.bRequiresCostsSatisfiedExternally = true;
	Effect.SourceGate.bBlockedByStunned = true;
	Effect.SourceGate.bOncePerTurn = true;
	Effect.SourceGate.OncePerTurnKey = TEXT("semantic_self_sacrifice_once");
	Effect.SourceGate.bHasExplicitSourceGate = true;
	Effect.Payloads.Add(MakeDeckSummonPayload());
	return Effect;
}

FWBCardEffectDefinition MakeNegateEffect()
{
	FWBCardEffectDefinition Effect;
	Effect.EffectId = TEXT("patch_test_negate");
	Effect.PublicLabel = TEXT("Negate");
	Effect.TargetRequirement = EWBCardEffectTargetRequirement::None;
	Effect.SourceGate.RequiredZone = EWBCardActivationSourceZone::Board;
	Effect.SourceGate.Timing = EWBCardActivationTimingRequirement::ResponseWindow;
	Effect.SourceGate.bRequiresFixtureZoneOwnership = true;
	Effect.SourceGate.bRequiresSourceUnit = true;
	Effect.SourceGate.bRequiresSourceUnitOwnership = true;
	Effect.SourceGate.bRequiresCostsSatisfiedExternally = true;
	Effect.SourceGate.bBlockedByStunned = true;
	Effect.SourceGate.bOncePerTurn = true;
	Effect.SourceGate.OncePerTurnKey = TEXT("patch_test_negate_once");
	Effect.SourceGate.bHasExplicitSourceGate = true;
	FWBGenericEffectPayload Payload;
	Payload.Operation = EWBGenericEffectOp::NegatePendingEffect;
	Effect.Payloads.Add(Payload);
	return Effect;
}

FWBCardDefinition MakeCharacter(
	const FString& CardId,
	const FString& Faction = TEXT("csn"),
	const int32 BaseRL = 2)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.PublicCategory = TEXT("Character");
	Definition.Kind = EWBCardDefinitionKind::Character;
	if (!Faction.IsEmpty()) Definition.PublicFactions.Add(Faction);
	Definition.CharacterStats.HP = 12;
	Definition.CharacterStats.ATK = 2;
	Definition.CharacterStats.AR = 2;
	Definition.CharacterStats.RL = BaseRL;
	return Definition;
}

FWBAfterUnitDestroyedTriggerDefinition MakeRookTrigger()
{
	FWBAfterUnitDestroyedTriggerDefinition Trigger;
	Trigger.TriggerId = TEXT("summon_csn_character_from_deck");
	Trigger.SourceScope = EWBAfterUnitDestroyedSourceScope::DestroyedSelf;
	Trigger.Operation = EWBPostDestructionEffectOperation::
		SummonCharacterFromDeckToDestroyedTile;
	Trigger.RequiredFaction = TEXT("csn");
	Trigger.SummonCount = 1;
	Trigger.bMandatory = true;
	Trigger.bIgnoreSummoningConditions = true;
	Trigger.bApplyCSNInheritance = true;
	return Trigger;
}

FWBCardDefinitionRepository MakeRepository(const bool bIncludeNegate = false)
{
	TArray<FWBCardDefinition> Definitions;
	Definitions.Add(MakeCharacter(TEXT("patch_test_hero"), TEXT("mage"), 4));
	FWBCardDefinition EnemyHero = MakeCharacter(
		TEXT("patch_test_enemy_hero"), TEXT("officer"), 4);
	if (bIncludeNegate) EnemyHero.ActivatedEffects.Add(MakeNegateEffect());
	Definitions.Add(MoveTemp(EnemyHero));

	FWBCardDefinition Source = MakeCharacter(TEXT("semantic_source"));
	Source.ActivatedEffects.Add(MakeDeckSummonEffect());
	Definitions.Add(Source);
	FWBCardDefinition Alternate = MakeCharacter(TEXT("alternate_identity"));
	Alternate.PublicName = TEXT("Alternate Identity");
	Alternate.ActivatedEffects.Add(MakeDeckSummonEffect());
	Definitions.Add(Alternate);
	Definitions.Add(MakeCharacter(TEXT("patch_like_without_metadata")));
	Definitions.Add(MakeCharacter(TEXT("candidate_a"), TEXT("csn"), 3));
	Definitions.Add(MakeCharacter(TEXT("candidate_b"), TEXT("csn"), 2));
	Definitions.Add(MakeCharacter(TEXT("id_contains_csn"), TEXT("officer"), 2));

	FWBCardDefinition Undertow = MakeCharacter(TEXT("candidate_undertow"));
	FWBAfterCSNInheritanceTriggerDefinition Draw;
	Draw.TriggerId = TEXT("draw_after_csn_inheritance");
	Draw.DrawCount = 1;
	Draw.bMandatory = true;
	Undertow.AfterCSNInheritanceTriggers.Add(Draw);
	Definitions.Add(Undertow);

	FWBCardDefinition Sable = MakeCharacter(TEXT("candidate_sable"));
	FWBAfterUnitDestroyedTriggerDefinition Grow;
	Grow.TriggerId = TEXT("grow_after_controlled_csn_destroyed");
	Grow.SourceScope = EWBAfterUnitDestroyedSourceScope::ControlledFactionUnitDestroyed;
	Grow.Operation = EWBPostDestructionEffectOperation::
		ApplyPersistentStatDeltaToTriggerSource;
	Grow.RequiredFaction = TEXT("csn");
	Grow.bMandatory = true;
	Grow.Target = EWBPostDestructionTarget::TriggerSource;
	Grow.StatDelta.ATKDelta = 1;
	Grow.StatDelta.MaxHPDelta = 1;
	Grow.StatDelta.CurrentHPDelta = 1;
	Sable.AfterUnitDestroyedTriggers.Add(Grow);
	Definitions.Add(Sable);

	FWBCardDefinition Vex = MakeCharacter(TEXT("candidate_vex"));
	FWBContinuousStatAuraDefinition Aura;
	Aura.AuraId = TEXT("enemy_ar_penalty_in_source_ar");
	Aura.TargetRelation = EWBContinuousAuraTargetRelation::Enemy;
	Aura.TargetStat = EWBContinuousStat::AR;
	Aura.Operation = EWBContinuousStatOperation::Add;
	Aura.Amount = -1;
	Aura.RangeStat = EWBContinuousAuraRangeStat::AR;
	Aura.Geometry = EWBContinuousAuraGeometry::AttackLine;
	Aura.bBlockedByWalls = true;
	Aura.bBlockedByUnits = true;
	Aura.MinimumResult = 0;
	Vex.ContinuousStatAuras.Add(Aura);
	Definitions.Add(Vex);

	FWBCardDefinition Rook = MakeCharacter(TEXT("candidate_rook"));
	Rook.AfterUnitDestroyedTriggers.Add(MakeRookTrigger());
	Definitions.Add(Rook);

	FWBCardDefinition Wand;
	Wand.CardId = TEXT("patch_test_wand");
	Wand.PublicName = TEXT("Patch Test Wand");
	Wand.PublicCategory = TEXT("Wand");
	Wand.Kind = EWBCardDefinitionKind::Wand;
	Wand.PublicFactions.Add(TEXT("csn"));
	Wand.WandStats.RR = 1;
	Definitions.Add(Wand);
	FWBCardDefinition Action;
	Action.CardId = TEXT("csn_action");
	Action.PublicName = TEXT("CSN Action");
	Action.PublicCategory = TEXT("Action");
	Action.PublicFactions.Add(TEXT("csn"));
	Action.Kind = EWBCardDefinitionKind::Action;
	Definitions.Add(Action);
	FWBCardDefinition Filler = Action;
	Filler.CardId = TEXT("patch_test_filler");
	Filler.PublicName = TEXT("Patch Test Filler");
	Filler.PublicFactions.Reset();
	Definitions.Add(Filler);
	FWBCardDefinition Trap;
	Trap.CardId = TEXT("patch_test_trap");
	Trap.PublicName = TEXT("Patch Test Trap");
	Trap.PublicCategory = TEXT("Trap");
	Trap.Kind = EWBCardDefinitionKind::Trap;
	Trap.TrapDamage = 1;
	Definitions.Add(Trap);
	FWBCardDefinition NPC = MakeCharacter(
		TEXT("patch_test_npc"), TEXT("officer"));
	NPC.Kind = EWBCardDefinitionKind::NPC;
	Definitions.Add(NPC);

	FWBCardDefinitionRepository Repository;
	WBCardDefinitionRepository::BuildRepositoryFromDefinitions(
		TEXT("patch_tests"), TEXT("v1"), Definitions, Repository);
	return Repository;
}

FWBUnitState MakeUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FString& CardId,
	const FWBTile Tile,
	const int32 BaseRL = 2,
	const int32 CurrentRL = 2,
	const int32 RLUsed = 0)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = CardId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = 12;
	Unit.MaxHP = 12;
	Unit.ATK = 2;
	Unit.AR = 2;
	Unit.SetCanonicalRL(BaseRL, CurrentRL, RLUsed);
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	return Unit;
}

FWBGameStateData MakeState(
	const FString& SourceCardId = TEXT("semantic_source"),
	const bool bSourceIsHero = false)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 3;
	State.Phase = EWBGamePhase::NormalTurn;
	FWBPlayerStateData Player0;
	Player0.PlayerId = 0;
	Player0.HeroUnitId = bSourceIsHero ? PatchSourceId : 1;
	FWBPlayerStateData Player1;
	Player1.PlayerId = 1;
	Player1.HeroUnitId = 2;
	State.Players = { Player0, Player1 };
	if (!bSourceIsHero)
	{
		State.AddUnitForTest(MakeUnit(
			1, 0, TEXT("patch_test_hero"), FWBTile(4, 8), 4, 4));
	}
	State.AddUnitForTest(MakeUnit(
		2, 1, TEXT("patch_test_enemy_hero"), FWBTile(4, 0), 4, 4));
	State.AddUnitForTest(MakeUnit(
		PatchSourceId, 0, SourceCardId, FWBTile(4, 4), 2, 5, 1));
	FWBPlayerCardZoneState Zones0;
	Zones0.PlayerId = 0;
	FWBPlayerCardZoneState Zones1;
	Zones1.PlayerId = 1;
	State.GetMutableCardZoneStateForTest().PlayerZones = { Zones0, Zones1 };
	return State;
}

void AddDeckCard(
	FWBGameStateData& State,
	const FString& InstanceId,
	const FString& CardId,
	const int32 PlayerId = 0)
{
	FWBPlayerCardZoneState* Zones = WBCardZoneState::FindMutablePlayerZones(
		State.GetMutableCardZoneStateForTest(), PlayerId);
	FWBZoneCardEntry Entry;
	Entry.Card.InstanceId = InstanceId;
	Entry.Card.CardId = CardId;
	Entry.Card.OwnerPlayerId = PlayerId;
	Entry.Zone = EWBCardZone::Deck;
	Entry.ZoneIndex = Zones->Deck.Num();
	Zones->Deck.Add(MoveTemp(Entry));
}

void AddWand(
	FWBGameStateData& State,
	const FString& InstanceId,
	const int32 EquipOrder)
{
	FWBEquippedCardEntry Entry;
	Entry.Card.InstanceId = InstanceId;
	Entry.Card.CardId = TEXT("patch_test_wand");
	Entry.Card.OwnerPlayerId = 0;
	Entry.EquippedToUnitId = PatchSourceId;
	Entry.SlotId = FString::Printf(TEXT("slot_%d"), EquipOrder);
	Entry.EquipOrder = EquipOrder;
	State.GetMutableCardZoneStateForTest().EquippedCards.Add(MoveTemp(Entry));
}

FWBCardActivationCommand MakeCommand(
	const FString& SourceCardId = TEXT("semantic_source"))
{
	FWBCardActivationCommand Command;
	Command.Source.PlayerId = 0;
	Command.Source.SourceUnitId = PatchSourceId;
	Command.Source.SourceCardId = SourceCardId;
	Command.Source.SourceZone = EWBCardZone::Board;
	Command.Source.SourceEffectId =
		TEXT("semantic_self_sacrifice_deck_summon");
	Command.EffectRequest.Source.PlayerId = 0;
	Command.EffectRequest.Source.SourceUnitId = PatchSourceId;
	Command.EffectRequest.Source.SourceCardId = SourceCardId;
	Command.EffectRequest.Source.SourceEffectId = Command.Source.SourceEffectId;
	Command.EffectRequest.Payloads.Add(MakeDeckSummonPayload());
	return Command;
}

FWBActivatedDeckSummonContinuationResult ResolveContinuation(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const FString& SourceCardId = TEXT("semantic_source"))
{
	return WBActivatedDeckSummonContinuation::Resolve(
		State, Repository, MakeCommand(SourceCardId),
		TEXT("activate:test"), TEXT("pending:test"), 0,
		static_cast<int32>(EWBMatchLoopPhase::Action));
}

const FWBUnitState* FindBoardCard(
	const FWBGameStateData& State,
	const FString& CardId)
{
	return State.Units.FindByPredicate([&CardId](const FWBUnitState& Unit)
	{
		return Unit.CardId == CardId && Unit.IsUnitOnBoard();
	});
}

const FWBUnitState* FindBoardCardAtTile(
	const FWBGameStateData& State,
	const FString& CardId,
	const FWBTile Tile)
{
	return State.Units.FindByPredicate([&CardId, Tile](const FWBUnitState& Unit)
	{
		return Unit.CardId == CardId
			&& Unit.IsUnitOnBoard()
			&& Unit.X == Tile.X
			&& Unit.Y == Tile.Y;
	});
}

bool HasTrace(const TArray<FWBTraceEvent>& Trace, const FName Kind)
{
	return Trace.ContainsByPredicate([Kind](const FWBTraceEvent& Event)
	{
		return Event.Kind == Kind;
	});
}

FWBCardInstanceRef MakeCardRef(
	const FString& InstanceId,
	const FString& CardId,
	const int32 PlayerId)
{
	FWBCardInstanceRef Card;
	Card.InstanceId = InstanceId;
	Card.CardId = CardId;
	Card.OwnerPlayerId = PlayerId;
	return Card;
}

FWBMatchInitializationRequest MakeCoordinatorRequest(
	const bool bIncludeNegate = false)
{
	FWBMatchInitializationRequest Request;
	Request.Seed = 55319;
	Request.FirstPlayerId = 0;
	Request.Repository = MakeRepository(bIncludeNegate);
	for (int32 PlayerId = 0; PlayerId < 2; ++PlayerId)
	{
		FWBMatchPlayerSetup Setup;
		Setup.PlayerId = PlayerId;
		Setup.HeroInstanceId = FString::Printf(
			TEXT("patch_p%d_hero"), PlayerId);
		Setup.HeroCardId = PlayerId == 0
			? TEXT("patch_test_hero") : TEXT("patch_test_enemy_hero");
		Setup.HeroSpawnTile = PlayerId == 0
			? FWBTile(4, 8) : FWBTile(4, 0);
		Setup.OrderedDeck.Add(MakeCardRef(
			Setup.HeroInstanceId, Setup.HeroCardId, PlayerId));
		for (int32 Index = 0; Index < 9; ++Index)
		{
			Setup.OrderedDeck.Add(MakeCardRef(
				FString::Printf(TEXT("patch_p%d_filler_%d"), PlayerId, Index),
				TEXT("patch_test_filler"), PlayerId));
		}
		Request.Players.Add(MoveTemp(Setup));
	}
	for (int32 Index = 0; Index < 8; ++Index)
	{
		FWBSetupMarkerPlacement Marker;
		Marker.PlayerId = Index < 4 ? 0 : 1;
		Marker.Type = Index % 2 == 0 ? EWBMarkerType::Trap : EWBMarkerType::NPC;
		Marker.DefinitionId = Marker.Type == EWBMarkerType::Trap
			? TEXT("patch_test_trap") : TEXT("patch_test_npc");
		Marker.Tile = Index < 4
			? FWBTile(Index, 7) : FWBTile(Index - 4, 1);
		Marker.PlacementOrder = Index;
		Request.MarkerPlacements.Add(Marker);
	}
	return Request;
}

const FWBMatchLegalAction* FindActivation(
	const TArray<FWBMatchLegalAction>& Actions,
	const FString& EffectId)
{
	return Actions.FindByPredicate([&EffectId](const FWBMatchLegalAction& Action)
	{
		return Action.Family == EWBMatchActionFamily::Activation
			&& Action.ActivationCommand.Source.SourceEffectId == EffectId;
	});
}
}

#define WB_PATCH_TEST(ClassName, TestName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, TestName, \
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

WB_PATCH_TEST(FWBCSNPatchProductionDefinitionTest,
	"Wandbound.CSNPatch.CardDB.ProductionDefinitionAndOwnerSupersession")
bool FWBCSNPatchProductionDefinitionTest::RunTest(const FString&)
{
	const FWBProductionCardDatabaseLoadResult Loaded =
		WBProductionCardDatabase::LoadManifestSuite(FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Data/CardDB/Production/CSNCrashIn/root_manifest.json")));
	TestTrue(TEXT("Production suite loads"), Loaded.bOk);
	if (!Loaded.Snapshot.IsValid()) return false;
	AddInfo(TEXT("Production Patch bundle digest: ")
		+ Loaded.Snapshot->ContentDigest);
	const FWBProductionCardRecord* Record =
		Loaded.Snapshot->FindRecord(TEXT("char_csn_patch"));
	TestNotNull(TEXT("Patch loads"), Record);
	if (Record == nullptr) return false;
	const FWBCardDefinition& Definition = Record->CoreDefinition;
	TestEqual(TEXT("HP 12"), Definition.CharacterStats.HP, 12);
	TestEqual(TEXT("ATK 2"), Definition.CharacterStats.ATK, 2);
	TestEqual(TEXT("AR 2"), Definition.CharacterStats.AR, 2);
	TestEqual(TEXT("RL 2"), Definition.CharacterStats.RL, 2);
	TestTrue(TEXT("CSN faction"), Definition.PublicFactions.Contains(TEXT("csn")));
	TestEqual(TEXT("One activation"), Definition.ActivatedEffects.Num(), 1);
	if (Definition.ActivatedEffects.Num() != 1) return false;
	const FWBCardEffectDefinition& Effect = Definition.ActivatedEffects[0];
	TestEqual(TEXT("Board source"), Effect.SourceGate.RequiredZone,
		EWBCardActivationSourceZone::Board);
	TestEqual(TEXT("Normal priority"), Effect.SourceGate.Timing,
		EWBCardActivationTimingRequirement::NormalTurnPriority);
	TestTrue(TEXT("Source required"), Effect.SourceGate.bRequiresSourceUnit);
	TestTrue(TEXT("Ownership required"),
		Effect.SourceGate.bRequiresSourceUnitOwnership);
	TestTrue(TEXT("Once per turn"), Effect.SourceGate.bOncePerTurn);
	TestEqual(TEXT("No target"), Effect.TargetRequirement,
		EWBCardEffectTargetRequirement::None);
	TestEqual(TEXT("Generic payload"), Effect.Payloads[0].Operation,
		EWBGenericEffectOp::SacrificeSourceThenSummonCharacterFromDeckToSourceTile);
	TestFalse(TEXT("No Wand tutor language"),
		Definition.PublicRulesText.Contains(TEXT("Add 1 Wand"), ESearchCase::IgnoreCase));
	TestFalse(TEXT("No old choice purpose"),
		Definition.PublicRulesText.Contains(TEXT("csn_patch_wand"), ESearchCase::IgnoreCase));
	return true;
}

WB_PATCH_TEST(FWBCSNPatchSacrificeAndChoiceMatrixTest,
	"Wandbound.CSNPatch.Continuation.SacrificeSnapshotFilteringAndChoiceMatrix")
bool FWBCSNPatchSacrificeAndChoiceMatrixTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData Empty = MakeState();
	AddWand(Empty, TEXT("empty_wand"), 0);
	const FWBActivatedDeckSummonContinuationResult EmptyResult =
		ResolveContinuation(Empty, Repository);
	TestTrue(TEXT("Zero candidates resolves"), EmptyResult.bOk);
	TestFalse(TEXT("Zero candidates has no choice"),
		Empty.HasPendingMandatoryDeckChoice());
	TestFalse(TEXT("Source remains sacrificed"),
		Empty.GetUnitById(PatchSourceId)->IsUnitOnBoard());
	TestEqual(TEXT("No destruction event"),
		Empty.PendingUnitDestructionEvents.Num(), 0);
	TestTrue(TEXT("Sacrifice traced"), HasTrace(
		EmptyResult.TraceEvents, FName(TEXT("unit_sacrificed"))));

	FWBGameStateData State = MakeState();
	AddWand(State, TEXT("wand_b"), 2);
	AddWand(State, TEXT("wand_a"), 1);
	AddDeckCard(State, TEXT("duplicate_2"), TEXT("candidate_a"));
	AddDeckCard(State, TEXT("officer"), TEXT("id_contains_csn"));
	AddDeckCard(State, TEXT("action"), TEXT("csn_action"));
	AddDeckCard(State, TEXT("wand"), TEXT("patch_test_wand"));
	AddDeckCard(State, TEXT("duplicate_1"), TEXT("candidate_a"));
	AddDeckCard(State, TEXT("other_name"), TEXT("candidate_b"));
	const FWBActivatedDeckSummonContinuationResult Pending =
		ResolveContinuation(State, Repository);
	TestTrue(TEXT("Continuation resolves"), Pending.bOk);
	TestTrue(TEXT("Choice opens"), Pending.bPendingChoice);
	TestTrue(TEXT("Generic origin"), State.PendingMandatoryDeckChoice.Origin
		== EWBMandatoryDeckChoiceOrigin::ActivatedEffectContinuation);
	TestEqual(TEXT("Source tile captured"),
		State.PendingMandatoryDeckChoice.ActivatedEffectSourceSnapshot.SourceTile,
		FWBTile(4, 4));
	TestEqual(TEXT("Current RL captured"),
		State.PendingMandatoryDeckChoice.ActivatedEffectSourceSnapshot.CurrentRLSnapshot, 5);
	TestEqual(TEXT("Two Wands captured"),
		State.PendingMandatoryDeckChoice.ActivatedEffectSourceSnapshot.EquippedWands.Num(), 2);
	TestEqual(TEXT("Equip order sorted"),
		State.PendingMandatoryDeckChoice.ActivatedEffectSourceSnapshot.EquippedWands[0].EquipOrder, 1);
	TestEqual(TEXT("No destruction queue"),
		State.PendingUnitDestructionEvents.Num(), 0);
	const TArray<FString> Actions =
		WBMandatoryDeckChoice::EnumerateLegalActionIds(State, Repository);
	TestEqual(TEXT("Only three CSN Characters"), Actions.Num(), 3);
	TestTrue(TEXT("First duplicate exact"), Actions[0].EndsWith(TEXT(":iduplicate_2")));
	TestTrue(TEXT("Second duplicate exact"), Actions[1].EndsWith(TEXT(":iduplicate_1")));
	TestTrue(TEXT("Differently named CSN Character qualifies"),
		Actions[2].EndsWith(TEXT(":iother_name")));
	TestFalse(TEXT("Action excluded"), FString::Join(Actions, TEXT("|")).Contains(TEXT(":iaction")));
	TestFalse(TEXT("CSN Wand excluded"), FString::Join(Actions, TEXT("|")).Contains(TEXT(":iwand")));
	TestFalse(TEXT("Factionless id match excluded"),
		FString::Join(Actions, TEXT("|")).Contains(TEXT(":iofficer")));
	TestFalse(TEXT("Illegal instance rejected"),
		WBMandatoryDeckChoice::Submit(
			State, Repository, TEXT("mandatory_deck_choice:illegal")).bOk);
	FWBGameStateData Stale = State;
	FWBPlayerCardZoneState* StaleZones = WBCardZoneState::FindMutablePlayerZones(
		Stale.GetMutableCardZoneStateForTest(), 0);
	StaleZones->Deck.RemoveAll([](const FWBZoneCardEntry& Entry)
	{
		return Entry.Card.InstanceId == TEXT("duplicate_2");
	});
	const FString BeforeRejectedState =
		WBProductionMatchReplay::BuildGameStateDigest(Stale);
	TestFalse(TEXT("Stale exact instance rejected"),
		WBMandatoryDeckChoice::Submit(Stale, Repository, Actions[0]).bOk);
	TestEqual(TEXT("Rejected stale choice preserves replay state"),
		WBProductionMatchReplay::BuildGameStateDigest(Stale), BeforeRejectedState);
	TestFalse(TEXT("Source remains sacrificed after stale rejection"),
		Stale.GetUnitById(PatchSourceId)->IsUnitOnBoard());
	TestTrue(TEXT("Continuation remains after stale rejection"),
		Stale.HasPendingMandatoryDeckChoice());
	TestEqual(TEXT("Detached Wands remain privately held"),
		Stale.PendingMandatoryDeckChoice.ActivatedEffectSourceSnapshot.EquippedWands.Num(), 2);
	const TArray<FString> RemainingActions =
		WBMandatoryDeckChoice::EnumerateLegalActionIds(Stale, Repository);
	TestEqual(TEXT("Remaining exact candidates stay selectable"),
		RemainingActions.Num(), 2);
	if (RemainingActions.IsEmpty()) return false;
	const FWBMandatoryDeckChoiceResult Recovered =
		WBMandatoryDeckChoice::Submit(Stale, Repository, RemainingActions[0]);
	TestTrue(TEXT("Valid remaining candidate resolves"), Recovered.bOk);
	TestTrue(TEXT("Valid remaining candidate summons"), Recovered.bSummoned);
	TestFalse(TEXT("Recovered continuation clears"),
		Stale.HasPendingMandatoryDeckChoice());
	TestEqual(TEXT("Recovered Wands transfer exactly once"),
		Stale.GetCardZoneState().EquippedCards.Num(), 2);
	return true;
}

WB_PATCH_TEST(FWBCSNPatchInheritanceTest,
	"Wandbound.CSNPatch.Inheritance.ExactWandsRLAndUnitCap")
bool FWBCSNPatchInheritanceTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState();
	State.AddUnitForTest(MakeUnit(11, 0, TEXT("candidate_a"), FWBTile(1, 1)));
	State.AddUnitForTest(MakeUnit(12, 0, TEXT("candidate_b"), FWBTile(2, 1)));
	TestEqual(TEXT("Starts at unit cap"), State.GetUnitsForPlayer(0).Num(), 4);
	AddWand(State, TEXT("wand_1"), 0);
	AddWand(State, TEXT("wand_2"), 1);
	AddDeckCard(State, TEXT("selected"), TEXT("candidate_a"));
	AddDeckCard(State, TEXT("deck_wand"), TEXT("patch_test_wand"));
	AddDeckCard(State, TEXT("deck_filler"), TEXT("patch_test_filler"));
	const FWBActivatedDeckSummonContinuationResult Pending =
		ResolveContinuation(State, Repository);
	TestTrue(TEXT("Choice pending"), Pending.bPendingChoice);
	const TArray<FString> Actions =
		WBMandatoryDeckChoice::EnumerateLegalActionIds(State, Repository);
	TestEqual(TEXT("Single candidate still explicit"), Actions.Num(), 1);
	if (Actions.IsEmpty()) return false;
	const FWBMandatoryDeckChoiceResult Resolved =
		WBMandatoryDeckChoice::Submit(State, Repository, Actions[0]);
	TestTrue(TEXT("Choice accepted"), Resolved.bOk);
	TestTrue(TEXT("Summon succeeds after slot freed"), Resolved.bSummoned);
	const FWBTraceEvent* DeclaredTarget = Resolved.TraceEvents.FindByPredicate(
		[](const FWBTraceEvent& Event)
		{
			return Event.Kind == FName(TEXT("mandatory_deck_target_declared"));
		});
	TestTrue(TEXT("Later exact Deck choice is a declared target"),
		DeclaredTarget != nullptr && DeclaredTarget->bDeclaredTarget);
	TestFalse(TEXT("Choice clears"), State.HasPendingMandatoryDeckChoice());
	const FWBUnitState* Summoned = FindBoardCardAtTile(
		State, TEXT("candidate_a"), FWBTile(4, 4));
	TestNotNull(TEXT("Character summoned"), Summoned);
	if (Summoned == nullptr) return false;
	TestEqual(TEXT("Exact source tile"), FWBTile(Summoned->X, Summoned->Y), FWBTile(4, 4));
	TestEqual(TEXT("Base RL plus source Current RL"), Summoned->GetBaseRLForRules(), 8);
	TestEqual(TEXT("Two exact Wands equipped"),
		State.GetCardZoneState().EquippedCards.Num(), 2);
	for (const FWBEquippedCardEntry& Wand : State.GetCardZoneState().EquippedCards)
	{
		TestEqual(TEXT("Wand owner retained"), Wand.Card.OwnerPlayerId, 0);
		TestEqual(TEXT("Wand targets summoned unit"),
			Wand.EquippedToUnitId, Summoned->UnitId);
	}
	const FWBPlayerCardZoneState* Zones = WBCardZoneState::FindPlayerZones(
		State.GetCardZoneState(), 0);
	TestEqual(TEXT("No duplicate Discard Wands"), Zones->Discard.Num(), 0);
	TestEqual(TEXT("No Wand tutor moves Deck Wand"), Zones->Deck[0].Card.InstanceId,
		FString(TEXT("deck_wand")));
	TestEqual(TEXT("No shuffle preserves following card"), Zones->Deck[1].Card.InstanceId,
		FString(TEXT("deck_filler")));
	TestEqual(TEXT("No Wand tutor adds Hand card"), Zones->Hand.Num(), 0);
	TestTrue(TEXT("Generic effect summon trace"), HasTrace(
		Resolved.TraceEvents, FName(TEXT("effect_summon_completed"))));
	TestFalse(TEXT("No ordinary summon reaction"), State.HasOpenReactionWindow());
	return true;
}

WB_PATCH_TEST(FWBCSNPatchActivationGatesTest,
	"Wandbound.CSNPatch.Activation.GenericGatesAndUsageReset")
bool FWBCSNPatchActivationGatesTest::RunTest(const FString&)
{
	WBMatchCoordinator Coordinator;
	TestTrue(TEXT("Coordinator initializes"),
		Coordinator.InitializeMatch(MakeCoordinatorRequest()).bOk);
	FWBGameStateData& State = Coordinator.GetMutableStateForTest();
	State.AddUnitForTest(MakeUnit(
		PatchSourceId, 0, TEXT("semantic_source"), FWBTile(4, 4)));
	auto FindPatchActivationId = [&Coordinator]()
	{
		const FWBMatchLegalActionGenerationResult Legal =
			Coordinator.EnumerateLegalActions();
		const FWBMatchLegalAction* Action = Legal.bOk ? FindActivation(
			Legal.Actions, TEXT("semantic_self_sacrifice_deck_summon")) : nullptr;
		return Action != nullptr ? Action->ActionId : FString();
	};
	const FString InitialActionId = FindPatchActivationId();
	TestFalse(TEXT("Owner can activate at normal priority"),
		InitialActionId.IsEmpty());
	if (InitialActionId.IsEmpty()) return false;
	TestFalse(TEXT("Opponent cannot submit owner activation"),
		Coordinator.SubmitActionId(1, InitialActionId).bOk);

	State.MarkActivationUsageKeyForTest(
		0, TEXT("semantic_self_sacrifice_once"));
	TestTrue(TEXT("Used source excluded in same turn"),
		FindPatchActivationId().IsEmpty());
	State.ClearActivationUsageKeysForPlayer(0);
	TestFalse(TEXT("New-turn usage reset restores activation"),
		FindPatchActivationId().IsEmpty());

	FWBUnitState* Source = State.GetMutableUnitById(PatchSourceId);
	Source->AddStatus(FName(TEXT("Frozen")), 1);
	TestTrue(TEXT("Frozen blocks activation when policy is omitted"),
		FindPatchActivationId().IsEmpty());
	Source->AddStatus(FName(TEXT("Stunned")), 1);
	TestTrue(TEXT("Stunned blocks generic Character activation"),
		FindPatchActivationId().IsEmpty());
	Source->RemoveStatus(FName(TEXT("Stunned")));
	Source->RemoveUnitFromBoard();
	TestTrue(TEXT("Off-board source cannot activate"),
		FindPatchActivationId().IsEmpty());
	return true;
}

WB_PATCH_TEST(FWBCSNPatchCompositionTest,
	"Wandbound.CSNPatch.Composition.UndertowSableVexRook")
bool FWBCSNPatchCompositionTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	auto Summon = [&Repository](
		const FString& CardId,
		FWBGameStateData& State,
		TArray<FWBTraceEvent>& Trace) -> const FWBUnitState*
	{
		State = MakeState();
		AddDeckCard(State, TEXT("selected"), CardId);
		AddDeckCard(State, TEXT("draw_card"), TEXT("patch_test_filler"));
		const FWBActivatedDeckSummonContinuationResult Pending =
			ResolveContinuation(State, Repository);
		Trace.Append(Pending.TraceEvents);
		const TArray<FString> Actions =
			WBMandatoryDeckChoice::EnumerateLegalActionIds(State, Repository);
		if (Actions.IsEmpty()) return nullptr;
		const FWBMandatoryDeckChoiceResult Resolved =
			WBMandatoryDeckChoice::Submit(State, Repository, Actions[0]);
		Trace.Append(Resolved.TraceEvents);
		return FindBoardCard(State, CardId);
	};

	FWBGameStateData UndertowState;
	TArray<FWBTraceEvent> UndertowTrace;
	const FWBUnitState* Undertow = Summon(
		TEXT("candidate_undertow"), UndertowState, UndertowTrace);
	TestNotNull(TEXT("Undertow summoned"), Undertow);
	const FWBPlayerCardZoneState* UndertowZones =
		WBCardZoneState::FindPlayerZones(UndertowState.GetCardZoneState(), 0);
	TestEqual(TEXT("Undertow draws exactly one"), UndertowZones->Hand.Num(), 1);
	TestTrue(TEXT("Inheritance draw traced"), HasTrace(
		UndertowTrace, FName(TEXT("csn_inheritance_card_drawn"))));

	FWBGameStateData SableState;
	TArray<FWBTraceEvent> SableTrace;
	const FWBUnitState* Sable = Summon(
		TEXT("candidate_sable"), SableState, SableTrace);
	TestNotNull(TEXT("Sable summoned"), Sable);
	if (Sable != nullptr)
	{
		TestEqual(TEXT("Sacrifice does not grow Sable ATK"), Sable->ATK, 2);
		TestEqual(TEXT("Sacrifice does not grow Sable MaxHP"), Sable->MaxHP, 12);
	}
	TestEqual(TEXT("Sacrifice creates no Sable event"),
		SableState.PendingUnitDestructionEvents.Num(), 0);

	FWBGameStateData VexState;
	TArray<FWBTraceEvent> VexTrace;
	const FWBUnitState* Vex = Summon(
		TEXT("candidate_vex"), VexState, VexTrace);
	TestNotNull(TEXT("Vex summoned"), Vex);
	if (Vex != nullptr)
	{
		FWBUnitState Enemy = MakeUnit(
			30, 1, TEXT("id_contains_csn"), FWBTile(4, 6));
		Enemy.AR = 4;
		VexState.AddUnitForTest(Enemy);
		TestEqual(TEXT("Vex aura activates normally"),
			WBUnitStatQuery::GetEffectiveAR(VexState, Repository, 30).EffectiveValue, 3);
	}

	FWBGameStateData RookState;
	TArray<FWBTraceEvent> RookTrace;
	const FWBUnitState* Rook = Summon(
		TEXT("candidate_rook"), RookState, RookTrace);
	TestNotNull(TEXT("Rook summoned"), Rook);
	TestEqual(TEXT("Patch sacrifice does not queue Rook trigger"),
		RookState.PendingUnitDestructionEvents.Num(), 0);
	return true;
}

WB_PATCH_TEST(FWBCSNPatchCoordinatorFlowTest,
	"Wandbound.CSNPatch.Coordinator.ActivationChoicePrivacyAndReplay")
bool FWBCSNPatchCoordinatorFlowTest::RunTest(const FString&)
{
	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Initialized = Coordinator.InitializeMatch(
		MakeCoordinatorRequest());
	TestTrue(TEXT("Coordinator initializes"), Initialized.bOk);
	if (!Initialized.bOk) return false;
	FWBGameStateData& State = Coordinator.GetMutableStateForTest();
	State.AddUnitForTest(MakeUnit(
		PatchSourceId, 0, TEXT("semantic_source"), FWBTile(4, 4), 2, 5, 1));
	AddDeckCard(State, TEXT("private_candidate"), TEXT("candidate_a"));
	AddWand(State, TEXT("coordinator_wand"), 0);

	const FWBMatchLegalActionGenerationResult Legal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Activation = Legal.bOk
		? FindActivation(Legal.Actions,
			TEXT("semantic_self_sacrifice_deck_summon")) : nullptr;
	TestNotNull(TEXT("Owner activation legal"), Activation);
	if (Activation == nullptr) return false;
	const FString ActivationActionId = Activation->ActionId;
	const FWBMatchOperationResult Activated = Coordinator.SubmitActionId(
		0, ActivationActionId);
	TestTrue(TEXT("Activation accepted"), Activated.bOk);
	TestEqual(TEXT("Choice phase reached"), Coordinator.GetMatchPhase(),
		EWBMatchLoopPhase::MandatoryChoice);
	TestTrue(TEXT("Usage consumed on accepted activation"),
		Coordinator.GetState().HasActivationUsageKeyThisTurn(
			0, TEXT("semantic_self_sacrifice_once")));
	TestFalse(TEXT("Source sacrificed"),
		Coordinator.GetState().GetUnitById(PatchSourceId)->IsUnitOnBoard());
	TestEqual(TEXT("No destruction event"),
		Coordinator.GetState().PendingUnitDestructionEvents.Num(), 0);
	const FWBMatchObservation Opponent = Coordinator.BuildObservation(1);
	TestEqual(TEXT("Opponent receives no private choice actions"),
		Opponent.LegalActions.Num(), 0);
	TestFalse(TEXT("Opponent does not see exact instance"),
		WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(
			Opponent.CardZones, TEXT("private_candidate")));
	const FWBMatchObservation Controller = Coordinator.BuildObservation(0);
	TestEqual(TEXT("Controller receives one explicit choice"),
		Controller.LegalActions.Num(), 1);
	if (Controller.LegalActions.IsEmpty()) return false;
	TestEqual(TEXT("Choice family retained"), Controller.LegalActions[0].Family,
		EWBMatchActionFamily::MandatoryDeckChoice);
	TestEqual(TEXT("Exact instance retained privately"),
		Controller.LegalActions[0].MandatoryChoiceCardInstanceId,
		FString(TEXT("private_candidate")));
	const FWBMatchOperationResult Chosen = Coordinator.SubmitActionId(
		0, Controller.LegalActions[0].ActionId);
	TestTrue(TEXT("Explicit choice accepted"), Chosen.bOk);
	TestEqual(TEXT("Returns to Action"), Coordinator.GetMatchPhase(),
		EWBMatchLoopPhase::Action);
	TestFalse(TEXT("No second summon reaction"),
		Coordinator.GetState().HasOpenReactionWindow());
	TestEqual(TEXT("Two real accepted decisions"),
		Coordinator.GetCommittedActionRecords().Num(), 2);
	TestEqual(TEXT("Activation replay family"),
		Coordinator.GetCommittedActionRecords()[0].ActionFamily,
		FString(TEXT("activate")));
	TestEqual(TEXT("Choice replay family"),
		Coordinator.GetCommittedActionRecords()[1].ActionFamily,
		FString(TEXT("mandatory_deck_choice")));
	TestEqual(TEXT("Replay schema one"), WBProductionMatchReplay::SchemaVersion, 1);
	return true;
}

WB_PATCH_TEST(FWBCSNPatchNegationTest,
	"Wandbound.CSNPatch.Reaction.NegatedActivationDoesNothing")
bool FWBCSNPatchNegationTest::RunTest(const FString&)
{
	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Initialized = Coordinator.InitializeMatch(
		MakeCoordinatorRequest(true));
	TestTrue(TEXT("Coordinator initializes"), Initialized.bOk);
	if (!Initialized.bOk) return false;
	Coordinator.GetMutableStateForTest().AddUnitForTest(MakeUnit(
		PatchSourceId, 0, TEXT("semantic_source"), FWBTile(4, 4), 2, 5, 1));
	AddDeckCard(Coordinator.GetMutableStateForTest(),
		TEXT("negated_candidate"), TEXT("candidate_a"));
	AddWand(Coordinator.GetMutableStateForTest(), TEXT("negated_wand"), 0);
	const FWBMatchLegalActionGenerationResult InitialLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Activation = FindActivation(
		InitialLegal.Actions, TEXT("semantic_self_sacrifice_deck_summon"));
	if (Activation == nullptr) return false;
	TestTrue(TEXT("Activation enters reaction"),
		Coordinator.SubmitActionId(0, Activation->ActionId).bOk);
	TestEqual(TEXT("Response phase"), Coordinator.GetMatchPhase(),
		EWBMatchLoopPhase::Response);
	const FWBMatchLegalActionGenerationResult ResponseLegal =
		Coordinator.EnumerateLegalActions();
	const FWBMatchLegalAction* Negate = FindActivation(
		ResponseLegal.Actions, TEXT("patch_test_negate"));
	TestNotNull(TEXT("Ordinary negate exposed"), Negate);
	if (Negate == nullptr) return false;
	const FWBMatchOperationResult Negated = Coordinator.SubmitActionId(
		1, Negate->ActionId);
	TestTrue(TEXT("Negate accepted"), Negated.bOk);
	const FWBUnitState* Source = Coordinator.GetState().GetUnitById(PatchSourceId);
	TestNotNull(TEXT("Source remains"), Source);
	TestTrue(TEXT("Source remains on board"), Source != nullptr && Source->IsUnitOnBoard());
	TestFalse(TEXT("No Deck choice"),
		Coordinator.GetState().HasPendingMandatoryDeckChoice());
	TestEqual(TEXT("Wand remains equipped"),
		Coordinator.GetState().GetCardZoneState().EquippedCards.Num(), 1);
	TestEqual(TEXT("No destruction event"),
		Coordinator.GetState().PendingUnitDestructionEvents.Num(), 0);
	TestFalse(TEXT("No sacrifice trace"), HasTrace(
		Coordinator.GetTraceLog(), FName(TEXT("unit_sacrificed"))));
	TestTrue(TEXT("Existing once-per-turn negation policy retained"),
		Coordinator.GetState().HasActivationUsageKeyThisTurn(
			0, TEXT("semantic_self_sacrifice_once")));
	return true;
}

WB_PATCH_TEST(FWBCSNPatchFailureAndHeroBoundaryTest,
	"Wandbound.CSNPatch.Boundaries.StaleTileSourceAndHero")
bool FWBCSNPatchFailureAndHeroBoundaryTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData Removed = MakeState();
	Removed.GetMutableUnitById(PatchSourceId)->RemoveUnitFromBoard();
	const FWBActivatedDeckSummonContinuationResult RemovedResult =
		ResolveContinuation(Removed, Repository);
	TestFalse(TEXT("Removed source fails closed"), RemovedResult.bOk);
	TestFalse(TEXT("Removed source creates no choice"),
		Removed.HasPendingMandatoryDeckChoice());

	FWBGameStateData Occupied = MakeState();
	AddWand(Occupied, TEXT("occupied_wand"), 0);
	AddDeckCard(Occupied, TEXT("selected"), TEXT("candidate_a"));
	TestTrue(TEXT("Occupied setup opens choice"),
		ResolveContinuation(Occupied, Repository).bPendingChoice);
	Occupied.AddUnitForTest(MakeUnit(
		31, 1, TEXT("id_contains_csn"), FWBTile(4, 4)));
	const TArray<FString> OccupiedActions =
		WBMandatoryDeckChoice::EnumerateLegalActionIds(Occupied, Repository);
	const FWBMandatoryDeckChoiceResult OccupiedResult =
		WBMandatoryDeckChoice::Submit(
			Occupied, Repository, OccupiedActions[0]);
	TestTrue(TEXT("Occupied destination completes fail-closed choice"),
		OccupiedResult.bOk);
	TestFalse(TEXT("Occupied destination does not summon"),
		OccupiedResult.bSummoned);
	TestFalse(TEXT("Sacrifice remains committed"),
		Occupied.GetUnitById(PatchSourceId)->IsUnitOnBoard());
	TestFalse(TEXT("Failed continuation clears choice"),
		Occupied.HasPendingMandatoryDeckChoice());
	TestEqual(TEXT("Occupied failure leaves no equipped Wand"),
		Occupied.GetCardZoneState().EquippedCards.Num(), 0);
	const FWBPlayerCardZoneState* OccupiedZones =
		WBCardZoneState::FindPlayerZones(Occupied.GetCardZoneState(), 0);
	TestNotNull(TEXT("Occupied failure retains player zones"), OccupiedZones);
	if (OccupiedZones != nullptr)
	{
		TestEqual(TEXT("Occupied failure discards exact Wand"),
			OccupiedZones->Discard.Num(), 1);
		if (!OccupiedZones->Discard.IsEmpty())
		{
			TestEqual(TEXT("Occupied failure preserves Wand instance"),
				OccupiedZones->Discard[0].Card.InstanceId,
				FString(TEXT("occupied_wand")));
			TestEqual(TEXT("Occupied failure preserves Wand owner"),
				OccupiedZones->Discard[0].Card.OwnerPlayerId, 0);
		}
	}
	TestTrue(TEXT("Failure traced"), HasTrace(
		OccupiedResult.TraceEvents, FName(TEXT("effect_summon_failed"))));

	FWBGameStateData Hero = MakeState(TEXT("semantic_source"), true);
	AddDeckCard(Hero, TEXT("hero_candidate"), TEXT("candidate_a"));
	const FWBActivatedDeckSummonContinuationResult HeroResult =
		ResolveContinuation(Hero, Repository);
	TestTrue(TEXT("Hero sacrifice resolves"), HeroResult.bOk);
	TestTrue(TEXT("Non-replaced Hero loss terminal"), Hero.bGameOver);
	TestEqual(TEXT("Opponent wins"), Hero.WinnerPlayerId, 1);
	TestEqual(TEXT("Canonical terminal reason"), Hero.TerminalOutcome.Reason,
		EWBTerminalReason::HeroDefeatedWithoutReplacement);
	TestFalse(TEXT("No Hybrid replacement exemption"),
		Hero.HasPendingMandatoryDeckChoice());
	return true;
}

WB_PATCH_TEST(FWBCSNPatchHeroWandTerminalTest,
	"Wandbound.CSNPatch.Boundaries.HeroSacrificeWandsTerminalDeterministic")
bool FWBCSNPatchHeroWandTerminalTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	auto ResolveHero = [&Repository](
		FWBGameStateData& State,
		FWBActivatedDeckSummonContinuationResult& Result)
	{
		State = MakeState(TEXT("semantic_source"), true);
		AddWand(State, TEXT("hero_wand_b"), 2);
		AddWand(State, TEXT("hero_wand_a"), 1);
		AddDeckCard(State, TEXT("hero_candidate"), TEXT("candidate_a"));
		Result = ResolveContinuation(State, Repository);
	};

	FWBGameStateData First;
	FWBActivatedDeckSummonContinuationResult FirstResult;
	ResolveHero(First, FirstResult);
	TestTrue(TEXT("Hero sacrifice resolves"), FirstResult.bOk);
	TestTrue(TEXT("Hero loss is terminal"), First.bGameOver);
	TestEqual(TEXT("Opponent wins terminal match"), First.WinnerPlayerId, 1);
	TestFalse(TEXT("Hero Patch remains sacrificed"),
		First.GetUnitById(PatchSourceId)->IsUnitOnBoard());
	TestFalse(TEXT("No mandatory choice opens"),
		First.HasPendingMandatoryDeckChoice());
	TestNull(TEXT("No replacement Character is summoned"),
		FindBoardCard(First, TEXT("candidate_a")));
	TestEqual(TEXT("No replacement Hero is created"),
		First.GetPlayerById(0)->HeroUnitId, PatchSourceId);
	TestEqual(TEXT("No Wand remains equipped"),
		First.GetCardZoneState().EquippedCards.Num(), 0);
	const FWBPlayerCardZoneState* FirstZones =
		WBCardZoneState::FindPlayerZones(First.GetCardZoneState(), 0);
	TestNotNull(TEXT("Hero owner zones remain"), FirstZones);
	if (FirstZones != nullptr)
	{
		TestEqual(TEXT("Both exact Wands reach Discard"),
			FirstZones->Discard.Num(), 2);
		TestTrue(TEXT("First exact Wand preserved"),
			FirstZones->Discard.ContainsByPredicate(
				[](const FWBZoneCardEntry& Entry)
				{
					return Entry.Card.InstanceId == TEXT("hero_wand_a")
						&& Entry.Card.OwnerPlayerId == 0;
				}));
		TestTrue(TEXT("Second exact Wand preserved"),
			FirstZones->Discard.ContainsByPredicate(
				[](const FWBZoneCardEntry& Entry)
				{
					return Entry.Card.InstanceId == TEXT("hero_wand_b")
						&& Entry.Card.OwnerPlayerId == 0;
				}));
		TestEqual(TEXT("Candidate remains in Deck after terminal"),
			FirstZones->Deck.Num(), 1);
	}
	TestEqual(TEXT("Hero sacrifice fabricates no destruction"),
		First.PendingUnitDestructionEvents.Num(), 0);

	FWBGameStateData Second;
	FWBActivatedDeckSummonContinuationResult SecondResult;
	ResolveHero(Second, SecondResult);
	TestEqual(TEXT("Hero terminal state is deterministic"),
		WBProductionMatchReplay::BuildGameStateDigest(First),
		WBProductionMatchReplay::BuildGameStateDigest(Second));
	TestEqual(TEXT("Hero terminal trace is deterministic"),
		WBProductionMatchReplay::BuildTraceDigest(FirstResult.TraceEvents),
		WBProductionMatchReplay::BuildTraceDigest(SecondResult.TraceEvents));
	return true;
}

WB_PATCH_TEST(FWBCSNPatchDestructionBoundaryTest,
	"Wandbound.CSNPatch.Destruction.TrueDeathGrowsSableSacrificeDoesNot")
bool FWBCSNPatchDestructionBoundaryTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData Sacrifice = MakeState();
	Sacrifice.AddUnitForTest(MakeUnit(
		11, 0, TEXT("candidate_sable"), FWBTile(2, 2)));
	AddDeckCard(Sacrifice, TEXT("sacrifice_candidate"), TEXT("candidate_a"));
	const FWBActivatedDeckSummonContinuationResult Pending =
		ResolveContinuation(Sacrifice, Repository);
	TestTrue(TEXT("Sacrifice continuation opens"), Pending.bPendingChoice);
	const TArray<FString> SacrificeActions =
		WBMandatoryDeckChoice::EnumerateLegalActionIds(Sacrifice, Repository);
	if (SacrificeActions.IsEmpty()) return false;
	const FWBMandatoryDeckChoiceResult SacrificeResult =
		WBMandatoryDeckChoice::Submit(
			Sacrifice, Repository, SacrificeActions[0]);
	TestTrue(TEXT("Sacrifice choice resolves"), SacrificeResult.bOk);
	const FWBUnitState* UnchangedSable = Sacrifice.GetUnitById(11);
	TestNotNull(TEXT("Pre-existing Sable survives sacrifice"), UnchangedSable);
	if (UnchangedSable != nullptr)
	{
		TestEqual(TEXT("Sacrifice does not grow Sable ATK"),
			UnchangedSable->ATK, 2);
		TestEqual(TEXT("Sacrifice does not grow Sable HP"),
			UnchangedSable->HP, 12);
		TestEqual(TEXT("Sacrifice does not grow Sable MaxHP"),
			UnchangedSable->MaxHP, 12);
	}
	TestEqual(TEXT("Sacrifice queues no destruction event"),
		Sacrifice.PendingUnitDestructionEvents.Num(), 0);

	FWBGameStateData Destruction = MakeState();
	Destruction.AddUnitForTest(MakeUnit(
		11, 0, TEXT("candidate_sable"), FWBTile(2, 2)));
	Destruction.GetMutableUnitById(PatchSourceId)->HP = 0;
	const FWBApplyActionResult Death =
		WBDeathResolution::ApplyZeroHPDeathResolution(
			Destruction, EWBUnitDestructionCause::BattleDamage);
	TestTrue(TEXT("True Patch death commits"), Death.bOk);
	TestEqual(TEXT("True Patch death queues one event"),
		Destruction.PendingUnitDestructionEvents.Num(), 1);
	if (Destruction.PendingUnitDestructionEvents.Num() == 1)
	{
		const FWBUnitDestructionSnapshot& Event =
			Destruction.PendingUnitDestructionEvents[0];
		TestEqual(TEXT("True destruction captures Patch"),
			Event.DestroyedUnitId, PatchSourceId);
		TestTrue(TEXT("True destruction captures Sable observer"),
			Event.ObserverSources.ContainsByPredicate(
				[](const FWBPostDestructionObserverSourceSnapshot& Source)
				{
					return Source.SourceUnitId == 11;
				}));
	}
	const FWBPostDestructionTriggerResult Advanced =
		WBPostDestructionTrigger::AdvanceToDecisionOrComplete(
			Destruction, Repository, 0,
			static_cast<int32>(EWBMatchLoopPhase::Action));
	TestTrue(TEXT("True Patch destruction observers resolve"), Advanced.bOk);
	const FWBUnitState* GrownSable = Destruction.GetUnitById(11);
	TestNotNull(TEXT("Sable survives true Patch destruction"), GrownSable);
	if (GrownSable != nullptr)
	{
		TestEqual(TEXT("True Patch death grows Sable ATK"), GrownSable->ATK, 3);
		TestEqual(TEXT("True Patch death grows Sable HP"), GrownSable->HP, 13);
		TestEqual(TEXT("True Patch death grows Sable MaxHP"),
			GrownSable->MaxHP, 13);
	}
	return true;
}

WB_PATCH_TEST(FWBCSNPatchUndertowEmptyDeckTest,
	"Wandbound.CSNPatch.Composition.UndertowEmptyDeckRollbackBoundary")
bool FWBCSNPatchUndertowEmptyDeckTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	auto ResolveEmptyDeck = [&Repository](
		FWBGameStateData& State,
		TArray<FWBTraceEvent>& Trace)
	{
		State = MakeState();
		AddWand(State, TEXT("undertow_empty_wand"), 0);
		AddDeckCard(State, TEXT("undertow_exact"), TEXT("candidate_undertow"));
		const FWBActivatedDeckSummonContinuationResult Pending =
			ResolveContinuation(State, Repository);
		Trace.Append(Pending.TraceEvents);
		const TArray<FString> Actions =
			WBMandatoryDeckChoice::EnumerateLegalActionIds(State, Repository);
		if (Actions.IsEmpty()) return false;
		const FWBMandatoryDeckChoiceResult Resolved =
			WBMandatoryDeckChoice::Submit(State, Repository, Actions[0]);
		Trace.Append(Resolved.TraceEvents);
		return Resolved.bOk && !Resolved.bSummoned;
	};

	FWBGameStateData First;
	TArray<FWBTraceEvent> FirstTrace;
	TestTrue(TEXT("Empty-Deck Undertow fails closed"),
		ResolveEmptyDeck(First, FirstTrace));
	TestFalse(TEXT("Patch sacrifice remains committed"),
		First.GetUnitById(PatchSourceId)->IsUnitOnBoard());
	TestNull(TEXT("Undertow summon transaction rolls back"),
		FindBoardCard(First, TEXT("candidate_undertow")));
	TestFalse(TEXT("Choice is consumed after failed summon"),
		First.HasPendingMandatoryDeckChoice());
	TestEqual(TEXT("No destruction event is fabricated"),
		First.PendingUnitDestructionEvents.Num(), 0);
	TestEqual(TEXT("No Wand remains equipped"),
		First.GetCardZoneState().EquippedCards.Num(), 0);
	const FWBPlayerCardZoneState* FirstZones =
		WBCardZoneState::FindPlayerZones(First.GetCardZoneState(), 0);
	TestNotNull(TEXT("Player zones remain after rollback"), FirstZones);
	if (FirstZones != nullptr)
	{
		TestEqual(TEXT("Selected Undertow remains exact Deck instance"),
			FirstZones->Deck.Num(), 1);
		if (!FirstZones->Deck.IsEmpty())
		{
			TestEqual(TEXT("Undertow instance is unchanged"),
				FirstZones->Deck[0].Card.InstanceId,
				FString(TEXT("undertow_exact")));
		}
		TestEqual(TEXT("Detached Wand reaches Discard"),
			FirstZones->Discard.Num(), 1);
		if (!FirstZones->Discard.IsEmpty())
		{
			TestEqual(TEXT("Discarded Wand instance is exact"),
				FirstZones->Discard[0].Card.InstanceId,
				FString(TEXT("undertow_empty_wand")));
			TestEqual(TEXT("Discarded Wand owner is preserved"),
				FirstZones->Discard[0].Card.OwnerPlayerId, 0);
		}
	}

	FWBGameStateData Second;
	TArray<FWBTraceEvent> SecondTrace;
	TestTrue(TEXT("Repeated empty-Deck Undertow fails identically"),
		ResolveEmptyDeck(Second, SecondTrace));
	TestEqual(TEXT("Empty-Deck rollback state is deterministic"),
		WBProductionMatchReplay::BuildGameStateDigest(First),
		WBProductionMatchReplay::BuildGameStateDigest(Second));
	TestEqual(TEXT("Empty-Deck rollback trace is deterministic"),
		WBProductionMatchReplay::BuildTraceDigest(FirstTrace),
		WBProductionMatchReplay::BuildTraceDigest(SecondTrace));
	return true;
}

WB_PATCH_TEST(FWBCSNPatchDefinitionDrivenTest,
	"Wandbound.CSNPatch.Authority.DefinitionDrivenNoCardIdBranch")
bool FWBCSNPatchDefinitionDrivenTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData Alternate = MakeState(TEXT("alternate_identity"));
	AddDeckCard(Alternate, TEXT("alternate_selected"), TEXT("candidate_a"));
	const FWBActivatedDeckSummonContinuationResult AlternateResult =
		ResolveContinuation(Alternate, Repository, TEXT("alternate_identity"));
	TestTrue(TEXT("Alternate semantic identity works"), AlternateResult.bOk);
	TestTrue(TEXT("Alternate opens generic choice"),
		AlternateResult.bPendingChoice);

	FWBMatchInitializationRequest Request = MakeCoordinatorRequest();
	WBMatchCoordinator Coordinator;
	TestTrue(TEXT("Coordinator initializes"),
		Coordinator.InitializeMatch(Request).bOk);
	Coordinator.GetMutableStateForTest().AddUnitForTest(MakeUnit(
		PatchSourceId, 0, TEXT("patch_like_without_metadata"), FWBTile(4, 4)));
	const FWBMatchLegalActionGenerationResult Legal =
		Coordinator.EnumerateLegalActions();
	TestTrue(TEXT("Legal generation succeeds"), Legal.bOk);
	TestNull(TEXT("Patch-like name has no semantic activation"),
		FindActivation(Legal.Actions,
			TEXT("semantic_self_sacrifice_deck_summon")));
	return true;
}

WB_PATCH_TEST(FWBCSNPatchProductionSmokeTest,
	"Wandbound.CSNPatch.Fixture.ProductionSmokeAndFreshReplay")
bool FWBCSNPatchProductionSmokeTest::RunTest(const FString&)
{
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/CardDB/Production/CSNCrashIn/root_manifest.json"));
	Request.MatchSpecificationPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Data/Replay/CSNPatchFixture/match_spec.json"));
	const FWBProductionCSNCrashInSmokeResult First =
		WBProductionCSNCrashInSmoke::RunPatch(Request);
	const FWBProductionCSNCrashInSmokeResult Second =
		WBProductionCSNCrashInSmoke::RunPatch(Request);
	if (!First.bOk) AddError(TEXT("First Patch smoke failed: ") + First.Reason);
	if (!Second.bOk) AddError(TEXT("Second Patch smoke failed: ") + Second.Reason);
	TestTrue(TEXT("First production smoke"), First.bOk);
	TestTrue(TEXT("Second production smoke"), Second.bOk);
	TestEqual(TEXT("Archive deterministic"),
		First.SerializedArchive, Second.SerializedArchive);
	TestEqual(TEXT("Receipt deterministic"),
		First.SerializedReceipt, Second.SerializedReceipt);
	TestEqual(TEXT("State digest deterministic"),
		First.FinalStateDigest, Second.FinalStateDigest);
	TestEqual(TEXT("Trace digest deterministic"),
		First.FinalTraceDigest, Second.FinalTraceDigest);
	TestEqual(TEXT("Replay count deterministic"),
		First.RecordsVerified, Second.RecordsVerified);
	TestEqual(TEXT("Replay schema remains one"),
		WBProductionMatchReplay::SchemaVersion, 1);
	return true;
}

#undef WB_PATCH_TEST

#endif
