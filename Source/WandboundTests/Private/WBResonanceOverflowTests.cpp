#include "Misc/AutomationTest.h"

#include "WBCardDefinitionRepository.h"
#include "WBCardZoneObservation.h"
#include "WBCardZoneState.h"
#include "WBGameStateData.h"
#include "WBResonanceOverflow.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
constexpr int32 OverflowPlayerId = 0;
constexpr int32 OverflowOpponentId = 1;
constexpr int32 OverflowUnitId = 10;
constexpr int32 OverflowOpponentUnitId = 20;

FWBGameStateData MakeOverflowState(const int32 RLTotal = 3, const int32 RLUsed = 0)
{
	FWBGameStateData State;
	State.CurrentPlayer = OverflowPlayerId;
	State.PriorityPlayer = OverflowPlayerId;

	FWBPlayerStateData Player;
	Player.PlayerId = OverflowPlayerId;
	Player.HeroUnitId = OverflowUnitId;
	State.Players.Add(Player);

	FWBPlayerStateData Opponent;
	Opponent.PlayerId = OverflowOpponentId;
	Opponent.HeroUnitId = OverflowOpponentUnitId;
	State.Players.Add(Opponent);

	FWBUnitState Unit;
	Unit.UnitId = OverflowUnitId;
	Unit.OwnerId = OverflowPlayerId;
	Unit.CardId = TEXT("hero_card");
	Unit.X = 4;
	Unit.Y = 4;
	Unit.RLTotal = RLTotal;
	Unit.RLUsed = RLUsed;
	State.Units.Add(Unit);

	FWBUnitState OpponentUnit = Unit;
	OpponentUnit.UnitId = OverflowOpponentUnitId;
	OpponentUnit.OwnerId = OverflowOpponentId;
	OpponentUnit.X = 8;
	OpponentUnit.Y = 8;
	OpponentUnit.RLUsed = 0;
	State.Units.Add(OpponentUnit);

	FWBPlayerCardZoneState Zones;
	Zones.PlayerId = OverflowPlayerId;
	State.CardZoneState.PlayerZones.Add(Zones);

	FWBPlayerCardZoneState OpponentZones;
	OpponentZones.PlayerId = OverflowOpponentId;
	State.CardZoneState.PlayerZones.Add(OpponentZones);
	return State;
}

FWBCardDefinition MakeOverflowWandDefinition(const FString& CardId, const int32 RR)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = CardId;
	Definition.Kind = EWBCardDefinitionKind::Wand;
	Definition.WandStats.RR = RR;
	return Definition;
}

FWBCardDefinition MakeOverflowCharacterDefinition(const FString& CardId)
{
	FWBCardDefinition Definition;
	Definition.CardId = CardId;
	Definition.PublicName = TEXT("Not A Wand");
	Definition.Kind = EWBCardDefinitionKind::Character;
	Definition.CharacterStats.HP = 1;
	return Definition;
}

FWBCardDefinitionRepository MakeOverflowRepository(const TArray<FWBCardDefinition>& Definitions)
{
	FWBCardDefinitionRepository Repository;
	Repository.RepositoryId = TEXT("overflow_repository");
	Repository.SourceVersion = TEXT("test_v1");
	Repository.Definitions = Definitions;
	return Repository;
}

void AddEquippedWand(
	FWBGameStateData& State,
	const FString& InstanceId,
	const FString& CardId,
	const FString& SlotId,
	const int32 EquipOrder,
	const int32 UnitId = OverflowUnitId)
{
	FWBEquippedCardEntry Entry;
	Entry.Card.InstanceId = InstanceId;
	Entry.Card.CardId = CardId;
	Entry.Card.OwnerPlayerId = OverflowPlayerId;
	Entry.EquippedToUnitId = UnitId;
	Entry.SlotId = SlotId;
	Entry.EquipOrder = EquipOrder;
	State.GetMutableCardZoneStateForTest().EquippedCards.Add(Entry);
}

