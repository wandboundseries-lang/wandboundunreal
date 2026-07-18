#include "Misc/AutomationTest.h"

#include "WBCardDefinitionRepository.h"
#include "WBCardZoneObservation.h"
#include "WBCardZoneState.h"
#include "WBEquipExecution.h"
#include "WBGameStateData.h"
#include "WBProductionSummonEquipDataProvider.h"
#include "WBPublicBoardSummary.h"
#include "WBResonanceOverflow.h"
#include "WBResonanceRecalculation.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 RLRecalcPlayerId = 0;
constexpr int32 RLRecalcOpponentId = 1;
constexpr int32 RLRecalcUnitId = 10;
constexpr int32 RLRecalcOtherUnitId = 11;
constexpr int32 RLRecalcOpponentUnitId = 20;

FWBCardDefinition MakeRLRecalcWandDefinition(const FString& CardId, const int32 RR)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.Kind = EWBCardDefinitionKind::Wand;
	Definition.WandStats.RR = RR;
	return Definition;
}

FWBCardDefinition MakeRLRecalcCharacterDefinition(const FString& CardId)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.Kind = EWBCardDefinitionKind::Character;
	Definition.CharacterStats.HP = 4;
	Definition.CharacterStats.ATK = 1;
	Definition.CharacterStats.AR = 1;
	Definition.CharacterStats.RL = 2;
	return Definition;
}

FWBCardDefinitionRepository MakeRLRecalcRepository(const TArray<FWBCardDefinition>& Definitions)
{
	FWBCardDefinitionRepository Repository;
	Repository.RepositoryId = TEXT("rl_recalculation_repository");
	Repository.SourceVersion = TEXT("test_v1");
	Repository.Definitions = Definitions;
	return Repository;
}

FWBUnitState MakeRLRecalcUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const int32 RL,
	const int32 RLUsed = 0)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = FString::Printf(TEXT("unit_%d"), UnitId);
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = 5;
	Unit.MaxHP = 5;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.RLTotal = RL;
	Unit.RLUsed = RLUsed;
	return Unit;
}

FWBPlayerCardZoneState MakeRLRecalcZones(const int32 PlayerId)
{
	FWBPlayerCardZoneState Zones;
	Zones.PlayerId = PlayerId;
	return Zones;
}

FWBGameStateData MakeRLRecalcState(const int32 RL = 4, const int32 RLUsed = 0)
{
	FWBGameStateData State;
	State.CurrentPlayer = RLRecalcPlayerId;
	State.PriorityPlayer = RLRecalcPlayerId;
	State.Phase = EWBGamePhase::NormalTurn;

	FWBPlayerStateData Player;
	Player.PlayerId = RLRecalcPlayerId;
	Player.HeroUnitId = RLRecalcUnitId;
	State.Players.Add(Player);

	FWBPlayerStateData Opponent;
	Opponent.PlayerId = RLRecalcOpponentId;
	Opponent.HeroUnitId = RLRecalcOpponentUnitId;
	State.Players.Add(Opponent);

	State.Units.Add(MakeRLRecalcUnit(RLRecalcUnitId, RLRecalcPlayerId, FWBTile(4, 4), RL, RLUsed));
	State.Units.Add(MakeRLRecalcUnit(RLRecalcOtherUnitId, RLRecalcPlayerId, FWBTile(5, 4), RL, 0));
	State.Units.Add(MakeRLRecalcUnit(RLRecalcOpponentUnitId, RLRecalcOpponentId, FWBTile(8, 8), RL, 0));

	State.CardZoneState.PlayerZones.Add(MakeRLRecalcZones(RLRecalcPlayerId));
	State.CardZoneState.PlayerZones.Add(MakeRLRecalcZones(RLRecalcOpponentId));
	return State;
}

FWBZoneCardEntry MakeRLRecalcZoneEntry(
	const FString& InstanceId,
	const FString& CardId,
	const int32 OwnerPlayerId,
	const EWBCardZone Zone,
	const int32 ZoneIndex)
{
	FWBZoneCardEntry Entry;
	Entry.Card.InstanceId = InstanceId;
	Entry.Card.CardId = CardId;
	Entry.Card.OwnerPlayerId = OwnerPlayerId;
	Entry.Zone = Zone;
	Entry.ZoneIndex = ZoneIndex;
	return Entry;
}

void AddRLRecalcHandCard(
	FWBGameStateData& State,
	const FString& InstanceId,
	const FString& CardId,
	const int32 ZoneIndex = 0)
{
	if (FWBPlayerCardZoneState* Zones =
		WBCardZoneState::FindMutablePlayerZones(State.GetMutableCardZoneStateForTest(), RLRecalcPlayerId))
	{
		Zones->Hand.Add(MakeRLRecalcZoneEntry(InstanceId, CardId, RLRecalcPlayerId, EWBCardZone::Hand, ZoneIndex));
	}
}

