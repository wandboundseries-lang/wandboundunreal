#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#include "WBCardDefinitionRepository.h"
#include "WBCardZoneObservation.h"
#include "WBCardZoneState.h"
#include "WBDeathResolution.h"
#include "WBEffectRunner.h"
#include "WBMatchCoordinator.h"
#include "WBPostDestructionTrigger.h"
#include "WBProductionCardDatabase.h"
#include "WBProductionCSNCrashInSmoke.h"
#include "WBProductionMatchReplay.h"
#include "WBUnitStatQuery.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 HeroId = 1;
constexpr int32 EnemyHeroId = 2;
constexpr int32 RookId = 10;

FWBAfterUnitDestroyedTriggerDefinition MakeDestructionTrigger()
{
	FWBAfterUnitDestroyedTriggerDefinition Trigger;
	Trigger.TriggerId = TEXT("summon_csn_character_from_deck");
	Trigger.SourceScope = EWBAfterUnitDestroyedSourceScope::DestroyedSelf;
	Trigger.Operation = EWBPostDestructionEffectOperation::SummonCharacterFromDeckToDestroyedTile;
	Trigger.RequiredFaction = TEXT("csn");
	Trigger.SummonCount = 1;
	Trigger.bMandatory = true;
	Trigger.bIgnoreSummoningConditions = true;
	Trigger.bApplyCSNInheritance = true;
	return Trigger;
}

FWBCardDefinition MakeCharacter(
	const FString& CardId,
	const FString& Faction,
	const int32 HP = 10,
	const int32 ATK = 2,
	const int32 AR = 2,
	const int32 RL = 2,
	const bool bDestructionTrigger = false,
	const bool bInheritanceDraw = false)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.PublicCategory = TEXT("Character");
	Definition.Kind = EWBCardDefinitionKind::Character;
	Definition.PublicFactions.Add(Faction);
	Definition.CharacterStats.HP = HP;
	Definition.CharacterStats.ATK = ATK;
	Definition.CharacterStats.AR = AR;
	Definition.CharacterStats.RL = RL;
	if (bDestructionTrigger)
	{
		Definition.AfterUnitDestroyedTriggers.Add(MakeDestructionTrigger());
	}
	if (bInheritanceDraw)
	{
		FWBAfterCSNInheritanceTriggerDefinition Trigger;
		Trigger.TriggerId = TEXT("draw_after_csn_inheritance");
		Trigger.DrawCount = 1;
		Trigger.bMandatory = true;
		Definition.AfterCSNInheritanceTriggers.Add(Trigger);
	}
	return Definition;
}

FWBCardDefinitionRepository MakeRepository(
	const FString& SourceId = TEXT("fixture_rook"),
	const bool bSourceHasTrigger = true)
{
	TArray<FWBCardDefinition> Definitions;
	Definitions.Add(MakeCharacter(TEXT("hero"), TEXT("csn"), 20, 2, 2, 4));
	Definitions.Add(MakeCharacter(TEXT("enemy_hero"), TEXT("officer"), 20, 2, 2, 4));
	Definitions.Add(MakeCharacter(SourceId, TEXT("csn"), 16, 3, 2, 2,
		bSourceHasTrigger));
	Definitions.Add(MakeCharacter(TEXT("csn_candidate_a"), TEXT("csn"), 11, 4, 3, 2));
	Definitions.Add(MakeCharacter(TEXT("csn_candidate_b"), TEXT("csn"), 12, 5, 4, 3));
	FWBCardDefinition Vex = MakeCharacter(
		TEXT("char_csn_vex"), TEXT("csn"), 13, 3, 4, 2);
	FWBContinuousStatAuraDefinition VexAura;
	VexAura.AuraId = TEXT("enemy_ar_penalty_in_source_ar");
	VexAura.TargetRelation = EWBContinuousAuraTargetRelation::Enemy;
	VexAura.TargetStat = EWBContinuousStat::AR;
	VexAura.Operation = EWBContinuousStatOperation::Add;
	VexAura.Amount = -1;
	VexAura.RangeStat = EWBContinuousAuraRangeStat::AR;
	VexAura.Geometry = EWBContinuousAuraGeometry::AttackLine;
	VexAura.bBlockedByWalls = true;
	VexAura.bBlockedByUnits = true;
	VexAura.MinimumResult = 0;
	Vex.ContinuousStatAuras.Add(VexAura);
	Definitions.Add(MoveTemp(Vex));
	Definitions.Add(MakeCharacter(TEXT("undertow_candidate"), TEXT("csn"), 11, 2, 3, 2,
		false, true));
	Definitions.Add(MakeCharacter(TEXT("officer_candidate"), TEXT("officer")));

	FWBCardDefinition Wand;
	Wand.CardId = TEXT("csn_wand");
	Wand.PublicName = TEXT("CSN Wand");
	Wand.PublicCategory = TEXT("Wand");
	Wand.Kind = EWBCardDefinitionKind::Wand;
	Wand.PublicFactions.Add(TEXT("csn"));
	Wand.WandStats.RR = 1;
	Definitions.Add(Wand);

	FWBCardDefinition Action;
	Action.CardId = TEXT("csn_action");
	Action.PublicName = TEXT("CSN Action");
	Action.PublicCategory = TEXT("Action");
	Action.Kind = EWBCardDefinitionKind::Action;
	Action.PublicFactions.Add(TEXT("csn"));
	Definitions.Add(Action);

	FWBCardDefinition Hybrid = MakeCharacter(TEXT("csn_hybrid"), TEXT("csn"));
	Hybrid.Kind = EWBCardDefinitionKind::Hybrid;
	Hybrid.HybridSummon.SacrificeCount = 1;
	Hybrid.HybridSummon.SacrificeRequirement = FName(TEXT("controlled_character"));
	Hybrid.HybridSummon.WandPaymentCount = 1;
	Hybrid.HybridSummon.WandPaymentSources = {
		FName(TEXT("hand")), FName(TEXT("sacrificed_unit")) };
	Hybrid.HybridSummon.HeroDestination = FName(TEXT("sacrificed_hero_tile"));
	Hybrid.HybridSummon.NonHeroDestination = FName(TEXT("adjacent_to_hero"));
	Definitions.Add(Hybrid);

	FWBCardDefinition PrivateDraw;
	PrivateDraw.CardId = TEXT("private_draw");
	PrivateDraw.PublicName = TEXT("Private Draw");
	PrivateDraw.PublicCategory = TEXT("Action");
	PrivateDraw.Kind = EWBCardDefinitionKind::Action;
	Definitions.Add(PrivateDraw);

	FWBCardDefinition Trap;
	Trap.CardId = TEXT("rook_test_trap");
	Trap.PublicName = TEXT("Rook Test Trap");
	Trap.PublicCategory = TEXT("Trap");
	Trap.Kind = EWBCardDefinitionKind::Trap;
	Trap.TrapDamage = 1;
	Definitions.Add(Trap);
	FWBCardDefinition NPC = MakeCharacter(TEXT("rook_test_npc"), TEXT("officer"), 5, 1, 1, 1);
	NPC.Kind = EWBCardDefinitionKind::NPC;
	Definitions.Add(NPC);

	FWBCardDefinitionRepository Repository;
	WBCardDefinitionRepository::BuildRepositoryFromDefinitions(
		TEXT("rook_tests"), TEXT("v1"), Definitions, Repository);
	return Repository;
}

