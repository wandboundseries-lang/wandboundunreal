#include "Misc/AutomationTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBActionCodec.h"
#include "WBCardZoneObservation.h"
#include "WBGameStateData.h"
#include "WBPublicBoardSummary.h"
#include "WBRules.h"
#include "WBRuntimeResultSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBZoneCardEntry MakeObservationZoneEntry(
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

FWBEquippedCardEntry MakeObservationEquipped(
	const FString& InstanceId,
	const FString& CardId,
	const int32 OwnerPlayerId,
	const int32 EquippedToUnitId,
	const FString& SlotId = TEXT("wand"))
{
	FWBEquippedCardEntry Entry;
	Entry.Card.InstanceId = InstanceId;
	Entry.Card.CardId = CardId;
	Entry.Card.OwnerPlayerId = OwnerPlayerId;
	Entry.EquippedToUnitId = EquippedToUnitId;
	Entry.SlotId = SlotId;
	return Entry;
}

FWBMarkerPlaceholderEntry MakeObservationMarker(
	const int32 MarkerId,
	const int32 OwnerPlayerId,
	const FWBTile& Tile,
	const FString& InternalMarkerCardId,
	const EWBMarkerPublicState PublicState = EWBMarkerPublicState::Hidden)
{
	FWBMarkerPlaceholderEntry Marker;
	Marker.MarkerId = MarkerId;
	Marker.OwnerPlayerId = OwnerPlayerId;
	Marker.Tile = Tile;
	Marker.PublicState = PublicState;
	Marker.InternalMarkerCardId = InternalMarkerCardId;
	return Marker;
}

FWBPlayerCardZoneState MakeObservationPlayerZones(const int32 PlayerId)
{
	FWBPlayerCardZoneState PlayerZones;
	PlayerZones.PlayerId = PlayerId;
	return PlayerZones;
}

FWBPlayerStateData MakeObservationPlayer(const int32 PlayerId)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = PlayerId == 0 ? 2 : 0;
	Player.LastMPRoll = Player.RemainingMP;
	return Player;
}

FWBUnitState MakeObservationUnit(
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
	return Unit;
}

FWBGameStateData MakeObservationState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeObservationPlayer(0));
	State.Players.Add(MakeObservationPlayer(1));
	State.AddUnitForTest(MakeObservationUnit(1, 0, TEXT("visible_board_player_card"), FWBTile(4, 4)));
	State.AddUnitForTest(MakeObservationUnit(2, 1, TEXT("visible_board_opponent_card"), FWBTile(5, 4)));

	FWBPlayerCardZoneState P0Zones = MakeObservationPlayerZones(0);
	P0Zones.Deck.Add(MakeObservationZoneEntry(TEXT("p0_deck_1"), TEXT("SECRET_P0_DECK_CARD_DO_NOT_LEAK"), 0, EWBCardZone::Deck, 1));
	P0Zones.Deck.Add(MakeObservationZoneEntry(TEXT("p0_deck_0"), TEXT("SECRET_P0_DECK_CARD_0_DO_NOT_LEAK"), 0, EWBCardZone::Deck, 0));
	P0Zones.Hand.Add(MakeObservationZoneEntry(TEXT("p0_hand_1"), TEXT("visible_own_hand_1"), 0, EWBCardZone::Hand, 1));
	P0Zones.Hand.Add(MakeObservationZoneEntry(TEXT("p0_hand_0"), TEXT("visible_own_hand_0"), 0, EWBCardZone::Hand, 0));
	P0Zones.Discard.Add(MakeObservationZoneEntry(TEXT("p0_discard_0"), TEXT("visible_own_discard_0"), 0, EWBCardZone::Discard, 0));

	FWBPlayerCardZoneState P1Zones = MakeObservationPlayerZones(1);
	P1Zones.Deck.Add(MakeObservationZoneEntry(TEXT("p1_deck_0"), TEXT("SECRET_P1_DECK_CARD_DO_NOT_LEAK"), 1, EWBCardZone::Deck, 0));
	P1Zones.Hand.Add(MakeObservationZoneEntry(TEXT("p1_hand_0"), TEXT("SECRET_P1_HAND_CARD_DO_NOT_LEAK"), 1, EWBCardZone::Hand, 0));
	P1Zones.Discard.Add(MakeObservationZoneEntry(TEXT("p1_discard_0"), TEXT("SECRET_P1_DISCARD_CARD_DO_NOT_LEAK"), 1, EWBCardZone::Discard, 0));

	State.CardZoneState.PlayerZones.Add(P1Zones);
	State.CardZoneState.PlayerZones.Add(P0Zones);
	State.CardZoneState.EquippedCards.Add(MakeObservationEquipped(TEXT("p0_eq_0"), TEXT("SECRET_EQUIPPED_CARD_DO_NOT_LEAK"), 0, 1));
	State.CardZoneState.MarkerPlaceholders.Add(MakeObservationMarker(8, 1, FWBTile(7, 7), TEXT("SECRET_MARKER_DO_NOT_LEAK")));
	State.CardZoneState.MarkerPlaceholders.Add(MakeObservationMarker(3, 0, FWBTile(2, 2), TEXT("SECRET_MARKER_2_DO_NOT_LEAK"), EWBMarkerPublicState::Revealed));
	return State;
}

