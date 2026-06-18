#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WBPublicBoardSummary.h"
#include "WBReplayFixtureTestUtils.h"
#include "WBRuntimeResultSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
using namespace WandboundTest;

FWBUnitState MakeBoardSummaryUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const FString& CardId = TEXT("char_visible_public"),
	const int32 HP = 5,
	const int32 MaxHP = 5,
	const int32 ATK = 2,
	const int32 AR = 1,
	const int32 RLTotal = 3,
	const int32 RLUsed = 0,
	const int32 AttacksLeft = 1)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = CardId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = HP;
	Unit.MaxHP = MaxHP;
	Unit.ATK = ATK;
	Unit.AR = AR;
	Unit.RLTotal = RLTotal;
	Unit.RLUsed = RLUsed;
	Unit.AttacksLeft = AttacksLeft;
	return Unit;
}

FWBGameStateData MakeBoardSummaryState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 3;
	FWBPlayerStateData Player0;
	Player0.PlayerId = 0;
	FWBPlayerStateData Player1;
	Player1.PlayerId = 1;
	State.Players.Add(Player0);
	State.Players.Add(Player1);
	return State;
}

TSharedPtr<FJsonObject> ParseJsonObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}

FString BoardSummaryFixturePath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Reference"), TEXT("GodotCanon"), TEXT("GoldenScenarios"), FileName);
}

bool LoadBoardSummaryFixture(const FString& FileName, TSharedPtr<FJsonObject>& OutFixture)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *BoardSummaryFixturePath(FileName)))
	{
		return false;
	}

	OutFixture = ParseJsonObject(Json);
	return OutFixture.IsValid();
}

bool TryReadIntegerField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int32& OutValue)
{
	if (!Object.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonValue>* Value = Object->Values.Find(FieldName);
	if (Value == nullptr || !Value->IsValid() || (*Value)->Type != EJson::Number)
	{
		return false;
	}

	OutValue = static_cast<int32>((*Value)->AsNumber());
	return true;
}

TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	const TSharedPtr<FJsonObject>* Child = nullptr;
	if (Object.IsValid()
		&& Object->TryGetObjectField(FieldName, Child)
		&& Child != nullptr
		&& Child->IsValid())
	{
		return *Child;
	}

	return nullptr;
}

bool ExpectStatusMatches(
	FAutomationTestBase& Test,
	const FString& Label,
	const FWBPublicUnitStatusSummary& Actual,
	const TSharedPtr<FJsonObject>& Expected)
{
	FString ExpectedStatusId;
	int32 ExpectedTurnsRemaining = -1;
	Test.TestTrue(*FString::Printf(TEXT("%s expected status id reads"), *Label), Expected.IsValid() && Expected->TryGetStringField(TEXT("status_id"), ExpectedStatusId));
	Test.TestTrue(*FString::Printf(TEXT("%s expected status turns reads"), *Label), TryReadIntegerField(Expected, TEXT("turns_remaining"), ExpectedTurnsRemaining));
	Test.TestEqual(*FString::Printf(TEXT("%s status id"), *Label), Actual.StatusId.GetPlainNameString(), ExpectedStatusId);
	Test.TestEqual(*FString::Printf(TEXT("%s turns remaining"), *Label), Actual.TurnsRemaining, ExpectedTurnsRemaining);
	return Actual.StatusId.GetPlainNameString() == ExpectedStatusId && Actual.TurnsRemaining == ExpectedTurnsRemaining;
}

