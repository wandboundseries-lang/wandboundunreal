#include "Misc/AutomationTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBActionCodec.h"
#include "WBCardLifecycle.h"
#include "WBCardZoneObservation.h"
#include "WBRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBPlayerStateData MakeLifecyclePlayer(const int32 PlayerId, const int32 RemainingMP = 2)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = RemainingMP;
	Player.LastMPRoll = RemainingMP;
	return Player;
}

FWBUnitState MakeLifecycleUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FString& CardId,
	const FWBTile& Tile)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = CardId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = 5;
	Unit.MaxHP = 5;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.RLTotal = 3;
	Unit.RLUsed = 0;
	Unit.AttacksLeft = 1;
	Unit.MaxAttacksPerTurn = 1;
	Unit.MPRemaining = OwnerId == 0 ? 2 : 0;
	return Unit;
}

FWBZoneCardEntry MakeLifecycleZoneEntry(
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

FWBPlayerCardZoneState MakeLifecyclePlayerZones(const int32 PlayerId)
{
	FWBPlayerCardZoneState PlayerZones;
	PlayerZones.PlayerId = PlayerId;
	return PlayerZones;
}

FWBGameStateData MakeLifecycleState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeLifecyclePlayer(0));
	State.Players.Add(MakeLifecyclePlayer(1, 0));
	State.AddUnitForTest(MakeLifecycleUnit(1, 0, TEXT("lifecycle_source"), FWBTile(4, 4)));
	State.AddUnitForTest(MakeLifecycleUnit(2, 1, TEXT("lifecycle_enemy"), FWBTile(5, 4)));
	State.CardZoneState.PlayerZones.Add(MakeLifecyclePlayerZones(0));
	State.CardZoneState.PlayerZones.Add(MakeLifecyclePlayerZones(1));
	return State;
}

FWBPlayerCardZoneState* FindLifecycleZones(FWBGameStateData& State, const int32 PlayerId)
{
	return WBCardZoneState::FindMutablePlayerZones(State.GetMutableCardZoneStateForTest(), PlayerId);
}

const FWBPlayerCardZoneState* FindLifecycleZones(const FWBGameStateData& State, const int32 PlayerId)
{
	return WBCardZoneState::FindPlayerZones(State.GetCardZoneState(), PlayerId);
}

void AddLifecycleDeckCard(
	FWBGameStateData& State,
	const int32 PlayerId,
	const FString& InstanceId,
	const FString& CardId,
	const int32 ZoneIndex)
{
	FWBPlayerCardZoneState* PlayerZones = FindLifecycleZones(State, PlayerId);
	if (PlayerZones != nullptr)
	{
		PlayerZones->Deck.Add(MakeLifecycleZoneEntry(InstanceId, CardId, PlayerId, EWBCardZone::Deck, ZoneIndex));
	}
}

void AddLifecycleHandCard(
	FWBGameStateData& State,
	const int32 PlayerId,
	const FString& InstanceId,
	const FString& CardId,
	const int32 ZoneIndex)
{
	FWBPlayerCardZoneState* PlayerZones = FindLifecycleZones(State, PlayerId);
	if (PlayerZones != nullptr)
	{
		PlayerZones->Hand.Add(MakeLifecycleZoneEntry(InstanceId, CardId, PlayerId, EWBCardZone::Hand, ZoneIndex));
	}
}

void AddLifecycleDiscardCard(
	FWBGameStateData& State,
	const int32 PlayerId,
	const FString& InstanceId,
	const FString& CardId,
	const int32 ZoneIndex)
{
	FWBPlayerCardZoneState* PlayerZones = FindLifecycleZones(State, PlayerId);
	if (PlayerZones != nullptr)
	{
		PlayerZones->Discard.Add(MakeLifecycleZoneEntry(InstanceId, CardId, PlayerId, EWBCardZone::Discard, ZoneIndex));
	}
}