void AddRLRecalcDeckCard(
	FWBGameStateData& State,
	const FString& InstanceId,
	const FString& CardId,
	const int32 ZoneIndex = 0)
{
	if (FWBPlayerCardZoneState* Zones =
		WBCardZoneState::FindMutablePlayerZones(State.GetMutableCardZoneStateForTest(), RLRecalcPlayerId))
	{
		Zones->Deck.Add(MakeRLRecalcZoneEntry(InstanceId, CardId, RLRecalcPlayerId, EWBCardZone::Deck, ZoneIndex));
	}
}

void AddRLRecalcDiscardCard(
	FWBGameStateData& State,
	const FString& InstanceId,
	const FString& CardId,
	const int32 ZoneIndex = 0)
{
	if (FWBPlayerCardZoneState* Zones =
		WBCardZoneState::FindMutablePlayerZones(State.GetMutableCardZoneStateForTest(), RLRecalcPlayerId))
	{
		Zones->Discard.Add(MakeRLRecalcZoneEntry(InstanceId, CardId, RLRecalcPlayerId, EWBCardZone::Discard, ZoneIndex));
	}
}

void AddRLRecalcEquippedCard(
	FWBGameStateData& State,
	const FString& InstanceId,
	const FString& CardId,
	const int32 RRSortOrder,
	const int32 UnitId = RLRecalcUnitId,
	const FString& SlotId = FString())
{
	FWBEquippedCardEntry Entry;
	Entry.Card.InstanceId = InstanceId;
	Entry.Card.CardId = CardId;
	Entry.Card.OwnerPlayerId = RLRecalcPlayerId;
	Entry.EquippedToUnitId = UnitId;
	Entry.SlotId = SlotId.IsEmpty()
		? FString::Printf(TEXT("wand_%d"), RRSortOrder)
		: SlotId;
	Entry.EquipOrder = RRSortOrder;
	State.GetMutableCardZoneStateForTest().EquippedCards.Add(Entry);
}

FWBResonanceModifierState MakeRLRecalcCurrentModifier(
	const FString& SourceId,
	const int32 Amount)
{
	FWBResonanceModifierState Modifier;
	Modifier.SourceId = SourceId;
	Modifier.Target = EWBResonanceModifierTarget::CurrentRL;
	Modifier.Operation = EWBResonanceModifierOperation::Add;
	Modifier.Amount = Amount;
	return Modifier;
}

FWBEquipExecutionRequest MakeRLRecalcEquipRequest(
	const FString& InstanceId = TEXT("hand_wand"),
	const FString& CardId = TEXT("wand_hand"),
	const int32 TargetUnitId = RLRecalcUnitId)
{
	FWBEquipExecutionRequest Request;
	Request.PlayerId = RLRecalcPlayerId;
	Request.SourceInstanceId = InstanceId;
	Request.SourceCardId = CardId;
	Request.TargetUnitId = TargetUnitId;
	return Request;
}

