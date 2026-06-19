#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WBBoardViewActor.h"
#include "WBBoardViewDemoData.h"
#include "WBBoardViewDemoHarnessActor.h"
#include "WBRuntimeResultSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FString SerializeDemoSummary()
{
	FWBRuntimeSelectedActionResult Envelope;
	Envelope.FinalPublicBoardSummary = WBBoardViewDemoData::MakeSmallDemoBoardSummary();

	FString Json;
	WBRuntimeResultSerializer::RuntimeSelectedActionResultToJsonString(Envelope, Json);
	return Json;
}

bool LoadHarnessSource(FString& OutSource)
{
	const FString HeaderPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Public"), TEXT("WBBoardViewDemoHarnessActor.h"));
	const FString SourcePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("WandboundRuntime"), TEXT("Private"), TEXT("WBBoardViewDemoHarnessActor.cpp"));

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewDemoHarnessClassExistsTest, "Wandbound.Runtime.BoardViewDemoHarness.ClassExists", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewDemoHarnessClassExistsTest::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("Demo harness class"), AWBBoardViewDemoHarnessActor::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewDemoHarnessNullActorSafeTest, "Wandbound.Runtime.BoardViewDemoHarness.RenderDemoBoardNullActorSafe", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewDemoHarnessNullActorSafeTest::RunTest(const FString& Parameters)
{
	AWBBoardViewDemoHarnessActor* Harness = NewObject<AWBBoardViewDemoHarnessActor>(GetTransientPackage());
	TestNotNull(TEXT("Harness"), Harness);
	if (Harness == nullptr)
	{
		return false;
	}

	Harness->SetBoardViewActor(nullptr);
	Harness->bFindBoardViewActorIfMissing = false;

	const FWBBoardViewRefreshResult Result = Harness->RenderDemoBoard();
	TestFalse(TEXT("Null actor not rendered"), Result.bRendered);
	TestEqual(TEXT("Tile count"), Result.TileCount, 0);
	TestEqual(TEXT("Unit count"), Result.UnitCount, 0);
	TestEqual(TEXT("Wall count"), Result.WallCount, 0);
	TestEqual(TEXT("Terrain count"), Result.TerrainCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewDemoHarnessRenderUpdatesCountsTest, "Wandbound.Runtime.BoardViewDemoHarness.RenderDemoBoardUpdatesCounts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewDemoHarnessRenderUpdatesCountsTest::RunTest(const FString& Parameters)
{
	AWBBoardViewActor* BoardView = NewObject<AWBBoardViewActor>(GetTransientPackage());
	AWBBoardViewDemoHarnessActor* Harness = NewObject<AWBBoardViewDemoHarnessActor>(GetTransientPackage());
	TestNotNull(TEXT("Board view"), BoardView);
	TestNotNull(TEXT("Harness"), Harness);
	if (BoardView == nullptr || Harness == nullptr)
	{
		return false;
	}

	Harness->SetBoardViewActor(BoardView);
	const FWBBoardViewRefreshResult Result = Harness->RenderDemoBoard();

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewDemoHarnessBeginPlayDefaultTest, "Wandbound.Runtime.BoardViewDemoHarness.BeginPlayDoesNotRenderByDefault", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewDemoHarnessBeginPlayDefaultTest::RunTest(const FString& Parameters)
{
	AWBBoardViewActor* BoardView = NewObject<AWBBoardViewActor>(GetTransientPackage());
	AWBBoardViewDemoHarnessActor* Harness = NewObject<AWBBoardViewDemoHarnessActor>(GetTransientPackage());
	TestNotNull(TEXT("Board view"), BoardView);
	TestNotNull(TEXT("Harness"), Harness);
	if (BoardView == nullptr || Harness == nullptr)
	{
		return false;
	}

	Harness->SetBoardViewActor(BoardView);
	TestFalse(TEXT("Render on begin play default false"), Harness->bRenderDemoOnBeginPlay);
	TestEqual(TEXT("No automatic tile render"), BoardView->GetRenderedTileCount(), 0);
	TestEqual(TEXT("No automatic unit render"), BoardView->GetRenderedUnitCount(), 0);
	TestEqual(TEXT("No automatic wall render"), BoardView->GetRenderedWallCount(), 0);
	TestEqual(TEXT("No automatic terrain render"), BoardView->GetRenderedTerrainCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewDemoHarnessNoGameStateSourceGuardTest, "Wandbound.Runtime.BoardViewDemoHarness.NoGameStateDependencySourceGuard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewDemoHarnessNoGameStateSourceGuardTest::RunTest(const FString& Parameters)
{
	FString Source;
	TestTrue(TEXT("Harness source loads"), LoadHarnessSource(Source));
	TestFalse(TEXT("Does not name FWBGameStateData"), Source.Contains(TEXT("FWBGameStateData")));
	TestFalse(TEXT("Does not call BuildPublicSummaryFromState"), Source.Contains(TEXT("BuildPublicSummaryFromState")));
	TestFalse(TEXT("Does not call GenerateLegalActions"), Source.Contains(TEXT("GenerateLegalActions")));
	TestFalse(TEXT("Does not call QueryMove"), Source.Contains(TEXT("QueryMove")));
	TestFalse(TEXT("Does not call ApplyMove"), Source.Contains(TEXT("ApplyMove")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewDemoHarnessDemoSummaryPublicOnlyTest, "Wandbound.Runtime.BoardViewDemoHarness.DemoSummaryHasNoHiddenData", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewDemoHarnessDemoSummaryPublicOnlyTest::RunTest(const FString& Parameters)
{
	const FWBPublicBoardSummary Summary = WBBoardViewDemoData::MakeSmallDemoBoardSummary();
	TestEqual(TEXT("Demo unit count"), Summary.Units.Num(), 2);
	TestEqual(TEXT("Demo wall count"), Summary.Walls.Num(), 1);
	TestEqual(TEXT("Demo terrain count"), Summary.TerrainTiles.Num(), 2);

	const FString Serialized = SerializeDemoSummary();
	TestFalse(TEXT("No deck field"), Serialized.Contains(TEXT("deck")));
	TestFalse(TEXT("No hand field"), Serialized.Contains(TEXT("hand")));
	TestFalse(TEXT("No discard field"), Serialized.Contains(TEXT("discard")));
	TestFalse(TEXT("No marker field"), Serialized.Contains(TEXT("marker")));
	TestFalse(TEXT("No secret token"), Serialized.Contains(TEXT("SECRET")));
	return true;
}

#endif