FWBUnitState MakeUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FString& CardId,
	const FWBTile Tile,
	const int32 HP,
	const int32 BaseRL,
	const int32 CurrentRL,
	const int32 RLUsed)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = CardId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = HP;
	Unit.MaxHP = FMath::Max(HP, 16);
	Unit.ATK = 3;
	Unit.AR = 2;
	Unit.SetCanonicalRL(BaseRL, CurrentRL, RLUsed);
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	return Unit;
}

FWBZoneCardEntry MakeDeckEntry(
	const FString& InstanceId,
	const FString& CardId,
	const int32 Index)
{
	FWBZoneCardEntry Entry;
	Entry.Card.InstanceId = InstanceId;
	Entry.Card.CardId = CardId;
	Entry.Card.OwnerPlayerId = 0;
	Entry.Zone = EWBCardZone::Deck;
	Entry.ZoneIndex = Index;
	return Entry;
}

FWBGameStateData MakeState(
	const FString& SourceId = TEXT("fixture_rook"),
	const int32 RookHP = 0,
	const bool bRookHero = false)
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 4;
	State.Phase = EWBGamePhase::NormalTurn;

	FWBPlayerStateData Player0;
	Player0.PlayerId = 0;
	Player0.HeroUnitId = bRookHero ? RookId : HeroId;
	FWBPlayerStateData Player1;
	Player1.PlayerId = 1;
	Player1.HeroUnitId = EnemyHeroId;
	State.Players = { Player0, Player1 };
	if (!bRookHero)
	{
		State.AddUnitForTest(MakeUnit(HeroId, 0, TEXT("hero"), FWBTile(0, 0), 20, 4, 4, 0));
	}
	State.AddUnitForTest(MakeUnit(EnemyHeroId, 1, TEXT("enemy_hero"), FWBTile(8, 8), 20, 4, 4, 0));
	State.AddUnitForTest(MakeUnit(RookId, 0, SourceId, FWBTile(4, 4), RookHP, 2, 5, 1));

	FWBPlayerCardZoneState Zones0;
	Zones0.PlayerId = 0;
	FWBPlayerCardZoneState Zones1;
	Zones1.PlayerId = 1;
	State.GetMutableCardZoneStateForTest().PlayerZones = { Zones0, Zones1 };
	return State;
}

FWBCardInstanceRef MakeCardRef(
	const FString& InstanceId,
	const FString& CardId,
	const int32 OwnerId)
{
	FWBCardInstanceRef Card;
	Card.InstanceId = InstanceId;
	Card.CardId = CardId;
	Card.OwnerPlayerId = OwnerId;
	return Card;
}

FWBMatchInitializationRequest MakeCoordinatorRequest()
{
	FWBMatchInitializationRequest Request;
	Request.Seed = 90125;
	Request.FirstPlayerId = 0;
	Request.Repository = MakeRepository();
	for (int32 PlayerId = 0; PlayerId < 2; ++PlayerId)
	{
		FWBMatchPlayerSetup Setup;
		Setup.PlayerId = PlayerId;
		Setup.HeroInstanceId = FString::Printf(TEXT("p%d_hero_instance"), PlayerId);
		Setup.HeroCardId = PlayerId == 0 ? TEXT("hero") : TEXT("enemy_hero");
		Setup.HeroSpawnTile = PlayerId == 0 ? FWBTile(4, 8) : FWBTile(4, 0);
		Setup.OrderedDeck.Add(MakeCardRef(Setup.HeroInstanceId, Setup.HeroCardId, PlayerId));
		for (int32 Index = 0; Index < 8; ++Index)
		{
			Setup.OrderedDeck.Add(MakeCardRef(
				FString::Printf(TEXT("p%d_filler_%d"), PlayerId, Index),
				TEXT("private_draw"), PlayerId));
		}
		if (PlayerId == 0)
		{
			Setup.OrderedDeck.Add(MakeCardRef(
				TEXT("coordinator_private_candidate"), TEXT("csn_candidate_a"), 0));
		}
		Request.Players.Add(MoveTemp(Setup));
	}
	for (int32 Index = 0; Index < 8; ++Index)
	{
		FWBSetupMarkerPlacement Marker;
		Marker.PlayerId = Index < 4 ? 0 : 1;
		Marker.Type = Index % 2 == 0 ? EWBMarkerType::Trap : EWBMarkerType::NPC;
		Marker.DefinitionId = Marker.Type == EWBMarkerType::Trap
			? TEXT("rook_test_trap") : TEXT("rook_test_npc");
		Marker.Tile = Index < 4 ? FWBTile(Index, 7) : FWBTile(Index - 4, 1);
		Marker.PlacementOrder = Index;
		Request.MarkerPlacements.Add(Marker);
	}
	return Request;
}

void AddDeckCard(FWBGameStateData& State, const FString& InstanceId, const FString& CardId)
{
	FWBPlayerCardZoneState* Zones = WBCardZoneState::FindMutablePlayerZones(
		State.GetMutableCardZoneStateForTest(), 0);
	check(Zones != nullptr);
	Zones->Deck.Add(MakeDeckEntry(InstanceId, CardId, Zones->Deck.Num()));
}

void AddRookWand(FWBGameStateData& State, const FString& InstanceId = TEXT("rook_wand_instance"))
{
	FWBEquippedCardEntry Wand;
	Wand.Card.InstanceId = InstanceId;
	Wand.Card.CardId = TEXT("csn_wand");
	Wand.Card.OwnerPlayerId = 0;
	Wand.EquippedToUnitId = RookId;
	Wand.SlotId = TEXT("primary");
	Wand.EquipOrder = 0;
	State.GetMutableCardZoneStateForTest().EquippedCards.Add(Wand);
}

const FWBPlayerCardZoneState* PlayerZones(const FWBGameStateData& State)
{
	return WBCardZoneState::FindPlayerZones(State.GetCardZoneState(), 0);
}

FWBPostDestructionTriggerResult DestroyAndAdvance(
	FWBGameStateData& State,
	const FWBCardDefinitionRepository& Repository,
	const EWBUnitDestructionCause Cause = EWBUnitDestructionCause::BattleDamage)
{
	const FWBApplyActionResult Death = WBDeathResolution::ApplyZeroHPDeathResolution(State, Cause);
	if (!Death.bOk)
	{
		FWBPostDestructionTriggerResult Failed;
		Failed.Reason = Death.Reason;
		return Failed;
	}
	return WBPostDestructionTrigger::AdvanceToDecisionOrComplete(
		State, Repository, 0, static_cast<int32>(EWBMatchLoopPhase::Action));
}

const FWBUnitState* FindBoardCard(const FWBGameStateData& State, const FString& CardId)
{
	return State.Units.FindByPredicate([&CardId](const FWBUnitState& Unit)
	{
		return Unit.CardId == CardId && Unit.IsUnitOnBoard();
	});
}