bool ExpectUnitMatches(
	FAutomationTestBase& Test,
	const FString& Label,
	const FWBPublicUnitBoardSummary& Actual,
	const TSharedPtr<FJsonObject>& Expected)
{
	int32 ExpectedInt = -1;
	FString ExpectedCardId;
	TryReadIntegerField(Expected, TEXT("unit_id"), ExpectedInt);
	Test.TestEqual(*FString::Printf(TEXT("%s unit id"), *Label), Actual.UnitId, ExpectedInt);
	TryReadIntegerField(Expected, TEXT("owner_id"), ExpectedInt);
	Test.TestEqual(*FString::Printf(TEXT("%s owner id"), *Label), Actual.OwnerId, ExpectedInt);
	Expected->TryGetStringField(TEXT("card_id"), ExpectedCardId);
	Test.TestEqual(*FString::Printf(TEXT("%s card id"), *Label), Actual.CardId, ExpectedCardId);

	const TCHAR* IntegerFields[] = {
		TEXT("x"),
		TEXT("y"),
		TEXT("hp"),
		TEXT("max_hp"),
		TEXT("atk"),
		TEXT("ar"),
		TEXT("rl_total"),
		TEXT("rl_used"),
		TEXT("attacks_left")
	};
	const int32 ActualValues[] = {
		Actual.X,
		Actual.Y,
		Actual.HP,
		Actual.MaxHP,
		Actual.ATK,
		Actual.AR,
		Actual.RLTotal,
		Actual.RLUsed,
		Actual.AttacksLeft
	};

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(IntegerFields); ++Index)
	{
		TryReadIntegerField(Expected, IntegerFields[Index], ExpectedInt);
		Test.TestEqual(
			*FString::Printf(TEXT("%s %s"), *Label, IntegerFields[Index]),
			ActualValues[Index],
			ExpectedInt);
	}

	const TCHAR* OptionalIntegerFields[] = {
		TEXT("current_armor"),
		TEXT("max_armor")
	};
	const int32 OptionalActualValues[] = {
		Actual.CurrentArmor,
		Actual.MaxArmor
	};

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(OptionalIntegerFields); ++Index)
	{
		if (!Expected.IsValid() || !Expected->HasField(OptionalIntegerFields[Index]))
		{
			continue;
		}

		TryReadIntegerField(Expected, OptionalIntegerFields[Index], ExpectedInt);
		Test.TestEqual(
			*FString::Printf(TEXT("%s %s"), *Label, OptionalIntegerFields[Index]),
			OptionalActualValues[Index],
			ExpectedInt);
	}

	const TArray<TSharedPtr<FJsonValue>>* ExpectedStatuses = nullptr;
	Test.TestTrue(
		*FString::Printf(TEXT("%s statuses exist"), *Label),
		Expected->TryGetArrayField(TEXT("statuses"), ExpectedStatuses) && ExpectedStatuses != nullptr);
	if (ExpectedStatuses != nullptr)
	{
		Test.TestEqual(*FString::Printf(TEXT("%s status count"), *Label), Actual.Statuses.Num(), ExpectedStatuses->Num());
		const int32 NumToCompare = FMath::Min(Actual.Statuses.Num(), ExpectedStatuses->Num());
		for (int32 StatusIndex = 0; StatusIndex < NumToCompare; ++StatusIndex)
		{
			ExpectStatusMatches(
				Test,
				FString::Printf(TEXT("%s status %d"), *Label, StatusIndex),
				Actual.Statuses[StatusIndex],
				(*ExpectedStatuses)[StatusIndex].IsValid() ? (*ExpectedStatuses)[StatusIndex]->AsObject() : nullptr);
		}
	}

	return true;
}

bool ExpectWallMatches(
	FAutomationTestBase& Test,
	const FString& Label,
	const FWBPublicWallEdgeSummary& Actual,
	const TSharedPtr<FJsonObject>& Expected)
{
	int32 ExpectedInt = -1;
	FString ExpectedOrientation;
	TryReadIntegerField(Expected, TEXT("ax"), ExpectedInt);
	Test.TestEqual(*FString::Printf(TEXT("%s ax"), *Label), Actual.AX, ExpectedInt);
	TryReadIntegerField(Expected, TEXT("ay"), ExpectedInt);
	Test.TestEqual(*FString::Printf(TEXT("%s ay"), *Label), Actual.AY, ExpectedInt);
	TryReadIntegerField(Expected, TEXT("bx"), ExpectedInt);
	Test.TestEqual(*FString::Printf(TEXT("%s bx"), *Label), Actual.BX, ExpectedInt);
	TryReadIntegerField(Expected, TEXT("by"), ExpectedInt);
	Test.TestEqual(*FString::Printf(TEXT("%s by"), *Label), Actual.BY, ExpectedInt);
	Expected->TryGetStringField(TEXT("orientation"), ExpectedOrientation);
	Test.TestEqual(*FString::Printf(TEXT("%s orientation"), *Label), Actual.Orientation.GetPlainNameString(), ExpectedOrientation);
	return true;
}

