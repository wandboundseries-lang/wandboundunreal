#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBBoardViewActor.h"
#include "WBBoardViewDemoData.h"
#include "WBBoardViewStateApplierComponent.h"
#include "WBPublicBoardSummary.h"
#include "WBRuntimeResultSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
UWBBoardViewStateApplierComponent* MakeApplier()
{
	return NewObject<UWBBoardViewStateApplierComponent>(GetTransientPackage());
}

FWBUnitState MakeStateApplierTestUnit(
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

FWBGameStateData MakeStateApplierTestState()
{
	FWBGameStateData State;
	State.CurrentPlayer = 0;
	State.PriorityPlayer = 0;
	State.TurnNumber = 5;

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

	State.AddUnitForTest(MakeStateApplierTestUnit(1, 0, FWBTile(4, 4), TEXT("char_visible_player")));
	State.AddUnitForTest(MakeStateApplierTestUnit(2, 1, FWBTile(5, 4), TEXT("char_visible_opponent")));
	State.AddWallForTest(FWBWallEdge(FWBTile(4, 4), FWBTile(4, 5)));
	State.SetTerrainForTest(FWBTile(2, 3), FName(TEXT("Mud")));

	return State;
}

void ExpectDemoSummaryCounts(
	FAutomationTestBase& Test,
	const FWBPublicBoardSummary& Summary)
{
	Test.TestEqual(TEXT("Board width"), Summary.BoardWidth, 9);
	Test.TestEqual(TEXT("Board height"), Summary.BoardHeight, 9);
	Test.TestEqual(TEXT("Unit count"), Summary.Units.Num(), 2);
	Test.TestEqual(TEXT("Wall count"), Summary.Walls.Num(), 1);
	Test.TestEqual(TEXT("Terrain count"), Summary.TerrainTiles.Num(), 2);
}

void ExpectSummaryStillMatches(
	FAutomationTestBase& Test,
	const FWBPublicBoardSummary& Summary,
	const FWBPublicBoardSummary& Before)
{
	Test.TestEqual(TEXT("Input board width unchanged"), Summary.BoardWidth, Before.BoardWidth);
	Test.TestEqual(TEXT("Input board height unchanged"), Summary.BoardHeight, Before.BoardHeight);
	Test.TestEqual(TEXT("Input unit count unchanged"), Summary.Units.Num(), Before.Units.Num());
	Test.TestEqual(TEXT("Input wall count unchanged"), Summary.Walls.Num(), Before.Walls.Num());
	Test.TestEqual(TEXT("Input terrain count unchanged"), Summary.TerrainTiles.Num(), Before.TerrainTiles.Num());
	if (Summary.Units.Num() > 0 && Before.Units.Num() > 0)
	{
		Test.TestEqual(TEXT("Input first unit id unchanged"), Summary.Units[0].UnitId, Before.Units[0].UnitId);
		Test.TestEqual(TEXT("Input first unit card unchanged"), Summary.Units[0].CardId, Before.Units[0].CardId);
		Test.TestEqual(TEXT("Input first unit x unchanged"), Summary.Units[0].X, Before.Units[0].X);
		Test.TestEqual(TEXT("Input first unit y unchanged"), Summary.Units[0].Y, Before.Units[0].Y);
	}
}

void ExpectPublicSummaryMatchesBuilder(
	FAutomationTestBase& Test,
	const FWBPublicBoardSummary& Actual,
	const FWBPublicBoardSummary& Expected)
{
	Test.TestEqual(TEXT("Builder board width"), Actual.BoardWidth, Expected.BoardWidth);
	Test.TestEqual(TEXT("Builder board height"), Actual.BoardHeight, Expected.BoardHeight);
	Test.TestEqual(TEXT("Builder unit count"), Actual.Units.Num(), Expected.Units.Num());
	Test.TestEqual(TEXT("Builder wall count"), Actual.Walls.Num(), Expected.Walls.Num());
	Test.TestEqual(TEXT("Builder terrain count"), Actual.TerrainTiles.Num(), Expected.TerrainTiles.Num());

	if (Actual.Units.Num() > 0 && Expected.Units.Num() > 0)
	{
		Test.TestEqual(TEXT("Builder first unit id"), Actual.Units[0].UnitId, Expected.Units[0].UnitId);
		Test.TestEqual(TEXT("Builder first unit card"), Actual.Units[0].CardId, Expected.Units[0].CardId);
		Test.TestEqual(TEXT("Builder first unit x"), Actual.Units[0].X, Expected.Units[0].X);
		Test.TestEqual(TEXT("Builder first unit y"), Actual.Units[0].Y, Expected.Units[0].Y);
	}

	if (Actual.Walls.Num() > 0 && Expected.Walls.Num() > 0)
	{
		Test.TestEqual(TEXT("Builder first wall ax"), Actual.Walls[0].AX, Expected.Walls[0].AX);
		Test.TestEqual(TEXT("Builder first wall ay"), Actual.Walls[0].AY, Expected.Walls[0].AY);
		Test.TestEqual(TEXT("Builder first wall bx"), Actual.Walls[0].BX, Expected.Walls[0].BX);
		Test.TestEqual(TEXT("Builder first wall by"), Actual.Walls[0].BY, Expected.Walls[0].BY);
	}

	if (Actual.TerrainTiles.Num() > 0 && Expected.TerrainTiles.Num() > 0)
	{
		Test.TestEqual(TEXT("Builder first terrain x"), Actual.TerrainTiles[0].X, Expected.TerrainTiles[0].X);
		Test.TestEqual(TEXT("Builder first terrain y"), Actual.TerrainTiles[0].Y, Expected.TerrainTiles[0].Y);
		Test.TestEqual(TEXT("Builder first terrain id"), Actual.TerrainTiles[0].TerrainId.GetPlainNameString(), Expected.TerrainTiles[0].TerrainId.GetPlainNameString());
	}
}

void ExpectStateStillMatches(
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

	if (State.Players.Num() > 0 && Before.Players.Num() > 0)
	{
		Test.TestEqual(TEXT("Deck secret unchanged"), State.Players[0].Deck[0], Before.Players[0].Deck[0]);
		Test.TestEqual(TEXT("Hand secret unchanged"), State.Players[0].Hand[0], Before.Players[0].Hand[0]);
		Test.TestEqual(TEXT("Discard secret unchanged"), State.Players[0].Discard[0], Before.Players[0].Discard[0]);
	}
}

FString SerializeSummary(const FWBPublicBoardSummary& Summary)
{
	FWBRuntimeSelectedActionResult Envelope;
	Envelope.FinalPublicBoardSummary = Summary;

	FString Json;
	WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Envelope, Json);
	return Json;
}

