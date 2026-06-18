#include "Misc/AutomationTest.h"
#include "WBBoardViewActor.h"
#include "WBBoardViewDemoData.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBPublicBoardSummary MakeActorTestSummary()
{
	FWBPublicBoardSummary Summary = WBBoardViewDemoData::MakeSmallDemoBoardSummary();
	Summary.BoardWidth = 9;
	Summary.BoardHeight = 9;
	return Summary;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewActorClassExistsTest, "Wandbound.Runtime.BoardViewActor.ClassExists", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewActorClassExistsTest::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("Board view actor class"), AWBBoardViewActor::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewActorNullMeshRenderTest, "Wandbound.Runtime.BoardViewActor.RenderPublicSummaryWithNullMeshesDoesNotCrash", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewActorNullMeshRenderTest::RunTest(const FString& Parameters)
{
	AWBBoardViewActor* Actor = NewObject<AWBBoardViewActor>(GetTransientPackage());
	TestNotNull(TEXT("Actor"), Actor);
	if (Actor == nullptr)
	{
		return false;
	}

	Actor->RenderPublicBoardSummary(MakeActorTestSummary());
	TestEqual(TEXT("Tile count"), Actor->GetRenderedTileCount(), 81);
	TestEqual(TEXT("Unit count"), Actor->GetRenderedUnitCount(), 2);
	TestEqual(TEXT("Wall count"), Actor->GetRenderedWallCount(), 1);
	TestEqual(TEXT("Terrain count"), Actor->GetRenderedTerrainCount(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewActorClearTest, "Wandbound.Runtime.BoardViewActor.ClearBoardViewDoesNotCrash", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewActorClearTest::RunTest(const FString& Parameters)
{
	AWBBoardViewActor* Actor = NewObject<AWBBoardViewActor>(GetTransientPackage());
	TestNotNull(TEXT("Actor"), Actor);
	if (Actor == nullptr)
	{
		return false;
	}

	Actor->RenderPublicBoardSummary(MakeActorTestSummary());
	Actor->ClearBoardView();
	TestEqual(TEXT("Tile count cleared"), Actor->GetRenderedTileCount(), 0);
	TestEqual(TEXT("Unit count cleared"), Actor->GetRenderedUnitCount(), 0);
	TestEqual(TEXT("Wall count cleared"), Actor->GetRenderedWallCount(), 0);
	TestEqual(TEXT("Terrain count cleared"), Actor->GetRenderedTerrainCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewActorCountsMatchSummaryTest, "Wandbound.Runtime.BoardViewActor.RenderCountsMatchPublicSummary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewActorCountsMatchSummaryTest::RunTest(const FString& Parameters)
{
	AWBBoardViewActor* Actor = NewObject<AWBBoardViewActor>(GetTransientPackage());
	TestNotNull(TEXT("Actor"), Actor);
	if (Actor == nullptr)
	{
		return false;
	}

	const FWBPublicBoardSummary Summary = MakeActorTestSummary();
	Actor->RenderPublicBoardSummary(Summary);
	TestEqual(TEXT("Tile count"), Actor->GetRenderedTileCount(), Summary.BoardWidth * Summary.BoardHeight);
	TestEqual(TEXT("Unit count"), Actor->GetRenderedUnitCount(), Summary.Units.Num());
	TestEqual(TEXT("Wall count"), Actor->GetRenderedWallCount(), Summary.Walls.Num());
	TestEqual(TEXT("Terrain count"), Actor->GetRenderedTerrainCount(), Summary.TerrainTiles.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewActorPublicSummaryOnlyTest, "Wandbound.Runtime.BoardViewActor.ConsumesPublicSummaryOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewActorPublicSummaryOnlyTest::RunTest(const FString& Parameters)
{
	AWBBoardViewActor* Actor = NewObject<AWBBoardViewActor>(GetTransientPackage());
	TestNotNull(TEXT("Actor"), Actor);
	if (Actor == nullptr)
	{
		return false;
	}

	FWBPublicBoardSummary Summary;
	Summary.BoardWidth = 9;
	Summary.BoardHeight = 9;
	Actor->RenderPublicBoardSummary(Summary);
	TestEqual(TEXT("Empty public board still renders tiles"), Actor->GetRenderedTileCount(), 81);
	TestEqual(TEXT("No units"), Actor->GetRenderedUnitCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewDemoDataPublicOnlyTest, "Wandbound.Runtime.BoardViewActor.DemoSummaryHasNoHiddenData", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewDemoDataPublicOnlyTest::RunTest(const FString& Parameters)
{
	const FWBPublicBoardSummary Summary = WBBoardViewDemoData::MakeSmallDemoBoardSummary();
	TestEqual(TEXT("Board width"), Summary.BoardWidth, 9);
	TestEqual(TEXT("Board height"), Summary.BoardHeight, 9);
	TestEqual(TEXT("Demo unit count"), Summary.Units.Num(), 2);
	TestEqual(TEXT("Demo wall count"), Summary.Walls.Num(), 1);
	TestEqual(TEXT("Demo terrain count"), Summary.TerrainTiles.Num(), 2);

	for (const FWBPublicUnitBoardSummary& Unit : Summary.Units)
	{
		TestFalse(TEXT("No secret marker in visible card id"), Unit.CardId.Contains(TEXT("secret")));
	}
	return true;
}

#endif