int32 CountTrace(const TArray<FWBTraceEvent>& Events, const FName Kind)
{
	return Events.FilterByPredicate([Kind](const FWBTraceEvent& Event)
	{
		return Event.Kind == Kind;
	}).Num();
}
}

#define WB_ROOK_TEST(ClassName, TestName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, TestName, \
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

WB_ROOK_TEST(FWBCSNRookProductionDefinitionTest,
	"Wandbound.CSNRook.CardDB.ProductionDefinition")
bool FWBCSNRookProductionDefinitionTest::RunTest(const FString&)
{
	const FString Path = FPaths::Combine(FPaths::ProjectDir(),
		TEXT("Data/CardDB/Production/CSNCrashIn/root_manifest.json"));
	const FWBProductionCardDatabaseLoadResult Loaded =
		WBProductionCardDatabase::LoadManifestSuite(Path);
	TestTrue(TEXT("1 Production suite loads"), Loaded.bOk);
	if (!Loaded.Snapshot.IsValid()) return false;
	const FWBProductionCardRecord* Record = Loaded.Snapshot->FindRecord(TEXT("char_csn_rook"));
	TestNotNull(TEXT("2 Rook production definition loads"), Record);
	if (Record == nullptr) return false;
	TestEqual(TEXT("3 HP 16"), Record->CoreDefinition.CharacterStats.HP, 16);
	TestEqual(TEXT("4 ATK 3"), Record->CoreDefinition.CharacterStats.ATK, 3);
	TestEqual(TEXT("5 AR 2"), Record->CoreDefinition.CharacterStats.AR, 2);
	TestEqual(TEXT("6 owner correction RL 2"), Record->CoreDefinition.CharacterStats.RL, 2);
	TestTrue(TEXT("7 CSN faction"), Record->CoreDefinition.PublicFactions.Contains(TEXT("csn")));
	TestEqual(TEXT("8 one destruction trigger"), Record->CoreDefinition.AfterUnitDestroyedTriggers.Num(), 1);
	if (Record->CoreDefinition.AfterUnitDestroyedTriggers.IsEmpty()) return false;
	const FWBAfterUnitDestroyedTriggerDefinition& Trigger = Record->CoreDefinition.AfterUnitDestroyedTriggers[0];
	TestEqual(TEXT("9 destroyed self"), Trigger.SourceScope, EWBAfterUnitDestroyedSourceScope::DestroyedSelf);
	TestEqual(TEXT("10 generic Deck summon"), Trigger.Operation,
		EWBPostDestructionEffectOperation::SummonCharacterFromDeckToDestroyedTile);
	TestTrue(TEXT("11 mandatory"), Trigger.bMandatory);
	TestTrue(TEXT("12 ignores ordinary summon conditions"), Trigger.bIgnoreSummoningConditions);
	TestTrue(TEXT("13 inheritance applies"), Trigger.bApplyCSNInheritance);
	return true;
}

WB_ROOK_TEST(FWBCSNRookDestructionEventTest,
	"Wandbound.CSNRook.Destruction.EventBoundaryAndCauses")
bool FWBCSNRookDestructionEventTest::RunTest(const FString&)
{
	const EWBUnitDestructionCause Causes[] = {
		EWBUnitDestructionCause::BattleDamage,
		EWBUnitDestructionCause::EffectDamage,
		EWBUnitDestructionCause::StatusDamage,
		EWBUnitDestructionCause::ExplicitDestroy };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Causes); ++Index)
	{
		FWBGameStateData State = MakeState(TEXT("fixture_rook"),
			Causes[Index] == EWBUnitDestructionCause::ExplicitDestroy ? 5 : 0);
		const FWBApplyActionResult Result = Causes[Index] == EWBUnitDestructionCause::ExplicitDestroy
			? WBDeathResolution::ApplyExplicitUnitDestruction(State, RookId)
			: WBDeathResolution::ApplyZeroHPDeathResolution(State, Causes[Index]);
		TestTrue(FString::Printf(TEXT("%d destruction succeeds"), Index + 14), Result.bOk);
		TestEqual(FString::Printf(TEXT("%d one event"), Index + 18),
			State.PendingUnitDestructionEvents.Num(), 1);
		if (!State.PendingUnitDestructionEvents.IsEmpty())
		{
			TestEqual(FString::Printf(TEXT("%d cause retained"), Index + 22),
				State.PendingUnitDestructionEvents[0].Cause, Causes[Index]);
		}
	}

	FWBGameStateData Removed = MakeState(TEXT("fixture_rook"), 5);
	Removed.GetMutableUnitById(RookId)->RemoveUnitFromBoard();
	TestEqual(TEXT("26 non-destruction removal emits no event"),
		Removed.PendingUnitDestructionEvents.Num(), 0);
	FWBGameStateData Sacrificed = MakeState(TEXT("fixture_rook"), 5);
	Sacrificed.GetMutableUnitById(RookId)->MarkUnitDefeated();
	Sacrificed.GetMutableUnitById(RookId)->RemoveUnitFromBoard();
	TestEqual(TEXT("27 sacrifice-like removal emits no event"),
		Sacrificed.PendingUnitDestructionEvents.Num(), 0);
	FWBGameStateData Captured = MakeState(TEXT("fixture_rook"), 5);
	FWBUnitDestructionSnapshot Snapshot;
	FString Reason;
	TestTrue(TEXT("28 capture succeeds before cleanup"),
		WBDeathResolution::BuildSuccessfulDestructionSnapshot(Captured, RookId,
			EWBUnitDestructionCause::BattleDamage, 0, Snapshot, Reason));
	TestEqual(TEXT("29 capture alone does not publish"), Captured.PendingUnitDestructionEvents.Num(), 0);
	return true;
}

WB_ROOK_TEST(FWBCSNRookPassiveSuppressionTest,
	"Wandbound.CSNRook.Passive.GenericSuppression")
bool FWBCSNRookPassiveSuppressionTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	const FName Statuses[] = { NAME_None, FName(TEXT("Negated")), FName(TEXT("Stunned")), FName(TEXT("Frozen")) };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Statuses); ++Index)
	{
		FWBGameStateData State = MakeState();
		AddDeckCard(State, TEXT("candidate"), TEXT("csn_candidate_a"));
		if (!Statuses[Index].IsNone()) State.GetMutableUnitById(RookId)->AddStatus(Statuses[Index]);
		const FWBPostDestructionTriggerResult Result = DestroyAndAdvance(State, Repository);
		TestTrue(FString::Printf(TEXT("%d resolution succeeds"), Index + 30), Result.bOk);
		TestEqual(FString::Printf(TEXT("%d eligibility preserved"), Index + 34),
			State.HasPendingMandatoryDeckChoice(), Index == 0);
		if (Index > 0)
		{
			TestEqual(FString::Printf(TEXT("%d suppressed trace"), Index + 37),
				CountTrace(Result.TraceEvents, FName(TEXT("post_destruction_trigger_suppressed"))), 1);
		}
	}
	return true;
}