bool LoadStateApplierSource(FString& OutSource)
{
	const FString HeaderPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Public"), TEXT("WBBoardViewStateApplierComponent.h"));
	const FString SourcePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBBoardViewStateApplierComponent.cpp"));

	FString Header;
	FString Source;
	if (!FFileHelper::LoadFileToString(Header, *HeaderPath) || !FFileHelper::LoadFileToString(Source, *SourcePath))
	{
		return false;
	}

	OutSource = Header + Source;
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewStateApplierClassExistsTest, "Wandbound.Runtime.BoardViewStateApplier.ClassExists", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewStateApplierClassExistsTest::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("State applier component class"), UWBBoardViewStateApplierComponent::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewStateApplierInitialStateTest, "Wandbound.Runtime.BoardViewStateApplier.InitialState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewStateApplierInitialStateTest::RunTest(const FString& Parameters)
{
	UWBBoardViewStateApplierComponent* Applier = MakeApplier();
	TestNotNull(TEXT("Applier"), Applier);
	if (Applier == nullptr)
	{
		return false;
	}

	TestFalse(TEXT("No latest public summary initially"), Applier->HasLatestPublicBoardSummary());
	const FWBBoardViewRefreshResult Result = Applier->ApplyLatestPublicBoardSummary();
	TestFalse(TEXT("No render without summary"), Result.bRendered);
	TestEqual(TEXT("No tile count"), Result.TileCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewStateApplierStoresSummaryTest, "Wandbound.Runtime.BoardViewStateApplier.SetSummaryStoresPublicSummary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewStateApplierStoresSummaryTest::RunTest(const FString& Parameters)
{
	UWBBoardViewStateApplierComponent* Applier = MakeApplier();
	TestNotNull(TEXT("Applier"), Applier);
	if (Applier == nullptr)
	{
		return false;
	}

	Applier->bApplyOnSummarySet = false;
	Applier->SetLatestPublicBoardSummary(WBBoardViewDemoData::MakeSmallDemoBoardSummary());
	TestTrue(TEXT("Has latest public summary"), Applier->HasLatestPublicBoardSummary());
	ExpectDemoSummaryCounts(*this, Applier->GetLatestPublicBoardSummary());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewStateApplierDoesNotMutateInputSummaryTest, "Wandbound.Runtime.BoardViewStateApplier.SetSummaryDoesNotMutateInput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewStateApplierDoesNotMutateInputSummaryTest::RunTest(const FString& Parameters)
{
	UWBBoardViewStateApplierComponent* Applier = MakeApplier();
	TestNotNull(TEXT("Applier"), Applier);
	if (Applier == nullptr)
	{
		return false;
	}

	FWBPublicBoardSummary Summary = WBBoardViewDemoData::MakeSmallDemoBoardSummary();
	const FWBPublicBoardSummary Before = Summary;
	Applier->SetLatestPublicBoardSummary(Summary);
	ExpectSummaryStillMatches(*this, Summary, Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewStateApplierNullActorApplySafeTest, "Wandbound.Runtime.BoardViewStateApplier.ApplyWithNullBoardActorSafe", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewStateApplierNullActorApplySafeTest::RunTest(const FString& Parameters)
{
	UWBBoardViewStateApplierComponent* Applier = MakeApplier();
	TestNotNull(TEXT("Applier"), Applier);
	if (Applier == nullptr)
	{
		return false;
	}

	Applier->bApplyOnSummarySet = false;
	Applier->SetBoardViewActor(nullptr);
	Applier->SetLatestPublicBoardSummary(WBBoardViewDemoData::MakeSmallDemoBoardSummary());

	const FWBBoardViewRefreshResult Result = Applier->ApplyLatestPublicBoardSummary();
	TestFalse(TEXT("Null board actor does not render"), Result.bRendered);
	TestEqual(TEXT("No tile count"), Result.TileCount, 0);
	TestEqual(TEXT("No unit count"), Result.UnitCount, 0);
	TestEqual(TEXT("No wall count"), Result.WallCount, 0);
	TestEqual(TEXT("No terrain count"), Result.TerrainCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewStateApplierActorApplyCountsTest, "Wandbound.Runtime.BoardViewStateApplier.ApplyWithActorUpdatesCounts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewStateApplierActorApplyCountsTest::RunTest(const FString& Parameters)
{
	AWBBoardViewActor* BoardView = NewObject<AWBBoardViewActor>(GetTransientPackage());
	UWBBoardViewStateApplierComponent* Applier = MakeApplier();
	TestNotNull(TEXT("Board view"), BoardView);
	TestNotNull(TEXT("Applier"), Applier);
	if (BoardView == nullptr || Applier == nullptr)
	{
		return false;
	}

	Applier->bApplyOnSummarySet = false;
	Applier->SetBoardViewActor(BoardView);
	Applier->SetLatestPublicBoardSummary(WBBoardViewDemoData::MakeSmallDemoBoardSummary());

	const FWBBoardViewRefreshResult Result = Applier->ApplyLatestPublicBoardSummary();
	TestTrue(TEXT("Rendered"), Result.bRendered);
	TestEqual(TEXT("Result tile count"), Result.TileCount, 81);
	TestEqual(TEXT("Result unit count"), Result.UnitCount, 2);
	TestEqual(TEXT("Result wall count"), Result.WallCount, 1);
	TestEqual(TEXT("Result terrain count"), Result.TerrainCount, 2);
	TestEqual(TEXT("Actor tile count"), BoardView->GetRenderedTileCount(), 81);
	TestEqual(TEXT("Actor unit count"), BoardView->GetRenderedUnitCount(), 2);
	TestEqual(TEXT("Actor wall count"), BoardView->GetRenderedWallCount(), 1);
	TestEqual(TEXT("Actor terrain count"), BoardView->GetRenderedTerrainCount(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewStateApplierFromStateMatchesBuilderTest, "Wandbound.Runtime.BoardViewStateApplier.SetSummaryFromStateMatchesBuilder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewStateApplierFromStateMatchesBuilderTest::RunTest(const FString& Parameters)
{
	UWBBoardViewStateApplierComponent* Applier = MakeApplier();
	TestNotNull(TEXT("Applier"), Applier);
	if (Applier == nullptr)
	{
		return false;
	}

	Applier->bApplyOnSummarySet = false;
	const FWBGameStateData State = MakeStateApplierTestState();
	Applier->SetLatestPublicBoardSummaryFromState(State);

	TestTrue(TEXT("Has latest public summary"), Applier->HasLatestPublicBoardSummary());
	ExpectPublicSummaryMatchesBuilder(*this, Applier->GetLatestPublicBoardSummary(), WBPublicBoardSummary::Build(State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewStateApplierFromStateDoesNotMutateTest, "Wandbound.Runtime.BoardViewStateApplier.SetSummaryFromStateDoesNotMutateState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewStateApplierFromStateDoesNotMutateTest::RunTest(const FString& Parameters)
{
	UWBBoardViewStateApplierComponent* Applier = MakeApplier();
	TestNotNull(TEXT("Applier"), Applier);
	if (Applier == nullptr)
	{
		return false;
	}

	Applier->bApplyOnSummarySet = false;
	FWBGameStateData State = MakeStateApplierTestState();
	const FWBGameStateData Before = State;
	Applier->SetLatestPublicBoardSummaryFromState(State);
	ExpectStateStillMatches(*this, State, Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewStateApplierApplyOnSetTest, "Wandbound.Runtime.BoardViewStateApplier.ApplyOnSummarySetBehavior", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewStateApplierApplyOnSetTest::RunTest(const FString& Parameters)
{
	AWBBoardViewActor* ManualBoardView = NewObject<AWBBoardViewActor>(GetTransientPackage());
	UWBBoardViewStateApplierComponent* ManualApplier = MakeApplier();
	TestNotNull(TEXT("Manual board view"), ManualBoardView);
	TestNotNull(TEXT("Manual applier"), ManualApplier);
	if (ManualBoardView == nullptr || ManualApplier == nullptr)
	{
		return false;
	}

	ManualApplier->SetBoardViewActor(ManualBoardView);
	ManualApplier->bApplyOnSummarySet = false;
	ManualApplier->SetLatestPublicBoardSummary(WBBoardViewDemoData::MakeSmallDemoBoardSummary());
	TestEqual(TEXT("Manual actor still unrendered"), ManualBoardView->GetRenderedTileCount(), 0);
	TestFalse(TEXT("Manual last result not rendered"), ManualApplier->GetLastRefreshResult().bRendered);

	AWBBoardViewActor* AutoBoardView = NewObject<AWBBoardViewActor>(GetTransientPackage());
	UWBBoardViewStateApplierComponent* AutoApplier = MakeApplier();
	TestNotNull(TEXT("Auto board view"), AutoBoardView);
	TestNotNull(TEXT("Auto applier"), AutoApplier);
	if (AutoBoardView == nullptr || AutoApplier == nullptr)
	{
		return false;
	}

	AutoApplier->SetBoardViewActor(AutoBoardView);
	AutoApplier->bApplyOnSummarySet = true;
	AutoApplier->SetLatestPublicBoardSummary(WBBoardViewDemoData::MakeSmallDemoBoardSummary());
	TestEqual(TEXT("Auto actor tile count"), AutoBoardView->GetRenderedTileCount(), 81);
	TestEqual(TEXT("Auto actor unit count"), AutoBoardView->GetRenderedUnitCount(), 2);
	TestTrue(TEXT("Auto last result rendered"), AutoApplier->GetLastRefreshResult().bRendered);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewStateApplierHiddenDataExcludedTest, "Wandbound.Runtime.BoardViewStateApplier.HiddenDataExcluded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewStateApplierHiddenDataExcludedTest::RunTest(const FString& Parameters)
{
	UWBBoardViewStateApplierComponent* Applier = MakeApplier();
	TestNotNull(TEXT("Applier"), Applier);
	if (Applier == nullptr)
	{
		return false;
	}

	Applier->bApplyOnSummarySet = false;
	Applier->SetLatestPublicBoardSummaryFromState(MakeStateApplierTestState());
	const FString Serialized = SerializeSummary(Applier->GetLatestPublicBoardSummary());

	TestFalse(TEXT("Player deck secret absent"), Serialized.Contains(TEXT("SECRET_DECK_CARD_DO_NOT_LEAK")));
	TestFalse(TEXT("Player hand secret absent"), Serialized.Contains(TEXT("SECRET_HAND_CARD_DO_NOT_LEAK")));
	TestFalse(TEXT("Player discard secret absent"), Serialized.Contains(TEXT("SECRET_DISCARD_CARD_DO_NOT_LEAK")));
	TestFalse(TEXT("Opponent deck secret absent"), Serialized.Contains(TEXT("SECRET_OPPONENT_DECK_CARD_DO_NOT_LEAK")));
	TestFalse(TEXT("Opponent hand secret absent"), Serialized.Contains(TEXT("SECRET_OPPONENT_HAND_CARD_DO_NOT_LEAK")));
	TestFalse(TEXT("Opponent discard secret absent"), Serialized.Contains(TEXT("SECRET_OPPONENT_DISCARD_CARD_DO_NOT_LEAK")));
	TestTrue(TEXT("Visible unit remains public"), Serialized.Contains(TEXT("char_visible_player")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewStateApplierNoActionGenerationSourceGuardTest, "Wandbound.Runtime.BoardViewStateApplier.NoActionGenerationSourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewStateApplierNoActionGenerationSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("State applier source loads"), LoadStateApplierSource(Source));
	TestFalse(TEXT("Does not call GenerateLegalActions"), Source.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("Does not call QueryMove"), Source.Contains(TEXT("QueryMove")));
	TestFalse(TEXT("Does not call ApplyMove"), Source.Contains(TEXT("ApplyMove")));
	TestFalse(TEXT("Does not mention Deck"), Source.Contains(TEXT("Deck")));
	TestFalse(TEXT("Does not mention Hand"), Source.Contains(TEXT("Hand")));
	TestFalse(TEXT("Does not mention Discard"), Source.Contains(TEXT("Discard")));
	return true;
}

#endif