bool UnitHasEquippedInstance(const FWBGameStateData& State, const FString& InstanceId)
{
	for (const FWBEquippedCardEntry& Entry : State.GetCardZoneState().EquippedCards)
	{
		if (Entry.Card.InstanceId == InstanceId)
		{
			return true;
		}
	}
	return false;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceRecalculationBaselineTest, "Wandbound.Core.ResonanceRecalculation.BaselineNoEquipment", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceRecalculationBaselineTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRLRecalcState(4, 99);
	const FWBCardDefinitionRepository Repository = MakeRLRecalcRepository({});

	const FWBResonanceRecalculationResult Result =
		WBResonanceRecalculation::RecalculateUnit(State, RLRecalcUnitId, Repository);
	const FWBUnitState* Unit = State.GetUnitById(RLRecalcUnitId);

	TestTrue(TEXT("Recalculation succeeds"), Result.bSucceeded);
	TestEqual(TEXT("Base RL from legacy RLTotal"), Result.RecalculatedBaseRL, 4);
	TestEqual(TEXT("Current RL from base"), Result.RecalculatedCurrentRL, 4);
	TestEqual(TEXT("RL used cleared from authoritative equipped cards"), Result.RecalculatedRLUsed, 0);
	TestEqual(TEXT("Available RL"), Result.AvailableRL, 4);
	TestFalse(TEXT("No overflow"), Result.bIsOverflowing);
	TestEqual(TEXT("Unit base RL committed"), Unit != nullptr ? Unit->BaseRL : -1, 4);
	TestEqual(TEXT("Unit current RL committed"), Unit != nullptr ? Unit->CurrentRL : -1, 4);
	TestEqual(TEXT("RLTotal compatibility mirrors current"), Unit != nullptr ? Unit->RLTotal : -1, 4);
	TestEqual(TEXT("Unit RL used committed"), Unit != nullptr ? Unit->RLUsed : -1, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceRecalculationEquippedRRTest, "Wandbound.Core.ResonanceRecalculation.EquippedWandsDeriveRLUsed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceRecalculationEquippedRRTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRLRecalcState(6, 0);
	AddRLRecalcEquippedCard(State, TEXT("wand_b"), TEXT("wand_b"), 2);
	AddRLRecalcEquippedCard(State, TEXT("wand_a"), TEXT("wand_a"), 1);
	const FWBCardDefinitionRepository Repository = MakeRLRecalcRepository({
		MakeRLRecalcWandDefinition(TEXT("wand_a"), 2),
		MakeRLRecalcWandDefinition(TEXT("wand_b"), 3)
	});

	const FWBResonanceRecalculationResult Result =
		WBResonanceRecalculation::RecalculateUnit(State, RLRecalcUnitId, Repository);

	TestTrue(TEXT("Recalculation succeeds"), Result.bSucceeded);
	TestEqual(TEXT("RL used summed"), Result.RecalculatedRLUsed, 5);
	TestEqual(TEXT("Available RL"), Result.AvailableRL, 1);
	TestEqual(TEXT("First ordered equipped instance"), Result.OrderedEquippedCardInstanceIds[0], FString(TEXT("wand_a")));
	TestEqual(TEXT("Second ordered equipped instance"), Result.OrderedEquippedCardInstanceIds[1], FString(TEXT("wand_b")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceRecalculationIgnoresNonEquippedZonesTest, "Wandbound.Core.ResonanceRecalculation.IgnoresHandDeckDiscardAndOtherUnits", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceRecalculationIgnoresNonEquippedZonesTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRLRecalcState(5, 0);
	AddRLRecalcHandCard(State, TEXT("hand_wand"), TEXT("wand_hand"));
	AddRLRecalcDeckCard(State, TEXT("deck_wand"), TEXT("wand_deck"));
	AddRLRecalcDiscardCard(State, TEXT("discard_wand"), TEXT("wand_discard"));
	AddRLRecalcEquippedCard(State, TEXT("other_unit_wand"), TEXT("wand_other"), 0, RLRecalcOtherUnitId, TEXT("wand"));
	AddRLRecalcEquippedCard(State, TEXT("real_wand"), TEXT("wand_real"), 1, RLRecalcUnitId, TEXT("wand"));
	const FWBCardDefinitionRepository Repository = MakeRLRecalcRepository({
		MakeRLRecalcWandDefinition(TEXT("wand_hand"), 4),
		MakeRLRecalcWandDefinition(TEXT("wand_deck"), 4),
		MakeRLRecalcWandDefinition(TEXT("wand_discard"), 4),
		MakeRLRecalcWandDefinition(TEXT("wand_other"), 4),
		MakeRLRecalcWandDefinition(TEXT("wand_real"), 2)
	});

	const FWBResonanceRecalculationResult Result =
		WBResonanceRecalculation::RecalculateUnit(State, RLRecalcUnitId, Repository);

	TestTrue(TEXT("Recalculation succeeds"), Result.bSucceeded);
	TestEqual(TEXT("Only target unit equipped RR counts"), Result.RecalculatedRLUsed, 2);
	TestEqual(TEXT("Only real target equipped listed"), Result.OrderedEquippedCardInstanceIds.Num(), 1);
	TestEqual(TEXT("Real target equipped listed"), Result.OrderedEquippedCardInstanceIds[0], FString(TEXT("real_wand")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceRecalculationIdempotentTest, "Wandbound.Core.ResonanceRecalculation.IdempotentNoDoubleCount", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceRecalculationIdempotentTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRLRecalcState(5, 99);
	AddRLRecalcEquippedCard(State, TEXT("wand_a"), TEXT("wand_a"), 0, RLRecalcUnitId, TEXT("wand"));
	const FWBCardDefinitionRepository Repository = MakeRLRecalcRepository({ MakeRLRecalcWandDefinition(TEXT("wand_a"), 2) });

	const FWBResonanceRecalculationResult First =
		WBResonanceRecalculation::RecalculateUnit(State, RLRecalcUnitId, Repository);
	const FWBResonanceRecalculationResult Second =
		WBResonanceRecalculation::RecalculateUnit(State, RLRecalcUnitId, Repository);
	FString FirstJson;
	FString SecondJson;
	WBResonanceRecalculation::RecalculationResultToJsonStringForTest(First, FirstJson);
	WBResonanceRecalculation::RecalculationResultToJsonStringForTest(Second, SecondJson);

	TestTrue(TEXT("First succeeds"), First.bSucceeded);
	TestTrue(TEXT("Second succeeds"), Second.bSucceeded);
	TestEqual(TEXT("RL used does not accumulate"), State.GetUnitById(RLRecalcUnitId)->RLUsed, 2);
	TestEqual(TEXT("Stable recalculated current RL"), First.RecalculatedCurrentRL, Second.RecalculatedCurrentRL);
	TestEqual(TEXT("Stable recalculated RL used"), First.RecalculatedRLUsed, Second.RecalculatedRLUsed);
	TestEqual(TEXT("Stable ordered equipped id count"), First.OrderedEquippedCardInstanceIds.Num(), Second.OrderedEquippedCardInstanceIds.Num());
	TestEqual(TEXT("Stable ordered equipped id"), First.OrderedEquippedCardInstanceIds[0], Second.OrderedEquippedCardInstanceIds[0]);
	TestTrue(TEXT("JSON serializes"), !FirstJson.IsEmpty() && !SecondJson.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceRecalculationCurrentModifierTest, "Wandbound.Core.ResonanceRecalculation.CurrentRLModifiersDeterministicAndIdempotent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceRecalculationCurrentModifierTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRLRecalcState(5, 99);
	FWBUnitState* Unit = State.GetMutableUnitById(RLRecalcUnitId);
	if (!TestNotNull(TEXT("Unit exists"), Unit))
	{
		return false;
	}

	Unit->BaseRL = 5;
	Unit->CurrentRL = 99;
	Unit->RLTotal = 99;
	Unit->ResonanceModifiers.Add(MakeRLRecalcCurrentModifier(TEXT("source_b_plus_two"), 2));
	Unit->ResonanceModifiers.Add(MakeRLRecalcCurrentModifier(TEXT("source_a_minus_one"), -1));
	AddRLRecalcEquippedCard(State, TEXT("wand_a"), TEXT("wand_a"), 0, RLRecalcUnitId, TEXT("wand"));
	const FWBCardDefinitionRepository Repository = MakeRLRecalcRepository({ MakeRLRecalcWandDefinition(TEXT("wand_a"), 2) });

	const FWBResonanceRecalculationResult First =
		WBResonanceRecalculation::RecalculateUnit(State, RLRecalcUnitId, Repository);
	const FWBResonanceRecalculationResult Second =
		WBResonanceRecalculation::RecalculateUnit(State, RLRecalcUnitId, Repository);
	const FWBUnitState* RecalculatedUnit = State.GetUnitById(RLRecalcUnitId);

	TestTrue(TEXT("First succeeds"), First.bSucceeded);
	TestTrue(TEXT("Second succeeds"), Second.bSucceeded);
	TestEqual(TEXT("Base RL unchanged"), First.RecalculatedBaseRL, 5);
	TestEqual(TEXT("Stale current ignored"), First.PreviousCurrentRL, 99);
	TestEqual(TEXT("Current RL applies sorted modifiers"), First.RecalculatedCurrentRL, 6);
	TestEqual(TEXT("RL used from equipped wand"), First.RecalculatedRLUsed, 2);
	TestEqual(TEXT("Available RL"), First.AvailableRL, 4);
	TestEqual(TEXT("First modifier sorted by source"), First.OrderedModifierSourceIds[0], FString(TEXT("source_a_minus_one")));
	TestEqual(TEXT("Second modifier sorted by source"), First.OrderedModifierSourceIds[1], FString(TEXT("source_b_plus_two")));
	TestEqual(TEXT("Second recalculation current stable"), Second.RecalculatedCurrentRL, 6);
	TestEqual(TEXT("Second recalculation RL used stable"), Second.RecalculatedRLUsed, 2);
	TestEqual(TEXT("Committed Base RL"), RecalculatedUnit != nullptr ? RecalculatedUnit->BaseRL : -1, 5);
	TestEqual(TEXT("Committed Current RL"), RecalculatedUnit != nullptr ? RecalculatedUnit->CurrentRL : -1, 6);
	TestEqual(TEXT("Compatibility RLTotal mirrors current"), RecalculatedUnit != nullptr ? RecalculatedUnit->RLTotal : -1, 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceRecalculationModifierInsertionOrderTest, "Wandbound.Core.ResonanceRecalculation.EquivalentModifierInsertionOrdersMatch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceRecalculationModifierInsertionOrderTest::RunTest(const FString& Parameters)
{
	FWBGameStateData StateA = MakeRLRecalcState(4, 0);
	FWBGameStateData StateB = MakeRLRecalcState(4, 0);
	StateA.GetMutableUnitById(RLRecalcUnitId)->ResonanceModifiers.Add(MakeRLRecalcCurrentModifier(TEXT("alpha"), 1));
	StateA.GetMutableUnitById(RLRecalcUnitId)->ResonanceModifiers.Add(MakeRLRecalcCurrentModifier(TEXT("beta"), 2));
	StateB.GetMutableUnitById(RLRecalcUnitId)->ResonanceModifiers.Add(MakeRLRecalcCurrentModifier(TEXT("beta"), 2));
	StateB.GetMutableUnitById(RLRecalcUnitId)->ResonanceModifiers.Add(MakeRLRecalcCurrentModifier(TEXT("alpha"), 1));
	const FWBCardDefinitionRepository Repository = MakeRLRecalcRepository({});

	const FWBResonanceRecalculationResult ResultA =
		WBResonanceRecalculation::RecalculateUnit(StateA, RLRecalcUnitId, Repository);
	const FWBResonanceRecalculationResult ResultB =
		WBResonanceRecalculation::RecalculateUnit(StateB, RLRecalcUnitId, Repository);

	TestTrue(TEXT("A succeeds"), ResultA.bSucceeded);
	TestTrue(TEXT("B succeeds"), ResultB.bSucceeded);
	TestEqual(TEXT("Current RL matches"), ResultA.RecalculatedCurrentRL, ResultB.RecalculatedCurrentRL);
	TestEqual(TEXT("Modifier count matches"), ResultA.OrderedModifierSourceIds.Num(), ResultB.OrderedModifierSourceIds.Num());
	TestEqual(TEXT("First sorted source matches"), ResultA.OrderedModifierSourceIds[0], ResultB.OrderedModifierSourceIds[0]);
	TestEqual(TEXT("Second sorted source matches"), ResultA.OrderedModifierSourceIds[1], ResultB.OrderedModifierSourceIds[1]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceRecalculationInvalidModifierAtomicTest, "Wandbound.Core.ResonanceRecalculation.InvalidModifierFailsAtomically", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceRecalculationInvalidModifierAtomicTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRLRecalcState(5, 7);
	FWBUnitState* Unit = State.GetMutableUnitById(RLRecalcUnitId);
	if (!TestNotNull(TEXT("Unit exists"), Unit))
	{
		return false;
	}

	Unit->BaseRL = 5;
	Unit->CurrentRL = 11;
	Unit->RLTotal = 11;
	Unit->ResonanceModifiers.Add(MakeRLRecalcCurrentModifier(FString(), 1));
	const FWBResonanceRecalculationResult Result =
		WBResonanceRecalculation::RecalculateUnit(State, RLRecalcUnitId, MakeRLRecalcRepository({}));

	TestFalse(TEXT("Invalid modifier fails"), Result.bSucceeded);
	TestEqual(TEXT("Invalid modifier reason"), Result.FailureReason, FString(TEXT("invalid_modifier_source")));
	TestEqual(TEXT("Base unchanged"), State.GetUnitById(RLRecalcUnitId)->BaseRL, 5);
	TestEqual(TEXT("Current unchanged"), State.GetUnitById(RLRecalcUnitId)->CurrentRL, 11);
	TestEqual(TEXT("RLTotal unchanged"), State.GetUnitById(RLRecalcUnitId)->RLTotal, 11);
	TestEqual(TEXT("RLUsed unchanged"), State.GetUnitById(RLRecalcUnitId)->RLUsed, 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceRecalculationModifierOverflowTest, "Wandbound.Core.ResonanceRecalculation.CurrentModifierCanExposeOverflow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceRecalculationModifierOverflowTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRLRecalcState(3, 0);
	State.GetMutableUnitById(RLRecalcUnitId)->ResonanceModifiers.Add(MakeRLRecalcCurrentModifier(TEXT("temporary_minus_one"), -1));
	AddRLRecalcEquippedCard(State, TEXT("wand_a"), TEXT("wand_a"), 0, RLRecalcUnitId, TEXT("wand"));
	const FWBCardDefinitionRepository Repository = MakeRLRecalcRepository({ MakeRLRecalcWandDefinition(TEXT("wand_a"), 3) });

	const FWBResonanceRecalculationResult Result =
		WBResonanceRecalculation::RecalculateUnit(State, RLRecalcUnitId, Repository);
	const FWBResonanceOverflowPlan Plan =
		WBResonanceOverflow::BuildOverflowPlanForUnit(State, Repository, RLRecalcUnitId);

	TestTrue(TEXT("Recalculation succeeds"), Result.bSucceeded);
	TestEqual(TEXT("Current RL reduced"), Result.RecalculatedCurrentRL, 2);
	TestEqual(TEXT("RL used unchanged from equipped"), Result.RecalculatedRLUsed, 3);
	TestEqual(TEXT("Available RL negative"), Result.AvailableRL, -1);
	TestTrue(TEXT("Overflow exposed"), Result.bIsOverflowing);
	TestEqual(TEXT("Recalculation does not destroy card"), State.GetCardZoneState().EquippedCards.Num(), 1);
	TestTrue(TEXT("Overflow planner succeeds"), Plan.bOk);
	TestTrue(TEXT("Overflow planner sees modifier overflow"), Plan.bHasOverflow);
	TestEqual(TEXT("Overflow amount"), Plan.OverflowAmount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceRecalculationOverflowExposureTest, "Wandbound.Core.ResonanceRecalculation.ExposesOverflowWithoutDestroying", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceRecalculationOverflowExposureTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRLRecalcState(3, 0);
	AddRLRecalcEquippedCard(State, TEXT("wand_a"), TEXT("wand_a"), 0, RLRecalcUnitId, TEXT("wand"));
	AddRLRecalcEquippedCard(State, TEXT("wand_b"), TEXT("wand_b"), 1, RLRecalcUnitId, TEXT("wand_1"));
	const FWBCardDefinitionRepository Repository = MakeRLRecalcRepository({
		MakeRLRecalcWandDefinition(TEXT("wand_a"), 2),
		MakeRLRecalcWandDefinition(TEXT("wand_b"), 2)
	});

	const FWBResonanceRecalculationResult Result =
		WBResonanceRecalculation::RecalculateUnit(State, RLRecalcUnitId, Repository);

	TestTrue(TEXT("Recalculation succeeds"), Result.bSucceeded);
	TestEqual(TEXT("RL used exceeds current"), Result.RecalculatedRLUsed, 4);
	TestEqual(TEXT("Available RL remains negative"), Result.AvailableRL, -1);
	TestTrue(TEXT("Overflow exposed"), Result.bIsOverflowing);
	TestEqual(TEXT("Wands are not destroyed by recalculation"), State.GetCardZoneState().EquippedCards.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceRecalculationFailureAtomicTest, "Wandbound.Core.ResonanceRecalculation.InvalidDataFailsAtomically", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceRecalculationFailureAtomicTest::RunTest(const FString& Parameters)
{
	FWBGameStateData MissingDefState = MakeRLRecalcState(5, 7);
	AddRLRecalcEquippedCard(MissingDefState, TEXT("missing_wand"), TEXT("missing_wand"), 0, RLRecalcUnitId, TEXT("wand"));
	const FWBResonanceRecalculationResult MissingDefResult =
		WBResonanceRecalculation::RecalculateUnit(MissingDefState, RLRecalcUnitId, MakeRLRecalcRepository({}));

	TestFalse(TEXT("Missing definition fails"), MissingDefResult.bSucceeded);
	TestEqual(TEXT("Missing definition reason"), MissingDefResult.FailureReason, FString(TEXT("card_definition_not_found")));
	TestEqual(TEXT("Missing definition did not mutate RLUsed"), MissingDefState.GetUnitById(RLRecalcUnitId)->RLUsed, 7);

	FWBGameStateData DuplicateState = MakeRLRecalcState(5, 7);
	AddRLRecalcEquippedCard(DuplicateState, TEXT("dup_wand"), TEXT("wand_a"), 0, RLRecalcUnitId, TEXT("wand"));
	AddRLRecalcEquippedCard(DuplicateState, TEXT("dup_wand"), TEXT("wand_a"), 1, RLRecalcUnitId, TEXT("wand_1"));
	const FWBResonanceRecalculationResult DuplicateResult =
		WBResonanceRecalculation::RecalculateUnit(
			DuplicateState,
			RLRecalcUnitId,
			MakeRLRecalcRepository({ MakeRLRecalcWandDefinition(TEXT("wand_a"), 1) }));

	TestFalse(TEXT("Duplicate fails"), DuplicateResult.bSucceeded);
	TestEqual(TEXT("Duplicate reason"), DuplicateResult.FailureReason, FString(TEXT("duplicate_card_attachment")));
	TestEqual(TEXT("Duplicate did not mutate RLUsed"), DuplicateState.GetUnitById(RLRecalcUnitId)->RLUsed, 7);

	FWBGameStateData NegativeRRState = MakeRLRecalcState(5, 7);
	AddRLRecalcEquippedCard(NegativeRRState, TEXT("bad_wand"), TEXT("bad_wand"), 0, RLRecalcUnitId, TEXT("wand"));
	const FWBResonanceRecalculationResult NegativeRRResult =
		WBResonanceRecalculation::RecalculateUnit(
			NegativeRRState,
			RLRecalcUnitId,
			MakeRLRecalcRepository({ MakeRLRecalcWandDefinition(TEXT("bad_wand"), -1) }));

	TestFalse(TEXT("Negative RR fails"), NegativeRRResult.bSucceeded);
	TestEqual(TEXT("Negative RR reason"), NegativeRRResult.FailureReason, FString(TEXT("invalid_rr")));
	TestEqual(TEXT("Negative RR did not mutate RLUsed"), NegativeRRState.GetUnitById(RLRecalcUnitId)->RLUsed, 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceRecalculationEquipIntegrationTest, "Wandbound.Core.ResonanceRecalculation.EquipUsesRecalculationNoDoubleCount", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceRecalculationEquipIntegrationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRLRecalcState(5, 99);
	AddRLRecalcEquippedCard(State, TEXT("existing_wand"), TEXT("wand_existing"), 0, RLRecalcUnitId, TEXT("wand"));
	AddRLRecalcHandCard(State, TEXT("hand_wand"), TEXT("wand_hand"));
	const FWBCardDefinitionRepository Repository = MakeRLRecalcRepository({
		MakeRLRecalcWandDefinition(TEXT("wand_existing"), 2),
		MakeRLRecalcWandDefinition(TEXT("wand_hand"), 1)
	});

	const FWBEquipExecutionResult Result =
		WBEquipExecution::ExecuteWandEquipFromHand(State, Repository, MakeRLRecalcEquipRequest());

	TestTrue(TEXT("Equip succeeds"), Result.bOk);
	TestEqual(TEXT("Pre-equip derived RL used"), Result.RLUsedBefore, 2);
	TestEqual(TEXT("Post-equip derived RL used"), Result.RLUsedAfter, 3);
	TestEqual(TEXT("Unit RL used derives equipped cards"), State.GetUnitById(RLRecalcUnitId)->RLUsed, 3);
	TestTrue(TEXT("Hand wand equipped"), UnitHasEquippedInstance(State, TEXT("hand_wand")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceRecalculationFailedEquipAtomicTest, "Wandbound.Core.ResonanceRecalculation.FailedEquipNoHalfEquip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceRecalculationFailedEquipAtomicTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRLRecalcState(5, 7);
	AddRLRecalcEquippedCard(State, TEXT("missing_existing"), TEXT("missing_existing"), 0, RLRecalcUnitId, TEXT("wand"));
	AddRLRecalcHandCard(State, TEXT("hand_wand"), TEXT("wand_hand"));
	const FWBGameStateData Before = State;
	const FWBCardDefinitionRepository Repository = MakeRLRecalcRepository({ MakeRLRecalcWandDefinition(TEXT("wand_hand"), 1) });

	const FWBEquipExecutionResult Result =
		WBEquipExecution::ExecuteWandEquipFromHand(State, Repository, MakeRLRecalcEquipRequest());

	TestFalse(TEXT("Equip fails"), Result.bOk);
	TestEqual(TEXT("Failure code"), Result.Code, EWBEquipExecutionResultCode::RLRecalculationFailed);
	TestEqual(TEXT("Failure reason"), Result.Reason, FString(TEXT("card_definition_not_found")));
	TestEqual(TEXT("Hand count unchanged"), State.GetCardZoneState().PlayerZones[0].Hand.Num(), Before.GetCardZoneState().PlayerZones[0].Hand.Num());
	TestEqual(TEXT("Equipped count unchanged"), State.GetCardZoneState().EquippedCards.Num(), Before.GetCardZoneState().EquippedCards.Num());
	TestEqual(TEXT("RLUsed unchanged"), State.GetUnitById(RLRecalcUnitId)->RLUsed, 7);
	TestFalse(TEXT("No half-equipped hand wand"), UnitHasEquippedInstance(State, TEXT("hand_wand")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceRecalculationOverflowPlannerTest, "Wandbound.Core.ResonanceRecalculation.OverflowPlannerUsesDerivedState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceRecalculationOverflowPlannerTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRLRecalcState(3, 0);
	AddRLRecalcEquippedCard(State, TEXT("wand_a"), TEXT("wand_a"), 0, RLRecalcUnitId, TEXT("wand"));
	AddRLRecalcEquippedCard(State, TEXT("wand_b"), TEXT("wand_b"), 1, RLRecalcUnitId, TEXT("wand_1"));
	const FWBCardDefinitionRepository Repository = MakeRLRecalcRepository({
		MakeRLRecalcWandDefinition(TEXT("wand_a"), 2),
		MakeRLRecalcWandDefinition(TEXT("wand_b"), 2)
	});

	const FWBResonanceOverflowPlan Plan =
		WBResonanceOverflow::BuildOverflowPlanForUnit(State, Repository, RLRecalcUnitId);

	TestTrue(TEXT("Plan succeeds"), Plan.bOk);
	TestTrue(TEXT("Plan detects overflow"), Plan.bHasOverflow);
	TestEqual(TEXT("Plan uses derived RL used"), Plan.RLUsedBefore, 4);
	TestEqual(TEXT("Overflow amount"), Plan.OverflowAmount, 1);
	TestEqual(TEXT("One removal restores legality"), Plan.Removals.Num(), 1);
	TestEqual(TEXT("Planner did not mutate stale state"), State.GetUnitById(RLRecalcUnitId)->RLUsed, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceRecalculationProviderRefreshTest, "Wandbound.Runtime.ResonanceRecalculation.ProviderEligibilityUsesDerivedState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceRecalculationProviderRefreshTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRLRecalcState(3, 0);
	State.GetMutableUnitById(RLRecalcOtherUnitId)->RLTotal = 1;
	AddRLRecalcEquippedCard(State, TEXT("existing_wand"), TEXT("wand_existing"), 0, RLRecalcUnitId, TEXT("wand"));
	AddRLRecalcHandCard(State, TEXT("hand_wand"), TEXT("wand_hand"));
	const FWBCardDefinitionRepository Repository = MakeRLRecalcRepository({
		MakeRLRecalcWandDefinition(TEXT("wand_existing"), 2),
		MakeRLRecalcWandDefinition(TEXT("wand_hand"), 2)
	});

	const FWBProductionSummonEquipDecisionData DecisionData =
		FWBProductionSummonEquipDataProvider().BuildDecisionData(State, Repository, RLRecalcPlayerId);

	TestEqual(TEXT("No equip options because derived available RL is 1"), DecisionData.EquipOptions.Num(), 0);
	TestEqual(TEXT("Diagnostic emitted"), DecisionData.Diagnostics.Num(), 1);
	if (DecisionData.Diagnostics.Num() > 0)
	{
		TestEqual(TEXT("No eligible equip diagnostic"), DecisionData.Diagnostics[0].Code, FString(TEXT("no_eligible_equip_unit")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceRecalculationPublicSummaryTest, "Wandbound.Core.ResonanceRecalculation.PublicSummaryUsesCanonicalRLValues", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceRecalculationPublicSummaryTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRLRecalcState(5, 99);
	AddRLRecalcEquippedCard(State, TEXT("wand_a"), TEXT("wand_a"), 0, RLRecalcUnitId, TEXT("wand"));
	const FWBCardDefinitionRepository Repository = MakeRLRecalcRepository({ MakeRLRecalcWandDefinition(TEXT("wand_a"), 2) });
	WBResonanceRecalculation::RecalculateUnit(State, RLRecalcUnitId, Repository);

	const FWBPublicBoardSummary Summary = WBPublicBoardSummary::Build(State);
	const FWBPublicUnitBoardSummary* UnitSummary = Summary.Units.FindByPredicate([](const FWBPublicUnitBoardSummary& Unit)
	{
		return Unit.UnitId == RLRecalcUnitId;
	});

	TestNotNull(TEXT("Unit summary exists"), UnitSummary);
	if (UnitSummary == nullptr)
	{
		return false;
	}

	TestEqual(TEXT("Public base RL"), UnitSummary->BaseRL, 5);
	TestEqual(TEXT("Public current RL"), UnitSummary->CurrentRL, 5);
	TestEqual(TEXT("Public compatibility RLTotal"), UnitSummary->RLTotal, 5);
	TestEqual(TEXT("Public RL used"), UnitSummary->RLUsed, 2);
	TestEqual(TEXT("Public available RL"), UnitSummary->AvailableRL, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceRecalculationHiddenInfoTest, "Wandbound.Core.ResonanceRecalculation.PublicObservationDoesNotLeakHiddenZones", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceRecalculationHiddenInfoTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeRLRecalcState(5, 0);
	AddRLRecalcDeckCard(State, TEXT("SECRET_RL_DECK_DO_NOT_LEAK"), TEXT("SECRET_RL_DECK_CARD_DO_NOT_LEAK"));
	if (FWBPlayerCardZoneState* OpponentZones =
		WBCardZoneState::FindMutablePlayerZones(State.GetMutableCardZoneStateForTest(), RLRecalcOpponentId))
	{
		OpponentZones->Hand.Add(MakeRLRecalcZoneEntry(
			TEXT("SECRET_RL_OPPONENT_HAND_DO_NOT_LEAK"),
			TEXT("SECRET_RL_OPPONENT_CARD_DO_NOT_LEAK"),
			RLRecalcOpponentId,
			EWBCardZone::Hand,
			0));
	}

	FWBMarkerPlaceholderEntry Marker;
	Marker.MarkerId = 44;
	Marker.OwnerPlayerId = RLRecalcOpponentId;
	Marker.Tile = FWBTile(1, 1);
	Marker.PublicState = EWBMarkerPublicState::Hidden;
	Marker.InternalMarkerCardId = TEXT("SECRET_RL_MARKER_DO_NOT_LEAK");
	State.GetMutableCardZoneStateForTest().MarkerPlaceholders.Add(Marker);

	const FWBCardDefinitionRepository Repository = MakeRLRecalcRepository({});
	const FWBResonanceRecalculationResult Result =
		WBResonanceRecalculation::RecalculateUnit(State, RLRecalcUnitId, Repository);
	const FWBCardZonePublicSummary PublicSummary = WBCardZoneObservation::BuildPublicSummary(State);

	TestTrue(TEXT("Recalculation succeeds"), Result.bSucceeded);
	TestFalse(TEXT("Deck identity hidden publicly"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(PublicSummary, TEXT("SECRET_RL_DECK")));
	TestFalse(TEXT("Opponent hand identity hidden publicly"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(PublicSummary, TEXT("SECRET_RL_OPPONENT")));
	TestFalse(TEXT("Marker identity hidden publicly"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(PublicSummary, TEXT("SECRET_RL_MARKER")));
	return true;
}

#endif