void AddPrivateSecrets(FWBGameStateData& State)
{
	FWBPlayerCardZoneState* PlayerZones =
		WBCardZoneState::FindMutablePlayerZones(State.GetMutableCardZoneStateForTest(), OverflowPlayerId);
	FWBPlayerCardZoneState* OpponentZones =
		WBCardZoneState::FindMutablePlayerZones(State.GetMutableCardZoneStateForTest(), OverflowOpponentId);
	if (PlayerZones != nullptr)
	{
		FWBZoneCardEntry DeckCard;
		DeckCard.Card.InstanceId = TEXT("SECRET_OVERFLOW_DECK_DO_NOT_LEAK");
		DeckCard.Card.CardId = TEXT("SECRET_OVERFLOW_DECK_CARD_DO_NOT_LEAK");
		DeckCard.Card.OwnerPlayerId = OverflowPlayerId;
		DeckCard.Zone = EWBCardZone::Deck;
		DeckCard.ZoneIndex = 0;
		PlayerZones->Deck.Add(DeckCard);
	}
	if (OpponentZones != nullptr)
	{
		FWBZoneCardEntry HandCard;
		HandCard.Card.InstanceId = TEXT("SECRET_OVERFLOW_OPPONENT_HAND_DO_NOT_LEAK");
		HandCard.Card.CardId = TEXT("SECRET_OVERFLOW_OPPONENT_CARD_DO_NOT_LEAK");
		HandCard.Card.OwnerPlayerId = OverflowOpponentId;
		HandCard.Zone = EWBCardZone::Hand;
		HandCard.ZoneIndex = 0;
		OpponentZones->Hand.Add(HandCard);
	}

	FWBMarkerPlaceholderEntry Marker;
	Marker.MarkerId = 81;
	Marker.OwnerPlayerId = OverflowOpponentId;
	Marker.Tile = FWBTile(2, 2);
	Marker.PublicState = EWBMarkerPublicState::Hidden;
	Marker.InternalMarkerCardId = TEXT("SECRET_OVERFLOW_MARKER_DO_NOT_LEAK");
	State.GetMutableCardZoneStateForTest().MarkerPlaceholders.Add(Marker);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceOverflowNoOverflowTest, "Wandbound.Core.ResonanceOverflow.NoOverflowAndExactRLNoop", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceOverflowNoOverflowTest::RunTest(const FString& Parameters)
{
	FWBGameStateData NoOverflowState = MakeOverflowState(3, 2);
	AddEquippedWand(NoOverflowState, TEXT("wand_a"), TEXT("wand_a"), TEXT("wand"), 0);
	const FWBCardDefinitionRepository Repository = MakeOverflowRepository({ MakeOverflowWandDefinition(TEXT("wand_a"), 1) });

	const FWBResonanceOverflowResult NoOverflowResult =
		WBResonanceOverflow::ResolveOverflowForUnit(NoOverflowState, Repository, OverflowUnitId);

	FWBGameStateData ExactState = MakeOverflowState(3, 3);
	AddEquippedWand(ExactState, TEXT("wand_a"), TEXT("wand_a"), TEXT("wand"), 0);
	const FWBResonanceOverflowResult ExactResult =
		WBResonanceOverflow::ResolveOverflowForUnit(ExactState, Repository, OverflowUnitId);

	TestTrue(TEXT("No overflow succeeds"), NoOverflowResult.bOk);
	TestFalse(TEXT("No overflow unresolved"), NoOverflowResult.bResolvedOverflow);
	TestEqual(TEXT("No overflow no trace"), NoOverflowResult.TraceEvents.Num(), 0);
	TestEqual(TEXT("No overflow RL unchanged"), NoOverflowState.GetUnitById(OverflowUnitId)->RLUsed, 2);
	TestTrue(TEXT("Exact RL succeeds"), ExactResult.bOk);
	TestFalse(TEXT("Exact RL no resolution"), ExactResult.bResolvedOverflow);
	TestEqual(TEXT("Exact RL unchanged"), ExactState.GetUnitById(OverflowUnitId)->RLUsed, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceOverflowExecutionTest, "Wandbound.Core.ResonanceOverflow.RemovesWandsToResolveOverflow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceOverflowExecutionTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeOverflowState(3, 7);
	AddEquippedWand(State, TEXT("wand_a"), TEXT("wand_a"), TEXT("wand"), 0);
	AddEquippedWand(State, TEXT("wand_b"), TEXT("wand_b"), TEXT("wand_1"), 1);
	const FWBCardDefinitionRepository Repository = MakeOverflowRepository({
		MakeOverflowWandDefinition(TEXT("wand_a"), 2),
		MakeOverflowWandDefinition(TEXT("wand_b"), 2)
	});

	const FWBResonanceOverflowResult Result =
		WBResonanceOverflow::ResolveOverflowForUnit(State, Repository, OverflowUnitId);
	const FWBPlayerCardZoneState* Zones =
		WBCardZoneState::FindPlayerZones(State.GetCardZoneState(), OverflowPlayerId);

	TestTrue(TEXT("Overflow succeeds"), Result.bOk);
	TestTrue(TEXT("Overflow resolved"), Result.bResolvedOverflow);
	TestEqual(TEXT("RL reduced exactly"), State.GetUnitById(OverflowUnitId)->RLUsed, 3);
	TestEqual(TEXT("Equipped removed"), State.GetCardZoneState().EquippedCards.Num(), 0);
	TestEqual(TEXT("Discard count"), Zones != nullptr ? Zones->Discard.Num() : -1, 2);
	TestEqual(TEXT("First discard"), Zones != nullptr ? Zones->Discard[0].Card.InstanceId : FString(), FString(TEXT("wand_a")));
	TestEqual(TEXT("Second discard"), Zones != nullptr ? Zones->Discard[1].Card.InstanceId : FString(), FString(TEXT("wand_b")));
	TestEqual(TEXT("Trace count"), Result.TraceEvents.Num(), 4);
	TestEqual(TEXT("Begin trace"), Result.TraceEvents[0].EventType, FString(TEXT("rl_overflow_begin")));
	TestEqual(TEXT("Remove trace"), Result.TraceEvents[1].EventType, FString(TEXT("rl_overflow_remove_wand")));
	TestEqual(TEXT("End trace"), Result.TraceEvents.Last().EventType, FString(TEXT("rl_overflow_end")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceOverflowOrderingTest, "Wandbound.Core.ResonanceOverflow.DeterministicOrderingUsesSlotEquipOrderInstance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceOverflowOrderingTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeOverflowState(3, 6);
	AddEquippedWand(State, TEXT("wand_late"), TEXT("wand_late"), TEXT("wand"), 2);
	AddEquippedWand(State, TEXT("wand_slot_two"), TEXT("wand_slot_two"), TEXT("wand_2"), 0);
	AddEquippedWand(State, TEXT("wand_early_b"), TEXT("wand_early_b"), TEXT("wand"), 1);
	AddEquippedWand(State, TEXT("wand_early_a"), TEXT("wand_early_a"), TEXT("wand"), 1);
	const FWBCardDefinitionRepository Repository = MakeOverflowRepository({
		MakeOverflowWandDefinition(TEXT("wand_late"), 1),
		MakeOverflowWandDefinition(TEXT("wand_slot_two"), 1),
		MakeOverflowWandDefinition(TEXT("wand_early_b"), 1),
		MakeOverflowWandDefinition(TEXT("wand_early_a"), 1)
	});

	const FWBResonanceOverflowPlan Plan =
		WBResonanceOverflow::BuildOverflowPlanForUnit(State, Repository, OverflowUnitId);

	TestTrue(TEXT("Plan succeeds"), Plan.bOk);
	TestEqual(TEXT("Three removals needed"), Plan.Removals.Num(), 3);
	TestEqual(TEXT("Equip order before instance"), Plan.Removals[0].SourceInstanceId, FString(TEXT("wand_early_a")));
	TestEqual(TEXT("Tie broken by instance"), Plan.Removals[1].SourceInstanceId, FString(TEXT("wand_early_b")));
	TestEqual(TEXT("Later equip third"), Plan.Removals[2].SourceInstanceId, FString(TEXT("wand_late")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceOverflowDeterminismTest, "Wandbound.Core.ResonanceOverflow.RepeatedExecutionDeterministic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceOverflowDeterminismTest::RunTest(const FString& Parameters)
{
	FWBGameStateData StateA = MakeOverflowState(3, 5);
	AddEquippedWand(StateA, TEXT("wand_a"), TEXT("wand_a"), TEXT("wand"), 0);
	AddEquippedWand(StateA, TEXT("wand_b"), TEXT("wand_b"), TEXT("wand_1"), 1);
	FWBGameStateData StateB = StateA;
	const FWBCardDefinitionRepository Repository = MakeOverflowRepository({
		MakeOverflowWandDefinition(TEXT("wand_a"), 1),
		MakeOverflowWandDefinition(TEXT("wand_b"), 1)
	});

	const FWBResonanceOverflowResult ResultA =
		WBResonanceOverflow::ResolveOverflowForUnit(StateA, Repository, OverflowUnitId);
	const FWBResonanceOverflowResult ResultB =
		WBResonanceOverflow::ResolveOverflowForUnit(StateB, Repository, OverflowUnitId);
	FString JsonA;
	FString JsonB;
	WBResonanceOverflow::OverflowResultToJsonStringForTest(ResultA, JsonA);
	WBResonanceOverflow::OverflowResultToJsonStringForTest(ResultB, JsonB);

	TestTrue(TEXT("A succeeds"), ResultA.bOk);
	TestTrue(TEXT("B succeeds"), ResultB.bOk);
	TestEqual(TEXT("Trace JSON deterministic"), JsonA, JsonB);
	TestEqual(TEXT("A final RL"), StateA.GetUnitById(OverflowUnitId)->RLUsed, 3);
	TestEqual(TEXT("B final RL"), StateB.GetUnitById(OverflowUnitId)->RLUsed, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceOverflowHiddenInfoTest, "Wandbound.Core.ResonanceOverflow.TraceDoesNotLeakHiddenInfo", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceOverflowHiddenInfoTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeOverflowState(3, 4);
	AddEquippedWand(State, TEXT("wand_a"), TEXT("wand_a"), TEXT("wand"), 0);
	AddPrivateSecrets(State);
	const FWBCardDefinitionRepository Repository = MakeOverflowRepository({ MakeOverflowWandDefinition(TEXT("wand_a"), 1) });

	const FWBResonanceOverflowResult Result =
		WBResonanceOverflow::ResolveOverflowForUnit(State, Repository, OverflowUnitId);
	FString Json;
	WBResonanceOverflow::OverflowResultToJsonStringForTest(Result, Json);
	const FWBCardZonePublicSummary PublicSummary = WBCardZoneObservation::BuildPublicSummary(State);

	TestTrue(TEXT("Overflow succeeds"), Result.bOk);
	TestFalse(TEXT("Opponent hand hidden from trace"), Json.Contains(TEXT("SECRET_OVERFLOW_OPPONENT_HAND")));
	TestFalse(TEXT("Deck hidden from trace"), Json.Contains(TEXT("SECRET_OVERFLOW_DECK")));
	TestFalse(TEXT("Marker hidden from trace"), Json.Contains(TEXT("SECRET_OVERFLOW_MARKER")));
	TestFalse(TEXT("Opponent hand hidden publicly"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(PublicSummary, TEXT("SECRET_OVERFLOW_OPPONENT_HAND")));
	TestFalse(TEXT("Deck hidden publicly"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(PublicSummary, TEXT("SECRET_OVERFLOW_DECK")));
	TestFalse(TEXT("Marker hidden publicly"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(PublicSummary, TEXT("SECRET_OVERFLOW_MARKER")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBResonanceOverflowInvalidStateTest, "Wandbound.Core.ResonanceOverflow.InvalidStatesFailClearly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBResonanceOverflowInvalidStateTest::RunTest(const FString& Parameters)
{
	auto ExpectFailure = [this](
		const TCHAR* Label,
		FWBGameStateData State,
		const FWBCardDefinitionRepository& Repository,
		const EWBResonanceOverflowResultCode ExpectedCode)
	{
		const FWBResonanceOverflowResult Result =
			WBResonanceOverflow::ResolveOverflowForUnit(State, Repository, OverflowUnitId);
		TestFalse(*FString::Printf(TEXT("%s fails"), Label), Result.bOk);
		TestEqual(*FString::Printf(TEXT("%s code"), Label), Result.Code, ExpectedCode);
	};

	FWBGameStateData MissingDefState = MakeOverflowState(3, 4);
	AddEquippedWand(MissingDefState, TEXT("wand_a"), TEXT("wand_a"), TEXT("wand"), 0);
	ExpectFailure(
		TEXT("missing definition"),
		MissingDefState,
		MakeOverflowRepository({}),
		EWBResonanceOverflowResultCode::CardDefinitionNotFound);

	FWBGameStateData NonWandState = MakeOverflowState(3, 4);
	AddEquippedWand(NonWandState, TEXT("not_wand"), TEXT("not_wand"), TEXT("wand"), 0);
	ExpectFailure(
		TEXT("non-wand"),
		NonWandState,
		MakeOverflowRepository({ MakeOverflowCharacterDefinition(TEXT("not_wand")) }),
		EWBResonanceOverflowResultCode::EquippedCardNotWand);

	FWBGameStateData InvalidRRState = MakeOverflowState(3, 4);
	AddEquippedWand(InvalidRRState, TEXT("wand_zero"), TEXT("wand_zero"), TEXT("wand"), 0);
	ExpectFailure(
		TEXT("invalid rr"),
		InvalidRRState,
		MakeOverflowRepository({ MakeOverflowWandDefinition(TEXT("wand_zero"), 0) }),
		EWBResonanceOverflowResultCode::InvalidRR);

	FWBGameStateData InsufficientState = MakeOverflowState(3, 7);
	AddEquippedWand(InsufficientState, TEXT("wand_a"), TEXT("wand_a"), TEXT("wand"), 0);
	ExpectFailure(
		TEXT("insufficient candidates"),
		InsufficientState,
		MakeOverflowRepository({ MakeOverflowWandDefinition(TEXT("wand_a"), 1) }),
		EWBResonanceOverflowResultCode::InsufficientOverflowCandidates);

	FWBGameStateData InvalidRLState = MakeOverflowState(-1, 4);
	ExpectFailure(
		TEXT("invalid rl"),
		InvalidRLState,
		MakeOverflowRepository({}),
		EWBResonanceOverflowResultCode::InvalidRLState);

	const FWBResonanceOverflowResult MissingUnitResult =
		WBResonanceOverflow::ResolveOverflowForUnit(
			InvalidRLState,
			MakeOverflowRepository({}),
			999);
	TestFalse(TEXT("Missing unit fails"), MissingUnitResult.bOk);
	TestEqual(TEXT("Missing unit code"), MissingUnitResult.Code, EWBResonanceOverflowResultCode::TargetUnitNotFound);
	return true;
}

#endif