const FWBObservedZoneSummary* FindObservedZoneByOwner(const TArray<FWBObservedZoneSummary>& Zones, const int32 OwnerPlayerId)
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

FString MakeRuntimeSourceText()
{
	const FString RuntimePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime"));
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *RuntimePath, TEXT("*.h"), true, false);
	IFileManager::Get().FindFilesRecursive(Files, *RuntimePath, TEXT("*.cpp"), true, false);

	Files.Sort();
	FString Combined;
	for (const FString& File : Files)
	{
		FString FileText;
		if (FFileHelper::LoadFileToString(FileText, *File))
		{
			Combined += FileText;
			Combined += TEXT("\n");
		}
	}
	return Combined;
}

FString SerializePublicBoardSummary(const FWBPublicBoardSummary& Summary)
{
	FWBRuntimeSelectedActionResult Result;
	Result.FinalPublicBoardSummary = Summary;
	FString Json;
	WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Result, Json);
	return Json;
}

FWBAction MakeObservationCodecAction(const EWBActionType Type, const int32 PlayerId = 0)
{
	FWBAction Action;
	Action.Type = Type;
	Action.PlayerId = PlayerId;
	Action.SourceUnitId = 1;
	Action.TargetUnitId = 2;
	Action.FromTile = FWBTile(4, 4);
	Action.ToTile = FWBTile(3, 4);
	return Action;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneObservationPublicDeckCountsOnlyTest, "Wandbound.Core.CardZoneObservation.PublicDeckCountsOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneObservationPublicDeckCountsOnlyTest::RunTest(const FString& Parameters)
{
	const FWBCardZonePublicSummary Summary = WBCardZoneObservation::BuildPublicSummary(MakeObservationState());
	const FWBObservedZoneSummary* P0Deck = FindObservedZoneByOwner(Summary.PlayerDecks, 0);
	TestNotNull(TEXT("P0 deck summary"), P0Deck);
	TestEqual(TEXT("P0 deck count"), P0Deck != nullptr ? P0Deck->Count : -1, 2);
	TestEqual(TEXT("P0 deck cards hidden"), P0Deck != nullptr ? P0Deck->Cards.Num() : -1, 0);
	TestFalse(TEXT("P0 deck secret absent"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(Summary, TEXT("SECRET_P0_DECK_CARD_DO_NOT_LEAK")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneObservationPublicHandCountsOnlyTest, "Wandbound.Core.CardZoneObservation.PublicHandCountsOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneObservationPublicHandCountsOnlyTest::RunTest(const FString& Parameters)
{
	const FWBCardZonePublicSummary Summary = WBCardZoneObservation::BuildPublicSummary(MakeObservationState());
	const FWBObservedZoneSummary* P0Hand = FindObservedZoneByOwner(Summary.PlayerHands, 0);
	const FWBObservedZoneSummary* P1Hand = FindObservedZoneByOwner(Summary.PlayerHands, 1);
	TestEqual(TEXT("P0 hand count"), P0Hand != nullptr ? P0Hand->Count : -1, 2);
	TestEqual(TEXT("P1 hand count"), P1Hand != nullptr ? P1Hand->Count : -1, 1);
	TestEqual(TEXT("P0 hand cards hidden"), P0Hand != nullptr ? P0Hand->Cards.Num() : -1, 0);
	TestEqual(TEXT("P1 hand cards hidden"), P1Hand != nullptr ? P1Hand->Cards.Num() : -1, 0);
	TestFalse(TEXT("Own hand secret absent from public"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(Summary, TEXT("visible_own_hand_0")));
	TestFalse(TEXT("Opponent hand secret absent from public"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(Summary, TEXT("SECRET_P1_HAND_CARD_DO_NOT_LEAK")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneObservationPublicDiscardCountsOnlyTest, "Wandbound.Core.CardZoneObservation.PublicDiscardCountsOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneObservationPublicDiscardCountsOnlyTest::RunTest(const FString& Parameters)
{
	const FWBCardZonePublicSummary Summary = WBCardZoneObservation::BuildPublicSummary(MakeObservationState());
	const FWBObservedZoneSummary* P0Discard = FindObservedZoneByOwner(Summary.PlayerDiscards, 0);
	TestEqual(TEXT("P0 discard count"), P0Discard != nullptr ? P0Discard->Count : -1, 1);
	TestEqual(TEXT("P0 discard cards hidden publicly"), P0Discard != nullptr ? P0Discard->Cards.Num() : -1, 0);
	TestFalse(TEXT("P0 discard secret absent from public"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(Summary, TEXT("visible_own_discard_0")));
	TestFalse(TEXT("P1 discard secret absent from public"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(Summary, TEXT("SECRET_P1_DISCARD_CARD_DO_NOT_LEAK")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneObservationOwnHandVisibleTest, "Wandbound.Core.CardZoneObservation.OwnHandVisible", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneObservationOwnHandVisibleTest::RunTest(const FString& Parameters)
{
	const FWBCardZonePlayerObservation Observation = WBCardZoneObservation::BuildObservationForPlayer(MakeObservationState(), 0);
	TestEqual(TEXT("Own hand visibility"), Observation.OwnHand.Visibility, EWBZoneObservationVisibility::OwnPrivate);
	TestEqual(TEXT("Own hand count"), Observation.OwnHand.Count, 2);
	TestEqual(TEXT("Own hand first card ordered"), Observation.OwnHand.Cards[0].CardId, FString(TEXT("visible_own_hand_0")));
	TestEqual(TEXT("Own hand first instance ordered"), Observation.OwnHand.Cards[0].InstanceId, FString(TEXT("p0_hand_0")));
	TestFalse(TEXT("Opponent hand absent"), WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(Observation, TEXT("SECRET_P1_HAND_CARD_DO_NOT_LEAK")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneObservationOpponentHandHiddenTest, "Wandbound.Core.CardZoneObservation.OpponentHandHidden", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneObservationOpponentHandHiddenTest::RunTest(const FString& Parameters)
{
	const FWBCardZonePlayerObservation Observation = WBCardZoneObservation::BuildObservationForPlayer(MakeObservationState(), 0);
	TestFalse(TEXT("Opponent hand card hidden"), WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(Observation, TEXT("SECRET_P1_HAND_CARD_DO_NOT_LEAK")));
	TestFalse(TEXT("Opponent hand instance hidden"), WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(Observation, TEXT("p1_hand_0")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneObservationDeckIdentityHiddenTest, "Wandbound.Core.CardZoneObservation.DeckIdentityHidden", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneObservationDeckIdentityHiddenTest::RunTest(const FString& Parameters)
{
	const FWBCardZonePlayerObservation Observation = WBCardZoneObservation::BuildObservationForPlayer(MakeObservationState(), 0);
	TestEqual(TEXT("Own deck count"), Observation.OwnDeck.Count, 2);
	TestEqual(TEXT("Own deck cards hidden"), Observation.OwnDeck.Cards.Num(), 0);
	TestFalse(TEXT("Own deck identity absent"), WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(Observation, TEXT("SECRET_P0_DECK_CARD_DO_NOT_LEAK")));
	TestFalse(TEXT("Opponent deck identity absent"), WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(Observation, TEXT("SECRET_P1_DECK_CARD_DO_NOT_LEAK")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneObservationOwnDiscardOnlyTest, "Wandbound.Core.CardZoneObservation.OwnDiscardOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneObservationOwnDiscardOnlyTest::RunTest(const FString& Parameters)
{
	const FWBCardZonePlayerObservation Observation = WBCardZoneObservation::BuildObservationForPlayer(MakeObservationState(), 0);
	TestEqual(TEXT("Own discard visibility"), Observation.OwnDiscard.Visibility, EWBZoneObservationVisibility::OwnPrivate);
	TestEqual(TEXT("Own discard count"), Observation.OwnDiscard.Count, 1);
	TestEqual(TEXT("Own discard card visible"), Observation.OwnDiscard.Cards[0].CardId, FString(TEXT("visible_own_discard_0")));
	TestFalse(TEXT("Opponent discard hidden"), WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(Observation, TEXT("SECRET_P1_DISCARD_CARD_DO_NOT_LEAK")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneObservationHiddenMarkerIdentityAbsentTest, "Wandbound.Core.CardZoneObservation.HiddenMarkerIdentityAbsent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneObservationHiddenMarkerIdentityAbsentTest::RunTest(const FString& Parameters)
{
	const FWBCardZonePublicSummary Summary = WBCardZoneObservation::BuildPublicSummary(MakeObservationState());
	const FWBCardZonePlayerObservation Observation = WBCardZoneObservation::BuildObservationForPlayer(MakeObservationState(), 0);
	TestFalse(TEXT("Public marker secret absent"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(Summary, TEXT("SECRET_MARKER_DO_NOT_LEAK")));
	TestFalse(TEXT("Observation marker secret absent"), WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(Observation, TEXT("SECRET_MARKER_DO_NOT_LEAK")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneObservationMarkerFieldsPresentTest, "Wandbound.Core.CardZoneObservation.MarkerPublicFieldsPresent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneObservationMarkerFieldsPresentTest::RunTest(const FString& Parameters)
{
	const FWBCardZonePublicSummary Summary = WBCardZoneObservation::BuildPublicSummary(MakeObservationState());
	TestEqual(TEXT("Marker count"), Summary.Markers.Num(), 2);
	TestEqual(TEXT("First marker sorted by tile"), Summary.Markers[0].MarkerId, 3);
	TestEqual(TEXT("First marker owner"), Summary.Markers[0].OwnerPlayerId, 0);
	TestEqual(TEXT("First marker x"), Summary.Markers[0].Tile.X, 2);
	TestEqual(TEXT("First marker y"), Summary.Markers[0].Tile.Y, 2);
	TestEqual(TEXT("First marker state"), Summary.Markers[0].PublicState, EWBMarkerPublicState::Revealed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneObservationBoardRefsPublicTest, "Wandbound.Core.CardZoneObservation.BoardReferencesPublic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneObservationBoardRefsPublicTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeObservationState();
	TestTrue(TEXT("Removed unit added"), State.AddUnitForTest(MakeObservationUnit(9, 0, TEXT("removed_board_card"), FWBTile(2, 5))));
	FWBUnitState* RemovedUnit = State.GetMutableUnitById(9);
	RemovedUnit->MarkUnitDefeated();
	RemovedUnit->RemoveUnitFromBoard();

	const FWBCardZonePublicSummary Summary = WBCardZoneObservation::BuildPublicSummary(State);
	TestEqual(TEXT("Board refs exclude removed"), Summary.BoardCardReferences.Num(), 2);
	TestEqual(TEXT("Visible source board card"), Summary.BoardCardReferences[0].CardId, FString(TEXT("visible_board_player_card")));
	TestFalse(TEXT("Removed board card excluded"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(Summary, TEXT("removed_board_card")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneObservationEquippedCountOnlyTest, "Wandbound.Core.CardZoneObservation.EquippedCountOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneObservationEquippedCountOnlyTest::RunTest(const FString& Parameters)
{
	const FWBCardZonePublicSummary Summary = WBCardZoneObservation::BuildPublicSummary(MakeObservationState());
	TestEqual(TEXT("Equipped visibility"), Summary.Equipped.Visibility, EWBZoneObservationVisibility::CountOnly);
	TestEqual(TEXT("Equipped count"), Summary.Equipped.Count, 1);
	TestEqual(TEXT("Equipped cards hidden"), Summary.Equipped.OwnVisibleEquippedCards.Num(), 0);
	TestFalse(TEXT("Equipped secret absent"), WBCardZoneObservation::PublicSummaryContainsForbiddenSubstringForTest(Summary, TEXT("SECRET_EQUIPPED_CARD_DO_NOT_LEAK")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneObservationOwnEquippedFailClosedTest, "Wandbound.Core.CardZoneObservation.OwnEquippedFailClosed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneObservationOwnEquippedFailClosedTest::RunTest(const FString& Parameters)
{
	const FWBCardZonePlayerObservation Observation = WBCardZoneObservation::BuildObservationForPlayer(MakeObservationState(), 0);
	TestEqual(TEXT("Equipped count available via public summary"), Observation.PublicSummary.Equipped.Count, 1);
	TestEqual(TEXT("No own equipped identity exposed"), Observation.PublicSummary.Equipped.OwnVisibleEquippedCards.Num(), 0);
	TestFalse(TEXT("Own equipped secret absent"), WBCardZoneObservation::PlayerObservationContainsForbiddenSubstringForTest(Observation, TEXT("SECRET_EQUIPPED_CARD_DO_NOT_LEAK")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneObservationDeterministicOrderingTest, "Wandbound.Core.CardZoneObservation.DeterministicOrdering", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneObservationDeterministicOrderingTest::RunTest(const FString& Parameters)
{
	const FWBCardZonePlayerObservation Observation = WBCardZoneObservation::BuildObservationForPlayer(MakeObservationState(), 0);
	TestEqual(TEXT("Deck owner order p0"), Observation.PublicSummary.PlayerDecks[0].OwnerPlayerId, 0);
	TestEqual(TEXT("Deck owner order p1"), Observation.PublicSummary.PlayerDecks[1].OwnerPlayerId, 1);
	TestEqual(TEXT("Own hand first sorted by ZoneIndex"), Observation.OwnHand.Cards[0].InstanceId, FString(TEXT("p0_hand_0")));
	TestEqual(TEXT("Own hand second sorted by ZoneIndex"), Observation.OwnHand.Cards[1].InstanceId, FString(TEXT("p0_hand_1")));
	TestEqual(TEXT("First marker sorted"), Observation.PublicSummary.Markers[0].MarkerId, 3);
	TestEqual(TEXT("Second marker sorted"), Observation.PublicSummary.Markers[1].MarkerId, 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneObservationInvalidViewerTest, "Wandbound.Core.CardZoneObservation.InvalidViewerEmptyPrivateData", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneObservationInvalidViewerTest::RunTest(const FString& Parameters)
{
	const FWBCardZonePlayerObservation Observation = WBCardZoneObservation::BuildObservationForPlayer(MakeObservationState(), 7);
	TestEqual(TEXT("Viewer preserved"), Observation.ViewerPlayerId, 7);
	TestEqual(TEXT("Public summary still available"), Observation.PublicSummary.PlayerHands.Num(), 2);
	TestEqual(TEXT("Own hand hidden"), Observation.OwnHand.Visibility, EWBZoneObservationVisibility::Hidden);
	TestEqual(TEXT("Own hand empty"), Observation.OwnHand.Cards.Num(), 0);
	TestEqual(TEXT("Own deck empty"), Observation.OwnDeck.Cards.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneObservationNoMutationTest, "Wandbound.Core.CardZoneObservation.DoesNotMutateState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneObservationNoMutationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeObservationState();
	const int32 PlayerZoneCount = State.CardZoneState.PlayerZones.Num();
	const FString P0FirstHand = State.CardZoneState.PlayerZones[1].Hand[0].Card.CardId;
	const FString MarkerSecret = State.CardZoneState.MarkerPlaceholders[0].InternalMarkerCardId;

	WBCardZoneObservation::BuildPublicSummary(State);
	WBCardZoneObservation::BuildObservationForPlayer(State, 0);

	TestEqual(TEXT("Player zone count unchanged"), State.CardZoneState.PlayerZones.Num(), PlayerZoneCount);
	TestEqual(TEXT("Hand order in source unchanged"), State.CardZoneState.PlayerZones[1].Hand[0].Card.CardId, P0FirstHand);
	TestEqual(TEXT("Marker secret unchanged"), State.CardZoneState.MarkerPlaceholders[0].InternalMarkerCardId, MarkerSecret);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneObservationPublicBoardUnchangedTest, "Wandbound.Core.CardZoneObservation.PublicBoardSummaryUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneObservationPublicBoardUnchangedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData BaselineState = MakeObservationState();
	FWBGameStateData ZoneState = BaselineState;
	WBCardZoneObservation::BuildPublicSummary(ZoneState);
	const FString Baseline = SerializePublicBoardSummary(WBPublicBoardSummary::Build(BaselineState));
	const FString After = SerializePublicBoardSummary(WBPublicBoardSummary::Build(ZoneState));
	TestEqual(TEXT("Public board summary unchanged"), After, Baseline);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneObservationLegalGenerationUnchangedTest, "Wandbound.Core.CardZoneObservation.LegalGenerationUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneObservationLegalGenerationUnchangedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeObservationState();
	const TArray<FString> BeforeIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));
	WBCardZoneObservation::BuildPublicSummary(State);
	WBCardZoneObservation::BuildObservationForPlayer(State, 0);
	const TArray<FString> AfterIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(State, 0));
	TestEqual(TEXT("Legal action count unchanged"), AfterIds.Num(), BeforeIds.Num());
	for (int32 Index = 0; Index < FMath::Min(AfterIds.Num(), BeforeIds.Num()); ++Index)
	{
		TestEqual(*FString::Printf(TEXT("Action %d unchanged"), Index), AfterIds[Index], BeforeIds[Index]);
		TestFalse(*FString::Printf(TEXT("No zone action %s"), *AfterIds[Index]), AfterIds[Index].Contains(TEXT("zone")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneObservationActionCodecUnchangedTest, "Wandbound.Core.CardZoneObservation.ActionCodecUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneObservationActionCodecUnchangedTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Move action id"), WBActionCodec::MakeActionId(MakeObservationCodecAction(EWBActionType::Move)), FString(TEXT("move:p0:u1:4,4->3,4")));
	TestEqual(TEXT("Attack action id"), WBActionCodec::MakeActionId(MakeObservationCodecAction(EWBActionType::Attack)), FString(TEXT("attack:p0:u1:t2")));
	TestEqual(TEXT("EndTurn action id"), WBActionCodec::MakeActionId(MakeObservationCodecAction(EWBActionType::EndTurn)), FString(TEXT("end_turn:p0")));
	TestEqual(TEXT("Pass action id"), WBActionCodec::MakeActionId(MakeObservationCodecAction(EWBActionType::Pass)), FString(TEXT("pass:p0")));
	TestEqual(TEXT("PassResponse action id"), WBActionCodec::MakeActionId(MakeObservationCodecAction(EWBActionType::PassResponse, 1)), FString(TEXT("pass_response:p1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneObservationRuntimeSourceUnchangedTest, "Wandbound.Core.CardZoneObservation.RuntimeSourceUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneObservationRuntimeSourceUnchangedTest::RunTest(const FString& Parameters)
{
	const FString RuntimeSource = MakeRuntimeSourceText();
	const FString ProviderHeaderPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Public"), TEXT("WBProductionActivationDataProvider.h"));
	const FString ProviderSourcePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBProductionActivationDataProvider.cpp"));
	FString ProviderHeader;
	FString ProviderSource;
	TestTrue(TEXT("Production provider header loads"), FFileHelper::LoadFileToString(ProviderHeader, *ProviderHeaderPath));
	TestTrue(TEXT("Production provider source loads"), FFileHelper::LoadFileToString(ProviderSource, *ProviderSourcePath));
	FString RuntimeSourceWithoutProductionProvider = RuntimeSource;
	RuntimeSourceWithoutProductionProvider.ReplaceInline(*ProviderHeader, TEXT(""));
	RuntimeSourceWithoutProductionProvider.ReplaceInline(*ProviderSource, TEXT(""));

	TestTrue(TEXT("Production provider consumes observation helper"), ProviderSource.Contains(TEXT("WBCardZoneObservation")));
	TestFalse(TEXT("Runtime outside production provider does not include observation helper"), RuntimeSourceWithoutProductionProvider.Contains(TEXT("WBCardZoneObservation")));
	TestFalse(TEXT("Runtime does not mention hidden marker identity"), RuntimeSource.Contains(TEXT("InternalMarkerCardId")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneObservationHiddenSourceGuardTest, "Wandbound.Core.CardZoneObservation.HiddenSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneObservationHiddenSourceGuardTest::RunTest(const FString& Parameters)
{
	const FString ObservationHeaderPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Public"), TEXT("WBCardZoneObservation.h"));
	FString ObservationHeader;
	TestTrue(TEXT("Observation header loads"), FFileHelper::LoadFileToString(ObservationHeader, *ObservationHeaderPath));
	TestFalse(TEXT("Public observation structs do not include InternalMarkerCardId"), ObservationHeader.Contains(TEXT("InternalMarkerCardId")));

	const FString PublicBoardSummaryPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBPublicBoardSummary.cpp"));
	FString PublicBoardSummarySource;
	TestTrue(TEXT("Public board summary source loads"), FFileHelper::LoadFileToString(PublicBoardSummarySource, *PublicBoardSummaryPath));
	TestFalse(TEXT("Public board summary does not inspect CardZoneObservation"), PublicBoardSummarySource.Contains(TEXT("WBCardZoneObservation")));
	return true;
}

#endif