WB_ROOK_TEST(FWBCSNRookDeckChoiceTest,
	"Wandbound.CSNRook.DeckChoice.EligibilityDeterminismAndPrivacy")
bool FWBCSNRookDeckChoiceTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData Empty = MakeState();
	const FWBPostDestructionTriggerResult EmptyResult = DestroyAndAdvance(Empty, Repository);
	TestTrue(TEXT("41 empty Deck resolves"), EmptyResult.bOk);
	TestFalse(TEXT("42 empty Deck does not deadlock"), Empty.HasPendingMandatoryDeckChoice());

	FWBGameStateData State = MakeState();
	AddDeckCard(State, TEXT("a_copy_2"), TEXT("csn_candidate_a"));
	AddDeckCard(State, TEXT("officer"), TEXT("officer_candidate"));
	AddDeckCard(State, TEXT("wand"), TEXT("csn_wand"));
	AddDeckCard(State, TEXT("action"), TEXT("csn_action"));
	AddDeckCard(State, TEXT("hybrid"), TEXT("csn_hybrid"));
	AddDeckCard(State, TEXT("a_copy_1"), TEXT("csn_candidate_a"));
	AddDeckCard(State, TEXT("candidate_b"), TEXT("csn_candidate_b"));
	const FWBPostDestructionTriggerResult Result = DestroyAndAdvance(State, Repository);
	TestTrue(TEXT("43 choice opens"), Result.bPendingChoice);
	const TArray<FString> Actions = WBPostDestructionTrigger::EnumerateLegalChoiceActionIds(State, Repository);
	TestEqual(TEXT("44 only CSN Characters qualify"), Actions.Num(), 3);
	if (Actions.Num() != 3) return false;
	TestTrue(TEXT("45 first duplicate exact instance"), Actions[0].EndsWith(TEXT(":ia_copy_2")));
	TestTrue(TEXT("46 second duplicate exact instance"), Actions[1].EndsWith(TEXT(":ia_copy_1")));
	TestTrue(TEXT("47 third exact instance"), Actions[2].EndsWith(TEXT(":icandidate_b")));
	TestFalse(TEXT("48 non-CSN excluded"), FString::Join(Actions, TEXT("|")).Contains(TEXT("officer")));
	TestFalse(TEXT("49 Wand excluded"), FString::Join(Actions, TEXT("|")).Contains(TEXT(":iwand")));
	TestFalse(TEXT("50 action excluded"), FString::Join(Actions, TEXT("|")).Contains(TEXT(":iaction")));
	TestFalse(TEXT("51 Hybrid excluded"), FString::Join(Actions, TEXT("|")).Contains(TEXT(":ihybrid")));
	TestEqual(TEXT("52 selected set immutable"), State.PendingMandatoryDeckChoice.EligibleCardInstanceIds.Num(), 3);

	const FWBCardZonePublicSummary Public = WBCardZoneObservation::BuildPublicSummary(State);
	TestFalse(TEXT("53 public zones hide candidate identity"),
		WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(Public, TEXT("a_copy_2")));
	TestFalse(TEXT("54 public zones hide selected card id"),
		WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(Public, TEXT("csn_candidate_a")));
	FWBGameStateData Stale = State;
	FWBPlayerCardZoneState* StaleZones = WBCardZoneState::FindMutablePlayerZones(
		Stale.GetMutableCardZoneStateForTest(), 0);
	StaleZones->Deck.RemoveAt(0);
	const FWBPostDestructionTriggerResult StaleResult =
		WBPostDestructionTrigger::SubmitChoice(Stale, Repository, Actions[0]);
	TestTrue(TEXT("55 stale choice resolves historical trigger without rollback"), StaleResult.bOk);
	TestFalse(TEXT("56 stale choice does not summon"), StaleResult.bSummoned);
	return true;
}

WB_ROOK_TEST(FWBCSNRookSummonInheritanceTest,
	"Wandbound.CSNRook.Summon.TransactionalInheritance")
bool FWBCSNRookSummonInheritanceTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState();
	AddDeckCard(State, TEXT("selected_instance"), TEXT("csn_candidate_a"));
	AddRookWand(State);
	const FWBPostDestructionTriggerResult Pending = DestroyAndAdvance(State, Repository);
	TestTrue(TEXT("57 pending choice"), Pending.bPendingChoice);
	if (!Pending.bPendingChoice || State.PendingUnitDestructionEvents.IsEmpty()) return false;
	TestEqual(TEXT("58 source Current RL captured"), State.PendingUnitDestructionEvents[0].CurrentRLSnapshot, 5);
	TestEqual(TEXT("59 exact Wand captured"), State.PendingUnitDestructionEvents[0].EquippedWands.Num(), 1);
	TestEqual(TEXT("60 Wand moved to Discard during death"), PlayerZones(State)->Discard.Num(), 1);
	const TArray<FString> Actions = WBPostDestructionTrigger::EnumerateLegalChoiceActionIds(State, Repository);
	if (Actions.IsEmpty()) return false;
	const FString Action = Actions[0];
	const FWBPostDestructionTriggerResult Resolved = WBPostDestructionTrigger::SubmitChoice(State, Repository, Action);
	TestTrue(TEXT("61 trigger summon succeeds"), Resolved.bSummoned);
	TestEqual(TEXT("62 selected Deck instance leaves exactly once"), PlayerZones(State)->Deck.Num(), 0);
	const FWBUnitState* Summoned = FindBoardCard(State, TEXT("csn_candidate_a"));
	TestNotNull(TEXT("63 one board unit created"), Summoned);
	if (Summoned == nullptr) return false;
	TestEqual(TEXT("64 exact old X"), Summoned->X, 4);
	TestEqual(TEXT("65 exact old Y"), Summoned->Y, 4);
	TestEqual(TEXT("66 printed HP"), Summoned->HP, 11);
	TestEqual(TEXT("67 printed ATK"), Summoned->ATK, 4);
	TestEqual(TEXT("68 printed AR"), Summoned->AR, 3);
	TestEqual(TEXT("69 inherited Base RL"), Summoned->GetBaseRLForRules(), 7);
	TestEqual(TEXT("70 canonical Current RL"), Summoned->GetCurrentRLForRules(), 7);
	TestEqual(TEXT("71 canonical RLUsed"), Summoned->RLUsed, 1);
	TestEqual(TEXT("72 Wand no longer in Discard"), PlayerZones(State)->Discard.Num(), 0);
	TestEqual(TEXT("73 exact Wand exists once equipped"), State.GetCardZoneState().EquippedCards.Num(), 1);
	TestEqual(TEXT("74 exact Wand instance preserved"),
		State.GetCardZoneState().EquippedCards[0].Card.InstanceId, FString(TEXT("rook_wand_instance")));
	TestEqual(TEXT("75 normal summon action untouched"), State.GetPlayerById(0)->RemainingMP, 0);
	TestFalse(TEXT("76 summoned unit is not Hero"), State.GetPlayerById(0)->HeroUnitId == Summoned->UnitId);
	return true;
}

WB_ROOK_TEST(FWBCSNRookUndertowCompositionTest,
	"Wandbound.CSNRook.Composition.UndertowAndAtomicFailure")
bool FWBCSNRookUndertowCompositionTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState();
	AddDeckCard(State, TEXT("undertow_instance"), TEXT("undertow_candidate"));
	AddDeckCard(State, TEXT("draw_instance"), TEXT("private_draw"));
	AddRookWand(State);
	const FWBPostDestructionTriggerResult Pending = DestroyAndAdvance(State, Repository);
	TestTrue(TEXT("77 Rook trigger reaches Undertow choice"), Pending.bPendingChoice);
	const TArray<FString> Actions = WBPostDestructionTrigger::EnumerateLegalChoiceActionIds(State, Repository);
	if (Actions.IsEmpty()) return false;
	const FString Action = Actions[0];
	const FWBPostDestructionTriggerResult Result = WBPostDestructionTrigger::SubmitChoice(State, Repository, Action);
	TestTrue(TEXT("78 Rook to Undertow succeeds"), Result.bSummoned);
	const FWBTraceEvent* DeclaredTarget = Result.TraceEvents.FindByPredicate(
		[](const FWBTraceEvent& Event)
		{
			return Event.Kind == FName(TEXT("mandatory_deck_target_declared"));
		});
	TestTrue(TEXT("78a Rook exact Deck choice is a declared target"),
		DeclaredTarget != nullptr && DeclaredTarget->bDeclaredTarget);
	TestTrue(TEXT("78b Rook trigger is not declared"),
		DeclaredTarget != nullptr && !DeclaredTarget->bDeclaredActivation);
	TestEqual(TEXT("78c Rook trigger remains resolution-only"),
		CountTrace(Result.TraceEvents,
			FName(TEXT("pending_effect_activation_declared"))), 0);
	const FWBUnitState* Undertow = FindBoardCard(State, TEXT("undertow_candidate"));
	TestNotNull(TEXT("79 Undertow summoned"), Undertow);
	TestEqual(TEXT("80 inheritance trace once"), CountTrace(Result.TraceEvents, FName(TEXT("csn_inheritance"))), 1);
	TestEqual(TEXT("81 Undertow draw trace once"), CountTrace(Result.TraceEvents, FName(TEXT("csn_inheritance_card_drawn"))), 1);
	TestEqual(TEXT("82 one private draw"), PlayerZones(State)->Hand.Num(), 1);
	if (PlayerZones(State)->Hand.IsEmpty()) return false;
	TestEqual(TEXT("83 private draw exact instance"), PlayerZones(State)->Hand[0].Card.InstanceId, FString(TEXT("draw_instance")));

	FWBGameStateData MissingWand = MakeState();
	AddDeckCard(MissingWand, TEXT("candidate"), TEXT("csn_candidate_a"));
	AddRookWand(MissingWand, TEXT("missing_wand"));
	DestroyAndAdvance(MissingWand, Repository);
	const TArray<FString> MissingActions = WBPostDestructionTrigger::EnumerateLegalChoiceActionIds(MissingWand, Repository);
	if (MissingActions.IsEmpty()) return false;
	const FString MissingAction = MissingActions[0];
	FWBPlayerCardZoneState* Zones = WBCardZoneState::FindMutablePlayerZones(
		MissingWand.GetMutableCardZoneStateForTest(), 0);
	Zones->Discard.Reset();
	const FWBPostDestructionTriggerResult Failed =
		WBPostDestructionTrigger::SubmitChoice(MissingWand, Repository, MissingAction);
	TestFalse(TEXT("84 unavailable Wand fails trigger"), Failed.bSummoned);
	TestEqual(TEXT("85 selected card remains in Deck"), Zones->Deck.Num(), 1);
	TestNull(TEXT("86 no partial spawn"), FindBoardCard(MissingWand, TEXT("csn_candidate_a")));
	TestNull(TEXT("87 Rook stays destroyed"), FindBoardCard(MissingWand, TEXT("fixture_rook")));
	TestEqual(TEXT("88 no Wand fabricated"), MissingWand.GetCardZoneState().EquippedCards.Num(), 0);
	return true;
}

WB_ROOK_TEST(FWBCSNRookVexCompositionTest,
	"Wandbound.CSNVex.Composition.RookInheritanceActivatesAura")
bool FWBCSNRookVexCompositionTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData State = MakeState();
	FWBUnitState* EnemyHero = State.GetMutableUnitById(EnemyHeroId);
	if (EnemyHero == nullptr) return false;
	EnemyHero->X = 4;
	EnemyHero->Y = 7;
	AddDeckCard(State, TEXT("vex_instance"), TEXT("char_csn_vex"));
	AddRookWand(State);
	const FWBPostDestructionTriggerResult Pending = DestroyAndAdvance(State, Repository);
	TestTrue(TEXT("Rook creates mandatory Vex choice"), Pending.bPendingChoice);
	const TArray<FString> Actions =
		WBPostDestructionTrigger::EnumerateLegalChoiceActionIds(State, Repository);
	if (Actions.Num() != 1) return false;
	const FWBPostDestructionTriggerResult Resolved =
		WBPostDestructionTrigger::SubmitChoice(State, Repository, Actions[0]);
	TestTrue(TEXT("Rook summons Vex"), Resolved.bSummoned);
	const FWBUnitState* Vex = FindBoardCard(State, TEXT("char_csn_vex"));
	TestNotNull(TEXT("Vex exists on Rook tile"), Vex);
	if (Vex == nullptr) return false;
	TestEqual(TEXT("Vex inherits Rook Current RL"), Vex->GetBaseRLForRules(), 7);
	TestEqual(TEXT("Exact Rook Wand transfers"),
		State.GetCardZoneState().EquippedCards[0].EquippedToUnitId, Vex->UnitId);
	TestEqual(TEXT("Vex aura activates after summon"),
		WBUnitStatQuery::GetEffectiveAR(State, Repository, EnemyHeroId).EffectiveValue,
		State.GetUnitById(EnemyHeroId)->AR - 1);
	return true;
}

WB_ROOK_TEST(FWBCSNRookEdgesTest,
	"Wandbound.CSNRook.Edges.OccupiedHeroCombatAndDefinitionDriven")
bool FWBCSNRookEdgesTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	FWBGameStateData Occupied = MakeState();
	AddDeckCard(Occupied, TEXT("candidate"), TEXT("csn_candidate_a"));
	const FWBApplyActionResult Death = WBDeathResolution::ApplyZeroHPDeathResolution(
		Occupied, EWBUnitDestructionCause::ReplacementEffect);
	TestTrue(TEXT("88 parent destruction commits"), Death.bOk);
	Occupied.AddUnitForTest(MakeUnit(30, 0, TEXT("csn_candidate_b"), FWBTile(4, 4), 12, 3, 3, 0));
	const FWBPostDestructionTriggerResult OccupiedResult =
		WBPostDestructionTrigger::AdvanceToDecisionOrComplete(Occupied, Repository, 0,
			static_cast<int32>(EWBMatchLoopPhase::Action));
	TestTrue(TEXT("89 occupied trigger resolves"), OccupiedResult.bOk);
	TestFalse(TEXT("90 occupied tile creates no choice"), Occupied.HasPendingMandatoryDeckChoice());
	TestEqual(TEXT("91 existing replacement preserved"), Occupied.UnitIdAt(FWBTile(4, 4)), 30);

	FWBGameStateData Combat = MakeState();
	FWBPendingAttackState Pending;
	Pending.bActive = true;
	Pending.AttackerUnitId = EnemyHeroId;
	Pending.DefenderUnitId = RookId;
	Pending.OriginalAttackerUnitId = EnemyHeroId;
	Pending.OriginalDefenderUnitId = RookId;
	Pending.AttackingPlayerId = 1;
	Pending.ContinuationId = TEXT("combat_rook");
	Combat.SetPendingAttackForTest(Pending);
	AddDeckCard(Combat, TEXT("candidate"), TEXT("csn_candidate_a"));
	DestroyAndAdvance(Combat, Repository);
	const FString CombatAction = WBPostDestructionTrigger::EnumerateLegalChoiceActionIds(Combat, Repository)[0];
	WBPostDestructionTrigger::SubmitChoice(Combat, Repository, CombatAction);
	TestFalse(TEXT("92 original combat remains complete"), Combat.HasPendingAttack());
	TestEqual(TEXT("93 no extra attack budget consumed"), Combat.GetUnitById(EnemyHeroId)->AttacksLeft, 1);
	TestEqual(TEXT("94 replacement cannot counter retroactively"),
		FindBoardCard(Combat, TEXT("csn_candidate_a"))->AttacksLeft, 0);

	FWBGameStateData Hero = MakeState(TEXT("fixture_rook"), 0, true);
	AddDeckCard(Hero, TEXT("candidate"), TEXT("csn_candidate_a"));
	const FWBApplyActionResult HeroDeath = WBDeathResolution::ApplyZeroHPDeathResolution(Hero,
		EWBUnitDestructionCause::BattleDamage);
	TestTrue(TEXT("95 Hero Rook death succeeds"), HeroDeath.bOk);
	TestTrue(TEXT("96 Hero Rook remains terminal"), Hero.bGameOver);
	const FWBPostDestructionTriggerResult HeroTrigger =
		WBPostDestructionTrigger::AdvanceToDecisionOrComplete(Hero, Repository, 0,
			static_cast<int32>(EWBMatchLoopPhase::Action));
	TestFalse(TEXT("97 no post-terminal choice"), HeroTrigger.bPendingChoice);
	TestNull(TEXT("98 no replacement Hero"), FindBoardCard(Hero, TEXT("csn_candidate_a")));

	const FWBCardDefinitionRepository Alternate = MakeRepository(TEXT("alternate_destroyed_self"), true);
	FWBGameStateData AlternateState = MakeState(TEXT("alternate_destroyed_self"));
	AddDeckCard(AlternateState, TEXT("candidate"), TEXT("csn_candidate_a"));
	TestTrue(TEXT("99 alternate ID works"), DestroyAndAdvance(AlternateState, Alternate).bPendingChoice);
	const FWBCardDefinitionRepository NoSemantic = MakeRepository(TEXT("rook_like_name"), false);
	FWBGameStateData NoSemanticState = MakeState(TEXT("rook_like_name"));
	AddDeckCard(NoSemanticState, TEXT("candidate"), TEXT("csn_candidate_a"));
	TestFalse(TEXT("100 Rook-like ID without definition does not trigger"),
		DestroyAndAdvance(NoSemanticState, NoSemantic).bPendingChoice);
	return true;
}

WB_ROOK_TEST(FWBCSNRookFundamentalLegalityTest,
	"Wandbound.CSNRook.Edges.FundamentalLegalityAndDeckOrder")
bool FWBCSNRookFundamentalLegalityTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();

	FWBGameStateData OrderedDeck = MakeState();
	AddDeckCard(OrderedDeck, TEXT("left_private"), TEXT("private_draw"));
	AddDeckCard(OrderedDeck, TEXT("selected_middle"), TEXT("csn_candidate_a"));
	AddDeckCard(OrderedDeck, TEXT("right_private"), TEXT("private_draw"));
	TestTrue(TEXT("126 ordered Deck opens exact middle choice"),
		DestroyAndAdvance(OrderedDeck, Repository).bPendingChoice);
	const TArray<FString> OrderedActions =
		WBPostDestructionTrigger::EnumerateLegalChoiceActionIds(
			OrderedDeck, Repository);
	TestEqual(TEXT("127 one eligible middle choice"), OrderedActions.Num(), 1);
	if (OrderedActions.Num() != 1) return false;
	TestTrue(TEXT("128 ordered Deck summon succeeds"),
		WBPostDestructionTrigger::SubmitChoice(
			OrderedDeck, Repository, OrderedActions[0]).bSummoned);
	const FWBPlayerCardZoneState* OrderedZones = PlayerZones(OrderedDeck);
	TestEqual(TEXT("129 unselected Deck entries remain"),
		OrderedZones != nullptr ? OrderedZones->Deck.Num() : -1, 2);
	if (OrderedZones == nullptr || OrderedZones->Deck.Num() != 2) return false;
	TestEqual(TEXT("130 left Deck instance remains first"),
		OrderedZones->Deck[0].Card.InstanceId, FString(TEXT("left_private")));
	TestEqual(TEXT("131 right Deck instance remains second"),
		OrderedZones->Deck[1].Card.InstanceId, FString(TEXT("right_private")));
	TestEqual(TEXT("132 remaining Deck reindexes canonically"),
		OrderedZones->Deck[1].ZoneIndex, 1);

	FWBGameStateData OutOfBounds = MakeState();
	AddDeckCard(OutOfBounds, TEXT("candidate"), TEXT("csn_candidate_a"));
	TestTrue(TEXT("133 out-of-bounds source death commits"),
		WBDeathResolution::ApplyZeroHPDeathResolution(
			OutOfBounds, EWBUnitDestructionCause::EffectDamage).bOk);
	if (OutOfBounds.PendingUnitDestructionEvents.IsEmpty()) return false;
	OutOfBounds.PendingUnitDestructionEvents[0].LastTile = FWBTile(-1, 4);
	const FWBPostDestructionTriggerResult OutOfBoundsResult =
		WBPostDestructionTrigger::AdvanceToDecisionOrComplete(
			OutOfBounds, Repository, 0,
			static_cast<int32>(EWBMatchLoopPhase::Action));
	TestTrue(TEXT("134 out-of-bounds trigger resolves fail closed"),
		OutOfBoundsResult.bOk);
	TestFalse(TEXT("135 out-of-bounds trigger creates no choice"),
		OutOfBounds.HasPendingMandatoryDeckChoice());
	TestNull(TEXT("136 out-of-bounds trigger creates no unit"),
		FindBoardCard(OutOfBounds, TEXT("csn_candidate_a")));

	FWBGameStateData AtCap = MakeState();
	AddDeckCard(AtCap, TEXT("candidate"), TEXT("csn_candidate_a"));
	TestTrue(TEXT("137 capped source death commits"),
		WBDeathResolution::ApplyZeroHPDeathResolution(
			AtCap, EWBUnitDestructionCause::BattleDamage).bOk);
	AtCap.AddUnitForTest(MakeUnit(31, 0, TEXT("csn_candidate_b"),
		FWBTile(1, 1), 12, 3, 3, 0));
	AtCap.AddUnitForTest(MakeUnit(32, 0, TEXT("csn_candidate_b"),
		FWBTile(2, 1), 12, 3, 3, 0));
	AtCap.AddUnitForTest(MakeUnit(33, 0, TEXT("csn_candidate_b"),
		FWBTile(3, 1), 12, 3, 3, 0));
	const FWBPostDestructionTriggerResult CapResult =
		WBPostDestructionTrigger::AdvanceToDecisionOrComplete(
			AtCap, Repository, 0,
			static_cast<int32>(EWBMatchLoopPhase::Action));
	TestTrue(TEXT("138 unit-cap trigger resolves fail closed"), CapResult.bOk);
	TestFalse(TEXT("139 unit-cap trigger creates no choice"),
		AtCap.HasPendingMandatoryDeckChoice());
	TestNull(TEXT("140 unit-cap trigger creates no unit"),
		FindBoardCard(AtCap, TEXT("csn_candidate_a")));

	FWBGameStateData OneForOne = MakeState();
	AddDeckCard(OneForOne, TEXT("candidate"), TEXT("csn_candidate_a"));
	OneForOne.AddUnitForTest(MakeUnit(31, 0, TEXT("csn_candidate_b"),
		FWBTile(1, 1), 12, 3, 3, 0));
	OneForOne.AddUnitForTest(MakeUnit(32, 0, TEXT("csn_candidate_b"),
		FWBTile(2, 1), 12, 3, 3, 0));
	TestTrue(TEXT("141 one-for-one replacement remains legal at prior cap"),
		DestroyAndAdvance(OneForOne, Repository).bPendingChoice);

	FWBGameStateData ZeroInheritance = MakeState();
	ZeroInheritance.GetMutableUnitById(RookId)->SetCanonicalRL(2, 0, 0);
	AddDeckCard(ZeroInheritance, TEXT("candidate"), TEXT("csn_candidate_a"));
	TestTrue(TEXT("142 zero-value inheritance opens choice"),
		DestroyAndAdvance(ZeroInheritance, Repository).bPendingChoice);
	const TArray<FString> ZeroActions =
		WBPostDestructionTrigger::EnumerateLegalChoiceActionIds(
			ZeroInheritance, Repository);
	if (ZeroActions.IsEmpty()) return false;
	TestTrue(TEXT("143 zero RL and zero Wands still inherit successfully"),
		WBPostDestructionTrigger::SubmitChoice(
			ZeroInheritance, Repository, ZeroActions[0]).bSummoned);
	const FWBUnitState* ZeroSummoned =
		FindBoardCard(ZeroInheritance, TEXT("csn_candidate_a"));
	TestNotNull(TEXT("144 zero-value inherited unit exists"), ZeroSummoned);
	if (ZeroSummoned == nullptr) return false;
	TestEqual(TEXT("145 zero source RL preserves printed Base RL"),
		ZeroSummoned->GetBaseRLForRules(), 2);
	TestEqual(TEXT("146 zero source RL reconciles Current RL"),
		ZeroSummoned->GetCurrentRLForRules(), 2);
	TestEqual(TEXT("147 zero Wands leaves RLUsed zero"), ZeroSummoned->RLUsed, 0);

	FWBGameStateData Overflow = MakeState();
	Overflow.GetMutableUnitById(RookId)->SetCanonicalRL(2, 5, 8);
	AddDeckCard(Overflow, TEXT("candidate"), TEXT("csn_candidate_a"));
	for (int32 Index = 0; Index < 8; ++Index)
	{
		AddRookWand(Overflow, FString::Printf(TEXT("overflow_wand_%d"), Index));
		FWBEquippedCardEntry& Wand =
			Overflow.GetMutableCardZoneStateForTest().EquippedCards.Last();
		Wand.SlotId = FString::Printf(TEXT("slot_%d"), Index);
		Wand.EquipOrder = Index;
	}
	TestTrue(TEXT("148 overflow inheritance opens choice"),
		DestroyAndAdvance(Overflow, Repository).bPendingChoice);
	const TArray<FString> OverflowActions =
		WBPostDestructionTrigger::EnumerateLegalChoiceActionIds(Overflow, Repository);
	if (OverflowActions.IsEmpty()) return false;
	TestTrue(TEXT("149 overflow inheritance succeeds"),
		WBPostDestructionTrigger::SubmitChoice(
			Overflow, Repository, OverflowActions[0]).bSummoned);
	const FWBUnitState* OverflowSummoned =
		FindBoardCard(Overflow, TEXT("csn_candidate_a"));
	TestNotNull(TEXT("150 overflow inherited unit exists"), OverflowSummoned);
	if (OverflowSummoned == nullptr) return false;
	TestEqual(TEXT("151 overflow preserves inherited Base RL"),
		OverflowSummoned->GetBaseRLForRules(), 7);
	TestEqual(TEXT("152 overflow reconciles RLUsed to capacity"),
		OverflowSummoned->RLUsed, 7);
	TestEqual(TEXT("153 overflow leaves seven exact Wands equipped"),
		Overflow.GetCardZoneState().EquippedCards.Num(), 7);
	TestEqual(TEXT("154 overflow moves one exact Wand to Discard"),
		PlayerZones(Overflow)->Discard.Num(), 1);
	return true;
}

WB_ROOK_TEST(FWBCSNRookDeterminismTest,
	"Wandbound.CSNRook.Determinism.QueueStateAndTrace")
bool FWBCSNRookDeterminismTest::RunTest(const FString&)
{
	const FWBCardDefinitionRepository Repository = MakeRepository();
	auto Run = [&Repository](FWBGameStateData& State, TArray<FWBTraceEvent>& Trace)
	{
		State = MakeState();
		AddDeckCard(State, TEXT("candidate_a"), TEXT("csn_candidate_a"));
		AddRookWand(State);
		const FWBPostDestructionTriggerResult Pending = DestroyAndAdvance(State, Repository);
		Trace.Append(Pending.TraceEvents);
		const TArray<FString> Actions = WBPostDestructionTrigger::EnumerateLegalChoiceActionIds(State, Repository);
		if (Actions.IsEmpty()) return;
		const FWBPostDestructionTriggerResult Resolved =
			WBPostDestructionTrigger::SubmitChoice(State, Repository, Actions[0]);
		Trace.Append(Resolved.TraceEvents);
	};
	FWBGameStateData First;
	FWBGameStateData Second;
	TArray<FWBTraceEvent> FirstTrace;
	TArray<FWBTraceEvent> SecondTrace;
	Run(First, FirstTrace);
	Run(Second, SecondTrace);
	TestEqual(TEXT("101 state digest deterministic"),
		WBProductionMatchReplay::BuildGameStateDigest(First),
		WBProductionMatchReplay::BuildGameStateDigest(Second));
	TestEqual(TEXT("102 trace digest deterministic"),
		WBProductionMatchReplay::BuildTraceDigest(FirstTrace),
		WBProductionMatchReplay::BuildTraceDigest(SecondTrace));

	FWBGameStateData Ordered = MakeState();
	FWBUnitDestructionSnapshot Later;
	Later.EventId = TEXT("later");
	Later.DestroyedUnitId = 20;
	Later.ResolutionOrder = 1;
	FWBUnitDestructionSnapshot Earlier = Later;
	Earlier.EventId = TEXT("earlier");
	Earlier.DestroyedUnitId = 10;
	Earlier.ResolutionOrder = 0;
	WBDeathResolution::QueueSuccessfulDestructionEvent(Ordered, Later);
	WBDeathResolution::QueueSuccessfulDestructionEvent(Ordered, Earlier);
	TestEqual(TEXT("103 simultaneous events stable first"), Ordered.PendingUnitDestructionEvents[0].EventId,
		FString(TEXT("earlier")));
	TestEqual(TEXT("104 simultaneous events stable second"), Ordered.PendingUnitDestructionEvents[1].EventId,
		FString(TEXT("later")));
	return true;
}

