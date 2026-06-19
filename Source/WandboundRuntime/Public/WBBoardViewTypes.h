#pragma once

#include "CoreMinimal.h"
#include "WBPublicBoardSummary.h"
#include "WBTypes.h"
#include "WBBoardViewTypes.generated.h"

USTRUCT()
struct WANDBOUNDRUNTIME_API FWBBoardViewSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Board View")
	int32 BoardWidth = 9;

	UPROPERTY(EditAnywhere, Category = "Board View")
	int32 BoardHeight = 9;

	UPROPERTY(EditAnywhere, Category = "Board View")
	float TileSize = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Board View")
	float TileThickness = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Board View")
	float UnitHeight = 55.0f;

	UPROPERTY(EditAnywhere, Category = "Board View")
	float WallHeight = 45.0f;

	UPROPERTY(EditAnywhere, Category = "Board View")
	float WallThickness = 10.0f;
};

class WANDBOUNDRUNTIME_API WBBoardViewTypes
{
public:
	static FVector TileCenterToWorld(const FWBTile& Tile, const FWBBoardViewSettings& Settings);
	static FTransform TileTransform(const FWBTile& Tile, const FWBBoardViewSettings& Settings);
	static FTransform UnitTransform(const FWBPublicUnitBoardSummary& Unit, const FWBBoardViewSettings& Settings);
	static FTransform WallTransform(const FWBPublicWallEdgeSummary& Wall, const FWBBoardViewSettings& Settings);
	static FTransform TerrainTransform(const FWBPublicTerrainTileSummary& Terrain, const FWBBoardViewSettings& Settings);
};
