#include "Misc/AutomationTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBActionCodec.h"
#include "WBCardZoneState.h"
#include "WBGameStateData.h"
#include "WBPublicBoardSummary.h"
#include "WBRules.h"
#include "WBRuntimeResultSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBZoneCardEntry MakeZoneEntry(
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

FWBEquippedCardEntry MakeEquippedEntry(
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

FWBPlayerCardZoneState MakePlayerZones(const int32 PlayerId)
{
	FWBPlayerCardZoneState PlayerZones;
	PlayerZones.PlayerId = PlayerId;
	return PlayerZones;
}

FWBMarkerPlaceholderEntry MakeMarker(
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

FWBUnitState MakeZoneTestUnit(
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

FWBPlayerStateData MakeZoneTestPlayer(const int32 PlayerId)
{
	FWBPlayerStateData Player;
	Player.PlayerId = PlayerId;
	Player.RemainingMP = PlayerId == 0 ? 2 : 0;
	Player.LastMPRoll = Player.RemainingMP;
	return Player;
}

FWBGameStateData MakeZoneActionState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.Phase = EWBGamePhase::NormalTurn;
	State.Players.Add(MakeZoneTestPlayer(0));
	State.Players.Add(MakeZoneTestPlayer(1));
	State.AddUnitForTest(MakeZoneTestUnit(1, 0, TEXT("zone_state_source"), FWBTile(4, 4)));
	State.AddUnitForTest(MakeZoneTestUnit(2, 1, TEXT("zone_state_target"), FWBTile(5, 4)));
	return State;
}

FString SerializePublicBoardSummary(const FWBPublicBoardSummary& Summary)
{
	FWBRuntimeSelectedActionResult Result;
	Result.FinalPublicBoardSummary = Summary;
	FString Json;
	WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Result, Json);
	return Json;
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

FWBAction MakeCodecAction(const EWBActionType Type, const int32 PlayerId = 0)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneStateStringRoundTripTest, "Wandbound.Core.CardZoneState.ZoneStringRoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneStateStringRoundTripTest::RunTest(const FString& Parameters)
{
	const EWBCardZone Zones[] = {
		EWBCardZone::Deck,
		EWBCardZone::Hand,
		EWBCardZone::Discard,
		EWBCardZone::Equipped,
		EWBCardZone::Board,
		EWBCardZone::Marker
	};

	for (const EWBCardZone Zone : Zones)
	{
		const FString ZoneString = WBCardZoneState::ZoneToString(Zone);
		TestEqual(*FString::Printf(TEXT("%s round-trip"), *ZoneString), WBCardZoneState::ZoneFromString(ZoneString), Zone);
		TestEqual(*FString::Printf(TEXT("%s lower round-trip"), *ZoneString), WBCardZoneState::ZoneFromString(ZoneString.ToLower()), Zone);
	}

	TestEqual(TEXT("Unknown string"), WBCardZoneState::ZoneFromString(TEXT("not_a_zone")), EWBCardZone::Unknown);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneStatePrivatePolicyTest, "Wandbound.Core.CardZoneState.PrivateZonePolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneStatePrivatePolicyTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Deck private"), WBCardZoneState::IsPrivateZone(EWBCardZone::Deck));
	TestTrue(TEXT("Hand private"), WBCardZoneState::IsPrivateZone(EWBCardZone::Hand));
	TestTrue(TEXT("Discard fail-closed private until observation policy"), WBCardZoneState::IsPrivateZone(EWBCardZone::Discard));
	TestFalse(TEXT("Board public"), WBCardZoneState::IsPrivateZone(EWBCardZone::Board));
	TestTrue(TEXT("Marker hidden by default"), WBCardZoneState::IsPrivateZone(EWBCardZone::Marker));
	TestTrue(TEXT("Deck ordered"), WBCardZoneState::IsOrderedZone(EWBCardZone::Deck));
	TestFalse(TEXT("Equipped not ordered zone"), WBCardZoneState::IsOrderedZone(EWBCardZone::Equipped));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneStatePlayerLookupTest, "Wandbound.Core.CardZoneState.PlayerZoneLookup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneStatePlayerLookupTest::RunTest(const FString& Parameters)
{
	FWBCardZoneState ZoneState;
	ZoneState.PlayerZones.Add(MakePlayerZones(0));
	ZoneState.PlayerZones.Add(MakePlayerZones(1));

	TestNotNull(TEXT("P0 found"), WBCardZoneState::FindPlayerZones(ZoneState, 0));
	TestNotNull(TEXT("P1 found"), WBCardZoneState::FindMutablePlayerZones(ZoneState, 1));
	TestNull(TEXT("Missing player absent"), WBCardZoneState::FindPlayerZones(ZoneState, 2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneStateCountsTest, "Wandbound.Core.CardZoneState.CountsPlayerZones", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneStateCountsTest::RunTest(const FString& Parameters)
{
	FWBCardZoneState ZoneState;
	FWBPlayerCardZoneState PlayerZones = MakePlayerZones(0);
	PlayerZones.Deck.Add(MakeZoneEntry(TEXT("d0"), TEXT("card_d0"), 0, EWBCardZone::Deck, 0));
	PlayerZones.Deck.Add(MakeZoneEntry(TEXT("d1"), TEXT("card_d1"), 0, EWBCardZone::Deck, 1));
	PlayerZones.Hand.Add(MakeZoneEntry(TEXT("h0"), TEXT("card_h0"), 0, EWBCardZone::Hand, 0));
	PlayerZones.Discard.Add(MakeZoneEntry(TEXT("x0"), TEXT("card_x0"), 0, EWBCardZone::Discard, 0));
	ZoneState.PlayerZones.Add(PlayerZones);

	TestEqual(TEXT("Deck count"), WBCardZoneState::CountCardsInZoneForPlayer(ZoneState, 0, EWBCardZone::Deck), 2);
	TestEqual(TEXT("Hand count"), WBCardZoneState::CountCardsInZoneForPlayer(ZoneState, 0, EWBCardZone::Hand), 1);
	TestEqual(TEXT("Discard count"), WBCardZoneState::CountCardsInZoneForPlayer(ZoneState, 0, EWBCardZone::Discard), 1);
	TestEqual(TEXT("Missing player count"), WBCardZoneState::CountCardsInZoneForPlayer(ZoneState, 1, EWBCardZone::Deck), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneStateDeterministicSortTest, "Wandbound.Core.CardZoneState.OrderedZonesSortDeterministically", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneStateDeterministicSortTest::RunTest(const FString& Parameters)
{
	FWBCardZoneState ZoneState;
	FWBPlayerCardZoneState PlayerZones = MakePlayerZones(0);
	PlayerZones.Hand.Add(MakeZoneEntry(TEXT("h2"), TEXT("card_h2"), 0, EWBCardZone::Hand, 2));
	PlayerZones.Hand.Add(MakeZoneEntry(TEXT("h0"), TEXT("card_h0"), 0, EWBCardZone::Hand, 0));
	PlayerZones.Hand.Add(MakeZoneEntry(TEXT("h1"), TEXT("card_h1"), 0, EWBCardZone::Hand, 1));
	ZoneState.PlayerZones.Add(PlayerZones);

	WBCardZoneState::SortOrderedZonesDeterministically(ZoneState);
	TestEqual(TEXT("First hand"), ZoneState.PlayerZones[0].Hand[0].Card.InstanceId, FString(TEXT("h0")));
	TestEqual(TEXT("Second hand"), ZoneState.PlayerZones[0].Hand[1].Card.InstanceId, FString(TEXT("h1")));
	TestEqual(TEXT("Third hand"), ZoneState.PlayerZones[0].Hand[2].Card.InstanceId, FString(TEXT("h2")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneStateDuplicateInstanceValidationTest, "Wandbound.Core.CardZoneState.DuplicateInstanceValidationFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneStateDuplicateInstanceValidationTest::RunTest(const FString& Parameters)
{
	FWBCardZoneState ZoneState;
	FWBPlayerCardZoneState PlayerZones = MakePlayerZones(0);
	PlayerZones.Deck.Add(MakeZoneEntry(TEXT("dup"), TEXT("card_a"), 0, EWBCardZone::Deck, 0));
	PlayerZones.Hand.Add(MakeZoneEntry(TEXT("dup"), TEXT("card_b"), 0, EWBCardZone::Hand, 0));
	ZoneState.PlayerZones.Add(PlayerZones);

	FString DuplicateInstanceId;
	TestTrue(TEXT("Duplicate detected"), WBCardZoneState::HasDuplicateCardInstanceIds(ZoneState, DuplicateInstanceId));
	TestEqual(TEXT("Duplicate id"), DuplicateInstanceId, FString(TEXT("dup")));

	FString Reason;
	TestFalse(TEXT("Validation fails"), WBCardZoneState::ValidateZoneStateForTest(ZoneState, Reason));
	TestTrue(TEXT("Reason reports duplicate"), Reason.Contains(TEXT("duplicate_instance_id")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneStateEmptyCardIdValidationTest, "Wandbound.Core.CardZoneState.EmptyCardIdValidationFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneStateEmptyCardIdValidationTest::RunTest(const FString& Parameters)
{
	FWBCardZoneState ZoneState;
	FWBPlayerCardZoneState PlayerZones = MakePlayerZones(0);
	PlayerZones.Deck.Add(MakeZoneEntry(TEXT("d0"), TEXT(""), 0, EWBCardZone::Deck, 0));
	ZoneState.PlayerZones.Add(PlayerZones);

	FString Reason;
	TestFalse(TEXT("Validation fails"), WBCardZoneState::ValidateZoneStateForTest(ZoneState, Reason));
	TestEqual(TEXT("Reason"), Reason, FString(TEXT("deck_card_id_empty")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneStateInvalidOwnerValidationTest, "Wandbound.Core.CardZoneState.InvalidOwnerValidationFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneStateInvalidOwnerValidationTest::RunTest(const FString& Parameters)
{
	FWBCardZoneState ZoneState;
	FWBPlayerCardZoneState PlayerZones = MakePlayerZones(0);
	PlayerZones.Hand.Add(MakeZoneEntry(TEXT("h0"), TEXT("card_h0"), 2, EWBCardZone::Hand, 0));
	ZoneState.PlayerZones.Add(PlayerZones);

	FString Reason;
	TestFalse(TEXT("Validation fails"), WBCardZoneState::ValidateZoneStateForTest(ZoneState, Reason));
	TestEqual(TEXT("Reason"), Reason, FString(TEXT("hand_owner_invalid")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneStateEquippedReferenceTest, "Wandbound.Core.CardZoneState.EquippedReferenceStored", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneStateEquippedReferenceTest::RunTest(const FString& Parameters)
{
	FWBCardZoneState ZoneState;
	ZoneState.PlayerZones.Add(MakePlayerZones(0));
	ZoneState.EquippedCards.Add(MakeEquippedEntry(TEXT("eq0"), TEXT("wand_basic"), 0, 12));

	FString Reason;
	TestTrue(TEXT("Validation succeeds"), WBCardZoneState::ValidateZoneStateForTest(ZoneState, Reason));
	TestEqual(TEXT("Equipped count"), WBCardZoneState::CountCardsInZoneForPlayer(ZoneState, 0, EWBCardZone::Equipped), 1);
	TestEqual(TEXT("Equipped unit id stored"), ZoneState.EquippedCards[0].EquippedToUnitId, 12);
	TestEqual(TEXT("Equipped slot stored"), ZoneState.EquippedCards[0].SlotId, FString(TEXT("wand")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneStateBoardReferencesExcludeRemovedTest, "Wandbound.Core.CardZoneState.BoardReferencesExcludeRemoved", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneStateBoardReferencesExcludeRemovedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeZoneActionState();
	TestTrue(TEXT("Removed unit added"), State.AddUnitForTest(MakeZoneTestUnit(3, 0, TEXT("removed_card"), FWBTile(2, 2))));
	FWBUnitState* RemovedUnit = State.GetMutableUnitById(3);
	RemovedUnit->MarkUnitDefeated();
	RemovedUnit->RemoveUnitFromBoard();

	const TArray<FWBBoardCardReference> References = WBCardZoneState::BuildBoardCardReferencesForTest(State);
	TestEqual(TEXT("Only active board refs"), References.Num(), 2);
	for (const FWBBoardCardReference& Reference : References)
	{
		TestNotEqual(TEXT("Removed unit excluded"), Reference.UnitId, 3);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneStateBoardReferencesSortTest, "Wandbound.Core.CardZoneState.BoardReferencesSortedDeterministically", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneStateBoardReferencesSortTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	State.Players.Add(MakeZoneTestPlayer(0));
	State.Players.Add(MakeZoneTestPlayer(1));
	TestTrue(TEXT("Unit 5 added"), State.AddUnitForTest(MakeZoneTestUnit(5, 0, TEXT("unit_5"), FWBTile(3, 3))));
	TestTrue(TEXT("Unit 3 added"), State.AddUnitForTest(MakeZoneTestUnit(3, 0, TEXT("unit_3"), FWBTile(1, 1))));
	TestTrue(TEXT("Unit 2 added"), State.AddUnitForTest(MakeZoneTestUnit(2, 0, TEXT("unit_2"), FWBTile(2, 1))));
	TestTrue(TEXT("Unit 1 added"), State.AddUnitForTest(MakeZoneTestUnit(1, 0, TEXT("unit_1"), FWBTile(0, 1))));

	const TArray<FWBBoardCardReference> References = WBCardZoneState::BuildBoardCardReferencesForTest(State);
	TestEqual(TEXT("First id"), References[0].UnitId, 1);
	TestEqual(TEXT("Second id"), References[1].UnitId, 3);
	TestEqual(TEXT("Third id"), References[2].UnitId, 2);
	TestEqual(TEXT("Fourth id"), References[3].UnitId, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneStateMarkerLookupTest, "Wandbound.Core.CardZoneState.MarkerPlaceholderLookup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneStateMarkerLookupTest::RunTest(const FString& Parameters)
{
	FWBCardZoneState ZoneState;
	ZoneState.MarkerPlaceholders.Add(MakeMarker(7, 1, FWBTile(3, 4), TEXT("secret_marker")));

	FWBMarkerPlaceholderEntry Marker;
	TestTrue(TEXT("Marker found"), WBCardZoneState::FindMarkerAtTileForTest(ZoneState, FWBTile(3, 4), Marker));
	TestEqual(TEXT("Marker id"), Marker.MarkerId, 7);
	TestEqual(TEXT("Owner"), Marker.OwnerPlayerId, 1);
	TestEqual(TEXT("Public state hidden"), Marker.PublicState, EWBMarkerPublicState::Hidden);
	TestEqual(TEXT("Internal id retained privately"), Marker.InternalMarkerCardId, FString(TEXT("secret_marker")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneStateMarkerDuplicateTileValidationTest, "Wandbound.Core.CardZoneState.MarkerDuplicateTileValidationFails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneStateMarkerDuplicateTileValidationTest::RunTest(const FString& Parameters)
{
	FWBCardZoneState ZoneState;
	ZoneState.MarkerPlaceholders.Add(MakeMarker(1, 0, FWBTile(2, 2), TEXT("secret_a")));
	ZoneState.MarkerPlaceholders.Add(MakeMarker(2, 1, FWBTile(2, 2), TEXT("secret_b")));

	FString Reason;
	TestFalse(TEXT("Validation fails"), WBCardZoneState::ValidateZoneStateForTest(ZoneState, Reason));
	TestTrue(TEXT("Reason reports duplicate marker tile"), Reason.Contains(TEXT("duplicate_marker_tile")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneStateMarkerHiddenIdentityPublicBoardTest, "Wandbound.Core.CardZoneState.MarkerHiddenIdentityAbsentFromPublicBoard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneStateMarkerHiddenIdentityPublicBoardTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeZoneActionState();
	State.CardZoneState.MarkerPlaceholders.Add(MakeMarker(1, 0, FWBTile(6, 6), TEXT("SECRET_MARKER_DO_NOT_LEAK")));

	const FString Serialized = SerializePublicBoardSummary(WBPublicBoardSummary::Build(State));
	TestFalse(TEXT("Marker identity absent"), Serialized.Contains(TEXT("SECRET_MARKER_DO_NOT_LEAK")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneStatePublicBoardUnchangedTest, "Wandbound.Core.CardZoneState.PublicBoardSummaryUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneStatePublicBoardUnchangedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData BaselineState = MakeZoneActionState();
	FWBGameStateData ZoneState = BaselineState;
	FWBPlayerCardZoneState PlayerZones = MakePlayerZones(0);
	PlayerZones.Deck.Add(MakeZoneEntry(TEXT("d0"), TEXT("secret_deck"), 0, EWBCardZone::Deck, 0));
	PlayerZones.Hand.Add(MakeZoneEntry(TEXT("h0"), TEXT("secret_hand"), 0, EWBCardZone::Hand, 0));
	PlayerZones.Discard.Add(MakeZoneEntry(TEXT("x0"), TEXT("secret_discard"), 0, EWBCardZone::Discard, 0));
	ZoneState.CardZoneState.PlayerZones.Add(PlayerZones);
	ZoneState.CardZoneState.EquippedCards.Add(MakeEquippedEntry(TEXT("eq0"), TEXT("secret_equipped"), 0, 1));

	const FString BaselineSummary = SerializePublicBoardSummary(WBPublicBoardSummary::Build(BaselineState));
	const FString ZoneSummary = SerializePublicBoardSummary(WBPublicBoardSummary::Build(ZoneState));
	TestEqual(TEXT("Public board summary unchanged"), ZoneSummary, BaselineSummary);
	TestFalse(TEXT("Deck secret absent"), ZoneSummary.Contains(TEXT("secret_deck")));
	TestFalse(TEXT("Hand secret absent"), ZoneSummary.Contains(TEXT("secret_hand")));
	TestFalse(TEXT("Discard secret absent"), ZoneSummary.Contains(TEXT("secret_discard")));
	TestFalse(TEXT("Equipped secret absent"), ZoneSummary.Contains(TEXT("secret_equipped")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneStateGameStateIntegrationTest, "Wandbound.Core.CardZoneState.GameStateIntegration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneStateGameStateIntegrationTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State;
	FWBCardZoneState& MutableZoneState = State.GetMutableCardZoneStateForTest();
	MutableZoneState.PlayerZones.Add(MakePlayerZones(0));
	MutableZoneState.PlayerZones[0].Deck.Add(MakeZoneEntry(TEXT("d0"), TEXT("card_d0"), 0, EWBCardZone::Deck, 0));

	TestEqual(TEXT("CardZoneState stored"), State.GetCardZoneState().PlayerZones.Num(), 1);
	State.ClearCardZoneStateForTest();
	TestEqual(TEXT("CardZoneState cleared"), State.GetCardZoneState().PlayerZones.Num(), 0);
	TestEqual(TEXT("Equipped cleared"), State.GetCardZoneState().EquippedCards.Num(), 0);
	TestEqual(TEXT("Markers cleared"), State.GetCardZoneState().MarkerPlaceholders.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneStateLegalGenerationUnchangedTest, "Wandbound.Core.CardZoneState.LegalGenerationUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneStateLegalGenerationUnchangedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData BaselineState = MakeZoneActionState();
	const TArray<FString> BaselineIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(BaselineState, 0));

	FWBGameStateData ZoneState = BaselineState;
	FWBPlayerCardZoneState PlayerZones = MakePlayerZones(0);
	PlayerZones.Hand.Add(MakeZoneEntry(TEXT("h0"), TEXT("card_h0"), 0, EWBCardZone::Hand, 0));
	ZoneState.CardZoneState.PlayerZones.Add(PlayerZones);
	ZoneState.CardZoneState.MarkerPlaceholders.Add(MakeMarker(1, 0, FWBTile(6, 6), TEXT("secret_marker")));

	const TArray<FString> ZoneIds = WBActionCodec::MakeActionIds(WBRules::GenerateLegalActionsForPlayer(ZoneState, 0));
	TestEqual(TEXT("Legal action count unchanged"), ZoneIds.Num(), BaselineIds.Num());
	for (int32 Index = 0; Index < FMath::Min(ZoneIds.Num(), BaselineIds.Num()); ++Index)
	{
		TestEqual(*FString::Printf(TEXT("Action id %d unchanged"), Index), ZoneIds[Index], BaselineIds[Index]);
		TestFalse(*FString::Printf(TEXT("No zone action id %s"), *ZoneIds[Index]), ZoneIds[Index].Contains(TEXT("zone")));
		TestFalse(*FString::Printf(TEXT("No card action id %s"), *ZoneIds[Index]), ZoneIds[Index].StartsWith(TEXT("card:")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneStateActionCodecUnchangedTest, "Wandbound.Core.CardZoneState.ActionCodecUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneStateActionCodecUnchangedTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Move action id"), WBActionCodec::MakeActionId(MakeCodecAction(EWBActionType::Move)), FString(TEXT("move:p0:u1:4,4->3,4")));
	TestEqual(TEXT("Attack action id"), WBActionCodec::MakeActionId(MakeCodecAction(EWBActionType::Attack)), FString(TEXT("attack:p0:u1:t2")));
	TestEqual(TEXT("EndTurn action id"), WBActionCodec::MakeActionId(MakeCodecAction(EWBActionType::EndTurn)), FString(TEXT("end_turn:p0")));
	TestEqual(TEXT("Pass action id"), WBActionCodec::MakeActionId(MakeCodecAction(EWBActionType::Pass)), FString(TEXT("pass:p0")));
	TestEqual(TEXT("PassResponse action id"), WBActionCodec::MakeActionId(MakeCodecAction(EWBActionType::PassResponse, 1)), FString(TEXT("pass_response:p1")));

	const FString ActionCodecPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBActionCodec.cpp"));
	FString ActionCodecSource;
	TestTrue(TEXT("Action codec source loads"), FFileHelper::LoadFileToString(ActionCodecSource, *ActionCodecPath));
	TestFalse(TEXT("Action codec has no zone IDs"), ActionCodecSource.Contains(TEXT("zone:")));
	TestFalse(TEXT("Action codec has no card action IDs"), ActionCodecSource.Contains(TEXT("card:")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneStateRuntimeSourceUnchangedTest, "Wandbound.Core.CardZoneState.RuntimeSourceUnchanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneStateRuntimeSourceUnchangedTest::RunTest(const FString& Parameters)
{
	const FString RuntimeSource = MakeRuntimeSourceText();
	TestFalse(TEXT("Runtime does not include card zone state"), RuntimeSource.Contains(TEXT("WBCardZoneState")));
	TestFalse(TEXT("Runtime does not mention hidden marker identity"), RuntimeSource.Contains(TEXT("InternalMarkerCardId")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBCardZoneStateHiddenInfoSourceGuardTest, "Wandbound.Core.CardZoneState.HiddenInfoSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBCardZoneStateHiddenInfoSourceGuardTest::RunTest(const FString& Parameters)
{
	const FString PublicBoardSummaryPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundCore"), TEXT("Private"), TEXT("WBPublicBoardSummary.cpp"));
	FString PublicBoardSummarySource;
	TestTrue(TEXT("Public board summary source loads"), FFileHelper::LoadFileToString(PublicBoardSummarySource, *PublicBoardSummaryPath));
	TestFalse(TEXT("Public board summary does not reference hidden marker id"), PublicBoardSummarySource.Contains(TEXT("InternalMarkerCardId")));

	const FString RuntimeSource = MakeRuntimeSourceText();
	TestFalse(TEXT("Runtime presentation does not reference hidden marker id"), RuntimeSource.Contains(TEXT("InternalMarkerCardId")));
	return true;
}

#endif
