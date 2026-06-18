#include "Misc/AutomationTest.h"
#include "WBBoardViewTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
FWBBoardViewSettings MakeCoordinateTestSettings()
{
	FWBBoardViewSettings Settings;
	Settings.BoardWidth = 9;
	Settings.BoardHeight = 9;
	Settings.TileSize = 100.0f;
	Settings.TileThickness = 4.0f;
	Settings.UnitHeight = 55.0f;
	Settings.WallHeight = 45.0f;
	Settings.WallThickness = 10.0f;
	return Settings;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewCenterTileTest, "Wandbound.Runtime.BoardViewCoordinates.CenterTileMapsToWorldOrigin", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewCenterTileTest::RunTest(const FString& Parameters)
{
	const FVector World = WBBoardViewTypes::TileCenterToWorld(FWBTile(4, 4), MakeCoordinateTestSettings());
	TestEqual(TEXT("World X"), World.X, 0.0);
	TestEqual(TEXT("World Y"), World.Y, 0.0);
	TestEqual(TEXT("World Z"), World.Z, 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewBottomLeftTileTest, "Wandbound.Runtime.BoardViewCoordinates.BottomLeftMapsNegative", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewBottomLeftTileTest::RunTest(const FString& Parameters)
{
	const FVector World = WBBoardViewTypes::TileCenterToWorld(FWBTile(0, 0), MakeCoordinateTestSettings());
	TestEqual(TEXT("World X"), World.X, -400.0);
	TestEqual(TEXT("World Y"), World.Y, -400.0);
	TestEqual(TEXT("World Z"), World.Z, 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewTopRightTileTest, "Wandbound.Runtime.BoardViewCoordinates.TopRightMapsPositive", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewTopRightTileTest::RunTest(const FString& Parameters)
{
	const FVector World = WBBoardViewTypes::TileCenterToWorld(FWBTile(8, 8), MakeCoordinateTestSettings());
	TestEqual(TEXT("World X"), World.X, 400.0);
	TestEqual(TEXT("World Y"), World.Y, 400.0);
	TestEqual(TEXT("World Z"), World.Z, 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewUnitTransformTest, "Wandbound.Runtime.BoardViewCoordinates.UnitTransformUsesTileCenterAndHeight", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewUnitTransformTest::RunTest(const FString& Parameters)
{
	FWBPublicUnitBoardSummary Unit;
	Unit.UnitId = 1;
	Unit.X = 4;
	Unit.Y = 4;

	const FTransform Transform = WBBoardViewTypes::UnitTransform(Unit, MakeCoordinateTestSettings());
	TestEqual(TEXT("Location X"), Transform.GetLocation().X, 0.0);
	TestEqual(TEXT("Location Y"), Transform.GetLocation().Y, 0.0);
	TestEqual(TEXT("Location Z"), Transform.GetLocation().Z, 55.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewHorizontalWallTransformTest, "Wandbound.Runtime.BoardViewCoordinates.HorizontalWallTransformDeterministic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewHorizontalWallTransformTest::RunTest(const FString& Parameters)
{
	FWBPublicWallEdgeSummary Wall;
	Wall.AX = 4;
	Wall.AY = 4;
	Wall.BX = 5;
	Wall.BY = 4;
	Wall.Orientation = FName(TEXT("horizontal"));

	const FTransform Transform = WBBoardViewTypes::WallTransform(Wall, MakeCoordinateTestSettings());
	TestEqual(TEXT("Location X"), Transform.GetLocation().X, 50.0);
	TestEqual(TEXT("Location Y"), Transform.GetLocation().Y, 0.0);
	TestEqual(TEXT("Location Z"), Transform.GetLocation().Z, 22.5);
	TestEqual(TEXT("Scale X"), Transform.GetScale3D().X, 1.0);
	TestEqual(TEXT("Scale Y"), Transform.GetScale3D().Y, 0.1);
	TestEqual(TEXT("Scale Z"), Transform.GetScale3D().Z, 0.45);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewVerticalWallTransformTest, "Wandbound.Runtime.BoardViewCoordinates.VerticalWallTransformDeterministic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewVerticalWallTransformTest::RunTest(const FString& Parameters)
{
	FWBPublicWallEdgeSummary Wall;
	Wall.AX = 4;
	Wall.AY = 4;
	Wall.BX = 4;
	Wall.BY = 5;
	Wall.Orientation = FName(TEXT("vertical"));

	const FTransform Transform = WBBoardViewTypes::WallTransform(Wall, MakeCoordinateTestSettings());
	TestEqual(TEXT("Location X"), Transform.GetLocation().X, 0.0);
	TestEqual(TEXT("Location Y"), Transform.GetLocation().Y, 50.0);
	TestEqual(TEXT("Location Z"), Transform.GetLocation().Z, 22.5);
	TestEqual(TEXT("Scale X"), Transform.GetScale3D().X, 0.1);
	TestEqual(TEXT("Scale Y"), Transform.GetScale3D().Y, 1.0);
	TestEqual(TEXT("Scale Z"), Transform.GetScale3D().Z, 0.45);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewTerrainTransformTest, "Wandbound.Runtime.BoardViewCoordinates.TerrainTransformUsesTileCenter", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewTerrainTransformTest::RunTest(const FString& Parameters)
{
	FWBPublicTerrainTileSummary Terrain;
	Terrain.X = 3;
	Terrain.Y = 5;
	Terrain.TerrainId = FName(TEXT("mud"));

	const FTransform Transform = WBBoardViewTypes::TerrainTransform(Terrain, MakeCoordinateTestSettings());
	TestEqual(TEXT("Location X"), Transform.GetLocation().X, -100.0);
	TestEqual(TEXT("Location Y"), Transform.GetLocation().Y, 100.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWBBoardViewHelpersDoNotMutateInputTest, "Wandbound.Runtime.BoardViewCoordinates.HelpersDoNotMutateInput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWBBoardViewHelpersDoNotMutateInputTest::RunTest(const FString& Parameters)
{
	FWBPublicUnitBoardSummary Unit;
	Unit.UnitId = 7;
	Unit.OwnerId = 1;
	Unit.CardId = TEXT("visible_unit");
	Unit.X = 2;
	Unit.Y = 6;
	Unit.HP = 3;

	const FWBPublicUnitBoardSummary Before = Unit;
	WBBoardViewTypes::UnitTransform(Unit, MakeCoordinateTestSettings());

	TestEqual(TEXT("Unit id unchanged"), Unit.UnitId, Before.UnitId);
	TestEqual(TEXT("Owner unchanged"), Unit.OwnerId, Before.OwnerId);
	TestEqual(TEXT("Card id unchanged"), Unit.CardId, Before.CardId);
	TestEqual(TEXT("X unchanged"), Unit.X, Before.X);
	TestEqual(TEXT("Y unchanged"), Unit.Y, Before.Y);
	TestEqual(TEXT("HP unchanged"), Unit.HP, Before.HP);
	return true;
}

#endif
