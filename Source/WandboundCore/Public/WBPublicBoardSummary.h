#pragma once

#include "CoreMinimal.h"
#include "WBGameStateData.h"

struct WANDBOUNDCORE_API FWBPublicUnitStatusSummary
{
	FName StatusId;
	int32 TurnsRemaining = 0;
};

struct WANDBOUNDCORE_API FWBPublicUnitBoardSummary
{
	int32 UnitId = -1;
	int32 OwnerId = -1;
	FString CardId;
	int32 X = -1;
	int32 Y = -1;
	int32 HP = 0;
	int32 MaxHP = 0;
	int32 CurrentArmor = 0;
	int32 MaxArmor = 0;
	int32 ATK = 0;
	int32 AR = 0;
	int32 RLTotal = 0;
	int32 RLUsed = 0;
	int32 AttacksLeft = 0;
	TArray<FWBPublicUnitStatusSummary> Statuses;
};

struct WANDBOUNDCORE_API FWBPublicWallEdgeSummary
{
	int32 AX = -1;
	int32 AY = -1;
	int32 BX = -1;
	int32 BY = -1;
	FName Orientation;
};

struct WANDBOUNDCORE_API FWBPublicTerrainTileSummary
{
	int32 X = -1;
	int32 Y = -1;
	FName TerrainId;
};

struct WANDBOUNDCORE_API FWBPublicBoardSummary
{
	int32 BoardWidth = 9;
	int32 BoardHeight = 9;
	FName DefaultTerrainId = FName(TEXT("Normal"));
	TArray<FWBPublicUnitBoardSummary> Units;
	TArray<FWBPublicWallEdgeSummary> Walls;
	TArray<FWBPublicTerrainTileSummary> TerrainTiles;
};

class WANDBOUNDCORE_API WBPublicBoardSummary
{
public:
	static FWBPublicBoardSummary Build(const FWBGameStateData& State);
};