bool ExpectTerrainTileMatches(
	FAutomationTestBase& Test,
	const FString& Label,
	const FWBPublicTerrainTileSummary& Actual,
	const TSharedPtr<FJsonObject>& Expected)
{
	int32 ExpectedInt = -1;
	FString ExpectedTerrainId;
	TryReadIntegerField(Expected, TEXT("x"), ExpectedInt);
	Test.TestEqual(*FString::Printf(TEXT("%s x"), *Label), Actual.X, ExpectedInt);
	TryReadIntegerField(Expected, TEXT("y"), ExpectedInt);
	Test.TestEqual(*FString::Printf(TEXT("%s y"), *Label), Actual.Y, ExpectedInt);
	Expected->TryGetStringField(TEXT("terrain_id"), ExpectedTerrainId);
	Test.TestEqual(*FString::Printf(TEXT("%s terrain id"), *Label), Actual.TerrainId.GetPlainNameString(), ExpectedTerrainId);
	return true;
}

bool ExpectBoardSummaryMatches(
	FAutomationTestBase& Test,
	const FString& Label,
	const FWBPublicBoardSummary& Actual,
	const TSharedPtr<FJsonObject>& Expected)
{
	int32 ExpectedInt = -1;
	TryReadIntegerField(Expected, TEXT("board_width"), ExpectedInt);
	Test.TestEqual(*FString::Printf(TEXT("%s board width"), *Label), Actual.BoardWidth, ExpectedInt);
	TryReadIntegerField(Expected, TEXT("board_height"), ExpectedInt);
	Test.TestEqual(*FString::Printf(TEXT("%s board height"), *Label), Actual.BoardHeight, ExpectedInt);

	FString ExpectedDefaultTerrainId;
	if (Expected->TryGetStringField(TEXT("default_terrain_id"), ExpectedDefaultTerrainId))
	{
		Test.TestEqual(
			*FString::Printf(TEXT("%s default terrain"), *Label),
			Actual.DefaultTerrainId.GetPlainNameString(),
			ExpectedDefaultTerrainId);
	}

	const TArray<TSharedPtr<FJsonValue>>* ExpectedUnits = nullptr;
	Test.TestTrue(
		*FString::Printf(TEXT("%s units exist"), *Label),
		Expected->TryGetArrayField(TEXT("units"), ExpectedUnits) && ExpectedUnits != nullptr);
	if (ExpectedUnits == nullptr)
	{
		return false;
	}

	Test.TestEqual(*FString::Printf(TEXT("%s unit count"), *Label), Actual.Units.Num(), ExpectedUnits->Num());
	const int32 NumToCompare = FMath::Min(Actual.Units.Num(), ExpectedUnits->Num());
	for (int32 UnitIndex = 0; UnitIndex < NumToCompare; ++UnitIndex)
	{
		ExpectUnitMatches(
			Test,
			FString::Printf(TEXT("%s unit %d"), *Label, UnitIndex),
			Actual.Units[UnitIndex],
			(*ExpectedUnits)[UnitIndex].IsValid() ? (*ExpectedUnits)[UnitIndex]->AsObject() : nullptr);
	}

	const TArray<TSharedPtr<FJsonValue>>* ExpectedWalls = nullptr;
	if (Expected->TryGetArrayField(TEXT("walls"), ExpectedWalls) && ExpectedWalls != nullptr)
	{
		Test.TestEqual(*FString::Printf(TEXT("%s wall count"), *Label), Actual.Walls.Num(), ExpectedWalls->Num());
		const int32 NumWallsToCompare = FMath::Min(Actual.Walls.Num(), ExpectedWalls->Num());
		for (int32 WallIndex = 0; WallIndex < NumWallsToCompare; ++WallIndex)
		{
			ExpectWallMatches(
				Test,
				FString::Printf(TEXT("%s wall %d"), *Label, WallIndex),
				Actual.Walls[WallIndex],
				(*ExpectedWalls)[WallIndex].IsValid() ? (*ExpectedWalls)[WallIndex]->AsObject() : nullptr);
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* ExpectedTerrainTiles = nullptr;
	if (Expected->TryGetArrayField(TEXT("terrain_tiles"), ExpectedTerrainTiles) && ExpectedTerrainTiles != nullptr)
	{
		Test.TestEqual(
			*FString::Printf(TEXT("%s terrain tile count"), *Label),
			Actual.TerrainTiles.Num(),
			ExpectedTerrainTiles->Num());
		const int32 NumTerrainTilesToCompare = FMath::Min(Actual.TerrainTiles.Num(), ExpectedTerrainTiles->Num());
		for (int32 TerrainIndex = 0; TerrainIndex < NumTerrainTilesToCompare; ++TerrainIndex)
		{
			ExpectTerrainTileMatches(
				Test,
				FString::Printf(TEXT("%s terrain %d"), *Label, TerrainIndex),
				Actual.TerrainTiles[TerrainIndex],
				(*ExpectedTerrainTiles)[TerrainIndex].IsValid() ? (*ExpectedTerrainTiles)[TerrainIndex]->AsObject() : nullptr);
		}
	}

	return true;
}

FString SerializeBoardSummaryThroughRuntimeResult(const FWBPublicBoardSummary& Summary)
{
	FWBRuntimeSelectedActionResult Envelope;
	Envelope.FinalPublicBoardSummary = Summary;
	FString Json;
	WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Envelope, Json);
	return Json;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPublicBoardSummaryPositionTest, "Wandbound.Core.PublicBoardSummary.IncludesVisibleUnitPosition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPublicBoardSummaryPositionTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBoardSummaryState();
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(MakeBoardSummaryUnit(1, 0, FWBTile(4, 4))));

	const FWBPublicBoardSummary Summary = WBPublicBoardSummary::Build(State);
	TestEqual(TEXT("Unit count"), Summary.Units.Num(), 1);
	TestEqual(TEXT("Unit id"), Summary.Units[0].UnitId, 1);
	TestEqual(TEXT("Owner id"), Summary.Units[0].OwnerId, 0);
	TestEqual(TEXT("X"), Summary.Units[0].X, 4);
	TestEqual(TEXT("Y"), Summary.Units[0].Y, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPublicBoardSummaryCombatFieldsTest, "Wandbound.Core.PublicBoardSummary.IncludesCombatFields", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPublicBoardSummaryCombatFieldsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBoardSummaryState();
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(MakeBoardSummaryUnit(1, 0, FWBTile(4, 4), TEXT("char_visible_public"), 3, 6, 4, 2, 5, 1, 0)));

	const FWBPublicUnitBoardSummary Unit = WBPublicBoardSummary::Build(State).Units[0];
	TestEqual(TEXT("HP"), Unit.HP, 3);
	TestEqual(TEXT("MaxHP"), Unit.MaxHP, 6);
	TestEqual(TEXT("Current armor"), Unit.CurrentArmor, 0);
	TestEqual(TEXT("Max armor"), Unit.MaxArmor, 0);
	TestEqual(TEXT("ATK"), Unit.ATK, 4);
	TestEqual(TEXT("AR"), Unit.AR, 2);
	TestEqual(TEXT("RL total"), Unit.RLTotal, 5);
	TestEqual(TEXT("RL used"), Unit.RLUsed, 1);
	TestEqual(TEXT("Attacks left"), Unit.AttacksLeft, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPublicBoardSummaryArmorFieldsTest, "Wandbound.Core.PublicBoardSummary.IncludesArmorFields", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPublicBoardSummaryArmorFieldsTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBoardSummaryState();
	FWBUnitState Unit = MakeBoardSummaryUnit(1, 0, FWBTile(4, 4));
	Unit.SetArmorForTest(2, 3);
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(Unit));

	const FWBPublicUnitBoardSummary SummaryUnit = WBPublicBoardSummary::Build(State).Units[0];
	TestEqual(TEXT("Current armor"), SummaryUnit.CurrentArmor, 2);
	TestEqual(TEXT("Max armor"), SummaryUnit.MaxArmor, 3);

	const FString Serialized = SerializeBoardSummaryThroughRuntimeResult(WBPublicBoardSummary::Build(State));
	TestTrue(TEXT("Current armor serializes"), Serialized.Contains(TEXT("current_armor")));
	TestTrue(TEXT("Max armor serializes"), Serialized.Contains(TEXT("max_armor")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPublicBoardSummaryStatusesTest, "Wandbound.Core.PublicBoardSummary.IncludesStatuses", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPublicBoardSummaryStatusesTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBoardSummaryState();
	FWBUnitState Unit = MakeBoardSummaryUnit(1, 0, FWBTile(4, 4));
	Unit.AddStatus(FName(TEXT("Burn")), 1);
	Unit.AddStatus(FName(TEXT("root")), 3);
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(Unit));

	const TArray<FWBPublicUnitStatusSummary>& Statuses = WBPublicBoardSummary::Build(State).Units[0].Statuses;
	TestEqual(TEXT("Status count"), Statuses.Num(), 2);
	TestEqual(TEXT("First status"), Statuses[0].StatusId.GetPlainNameString(), FString(TEXT("Burn")));
	TestEqual(TEXT("First status turns"), Statuses[0].TurnsRemaining, 1);
	TestEqual(TEXT("Second status canonical"), Statuses[1].StatusId.GetPlainNameString(), FString(TEXT("Rooted")));
	TestEqual(TEXT("Second status turns"), Statuses[1].TurnsRemaining, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPublicBoardSummaryUnitSortTest, "Wandbound.Core.PublicBoardSummary.UnitsSortedDeterministically", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPublicBoardSummaryUnitSortTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBoardSummaryState();
	State.Units.Add(MakeBoardSummaryUnit(5, 0, FWBTile(1, 1)));
	State.Units.Add(MakeBoardSummaryUnit(3, 0, FWBTile(1, 0)));
	State.Units.Add(MakeBoardSummaryUnit(2, 0, FWBTile(2, 0)));
	State.Units.Add(MakeBoardSummaryUnit(1, 0, FWBTile(1, 0)));

	const FWBPublicBoardSummary Summary = WBPublicBoardSummary::Build(State);
	TestEqual(TEXT("First unit sorted by y/x/id"), Summary.Units[0].UnitId, 1);
	TestEqual(TEXT("Second unit sorted by y/x/id"), Summary.Units[1].UnitId, 3);
	TestEqual(TEXT("Third unit sorted by y/x/id"), Summary.Units[2].UnitId, 2);
	TestEqual(TEXT("Fourth unit sorted by y/x/id"), Summary.Units[3].UnitId, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPublicBoardSummaryStatusSortTest, "Wandbound.Core.PublicBoardSummary.StatusesSortedDeterministically", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPublicBoardSummaryStatusSortTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBoardSummaryState();
	FWBUnitState Unit = MakeBoardSummaryUnit(1, 0, FWBTile(4, 4));
	Unit.AddStatus(FName(TEXT("stun")), 2);
	Unit.AddStatus(FName(TEXT("Burn")), 1);
	Unit.AddStatus(FName(TEXT("root")), 3);
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(Unit));

	const TArray<FWBPublicUnitStatusSummary>& Statuses = WBPublicBoardSummary::Build(State).Units[0].Statuses;
	TestEqual(TEXT("First status"), Statuses[0].StatusId.GetPlainNameString(), FString(TEXT("Burn")));
	TestEqual(TEXT("Second status"), Statuses[1].StatusId.GetPlainNameString(), FString(TEXT("Rooted")));
	TestEqual(TEXT("Third status"), Statuses[2].StatusId.GetPlainNameString(), FString(TEXT("Stunned")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPublicBoardSummaryHiddenDataTest, "Wandbound.Core.PublicBoardSummary.HiddenDataExcluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPublicBoardSummaryHiddenDataTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBoardSummaryState();
	State.Players[0].Deck.Add(TEXT("SECRET_DECK_CARD_DO_NOT_LEAK"));
	State.Players[0].Hand.Add(TEXT("SECRET_HAND_CARD_DO_NOT_LEAK"));
	State.Players[0].Discard.Add(TEXT("SECRET_DISCARD_CARD_DO_NOT_LEAK"));
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(MakeBoardSummaryUnit(1, 0, FWBTile(4, 4), TEXT("char_visible_public_identity"))));

	const FString Serialized = SerializeBoardSummaryThroughRuntimeResult(WBPublicBoardSummary::Build(State));
	TestFalse(TEXT("Deck secret excluded"), Serialized.Contains(TEXT("SECRET_DECK_CARD_DO_NOT_LEAK")));
	TestFalse(TEXT("Hand secret excluded"), Serialized.Contains(TEXT("SECRET_HAND_CARD_DO_NOT_LEAK")));
	TestFalse(TEXT("Discard secret excluded"), Serialized.Contains(TEXT("SECRET_DISCARD_CARD_DO_NOT_LEAK")));
	TestFalse(TEXT("Hidden marker placeholder secret absent"), Serialized.Contains(TEXT("SECRET_HIDDEN_MARKER_CARD_DO_NOT_LEAK")));
	TestFalse(TEXT("Pending choice placeholder secret absent"), Serialized.Contains(TEXT("SECRET_PENDING_CHOICE_DO_NOT_LEAK")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPublicBoardSummaryVisibleCardIdentityTest, "Wandbound.Core.PublicBoardSummary.VisibleUnitCardIdentityPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPublicBoardSummaryVisibleCardIdentityTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBoardSummaryState();
	TestTrue(TEXT("Unit added"), State.AddUnitForTest(MakeBoardSummaryUnit(1, 0, FWBTile(4, 4), TEXT("char_visible_public_identity"))));

	const FWBPublicBoardSummary Summary = WBPublicBoardSummary::Build(State);
	TestEqual(TEXT("Visible unit card id included"), Summary.Units[0].CardId, FString(TEXT("char_visible_public_identity")));
	const FString Serialized = SerializeBoardSummaryThroughRuntimeResult(Summary);
	TestTrue(TEXT("Visible card id serializes"), Serialized.Contains(TEXT("char_visible_public_identity")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPublicBoardSummaryWallsIncludedTest, "Wandbound.Core.PublicBoardSummary.WallsIncluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPublicBoardSummaryWallsIncludedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBoardSummaryState();
	State.AddWallForTest(FWBWallEdge(FWBTile(4, 4), FWBTile(4, 5)));

	const FWBPublicBoardSummary Summary = WBPublicBoardSummary::Build(State);
	TestEqual(TEXT("Wall count"), Summary.Walls.Num(), 1);
	TestEqual(TEXT("Wall ax"), Summary.Walls[0].AX, 4);
	TestEqual(TEXT("Wall ay"), Summary.Walls[0].AY, 4);
	TestEqual(TEXT("Wall bx"), Summary.Walls[0].BX, 4);
	TestEqual(TEXT("Wall by"), Summary.Walls[0].BY, 5);
	TestEqual(TEXT("Wall orientation"), Summary.Walls[0].Orientation.GetPlainNameString(), FString(TEXT("vertical")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPublicBoardSummaryWallNormalizedTest, "Wandbound.Core.PublicBoardSummary.WallEdgeNormalized", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPublicBoardSummaryWallNormalizedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData ForwardState = MakeBoardSummaryState();
	ForwardState.AddWallForTest(FWBWallEdge(FWBTile(4, 4), FWBTile(4, 5)));
	FWBGameStateData ReverseState = MakeBoardSummaryState();
	ReverseState.AddWallForTest(FWBWallEdge(FWBTile(4, 5), FWBTile(4, 4)));

	const FWBPublicBoardSummary ForwardSummary = WBPublicBoardSummary::Build(ForwardState);
	const FWBPublicBoardSummary ReverseSummary = WBPublicBoardSummary::Build(ReverseState);
	TestEqual(TEXT("Forward wall count"), ForwardSummary.Walls.Num(), 1);
	TestEqual(TEXT("Reverse wall count"), ReverseSummary.Walls.Num(), 1);
	TestEqual(TEXT("AX matches"), ForwardSummary.Walls[0].AX, ReverseSummary.Walls[0].AX);
	TestEqual(TEXT("AY matches"), ForwardSummary.Walls[0].AY, ReverseSummary.Walls[0].AY);
	TestEqual(TEXT("BX matches"), ForwardSummary.Walls[0].BX, ReverseSummary.Walls[0].BX);
	TestEqual(TEXT("BY matches"), ForwardSummary.Walls[0].BY, ReverseSummary.Walls[0].BY);
	TestEqual(
		TEXT("Orientation matches"),
		ForwardSummary.Walls[0].Orientation.GetPlainNameString(),
		ReverseSummary.Walls[0].Orientation.GetPlainNameString());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPublicBoardSummaryWallSortTest, "Wandbound.Core.PublicBoardSummary.WallsSortedDeterministically", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPublicBoardSummaryWallSortTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBoardSummaryState();
	State.AddWallForTest(FWBWallEdge(FWBTile(5, 5), FWBTile(5, 6)));
	State.AddWallForTest(FWBWallEdge(FWBTile(1, 2), FWBTile(2, 2)));
	State.AddWallForTest(FWBWallEdge(FWBTile(0, 1), FWBTile(0, 2)));

	const FWBPublicBoardSummary Summary = WBPublicBoardSummary::Build(State);
	TestEqual(TEXT("Wall count"), Summary.Walls.Num(), 3);
	TestEqual(TEXT("First wall ay"), Summary.Walls[0].AY, 1);
	TestEqual(TEXT("First wall ax"), Summary.Walls[0].AX, 0);
	TestEqual(TEXT("Second wall ay"), Summary.Walls[1].AY, 2);
	TestEqual(TEXT("Second wall ax"), Summary.Walls[1].AX, 1);
	TestEqual(TEXT("Third wall ay"), Summary.Walls[2].AY, 5);
	TestEqual(TEXT("Third wall ax"), Summary.Walls[2].AX, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPublicBoardSummaryInvalidWallExcludedTest, "Wandbound.Core.PublicBoardSummary.InvalidWallExcluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPublicBoardSummaryInvalidWallExcludedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBoardSummaryState();
	State.Walls.Add(FWBWallEdge(FWBTile(1, 1), FWBTile(3, 3)));

	const FWBPublicBoardSummary Summary = WBPublicBoardSummary::Build(State);
	TestEqual(TEXT("Invalid wall excluded"), Summary.Walls.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPublicBoardSummaryTerrainIncludedTest, "Wandbound.Core.PublicBoardSummary.TerrainIncluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPublicBoardSummaryTerrainIncludedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBoardSummaryState();
	State.SetTerrainForTest(FWBTile(2, 3), FName(TEXT("Mud")));

	const FWBPublicBoardSummary Summary = WBPublicBoardSummary::Build(State);
	TestEqual(TEXT("Default terrain"), Summary.DefaultTerrainId.GetPlainNameString(), FString(TEXT("Normal")));
	TestEqual(TEXT("Terrain count"), Summary.TerrainTiles.Num(), 1);
	TestEqual(TEXT("Terrain x"), Summary.TerrainTiles[0].X, 2);
	TestEqual(TEXT("Terrain y"), Summary.TerrainTiles[0].Y, 3);
	TestEqual(TEXT("Terrain id"), Summary.TerrainTiles[0].TerrainId.GetPlainNameString(), FString(TEXT("mud")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPublicBoardSummaryDefaultTerrainExcludedTest, "Wandbound.Core.PublicBoardSummary.DefaultTerrainExcluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPublicBoardSummaryDefaultTerrainExcludedTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBoardSummaryState();
	State.SetTerrainForTest(FWBTile(2, 3), FName(TEXT("Normal")));

	const FWBPublicBoardSummary Summary = WBPublicBoardSummary::Build(State);
	TestEqual(TEXT("Default terrain"), Summary.DefaultTerrainId.GetPlainNameString(), FString(TEXT("Normal")));
	TestEqual(TEXT("Default terrain tile omitted"), Summary.TerrainTiles.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPublicBoardSummaryTerrainSortTest, "Wandbound.Core.PublicBoardSummary.TerrainSortedDeterministically", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPublicBoardSummaryTerrainSortTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBoardSummaryState();
	State.SetTerrainForTest(FWBTile(5, 5), FName(TEXT("lava")));
	State.SetTerrainForTest(FWBTile(1, 2), FName(TEXT("ice")));
	State.SetTerrainForTest(FWBTile(1, 1), FName(TEXT("water")));

	const FWBPublicBoardSummary Summary = WBPublicBoardSummary::Build(State);
	TestEqual(TEXT("Terrain count"), Summary.TerrainTiles.Num(), 3);
	TestEqual(TEXT("First terrain y"), Summary.TerrainTiles[0].Y, 1);
	TestEqual(TEXT("First terrain x"), Summary.TerrainTiles[0].X, 1);
	TestEqual(TEXT("Second terrain y"), Summary.TerrainTiles[1].Y, 2);
	TestEqual(TEXT("Second terrain x"), Summary.TerrainTiles[1].X, 1);
	TestEqual(TEXT("Third terrain y"), Summary.TerrainTiles[2].Y, 5);
	TestEqual(TEXT("Third terrain x"), Summary.TerrainTiles[2].X, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBPublicBoardSummaryFixtureScenariosTest, "Wandbound.Core.PublicBoardSummary.FixtureScenarios", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBPublicBoardSummaryFixtureScenariosTest::RunTest(const FString& Parameters)
{
	const TArray<FString> FixtureNames = {
		TEXT("public_board_summary_units_sorted.json"),
		TEXT("public_board_summary_statuses_sorted.json"),
		TEXT("public_board_summary_hidden_data_excluded.json"),
		TEXT("public_board_summary_walls_sorted.json"),
		TEXT("public_board_summary_wall_edge_normalized.json"),
		TEXT("public_board_summary_terrain_sparse_sorted.json"),
		TEXT("public_board_summary_wall_terrain_hidden_data_excluded.json")
	};

	for (const FString& FixtureName : FixtureNames)
	{
		TSharedPtr<FJsonObject> Fixture;
		TestTrue(*FString::Printf(TEXT("%s loads"), *FixtureName), LoadBoardSummaryFixture(FixtureName, Fixture));
		if (!Fixture.IsValid())
		{
			continue;
		}

		FString Operation;
		TestTrue(*FString::Printf(TEXT("%s operation reads"), *FixtureName), Fixture->TryGetStringField(TEXT("operation"), Operation));
		TestEqual(*FString::Printf(TEXT("%s operation kind"), *FixtureName), Operation, FString(TEXT("build_public_board_summary")));

		FWBGameStateData State;
		FString Reason;
		TestTrue(*FString::Printf(TEXT("%s state builds: %s"), *FixtureName, *Reason), BuildGameStateFromFixture(Fixture, State, Reason));
		if (!Reason.IsEmpty())
		{
			continue;
		}

		const FWBPublicBoardSummary Summary = WBPublicBoardSummary::Build(State);
		const TSharedPtr<FJsonObject> Expected = GetObjectField(Fixture, TEXT("expected"));
		TestTrue(*FString::Printf(TEXT("%s expected exists"), *FixtureName), Expected.IsValid());
		if (Expected.IsValid())
		{
			ExpectBoardSummaryMatches(*this, FixtureName, Summary, Expected);

			const FString Serialized = SerializeBoardSummaryThroughRuntimeResult(Summary);
			const TArray<TSharedPtr<FJsonValue>>* ForbiddenSubstrings = nullptr;
			if (Expected->TryGetArrayField(TEXT("forbidden_substrings"), ForbiddenSubstrings) && ForbiddenSubstrings != nullptr)
			{
				for (const TSharedPtr<FJsonValue>& ForbiddenValue : *ForbiddenSubstrings)
				{
					const FString Forbidden = ForbiddenValue.IsValid() ? ForbiddenValue->AsString() : FString();
					TestFalse(*FString::Printf(TEXT("%s excludes %s"), *FixtureName, *Forbidden), Serialized.Contains(Forbidden));
				}
			}
		}
	}

	return true;
}

#endif
