#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBBoardSummaryBridge.h"
#include "WBBoardViewActor.h"
#include "WBRuntimeResultSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBUnitState MakeBridgeTestUnit(
	const int32 UnitId,
	const int32 OwnerId,
	const FWBTile& Tile,
	const FString& CardId)
{
	FWBUnitState Unit;
	Unit.UnitId = UnitId;
	Unit.OwnerId = OwnerId;
	Unit.CardId = CardId;
	Unit.X = Tile.X;
	Unit.Y = Tile.Y;
	Unit.HP = 4;
	Unit.MaxHP = 5;
	Unit.ATK = 2;
	Unit.AR = 1;
	Unit.AttacksLeft = 1;
	return Unit;
}

FWBGameStateData MakeBridgeTestState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 4;

	FWBPlayerStateData Player0;
	Player0.PlayerId = 0;
	Player0.Deck.Add(TEXT("SECRET_DECK_CARD_DO_NOT_LEAK"));
	Player0.Hand.Add(TEXT("SECRET_HAND_CARD_DO_NOT_LEAK"));
	Player0.Discard.Add(TEXT("SECRET_DISCARD_CARD_DO_NOT_LEAK"));

	FWBPlayerStateData Player1;
	Player1.PlayerId = 1;
	Player1.Deck.Add(TEXT("SECRET_OPPONENT_DECK_CARD_DO_NOT_LEAK"));
	Player1.Hand.Add(TEXT("SECRET_OPPONENT_HAND_CARD_DO_NOT_LEAK"));
	Player1.Discard.Add(TEXT("SECRET_OPPONENT_DISCARD_CARD_DO_NOT_LEAK"));

	State.Players.Add(Player0);
	State.Players.Add(Player1);

	State.AddUnitForTest(MakeBridgeTestUnit(1, 0, FWBTile(4, 4), TEXT("char_visible_player")));
	State.AddUnitForTest(MakeBridgeTestUnit(2, 1, FWBTile(5, 4), TEXT("char_visible_opponent")));
	State.AddWallForTest(FWBWallEdge(FWBTile(4, 4), FWBTile(4, 5)));
	State.SetTerrainForTest(FWBTile(2, 3), FName(TEXT("Mud")));

	return State;
}

void ExpectSummaryMatches(
	FAutomationTestBase& Test,
	const FWBPublicBoardSummary& Actual,
	const FWBPublicBoardSummary& Expected)
{
	Test.TestEqual(TEXT("Board width"), Actual.BoardWidth, Expected.BoardWidth);
	Test.TestEqual(TEXT("Board height"), Actual.BoardHeight, Expected.BoardHeight);
	Test.TestEqual(TEXT("Default terrain"), Actual.DefaultTerrainId.GetPlainNameString(), Expected.DefaultTerrainId.GetPlainNameString());
	Test.TestEqual(TEXT("Unit count"), Actual.Units.Num(), Expected.Units.Num());
	Test.TestEqual(TEXT("Wall count"), Actual.Walls.Num(), Expected.Walls.Num());
	Test.TestEqual(TEXT("Terrain count"), Actual.TerrainTiles.Num(), Expected.TerrainTiles.Num());

	const int32 UnitCount = FMath::Min(Actual.Units.Num(), Expected.Units.Num());
	for (int32 Index = 0; Index < UnitCount; ++Index)
	{
		const FWBPublicUnitBoardSummary& ActualUnit = Actual.Units[Index];
		const FWBPublicUnitBoardSummary& ExpectedUnit = Expected.Units[Index];
		Test.TestEqual(*FString::Printf(TEXT("Unit %d id"), Index), ActualUnit.UnitId, ExpectedUnit.UnitId);
		Test.TestEqual(*FString::Printf(TEXT("Unit %d owner"), Index), ActualUnit.OwnerId, ExpectedUnit.OwnerId);
		Test.TestEqual(*FString::Printf(TEXT("Unit %d card"), Index), ActualUnit.CardId, ExpectedUnit.CardId);
		Test.TestEqual(*FString::Printf(TEXT("Unit %d x"), Index), ActualUnit.X, ExpectedUnit.X);
		Test.TestEqual(*FString::Printf(TEXT("Unit %d y"), Index), ActualUnit.Y, ExpectedUnit.Y);
		Test.TestEqual(*FString::Printf(TEXT("Unit %d hp"), Index), ActualUnit.HP, ExpectedUnit.HP);
		Test.TestEqual(*FString::Printf(TEXT("Unit %d max hp"), Index), ActualUnit.MaxHP, ExpectedUnit.MaxHP);
	}

	const int32 WallCount = FMath::Min(Actual.Walls.Num(), Expected.Walls.Num());
	for (int32 Index = 0; Index < WallCount; ++Index)
	{
		const FWBPublicWallEdgeSummary& ActualWall = Actual.Walls[Index];
		const FWBPublicWallEdgeSummary& ExpectedWall = Expected.Walls[Index];
		Test.TestEqual(*FString::Printf(TEXT("Wall %d ax"), Index), ActualWall.AX, ExpectedWall.AX);
		Test.TestEqual(*FString::Printf(TEXT("Wall %d ay"), Index), ActualWall.AY, ExpectedWall.AY);
		Test.TestEqual(*FString::Printf(TEXT("Wall %d bx"), Index), ActualWall.BX, ExpectedWall.BX);
		Test.TestEqual(*FString::Printf(TEXT("Wall %d by"), Index), ActualWall.BY, ExpectedWall.BY);
		Test.TestEqual(*FString::Printf(TEXT("Wall %d orientation"), Index), ActualWall.Orientation.GetPlainNameString(), ExpectedWall.Orientation.GetPlainNameString());
	}

	const int32 TerrainCount = FMath::Min(Actual.TerrainTiles.Num(), Expected.TerrainTiles.Num());
	for (int32 Index = 0; Index < TerrainCount; ++Index)
	{
		const FWBPublicTerrainTileSummary& ActualTerrain = Actual.TerrainTiles[Index];
		const FWBPublicTerrainTileSummary& ExpectedTerrain = Expected.TerrainTiles[Index];
		Test.TestEqual(*FString::Printf(TEXT("Terrain %d x"), Index), ActualTerrain.X, ExpectedTerrain.X);
		Test.TestEqual(*FString::Printf(TEXT("Terrain %d y"), Index), ActualTerrain.Y, ExpectedTerrain.Y);
		Test.TestEqual(*FString::Printf(TEXT("Terrain %d id"), Index), ActualTerrain.TerrainId.GetPlainNameString(), ExpectedTerrain.TerrainId.GetPlainNameString());
	}
}