void AddLifecycleMarkerSecret(FWBGameStateData& State)
{
	FWBMarkerPlaceholderEntry Marker;
	Marker.MarkerId = 31;
	Marker.OwnerPlayerId = 1;
	Marker.Tile = FWBTile(3, 3);
	Marker.PublicState = EWBMarkerPublicState::Hidden;
	Marker.InternalMarkerCardId = TEXT("SECRET_MARKER_DO_NOT_LEAK");
	State.GetMutableCardZoneStateForTest().MarkerPlaceholders.Add(Marker);
}

const FWBObservedZoneSummary* FindObservedZoneByOwner(
	const TArray<FWBObservedZoneSummary>& Zones,
	const int32 OwnerPlayerId)
{
	for (const FWBObservedZoneSummary& Zone : Zones)
	{
		if (Zone.OwnerPlayerId == OwnerPlayerId)
		{
			return &Zone;
		}
	}
	return nullptr;
}

bool LoadSourceFile(const TArray<FString>& Parts, FString& OutSource)
{
	FString Path = FPaths::ProjectDir();
	for (const FString& Part : Parts)
	{
		Path = FPaths::Combine(Path, Part);
	}

	return FFileHelper::LoadFileToString(OutSource, *Path);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardLifecycleDrawOneMovesTopDeckCardTest, "Wandbound.Core.CardLifecycle.DrawOneMovesTopDeckCardToHand", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardLifecycleDrawOneMovesTopDeckCardTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLifecycleState();
	AddLifecycleDeckCard(State, 0, TEXT("deck_top"), TEXT("drawn_card"), 0);
	AddLifecycleDeckCard(State, 0, TEXT("deck_next"), TEXT("next_card"), 1);

	const FWBCardLifecycleResult Result = WBCardLifecycle::DrawOneCard(State, 0);
	const FWBPlayerCardZoneState* PlayerZones = FindLifecycleZones(State, 0);

	TestTrue(TEXT("Draw succeeds"), Result.bOk);
	TestEqual(TEXT("Result instance"), Result.CardInstanceId, FString(TEXT("deck_top")));
	TestEqual(TEXT("Deck count decreases"), PlayerZones != nullptr ? PlayerZones->Deck.Num() : -1, 1);
	TestEqual(TEXT("Hand count increases"), PlayerZones != nullptr ? PlayerZones->Hand.Num() : -1, 1);
	TestEqual(TEXT("Drawn card in hand"), PlayerZones != nullptr ? PlayerZones->Hand[0].Card.InstanceId : FString(), FString(TEXT("deck_top")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardLifecycleDrawOrderDeterministicTest, "Wandbound.Core.CardLifecycle.DrawOrderDeterministicByZoneIndex", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardLifecycleDrawOrderDeterministicTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLifecycleState();
	AddLifecycleDeckCard(State, 0, TEXT("late"), TEXT("late_card"), 7);
	AddLifecycleDeckCard(State, 0, TEXT("early"), TEXT("early_card"), 1);

	const FWBCardLifecycleResult Result = WBCardLifecycle::DrawOneCard(State, 0);
	TestTrue(TEXT("Draw succeeds"), Result.bOk);
	TestEqual(TEXT("Lowest ZoneIndex draws"), Result.CardInstanceId, FString(TEXT("early")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardLifecycleDrawCardsMultipleInOrderTest, "Wandbound.Core.CardLifecycle.DrawCardsMultipleInOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardLifecycleDrawCardsMultipleInOrderTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLifecycleState();
	AddLifecycleDeckCard(State, 0, TEXT("third"), TEXT("third_card"), 2);
	AddLifecycleDeckCard(State, 0, TEXT("first"), TEXT("first_card"), 0);
	AddLifecycleDeckCard(State, 0, TEXT("second"), TEXT("second_card"), 1);

	const FWBCardLifecycleResult Result = WBCardLifecycle::DrawCards(State, 0, 2);
	const FWBPlayerCardZoneState* PlayerZones = FindLifecycleZones(State, 0);

	TestTrue(TEXT("Draw succeeds"), Result.bOk);
	TestEqual(TEXT("Hand count"), PlayerZones != nullptr ? PlayerZones->Hand.Num() : -1, 2);
	TestEqual(TEXT("First hand"), PlayerZones != nullptr ? PlayerZones->Hand[0].Card.InstanceId : FString(), FString(TEXT("first")));
	TestEqual(TEXT("Second hand"), PlayerZones != nullptr ? PlayerZones->Hand[1].Card.InstanceId : FString(), FString(TEXT("second")));
	TestEqual(TEXT("Remaining deck"), PlayerZones != nullptr ? PlayerZones->Deck[0].Card.InstanceId : FString(), FString(TEXT("third")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardLifecycleDrawCardsPartialEmptyDeckTest, "Wandbound.Core.CardLifecycle.DrawCardsStopsSafelyOnEmptyDeck", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardLifecycleDrawCardsPartialEmptyDeckTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLifecycleState();
	AddLifecycleDeckCard(State, 0, TEXT("only"), TEXT("only_card"), 0);

	const FWBCardLifecycleResult Result = WBCardLifecycle::DrawCards(State, 0, 2);
	const FWBPlayerCardZoneState* PlayerZones = FindLifecycleZones(State, 0);

	TestFalse(TEXT("Second draw fails"), Result.bOk);
	TestEqual(TEXT("Deck empty code"), Result.Code, EWBCardLifecycleResultCode::DeckEmpty);
	TestEqual(TEXT("Previous successful draw remains applied"), PlayerZones != nullptr ? PlayerZones->Hand.Num() : -1, 1);
	TestEqual(TEXT("Deck empty"), PlayerZones != nullptr ? PlayerZones->Deck.Num() : -1, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardLifecycleDrawOneEmptyDeckTest, "Wandbound.Core.CardLifecycle.DrawOneEmptyDeckReturnsDeckEmpty", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardLifecycleDrawOneEmptyDeckTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLifecycleState();
	const FWBCardLifecycleResult Result = WBCardLifecycle::DrawOneCard(State, 0);
	TestFalse(TEXT("Draw fails"), Result.bOk);
	TestEqual(TEXT("Code"), Result.Code, EWBCardLifecycleResultCode::DeckEmpty);
	TestEqual(TEXT("Reason"), Result.Reason, FString(TEXT("deck_empty")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardLifecycleInvalidPlayerTest, "Wandbound.Core.CardLifecycle.InvalidPlayerFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardLifecycleInvalidPlayerTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLifecycleState();
	const FWBCardLifecycleResult Result = WBCardLifecycle::DrawOneCard(State, 7);
	TestFalse(TEXT("Invalid player fails"), Result.bOk);
	TestEqual(TEXT("Code"), Result.Code, EWBCardLifecycleResultCode::InvalidPlayer);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardLifecycleSetupDrawSixTest, "Wandbound.Core.CardLifecycle.SetupDrawSixCards", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardLifecycleSetupDrawSixTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLifecycleState();
	for (int32 Index = 0; Index < 6; ++Index)
	{
		AddLifecycleDeckCard(
			State,
			0,
			FString::Printf(TEXT("setup_%d"), Index),
			FString::Printf(TEXT("setup_card_%d"), Index),
			Index);
	}

	const FWBCardLifecycleResult Result = WBCardLifecycle::ApplySetupDraw(State, 0, 6);
	const FWBPlayerCardZoneState* PlayerZones = FindLifecycleZones(State, 0);

	TestTrue(TEXT("Setup draw succeeds"), Result.bOk);
	TestEqual(TEXT("Six hand cards"), PlayerZones != nullptr ? PlayerZones->Hand.Num() : -1, 6);
	TestEqual(TEXT("Deck empty"), PlayerZones != nullptr ? PlayerZones->Deck.Num() : -1, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardLifecycleFirstPlayerFirstTurnSkippedTest, "Wandbound.Core.CardLifecycle.FirstPlayerFirstTurnDrawSkipped", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardLifecycleFirstPlayerFirstTurnSkippedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLifecycleState();
	AddLifecycleDeckCard(State, 0, TEXT("p0_top"), TEXT("p0_top_card"), 0);

	const FWBCardLifecycleResult Result = WBCardLifecycle::ApplyTurnStartDraw(State, 0, 1, 0);
	const FWBPlayerCardZoneState* PlayerZones = FindLifecycleZones(State, 0);

	TestTrue(TEXT("Skip succeeds"), Result.bOk);
	TestEqual(TEXT("Skip code"), Result.Code, EWBCardLifecycleResultCode::FirstPlayerFirstTurnDrawSkipped);
	TestEqual(TEXT("Skip reason"), Result.Reason, FString(TEXT("first_player_first_turn_draw_skipped")));
	TestEqual(TEXT("Deck unchanged"), PlayerZones != nullptr ? PlayerZones->Deck.Num() : -1, 1);
	TestEqual(TEXT("Hand unchanged"), PlayerZones != nullptr ? PlayerZones->Hand.Num() : -1, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardLifecycleNonFirstPlayerFirstTurnDrawsTest, "Wandbound.Core.CardLifecycle.NonFirstPlayerFirstTurnDraws", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardLifecycleNonFirstPlayerFirstTurnDrawsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLifecycleState();
	AddLifecycleDeckCard(State, 1, TEXT("p1_top"), TEXT("p1_top_card"), 0);

	const FWBCardLifecycleResult Result = WBCardLifecycle::ApplyTurnStartDraw(State, 1, 1, 0);
	const FWBPlayerCardZoneState* PlayerZones = FindLifecycleZones(State, 1);

	TestTrue(TEXT("Draw succeeds"), Result.bOk);
	TestEqual(TEXT("P1 hand count"), PlayerZones != nullptr ? PlayerZones->Hand.Num() : -1, 1);
	TestEqual(TEXT("P1 drawn"), PlayerZones != nullptr ? PlayerZones->Hand[0].Card.InstanceId : FString(), FString(TEXT("p1_top")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardLifecycleLaterFirstPlayerDrawsTest, "Wandbound.Core.CardLifecycle.LaterFirstPlayerTurnDraws", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardLifecycleLaterFirstPlayerDrawsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLifecycleState();
	AddLifecycleDeckCard(State, 0, TEXT("p0_later"), TEXT("p0_later_card"), 0);

	const FWBCardLifecycleResult Result = WBCardLifecycle::ApplyTurnStartDraw(State, 0, 2, 0);
	const FWBPlayerCardZoneState* PlayerZones = FindLifecycleZones(State, 0);

	TestTrue(TEXT("Draw succeeds"), Result.bOk);
	TestEqual(TEXT("P0 hand count"), PlayerZones != nullptr ? PlayerZones->Hand.Num() : -1, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardLifecycleMoveHandToDiscardSucceedsTest, "Wandbound.Core.CardLifecycle.MoveHandCardToDiscardSucceeds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardLifecycleMoveHandToDiscardSucceedsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLifecycleState();
	AddLifecycleHandCard(State, 0, TEXT("hand_0"), TEXT("hand_card"), 0);

	const FWBCardLifecycleResult Result = WBCardLifecycle::MoveHandCardToDiscard(State, 0, TEXT("hand_0"));
	const FWBPlayerCardZoneState* PlayerZones = FindLifecycleZones(State, 0);

	TestTrue(TEXT("Move succeeds"), Result.bOk);
	TestEqual(TEXT("Hand empty"), PlayerZones != nullptr ? PlayerZones->Hand.Num() : -1, 0);
	TestEqual(TEXT("Discard count"), PlayerZones != nullptr ? PlayerZones->Discard.Num() : -1, 1);
	TestEqual(TEXT("Discarded instance"), PlayerZones != nullptr ? PlayerZones->Discard[0].Card.InstanceId : FString(), FString(TEXT("hand_0")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardLifecycleMoveMissingHandCardFailsTest, "Wandbound.Core.CardLifecycle.MoveMissingHandCardFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardLifecycleMoveMissingHandCardFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLifecycleState();
	const FWBCardLifecycleResult Result = WBCardLifecycle::MoveHandCardToDiscard(State, 0, TEXT("missing"));
	TestFalse(TEXT("Move fails"), Result.bOk);
	TestEqual(TEXT("Code"), Result.Code, EWBCardLifecycleResultCode::CardInstanceMissing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardLifecycleMoveDeckCardToDiscardFailsTest, "Wandbound.Core.CardLifecycle.MoveDeckCardToDiscardFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardLifecycleMoveDeckCardToDiscardFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLifecycleState();
	AddLifecycleDeckCard(State, 0, TEXT("deck_card"), TEXT("deck_card_id"), 0);

	const FWBCardLifecycleResult Result = WBCardLifecycle::MoveHandCardToDiscard(State, 0, TEXT("deck_card"));
	TestFalse(TEXT("Move fails"), Result.bOk);
	TestEqual(TEXT("Code"), Result.Code, EWBCardLifecycleResultCode::CardNotInExpectedZone);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardLifecycleDiscardAppendOrderingTest, "Wandbound.Core.CardLifecycle.MoveHandCardPreservesDiscardOrdering", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardLifecycleDiscardAppendOrderingTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLifecycleState();
	AddLifecycleDiscardCard(State, 0, TEXT("discard_0"), TEXT("discard_card"), 0);
	AddLifecycleHandCard(State, 0, TEXT("hand_0"), TEXT("hand_card"), 0);

	const FWBCardLifecycleResult Result = WBCardLifecycle::MoveHandCardToDiscard(State, 0, TEXT("hand_0"));
	const FWBPlayerCardZoneState* PlayerZones = FindLifecycleZones(State, 0);

	TestTrue(TEXT("Move succeeds"), Result.bOk);
	TestEqual(TEXT("Existing discard first"), PlayerZones != nullptr ? PlayerZones->Discard[0].Card.InstanceId : FString(), FString(TEXT("discard_0")));
	TestEqual(TEXT("Moved discard second"), PlayerZones != nullptr ? PlayerZones->Discard[1].Card.InstanceId : FString(), FString(TEXT("hand_0")));
	TestEqual(TEXT("Moved discard appended index"), PlayerZones != nullptr ? PlayerZones->Discard[1].ZoneIndex : -1, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardLifecycleZoneIndexesDeterministicAfterDrawTest, "Wandbound.Core.CardLifecycle.ZoneIndexesDeterministicAfterDrawRemoval", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardLifecycleZoneIndexesDeterministicAfterDrawTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLifecycleState();
	AddLifecycleHandCard(State, 0, TEXT("hand_existing"), TEXT("hand_existing_card"), 3);
	AddLifecycleDeckCard(State, 0, TEXT("drawn"), TEXT("drawn_card"), 2);
	AddLifecycleDeckCard(State, 0, TEXT("remaining_a"), TEXT("remaining_a_card"), 5);
	AddLifecycleDeckCard(State, 0, TEXT("remaining_b"), TEXT("remaining_b_card"), 8);

	const FWBCardLifecycleResult Result = WBCardLifecycle::DrawOneCard(State, 0);
	const FWBPlayerCardZoneState* PlayerZones = FindLifecycleZones(State, 0);

	TestTrue(TEXT("Draw succeeds"), Result.bOk);
	TestEqual(TEXT("First remaining normalized"), PlayerZones != nullptr ? PlayerZones->Deck[0].ZoneIndex : -1, 0);
	TestEqual(TEXT("Second remaining normalized"), PlayerZones != nullptr ? PlayerZones->Deck[1].ZoneIndex : -1, 1);
	TestEqual(TEXT("Existing hand remains first"), PlayerZones != nullptr ? PlayerZones->Hand[0].Card.InstanceId : FString(), FString(TEXT("hand_existing")));
	TestEqual(TEXT("Drawn hand appended"), PlayerZones != nullptr ? PlayerZones->Hand[1].Card.InstanceId : FString(), FString(TEXT("drawn")));
	TestEqual(TEXT("Drawn hand append index"), PlayerZones != nullptr ? PlayerZones->Hand[1].ZoneIndex : -1, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardLifecycleDuplicateInstanceFailsTest, "Wandbound.Core.CardLifecycle.DuplicateInstanceFailsValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardLifecycleDuplicateInstanceFailsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLifecycleState();
	AddLifecycleDeckCard(State, 0, TEXT("dup"), TEXT("deck_card"), 0);
	AddLifecycleHandCard(State, 0, TEXT("dup"), TEXT("hand_card"), 0);

	const FWBCardLifecycleResult Result = WBCardLifecycle::DrawOneCard(State, 0);
	TestFalse(TEXT("Draw fails"), Result.bOk);
	TestEqual(TEXT("Code"), Result.Code, EWBCardLifecycleResultCode::DuplicateInstanceId);
	TestEqual(TEXT("Duplicate id returned"), Result.CardInstanceId, FString(TEXT("dup")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardLifecyclePublicObservationAfterDrawCountsOnlyTest, "Wandbound.Core.CardLifecycle.PublicObservationAfterDrawCountsOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardLifecyclePublicObservationAfterDrawCountsOnlyTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLifecycleState();
	AddLifecycleDeckCard(State, 0, TEXT("secret_draw"), TEXT("SECRET_DRAWN_CARD_DO_NOT_LEAK"), 0);
	AddLifecycleDeckCard(State, 0, TEXT("next"), TEXT("SECRET_NEXT_DECK_CARD_DO_NOT_LEAK"), 1);

	TestTrue(TEXT("Draw succeeds"), WBCardLifecycle::DrawOneCard(State, 0).bOk);
	const FWBCardZonePublicSummary Summary = WBCardZoneObservation::BuildPublicSummary(State);
	const FWBObservedZoneSummary* Deck = FindObservedZoneByOwner(Summary.PlayerDecks, 0);
	const FWBObservedZoneSummary* Hand = FindObservedZoneByOwner(Summary.PlayerHands, 0);

	TestEqual(TEXT("Public deck count"), Deck != nullptr ? Deck->Count : -1, 1);
	TestEqual(TEXT("Public hand count"), Hand != nullptr ? Hand->Count : -1, 1);
	TestFalse(TEXT("Drawn card hidden publicly"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(Summary, TEXT("SECRET_DRAWN_CARD")));
	TestFalse(TEXT("Deck identity hidden publicly"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(Summary, TEXT("SECRET_NEXT_DECK")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardLifecyclePlayerObservationAfterDrawOwnHandVisibleTest, "Wandbound.Core.CardLifecycle.PlayerObservationAfterDrawOwnHandVisible", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardLifecyclePlayerObservationAfterDrawOwnHandVisibleTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLifecycleState();
	AddLifecycleDeckCard(State, 0, TEXT("drawn_instance"), TEXT("drawn_visible_to_owner"), 0);

	TestTrue(TEXT("Draw succeeds"), WBCardLifecycle::DrawOneCard(State, 0).bOk);
	const FWBCardZonePlayerObservation Observation = WBCardZoneObservation::BuildObservationForPlayer(State, 0);

	TestEqual(TEXT("Own hand count"), Observation.OwnHand.Count, 1);
	TestEqual(TEXT("Own hand card visible"), Observation.OwnHand.Cards[0].CardId, FString(TEXT("drawn_visible_to_owner")));
	TestEqual(TEXT("Own hand instance visible"), Observation.OwnHand.Cards[0].InstanceId, FString(TEXT("drawn_instance")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardLifecycleOpponentObservationAfterDrawHidesIdentityTest, "Wandbound.Core.CardLifecycle.OpponentObservationAfterDrawHidesIdentity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardLifecycleOpponentObservationAfterDrawHidesIdentityTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLifecycleState();
	AddLifecycleDeckCard(State, 0, TEXT("drawn_secret_instance"), TEXT("SECRET_DRAWN_CARD_DO_NOT_LEAK"), 0);

	TestTrue(TEXT("Draw succeeds"), WBCardLifecycle::DrawOneCard(State, 0).bOk);
	const FWBCardZonePlayerObservation Observation = WBCardZoneObservation::BuildObservationForPlayer(State, 1);

	TestFalse(TEXT("Opponent cannot see drawn card id"), WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(Observation, TEXT("SECRET_DRAWN_CARD")));
	TestFalse(TEXT("Opponent cannot see drawn instance"), WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(Observation, TEXT("drawn_secret_instance")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardLifecyclePublicObservationAfterDiscardCountsOnlyTest, "Wandbound.Core.CardLifecycle.PublicObservationAfterDiscardCountsOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardLifecyclePublicObservationAfterDiscardCountsOnlyTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLifecycleState();
	AddLifecycleHandCard(State, 0, TEXT("hand_secret_instance"), TEXT("SECRET_DISCARDED_CARD_DO_NOT_LEAK"), 0);

	TestTrue(TEXT("Discard succeeds"), WBCardLifecycle::MoveHandCardToDiscard(State, 0, TEXT("hand_secret_instance")).bOk);
	const FWBCardZonePublicSummary Summary = WBCardZoneObservation::BuildPublicSummary(State);
	const FWBObservedZoneSummary* Discard = FindObservedZoneByOwner(Summary.PlayerDiscards, 0);

	TestEqual(TEXT("Public discard count"), Discard != nullptr ? Discard->Count : -1, 1);
	TestFalse(TEXT("Discard identity hidden publicly"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(Summary, TEXT("SECRET_DISCARDED_CARD")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardLifecycleOwnerObservationAfterDiscardVisibleTest, "Wandbound.Core.CardLifecycle.OwnerObservationAfterDiscardVisible", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardLifecycleOwnerObservationAfterDiscardVisibleTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLifecycleState();
	AddLifecycleHandCard(State, 0, TEXT("hand_instance"), TEXT("discard_visible_to_owner"), 0);

	TestTrue(TEXT("Discard succeeds"), WBCardLifecycle::MoveHandCardToDiscard(State, 0, TEXT("hand_instance")).bOk);
	const FWBCardZonePlayerObservation Observation = WBCardZoneObservation::BuildObservationForPlayer(State, 0);

	TestEqual(TEXT("Own discard count"), Observation.OwnDiscard.Count, 1);
	TestEqual(TEXT("Own discard card visible"), Observation.OwnDiscard.Cards[0].CardId, FString(TEXT("discard_visible_to_owner")));
	TestEqual(TEXT("Own discard instance visible"), Observation.OwnDiscard.Cards[0].InstanceId, FString(TEXT("hand_instance")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardLifecycleSecretsRemainHiddenAfterLifecycleTest, "Wandbound.Core.CardLifecycle.OpponentDeckHandMarkerSecretsRemainHidden", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardLifecycleSecretsRemainHiddenAfterLifecycleTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLifecycleState();
	AddLifecycleHandCard(State, 0, TEXT("own_hand"), TEXT("own_hand_card"), 0);
	AddLifecycleDeckCard(State, 1, TEXT("SECRET_OPPONENT_DECK_INSTANCE_DO_NOT_LEAK"), TEXT("SECRET_OPPONENT_DECK_CARD_DO_NOT_LEAK"), 0);
	AddLifecycleHandCard(State, 1, TEXT("SECRET_OPPONENT_HAND_INSTANCE_DO_NOT_LEAK"), TEXT("SECRET_OPPONENT_HAND_CARD_DO_NOT_LEAK"), 0);
	AddLifecycleMarkerSecret(State);

	TestTrue(TEXT("Discard succeeds"), WBCardLifecycle::MoveHandCardToDiscard(State, 0, TEXT("own_hand")).bOk);
	const FWBCardZonePublicSummary PublicSummary = WBCardZoneObservation::BuildPublicSummary(State);
	const FWBCardZonePlayerObservation OwnerObservation = WBCardZoneObservation::BuildObservationForPlayer(State, 0);

	TestFalse(TEXT("Opponent deck hidden publicly"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(PublicSummary, TEXT("SECRET_OPPONENT_DECK")));
	TestFalse(TEXT("Opponent hand hidden publicly"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(PublicSummary, TEXT("SECRET_OPPONENT_HAND")));
	TestFalse(TEXT("Marker hidden publicly"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(PublicSummary, TEXT("SECRET_MARKER")));
	TestFalse(TEXT("Opponent deck hidden from owner"), WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(OwnerObservation, TEXT("SECRET_OPPONENT_DECK")));
	TestFalse(TEXT("Opponent hand hidden from owner"), WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(OwnerObservation, TEXT("SECRET_OPPONENT_HAND")));
	TestFalse(TEXT("Marker hidden from owner"), WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(OwnerObservation, TEXT("SECRET_MARKER")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardLifecycleActionCodecSourceGuardTest, "Wandbound.Core.CardLifecycle.ActionCodecHasNoLifecycleDependency", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardLifecycleActionCodecSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Action codec source loads"), LoadSourceFile({ TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBActionCodec.cpp") }, Source));
	TestFalse(TEXT("No lifecycle dependency"), Source.Contains(TEXT("WBCardLifecycle")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardLifecycleRulesGenerationUnchangedTest, "Wandbound.Core.CardLifecycle.RulesGenerateLegalActionsUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardLifecycleRulesGenerationUnchangedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeLifecycleState();
	AddLifecycleDeckCard(State, 0, TEXT("drawn"), TEXT("drawn_card"), 0);

	const TArray<FString> BeforeActionIds = WBActionCodec::MakeActionIds(
		WBRules::GenerateLegalActionsForPlayer(State, 0));
	TestTrue(TEXT("Draw succeeds"), WBCardLifecycle::DrawOneCard(State, 0).bOk);
	const TArray<FString> AfterActionIds = WBActionCodec::MakeActionIds(
		WBRules::GenerateLegalActionsForPlayer(State, 0));

	TestEqual(TEXT("Legal action count unchanged"), AfterActionIds.Num(), BeforeActionIds.Num());
	const int32 CompareCount = FMath::Min(BeforeActionIds.Num(), AfterActionIds.Num());
	for (int32 Index = 0; Index < CompareCount; ++Index)
	{
		TestEqual(*FString::Printf(TEXT("Action id %d unchanged"), Index), AfterActionIds[Index], BeforeActionIds[Index]);
		TestFalse(*FString::Printf(TEXT("No draw action id %d"), Index), AfterActionIds[Index].Contains(TEXT("draw")));
		TestFalse(*FString::Printf(TEXT("No discard action id %d"), Index), AfterActionIds[Index].Contains(TEXT("discard")));
	}

	FString RulesSource;
	TestTrue(TEXT("Rules source loads"), LoadSourceFile({ TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBRules.cpp") }, RulesSource));
	TestFalse(TEXT("Rules source has no lifecycle dependency"), RulesSource.Contains(TEXT("WBCardLifecycle")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardLifecycleSourceGuardNoActionOrRulesTest, "Wandbound.Core.CardLifecycle.NoActionCodecFWBActionOrRulesDependency", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardLifecycleSourceGuardNoActionOrRulesTest::RunTest(const FString& Parameters)
{
	FString Source;
	FString Header;
	TestTrue(TEXT("Lifecycle source loads"), LoadSourceFile({ TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBCardLifecycle.cpp") }, Source));
	TestTrue(TEXT("Lifecycle header loads"), LoadSourceFile({ TEXT("Source"), TEXT("WandboundCore"), TEXT("Public"), TEXT("WBCardLifecycle.h") }, Header));
	TestFalse(TEXT("No action codec source"), Source.Contains(TEXT("WBActionCodec")));
	TestFalse(TEXT("No action codec header"), Header.Contains(TEXT("WBActionCodec")));
	TestFalse(TEXT("No FWBAction source"), Source.Contains(TEXT("FWBAction")));
	TestFalse(TEXT("No FWBAction header"), Header.Contains(TEXT("FWBAction")));
	TestFalse(TEXT("No WBRules source"), Source.Contains(TEXT("WBRules")));
	TestFalse(TEXT("No WBRules header"), Header.Contains(TEXT("WBRules")));
	return true;
}

#endif