WB_ROOK_TEST(FWBCSNRookCoordinatorChoiceTest,
	"Wandbound.CSNRook.Authority.CoordinatorChoiceReplayAndPrivacy")
bool FWBCSNRookCoordinatorChoiceTest::RunTest(const FString&)
{
	WBMatchCoordinator Coordinator;
	const FWBMatchOperationResult Initialized = Coordinator.InitializeMatch(
		MakeCoordinatorRequest());
	if (!Initialized.bOk)
	{
		AddError(FString::Printf(TEXT("Coordinator initialization failed: %s"),
			*Initialized.Reason));
	}
	TestTrue(TEXT("105 coordinator initializes"), Initialized.bOk);
	if (!Initialized.bOk) return false;

	FWBGameStateData& State = Coordinator.GetMutableStateForTest();
	FWBUnitDestructionSnapshot Event;
	Event.EventId = TEXT("coordinator_destroyed_event");
	Event.DestroyedUnitId = RookId;
	Event.DestroyedCardId = TEXT("fixture_rook");
	Event.ControllerPlayerId = 0;
	Event.LastTile = FWBTile(4, 4);
	Event.Cause = EWBUnitDestructionCause::EffectDamage;
	Event.bCharacterPassiveEligible = true;
	Event.ResolutionOrder = 0;
	WBDeathResolution::QueueSuccessfulDestructionEvent(State, Event);
	const FWBPostDestructionTriggerResult Pending =
		WBPostDestructionTrigger::AdvanceToDecisionOrComplete(
			State, Coordinator.GetRepository(), State.PriorityPlayer,
			static_cast<int32>(Coordinator.GetMatchPhase()));
	TestTrue(TEXT("106 coordinator choice becomes pending"), Pending.bPendingChoice);
	if (!Pending.bPendingChoice) return false;
	State.PriorityPlayer = 0;

	const FWBMatchLegalActionGenerationResult Legal = Coordinator.EnumerateLegalActions();
	TestTrue(TEXT("107 legal generation succeeds"), Legal.bOk);
	TestEqual(TEXT("108 exactly one private choice"), Legal.Actions.Num(), 1);
	if (Legal.Actions.IsEmpty()) return false;
	TestEqual(TEXT("109 generic mandatory choice family"), Legal.Actions[0].Family,
		EWBMatchActionFamily::MandatoryDeckChoice);
	TestEqual(TEXT("110 exact selected instance retained internally"),
		Legal.Actions[0].MandatoryChoiceCardInstanceId,
		FString(TEXT("coordinator_private_candidate")));
	const FWBMatchObservation Opponent = Coordinator.BuildObservation(1);
	TestEqual(TEXT("111 opponent receives no private actions"), Opponent.LegalActions.Num(), 0);
	TestFalse(TEXT("112 public observation hides candidate"),
		WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(
			Opponent.CardZones, TEXT("coordinator_private_candidate")));

	const FWBMatchOperationResult Submitted = Coordinator.SubmitActionId(
		0, Legal.Actions[0].ActionId);
	TestTrue(TEXT("113 coordinator accepts exact choice"), Submitted.bOk);
	TestEqual(TEXT("114 accepted replay record count"),
		Coordinator.GetCommittedActionRecords().Num(), 1);
	if (Coordinator.GetCommittedActionRecords().IsEmpty()) return false;
	TestEqual(TEXT("115 replay family generic"),
		Coordinator.GetCommittedActionRecords()[0].ActionFamily,
		FString(TEXT("mandatory_deck_choice")));
	TestEqual(TEXT("116 replay records exact authoritative action"),
		Coordinator.GetCommittedActionRecords()[0].ChosenActionId,
		Legal.Actions[0].ActionId);
	TestNotNull(TEXT("117 chosen unit publicly appears only after resolution"),
		FindBoardCard(Coordinator.GetState(), TEXT("csn_candidate_a")));
	TestEqual(TEXT("118 replay schema remains one"), WBProductionMatchReplay::SchemaVersion, 1);
	return true;
}

WB_ROOK_TEST(FWBCSNRookProductionSmokeTest,
	"Wandbound.CSNRook.Fixture.ProductionSmokeAndFreshReplay")
bool FWBCSNRookProductionSmokeTest::RunTest(const FString&)
{
	FWBProductionRuntimeBootstrapRequest Request;
	Request.CardBundleManifestPath = FPaths::Combine(FPaths::ProjectDir(),
		TEXT("Data/CardDB/Production/CSNCrashIn/root_manifest.json"));
	Request.MatchSpecificationPath = FPaths::Combine(FPaths::ProjectDir(),
		TEXT("Data/Replay/CSNRookFixture/match_spec.json"));
	const FWBProductionCSNCrashInSmokeResult First =
		WBProductionCSNCrashInSmoke::RunRook(Request);
	const FWBProductionCSNCrashInSmokeResult Second =
		WBProductionCSNCrashInSmoke::RunRook(Request);
	if (!First.bOk) AddError(FString::Printf(TEXT("First Rook smoke failed: %s"), *First.Reason));
	if (!Second.bOk) AddError(FString::Printf(TEXT("Second Rook smoke failed: %s"), *Second.Reason));
	TestTrue(TEXT("119 first production smoke"), First.bOk);
	TestTrue(TEXT("120 second production smoke"), Second.bOk);
	TestEqual(TEXT("121 archive byte deterministic"), First.SerializedArchive, Second.SerializedArchive);
	TestEqual(TEXT("122 receipt byte deterministic"), First.SerializedReceipt, Second.SerializedReceipt);
	TestEqual(TEXT("123 state digest deterministic"), First.FinalStateDigest, Second.FinalStateDigest);
	TestEqual(TEXT("124 trace digest deterministic"), First.FinalTraceDigest, Second.FinalTraceDigest);
	TestEqual(TEXT("125 replay count deterministic"), First.RecordsVerified, Second.RecordsVerified);
	return true;
}

#endif