void ExpectStateStillMatchesBridgeFixture(
	FAutomationTestBase& Test,
	const FWBGameStateData& State,
	const FWBGameStateData& Before)
{
	Test.TestEqual(TEXT("Current player unchanged"), State.CurrentPlayer, Before.CurrentPlayer);
	Test.TestEqual(TEXT("Priority player unchanged"), State.PriorityPlayer, Before.PriorityPlayer);
	Test.TestEqual(TEXT("Turn number unchanged"), State.TurnNumber, Before.TurnNumber);
	Test.TestEqual(TEXT("Unit count unchanged"), State.Units.Num(), Before.Units.Num());
	Test.TestEqual(TEXT("Wall count unchanged"), State.Walls.Num(), Before.Walls.Num());
	Test.TestEqual(TEXT("Terrain count unchanged"), State.TerrainByTileIndex.Num(), Before.TerrainByTileIndex.Num());
	Test.TestEqual(TEXT("Player count unchanged"), State.Players.Num(), Before.Players.Num());

	if (State.Units.Num() > 0 && Before.Units.Num() > 0)
	{
		Test.TestEqual(TEXT("First unit id unchanged"), State.Units[0].UnitId, Before.Units[0].UnitId);
		Test.TestEqual(TEXT("First unit x unchanged"), State.Units[0].X, Before.Units[0].X);
		Test.TestEqual(TEXT("First unit y unchanged"), State.Units[0].Y, Before.Units[0].Y);
		Test.TestEqual(TEXT("First unit hp unchanged"), State.Units[0].HP, Before.Units[0].HP);
	}

	if (State.Players.Num() > 0 && Before.Players.Num() > 0)
	{
		Test.TestEqual(TEXT("Deck secret unchanged"), State.Players[0].Deck[0], Before.Players[0].Deck[0]);
		Test.TestEqual(TEXT("Hand secret unchanged"), State.Players[0].Hand[0], Before.Players[0].Hand[0]);
		Test.TestEqual(TEXT("Discard secret unchanged"), State.Players[0].Discard[0], Before.Players[0].Discard[0]);
	}
}

FString SerializeSummaryForHiddenBoundaryTest(const FWBPublicBoardSummary& Summary)
{
	FWBRuntimeSelectedActionResult Envelope;
	Envelope.FinalPublicBoardSummary = Summary;

	FString Json;
	WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Envelope, Json);
	return Json;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardSummaryBridgeBuildMatchesPublicSummaryTest, "Wandbound.Runtime.BoardSummaryBridge.BuildMatchesPublicSummaryBuilder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardSummaryBridgeBuildMatchesPublicSummaryTest::RunTest(const FString& Parameters)
{
	const FWBGameStateData State = MakeBridgeTestState();
	const FWBPublicBoardSummary Expected = WBPublicBoardSummary::Build(State);
	const FWBPublicBoardSummary Actual = WBBoardSummaryBridge::BuildPublicSummaryFromState(State);

	ExpectSummaryMatches(*this, Actual, Expected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardSummaryBridgeNullActorTest, "Wandbound.Runtime.BoardSummaryBridge.RenderSummaryNullActorSafe", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardSummaryBridgeNullActorTest::RunTest(const FString& Parameters)
{
	const FWBPublicBoardSummary Summary = WBBoardSummaryBridge::BuildPublicSummaryFromState(MakeBridgeTestState());
	const FWBBoardViewRefreshResult Result = WBBoardSummaryBridge::RenderSummaryToBoardView(Summary, nullptr);
	TestFalse(TEXT("Null actor not rendered"), Result.bRendered);
	TestEqual(TEXT("No tiles rendered"), Result.TileCount, 0);
	TestEqual(TEXT("No units rendered"), Result.UnitCount, 0);
	TestEqual(TEXT("No walls rendered"), Result.WallCount, 0);
	TestEqual(TEXT("No terrain rendered"), Result.TerrainCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardSummaryBridgeRenderStateDoesNotMutateTest, "Wandbound.Runtime.BoardSummaryBridge.RenderStateDoesNotMutateState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardSummaryBridgeRenderStateDoesNotMutateTest::RunTest(const FString& Parameters)
{
	FWBGameStateData State = MakeBridgeTestState();
	const FWBGameStateData Before = State;

	WBBoardSummaryBridge::RenderStateToBoardView(State, nullptr);

	ExpectStateStillMatchesBridgeFixture(*this, State, Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardSummaryBridgeRenderStateUpdatesCountsTest, "Wandbound.Runtime.BoardSummaryBridge.RenderStateUpdatesActorCounts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardSummaryBridgeRenderStateUpdatesCountsTest::RunTest(const FString& Parameters)
{
	AWBBoardViewActor* Actor = NewObject<AWBBoardViewActor>(GetTransientPackage());
	TestNotNull(TEXT("Actor"), Actor);
	if (Actor == nullptr)
	{
		return false;
	}

	const FWBBoardViewRefreshResult Result = WBBoardSummaryBridge::RenderStateToBoardView(MakeBridgeTestState(), Actor);
	TestTrue(TEXT("Rendered"), Result.bRendered);
	TestEqual(TEXT("Result tile count"), Result.TileCount, 81);
	TestEqual(TEXT("Result unit count"), Result.UnitCount, 2);
	TestEqual(TEXT("Result wall count"), Result.WallCount, 1);
	TestEqual(TEXT("Result terrain count"), Result.TerrainCount, 1);
	TestEqual(TEXT("Actor tile count"), Actor->GetRenderedTileCount(), 81);
	TestEqual(TEXT("Actor unit count"), Actor->GetRenderedUnitCount(), 2);
	TestEqual(TEXT("Actor wall count"), Actor->GetRenderedWallCount(), 1);
	TestEqual(TEXT("Actor terrain count"), Actor->GetRenderedTerrainCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardSummaryBridgeHiddenDataExcludedTest, "Wandbound.Runtime.BoardSummaryBridge.HiddenDataExcluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardSummaryBridgeHiddenDataExcludedTest::RunTest(const FString& Parameters)
{
	const FWBPublicBoardSummary Summary = WBBoardSummaryBridge::BuildPublicSummaryFromState(MakeBridgeTestState());
	const FString Serialized = SerializeSummaryForHiddenBoundaryTest(Summary);

	TestFalse(TEXT("Player deck secret absent"), Serialized.Contains(TEXT("SECRET_DECK_CARD_DO_NOT_LEAK")));
	TestFalse(TEXT("Player hand secret absent"), Serialized.Contains(TEXT("SECRET_HAND_CARD_DO_NOT_LEAK")));
	TestFalse(TEXT("Player discard secret absent"), Serialized.Contains(TEXT("SECRET_DISCARD_CARD_DO_NOT_LEAK")));
	TestFalse(TEXT("Opponent deck secret absent"), Serialized.Contains(TEXT("SECRET_OPPONENT_DECK_CARD_DO_NOT_LEAK")));
	TestFalse(TEXT("Opponent hand secret absent"), Serialized.Contains(TEXT("SECRET_OPPONENT_HAND_CARD_DO_NOT_LEAK")));
	TestFalse(TEXT("Opponent discard secret absent"), Serialized.Contains(TEXT("SECRET_OPPONENT_DISCARD_CARD_DO_NOT_LEAK")));
	TestTrue(TEXT("Visible unit remains public"), Serialized.Contains(TEXT("char_visible_player")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardSummaryBridgeNoActionGenerationSourceGuardTest, "Wandbound.Runtime.BoardSummaryBridge.NoActionGenerationSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardSummaryBridgeNoActionGenerationSourceGuardTest::RunTest(const FString& Parameters)
{
	const FString SourcePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBBoardSummaryBridge.cpp"));
	FString Source;
	TestTrue(TEXT("Bridge source loads"), FFileHelper::LoadFileToString(Source, *SourcePath));
	TestFalse(TEXT("Does not call GenerateLegalActions"), Source.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("Does not call QueryMove"), Source.Contains(TEXT("QueryMove")));
	TestFalse(TEXT("Does not call ApplyMove"), Source.Contains(TEXT("ApplyMove")));
	return true;
}

#endif
